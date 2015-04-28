/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#define _USE_MATH_DEFINES

#include "webrtc/modules/audio_processing/beamformer/beamformer.h"

#include <algorithm>
#include <cmath>

#include "webrtc/base/arraysize.h"
#include "webrtc/common_audio/window_generator.h"
#include "webrtc/modules/audio_processing/beamformer/covariance_matrix_generator.h"

namespace webrtc {
namespace {

// Alpha for the Kaiser Bessel Derived window.
const float kAlpha = 1.5f;

// The minimum value a post-processing mask can take.
const float kMaskMinimum = 0.01f;

const float kSpeedOfSoundMeterSeconds = 343;

// For both target and interference angles, 0 is perpendicular to the microphone
// array, facing forwards. The positive direction goes counterclockwise.
// The angle at which we amplify sound.
const float kTargetAngleRadians = 0.f;

// The angle at which we suppress sound. Suppression is symmetric around 0
// radians, so sound is suppressed at both +|kInterfAngleRadians| and
// -|kInterfAngleRadians|. Since the beamformer is robust, this should
// suppress sound coming from angles near +-|kInterfAngleRadians| as well.
const float kInterfAngleRadians = static_cast<float>(M_PI) / 4.f;

// When calculating the interference covariance matrix, this is the weight for
// the weighted average between the uniform covariance matrix and the angled
// covariance matrix.
// Rpsi = Rpsi_angled * kBalance + Rpsi_uniform * (1 - kBalance)
const float kBalance = 0.4f;

// TODO(claguna): need comment here.
const float kBeamwidthConstant = 0.00002f;

// Width of the boxcar.
const float kBoxcarHalfWidth = 0.01f;

// We put a gap in the covariance matrix where we expect the target to come
// from. Warning: This must be very small, ex. < 0.01, because otherwise it can
// cause the covariance matrix not to be positive semidefinite, and we require
// that our covariance matrices are positive semidefinite.
const float kCovUniformGapHalfWidth = 0.01f;

// Alpha coefficient for mask smoothing.
const float kMaskSmoothAlpha = 0.2f;

// The average mask is computed from masks in this mid-frequency range. If these
// ranges are changed |kMaskQuantile| might need to be adjusted.
const int kLowAverageStartHz = 200;
const int kLowAverageEndHz = 400;

const int kHighAverageStartHz = 6000;
const int kHighAverageEndHz = 6500;

// Quantile of mask values which is used to estimate target presence.
const float kMaskQuantile = 0.3f;
// Mask threshold over which the data is considered signal and not interference.
const float kMaskTargetThreshold = 0.3f;
// Time in seconds after which the data is considered interference if the mask
// does not pass |kMaskTargetThreshold|.
const float kHoldTargetSeconds = 0.25f;

// Does conjugate(|norm_mat|) * |mat| * transpose(|norm_mat|). No extra space is
// used; to accomplish this, we compute both multiplications in the same loop.
// The returned norm is clamped to be non-negative.
float Norm(const ComplexMatrix<float>& mat,
           const ComplexMatrix<float>& norm_mat) {
  CHECK_EQ(norm_mat.num_rows(), 1);
  CHECK_EQ(norm_mat.num_columns(), mat.num_rows());
  CHECK_EQ(norm_mat.num_columns(), mat.num_columns());

  complex<float> first_product = complex<float>(0.f, 0.f);
  complex<float> second_product = complex<float>(0.f, 0.f);

  const complex<float>* const* mat_els = mat.elements();
  const complex<float>* const* norm_mat_els = norm_mat.elements();

  for (int i = 0; i < norm_mat.num_columns(); ++i) {
    for (int j = 0; j < norm_mat.num_columns(); ++j) {
      first_product += conj(norm_mat_els[0][j]) * mat_els[j][i];
    }
    second_product += first_product * norm_mat_els[0][i];
    first_product = 0.f;
  }
  return std::max(second_product.real(), 0.f);
}

// Does conjugate(|lhs|) * |rhs| for row vectors |lhs| and |rhs|.
complex<float> ConjugateDotProduct(const ComplexMatrix<float>& lhs,
                                   const ComplexMatrix<float>& rhs) {
  CHECK_EQ(lhs.num_rows(), 1);
  CHECK_EQ(rhs.num_rows(), 1);
  CHECK_EQ(lhs.num_columns(), rhs.num_columns());

  const complex<float>* const* lhs_elements = lhs.elements();
  const complex<float>* const* rhs_elements = rhs.elements();

  complex<float> result = complex<float>(0.f, 0.f);
  for (int i = 0; i < lhs.num_columns(); ++i) {
    result += conj(lhs_elements[0][i]) * rhs_elements[0][i];
  }

  return result;
}

// Works for positive numbers only.
int Round(float x) {
  return std::floor(x + 0.5f);
}

// Calculates the sum of absolute values of a complex matrix.
float SumAbs(const ComplexMatrix<float>& mat) {
  float sum_abs = 0.f;
  const complex<float>* const* mat_els = mat.elements();
  for (int i = 0; i < mat.num_rows(); ++i) {
    for (int j = 0; j < mat.num_columns(); ++j) {
      sum_abs += std::abs(mat_els[i][j]);
    }
  }
  return sum_abs;
}

// Calculates the sum of squares of a complex matrix.
float SumSquares(const ComplexMatrix<float>& mat) {
  float sum_squares = 0.f;
  const complex<float>* const* mat_els = mat.elements();
  for (int i = 0; i < mat.num_rows(); ++i) {
    for (int j = 0; j < mat.num_columns(); ++j) {
      float abs_value = std::abs(mat_els[i][j]);
      sum_squares += abs_value * abs_value;
    }
  }
  return sum_squares;
}

}  // namespace

Beamformer::Beamformer(const std::vector<Point>& array_geometry)
    : num_input_channels_(array_geometry.size()),
      mic_spacing_(MicSpacingFromGeometry(array_geometry)) {
  WindowGenerator::KaiserBesselDerived(kAlpha, kFftSize, window_);
}

void Beamformer::Initialize(int chunk_size_ms, int sample_rate_hz) {
  chunk_length_ = sample_rate_hz / (1000.f / chunk_size_ms);
  sample_rate_hz_ = sample_rate_hz;
  low_average_start_bin_ =
      Round(kLowAverageStartHz * kFftSize / sample_rate_hz_);
  low_average_end_bin_ =
      Round(kLowAverageEndHz * kFftSize / sample_rate_hz_);
  high_average_start_bin_ =
      Round(kHighAverageStartHz * kFftSize / sample_rate_hz_);
  high_average_end_bin_ =
      Round(kHighAverageEndHz * kFftSize / sample_rate_hz_);
  high_pass_postfilter_mask_ = 1.f;
  is_target_present_ = false;
  hold_target_blocks_ = kHoldTargetSeconds * 2 * sample_rate_hz / kFftSize;
  interference_blocks_count_ = hold_target_blocks_;

  DCHECK_LE(low_average_end_bin_, kNumFreqBins);
  DCHECK_LT(low_average_start_bin_, low_average_end_bin_);
  DCHECK_LE(high_average_end_bin_, kNumFreqBins);
  DCHECK_LT(high_average_start_bin_, high_average_end_bin_);

  lapped_transform_.reset(new LappedTransform(num_input_channels_,
                                              1,
                                              chunk_length_,
                                              window_,
                                              kFftSize,
                                              kFftSize / 2,
                                              this));
  for (int i = 0; i < kNumFreqBins; ++i) {
    postfilter_mask_[i] = 1.f;
    float freq_hz = (static_cast<float>(i) / kFftSize) * sample_rate_hz_;
    wave_numbers_[i] = 2 * M_PI * freq_hz / kSpeedOfSoundMeterSeconds;
    mask_thresholds_[i] = num_input_channels_ * num_input_channels_ *
                          kBeamwidthConstant * wave_numbers_[i] *
                          wave_numbers_[i];
  }

  // Initialize all nonadaptive values before looping through the frames.
  InitDelaySumMasks();
  InitTargetCovMats();
  InitInterfCovMats();

  for (int i = 0; i < kNumFreqBins; ++i) {
    rxiws_[i] = Norm(target_cov_mats_[i], delay_sum_masks_[i]);
    rpsiws_[i] = Norm(interf_cov_mats_[i], delay_sum_masks_[i]);
    reflected_rpsiws_[i] =
        Norm(reflected_interf_cov_mats_[i], delay_sum_masks_[i]);
  }
}

void Beamformer::InitDelaySumMasks() {
  float sin_target = sin(kTargetAngleRadians);
  for (int f_ix = 0; f_ix < kNumFreqBins; ++f_ix) {
    delay_sum_masks_[f_ix].Resize(1, num_input_channels_);
    CovarianceMatrixGenerator::PhaseAlignmentMasks(f_ix,
                                                   kFftSize,
                                                   sample_rate_hz_,
                                                   kSpeedOfSoundMeterSeconds,
                                                   mic_spacing_,
                                                   num_input_channels_,
                                                   sin_target,
                                                   &delay_sum_masks_[f_ix]);

    complex_f norm_factor = sqrt(
        ConjugateDotProduct(delay_sum_masks_[f_ix], delay_sum_masks_[f_ix]));
    delay_sum_masks_[f_ix].Scale(1.f / norm_factor);
    normalized_delay_sum_masks_[f_ix].CopyFrom(delay_sum_masks_[f_ix]);
    normalized_delay_sum_masks_[f_ix].Scale(1.f / SumAbs(
        normalized_delay_sum_masks_[f_ix]));
  }
}

void Beamformer::InitTargetCovMats() {
  target_cov_mats_[0].Resize(num_input_channels_, num_input_channels_);
  CovarianceMatrixGenerator::DCCovarianceMatrix(
      num_input_channels_, kBoxcarHalfWidth, &target_cov_mats_[0]);

  complex_f normalization_factor = target_cov_mats_[0].Trace();
  target_cov_mats_[0].Scale(1.f / normalization_factor);

  for (int i = 1; i < kNumFreqBins; ++i) {
    target_cov_mats_[i].Resize(num_input_channels_, num_input_channels_);
    CovarianceMatrixGenerator::Boxcar(wave_numbers_[i],
                                      num_input_channels_,
                                      mic_spacing_,
                                      kBoxcarHalfWidth,
                                      &target_cov_mats_[i]);

    complex_f normalization_factor = target_cov_mats_[i].Trace();
    target_cov_mats_[i].Scale(1.f / normalization_factor);
  }
}

void Beamformer::InitInterfCovMats() {
  interf_cov_mats_[0].Resize(num_input_channels_, num_input_channels_);
  CovarianceMatrixGenerator::DCCovarianceMatrix(
      num_input_channels_, kCovUniformGapHalfWidth, &interf_cov_mats_[0]);

  complex_f normalization_factor = interf_cov_mats_[0].Trace();
  interf_cov_mats_[0].Scale(1.f / normalization_factor);
  reflected_interf_cov_mats_[0].PointwiseConjugate(interf_cov_mats_[0]);
  for (int i = 1; i < kNumFreqBins; ++i) {
    interf_cov_mats_[i].Resize(num_input_channels_, num_input_channels_);
    ComplexMatrixF uniform_cov_mat(num_input_channels_, num_input_channels_);
    ComplexMatrixF angled_cov_mat(num_input_channels_, num_input_channels_);

    CovarianceMatrixGenerator::GappedUniformCovarianceMatrix(
        wave_numbers_[i],
        num_input_channels_,
        mic_spacing_,
        kCovUniformGapHalfWidth,
        &uniform_cov_mat);

    CovarianceMatrixGenerator::AngledCovarianceMatrix(kSpeedOfSoundMeterSeconds,
                                                      kInterfAngleRadians,
                                                      i,
                                                      kFftSize,
                                                      kNumFreqBins,
                                                      sample_rate_hz_,
                                                      num_input_channels_,
                                                      mic_spacing_,
                                                      &angled_cov_mat);
    // Normalize matrices before averaging them.
    complex_f normalization_factor = uniform_cov_mat.Trace();
    uniform_cov_mat.Scale(1.f / normalization_factor);
    normalization_factor = angled_cov_mat.Trace();
    angled_cov_mat.Scale(1.f / normalization_factor);

    // Average matrices.
    uniform_cov_mat.Scale(1 - kBalance);
    angled_cov_mat.Scale(kBalance);
    interf_cov_mats_[i].Add(uniform_cov_mat, angled_cov_mat);
    reflected_interf_cov_mats_[i].PointwiseConjugate(interf_cov_mats_[i]);
  }
}

void Beamformer::ProcessChunk(const float* const* input,
                              const float* const* high_pass_split_input,
                              int num_input_channels,
                              int num_frames_per_band,
                              float* const* output,
                              float* const* high_pass_split_output) {
  CHECK_EQ(num_input_channels, num_input_channels_);
  CHECK_EQ(num_frames_per_band, chunk_length_);

  float old_high_pass_mask = high_pass_postfilter_mask_;
  lapped_transform_->ProcessChunk(input, output);

  // Apply delay and sum and post-filter in the time domain. WARNING: only works
  // because delay-and-sum is not frequency dependent.
  if (high_pass_split_input != NULL) {
    // Ramp up/down for smoothing. 1 mask per 10ms results in audible
    // discontinuities.
    float ramp_inc =
        (high_pass_postfilter_mask_ - old_high_pass_mask) / num_frames_per_band;
    for (int i = 0; i < num_frames_per_band; ++i) {
      old_high_pass_mask += ramp_inc;

      // Applying the delay and sum (at zero degrees, this is equivalent to
      // averaging).
      float sum = 0.f;
      for (int j = 0; j < num_input_channels; ++j) {
        sum += high_pass_split_input[j][i];
      }
      high_pass_split_output[0][i] =
          sum / num_input_channels * old_high_pass_mask;
    }
  }
}

void Beamformer::ProcessAudioBlock(const complex_f* const* input,
                                   int num_input_channels,
                                   int num_freq_bins,
                                   int num_output_channels,
                                   complex_f* const* output) {
  CHECK_EQ(num_freq_bins, kNumFreqBins);
  CHECK_EQ(num_input_channels, num_input_channels_);
  CHECK_EQ(num_output_channels, 1);

  // Calculating the post-filter masks. Note that we need two for each
  // frequency bin to account for the positive and negative interferer
  // angle.
  for (int i = low_average_start_bin_; i < high_average_end_bin_; ++i) {
    eig_m_.CopyFromColumn(input, i, num_input_channels_);
    float eig_m_norm_factor = std::sqrt(SumSquares(eig_m_));
    if (eig_m_norm_factor != 0.f) {
      eig_m_.Scale(1.f / eig_m_norm_factor);
    }

    float rxim = Norm(target_cov_mats_[i], eig_m_);
    float ratio_rxiw_rxim = 0.f;
    if (rxim > 0.f) {
      ratio_rxiw_rxim = rxiws_[i] / rxim;
    }

    complex_f rmw = abs(ConjugateDotProduct(delay_sum_masks_[i], eig_m_));
    rmw *= rmw;
    float rmw_r = rmw.real();

    new_mask_[i] = CalculatePostfilterMask(interf_cov_mats_[i],
                                           rpsiws_[i],
                                           ratio_rxiw_rxim,
                                           rmw_r,
                                           mask_thresholds_[i]);

    new_mask_[i] *= CalculatePostfilterMask(reflected_interf_cov_mats_[i],
                                            reflected_rpsiws_[i],
                                            ratio_rxiw_rxim,
                                            rmw_r,
                                            mask_thresholds_[i]);
  }

  ApplyMaskSmoothing();
  ApplyLowFrequencyCorrection();
  ApplyHighFrequencyCorrection();
  ApplyMasks(input, output);

  EstimateTargetPresence();
}

float Beamformer::CalculatePostfilterMask(const ComplexMatrixF& interf_cov_mat,
                                          float rpsiw,
                                          float ratio_rxiw_rxim,
                                          float rmw_r,
                                          float mask_threshold) {
  float rpsim = Norm(interf_cov_mat, eig_m_);

  // Find lambda.
  float ratio = 0.f;
  if (rpsim > 0.f) {
    ratio = rpsiw / rpsim;
  }
  float numerator = rmw_r - ratio;
  float denominator = ratio_rxiw_rxim - ratio;

  float mask = 1.f;
  if (denominator > mask_threshold) {
    float lambda = numerator / denominator;
    mask = std::max(lambda * ratio_rxiw_rxim / rmw_r, kMaskMinimum);
  }
  return mask;
}

void Beamformer::ApplyMasks(const complex_f* const* input,
                            complex_f* const* output) {
  complex_f* output_channel = output[0];
  for (int f_ix = 0; f_ix < kNumFreqBins; ++f_ix) {
    output_channel[f_ix] = complex_f(0.f, 0.f);

    const complex_f* delay_sum_mask_els =
        normalized_delay_sum_masks_[f_ix].elements()[0];
    for (int c_ix = 0; c_ix < num_input_channels_; ++c_ix) {
      output_channel[f_ix] += input[c_ix][f_ix] * delay_sum_mask_els[c_ix];
    }

    output_channel[f_ix] *= postfilter_mask_[f_ix];
  }
}

void Beamformer::ApplyMaskSmoothing() {
  for (int i = 0; i < kNumFreqBins; ++i) {
    postfilter_mask_[i] = kMaskSmoothAlpha * new_mask_[i] +
                          (1.f - kMaskSmoothAlpha) * postfilter_mask_[i];
  }
}

void Beamformer::ApplyLowFrequencyCorrection() {
  float low_frequency_mask = 0.f;
  for (int i = low_average_start_bin_; i < low_average_end_bin_; ++i) {
    low_frequency_mask += postfilter_mask_[i];
  }

  low_frequency_mask /= low_average_end_bin_ - low_average_start_bin_;

  for (int i = 0; i < low_average_start_bin_; ++i) {
    postfilter_mask_[i] = low_frequency_mask;
  }
}

void Beamformer::ApplyHighFrequencyCorrection() {
  high_pass_postfilter_mask_ = 0.f;
  for (int i = high_average_start_bin_; i < high_average_end_bin_; ++i) {
    high_pass_postfilter_mask_ += postfilter_mask_[i];
  }

  high_pass_postfilter_mask_ /= high_average_end_bin_ - high_average_start_bin_;

  for (int i = high_average_end_bin_; i < kNumFreqBins; ++i) {
    postfilter_mask_[i] = high_pass_postfilter_mask_;
  }
}

// This method CHECKs for a uniform linear array.
float Beamformer::MicSpacingFromGeometry(const std::vector<Point>& geometry) {
  CHECK_GE(geometry.size(), 2u);
  float mic_spacing = 0.f;
  for (size_t i = 0u; i < 3u; ++i) {
    float difference = geometry[1].c[i] - geometry[0].c[i];
    for (size_t j = 2u; j < geometry.size(); ++j) {
      CHECK_LT(geometry[j].c[i] - geometry[j - 1].c[i] - difference, 1e-6);
    }
    mic_spacing += difference * difference;
  }
  return sqrt(mic_spacing);
}

void Beamformer::EstimateTargetPresence() {
  const int quantile = (1.f - kMaskQuantile) * high_average_end_bin_ +
                       kMaskQuantile * low_average_start_bin_;
  std::nth_element(new_mask_ + low_average_start_bin_,
                   new_mask_ + quantile,
                   new_mask_ + high_average_end_bin_);
  if (new_mask_[quantile] > kMaskTargetThreshold) {
    is_target_present_ = true;
    interference_blocks_count_ = 0;
  } else {
    is_target_present_ = interference_blocks_count_++ < hold_target_blocks_;
  }
}

}  // namespace webrtc

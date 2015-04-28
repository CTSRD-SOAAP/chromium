/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_BEAMFORMER_MOCK_BEAMFORMER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_BEAMFORMER_MOCK_BEAMFORMER_H_

#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "webrtc/modules/audio_processing/beamformer/beamformer.h"

namespace webrtc {

class MockBeamformer : public Beamformer {
 public:
  explicit MockBeamformer(const std::vector<Point>& array_geometry);
  ~MockBeamformer() override;

  MOCK_METHOD2(Initialize, void(int chunk_size_ms, int sample_rate_hz));
  MOCK_METHOD6(ProcessChunk, void(const float* const* input,
                                  const float* const* high_pass_split_input,
                                  int num_input_channels,
                                  int num_frames_per_band,
                                  float* const* output,
                                  float* const* high_pass_split_output));
  MOCK_METHOD0(is_target_present, bool());
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_BEAMFORMER_MOCK_BEAMFORMER_H_

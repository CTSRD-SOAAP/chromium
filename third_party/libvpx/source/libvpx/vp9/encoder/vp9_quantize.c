/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include "vpx_mem/vpx_mem.h"

#include "vp9/encoder/vp9_onyx_int.h"
#include "vp9/encoder/vp9_quantize.h"
#include "vp9/common/vp9_quant_common.h"

#include "vp9/common/vp9_seg_common.h"

#ifdef ENC_DEBUG
extern int enc_debug;
#endif

static INLINE int plane_idx(int plane) {
  return plane == 0 ? 0 :
         plane == 1 ? 16 : 20;
}

static void quantize(int16_t *zbin_boost_orig_ptr,
                     int16_t *coeff_ptr, int n_coeffs, int skip_block,
                     int16_t *zbin_ptr, int16_t *round_ptr, int16_t *quant_ptr,
                     uint8_t *quant_shift_ptr,
                     int16_t *qcoeff_ptr, int16_t *dqcoeff_ptr,
                     int16_t *dequant_ptr, int zbin_oq_value,
                     uint16_t *eob_ptr,
                     const int *scan, int mul) {
  int i, rc, eob;
  int zbins[2], nzbins[2], zbin;
  int x, y, z, sz;
  int zero_run = 0;
  int16_t *zbin_boost_ptr = zbin_boost_orig_ptr;
  int zero_flag = n_coeffs;

  vpx_memset(qcoeff_ptr, 0, n_coeffs*sizeof(int16_t));
  vpx_memset(dqcoeff_ptr, 0, n_coeffs*sizeof(int16_t));

  eob = -1;

  // Base ZBIN
  zbins[0] = zbin_ptr[0] + zbin_oq_value;
  zbins[1] = zbin_ptr[1] + zbin_oq_value;
  nzbins[0] = zbins[0] * -1;
  nzbins[1] = zbins[1] * -1;

  if (!skip_block) {
    // Pre-scan pass
    for (i = n_coeffs - 1; i >= 0; i--) {
      rc = scan[i];
      z = coeff_ptr[rc] * mul;

      if (z < zbins[rc != 0] && z > nzbins[rc != 0]) {
        zero_flag--;
      } else {
        break;
      }
    }

    // Quantization pass: All coefficients with index >= zero_flag are
    // skippable. Note: zero_flag can be zero.
    for (i = 0; i < zero_flag; i++) {
      rc = scan[i];
      z  = coeff_ptr[rc] * mul;

      zbin = (zbins[rc != 0] + zbin_boost_ptr[zero_run]);
      zero_run += (zero_run < 15);

      sz = (z >> 31);                               // sign of z
      x  = (z ^ sz) - sz;

      if (x >= zbin) {
        x += (round_ptr[rc != 0]);
        y  = ((int)(((int)(x * quant_ptr[rc != 0]) >> 16) + x))
            >> quant_shift_ptr[rc != 0];            // quantize (x)
        x  = (y ^ sz) - sz;                         // get the sign back
        qcoeff_ptr[rc]  = x;                        // write to destination
        dqcoeff_ptr[rc] = x * dequant_ptr[rc != 0] / mul;  // dequantized value

        if (y) {
          eob = i;                                  // last nonzero coeffs
          zero_run = 0;                             // set zero_run
        }
      }
    }
  }
  *eob_ptr = eob + 1;
}

// This function works well for large transform size.
static void quantize_sparse(int16_t *zbin_boost_orig_ptr,
                            int16_t *coeff_ptr, int n_coeffs, int skip_block,
                            int16_t *zbin_ptr, int16_t *round_ptr,
                            int16_t *quant_ptr, uint8_t *quant_shift_ptr,
                            int16_t *qcoeff_ptr, int16_t *dqcoeff_ptr,
                            int16_t *dequant_ptr, int zbin_oq_value,
                            uint16_t *eob_ptr, const int *scan, int mul,
                            int *idx_arr) {
  int i, rc, eob;
  int zbins[2], pzbins[2], nzbins[2], zbin;
  int x, y, z, sz;
  int zero_run = 0;
  int16_t *zbin_boost_ptr = zbin_boost_orig_ptr;
  int idx = 0;
  int pre_idx = 0;

  vpx_memset(qcoeff_ptr, 0, n_coeffs*sizeof(int16_t));
  vpx_memset(dqcoeff_ptr, 0, n_coeffs*sizeof(int16_t));

  eob = -1;

  // Base ZBIN
  zbins[0] = zbin_ptr[0] + zbin_oq_value;
  zbins[1] = zbin_ptr[1] + zbin_oq_value;
  // Positive and negative ZBIN
  pzbins[0] = zbins[0]/mul;
  pzbins[1] = zbins[1]/mul;
  nzbins[0] = pzbins[0] * -1;
  nzbins[1] = pzbins[1] * -1;

  if (!skip_block) {
    // Pre-scan pass
    for (i = 0; i < n_coeffs; i++) {
      rc = scan[i];
      z = coeff_ptr[rc];

      // If the coefficient is out of the base ZBIN range, keep it for
      // quantization.
      if (z >= pzbins[rc != 0] || z <= nzbins[rc != 0])
        idx_arr[idx++] = i;
    }

    // Quantization pass: only process the coefficients selected in
    // pre-scan pass. Note: idx can be zero.
    for (i = 0; i < idx; i++) {
      rc = scan[idx_arr[i]];

      // Calculate ZBIN
      zero_run += idx_arr[i] - pre_idx;
      if(zero_run > 15) zero_run = 15;
      zbin = (zbins[rc != 0] + zbin_boost_ptr[zero_run]);

      pre_idx = idx_arr[i];
      z = coeff_ptr[rc] * mul;
      sz = (z >> 31);                               // sign of z
      x  = (z ^ sz) - sz;                           // x = abs(z)

      if (x >= zbin) {
        x += (round_ptr[rc != 0]);
        y  = ((int)(((int)(x * quant_ptr[rc != 0]) >> 16) + x))
            >> quant_shift_ptr[rc != 0];            // quantize (x)

        x  = (y ^ sz) - sz;                         // get the sign back
        qcoeff_ptr[rc]  = x;                        // write to destination
        dqcoeff_ptr[rc] = x * dequant_ptr[rc != 0] / mul;  // dequantized value

        if (y) {
          eob = idx_arr[i];                         // last nonzero coeffs
          zero_run = -1;                            // set zero_run
        }
      }
    }
  }
  *eob_ptr = eob + 1;
}
#if 0
// Original quantize function
static void quantize(int16_t *zbin_boost_orig_ptr,
                     int16_t *coeff_ptr, int n_coeffs, int skip_block,
                     int16_t *zbin_ptr, int16_t *round_ptr, int16_t *quant_ptr,
                     uint8_t *quant_shift_ptr,
                     int16_t *qcoeff_ptr, int16_t *dqcoeff_ptr,
                     int16_t *dequant_ptr, int zbin_oq_value,
                     uint16_t *eob_ptr,
                     const int *scan, int mul) {
  int i, rc, eob;
  int zbin;
  int x, y, z, sz;
  int zero_run = 0;
  int16_t *zbin_boost_ptr = zbin_boost_orig_ptr;

  vpx_memset(qcoeff_ptr, 0, n_coeffs*sizeof(int16_t));
  vpx_memset(dqcoeff_ptr, 0, n_coeffs*sizeof(int16_t));

  eob = -1;

  if (!skip_block) {
    for (i = 0; i < n_coeffs; i++) {
      rc   = scan[i];
      z    = coeff_ptr[rc] * mul;

      zbin = (zbin_ptr[rc != 0] + zbin_boost_ptr[zero_run] + zbin_oq_value);
      zero_run += (zero_run < 15);

      sz = (z >> 31);                               // sign of z
      x  = (z ^ sz) - sz;                           // x = abs(z)

      if (x >= zbin) {
        x += (round_ptr[rc != 0]);
        y  = ((int)(((int)(x * quant_ptr[rc != 0]) >> 16) + x))
            >> quant_shift_ptr[rc != 0];            // quantize (x)
        x  = (y ^ sz) - sz;                         // get the sign back
        qcoeff_ptr[rc]  = x;                        // write to destination
        dqcoeff_ptr[rc] = x * dequant_ptr[rc != 0] / mul;  // dequantized value

        if (y) {
          eob = i;                                  // last nonzero coeffs
          zero_run = 0;
        }
      }
    }
  }

  *eob_ptr = eob + 1;
}
#endif

void vp9_quantize(MACROBLOCK *mb, int plane, int block, int n_coeffs,
                  TX_TYPE tx_type) {
  MACROBLOCKD *const xd = &mb->e_mbd;
  const int mul = n_coeffs == 1024 ? 2 : 1;
  const int *scan;

  // These contexts may be available in the caller
  switch (n_coeffs) {
    case 4 * 4:
      scan = get_scan_4x4(tx_type);
      break;
    case 8 * 8:
      scan = get_scan_8x8(tx_type);
      break;
    case 16 * 16:
      scan = get_scan_16x16(tx_type);
      break;
    default:
      scan = vp9_default_scan_32x32;
      break;
  }

  // Call different quantization for different transform size.
  if (n_coeffs >= 1024) {
    // Save index of picked coefficient in pre-scan pass.
    int idx_arr[1024];

    quantize_sparse(mb->plane[plane].zrun_zbin_boost,
                    BLOCK_OFFSET(mb->plane[plane].coeff, block, 16),
                    n_coeffs, mb->skip_block,
                    mb->plane[plane].zbin,
                    mb->plane[plane].round,
                    mb->plane[plane].quant,
                    mb->plane[plane].quant_shift,
                    BLOCK_OFFSET(xd->plane[plane].qcoeff, block, 16),
                    BLOCK_OFFSET(xd->plane[plane].dqcoeff, block, 16),
                    xd->plane[plane].dequant,
                    mb->plane[plane].zbin_extra,
                    &xd->plane[plane].eobs[block],
                    scan, mul, idx_arr);
  }
  else {
    quantize(mb->plane[plane].zrun_zbin_boost,
             BLOCK_OFFSET(mb->plane[plane].coeff, block, 16),
             n_coeffs, mb->skip_block,
             mb->plane[plane].zbin,
             mb->plane[plane].round,
             mb->plane[plane].quant,
             mb->plane[plane].quant_shift,
             BLOCK_OFFSET(xd->plane[plane].qcoeff, block, 16),
             BLOCK_OFFSET(xd->plane[plane].dqcoeff, block, 16),
             xd->plane[plane].dequant,
             mb->plane[plane].zbin_extra,
             &xd->plane[plane].eobs[block],
             scan, mul);
  }
}

void vp9_regular_quantize_b_4x4(MACROBLOCK *mb, int b_idx, TX_TYPE tx_type,
                                int y_blocks) {
  MACROBLOCKD *const xd = &mb->e_mbd;
  const struct plane_block_idx pb_idx = plane_block_idx(y_blocks, b_idx);
  const int *pt_scan = get_scan_4x4(tx_type);

  quantize(mb->plane[pb_idx.plane].zrun_zbin_boost,
           BLOCK_OFFSET(mb->plane[pb_idx.plane].coeff, pb_idx.block, 16),
           16, mb->skip_block,
           mb->plane[pb_idx.plane].zbin,
           mb->plane[pb_idx.plane].round,
           mb->plane[pb_idx.plane].quant,
           mb->plane[pb_idx.plane].quant_shift,
           BLOCK_OFFSET(xd->plane[pb_idx.plane].qcoeff, pb_idx.block, 16),
           BLOCK_OFFSET(xd->plane[pb_idx.plane].dqcoeff, pb_idx.block, 16),
           xd->plane[pb_idx.plane].dequant,
           mb->plane[pb_idx.plane].zbin_extra,
           &xd->plane[pb_idx.plane].eobs[pb_idx.block],
           pt_scan, 1);
}

static void invert_quant(int16_t *quant, uint8_t *shift, int d) {
  unsigned t;
  int l;
  t = d;
  for (l = 0; t > 1; l++)
    t >>= 1;
  t = 1 + (1 << (16 + l)) / d;
  *quant = (int16_t)(t - (1 << 16));
  *shift = l;
}

void vp9_init_quantizer(VP9_COMP *cpi) {
  int i;
  int quant_val;
  int quant_uv_val;
#if CONFIG_ALPHA
  int quant_alpha_val;
#endif
  int q;

  static const int zbin_boost[16] = { 0,  0,  0,  8,  8,  8, 10, 12,
                                     14, 16, 20, 24, 28, 32, 36, 40 };

  for (q = 0; q < QINDEX_RANGE; q++) {
    int qzbin_factor = (vp9_dc_quant(q, 0) < 148) ? 84 : 80;
    int qrounding_factor = 48;
    if (q == 0) {
      qzbin_factor = 64;
      qrounding_factor = 64;
    }
    // dc values
    quant_val = vp9_dc_quant(q, cpi->common.y_dc_delta_q);
    invert_quant(cpi->y_quant[q] + 0, cpi->y_quant_shift[q] + 0, quant_val);
    cpi->y_zbin[q][0] = ROUND_POWER_OF_TWO(qzbin_factor * quant_val, 7);
    cpi->y_round[q][0] = (qrounding_factor * quant_val) >> 7;
    cpi->common.y_dequant[q][0] = quant_val;
    cpi->zrun_zbin_boost_y[q][0] = (quant_val * zbin_boost[0]) >> 7;

    quant_val = vp9_dc_quant(q, cpi->common.uv_dc_delta_q);
    invert_quant(cpi->uv_quant[q] + 0, cpi->uv_quant_shift[q] + 0, quant_val);
    cpi->uv_zbin[q][0] = ROUND_POWER_OF_TWO(qzbin_factor * quant_val, 7);
    cpi->uv_round[q][0] = (qrounding_factor * quant_val) >> 7;
    cpi->common.uv_dequant[q][0] = quant_val;
    cpi->zrun_zbin_boost_uv[q][0] = (quant_val * zbin_boost[0]) >> 7;

#if CONFIG_ALPHA
    quant_val = vp9_dc_quant(q, cpi->common.a_dc_delta_q);
    invert_quant(cpi->a_quant[q] + 0, cpi->a_quant_shift[q] + 0, quant_val);
    cpi->a_zbin[q][0] = ROUND_POWER_OF_TWO(qzbin_factor * quant_val, 7);
    cpi->a_round[q][0] = (qrounding_factor * quant_val) >> 7;
    cpi->common.a_dequant[q][0] = quant_val;
    cpi->zrun_zbin_boost_a[q][0] = (quant_val * zbin_boost[0]) >> 7;
#endif

    quant_val = vp9_ac_quant(q, 0);
    cpi->common.y_dequant[q][1] = quant_val;
    quant_uv_val = vp9_ac_quant(q, cpi->common.uv_ac_delta_q);
    cpi->common.uv_dequant[q][1] = quant_uv_val;
#if CONFIG_ALPHA
    quant_alpha_val = vp9_ac_quant(q, cpi->common.a_ac_delta_q);
    cpi->common.a_dequant[q][1] = quant_alpha_val;
#endif
    // all the 4x4 ac values =;
    for (i = 1; i < 16; i++) {
      int rc = vp9_default_scan_4x4[i];

      invert_quant(cpi->y_quant[q] + rc, cpi->y_quant_shift[q] + rc, quant_val);
      cpi->y_zbin[q][rc] = ROUND_POWER_OF_TWO(qzbin_factor * quant_val, 7);
      cpi->y_round[q][rc] = (qrounding_factor * quant_val) >> 7;
      cpi->zrun_zbin_boost_y[q][i] =
          ROUND_POWER_OF_TWO(quant_val * zbin_boost[i], 7);

      invert_quant(cpi->uv_quant[q] + rc, cpi->uv_quant_shift[q] + rc,
        quant_uv_val);
      cpi->uv_zbin[q][rc] = ROUND_POWER_OF_TWO(qzbin_factor * quant_uv_val, 7);
      cpi->uv_round[q][rc] = (qrounding_factor * quant_uv_val) >> 7;
      cpi->zrun_zbin_boost_uv[q][i] =
          ROUND_POWER_OF_TWO(quant_uv_val * zbin_boost[i], 7);

#if CONFIG_ALPHA
      invert_quant(cpi->a_quant[q] + rc, cpi->a_quant_shift[q] + rc,
          quant_alpha_val);
      cpi->a_zbin[q][rc] =
          ROUND_POWER_OF_TWO(qzbin_factor * quant_alpha_val, 7);
      cpi->a_round[q][rc] = (qrounding_factor * quant_alpha_val) >> 7;
      cpi->zrun_zbin_boost_a[q][i] =
          ROUND_POWER_OF_TWO(quant_alpha_val * zbin_boost[i], 7);
#endif
    }
  }
}

void vp9_mb_init_quantizer(VP9_COMP *cpi, MACROBLOCK *x) {
  int i;
  MACROBLOCKD *xd = &x->e_mbd;
  int zbin_extra;
  int segment_id = xd->mode_info_context->mbmi.segment_id;
  const int qindex = vp9_get_qindex(xd, segment_id, cpi->common.base_qindex);

  // Y
  zbin_extra = (cpi->common.y_dequant[qindex][1] *
                 (cpi->zbin_mode_boost + x->act_zbin_adj)) >> 7;

  x->plane[0].quant = cpi->y_quant[qindex];
  x->plane[0].quant_shift = cpi->y_quant_shift[qindex];
  x->plane[0].zbin = cpi->y_zbin[qindex];
  x->plane[0].round = cpi->y_round[qindex];
  x->plane[0].zrun_zbin_boost = cpi->zrun_zbin_boost_y[qindex];
  x->plane[0].zbin_extra = (int16_t)zbin_extra;
  x->e_mbd.plane[0].dequant = cpi->common.y_dequant[qindex];

  // UV
  zbin_extra = (cpi->common.uv_dequant[qindex][1] *
                (cpi->zbin_mode_boost + x->act_zbin_adj)) >> 7;

  for (i = 1; i < 3; i++) {
    x->plane[i].quant = cpi->uv_quant[qindex];
    x->plane[i].quant_shift = cpi->uv_quant_shift[qindex];
    x->plane[i].zbin = cpi->uv_zbin[qindex];
    x->plane[i].round = cpi->uv_round[qindex];
    x->plane[i].zrun_zbin_boost = cpi->zrun_zbin_boost_uv[qindex];
    x->plane[i].zbin_extra = (int16_t)zbin_extra;
    x->e_mbd.plane[i].dequant = cpi->common.uv_dequant[qindex];
  }

#if CONFIG_ALPHA
  x->plane[3].quant = cpi->a_quant[qindex];
  x->plane[3].quant_shift = cpi->a_quant_shift[qindex];
  x->plane[3].zbin = cpi->a_zbin[qindex];
  x->plane[3].round = cpi->a_round[qindex];
  x->plane[3].zrun_zbin_boost = cpi->zrun_zbin_boost_a[qindex];
  x->plane[3].zbin_extra = (int16_t)zbin_extra;
  x->e_mbd.plane[3].dequant = cpi->common.a_dequant[qindex];
#endif

  x->skip_block = vp9_segfeature_active(xd, segment_id, SEG_LVL_SKIP);

  /* save this macroblock QIndex for vp9_update_zbin_extra() */
  x->e_mbd.q_index = qindex;
}

void vp9_update_zbin_extra(VP9_COMP *cpi, MACROBLOCK *x) {
  const int qindex = x->e_mbd.q_index;
  const int y_zbin_extra = (cpi->common.y_dequant[qindex][1] *
                (cpi->zbin_mode_boost + x->act_zbin_adj)) >> 7;
  const int uv_zbin_extra = (cpi->common.uv_dequant[qindex][1] *
                  (cpi->zbin_mode_boost + x->act_zbin_adj)) >> 7;

  x->plane[0].zbin_extra = (int16_t)y_zbin_extra;
  x->plane[1].zbin_extra = (int16_t)uv_zbin_extra;
  x->plane[2].zbin_extra = (int16_t)uv_zbin_extra;
}

void vp9_frame_init_quantizer(VP9_COMP *cpi) {
  // Clear Zbin mode boost for default case
  cpi->zbin_mode_boost = 0;

  // MB level quantizer setup
  vp9_mb_init_quantizer(cpi, &cpi->mb);
}

void vp9_set_quantizer(struct VP9_COMP *cpi, int Q) {
  VP9_COMMON *cm = &cpi->common;

  cm->base_qindex = Q;

  // if any of the delta_q values are changing update flag will
  // have to be set.
  cm->y_dc_delta_q = 0;
  cm->uv_dc_delta_q = 0;
  cm->uv_ac_delta_q = 0;

  // quantizer has to be reinitialized if any delta_q changes.
  // As there are not any here for now this is inactive code.
  // if(update)
  //    vp9_init_quantizer(cpi);
}

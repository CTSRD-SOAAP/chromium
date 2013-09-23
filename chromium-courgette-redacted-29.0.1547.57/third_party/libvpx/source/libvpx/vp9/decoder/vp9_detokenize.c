/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vpx_mem/vpx_mem.h"
#include "vpx_ports/mem.h"

#include "vp9/common/vp9_blockd.h"
#include "vp9/common/vp9_common.h"
#include "vp9/common/vp9_seg_common.h"

#include "vp9/decoder/vp9_detokenize.h"
#include "vp9/decoder/vp9_onyxd_int.h"

#if CONFIG_BALANCED_COEFTREE
#define ZERO_CONTEXT_NODE           0
#define EOB_CONTEXT_NODE            1
#else
#define EOB_CONTEXT_NODE            0
#define ZERO_CONTEXT_NODE           1
#endif

#define ONE_CONTEXT_NODE            2
#define LOW_VAL_CONTEXT_NODE        3
#define TWO_CONTEXT_NODE            4
#define THREE_CONTEXT_NODE          5
#define HIGH_LOW_CONTEXT_NODE       6
#define CAT_ONE_CONTEXT_NODE        7
#define CAT_THREEFOUR_CONTEXT_NODE  8
#define CAT_THREE_CONTEXT_NODE      9
#define CAT_FIVE_CONTEXT_NODE       10

#define CAT1_MIN_VAL    5
#define CAT2_MIN_VAL    7
#define CAT3_MIN_VAL   11
#define CAT4_MIN_VAL   19
#define CAT5_MIN_VAL   35
#define CAT6_MIN_VAL   67
#define CAT1_PROB0    159
#define CAT2_PROB0    145
#define CAT2_PROB1    165

#define CAT3_PROB0 140
#define CAT3_PROB1 148
#define CAT3_PROB2 173

#define CAT4_PROB0 135
#define CAT4_PROB1 140
#define CAT4_PROB2 155
#define CAT4_PROB3 176

#define CAT5_PROB0 130
#define CAT5_PROB1 134
#define CAT5_PROB2 141
#define CAT5_PROB3 157
#define CAT5_PROB4 180

static const vp9_prob cat6_prob[15] = {
  254, 254, 254, 252, 249, 243, 230, 196, 177, 153, 140, 133, 130, 129, 0
};

DECLARE_ALIGNED(16, extern const uint8_t,
                vp9_pt_energy_class[MAX_ENTROPY_TOKENS]);
#define INCREMENT_COUNT(token)               \
  do {                                       \
    coef_counts[type][ref][band][pt]         \
               [token >= TWO_TOKEN ?     \
                (token == DCT_EOB_TOKEN ? DCT_EOB_MODEL_TOKEN : TWO_TOKEN) : \
                token]++;     \
    token_cache[scan[c]] = vp9_pt_energy_class[token]; \
  } while (0)

#define WRITE_COEF_CONTINUE(val, token)                  \
  {                                                      \
    qcoeff_ptr[scan[c]] = vp9_read_and_apply_sign(r, val) * \
                            dq[c > 0] / (1 + (txfm_size == TX_32X32)); \
    INCREMENT_COUNT(token);                              \
    c++;                                                 \
    continue;                                            \
  }

#define ADJUST_COEF(prob, bits_count)  \
  do {                                 \
    if (vp9_read(r, prob))             \
      val += 1 << bits_count;          \
  } while (0);

static int decode_coefs(FRAME_CONTEXT *fc, const MACROBLOCKD *xd,
                        vp9_reader *r, int block_idx,
                        PLANE_TYPE type, int seg_eob, int16_t *qcoeff_ptr,
                        TX_SIZE txfm_size, const int16_t *dq,
                        ENTROPY_CONTEXT *A, ENTROPY_CONTEXT *L) {
  ENTROPY_CONTEXT above_ec, left_ec;
  int pt, c = 0, pad, default_eob;
  int band;
  vp9_prob (*coef_probs)[PREV_COEF_CONTEXTS][UNCONSTRAINED_NODES];
  vp9_prob coef_probs_full[COEF_BANDS][PREV_COEF_CONTEXTS][ENTROPY_NODES];
  uint8_t load_map[COEF_BANDS][PREV_COEF_CONTEXTS] = {
    {0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0},
  };

  vp9_prob *prob;
  vp9_coeff_count_model *coef_counts;
  const int ref = xd->mode_info_context->mbmi.ref_frame[0] != INTRA_FRAME;
  TX_TYPE tx_type = DCT_DCT;
  const int *scan, *nb;
  uint8_t token_cache[1024];
  const uint8_t * band_translate;
#if CONFIG_BALANCED_COEFTREE
  int skip_eob_node = 0;
#endif

  coef_probs  = fc->coef_probs[txfm_size][type][ref];
  coef_counts = fc->coef_counts[txfm_size];
  switch (txfm_size) {
    default:
    case TX_4X4: {
      tx_type = (type == PLANE_TYPE_Y_WITH_DC) ?
          get_tx_type_4x4(xd, block_idx) : DCT_DCT;
      scan = get_scan_4x4(tx_type);
      above_ec = A[0] != 0;
      left_ec = L[0] != 0;
      default_eob = 16;
      band_translate = vp9_coefband_trans_4x4;
      break;
    }
    case TX_8X8: {
      const BLOCK_SIZE_TYPE sb_type = xd->mode_info_context->mbmi.sb_type;
      const int sz = 1 + b_width_log2(sb_type);
      const int x = block_idx & ((1 << sz) - 1);
      const int y = block_idx - x;
      tx_type = (type == PLANE_TYPE_Y_WITH_DC) ?
          get_tx_type_8x8(xd, y + (x >> 1)) : DCT_DCT;
      scan = get_scan_8x8(tx_type);
      above_ec = (A[0] + A[1]) != 0;
      left_ec = (L[0] + L[1]) != 0;
      default_eob = 64;
      band_translate = vp9_coefband_trans_8x8plus;
      break;
    }
    case TX_16X16: {
      const BLOCK_SIZE_TYPE sb_type = xd->mode_info_context->mbmi.sb_type;
      const int sz = 2 + b_width_log2(sb_type);
      const int x = block_idx & ((1 << sz) - 1);
      const int y = block_idx - x;
      tx_type = (type == PLANE_TYPE_Y_WITH_DC) ?
          get_tx_type_16x16(xd, y + (x >> 2)) : DCT_DCT;
      scan = get_scan_16x16(tx_type);
      above_ec = (A[0] + A[1] + A[2] + A[3]) != 0;
      left_ec = (L[0] + L[1] + L[2] + L[3]) != 0;
      default_eob = 256;
      band_translate = vp9_coefband_trans_8x8plus;
      break;
    }
    case TX_32X32:
      scan = vp9_default_scan_32x32;
      above_ec = (A[0] + A[1] + A[2] + A[3] + A[4] + A[5] + A[6] + A[7]) != 0;
      left_ec = (L[0] + L[1] + L[2] + L[3] + L[4] + L[5] + L[6] + L[7]) != 0;
      default_eob = 1024;
      band_translate = vp9_coefband_trans_8x8plus;
      break;
  }

  pt = combine_entropy_contexts(above_ec, left_ec);
  nb = vp9_get_coef_neighbors_handle(scan, &pad);

  while (1) {
    int val;
    const uint8_t *cat6 = cat6_prob;
    if (c >= seg_eob)
      break;
    if (c)
      pt = vp9_get_coef_context(scan, nb, pad, token_cache,
                                c, default_eob);
    band = get_coef_band(band_translate, c);
    prob = coef_probs[band][pt];
#if !CONFIG_BALANCED_COEFTREE
    fc->eob_branch_counts[txfm_size][type][ref][band][pt]++;
    if (!vp9_read(r, prob[EOB_CONTEXT_NODE]))
      break;

SKIP_START:
#endif
    if (c >= seg_eob)
      break;
    if (c)
      pt = vp9_get_coef_context(scan, nb, pad, token_cache,
                                c, default_eob);
    band = get_coef_band(band_translate, c);
    prob = coef_probs[band][pt];

    if (!vp9_read(r, prob[ZERO_CONTEXT_NODE])) {
      INCREMENT_COUNT(ZERO_TOKEN);
      ++c;
#if CONFIG_BALANCED_COEFTREE
      skip_eob_node = 1;
      continue;
#else
      goto SKIP_START;
#endif
    }
#if CONFIG_BALANCED_COEFTREE
    if (!skip_eob_node) {
      fc->eob_branch_counts[txfm_size][type][ref][band][pt]++;
      if (!vp9_read(r, prob[EOB_CONTEXT_NODE]))
        break;
    }
    skip_eob_node = 0;
#endif

    // ONE_CONTEXT_NODE_0_
    if (!vp9_read(r, prob[ONE_CONTEXT_NODE])) {
      WRITE_COEF_CONTINUE(1, ONE_TOKEN);
    }
    // Load full probabilities if not already loaded
    if (!load_map[band][pt]) {
      vp9_model_to_full_probs(coef_probs[band][pt],
                              coef_probs_full[band][pt]);
      load_map[band][pt] = 1;
    }
    prob = coef_probs_full[band][pt];
    // LOW_VAL_CONTEXT_NODE_0_
    if (!vp9_read(r, prob[LOW_VAL_CONTEXT_NODE])) {
      if (!vp9_read(r, prob[TWO_CONTEXT_NODE])) {
        WRITE_COEF_CONTINUE(2, TWO_TOKEN);
      }
      if (!vp9_read(r, prob[THREE_CONTEXT_NODE])) {
        WRITE_COEF_CONTINUE(3, THREE_TOKEN);
      }
      WRITE_COEF_CONTINUE(4, FOUR_TOKEN);
    }
    // HIGH_LOW_CONTEXT_NODE_0_
    if (!vp9_read(r, prob[HIGH_LOW_CONTEXT_NODE])) {
      if (!vp9_read(r, prob[CAT_ONE_CONTEXT_NODE])) {
        val = CAT1_MIN_VAL;
        ADJUST_COEF(CAT1_PROB0, 0);
        WRITE_COEF_CONTINUE(val, DCT_VAL_CATEGORY1);
      }
      val = CAT2_MIN_VAL;
      ADJUST_COEF(CAT2_PROB1, 1);
      ADJUST_COEF(CAT2_PROB0, 0);
      WRITE_COEF_CONTINUE(val, DCT_VAL_CATEGORY2);
    }
    // CAT_THREEFOUR_CONTEXT_NODE_0_
    if (!vp9_read(r, prob[CAT_THREEFOUR_CONTEXT_NODE])) {
      if (!vp9_read(r, prob[CAT_THREE_CONTEXT_NODE])) {
        val = CAT3_MIN_VAL;
        ADJUST_COEF(CAT3_PROB2, 2);
        ADJUST_COEF(CAT3_PROB1, 1);
        ADJUST_COEF(CAT3_PROB0, 0);
        WRITE_COEF_CONTINUE(val, DCT_VAL_CATEGORY3);
      }
      val = CAT4_MIN_VAL;
      ADJUST_COEF(CAT4_PROB3, 3);
      ADJUST_COEF(CAT4_PROB2, 2);
      ADJUST_COEF(CAT4_PROB1, 1);
      ADJUST_COEF(CAT4_PROB0, 0);
      WRITE_COEF_CONTINUE(val, DCT_VAL_CATEGORY4);
    }
    // CAT_FIVE_CONTEXT_NODE_0_:
    if (!vp9_read(r, prob[CAT_FIVE_CONTEXT_NODE])) {
      val = CAT5_MIN_VAL;
      ADJUST_COEF(CAT5_PROB4, 4);
      ADJUST_COEF(CAT5_PROB3, 3);
      ADJUST_COEF(CAT5_PROB2, 2);
      ADJUST_COEF(CAT5_PROB1, 1);
      ADJUST_COEF(CAT5_PROB0, 0);
      WRITE_COEF_CONTINUE(val, DCT_VAL_CATEGORY5);
    }
    val = 0;
    while (*cat6) {
      val = (val << 1) | vp9_read(r, *cat6++);
    }
    val += CAT6_MIN_VAL;
    WRITE_COEF_CONTINUE(val, DCT_VAL_CATEGORY6);
  }

  if (c < seg_eob)
    coef_counts[type][ref][band][pt][DCT_EOB_MODEL_TOKEN]++;


  return c;
}

static int get_eob(MACROBLOCKD* const xd, int segment_id, int eob_max) {
  return vp9_segfeature_active(xd, segment_id, SEG_LVL_SKIP) ? 0 : eob_max;
}

struct decode_block_args {
  VP9D_COMP *pbi;
  vp9_reader *r;
  int *eobtotal;
};

static void decode_block(int plane, int block,
                         BLOCK_SIZE_TYPE bsize,
                         int ss_txfrm_size,
                         void *argv) {
  const struct decode_block_args* const arg = argv;
  const int bw = b_width_log2(bsize);

  // find the maximum eob for this transform size, adjusted by segment
  MACROBLOCKD *xd = &arg->pbi->mb;
  struct macroblockd_plane* pd = &xd->plane[plane];
  const int segment_id = xd->mode_info_context->mbmi.segment_id;
  const TX_SIZE ss_tx_size = ss_txfrm_size / 2;
  const int seg_eob = get_eob(xd, segment_id, 16 << ss_txfrm_size);
  const int off = block >> ss_txfrm_size;
  const int mod = bw - ss_tx_size - pd->subsampling_x;
  const int aoff = (off & ((1 << mod) - 1)) << ss_tx_size;
  const int loff = (off >> mod) << ss_tx_size;

  ENTROPY_CONTEXT *A = pd->above_context + aoff;
  ENTROPY_CONTEXT *L = pd->left_context + loff;
  const int eob = decode_coefs(&arg->pbi->common.fc, xd, arg->r, block,
                               pd->plane_type, seg_eob,
                               BLOCK_OFFSET(pd->qcoeff, block, 16),
                               ss_tx_size, pd->dequant, A, L);

  if (xd->mb_to_right_edge < 0 || xd->mb_to_bottom_edge < 0) {
    set_contexts_on_border(xd, bsize, plane, ss_tx_size, eob, aoff, loff, A, L);
  } else {
    int pt;
    for (pt = 0; pt < (1 << ss_tx_size); pt++)
      A[pt] = L[pt] = eob > 0;
  }
  pd->eobs[block] = eob;
  *arg->eobtotal += eob;
}

int vp9_decode_tokens(VP9D_COMP *pbi, vp9_reader *r, BLOCK_SIZE_TYPE bsize) {
  int eobtotal = 0;
  struct decode_block_args args = {pbi, r, &eobtotal};
  foreach_transformed_block(&pbi->mb, bsize, decode_block, &args);
  return eobtotal;
}

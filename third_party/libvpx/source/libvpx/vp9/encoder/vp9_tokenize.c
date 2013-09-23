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
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "vp9/encoder/vp9_onyx_int.h"
#include "vp9/encoder/vp9_tokenize.h"
#include "vpx_mem/vpx_mem.h"

#include "vp9/common/vp9_pred_common.h"
#include "vp9/common/vp9_seg_common.h"
#include "vp9/common/vp9_entropy.h"

/* Global event counters used for accumulating statistics across several
   compressions, then generating vp9_context.c = initial stats. */

#ifdef ENTROPY_STATS
vp9_coeff_accum context_counters[TX_SIZE_MAX_SB][BLOCK_TYPES];
extern vp9_coeff_stats tree_update_hist[TX_SIZE_MAX_SB][BLOCK_TYPES];
#endif  /* ENTROPY_STATS */

DECLARE_ALIGNED(16, extern const uint8_t,
                vp9_pt_energy_class[MAX_ENTROPY_TOKENS]);

static TOKENVALUE dct_value_tokens[DCT_MAX_VALUE * 2];
const TOKENVALUE *vp9_dct_value_tokens_ptr;
static int dct_value_cost[DCT_MAX_VALUE * 2];
const int *vp9_dct_value_cost_ptr;

static void fill_value_tokens() {

  TOKENVALUE *const t = dct_value_tokens + DCT_MAX_VALUE;
  vp9_extra_bit *const e = vp9_extra_bits;

  int i = -DCT_MAX_VALUE;
  int sign = 1;

  do {
    if (!i)
      sign = 0;

    {
      const int a = sign ? -i : i;
      int eb = sign;

      if (a > 4) {
        int j = 4;

        while (++j < 11  &&  e[j].base_val <= a) {}

        t[i].token = --j;
        eb |= (a - e[j].base_val) << 1;
      } else
        t[i].token = a;

      t[i].extra = eb;
    }

    // initialize the cost for extra bits for all possible coefficient value.
    {
      int cost = 0;
      vp9_extra_bit *p = vp9_extra_bits + t[i].token;

      if (p->base_val) {
        const int extra = t[i].extra;
        const int length = p->len;

        if (length)
          cost += treed_cost(p->tree, p->prob, extra >> 1, length);

        cost += vp9_cost_bit(vp9_prob_half, extra & 1); /* sign */
        dct_value_cost[i + DCT_MAX_VALUE] = cost;
      }

    }

  } while (++i < DCT_MAX_VALUE);

  vp9_dct_value_tokens_ptr = dct_value_tokens + DCT_MAX_VALUE;
  vp9_dct_value_cost_ptr   = dct_value_cost + DCT_MAX_VALUE;
}

extern const int *vp9_get_coef_neighbors_handle(const int *scan, int *pad);

struct tokenize_b_args {
  VP9_COMP *cpi;
  MACROBLOCKD *xd;
  TOKENEXTRA **tp;
  TX_SIZE tx_size;
  int dry_run;
};

static void tokenize_b(int plane, int block, BLOCK_SIZE_TYPE bsize,
                       int ss_txfrm_size, void *arg) {
  struct tokenize_b_args* const args = arg;
  VP9_COMP *cpi = args->cpi;
  MACROBLOCKD *xd = args->xd;
  TOKENEXTRA **tp = args->tp;
  PLANE_TYPE type = plane ? PLANE_TYPE_UV : PLANE_TYPE_Y_WITH_DC;
  TX_SIZE tx_size = ss_txfrm_size / 2;
  int dry_run = args->dry_run;

  MB_MODE_INFO *mbmi = &xd->mode_info_context->mbmi;
  int pt; /* near block/prev token context index */
  int c = 0, rc = 0;
  TOKENEXTRA *t = *tp;        /* store tokens starting here */
  const int eob = xd->plane[plane].eobs[block];
  const int16_t *qcoeff_ptr = BLOCK_OFFSET(xd->plane[plane].qcoeff, block, 16);
  const BLOCK_SIZE_TYPE sb_type = (mbmi->sb_type < BLOCK_SIZE_SB8X8) ?
                                   BLOCK_SIZE_SB8X8 : mbmi->sb_type;
  const int bwl = b_width_log2(sb_type);
  const int off = block >> (2 * tx_size);
  const int mod = bwl - tx_size - xd->plane[plane].subsampling_x;
  const int aoff = (off & ((1 << mod) - 1)) << tx_size;
  const int loff = (off >> mod) << tx_size;
  ENTROPY_CONTEXT *A = xd->plane[plane].above_context + aoff;
  ENTROPY_CONTEXT *L = xd->plane[plane].left_context + loff;
  int seg_eob, default_eob, pad;
  const int segment_id = mbmi->segment_id;
  const int *scan, *nb;
  vp9_coeff_count *counts;
  vp9_coeff_probs_model *coef_probs;
  const int ref = mbmi->ref_frame[0] != INTRA_FRAME;
  ENTROPY_CONTEXT above_ec, left_ec;
  uint8_t token_cache[1024];
  TX_TYPE tx_type = DCT_DCT;
  const uint8_t * band_translate;
  assert((!type && !plane) || (type && plane));

  counts = cpi->coef_counts[tx_size];
  coef_probs = cpi->common.fc.coef_probs[tx_size];
  switch (tx_size) {
    default:
    case TX_4X4: {
      tx_type = (type == PLANE_TYPE_Y_WITH_DC) ?
          get_tx_type_4x4(xd, block) : DCT_DCT;
      above_ec = A[0] != 0;
      left_ec = L[0] != 0;
      seg_eob = 16;
      scan = get_scan_4x4(tx_type);
      band_translate = vp9_coefband_trans_4x4;
      break;
    }
    case TX_8X8: {
      const int sz = 1 + b_width_log2(sb_type);
      const int x = block & ((1 << sz) - 1), y = block - x;
      tx_type = (type == PLANE_TYPE_Y_WITH_DC) ?
          get_tx_type_8x8(xd, y + (x >> 1)) : DCT_DCT;
      above_ec = (A[0] + A[1]) != 0;
      left_ec = (L[0] + L[1]) != 0;
      seg_eob = 64;
      scan = get_scan_8x8(tx_type);
      band_translate = vp9_coefband_trans_8x8plus;
      break;
    }
    case TX_16X16: {
      const int sz = 2 + b_width_log2(sb_type);
      const int x = block & ((1 << sz) - 1), y = block - x;
      tx_type = (type == PLANE_TYPE_Y_WITH_DC) ?
          get_tx_type_16x16(xd, y + (x >> 2)) : DCT_DCT;
      above_ec = (A[0] + A[1] + A[2] + A[3]) != 0;
      left_ec = (L[0] + L[1] + L[2] + L[3]) != 0;
      seg_eob = 256;
      scan = get_scan_16x16(tx_type);
      band_translate = vp9_coefband_trans_8x8plus;
      break;
    }
    case TX_32X32:
      above_ec = (A[0] + A[1] + A[2] + A[3] + A[4] + A[5] + A[6] + A[7]) != 0;
      left_ec = (L[0] + L[1] + L[2] + L[3] + L[4] + L[5] + L[6] + L[7]) != 0;
      seg_eob = 1024;
      scan = vp9_default_scan_32x32;
      band_translate = vp9_coefband_trans_8x8plus;
      break;
  }

  pt = combine_entropy_contexts(above_ec, left_ec);
  nb = vp9_get_coef_neighbors_handle(scan, &pad);
  default_eob = seg_eob;

  if (vp9_segfeature_active(xd, segment_id, SEG_LVL_SKIP))
    seg_eob = 0;

  c = 0;
  do {
    const int band = get_coef_band(band_translate, c);
    int token;
    int v = 0;
    rc = scan[c];
    if (c)
      pt = vp9_get_coef_context(scan, nb, pad, token_cache, c, default_eob);
    if (c < eob) {
      v = qcoeff_ptr[rc];
      assert(-DCT_MAX_VALUE <= v  &&  v < DCT_MAX_VALUE);

      t->extra = vp9_dct_value_tokens_ptr[v].extra;
      token    = vp9_dct_value_tokens_ptr[v].token;
    } else {
      token = DCT_EOB_TOKEN;
    }

    t->token = token;
    t->context_tree = coef_probs[type][ref][band][pt];
    t->skip_eob_node = (c > 0) && (token_cache[scan[c - 1]] == 0);

#if CONFIG_BALANCED_COEFTREE
    assert(token <= ZERO_TOKEN ||
           vp9_coef_encodings[t->token].len - t->skip_eob_node > 0);
#else
    assert(vp9_coef_encodings[t->token].len - t->skip_eob_node > 0);
#endif

    if (!dry_run) {
      ++counts[type][ref][band][pt][token];
#if CONFIG_BALANCED_COEFTREE
      if (!t->skip_eob_node && token > ZERO_TOKEN)
#else
      if (!t->skip_eob_node)
#endif
        ++cpi->common.fc.eob_branch_counts[tx_size][type][ref][band][pt];
    }
    token_cache[scan[c]] = vp9_pt_energy_class[token];
    ++t;
  } while (c < eob && ++c < seg_eob);

  *tp = t;
  if (xd->mb_to_right_edge < 0 || xd->mb_to_bottom_edge < 0) {
    set_contexts_on_border(xd, bsize, plane, tx_size, c, aoff, loff, A, L);
  } else {
    for (pt = 0; pt < (1 << tx_size); pt++) {
      A[pt] = L[pt] = c > 0;
    }
  }
}

struct is_skippable_args {
  MACROBLOCKD *xd;
  int *skippable;
};
static void is_skippable(int plane, int block,
                         BLOCK_SIZE_TYPE bsize, int ss_txfrm_size, void *argv) {
  struct is_skippable_args *args = argv;
  args->skippable[0] &= (!args->xd->plane[plane].eobs[block]);
}

int vp9_sb_is_skippable(MACROBLOCKD *xd, BLOCK_SIZE_TYPE bsize) {
  int result = 1;
  struct is_skippable_args args = {xd, &result};
  foreach_transformed_block(xd, bsize, is_skippable, &args);
  return result;
}

int vp9_sby_is_skippable(MACROBLOCKD *xd, BLOCK_SIZE_TYPE bsize) {
  int result = 1;
  struct is_skippable_args args = {xd, &result};
  foreach_transformed_block_in_plane(xd, bsize, 0,
                                     is_skippable, &args);
  return result;
}

int vp9_sbuv_is_skippable(MACROBLOCKD *xd, BLOCK_SIZE_TYPE bsize) {
  int result = 1;
  struct is_skippable_args args = {xd, &result};
  foreach_transformed_block_uv(xd, bsize, is_skippable, &args);
  return result;
}

void vp9_tokenize_sb(VP9_COMP *cpi,
                     MACROBLOCKD *xd,
                     TOKENEXTRA **t,
                     int dry_run, BLOCK_SIZE_TYPE bsize) {
  VP9_COMMON * const cm = &cpi->common;
  MB_MODE_INFO * const mbmi = &xd->mode_info_context->mbmi;
  TOKENEXTRA *t_backup = *t;
  const int mb_skip_context = vp9_get_pred_context(cm, xd, PRED_MBSKIP);
  const int segment_id = mbmi->segment_id;
  const int skip_inc = !vp9_segfeature_active(xd, segment_id, SEG_LVL_SKIP);
  const TX_SIZE txfm_size = mbmi->txfm_size;
  struct tokenize_b_args arg = {
    cpi, xd, t, txfm_size, dry_run
  };

  mbmi->mb_skip_coeff = vp9_sb_is_skippable(xd, bsize);

  if (mbmi->mb_skip_coeff) {
    if (!dry_run)
      cm->fc.mbskip_count[mb_skip_context][1] += skip_inc;
    vp9_reset_sb_tokens_context(xd, bsize);
    if (dry_run)
      *t = t_backup;
    return;
  }

  if (!dry_run)
    cm->fc.mbskip_count[mb_skip_context][0] += skip_inc;

  foreach_transformed_block(xd, bsize, tokenize_b, &arg);

  if (dry_run)
    *t = t_backup;
}

#ifdef ENTROPY_STATS
void init_context_counters(void) {
  FILE *f = fopen("context.bin", "rb");
  if (!f) {
    vp9_zero(context_counters);
  } else {
    fread(context_counters, sizeof(context_counters), 1, f);
    fclose(f);
  }

  f = fopen("treeupdate.bin", "rb");
  if (!f) {
    vpx_memset(tree_update_hist, 0, sizeof(tree_update_hist));
  } else {
    fread(tree_update_hist, sizeof(tree_update_hist), 1, f);
    fclose(f);
  }
}

static void print_counter(FILE *f, vp9_coeff_accum *context_counters,
                          int block_types, const char *header) {
  int type, ref, band, pt, t;

  fprintf(f, "static const vp9_coeff_count %s = {\n", header);

#define Comma(X) (X ? "," : "")
  type = 0;
  do {
    ref = 0;
    fprintf(f, "%s\n  { /* block Type %d */", Comma(type), type);
    do {
      fprintf(f, "%s\n    { /* %s */", Comma(type), ref ? "Inter" : "Intra");
      band = 0;
      do {
        fprintf(f, "%s\n      { /* Coeff Band %d */", Comma(band), band);
        pt = 0;
        do {
          fprintf(f, "%s\n        {", Comma(pt));

          t = 0;
          do {
            const int64_t x = context_counters[type][ref][band][pt][t];
            const int y = (int) x;

            assert(x == (int64_t) y);  /* no overflow handling yet */
            fprintf(f, "%s %d", Comma(t), y);
          } while (++t < 1 + MAX_ENTROPY_TOKENS);
          fprintf(f, "}");
        } while (++pt < PREV_COEF_CONTEXTS);
        fprintf(f, "\n      }");
      } while (++band < COEF_BANDS);
      fprintf(f, "\n    }");
    } while (++ref < REF_TYPES);
    fprintf(f, "\n  }");
  } while (++type < block_types);
  fprintf(f, "\n};\n");
}

static void print_probs(FILE *f, vp9_coeff_accum *context_counters,
                        int block_types, const char *header) {
  int type, ref, band, pt, t;

  fprintf(f, "static const vp9_coeff_probs %s = {", header);

  type = 0;
#define Newline(x, spaces) (x ? " " : "\n" spaces)
  do {
    fprintf(f, "%s%s{ /* block Type %d */",
            Comma(type), Newline(type, "  "), type);
    ref = 0;
    do {
      fprintf(f, "%s%s{ /* %s */",
              Comma(band), Newline(band, "    "), ref ? "Inter" : "Intra");
      band = 0;
      do {
        fprintf(f, "%s%s{ /* Coeff Band %d */",
                Comma(band), Newline(band, "      "), band);
        pt = 0;
        do {
          unsigned int branch_ct[ENTROPY_NODES][2];
          unsigned int coef_counts[MAX_ENTROPY_TOKENS + 1];
          vp9_prob coef_probs[ENTROPY_NODES];

          if (pt >= 3 && band == 0)
            break;
          for (t = 0; t < MAX_ENTROPY_TOKENS + 1; ++t)
            coef_counts[t] = context_counters[type][ref][band][pt][t];
          vp9_tree_probs_from_distribution(vp9_coef_tree, coef_probs,
                                           branch_ct, coef_counts, 0);
          branch_ct[0][1] = coef_counts[MAX_ENTROPY_TOKENS] - branch_ct[0][0];
          coef_probs[0] = get_binary_prob(branch_ct[0][0], branch_ct[0][1]);
          fprintf(f, "%s\n      {", Comma(pt));

          t = 0;
          do {
            fprintf(f, "%s %3d", Comma(t), coef_probs[t]);
          } while (++t < ENTROPY_NODES);

          fprintf(f, " }");
        } while (++pt < PREV_COEF_CONTEXTS);
        fprintf(f, "\n      }");
      } while (++band < COEF_BANDS);
      fprintf(f, "\n    }");
    } while (++ref < REF_TYPES);
    fprintf(f, "\n  }");
  } while (++type < block_types);
  fprintf(f, "\n};\n");
}

void print_context_counters() {
  FILE *f = fopen("vp9_context.c", "w");

  fprintf(f, "#include \"vp9_entropy.h\"\n");
  fprintf(f, "\n/* *** GENERATED FILE: DO NOT EDIT *** */\n\n");

  /* print counts */
  print_counter(f, context_counters[TX_4X4], BLOCK_TYPES,
                "vp9_default_coef_counts_4x4[BLOCK_TYPES]");
  print_counter(f, context_counters[TX_8X8], BLOCK_TYPES,
                "vp9_default_coef_counts_8x8[BLOCK_TYPES]");
  print_counter(f, context_counters[TX_16X16], BLOCK_TYPES,
                "vp9_default_coef_counts_16x16[BLOCK_TYPES]");
  print_counter(f, context_counters[TX_32X32], BLOCK_TYPES,
                "vp9_default_coef_counts_32x32[BLOCK_TYPES]");

  /* print coefficient probabilities */
  print_probs(f, context_counters[TX_4X4], BLOCK_TYPES,
              "default_coef_probs_4x4[BLOCK_TYPES]");
  print_probs(f, context_counters[TX_8X8], BLOCK_TYPES,
              "default_coef_probs_8x8[BLOCK_TYPES]");
  print_probs(f, context_counters[TX_16X16], BLOCK_TYPES,
              "default_coef_probs_16x16[BLOCK_TYPES]");
  print_probs(f, context_counters[TX_32X32], BLOCK_TYPES,
              "default_coef_probs_32x32[BLOCK_TYPES]");

  fclose(f);

  f = fopen("context.bin", "wb");
  fwrite(context_counters, sizeof(context_counters), 1, f);
  fclose(f);
}
#endif

void vp9_tokenize_initialize() {
  fill_value_tokens();
}

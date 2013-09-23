/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP9_COMMON_VP9_FINDNEARMV_H_
#define VP9_COMMON_VP9_FINDNEARMV_H_

#include "vp9/common/vp9_mv.h"
#include "vp9/common/vp9_blockd.h"
#include "vp9/common/vp9_treecoder.h"
#include "vp9/common/vp9_onyxc_int.h"

#define LEFT_TOP_MARGIN     ((VP9BORDERINPIXELS - VP9_INTERP_EXTEND) << 3)
#define RIGHT_BOTTOM_MARGIN ((VP9BORDERINPIXELS - VP9_INTERP_EXTEND) << 3)

// check a list of motion vectors by sad score using a number rows of pixels
// above and a number cols of pixels in the left to select the one with best
// score to use as ref motion vector
void vp9_find_best_ref_mvs(MACROBLOCKD *xd,
                           int_mv *mvlist,
                           int_mv *nearest,
                           int_mv *near);

static void mv_bias(int refmb_ref_frame_sign_bias, int refframe,
                    int_mv *mvp, const int *ref_frame_sign_bias) {
  MV xmv = mvp->as_mv;

  if (refmb_ref_frame_sign_bias != ref_frame_sign_bias[refframe]) {
    xmv.row *= -1;
    xmv.col *= -1;
  }

  mvp->as_mv = xmv;
}

// TODO(jingning): this mv clamping function should be block size dependent.
static void clamp_mv(int_mv *mv,
                     int mb_to_left_edge,
                     int mb_to_right_edge,
                     int mb_to_top_edge,
                     int mb_to_bottom_edge) {
  mv->as_mv.col = clamp(mv->as_mv.col, mb_to_left_edge, mb_to_right_edge);
  mv->as_mv.row = clamp(mv->as_mv.row, mb_to_top_edge, mb_to_bottom_edge);
}

static int clamp_mv2(int_mv *mv, const MACROBLOCKD *xd) {
  int_mv tmp_mv;
  tmp_mv.as_int = mv->as_int;
  clamp_mv(mv,
           xd->mb_to_left_edge - LEFT_TOP_MARGIN,
           xd->mb_to_right_edge + RIGHT_BOTTOM_MARGIN,
           xd->mb_to_top_edge - LEFT_TOP_MARGIN,
           xd->mb_to_bottom_edge + RIGHT_BOTTOM_MARGIN);
  return tmp_mv.as_int != mv->as_int;
}

static int check_mv_bounds(int_mv *mv,
                           int mb_to_left_edge, int mb_to_right_edge,
                           int mb_to_top_edge, int mb_to_bottom_edge) {
  return mv->as_mv.col < mb_to_left_edge ||
         mv->as_mv.col > mb_to_right_edge ||
         mv->as_mv.row < mb_to_top_edge ||
         mv->as_mv.row > mb_to_bottom_edge;
}

void vp9_append_sub8x8_mvs_for_idx(VP9_COMMON *pc,
                                   MACROBLOCKD *xd,
                                   int_mv *dst_nearest,
                                   int_mv *dst_near,
                                   int block_idx, int ref_idx);

static MB_PREDICTION_MODE left_block_mode(const MODE_INFO *cur_mb, int b) {
  // FIXME(rbultje, jingning): temporary hack because jenkins doesn't
  // understand this condition. This will go away soon.
  if (b == 0 || b == 2) {
    /* On L edge, get from MB to left of us */
    --cur_mb;

    if (cur_mb->mbmi.ref_frame[0] != INTRA_FRAME) {
      return DC_PRED;
    } else if (cur_mb->mbmi.sb_type < BLOCK_SIZE_SB8X8) {
      return ((cur_mb->bmi + 1 + b)->as_mode.first);
    } else {
      return cur_mb->mbmi.mode;
    }
  }
  assert(b == 1 || b == 3);
  return (cur_mb->bmi + b - 1)->as_mode.first;
}

static MB_PREDICTION_MODE above_block_mode(const MODE_INFO *cur_mb,
                                          int b, int mi_stride) {
  if (!(b >> 1)) {
    /* On top edge, get from MB above us */
    cur_mb -= mi_stride;

    if (cur_mb->mbmi.ref_frame[0] != INTRA_FRAME) {
      return DC_PRED;
    } else if (cur_mb->mbmi.sb_type < BLOCK_SIZE_SB8X8) {
      return ((cur_mb->bmi + 2 + b)->as_mode.first);
    } else {
      return cur_mb->mbmi.mode;
    }
  }

  return (cur_mb->bmi + b - 2)->as_mode.first;
}

#endif  // VP9_COMMON_VP9_FINDNEARMV_H_

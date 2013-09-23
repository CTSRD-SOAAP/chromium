/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <limits.h>

#include "vp9/common/vp9_findnearmv.h"
#include "vp9/common/vp9_mvref_common.h"
#include "vp9/common/vp9_sadmxn.h"

static void lower_mv_precision(int_mv *mv, int usehp) {
  if (!usehp || !vp9_use_mv_hp(&mv->as_mv)) {
    if (mv->as_mv.row & 1)
      mv->as_mv.row += (mv->as_mv.row > 0 ? -1 : 1);
    if (mv->as_mv.col & 1)
      mv->as_mv.col += (mv->as_mv.col > 0 ? -1 : 1);
  }
}


void vp9_find_best_ref_mvs(MACROBLOCKD *xd,
                           int_mv *mvlist,
                           int_mv *nearest,
                           int_mv *near) {
  int i;
  // Make sure all the candidates are properly clamped etc
  for (i = 0; i < MAX_MV_REF_CANDIDATES; ++i) {
    lower_mv_precision(&mvlist[i], xd->allow_high_precision_mv);
    clamp_mv2(&mvlist[i], xd);
  }
  *nearest = mvlist[0];
  *near = mvlist[1];
}

void vp9_append_sub8x8_mvs_for_idx(VP9_COMMON *cm, MACROBLOCKD *xd,
                                   int_mv *dst_nearest,
                                   int_mv *dst_near,
                                   int block_idx, int ref_idx) {
  int_mv dst_list[MAX_MV_REF_CANDIDATES];
  int_mv mv_list[MAX_MV_REF_CANDIDATES];
  MODE_INFO *mi = xd->mode_info_context;
  MB_MODE_INFO *const mbmi = &mi->mbmi;

  assert(ref_idx == 0 || ref_idx == 1);
  assert(MAX_MV_REF_CANDIDATES == 2);  // makes code here slightly easier

  vp9_find_mv_refs_idx(cm, xd, xd->mode_info_context,
                       xd->prev_mode_info_context,
                       mbmi->ref_frame[ref_idx],
                       mv_list, cm->ref_frame_sign_bias, block_idx);

  dst_list[1].as_int = 0;
  if (block_idx == 0) {
    memcpy(dst_list, mv_list, MAX_MV_REF_CANDIDATES * sizeof(int_mv));
  } else if (block_idx == 1 || block_idx == 2) {
    int dst = 0, n;
    union b_mode_info *bmi = mi->bmi;

    dst_list[dst++].as_int = bmi[0].as_mv[ref_idx].as_int;
    for (n = 0; dst < MAX_MV_REF_CANDIDATES &&
                n < MAX_MV_REF_CANDIDATES; n++)
      if (mv_list[n].as_int != dst_list[0].as_int)
        dst_list[dst++].as_int = mv_list[n].as_int;
  } else {
    int dst = 0, n;
    union b_mode_info *bmi = mi->bmi;

    assert(block_idx == 3);
    dst_list[dst++].as_int = bmi[2].as_mv[ref_idx].as_int;
    if (dst_list[0].as_int != bmi[1].as_mv[ref_idx].as_int)
      dst_list[dst++].as_int = bmi[1].as_mv[ref_idx].as_int;
    if (dst < MAX_MV_REF_CANDIDATES &&
        dst_list[0].as_int != bmi[0].as_mv[ref_idx].as_int)
      dst_list[dst++].as_int = bmi[0].as_mv[ref_idx].as_int;
    for (n = 0; dst < MAX_MV_REF_CANDIDATES &&
                n < MAX_MV_REF_CANDIDATES; n++)
      if (mv_list[n].as_int != dst_list[0].as_int)
        dst_list[dst++].as_int = mv_list[n].as_int;
  }

  dst_nearest->as_int = dst_list[0].as_int;
  dst_near->as_int = dst_list[1].as_int;
}

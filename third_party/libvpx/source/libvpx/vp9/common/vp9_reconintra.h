/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_COMMON_VP9_RECONINTRA_H_
#define VP9_COMMON_VP9_RECONINTRA_H_

#include "vpx/vpx_integer.h"
#include "vp9/common/vp9_blockd.h"

MB_PREDICTION_MODE vp9_find_dominant_direction(uint8_t *ptr,
                                               int stride, int n,
                                               int tx, int ty);

MB_PREDICTION_MODE vp9_find_bpred_context(MACROBLOCKD *xd, int block,
                                          uint8_t *ptr, int stride);

void vp9_predict_intra_block(MACROBLOCKD *xd,
                            int block_idx,
                            int bwl_in,
                            TX_SIZE tx_size,
                            int mode,
                            uint8_t *predictor, int pre_stride);
#endif  // VP9_COMMON_VP9_RECONINTRA_H_

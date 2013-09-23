/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_DECODER_VP9_DBOOLHUFF_H_
#define VP9_DECODER_VP9_DBOOLHUFF_H_

#include <stddef.h>
#include <limits.h>

#include "./vpx_config.h"
#include "vpx_ports/mem.h"
#include "vpx/vpx_integer.h"

typedef size_t VP9_BD_VALUE;

#define VP9_BD_VALUE_SIZE ((int)sizeof(VP9_BD_VALUE)*CHAR_BIT)

// This is meant to be a large, positive constant that can still be efficiently
// loaded as an immediate (on platforms like ARM, for example).
// Even relatively modest values like 100 would work fine.
#define VP9_LOTS_OF_BITS 0x40000000

typedef struct {
  const uint8_t *buffer_end;
  const uint8_t *buffer;
  VP9_BD_VALUE value;
  int count;
  unsigned int range;
} vp9_reader;

DECLARE_ALIGNED(16, extern const uint8_t, vp9_norm[256]);

int vp9_reader_init(vp9_reader *r, const uint8_t *buffer, size_t size);

void vp9_reader_fill(vp9_reader *r);

const uint8_t *vp9_reader_find_end(vp9_reader *r);

static int vp9_read(vp9_reader *br, int probability) {
  unsigned int bit = 0;
  VP9_BD_VALUE value;
  VP9_BD_VALUE bigsplit;
  int count;
  unsigned int range;
  unsigned int split = 1 + (((br->range - 1) * probability) >> 8);

  if (br->count < 0)
    vp9_reader_fill(br);

  value = br->value;
  count = br->count;

  bigsplit = (VP9_BD_VALUE)split << (VP9_BD_VALUE_SIZE - 8);

  range = split;

  if (value >= bigsplit) {
    range = br->range - split;
    value = value - bigsplit;
    bit = 1;
  }

  {
    register unsigned int shift = vp9_norm[range];
    range <<= shift;
    value <<= shift;
    count -= shift;
  }
  br->value = value;
  br->count = count;
  br->range = range;

  return bit;
}

static int vp9_read_bit(vp9_reader *r) {
  return vp9_read(r, 128);  // vp9_prob_half
}

static int vp9_read_literal(vp9_reader *br, int bits) {
  int z = 0, bit;

  for (bit = bits - 1; bit >= 0; bit--)
    z |= vp9_read_bit(br) << bit;

  return z;
}

static int vp9_reader_has_error(vp9_reader *r) {
  // Check if we have reached the end of the buffer.
  //
  // Variable 'count' stores the number of bits in the 'value' buffer, minus
  // 8. The top byte is part of the algorithm, and the remainder is buffered
  // to be shifted into it. So if count == 8, the top 16 bits of 'value' are
  // occupied, 8 for the algorithm and 8 in the buffer.
  //
  // When reading a byte from the user's buffer, count is filled with 8 and
  // one byte is filled into the value buffer. When we reach the end of the
  // data, count is additionally filled with VP9_LOTS_OF_BITS. So when
  // count == VP9_LOTS_OF_BITS - 1, the user's data has been exhausted.
  //
  // 1 if we have tried to decode bits after the end of stream was encountered.
  // 0 No error.
  return r->count > VP9_BD_VALUE_SIZE && r->count < VP9_LOTS_OF_BITS;
}

#endif  // VP9_DECODER_VP9_DBOOLHUFF_H_

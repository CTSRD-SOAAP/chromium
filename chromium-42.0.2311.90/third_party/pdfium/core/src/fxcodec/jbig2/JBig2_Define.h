// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
 
// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef _JBIG2_DEFINE_H_
#define _JBIG2_DEFINE_H_
#include "../../../include/fxcrt/fx_system.h"
#define JBIG2_memset	FXSYS_memset8
#define JBIG2_memcmp	FXSYS_memcmp32
#define JBIG2_memcpy	FXSYS_memcpy32
#include "JBig2_Object.h"
#define JBIG2_OOB			1
typedef struct {
    FX_INT32 width,
             height;
    FX_INT32 x,
             y;
    FX_BYTE flags;
} JBig2RegionInfo;
typedef struct {
    FX_INT32 codelen;
    FX_INT32 code;
} JBig2HuffmanCode;
extern "C" {
    void _FaxG4Decode(void *pModule, FX_LPCBYTE src_buf, FX_DWORD src_size, int* pbitpos, FX_LPBYTE dest_buf, int width, int height, int pitch = 0);
};
#define JBIG2_MAX_REFERRED_SEGMENT_COUNT		64
#define JBIG2_MAX_EXPORT_SYSMBOLS				65535
#define JBIG2_MAX_NEW_SYSMBOLS					65535
#define JBIG2_MAX_PATTERN_INDEX					65535
#define JBIG2_MAX_IMAGE_SIZE					65535
#endif

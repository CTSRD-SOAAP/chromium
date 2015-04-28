// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
 
// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef _FX_DIRECT_WRITE_
#define _FX_DIRECT_WRITE_
#ifndef DECLSPEC_UUID
#if (_MSC_VER >= 1100) && defined (__cplusplus)
#define DECLSPEC_UUID(x)    __declspec(uuid(x))
#else
#define DECLSPEC_UUID(x)
#endif
#endif
#ifndef DECLSPEC_NOVTABLE
#if (_MSC_VER >= 1100) && defined(__cplusplus)
#define DECLSPEC_NOVTABLE   __declspec(novtable)
#else
#define DECLSPEC_NOVTABLE
#endif
#endif
#if(WINVER < 0x0500)
#ifndef _MAC
DECLARE_HANDLE(HMONITOR);
#endif
#endif
class CDWriteExt
{
public:
    CDWriteExt();
    ~CDWriteExt();

    void			Load();
    void            Unload();

    FX_BOOL			IsAvailable()
    {
        return m_pDWriteFactory != NULL;
    }

    void*			DwCreateFontFaceFromStream(FX_LPBYTE pData, FX_DWORD size, int simulation_style);
    FX_BOOL         DwCreateRenderingTarget(CFX_DIBitmap* pSrc, void** renderTarget);
    void            DwDeleteRenderingTarget(void* renderTarget);
    FX_BOOL			DwRendingString(void* renderTarget, CFX_ClipRgn* pClipRgn, FX_RECT& stringRect, CFX_AffineMatrix* pMatrix,
                                    void *font, FX_FLOAT font_size, FX_ARGB text_color,
                                    int glyph_count, unsigned short* glyph_indices,
                                    FX_FLOAT baselineOriginX, FX_FLOAT baselineOriginY,
                                    void* glyph_offsets,
                                    FX_FLOAT* glyph_advances);
    void			DwDeleteFont(void* pFont);

protected:
    void*			m_hModule;
    void*			m_pDWriteFactory;
    void*		    m_pDwFontContext;
    void*	        m_pDwTextRenderer;
};
#endif

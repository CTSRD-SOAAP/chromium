/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef SkPDFCanon_DEFINED
#define SkPDFCanon_DEFINED

#include "SkPDFShader.h"
#include "SkTDArray.h"

class SkBitmap;
class SkPDFFont;
class SkPDFGraphicState;
class SkPDFBitmap;
class SkPaint;

/**
 *  The SkPDFCanon canonicalizes objects across PDF pages(SkPDFDevices).
 *
 *  The PDF backend works correctly if:
 *  -  There is no more than one SkPDFCanon for each thread.
 *  -  Every SkPDFDevice is given a pointer to a SkPDFCanon on creation.
 *  -  All SkPDFDevices in a document share the same SkPDFCanon.
 *  The SkDocument_PDF class makes this happen by owning a single
 *  SkPDFCanon.
 *
 *  Note that this class does not create, delete, reference or
 *  dereference the SkPDFObject objects that it indexes.  It is up to
 *  the caller to manage the lifetime of these objects.
 */
class SkPDFCanon : SkNoncopyable {
public:
    SkPDFCanon();
    ~SkPDFCanon();

    // Returns exact match if there is one.  If not, it returns NULL.
    // If there is no exact match, but there is a related font, we
    // still return NULL, but also set *relatedFont.
    SkPDFFont* findFont(uint32_t fontID,
                        uint16_t glyphID,
                        SkPDFFont** relatedFont) const;
    void addFont(SkPDFFont* font, uint32_t fontID, uint16_t fGlyphID);
    void removeFont(SkPDFFont*);

    SkPDFFunctionShader* findFunctionShader(const SkPDFShader::State&) const;
    void addFunctionShader(SkPDFFunctionShader*);
    void removeFunctionShader(SkPDFFunctionShader*);

    SkPDFAlphaFunctionShader* findAlphaShader(const SkPDFShader::State&) const;
    void addAlphaShader(SkPDFAlphaFunctionShader*);
    void removeAlphaShader(SkPDFAlphaFunctionShader*);

    SkPDFImageShader* findImageShader(const SkPDFShader::State&) const;
    void addImageShader(SkPDFImageShader*);
    void removeImageShader(SkPDFImageShader*);

    SkPDFGraphicState* findGraphicState(const SkPaint&) const;
    void addGraphicState(SkPDFGraphicState*);
    void removeGraphicState(SkPDFGraphicState*);

    SkPDFBitmap* findBitmap(const SkBitmap&) const;
    void addBitmap(SkPDFBitmap*);
    void removeBitmap(SkPDFBitmap*);

    void assertEmpty() const {
        SkASSERT(fFontRecords.isEmpty());
        SkASSERT(fFunctionShaderRecords.isEmpty());
        SkASSERT(fAlphaShaderRecords.isEmpty());
        SkASSERT(fImageShaderRecords.isEmpty());
        SkASSERT(fGraphicStateRecords.isEmpty());
        SkASSERT(fBitmapRecords.isEmpty());
    }

private:
    struct FontRec {
        SkPDFFont* fFont;
        uint32_t fFontID;
        uint16_t fGlyphID;
    };
    SkTDArray<FontRec> fFontRecords;

    SkTDArray<SkPDFFunctionShader*> fFunctionShaderRecords;

    SkTDArray<SkPDFAlphaFunctionShader*> fAlphaShaderRecords;

    SkTDArray<SkPDFImageShader*> fImageShaderRecords;

    SkTDArray<SkPDFGraphicState*> fGraphicStateRecords;

    SkTDArray<SkPDFBitmap*> fBitmapRecords;
};
#endif  // SkPDFCanon_DEFINED

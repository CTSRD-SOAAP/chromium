/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef SkPDFBitmap_DEFINED
#define SkPDFBitmap_DEFINED

#include "SkPDFTypes.h"
#include "SkBitmap.h"

class SkPDFCanon;

/**
 * SkPDFBitmap wraps a SkBitmap and serializes it as an image Xobject.
 * It is designed to use a minimal amout of memory, aside from refing
 * the bitmap's pixels, and its emitObject() does not cache any data.
 *
 * As of now, it only supports 8888 bitmaps (the most common case).
 *
 * The SkPDFBitmap::Create function will check the canon for duplicates.
 */
class SkPDFBitmap : public SkPDFObject {
public:
    // Returns NULL on unsupported bitmap;
    // TODO(halcanary): support other bitmap colortypes and replace
    // SkPDFImage.
    static SkPDFBitmap* Create(SkPDFCanon*,
                               const SkBitmap&,
                               const SkIRect& subset);
    ~SkPDFBitmap();
    void emitObject(SkWStream*, SkPDFCatalog*) SK_OVERRIDE;
    void addResources(SkTSet<SkPDFObject*>* resourceSet,
                      SkPDFCatalog* catalog) const SK_OVERRIDE;
    bool equals(const SkBitmap& other) const {
        return fBitmap.getGenerationID() == other.getGenerationID() &&
               fBitmap.pixelRefOrigin() == other.pixelRefOrigin() &&
               fBitmap.dimensions() == other.dimensions();
    }

private:
    SkPDFCanon* const fCanon;
    const SkBitmap fBitmap;
    const SkAutoTUnref<SkPDFObject> fSMask;
    SkPDFBitmap(SkPDFCanon*, const SkBitmap&, SkPDFObject*);
    void emitDict(SkWStream*, SkPDFCatalog*, size_t, bool) const;
};

#endif  // SkPDFBitmap_DEFINED

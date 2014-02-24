/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkScaledImageCache.h"
#include "SkMipMap.h"
#include "SkOnce.h"
#include "SkPixelRef.h"
#include "SkRect.h"

#ifndef SK_DEFAULT_IMAGE_CACHE_LIMIT
    #define SK_DEFAULT_IMAGE_CACHE_LIMIT     (2 * 1024 * 1024)
#endif

static inline SkScaledImageCache::ID* rec_to_id(SkScaledImageCache::Rec* rec) {
    return reinterpret_cast<SkScaledImageCache::ID*>(rec);
}

static inline SkScaledImageCache::Rec* id_to_rec(SkScaledImageCache::ID* id) {
    return reinterpret_cast<SkScaledImageCache::Rec*>(id);
}

 // Implemented from en.wikipedia.org/wiki/MurmurHash.
static uint32_t compute_hash(const uint32_t data[], int count) {
    uint32_t hash = 0;

    for (int i = 0; i < count; ++i) {
        uint32_t k = data[i];
        k *= 0xcc9e2d51;
        k = (k << 15) | (k >> 17);
        k *= 0x1b873593;

        hash ^= k;
        hash = (hash << 13) | (hash >> 19);
        hash *= 5;
        hash += 0xe6546b64;
    }

    //    hash ^= size;
    hash ^= hash >> 16;
    hash *= 0x85ebca6b;
    hash ^= hash >> 13;
    hash *= 0xc2b2ae35;
    hash ^= hash >> 16;

    return hash;
}

struct Key {
    Key(uint32_t genID,
        SkScalar scaleX,
        SkScalar scaleY,
        SkIRect  bounds)
        : fGenID(genID)
        , fScaleX(scaleX)
        , fScaleY(scaleY)
        , fBounds(bounds) {
        fHash = compute_hash(&fGenID, 7);
    }

    bool operator<(const Key& other) const {
        const uint32_t* a = &fGenID;
        const uint32_t* b = &other.fGenID;
        for (int i = 0; i < 7; ++i) {
            if (a[i] < b[i]) {
                return true;
            }
            if (a[i] > b[i]) {
                return false;
            }
        }
        return false;
    }

    bool operator==(const Key& other) const {
        const uint32_t* a = &fHash;
        const uint32_t* b = &other.fHash;
        for (int i = 0; i < 8; ++i) {
            if (a[i] != b[i]) {
                return false;
            }
        }
        return true;
    }

    uint32_t    fHash;
    uint32_t    fGenID;
    float       fScaleX;
    float       fScaleY;
    SkIRect     fBounds;
};

struct SkScaledImageCache::Rec {
    Rec(const Key& key, const SkBitmap& bm) : fKey(key), fBitmap(bm) {
        fLockCount = 1;
        fMip = NULL;
    }

    Rec(const Key& key, const SkMipMap* mip) : fKey(key) {
        fLockCount = 1;
        fMip = mip;
        mip->ref();
    }

    ~Rec() {
        SkSafeUnref(fMip);
    }

    size_t bytesUsed() const {
        return fMip ? fMip->getSize() : fBitmap.getSize();
    }

    Rec*    fNext;
    Rec*    fPrev;

    // this guy wants to be 64bit aligned
    Key     fKey;

    int32_t fLockCount;

    // we use either fBitmap or fMip, but not both
    SkBitmap fBitmap;
    const SkMipMap* fMip;
};

#include "SkTDynamicHash.h"

namespace { // can't use static functions w/ template parameters
const Key& key_from_rec(const SkScaledImageCache::Rec& rec) {
    return rec.fKey;
}

uint32_t hash_from_key(const Key& key) {
    return key.fHash;
}

bool eq_rec_key(const SkScaledImageCache::Rec& rec, const Key& key) {
    return rec.fKey == key;
}
}

class SkScaledImageCache::Hash : public SkTDynamicHash<SkScaledImageCache::Rec,
                                   Key, key_from_rec, hash_from_key,
                                   eq_rec_key> {};

///////////////////////////////////////////////////////////////////////////////

// experimental hash to speed things up
#define USE_HASH

#if !defined(USE_HASH)
static inline SkScaledImageCache::Rec* find_rec_in_list(
        SkScaledImageCache::Rec* head, const Key & key) {
    SkScaledImageCache::Rec* rec = head;
    while ((rec != NULL) && (rec->fKey != key)) {
        rec = rec->fNext;
    }
    return rec;
}
#endif

SkScaledImageCache::SkScaledImageCache(size_t byteLimit) {
    fHead = NULL;
    fTail = NULL;
#ifdef USE_HASH
    fHash = new Hash;
#else
    fHash = NULL;
#endif
    fBytesUsed = 0;
    fByteLimit = byteLimit;
    fCount = 0;
}

SkScaledImageCache::~SkScaledImageCache() {
    Rec* rec = fHead;
    while (rec) {
        Rec* next = rec->fNext;
        SkDELETE(rec);
        rec = next;
    }
    delete fHash;
}

////////////////////////////////////////////////////////////////////////////////

/**
   This private method is the fully general record finder. All other
   record finders should call this funtion. */
SkScaledImageCache::Rec* SkScaledImageCache::findAndLock(uint32_t genID,
                                                        SkScalar scaleX,
                                                        SkScalar scaleY,
                                                        const SkIRect& bounds) {
    if (bounds.isEmpty()) {
        return NULL;
    }
    Key key(genID, scaleX, scaleY, bounds);
#ifdef USE_HASH
    Rec* rec = fHash->find(key);
#else
    Rec* rec = find_rec_in_list(fHead, key);
#endif
    if (rec) {
        this->moveToHead(rec);  // for our LRU
        rec->fLockCount += 1;
    }
    return rec;
}

/**
   This function finds the bounds of the bitmap *within its pixelRef*.
   If the bitmap lacks a pixelRef, it will return an empty rect, since
   that doesn't make sense.  This may be a useful enough function that
   it should be somewhere else (in SkBitmap?). */
static SkIRect get_bounds_from_bitmap(const SkBitmap& bm) {
    if (!(bm.pixelRef())) {
        return SkIRect::MakeEmpty();
    }
    size_t x, y;
    SkTDivMod(bm.pixelRefOffset(), bm.rowBytes(), &y, &x);
    x >>= bm.shiftPerPixel();
    return SkIRect::MakeXYWH(x, y, bm.width(), bm.height());
}


SkScaledImageCache::ID* SkScaledImageCache::findAndLock(uint32_t genID,
                                                        int32_t width,
                                                        int32_t height,
                                                        SkBitmap* bitmap) {
    Rec* rec = this->findAndLock(genID, SK_Scalar1, SK_Scalar1,
                                 SkIRect::MakeWH(width, height));
    if (rec) {
        SkASSERT(NULL == rec->fMip);
        SkASSERT(rec->fBitmap.pixelRef());
        *bitmap = rec->fBitmap;
    }
    return rec_to_id(rec);
}

SkScaledImageCache::ID* SkScaledImageCache::findAndLock(const SkBitmap& orig,
                                                        SkScalar scaleX,
                                                        SkScalar scaleY,
                                                        SkBitmap* scaled) {
    if (0 == scaleX || 0 == scaleY) {
        // degenerate, and the key we use for mipmaps
        return NULL;
    }
    Rec* rec = this->findAndLock(orig.getGenerationID(), scaleX,
                                 scaleY, get_bounds_from_bitmap(orig));
    if (rec) {
        SkASSERT(NULL == rec->fMip);
        SkASSERT(rec->fBitmap.pixelRef());
        *scaled = rec->fBitmap;
    }
    return rec_to_id(rec);
}

SkScaledImageCache::ID* SkScaledImageCache::findAndLockMip(const SkBitmap& orig,
                                                           SkMipMap const ** mip) {
    Rec* rec = this->findAndLock(orig.getGenerationID(), 0, 0,
                                 get_bounds_from_bitmap(orig));
    if (rec) {
        SkASSERT(rec->fMip);
        SkASSERT(NULL == rec->fBitmap.pixelRef());
        *mip = rec->fMip;
    }
    return rec_to_id(rec);
}


////////////////////////////////////////////////////////////////////////////////
/**
   This private method is the fully general record adder. All other
   record adders should call this funtion. */
void SkScaledImageCache::addAndLock(SkScaledImageCache::Rec* rec) {
    SkASSERT(rec);
    this->addToHead(rec);
    SkASSERT(1 == rec->fLockCount);
#ifdef USE_HASH
    SkASSERT(fHash);
    fHash->add(rec);
#endif
    // We may (now) be overbudget, so see if we need to purge something.
    this->purgeAsNeeded();
}

SkScaledImageCache::ID* SkScaledImageCache::addAndLock(uint32_t genID,
                                                       int32_t width,
                                                       int32_t height,
                                                       const SkBitmap& bitmap) {
    Key key(genID, SK_Scalar1, SK_Scalar1, SkIRect::MakeWH(width, height));
    Rec* rec = SkNEW_ARGS(Rec, (key, bitmap));
    this->addAndLock(rec);
    return rec_to_id(rec);
}

SkScaledImageCache::ID* SkScaledImageCache::addAndLock(const SkBitmap& orig,
                                                       SkScalar scaleX,
                                                       SkScalar scaleY,
                                                       const SkBitmap& scaled) {
    if (0 == scaleX || 0 == scaleY) {
        // degenerate, and the key we use for mipmaps
        return NULL;
    }
    SkIRect bounds = get_bounds_from_bitmap(orig);
    if (bounds.isEmpty()) {
        return NULL;
    }
    Key key(orig.getGenerationID(), scaleX, scaleY, bounds);
    Rec* rec = SkNEW_ARGS(Rec, (key, scaled));
    this->addAndLock(rec);
    return rec_to_id(rec);
}

SkScaledImageCache::ID* SkScaledImageCache::addAndLockMip(const SkBitmap& orig,
                                                          const SkMipMap* mip) {
    SkIRect bounds = get_bounds_from_bitmap(orig);
    if (bounds.isEmpty()) {
        return NULL;
    }
    Key key(orig.getGenerationID(), 0, 0, bounds);
    Rec* rec = SkNEW_ARGS(Rec, (key, mip));
    this->addAndLock(rec);
    return rec_to_id(rec);
}

void SkScaledImageCache::unlock(SkScaledImageCache::ID* id) {
    SkASSERT(id);

#ifdef SK_DEBUG
    {
        bool found = false;
        Rec* rec = fHead;
        while (rec != NULL) {
            if (rec == id_to_rec(id)) {
                found = true;
                break;
            }
            rec = rec->fNext;
        }
        SkASSERT(found);
    }
#endif
    Rec* rec = id_to_rec(id);
    SkASSERT(rec->fLockCount > 0);
    rec->fLockCount -= 1;

    // we may have been over-budget, but now have released something, so check
    // if we should purge.
    if (0 == rec->fLockCount) {
        this->purgeAsNeeded();
    }
}

void SkScaledImageCache::purgeAsNeeded() {
    size_t byteLimit = fByteLimit;
    size_t bytesUsed = fBytesUsed;

    Rec* rec = fTail;
    while (rec) {
        if (bytesUsed < byteLimit) {
            break;
        }
        Rec* prev = rec->fPrev;
        if (0 == rec->fLockCount) {
            size_t used = rec->bytesUsed();
            SkASSERT(used <= bytesUsed);
            bytesUsed -= used;
            this->detach(rec);
#ifdef USE_HASH
            fHash->remove(rec->fKey);
#endif

            SkDELETE(rec);
            fCount -= 1;
        }
        rec = prev;
    }
    fBytesUsed = bytesUsed;
}

size_t SkScaledImageCache::setByteLimit(size_t newLimit) {
    size_t prevLimit = fByteLimit;
    fByteLimit = newLimit;
    if (newLimit < prevLimit) {
        this->purgeAsNeeded();
    }
    return prevLimit;
}

///////////////////////////////////////////////////////////////////////////////

void SkScaledImageCache::detach(Rec* rec) {
    Rec* prev = rec->fPrev;
    Rec* next = rec->fNext;

    if (!prev) {
        SkASSERT(fHead == rec);
        fHead = next;
    } else {
        prev->fNext = next;
    }

    if (!next) {
        fTail = prev;
    } else {
        next->fPrev = prev;
    }

    rec->fNext = rec->fPrev = NULL;
}

void SkScaledImageCache::moveToHead(Rec* rec) {
    if (fHead == rec) {
        return;
    }

    SkASSERT(fHead);
    SkASSERT(fTail);

    this->validate();

    this->detach(rec);

    fHead->fPrev = rec;
    rec->fNext = fHead;
    fHead = rec;

    this->validate();
}

void SkScaledImageCache::addToHead(Rec* rec) {
    this->validate();

    rec->fPrev = NULL;
    rec->fNext = fHead;
    if (fHead) {
        fHead->fPrev = rec;
    }
    fHead = rec;
    if (!fTail) {
        fTail = rec;
    }
    fBytesUsed += rec->bytesUsed();
    fCount += 1;

    this->validate();
}

#ifdef SK_DEBUG
void SkScaledImageCache::validate() const {
    if (NULL == fHead) {
        SkASSERT(NULL == fTail);
        SkASSERT(0 == fBytesUsed);
        return;
    }

    if (fHead == fTail) {
        SkASSERT(NULL == fHead->fPrev);
        SkASSERT(NULL == fHead->fNext);
        SkASSERT(fHead->bytesUsed() == fBytesUsed);
        return;
    }

    SkASSERT(NULL == fHead->fPrev);
    SkASSERT(NULL != fHead->fNext);
    SkASSERT(NULL == fTail->fNext);
    SkASSERT(NULL != fTail->fPrev);

    size_t used = 0;
    int count = 0;
    const Rec* rec = fHead;
    while (rec) {
        count += 1;
        used += rec->bytesUsed();
        SkASSERT(used <= fBytesUsed);
        rec = rec->fNext;
    }
    SkASSERT(fCount == count);

    rec = fTail;
    while (rec) {
        SkASSERT(count > 0);
        count -= 1;
        SkASSERT(used >= rec->bytesUsed());
        used -= rec->bytesUsed();
        rec = rec->fPrev;
    }

    SkASSERT(0 == count);
    SkASSERT(0 == used);
}
#endif

///////////////////////////////////////////////////////////////////////////////

#include "SkThread.h"

SK_DECLARE_STATIC_MUTEX(gMutex);

static void create_cache(SkScaledImageCache** cache) {
    *cache = SkNEW_ARGS(SkScaledImageCache, (SK_DEFAULT_IMAGE_CACHE_LIMIT));
}

static SkScaledImageCache* get_cache() {
    static SkScaledImageCache* gCache(NULL);
    SK_DECLARE_STATIC_ONCE(create_cache_once);
    SkOnce<SkScaledImageCache**>(&create_cache_once, create_cache, &gCache);
    SkASSERT(NULL != gCache);
    return gCache;
}


SkScaledImageCache::ID* SkScaledImageCache::FindAndLock(
                                uint32_t pixelGenerationID,
                                int32_t width,
                                int32_t height,
                                SkBitmap* scaled) {
    SkAutoMutexAcquire am(gMutex);
    return get_cache()->findAndLock(pixelGenerationID, width, height, scaled);
}

SkScaledImageCache::ID* SkScaledImageCache::AddAndLock(
                               uint32_t pixelGenerationID,
                               int32_t width,
                               int32_t height,
                               const SkBitmap& scaled) {
    SkAutoMutexAcquire am(gMutex);
    return get_cache()->addAndLock(pixelGenerationID, width, height, scaled);
}


SkScaledImageCache::ID* SkScaledImageCache::FindAndLock(const SkBitmap& orig,
                                                        SkScalar scaleX,
                                                        SkScalar scaleY,
                                                        SkBitmap* scaled) {
    SkAutoMutexAcquire am(gMutex);
    return get_cache()->findAndLock(orig, scaleX, scaleY, scaled);
}

SkScaledImageCache::ID* SkScaledImageCache::FindAndLockMip(const SkBitmap& orig,
                                                       SkMipMap const ** mip) {
    SkAutoMutexAcquire am(gMutex);
    return get_cache()->findAndLockMip(orig, mip);
}

SkScaledImageCache::ID* SkScaledImageCache::AddAndLock(const SkBitmap& orig,
                                                       SkScalar scaleX,
                                                       SkScalar scaleY,
                                                       const SkBitmap& scaled) {
    SkAutoMutexAcquire am(gMutex);
    return get_cache()->addAndLock(orig, scaleX, scaleY, scaled);
}

SkScaledImageCache::ID* SkScaledImageCache::AddAndLockMip(const SkBitmap& orig,
                                                          const SkMipMap* mip) {
    SkAutoMutexAcquire am(gMutex);
    return get_cache()->addAndLockMip(orig, mip);
}

void SkScaledImageCache::Unlock(SkScaledImageCache::ID* id) {
    SkAutoMutexAcquire am(gMutex);
    return get_cache()->unlock(id);
}

size_t SkScaledImageCache::GetBytesUsed() {
    SkAutoMutexAcquire am(gMutex);
    return get_cache()->getBytesUsed();
}

size_t SkScaledImageCache::GetByteLimit() {
    SkAutoMutexAcquire am(gMutex);
    return get_cache()->getByteLimit();
}

size_t SkScaledImageCache::SetByteLimit(size_t newLimit) {
    SkAutoMutexAcquire am(gMutex);
    return get_cache()->setByteLimit(newLimit);
}

///////////////////////////////////////////////////////////////////////////////

#include "SkGraphics.h"

size_t SkGraphics::GetImageCacheBytesUsed() {
    return SkScaledImageCache::GetBytesUsed();
}

size_t SkGraphics::GetImageCacheByteLimit() {
    return SkScaledImageCache::GetByteLimit();
}

size_t SkGraphics::SetImageCacheByteLimit(size_t newLimit) {
    return SkScaledImageCache::SetByteLimit(newLimit);
}

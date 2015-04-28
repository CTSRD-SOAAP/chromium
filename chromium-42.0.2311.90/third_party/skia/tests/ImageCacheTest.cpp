 /*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkDiscardableMemory.h"
#include "SkResourceCache.h"
#include "Test.h"

namespace {
static void* gGlobalAddress;
struct TestingKey : public SkResourceCache::Key {
    intptr_t    fValue;

    TestingKey(intptr_t value) : fValue(value) {
        this->init(&gGlobalAddress, sizeof(fValue));
    }
};
struct TestingRec : public SkResourceCache::Rec {
    TestingRec(const TestingKey& key, uint32_t value) : fKey(key), fValue(value) {}

    TestingKey  fKey;
    intptr_t    fValue;

    const Key& getKey() const SK_OVERRIDE { return fKey; }
    size_t bytesUsed() const SK_OVERRIDE { return sizeof(fKey) + sizeof(fValue); }

    static bool Visitor(const SkResourceCache::Rec& baseRec, void* context) {
        const TestingRec& rec = static_cast<const TestingRec&>(baseRec);
        intptr_t* result = (intptr_t*)context;
        
        *result = rec.fValue;
        return true;
    }
};
}

static const int COUNT = 10;
static const int DIM = 256;

static void test_cache(skiatest::Reporter* reporter, SkResourceCache& cache, bool testPurge) {
    for (int i = 0; i < COUNT; ++i) {
        TestingKey key(i);
        intptr_t value = -1;

        REPORTER_ASSERT(reporter, !cache.find(key, TestingRec::Visitor, &value));
        REPORTER_ASSERT(reporter, -1 == value);

        cache.add(SkNEW_ARGS(TestingRec, (key, i)));

        REPORTER_ASSERT(reporter, cache.find(key, TestingRec::Visitor, &value));
        REPORTER_ASSERT(reporter, i == value);
    }

    if (testPurge) {
        // stress test, should trigger purges
        for (int i = 0; i < COUNT * 100; ++i) {
            TestingKey key(i);
            cache.add(SkNEW_ARGS(TestingRec, (key, i)));
        }
    }

    // test the originals after all that purging
    for (int i = 0; i < COUNT; ++i) {
        intptr_t value;
        (void)cache.find(TestingKey(i), TestingRec::Visitor, &value);
    }

    cache.setTotalByteLimit(0);
}

#include "SkDiscardableMemoryPool.h"

static SkDiscardableMemoryPool* gPool;
static SkDiscardableMemory* pool_factory(size_t bytes) {
    SkASSERT(gPool);
    return gPool->create(bytes);
}

DEF_TEST(ImageCache, reporter) {
    static const size_t defLimit = DIM * DIM * 4 * COUNT + 1024;    // 1K slop

    {
        SkResourceCache cache(defLimit);
        test_cache(reporter, cache, true);
    }
    {
        SkAutoTUnref<SkDiscardableMemoryPool> pool(
                SkDiscardableMemoryPool::Create(defLimit, NULL));
        gPool = pool.get();
        SkResourceCache cache(pool_factory);
        test_cache(reporter, cache, true);
    }
    {
        SkResourceCache cache(SkDiscardableMemory::Create);
        test_cache(reporter, cache, false);
    }
}

DEF_TEST(ImageCache_doubleAdd, r) {
    // Adding the same key twice should be safe.
    SkResourceCache cache(4096);

    TestingKey key(1);

    cache.add(SkNEW_ARGS(TestingRec, (key, 2)));
    cache.add(SkNEW_ARGS(TestingRec, (key, 3)));

    // Lookup can return either value.
    intptr_t value = -1;
    REPORTER_ASSERT(r, cache.find(key, TestingRec::Visitor, &value));
    REPORTER_ASSERT(r, 2 == value || 3 == value);
}

/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "core/loader/cache/MemoryCache.h"

#include <stdio.h>
#include "core/dom/CrossThreadTask.h"
#include "core/dom/Document.h"
#include "core/dom/WebCoreMemoryInstrumentation.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderTypes.h"
#include "core/loader/cache/CachedResource.h"
#include "core/loader/cache/CachedResourceHandle.h"
#include "core/page/FrameView.h"
#include "core/platform/Logging.h"
#include "core/platform/graphics/Image.h"
#include "core/platform/network/ResourceHandle.h"
#include "core/workers/WorkerContext.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerThread.h"
#include "weborigin/SecurityOrigin.h"
#include "weborigin/SecurityOriginHash.h"
#include "wtf/Assertions.h"
#include "wtf/CurrentTime.h"
#include "wtf/MathExtras.h"
#include "wtf/MemoryInstrumentationHashMap.h"
#include "wtf/MemoryInstrumentationVector.h"
#include "wtf/MemoryObjectInfo.h"
#include "wtf/TemporaryChange.h"
#include "wtf/text/CString.h"

using namespace std;

namespace WebCore {

static MemoryCache* gMemoryCache;

static const int cDefaultCacheCapacity = 8192 * 1024;
static const double cMinDelayBeforeLiveDecodedPrune = 1; // Seconds.
static const float cTargetPrunePercentage = .95f; // Percentage of capacity toward which we prune, to avoid immediately pruning again.
static const double cDefaultDecodedDataDeletionInterval = 0;

MemoryCache* memoryCache()
{
    ASSERT(WTF::isMainThread());
    if (!gMemoryCache)
        gMemoryCache = new MemoryCache();
    return gMemoryCache;
}

void setMemoryCacheForTesting(MemoryCache* memoryCache)
{
    gMemoryCache = memoryCache;
}

MemoryCache::MemoryCache()
    : m_inPruneResources(false)
    , m_capacity(cDefaultCacheCapacity)
    , m_minDeadCapacity(0)
    , m_maxDeadCapacity(cDefaultCacheCapacity)
    , m_deadDecodedDataDeletionInterval(cDefaultDecodedDataDeletionInterval)
    , m_liveSize(0)
    , m_deadSize(0)
#ifdef MEMORY_CACHE_STATS
    , m_statsTimer(this, &MemoryCache::dumpStats)
#endif
{
#ifdef MEMORY_CACHE_STATS
    const double statsIntervalInSeconds = 15;
    m_statsTimer.startRepeating(statsIntervalInSeconds);
#endif
}

KURL MemoryCache::removeFragmentIdentifierIfNeeded(const KURL& originalURL)
{
    if (!originalURL.hasFragmentIdentifier())
        return originalURL;
    // Strip away fragment identifier from HTTP URLs.
    // Data URLs must be unmodified. For file and custom URLs clients may expect resources 
    // to be unique even when they differ by the fragment identifier only.
    if (!originalURL.protocolIsInHTTPFamily())
        return originalURL;
    KURL url = originalURL;
    url.removeFragmentIdentifier();
    return url;
}

void MemoryCache::add(CachedResource* resource)
{
    ASSERT(WTF::isMainThread());
    m_resources.set(resource->url(), resource);
    resource->setInCache(true);
    resource->updateForAccess();
    
    LOG(ResourceLoading, "MemoryCache::add Added '%s', resource %p\n", resource->url().string().latin1().data(), resource);
}

void MemoryCache::replace(CachedResource* newResource, CachedResource* oldResource)
{
    evict(oldResource);
    ASSERT(!m_resources.get(newResource->url()));
    m_resources.set(newResource->url(), newResource);
    newResource->setInCache(true);
    insertInLRUList(newResource);
    int delta = newResource->size();
    if (newResource->decodedSize() && newResource->hasClients())
        insertInLiveDecodedResourcesList(newResource);
    if (delta)
        adjustSize(newResource->hasClients(), delta);
}

CachedResource* MemoryCache::resourceForURL(const KURL& resourceURL)
{
    ASSERT(WTF::isMainThread());
    KURL url = removeFragmentIdentifierIfNeeded(resourceURL);
    CachedResource* resource = m_resources.get(url);
    if (resource && !resource->makePurgeable(false)) {
        ASSERT(!resource->hasClients());
        evict(resource);
        return 0;
    }
    return resource;
}

unsigned MemoryCache::deadCapacity() const 
{
    // Dead resource capacity is whatever space is not occupied by live resources, bounded by an independent minimum and maximum.
    unsigned capacity = m_capacity - min(m_liveSize, m_capacity); // Start with available capacity.
    capacity = max(capacity, m_minDeadCapacity); // Make sure it's above the minimum.
    capacity = min(capacity, m_maxDeadCapacity); // Make sure it's below the maximum.
    return capacity;
}

unsigned MemoryCache::liveCapacity() const 
{ 
    // Live resource capacity is whatever is left over after calculating dead resource capacity.
    return m_capacity - deadCapacity();
}

void MemoryCache::pruneLiveResources()
{
    unsigned capacity = liveCapacity();
    if (capacity && m_liveSize <= capacity)
        return;

    unsigned targetSize = static_cast<unsigned>(capacity * cTargetPrunePercentage); // Cut by a percentage to avoid immediately pruning again.

    pruneLiveResourcesToSize(targetSize);
}

void MemoryCache::pruneLiveResourcesToPercentage(float prunePercentage)
{
    if (prunePercentage < 0.0f  || prunePercentage > 0.95f)
        return;

    unsigned currentSize = m_liveSize + m_deadSize;
    unsigned targetSize = static_cast<unsigned>(currentSize * prunePercentage);

    pruneLiveResourcesToSize(targetSize);
}

void MemoryCache::pruneLiveResourcesToSize(unsigned targetSize)
{
    if (m_inPruneResources)
        return;
    TemporaryChange<bool> reentrancyProtector(m_inPruneResources, true);

    double currentTime = FrameView::currentPaintTimeStamp();
    if (!currentTime) // In case prune is called directly, outside of a Frame paint.
        currentTime = WTF::currentTime();
    
    // Destroy any decoded data in live objects that we can.
    // Start from the tail, since this is the least recently accessed of the objects.

    // The list might not be sorted by the m_lastDecodedAccessTime. The impact
    // of this weaker invariant is minor as the below if statement to check the
    // elapsedTime will evaluate to false as the currentTime will be a lot
    // greater than the current->m_lastDecodedAccessTime.
    // For more details see: https://bugs.webkit.org/show_bug.cgi?id=30209
    CachedResource* current = m_liveDecodedResources.m_tail;
    while (current) {
        CachedResource* prev = current->m_prevInLiveResourcesList;
        ASSERT(current->hasClients());
        if (current->isLoaded() && current->decodedSize()) {
            // Check to see if the remaining resources are too new to prune.
            double elapsedTime = currentTime - current->m_lastDecodedAccessTime;
            if (elapsedTime < cMinDelayBeforeLiveDecodedPrune)
                return;

            // Destroy our decoded data. This will remove us from 
            // m_liveDecodedResources, and possibly move us to a different LRU 
            // list in m_allResources.
            current->destroyDecodedData();

            if (targetSize && m_liveSize <= targetSize)
                return;
        }
        current = prev;
    }
}

void MemoryCache::pruneDeadResources()
{
    unsigned capacity = deadCapacity();
    if (capacity && m_deadSize <= capacity)
        return;

    unsigned targetSize = static_cast<unsigned>(capacity * cTargetPrunePercentage); // Cut by a percentage to avoid immediately pruning again.
    pruneDeadResourcesToSize(targetSize);
}

void MemoryCache::pruneDeadResourcesToPercentage(float prunePercentage)
{
    if (prunePercentage < 0.0f  || prunePercentage > 0.95f)
        return;

    unsigned currentSize = m_liveSize + m_deadSize;
    unsigned targetSize = static_cast<unsigned>(currentSize * prunePercentage);

    pruneDeadResourcesToSize(targetSize);
}

void MemoryCache::pruneDeadResourcesToSize(unsigned targetSize)
{
    if (m_inPruneResources)
        return;
    TemporaryChange<bool> reentrancyProtector(m_inPruneResources, true);

    int size = m_allResources.size();
 
    // See if we have any purged resources we can evict.
    for (int i = 0; i < size; i++) {
        CachedResource* current = m_allResources[i].m_tail;
        while (current) {
            CachedResource* prev = current->m_prevInAllResourcesList;
            if (current->wasPurged()) {
                ASSERT(!current->hasClients());
                ASSERT(!current->isPreloaded());
                evict(current);
            }
            current = prev;
        }
    }
    if (targetSize && m_deadSize <= targetSize)
        return;

    bool canShrinkLRULists = true;
    for (int i = size - 1; i >= 0; i--) {
        // Remove from the tail, since this is the least frequently accessed of the objects.
        CachedResource* current = m_allResources[i].m_tail;
        
        // First flush all the decoded data in this queue.
        while (current) {
            // Protect 'previous' so it can't get deleted during destroyDecodedData().
            CachedResourceHandle<CachedResource> previous = current->m_prevInAllResourcesList;
            ASSERT(!previous || previous->inCache());
            if (!current->hasClients() && !current->isPreloaded() && current->isLoaded()) {
                // Destroy our decoded data. This will remove us from 
                // m_liveDecodedResources, and possibly move us to a different 
                // LRU list in m_allResources.
                current->destroyDecodedData();

                if (targetSize && m_deadSize <= targetSize)
                    return;
            }
            // Decoded data may reference other resources. Stop iterating if 'previous' somehow got
            // kicked out of cache during destroyDecodedData().
            if (previous && !previous->inCache())
                break;
            current = previous.get();
        }

        // Now evict objects from this queue.
        current = m_allResources[i].m_tail;
        while (current) {
            CachedResourceHandle<CachedResource> previous = current->m_prevInAllResourcesList;
            ASSERT(!previous || previous->inCache());
            if (!current->hasClients() && !current->isPreloaded() && !current->isCacheValidator()) {
                evict(current);
                if (targetSize && m_deadSize <= targetSize)
                    return;
            }
            if (previous && !previous->inCache())
                break;
            current = previous.get();
        }
            
        // Shrink the vector back down so we don't waste time inspecting
        // empty LRU lists on future prunes.
        if (m_allResources[i].m_head)
            canShrinkLRULists = false;
        else if (canShrinkLRULists)
            m_allResources.resize(i);
    }
}

void MemoryCache::setCapacities(unsigned minDeadBytes, unsigned maxDeadBytes, unsigned totalBytes)
{
    ASSERT(minDeadBytes <= maxDeadBytes);
    ASSERT(maxDeadBytes <= totalBytes);
    m_minDeadCapacity = minDeadBytes;
    m_maxDeadCapacity = maxDeadBytes;
    m_capacity = totalBytes;
    prune();
}

void MemoryCache::evict(CachedResource* resource)
{
    ASSERT(WTF::isMainThread());
    LOG(ResourceLoading, "Evicting resource %p for '%s' from cache", resource, resource->url().string().latin1().data());
    // The resource may have already been removed by someone other than our caller,
    // who needed a fresh copy for a reload. See <http://bugs.webkit.org/show_bug.cgi?id=12479#c6>.
    if (resource->inCache()) {
        // Remove from the resource map.
        m_resources.remove(resource->url());
        resource->setInCache(false);

        // Remove from the appropriate LRU list.
        removeFromLRUList(resource);
        removeFromLiveDecodedResourcesList(resource);
        adjustSize(resource->hasClients(), -static_cast<int>(resource->size()));
    } else
        ASSERT(m_resources.get(resource->url()) != resource);

    resource->deleteIfPossible();
}

MemoryCache::LRUList* MemoryCache::lruListFor(CachedResource* resource)
{
    unsigned accessCount = max(resource->accessCount(), 1U);
    unsigned queueIndex = WTF::fastLog2(resource->size() / accessCount);
#ifndef NDEBUG
    resource->m_lruIndex = queueIndex;
#endif
    if (m_allResources.size() <= queueIndex)
        m_allResources.grow(queueIndex + 1);
    return &m_allResources[queueIndex];
}

void MemoryCache::removeFromLRUList(CachedResource* resource)
{
    // If we've never been accessed, then we're brand new and not in any list.
    if (resource->accessCount() == 0)
        return;

#if !ASSERT_DISABLED
    unsigned oldListIndex = resource->m_lruIndex;
#endif

    LRUList* list = lruListFor(resource);

#if !ASSERT_DISABLED
    // Verify that the list we got is the list we want.
    ASSERT(resource->m_lruIndex == oldListIndex);

    // Verify that we are in fact in this list.
    bool found = false;
    for (CachedResource* current = list->m_head; current; current = current->m_nextInAllResourcesList) {
        if (current == resource) {
            found = true;
            break;
        }
    }
    ASSERT(found);
#endif

    CachedResource* next = resource->m_nextInAllResourcesList;
    CachedResource* prev = resource->m_prevInAllResourcesList;
    
    if (next == 0 && prev == 0 && list->m_head != resource)
        return;
    
    resource->m_nextInAllResourcesList = 0;
    resource->m_prevInAllResourcesList = 0;
    
    if (next)
        next->m_prevInAllResourcesList = prev;
    else if (list->m_tail == resource)
        list->m_tail = prev;

    if (prev)
        prev->m_nextInAllResourcesList = next;
    else if (list->m_head == resource)
        list->m_head = next;
}

void MemoryCache::insertInLRUList(CachedResource* resource)
{
    // Make sure we aren't in some list already.
    ASSERT(!resource->m_nextInAllResourcesList && !resource->m_prevInAllResourcesList);
    ASSERT(resource->inCache());
    ASSERT(resource->accessCount() > 0);
    
    LRUList* list = lruListFor(resource);

    resource->m_nextInAllResourcesList = list->m_head;
    if (list->m_head)
        list->m_head->m_prevInAllResourcesList = resource;
    list->m_head = resource;
    
    if (!resource->m_nextInAllResourcesList)
        list->m_tail = resource;
        
#if !ASSERT_DISABLED
    // Verify that we are in now in the list like we should be.
    list = lruListFor(resource);
    bool found = false;
    for (CachedResource* current = list->m_head; current; current = current->m_nextInAllResourcesList) {
        if (current == resource) {
            found = true;
            break;
        }
    }
    ASSERT(found);
#endif

}

void MemoryCache::removeFromLiveDecodedResourcesList(CachedResource* resource)
{
    // If we've never been accessed, then we're brand new and not in any list.
    if (!resource->m_inLiveDecodedResourcesList)
        return;
    resource->m_inLiveDecodedResourcesList = false;

#if !ASSERT_DISABLED
    // Verify that we are in fact in this list.
    bool found = false;
    for (CachedResource* current = m_liveDecodedResources.m_head; current; current = current->m_nextInLiveResourcesList) {
        if (current == resource) {
            found = true;
            break;
        }
    }
    ASSERT(found);
#endif

    CachedResource* next = resource->m_nextInLiveResourcesList;
    CachedResource* prev = resource->m_prevInLiveResourcesList;
    
    if (next == 0 && prev == 0 && m_liveDecodedResources.m_head != resource)
        return;
    
    resource->m_nextInLiveResourcesList = 0;
    resource->m_prevInLiveResourcesList = 0;
    
    if (next)
        next->m_prevInLiveResourcesList = prev;
    else if (m_liveDecodedResources.m_tail == resource)
        m_liveDecodedResources.m_tail = prev;

    if (prev)
        prev->m_nextInLiveResourcesList = next;
    else if (m_liveDecodedResources.m_head == resource)
        m_liveDecodedResources.m_head = next;
}

void MemoryCache::insertInLiveDecodedResourcesList(CachedResource* resource)
{
    // Make sure we aren't in the list already.
    ASSERT(!resource->m_nextInLiveResourcesList && !resource->m_prevInLiveResourcesList && !resource->m_inLiveDecodedResourcesList);
    resource->m_inLiveDecodedResourcesList = true;

    resource->m_nextInLiveResourcesList = m_liveDecodedResources.m_head;
    if (m_liveDecodedResources.m_head)
        m_liveDecodedResources.m_head->m_prevInLiveResourcesList = resource;
    m_liveDecodedResources.m_head = resource;
    
    if (!resource->m_nextInLiveResourcesList)
        m_liveDecodedResources.m_tail = resource;
        
#if !ASSERT_DISABLED
    // Verify that we are in now in the list like we should be.
    bool found = false;
    for (CachedResource* current = m_liveDecodedResources.m_head; current; current = current->m_nextInLiveResourcesList) {
        if (current == resource) {
            found = true;
            break;
        }
    }
    ASSERT(found);
#endif

}

void MemoryCache::addToLiveResourcesSize(CachedResource* resource)
{
    m_liveSize += resource->size();
    m_deadSize -= resource->size();
}

void MemoryCache::removeFromLiveResourcesSize(CachedResource* resource)
{
    m_liveSize -= resource->size();
    m_deadSize += resource->size();
}

void MemoryCache::adjustSize(bool live, int delta)
{
    if (live) {
        ASSERT(delta >= 0 || ((int)m_liveSize + delta >= 0));
        m_liveSize += delta;
    } else {
        ASSERT(delta >= 0 || ((int)m_deadSize + delta >= 0));
        m_deadSize += delta;
    }
}

void MemoryCache::removeURLFromCache(ScriptExecutionContext* context, const KURL& url)
{
    if (context->isWorkerContext()) {
        WorkerContext* workerContext = static_cast<WorkerContext*>(context);
        workerContext->thread()->workerLoaderProxy().postTaskToLoader(createCallbackTask(&removeURLFromCacheInternal, url));
        return;
    }
    removeURLFromCacheInternal(context, url);
}

void MemoryCache::removeURLFromCacheInternal(ScriptExecutionContext*, const KURL& url)
{
    if (CachedResource* resource = memoryCache()->resourceForURL(url))
        memoryCache()->remove(resource);
}

void MemoryCache::TypeStatistic::addResource(CachedResource* o)
{
    bool purged = o->wasPurged();
    bool purgeable = o->isPurgeable() && !purged; 
    int pageSize = (o->encodedSize() + o->overheadSize() + 4095) & ~4095;
    count++;
    size += purged ? 0 : o->size();
    liveSize += o->hasClients() ? o->size() : 0;
    decodedSize += o->decodedSize();
    encodedSize += o->encodedSize();
    encodedSizeDuplicatedInDataURLs += o->url().protocolIsData() ? o->encodedSize() : 0;
    purgeableSize += purgeable ? pageSize : 0;
    purgedSize += purged ? pageSize : 0;
}

MemoryCache::Statistics MemoryCache::getStatistics()
{
    Statistics stats;
    CachedResourceMap::iterator e = m_resources.end();
    for (CachedResourceMap::iterator i = m_resources.begin(); i != e; ++i) {
        CachedResource* resource = i->value;
        switch (resource->type()) {
        case CachedResource::ImageResource:
            stats.images.addResource(resource);
            break;
        case CachedResource::CSSStyleSheet:
            stats.cssStyleSheets.addResource(resource);
            break;
        case CachedResource::Script:
            stats.scripts.addResource(resource);
            break;
        case CachedResource::XSLStyleSheet:
            stats.xslStyleSheets.addResource(resource);
            break;
        case CachedResource::FontResource:
            stats.fonts.addResource(resource);
            break;
        default:
            stats.other.addResource(resource);
            break;
        }
    }
    return stats;
}

void MemoryCache::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::MemoryCacheStructures);
    memoryObjectInfo->setClassName("MemoryCache");
    info.addMember(m_resources, "resources");
    info.addMember(m_allResources, "allResources");
    info.addMember(m_liveDecodedResources, "liveDecodedResources");
    for (CachedResourceMap::const_iterator i = m_resources.begin(); i != m_resources.end(); ++i)
        info.addMember(i->value, "cachedResourceItem", WTF::RetainingPointer);
}

void MemoryCache::evictResources()
{
    for (;;) {
        CachedResourceMap::iterator i = m_resources.begin();
        if (i == m_resources.end())
            break;
        evict(i->value);
    }
}

void MemoryCache::prune()
{
    if (m_liveSize + m_deadSize <= m_capacity && m_maxDeadCapacity && m_deadSize <= m_maxDeadCapacity) // Fast path.
        return;
        
    pruneDeadResources(); // Prune dead first, in case it was "borrowing" capacity from live.
    pruneLiveResources();
}

void MemoryCache::pruneToPercentage(float targetPercentLive)
{
    pruneDeadResourcesToPercentage(targetPercentLive); // Prune dead first, in case it was "borrowing" capacity from live.
    pruneLiveResourcesToPercentage(targetPercentLive);
}


#ifdef MEMORY_CACHE_STATS

void MemoryCache::dumpStats(Timer<MemoryCache>*)
{
    Statistics s = getStatistics();
    printf("%-13s %-13s %-13s %-13s %-13s %-13s %-13s\n", "", "Count", "Size", "LiveSize", "DecodedSize", "PurgeableSize", "PurgedSize");
    printf("%-13s %-13s %-13s %-13s %-13s %-13s %-13s\n", "-------------", "-------------", "-------------", "-------------", "-------------", "-------------", "-------------");
    printf("%-13s %13d %13d %13d %13d %13d %13d\n", "Images", s.images.count, s.images.size, s.images.liveSize, s.images.decodedSize, s.images.purgeableSize, s.images.purgedSize);
    printf("%-13s %13d %13d %13d %13d %13d %13d\n", "CSS", s.cssStyleSheets.count, s.cssStyleSheets.size, s.cssStyleSheets.liveSize, s.cssStyleSheets.decodedSize, s.cssStyleSheets.purgeableSize, s.cssStyleSheets.purgedSize);
    printf("%-13s %13d %13d %13d %13d %13d %13d\n", "XSL", s.xslStyleSheets.count, s.xslStyleSheets.size, s.xslStyleSheets.liveSize, s.xslStyleSheets.decodedSize, s.xslStyleSheets.purgeableSize, s.xslStyleSheets.purgedSize);
    printf("%-13s %13d %13d %13d %13d %13d %13d\n", "JavaScript", s.scripts.count, s.scripts.size, s.scripts.liveSize, s.scripts.decodedSize, s.scripts.purgeableSize, s.scripts.purgedSize);
    printf("%-13s %13d %13d %13d %13d %13d %13d\n", "Fonts", s.fonts.count, s.fonts.size, s.fonts.liveSize, s.fonts.decodedSize, s.fonts.purgeableSize, s.fonts.purgedSize);
    printf("%-13s %13d %13d %13d %13d %13d %13d\n", "Other", s.other.count, s.other.size, s.other.liveSize, s.other.decodedSize, s.other.purgeableSize, s.other.purgedSize);
    printf("%-13s %-13s %-13s %-13s %-13s %-13s %-13s\n\n", "-------------", "-------------", "-------------", "-------------", "-------------", "-------------", "-------------");

    printf("Duplication of encoded data from data URLs\n");
    printf("%-13s %13d of %13d\n", "Images",     s.images.encodedSizeDuplicatedInDataURLs,         s.images.encodedSize);
    printf("%-13s %13d of %13d\n", "CSS",        s.cssStyleSheets.encodedSizeDuplicatedInDataURLs, s.cssStyleSheets.encodedSize);
    printf("%-13s %13d of %13d\n", "XSL",        s.xslStyleSheets.encodedSizeDuplicatedInDataURLs, s.xslStyleSheets.encodedSize);
    printf("%-13s %13d of %13d\n", "JavaScript", s.scripts.encodedSizeDuplicatedInDataURLs,        s.scripts.encodedSize);
    printf("%-13s %13d of %13d\n", "Fonts",      s.fonts.encodedSizeDuplicatedInDataURLs,          s.fonts.encodedSize);
    printf("%-13s %13d of %13d\n", "Other",      s.other.encodedSizeDuplicatedInDataURLs,          s.other.encodedSize);
}

void MemoryCache::dumpLRULists(bool includeLive) const
{
    printf("LRU-SP lists in eviction order (Kilobytes decoded, Kilobytes encoded, Access count, Referenced, isPurgeable, wasPurged):\n");

    int size = m_allResources.size();
    for (int i = size - 1; i >= 0; i--) {
        printf("\n\nList %d: ", i);
        CachedResource* current = m_allResources[i].m_tail;
        while (current) {
            CachedResource* prev = current->m_prevInAllResourcesList;
            if (includeLive || !current->hasClients())
                printf("(%.1fK, %.1fK, %uA, %dR, %d, %d); ", current->decodedSize() / 1024.0f, (current->encodedSize() + current->overheadSize()) / 1024.0f, current->accessCount(), current->hasClients(), current->isPurgeable(), current->wasPurged());

            current = prev;
        }
    }
}

#endif // MEMORY_CACHE_STATS

} // namespace WebCore

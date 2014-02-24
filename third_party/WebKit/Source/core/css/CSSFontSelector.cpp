/*
 * Copyright (C) 2007, 2008, 2011 Apple Inc. All rights reserved.
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/css/CSSFontSelector.h"

#include "RuntimeEnabledFeatures.h"
#include "core/css/CSSFontFace.h"
#include "core/css/CSSFontFaceRule.h"
#include "core/css/CSSFontFaceSource.h"
#include "core/css/CSSSegmentedFontFace.h"
#include "core/css/CSSValueList.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/fetch/FontResource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/loader/FrameLoader.h"
#include "core/frame/Frame.h"
#include "core/page/Settings.h"
#include "core/platform/graphics/FontCache.h"
#include "core/platform/graphics/SimpleFontData.h"
#include "wtf/text/AtomicString.h"

using namespace std;

namespace WebCore {

FontLoader::FontLoader(ResourceFetcher* resourceFetcher)
    : m_beginLoadingTimer(this, &FontLoader::beginLoadTimerFired)
    , m_resourceFetcher(resourceFetcher)
{
}

void FontLoader::addFontToBeginLoading(FontResource* fontResource)
{
    if (!m_resourceFetcher)
        return;

    m_fontsToBeginLoading.append(fontResource);
    // FIXME: Use RequestCountTracker??!!
    // Increment the request count now, in order to prevent didFinishLoad from being dispatched
    // after this font has been requested but before it began loading. Balanced by
    // decrementRequestCount() in beginLoadTimerFired() and in clearDocument().
    m_resourceFetcher->incrementRequestCount(fontResource);
    m_beginLoadingTimer.startOneShot(0);
}

void FontLoader::beginLoadTimerFired(Timer<WebCore::FontLoader>*)
{
    ASSERT(m_resourceFetcher);

    Vector<ResourcePtr<FontResource> > fontsToBeginLoading;
    fontsToBeginLoading.swap(m_fontsToBeginLoading);

    for (size_t i = 0; i < fontsToBeginLoading.size(); ++i) {
        fontsToBeginLoading[i]->beginLoadIfNeeded(m_resourceFetcher);
        // Balances incrementRequestCount() in beginLoadingFontSoon().
        m_resourceFetcher->decrementRequestCount(fontsToBeginLoading[i].get());
    }
}

void FontLoader::clearResourceFetcher()
{
    if (!m_resourceFetcher) {
        ASSERT(m_fontsToBeginLoading.isEmpty());
        return;
    }

    m_beginLoadingTimer.stop();

    for (size_t i = 0; i < m_fontsToBeginLoading.size(); ++i) {
        // Balances incrementRequestCount() in beginLoadingFontSoon().
        m_resourceFetcher->decrementRequestCount(m_fontsToBeginLoading[i].get());
    }

    m_fontsToBeginLoading.clear();
    m_resourceFetcher = 0;
}

CSSFontSelector::CSSFontSelector(Document* document)
    : m_document(document)
    , m_fontLoader(document->fetcher())
{
    // FIXME: An old comment used to say there was no need to hold a reference to m_document
    // because "we are guaranteed to be destroyed before the document". But there does not
    // seem to be any such guarantee.

    ASSERT(m_document);
    fontCache()->addClient(this);
}

CSSFontSelector::~CSSFontSelector()
{
    clearDocument();
    fontCache()->removeClient(this);
}

void CSSFontSelector::registerForInvalidationCallbacks(FontSelectorClient* client)
{
    m_clients.add(client);
}

void CSSFontSelector::unregisterForInvalidationCallbacks(FontSelectorClient* client)
{
    m_clients.remove(client);
}

void CSSFontSelector::dispatchInvalidationCallbacks()
{
    Vector<FontSelectorClient*> clients;
    copyToVector(m_clients, clients);
    for (size_t i = 0; i < clients.size(); ++i)
        clients[i]->fontsNeedUpdate(this);
}

void CSSFontSelector::fontLoaded()
{
    dispatchInvalidationCallbacks();
}

void CSSFontSelector::fontCacheInvalidated()
{
    dispatchInvalidationCallbacks();
}

void CSSFontSelector::addFontFaceRule(const StyleRuleFontFace* fontFaceRule)
{
    m_cssSegmentedFontFaceCache.addFontFaceRule(this, fontFaceRule);
}

PassRefPtr<FontData> CSSFontSelector::getFontData(const FontDescription& fontDescription, const AtomicString& familyName)
{
    if (!m_document || !m_document->frame())
        return 0;

    return m_cssSegmentedFontFaceCache.getFontData(m_document->frame()->settings(), fontDescription, familyName);
}

CSSSegmentedFontFace* CSSFontSelector::getFontFace(const FontDescription& fontDescription, const AtomicString& familyName)
{
    return m_cssSegmentedFontFaceCache.getFontFace(fontDescription, familyName);
}

void CSSFontSelector::willUseFontData(const FontDescription& fontDescription, const AtomicString& family)
{
    CSSSegmentedFontFace* face = getFontFace(fontDescription, family);
    if (face)
        face->willUseFontData(fontDescription);
}

void CSSFontSelector::clearDocument()
{
    m_fontLoader.clearResourceFetcher();
    m_document = 0;
}

void CSSFontSelector::beginLoadingFontSoon(FontResource* font)
{
    m_fontLoader.addFontToBeginLoading(font);
}

}

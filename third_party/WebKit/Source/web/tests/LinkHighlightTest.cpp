/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "LinkHighlight.h"

#include <gtest/gtest.h>
#include "FrameTestHelpers.h"
#include "URLTestHelpers.h"
#include "WebFrame.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebInputEvent.h"
#include "WebInputEventConversion.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Node.h"
#include "core/frame/FrameView.h"
#include "core/page/TouchDisambiguation.h"
#include "platform/geometry/IntRect.h"
#include "public/platform/Platform.h"
#include "public/platform/WebContentLayer.h"
#include "public/platform/WebFloatPoint.h"
#include "public/platform/WebSize.h"
#include "public/platform/WebUnitTestSupport.h"
#include "wtf/PassOwnPtr.h"


using namespace WebKit;
using namespace WebCore;

namespace {

TEST(LinkHighlightTest, verifyWebViewImplIntegration)
{
    const std::string baseURL("http://www.test.com/");
    const std::string fileName("test_touch_link_highlight.html");

    URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(baseURL.c_str()), WebString::fromUTF8("test_touch_link_highlight.html"));
    FrameTestHelpers::WebViewHelper webViewHelper;
    WebViewImpl* webViewImpl = webViewHelper.initializeAndLoad(baseURL + fileName, true);
    int pageWidth = 640;
    int pageHeight = 480;
    webViewImpl->resize(WebSize(pageWidth, pageHeight));
    webViewImpl->layout();

    WebGestureEvent touchEvent;
    touchEvent.type = WebInputEvent::GestureShowPress;

    // The coordinates below are linked to absolute positions in the referenced .html file.
    touchEvent.x = 20;
    touchEvent.y = 20;

    {
        PlatformGestureEventBuilder platformEvent(webViewImpl->mainFrameImpl()->frameView(), touchEvent);
        Node* touchNode = webViewImpl->bestTapNode(platformEvent);
        ASSERT_TRUE(touchNode);
    }

    touchEvent.y = 40;
    {
        PlatformGestureEventBuilder platformEvent(webViewImpl->mainFrameImpl()->frameView(), touchEvent);
        EXPECT_FALSE(webViewImpl->bestTapNode(platformEvent));
    }

    touchEvent.y = 20;
    // Shouldn't crash.

    {
        PlatformGestureEventBuilder platformEvent(webViewImpl->mainFrameImpl()->frameView(), touchEvent);
        webViewImpl->enableTapHighlightAtPoint(platformEvent);
    }

    EXPECT_TRUE(webViewImpl->linkHighlight(0));
    EXPECT_TRUE(webViewImpl->linkHighlight(0)->contentLayer());
    EXPECT_TRUE(webViewImpl->linkHighlight(0)->clipLayer());

    // Find a target inside a scrollable div

    touchEvent.y = 100;
    {
        PlatformGestureEventBuilder platformEvent(webViewImpl->mainFrameImpl()->frameView(), touchEvent);
        webViewImpl->enableTapHighlightAtPoint(platformEvent);
    }

    ASSERT_TRUE(webViewImpl->linkHighlight(0));

    // Don't highlight if no "hand cursor"
    touchEvent.y = 220; // An A-link with cross-hair cursor.
    {
        PlatformGestureEventBuilder platformEvent(webViewImpl->mainFrameImpl()->frameView(), touchEvent);
        webViewImpl->enableTapHighlightAtPoint(platformEvent);
    }
    ASSERT_EQ(0U, webViewImpl->numLinkHighlights());

    touchEvent.y = 260; // A text input box.
    {
        PlatformGestureEventBuilder platformEvent(webViewImpl->mainFrameImpl()->frameView(), touchEvent);
        webViewImpl->enableTapHighlightAtPoint(platformEvent);
    }
    ASSERT_EQ(0U, webViewImpl->numLinkHighlights());

    Platform::current()->unitTestSupport()->unregisterAllMockedURLs();
}

class FakeWebFrameClient : public WebFrameClient {
    // To make the destructor public.
};

class FakeCompositingWebViewClient : public WebViewClient {
public:
    virtual ~FakeCompositingWebViewClient()
    {
    }

    virtual void initializeLayerTreeView() OVERRIDE
    {
        m_layerTreeView = adoptPtr(Platform::current()->unitTestSupport()->createLayerTreeViewForTesting(WebUnitTestSupport::TestViewTypeUnitTest));
        ASSERT(m_layerTreeView);
    }

    virtual WebLayerTreeView* layerTreeView() OVERRIDE
    {
        return m_layerTreeView.get();
    }

    FakeWebFrameClient m_fakeWebFrameClient;

private:
    OwnPtr<WebLayerTreeView> m_layerTreeView;
};

static WebViewClient* compositingWebViewClient()
{
    DEFINE_STATIC_LOCAL(FakeCompositingWebViewClient, client, ());
    return &client;
}

TEST(LinkHighlightTest, resetDuringNodeRemoval)
{
    const std::string baseURL("http://www.test.com/");
    const std::string fileName("test_touch_link_highlight.html");

    URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(baseURL.c_str()), WebString::fromUTF8("test_touch_link_highlight.html"));
    FrameTestHelpers::WebViewHelper webViewHelper;
    WebViewImpl* webViewImpl = webViewHelper.initializeAndLoad(baseURL + fileName, true, 0, compositingWebViewClient());

    int pageWidth = 640;
    int pageHeight = 480;
    webViewImpl->resize(WebSize(pageWidth, pageHeight));
    webViewImpl->layout();

    WebGestureEvent touchEvent;
    touchEvent.type = WebInputEvent::GestureShowPress;
    touchEvent.x = 20;
    touchEvent.y = 20;

    PlatformGestureEventBuilder platformEvent(webViewImpl->mainFrameImpl()->frameView(), touchEvent);
    Node* touchNode = webViewImpl->bestTapNode(platformEvent);
    ASSERT_TRUE(touchNode);

    webViewImpl->enableTapHighlightAtPoint(platformEvent);
    ASSERT_TRUE(webViewImpl->linkHighlight(0));

    GraphicsLayer* highlightLayer = webViewImpl->linkHighlight(0)->currentGraphicsLayerForTesting();
    ASSERT_TRUE(highlightLayer);
    EXPECT_TRUE(highlightLayer->linkHighlight(0));

    touchNode->remove(IGNORE_EXCEPTION);
    webViewImpl->layout();
    ASSERT_EQ(0U, highlightLayer->numLinkHighlights());

    Platform::current()->unitTestSupport()->unregisterAllMockedURLs();
}

TEST(LinkHighlightTest, multipleHighlights)
{
    const std::string baseURL("http://www.test.com/");
    const std::string fileName("test_touch_link_highlight.html");

    URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(baseURL.c_str()), WebString::fromUTF8("test_touch_link_highlight.html"));
    FrameTestHelpers::WebViewHelper webViewHelper;
    WebViewImpl* webViewImpl = webViewHelper.initializeAndLoad(baseURL + fileName, true, 0, compositingWebViewClient());

    int pageWidth = 640;
    int pageHeight = 480;
    webViewImpl->resize(WebSize(pageWidth, pageHeight));
    webViewImpl->layout();

    WebGestureEvent touchEvent;
    touchEvent.x = 50;
    touchEvent.y = 310;
    touchEvent.data.tap.width = 30;
    touchEvent.data.tap.height = 30;

    Vector<IntRect> goodTargets;
    Vector<Node*> highlightNodes;
    IntRect boundingBox(touchEvent.x - touchEvent.data.tap.width / 2, touchEvent.y - touchEvent.data.tap.height / 2, touchEvent.data.tap.width, touchEvent.data.tap.height);
    findGoodTouchTargets(boundingBox, webViewImpl->mainFrameImpl()->frame(), goodTargets, highlightNodes);

    webViewImpl->enableTapHighlights(highlightNodes);
    EXPECT_EQ(2U, webViewImpl->numLinkHighlights());

    Platform::current()->unitTestSupport()->unregisterAllMockedURLs();
}

} // namespace

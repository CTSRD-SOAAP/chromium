/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/inspector/InspectorFrontendHost.h"

#include "bindings/v8/ScriptFunctionCall.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/inspector/InspectorController.h"
#include "core/inspector/InspectorFrontendClient.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/TextResourceDecoder.h"
#include "core/page/ContextMenuController.h"
#include "core/page/ContextMenuProvider.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "core/platform/ContextMenu.h"
#include "core/platform/ContextMenuItem.h"
#include "core/platform/Pasteboard.h"
#include "core/platform/network/ResourceError.h"
#include "core/platform/network/ResourceRequest.h"
#include "core/platform/network/ResourceResponse.h"
#include "core/rendering/RenderTheme.h"
#include "modules/filesystem/DOMFileSystem.h"
#include <wtf/StdLibExtras.h>

using namespace std;

namespace WebCore {

class FrontendMenuProvider : public ContextMenuProvider {
public:
    static PassRefPtr<FrontendMenuProvider> create(InspectorFrontendHost* frontendHost, ScriptObject frontendApiObject, const Vector<ContextMenuItem>& items)
    {
        return adoptRef(new FrontendMenuProvider(frontendHost, frontendApiObject, items));
    }

    void disconnect()
    {
        m_frontendApiObject = ScriptObject();
        m_frontendHost = 0;
    }

private:
    FrontendMenuProvider(InspectorFrontendHost* frontendHost, ScriptObject frontendApiObject, const Vector<ContextMenuItem>& items)
        : m_frontendHost(frontendHost)
        , m_frontendApiObject(frontendApiObject)
        , m_items(items)
    {
    }

    virtual ~FrontendMenuProvider()
    {
        contextMenuCleared();
    }

    virtual void populateContextMenu(ContextMenu* menu)
    {
        for (size_t i = 0; i < m_items.size(); ++i)
            menu->appendItem(m_items[i]);
    }

    virtual void contextMenuItemSelected(const ContextMenuItem* item)
    {
        if (m_frontendHost) {
            UserGestureIndicator gestureIndicator(DefinitelyProcessingNewUserGesture);
            int itemNumber = item->action() - ContextMenuItemBaseCustomTag;

            ScriptFunctionCall function(m_frontendApiObject, "contextMenuItemSelected");
            function.appendArgument(itemNumber);
            function.call();
        }
    }

    virtual void contextMenuCleared()
    {
        if (m_frontendHost) {
            ScriptFunctionCall function(m_frontendApiObject, "contextMenuCleared");
            function.call();

            m_frontendHost->m_menuProvider = 0;
        }
        m_items.clear();
    }

    InspectorFrontendHost* m_frontendHost;
    ScriptObject m_frontendApiObject;
    Vector<ContextMenuItem> m_items;
};

InspectorFrontendHost::InspectorFrontendHost(InspectorFrontendClient* client, Page* frontendPage)
    : m_client(client)
    , m_frontendPage(frontendPage)
    , m_menuProvider(0)
{
    ScriptWrappable::init(this);
}

InspectorFrontendHost::~InspectorFrontendHost()
{
    ASSERT(!m_client);
}

void InspectorFrontendHost::disconnectClient()
{
    m_client = 0;
    if (m_menuProvider)
        m_menuProvider->disconnect();
    m_frontendPage = 0;
}

void InspectorFrontendHost::loaded()
{
}

void InspectorFrontendHost::requestSetDockSide(const String& side)
{
    if (!m_client)
        return;
    if (side == "undocked")
        m_client->requestSetDockSide(InspectorFrontendClient::Undocked);
    else if (side == "right")
        m_client->requestSetDockSide(InspectorFrontendClient::DockedToRight);
    else if (side == "bottom")
        m_client->requestSetDockSide(InspectorFrontendClient::DockedToBottom);
}

void InspectorFrontendHost::closeWindow()
{
    if (m_client) {
        m_client->closeWindow();
        disconnectClient(); // Disconnect from client.
    }
}

void InspectorFrontendHost::bringToFront()
{
    if (m_client)
        m_client->bringToFront();
}

void InspectorFrontendHost::setZoomFactor(float zoom)
{
    m_frontendPage->mainFrame()->setPageAndTextZoomFactors(zoom, 1);
}

void InspectorFrontendHost::inspectedURLChanged(const String& newURL)
{
    if (m_client)
        m_client->inspectedURLChanged(newURL);
}

void InspectorFrontendHost::setAttachedWindowHeight(unsigned height)
{
    if (m_client)
        m_client->changeAttachedWindowHeight(height);
}

void InspectorFrontendHost::moveWindowBy(float x, float y) const
{
    if (m_client)
        m_client->moveWindowBy(x, y);
}

void InspectorFrontendHost::setInjectedScriptForOrigin(const String& origin, const String& script)
{
    ASSERT(m_frontendPage->inspectorController());
    m_frontendPage->inspectorController()->setInjectedScriptForOrigin(origin, script);
}

String InspectorFrontendHost::localizedStringsURL()
{
    return "";
}

void InspectorFrontendHost::copyText(const String& text)
{
    Pasteboard::generalPasteboard()->writePlainText(text, Pasteboard::CannotSmartReplace);
}

void InspectorFrontendHost::openInNewTab(const String& url)
{
    if (m_client)
        m_client->openInNewTab(url);
}

bool InspectorFrontendHost::canSave()
{
    return true;
}

void InspectorFrontendHost::save(const String& url, const String& content, bool forceSaveAs)
{
    if (m_client)
        m_client->save(url, content, forceSaveAs);
}

void InspectorFrontendHost::append(const String& url, const String& content)
{
    if (m_client)
        m_client->append(url, content);
}

void InspectorFrontendHost::close(const String&)
{
}

void InspectorFrontendHost::sendMessageToBackend(const String& message)
{
    if (m_client)
        m_client->sendMessageToBackend(message);
}

void InspectorFrontendHost::showContextMenu(Event* event, const Vector<ContextMenuItem>& items)
{
    if (!event)
        return;

    ASSERT(m_frontendPage);
    ScriptState* frontendScriptState = mainWorldScriptState(m_frontendPage->mainFrame());
    ScriptObject frontendApiObject;
    if (!ScriptGlobalObject::get(frontendScriptState, "InspectorFrontendAPI", frontendApiObject)) {
        ASSERT_NOT_REACHED();
        return;
    }
    RefPtr<FrontendMenuProvider> menuProvider = FrontendMenuProvider::create(this, frontendApiObject, items);
    ContextMenuController* menuController = m_frontendPage->contextMenuController();
    menuController->showContextMenu(event, menuProvider);
    m_menuProvider = menuProvider.get();
}

String InspectorFrontendHost::loadResourceSynchronously(const String& url)
{
    ResourceRequest request(url);
    request.setHTTPMethod("GET");

    Vector<char> data;
    ResourceError error;
    ResourceResponse response;
    m_frontendPage->mainFrame()->loader()->loadResourceSynchronously(request, DoNotAllowStoredCredentials, error, response, data);
    WTF::TextEncoding textEncoding(response.textEncodingName());
    bool useDetector = false;
    if (!textEncoding.isValid()) {
        textEncoding = UTF8Encoding();
        useDetector = true;
    }
    RefPtr<TextResourceDecoder> decoder = TextResourceDecoder::create("text/plain", textEncoding, useDetector);
    return decoder->decode(data.data(), data.size()) + decoder->flush();
}

String InspectorFrontendHost::getSelectionBackgroundColor()
{
    Color color = m_frontendPage->theme()->activeSelectionBackgroundColor();
    return color.isValid() ? color.serialized() : "";
}

String InspectorFrontendHost::getSelectionForegroundColor()
{
    Color color = m_frontendPage->theme()->activeSelectionForegroundColor();
    return color.isValid() ? color.serialized() : "";
}

bool InspectorFrontendHost::supportsFileSystems()
{
    return true;
}

void InspectorFrontendHost::requestFileSystems()
{
    if (m_client)
        m_client->requestFileSystems();
}

void InspectorFrontendHost::addFileSystem()
{
    if (m_client)
        m_client->addFileSystem();
}

void InspectorFrontendHost::removeFileSystem(const String& fileSystemPath)
{
    if (m_client)
        m_client->removeFileSystem(fileSystemPath);
}

PassRefPtr<DOMFileSystem> InspectorFrontendHost::isolatedFileSystem(const String& fileSystemName, const String& rootURL)
{
    ScriptExecutionContext* context = m_frontendPage->mainFrame()->document();
    return DOMFileSystem::create(context, fileSystemName, FileSystemTypeIsolated, KURL(ParsedURLString, rootURL), AsyncFileSystem::create());
}

bool InspectorFrontendHost::isUnderTest()
{
    return m_client && m_client->isUnderTest();
}

bool InspectorFrontendHost::canSaveAs()
{
    return false;
}

bool InspectorFrontendHost::canInspectWorkers()
{
    return false;
}

String InspectorFrontendHost::hiddenPanels()
{
    return "";
}

} // namespace WebCore

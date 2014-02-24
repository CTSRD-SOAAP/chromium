/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2009, 2011, 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Notification_h
#define Notification_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventTarget.h"
#include "core/events/ThreadLocalEventNames.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "modules/notifications/NotificationClient.h"
#include "platform/AsyncMethodRunner.h"
#include "platform/SharedBuffer.h"
#include "platform/text/TextDirection.h"
#include "weborigin/KURL.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/text/AtomicStringHash.h"

namespace WebCore {

class Dictionary;
class ExceptionState;
class NotificationCenter;
class NotificationPermissionCallback;
class ResourceError;
class ResourceResponse;
class ExecutionContext;
class ThreadableLoader;

class Notification : public RefCounted<Notification>, public ScriptWrappable, public ActiveDOMObject, public EventTargetWithInlineData {
    WTF_MAKE_FAST_ALLOCATED;
    REFCOUNTED_EVENT_TARGET(Notification);
public:
    Notification();
#if ENABLE(LEGACY_NOTIFICATIONS)
    static PassRefPtr<Notification> create(const String& title, const String& body, const String& iconURI, ExecutionContext*, ExceptionState&, PassRefPtr<NotificationCenter> provider);
#endif
    static PassRefPtr<Notification> create(ExecutionContext*, const String& title, const Dictionary& options);

    virtual ~Notification();

    void show();
#if ENABLE(LEGACY_NOTIFICATIONS)
    void cancel() { close(); }
#endif
    void close();

    KURL iconURL() const { return m_icon; }
    void setIconURL(const KURL& url) { m_icon = url; }

    String title() const { return m_title; }
    String body() const { return m_body; }

    String lang() const { return m_lang; }
    void setLang(const String& lang) { m_lang = lang; }

    String dir() const { return m_direction; }
    void setDir(const String& dir) { m_direction = dir; }

#if ENABLE(LEGACY_NOTIFICATIONS)
    String replaceId() const { return tag(); }
    void setReplaceId(const String& replaceId) { setTag(replaceId); }
#endif

    String tag() const { return m_tag; }
    void setTag(const String& tag) { m_tag = tag; }

    TextDirection direction() const { return dir() == "rtl" ? RTL : LTR; }

#if ENABLE(LEGACY_NOTIFICATIONS)
    DEFINE_MAPPED_ATTRIBUTE_EVENT_LISTENER(display, show);
#endif
    DEFINE_ATTRIBUTE_EVENT_LISTENER(show);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(close);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(click);

    void dispatchClickEvent();
    void dispatchCloseEvent();
    void dispatchErrorEvent();
    void dispatchShowEvent();

    // EventTarget interface
    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE { return ActiveDOMObject::executionContext(); }
    virtual bool dispatchEvent(PassRefPtr<Event>) OVERRIDE;

    // ActiveDOMObject interface
    virtual void contextDestroyed();

    void stopLoadingIcon();

    // Deprecated. Use functions from NotificationCenter.
    void detachPresenter() { }

    void finalize();

    static const String& permission(ExecutionContext*);
    static const String& permissionString(NotificationClient::Permission);
    static void requestPermission(ExecutionContext*, PassRefPtr<NotificationPermissionCallback> = 0);

private:
#if ENABLE(LEGACY_NOTIFICATIONS)
    Notification(const String& title, const String& body, const String& iconURI, ExecutionContext*, ExceptionState&, PassRefPtr<NotificationCenter>);
#endif
    Notification(ExecutionContext*, const String& title);

    void setBody(const String& body) { m_body = body; }

    void startLoadingIcon();
    void finishLoadingIcon();

    void showSoon();

    // Text notifications.
    KURL m_icon;
    String m_title;
    String m_body;

    String m_direction;
    String m_lang;
    String m_tag;

    enum NotificationState {
        Idle = 0,
        Showing = 1,
        Closed = 2,
    };

    NotificationState m_state;

    NotificationClient* m_notificationClient;

    OwnPtr<AsyncMethodRunner<Notification> > m_asyncRunner;
};

} // namespace WebCore

#endif // Notifications_h

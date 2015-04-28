// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_tab_helper.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

#if defined(ENABLE_EXTENSIONS)
#include "extensions/common/extension_messages.h"
#endif

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SessionTabHelper);

SessionTabHelper::SessionTabHelper(content::WebContents* contents)
    : content::WebContentsObserver(contents) {
}

SessionTabHelper::~SessionTabHelper() {
}

void SessionTabHelper::SetWindowID(const SessionID& id) {
  window_id_ = id;

#if defined(ENABLE_EXTENSIONS)
  // Extension code in the renderer holds the ID of the window that hosts it.
  // Notify it that the window ID changed.
  web_contents()->GetRenderViewHost()->Send(
          new ExtensionMsg_UpdateBrowserWindowId(
          web_contents()->GetRenderViewHost()->GetRoutingID(), id.id()));
#endif
}

// static
SessionID::id_type SessionTabHelper::IdForTab(const content::WebContents* tab) {
  const SessionTabHelper* session_tab_helper =
      tab ? SessionTabHelper::FromWebContents(tab) : NULL;
  return session_tab_helper ? session_tab_helper->session_id().id() : -1;
}

// static
SessionID::id_type SessionTabHelper::IdForWindowContainingTab(
    const content::WebContents* tab) {
  const SessionTabHelper* session_tab_helper =
      tab ? SessionTabHelper::FromWebContents(tab) : NULL;
  return session_tab_helper ? session_tab_helper->window_id().id() : -1;
}

#if defined(ENABLE_EXTENSIONS)
void SessionTabHelper::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  render_view_host->Send(
      new ExtensionMsg_UpdateBrowserWindowId(render_view_host->GetRoutingID(),
                                             window_id_.id()));
}
#endif

void SessionTabHelper::UserAgentOverrideSet(const std::string& user_agent) {
#if defined(ENABLE_SESSION_SERVICE)
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  SessionService* session = SessionServiceFactory::GetForProfile(profile);
  if (session)
    session->SetTabUserAgentOverride(window_id(), session_id(), user_agent);
#endif
}

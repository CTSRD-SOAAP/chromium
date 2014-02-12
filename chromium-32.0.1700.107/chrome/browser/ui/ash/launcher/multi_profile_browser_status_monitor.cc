// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/multi_profile_browser_status_monitor.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "google_apis/gaia/gaia_auth_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

MultiProfileBrowserStatusMonitor::MultiProfileBrowserStatusMonitor(
    ChromeLauncherController* launcher_controller)
    : BrowserStatusMonitor(launcher_controller),
      launcher_controller_(launcher_controller) {
}

MultiProfileBrowserStatusMonitor::~MultiProfileBrowserStatusMonitor() {
}

void MultiProfileBrowserStatusMonitor::ActiveUserChanged(
    const std::string& user_email) {
  for (AppList::iterator it = app_list_.begin(); it != app_list_.end(); ++it) {
    bool owned = IsV1AppOwnedByCurrentUser(*it);
    bool shown = IsV1AppInShelf(*it);
    if (owned && !shown)
      ConnectV1AppToLauncher(*it);
    else if (!owned && shown)
      DisconnectV1AppFromLauncher(*it);
  }
}

void MultiProfileBrowserStatusMonitor::AddV1AppToShelf(Browser* browser) {
  DCHECK(browser->is_type_popup() && browser->is_app());
  DCHECK(std::find(app_list_.begin(), app_list_.end(), browser) ==
             app_list_.end());
  app_list_.push_back(browser);
  if (IsV1AppOwnedByCurrentUser(browser)) {
    BrowserStatusMonitor::AddV1AppToShelf(browser);
  }
}

void MultiProfileBrowserStatusMonitor::RemoveV1AppFromShelf(Browser* browser) {
  DCHECK(browser->is_type_popup() && browser->is_app());
  AppList::iterator it = std::find(app_list_.begin(), app_list_.end(), browser);
  DCHECK(it != app_list_.end());
  app_list_.erase(it);
  if (IsV1AppInShelf(browser)) {
    BrowserStatusMonitor::RemoveV1AppFromShelf(browser);
  }
}

bool MultiProfileBrowserStatusMonitor::IsV1AppOwnedByCurrentUser(
    Browser* browser) {
  Profile* profile = browser->profile()->GetOriginalProfile();
#if defined(OS_CHROMEOS)
  return
     gaia::CanonicalizeEmail(gaia::SanitizeEmail(profile->GetProfileName())) ==
         chromeos::UserManager::Get()->GetActiveUser()->email();
#else
  return profile == ProfileManager::GetDefaultProfile();
#endif
}

void MultiProfileBrowserStatusMonitor::ConnectV1AppToLauncher(
    Browser* browser) {
  // Adding a V1 app to the launcher consists of two actions: Add the browser
  // (launcher item) and add the content (launcher item status).
  BrowserStatusMonitor::AddV1AppToShelf(browser);
  launcher_controller_->UpdateAppState(
      browser->tab_strip_model()->GetActiveWebContents(),
      ChromeLauncherController::APP_STATE_INACTIVE);
}

void MultiProfileBrowserStatusMonitor::DisconnectV1AppFromLauncher(
    Browser* browser) {
  // Removing a V1 app from the launcher requires to remove the content and
  // the launcher item.
  launcher_controller_->UpdateAppState(
      browser->tab_strip_model()->GetActiveWebContents(),
      ChromeLauncherController::APP_STATE_REMOVED);
  BrowserStatusMonitor::RemoveV1AppFromShelf(browser);
}


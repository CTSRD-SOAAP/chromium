// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/save_password_infobar_delegate.h"

#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_urls.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(ENABLE_ONE_CLICK_SIGNIN)
#include "chrome/browser/ui/sync/one_click_signin_helper.h"
#endif

// static
void SavePasswordInfoBarDelegate::Create(
    content::WebContents* web_contents,
    scoped_ptr<password_manager::PasswordFormManager> form_to_save,
    const std::string& uma_histogram_suffix) {
#if defined(ENABLE_ONE_CLICK_SIGNIN)
  // Don't show the password manager infobar if this form is for a google
  // account and we are going to show the one-click signin infobar.
  GURL realm(form_to_save->realm());
  // TODO(mathp): Checking only against associated_username() causes a bug
  // referenced here: crbug.com/133275
  // TODO(vabr): The check IsEnableWebBasedSignin is a hack for the time when
  // OneClickSignin is disabled. http://crbug.com/339804
  if (((realm == GaiaUrls::GetInstance()->gaia_login_form_realm()) ||
       (realm == GURL("https://www.google.com/"))) &&
      switches::IsEnableWebBasedSignin() &&
      OneClickSigninHelper::CanOffer(
          web_contents,
          OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
          base::UTF16ToUTF8(form_to_save->associated_username()),
          NULL))
    return;
#endif

  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(new SavePasswordInfoBarDelegate(
          form_to_save.Pass(), uma_histogram_suffix))));
}

SavePasswordInfoBarDelegate::~SavePasswordInfoBarDelegate() {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.InfoBarResponse",
                            infobar_response_,
                            password_manager::metrics_util::NUM_RESPONSE_TYPES);

  password_manager::metrics_util::LogUIDismissalReason(infobar_response_);

  // The shortest period for which the prompt needs to live, so that we don't
  // consider it killed prematurely, as might happen, e.g., if a pre-rendered
  // page gets swapped in (and the current WebContents is destroyed).
  const base::TimeDelta kMinimumPromptDisplayTime =
      base::TimeDelta::FromSeconds(1);

  if (!uma_histogram_suffix_.empty()) {
    password_manager::metrics_util::LogUMAHistogramEnumeration(
        "PasswordManager.SavePasswordPromptResponse_" + uma_histogram_suffix_,
        infobar_response_,
        password_manager::metrics_util::NUM_RESPONSE_TYPES);
    password_manager::metrics_util::LogUMAHistogramBoolean(
        "PasswordManager.SavePasswordPromptDisappearedQuickly_" +
            uma_histogram_suffix_,
        timer_.Elapsed() < kMinimumPromptDisplayTime);
  }
}

SavePasswordInfoBarDelegate::SavePasswordInfoBarDelegate(
    scoped_ptr<password_manager::PasswordFormManager> form_to_save,
    const std::string& uma_histogram_suffix)
    : ConfirmInfoBarDelegate(),
      form_to_save_(form_to_save.Pass()),
      infobar_response_(password_manager::metrics_util::NO_RESPONSE),
      uma_histogram_suffix_(uma_histogram_suffix) {
  if (!uma_histogram_suffix_.empty()) {
    password_manager::metrics_util::LogUMAHistogramBoolean(
        "PasswordManager.SavePasswordPromptDisplayed_" + uma_histogram_suffix_,
        true);
  }
}

bool SavePasswordInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return !details.is_redirect &&
         infobars::InfoBarDelegate::ShouldExpire(details);
}

int SavePasswordInfoBarDelegate::GetIconID() const {
  return IDR_INFOBAR_SAVE_PASSWORD;
}

infobars::InfoBarDelegate::Type SavePasswordInfoBarDelegate::GetInfoBarType()
    const {
  return PAGE_ACTION_TYPE;
}

base::string16 SavePasswordInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT);
}

base::string16 SavePasswordInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_PASSWORD_MANAGER_SAVE_BUTTON : IDS_PASSWORD_MANAGER_BLACKLIST_BUTTON);
}

bool SavePasswordInfoBarDelegate::Accept() {
  DCHECK(form_to_save_.get());
  form_to_save_->Save();
  infobar_response_ = password_manager::metrics_util::REMEMBER_PASSWORD;
  return true;
}

bool SavePasswordInfoBarDelegate::Cancel() {
  DCHECK(form_to_save_.get());
  form_to_save_->PermanentlyBlacklist();
  infobar_response_ = password_manager::metrics_util::NEVER_REMEMBER_PASSWORD;
  return true;
}

void SavePasswordInfoBarDelegate::InfoBarDismissed() {
  DCHECK(form_to_save_.get());
  infobar_response_ = password_manager::metrics_util::INFOBAR_DISMISSED;
}

infobars::InfoBarDelegate::InfoBarAutomationType
SavePasswordInfoBarDelegate::GetInfoBarAutomationType() const {
  return PASSWORD_INFOBAR;
}

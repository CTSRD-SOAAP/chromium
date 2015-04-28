// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/inline_login_handler_impl.h"

#include <string>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/local_auth.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/sync/one_click_signin_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/inline_login_ui.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/url_constants.h"
#include "components/signin/core/browser/about_signin_internals.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"

namespace {

class InlineSigninHelper : public GaiaAuthConsumer {
 public:
  InlineSigninHelper(
      base::WeakPtr<InlineLoginHandlerImpl> handler,
      net::URLRequestContextGetter* getter,
      Profile* profile,
      const GURL& current_url,
      const std::string& email,
      const std::string& gaia_id,
      const std::string& password,
      const std::string& session_index,
      const std::string& signin_scoped_device_id,
      bool choose_what_to_sync,
      bool confirm_untrusted_signin);

 private:
  // Overridden from GaiaAuthConsumer.
  void OnClientOAuthSuccess(const ClientOAuthResult& result) override;
  void OnClientOAuthFailure(const GoogleServiceAuthError& error)
      override;

  GaiaAuthFetcher gaia_auth_fetcher_;
  base::WeakPtr<InlineLoginHandlerImpl> handler_;
  Profile* profile_;
  GURL current_url_;
  std::string email_;
  std::string gaia_id_;
  std::string password_;
  std::string session_index_;
  bool choose_what_to_sync_;
  bool confirm_untrusted_signin_;

  DISALLOW_COPY_AND_ASSIGN(InlineSigninHelper);
};

InlineSigninHelper::InlineSigninHelper(
    base::WeakPtr<InlineLoginHandlerImpl> handler,
    net::URLRequestContextGetter* getter,
    Profile* profile,
    const GURL& current_url,
    const std::string& email,
    const std::string& gaia_id,
    const std::string& password,
    const std::string& session_index,
    const std::string& signin_scoped_device_id,
    bool choose_what_to_sync,
    bool confirm_untrusted_signin)
    : gaia_auth_fetcher_(this, GaiaConstants::kChromeSource, getter),
      handler_(handler),
      profile_(profile),
      current_url_(current_url),
      email_(email),
      gaia_id_(gaia_id),
      password_(password),
      session_index_(session_index),
      choose_what_to_sync_(choose_what_to_sync),
      confirm_untrusted_signin_(confirm_untrusted_signin) {
  DCHECK(profile_);
  DCHECK(!email_.empty());
  gaia_auth_fetcher_.StartCookieForOAuthLoginTokenExchangeWithDeviceId(
      session_index, signin_scoped_device_id);
}

void InlineSigninHelper::OnClientOAuthSuccess(const ClientOAuthResult& result) {
  content::WebContents* contents = NULL;
  Browser* browser = NULL;
  if (handler_) {
    contents = handler_->web_ui()->GetWebContents();
    browser = handler_->GetDesktopBrowser();
  }

  AboutSigninInternals* about_signin_internals =
      AboutSigninInternalsFactory::GetForProfile(profile_);
  about_signin_internals->OnRefreshTokenReceived("Successful");

  AccountTrackerService* account_tracker =
      AccountTrackerServiceFactory::GetForProfile(profile_);
  std::string account_id =
      account_tracker->PickAccountIdForAccount(gaia_id_, email_);

  // Prime the account tracker with this combination of gaia id/display email.
  account_tracker->SeedAccountInfo(gaia_id_, email_);

  signin_metrics::Source source = signin::GetSourceForPromoURL(current_url_);

  SigninManager* signin_manager = SigninManagerFactory::GetForProfile(profile_);
  std::string primary_email = signin_manager->GetAuthenticatedUsername();
  if (gaia::AreEmailsSame(email_, primary_email) &&
      source == signin_metrics::SOURCE_REAUTH &&
      switches::IsNewProfileManagement() &&
      !password_.empty() &&
      profiles::IsLockAvailable(profile_)) {
    LocalAuth::SetLocalAuthCredentials(profile_, password_);
  }

  if (source == signin_metrics::SOURCE_AVATAR_BUBBLE_ADD_ACCOUNT ||
      source == signin_metrics::SOURCE_REAUTH) {
    ProfileOAuth2TokenServiceFactory::GetForProfile(profile_)->
        UpdateCredentials(account_id, result.refresh_token);

    if (signin::IsAutoCloseEnabledInURL(current_url_)) {
      // Close the gaia sign in tab via a task to make sure we aren't in the
      // middle of any webui handler code.
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&InlineLoginHandlerImpl::CloseTab,
          handler_,
          signin::ShouldShowAccountManagement(current_url_)));
    }

    if (source == signin_metrics::SOURCE_REAUTH)
      signin_manager->MergeSigninCredentialIntoCookieJar();
  } else {
    ProfileSyncService* sync_service =
        ProfileSyncServiceFactory::GetForProfile(profile_);
    SigninErrorController* error_controller =
        SigninErrorControllerFactory::GetForProfile(profile_);

    bool is_new_avatar_menu = switches::IsNewAvatarMenu();

    OneClickSigninSyncStarter::StartSyncMode start_mode;
    if (source == signin_metrics::SOURCE_SETTINGS || choose_what_to_sync_) {
      bool show_settings_without_configure =
          error_controller->HasError() &&
          sync_service &&
          sync_service->HasSyncSetupCompleted();
      start_mode = show_settings_without_configure ?
          OneClickSigninSyncStarter::SHOW_SETTINGS_WITHOUT_CONFIGURE :
          OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST;
    } else {
      start_mode = is_new_avatar_menu ?
          OneClickSigninSyncStarter::CONFIRM_SYNC_SETTINGS_FIRST :
          OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS;
    }

    OneClickSigninSyncStarter::ConfirmationRequired confirmation_required;
    if (confirm_untrusted_signin_) {
      confirmation_required =
          OneClickSigninSyncStarter::CONFIRM_UNTRUSTED_SIGNIN;
    } else if (is_new_avatar_menu) {
      confirmation_required = OneClickSigninSyncStarter::CONFIRM_AFTER_SIGNIN;
    } else {
      confirmation_required =
          source == signin_metrics::SOURCE_SETTINGS ||
          choose_what_to_sync_ ?
              OneClickSigninSyncStarter::NO_CONFIRMATION :
              OneClickSigninSyncStarter::CONFIRM_AFTER_SIGNIN;
    }

    bool start_signin =
        !OneClickSigninHelper::HandleCrossAccountError(
            profile_, "",
            email_, password_, result.refresh_token,
            OneClickSigninHelper::AUTO_ACCEPT_EXPLICIT,
            source, start_mode,
            base::Bind(&InlineLoginHandlerImpl::SyncStarterCallback,
                       handler_));
    if (start_signin) {
      // Call OneClickSigninSyncStarter to exchange oauth code for tokens.
      // OneClickSigninSyncStarter will delete itself once the job is done.
      new OneClickSigninSyncStarter(
          profile_, browser,
          email_, password_, result.refresh_token,
          start_mode,
          contents,
          confirmation_required,
          signin::GetNextPageURLForPromoURL(current_url_),
          base::Bind(&InlineLoginHandlerImpl::SyncStarterCallback, handler_));
    }
  }

  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void InlineSigninHelper::OnClientOAuthFailure(
  const GoogleServiceAuthError& error) {
  if (handler_)
    handler_->HandleLoginError(error.ToString());

  AboutSigninInternals* about_signin_internals =
    AboutSigninInternalsFactory::GetForProfile(profile_);
  about_signin_internals->OnRefreshTokenReceived("Failure");

  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

}  // namespace

InlineLoginHandlerImpl::InlineLoginHandlerImpl()
      : confirm_untrusted_signin_(false),
        weak_factory_(this) {
}

InlineLoginHandlerImpl::~InlineLoginHandlerImpl() {}

// This method is not called with webview sign in enabled.
void InlineLoginHandlerImpl::DidCommitProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& url,
    ui::PageTransition transition_type) {
  if (!web_contents())
    return;

  // Returns early if this is not a gaia iframe navigation.
  const GURL kGaiaExtOrigin(
      "chrome-extension://mfffpogegjflfpflabcdkioaeobkgjik/");
  content::RenderFrameHost* gaia_iframe = InlineLoginUI::GetAuthIframe(
      web_contents(), kGaiaExtOrigin, "signin-frame");
  if (render_frame_host != gaia_iframe)
    return;

  // Loading any untrusted (e.g., HTTP) URLs in the privileged sign-in process
  // will require confirmation before the sign in takes effect.
  if (!url.is_empty()) {
    GURL origin(url.GetOrigin());
    if (url.spec() != url::kAboutBlankURL &&
        origin != kGaiaExtOrigin &&
        !gaia::IsGaiaSignonRealm(origin)) {
      confirm_untrusted_signin_ = true;
    }
  }
}

void InlineLoginHandlerImpl::SetExtraInitParams(base::DictionaryValue& params) {
  params.SetString("service", "chromiumsync");

  content::WebContents* contents = web_ui()->GetWebContents();
  const GURL& current_url = contents->GetURL();
  std::string is_constrained;
  net::GetValueForKeyInQuery(current_url, "constrained", &is_constrained);

  content::WebContentsObserver::Observe(contents);
  OneClickSigninHelper::LogHistogramValue(signin_metrics::HISTOGRAM_SHOWN);
}

void InlineLoginHandlerImpl::CompleteLogin(const base::ListValue* args) {
  content::WebContents* contents = web_ui()->GetWebContents();
  const GURL& current_url = contents->GetURL();

  const base::DictionaryValue* dict = NULL;
  args->GetDictionary(0, &dict);

  bool skip_for_now = false;
  dict->GetBoolean("skipForNow", &skip_for_now);
  if (skip_for_now) {
    signin::SetUserSkippedPromo(Profile::FromWebUI(web_ui()));
    SyncStarterCallback(OneClickSigninSyncStarter::SYNC_SETUP_FAILURE);
    return;
  }

  // This value exists only for webview sign in.
  bool trusted = false;
  if (dict->GetBoolean("trusted", &trusted))
    confirm_untrusted_signin_ = !trusted;

  base::string16 email_string16;
  dict->GetString("email", &email_string16);
  DCHECK(!email_string16.empty());
  std::string email(base::UTF16ToASCII(email_string16));

  base::string16 password_string16;
  dict->GetString("password", &password_string16);
  std::string password(base::UTF16ToASCII(password_string16));

  base::string16 gaia_id_string16;
  dict->GetString("gaiaId", &gaia_id_string16);
  DCHECK(!gaia_id_string16.empty());
  std::string gaia_id = base::UTF16ToASCII(gaia_id_string16);

  // When doing a SAML sign in, this email check may result in a false
  // positive.  This happens when the user types one email address in the
  // gaia sign in page, but signs in to a different account in the SAML sign in
  // page.
  std::string default_email;
  std::string validate_email;
  if (net::GetValueForKeyInQuery(current_url, "email", &default_email) &&
      net::GetValueForKeyInQuery(current_url, "validateEmail",
                                 &validate_email) &&
      validate_email == "1") {
    if (!gaia::AreEmailsSame(email, default_email)) {
      SyncStarterCallback(OneClickSigninSyncStarter::SYNC_SETUP_FAILURE);
      return;
    }
  }

  base::string16 session_index_string16;
  dict->GetString("sessionIndex", &session_index_string16);
  std::string session_index = base::UTF16ToASCII(session_index_string16);
  DCHECK(!session_index.empty());

  bool choose_what_to_sync = false;
  dict->GetBoolean("chooseWhatToSync", &choose_what_to_sync);

  signin_metrics::Source source = signin::GetSourceForPromoURL(current_url);
  OneClickSigninHelper::LogHistogramValue(signin_metrics::HISTOGRAM_ACCEPTED);
  bool switch_to_advanced =
      choose_what_to_sync && (source != signin_metrics::SOURCE_SETTINGS);
  OneClickSigninHelper::LogHistogramValue(
      switch_to_advanced ? signin_metrics::HISTOGRAM_WITH_ADVANCED :
                           signin_metrics::HISTOGRAM_WITH_DEFAULTS);

  OneClickSigninHelper::CanOfferFor can_offer_for =
      OneClickSigninHelper::CAN_OFFER_FOR_ALL;
  switch (source) {
    case signin_metrics::SOURCE_AVATAR_BUBBLE_ADD_ACCOUNT:
      can_offer_for = OneClickSigninHelper::CAN_OFFER_FOR_SECONDARY_ACCOUNT;
      break;
    case signin_metrics::SOURCE_REAUTH: {
      std::string primary_username =
          SigninManagerFactory::GetForProfile(
              Profile::FromWebUI(web_ui()))->GetAuthenticatedUsername();
      if (!gaia::AreEmailsSame(default_email, primary_username))
        can_offer_for = OneClickSigninHelper::CAN_OFFER_FOR_SECONDARY_ACCOUNT;
      break;
    }
    default:
      // No need to change |can_offer_for|.
      break;
  }

  std::string error_msg;
  bool can_offer = OneClickSigninHelper::CanOffer(
      contents, can_offer_for, email, &error_msg);
  if (!can_offer) {
    HandleLoginError(error_msg);
    return;
  }

  AboutSigninInternals* about_signin_internals =
      AboutSigninInternalsFactory::GetForProfile(Profile::FromWebUI(web_ui()));
  about_signin_internals->OnAuthenticationResultReceived(
      "GAIA Auth Successful");

  content::StoragePartition* partition =
      content::BrowserContext::GetStoragePartitionForSite(
          contents->GetBrowserContext(), signin::GetSigninPartitionURL());

  SigninClient* signin_client =
      ChromeSigninClientFactory::GetForProfile(Profile::FromWebUI(web_ui()));
  std::string signin_scoped_device_id =
      signin_client->GetSigninScopedDeviceId();
  // InlineSigninHelper will delete itself.
  new InlineSigninHelper(GetWeakPtr(), partition->GetURLRequestContext(),
                         Profile::FromWebUI(web_ui()), current_url,
                         email, gaia_id, password, session_index,
                         signin_scoped_device_id, choose_what_to_sync,
                         confirm_untrusted_signin_);

  web_ui()->CallJavascriptFunction("inline.login.closeDialog");
}

void InlineLoginHandlerImpl::HandleLoginError(const std::string& error_msg) {
  SyncStarterCallback(OneClickSigninSyncStarter::SYNC_SETUP_FAILURE);

  Browser* browser = GetDesktopBrowser();
  if (browser && !error_msg.empty()) {
    LoginUIServiceFactory::GetForProfile(Profile::FromWebUI(web_ui()))->
        DisplayLoginResult(browser, base::UTF8ToUTF16(error_msg));
  }
}

Browser* InlineLoginHandlerImpl::GetDesktopBrowser() {
  Browser* browser = chrome::FindBrowserWithWebContents(
      web_ui()->GetWebContents());
  if (!browser) {
    browser = chrome::FindLastActiveWithProfile(
        Profile::FromWebUI(web_ui()), chrome::GetActiveDesktop());
  }
  return browser;
}

void InlineLoginHandlerImpl::SyncStarterCallback(
    OneClickSigninSyncStarter::SyncSetupResult result) {
  content::WebContents* contents = web_ui()->GetWebContents();

  if (contents->GetController().GetPendingEntry()) {
    // Do nothing if a navigation is pending, since this call can be triggered
    // from DidStartLoading. This avoids deleting the pending entry while we are
    // still navigating to it. See crbug/346632.
    return;
  }

  const GURL& current_url = contents->GetLastCommittedURL();
  signin_metrics::Source source = signin::GetSourceForPromoURL(current_url);
  bool auto_close = signin::IsAutoCloseEnabledInURL(current_url);

  if (result == OneClickSigninSyncStarter::SYNC_SETUP_FAILURE) {
    OneClickSigninHelper::RedirectToNtpOrAppsPage(contents, source);
  } else if (auto_close) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&InlineLoginHandlerImpl::CloseTab,
                   weak_factory_.GetWeakPtr(),
                   signin::ShouldShowAccountManagement(current_url)));
  } else {
     OneClickSigninHelper::RedirectToNtpOrAppsPageIfNecessary(contents, source);
  }
}

void InlineLoginHandlerImpl::CloseTab(bool show_account_management) {
  content::WebContents* tab = web_ui()->GetWebContents();
  Browser* browser = chrome::FindBrowserWithWebContents(tab);
  if (browser) {
    TabStripModel* tab_strip_model = browser->tab_strip_model();
    if (tab_strip_model) {
      int index = tab_strip_model->GetIndexOfWebContents(tab);
      if (index != TabStripModel::kNoTab) {
        tab_strip_model->ExecuteContextMenuCommand(
            index, TabStripModel::CommandCloseTab);
      }
    }

    if (show_account_management) {
      browser->window()->ShowAvatarBubbleFromAvatarButton(
            BrowserWindow::AVATAR_BUBBLE_MODE_ACCOUNT_MANAGEMENT,
            signin::ManageAccountsParams());
    }
  }
}

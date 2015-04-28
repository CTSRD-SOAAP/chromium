// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extension_settings_handler.h"

#include "apps/app_load_service.h"
#include "apps/saved_files_service.h"
#include "base/auto_reset.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/background/background_contents.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/devtools_util.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_disabled_ui.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/path_util.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/extensions/webstore_reinstaller.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/apps/app_info_dialog.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/webui/extensions/extension_basic_info.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/browser/google_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/browser/api/device_permissions_manager.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/blacklist_state.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/lazy_background_task_queue.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/browser/warning_set.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/permissions/permissions_data.h"
#include "grit/browser_resources.h"
#include "grit/components_strings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

using base::DictionaryValue;
using base::ListValue;
using content::RenderViewHost;
using content::WebContents;

namespace {

const char kAppsDeveloperToolsExtensionId[] =
    "ohmmkhmmmpcnpikjeljgnaoabkaalbgc";

// Returns true if the extensions page should display the new-style extension
// info dialog. If false, display the old permissions dialog.
bool ShouldDisplayExtensionInfoDialog() {
#if defined(OS_MACOSX)
  return false;
#else
  return true;
#endif
}

}  // namespace

namespace extensions {

ExtensionPage::ExtensionPage(const GURL& url,
                             int render_process_id,
                             int render_view_id,
                             bool incognito,
                             bool generated_background_page)
    : url(url),
      render_process_id(render_process_id),
      render_view_id(render_view_id),
      incognito(incognito),
      generated_background_page(generated_background_page) {
}

// The install prompt is not necessarily modal (e.g. Mac, Linux Unity). This
// means that the user can navigate while the dialog is up, causing the dialog
// handler to outlive the ExtensionSettingsHandler. That's a problem because the
// dialog framework will try to contact us back once the dialog is closed, which
// causes a crash. This class is designed to broker the message between the two
// objects, while managing its own lifetime so that it can outlive the
// ExtensionSettingsHandler and (when doing so) gracefully ignore the message
// from the dialog.
class BrokerDelegate : public ExtensionInstallPrompt::Delegate {
 public:
  explicit BrokerDelegate(
      const base::WeakPtr<ExtensionSettingsHandler>& delegate)
      : delegate_(delegate) {}

  // ExtensionInstallPrompt::Delegate implementation.
  void InstallUIProceed() override {
    if (delegate_)
      delegate_->InstallUIProceed();
    delete this;
  };

  void InstallUIAbort(bool user_initiated) override {
    if (delegate_)
      delegate_->InstallUIAbort(user_initiated);
    delete this;
  };

  void AppInfoDialogClosed() {
    if (delegate_)
      delegate_->AppInfoDialogClosed();
    delete this;
  }

 private:
  base::WeakPtr<ExtensionSettingsHandler> delegate_;

  DISALLOW_COPY_AND_ASSIGN(BrokerDelegate);
};

///////////////////////////////////////////////////////////////////////////////
//
// ExtensionSettingsHandler
//
///////////////////////////////////////////////////////////////////////////////

ExtensionSettingsHandler::ExtensionSettingsHandler()
    : extension_service_(NULL),
      management_policy_(NULL),
      ignore_notifications_(false),
      deleting_rvh_(NULL),
      deleting_rwh_id_(-1),
      deleting_rph_id_(-1),
      registered_for_notifications_(false),
      warning_service_observer_(this),
      error_console_observer_(this),
      extension_prefs_observer_(this),
      extension_registry_observer_(this),
      extension_management_observer_(this),
      should_do_verification_check_(false) {
}

ExtensionSettingsHandler::~ExtensionSettingsHandler() {
}

ExtensionSettingsHandler::ExtensionSettingsHandler(ExtensionService* service,
                                                   ManagementPolicy* policy)
    : extension_service_(service),
      management_policy_(policy),
      ignore_notifications_(false),
      deleting_rvh_(NULL),
      deleting_rwh_id_(-1),
      deleting_rph_id_(-1),
      registered_for_notifications_(false),
      warning_service_observer_(this),
      error_console_observer_(this),
      extension_prefs_observer_(this),
      extension_registry_observer_(this),
      extension_management_observer_(this),
      should_do_verification_check_(false) {
}

// static
void ExtensionSettingsHandler::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kExtensionsUIDeveloperMode,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kExtensionsUIDismissedADTPromo,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

base::DictionaryValue* ExtensionSettingsHandler::CreateExtensionDetailValue(
    const Extension* extension,
    const std::vector<ExtensionPage>& pages,
    const WarningService* warning_service) {
  // The items which are to be written into app_dict are also described in
  // chrome/browser/resources/extensions/extension_list.js in @typedef for
  // ExtensionData. Please update it whenever you add or remove any keys here.
  base::DictionaryValue* extension_data = new base::DictionaryValue();
  bool enabled = extension_service_->IsExtensionEnabled(extension->id());
  GetExtensionBasicInfo(extension, enabled, extension_data);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(extension_service_->profile());
  int disable_reasons = prefs->GetDisableReasons(extension->id());

  bool suspicious_install =
      (disable_reasons & Extension::DISABLE_NOT_VERIFIED) != 0;
  extension_data->SetBoolean("suspiciousInstall", suspicious_install);
  if (suspicious_install)
    should_do_verification_check_ = true;

  bool corrupt_install =
      (disable_reasons & Extension::DISABLE_CORRUPTED) != 0;
  extension_data->SetBoolean("corruptInstall", corrupt_install);

  bool update_required_by_policy =
      (disable_reasons & Extension::DISABLE_UPDATE_REQUIRED_BY_POLICY) != 0;
  extension_data->SetBoolean("updateRequiredByPolicy",
                             update_required_by_policy);

  bool managed_install =
      !management_policy_->UserMayModifySettings(extension, NULL);
  extension_data->SetBoolean("managedInstall", managed_install);

  bool recommended_install =
      !managed_install &&
      management_policy_->MustRemainInstalled(extension, NULL);
  extension_data->SetBoolean("recommendedInstall", recommended_install);

  // Suspicious install should always be mutually exclusive to managed and/or
  // recommended install.
  DCHECK(!(managed_install || recommended_install) || !suspicious_install);

  GURL icon =
      ExtensionIconSource::GetIconURL(extension,
                                      extension_misc::EXTENSION_ICON_MEDIUM,
                                      ExtensionIconSet::MATCH_BIGGER,
                                      !enabled, NULL);
  if (Manifest::IsUnpackedLocation(extension->location())) {
    extension_data->SetString("path", extension->path().value());
    extension_data->SetString(
        "prettifiedPath",
        extensions::path_util::PrettifyPath(extension->path()).value());
  }
  extension_data->SetString("icon", icon.spec());
  extension_data->SetBoolean("isUnpacked",
      Manifest::IsUnpackedLocation(extension->location()));
  extension_data->SetBoolean("isFromStore",
                             extension->location() == Manifest::INTERNAL &&
                                 ManifestURL::UpdatesFromGallery(extension));
  ExtensionRegistry* registry =
      ExtensionRegistry::Get(extension_service_->profile());
  extension_data->SetBoolean(
      "terminated",
      registry->terminated_extensions().Contains(extension->id()));
  extension_data->SetBoolean("enabledIncognito",
      util::IsIncognitoEnabled(extension->id(), extension_service_->profile()));
  extension_data->SetBoolean("incognitoCanBeEnabled",
                             extension->can_be_incognito_enabled());
  extension_data->SetBoolean("wantsFileAccess", extension->wants_file_access());
  extension_data->SetBoolean("allowFileAccess",
      util::AllowFileAccess(extension->id(), extension_service_->profile()));
  extension_data->SetBoolean("allow_reload",
      Manifest::IsUnpackedLocation(extension->location()));
  extension_data->SetBoolean("is_hosted_app", extension->is_hosted_app());
  extension_data->SetBoolean("is_platform_app", extension->is_platform_app());
  extension_data->SetBoolean("homepageProvided",
      ManifestURL::SpecifiedHomepageURL(extension));
  extension_data->SetBoolean("optionsOpenInTab",
                             OptionsPageInfo::ShouldOpenInTab(extension));
  extension_data->SetString("optionsPageHref",
                            OptionsPageInfo::GetOptionsPage(extension).spec());
  extension_data->SetBoolean("enableExtensionInfoDialog",
                             ShouldDisplayExtensionInfoDialog());

  // Add dependent extensions.
  base::ListValue* dependents_list = new base::ListValue;
  if (extension->is_shared_module()) {
    scoped_ptr<ExtensionSet> dependent_extensions =
        extension_service_->shared_module_service()->GetDependentExtensions(
            extension);
    for (ExtensionSet::const_iterator i = dependent_extensions->begin();
         i != dependent_extensions->end();
         i++) {
      base::DictionaryValue* dependent_entry = new base::DictionaryValue;
      dependent_entry->SetString("id", (*i)->id());
      dependent_entry->SetString("name", (*i)->name());
      dependents_list->Append(dependent_entry);
    }
  }
  extension_data->Set("dependentExtensions", dependents_list);

  // We show the "all urls" checkbox if:
  // - The feature is enabled for the given extension, or has been set in the
  //   past.
  // - The extension has access to enough urls that we can't just let it run
  //   on those specified in the permissions.
  content::BrowserContext* context = extension_service_->GetBrowserContext();
  bool show_all_urls =
      (FeatureSwitch::scripts_require_action()->IsEnabled() &&
       PermissionsData::ScriptsMayRequireActionForExtension(
           extension,
           extension->permissions_data()->active_permissions().get())) ||
      extension->permissions_data()->HasWithheldImpliedAllHosts() ||
      util::HasSetAllowedScriptingOnAllUrls(extension->id(), context);
  extension_data->SetBoolean("showAllUrls", show_all_urls);
  extension_data->SetBoolean(
      "allowAllUrls",
      util::AllowedScriptingOnAllUrls(extension->id(), context));

  base::string16 location_text;
  if (extension->location() == Manifest::INTERNAL &&
      !ManifestURL::UpdatesFromGallery(extension)) {
    location_text = l10n_util::GetStringUTF16(
        IDS_OPTIONS_INSTALL_LOCATION_UNKNOWN);
  } else if (extension->location() == Manifest::EXTERNAL_REGISTRY) {
    location_text = l10n_util::GetStringUTF16(
        IDS_OPTIONS_INSTALL_LOCATION_3RD_PARTY);
  } else if (extension->is_shared_module()) {
    location_text = l10n_util::GetStringUTF16(
        IDS_OPTIONS_INSTALL_LOCATION_SHARED_MODULE);
  }
  extension_data->SetString("locationText", location_text);

  base::string16 policy_text;
  if (Manifest::IsPolicyLocation(extension->location())) {
    policy_text = l10n_util::GetStringUTF16(
        IDS_OPTIONS_INSTALL_LOCATION_ENTERPRISE);
  }
  extension_data->SetString("policyText", policy_text);

  base::string16 blacklist_text;
  switch (prefs->GetExtensionBlacklistState(extension->id())) {
    case BLACKLISTED_SECURITY_VULNERABILITY:
      blacklist_text = l10n_util::GetStringUTF16(
          IDS_OPTIONS_BLACKLISTED_SECURITY_VULNERABILITY);
      break;

    case BLACKLISTED_CWS_POLICY_VIOLATION:
      blacklist_text = l10n_util::GetStringUTF16(
          IDS_OPTIONS_BLACKLISTED_CWS_POLICY_VIOLATION);
      break;

    case BLACKLISTED_POTENTIALLY_UNWANTED:
      blacklist_text = l10n_util::GetStringUTF16(
          IDS_OPTIONS_BLACKLISTED_POTENTIALLY_UNWANTED);
      break;

    default:
      break;
  }
  extension_data->SetString("blacklistText", blacklist_text);

  // Force unpacked extensions to show at the top.
  if (Manifest::IsUnpackedLocation(extension->location()))
    extension_data->SetInteger("order", 1);
  else
    extension_data->SetInteger("order", 2);

  // Don't show the "show button" for the browser action if the toolbar
  // redesign is enabled, because "hidden" buttons are now just hidden in the
  // wrench menu.
  if (!ExtensionActionAPI::GetBrowserActionVisibility(prefs, extension->id()) &&
      !FeatureSwitch::extension_action_redesign()->IsEnabled()) {
    extension_data->SetBoolean("enable_show_button", true);
  }

  // Add views
  base::ListValue* views = new base::ListValue;
  for (std::vector<ExtensionPage>::const_iterator iter = pages.begin();
       iter != pages.end(); ++iter) {
    base::DictionaryValue* view_value = new base::DictionaryValue;
    if (iter->url.scheme() == kExtensionScheme) {
      // No leading slash.
      view_value->SetString("path", iter->url.path().substr(1));
    } else {
      // For live pages, use the full URL.
      view_value->SetString("path", iter->url.spec());
    }
    view_value->SetInteger("renderViewId", iter->render_view_id);
    view_value->SetInteger("renderProcessId", iter->render_process_id);
    view_value->SetBoolean("incognito", iter->incognito);
    view_value->SetBoolean("generatedBackgroundPage",
                           iter->generated_background_page);
    views->Append(view_value);
  }
  extension_data->Set("views", views);
  ExtensionActionManager* extension_action_manager =
      ExtensionActionManager::Get(extension_service_->profile());
  extension_data->SetBoolean(
      "hasPopupAction",
      extension_action_manager->GetBrowserAction(*extension) ||
      extension_action_manager->GetPageAction(*extension));

  // Add warnings.
  if (warning_service) {
    std::vector<std::string> warnings =
        warning_service->GetWarningMessagesForExtension(extension->id());

    if (!warnings.empty()) {
      base::ListValue* warnings_list = new base::ListValue;
      for (std::vector<std::string>::const_iterator iter = warnings.begin();
           iter != warnings.end(); ++iter) {
        warnings_list->Append(new base::StringValue(*iter));
      }
      extension_data->Set("warnings", warnings_list);
    }
  }

  // If the ErrorConsole is enabled and the extension is unpacked, use the more
  // detailed errors from the ErrorConsole. Otherwise, use the install warnings
  // (using both is redundant).
  ErrorConsole* error_console =
      ErrorConsole::Get(extension_service_->profile());
  bool error_console_is_enabled =
      error_console->IsEnabledForChromeExtensionsPage();
  extension_data->SetBoolean("wantsErrorCollection", error_console_is_enabled);
  if (error_console_is_enabled) {
    extension_data->SetBoolean("errorCollectionEnabled",
                               error_console->IsReportingEnabledForExtension(
                                   extension->id()));
    const ErrorList& errors =
        error_console->GetErrorsForExtension(extension->id());
    if (!errors.empty()) {
      scoped_ptr<base::ListValue> manifest_errors(new base::ListValue);
      scoped_ptr<base::ListValue> runtime_errors(new base::ListValue);
      for (ErrorList::const_iterator iter = errors.begin();
           iter != errors.end(); ++iter) {
        if ((*iter)->type() == ExtensionError::MANIFEST_ERROR) {
          manifest_errors->Append((*iter)->ToValue().release());
        } else {  // Handle runtime error.
          const RuntimeError* error = static_cast<const RuntimeError*>(*iter);
          scoped_ptr<base::DictionaryValue> value = error->ToValue();
          bool can_inspect =
              !(deleting_rwh_id_ == error->render_view_id() &&
                deleting_rph_id_ == error->render_process_id()) &&
              RenderViewHost::FromID(error->render_process_id(),
                                     error->render_view_id()) != NULL;
          value->SetBoolean("canInspect", can_inspect);
          runtime_errors->Append(value.release());
        }
      }
      if (!manifest_errors->empty())
        extension_data->Set("manifestErrors", manifest_errors.release());
      if (!runtime_errors->empty())
        extension_data->Set("runtimeErrors", runtime_errors.release());
    }
  } else if (Manifest::IsUnpackedLocation(extension->location())) {
    const std::vector<InstallWarning>& install_warnings =
        extension->install_warnings();
    if (!install_warnings.empty()) {
      scoped_ptr<base::ListValue> list(new base::ListValue());
      for (std::vector<InstallWarning>::const_iterator it =
               install_warnings.begin(); it != install_warnings.end(); ++it) {
        base::DictionaryValue* item = new base::DictionaryValue();
        item->SetString("message", it->message);
        list->Append(item);
      }
      extension_data->Set("installWarnings", list.release());
    }
  }

  return extension_data;
}

void ExtensionSettingsHandler::GetLocalizedValues(
    content::WebUIDataSource* source) {
  source->AddString("extensionSettings",
      l10n_util::GetStringUTF16(IDS_MANAGE_EXTENSIONS_SETTING_WINDOWS_TITLE));

  source->AddString("extensionSettingsDeveloperMode",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_DEVELOPER_MODE_LINK));
  source->AddString("extensionSettingsNoExtensions",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_NONE_INSTALLED));
  source->AddString(
      "extensionSettingsSuggestGallery",
      l10n_util::GetStringFUTF16(
          IDS_EXTENSIONS_NONE_INSTALLED_SUGGEST_GALLERY,
          base::ASCIIToUTF16(
              google_util::AppendGoogleLocaleParam(
                  GURL(extension_urls::GetWebstoreExtensionsCategoryURL()),
                  g_browser_process->GetApplicationLocale()).spec())));
  source->AddString("extensionSettingsGetMoreExtensions",
      l10n_util::GetStringUTF16(IDS_GET_MORE_EXTENSIONS));
  source->AddString(
      "extensionSettingsGetMoreExtensionsUrl",
      base::ASCIIToUTF16(
          google_util::AppendGoogleLocaleParam(
              GURL(extension_urls::GetWebstoreExtensionsCategoryURL()),
              g_browser_process->GetApplicationLocale()).spec()));
  source->AddString("extensionSettingsExtensionId",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ID));
  source->AddString("extensionSettingsExtensionPath",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_PATH));
  source->AddString("extensionSettingsInspectViews",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_INSPECT_VIEWS));
  source->AddString("extensionSettingsInstallWarnings",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_INSTALL_WARNINGS));
  source->AddString("viewIncognito",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_VIEW_INCOGNITO));
  source->AddString("viewInactive",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_VIEW_INACTIVE));
  source->AddString("backgroundPage",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_BACKGROUND_PAGE));
  source->AddString("extensionSettingsEnable",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ENABLE));
  source->AddString("extensionSettingsEnabled",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ENABLED));
  source->AddString("extensionSettingsRemove",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_REMOVE));
  source->AddString("extensionSettingsEnableIncognito",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ENABLE_INCOGNITO));
  source->AddString("extensionSettingsEnableErrorCollection",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ENABLE_ERROR_COLLECTION));
  source->AddString("extensionSettingsAllowFileAccess",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ALLOW_FILE_ACCESS));
  source->AddString("extensionSettingsAllowOnAllUrls",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ALLOW_ON_ALL_URLS));
  source->AddString("extensionSettingsIncognitoWarning",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_INCOGNITO_WARNING));
  source->AddString("extensionSettingsReloadTerminated",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_RELOAD_TERMINATED));
  source->AddString("extensionSettingsRepairCorrupted",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_REPAIR_CORRUPTED));
  source->AddString("extensionSettingsLaunch",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_LAUNCH));
  source->AddString("extensionSettingsReloadUnpacked",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_RELOAD_UNPACKED));
  source->AddString("extensionSettingsOptions",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_OPTIONS_LINK));
  if (ShouldDisplayExtensionInfoDialog()) {
    source->AddString("extensionSettingsPermissions",
                      l10n_util::GetStringUTF16(IDS_EXTENSIONS_INFO_LINK));
  } else {
    source->AddString(
        "extensionSettingsPermissions",
        l10n_util::GetStringUTF16(IDS_EXTENSIONS_PERMISSIONS_LINK));
  }
  source->AddString("extensionSettingsVisitWebsite",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_VISIT_WEBSITE));
  source->AddString("extensionSettingsVisitWebStore",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_VISIT_WEBSTORE));
  source->AddString("extensionSettingsPolicyControlled",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_POLICY_CONTROLLED));
  source->AddString("extensionSettingsPolicyRecommeneded",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_POLICY_RECOMMENDED));
  source->AddString("extensionSettingsDependentExtensions",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_DEPENDENT_EXTENSIONS));
  source->AddString("extensionSettingsSupervisedUser",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_LOCKED_SUPERVISED_USER));
  source->AddString("extensionSettingsCorruptInstall",
      l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_CORRUPTED_EXTENSION));
  source->AddString("extensionSettingsSuspiciousInstall",
      l10n_util::GetStringFUTF16(
          IDS_EXTENSIONS_ADDED_WITHOUT_KNOWLEDGE,
          l10n_util::GetStringUTF16(IDS_EXTENSION_WEB_STORE_TITLE)));
  source->AddString("extensionSettingsLearnMore",
      l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  source->AddString("extensionSettingsSuspiciousInstallHelpUrl",
                    base::ASCIIToUTF16(
                        google_util::AppendGoogleLocaleParam(
                            GURL(chrome::kRemoveNonCWSExtensionURL),
                            g_browser_process->GetApplicationLocale()).spec()));
  source->AddString("extensionSettingsShowButton",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_BUTTON));
  source->AddString("extensionSettingsLoadUnpackedButton",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_LOAD_UNPACKED_BUTTON));
  source->AddString("extensionSettingsPackButton",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_PACK_BUTTON));
  source->AddString("extensionSettingsCommandsLink",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_COMMANDS_CONFIGURE));
  source->AddString("extensionSettingsUpdateButton",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_UPDATE_BUTTON));
  source->AddString(
      "extensionSettingsAppsDevToolsPromoHTML",
      l10n_util::GetStringFUTF16(
          IDS_EXTENSIONS_APPS_DEV_TOOLS_PROMO_HTML,
          base::ASCIIToUTF16(
              google_util::AppendGoogleLocaleParam(
                  GURL(extension_urls::GetWebstoreItemDetailURLPrefix() +
                       kAppsDeveloperToolsExtensionId),
                  g_browser_process->GetApplicationLocale()).spec())));
  source->AddString(
      "extensionSettingsAppDevToolsPromoClose",
      l10n_util::GetStringUTF16(IDS_CLOSE));
  source->AddString("extensionSettingsCrashMessage",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_CRASHED_EXTENSION));
  source->AddString("extensionSettingsInDevelopment",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_IN_DEVELOPMENT));
  source->AddString("extensionSettingsWarningsTitle",
      l10n_util::GetStringUTF16(IDS_EXTENSION_WARNINGS_TITLE));
  source->AddString("extensionSettingsShowDetails",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_DETAILS));
  source->AddString("extensionSettingsHideDetails",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_HIDE_DETAILS));
  source->AddString("extensionSettingsUpdateRequiredBePolicy",
                    l10n_util::GetStringUTF16(
                        IDS_EXTENSIONS_DISABLED_UPDATE_REQUIRED_BY_POLICY));

  // TODO(estade): comb through the above strings to find ones no longer used in
  // uber extensions.
  source->AddString("extensionUninstall",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_UNINSTALL));
}

void ExtensionSettingsHandler::RenderViewDeleted(
    RenderViewHost* render_view_host) {
  deleting_rvh_ = render_view_host;
  Profile* source_profile = Profile::FromBrowserContext(
      render_view_host->GetSiteInstance()->GetBrowserContext());
  if (!Profile::FromWebUI(web_ui())->IsSameProfile(source_profile))
    return;
  MaybeUpdateAfterNotification();
}

void ExtensionSettingsHandler::DidStartNavigationToPendingEntry(
    const GURL& url,
    content::NavigationController::ReloadType reload_type) {
  if (reload_type != content::NavigationController::NO_RELOAD)
    ReloadUnpackedExtensions();
}

void ExtensionSettingsHandler::RegisterMessages() {
  // Don't override an |extension_service_| or |management_policy_| injected
  // for testing.
  if (!extension_service_) {
    Profile* profile = Profile::FromWebUI(web_ui())->GetOriginalProfile();
    extension_service_ =
        extensions::ExtensionSystem::Get(profile)->extension_service();
  }
  if (!management_policy_) {
    management_policy_ = ExtensionSystem::Get(
        extension_service_->profile())->management_policy();
  }

  web_ui()->RegisterMessageCallback("extensionSettingsRequestExtensionsData",
      base::Bind(&ExtensionSettingsHandler::HandleRequestExtensionsData,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback("extensionSettingsToggleDeveloperMode",
      base::Bind(&ExtensionSettingsHandler::HandleToggleDeveloperMode,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback("extensionSettingsInspect",
      base::Bind(&ExtensionSettingsHandler::HandleInspectMessage,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback("extensionSettingsLaunch",
      base::Bind(&ExtensionSettingsHandler::HandleLaunchMessage,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback("extensionSettingsReload",
      base::Bind(&ExtensionSettingsHandler::HandleReloadMessage,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback("extensionSettingsRepair",
      base::Bind(&ExtensionSettingsHandler::HandleRepairMessage,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback("extensionSettingsEnable",
      base::Bind(&ExtensionSettingsHandler::HandleEnableMessage,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback("extensionSettingsEnableIncognito",
      base::Bind(&ExtensionSettingsHandler::HandleEnableIncognitoMessage,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback("extensionSettingsEnableErrorCollection",
      base::Bind(&ExtensionSettingsHandler::HandleEnableErrorCollectionMessage,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback("extensionSettingsAllowFileAccess",
      base::Bind(&ExtensionSettingsHandler::HandleAllowFileAccessMessage,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback("extensionSettingsAllowOnAllUrls",
      base::Bind(&ExtensionSettingsHandler::HandleAllowOnAllUrlsMessage,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback("extensionSettingsUninstall",
      base::Bind(&ExtensionSettingsHandler::HandleUninstallMessage,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback("extensionSettingsOptions",
      base::Bind(&ExtensionSettingsHandler::HandleOptionsMessage,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback("extensionSettingsPermissions",
      base::Bind(&ExtensionSettingsHandler::HandlePermissionsMessage,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback("extensionSettingsShowButton",
      base::Bind(&ExtensionSettingsHandler::HandleShowButtonMessage,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback("extensionSettingsAutoupdate",
      base::Bind(&ExtensionSettingsHandler::HandleAutoUpdateMessage,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback("extensionSettingsDismissADTPromo",
      base::Bind(&ExtensionSettingsHandler::HandleDismissADTPromoMessage,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback("extensionSettingsShowPath",
      base::Bind(&ExtensionSettingsHandler::HandleShowPath,
                 AsWeakPtr()));
}

void ExtensionSettingsHandler::OnErrorAdded(const ExtensionError* error) {
  MaybeUpdateAfterNotification();
}

void ExtensionSettingsHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  Profile* profile = Profile::FromWebUI(web_ui());
  Profile* source_profile = NULL;
  switch (type) {
    // We listen for notifications that will result in the page being
    // repopulated with data twice for the same event in certain cases.
    // For instance, EXTENSION_LOADED & EXTENSION_HOST_CREATED because
    // we don't know about the views for an extension at EXTENSION_LOADED, but
    // if we only listen to EXTENSION_HOST_CREATED, we'll miss extensions
    // that don't have a process at startup.
    //
    // Doing it this way gets everything but causes the page to be rendered
    // more than we need. It doesn't seem to result in any noticeable flicker.
    case chrome::NOTIFICATION_BACKGROUND_CONTENTS_DELETED:
      deleting_rvh_ = content::Details<BackgroundContents>(details)->
          web_contents()->GetRenderViewHost();
      // Fall through.
    case chrome::NOTIFICATION_BACKGROUND_CONTENTS_NAVIGATED:
    case extensions::NOTIFICATION_EXTENSION_HOST_CREATED:
      source_profile = content::Source<Profile>(source).ptr();
      if (!profile->IsSameProfile(source_profile))
        return;
      MaybeUpdateAfterNotification();
      break;
    case content::NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED: {
      content::RenderWidgetHost* rwh =
          content::Source<content::RenderWidgetHost>(source).ptr();
      deleting_rwh_id_ = rwh->GetRoutingID();
      deleting_rph_id_ = rwh->GetProcess()->GetID();
      MaybeUpdateAfterNotification();
      break;
    }
    case extensions::NOTIFICATION_EXTENSION_UPDATE_DISABLED:
    case extensions::NOTIFICATION_EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED:
      MaybeUpdateAfterNotification();
      break;
    case extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED:
       // This notification is sent when the extension host destruction begins,
       // not when it finishes. We use PostTask to delay the update until after
       // the destruction finishes.
       base::MessageLoop::current()->PostTask(
           FROM_HERE,
           base::Bind(&ExtensionSettingsHandler::MaybeUpdateAfterNotification,
                      AsWeakPtr()));
       break;
    default:
      NOTREACHED();
  }
}

void ExtensionSettingsHandler::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  MaybeUpdateAfterNotification();
}

void ExtensionSettingsHandler::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  MaybeUpdateAfterNotification();
}

void ExtensionSettingsHandler::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  MaybeUpdateAfterNotification();
}

void ExtensionSettingsHandler::OnExtensionDisableReasonsChanged(
    const std::string& extension_id, int disable_reasons) {
  MaybeUpdateAfterNotification();
}

void ExtensionSettingsHandler::OnExtensionManagementSettingsChanged() {
  MaybeUpdateAfterNotification();
}

void ExtensionSettingsHandler::ExtensionUninstallAccepted() {
  DCHECK(!extension_id_prompting_.empty());

  bool was_terminated = false;

  // The extension can be uninstalled in another window while the UI was
  // showing. Do nothing in that case.
  const Extension* extension =
      extension_service_->GetExtensionById(extension_id_prompting_, true);
  if (!extension) {
    extension =
        ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))->GetExtensionById(
            extension_id_prompting_, ExtensionRegistry::TERMINATED);
    was_terminated = true;
  }
  if (!extension)
    return;

  extension_service_->UninstallExtension(
      extension_id_prompting_,
      extensions::UNINSTALL_REASON_USER_INITIATED,
      base::Bind(&base::DoNothing),
      NULL);  // Error.
  extension_id_prompting_ = "";

  // There will be no EXTENSION_UNLOADED notification for terminated
  // extensions as they were already unloaded.
  if (was_terminated)
    HandleRequestExtensionsData(NULL);
}

void ExtensionSettingsHandler::ExtensionUninstallCanceled() {
  extension_id_prompting_ = "";
}

void ExtensionSettingsHandler::ExtensionWarningsChanged() {
  MaybeUpdateAfterNotification();
}

// This is called when the user clicks "Revoke File/Device Access."
void ExtensionSettingsHandler::InstallUIProceed() {
  Profile* profile = Profile::FromWebUI(web_ui());
  extensions::DevicePermissionsManager::Get(profile)
      ->Clear(extension_id_prompting_);
  apps::SavedFilesService::Get(profile)->ClearQueue(
      extension_service_->GetExtensionById(extension_id_prompting_, true));
  apps::AppLoadService::Get(profile)
      ->RestartApplicationIfRunning(extension_id_prompting_);
  extension_id_prompting_.clear();
}

void ExtensionSettingsHandler::InstallUIAbort(bool user_initiated) {
  extension_id_prompting_.clear();
}

void ExtensionSettingsHandler::AppInfoDialogClosed() {
  extension_id_prompting_.clear();
}

void ExtensionSettingsHandler::ReloadUnpackedExtensions() {
  ExtensionRegistry* registry =
      ExtensionRegistry::Get(extension_service_->profile());
  std::vector<const Extension*> unpacked_extensions;
  for (const scoped_refptr<const extensions::Extension>& extension :
       registry->enabled_extensions()) {
    if (Manifest::IsUnpackedLocation(extension->location()))
      unpacked_extensions.push_back(extension.get());
  }

  for (std::vector<const Extension*>::iterator iter =
       unpacked_extensions.begin(); iter != unpacked_extensions.end(); ++iter) {
    extension_service_->ReloadExtensionWithQuietFailure((*iter)->id());
  }
}

void ExtensionSettingsHandler::HandleRequestExtensionsData(
    const base::ListValue* args) {
  // The items which are to be written into results are also described in
  // chrome/browser/resources/extensions/extensions.js in @typedef for
  // ExtensionDataResponse. Please update it whenever you add or remove any keys
  // here.
  base::DictionaryValue results;

  Profile* profile = Profile::FromWebUI(web_ui());

  // Add the extensions to the results structure.
  base::ListValue* extensions_list = new base::ListValue();

  WarningService* warnings = WarningService::Get(profile);

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
  const ExtensionSet& enabled_set = registry->enabled_extensions();
  for (ExtensionSet::const_iterator extension = enabled_set.begin();
       extension != enabled_set.end(); ++extension) {
    if (ui_util::ShouldDisplayInExtensionSettings(extension->get(), profile)) {
      extensions_list->Append(CreateExtensionDetailValue(
          extension->get(),
          GetInspectablePagesForExtension(extension->get(), true),
          warnings));
    }
  }
  const ExtensionSet& disabled_set = registry->disabled_extensions();
  for (ExtensionSet::const_iterator extension = disabled_set.begin();
       extension != disabled_set.end(); ++extension) {
    if (ui_util::ShouldDisplayInExtensionSettings(extension->get(), profile)) {
      extensions_list->Append(CreateExtensionDetailValue(
          extension->get(),
          GetInspectablePagesForExtension(extension->get(), false),
          warnings));
    }
  }
  const ExtensionSet& terminated_set = registry->terminated_extensions();
  std::vector<ExtensionPage> empty_pages;
  for (ExtensionSet::const_iterator extension = terminated_set.begin();
       extension != terminated_set.end(); ++extension) {
    if (ui_util::ShouldDisplayInExtensionSettings(extension->get(), profile)) {
      extensions_list->Append(CreateExtensionDetailValue(
          extension->get(),
          empty_pages,  // Terminated process has no active pages.
          warnings));
    }
  }
  results.Set("extensions", extensions_list);

  bool is_supervised = profile->IsSupervised();
  bool incognito_available =
      IncognitoModePrefs::GetAvailability(profile->GetPrefs()) !=
          IncognitoModePrefs::DISABLED;
  bool developer_mode =
      !is_supervised &&
      profile->GetPrefs()->GetBoolean(prefs::kExtensionsUIDeveloperMode);
  results.SetBoolean("profileIsSupervised", is_supervised);
  results.SetBoolean("incognitoAvailable", incognito_available);
  results.SetBoolean("developerMode", developer_mode);

  // Promote the Chrome Apps & Extensions Developer Tools if they are not
  // installed and the user has not previously dismissed the warning.
  bool promote_apps_dev_tools = false;
  if (!ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))->
          GetExtensionById(kAppsDeveloperToolsExtensionId,
                           ExtensionRegistry::EVERYTHING) &&
      !profile->GetPrefs()->GetBoolean(prefs::kExtensionsUIDismissedADTPromo)) {
    promote_apps_dev_tools = true;
  }
  results.SetBoolean("promoteAppsDevTools", promote_apps_dev_tools);

  const bool load_unpacked_disabled =
      ExtensionManagementFactory::GetForBrowserContext(profile)
          ->BlacklistedByDefault();
  results.SetBoolean("loadUnpackedDisabled", load_unpacked_disabled);

  web_ui()->CallJavascriptFunction(
      "extensions.ExtensionSettings.returnExtensionsData", results);

  MaybeRegisterForNotifications();
  UMA_HISTOGRAM_BOOLEAN("ExtensionSettings.ShouldDoVerificationCheck",
                        should_do_verification_check_);
  if (should_do_verification_check_) {
    should_do_verification_check_ = false;
    ExtensionSystem::Get(Profile::FromWebUI(web_ui()))
        ->install_verifier()
        ->VerifyAllExtensions();
  }
}

void ExtensionSettingsHandler::HandleToggleDeveloperMode(
    const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (profile->IsSupervised())
    return;

  bool developer_mode_on;
  CHECK(args->GetBoolean(0, &developer_mode_on));
  profile->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode,
                                  developer_mode_on);
}

void ExtensionSettingsHandler::HandleInspectMessage(
    const base::ListValue* args) {
  std::string extension_id;
  std::string render_process_id_str;
  std::string render_view_id_str;
  int render_process_id;
  int render_view_id;
  bool incognito;
  CHECK_EQ(4U, args->GetSize());
  CHECK(args->GetString(0, &extension_id));
  CHECK(args->GetString(1, &render_process_id_str));
  CHECK(args->GetString(2, &render_view_id_str));
  CHECK(args->GetBoolean(3, &incognito));
  CHECK(base::StringToInt(render_process_id_str, &render_process_id));
  CHECK(base::StringToInt(render_view_id_str, &render_view_id));

  if (render_process_id == -1) {
    // This message is for a lazy background page. Start the page if necessary.
    Profile* profile = Profile::FromWebUI(web_ui());
    const Extension* extension =
        ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(
            extension_id);
    DCHECK(extension);
    if (incognito)
      profile = profile->GetOffTheRecordProfile();
    devtools_util::InspectBackgroundPage(extension, profile);
    return;
  }

  RenderViewHost* host = RenderViewHost::FromID(render_process_id,
                                                render_view_id);
  if (!host || !WebContents::FromRenderViewHost(host)) {
    // This can happen if the host has gone away since the page was displayed.
    return;
  }

  DevToolsWindow::OpenDevToolsWindow(WebContents::FromRenderViewHost(host));
}

void ExtensionSettingsHandler::HandleLaunchMessage(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  std::string extension_id;
  CHECK(args->GetString(0, &extension_id));
  const Extension* extension =
      extension_service_->GetExtensionById(extension_id, false);
  OpenApplication(AppLaunchParams(extension_service_->profile(), extension,
                                  extensions::LAUNCH_CONTAINER_WINDOW,
                                  NEW_WINDOW,
                                  extensions::SOURCE_EXTENSIONS_PAGE));
}

void ExtensionSettingsHandler::HandleReloadMessage(
    const base::ListValue* args) {
  std::string extension_id = base::UTF16ToUTF8(ExtractStringValue(args));
  CHECK(!extension_id.empty());
  extension_service_->ReloadExtensionWithQuietFailure(extension_id);
}

void ExtensionSettingsHandler::HandleRepairMessage(
    const base::ListValue* args) {
  std::string extension_id = base::UTF16ToUTF8(ExtractStringValue(args));
  CHECK(!extension_id.empty());
  scoped_refptr<WebstoreReinstaller> reinstaller(new WebstoreReinstaller(
      web_contents(),
      extension_id,
      base::Bind(&ExtensionSettingsHandler::OnReinstallComplete,
                 AsWeakPtr())));
  reinstaller->BeginReinstall();
}

void ExtensionSettingsHandler::HandleEnableMessage(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  std::string extension_id, enable_str;
  CHECK(args->GetString(0, &extension_id));
  CHECK(args->GetString(1, &enable_str));

  const Extension* extension =
      extension_service_->GetInstalledExtension(extension_id);
  if (!extension)
    return;

  if (!management_policy_->UserMayModifySettings(extension, NULL)) {
    LOG(ERROR) << "An attempt was made to enable an extension that is "
               << "non-usermanagable. Extension id: " << extension->id();
    return;
  }

  if (enable_str == "true") {
    ExtensionPrefs* prefs = ExtensionPrefs::Get(extension_service_->profile());
    if (prefs->DidExtensionEscalatePermissions(extension_id)) {
      ShowExtensionDisabledDialog(
          extension_service_, web_ui()->GetWebContents(), extension);
    } else if ((prefs->GetDisableReasons(extension_id) &
                   Extension::DISABLE_UNSUPPORTED_REQUIREMENT) &&
               !requirements_checker_.get()) {
      // Recheck the requirements.
      scoped_refptr<const Extension> extension =
          extension_service_->GetExtensionById(extension_id,
                                               true /* include disabled */);
      requirements_checker_.reset(new RequirementsChecker);
      requirements_checker_->Check(
          extension,
          base::Bind(&ExtensionSettingsHandler::OnRequirementsChecked,
                     AsWeakPtr(), extension_id));
    } else {
      extension_service_->EnableExtension(extension_id);
    }
  } else {
    extension_service_->DisableExtension(
        extension_id, Extension::DISABLE_USER_ACTION);
  }
}

void ExtensionSettingsHandler::HandleEnableIncognitoMessage(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  std::string extension_id, enable_str;
  CHECK(args->GetString(0, &extension_id));
  CHECK(args->GetString(1, &enable_str));
  const Extension* extension =
      extension_service_->GetInstalledExtension(extension_id);
  if (!extension)
    return;

  // Flipping the incognito bit will generate unload/load notifications for the
  // extension, but we don't want to reload the page, because a) we've already
  // updated the UI to reflect the change, and b) we want the yellow warning
  // text to stay until the user has left the page.
  //
  // TODO(aa): This creates crappiness in some cases. For example, in a main
  // window, when toggling this, the browser action will flicker because it gets
  // unloaded, then reloaded. It would be better to have a dedicated
  // notification for this case.
  //
  // Bug: http://crbug.com/41384
  base::AutoReset<bool> auto_reset_ignore_notifications(
      &ignore_notifications_, true);
  util::SetIsIncognitoEnabled(extension->id(),
                              extension_service_->profile(),
                              enable_str == "true");
}

void ExtensionSettingsHandler::HandleEnableErrorCollectionMessage(
    const base::ListValue* args) {
  CHECK_EQ(2u, args->GetSize());
  std::string extension_id;
  std::string enable_str;
  CHECK(args->GetString(0, &extension_id));
  CHECK(args->GetString(1, &enable_str));
  bool enabled = enable_str == "true";
  ErrorConsole::Get(Profile::FromWebUI(web_ui()))
      ->SetReportingAllForExtension(extension_id, enabled);
}

void ExtensionSettingsHandler::HandleAllowFileAccessMessage(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  std::string extension_id, allow_str;
  CHECK(args->GetString(0, &extension_id));
  CHECK(args->GetString(1, &allow_str));
  const Extension* extension =
      extension_service_->GetInstalledExtension(extension_id);
  if (!extension)
    return;

  if (!management_policy_->UserMayModifySettings(extension, NULL)) {
    LOG(ERROR) << "An attempt was made to change allow file access of an"
               << " extension that is non-usermanagable. Extension id : "
               << extension->id();
    return;
  }

  util::SetAllowFileAccess(
      extension_id, extension_service_->profile(), allow_str == "true");
}

void ExtensionSettingsHandler::HandleAllowOnAllUrlsMessage(
    const base::ListValue* args) {
  DCHECK(FeatureSwitch::scripts_require_action()->IsEnabled());
  CHECK_EQ(2u, args->GetSize());
  std::string extension_id;
  std::string allow_str;
  CHECK(args->GetString(0, &extension_id));
  CHECK(args->GetString(1, &allow_str));
  util::SetAllowedScriptingOnAllUrls(extension_id,
                                     extension_service_->GetBrowserContext(),
                                     allow_str == "true");
}

void ExtensionSettingsHandler::HandleUninstallMessage(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  std::string extension_id;
  CHECK(args->GetString(0, &extension_id));
  const Extension* extension =
      extension_service_->GetInstalledExtension(extension_id);
  if (!extension)
    return;

  if (!management_policy_->UserMayModifySettings(extension, NULL) ||
      management_policy_->MustRemainInstalled(extension, NULL)) {
    LOG(ERROR) << "An attempt was made to uninstall an extension that is "
               << "non-usermanagable. Extension id : " << extension->id();
    return;
  }

  if (!extension_id_prompting_.empty())
    return;  // Only one prompt at a time.

  extension_id_prompting_ = extension_id;

  GetExtensionUninstallDialog()->ConfirmUninstall(extension);
}

void ExtensionSettingsHandler::HandleOptionsMessage(
    const base::ListValue* args) {
  const Extension* extension = GetActiveExtension(args);
  if (!extension || OptionsPageInfo::GetOptionsPage(extension).is_empty())
    return;
  ExtensionTabUtil::OpenOptionsPage(extension,
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents()));
}

void ExtensionSettingsHandler::HandlePermissionsMessage(
    const base::ListValue* args) {
  std::string extension_id(base::UTF16ToUTF8(ExtractStringValue(args)));
  CHECK(!extension_id.empty());
  const Extension* extension =
      ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))
          ->GetExtensionById(extension_id, ExtensionRegistry::EVERYTHING);
  if (!extension)
    return;

  if (!extension_id_prompting_.empty())
    return;  // Only one prompt at a time.
  extension_id_prompting_ = extension->id();

  // The BrokerDelegate manages its own lifetime.
  BrokerDelegate* broker_delegate = new BrokerDelegate(AsWeakPtr());

  // Show the new-style extensions dialog when the flag is set. The flag cannot
  // be set on Mac platforms.
  if (ShouldDisplayExtensionInfoDialog()) {
    UMA_HISTOGRAM_ENUMERATION("Apps.AppInfoDialog.Launches",
                              AppInfoLaunchSource::FROM_EXTENSIONS_PAGE,
                              AppInfoLaunchSource::NUM_LAUNCH_SOURCES);

    // Display the dialog at a size similar to the app list.
    const int kAppInfoDialogWidth = 380;
    const int kAppInfoDialogHeight = 490;

    ShowAppInfoInNativeDialog(
        web_contents()->GetTopLevelNativeWindow(),
        gfx::Size(kAppInfoDialogWidth, kAppInfoDialogHeight),
        Profile::FromWebUI(web_ui()), extension,
        base::Bind(&BrokerDelegate::AppInfoDialogClosed,
                   base::Unretained(broker_delegate)));
  } else {
    prompt_.reset(new ExtensionInstallPrompt(web_contents()));
    std::vector<base::FilePath> retained_file_paths;
    if (extension->permissions_data()->HasAPIPermission(
            APIPermission::kFileSystem)) {
      std::vector<apps::SavedFileEntry> retained_file_entries =
          apps::SavedFilesService::Get(Profile::FromWebUI(web_ui()))
              ->GetAllFileEntries(extension_id_prompting_);
      for (size_t i = 0; i < retained_file_entries.size(); ++i) {
        retained_file_paths.push_back(retained_file_entries[i].path);
      }
    }
    std::vector<base::string16> retained_device_messages;
    if (extension->permissions_data()->HasAPIPermission(APIPermission::kUsb)) {
      retained_device_messages =
          extensions::DevicePermissionsManager::Get(
              Profile::FromWebUI(web_ui()))
              ->GetPermissionMessageStrings(extension_id_prompting_);
    }

    prompt_->ReviewPermissions(broker_delegate, extension, retained_file_paths,
                               retained_device_messages);
  }
}

void ExtensionSettingsHandler::HandleShowButtonMessage(
    const base::ListValue* args) {
  const Extension* extension = GetActiveExtension(args);
  if (!extension)
    return;
  ExtensionActionAPI::SetBrowserActionVisibility(
      ExtensionPrefs::Get(extension_service_->profile()),
      extension->id(),
      true);
}

void ExtensionSettingsHandler::HandleAutoUpdateMessage(
    const base::ListValue* args) {
  ExtensionUpdater* updater = extension_service_->updater();
  if (updater) {
    ExtensionUpdater::CheckParams params;
    params.install_immediately = true;
    updater->CheckNow(params);
  }
}

void ExtensionSettingsHandler::HandleDismissADTPromoMessage(
    const base::ListValue* args) {
  DCHECK(args->empty());
  Profile::FromWebUI(web_ui())->GetPrefs()->SetBoolean(
      prefs::kExtensionsUIDismissedADTPromo, true);
}

void ExtensionSettingsHandler::HandleShowPath(const base::ListValue* args) {
  DCHECK(!args->empty());
  std::string extension_id = base::UTF16ToUTF8(ExtractStringValue(args));

  Profile* profile = Profile::FromWebUI(web_ui());
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
  const Extension* extension = registry->GetExtensionById(
      extension_id,
      ExtensionRegistry::EVERYTHING);
  CHECK(extension);
  // We explicitly show manifest.json in order to work around an issue in OSX
  // where opening the directory doesn't focus the Finder.
  platform_util::ShowItemInFolder(profile,
                                  extension->path().Append(kManifestFilename));
}

void ExtensionSettingsHandler::ShowAlert(const std::string& message) {
  base::ListValue arguments;
  arguments.Append(new base::StringValue(message));
  web_ui()->CallJavascriptFunction("alert", arguments);
}

const Extension* ExtensionSettingsHandler::GetActiveExtension(
    const base::ListValue* args) {
  std::string extension_id = base::UTF16ToUTF8(ExtractStringValue(args));
  CHECK(!extension_id.empty());
  return extension_service_->GetExtensionById(extension_id, false);
}

void ExtensionSettingsHandler::MaybeUpdateAfterNotification() {
  WebContents* contents = web_ui()->GetWebContents();
  if (!ignore_notifications_ && contents && contents->GetRenderViewHost())
    HandleRequestExtensionsData(NULL);
  deleting_rvh_ = NULL;
}

void ExtensionSettingsHandler::MaybeRegisterForNotifications() {
  if (registered_for_notifications_)
    return;

  registered_for_notifications_  = true;
  Profile* profile = Profile::FromWebUI(web_ui());

  // Register for notifications that we need to reload the page.
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_UPDATE_DISABLED,
                 content::Source<Profile>(profile));
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_HOST_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_BACKGROUND_CONTENTS_NAVIGATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_BACKGROUND_CONTENTS_DELETED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(
      this,
      extensions::NOTIFICATION_EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED,
      content::Source<ExtensionPrefs>(ExtensionPrefs::Get(profile)));
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
                 content::NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
                 content::NotificationService::AllBrowserContextsAndSources());

  extension_registry_observer_.Add(ExtensionRegistry::Get(profile));

  content::WebContentsObserver::Observe(web_ui()->GetWebContents());

  warning_service_observer_.Add(WarningService::Get(profile));

  error_console_observer_.Add(ErrorConsole::Get(profile));

  extension_management_observer_.Add(
      ExtensionManagementFactory::GetForBrowserContext(profile));
}

std::vector<ExtensionPage>
ExtensionSettingsHandler::GetInspectablePagesForExtension(
    const Extension* extension, bool extension_is_enabled) {
  std::vector<ExtensionPage> result;

  // Get the extension process's active views.
  extensions::ProcessManager* process_manager =
      ProcessManager::Get(extension_service_->profile());
  GetInspectablePagesForExtensionProcess(
      extension,
      process_manager->GetRenderViewHostsForExtension(extension->id()),
      &result);

  // Get app window views
  GetAppWindowPagesForExtensionProfile(
      extension, extension_service_->profile(), &result);

  // Include a link to start the lazy background page, if applicable.
  if (BackgroundInfo::HasLazyBackgroundPage(extension) &&
      extension_is_enabled &&
      !process_manager->GetBackgroundHostForExtension(extension->id())) {
    result.push_back(ExtensionPage(
        BackgroundInfo::GetBackgroundURL(extension),
        -1,
        -1,
        false,
        BackgroundInfo::HasGeneratedBackgroundPage(extension)));
  }

  // Repeat for the incognito process, if applicable. Don't try to get
  // app windows for incognito processes.
  if (extension_service_->profile()->HasOffTheRecordProfile() &&
      IncognitoInfo::IsSplitMode(extension) &&
      util::IsIncognitoEnabled(extension->id(),
                               extension_service_->profile())) {
    extensions::ProcessManager* process_manager = ProcessManager::Get(
        extension_service_->profile()->GetOffTheRecordProfile());
    GetInspectablePagesForExtensionProcess(
        extension,
        process_manager->GetRenderViewHostsForExtension(extension->id()),
        &result);

    if (BackgroundInfo::HasLazyBackgroundPage(extension) &&
        extension_is_enabled &&
        !process_manager->GetBackgroundHostForExtension(extension->id())) {
      result.push_back(ExtensionPage(
          BackgroundInfo::GetBackgroundURL(extension),
          -1,
          -1,
          true,
          BackgroundInfo::HasGeneratedBackgroundPage(extension)));
    }
  }

  return result;
}

void ExtensionSettingsHandler::GetInspectablePagesForExtensionProcess(
    const Extension* extension,
    const std::set<RenderViewHost*>& views,
    std::vector<ExtensionPage>* result) {
  bool has_generated_background_page =
      BackgroundInfo::HasGeneratedBackgroundPage(extension);
  for (std::set<RenderViewHost*>::const_iterator iter = views.begin();
       iter != views.end(); ++iter) {
    RenderViewHost* host = *iter;
    WebContents* web_contents = WebContents::FromRenderViewHost(host);
    ViewType host_type = GetViewType(web_contents);
    if (host == deleting_rvh_ ||
        VIEW_TYPE_EXTENSION_POPUP == host_type ||
        VIEW_TYPE_EXTENSION_DIALOG == host_type)
      continue;

    GURL url = web_contents->GetURL();
    content::RenderProcessHost* process = host->GetProcess();
    bool is_background_page =
        (url == BackgroundInfo::GetBackgroundURL(extension));
    result->push_back(
        ExtensionPage(url,
                      process->GetID(),
                      host->GetRoutingID(),
                      process->GetBrowserContext()->IsOffTheRecord(),
                      is_background_page && has_generated_background_page));
  }
}

void ExtensionSettingsHandler::GetAppWindowPagesForExtensionProfile(
    const Extension* extension,
    Profile* profile,
    std::vector<ExtensionPage>* result) {
  AppWindowRegistry* registry = AppWindowRegistry::Get(profile);
  if (!registry) return;

  const AppWindowRegistry::AppWindowList windows =
      registry->GetAppWindowsForApp(extension->id());

  bool has_generated_background_page =
      BackgroundInfo::HasGeneratedBackgroundPage(extension);
  for (AppWindowRegistry::const_iterator it = windows.begin();
       it != windows.end();
       ++it) {
    WebContents* web_contents = (*it)->web_contents();
    RenderViewHost* host = web_contents->GetRenderViewHost();
    content::RenderProcessHost* process = host->GetProcess();

    bool is_background_page =
        (web_contents->GetURL() == BackgroundInfo::GetBackgroundURL(extension));
    result->push_back(
        ExtensionPage(web_contents->GetURL(),
                      process->GetID(),
                      host->GetRoutingID(),
                      process->GetBrowserContext()->IsOffTheRecord(),
                      is_background_page && has_generated_background_page));
  }
}

ExtensionUninstallDialog*
ExtensionSettingsHandler::GetExtensionUninstallDialog() {
  if (!extension_uninstall_dialog_.get()) {
    Browser* browser = chrome::FindBrowserWithWebContents(
        web_ui()->GetWebContents());
    extension_uninstall_dialog_.reset(
        ExtensionUninstallDialog::Create(extension_service_->profile(),
                                         browser->window()->GetNativeWindow(),
                                         this));
  }
  return extension_uninstall_dialog_.get();
}

void ExtensionSettingsHandler::OnReinstallComplete(
    bool success,
    const std::string& error,
    webstore_install::Result result) {
  MaybeUpdateAfterNotification();
}

void ExtensionSettingsHandler::OnRequirementsChecked(
    std::string extension_id,
    std::vector<std::string> requirement_errors) {
  if (requirement_errors.empty()) {
    extension_service_->EnableExtension(extension_id);
  } else {
    ExtensionErrorReporter::GetInstance()->ReportError(
        base::UTF8ToUTF16(JoinString(requirement_errors, ' ')),
        true);  // Be noisy.
  }
  requirements_checker_.reset();
}

}  // namespace extensions

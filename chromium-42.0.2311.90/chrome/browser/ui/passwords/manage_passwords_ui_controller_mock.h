// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_UI_CONTROLLER_MOCK_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_UI_CONTROLLER_MOCK_H_

#include "base/basictypes.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "content/public/browser/navigation_details.h"

namespace content {
class WebContents;
}  // namespace content

namespace password_manager {
enum class CredentialType : unsigned int;
}

// This mock is used in tests to ensure that we're just testing the controller
// behavior, and not the behavior of the bits and pieces it relies upon (like
// FormManager).
class ManagePasswordsUIControllerMock
    : public ManagePasswordsUIController {
 public:
  explicit ManagePasswordsUIControllerMock(
      content::WebContents* contents);
  ~ManagePasswordsUIControllerMock() override;

  // Navigation, surprisingly, is platform-specific; Android's settings page
  // is native UI and therefore isn't available in a tab for unit tests.
  //
  // TODO(mkwst): Determine how to reasonably test this on that platform.
  void NavigateToPasswordManagerSettingsPage() override;
  bool navigated_to_settings_page() const {
    return navigated_to_settings_page_;
  }

  // We don't have a FormManager in tests, so stub these out.
  void SavePasswordInternal() override;
  bool saved_password() const { return saved_password_; }

  void NeverSavePasswordInternal() override;
  bool never_saved_password() const { return never_saved_password_; }

  void ChooseCredential(const autofill::PasswordForm& form,
                        password_manager::CredentialType form_type) override;
  bool choose_credential() const { return choose_credential_; }

  const autofill::PasswordForm& PendingPassword() const override;
  void SetPendingPassword(autofill::PasswordForm pending_password);

  void UpdateBubbleAndIconVisibility() override;

  void UpdateAndroidAccountChooserInfoBarVisibility() override;

  base::TimeDelta Elapsed() const override;

  // Sneaky setters for testing.
  void SetPasswordFormMap(const autofill::ConstPasswordFormMap& map) {
    password_form_map_ = map;
  }
  using ManagePasswordsUIController::SetState;

  void SetElapsed(base::TimeDelta elapsed) { elapsed_ = elapsed; }

  // True if this controller is installed on |web_contents()|.
  bool IsInstalled() const;

  using ManagePasswordsUIController::DidNavigateMainFrame;

 private:
  bool navigated_to_settings_page_;
  bool saved_password_;
  bool never_saved_password_;
  bool choose_credential_;
  base::TimeDelta elapsed_;

  autofill::PasswordForm pending_password_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsUIControllerMock);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_UI_CONTROLLER_MOCK_H_

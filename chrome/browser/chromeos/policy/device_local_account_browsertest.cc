// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/cloud/cloud_policy_constants.h"
#include "chrome/browser/policy/cloud/policy_builder.h"
#include "chrome/browser/policy/policy_service.h"
#include "chrome/browser/policy/proto/chromeos/chrome_device_policy.pb.h"
#include "chrome/browser/policy/test/local_policy_test_server.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/dbus/mock_dbus_thread_manager_without_gmock.h"
#include "chromeos/dbus/session_manager_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace em = enterprise_management;

using testing::Return;

namespace policy {

namespace {

const char kAccountId1[] = "dla1@example.com";
const char kAccountId2[] = "dla2@example.com";
const char kDisplayName1[] = "display name for account 1";
const char kDisplayName2[] = "display name for account 2";
const char* kStartupURLs[] = {
  "chrome://policy",
  "chrome://about",
};

}  // namespace

class DeviceLocalAccountTest : public InProcessBrowserTest {
 protected:
  DeviceLocalAccountTest()
      : user_id_1_(GenerateDeviceLocalAccountUserId(
            kAccountId1, DeviceLocalAccount::TYPE_PUBLIC_SESSION)),
        user_id_2_(GenerateDeviceLocalAccountUserId(
            kAccountId2, DeviceLocalAccount::TYPE_PUBLIC_SESSION)) {}

  virtual ~DeviceLocalAccountTest() {}

  virtual void SetUp() OVERRIDE {
    // Configure and start the test server.
    scoped_ptr<crypto::RSAPrivateKey> signing_key(
        PolicyBuilder::CreateTestSigningKey());
    ASSERT_TRUE(test_server_.SetSigningKey(signing_key.get()));
    signing_key.reset();
    test_server_.RegisterClient(PolicyBuilder::kFakeToken,
                                PolicyBuilder::kFakeDeviceId);
    ASSERT_TRUE(test_server_.Start());

    InProcessBrowserTest::SetUp();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
    command_line->AppendSwitchASCII(
        switches::kDeviceManagementUrl, test_server_.GetServiceURL().spec());
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Clear command-line arguments (but keep command-line switches) so the
    // startup pages policy takes effect.
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    CommandLine::StringVector argv(command_line->argv());
    argv.erase(argv.begin() + argv.size() - command_line->GetArgs().size(),
               argv.end());
    command_line->InitFromArgv(argv);

    // Mark the device enterprise-enrolled.
    DevicePolicyCrosBrowserTest::MarkAsEnterpriseOwned(&temp_dir_);

    // Redirect session_manager DBus calls to FakeSessionManagerClient.
    chromeos::MockDBusThreadManagerWithoutGMock* dbus_thread_manager =
        new chromeos::MockDBusThreadManagerWithoutGMock();
    session_manager_client_ =
        dbus_thread_manager->fake_session_manager_client();
    chromeos::DBusThreadManager::InitializeForTesting(dbus_thread_manager);

    SetUpPolicy();
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    // This shuts down the login UI.
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(&chrome::AttemptExit));
    base::RunLoop().RunUntilIdle();
  }

  void SetUpPolicy() {
    // Configure two device-local accounts in device settings.
    DevicePolicyBuilder device_policy;
    device_policy.policy_data().set_public_key_version(1);
    em::ChromeDeviceSettingsProto& proto(device_policy.payload());
    proto.mutable_show_user_names()->set_show_user_names(true);
    em::DeviceLocalAccountInfoProto* account1 =
        proto.mutable_device_local_accounts()->add_account();
    account1->set_account_id(kAccountId1);
    account1->set_type(
        em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_PUBLIC_SESSION);
    em::DeviceLocalAccountInfoProto* account2 =
        proto.mutable_device_local_accounts()->add_account();
    account2->set_account_id(kAccountId2);
    account2->set_type(
        em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_PUBLIC_SESSION);
    device_policy.Build();
    session_manager_client_->set_device_policy(device_policy.GetBlob());
    test_server_.UpdatePolicy(dm_protocol::kChromeDevicePolicyType,
                              std::string(), proto.SerializeAsString());

    // Install the owner key.
    base::FilePath owner_key_file = temp_dir_.path().AppendASCII("owner.key");
    std::vector<uint8> owner_key_bits;
    ASSERT_TRUE(device_policy.signing_key()->ExportPublicKey(&owner_key_bits));
    ASSERT_EQ(
        static_cast<int>(owner_key_bits.size()),
        file_util::WriteFile(
            owner_key_file,
            reinterpret_cast<const char*>(vector_as_array(&owner_key_bits)),
            owner_key_bits.size()));
    ASSERT_TRUE(
        PathService::Override(chromeos::FILE_OWNER_KEY, owner_key_file));

    // Configure device-local account policy for the first device-local account.
    UserPolicyBuilder device_local_account_policy;
    device_local_account_policy.policy_data().set_policy_type(
        dm_protocol::kChromePublicAccountPolicyType);
    device_local_account_policy.policy_data().set_username(kAccountId1);
    device_local_account_policy.policy_data().set_settings_entity_id(
        kAccountId1);
    device_local_account_policy.policy_data().set_public_key_version(1);
    device_local_account_policy.payload().mutable_restoreonstartup()->set_value(
        SessionStartupPref::kPrefValueURLs);
    em::StringListPolicyProto* startup_urls_proto =
        device_local_account_policy.payload().mutable_restoreonstartupurls();
    for (size_t i = 0; i < arraysize(kStartupURLs); ++i)
      startup_urls_proto->mutable_value()->add_entries(kStartupURLs[i]);
    device_local_account_policy.payload().mutable_userdisplayname()->set_value(
        kDisplayName1);
    device_local_account_policy.Build();
    session_manager_client_->set_device_local_account_policy(
        kAccountId1, device_local_account_policy.GetBlob());
    test_server_.UpdatePolicy(
        dm_protocol::kChromePublicAccountPolicyType, kAccountId1,
        device_local_account_policy.payload().SerializeAsString());

    // Make policy for the second account available from the server.
    device_local_account_policy.payload().mutable_userdisplayname()->set_value(
        kDisplayName2);
    test_server_.UpdatePolicy(
        dm_protocol::kChromePublicAccountPolicyType, kAccountId2,
        device_local_account_policy.payload().SerializeAsString());

    // Don't install policy for |kAccountId2| yet so initial download gets
    // test coverage.
    ASSERT_TRUE(session_manager_client_->device_local_account_policy(
        kAccountId2).empty());
  }

  void CheckPublicSessionPresent(const std::string& id) {
    const chromeos::User* user = chromeos::UserManager::Get()->FindUser(id);
    ASSERT_TRUE(user);
    EXPECT_EQ(id, user->email());
    EXPECT_EQ(chromeos::User::USER_TYPE_PUBLIC_ACCOUNT, user->GetType());
  }

  const std::string user_id_1_;
  const std::string user_id_2_;

  LocalPolicyTestServer test_server_;
  base::ScopedTempDir temp_dir_;

  chromeos::FakeSessionManagerClient* session_manager_client_;
};

static bool IsKnownUser(const std::string& account_id) {
  return chromeos::UserManager::Get()->IsKnownUser(account_id);
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, LoginScreen) {
  content::WindowedNotificationObserver(chrome::NOTIFICATION_USER_LIST_CHANGED,
                                        base::Bind(&IsKnownUser, user_id_1_))
      .Wait();
  content::WindowedNotificationObserver(chrome::NOTIFICATION_USER_LIST_CHANGED,
                                        base::Bind(&IsKnownUser, user_id_2_))
      .Wait();

  CheckPublicSessionPresent(user_id_1_);
  CheckPublicSessionPresent(user_id_2_);
}

static bool DisplayNameMatches(const std::string& account_id,
                        const std::string& display_name) {
  const chromeos::User* user =
      chromeos::UserManager::Get()->FindUser(account_id);
  if (!user || user->display_name().empty())
    return false;
  EXPECT_EQ(UTF8ToUTF16(display_name), user->display_name());
  return true;
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, DisplayName) {
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_USER_LIST_CHANGED,
      base::Bind(&DisplayNameMatches, user_id_1_, kDisplayName1)).Wait();
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, PolicyDownload) {
  // Policy for kAccountId2 is not installed in session_manager_client, make
  // sure it gets fetched from the server. Note that the test setup doesn't set
  // up policy for kAccountId2, so the presence of the display name can be used
  // as signal to indicate successful policy download.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_USER_LIST_CHANGED,
      base::Bind(&DisplayNameMatches, user_id_2_, kDisplayName2)).Wait();

  // Sanity check: The policy should be present now.
  ASSERT_FALSE(session_manager_client_->device_local_account_policy(
      kAccountId2).empty());
}

static bool IsNotKnownUser(const std::string& account_id) {
  return !IsKnownUser(account_id);
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, DevicePolicyChange) {
  // Wait until the login screen is up.
  content::WindowedNotificationObserver(chrome::NOTIFICATION_USER_LIST_CHANGED,
                                        base::Bind(&IsKnownUser, user_id_1_))
      .Wait();
  content::WindowedNotificationObserver(chrome::NOTIFICATION_USER_LIST_CHANGED,
                                        base::Bind(&IsKnownUser, user_id_2_))
      .Wait();

  // Update policy to remove kAccountId2.
  em::ChromeDeviceSettingsProto policy;
  policy.mutable_show_user_names()->set_show_user_names(true);
  em::DeviceLocalAccountInfoProto* account1 =
      policy.mutable_device_local_accounts()->add_account();
  account1->set_account_id(kAccountId1);
  account1->set_type(
      em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_PUBLIC_SESSION);

  test_server_.UpdatePolicy(dm_protocol::kChromeDevicePolicyType, std::string(),
                            policy.SerializeAsString());
  g_browser_process->policy_service()->RefreshPolicies(base::Closure());

  // Make sure the second device-local account disappears.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_USER_LIST_CHANGED,
      base::Bind(&IsNotKnownUser, user_id_2_)).Wait();
}

static bool IsSessionStarted() {
  return chromeos::UserManager::Get()->IsSessionStarted();
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, StartSession) {
  // This observes the display name becoming available as this indicates
  // device-local account policy is fully loaded, which is a prerequisite for
  // successful login.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_USER_LIST_CHANGED,
      base::Bind(&DisplayNameMatches, user_id_1_, kDisplayName1)).Wait();

  chromeos::LoginDisplayHost* host =
      chromeos::LoginDisplayHostImpl::default_host();
  ASSERT_TRUE(host);
  host->StartSignInScreen();
  chromeos::ExistingUserController* controller =
      chromeos::ExistingUserController::current_controller();
  ASSERT_TRUE(controller);
  controller->LoginAsPublicAccount(user_id_1_);

  // Wait for the session to start.
  content::WindowedNotificationObserver(chrome::NOTIFICATION_SESSION_STARTED,
                                        base::Bind(IsSessionStarted)).Wait();

  // Check that the startup pages specified in policy were opened.
  BrowserList* browser_list =
    BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH);
  EXPECT_EQ(1U, browser_list->size());
  Browser* browser = browser_list->get(0);
  ASSERT_TRUE(browser);

  TabStripModel* tabs = browser->tab_strip_model();
  ASSERT_TRUE(tabs);
  int expected_tab_count = static_cast<int>(arraysize(kStartupURLs));
  EXPECT_EQ(expected_tab_count, tabs->count());
  for (int i = 0; i < expected_tab_count && i < tabs->count(); ++i)
    EXPECT_EQ(GURL(kStartupURLs[i]), tabs->GetWebContentsAt(i)->GetURL());
}

}  // namespace policy

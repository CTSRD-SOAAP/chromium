// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "googleurl/src/gurl.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using extensions::Extension;

namespace {

class MessageSender : public content::NotificationObserver {
 public:
  MessageSender() {
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING,
                   content::NotificationService::AllSources());
  }

 private:
  static scoped_ptr<base::ListValue> BuildEventArguments(
      const bool last_message,
      const std::string& data) {
    DictionaryValue* event = new DictionaryValue();
    event->SetBoolean("lastMessage", last_message);
    event->SetString("data", data);
    scoped_ptr<base::ListValue> arguments(new base::ListValue());
    arguments->Append(event);
    return arguments.Pass();
  }

  static scoped_ptr<extensions::Event> BuildEvent(
      scoped_ptr<base::ListValue> event_args,
      Profile* profile,
      GURL event_url) {
    scoped_ptr<extensions::Event> event(new extensions::Event(
        "test.onMessage", event_args.Pass()));
    event->restrict_to_profile = profile;
    event->event_url = event_url;
    return event.Pass();
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    extensions::EventRouter* event_router =
        extensions::ExtensionSystem::Get(
            content::Source<Profile>(source).ptr())->event_router();

    // Sends four messages to the extension. All but the third message sent
    // from the origin http://b.com/ are supposed to arrive.
    event_router->BroadcastEvent(BuildEvent(
        BuildEventArguments(false, "no restriction"),
        content::Source<Profile>(source).ptr(),
        GURL()));
    event_router->BroadcastEvent(BuildEvent(
        BuildEventArguments(false, "http://a.com/"),
        content::Source<Profile>(source).ptr(),
        GURL("http://a.com/")));
    event_router->BroadcastEvent(BuildEvent(
        BuildEventArguments(false, "http://b.com/"),
        content::Source<Profile>(source).ptr(),
        GURL("http://b.com/")));
    event_router->BroadcastEvent(BuildEvent(
        BuildEventArguments(true, "last message"),
        content::Source<Profile>(source).ptr(),
        GURL()));
  }

  content::NotificationRegistrar registrar_;
};

}  // namespace

// Tests that message passing between extensions and content scripts works.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Messaging) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionTest("messaging/connect")) << message_;
}

// Tests that message passing from one extension to another works.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MessagingExternal) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("..").AppendASCII("good")
                    .AppendASCII("Extensions")
                    .AppendASCII("bjafgdebaacbbbecmhlhpofkepfkgcpa")
                    .AppendASCII("1.0")));

  ASSERT_TRUE(RunExtensionTest("messaging/connect_external")) << message_;
}

// Tests that messages with event_urls are only passed to extensions with
// appropriate permissions.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MessagingEventURL) {
  MessageSender sender;
  ASSERT_TRUE(RunExtensionTest("messaging/event_url")) << message_;
}

// Tests connecting from a panel to its extension.
class PanelMessagingTest : public ExtensionApiTest {
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnablePanels);
  }
};

IN_PROC_BROWSER_TEST_F(PanelMessagingTest, MessagingPanel) {
  ASSERT_TRUE(RunExtensionTest("messaging/connect_panel")) << message_;
}

// Tests externally_connectable between a web page and an extension.
//
// TODO(kalman): Test between extensions. This is already tested in this file,
// but not with externally_connectable set in the manifest.
//
// TODO(kalman): Test with host permissions.
class ExternallyConnectableMessagingTest : public ExtensionApiTest {
 protected:
  // Result codes from the test. These must match up with |results| in
  // c/t/d/extensions/api_test/externally_connectable/sites/assertions.json.
  enum Result {
    OK = 0,
    NAMESPACE_NOT_DEFINED = 1,
    FUNCTION_NOT_DEFINED = 2,
    COULD_NOT_ESTABLISH_CONNECTION_ERROR = 3,
    OTHER_ERROR = 4,
    INCORRECT_RESPONSE_SENDER = 5,
    INCORRECT_RESPONSE_MESSAGE = 6,
  };

  Result CanConnectAndSendMessages(const std::string& extension_id) {
    int result;
    CHECK(content::ExecuteScriptAndExtractInt(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "assertions.canConnectAndSendMessages('" + extension_id + "')",
        &result));
    return static_cast<Result>(result);
  }

  testing::AssertionResult AreAnyNonWebApisDefined() {
    // All runtime API methods are non-web except for sendRequest and connect.
    const char* non_messaging_apis[] = {
        "getBackgroundPage",
        "getManifest",
        "getURL",
        "reload",
        "requestUpdateCheck",
        "connectNative",
        "sendNativeMessage",
        "onStartup",
        "onInstalled",
        "onSuspend",
        "onSuspendCanceled",
        "onUpdateAvailable",
        "onBrowserUpdateAvailable",
        "onConnect",
        "onConnectExternal",
        "onMessage",
        "onMessageExternal",
        "onRestartRequired",
        "id",
    };

    // Turn the array into a JS array, which effectively gets eval()ed.
    std::string as_js_array;
    for (size_t i = 0; i < arraysize(non_messaging_apis); ++i) {
      as_js_array += as_js_array.empty() ? "[" : ",";
      as_js_array += base::StringPrintf("'%s'", non_messaging_apis[i]);
    }
    as_js_array += "]";

    bool any_defined;
    CHECK(content::ExecuteScriptAndExtractBool(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "assertions.areAnyRuntimePropertiesDefined(" + as_js_array + ")",
        &any_defined));
    return any_defined ?
        testing::AssertionSuccess() : testing::AssertionFailure();
  }

  GURL GetURLForPath(const std::string& host, const std::string& path) {
    std::string port = base::IntToString(embedded_test_server()->port());
    GURL::Replacements replacements;
    replacements.SetHostStr(host);
    replacements.SetPortStr(port);
    return embedded_test_server()->GetURL(path).ReplaceComponents(replacements);
  }

  GURL chromium_org_url() {
    return GetURLForPath("www.chromium.org", "/sites/chromium.org.html");
  }

  GURL google_com_url() {
    return GetURLForPath("www.google.com", "/sites/google.com.html");
  }

  scoped_refptr<const Extension> LoadTestExtension(const std::string& name) {
    return LoadExtension(test_data_dir_.AppendASCII(extension_dir())
                                       .AppendASCII(name));
  }

  void InitializeTestServer() {
    base::FilePath test_data;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data));
    embedded_test_server()->ServeFilesFromDirectory(
        test_data.AppendASCII("extensions/api_test")
                 .AppendASCII(extension_dir()));
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

    host_resolver()->AddRule("*", embedded_test_server()->base_url().host());
  }

 private:
  const char* extension_dir() {
    return "messaging/externally_connectable";
  }
};

IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest, NotInstalled) {
  InitializeTestServer();

  const char kFakeId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

  ui_test_utils::NavigateToURL(browser(), chromium_org_url());
  EXPECT_EQ(NAMESPACE_NOT_DEFINED, CanConnectAndSendMessages(kFakeId));
  EXPECT_FALSE(AreAnyNonWebApisDefined());

  ui_test_utils::NavigateToURL(browser(), google_com_url());
  EXPECT_EQ(NAMESPACE_NOT_DEFINED, CanConnectAndSendMessages(kFakeId));
  EXPECT_FALSE(AreAnyNonWebApisDefined());
}

// Tests two extensions on the same sites: one web connectable, one not.
IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       WebConnectableAndNotConnectable) {
  InitializeTestServer();

  // Install the web connectable extension. chromium.org can connect to it,
  // google.com can't.
  scoped_refptr<const Extension> web_connectable =
      LoadTestExtension("web_connectable");

  ui_test_utils::NavigateToURL(browser(), chromium_org_url());
  EXPECT_EQ(OK, CanConnectAndSendMessages(web_connectable->id()));
  EXPECT_FALSE(AreAnyNonWebApisDefined());

  ui_test_utils::NavigateToURL(browser(), google_com_url());
  EXPECT_EQ(NAMESPACE_NOT_DEFINED,
            CanConnectAndSendMessages(web_connectable->id()));
  EXPECT_FALSE(AreAnyNonWebApisDefined());

  // Install the non-connectable extension. Nothing can connect to it.
  scoped_refptr<const Extension> not_connectable =
      LoadTestExtension("not_connectable");

  ui_test_utils::NavigateToURL(browser(), chromium_org_url());
  // Namespace will be defined here because |web_connectable| can connect to
  // it - so this will be the "cannot establish connection" error.
  EXPECT_EQ(COULD_NOT_ESTABLISH_CONNECTION_ERROR,
            CanConnectAndSendMessages(not_connectable->id()));
  EXPECT_FALSE(AreAnyNonWebApisDefined());

  ui_test_utils::NavigateToURL(browser(), google_com_url());
  EXPECT_EQ(NAMESPACE_NOT_DEFINED,
            CanConnectAndSendMessages(not_connectable->id()));
  EXPECT_FALSE(AreAnyNonWebApisDefined());
}

// Tests that enabling and disabling an extension makes the runtime bindings
// appear and disappear.
//
// TODO(kalman): Test with multiple extensions that can be accessed by the same
// host.
IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       EnablingAndDisabling) {
  InitializeTestServer();

  scoped_refptr<const Extension> web_connectable =
      LoadTestExtension("web_connectable");
  scoped_refptr<const Extension> not_connectable =
      LoadTestExtension("not_connectable");

  ui_test_utils::NavigateToURL(browser(), chromium_org_url());
  EXPECT_EQ(OK, CanConnectAndSendMessages(web_connectable->id()));
  EXPECT_EQ(COULD_NOT_ESTABLISH_CONNECTION_ERROR,
            CanConnectAndSendMessages(not_connectable->id()));

  DisableExtension(web_connectable->id());
  EXPECT_EQ(COULD_NOT_ESTABLISH_CONNECTION_ERROR,
            CanConnectAndSendMessages(web_connectable->id()));

  EnableExtension(web_connectable->id());
  EXPECT_EQ(OK, CanConnectAndSendMessages(web_connectable->id()));
  EXPECT_EQ(COULD_NOT_ESTABLISH_CONNECTION_ERROR,
            CanConnectAndSendMessages(not_connectable->id()));
}

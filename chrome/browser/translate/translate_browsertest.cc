// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_BROWSERTEST_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_BROWSERTEST_H_

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "net/http/http_status_code.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace {

const base::FilePath::CharType kTranslateRoot[] =
    FILE_PATH_LITERAL("chrome/test/data/translate");
const char kNonSecurePrefix[] = "files/translate/";
const char kSecurePrefix[] = "files/";
const char kFrenchTestPath[] = "fr_test.html";
const char kRefreshMetaTagTestPath[] = "refresh_meta_tag.html";
const char kRefreshMetaTagCaseInsensitiveTestPath[] =
    "refresh_meta_tag_casei.html";
const char kRefreshMetaTagAtOnloadTestPath[] =
    "refresh_meta_tag_at_onload.html";
const char kUpdateLocationTestPath[] = "update_location.html";
const char kUpdateLocationAtOnloadTestPath[] = "update_location_at_onload.html";
const char kMainScriptPath[] = "pseudo_main.js";
const char kElementMainScriptPath[] = "pseudo_element_main.js";

};  // namespace

class TranslateBrowserTest : public InProcessBrowserTest {
 public:
  TranslateBrowserTest()
      : https_server_(net::SpawnedTestServer::TYPE_HTTPS,
                      SSLOptions(SSLOptions::CERT_OK),
                      base::FilePath(kTranslateRoot)) {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ASSERT_TRUE(https_server_.Start());
  }

 protected:
  GURL GetNonSecureURL(const std::string& path) const {
    std::string prefix(kNonSecurePrefix);
    return test_server()->GetURL(prefix + path);
  }

  GURL GetSecureURL(const std::string& path) const {
    std::string prefix(kSecurePrefix);
    return https_server_.GetURL(prefix + path);
  }

 private:
  net::SpawnedTestServer https_server_;

  typedef net::SpawnedTestServer::SSLOptions SSLOptions;

  DISALLOW_COPY_AND_ASSIGN(TranslateBrowserTest);
};

// TODO(toyoshim): This test should be changed to work in an isolated world.
// See also http://crbug.com/164547 .
IN_PROC_BROWSER_TEST_F(TranslateBrowserTest, DISABLED_Translate) {
  ASSERT_TRUE(test_server()->Start());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  net::TestURLFetcherFactory factory;

  // Setup infobar observer.
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  ASSERT_TRUE(infobar_service);
  EXPECT_EQ(0U, infobar_service->infobar_count());
  content::WindowedNotificationObserver infobar(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
      content::NotificationService::AllSources());

  // Setup page title observer.
  content::TitleWatcher watcher(web_contents, ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(ASCIIToUTF16("FAIL"));

  // Visit non-secure page which is going to be translated.
  ui_test_utils::NavigateToURL(browser(), GetNonSecureURL(kFrenchTestPath));

  // Wait for Chrome Translate infobar.
  infobar.Wait();

  // Perform Chrome Translate.
  InfoBarDelegate* delegate = infobar_service->infobar_at(0);
  ASSERT_TRUE(delegate);
  TranslateInfoBarDelegate* translate = delegate->AsTranslateInfoBarDelegate();
  ASSERT_TRUE(translate);
  translate->Translate();

  // Hook URLFetcher for element.js.
  GURL script1_url = GetSecureURL(kMainScriptPath);
  GURL script2_url = GetSecureURL(kElementMainScriptPath);
  std::string element_js = "main_script_url = '" + script1_url.spec() + "';\n";
  element_js += "element_main_script_url = '" + script2_url.spec() + "';\n";
  element_js +=
    "google = { 'translate' : { 'TranslateService' : function() { return {\n"
    "  isAvailable: function() {\n"
    "    var script = document.createElement('script');\n"
    "    script.src = main_script_url;\n"
    "    document.getElementsByTagName('head')[0].appendChild(script);\n"
    "    return true;\n"
    "  },\n"
    "  translatePage: function(sl, tl, cb) {\n"
    "    cb(1, true);\n"
    "  }\n"
    "} } } };\n"
    "cr.googleTranslate.onTranslateElementLoad();\n";
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  net::URLRequestStatus status;
  status.set_status(net::URLRequestStatus::SUCCESS);
  fetcher->set_status(status);
  fetcher->set_url(fetcher->GetOriginalURL());
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(element_js);
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  // Wait for the page title is changed after the test finished.
  const string16 result = watcher.WaitAndGetTitle();
  EXPECT_EQ("PASS", UTF16ToASCII(result));
}

IN_PROC_BROWSER_TEST_F(TranslateBrowserTest, IgnoreRefreshMetaTag) {
  ASSERT_TRUE(test_server()->Start());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // Check infobar count.
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  ASSERT_TRUE(infobar_service);
  EXPECT_EQ(0U, infobar_service->infobar_count());

  // Setup page title observer.
  content::TitleWatcher watcher(web_contents, ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(ASCIIToUTF16("FAIL"));

  // Visit a test page.
  ui_test_utils::NavigateToURL(
      browser(),
      GetNonSecureURL(kRefreshMetaTagTestPath));

  // Wait for the page title is changed after the test finished.
  const string16 result = watcher.WaitAndGetTitle();
  EXPECT_EQ("PASS", UTF16ToASCII(result));

  // Check there is not infobar.
  EXPECT_EQ(0U, infobar_service->infobar_count());
}

IN_PROC_BROWSER_TEST_F(TranslateBrowserTest,
                       IgnoreRefreshMetaTagInCaseInsensitive) {
  ASSERT_TRUE(test_server()->Start());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // Check infobar count.
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  ASSERT_TRUE(infobar_service);
  EXPECT_EQ(0U, infobar_service->infobar_count());

  // Setup page title observer.
  content::TitleWatcher watcher(web_contents, ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(ASCIIToUTF16("FAIL"));

  // Visit a test page.
  ui_test_utils::NavigateToURL(
      browser(),
      GetNonSecureURL(kRefreshMetaTagCaseInsensitiveTestPath));

  // Wait for the page title is changed after the test finished.
  const string16 result = watcher.WaitAndGetTitle();
  EXPECT_EQ("PASS", UTF16ToASCII(result));

  // Check there is not infobar.
  EXPECT_EQ(0U, infobar_service->infobar_count());
}

IN_PROC_BROWSER_TEST_F(TranslateBrowserTest, IgnoreRefreshMetaTagAtOnload) {
  ASSERT_TRUE(test_server()->Start());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // Check infobar count.
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  ASSERT_TRUE(infobar_service);
  EXPECT_EQ(0U, infobar_service->infobar_count());

  // Setup page title observer.
  content::TitleWatcher watcher(web_contents, ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(ASCIIToUTF16("FAIL"));

  // Visit a test page.
  ui_test_utils::NavigateToURL(
      browser(),
      GetNonSecureURL(kRefreshMetaTagAtOnloadTestPath));

  // Wait for the page title is changed after the test finished.
  const string16 result = watcher.WaitAndGetTitle();
  EXPECT_EQ("PASS", UTF16ToASCII(result));

  // Check there is not infobar.
  EXPECT_EQ(0U, infobar_service->infobar_count());
}

IN_PROC_BROWSER_TEST_F(TranslateBrowserTest, UpdateLocation) {
  ASSERT_TRUE(test_server()->Start());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // Check infobar count.
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  ASSERT_TRUE(infobar_service);
  EXPECT_EQ(0U, infobar_service->infobar_count());

  // Setup page title observer.
  content::TitleWatcher watcher(web_contents, ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(ASCIIToUTF16("FAIL"));

  // Visit a test page.
  ui_test_utils::NavigateToURL(
      browser(),
      GetNonSecureURL(kUpdateLocationTestPath));

  // Wait for the page title is changed after the test finished.
  const string16 result = watcher.WaitAndGetTitle();
  EXPECT_EQ("PASS", UTF16ToASCII(result));

  // Check there is not infobar.
  EXPECT_EQ(0U, infobar_service->infobar_count());
}

IN_PROC_BROWSER_TEST_F(TranslateBrowserTest, UpdateLocationAtOnload) {
  ASSERT_TRUE(test_server()->Start());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // Check infobar count.
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  ASSERT_TRUE(infobar_service);
  EXPECT_EQ(0U, infobar_service->infobar_count());

  // Setup page title observer.
  content::TitleWatcher watcher(web_contents, ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(ASCIIToUTF16("FAIL"));

  // Visit a test page.
  ui_test_utils::NavigateToURL(
      browser(),
      GetNonSecureURL(kUpdateLocationAtOnloadTestPath));

  // Wait for the page title is changed after the test finished.
  const string16 result = watcher.WaitAndGetTitle();
  EXPECT_EQ("PASS", UTF16ToASCII(result));

  // Check there is not infobar.
  EXPECT_EQ(0U, infobar_service->infobar_count());
}

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_BROWSERTEST_H_

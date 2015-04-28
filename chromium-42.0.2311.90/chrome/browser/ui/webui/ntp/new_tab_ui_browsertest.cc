// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "url/gurl.h"

using content::OpenURLParams;
using content::Referrer;

namespace {

static bool had_console_errors = false;

bool HandleMessage(int severity,
                   const char* file,
                   int line,
                   size_t message_start,
                   const std::string& str) {
  if (severity == logging::LOG_ERROR && file && file == std::string("CONSOLE"))
    had_console_errors = true;
  return false;
}

}  // namespace

class NewTabUIBrowserTest : public InProcessBrowserTest {
 public:
  NewTabUIBrowserTest() {
    logging::SetLogMessageHandler(&HandleMessage);
  }

  ~NewTabUIBrowserTest() override { logging::SetLogMessageHandler(NULL); }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    ASSERT_FALSE(had_console_errors);
  }
};

// TODO(samarth): delete along with rest of NTP4 code.
// #if defined(OS_WIN)
// // Flaky on Windows (http://crbug.com/174819)
// #define MAYBE_LoadNTPInExistingProcess DISABLED_LoadNTPInExistingProcess
// #else
// #define MAYBE_LoadNTPInExistingProcess LoadNTPInExistingProcess
// #endif

// Ensure loading a NTP with an existing SiteInstance in a reused process
// doesn't cause us to kill the process.  See http://crbug.com/104258.
IN_PROC_BROWSER_TEST_F(NewTabUIBrowserTest, DISABLED_LoadNTPInExistingProcess) {
  // Set max renderers to 1 to force running out of processes.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);

  // Start server for simple page.
  ASSERT_TRUE(test_server()->Start());

  // Load a NTP in a new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_EQ(1,
            browser()->tab_strip_model()->GetWebContentsAt(1)->GetMaxPageID());

  // Navigate that tab to another site.  This allows the NTP process to exit,
  // but it keeps the NTP SiteInstance (and its max_page_id) alive in history.
  {
    // Wait not just for the navigation to finish, but for the NTP process to
    // exit as well.
    content::RenderProcessHostWatcher process_exited_observer(
        browser()->tab_strip_model()->GetActiveWebContents(),
        content::RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
    browser()->OpenURL(OpenURLParams(
        test_server()->GetURL("files/title1.html"), Referrer(), CURRENT_TAB,
        ui::PAGE_TRANSITION_TYPED, false));
    process_exited_observer.Wait();
  }

  // Creating a NTP in another tab should not be affected, since page IDs
  // are now specific to a tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_EQ(1,
            browser()->tab_strip_model()->GetWebContentsAt(2)->GetMaxPageID());
  chrome::CloseTab(browser());

  // Open another Web UI page in a new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUISettingsURL), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_EQ(1,
            browser()->tab_strip_model()->GetWebContentsAt(2)->GetMaxPageID());

  // At this point, opening another NTP will use the existing WebUI process
  // but its own SiteInstance, so the page IDs shouldn't affect each other.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_EQ(1,
            browser()->tab_strip_model()->GetWebContentsAt(3)->GetMaxPageID());

  // Navigating to the NTP in the original tab causes a BrowsingInstance
  // swap, so it gets a new SiteInstance starting with page ID 1 again.
  browser()->tab_strip_model()->ActivateTabAt(1, true);
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  EXPECT_EQ(1,
            browser()->tab_strip_model()->GetWebContentsAt(1)->GetMaxPageID());
}

// TODO(samarth): delete along with rest of NTP4 code.
// Loads chrome://hang/ into two NTP tabs, ensuring we don't crash.
// See http://crbug.com/59859.
// If this flakes, use http://crbug.com/87200.
IN_PROC_BROWSER_TEST_F(NewTabUIBrowserTest, DISABLED_ChromeHangInNTP) {
  // Bring up a new tab page.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Navigate to chrome://hang/ to stall the process.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(content::kChromeUIHangURL), CURRENT_TAB, 0);

  // Visit chrome://hang/ again in another NTP. Don't bother waiting for the
  // NTP to load, because it's hung.
  chrome::NewTab(browser());
  browser()->OpenURL(OpenURLParams(
      GURL(content::kChromeUIHangURL), Referrer(), CURRENT_TAB,
      ui::PAGE_TRANSITION_TYPED, false));
}

// Navigate to incognito NTP. Fails if there are console errors.
IN_PROC_BROWSER_TEST_F(NewTabUIBrowserTest, ShowIncognito) {
  ui_test_utils::NavigateToURL(CreateIncognitoBrowser(),
                               GURL(chrome::kChromeUINewTabURL));
}

class NewTabUIProcessPerTabTest : public NewTabUIBrowserTest {
 public:
   NewTabUIProcessPerTabTest() {}

   void SetUpCommandLine(base::CommandLine* command_line) override {
     command_line->AppendSwitch(switches::kProcessPerTab);
   }
};

// Navigates away from NTP before it commits, in process-per-tab mode.
// Ensures that we don't load the normal page in the NTP process (and thus
// crash), as in http://crbug.com/69224.
// If this flakes, use http://crbug.com/87200
IN_PROC_BROWSER_TEST_F(NewTabUIProcessPerTabTest, NavBeforeNTPCommits) {
  // Bring up a new tab page.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  // Navigate to chrome://hang/ to stall the process.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(content::kChromeUIHangURL), CURRENT_TAB, 0);

  // Visit a normal URL in another NTP that hasn't committed.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL), NEW_FOREGROUND_TAB, 0);

  // We don't use ui_test_utils::NavigateToURLWithDisposition because that waits
  // for current loading to stop.
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  browser()->OpenURL(OpenURLParams(
      GURL("data:text/html,hello world"), Referrer(), CURRENT_TAB,
      ui::PAGE_TRANSITION_TYPED, false));
  observer.Wait();
}

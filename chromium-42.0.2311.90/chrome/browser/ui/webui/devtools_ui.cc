// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/devtools_ui.h"

#include <string>

#include "base/command_line.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/devtools/device/devtools_android_bridge.h"
#include "chrome/browser/devtools/devtools_target_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_http_handler.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserThread;
using content::WebContents;

namespace {

std::string PathWithoutParams(const std::string& path) {
  return GURL(std::string("chrome-devtools://devtools/") + path)
      .path().substr(1);
}

const char kRemoteFrontendDomain[] = "chrome-devtools-frontend.appspot.com";
const char kRemoteFrontendBase[] =
    "https://chrome-devtools-frontend.appspot.com/";
const char kHttpNotFound[] = "HTTP/1.1 404 Not Found\n\n";

#if defined(DEBUG_DEVTOOLS)
// Local frontend url provided by InspectUI.
const char kFallbackFrontendURL[] =
    "chrome-devtools://devtools/bundled/inspector.html";
#else
// URL causing the DevTools window to display a plain text warning.
const char kFallbackFrontendURL[] =
    "data:text/plain,Cannot load DevTools frontend from an untrusted origin";
#endif  // defined(DEBUG_DEVTOOLS)

const char kRemoteOpenPrefix[] = "remote/open";

#if defined(DEBUG_DEVTOOLS)
const char kLocalSerial[] = "local";
#endif  // defined(DEBUG_DEVTOOLS)

// DevToolsDataSource ---------------------------------------------------------

std::string GetMimeTypeForPath(const std::string& path) {
  std::string filename = PathWithoutParams(path);
  if (EndsWith(filename, ".html", false)) {
    return "text/html";
  } else if (EndsWith(filename, ".css", false)) {
    return "text/css";
  } else if (EndsWith(filename, ".js", false)) {
    return "application/javascript";
  } else if (EndsWith(filename, ".png", false)) {
    return "image/png";
  } else if (EndsWith(filename, ".gif", false)) {
    return "image/gif";
  } else if (EndsWith(filename, ".manifest", false)) {
    return "text/cache-manifest";
  }
  return "text/html";
}

// An URLDataSource implementation that handles chrome-devtools://devtools/
// requests. Three types of requests could be handled based on the URL path:
// 1. /bundled/: bundled DevTools frontend is served.
// 2. /remote/: remote DevTools frontend is served from App Engine.
// 3. /remote/open/: query is URL which is opened on remote device.
class DevToolsDataSource : public content::URLDataSource,
                           public net::URLFetcherDelegate {
 public:
  using GotDataCallback = content::URLDataSource::GotDataCallback;

  explicit DevToolsDataSource(net::URLRequestContextGetter* request_context);

  // content::URLDataSource implementation.
  std::string GetSource() const override;

  void StartDataRequest(const std::string& path,
                        int render_process_id,
                        int render_frame_id,
                        const GotDataCallback& callback) override;

 private:
  // content::URLDataSource overrides.
  std::string GetMimeType(const std::string& path) const override;
  bool ShouldAddContentSecurityPolicy() const override;
  bool ShouldDenyXFrameOptions() const override;
  bool ShouldServeMimeTypeAsContentTypeHeader() const override;

  // net::URLFetcherDelegate overrides.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // Serves bundled DevTools frontend from ResourceBundle.
  void StartBundledDataRequest(const std::string& path,
                               int render_process_id,
                               int render_frame_id,
                               const GotDataCallback& callback);

  // Serves remote DevTools frontend from hard-coded App Engine domain.
  void StartRemoteDataRequest(const std::string& path,
                              int render_process_id,
                              int render_frame_id,
                              const GotDataCallback& callback);

  ~DevToolsDataSource() override;

  scoped_refptr<net::URLRequestContextGetter> request_context_;

  using PendingRequestsMap = std::map<const net::URLFetcher*, GotDataCallback>;
  PendingRequestsMap pending_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsDataSource);
};

DevToolsDataSource::DevToolsDataSource(
    net::URLRequestContextGetter* request_context)
    : request_context_(request_context) {
}

DevToolsDataSource::~DevToolsDataSource() {
  for (const auto& pair : pending_) {
    delete pair.first;
    pair.second.Run(
        new base::RefCountedStaticMemory(kHttpNotFound, strlen(kHttpNotFound)));
  }
}

std::string DevToolsDataSource::GetSource() const {
  return chrome::kChromeUIDevToolsHost;
}

void DevToolsDataSource::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  // Serve request from local bundle.
  std::string bundled_path_prefix(chrome::kChromeUIDevToolsBundledPath);
  bundled_path_prefix += "/";
  if (StartsWithASCII(path, bundled_path_prefix, false)) {
    StartBundledDataRequest(path.substr(bundled_path_prefix.length()),
                            render_process_id, render_frame_id, callback);
    return;
  }

  // Serve static response while connecting to the remote device.
  if (StartsWithASCII(path, kRemoteOpenPrefix, false)) {
    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableDevToolsExperiments)) {
      callback.Run(NULL);
      return;
    }
    std::string response = "Connecting to the device...";
    callback.Run(base::RefCountedString::TakeString(&response));
    return;
  }

  // Serve request from remote location.
  std::string remote_path_prefix(chrome::kChromeUIDevToolsRemotePath);
  remote_path_prefix += "/";
  if (StartsWithASCII(path, remote_path_prefix, false)) {
    StartRemoteDataRequest(path.substr(remote_path_prefix.length()),
                           render_process_id, render_frame_id, callback);
    return;
  }

  callback.Run(NULL);
}

std::string DevToolsDataSource::GetMimeType(const std::string& path) const {
  return GetMimeTypeForPath(path);
}

bool DevToolsDataSource::ShouldAddContentSecurityPolicy() const {
  return false;
}

bool DevToolsDataSource::ShouldDenyXFrameOptions() const {
  return false;
}

bool DevToolsDataSource::ShouldServeMimeTypeAsContentTypeHeader() const {
  return true;
}

void DevToolsDataSource::StartBundledDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  std::string filename = PathWithoutParams(path);

  int resource_id =
      content::DevToolsHttpHandler::GetFrontendResourceId(filename);

  DLOG_IF(WARNING, resource_id == -1)
      << "Unable to find dev tool resource: " << filename
      << ". If you compiled with debug_devtools=1, try running with "
         "--debug-devtools.";
  const ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  scoped_refptr<base::RefCountedStaticMemory> bytes(rb.LoadDataResourceBytes(
      resource_id));
  callback.Run(bytes.get());
}

void DevToolsDataSource::StartRemoteDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  GURL url = GURL(kRemoteFrontendBase + path);
  CHECK_EQ(url.host(), kRemoteFrontendDomain);
  if (!url.is_valid()) {
    callback.Run(
        new base::RefCountedStaticMemory(kHttpNotFound, strlen(kHttpNotFound)));
    return;
  }
  net::URLFetcher* fetcher =
      net::URLFetcher::Create(url, net::URLFetcher::GET, this);
  pending_[fetcher] = callback;
  fetcher->SetRequestContext(request_context_.get());
  fetcher->Start();
}

void DevToolsDataSource::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(source);
  PendingRequestsMap::iterator it = pending_.find(source);
  DCHECK(it != pending_.end());
  std::string response;
  source->GetResponseAsString(&response);
  delete source;
  it->second.Run(base::RefCountedString::TakeString(&response));
  pending_.erase(it);
}

// OpenRemotePageRequest ------------------------------------------------------

class OpenRemotePageRequest : public DevToolsAndroidBridge::DeviceListListener {
 public:
  OpenRemotePageRequest(
      Profile* profile,
      const std::string url,
      const DevToolsAndroidBridge::RemotePageCallback& callback);
  ~OpenRemotePageRequest() override {}

 private:
  // DevToolsAndroidBridge::Listener overrides.
  void DeviceListChanged(
      const DevToolsAndroidBridge::RemoteDevices& devices) override;

  bool OpenInBrowser(
      scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> browser);
  void RemotePageOpened(scoped_refptr<DevToolsAndroidBridge::RemotePage> page);

  std::string url_;
  DevToolsAndroidBridge::RemotePageCallback callback_;
  bool opening_;
  DevToolsAndroidBridge* android_bridge_;

  DISALLOW_COPY_AND_ASSIGN(OpenRemotePageRequest);
};

OpenRemotePageRequest::OpenRemotePageRequest(
    Profile* profile,
    const std::string url,
    const DevToolsAndroidBridge::RemotePageCallback& callback)
    : url_(url),
      callback_(callback),
      opening_(false),
      android_bridge_(
          DevToolsAndroidBridge::Factory::GetForProfile(profile)) {
  android_bridge_->AddDeviceListListener(this);
}

void OpenRemotePageRequest::DeviceListChanged(
    const DevToolsAndroidBridge::RemoteDevices& devices) {
  if (opening_)
    return;

  for (DevToolsAndroidBridge::RemoteDevices::const_iterator dit =
      devices.begin(); dit != devices.end(); ++dit) {
    DevToolsAndroidBridge::RemoteDevice* device = dit->get();
    if (!device->is_connected())
      continue;

    DevToolsAndroidBridge::RemoteBrowsers& browsers = device->browsers();
    for (DevToolsAndroidBridge::RemoteBrowsers::iterator bit =
        browsers.begin(); bit != browsers.end(); ++bit) {
      if (OpenInBrowser(bit->get())) {
        opening_ = true;
        return;
      }
    }
  }
}

bool OpenRemotePageRequest::OpenInBrowser(
    scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> browser) {
  if (!browser->IsChrome())
    return false;
#if defined(DEBUG_DEVTOOLS)
  if (browser->serial() == kLocalSerial)
    return false;
#endif  // defined(DEBUG_DEVTOOLS)
  android_bridge_->OpenRemotePage(
      browser,
      url_,
      base::Bind(&OpenRemotePageRequest::RemotePageOpened,
                 base::Unretained(this)));
  return true;
}

void OpenRemotePageRequest::RemotePageOpened(
    scoped_refptr<DevToolsAndroidBridge::RemotePage> page) {
  callback_.Run(page);
  android_bridge_->RemoveDeviceListListener(this);
  delete this;
}

}  // namespace

// DevToolsUI -----------------------------------------------------------------

// static
GURL DevToolsUI::GetProxyURL(const std::string& frontend_url) {
  GURL url(frontend_url);
  if (!url.is_valid() || url.host() != kRemoteFrontendDomain)
    return GURL(kFallbackFrontendURL);
  return GURL(base::StringPrintf("%s://%s/%s/%s",
              content::kChromeDevToolsScheme,
              chrome::kChromeUIDevToolsHost,
              chrome::kChromeUIDevToolsRemotePath,
              url.path().substr(1).c_str()));
}

DevToolsUI::DevToolsUI(content::WebUI* web_ui)
    : WebUIController(web_ui),
      content::WebContentsObserver(web_ui->GetWebContents()),
      bindings_(web_ui->GetWebContents()),
      weak_factory_(this) {
  web_ui->SetBindings(0);
  Profile* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(
      profile,
      new DevToolsDataSource(profile->GetRequestContext()));
}

DevToolsUI::~DevToolsUI() {
}

void DevToolsUI::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  content::NavigationEntry* entry = load_details.entry;
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableDevToolsExperiments)) {
    return;
  }

  if (entry->GetVirtualURL() == remote_frontend_loading_url_) {
    remote_frontend_loading_url_ = GURL();
    return;
  }

  std::string path = entry->GetVirtualURL().path().substr(1);
  if (!StartsWithASCII(path, kRemoteOpenPrefix, false))
    return;

  bindings_.Detach();
  remote_page_opening_url_ = entry->GetVirtualURL();
  new OpenRemotePageRequest(Profile::FromWebUI(web_ui()),
                            entry->GetVirtualURL().query(),
                            base::Bind(&DevToolsUI::RemotePageOpened,
                                       weak_factory_.GetWeakPtr(),
                                       entry->GetVirtualURL()));
}

void DevToolsUI::RemotePageOpened(
    const GURL& virtual_url,
    scoped_refptr<DevToolsAndroidBridge::RemotePage> page) {
  // Already navigated away while connecting to remote device.
  if (remote_page_opening_url_ != virtual_url)
    return;

  remote_page_opening_url_ = GURL();

  Profile* profile = Profile::FromWebUI(web_ui());
  GURL url = DevToolsUIBindings::ApplyThemeToURL(profile,
      DevToolsUI::GetProxyURL(page->frontend_url()));

  content::NavigationController& navigation_controller =
      web_ui()->GetWebContents()->GetController();
  content::NavigationController::LoadURLParams params(url);
  params.should_replace_current_entry = true;
  remote_frontend_loading_url_ = virtual_url;
  navigation_controller.LoadURLWithParams(params);
  navigation_controller.GetPendingEntry()->SetVirtualURL(virtual_url);

  DevToolsAndroidBridge* bridge =
      DevToolsAndroidBridge::Factory::GetForProfile(profile);
  scoped_ptr<DevToolsTargetImpl> target(bridge->CreatePageTarget(page));
  bindings_.AttachTo(target->GetAgentHost());
}

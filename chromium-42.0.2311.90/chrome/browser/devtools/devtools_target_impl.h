// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_TARGET_IMPL_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_TARGET_IMPL_H_

#include <vector>

#include "base/callback.h"
#include "content/public/browser/devtools_target.h"
#include "content/public/browser/worker_service.h"

class Profile;

namespace content {
class DevToolsAgentHost;
class RenderViewHost;
class WebContents;
}

class DevToolsTargetImpl : public content::DevToolsTarget {
 public:
  static const char kTargetTypeApp[];
  static const char kTargetTypeBackgroundPage[];
  static const char kTargetTypePage[];
  static const char kTargetTypeWorker[];
  static const char kTargetTypeWebView[];
  static const char kTargetTypeIFrame[];
  static const char kTargetTypeOther[];
  static const char kTargetTypeServiceWorker[];

  explicit DevToolsTargetImpl(
      scoped_refptr<content::DevToolsAgentHost> agent_host);
  ~DevToolsTargetImpl() override;

  // content::DevToolsTarget overrides:
  std::string GetId() const override;
  std::string GetParentId() const override;
  std::string GetType() const override;
  std::string GetTitle() const override;
  std::string GetDescription() const override;
  GURL GetURL() const override;
  GURL GetFaviconURL() const override;
  base::TimeTicks GetLastActivityTime() const override;
  scoped_refptr<content::DevToolsAgentHost> GetAgentHost() const override;
  bool IsAttached() const override;
  bool Activate() const override;
  bool Close() const override;

  // Returns the WebContents associated with the target on NULL if there is
  // not any.
  virtual content::WebContents* GetWebContents() const;

  // Returns the tab id if the target is associated with a tab, -1 otherwise.
  virtual int GetTabId() const;

  // Returns the extension id if the target is associated with an extension
  // background page.
  virtual std::string GetExtensionId() const;

  // Open a new DevTools window or activate the existing one.
  virtual void Inspect(Profile* profile) const;

  // Reload the target page.
  virtual void Reload() const;

  // Creates a new target associated with WebContents.
  static scoped_ptr<DevToolsTargetImpl> CreateForWebContents(
      content::WebContents* web_contents,
      bool is_tab);

  void set_parent_id(const std::string& parent_id) { parent_id_ = parent_id; }
  void set_type(const std::string& type) { type_ = type; }
  void set_title(const std::string& title) { title_ = title; }
  void set_description(const std::string& desc) { description_ = desc; }
  void set_url(const GURL& url) { url_ = url; }
  void set_favicon_url(const GURL& url) { favicon_url_ = url; }
  void set_last_activity_time(const base::TimeTicks& time) {
     last_activity_time_ = time;
  }

  typedef std::vector<DevToolsTargetImpl*> List;
  typedef base::Callback<void(const List&)> Callback;

  static void EnumerateAllTargets(Callback callback);

 private:
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  std::string parent_id_;
  std::string type_;
  std::string title_;
  std::string description_;
  GURL url_;
  GURL favicon_url_;
  base::TimeTicks last_activity_time_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_TARGET_IMPL_H_

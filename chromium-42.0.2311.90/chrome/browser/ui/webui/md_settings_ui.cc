// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/md_settings_ui.h"

#include <string>

#include "base/values.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"

MdSettingsUI::MdSettingsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIMdSettingsHost);

  html_source->SetDefaultResource(IDR_MD_SETTINGS_UI_HTML);

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html_source);
}

MdSettingsUI::~MdSettingsUI() {
}

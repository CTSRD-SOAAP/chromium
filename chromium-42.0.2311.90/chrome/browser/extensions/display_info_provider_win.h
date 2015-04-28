// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_DISPLAY_INFO_PROVIDER_WIN_H_
#define CHROME_BROWSER_EXTENSIONS_DISPLAY_INFO_PROVIDER_WIN_H_

#include "extensions/browser/api/system_display/display_info_provider.h"

namespace extensions {

class DisplayInfoProviderWin : public DisplayInfoProvider {
 public:
  DisplayInfoProviderWin();
  virtual ~DisplayInfoProviderWin();

  // DisplayInfoProvider implementation.
  virtual bool SetInfo(const std::string& display_id,
                       const core_api::system_display::DisplayProperties& info,
                       std::string* error) override;
  virtual void UpdateDisplayUnitInfoForPlatform(
      const gfx::Display& display,
      core_api::system_display::DisplayUnitInfo* unit) override;
  virtual gfx::Screen* GetActiveScreen() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayInfoProviderWin);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_DISPLAY_INFO_PROVIDER_WIN_H_

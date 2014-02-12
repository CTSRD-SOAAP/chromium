// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FIRST_RUN_FIRST_RUN_HELPER_IMPL_H_
#define ASH_FIRST_RUN_FIRST_RUN_HELPER_IMPL_H_

#include "ash/first_run/first_run_helper.h"
#include "base/compiler_specific.h"

namespace ash {

class Shell;

class FirstRunHelperImpl : public FirstRunHelper {
 public:
  FirstRunHelperImpl();

  // Overriden from FirstRunHelper.
  virtual void OpenAppList() OVERRIDE;
  virtual void CloseAppList() OVERRIDE;
  virtual gfx::Rect GetLauncherBounds() OVERRIDE;
  virtual gfx::Rect GetAppListButtonBounds() OVERRIDE;
  virtual gfx::Rect GetAppListBounds() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(FirstRunHelperImpl);
};

}  // namespace ash

#endif  // ASH_FIRST_RUN_FIRST_RUN_HELPER_IMPL_H_


// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/window_finder.h"

#include "ash/shell_window_ids.h"
#include "ash/wm/coordinate_conversion.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"

namespace {

gfx::NativeWindow GetLocalProcessWindowAtPointImpl(
    const gfx::Point& screen_point,
    const std::set<gfx::NativeWindow>& ignore,
    gfx::NativeWindow window) {
  if (ignore.find(window) != ignore.end())
    return NULL;

  if (!window->IsVisible())
    return NULL;

  if (window->id() == ash::kShellWindowId_PhantomWindow ||
      window->id() == ash::kShellWindowId_OverlayContainer ||
      window->id() == ash::kShellWindowId_MouseCursorContainer)
    return NULL;

  if (window->layer()->type() == ui::LAYER_TEXTURED)
    return window->GetBoundsInScreen().Contains(screen_point) ? window : NULL;

  for (aura::Window::Windows::const_reverse_iterator i =
           window->children().rbegin(); i != window->children().rend(); ++i) {
    gfx::NativeWindow result =
        GetLocalProcessWindowAtPointImpl(screen_point, ignore, *i);
    if (result)
      return result;
  }
  return NULL;
}

}  // namespace

gfx::NativeWindow GetLocalProcessWindowAtPointAsh(
    const gfx::Point& screen_point,
    const std::set<gfx::NativeWindow>& ignore) {
  return GetLocalProcessWindowAtPointImpl(
      screen_point, ignore, ::ash::wm::GetRootWindowAt(screen_point));
}

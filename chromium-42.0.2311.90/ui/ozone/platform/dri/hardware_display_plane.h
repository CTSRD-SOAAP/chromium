// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_HARDWARE_DISPLAY_PLANE_H_
#define UI_OZONE_PLATFORM_DRI_HARDWARE_DISPLAY_PLANE_H_

#include <stddef.h>
#include <stdint.h>

#include "base/basictypes.h"
#include "ui/ozone/ozone_export.h"
#include "ui/ozone/platform/dri/scoped_drm_types.h"

namespace gfx {
class Rect;
}

namespace ui {

class DriWrapper;

class OZONE_EXPORT HardwareDisplayPlane {
 public:
  HardwareDisplayPlane(ScopedDrmPlanePtr plane);
  HardwareDisplayPlane(uint32_t plane_id, uint32_t possible_crtcs);

  virtual ~HardwareDisplayPlane();

  bool Initialize(DriWrapper* drm);

  bool CanUseForCrtc(uint32_t crtc_index);

  bool in_use() const { return in_use_; }
  void set_in_use(bool in_use) { in_use_ = in_use; }

  uint32_t plane_id() const { return plane_id_; }

  void set_owning_crtc(uint32_t crtc) { owning_crtc_ = crtc; }
  uint32_t owning_crtc() const { return owning_crtc_; }

 protected:
  uint32_t plane_id_;
  uint32_t possible_crtcs_;
  uint32_t owning_crtc_;
  bool in_use_;

  DISALLOW_COPY_AND_ASSIGN(HardwareDisplayPlane);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_HARDWARE_DISPLAY_PLANE_H_

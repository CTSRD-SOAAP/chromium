// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_POWER_POWER_STATUS_VIEW_H_
#define ASH_SYSTEM_CHROMEOS_POWER_POWER_STATUS_VIEW_H_

#include "chromeos/dbus/power_supply_status.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
}

namespace ash {
namespace internal {

class PowerStatusView : public views::View {
 public:
  enum ViewType {
    VIEW_DEFAULT,
    VIEW_NOTIFICATION
  };

  PowerStatusView(ViewType view_type, bool default_view_right_align);
  virtual ~PowerStatusView() {}

  void UpdatePowerStatus(const chromeos::PowerSupplyStatus& status);
  const base::string16& accessible_name() const { return accessible_name_; }

  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual int GetHeightForWidth(int width) OVERRIDE;
  virtual void Layout() OVERRIDE;

 private:
  void LayoutDefaultView();
  void LayoutNotificationView();
  void UpdateText();
  void UpdateIcon();
  void Update();
  void UpdateTextForDefaultView();
  void UpdateTextForNotificationView();
  base::string16 GetBatteryTimeAccessibilityString(int hour, int min);

  // Overridden from views::View.
  virtual void ChildPreferredSizeChanged(views::View* child) OVERRIDE;

  // Layout default view UI items on the right side of system tray pop up item
  // if true; otherwise, layout the UI items on the left side.
  bool default_view_right_align_;

  // Labels used only for VIEW_NOTIFICATION.
  views::Label* status_label_;
  views::Label* time_label_;

  // Labels used only for VIEW_DEFAULT.
  views::Label* time_status_label_;
  views::Label* percentage_label_;

  // Battery status indicator icon.
  views::ImageView* icon_;

  // Index of the current icon in the icon array image, or -1 if unknown.
  int icon_image_index_;

  // Horizontal offset of the current icon in the icon array image.
  int icon_image_offset_;

  // Battery charging may be unreliable for non-standard power supplies.
  // It may change from charging to discharging frequently depending on
  // charger power and current power consumption. We show different UIs
  // when in this state. See TrayPower::IsBatteryChargingUnreliable.
  bool battery_charging_unreliable_;

  ViewType view_type_;

  chromeos::PowerSupplyStatus supply_status_;

  base::string16 accessible_name_;

  DISALLOW_COPY_AND_ASSIGN(PowerStatusView);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_POWER_POWER_STATUS_VIEW_H_

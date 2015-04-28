// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/platform/x11/x11_hotplug_event_handler.h"

#include <X11/Xatom.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/worker_pool.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_hotplug_event_observer.h"
#include "ui/events/devices/device_util_linux.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/gfx/x/x11_types.h"

namespace ui {

namespace {

// Names of all known internal devices that should not be considered as
// keyboards.
// TODO(rsadam@): Identify these devices using udev rules. (Crbug.com/420728.)
const char* kKnownInvalidKeyboardDeviceNames[] = {"Power Button",
                                                  "Sleep Button",
                                                  "Video Bus",
                                                  "gpio-keys.12",
                                                  "ROCKCHIP-I2S Headset Jack"};

const char* kCachedAtomList[] = {
  "Abs MT Position X",
  "Abs MT Position Y",
  NULL,
};

typedef base::Callback<void(const std::vector<KeyboardDevice>&)>
    KeyboardDeviceCallback;

typedef base::Callback<void(const std::vector<TouchscreenDevice>&)>
    TouchscreenDeviceCallback;

// Used for updating the state on the UI thread once device information is
// parsed on helper threads.
struct UiCallbacks {
  KeyboardDeviceCallback keyboard_callback;
  TouchscreenDeviceCallback touchscreen_callback;
};

// Stores a copy of the XIValuatorClassInfo values so X11 device processing can
// happen on a worker thread. This is needed since X11 structs are not copyable.
struct ValuatorClassInfo {
  ValuatorClassInfo(const XIValuatorClassInfo& info)
      : label(info.label),
        max(info.max),
        min(info.min),
        mode(info.mode),
        number(info.number) {}

  Atom label;
  double max;
  double min;
  int mode;
  int number;
};

// Stores a copy of the XITouchClassInfo values so X11 device processing can
// happen on a worker thread. This is needed since X11 structs are not copyable.
struct TouchClassInfo {
  TouchClassInfo() : mode(0), num_touches(0) {}

  explicit TouchClassInfo(const XITouchClassInfo& info)
      : mode(info.mode), num_touches(info.num_touches) {}

  int mode;
  int num_touches;
};

struct DeviceInfo {
  DeviceInfo(const XIDeviceInfo& device, const base::FilePath& path)
      : id(device.deviceid),
        name(device.name),
        use(device.use),
        enabled(device.enabled),
        path(path) {
    for (int i = 0; i < device.num_classes; ++i) {
      switch (device.classes[i]->type) {
        case XIValuatorClass:
          valuator_class_infos.push_back(ValuatorClassInfo(
              *reinterpret_cast<XIValuatorClassInfo*>(device.classes[i])));
          break;
        case XITouchClass:
          // A device can have at most one XITouchClassInfo. Ref:
          // http://manpages.ubuntu.com/manpages/saucy/man3/XIQueryDevice.3.html
          DCHECK(!touch_class_info.mode);
          touch_class_info = TouchClassInfo(
              *reinterpret_cast<XITouchClassInfo*>(device.classes[i]));
          break;
        default:
          break;
      }
    }
  }

  // Unique device identifier.
  int id;

  // Internal device name.
  std::string name;

  // Device type (ie: XIMasterPointer)
  int use;

  // Specifies if the device is enabled and can send events.
  bool enabled;

  // Path to the actual device (ie: /dev/input/eventXX)
  base::FilePath path;

  std::vector<ValuatorClassInfo> valuator_class_infos;

  TouchClassInfo touch_class_info;
};

// X11 display cache used on worker threads. This is filled on the UI thread and
// passed in to the worker threads.
struct DisplayState {
  Atom mt_position_x;
  Atom mt_position_y;
};

// Returns true if |name| is the name of a known invalid keyboard device. Note,
// this may return false negatives.
bool IsKnownInvalidKeyboardDevice(const std::string& name) {
  for (const char* device_name : kKnownInvalidKeyboardDeviceNames) {
    if (name == device_name)
      return true;
  }
  return false;
}

// Returns true if |name| is the name of a known XTEST device. Note, this may
// return false negatives.
bool IsTestKeyboard(const std::string& name) {
  return name.find("XTEST") != std::string::npos;
}

base::FilePath GetDevicePath(XDisplay* dpy, const XIDeviceInfo& device) {
  // Skip the main pointer and keyboard since XOpenDevice() generates a
  // BadDevice error when passed these devices.
  if (device.use == XIMasterPointer || device.use == XIMasterKeyboard)
    return base::FilePath();

  // Input device has a property "Device Node" pointing to its dev input node,
  // e.g.   Device Node (250): "/dev/input/event8"
  Atom device_node = XInternAtom(dpy, "Device Node", False);
  if (device_node == None)
    return base::FilePath();

  Atom actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  unsigned char* data;
  XDevice* dev = XOpenDevice(dpy, device.deviceid);
  if (!dev)
    return base::FilePath();

  if (XGetDeviceProperty(dpy,
                         dev,
                         device_node,
                         0,
                         1000,
                         False,
                         AnyPropertyType,
                         &actual_type,
                         &actual_format,
                         &nitems,
                         &bytes_after,
                         &data) != Success) {
    XCloseDevice(dpy, dev);
    return base::FilePath();
  }

  std::string path;
  // Make sure the returned value is a string.
  if (actual_type == XA_STRING && actual_format == 8)
    path = reinterpret_cast<char*>(data);

  XFree(data);
  XCloseDevice(dpy, dev);

  return base::FilePath(path);
}

// Helper used to parse keyboard information. When it is done it uses
// |reply_runner| and |callback| to update the state on the UI thread.
void HandleKeyboardDevicesInWorker(
    const std::vector<DeviceInfo>& device_infos,
    scoped_refptr<base::TaskRunner> reply_runner,
    const KeyboardDeviceCallback& callback) {
  std::vector<KeyboardDevice> devices;

  for (const DeviceInfo& device_info : device_infos) {
    if (!device_info.enabled || device_info.use != XISlaveKeyboard)
      continue;  // Assume all keyboards are keyboard slaves
    std::string device_name(device_info.name);
    base::TrimWhitespaceASCII(device_name, base::TRIM_TRAILING, &device_name);
    if (IsTestKeyboard(device_name))
      continue;  // Skip test devices.
    if (IsKnownInvalidKeyboardDevice(device_name))
      continue;  // Skip invalid devices.
    InputDeviceType type = GetInputDeviceTypeFromPath(device_info.path);
    devices.push_back(KeyboardDevice(device_info.id, type));
  }

  reply_runner->PostTask(FROM_HERE, base::Bind(callback, devices));
}

// Helper used to parse touchscreen information. When it is done it uses
// |reply_runner| and |callback| to update the state on the UI thread.
void HandleTouchscreenDevicesInWorker(
    const std::vector<DeviceInfo>& device_infos,
    const DisplayState& display_state,
    scoped_refptr<base::TaskRunner> reply_runner,
    const TouchscreenDeviceCallback& callback) {
  std::vector<TouchscreenDevice> devices;
  if (display_state.mt_position_x == None ||
      display_state.mt_position_y == None)
    return;

  std::set<int> no_match_touchscreen;
  for (const DeviceInfo& device_info : device_infos) {
    if (!device_info.enabled || (device_info.use != XIFloatingSlave
        && device_info.use != XISlavePointer))
      continue;

    double max_x = -1.0;
    double max_y = -1.0;
    bool is_direct_touch = false;

    for (const ValuatorClassInfo& valuator : device_info.valuator_class_infos) {
      if (display_state.mt_position_x == valuator.label) {
        // Ignore X axis valuator with unexpected properties
        if (valuator.number == 0 && valuator.mode == Absolute &&
            valuator.min == 0.0) {
          max_x = valuator.max;
        }
      } else if (display_state.mt_position_y == valuator.label) {
        // Ignore Y axis valuator with unexpected properties
        if (valuator.number == 1 && valuator.mode == Absolute &&
            valuator.min == 0.0) {
          max_y = valuator.max;
        }
      }
    }

    if (device_info.touch_class_info.mode)
      is_direct_touch = device_info.touch_class_info.mode == XIDirectTouch;

    // Touchscreens should have absolute X and Y axes, and be direct touch
    // devices.
    if (max_x > 0.0 && max_y > 0.0 && is_direct_touch) {
      InputDeviceType type = GetInputDeviceTypeFromPath(device_info.path);
      // |max_x| and |max_y| are inclusive values, so we need to add 1 to get
      // the size.
      devices.push_back(TouchscreenDevice(
          device_info.id, type, gfx::Size(max_x + 1, max_y + 1),
          device_info.touch_class_info.num_touches));
    }
  }

  reply_runner->PostTask(FROM_HERE, base::Bind(callback, devices));
}

// Called on a worker thread to parse the device information.
void HandleHotplugEventInWorker(
    const std::vector<DeviceInfo>& devices,
    const DisplayState& display_state,
    scoped_refptr<base::TaskRunner> reply_runner,
    const UiCallbacks& callbacks) {
  HandleTouchscreenDevicesInWorker(
      devices, display_state, reply_runner, callbacks.touchscreen_callback);
  HandleKeyboardDevicesInWorker(
      devices, reply_runner, callbacks.keyboard_callback);
}

DeviceHotplugEventObserver* GetHotplugEventObserver() {
  return DeviceDataManager::GetInstance();
}

void OnKeyboardDevices(const std::vector<KeyboardDevice>& devices) {
  GetHotplugEventObserver()->OnKeyboardDevicesUpdated(devices);
}

void OnTouchscreenDevices(const std::vector<TouchscreenDevice>& devices) {
  GetHotplugEventObserver()->OnTouchscreenDevicesUpdated(devices);
}

}  // namespace

X11HotplugEventHandler::X11HotplugEventHandler()
    : atom_cache_(gfx::GetXDisplay(), kCachedAtomList) {
}

X11HotplugEventHandler::~X11HotplugEventHandler() {
}

void X11HotplugEventHandler::OnHotplugEvent() {
  const XIDeviceList& device_list =
      DeviceListCacheX11::GetInstance()->GetXI2DeviceList(gfx::GetXDisplay());
  Display* display = gfx::GetXDisplay();

  std::vector<DeviceInfo> device_infos;
  for (int i = 0; i < device_list.count; ++i) {
    const XIDeviceInfo& device = device_list[i];
    device_infos.push_back(DeviceInfo(device, GetDevicePath(display, device)));
  }

  // X11 is not thread safe, so first get all the required state.
  DisplayState display_state;
  display_state.mt_position_x = atom_cache_.GetAtom("Abs MT Position X");
  display_state.mt_position_y = atom_cache_.GetAtom("Abs MT Position Y");

  UiCallbacks callbacks;
  callbacks.keyboard_callback = base::Bind(&OnKeyboardDevices);
  callbacks.touchscreen_callback = base::Bind(&OnTouchscreenDevices);
  // TODO(pkotwicz): Compute the lists of mice and touchpads and send the new
  // lists to DeviceHotplugEventObserver.

  // Parsing the device information may block, so delegate the operation to a
  // worker thread. Once the device information is extracted the parsed devices
  // will be returned via the callbacks.
  base::WorkerPool::PostTask(FROM_HERE,
                             base::Bind(&HandleHotplugEventInWorker,
                                        device_infos,
                                        display_state,
                                        base::ThreadTaskRunnerHandle::Get(),
                                        callbacks),
                             true /* task_is_slow */);
}

}  // namespace ui

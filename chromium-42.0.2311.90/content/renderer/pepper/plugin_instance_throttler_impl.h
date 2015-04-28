// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PLUGIN_INSTANCE_THROTTLER_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PLUGIN_INSTANCE_THROTTLER_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "content/common/content_export.h"
#include "content/public/renderer/plugin_instance_throttler.h"
#include "ppapi/shared_impl/ppb_view_shared.h"

namespace blink {
class WebInputEvent;
struct WebRect;
}

namespace content {

class RenderFrameImpl;

class CONTENT_EXPORT PluginInstanceThrottlerImpl
    : public PluginInstanceThrottler {
 public:
  PluginInstanceThrottlerImpl(bool power_saver_enabled);

  ~PluginInstanceThrottlerImpl() override;

  // PluginInstanceThrottler implementation:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool IsThrottled() const override;
  bool IsHiddenForPlaceholder() const override;
  void MarkPluginEssential(PowerSaverUnthrottleMethod method) override;
  void SetHiddenForPlaceholder(bool hidden) override;
  blink::WebPlugin* GetWebPlugin() const override;

  void SetWebPlugin(blink::WebPlugin* web_plugin);

  bool needs_representative_keyframe() const {
    return state_ == THROTTLER_STATE_AWAITING_KEYFRAME;
  }

  bool power_saver_enabled() const {
    return state_ == THROTTLER_STATE_AWAITING_KEYFRAME ||
           state_ == THROTTLER_STATE_PLUGIN_THROTTLED;
  }

  // Throttler needs to be initialized with the real plugin's view bounds.
  void Initialize(RenderFrameImpl* frame,
                  const GURL& content_origin,
                  const std::string& plugin_module_name,
                  const blink::WebRect& bounds);

  // Called when the plugin flushes it's graphics context. Supplies the
  // throttler with a candidate to use as the representative keyframe.
  void OnImageFlush(const SkBitmap* bitmap);

  // Returns true if |event| was handled and shouldn't be further processed.
  bool ConsumeInputEvent(const blink::WebInputEvent& event);

 private:
  friend class PluginInstanceThrottlerImplTest;

  enum ThrottlerState {
    // Power saver is disabled, but the plugin instance is still peripheral.
    THROTTLER_STATE_POWER_SAVER_DISABLED,
    // Plugin has been found to be peripheral, Plugin Power Saver is enabled,
    // and throttler is awaiting a representative keyframe.
    THROTTLER_STATE_AWAITING_KEYFRAME,
    // A representative keyframe has been chosen and the plugin is throttled.
    THROTTLER_STATE_PLUGIN_THROTTLED,
    // Plugin instance has been marked essential.
    THROTTLER_STATE_MARKED_ESSENTIAL,
  };

  // Maximum number of frames to examine for a suitable keyframe. After that, we
  // simply suspend the plugin where it's at. Chosen arbitrarily.
  static const int kMaximumFramesToExamine;

  void EngageThrottle();

  ThrottlerState state_;

  bool is_hidden_for_placeholder_;

  blink::WebPlugin* web_plugin_;

  // Number of consecutive interesting frames we've encountered.
  int consecutive_interesting_frames_;

  // Number of frames we've examined to find a keyframe.
  int frames_examined_;

  ObserverList<Observer> observer_list_;

  base::WeakPtrFactory<PluginInstanceThrottlerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PluginInstanceThrottlerImpl);
};
}

#endif  // CONTENT_RENDERER_PEPPER_PLUGIN_INSTANCE_THROTTLER_IMPL_H_

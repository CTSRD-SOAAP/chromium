// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_BROWSER_VIEW_RENDERER_H_
#define ANDROID_WEBVIEW_BROWSER_BROWSER_VIEW_RENDERER_H_

#include "android_webview/browser/parent_compositor_draw_constraints.h"
#include "android_webview/browser/shared_renderer_state.h"
#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/browser/android/synchronous_compositor_client.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d_f.h"

class SkCanvas;
class SkPicture;

namespace android_webview {

class BrowserViewRendererClient;

// Interface for all the WebView-specific content rendering operations.
// Provides software and hardware rendering and the Capture Picture API.
class BrowserViewRenderer : public content::SynchronousCompositorClient {
 public:
  static void CalculateTileMemoryPolicy();

  BrowserViewRenderer(
      BrowserViewRendererClient* client,
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner);

  ~BrowserViewRenderer() override;

  SharedRendererState* GetAwDrawGLViewContext();
  bool RequestDrawGL(bool wait_for_completion);

  // Called before either OnDrawHardware or OnDrawSoftware to set the view
  // state of this frame. |scroll| is the view's current scroll offset.
  // |global_visible_rect| is the intersection of the view size and the window
  // in window coordinates.
  void PrepareToDraw(const gfx::Vector2d& scroll,
                     const gfx::Rect& global_visible_rect);

  // Main handlers for view drawing. A false return value indicates no new
  // frame is produced.
  bool OnDrawHardware();
  bool OnDrawSoftware(SkCanvas* canvas);

  // CapturePicture API methods.
  skia::RefPtr<SkPicture> CapturePicture(int width, int height);
  void EnableOnNewPicture(bool enabled);

  void ClearView();

  // View update notifications.
  void SetIsPaused(bool paused);
  void SetViewVisibility(bool visible);
  void SetWindowVisibility(bool visible);
  void OnSizeChanged(int width, int height);
  void OnAttachedToWindow(int width, int height);
  void OnDetachedFromWindow();

  // Sets the scale for logical<->physical pixel conversions.
  void SetDipScale(float dip_scale);

  // Set the root layer scroll offset to |new_value|.
  void ScrollTo(gfx::Vector2d new_value);

  // Android views hierarchy gluing.
  bool IsVisible() const;
  gfx::Rect GetScreenRect() const;
  bool attached_to_window() const { return attached_to_window_; }
  bool hardware_enabled() const { return hardware_enabled_; }
  gfx::Size size() const { return size_; }
  void ReleaseHardware();

  void TrimMemory(const int level, const bool visible);

  // SynchronousCompositorClient overrides.
  void DidInitializeCompositor(
      content::SynchronousCompositor* compositor) override;
  void DidDestroyCompositor(
      content::SynchronousCompositor* compositor) override;
  void SetContinuousInvalidate(bool invalidate) override;
  void DidUpdateContent() override;
  gfx::Vector2dF GetTotalRootLayerScrollOffset() override;
  void UpdateRootLayerState(const gfx::Vector2dF& total_scroll_offset_dip,
                            const gfx::Vector2dF& max_scroll_offset_dip,
                            const gfx::SizeF& scrollable_size_dip,
                            float page_scale_factor,
                            float min_page_scale_factor,
                            float max_page_scale_factor) override;
  bool IsExternalFlingActive() const override;
  void DidOverscroll(gfx::Vector2dF accumulated_overscroll,
                     gfx::Vector2dF latest_overscroll_delta,
                     gfx::Vector2dF current_fling_velocity) override;

  void UpdateParentDrawConstraints();
  void DidSkipCommitFrame();
  void DetachFunctorFromView();

 private:
  void SetTotalRootLayerScrollOffset(gfx::Vector2dF new_value_dip);
  bool CanOnDraw();
  // Checks the continuous invalidate and block invalidate state, and schedule
  // invalidates appropriately. If |force_invalidate| is true, then send a view
  // invalidate regardless of compositor expectation. If |skip_reschedule_tick|
  // is true and if there is already a pending fallback tick, don't reschedule
  // them.
  void EnsureContinuousInvalidation(bool force_invalidate,
                                    bool skip_reschedule_tick);
  bool CompositeSW(SkCanvas* canvas);
  void DidComposite();
  void DidSkipCompositeInDraw();
  scoped_refptr<base::trace_event::ConvertableToTraceFormat>
  RootLayerStateAsValue(const gfx::Vector2dF& total_scroll_offset_dip,
                        const gfx::SizeF& scrollable_size_dip);

  scoped_ptr<cc::CompositorFrame> CompositeHw();
  void ReturnUnusedResource(scoped_ptr<cc::CompositorFrame> frame);
  void ReturnResourceFromParent();

  // If we call up view invalidate and OnDraw is not called before a deadline,
  // then we keep ticking the SynchronousCompositor so it can make progress.
  // Do this in a two stage tick due to native MessageLoop favors delayed task,
  // so ensure delayed task is inserted only after the draw task returns.
  void PostFallbackTick();
  void FallbackTickFired();

  // Force invoke the compositor to run produce a 1x1 software frame that is
  // immediately discarded. This is a hack to force invoke parts of the
  // compositor that are not directly exposed here.
  void ForceFakeCompositeSW();

  gfx::Vector2d max_scroll_offset() const;

  size_t CalculateDesiredMemoryPolicy();
  // For debug tracing or logging. Return the string representation of this
  // view renderer's state.
  std::string ToString() const;

  BrowserViewRendererClient* client_;
  SharedRendererState shared_renderer_state_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  content::SynchronousCompositor* compositor_;

  bool is_paused_;
  bool view_visible_;
  bool window_visible_;  // Only applicable if |attached_to_window_| is true.
  bool attached_to_window_;
  bool hardware_enabled_;
  float dip_scale_;
  float page_scale_factor_;
  bool on_new_picture_enable_;
  bool clear_view_;

  gfx::Vector2d last_on_draw_scroll_offset_;
  gfx::Rect last_on_draw_global_visible_rect_;

  // The draw constraints from the parent compositor. These are only used for
  // tiling priority.
  ParentCompositorDrawConstraints parent_draw_constraints_;

  // When true, we should continuously invalidate and keep drawing, for example
  // to drive animation. This value is set by the compositor and should always
  // reflect the expectation of the compositor and not be reused for other
  // states.
  bool compositor_needs_continuous_invalidate_;

  bool invalidate_after_composite_;

  // Used to block additional invalidates while one is already pending.
  bool block_invalidates_;

  base::CancelableClosure post_fallback_tick_;
  base::CancelableClosure fallback_tick_fired_;
  bool fallback_tick_pending_;

  gfx::Size size_;

  // Current scroll offset in CSS pixels.
  gfx::Vector2dF scroll_offset_dip_;

  // Max scroll offset in CSS pixels.
  gfx::Vector2dF max_scroll_offset_dip_;

  // Used to prevent rounding errors from accumulating enough to generate
  // visible skew (especially noticeable when scrolling up and down in the same
  // spot over a period of time).
  gfx::Vector2dF overscroll_rounding_error_;

  DISALLOW_COPY_AND_ASSIGN(BrowserViewRenderer);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_BROWSER_VIEW_RENDERER_H_

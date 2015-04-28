// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_PAGE_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_PAGE_HANDLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/output/compositor_frame_metadata.h"
#include "content/browser/devtools/protocol/devtools_protocol_handler.h"
#include "content/public/browser/readback_types.h"

class SkBitmap;

namespace content {

class RenderViewHostImpl;

namespace devtools {
namespace page {

class ColorPicker;
class FrameRecorder;

class PageHandler {
 public:
  typedef DevToolsProtocolClient::Response Response;

  PageHandler();
  virtual ~PageHandler();

  void SetRenderViewHost(RenderViewHostImpl* host);
  void SetClient(scoped_ptr<Client> client);
  void Detached();
  void OnSwapCompositorFrame(const cc::CompositorFrameMetadata& frame_metadata);
  void OnVisibilityChanged(bool visible);
  void DidAttachInterstitialPage();
  void DidDetachInterstitialPage();

  Response Enable();
  Response Disable();

  Response Reload(const bool* ignoreCache,
                  const std::string* script_to_evaluate_on_load,
                  const std::string* script_preprocessor = NULL);

  Response Navigate(const std::string& url, FrameId* frame_id);

  using NavigationEntries = std::vector<scoped_refptr<NavigationEntry>>;
  Response GetNavigationHistory(int* current_index,
                                NavigationEntries* entries);

  Response NavigateToHistoryEntry(int entry_id);

  Response SetGeolocationOverride(double* latitude,
                                  double* longitude,
                                  double* accuracy);

  Response ClearGeolocationOverride();

  Response SetTouchEmulationEnabled(bool enabled,
                                    const std::string* configuration);

  Response CaptureScreenshot(DevToolsCommandId command_id);

  Response CanScreencast(bool* result);
  Response CanEmulate(bool* result);

  Response StartScreencast(const std::string* format,
                           const int* quality,
                           const int* max_width,
                           const int* max_height);
  Response StopScreencast();
  Response ScreencastFrameAck(int frame_number);

  Response StartRecordingFrames(int max_frame_count);
  Response StopRecordingFrames(DevToolsCommandId command_id);
  Response CancelRecordingFrames();

  Response HandleJavaScriptDialog(bool accept, const std::string* prompt_text);

  Response QueryUsageAndQuota(DevToolsCommandId command_id,
                              const std::string& security_origin);

  Response SetColorPickerEnabled(bool enabled);

 private:
  void UpdateTouchEventEmulationState();

  void NotifyScreencastVisibility(bool visible);
  void InnerSwapCompositorFrame();
  void ScreencastFrameCaptured(const cc::CompositorFrameMetadata& metadata,
                               const SkBitmap& bitmap,
                               ReadbackResponse response);
  void ScreencastFrameEncoded(const cc::CompositorFrameMetadata& metadata,
                              const base::Time& timestamp,
                              const std::string& data);

  void ScreenshotCaptured(
      DevToolsCommandId command_id,
      const unsigned char* png_data,
      size_t png_size);

  void OnColorPicked(int r, int g, int b, int a);
  void OnFramesRecorded(
      DevToolsCommandId command_id,
      scoped_refptr<StopRecordingFramesResponse> response_data);

  void QueryUsageAndQuotaCompleted(
      DevToolsCommandId command_id,
      scoped_refptr<QueryUsageAndQuotaResponse> response);

  bool enabled_;
  bool touch_emulation_enabled_;
  std::string touch_emulation_configuration_;

  bool screencast_enabled_;
  std::string screencast_format_;
  int screencast_quality_;
  int screencast_max_width_;
  int screencast_max_height_;
  int capture_retry_count_;
  bool has_compositor_frame_metadata_;
  cc::CompositorFrameMetadata next_compositor_frame_metadata_;
  cc::CompositorFrameMetadata last_compositor_frame_metadata_;
  int screencast_frame_sent_;
  int screencast_frame_acked_;
  bool processing_screencast_frame_;

  scoped_ptr<ColorPicker> color_picker_;
  scoped_ptr<FrameRecorder> frame_recorder_;

  RenderViewHostImpl* host_;
  scoped_ptr<Client> client_;
  base::WeakPtrFactory<PageHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PageHandler);
};

}  // namespace page
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_PAGE_HANDLER_H_

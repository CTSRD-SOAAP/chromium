// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_TEST_MOCK_DRI_WRAPPER_H_
#define UI_OZONE_PLATFORM_DRI_TEST_MOCK_DRI_WRAPPER_H_

#include <queue>
#include <vector>

#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"

namespace ui {

class CrtcController;

// The real DriWrapper makes actual DRM calls which we can't use in unit tests.
class MockDriWrapper : public ui::DriWrapper {
 public:
  MockDriWrapper();
  MockDriWrapper(bool use_sync_flips,
                 std::vector<uint32_t> crtcs,
                 size_t planes_per_crtc);

  int get_get_crtc_call_count() const { return get_crtc_call_count_; }
  int get_set_crtc_call_count() const { return set_crtc_call_count_; }
  int get_restore_crtc_call_count() const { return restore_crtc_call_count_; }
  int get_add_framebuffer_call_count() const {
    return add_framebuffer_call_count_;
  }
  int get_remove_framebuffer_call_count() const {
    return remove_framebuffer_call_count_;
  }
  int get_page_flip_call_count() const { return page_flip_call_count_; }
  int get_overlay_flip_call_count() const { return overlay_flip_call_count_; }
  int get_overlay_clear_call_count() const { return overlay_clear_call_count_; }
  void set_set_crtc_expectation(bool state) { set_crtc_expectation_ = state; }
  void set_page_flip_expectation(bool state) { page_flip_expectation_ = state; }
  void set_add_framebuffer_expectation(bool state) {
    add_framebuffer_expectation_ = state;
  }
  void set_create_dumb_buffer_expectation(bool state) {
    create_dumb_buffer_expectation_ = state;
  }

  uint32_t current_framebuffer() const { return current_framebuffer_; }

  const std::vector<skia::RefPtr<SkSurface> > buffers() const {
    return buffers_;
  }

  void RunCallbacks();

  // DriWrapper:
  ScopedDrmCrtcPtr GetCrtc(uint32_t crtc_id) override;
  bool SetCrtc(uint32_t crtc_id,
               uint32_t framebuffer,
               std::vector<uint32_t> connectors,
               drmModeModeInfo* mode) override;
  bool SetCrtc(drmModeCrtc* crtc, std::vector<uint32_t> connectors) override;
  bool DisableCrtc(uint32_t crtc_id) override;
  ScopedDrmConnectorPtr GetConnector(uint32_t connector_id) override;
  bool AddFramebuffer(uint32_t width,
                      uint32_t height,
                      uint8_t depth,
                      uint8_t bpp,
                      uint32_t stride,
                      uint32_t handle,
                      uint32_t* framebuffer) override;
  bool RemoveFramebuffer(uint32_t framebuffer) override;
  ScopedDrmFramebufferPtr GetFramebuffer(uint32_t framebuffer) override;
  bool PageFlip(uint32_t crtc_id,
                uint32_t framebuffer,
                bool is_sync,
                const PageFlipCallback& callback) override;
  bool PageFlipOverlay(uint32_t crtc_id,
                       uint32_t framebuffer,
                       const gfx::Rect& location,
                       const gfx::Rect& source,
                       int overlay_plane) override;
  ScopedDrmPropertyPtr GetProperty(drmModeConnector* connector,
                                   const char* name) override;
  bool SetProperty(uint32_t connector_id,
                   uint32_t property_id,
                   uint64_t value) override;
  bool GetCapability(uint64_t capability, uint64_t* value) override;
  ScopedDrmPropertyBlobPtr GetPropertyBlob(drmModeConnector* connector,
                                           const char* name) override;
  bool SetCursor(uint32_t crtc_id,
                 uint32_t handle,
                 const gfx::Size& size) override;
  bool MoveCursor(uint32_t crtc_id, const gfx::Point& point) override;
  bool CreateDumbBuffer(const SkImageInfo& info,
                        uint32_t* handle,
                        uint32_t* stride,
                        void** pixels) override;
  void DestroyDumbBuffer(const SkImageInfo& info,
                         uint32_t handle,
                         uint32_t stride,
                         void* pixels) override;

 private:
  ~MockDriWrapper() override;

  int get_crtc_call_count_;
  int set_crtc_call_count_;
  int restore_crtc_call_count_;
  int add_framebuffer_call_count_;
  int remove_framebuffer_call_count_;
  int page_flip_call_count_;
  int overlay_flip_call_count_;
  int overlay_clear_call_count_;

  bool set_crtc_expectation_;
  bool add_framebuffer_expectation_;
  bool page_flip_expectation_;
  bool create_dumb_buffer_expectation_;

  bool use_sync_flips_;

  uint32_t current_framebuffer_;

  std::vector<skia::RefPtr<SkSurface> > buffers_;

  std::queue<PageFlipCallback> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(MockDriWrapper);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_TEST_MOCK_DRI_WRAPPER_H_

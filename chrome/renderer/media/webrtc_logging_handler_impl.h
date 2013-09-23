// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_WEBRTC_LOGGING_HANDLER_IMPL_H_
#define CHROME_RENDERER_MEDIA_WEBRTC_LOGGING_HANDLER_IMPL_H_

#include <string>

#include "base/shared_memory.h"
#include "content/public/renderer/webrtc_log_message_delegate.h"
#include "ipc/ipc_channel_proxy.h"

namespace base {
class MessageLoopProxy;
}

class PartialCircularBuffer;
class WebRtcLoggingMessageFilter;

// WebRtcLoggingHandlerImpl handles WebRTC logging. There is one object per
// render process, owned by WebRtcLoggingMessageFilter. It communicates with
// WebRtcLoggingHandlerHost and receives logging messages from libjingle and
// writes them to a shared memory buffer.
class WebRtcLoggingHandlerImpl
    : public content::WebRtcLogMessageDelegate,
      public base::NonThreadSafe {
 public:
  WebRtcLoggingHandlerImpl(
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop,
      WebRtcLoggingMessageFilter* message_filter);

  virtual ~WebRtcLoggingHandlerImpl();

  // content::WebRtcLogMessageDelegate implementation.
  virtual void InitLogging(const std::string& app_session_id,
                           const std::string& app_url) OVERRIDE;
  virtual void LogMessage(const std::string& message) OVERRIDE;

  void OnFilterRemoved();

  void OnLogOpened(base::SharedMemoryHandle handle, uint32 length);
  void OnOpenLogFailed();

 private:
  scoped_refptr<base::MessageLoopProxy> io_message_loop_;
  scoped_ptr<base::SharedMemory> shared_memory_;
  scoped_ptr<PartialCircularBuffer> circular_buffer_;

  WebRtcLoggingMessageFilter* message_filter_;
  bool log_initialized_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcLoggingHandlerImpl);
};

#endif  // CHROME_RENDERER_MEDIA_WEBRTC_LOGGING_HANDLER_IMPL_H_

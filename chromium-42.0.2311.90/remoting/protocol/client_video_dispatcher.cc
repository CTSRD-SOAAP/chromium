// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/client_video_dispatcher.h"

#include "base/bind.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/constants.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/video_stub.h"

namespace remoting {
namespace protocol {

ClientVideoDispatcher::ClientVideoDispatcher(VideoStub* video_stub)
    : ChannelDispatcherBase(kVideoChannelName),
      parser_(base::Bind(&VideoStub::ProcessVideoPacket,
                         base::Unretained(video_stub)),
              reader()) {
}

ClientVideoDispatcher::~ClientVideoDispatcher() {
}

}  // namespace protocol
}  // namespace remoting

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_test_util_spdy2.h"

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/strings/string_number_conversions.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_server_properties_impl.h"
#include "net/spdy/buffered_spdy_framer.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_session.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test_spdy2 {

SpdyFrame* ConstructSpdyPush(const char* const extra_headers[],
                             int extra_header_count,
                             int stream_id,
                             int associated_stream_id,
                             const char* url) {
  SpdyTestUtil util(kProtoSPDY2);
  return util.ConstructSpdyPush(extra_headers, extra_header_count,
                                stream_id, associated_stream_id, url);
}
SpdyFrame* ConstructSpdyPush(const char* const extra_headers[],
                             int extra_header_count,
                             int stream_id,
                             int associated_stream_id,
                             const char* url,
                             const char* status,
                             const char* location) {
  SpdyTestUtil util(kProtoSPDY2);
  return util.ConstructSpdyPush(extra_headers, extra_header_count,
                                stream_id, associated_stream_id, url,
                                status, location);
}

SpdyFrame* ConstructSpdyPushHeaders(int stream_id,
                                    const char* const extra_headers[],
                                    int extra_header_count) {
  SpdyTestUtil util(kProtoSPDY2);
  return util.ConstructSpdyPushHeaders(stream_id, extra_headers,
                                       extra_header_count);
}

SpdyFrame* ConstructSpdySynReplyError(const char* const status,
                                      const char* const* const extra_headers,
                                      int extra_header_count,
                                      int stream_id) {
  SpdyTestUtil util(kProtoSPDY2);
  return util.ConstructSpdySynReplyError(status, extra_headers,
                                         extra_header_count, stream_id);
}

SpdyFrame* ConstructSpdyGetSynReplyRedirect(int stream_id) {
  SpdyTestUtil util(kProtoSPDY2);
  return util.ConstructSpdyGetSynReplyRedirect(stream_id);
}

SpdyFrame* ConstructSpdySynReplyError(int stream_id) {
  SpdyTestUtil util(kProtoSPDY2);
  return util.ConstructSpdySynReplyError(stream_id);
}

SpdyFrame* ConstructSpdyGetSynReply(const char* const extra_headers[],
                                    int extra_header_count,
                                    int stream_id) {
  SpdyTestUtil util(kProtoSPDY2);
  return util.ConstructSpdyGetSynReply(extra_headers, extra_header_count,
                                       stream_id);
}

SpdyFrame* ConstructSpdyPost(const char* url,
                             SpdyStreamId stream_id,
                             int64 content_length,
                             RequestPriority priority,
                             const char* const extra_headers[],
                             int extra_header_count) {
  SpdyTestUtil util(kProtoSPDY2);
  return util.ConstructSpdyPost(url, stream_id, content_length,
                                priority, extra_headers, extra_header_count);
}

SpdyFrame* ConstructChunkedSpdyPost(const char* const extra_headers[],
                                    int extra_header_count) {
  SpdyTestUtil util(kProtoSPDY2);
  return util.ConstructChunkedSpdyPost(extra_headers, extra_header_count);
}

SpdyFrame* ConstructSpdyPostSynReply(const char* const extra_headers[],
                                     int extra_header_count) {
  SpdyTestUtil util(kProtoSPDY2);
  return util.ConstructSpdyPostSynReply(extra_headers, extra_header_count);
}

SpdyFrame* ConstructSpdyBodyFrame(int stream_id, bool fin) {
  SpdyTestUtil util(kProtoSPDY2);
  return util.ConstructSpdyBodyFrame(stream_id, fin);
}

SpdyFrame* ConstructSpdyBodyFrame(int stream_id, const char* data,
                                  uint32 len, bool fin) {
  SpdyTestUtil util(kProtoSPDY2);
  return util.ConstructSpdyBodyFrame(stream_id, data, len, fin);
}

SpdyFrame* ConstructWrappedSpdyFrame(const scoped_ptr<SpdyFrame>& frame,
                                     int stream_id) {
  SpdyTestUtil util(kProtoSPDY2);
  return util.ConstructWrappedSpdyFrame(frame, stream_id);
}

const SpdyHeaderInfo MakeSpdyHeader(SpdyFrameType type) {
  SpdyTestUtil util(kProtoSPDY2);
  return util.MakeSpdyHeader(type);
}

}  // namespace test_spdy2
}  // namespace net

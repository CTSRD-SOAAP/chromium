// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/incident_handler_util.h"

#include <string>

#include "base/hash.h"
#include "base/logging.h"
//XXX(rene) needs shim headers?
#if defined(USE_SYSTEM_PROTOBUF)
#include <google/protobuf/message_lite.h>
#else
#include "third_party/protobuf/src/google/protobuf/message_lite.h"
#endif

namespace safe_browsing {

// Computes a simple hash digest over the serialized form of |message|.
// |message| must be in a canonical form.
uint32_t HashMessage(const google::protobuf::MessageLite& message) {
  std::string message_string;
  if (!message.SerializeToString(&message_string)) {
    NOTREACHED();
    return 0;
  }
  return base::Hash(message_string);
}

}  // namespace safe_browsing

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/util/get_session_name_linux.h"

#include <limits.h>  // for HOST_NAME_MAX
#include <unistd.h>  // for gethostname()

#include "base/linux_util.h"

namespace syncer {
namespace internal {

std::string GetHostname() {
  int len = sysconf(_SC_HOST_NAME_MAX);
  char hostname[len];
  if (gethostname(hostname, len) == 0)  // Success.
    return hostname;
  return base::GetLinuxDistro();
}

}  // namespace internal
}  // namespace syncer


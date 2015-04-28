/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/include/minsfi_syscalls.h"

void _exit(int status) {
  __minsfi_syscall_exit(status);
}

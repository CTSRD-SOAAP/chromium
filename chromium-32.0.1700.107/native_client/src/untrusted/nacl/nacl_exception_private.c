/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/include/nacl/nacl_exception.h"

#include <errno.h>

#include "native_client/src/trusted/service_runtime/include/sys/nacl_syscalls.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

int nacl_exception_set_handler(nacl_exception_handler_t handler) {
  return -NACL_SYSCALL(exception_handler)(handler, NULL);
}

int nacl_exception_set_stack(void *stack, size_t size) {
  return -NACL_SYSCALL(exception_stack)(stack, size);
}

int nacl_exception_clear_flag() {
  return -NACL_SYSCALL(exception_clear_flag)();
}

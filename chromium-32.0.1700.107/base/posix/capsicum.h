// Copyright (c) 2014 Jonathan Anderson
// All rights reserved.
//
// NOTE: upstreaming this work will probably require a contribution agreement
//
// This software was developed by SRI International and the University of
// Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
// ("CTSRD"), as part of the DARPA CRASH research programme.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.

#ifndef BASE_POSIX_CAPSICUM_H_
#define BASE_POSIX_CAPSICUM_H_

#include "base/base_export.h"

#if defined(CAPSICUM_SUPPORT)

//
// Base OS support for Capsicum features.
//
class BASE_EXPORT Capsicum {
 public:
  // Capability rights that are actually used by Chromium.
  struct Rights {
    bool stat = false;            // fstat(2)
    bool tell = false;            // ftell(2)
    bool read = false;            // read(2), readable shared memory (see mmap)
    bool write = false;           // write(2), writable shared memory (see mmap)
    bool lock = false;            // fcntl(F_SETLK)
    bool mmap = false;            // mmap(2)
    bool tty = false;             // various tty-related ioctl(2) values
    bool poll = false;            // poll(2), select(2) and kevent(2)
    bool kqueue = false;          // modify a kqueue or send events
    bool directoryLookup = false; // Allow the *at(2) family of system calls.
  };

  // The current process is in Capsicum's least-privileged capability mode.
  static bool InCapabilityMode();

  // Enter Capsicum capability mode.
  static bool EnterCapabilityMode();

  // Restrict a file descriptor with Capsicum rights.
  static bool RestrictFile(int fd, const Rights&);
};

#endif

#endif  // BASE_POSIX_CAPSICUM_H_

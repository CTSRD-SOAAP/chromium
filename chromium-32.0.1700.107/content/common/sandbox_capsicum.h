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

#ifndef CONTENT_COMMON_SANDBOX_CAPSICUM_H_
#define CONTENT_COMMON_SANDBOX_CAPSICUM_H_

#include "base/posix/capsicum.h"

namespace content {

// A singleton class to represent and change our sandboxing state on the
// BSD operating systems.
class CapsicumSandbox {
 public:
  static CapsicumSandbox* Create();

  // Restrict the current process.
  bool InitializeSandbox();

  // Have we been sandboxed?
  bool Sandboxed() const;

 private:
  CapsicumSandbox(bool haveCapabilities, bool haveCapMode);

  // Acquire whatever rights are required before entering the sandbox.
  bool PreinitializeSandbox();

  // Restrict any file descriptors we hold.
  bool RestrictFileDescriptors();

  // This platform has Capsicum capability support.
  const bool haveCapabilities;

  // This platform has least-privileged capability mode support.
  const bool haveCapabilityMode;
};

}  // namespace content

#endif  // CONTENT_COMMON_SANDBOX_CAPSICUM_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_main_platform_delegate.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#if defined(CAPSICUM_SUPPORT)
#include "content/common/sandbox_capsicum.h"
#endif
#if defined(OS_LINUX)
#include "content/common/sandbox_linux.h"
#endif
#include "content/public/common/content_switches.h"
#include "content/public/common/sandbox_init.h"

#ifdef ENABLE_VTUNE_JIT_INTERFACE
#include "v8/src/third_party/vtune/v8-vtune.h"
#endif

namespace content {

RendererMainPlatformDelegate::RendererMainPlatformDelegate(
    const MainFunctionParams& parameters)
    : parameters_(parameters) {
}

RendererMainPlatformDelegate::~RendererMainPlatformDelegate() {
}

void RendererMainPlatformDelegate::PlatformInitialize() {
#ifdef ENABLE_VTUNE_JIT_INTERFACE
  const CommandLine& command_line = parameters_.command_line;
  if (command_line.HasSwitch(switches::kEnableVtune))
    vTune::InitializeVtuneForV8();
#endif
}

void RendererMainPlatformDelegate::PlatformUninitialize() {
}

bool RendererMainPlatformDelegate::InitSandboxTests(bool no_sandbox) {
  // The sandbox is started in the zygote process: zygote_main_linux.cc
  // http://code.google.com/p/chromium/wiki/LinuxSUIDSandbox
  return true;
}

bool RendererMainPlatformDelegate::EnableSandbox() {
#if defined(CAPSICUM_SUPPORT)
  capsicum_sandbox_.reset(CapsicumSandbox::Create());
  CHECK(capsicum_sandbox_);

  if (not capsicum_sandbox_->InitializeSandbox())
    return false;
#elif defined(OS_LINUX)
  // The setuid sandbox is started in the zygote process: zygote_main_linux.cc
  // http://code.google.com/p/chromium/wiki/LinuxSUIDSandbox
  //
  // Anything else is started in InitializeSandbox().
  LinuxSandbox::InitializeSandbox();
#endif
  return true;
}

void RendererMainPlatformDelegate::RunSandboxTests(bool no_sandbox) {
  // The LinuxSandbox class requires going through initialization before
  // GetStatus() and others can be used.  When we are not launched through the
  // Zygote, this initialization will only happen in the renderer process if
  // EnableSandbox() above is called, which it won't necesserily be.
  // This only happens with flags such as --renderer-cmd-prefix which are
  // for debugging.
  if (no_sandbox)
    return;

#if defined(OS_BSD)
  // In a sandbox, we should not have access to any global namespaces.
  //
  // This includes:
  //  * filesystems
  //  * the network
  //  * PIDs
  CHECK(!base::PathExists(base::FilePath("/bin/true")));
  CHECK_EQ(open("/bin/true", 0), -1);
  CHECK_EQ(errno, ECAPMODE);
#else
  // about:sandbox uses a value returned from LinuxSandbox::GetStatus() before
  // any renderer has been started.
  // Here, we test that the status of SeccompBpf in the renderer is consistent
  // with what LinuxSandbox::GetStatus() said we would do.
  class LinuxSandbox* linux_sandbox = LinuxSandbox::GetInstance();
  if (linux_sandbox->GetStatus() & kSandboxLinuxSeccompBpf) {
    CHECK(linux_sandbox->seccomp_bpf_started());
  }

  // Under the setuid sandbox, we should not be able to open any file via the
  // filesystem.
  if (linux_sandbox->GetStatus() & kSandboxLinuxSUID) {
    CHECK(!base::PathExists(base::FilePath("/proc/cpuinfo")));
  }

#if defined(__x86_64__)
  // Limit this test to architectures where seccomp BPF is active in renderers.
  if (linux_sandbox->seccomp_bpf_started()) {
    errno = 0;
    // This should normally return EBADF since the first argument is bogus,
    // but we know that under the seccomp-bpf sandbox, this should return EPERM.
    CHECK_EQ(fchmod(-1, 07777), -1);
    CHECK_EQ(errno, EPERM);
  }
#endif  // __x86_64__
#endif
}

}  // namespace content

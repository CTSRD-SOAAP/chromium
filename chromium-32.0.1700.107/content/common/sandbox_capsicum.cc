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

#include <sys/types.h>

// TODO(JA): drop __{BEGIN,END}_DECLS once <sys/capability.h> has them
__BEGIN_DECLS
#include <sys/capability.h>
__END_DECLS

#include <sys/stat.h>
#include <sys/sysctl.h>

#include <fcntl.h>
#include <soaap.h>
#include <termios.h>

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/posix/capsicum.h"
#include "base/posix/global_descriptors.h"
#include "content/common/child_process_sandbox_support_impl_linux.h"
#include "content/common/font_config_ipc_linux.h"
#include "content/common/sandbox_capsicum.h"
#include "crypto/nss_util.h"
#include "ipc/ipc_descriptors.h"
#include "third_party/skia/include/ports/SkFontConfigInterface.h"

namespace content {

const char kFeatureCapabilities[] = "kern.features.security_capabilities";
const char kFeatureCapMode[] = "kern.features.security_capability_mode";

CapsicumSandbox* CapsicumSandbox::Create() {
  int haveCaps = 0, haveCapMode = 0;
  size_t size = sizeof(haveCaps);

  int ret = sysctlbyname(kFeatureCapabilities, &haveCaps, &size, NULL, 0);
  CHECK_EQ(ret, 0);

  ret = sysctlbyname(kFeatureCapMode, &haveCapMode, &size, NULL, 0);
  CHECK_EQ(ret, 0);

  return new CapsicumSandbox(haveCaps, haveCapMode);
}

CapsicumSandbox::CapsicumSandbox(bool haveCapabilities, bool haveCapMode)
  : haveCapabilities(haveCapabilities), haveCapabilityMode(haveCapMode)
{
  if (not haveCapabilities)
    LOG(WARNING) << "capabilities not available on this platform";

  if (not haveCapabilityMode)
    LOG(WARNING) << "capability mode not available on this platform";
}


bool CapsicumSandbox::InitializeSandbox() {
  LOG(INFO) << "initializing sandbox";

  bool success = PreinitializeSandbox()
    and haveCapabilities and RestrictFileDescriptors()
    and haveCapabilityMode and Capsicum::EnterCapabilityMode();

  if (success)
    LOG(INFO) << "initialized Capsicum sandbox";
  else
    LOG(ERROR) << "failed to initialize Capsicum sandbox";

  return success;
}


bool CapsicumSandbox::PreinitializeSandbox() {
  //
  // Do some pre-initialisation of things that we won't be able to access
  // once we've entered capability mode.  This is the same approach that
  // the Linux zygote takes.
  //

  // RandUint64() on POSIX uses /dev/urandom.
  base::RandUint64();

#if defined(USE_NSS)
  // NSS will dlopen() libraries on first use.
  crypto::LoadNSSLibraries();
#endif

  // Capsicum lets us use font directories natively (via openat() and friends),
  // but we must load these directories before entering the sandbox.
  SkFontConfigInterface::SetGlobal(
    SkFontConfigInterface::GetSingletonDirectInterface());

  return true;
}


// Restrict a file descriptor (or die trying).
static bool Restrict(int fd, const char *name, cap_rights_t& rights) {
  if (cap_rights_limit(fd, &rights) != 0) {
    PLOG(ERROR) << "unable to limit " << name << " descriptor";
    return false;
  }
  return true;
}

bool CapsicumSandbox::RestrictFileDescriptors() {
  cap_rights_t readOnly, writeOnly, ipc;
  cap_rights_init(&readOnly, CAP_READ);
  cap_rights_init(&writeOnly, CAP_WRITE);
  cap_rights_init(&ipc, CAP_READ, CAP_WRITE, CAP_EVENT);

  //
  // Restrict stdin to CAP_READ and stdout and stderr to CAP_WRITE.
  //
  __soaap_limit_fd_syscalls(STDIN_FILENO, read);
  __soaap_limit_fd_syscalls(STDOUT_FILENO, read, write);
  __soaap_limit_fd_syscalls(STDERR_FILENO, read, write);

  if (not Restrict(STDIN_FILENO, "stdin", readOnly)
      or not Restrict(STDOUT_FILENO, "stdout", writeOnly)
      or not Restrict(STDERR_FILENO, "stderr", writeOnly))
    return false;

  //
  // Limit global file descriptors mapped by base::GlobalDescriptors.
  // Note that GlobalDescriptors::Get() will cause a fatal error if the
  // descriptor is not mapped, so we don't need to check descriptor validity.
  //
  base::GlobalDescriptors *globals = base::GlobalDescriptors::GetInstance();

  __soaap_limit_fd_syscalls(globals->Get(kPrimaryIPCChannel),
                            read, write, poll, select);
  if (not Restrict(globals->Get(kPrimaryIPCChannel), "primary IPC", ipc))
    return false;

#if defined(OS_LINUX) || defined(OS_OPENBSD)
  // TODO(JA): why isn't kCrashDumpSignal set on FreeBSD.
  __soaap_limit_fd_syscalls(globals->Get(kCrashDumpSignal),
                            read, write, poll, select);
  if(not Restrict(globals->Get(kCrashDumpSignal), "crash dump signal", ipc))
    return false;
#endif

  /*
  __soaap_limit_fd_syscalls(globals->Get(kSandboxIPCChannel),
                            read, write, poll, select);
  if (not Restrict(globals->Get(kSandboxIPCChannel), "sandbox IPC", ipc))
    return false;
  */

  return true;
}

}  // namespace content

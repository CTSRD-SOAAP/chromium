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
#include <termios.h>

#include "base/posix/capsicum.h"
#include "ipc/ipc_descriptors.h"

const char kFeatureCapabilities[] = "kern.features.security_capabilities";
const char kFeatureCapMode[] = "kern.features.security_capability_mode";


bool Capsicum::RestrictFile(int fd, const Rights& need) {
  cap_rights_t fd_rights;
  uint32_t fcntl_rights = CAP_FCNTL_ALL;

  cap_rights_init(&fd_rights, CAP_FCNTL);

  if (need.stat)
    cap_rights_set(&fd_rights, CAP_FSTAT);

  if (need.tell)
    cap_rights_set(&fd_rights, CAP_SEEK_TELL);

  if (need.read) {
    cap_rights_set(&fd_rights, CAP_READ);

    if (need.mmap)
      cap_rights_set(&fd_rights, CAP_MMAP_RX);
  }

  if (need.write) {
    cap_rights_set(&fd_rights, CAP_WRITE, CAP_FSYNC, CAP_FTRUNCATE);

    if (need.mmap)
      cap_rights_set(&fd_rights, CAP_MMAP_W);
  }

  if (need.lock)
    cap_rights_set(&fd_rights, CAP_FLOCK);

  if (need.tty) {
    static const unsigned long tty_ioctls[] = { TIOCGETA, TIOCGWINSZ };
    static const size_t len = sizeof(tty_ioctls) / sizeof(tty_ioctls[0]);

    cap_rights_set(&fd_rights, CAP_IOCTL);

    if (cap_ioctls_limit(fd, tty_ioctls, len) != 0)
      return false;
  }

  if (need.poll)
    cap_rights_set(&fd_rights, CAP_EVENT);

  if (need.kqueue)
    cap_rights_set(&fd_rights, CAP_KQUEUE);

  if (need.directoryLookup)
    cap_rights_set(&fd_rights, CAP_LOOKUP);

  return (cap_fcntls_limit(fd, fcntl_rights) == 0)
    and (cap_rights_limit(fd, &fd_rights) == 0);
}


bool Capsicum::InCapabilityMode() {
  return cap_sandboxed();
}


bool Capsicum::EnterCapabilityMode() {
  return (cap_enter() == 0);
}

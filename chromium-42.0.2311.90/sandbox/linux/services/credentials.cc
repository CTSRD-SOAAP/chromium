// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/credentials.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/capability.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "base/template_util.h"
#include "base/third_party/valgrind/valgrind.h"
#include "sandbox/linux/services/namespace_utils.h"
#include "sandbox/linux/services/syscall_wrappers.h"

namespace sandbox {

namespace {

bool IsRunningOnValgrind() { return RUNNING_ON_VALGRIND; }

struct CapFreeDeleter {
  inline void operator()(cap_t cap) const {
    int ret = cap_free(cap);
    CHECK_EQ(0, ret);
  }
};

// Wrapper to manage libcap2's cap_t type.
typedef scoped_ptr<typeof(*((cap_t)0)), CapFreeDeleter> ScopedCap;

struct CapTextFreeDeleter {
  inline void operator()(char* cap_text) const {
    int ret = cap_free(cap_text);
    CHECK_EQ(0, ret);
  }
};

// Wrapper to manage the result from libcap2's cap_from_text().
typedef scoped_ptr<char, CapTextFreeDeleter> ScopedCapText;

// Checks that the set of RES-uids and the set of RES-gids have
// one element each and return that element in |resuid| and |resgid|
// respectively. It's ok to pass NULL as one or both of the ids.
bool GetRESIds(uid_t* resuid, gid_t* resgid) {
  uid_t ruid, euid, suid;
  gid_t rgid, egid, sgid;
  PCHECK(getresuid(&ruid, &euid, &suid) == 0);
  PCHECK(getresgid(&rgid, &egid, &sgid) == 0);
  const bool uids_are_equal = (ruid == euid) && (ruid == suid);
  const bool gids_are_equal = (rgid == egid) && (rgid == sgid);
  if (!uids_are_equal || !gids_are_equal) return false;
  if (resuid) *resuid = euid;
  if (resgid) *resgid = egid;
  return true;
}

const int kExitSuccess = 0;

int ChrootToSelfFdinfo(void*) {
  RAW_CHECK(chroot("/proc/self/fdinfo/") == 0);

  // CWD is essentially an implicit file descriptor, so be careful to not
  // leave it behind.
  RAW_CHECK(chdir("/") == 0);
  _exit(kExitSuccess);
}

// chroot() to an empty dir that is "safe". To be safe, it must not contain
// any subdirectory (chroot-ing there would allow a chroot escape) and it must
// be impossible to create an empty directory there.
// We achieve this by doing the following:
// 1. We create a new process sharing file system information.
// 2. In the child, we chroot to /proc/self/fdinfo/
// This is already "safe", since fdinfo/ does not contain another directory and
// one cannot create another directory there.
// 3. The process dies
// After (3) happens, the directory is not available anymore in /proc.
bool ChrootToSafeEmptyDir() {
  // We need to chroot to a fdinfo that is unique to a process and have that
  // process die.
  // 1. We don't want to simply fork() because duplicating the page tables is
  // slow with a big address space.
  // 2. We do not use a regular thread (that would unshare CLONE_FILES) because
  // when we are in a PID namespace, we cannot easily get a handle to the
  // /proc/tid directory for the thread (since /proc may not be aware of the
  // PID namespace). With a process, we can just use /proc/self.
  pid_t pid = -1;
  char stack_buf[PTHREAD_STACK_MIN];
#if defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARM_FAMILY) || \
    defined(ARCH_CPU_MIPS64_FAMILY) || defined(ARCH_CPU_MIPS_FAMILY)
  // The stack grows downward.
  void* stack = stack_buf + sizeof(stack_buf);
#else
#error "Unsupported architecture"
#endif
  pid = clone(ChrootToSelfFdinfo, stack,
              CLONE_VM | CLONE_VFORK | CLONE_FS | SIGCHLD, nullptr, nullptr,
              nullptr, nullptr);
  PCHECK(pid != -1);

  int status = -1;
  PCHECK(HANDLE_EINTR(waitpid(pid, &status, 0)) == pid);

  return WIFEXITED(status) && WEXITSTATUS(status) == kExitSuccess;
}

// CHECK() that an attempt to move to a new user namespace raised an expected
// errno.
void CheckCloneNewUserErrno(int error) {
  // EPERM can happen if already in a chroot. EUSERS if too many nested
  // namespaces are used. EINVAL for kernels that don't support the feature.
  // Valgrind will ENOSYS unshare().
  PCHECK(error == EPERM || error == EUSERS || error == EINVAL ||
         error == ENOSYS);
}

}  // namespace.

bool Credentials::DropAllCapabilities() {
  ScopedCap cap(cap_init());
  CHECK(cap);
  PCHECK(0 == cap_set_proc(cap.get()));
  CHECK(!HasAnyCapability());
  // We never let this function fail.
  return true;
}

bool Credentials::HasAnyCapability() {
  ScopedCap current_cap(cap_get_proc());
  CHECK(current_cap);
  ScopedCap empty_cap(cap_init());
  CHECK(empty_cap);
  return cap_compare(current_cap.get(), empty_cap.get()) != 0;
}

scoped_ptr<std::string> Credentials::GetCurrentCapString() {
  ScopedCap current_cap(cap_get_proc());
  CHECK(current_cap);
  ScopedCapText cap_text(cap_to_text(current_cap.get(), NULL));
  CHECK(cap_text);
  return scoped_ptr<std::string> (new std::string(cap_text.get()));
}

// static
bool Credentials::CanCreateProcessInNewUserNS() {
  // Valgrind will let clone(2) pass-through, but doesn't support unshare(),
  // so always consider UserNS unsupported there.
  if (IsRunningOnValgrind()) {
    return false;
  }

  // This is roughly a fork().
  const pid_t pid = sys_clone(CLONE_NEWUSER | SIGCHLD, 0, 0, 0, 0);

  if (pid == -1) {
    CheckCloneNewUserErrno(errno);
    return false;
  }

  // The parent process could have had threads. In the child, these threads
  // have disappeared. Make sure to not do anything in the child, as this is a
  // fragile execution environment.
  if (pid == 0) {
    _exit(kExitSuccess);
  }

  // Always reap the child.
  int status = -1;
  PCHECK(HANDLE_EINTR(waitpid(pid, &status, 0)) == pid);
  CHECK(WIFEXITED(status));
  CHECK_EQ(kExitSuccess, WEXITSTATUS(status));

  // clone(2) succeeded, we can use CLONE_NEWUSER.
  return true;
}

bool Credentials::MoveToNewUserNS() {
  uid_t uid;
  gid_t gid;
  if (!GetRESIds(&uid, &gid)) {
    // If all the uids (or gids) are not equal to each other, the security
    // model will most likely confuse the caller, abort.
    DVLOG(1) << "uids or gids differ!";
    return false;
  }
  int ret = unshare(CLONE_NEWUSER);
  if (ret) {
    const int unshare_errno = errno;
    VLOG(1) << "Looks like unprivileged CLONE_NEWUSER may not be available "
            << "on this kernel.";
    CheckCloneNewUserErrno(unshare_errno);
    return false;
  }

  if (NamespaceUtils::KernelSupportsDenySetgroups()) {
    PCHECK(NamespaceUtils::DenySetgroups());
  }

  // The current {r,e,s}{u,g}id is now an overflow id (c.f.
  // /proc/sys/kernel/overflowuid). Setup the uid and gid maps.
  DCHECK(GetRESIds(NULL, NULL));
  const char kGidMapFile[] = "/proc/self/gid_map";
  const char kUidMapFile[] = "/proc/self/uid_map";
  PCHECK(NamespaceUtils::WriteToIdMapFile(kGidMapFile, gid));
  PCHECK(NamespaceUtils::WriteToIdMapFile(kUidMapFile, uid));
  DCHECK(GetRESIds(NULL, NULL));
  return true;
}

bool Credentials::DropFileSystemAccess() {
  CHECK(ChrootToSafeEmptyDir());
  CHECK(!base::DirectoryExists(base::FilePath("/proc")));
  // We never let this function fail.
  return true;
}

}  // namespace sandbox.

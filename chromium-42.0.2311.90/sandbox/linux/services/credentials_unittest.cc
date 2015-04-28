// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/credentials.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

bool WorkingDirectoryIsRoot() {
  char current_dir[PATH_MAX];
  char* cwd = getcwd(current_dir, sizeof(current_dir));
  PCHECK(cwd);
  if (strcmp("/", cwd)) return false;

  // The current directory is the root. Add a few paranoid checks.
  struct stat current;
  CHECK_EQ(0, stat(".", &current));
  struct stat parrent;
  CHECK_EQ(0, stat("..", &parrent));
  CHECK_EQ(current.st_dev, parrent.st_dev);
  CHECK_EQ(current.st_ino, parrent.st_ino);
  CHECK_EQ(current.st_mode, parrent.st_mode);
  CHECK_EQ(current.st_uid, parrent.st_uid);
  CHECK_EQ(current.st_gid, parrent.st_gid);
  return true;
}

SANDBOX_TEST(Credentials, DropAllCaps) {
  CHECK(Credentials::DropAllCapabilities());
  CHECK(!Credentials::HasAnyCapability());
}

SANDBOX_TEST(Credentials, GetCurrentCapString) {
  CHECK(Credentials::DropAllCapabilities());
  const char kNoCapabilityText[] = "=";
  CHECK(*Credentials::GetCurrentCapString() == kNoCapabilityText);
}

SANDBOX_TEST(Credentials, MoveToNewUserNS) {
  CHECK(Credentials::DropAllCapabilities());
  bool moved_to_new_ns = Credentials::MoveToNewUserNS();
  fprintf(stdout,
          "Unprivileged CLONE_NEWUSER supported: %s\n",
          moved_to_new_ns ? "true." : "false.");
  fflush(stdout);
  if (!moved_to_new_ns) {
    fprintf(stdout, "This kernel does not support unprivileged namespaces. "
            "USERNS tests will succeed without running.\n");
    fflush(stdout);
    return;
  }
  CHECK(Credentials::HasAnyCapability());
  CHECK(Credentials::DropAllCapabilities());
  CHECK(!Credentials::HasAnyCapability());
}

SANDBOX_TEST(Credentials, CanCreateProcessInNewUserNS) {
  CHECK(Credentials::DropAllCapabilities());
  bool user_ns_supported = Credentials::CanCreateProcessInNewUserNS();
  bool moved_to_new_ns = Credentials::MoveToNewUserNS();
  CHECK_EQ(user_ns_supported, moved_to_new_ns);
}

SANDBOX_TEST(Credentials, UidIsPreserved) {
  CHECK(Credentials::DropAllCapabilities());
  uid_t old_ruid, old_euid, old_suid;
  gid_t old_rgid, old_egid, old_sgid;
  PCHECK(0 == getresuid(&old_ruid, &old_euid, &old_suid));
  PCHECK(0 == getresgid(&old_rgid, &old_egid, &old_sgid));
  // Probably missing kernel support.
  if (!Credentials::MoveToNewUserNS()) return;
  uid_t new_ruid, new_euid, new_suid;
  PCHECK(0 == getresuid(&new_ruid, &new_euid, &new_suid));
  CHECK(old_ruid == new_ruid);
  CHECK(old_euid == new_euid);
  CHECK(old_suid == new_suid);

  gid_t new_rgid, new_egid, new_sgid;
  PCHECK(0 == getresgid(&new_rgid, &new_egid, &new_sgid));
  CHECK(old_rgid == new_rgid);
  CHECK(old_egid == new_egid);
  CHECK(old_sgid == new_sgid);
}

bool NewUserNSCycle() {
  if (!Credentials::MoveToNewUserNS() ||
      !Credentials::HasAnyCapability() ||
      !Credentials::DropAllCapabilities() ||
      Credentials::HasAnyCapability()) {
    return false;
  }
  return true;
}

SANDBOX_TEST(Credentials, NestedUserNS) {
  CHECK(Credentials::DropAllCapabilities());
  // Probably missing kernel support.
  if (!Credentials::MoveToNewUserNS()) return;
  CHECK(Credentials::DropAllCapabilities());
  // As of 3.12, the kernel has a limit of 32. See create_user_ns().
  const int kNestLevel = 10;
  for (int i = 0; i < kNestLevel; ++i) {
    CHECK(NewUserNSCycle()) << "Creating new user NS failed at iteration "
                                  << i << ".";
  }
}

// Test the WorkingDirectoryIsRoot() helper.
SANDBOX_TEST(Credentials, CanDetectRoot) {
  PCHECK(0 == chdir("/proc/"));
  CHECK(!WorkingDirectoryIsRoot());
  PCHECK(0 == chdir("/"));
  CHECK(WorkingDirectoryIsRoot());
}

// Disabled on ASAN because of crbug.com/451603.
SANDBOX_TEST(Credentials, DISABLE_ON_ASAN(DropFileSystemAccessIsSafe)) {
  CHECK(Credentials::DropAllCapabilities());
  // Probably missing kernel support.
  if (!Credentials::MoveToNewUserNS()) return;
  CHECK(Credentials::DropFileSystemAccess());
  CHECK(!base::DirectoryExists(base::FilePath("/proc")));
  CHECK(WorkingDirectoryIsRoot());
  CHECK(base::IsDirectoryEmpty(base::FilePath("/")));
  // We want the chroot to never have a subdirectory. A subdirectory
  // could allow a chroot escape.
  CHECK_NE(0, mkdir("/test", 0700));
}

// Check that after dropping filesystem access and dropping privileges
// it is not possible to regain capabilities.
SANDBOX_TEST(Credentials, DISABLE_ON_ASAN(CannotRegainPrivileges)) {
  CHECK(Credentials::DropAllCapabilities());
  // Probably missing kernel support.
  if (!Credentials::MoveToNewUserNS()) return;
  CHECK(Credentials::DropFileSystemAccess());
  CHECK(Credentials::DropAllCapabilities());

  // The kernel should now prevent us from regaining capabilities because we
  // are in a chroot.
  CHECK(!Credentials::CanCreateProcessInNewUserNS());
  CHECK(!Credentials::MoveToNewUserNS());
}

}  // namespace.

}  // namespace sandbox.

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_handle.h"

#include "base/file_util.h"
#include "base/process/internal_linux.h"

namespace base {

#ifdef OS_LINUX
ProcessId GetParentProcessId(ProcessHandle process) {
  ProcessId pid =
      internal::ReadProcStatsAndGetFieldAsInt(process, internal::VM_PPID);
  if (pid)
    return pid;
  return -1;
}

FilePath GetProcessExecutablePath(ProcessHandle process) {
  FilePath stat_file = internal::GetProcPidDir(process).Append("exe");
  FilePath exe_name;
  if (!file_util::ReadSymbolicLink(stat_file, &exe_name)) {
    // No such process.  Happens frequently in e.g. TerminateAllChromeProcesses
    return FilePath();
  }
  return exe_name;
}
#endif

}  // namespace base

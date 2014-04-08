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

#include "base/posix/capsicum.h"
#include "ipc/ipc_descriptors.h"


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
  __soaap_limit_syscalls(
    __acl_aclcheck_fd, __acl_delete_fd, __acl_get_fd, __acl_set_fd,
    __mac_get_fd, __mac_get_proc, __mac_set_fd, __mac_set_proc,
    __sysctl, _umtx_lock, _umtx_op, _umtx_unlock, abort2, accept, accept4,
    aio_cancel, aio_error, aio_fsync, aio_read, aio_return, aio_suspend,
    aio_waitcomplete, aio_write, bindat, cap_enter, cap_fcntls_get,
    cap_fcntls_limit, cap_getmode, cap_ioctls_get, cap_ioctls_limit,
    __cap_rights_get, cap_rights_limit, clock_getres, clock_gettime, close,
    closefrom, connectat, dup, dup2, extattr_delete_fd, extattr_get_fd,
    extattr_list_fd, extattr_set_fd, fchflags, fchmod, fchown, fcntl, fexecve,
    flock, fork, fpathconf, freebsd6_ftruncate, freebsd6_lseek, freebsd6_mmap,
    freebsd6_pread, freebsd6_pwrite, fstat, fstatfs, fsync, ftruncate, futimes,
    getaudit, getaudit_addr, getauid, getcontext, getdents, getdirentries,
    getdomainname, getegid, geteuid, gethostid, gethostname, getitimer, getgid,
    getgroups, getlogin, getpagesize, getpeername, getpgid, getpgrp, getpid,
    getppid, getpriority, getresgid, getresuid, getrlimit, getrusage, getsid,
    getsockname, getsockopt, gettimeofday, getuid, ioctl, issetugid, kevent,
    kill, kmq_notify, kmq_setattr, kmq_timedreceive, kmq_timedsend, kqueue,
    ktimer_create, ktimer_delete, ktimer_getoverrun, ktimer_gettime,
    ktimer_settime, lio_listio, listen, lseek, madvise, mincore, minherit,
    mlock, mlockall, mmap, mprotect, msync, munlock, munlockall, munmap,
    nanosleep, ntp_gettime, oaio_read, oaio_write, obreak, olio_listio,
    chflagsat, faccessat, fchmodat, fchownat, fstatat, futimesat, linkat,
    mkdirat, mkfifoat, mknodat, openat, readlinkat, renameat, symlinkat,
    unlinkat, open, openbsd_poll, pdfork, pdgetpid, pdkill, pipe, pipe2, poll,
    pread, preadv, profil, pwrite, pwritev, read, readv, recv, recvfrom,
    recvmsg, rtprio, rtprio_thread, sbrk, sched_get_priority_max,
    sched_get_priority_min, sched_getparam, sched_getscheduler,
    sched_rr_get_interval, sched_setparam, sched_setscheduler, sched_yield,
    sctp_generic_recvmsg, sctp_generic_sendmsg, sctp_generic_sendmsg_iov,
    sctp_peeloff, pselect, select, send, sendfile, sendmsg, sendto, setaudit,
    setaudit_addr, setauid, setcontext, setegid, seteuid, setgid, setitimer,
    setpriority, setregid, setresgid, setresuid, setreuid, setrlimit, setsid,
    setsockopt, setuid, shm_open, shutdown, sigaction, sigaltstack, sigblock,
    sigpending, sigprocmask, sigqueue, sigreturn, sigsetmask, sigstack,
    sigsuspend, sigtimedwait, sigvec, sigwaitinfo, sigwait, socket,
    socketpair, sstk, sync, sys_exit, sysarch, thr_create, thr_exit, thr_kill,
    thr_new, thr_self, thr_set_name, thr_suspend, thr_wake, umask, utrace,
    uuidgen, write, writev, yield
  );
  return (cap_enter() == 0);
}

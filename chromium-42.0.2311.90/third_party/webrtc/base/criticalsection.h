/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_CRITICALSECTION_H__
#define WEBRTC_BASE_CRITICALSECTION_H__

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/thread_annotations.h"

#if defined(WEBRTC_WIN)
#include "webrtc/base/win32.h"
#endif

#if defined(WEBRTC_POSIX)
#include <pthread.h>
#endif

#ifdef _DEBUG
#define CS_TRACK_OWNER 1
#endif  // _DEBUG

#if CS_TRACK_OWNER
#define TRACK_OWNER(x) x
#else  // !CS_TRACK_OWNER
#define TRACK_OWNER(x)
#endif  // !CS_TRACK_OWNER

namespace rtc {

#if defined(WEBRTC_WIN)
class LOCKABLE CriticalSection {
 public:
  CriticalSection() { InitializeCriticalSection(&crit_); }
  ~CriticalSection() { DeleteCriticalSection(&crit_); }
  void Enter() EXCLUSIVE_LOCK_FUNCTION() {
    EnterCriticalSection(&crit_);
  }
  bool TryEnter() EXCLUSIVE_TRYLOCK_FUNCTION(true) {
    return TryEnterCriticalSection(&crit_) != FALSE;
  }
  void Leave() UNLOCK_FUNCTION() {
    LeaveCriticalSection(&crit_);
  }

  // Used for debugging.
  bool CurrentThreadIsOwner() const {
    return crit_.OwningThread == reinterpret_cast<HANDLE>(GetCurrentThreadId());
  }

 private:
  CRITICAL_SECTION crit_;
};
#endif // WEBRTC_WIN

#if defined(WEBRTC_POSIX)
class LOCKABLE CriticalSection {
 public:
  CriticalSection() {
    pthread_mutexattr_t mutex_attribute;
    pthread_mutexattr_init(&mutex_attribute);
    pthread_mutexattr_settype(&mutex_attribute, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&mutex_, &mutex_attribute);
    pthread_mutexattr_destroy(&mutex_attribute);
    TRACK_OWNER(thread_ = 0);
  }
  ~CriticalSection() {
    pthread_mutex_destroy(&mutex_);
  }
  void Enter() EXCLUSIVE_LOCK_FUNCTION() {
    pthread_mutex_lock(&mutex_);
    TRACK_OWNER(thread_ = pthread_self());
  }
  bool TryEnter() EXCLUSIVE_TRYLOCK_FUNCTION(true) {
    if (pthread_mutex_trylock(&mutex_) == 0) {
      TRACK_OWNER(thread_ = pthread_self());
      return true;
    }
    return false;
  }
  void Leave() UNLOCK_FUNCTION() {
    TRACK_OWNER(thread_ = 0);
    pthread_mutex_unlock(&mutex_);
  }

  // Used for debugging.
  bool CurrentThreadIsOwner() const {
#if CS_TRACK_OWNER
    return pthread_equal(thread_, pthread_self());
#else
    return true;
#endif  // CS_TRACK_OWNER
  }

 private:
  pthread_mutex_t mutex_;
  TRACK_OWNER(pthread_t thread_);
};
#endif // WEBRTC_POSIX

// CritScope, for serializing execution through a scope.
class SCOPED_LOCKABLE CritScope {
 public:
  explicit CritScope(CriticalSection *pcrit) EXCLUSIVE_LOCK_FUNCTION(pcrit) {
    pcrit_ = pcrit;
    pcrit_->Enter();
  }
  ~CritScope() UNLOCK_FUNCTION() {
    pcrit_->Leave();
  }
 private:
  CriticalSection *pcrit_;
  DISALLOW_COPY_AND_ASSIGN(CritScope);
};

// Tries to lock a critical section on construction via
// CriticalSection::TryEnter, and unlocks on destruction if the
// lock was taken. Never blocks.
//
// IMPORTANT: Unlike CritScope, the lock may not be owned by this thread in
// subsequent code. Users *must* check locked() to determine if the
// lock was taken. If you're not calling locked(), you're doing it wrong!
class TryCritScope {
 public:
  explicit TryCritScope(CriticalSection *pcrit) {
    pcrit_ = pcrit;
    locked_ = pcrit_->TryEnter();
  }
  ~TryCritScope() {
    if (locked_) {
      pcrit_->Leave();
    }
  }
  bool locked() const {
    return locked_;
  }
 private:
  CriticalSection *pcrit_;
  bool locked_;
  DISALLOW_COPY_AND_ASSIGN(TryCritScope);
};

// TODO: Move this to atomicops.h, which can't be done easily because of
// complex compile rules.
class AtomicOps {
 public:
#if defined(WEBRTC_WIN)
  // Assumes sizeof(int) == sizeof(LONG), which it is on Win32 and Win64.
  static int Increment(int* i) {
    return ::InterlockedIncrement(reinterpret_cast<LONG*>(i));
  }
  static int Decrement(int* i) {
    return ::InterlockedDecrement(reinterpret_cast<LONG*>(i));
  }
#else
  static int Increment(int* i) {
    return __sync_add_and_fetch(i, 1);
  }
  static int Decrement(int* i) {
    return __sync_sub_and_fetch(i, 1);
  }
#endif
};

} // namespace rtc

#endif // WEBRTC_BASE_CRITICALSECTION_H__

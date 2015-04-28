// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Scheduler_h
#define Scheduler_h

#include "platform/PlatformExport.h"
#include "public/platform/WebThread.h"
#include "wtf/Functional.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassOwnPtr.h"

namespace blink {
class WebScheduler;

// The scheduler is an opinionated gateway for arranging work to be run on the
// main thread. It decides which tasks get priority over others based on a
// scheduling policy and the overall system state.
class PLATFORM_EXPORT Scheduler {
    WTF_MAKE_NONCOPYABLE(Scheduler);
public:
    // An IdleTask is passed a deadline in CLOCK_MONOTONIC seconds and is expected to complete before this deadline.
    typedef Function<void(double deadlineSeconds)> IdleTask;

    static Scheduler* shared();
    static void shutdown();

    // For non-critical tasks which may be reordered relative to other task types and may be starved
    // for an arbitrarily long time if no idle time is available.
    void postIdleTask(const WebTraceLocation&, PassOwnPtr<IdleTask>);

    // For tasks related to loading, e.g. HTML parsing.  Loading tasks usually have default priority
    // but they may be deprioritized when the user is interacting with the device.
    // Takes ownership of |WebThread::Task|.
    void postLoadingTask(const WebTraceLocation&, WebThread::Task*);

    // Returns true if there is high priority work pending on the main thread
    // and the caller should yield to let the scheduler service that work.
    // Must be called on the main thread.
    bool shouldYieldForHighPriorityWork() const;

protected:
    Scheduler(WebScheduler*);
    virtual ~Scheduler();

    static Scheduler* s_sharedScheduler;
    WebScheduler* m_webScheduler;
};

} // namespace blink

#endif // Scheduler_h

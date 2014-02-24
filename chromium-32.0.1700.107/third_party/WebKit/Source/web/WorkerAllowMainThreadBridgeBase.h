/*
 * Copyright (C) 2010, 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "WebWorkerBase.h"
#include "core/dom/CrossThreadTask.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/CrossThreadCopier.h"

namespace WebKit {

// FIXME: Deprecate this bridge code when PermissionClientProxy is implemented
// by the embedder.
// Base class for worker thread bridges. This class adds an observer to
// WorkerGlobalScope so that it doesn't try to use deleted pointers when
// WorkerGlobalScope is destroyed.
class WorkerAllowMainThreadBridgeBase : public ThreadSafeRefCounted<WorkerAllowMainThreadBridgeBase> {
    WTF_MAKE_NONCOPYABLE(WorkerAllowMainThreadBridgeBase);
public:
    WorkerAllowMainThreadBridgeBase(WebCore::WorkerGlobalScope*, WebWorkerBase*);

    virtual ~WorkerAllowMainThreadBridgeBase()
    {
    }

    // This class is passed across threads, so subclasses if it should make sure
    // any string fields are copied.
    class AllowParams {
    public:
        AllowParams(const String& mode)
            : m_mode(mode.isolatedCopy())
        {
        }

        virtual ~AllowParams()
        {
        }

        String m_mode;
    };

    // These methods are invoked on the worker context.
    void cancel()
    {
        MutexLocker locker(m_mutex);
        m_webWorkerBase = 0;
    }

    bool result()
    {
        return m_result;
    }

    virtual bool allowOnMainThread(WebCommonWorkerClient*, AllowParams*) = 0;

protected:
    void postTaskToMainThread(PassOwnPtr<AllowParams>);

private:
    static void allowTask(WebCore::ExecutionContext*, PassOwnPtr<AllowParams>, PassRefPtr<WorkerAllowMainThreadBridgeBase>);
    static void didComplete(WebCore::ExecutionContext*, PassRefPtr<WorkerAllowMainThreadBridgeBase>, bool);

    Mutex m_mutex;
    WebWorkerBase* m_webWorkerBase;
    OwnPtr<WebCore::WorkerGlobalScope::Observer> m_workerGlobalScopeObserver;
    bool m_result;
};

} // namespace WebKit

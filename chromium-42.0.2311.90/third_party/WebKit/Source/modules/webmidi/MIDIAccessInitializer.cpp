// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/webmidi/MIDIAccessInitializer.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMError.h"
#include "core/dom/Document.h"
#include "core/frame/Navigator.h"
#include "modules/webmidi/MIDIAccess.h"
#include "modules/webmidi/MIDIController.h"
#include "modules/webmidi/MIDIOptions.h"
#include "modules/webmidi/MIDIPort.h"

namespace blink {

MIDIAccessInitializer::MIDIAccessInitializer(ScriptState* scriptState, const MIDIOptions& options)
    : ScriptPromiseResolver(scriptState)
    , m_requestSysex(false)
    , m_hasBeenDisposed(false)
    , m_sysexPermissionResolved(false)
{
#if ENABLE(OILPAN)
    // A prefinalizer has already been registered (as a LifecycleObserver);
    // remove it and register a combined one, as the infrastructure doesn't
    // support multiple prefinalizers for an object.
    //
    // FIXME: Oilpan: remove LifecycleObserver's need for a prefinalizer,
    // and as a consequence, this unregistration step. If the former is independently
    // removed, the unregisterPreFinalizer() call will assert.
    ThreadState::current()->unregisterPreFinalizer(*static_cast<LifecycleObserver*>(this));
    ThreadState::current()->registerPreFinalizer(*this);
#endif
    if (options.hasSysex())
        m_requestSysex = options.sysex();
}

MIDIAccessInitializer::~MIDIAccessInitializer()
{
#if !ENABLE(OILPAN)
    dispose();
#endif
}

void MIDIAccessInitializer::contextDestroyed()
{
    dispose();
}

void MIDIAccessInitializer::dispose()
{
    if (m_hasBeenDisposed)
        return;

    if (!executionContext())
        return;

    if (!m_sysexPermissionResolved) {
        Document* document = toDocument(executionContext());
        ASSERT(document);
        if (MIDIController* controller = MIDIController::from(document->frame()))
            controller->cancelSysexPermissionRequest(this);
        m_sysexPermissionResolved = true;
    }

    m_hasBeenDisposed = true;

#if ENABLE(OILPAN)
    // Delegate to LifecycleObserver's prefinalizer.
    LifecycleObserver::dispose();
#endif
}

ScriptPromise MIDIAccessInitializer::start()
{
    ScriptPromise promise = this->promise();
    m_accessor = MIDIAccessor::create(this);

    if (!m_requestSysex) {
        m_accessor->startSession();
        return promise;
    }
    Document* document = toDocument(executionContext());
    ASSERT(document);
    if (MIDIController* controller = MIDIController::from(document->frame()))
        controller->requestSysexPermission(this);
    else
        reject(DOMError::create("SecurityError"));

    return promise;
}

void MIDIAccessInitializer::didAddInputPort(const String& id, const String& manufacturer, const String& name, const String& version, bool isActive)
{
    ASSERT(m_accessor);
    m_portDescriptors.append(PortDescriptor(id, manufacturer, name, MIDIPort::MIDIPortTypeInput, version, isActive));
}

void MIDIAccessInitializer::didAddOutputPort(const String& id, const String& manufacturer, const String& name, const String& version, bool isActive)
{
    ASSERT(m_accessor);
    m_portDescriptors.append(PortDescriptor(id, manufacturer, name, MIDIPort::MIDIPortTypeOutput, version, isActive));
}

void MIDIAccessInitializer::didSetInputPortState(unsigned portIndex, bool isActive)
{
    // didSetInputPortState() is not allowed to call before didStartSession()
    // is called. Once didStartSession() is called, MIDIAccessorClient methods
    // are delegated to MIDIAccess. See constructor of MIDIAccess.
    ASSERT_NOT_REACHED();
}

void MIDIAccessInitializer::didSetOutputPortState(unsigned portIndex, bool isActive)
{
    // See comments on didSetInputPortState().
    ASSERT_NOT_REACHED();
}

void MIDIAccessInitializer::didStartSession(bool success, const String& error, const String& message)
{
    ASSERT(m_accessor);
    if (success) {
        resolve(MIDIAccess::create(m_accessor.release(), m_requestSysex, m_portDescriptors, executionContext()));
    } else {
        reject(DOMError::create(error, message));
    }
}

void MIDIAccessInitializer::resolveSysexPermission(bool allowed)
{
    m_sysexPermissionResolved = true;
    if (allowed)
        m_accessor->startSession();
    else
        reject(DOMError::create("SecurityError"));
}

SecurityOrigin* MIDIAccessInitializer::securityOrigin() const
{
    return executionContext()->securityOrigin();
}

ExecutionContext* MIDIAccessInitializer::executionContext() const
{
    return scriptState()->executionContext();
}

} // namespace blink

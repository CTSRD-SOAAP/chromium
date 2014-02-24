/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "core/dom/MessagePort.h"

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/MessageEvent.h"
#include "core/events/ThreadLocalEventNames.h"
#include "core/frame/DOMWindow.h"
#include "core/workers/WorkerGlobalScope.h"
#include "wtf/text/AtomicString.h"

namespace WebCore {

MessagePort::MessagePort(ExecutionContext& executionContext)
    : m_started(false)
    , m_closed(false)
    , m_executionContext(&executionContext)
{
    ScriptWrappable::init(this);
    m_executionContext->createdMessagePort(this);

    // Don't need to call processMessagePortMessagesSoon() here, because the port will not be opened until start() is invoked.
}

MessagePort::~MessagePort()
{
    close();
    if (m_executionContext)
        m_executionContext->destroyedMessagePort(this);
}

void MessagePort::postMessage(PassRefPtr<SerializedScriptValue> message, const MessagePortArray* ports, ExceptionState& es)
{
    if (!isEntangled())
        return;
    ASSERT(m_executionContext);

    OwnPtr<MessagePortChannelArray> channels;
    // Make sure we aren't connected to any of the passed-in ports.
    if (ports) {
        for (unsigned int i = 0; i < ports->size(); ++i) {
            MessagePort* dataPort = (*ports)[i].get();
            if (dataPort == this) {
                es.throwDOMException(DataCloneError, ExceptionMessages::failedToExecute("postMessage", "MessagePort", "Item #" + String::number(i) + " in the array of ports contains the source port."));
                return;
            }
        }
        channels = MessagePort::disentanglePorts(ports, es);
        if (es.hadException())
            return;
    }
    m_entangledChannel->postMessageToRemote(message, channels.release());
}

PassOwnPtr<MessagePortChannel> MessagePort::disentangle()
{
    ASSERT(m_entangledChannel);

    m_entangledChannel->disentangle();

    // We can't receive any messages or generate any events, so remove ourselves from the list of active ports.
    ASSERT(m_executionContext);
    m_executionContext->destroyedMessagePort(this);
    m_executionContext = 0;

    return m_entangledChannel.release();
}

// Invoked to notify us that there are messages available for this port.
// This code may be called from another thread, and so should not call any non-threadsafe APIs (i.e. should not call into the entangled channel or access mutable variables).
void MessagePort::messageAvailable()
{
    ASSERT(m_executionContext);
    m_executionContext->processMessagePortMessagesSoon();
}

void MessagePort::start()
{
    // Do nothing if we've been cloned or closed.
    if (!isEntangled())
        return;

    ASSERT(m_executionContext);
    if (m_started)
        return;

    m_started = true;
    m_executionContext->processMessagePortMessagesSoon();
}

void MessagePort::close()
{
    if (isEntangled())
        m_entangledChannel->close();
    m_closed = true;
}

void MessagePort::entangle(PassOwnPtr<MessagePortChannel> remote)
{
    // Only invoked to set our initial entanglement.
    ASSERT(!m_entangledChannel);
    ASSERT(m_executionContext);

    remote->entangle(this);
    m_entangledChannel = remote;
}

void MessagePort::contextDestroyed()
{
    ASSERT(m_executionContext);
    // Must be closed before blowing away the cached context, to ensure that we get no more calls to messageAvailable().
    // ExecutionContext::closeMessagePorts() takes care of that.
    ASSERT(m_closed);
    m_executionContext = 0;
}

const AtomicString& MessagePort::interfaceName() const
{
    return EventTargetNames::MessagePort;
}

ExecutionContext* MessagePort::executionContext() const
{
    return m_executionContext;
}

void MessagePort::dispatchMessages()
{
    // Messages for contexts that are not fully active get dispatched too, but JSAbstractEventListener::handleEvent() doesn't call handlers for these.
    // The HTML5 spec specifies that any messages sent to a document that is not fully active should be dropped, so this behavior is OK.
    ASSERT(started());

    RefPtr<SerializedScriptValue> message;
    OwnPtr<MessagePortChannelArray> channels;
    while (m_entangledChannel && m_entangledChannel->tryGetMessageFromRemote(message, channels)) {

        // close() in Worker onmessage handler should prevent next message from dispatching.
        if (m_executionContext->isWorkerGlobalScope() && toWorkerGlobalScope(m_executionContext)->isClosing())
            return;

        OwnPtr<MessagePortArray> ports = MessagePort::entanglePorts(*m_executionContext, channels.release());
        RefPtr<Event> evt = MessageEvent::create(ports.release(), message.release());

        dispatchEvent(evt.release(), ASSERT_NO_EXCEPTION);
    }
}

bool MessagePort::hasPendingActivity()
{
    // The spec says that entangled message ports should always be treated as if they have a strong reference.
    // We'll also stipulate that the queue needs to be open (if the app drops its reference to the port before start()-ing it, then it's not really entangled as it's unreachable).
    if (m_started && m_entangledChannel && m_entangledChannel->hasPendingActivity())
        return true;
    return isEntangled();
}

PassOwnPtr<MessagePortChannelArray> MessagePort::disentanglePorts(const MessagePortArray* ports, ExceptionState& es)
{
    if (!ports || !ports->size())
        return nullptr;

    // HashSet used to efficiently check for duplicates in the passed-in array.
    HashSet<MessagePort*> portSet;

    // Walk the incoming array - if there are any duplicate ports, or null ports or cloned ports, throw an error (per section 8.3.3 of the HTML5 spec).
    for (unsigned i = 0; i < ports->size(); ++i) {
        MessagePort* port = (*ports)[i].get();
        if (!port || port->isNeutered() || portSet.contains(port)) {
            String type;
            if (!port)
                type = "null";
            else if (port->isNeutered())
                type = "already neutered";
            else
                type = "a duplicate";
            es.throwDOMException(DataCloneError, ExceptionMessages::failedToExecute("disentanglePorts", "MessagePort", "Item #"  + String::number(i) + " in the array of ports is " + type + "."));
            return nullptr;
        }
        portSet.add(port);
    }

    // Passed-in ports passed validity checks, so we can disentangle them.
    OwnPtr<MessagePortChannelArray> portArray = adoptPtr(new MessagePortChannelArray(ports->size()));
    for (unsigned i = 0; i < ports->size(); ++i)
        (*portArray)[i] = (*ports)[i]->disentangle();
    return portArray.release();
}

PassOwnPtr<MessagePortArray> MessagePort::entanglePorts(ExecutionContext& context, PassOwnPtr<MessagePortChannelArray> channels)
{
    if (!channels || !channels->size())
        return nullptr;

    OwnPtr<MessagePortArray> portArray = adoptPtr(new MessagePortArray(channels->size()));
    for (unsigned int i = 0; i < channels->size(); ++i) {
        RefPtr<MessagePort> port = MessagePort::create(context);
        port->entangle((*channels)[i].release());
        (*portArray)[i] = port.release();
    }
    return portArray.release();
}

} // namespace WebCore

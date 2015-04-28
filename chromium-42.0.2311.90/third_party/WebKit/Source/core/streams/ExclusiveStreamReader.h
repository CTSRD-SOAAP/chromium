// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ExclusiveStreamReader_h
#define ExclusiveStreamReader_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseProperty.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/ToV8.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/streams/ReadableStream.h"
#include "platform/heap/Handle.h"

namespace blink {

class DOMException;
class ExceptionState;
class ScriptState;

// ExclusiveStreamReader corresponds to the same-name class in the Streams spec
// https://streams.spec.whatwg.org/. This class trusts ReadableStream, contrary
// to the class in the Streams spec, because we only support
// blink::Readable[Byte]Stream as this reader's customer.
class ExclusiveStreamReader final : public GarbageCollectedFinalized<ExclusiveStreamReader>, public ScriptWrappable, public ActiveDOMObject {
    DEFINE_WRAPPERTYPEINFO();
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(ExclusiveStreamReader);
public:
    // The stream must not be locked to any ExclusiveStreamReader when called.
    ExclusiveStreamReader(ReadableStream*);

    ScriptPromise closed(ScriptState*);
    bool isActive() const;
    ScriptPromise ready(ScriptState*);
    String state() const;
    ScriptPromise cancel(ScriptState*, ScriptValue reason);
    ScriptValue read(ScriptState*, ExceptionState&);
    void releaseLock();
    ScriptPromise released(ScriptState*);

    bool hasPendingActivity() const override;
    void stop() override;

    DECLARE_TRACE();

private:
    using ReleasedPromise = ScriptPromiseProperty<Member<ExclusiveStreamReader>, ToV8UndefinedGenerator, ToV8UndefinedGenerator>;
    using ClosedPromise = ScriptPromiseProperty<Member<ExclusiveStreamReader>, ToV8UndefinedGenerator, RefPtrWillBeMember<DOMException>>;
    using ReadyPromise = ScriptPromiseProperty<Member<ExclusiveStreamReader>, ToV8UndefinedGenerator, ToV8UndefinedGenerator>;

    const Member<ReadableStream> m_stream;
    const Member<ReleasedPromise> m_released;
    ReadableStream::State m_stateAfterRelease;
    Member<ClosedPromise> m_closedAfterRelease;
    Member<ReadyPromise> m_readyAfterRelease;
};

} // namespace blink

#endif // ExclusiveStreamReader_h

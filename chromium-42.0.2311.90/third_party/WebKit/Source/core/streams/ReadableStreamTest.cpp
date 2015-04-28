// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/streams/ReadableStream.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/streams/ExclusiveStreamReader.h"
#include "core/streams/ReadableStreamImpl.h"
#include "core/streams/UnderlyingSource.h"
#include "core/testing/DummyPageHolder.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace blink {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;

namespace {

using Checkpoint = ::testing::StrictMock<::testing::MockFunction<void(int)>>;
using StringStream = ReadableStreamImpl<ReadableStreamChunkTypeTraits<String>>;

class StringCapturingFunction : public ScriptFunction {
public:
    static v8::Handle<v8::Function> createFunction(ScriptState* scriptState, String* value)
    {
        StringCapturingFunction* self = new StringCapturingFunction(scriptState, value);
        return self->bindToV8Function();
    }

private:
    StringCapturingFunction(ScriptState* scriptState, String* value)
        : ScriptFunction(scriptState)
        , m_value(value)
    {
    }

    ScriptValue call(ScriptValue value) override
    {
        ASSERT(!value.isEmpty());
        *m_value = toCoreString(value.v8Value()->ToString(scriptState()->isolate()));
        return value;
    }

    String* m_value;
};

class MockUnderlyingSource : public GarbageCollectedFinalized<MockUnderlyingSource>, public UnderlyingSource {
    USING_GARBAGE_COLLECTED_MIXIN(MockUnderlyingSource);
public:
    ~MockUnderlyingSource() override { }
    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        UnderlyingSource::trace(visitor);
    }

    MOCK_METHOD0(pullSource, void());
    MOCK_METHOD2(cancelSource, ScriptPromise(ScriptState*, ScriptValue));
};

class PermissiveStrategy : public StringStream::Strategy {
public:
    bool shouldApplyBackpressure(size_t, ReadableStream*) override { return false; }
};

class MockStrategy : public StringStream::Strategy {
public:
    static ::testing::StrictMock<MockStrategy>* create() { return new ::testing::StrictMock<MockStrategy>; }

    MOCK_METHOD2(shouldApplyBackpressure, bool(size_t, ReadableStream*));
    MOCK_METHOD2(size, size_t(const String&, ReadableStream*));
};

class ThrowError {
public:
    explicit ThrowError(const String& message)
        : m_message(message) { }

    void operator()(ExceptionState* exceptionState)
    {
        exceptionState->throwTypeError(m_message);
    }

private:
    String m_message;
};

} // unnamed namespace

class ReadableStreamTest : public ::testing::Test {
public:
    ReadableStreamTest()
        : m_page(DummyPageHolder::create(IntSize(1, 1)))
        , m_scope(scriptState())
        , m_underlyingSource(new ::testing::StrictMock<MockUnderlyingSource>)
        , m_exceptionState(ExceptionState::ConstructionContext, "property", "interface", scriptState()->context()->Global(), isolate())
    {
    }

    ~ReadableStreamTest() override
    {
    }

    ScriptState* scriptState() { return ScriptState::forMainWorld(m_page->document().frame()); }
    v8::Isolate* isolate() { return scriptState()->isolate(); }

    v8::Handle<v8::Function> createCaptor(String* value)
    {
        return StringCapturingFunction::createFunction(scriptState(), value);
    }

    StringStream* construct(MockStrategy* strategy)
    {
        Checkpoint checkpoint;
        {
            InSequence s;
            EXPECT_CALL(checkpoint, Call(0));
            EXPECT_CALL(*strategy, shouldApplyBackpressure(0, _)).WillOnce(Return(true));
            EXPECT_CALL(checkpoint, Call(1));
        }
        StringStream* stream = new StringStream(scriptState()->executionContext(), m_underlyingSource, strategy);
        checkpoint.Call(0);
        stream->didSourceStart();
        checkpoint.Call(1);
        return stream;
    }
    StringStream* construct()
    {
        Checkpoint checkpoint;
        {
            InSequence s;
            EXPECT_CALL(checkpoint, Call(0));
            EXPECT_CALL(*m_underlyingSource, pullSource()).Times(1);
            EXPECT_CALL(checkpoint, Call(1));
        }
        StringStream* stream = new StringStream(scriptState()->executionContext(), m_underlyingSource, new PermissiveStrategy);
        checkpoint.Call(0);
        stream->didSourceStart();
        checkpoint.Call(1);
        return stream;
    }

    OwnPtr<DummyPageHolder> m_page;
    ScriptState::Scope m_scope;
    Persistent<MockUnderlyingSource> m_underlyingSource;
    ExceptionState m_exceptionState;
};

TEST_F(ReadableStreamTest, Start)
{
    Checkpoint checkpoint;
    {
        InSequence s;
        EXPECT_CALL(checkpoint, Call(0));
        EXPECT_CALL(*m_underlyingSource, pullSource()).Times(1);
        EXPECT_CALL(checkpoint, Call(1));
    }

    StringStream* stream = new StringStream(scriptState()->executionContext(), m_underlyingSource);
    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_FALSE(stream->isStarted());
    EXPECT_FALSE(stream->isDraining());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_EQ(stream->stateInternal(), ReadableStream::Waiting);

    checkpoint.Call(0);
    stream->didSourceStart();
    checkpoint.Call(1);

    EXPECT_TRUE(stream->isStarted());
    EXPECT_FALSE(stream->isDraining());
    EXPECT_TRUE(stream->isPulling());
    EXPECT_EQ(stream->stateInternal(), ReadableStream::Waiting);

    // We need to call |error| in order to make
    // ActiveDOMObject::hasPendingActivity return false.
    stream->error(DOMException::create(AbortError, "done"));
}

TEST_F(ReadableStreamTest, StartFail)
{
    StringStream* stream = new StringStream(scriptState()->executionContext(), m_underlyingSource);
    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_FALSE(stream->isStarted());
    EXPECT_FALSE(stream->isDraining());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_EQ(stream->stateInternal(), ReadableStream::Waiting);

    stream->error(DOMException::create(NotFoundError));

    EXPECT_FALSE(stream->isStarted());
    EXPECT_FALSE(stream->isDraining());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_EQ(stream->stateInternal(), ReadableStream::Errored);
}

TEST_F(ReadableStreamTest, WaitOnWaiting)
{
    StringStream* stream = construct();
    Checkpoint checkpoint;

    EXPECT_EQ(ReadableStream::Waiting, stream->stateInternal());
    EXPECT_TRUE(stream->isStarted());
    EXPECT_TRUE(stream->isPulling());

    EXPECT_EQ(ReadableStream::Waiting, stream->stateInternal());

    stream->error(DOMException::create(AbortError, "done"));
}

TEST_F(ReadableStreamTest, WaitDuringStarting)
{
    StringStream* stream = new StringStream(scriptState()->executionContext(), m_underlyingSource);
    Checkpoint checkpoint;

    EXPECT_EQ(ReadableStream::Waiting, stream->stateInternal());
    EXPECT_FALSE(stream->isStarted());
    EXPECT_FALSE(stream->isPulling());

    {
        InSequence s;
        EXPECT_CALL(checkpoint, Call(0));
        EXPECT_CALL(*m_underlyingSource, pullSource()).Times(1);
        EXPECT_CALL(checkpoint, Call(1));
    }

    stream->ready(scriptState());
    checkpoint.Call(0);
    stream->didSourceStart();
    checkpoint.Call(1);

    EXPECT_EQ(ReadableStream::Waiting, stream->stateInternal());
    EXPECT_TRUE(stream->isStarted());
    EXPECT_TRUE(stream->isPulling());

    stream->error(DOMException::create(AbortError, "done"));
}

TEST_F(ReadableStreamTest, WaitAndError)
{
    StringStream* stream = construct();
    String onFulfilled, onRejected;

    ScriptPromise promise = stream->ready(scriptState());
    promise.then(createCaptor(&onFulfilled), createCaptor(&onRejected));
    EXPECT_EQ(ReadableStream::Waiting, stream->stateInternal());
    EXPECT_TRUE(stream->isPulling());
    stream->error(DOMException::create(NotFoundError, "hello, error"));
    EXPECT_EQ(ReadableStream::Errored, stream->stateInternal());
    EXPECT_TRUE(stream->isPulling());
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    isolate()->RunMicrotasks();
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_EQ("NotFoundError: hello, error", onRejected);
}

TEST_F(ReadableStreamTest, ErrorAndEnqueue)
{
    StringStream* stream = construct();

    stream->error(DOMException::create(NotFoundError, "error"));
    EXPECT_EQ(ReadableStream::Errored, stream->stateInternal());

    bool result = stream->enqueue("hello");
    EXPECT_FALSE(result);
    EXPECT_EQ(ReadableStream::Errored, stream->stateInternal());
}

TEST_F(ReadableStreamTest, CloseAndEnqueue)
{
    StringStream* stream = construct();

    stream->close();
    EXPECT_EQ(ReadableStream::Closed, stream->stateInternal());

    bool result = stream->enqueue("hello");
    EXPECT_FALSE(result);
    EXPECT_EQ(ReadableStream::Closed, stream->stateInternal());
}

TEST_F(ReadableStreamTest, EnqueueAndWait)
{
    StringStream* stream = construct();
    String onFulfilled, onRejected;
    EXPECT_EQ(ReadableStream::Waiting, stream->stateInternal());

    bool result = stream->enqueue("hello");
    EXPECT_TRUE(result);
    EXPECT_EQ(ReadableStream::Readable, stream->stateInternal());

    stream->ready(scriptState()).then(createCaptor(&onFulfilled), createCaptor(&onRejected));
    EXPECT_EQ(ReadableStream::Readable, stream->stateInternal());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    isolate()->RunMicrotasks();
    EXPECT_EQ(ReadableStream::Readable, stream->stateInternal());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_EQ("undefined", onFulfilled);
    EXPECT_TRUE(onRejected.isNull());

    stream->error(DOMException::create(AbortError, "done"));
}

TEST_F(ReadableStreamTest, WaitAndEnqueue)
{
    StringStream* stream = construct();
    String onFulfilled, onRejected;
    EXPECT_EQ(ReadableStream::Waiting, stream->stateInternal());

    stream->ready(scriptState()).then(createCaptor(&onFulfilled), createCaptor(&onRejected));
    isolate()->RunMicrotasks();

    EXPECT_EQ(ReadableStream::Waiting, stream->stateInternal());
    EXPECT_TRUE(stream->isPulling());
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    bool result = stream->enqueue("hello");
    EXPECT_TRUE(result);
    EXPECT_EQ(ReadableStream::Readable, stream->stateInternal());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    isolate()->RunMicrotasks();
    EXPECT_EQ("undefined", onFulfilled);
    EXPECT_TRUE(onRejected.isNull());

    stream->error(DOMException::create(AbortError, "done"));
}

TEST_F(ReadableStreamTest, WaitAndEnqueueAndError)
{
    StringStream* stream = construct();
    String onFulfilled, onRejected;
    EXPECT_EQ(ReadableStream::Waiting, stream->stateInternal());

    ScriptPromise promise = stream->ready(scriptState());
    promise.then(createCaptor(&onFulfilled), createCaptor(&onRejected));
    isolate()->RunMicrotasks();

    EXPECT_EQ(ReadableStream::Waiting, stream->stateInternal());
    EXPECT_TRUE(stream->isPulling());
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    bool result = stream->enqueue("hello");
    EXPECT_TRUE(result);
    EXPECT_EQ(ReadableStream::Readable, stream->stateInternal());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    isolate()->RunMicrotasks();
    EXPECT_EQ("undefined", onFulfilled);
    EXPECT_TRUE(onRejected.isNull());

    stream->error(DOMException::create(NotFoundError, "error"));
    EXPECT_EQ(ReadableStream::Errored, stream->stateInternal());

    EXPECT_NE(promise, stream->ready(scriptState()));
}

TEST_F(ReadableStreamTest, CloseWhenWaiting)
{
    String onWaitFulfilled, onWaitRejected;
    String onClosedFulfilled, onClosedRejected;

    StringStream* stream = construct();

    EXPECT_EQ(ReadableStream::Waiting, stream->stateInternal());
    stream->ready(scriptState()).then(createCaptor(&onWaitFulfilled), createCaptor(&onWaitRejected));
    stream->closed(scriptState()).then(createCaptor(&onClosedFulfilled), createCaptor(&onClosedRejected));

    isolate()->RunMicrotasks();
    EXPECT_TRUE(onWaitFulfilled.isNull());
    EXPECT_TRUE(onWaitRejected.isNull());
    EXPECT_TRUE(onClosedFulfilled.isNull());
    EXPECT_TRUE(onClosedRejected.isNull());

    stream->close();
    EXPECT_EQ(ReadableStream::Closed, stream->stateInternal());
    isolate()->RunMicrotasks();
    EXPECT_EQ("undefined", onWaitFulfilled);
    EXPECT_TRUE(onWaitRejected.isNull());
    EXPECT_EQ("undefined", onClosedFulfilled);
    EXPECT_TRUE(onClosedRejected.isNull());
}

TEST_F(ReadableStreamTest, CloseWhenErrored)
{
    String onFulfilled, onRejected;
    StringStream* stream = construct();
    EXPECT_EQ(ReadableStream::Waiting, stream->stateInternal());
    stream->closed(scriptState()).then(createCaptor(&onFulfilled), createCaptor(&onRejected));

    stream->error(DOMException::create(NotFoundError, "error"));
    stream->close();

    EXPECT_EQ(ReadableStream::Errored, stream->stateInternal());
    isolate()->RunMicrotasks();

    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_EQ("NotFoundError: error", onRejected);
}

TEST_F(ReadableStreamTest, ReadWhenWaiting)
{
    StringStream* stream = construct();
    EXPECT_EQ(ReadableStream::Waiting, stream->stateInternal());
    EXPECT_FALSE(m_exceptionState.hadException());

    stream->read(scriptState(), m_exceptionState);
    EXPECT_EQ(ReadableStream::Waiting, stream->stateInternal());
    EXPECT_TRUE(m_exceptionState.hadException());
    EXPECT_EQ(V8TypeError, m_exceptionState.code());
    EXPECT_EQ("read is called while state is waiting", m_exceptionState.message());

    stream->error(DOMException::create(AbortError, "done"));
}

TEST_F(ReadableStreamTest, ReadWhenClosed)
{
    StringStream* stream = construct();
    stream->close();

    EXPECT_EQ(ReadableStream::Closed, stream->stateInternal());
    EXPECT_FALSE(m_exceptionState.hadException());

    stream->read(scriptState(), m_exceptionState);
    EXPECT_EQ(ReadableStream::Closed, stream->stateInternal());
    EXPECT_TRUE(m_exceptionState.hadException());
    EXPECT_EQ(V8TypeError, m_exceptionState.code());
    EXPECT_EQ("read is called while state is closed", m_exceptionState.message());
}

TEST_F(ReadableStreamTest, ReadWhenErrored)
{
    // DOMException values specified in the spec are different from enum values
    // defined in ExceptionCode.h.
    const int notFoundExceptionCode = 8;
    StringStream* stream = construct();
    stream->error(DOMException::create(NotFoundError, "error"));

    EXPECT_EQ(ReadableStream::Errored, stream->stateInternal());
    EXPECT_FALSE(m_exceptionState.hadException());

    stream->read(scriptState(), m_exceptionState);
    EXPECT_EQ(ReadableStream::Errored, stream->stateInternal());
    EXPECT_TRUE(m_exceptionState.hadException());
    EXPECT_EQ(notFoundExceptionCode, m_exceptionState.code());
    EXPECT_EQ("error", m_exceptionState.message());
}

TEST_F(ReadableStreamTest, EnqueuedAndRead)
{
    StringStream* stream = construct();
    String onFulfilled, onRejected;
    Checkpoint checkpoint;

    {
        InSequence s;
        EXPECT_CALL(checkpoint, Call(0));
        EXPECT_CALL(*m_underlyingSource, pullSource()).Times(1);
        EXPECT_CALL(checkpoint, Call(1));
    }

    stream->enqueue("hello");
    ScriptPromise promise = stream->ready(scriptState());
    EXPECT_EQ(ReadableStream::Readable, stream->stateInternal());
    EXPECT_FALSE(stream->isPulling());

    checkpoint.Call(0);
    String chunk;
    EXPECT_TRUE(stream->read(scriptState(), m_exceptionState).toString(chunk));
    checkpoint.Call(1);
    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ("hello", chunk);
    EXPECT_EQ(ReadableStream::Waiting, stream->stateInternal());
    EXPECT_TRUE(stream->isPulling());
    EXPECT_FALSE(stream->isDraining());

    ScriptPromise newPromise = stream->ready(scriptState());
    newPromise.then(createCaptor(&onFulfilled), createCaptor(&onRejected));
    isolate()->RunMicrotasks();
    EXPECT_NE(promise, newPromise);
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    stream->error(DOMException::create(AbortError, "done"));
    isolate()->RunMicrotasks();
}

TEST_F(ReadableStreamTest, EnqueueTwiceAndRead)
{
    StringStream* stream = construct();
    Checkpoint checkpoint;

    {
        InSequence s;
        EXPECT_CALL(checkpoint, Call(0));
        EXPECT_CALL(*m_underlyingSource, pullSource()).Times(1);
        EXPECT_CALL(checkpoint, Call(1));
    }

    EXPECT_TRUE(stream->enqueue("hello"));
    EXPECT_TRUE(stream->enqueue("bye"));
    ScriptPromise promise = stream->ready(scriptState());
    EXPECT_EQ(ReadableStream::Readable, stream->stateInternal());
    EXPECT_FALSE(stream->isPulling());

    String chunk;
    checkpoint.Call(0);
    EXPECT_TRUE(stream->read(scriptState(), m_exceptionState).toString(chunk));
    checkpoint.Call(1);
    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ("hello", chunk);
    EXPECT_EQ(ReadableStream::Readable, stream->stateInternal());
    EXPECT_TRUE(stream->isPulling());
    EXPECT_FALSE(stream->isDraining());

    ScriptPromise newPromise = stream->ready(scriptState());
    EXPECT_EQ(promise, newPromise);

    stream->error(DOMException::create(AbortError, "done"));
}

TEST_F(ReadableStreamTest, ReadQueue)
{
    StringStream* stream = construct();
    Checkpoint checkpoint;

    {
        InSequence s;
        EXPECT_CALL(checkpoint, Call(0));
        EXPECT_CALL(*m_underlyingSource, pullSource()).Times(1);
        EXPECT_CALL(checkpoint, Call(1));
    }

    Deque<std::pair<String, size_t>> queue;

    EXPECT_TRUE(stream->enqueue("hello"));
    EXPECT_TRUE(stream->enqueue("bye"));
    EXPECT_EQ(ReadableStream::Readable, stream->stateInternal());
    EXPECT_FALSE(stream->isPulling());

    String chunk;
    checkpoint.Call(0);
    stream->readInternal(queue);
    checkpoint.Call(1);
    ASSERT_EQ(2u, queue.size());

    EXPECT_EQ(std::make_pair(String("hello"), static_cast<size_t>(5)), queue[0]);
    EXPECT_EQ(std::make_pair(String("bye"), static_cast<size_t>(3)), queue[1]);

    EXPECT_EQ(ReadableStream::Waiting, stream->stateInternal());
    EXPECT_TRUE(stream->isPulling());
    EXPECT_FALSE(stream->isDraining());
}

TEST_F(ReadableStreamTest, CloseWhenReadable)
{
    StringStream* stream = construct();
    String onClosedFulfilled, onClosedRejected;

    stream->closed(scriptState()).then(createCaptor(&onClosedFulfilled), createCaptor(&onClosedRejected));
    EXPECT_TRUE(stream->enqueue("hello"));
    EXPECT_TRUE(stream->enqueue("bye"));
    stream->close();
    EXPECT_FALSE(stream->enqueue("should be ignored"));

    ScriptPromise promise = stream->ready(scriptState());
    EXPECT_EQ(ReadableStream::Readable, stream->stateInternal());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_TRUE(stream->isDraining());

    String chunk;
    EXPECT_TRUE(stream->read(scriptState(), m_exceptionState).toString(chunk));
    EXPECT_EQ("hello", chunk);
    EXPECT_EQ(promise, stream->ready(scriptState()));

    isolate()->RunMicrotasks();

    EXPECT_EQ(ReadableStream::Readable, stream->stateInternal());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_TRUE(stream->isDraining());

    EXPECT_TRUE(stream->read(scriptState(), m_exceptionState).toString(chunk));
    EXPECT_EQ("bye", chunk);
    EXPECT_FALSE(m_exceptionState.hadException());

    EXPECT_EQ(promise, stream->ready(scriptState()));

    EXPECT_EQ(ReadableStream::Closed, stream->stateInternal());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_TRUE(stream->isDraining());

    EXPECT_TRUE(onClosedFulfilled.isNull());
    EXPECT_TRUE(onClosedRejected.isNull());

    isolate()->RunMicrotasks();
    EXPECT_EQ("undefined", onClosedFulfilled);
    EXPECT_TRUE(onClosedRejected.isNull());
}

TEST_F(ReadableStreamTest, CancelWhenClosed)
{
    StringStream* stream = construct();
    String onFulfilled, onRejected;
    stream->close();
    EXPECT_EQ(ReadableStream::Closed, stream->stateInternal());

    ScriptPromise promise = stream->cancel(scriptState(), ScriptValue());
    EXPECT_EQ(ReadableStream::Closed, stream->stateInternal());

    promise.then(createCaptor(&onFulfilled), createCaptor(&onRejected));
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    isolate()->RunMicrotasks();
    EXPECT_EQ("undefined", onFulfilled);
    EXPECT_TRUE(onRejected.isNull());
}

TEST_F(ReadableStreamTest, CancelWhenErrored)
{
    StringStream* stream = construct();
    String onFulfilled, onRejected;
    stream->error(DOMException::create(NotFoundError, "error"));
    EXPECT_EQ(ReadableStream::Errored, stream->stateInternal());

    ScriptPromise promise = stream->cancel(scriptState(), ScriptValue());
    EXPECT_EQ(ReadableStream::Errored, stream->stateInternal());

    promise.then(createCaptor(&onFulfilled), createCaptor(&onRejected));
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    isolate()->RunMicrotasks();
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_EQ("NotFoundError: error", onRejected);
}

TEST_F(ReadableStreamTest, CancelWhenWaiting)
{
    StringStream* stream = construct();
    String onFulfilled, onRejected;
    ScriptValue reason(scriptState(), v8String(scriptState()->isolate(), "reason"));
    ScriptPromise promise = ScriptPromise::cast(scriptState(), v8String(scriptState()->isolate(), "hello"));

    {
        InSequence s;
        EXPECT_CALL(*m_underlyingSource, cancelSource(scriptState(), reason)).WillOnce(Return(promise));
    }

    EXPECT_EQ(ReadableStream::Waiting, stream->stateInternal());
    ScriptPromise ready = stream->ready(scriptState());
    EXPECT_NE(promise, stream->cancel(scriptState(), reason));
    EXPECT_EQ(ReadableStream::Closed, stream->stateInternal());

    ready.then(createCaptor(&onFulfilled), createCaptor(&onRejected));
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    isolate()->RunMicrotasks();
    EXPECT_EQ("undefined", onFulfilled);
    EXPECT_TRUE(onRejected.isNull());
}

TEST_F(ReadableStreamTest, CancelWhenReadable)
{
    StringStream* stream = construct();
    String onFulfilled, onRejected;
    String onCancelFulfilled, onCancelRejected;
    ScriptValue reason(scriptState(), v8String(scriptState()->isolate(), "reason"));
    ScriptPromise promise = ScriptPromise::cast(scriptState(), v8String(scriptState()->isolate(), "hello"));

    {
        InSequence s;
        EXPECT_CALL(*m_underlyingSource, cancelSource(scriptState(), reason)).WillOnce(Return(promise));
    }

    stream->enqueue("hello");
    ScriptPromise ready = stream->ready(scriptState());
    EXPECT_EQ(ReadableStream::Readable, stream->stateInternal());

    ScriptPromise cancelResult = stream->cancel(scriptState(), reason);
    cancelResult.then(createCaptor(&onCancelFulfilled), createCaptor(&onCancelRejected));

    EXPECT_NE(promise, cancelResult);
    EXPECT_EQ(ReadableStream::Closed, stream->stateInternal());

    EXPECT_EQ(stream->ready(scriptState()), ready);

    EXPECT_TRUE(onCancelFulfilled.isNull());
    EXPECT_TRUE(onCancelRejected.isNull());

    isolate()->RunMicrotasks();
    EXPECT_EQ("undefined", onCancelFulfilled);
    EXPECT_TRUE(onCancelRejected.isNull());
}

TEST_F(ReadableStreamTest, ReadableArrayBufferCompileTest)
{
    // This test tests if ReadableStreamImpl<DOMArrayBuffer> can be
    // instantiated.
    new ReadableStreamImpl<ReadableStreamChunkTypeTraits<DOMArrayBuffer>>(scriptState()->executionContext(), m_underlyingSource);
}

TEST_F(ReadableStreamTest, BackpressureOnEnqueueing)
{
    auto strategy = MockStrategy::create();
    Checkpoint checkpoint;

    StringStream* stream = construct(strategy);
    EXPECT_EQ(ReadableStream::Waiting, stream->stateInternal());

    {
        InSequence s;
        EXPECT_CALL(checkpoint, Call(0));
        EXPECT_CALL(*strategy, size(String("hello"), stream)).WillOnce(Return(1));
        EXPECT_CALL(*strategy, shouldApplyBackpressure(1, stream)).WillOnce(Return(false));
        EXPECT_CALL(checkpoint, Call(1));
        EXPECT_CALL(checkpoint, Call(2));
        EXPECT_CALL(*strategy, size(String("world"), stream)).WillOnce(Return(2));
        EXPECT_CALL(*strategy, shouldApplyBackpressure(3, stream)).WillOnce(Return(true));
        EXPECT_CALL(checkpoint, Call(3));
    }
    checkpoint.Call(0);
    bool result = stream->enqueue("hello");
    checkpoint.Call(1);
    EXPECT_TRUE(result);

    checkpoint.Call(2);
    result = stream->enqueue("world");
    checkpoint.Call(3);
    EXPECT_FALSE(result);

    stream->error(DOMException::create(AbortError, "done"));
}

TEST_F(ReadableStreamTest, BackpressureOnReading)
{
    auto strategy = MockStrategy::create();
    Checkpoint checkpoint;

    StringStream* stream = construct(strategy);
    EXPECT_EQ(ReadableStream::Waiting, stream->stateInternal());

    {
        InSequence s;
        EXPECT_CALL(*strategy, size(String("hello"), stream)).WillOnce(Return(2));
        EXPECT_CALL(*strategy, shouldApplyBackpressure(2, stream)).WillOnce(Return(false));
        EXPECT_CALL(*strategy, size(String("world"), stream)).WillOnce(Return(3));
        EXPECT_CALL(*strategy, shouldApplyBackpressure(5, stream)).WillOnce(Return(false));

        EXPECT_CALL(checkpoint, Call(0));
        EXPECT_CALL(*strategy, shouldApplyBackpressure(3, stream)).WillOnce(Return(false));
        EXPECT_CALL(*m_underlyingSource, pullSource()).Times(1);
        EXPECT_CALL(checkpoint, Call(1));
        // shouldApplyBackpressure and pullSource are not called because the
        // stream is pulling.
        EXPECT_CALL(checkpoint, Call(2));
        EXPECT_CALL(*strategy, size(String("foo"), stream)).WillOnce(Return(4));
        EXPECT_CALL(*strategy, shouldApplyBackpressure(4, stream)).WillOnce(Return(true));
        EXPECT_CALL(*strategy, size(String("bar"), stream)).WillOnce(Return(5));
        EXPECT_CALL(*strategy, shouldApplyBackpressure(9, stream)).WillOnce(Return(true));
        EXPECT_CALL(checkpoint, Call(3));
        EXPECT_CALL(*strategy, shouldApplyBackpressure(5, stream)).WillOnce(Return(true));
        EXPECT_CALL(checkpoint, Call(4));
    }
    stream->enqueue("hello");
    stream->enqueue("world");

    String chunk;
    checkpoint.Call(0);
    EXPECT_TRUE(stream->read(scriptState(), m_exceptionState).toString(chunk));
    EXPECT_EQ("hello", chunk);
    checkpoint.Call(1);
    EXPECT_TRUE(stream->read(scriptState(), m_exceptionState).toString(chunk));
    EXPECT_EQ("world", chunk);
    checkpoint.Call(2);
    stream->enqueue("foo");
    stream->enqueue("bar");
    checkpoint.Call(3);
    EXPECT_TRUE(stream->read(scriptState(), m_exceptionState).toString(chunk));
    EXPECT_EQ("foo", chunk);
    checkpoint.Call(4);

    stream->error(DOMException::create(AbortError, "done"));
}

// Note: Detailed tests are on ExclusiveStreamReaderTest.
TEST_F(ReadableStreamTest, ExclusiveStreamReader)
{
    StringStream* stream = construct();
    ExclusiveStreamReader* reader = stream->getReader(m_exceptionState);

    ASSERT_TRUE(reader);
    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_TRUE(reader->isActive());
    EXPECT_TRUE(stream->isLockedTo(reader));

    ExclusiveStreamReader* another = stream->getReader(m_exceptionState);
    ASSERT_EQ(nullptr, another);
    EXPECT_TRUE(m_exceptionState.hadException());
    EXPECT_TRUE(reader->isActive());
    EXPECT_TRUE(stream->isLockedTo(reader));
}

} // namespace blink

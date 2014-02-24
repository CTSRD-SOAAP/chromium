/*
 * Copyright (C) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
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

#include "config.h"
#include "bindings/v8/V8Binding.h"

#include "V8Element.h"
#include "V8NodeFilter.h"
#include "V8Window.h"
#include "V8WorkerGlobalScope.h"
#include "V8XPathNSResolver.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/V8NodeFilterCondition.h"
#include "bindings/v8/V8ObjectConstructor.h"
#include "bindings/v8/V8WindowShell.h"
#include "bindings/v8/WorkerScriptController.h"
#include "bindings/v8/custom/V8CustomXPathNSResolver.h"
#include "core/dom/Element.h"
#include "core/dom/NodeFilter.h"
#include "core/dom/QualifiedName.h"
#include "core/inspector/BindingVisitors.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/frame/Frame.h"
#include "core/page/Settings.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/xml/XPathNSResolver.h"
#include "wtf/ArrayBufferContents.h"
#include "wtf/MainThread.h"
#include "wtf/MathExtras.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Threading.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringBuffer.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

v8::Handle<v8::Value> setDOMException(int exceptionCode, v8::Isolate* isolate)
{
    return V8ThrowException::throwDOMException(exceptionCode, isolate);
}

v8::Handle<v8::Value> setDOMException(int exceptionCode, const String& message, v8::Isolate* isolate)
{
    return V8ThrowException::throwDOMException(exceptionCode, message, isolate);
}

v8::Handle<v8::Value> throwError(V8ErrorType errorType, const String& message, v8::Isolate* isolate)
{
    return V8ThrowException::throwError(errorType, message, isolate);
}

v8::Handle<v8::Value> throwError(v8::Handle<v8::Value> exception, v8::Isolate* isolate)
{
    return V8ThrowException::throwError(exception, isolate);
}

v8::Handle<v8::Value> throwUninformativeAndGenericTypeError(v8::Isolate* isolate)
{
    return V8ThrowException::throwTypeError(String(), isolate);
}

v8::Handle<v8::Value> throwTypeError(const String& message, v8::Isolate* isolate)
{
    return V8ThrowException::throwTypeError(message, isolate);
}

class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
    virtual void* Allocate(size_t size) OVERRIDE
    {
        void* data;
        WTF::ArrayBufferContents::allocateMemory(size, WTF::ArrayBufferContents::ZeroInitialize, data);
        return data;
    }

    virtual void* AllocateUninitialized(size_t size) OVERRIDE
    {
        void* data;
        WTF::ArrayBufferContents::allocateMemory(size, WTF::ArrayBufferContents::DontInitialize, data);
        return data;
    }

    virtual void Free(void* data, size_t size) OVERRIDE
    {
        WTF::ArrayBufferContents::freeMemory(data, size);
    }
};

v8::ArrayBuffer::Allocator* v8ArrayBufferAllocator()
{
    DEFINE_STATIC_LOCAL(ArrayBufferAllocator, arrayBufferAllocator, ());
    return &arrayBufferAllocator;
}

Vector<v8::Handle<v8::Value> > toVectorOfArguments(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    Vector<v8::Handle<v8::Value> > result;
    size_t length = info.Length();
    for (size_t i = 0; i < length; ++i)
        result.append(info[i]);
    return result;
}

PassRefPtr<NodeFilter> toNodeFilter(v8::Handle<v8::Value> callback, v8::Isolate* isolate)
{
    RefPtr<NodeFilter> filter = NodeFilter::create();

    // FIXME: Should pass in appropriate creationContext
    v8::Handle<v8::Object> filterWrapper = toV8(filter, v8::Handle<v8::Object>(), isolate).As<v8::Object>();

    RefPtr<NodeFilterCondition> condition = V8NodeFilterCondition::create(callback, filterWrapper, isolate);
    filter->setCondition(condition.release());

    return filter.release();
}

const int32_t kMaxInt32 = 0x7fffffff;
const int32_t kMinInt32 = -kMaxInt32 - 1;
const uint32_t kMaxUInt32 = 0xffffffff;
const int64_t kJSMaxInteger = 0x20000000000000LL - 1; // 2^53 - 1, maximum integer exactly representable in ECMAScript.

static double enforceRange(double x, double minimum, double maximum, bool& ok)
{
    if (std::isnan(x) || std::isinf(x)) {
        ok = false;
        return 0;
    }
    x = trunc(x);
    if (x < minimum || x > maximum) {
        ok = false;
        return 0;
    }
    return x;
}

template <typename T>
struct IntTypeLimits {
};

template <>
struct IntTypeLimits<int8_t> {
    static const int8_t minValue = -128;
    static const int8_t maxValue = 127;
    static const unsigned numberOfValues = 256; // 2^8
};

template <>
struct IntTypeLimits<uint8_t> {
    static const uint8_t maxValue = 255;
    static const unsigned numberOfValues = 256; // 2^8
};

template <>
struct IntTypeLimits<int16_t> {
    static const short minValue = -32768;
    static const short maxValue = 32767;
    static const unsigned numberOfValues = 65536; // 2^16
};

template <>
struct IntTypeLimits<uint16_t> {
    static const unsigned short maxValue = 65535;
    static const unsigned numberOfValues = 65536; // 2^16
};

template <typename T>
static inline T toSmallerInt(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, bool& ok)
{
    typedef IntTypeLimits<T> LimitsTrait;
    ok = true;

    // Fast case. The value is already a 32-bit integer in the right range.
    if (value->IsInt32()) {
        int32_t result = value->Int32Value();
        if (result >= LimitsTrait::minValue && result <= LimitsTrait::maxValue)
            return static_cast<T>(result);
        if (configuration == EnforceRange) {
            ok = false;
            return 0;
        }
        result %= LimitsTrait::numberOfValues;
        return static_cast<T>(result > LimitsTrait::maxValue ? result - LimitsTrait::numberOfValues : result);
    }

    // Can the value be converted to a number?
    v8::Local<v8::Number> numberObject = value->ToNumber();
    if (numberObject.IsEmpty()) {
        ok = false;
        return 0;
    }

    if (configuration == EnforceRange)
        return enforceRange(numberObject->Value(), LimitsTrait::minValue, LimitsTrait::maxValue, ok);

    double numberValue = numberObject->Value();
    if (std::isnan(numberValue) || std::isinf(numberValue) || !numberValue)
        return 0;

    numberValue = numberValue < 0 ? -floor(fabs(numberValue)) : floor(fabs(numberValue));
    numberValue = fmod(numberValue, LimitsTrait::numberOfValues);

    return static_cast<T>(numberValue > LimitsTrait::maxValue ? numberValue - LimitsTrait::numberOfValues : numberValue);
}

template <typename T>
static inline T toSmallerUInt(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, bool& ok)
{
    typedef IntTypeLimits<T> LimitsTrait;
    ok = true;

    // Fast case. The value is a 32-bit signed integer - possibly positive?
    if (value->IsInt32()) {
        int32_t result = value->Int32Value();
        if (result >= 0 && result <= LimitsTrait::maxValue)
            return static_cast<T>(result);
        if (configuration == EnforceRange) {
            ok = false;
            return 0;
        }
        return static_cast<T>(result);
    }

    // Can the value be converted to a number?
    v8::Local<v8::Number> numberObject = value->ToNumber();
    if (numberObject.IsEmpty()) {
        ok = false;
        return 0;
    }

    if (configuration == EnforceRange)
        return enforceRange(numberObject->Value(), 0, LimitsTrait::maxValue, ok);

    // Does the value convert to nan or to an infinity?
    double numberValue = numberObject->Value();
    if (std::isnan(numberValue) || std::isinf(numberValue) || !numberValue)
        return 0;

    numberValue = numberValue < 0 ? -floor(fabs(numberValue)) : floor(fabs(numberValue));
    return static_cast<T>(fmod(numberValue, LimitsTrait::numberOfValues));
}

int8_t toInt8(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, bool& ok)
{
    return toSmallerInt<int8_t>(value, configuration, ok);
}

uint8_t toUInt8(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, bool& ok)
{
    return toSmallerUInt<uint8_t>(value, configuration, ok);
}

int16_t toInt16(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, bool& ok)
{
    return toSmallerInt<int16_t>(value, configuration, ok);
}

uint16_t toUInt16(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, bool& ok)
{
    return toSmallerUInt<uint16_t>(value, configuration, ok);
}

int32_t toInt32(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, bool& ok)
{
    ok = true;

    // Fast case. The value is already a 32-bit integer.
    if (value->IsInt32())
        return value->Int32Value();

    // Can the value be converted to a number?
    v8::Local<v8::Number> numberObject = value->ToNumber();
    if (numberObject.IsEmpty()) {
        ok = false;
        return 0;
    }

    if (configuration == EnforceRange)
        return enforceRange(numberObject->Value(), kMinInt32, kMaxInt32, ok);

    // Does the value convert to nan or to an infinity?
    double numberValue = numberObject->Value();
    if (std::isnan(numberValue) || std::isinf(numberValue))
        return 0;
    return numberObject->Int32Value();
}

uint32_t toUInt32(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, bool& ok)
{
    ok = true;

    // Fast case. The value is already a 32-bit unsigned integer.
    if (value->IsUint32())
        return value->Uint32Value();

    // Fast case. The value is a 32-bit signed integer - possibly positive?
    if (value->IsInt32()) {
        int32_t result = value->Int32Value();
        if (result >= 0)
            return result;
        if (configuration == EnforceRange) {
            ok = false;
            return 0;
        }
        return result;
    }

    // Can the value be converted to a number?
    v8::Local<v8::Number> numberObject = value->ToNumber();
    if (numberObject.IsEmpty()) {
        ok = false;
        return 0;
    }

    if (configuration == EnforceRange)
        return enforceRange(numberObject->Value(), 0, kMaxUInt32, ok);

    // Does the value convert to nan or to an infinity?
    double numberValue = numberObject->Value();
    if (std::isnan(numberValue) || std::isinf(numberValue))
        return 0;
    return numberObject->Uint32Value();
}

int64_t toInt64(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, bool& ok)
{
    ok = true;

    // Fast case. The value is a 32-bit integer.
    if (value->IsInt32())
        return value->Int32Value();

    // Can the value be converted to a number?
    v8::Local<v8::Number> numberObject = value->ToNumber();
    if (numberObject.IsEmpty()) {
        ok = false;
        return 0;
    }

    double x = numberObject->Value();

    if (configuration == EnforceRange)
        return enforceRange(x, -kJSMaxInteger, kJSMaxInteger, ok);

    // NaNs and +/-Infinity should be 0, otherwise modulo 2^64.
    unsigned long long integer;
    doubleToInteger(x, integer);
    return integer;
}

uint64_t toUInt64(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, bool& ok)
{
    ok = true;

    // Fast case. The value is a 32-bit unsigned integer.
    if (value->IsUint32())
        return value->Uint32Value();

    // Fast case. The value is a 32-bit integer.
    if (value->IsInt32()) {
        int32_t result = value->Int32Value();
        if (result >= 0)
            return result;
        if (configuration == EnforceRange) {
            ok = false;
            return 0;
        }
        return result;
    }

    // Can the value be converted to a number?
    v8::Local<v8::Number> numberObject = value->ToNumber();
    if (numberObject.IsEmpty()) {
        ok = false;
        return 0;
    }

    double x = numberObject->Value();

    if (configuration == EnforceRange)
        return enforceRange(x, 0, kJSMaxInteger, ok);

    // NaNs and +/-Infinity should be 0, otherwise modulo 2^64.
    unsigned long long integer;
    doubleToInteger(x, integer);
    return integer;
}

v8::Handle<v8::FunctionTemplate> createRawTemplate(v8::Isolate* isolate)
{
    v8::HandleScope scope(isolate);
    v8::Local<v8::FunctionTemplate> result = v8::FunctionTemplate::New(V8ObjectConstructor::isValidConstructorMode);
    return scope.Close(result);
}

PassRefPtr<XPathNSResolver> toXPathNSResolver(v8::Handle<v8::Value> value, v8::Isolate* isolate)
{
    RefPtr<XPathNSResolver> resolver;
    if (V8XPathNSResolver::HasInstance(value, isolate, worldType(isolate)))
        resolver = V8XPathNSResolver::toNative(v8::Handle<v8::Object>::Cast(value));
    else if (value->IsObject())
        resolver = V8CustomXPathNSResolver::create(value->ToObject(), isolate);
    return resolver;
}

v8::Handle<v8::Object> toInnerGlobalObject(v8::Handle<v8::Context> context)
{
    return v8::Handle<v8::Object>::Cast(context->Global()->GetPrototype());
}

DOMWindow* toDOMWindow(v8::Handle<v8::Context> context)
{
    v8::Handle<v8::Object> global = context->Global();
    ASSERT(!global.IsEmpty());
    v8::Handle<v8::Object> window = global->FindInstanceInPrototypeChain(V8Window::GetTemplate(context->GetIsolate(), MainWorld));
    if (!window.IsEmpty())
        return V8Window::toNative(window);
    window = global->FindInstanceInPrototypeChain(V8Window::GetTemplate(context->GetIsolate(), IsolatedWorld));
    ASSERT(!window.IsEmpty());
    return V8Window::toNative(window);
}

ExecutionContext* toExecutionContext(v8::Handle<v8::Context> context)
{
    v8::Handle<v8::Object> global = context->Global();
    v8::Handle<v8::Object> windowWrapper = global->FindInstanceInPrototypeChain(V8Window::GetTemplate(context->GetIsolate(), MainWorld));
    if (!windowWrapper.IsEmpty())
        return V8Window::toNative(windowWrapper)->executionContext();
    windowWrapper = global->FindInstanceInPrototypeChain(V8Window::GetTemplate(context->GetIsolate(), IsolatedWorld));
    if (!windowWrapper.IsEmpty())
        return V8Window::toNative(windowWrapper)->executionContext();
    v8::Handle<v8::Object> workerWrapper = global->FindInstanceInPrototypeChain(V8WorkerGlobalScope::GetTemplate(context->GetIsolate(), WorkerWorld));
    if (!workerWrapper.IsEmpty())
        return V8WorkerGlobalScope::toNative(workerWrapper)->executionContext();
    // FIXME: Is this line of code reachable?
    return 0;
}

DOMWindow* activeDOMWindow()
{
    v8::Handle<v8::Context> context = v8::Context::GetCalling();
    if (context.IsEmpty()) {
        // Unfortunately, when processing script from a plug-in, we might not
        // have a calling context. In those cases, we fall back to the
        // entered context.
        context = v8::Context::GetEntered();
    }
    return toDOMWindow(context);
}

ExecutionContext* activeExecutionContext()
{
    v8::Handle<v8::Context> context = v8::Context::GetCalling();
    if (context.IsEmpty()) {
        // Unfortunately, when processing script from a plug-in, we might not
        // have a calling context. In those cases, we fall back to the
        // entered context.
        context = v8::Context::GetEntered();
    }
    return toExecutionContext(context);
}

DOMWindow* firstDOMWindow()
{
    return toDOMWindow(v8::Context::GetEntered());
}

Document* currentDocument()
{
    return toDOMWindow(v8::Context::GetCurrent())->document();
}

Frame* toFrameIfNotDetached(v8::Handle<v8::Context> context)
{
    DOMWindow* window = toDOMWindow(context);
    if (window->isCurrentlyDisplayedInFrame())
        return window->frame();
    // We return 0 here because |context| is detached from the Frame. If we
    // did return |frame| we could get in trouble because the frame could be
    // navigated to another security origin.
    return 0;
}

v8::Local<v8::Context> toV8Context(ExecutionContext* context, DOMWrapperWorld* world)
{
    if (context->isDocument()) {
        ASSERT(world);
        if (Frame* frame = toDocument(context)->frame())
            return frame->script().windowShell(world)->context();
    } else if (context->isWorkerGlobalScope()) {
        ASSERT(!world);
        if (WorkerScriptController* script = toWorkerGlobalScope(context)->script())
            return script->context();
    }
    return v8::Local<v8::Context>();
}

bool handleOutOfMemory()
{
    v8::Local<v8::Context> context = v8::Context::GetCurrent();

    if (!context->HasOutOfMemoryException())
        return false;

    // Warning, error, disable JS for this frame?
    Frame* frame = toFrameIfNotDetached(context);
    if (!frame)
        return true;

    frame->script().clearForOutOfMemory();
    frame->loader().client()->didExhaustMemoryAvailableForScript();

    if (Settings* settings = frame->settings())
        settings->setScriptEnabled(false);

    return true;
}

v8::Local<v8::Value> handleMaxRecursionDepthExceeded(v8::Isolate* isolate)
{
    throwError(v8RangeError, "Maximum call stack size exceeded.", isolate);
    return v8::Local<v8::Value>();
}

void crashIfV8IsDead()
{
    if (v8::V8::IsDead()) {
        // FIXME: We temporarily deal with V8 internal error situations
        // such as out-of-memory by crashing the renderer.
        CRASH();
    }
}

WrapperWorldType worldType(v8::Isolate* isolate)
{
    V8PerIsolateData* data = V8PerIsolateData::from(isolate);
    if (!data->workerDOMDataStore())
        return worldTypeInMainThread(isolate);
    return WorkerWorld;
}

WrapperWorldType worldTypeInMainThread(v8::Isolate* isolate)
{
    if (!DOMWrapperWorld::isolatedWorldsExist())
        return MainWorld;
    ASSERT(!v8::Context::GetEntered().IsEmpty());
    DOMWrapperWorld* isolatedWorld = DOMWrapperWorld::isolatedWorld(v8::Context::GetEntered());
    if (isolatedWorld)
        return IsolatedWorld;
    return MainWorld;
}

DOMWrapperWorld* isolatedWorldForIsolate(v8::Isolate* isolate)
{
    V8PerIsolateData* data = V8PerIsolateData::from(isolate);
    if (data->workerDOMDataStore())
        return 0;
    if (!DOMWrapperWorld::isolatedWorldsExist())
        return 0;
    ASSERT(v8::Context::InContext());
    return DOMWrapperWorld::isolatedWorld(v8::Context::GetCurrent());
}

v8::Local<v8::Value> getHiddenValueFromMainWorldWrapper(v8::Isolate* isolate, ScriptWrappable* wrappable, v8::Handle<v8::String> key)
{
    v8::Local<v8::Object> wrapper = wrappable->newLocalWrapper(isolate);
    return wrapper.IsEmpty() ? v8::Local<v8::Value>() : wrapper->GetHiddenValue(key);
}

static v8::Isolate* mainIsolate = 0;

v8::Isolate* mainThreadIsolate()
{
    ASSERT(mainIsolate);
    ASSERT(isMainThread());
    return mainIsolate;
}

void setMainThreadIsolate(v8::Isolate* isolate)
{
    ASSERT(!mainIsolate);
    ASSERT(isMainThread());
    mainIsolate = isolate;
}

v8::Isolate* toIsolate(ExecutionContext* context)
{
    if (context && context->isDocument())
        return mainThreadIsolate();
    return v8::Isolate::GetCurrent();
}

v8::Isolate* toIsolate(Frame* frame)
{
    return frame->script().isolate();
}

} // namespace WebCore

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/loader/BeaconLoader.h"

#include "core/FetchInitiatorTypeNames.h"
#include "core/dom/DOMArrayBufferView.h"
#include "core/fetch/FetchContext.h"
#include "core/fileapi/File.h"
#include "core/frame/LocalFrame.h"
#include "core/html/DOMFormData.h"
#include "platform/network/FormData.h"
#include "platform/network/ParsedContentType.h"
#include "platform/network/ResourceRequest.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/Functional.h"

namespace blink {

namespace {

class Beacon {
public:
    virtual bool serialize(ResourceRequest&, int, int&) const = 0;
    virtual unsigned long long size() const = 0;

protected:
    static unsigned long long beaconSize(const String&);
    static unsigned long long beaconSize(Blob*);
    static unsigned long long beaconSize(PassRefPtr<DOMArrayBufferView>);
    static unsigned long long beaconSize(PassRefPtrWillBeRawPtr<DOMFormData>);

    static bool serialize(const String&, ResourceRequest&, int, int&);
    static bool serialize(Blob*, ResourceRequest&, int, int&);
    static bool serialize(PassRefPtr<DOMArrayBufferView>, ResourceRequest&, int, int&);
    static bool serialize(PassRefPtrWillBeRawPtr<DOMFormData>, ResourceRequest&, int, int&);
};

template<typename Payload>
class BeaconData final : public Beacon {
public:
    BeaconData(const Payload& data)
        : m_data(data)
    {
    }

    bool serialize(ResourceRequest& request, int allowance, int& payloadLength) const override
    {
        return Beacon::serialize(m_data, request, allowance, payloadLength);
    }

    unsigned long long size() const override
    {
        return beaconSize(m_data);
    }

private:
    const typename WTF::ParamStorageTraits<Payload>::StorageType m_data;
};

} // namespace

class BeaconLoader::Sender {
public:
    static bool send(LocalFrame* frame, int allowance, const KURL& beaconURL, const Beacon& beacon, int& payloadLength)
    {
        unsigned long long entitySize = beacon.size();
        if (allowance > 0 && static_cast<unsigned long long>(allowance) < entitySize)
            return false;

        ResourceRequest request(beaconURL);
        request.setRequestContext(WebURLRequest::RequestContextBeacon);
        request.setHTTPMethod("POST");
        request.setHTTPHeaderField("Cache-Control", "max-age=0");
        request.setAllowStoredCredentials(true);
        frame->loader().fetchContext().addAdditionalRequestHeaders(frame->document(), request, FetchSubresource);
        frame->loader().fetchContext().setFirstPartyForCookies(request);

        payloadLength = entitySize;
        if (!beacon.serialize(request, allowance, payloadLength))
            return false;

        FetchInitiatorInfo initiatorInfo;
        initiatorInfo.name = FetchInitiatorTypeNames::beacon;

        PingLoader::start(frame, request, initiatorInfo);
        return true;
    }
};

bool BeaconLoader::sendBeacon(LocalFrame* frame, int allowance, const KURL& beaconURL, const String& data, int& payloadLength)
{
    BeaconData<decltype(data)> beacon(data);
    return Sender::send(frame, allowance, beaconURL, beacon, payloadLength);
}

bool BeaconLoader::sendBeacon(LocalFrame* frame, int allowance, const KURL& beaconURL, PassRefPtr<DOMArrayBufferView> data, int& payloadLength)
{
    BeaconData<decltype(data)> beacon(data);
    return Sender::send(frame, allowance, beaconURL, beacon, payloadLength);
}

bool BeaconLoader::sendBeacon(LocalFrame* frame, int allowance, const KURL& beaconURL, PassRefPtrWillBeRawPtr<DOMFormData> data, int& payloadLength)
{
    BeaconData<decltype(data)> beacon(data);
    return Sender::send(frame, allowance, beaconURL, beacon, payloadLength);
}

bool BeaconLoader::sendBeacon(LocalFrame* frame, int allowance, const KURL& beaconURL, Blob* data, int& payloadLength)
{
    BeaconData<decltype(data)> beacon(data);
    return Sender::send(frame, allowance, beaconURL, beacon, payloadLength);
}

namespace {

unsigned long long Beacon::beaconSize(const String& data)
{
    return data.sizeInBytes();
}

bool Beacon::serialize(const String& data, ResourceRequest& request, int, int&)
{
    RefPtr<FormData> entityBody = FormData::create(data.utf8());
    request.setHTTPBody(entityBody);
    request.setHTTPContentType("text/plain;charset=UTF-8");
    return true;
}

unsigned long long Beacon::beaconSize(Blob* data)
{
    return data->size();
}

bool Beacon::serialize(Blob* data, ResourceRequest& request, int, int&)
{
    ASSERT(data);
    RefPtr<FormData> entityBody = FormData::create();
    if (data->hasBackingFile())
        entityBody->appendFile(toFile(data)->path());
    else
        entityBody->appendBlob(data->uuid(), data->blobDataHandle());

    request.setHTTPBody(entityBody.release());

    AtomicString contentType;
    const String& blobType = data->type();
    if (!blobType.isEmpty() && isValidContentType(blobType))
        request.setHTTPContentType(AtomicString(contentType));

    return true;
}

unsigned long long Beacon::beaconSize(PassRefPtr<DOMArrayBufferView> data)
{
    return data->byteLength();
}

bool Beacon::serialize(PassRefPtr<DOMArrayBufferView> data, ResourceRequest& request, int, int&)
{
    ASSERT(data);
    RefPtr<FormData> entityBody = FormData::create(data->baseAddress(), data->byteLength());
    request.setHTTPBody(entityBody.release());

    // FIXME: a reasonable choice, but not in the spec; should it give a default?
    AtomicString contentType = AtomicString("application/octet-stream");
    request.setHTTPContentType(contentType);

    return true;
}

unsigned long long Beacon::beaconSize(PassRefPtrWillBeRawPtr<DOMFormData> data)
{
    // DOMFormData's size cannot be determined until serialized.
    return 0;
}

bool Beacon::serialize(PassRefPtrWillBeRawPtr<DOMFormData> data, ResourceRequest& request, int allowance, int& payloadLength)
{
    ASSERT(data);
    RefPtr<FormData> entityBody = data->createMultiPartFormData();
    unsigned long long entitySize = entityBody->sizeInBytes();
    if (allowance > 0 && static_cast<unsigned long long>(allowance) < entitySize)
        return false;

    AtomicString contentType = AtomicString("multipart/form-data; boundary=", AtomicString::ConstructFromLiteral) + entityBody->boundary().data();
    request.setHTTPBody(entityBody.release());
    request.setHTTPContentType(contentType);

    payloadLength = entitySize;
    return true;
}

} // namespace

} // namespace blink

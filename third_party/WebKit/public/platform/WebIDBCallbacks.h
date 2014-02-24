/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebIDBCallbacks_h
#define WebIDBCallbacks_h

#include "WebCommon.h"
#include "WebString.h"
#include "WebVector.h"

namespace WebKit {

class WebData;
class WebIDBCursor;
class WebIDBDatabase;
class WebIDBDatabaseError;
class WebIDBIndex;
class WebIDBKey;
class WebIDBKeyPath;
struct WebIDBMetadata;

class WebIDBCallbacks {
public:
    virtual ~WebIDBCallbacks() { }

    enum DataLoss {
        DataLossNone = 0,
        DataLossTotal = 1
    };

    // For classes that follow the PImpl pattern, pass a const reference.
    // For the rest, pass ownership to the callee via a pointer.
    virtual void onError(const WebIDBDatabaseError&) { BLINK_ASSERT_NOT_REACHED(); }
    virtual void onSuccess(const WebVector<WebString>&) { BLINK_ASSERT_NOT_REACHED(); }
    virtual void onSuccess(WebIDBCursor*, const WebIDBKey& key, const WebIDBKey& primaryKey, const WebData&) { BLINK_ASSERT_NOT_REACHED(); }
    virtual void onSuccess(WebIDBDatabase*) { BLINK_ASSERT_NOT_REACHED(); }
    virtual void onSuccess(WebIDBDatabase*, const WebIDBMetadata&) { BLINK_ASSERT_NOT_REACHED(); }
    virtual void onSuccess(const WebIDBKey&) { BLINK_ASSERT_NOT_REACHED(); }
    virtual void onSuccess(const WebData&) { BLINK_ASSERT_NOT_REACHED(); }
    virtual void onSuccess(const WebData&, const WebIDBKey&, const WebIDBKeyPath&) { BLINK_ASSERT_NOT_REACHED(); }
    virtual void onSuccess(long long) { BLINK_ASSERT_NOT_REACHED(); }
    virtual void onSuccess() { BLINK_ASSERT_NOT_REACHED(); }
    virtual void onSuccess(const WebIDBKey& key, const WebIDBKey& primaryKey, const WebData&) { BLINK_ASSERT_NOT_REACHED(); }
    virtual void onBlocked(long long oldVersion) { BLINK_ASSERT_NOT_REACHED(); }
    // FIXME: Remove this function after the associated chrome patch
    // (https://codereview.chromium.org/47513010/) has landed.
    virtual void onUpgradeNeeded(long long oldVersion, WebIDBDatabase* database, const WebIDBMetadata& metadata, DataLoss dataLoss)
    {
        onUpgradeNeeded(oldVersion, database, metadata, dataLoss, WebString());
    }
    virtual void onUpgradeNeeded(long long oldVersion, WebIDBDatabase*, const WebIDBMetadata&, DataLoss dataLoss, WebString dataLossMessage) { BLINK_ASSERT_NOT_REACHED(); }
};

} // namespace WebKit

#endif // WebIDBCallbacks_h

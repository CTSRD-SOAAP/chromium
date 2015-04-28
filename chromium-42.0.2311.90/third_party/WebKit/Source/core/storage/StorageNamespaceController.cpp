// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/storage/StorageNamespaceController.h"

#include "core/page/StorageClient.h"
#include "core/storage/StorageNamespace.h"

namespace blink {

const char* StorageNamespaceController::supplementName()
{
    return "StorageNamespaceController";
}

StorageNamespaceController::StorageNamespaceController(StorageClient* client)
    : m_client(client)
{
}

StorageNamespaceController::~StorageNamespaceController()
{
}

StorageNamespace* StorageNamespaceController::sessionStorage(bool optionalCreate)
{
    if (!m_sessionStorage && optionalCreate)
        m_sessionStorage = m_client->createSessionStorageNamespace();
    return m_sessionStorage.get();
}

void StorageNamespaceController::provideStorageNamespaceTo(Page& page, StorageClient* client)
{
    StorageNamespaceController::provideTo(page, supplementName(), adoptPtrWillBeNoop(new StorageNamespaceController(client)));
}

} // namespace blink

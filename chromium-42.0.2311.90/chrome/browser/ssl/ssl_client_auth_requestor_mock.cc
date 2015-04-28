// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_client_auth_requestor_mock.h"

#include "net/ssl/ssl_cert_request_info.h"
#include "net/url_request/url_request.h"

SSLClientAuthRequestorMock::SSLClientAuthRequestorMock(
    net::URLRequest* request,
    const scoped_refptr<net::SSLCertRequestInfo>& cert_request_info)
    : cert_request_info_(cert_request_info) {
}

SSLClientAuthRequestorMock::~SSLClientAuthRequestorMock() {}

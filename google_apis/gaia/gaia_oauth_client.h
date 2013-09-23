// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_OAUTH_CLIENT_H_
#define GOOGLE_APIS_GAIA_GAIA_OAUTH_CLIENT_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/values.h"

namespace net {
class URLRequestContextGetter;
}

// A helper class to get and refresh OAuth tokens given an authorization code.
// Also exposes utility methods for fetching user email and token owner.
// Supports one request at a time; for parallel requests, create multiple
// instances.
namespace gaia {

struct OAuthClientInfo {
  std::string client_id;
  std::string client_secret;
  std::string redirect_uri;
};

class GaiaOAuthClient {
 public:
  const static int kUrlFetcherId;

  class Delegate {
   public:
    // Invoked on a successful response to the GetTokensFromAuthCode request.
    virtual void OnGetTokensResponse(const std::string& refresh_token,
                                     const std::string& access_token,
                                     int expires_in_seconds) {}
    // Invoked on a successful response to the RefreshToken request.
    virtual void OnRefreshTokenResponse(const std::string& access_token,
                                        int expires_in_seconds) {}
    // Invoked on a successful response to the GetUserInfo request.
    virtual void OnGetUserInfoResponse(const std::string& user_email) {}
    // Invoked on a successful response to the GetTokenInfo request.
    virtual void OnGetTokenInfoResponse(
        scoped_ptr<DictionaryValue> token_info) {}
    // Invoked when there is an OAuth error with one of the requests.
    virtual void OnOAuthError() = 0;
    // Invoked when there is a network error or upon receiving an invalid
    // response. This is invoked when the maximum number of retries have been
    // exhausted. If max_retries is -1, this is never invoked.
    virtual void OnNetworkError(int response_code) = 0;

   protected:
    virtual ~Delegate() {}
  };

  GaiaOAuthClient(net::URLRequestContextGetter* context_getter);
  ~GaiaOAuthClient();

  // In the below methods, |max_retries| specifies the maximum number of times
  // we should retry on a network error in invalid response. This does not
  // apply in the case of an OAuth error (i.e. there was something wrong with
  // the input arguments). Setting |max_retries| to -1 implies infinite retries.
  void GetTokensFromAuthCode(const OAuthClientInfo& oauth_client_info,
                             const std::string& auth_code,
                             int max_retries,
                             Delegate* delegate);
  void RefreshToken(const OAuthClientInfo& oauth_client_info,
                    const std::string& refresh_token,
                    const std::vector<std::string>& scopes,
                    int max_retries,
                    Delegate* delegate);
  void GetUserInfo(const std::string& oauth_access_token,
                   int max_retries,
                   Delegate* delegate);
  void GetTokenInfo(const std::string& oauth_access_token,
                    int max_retries,
                    Delegate* delegate);

 private:
  // The guts of the implementation live in this class.
  class Core;
  scoped_refptr<Core> core_;
  DISALLOW_COPY_AND_ASSIGN(GaiaOAuthClient);
};
}

#endif  // GOOGLE_APIS_GAIA_GAIA_OAUTH_CLIENT_H_

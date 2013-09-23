// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for GaiaOAuthClient.

#include <string>
#include <vector>

#include "base/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/test/base/testing_profile.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Pointee;
using ::testing::SaveArg;

namespace {

// Responds as though OAuth returned from the server.
class MockOAuthFetcher : public net::TestURLFetcher {
 public:
  MockOAuthFetcher(int response_code,
                   int max_failure_count,
                   bool complete_immediately,
                   const GURL& url,
                   const std::string& results,
                   net::URLFetcher::RequestType request_type,
                   net::URLFetcherDelegate* d)
      : net::TestURLFetcher(0, url, d),
        max_failure_count_(max_failure_count),
        current_failure_count_(0),
        complete_immediately_(complete_immediately) {
    set_url(url);
    set_response_code(response_code);
    SetResponseString(results);
  }

  virtual ~MockOAuthFetcher() { }

  virtual void Start() OVERRIDE {
    if ((GetResponseCode() != net::HTTP_OK) && (max_failure_count_ != -1) &&
        (current_failure_count_ == max_failure_count_)) {
      set_response_code(net::HTTP_OK);
    }

    net::URLRequestStatus::Status code = net::URLRequestStatus::SUCCESS;
    if (GetResponseCode() != net::HTTP_OK) {
      code = net::URLRequestStatus::FAILED;
      current_failure_count_++;
    }
    set_status(net::URLRequestStatus(code, 0));

    if (complete_immediately_)
      delegate()->OnURLFetchComplete(this);
  }

  void Finish() {
    ASSERT_FALSE(complete_immediately_);
    delegate()->OnURLFetchComplete(this);
  }

 private:
  int max_failure_count_;
  int current_failure_count_;
  bool complete_immediately_;
  DISALLOW_COPY_AND_ASSIGN(MockOAuthFetcher);
};

class MockOAuthFetcherFactory : public net::URLFetcherFactory,
                                public net::ScopedURLFetcherFactory {
 public:
  MockOAuthFetcherFactory()
      : net::ScopedURLFetcherFactory(this),
        response_code_(net::HTTP_OK),
        complete_immediately_(true) {
  }
  virtual ~MockOAuthFetcherFactory() {}
  virtual net::URLFetcher* CreateURLFetcher(
      int id,
      const GURL& url,
      net::URLFetcher::RequestType request_type,
      net::URLFetcherDelegate* d) OVERRIDE {
    url_fetcher_ = new MockOAuthFetcher(
        response_code_,
        max_failure_count_,
        complete_immediately_,
        url,
        results_,
        request_type,
        d);
    return url_fetcher_;
  }
  void set_response_code(int response_code) {
    response_code_ = response_code;
  }
  void set_max_failure_count(int count) {
    max_failure_count_ = count;
  }
  void set_results(const std::string& results) {
    results_ = results;
  }
  MockOAuthFetcher* get_url_fetcher() {
    return url_fetcher_;
  }
  void set_complete_immediately(bool complete_immediately) {
    complete_immediately_ = complete_immediately;
  }
 private:
  MockOAuthFetcher* url_fetcher_;
  int response_code_;
  bool complete_immediately_;
  int max_failure_count_;
  std::string results_;
  DISALLOW_COPY_AND_ASSIGN(MockOAuthFetcherFactory);
};

const std::string kTestAccessToken = "1/fFAGRNJru1FTz70BzhT3Zg";
const std::string kTestRefreshToken =
    "1/6BMfW9j53gdGImsixUH6kU5RsR4zwI9lUVX-tqf8JXQ";
const std::string kTestUserEmail = "a_user@gmail.com";
const int kTestExpiresIn = 3920;

const std::string kDummyGetTokensResult =
  "{\"access_token\":\"" + kTestAccessToken + "\","
  "\"expires_in\":" + base::IntToString(kTestExpiresIn) + ","
  "\"refresh_token\":\"" + kTestRefreshToken + "\"}";

const std::string kDummyRefreshTokenResult =
  "{\"access_token\":\"" + kTestAccessToken + "\","
  "\"expires_in\":" + base::IntToString(kTestExpiresIn) + "}";

const std::string kDummyUserInfoResult =
  "{\"email\":\"" + kTestUserEmail + "\"}";

const std::string kDummyTokenInfoResult =
  "{\"issued_to\": \"1234567890.apps.googleusercontent.com\","
  "\"audience\": \"1234567890.apps.googleusercontent.com\","
  "\"scope\": \"https://googleapis.com/oauth2/v2/tokeninfo\","
  "\"expires_in\":" + base::IntToString(kTestExpiresIn) + "}";
}

namespace gaia {

class GaiaOAuthClientTest : public testing::Test {
 public:
  GaiaOAuthClientTest() {}
  virtual void SetUp() OVERRIDE {
    client_info_.client_id = "test_client_id";
    client_info_.client_secret = "test_client_secret";
    client_info_.redirect_uri = "test_redirect_uri";
  };

  TestingProfile profile_;
 protected:
  base::MessageLoop message_loop_;
  OAuthClientInfo client_info_;
};

class MockGaiaOAuthClientDelegate : public gaia::GaiaOAuthClient::Delegate {
 public:
  MockGaiaOAuthClientDelegate() {}
  ~MockGaiaOAuthClientDelegate() {}

  MOCK_METHOD3(OnGetTokensResponse, void(const std::string& refresh_token,
                                         const std::string& access_token,
                                         int expires_in_seconds));
  MOCK_METHOD2(OnRefreshTokenResponse, void(const std::string& access_token,
                                            int expires_in_seconds));
  MOCK_METHOD1(OnGetUserInfoResponse, void(const std::string& user_email));
  MOCK_METHOD0(OnOAuthError, void());
  MOCK_METHOD1(OnNetworkError, void(int response_code));

  // gMock doesn't like methods that take or return scoped_ptr.  A
  // work-around is to create a mock method that takes a raw ptr, and
  // override the problematic method to call through to it.
  // https://groups.google.com/a/chromium.org/d/msg/chromium-dev/01sDxsJ1OYw/I_S0xCBRF2oJ
  MOCK_METHOD1(OnGetTokenInfoResponsePtr,
               void(const DictionaryValue* token_info));
  virtual void OnGetTokenInfoResponse(scoped_ptr<DictionaryValue> token_info)
      OVERRIDE {
    token_info_.reset(token_info.release());
    OnGetTokenInfoResponsePtr(token_info_.get());
  }

 private:
  scoped_ptr<DictionaryValue> token_info_;
  DISALLOW_COPY_AND_ASSIGN(MockGaiaOAuthClientDelegate);
};

TEST_F(GaiaOAuthClientTest, NetworkFailure) {
  int response_code = net::HTTP_INTERNAL_SERVER_ERROR;

  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnNetworkError(response_code))
      .Times(1);

  MockOAuthFetcherFactory factory;
  factory.set_response_code(response_code);
  factory.set_max_failure_count(4);

  GaiaOAuthClient auth(profile_.GetRequestContext());
  auth.GetTokensFromAuthCode(client_info_, "auth_code", 2, &delegate);
}

TEST_F(GaiaOAuthClientTest, NetworkFailureRecover) {
  int response_code = net::HTTP_INTERNAL_SERVER_ERROR;

  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnGetTokensResponse(kTestRefreshToken, kTestAccessToken,
      kTestExpiresIn)).Times(1);

  MockOAuthFetcherFactory factory;
  factory.set_response_code(response_code);
  factory.set_max_failure_count(4);
  factory.set_results(kDummyGetTokensResult);

  GaiaOAuthClient auth(profile_.GetRequestContext());
  auth.GetTokensFromAuthCode(client_info_, "auth_code", -1, &delegate);
}

TEST_F(GaiaOAuthClientTest, OAuthFailure) {
  int response_code = net::HTTP_BAD_REQUEST;

  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnOAuthError()).Times(1);

  MockOAuthFetcherFactory factory;
  factory.set_response_code(response_code);
  factory.set_max_failure_count(-1);
  factory.set_results(kDummyGetTokensResult);

  GaiaOAuthClient auth(profile_.GetRequestContext());
  auth.GetTokensFromAuthCode(client_info_, "auth_code", -1, &delegate);
}


TEST_F(GaiaOAuthClientTest, GetTokensSuccess) {
  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnGetTokensResponse(kTestRefreshToken, kTestAccessToken,
      kTestExpiresIn)).Times(1);

  MockOAuthFetcherFactory factory;
  factory.set_results(kDummyGetTokensResult);

  GaiaOAuthClient auth(profile_.GetRequestContext());
  auth.GetTokensFromAuthCode(client_info_, "auth_code", -1, &delegate);
}

TEST_F(GaiaOAuthClientTest, RefreshTokenSuccess) {
  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnRefreshTokenResponse(kTestAccessToken,
      kTestExpiresIn)).Times(1);

  MockOAuthFetcherFactory factory;
  factory.set_results(kDummyRefreshTokenResult);
  factory.set_complete_immediately(false);

  GaiaOAuthClient auth(profile_.GetRequestContext());
  auth.RefreshToken(client_info_, "refresh_token", std::vector<std::string>(),
                    -1, &delegate);
  EXPECT_THAT(factory.get_url_fetcher()->upload_data(),
              Not(HasSubstr("scope")));
  factory.get_url_fetcher()->Finish();
}

TEST_F(GaiaOAuthClientTest, RefreshTokenDownscopingSuccess) {
  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnRefreshTokenResponse(kTestAccessToken,
      kTestExpiresIn)).Times(1);

  MockOAuthFetcherFactory factory;
  factory.set_results(kDummyRefreshTokenResult);
  factory.set_complete_immediately(false);

  GaiaOAuthClient auth(profile_.GetRequestContext());
  auth.RefreshToken(client_info_, "refresh_token",
                    std::vector<std::string>(1, "scope4test"), -1, &delegate);
  EXPECT_THAT(factory.get_url_fetcher()->upload_data(),
              HasSubstr("&scope=scope4test"));
  factory.get_url_fetcher()->Finish();
}


TEST_F(GaiaOAuthClientTest, GetUserInfo) {
  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnGetUserInfoResponse(kTestUserEmail)).Times(1);

  MockOAuthFetcherFactory factory;
  factory.set_results(kDummyUserInfoResult);

  GaiaOAuthClient auth(profile_.GetRequestContext());
  auth.GetUserInfo("access_token", 1, &delegate);
}

TEST_F(GaiaOAuthClientTest, GetTokenInfo) {
  const DictionaryValue* captured_result;

  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnGetTokenInfoResponsePtr(_))
      .WillOnce(SaveArg<0>(&captured_result));

  MockOAuthFetcherFactory factory;
  factory.set_results(kDummyTokenInfoResult);

  GaiaOAuthClient auth(profile_.GetRequestContext());
  auth.GetTokenInfo("access_token", 1, &delegate);

  std::string issued_to;
  ASSERT_TRUE(captured_result->GetString("issued_to",  &issued_to));
  ASSERT_EQ("1234567890.apps.googleusercontent.com", issued_to);
}

}  // namespace gaia

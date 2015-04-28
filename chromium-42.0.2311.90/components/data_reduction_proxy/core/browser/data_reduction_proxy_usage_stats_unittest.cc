// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_usage_stats.h"

#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/prefs/testing_pref_service.h"
#include "base/test/histogram_tester.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/request_priority.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::MockRead;
using net::MockWrite;
using testing::Return;

namespace data_reduction_proxy {

namespace {

class DataReductionProxyParamsMock :
    public DataReductionProxyParams {
 public:
  DataReductionProxyParamsMock() :
      DataReductionProxyParams(0) {}
  virtual ~DataReductionProxyParamsMock() {}

  MOCK_CONST_METHOD2(
      IsDataReductionProxy,
      bool(const net::HostPortPair& host_port_pair,
           DataReductionProxyTypeInfo* proxy_info));
  MOCK_CONST_METHOD2(
      WasDataReductionProxyUsed,
      bool(const net::URLRequest*,
           DataReductionProxyTypeInfo* proxy_info));

 private:
  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyParamsMock);
};

const std::string kBody = "hello";
const std::string kNextBody = "hello again";
const std::string kErrorBody = "bad";

}  // namespace

class DataReductionProxyUsageStatsTest : public testing::Test {
 public:
  DataReductionProxyUsageStatsTest()
      : context_(true) {
    context_.Init();

    // The |test_job_factory_| takes ownership of the interceptor.
    test_job_interceptor_ = new net::TestJobInterceptor();
    EXPECT_TRUE(test_job_factory_.SetProtocolHandler(url::kHttpScheme,
                                                     test_job_interceptor_));

    context_.set_job_factory(&test_job_factory_);

    test_context_.reset(
        new DataReductionProxyTestContext(
            DataReductionProxyParams::kAllowed |
                DataReductionProxyParams::kFallbackAllowed |
                DataReductionProxyParams::kPromoAllowed,
            TestDataReductionProxyParams::HAS_EVERYTHING &
                ~TestDataReductionProxyParams::HAS_DEV_ORIGIN &
                ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN,
            DataReductionProxyTestContext::DEFAULT_TEST_CONTEXT_OPTIONS));
    mock_url_request_ = context_.CreateRequest(GURL(), net::IDLE, &delegate_,
                                               NULL);
  }

  scoped_ptr<net::URLRequest> CreateURLRequestWithResponseHeaders(
      const GURL& url,
      const std::string& raw_response_headers) {
    scoped_ptr<net::URLRequest> fake_request = context_.CreateRequest(
        url, net::IDLE, &delegate_, NULL);

    // Create a test job that will fill in the given response headers for the
    // |fake_request|.
    scoped_refptr<net::URLRequestTestJob> test_job(
        new net::URLRequestTestJob(fake_request.get(),
                                   context_.network_delegate(),
                                   raw_response_headers, std::string(), true));

    // Configure the interceptor to use the test job to handle the next request.
    test_job_interceptor_->set_main_intercept_job(test_job.get());
    fake_request->Start();
    test_context_->RunUntilIdle();

    EXPECT_TRUE(fake_request->response_headers() != NULL);
    return fake_request.Pass();
  }

  bool IsUnreachable() const {
    return test_context_->settings()->IsDataReductionProxyUnreachable();
  }

 protected:
  net::TestURLRequestContext context_;
  net::TestDelegate delegate_;
  DataReductionProxyParamsMock mock_params_;
  scoped_ptr<net::URLRequest> mock_url_request_;
  // |test_job_interceptor_| is owned by |test_job_factory_|.
  net::TestJobInterceptor* test_job_interceptor_;
  net::URLRequestJobFactoryImpl test_job_factory_;
  scoped_ptr<DataReductionProxyTestContext> test_context_;
};

TEST_F(DataReductionProxyUsageStatsTest, IsDataReductionProxyUnreachable) {
  net::ProxyServer fallback_proxy_server =
      net::ProxyServer::FromURI("foo.com", net::ProxyServer::SCHEME_HTTP);
  data_reduction_proxy::DataReductionProxyTypeInfo proxy_info;
  struct TestCase {
    bool fallback_proxy_server_is_data_reduction_proxy;
    bool was_proxy_used;
    bool is_unreachable;
  };
  const TestCase test_cases[] = {
    {
      false,
      false,
      false
    },
    {
      false,
      true,
      false
    },
    {
      true,
      true,
      false
    },
    {
      true,
      false,
      true
    }
  };
  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    TestCase test_case = test_cases[i];

    EXPECT_CALL(mock_params_, IsDataReductionProxy(testing::_, testing::_))
        .WillRepeatedly(testing::Return(
            test_case.fallback_proxy_server_is_data_reduction_proxy));
    EXPECT_CALL(mock_params_,
                WasDataReductionProxyUsed(mock_url_request_.get(), testing::_))
        .WillRepeatedly(testing::Return(test_case.was_proxy_used));

    scoped_ptr<DataReductionProxyUsageStats> usage_stats(
        new DataReductionProxyUsageStats(
            &mock_params_,
            test_context_->data_reduction_proxy_service()->GetWeakPtr(),
            test_context_->task_runner()));

    usage_stats->OnProxyFallback(fallback_proxy_server,
                                 net::ERR_PROXY_CONNECTION_FAILED);
    usage_stats->OnUrlRequestCompleted(mock_url_request_.get(), false);
    test_context_->RunUntilIdle();

    EXPECT_EQ(test_case.is_unreachable, IsUnreachable());
  }
}

TEST_F(DataReductionProxyUsageStatsTest, ProxyUnreachableThenReachable) {
  net::ProxyServer fallback_proxy_server =
      net::ProxyServer::FromURI("foo.com", net::ProxyServer::SCHEME_HTTP);
  scoped_ptr<DataReductionProxyUsageStats> usage_stats(
      new DataReductionProxyUsageStats(
          &mock_params_,
          test_context_->data_reduction_proxy_service()->GetWeakPtr(),
          test_context_->task_runner()));
  EXPECT_CALL(mock_params_, IsDataReductionProxy(testing::_, testing::_))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(mock_params_,
              WasDataReductionProxyUsed(mock_url_request_.get(), testing::_))
      .WillOnce(testing::Return(true));

  // proxy falls back
  usage_stats->OnProxyFallback(fallback_proxy_server,
                               net::ERR_PROXY_CONNECTION_FAILED);
  test_context_->RunUntilIdle();
  EXPECT_TRUE(IsUnreachable());

  // proxy succeeds
  usage_stats->OnUrlRequestCompleted(mock_url_request_.get(), false);
  test_context_->RunUntilIdle();
  EXPECT_FALSE(IsUnreachable());
}

TEST_F(DataReductionProxyUsageStatsTest, ProxyReachableThenUnreachable) {
  net::ProxyServer fallback_proxy_server =
      net::ProxyServer::FromURI("foo.com", net::ProxyServer::SCHEME_HTTP);
  scoped_ptr<DataReductionProxyUsageStats> usage_stats(
      new DataReductionProxyUsageStats(
          &mock_params_,
          test_context_->data_reduction_proxy_service()->GetWeakPtr(),
          test_context_->task_runner()));
  EXPECT_CALL(mock_params_,
              WasDataReductionProxyUsed(mock_url_request_.get(), testing::_))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(mock_params_, IsDataReductionProxy(testing::_, testing::_))
      .WillRepeatedly(testing::Return(true));

  // Proxy succeeds.
  usage_stats->OnUrlRequestCompleted(mock_url_request_.get(), false);
  test_context_->RunUntilIdle();
  EXPECT_FALSE(IsUnreachable());

  // Then proxy falls back indefinitely.
  usage_stats->OnProxyFallback(fallback_proxy_server,
                               net::ERR_PROXY_CONNECTION_FAILED);
  usage_stats->OnProxyFallback(fallback_proxy_server,
                               net::ERR_PROXY_CONNECTION_FAILED);
  usage_stats->OnProxyFallback(fallback_proxy_server,
                               net::ERR_PROXY_CONNECTION_FAILED);
  usage_stats->OnProxyFallback(fallback_proxy_server,
                               net::ERR_PROXY_CONNECTION_FAILED);
  test_context_->RunUntilIdle();
  EXPECT_TRUE(IsUnreachable());
}

TEST_F(DataReductionProxyUsageStatsTest,
       DetectAndRecordMissingViaHeaderResponseCode) {
  const std::string kPrimaryHistogramName =
      "DataReductionProxy.MissingViaHeader.ResponseCode.Primary";
  const std::string kFallbackHistogramName =
      "DataReductionProxy.MissingViaHeader.ResponseCode.Fallback";

  struct TestCase {
    bool is_primary;
    const char* headers;
    int expected_primary_sample;   // -1 indicates no expected sample.
    int expected_fallback_sample;  // -1 indicates no expected sample.
  };
  const TestCase test_cases[] = {
    {
      true,
      "HTTP/1.1 200 OK\n"
      "Via: 1.1 Chrome-Compression-Proxy\n",
      -1,
      -1
    },
    {
      false,
      "HTTP/1.1 200 OK\n"
      "Via: 1.1 Chrome-Compression-Proxy\n",
      -1,
      -1
    },
    {
      true,
      "HTTP/1.1 200 OK\n",
      200,
      -1
    },
    {
      false,
      "HTTP/1.1 200 OK\n",
      -1,
      200
    },
    {
      true,
      "HTTP/1.1 304 Not Modified\n",
      304,
      -1
    },
    {
      false,
      "HTTP/1.1 304 Not Modified\n",
      -1,
      304
    },
    {
      true,
      "HTTP/1.1 404 Not Found\n",
      404,
      -1
    },
    {
      false,
      "HTTP/1.1 404 Not Found\n",
      -1,
      404
    }
  };

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    base::HistogramTester histogram_tester;
    std::string raw_headers(test_cases[i].headers);
    HeadersToRaw(&raw_headers);
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders(raw_headers));

    DataReductionProxyUsageStats::DetectAndRecordMissingViaHeaderResponseCode(
        test_cases[i].is_primary, headers.get());

    if (test_cases[i].expected_primary_sample == -1) {
      histogram_tester.ExpectTotalCount(kPrimaryHistogramName, 0);
    } else {
      histogram_tester.ExpectUniqueSample(
          kPrimaryHistogramName, test_cases[i].expected_primary_sample, 1);
    }

    if (test_cases[i].expected_fallback_sample == -1) {
      histogram_tester.ExpectTotalCount(kFallbackHistogramName, 0);
    } else {
      histogram_tester.ExpectUniqueSample(
          kFallbackHistogramName, test_cases[i].expected_fallback_sample, 1);
    }
  }
}

TEST_F(DataReductionProxyUsageStatsTest, RecordMissingViaHeaderBytes) {
  const std::string k4xxHistogramName =
      "DataReductionProxy.MissingViaHeader.Bytes.4xx";
  const std::string kOtherHistogramName =
      "DataReductionProxy.MissingViaHeader.Bytes.Other";
  const int64 kResponseContentLength = 100;

  struct TestCase {
    bool was_proxy_used;
    const char* headers;
    bool is_4xx_sample_expected;
    bool is_other_sample_expected;
  };
  const TestCase test_cases[] = {
    // Nothing should be recorded for requests that don't use the proxy.
    {
      false,
      "HTTP/1.1 404 Not Found\n",
      false,
      false
    },
    {
      false,
      "HTTP/1.1 200 OK\n",
      false,
      false
    },
    // Nothing should be recorded for responses that have the via header.
    {
      true,
      "HTTP/1.1 404 Not Found\n"
      "Via: 1.1 Chrome-Compression-Proxy\n",
      false,
      false
    },
    {
      true,
      "HTTP/1.1 200 OK\n"
      "Via: 1.1 Chrome-Compression-Proxy\n",
      false,
      false
    },
    // 4xx responses that used the proxy and don't have the via header should be
    // recorded.
    {
      true,
      "HTTP/1.1 404 Not Found\n",
      true,
      false
    },
    {
      true,
      "HTTP/1.1 400 Bad Request\n",
      true,
      false
    },
    {
      true,
      "HTTP/1.1 499 Big Client Error Response Code\n",
      true,
      false
    },
    // Non-4xx responses that used the proxy and don't have the via header
    // should be recorded.
    {
      true,
      "HTTP/1.1 200 OK\n",
      false,
      true
    },
    {
      true,
      "HTTP/1.1 399 Big Redirection Response Code\n",
      false,
      true
    },
    {
      true,
      "HTTP/1.1 500 Internal Server Error\n",
      false,
      true
    }
  };

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    base::HistogramTester histogram_tester;
    scoped_ptr<DataReductionProxyUsageStats> usage_stats(
        new DataReductionProxyUsageStats(
            &mock_params_,
            test_context_->data_reduction_proxy_service()->GetWeakPtr(),
            test_context_->task_runner()));

    std::string raw_headers(test_cases[i].headers);
    HeadersToRaw(&raw_headers);

    scoped_ptr<net::URLRequest> fake_request(
        CreateURLRequestWithResponseHeaders(GURL("http://www.google.com/"),
                                            raw_headers));
    fake_request->set_received_response_content_length(kResponseContentLength);

    EXPECT_CALL(mock_params_,
                WasDataReductionProxyUsed(fake_request.get(), testing::_))
        .WillRepeatedly(Return(test_cases[i].was_proxy_used));

    usage_stats->RecordMissingViaHeaderBytes(*fake_request);

    if (test_cases[i].is_4xx_sample_expected) {
      histogram_tester.ExpectUniqueSample(k4xxHistogramName,
                                          kResponseContentLength, 1);
    } else {
      histogram_tester.ExpectTotalCount(k4xxHistogramName, 0);
    }

    if (test_cases[i].is_other_sample_expected) {
      histogram_tester.ExpectUniqueSample(kOtherHistogramName,
                                          kResponseContentLength, 1);
    } else {
      histogram_tester.ExpectTotalCount(kOtherHistogramName, 0);
    }
  }
}

TEST_F(DataReductionProxyUsageStatsTest, RequestCompletionErrorCodes) {
  const std::string kPrimaryHistogramName =
      "DataReductionProxy.RequestCompletionErrorCodes.Primary";
  const std::string kFallbackHistogramName =
      "DataReductionProxy.RequestCompletionErrorCodes.Fallback";
  const std::string kPrimaryMainFrameHistogramName =
      "DataReductionProxy.RequestCompletionErrorCodes.MainFrame.Primary";
  const std::string kFallbackMainFrameHistogramName =
      "DataReductionProxy.RequestCompletionErrorCodes.MainFrame.Fallback";

  struct TestCase {
    bool was_proxy_used;
    bool is_load_bypass_proxy;
    bool is_fallback;
    bool is_main_frame;
    net::Error net_error;
  };

  const TestCase test_cases[] = {
    {false, true, false, true, net::OK},
    {false, true, false, false, net::ERR_TOO_MANY_REDIRECTS},
    {false, false, false, true, net::OK},
    {false, false, false, false, net::ERR_TOO_MANY_REDIRECTS},
    {true, false, false, true, net::OK},
    {true, false, false, true, net::ERR_TOO_MANY_REDIRECTS},
    {true, false, false, false, net::OK},
    {true, false, false, false, net::ERR_TOO_MANY_REDIRECTS},
    {true, false, true, true, net::OK},
    {true, false, true, true, net::ERR_TOO_MANY_REDIRECTS},
    {true, false, true, false, net::OK},
    {true, false, true, false, net::ERR_TOO_MANY_REDIRECTS}
  };

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    base::HistogramTester histogram_tester;
    scoped_ptr<DataReductionProxyUsageStats> usage_stats(
        new DataReductionProxyUsageStats(
            &mock_params_,
            test_context_->data_reduction_proxy_service()->GetWeakPtr(),
            test_context_->task_runner()));

    std::string raw_headers("HTTP/1.1 200 OK\n"
                            "Via: 1.1 Chrome-Compression-Proxy\n");
    HeadersToRaw(&raw_headers);
    scoped_ptr<net::URLRequest> fake_request(
        CreateURLRequestWithResponseHeaders(GURL("http://www.google.com/"),
                                            raw_headers));
    if (test_cases[i].is_load_bypass_proxy) {
      fake_request->SetLoadFlags(fake_request->load_flags() |
                                 net::LOAD_BYPASS_PROXY);
    }
    if (test_cases[i].is_main_frame) {
      fake_request->SetLoadFlags(fake_request->load_flags() |
                                 net::LOAD_MAIN_FRAME);
    }

    int net_error_int = static_cast<int>(test_cases[i].net_error);
    if (test_cases[i].net_error != net::OK) {
      fake_request->CancelWithError(net_error_int);
    }

    DataReductionProxyTypeInfo proxy_info;
    proxy_info.is_fallback = test_cases[i].is_fallback;
    EXPECT_CALL(mock_params_, WasDataReductionProxyUsed(fake_request.get(),
                                                        testing::NotNull()))
        .WillRepeatedly(testing::DoAll(testing::SetArgPointee<1>(proxy_info),
                                       Return(test_cases[i].was_proxy_used)));

    usage_stats->OnUrlRequestCompleted(fake_request.get(), false);

    if (test_cases[i].was_proxy_used && !test_cases[i].is_load_bypass_proxy &&
        !test_cases[i].is_fallback) {
      histogram_tester.ExpectUniqueSample(
          kPrimaryHistogramName, -net_error_int, 1);
    } else {
      histogram_tester.ExpectTotalCount(kPrimaryHistogramName, 0);
    }
    if (test_cases[i].was_proxy_used && !test_cases[i].is_load_bypass_proxy &&
        test_cases[i].is_fallback) {
      histogram_tester.ExpectUniqueSample(
          kFallbackHistogramName, -net_error_int, 1);
    } else {
      histogram_tester.ExpectTotalCount(kFallbackHistogramName, 0);
    }
    if (test_cases[i].was_proxy_used && !test_cases[i].is_load_bypass_proxy &&
        !test_cases[i].is_fallback && test_cases[i].is_main_frame) {
      histogram_tester.ExpectUniqueSample(
          kPrimaryMainFrameHistogramName, -net_error_int, 1);
    } else {
      histogram_tester.ExpectTotalCount(kPrimaryMainFrameHistogramName, 0);
    }
    if (test_cases[i].was_proxy_used && !test_cases[i].is_load_bypass_proxy &&
        test_cases[i].is_fallback && test_cases[i].is_main_frame) {
      histogram_tester.ExpectUniqueSample(
          kFallbackMainFrameHistogramName, -net_error_int, 1);
    } else {
      histogram_tester.ExpectTotalCount(kFallbackMainFrameHistogramName, 0);
    }
  }
}

// End-to-end tests for the DataReductionProxy.BypassedBytes histograms.
class DataReductionProxyUsageStatsEndToEndTest : public testing::Test {
 public:
  DataReductionProxyUsageStatsEndToEndTest()
      : context_(true) {}

  ~DataReductionProxyUsageStatsEndToEndTest() override {
    test_context_->io_data()->ShutdownOnUIThread();
    test_context_->RunUntilIdle();
  }

  void SetUp() override {
    // Only use the primary data reduction proxy in order to make it easier to
    // test bypassed bytes due to proxy fallbacks. This way, a test just needs
    // to cause one proxy fallback in order for the data reduction proxy to be
    // fully bypassed.
    test_context_.reset(new DataReductionProxyTestContext(
        DataReductionProxyParams::kAllowed,
        TestDataReductionProxyParams::HAS_ORIGIN,
        DataReductionProxyTestContext::SKIP_SETTINGS_INITIALIZATION));
    TestingPrefServiceSimple* simple_prefs = test_context_->pref_service();
    RegisterSimpleProfilePrefs(simple_prefs->registry());

    BooleanPrefMember enabled;
    enabled.Init(prefs::kDataReductionProxyEnabled, simple_prefs);
    enabled.SetValue(true);
    enabled.Destroy();

    test_context_->InitSettings();

    network_delegate_ = test_context_->io_data()->CreateNetworkDelegate(
        scoped_ptr<net::NetworkDelegate>(new net::TestNetworkDelegate()), true);
    context_.set_network_delegate(network_delegate_.get());

    context_.set_client_socket_factory(&mock_socket_factory_);

    job_factory_.reset(new net::URLRequestInterceptingJobFactory(
        scoped_ptr<net::URLRequestJobFactory>(
            new net::URLRequestJobFactoryImpl()),
        test_context_->io_data()->CreateInterceptor().Pass()));
    context_.set_job_factory(job_factory_.get());

    test_context_->io_data()->InitOnUIThread(simple_prefs);
    test_context_->configurator()->Enable(false, true,
                                          params()->origin().ToURI(),
                                          std::string(), std::string());
    test_context_->RunUntilIdle();
  }

  // Create and execute a fake request using the data reduction proxy stack.
  // Passing in nullptr for |retry_response_headers| indicates that the request
  // is not expected to be retried.
  void CreateAndExecuteRequest(const GURL& url,
                               const char* initial_response_headers,
                               const char* initial_response_body,
                               const char* retry_response_headers,
                               const char* retry_response_body) {
    // Support HTTPS URLs.
    net::SSLSocketDataProvider ssl_socket_data_provider(net::ASYNC, net::OK);
    if (url.SchemeIsSecure()) {
      mock_socket_factory_.AddSSLSocketDataProvider(&ssl_socket_data_provider);
    }

    // Prepare for the initial response.
    MockRead initial_data_reads[] = {
        MockRead(initial_response_headers),
        MockRead(initial_response_body),
        MockRead(net::SYNCHRONOUS, net::OK),
    };
    net::StaticSocketDataProvider initial_socket_data_provider(
        initial_data_reads, arraysize(initial_data_reads), nullptr, 0);
    mock_socket_factory_.AddSocketDataProvider(&initial_socket_data_provider);

    // Prepare for the response from retrying the request, if applicable.
    // |retry_data_reads| and |retry_socket_data_provider| are out here so that
    // they stay in scope for when the request is executed.
    std::vector<MockRead> retry_data_reads;
    scoped_ptr<net::StaticSocketDataProvider> retry_socket_data_provider;
    if (retry_response_headers) {
      retry_data_reads.push_back(MockRead(retry_response_headers));
      retry_data_reads.push_back(MockRead(retry_response_body));
      retry_data_reads.push_back(MockRead(net::SYNCHRONOUS, net::OK));

      retry_socket_data_provider.reset(new net::StaticSocketDataProvider(
          &retry_data_reads.front(), retry_data_reads.size(), nullptr, 0));
      mock_socket_factory_.AddSocketDataProvider(
          retry_socket_data_provider.get());
    }

    scoped_ptr<net::URLRequest> request(
        context_.CreateRequest(url, net::IDLE, &delegate_, NULL));
    request->set_method("GET");
    request->SetLoadFlags(net::LOAD_NORMAL);
    request->Start();
    test_context_->RunUntilIdle();
  }

  void set_proxy_service(net::ProxyService* proxy_service) {
    context_.set_proxy_service(proxy_service);
  }

  void set_host_resolver(net::HostResolver* host_resolver) {
    context_.set_host_resolver(host_resolver);
  }

  const DataReductionProxySettings* settings() const {
    return test_context_->settings();
  }

  const DataReductionProxyParams* params() const {
    return test_context_->config()->test_params();
  }

  void ClearBadProxies() {
    context_.proxy_service()->ClearBadProxiesCache();
  }

  void InitializeContext() {
    context_.Init();
  }

  void ExpectOtherBypassedBytesHistogramsEmpty(
      const base::HistogramTester& histogram_tester,
      const std::set<std::string>& excluded_histograms) const {
    const std::string kHistograms[] = {
        "DataReductionProxy.BypassedBytes.NotBypassed",
        "DataReductionProxy.BypassedBytes.SSL",
        "DataReductionProxy.BypassedBytes.LocalBypassRules",
        "DataReductionProxy.BypassedBytes.ProxyOverridden",
        "DataReductionProxy.BypassedBytes.Current",
        "DataReductionProxy.BypassedBytes.ShortAll",
        "DataReductionProxy.BypassedBytes.ShortTriggeringRequest",
        "DataReductionProxy.BypassedBytes.ShortAudioVideo",
        "DataReductionProxy.BypassedBytes.MediumAll",
        "DataReductionProxy.BypassedBytes.MediumTriggeringRequest",
        "DataReductionProxy.BypassedBytes.LongAll",
        "DataReductionProxy.BypassedBytes.LongTriggeringRequest",
        "DataReductionProxy.BypassedBytes.MissingViaHeader4xx",
        "DataReductionProxy.BypassedBytes.MissingViaHeaderOther",
        "DataReductionProxy.BypassedBytes.Malformed407",
        "DataReductionProxy.BypassedBytes.Status500HttpInternalServerError",
        "DataReductionProxy.BypassedBytes.Status502HttpBadGateway",
        "DataReductionProxy.BypassedBytes.Status503HttpServiceUnavailable",
        "DataReductionProxy.BypassedBytes.NetworkErrorOther",
    };

    for (const std::string& histogram : kHistograms) {
      if (excluded_histograms.find(histogram) ==
          excluded_histograms.end()) {
        histogram_tester.ExpectTotalCount(histogram, 0);
      }
    }
  }

  void ExpectOtherBypassedBytesHistogramsEmpty(
      const base::HistogramTester& histogram_tester,
      const std::string& excluded_histogram) const {
    std::set<std::string> excluded_histograms;
    excluded_histograms.insert(excluded_histogram);
    ExpectOtherBypassedBytesHistogramsEmpty(histogram_tester,
                                            excluded_histograms);
  }

  void ExpectOtherBypassedBytesHistogramsEmpty(
      const base::HistogramTester& histogram_tester,
      const std::string& first_excluded_histogram,
      const std::string& second_excluded_histogram) const {
    std::set<std::string> excluded_histograms;
    excluded_histograms.insert(first_excluded_histogram);
    excluded_histograms.insert(second_excluded_histogram);
    ExpectOtherBypassedBytesHistogramsEmpty(histogram_tester,
                                            excluded_histograms);
  }

 private:
  net::TestDelegate delegate_;
  net::MockClientSocketFactory mock_socket_factory_;
  scoped_ptr<DataReductionProxyNetworkDelegate> network_delegate_;
  scoped_ptr<net::URLRequestJobFactory> job_factory_;
  net::TestURLRequestContext context_;
  scoped_ptr<DataReductionProxyTestContext> test_context_;
};

TEST_F(DataReductionProxyUsageStatsEndToEndTest, BypassedBytesNoRetry) {
  struct TestCase {
    GURL url;
    const char* histogram_name;
    const char* initial_response_headers;
  };
  const TestCase test_cases[] = {
    { GURL("http://foo.com"),
      "DataReductionProxy.BypassedBytes.NotBypassed",
      "HTTP/1.1 200 OK\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
    },
    { GURL("https://foo.com"),
      "DataReductionProxy.BypassedBytes.SSL",
      "HTTP/1.1 200 OK\r\n\r\n",
    },
    { GURL("http://localhost"),
      "DataReductionProxy.BypassedBytes.LocalBypassRules",
      "HTTP/1.1 200 OK\r\n\r\n",
    },
  };

  InitializeContext();
  for (const TestCase& test_case : test_cases) {
    ClearBadProxies();
    base::HistogramTester histogram_tester;
    CreateAndExecuteRequest(test_case.url, test_case.initial_response_headers,
                            kBody.c_str(), nullptr, nullptr);

    histogram_tester.ExpectUniqueSample(test_case.histogram_name, kBody.size(),
                                        1);
    ExpectOtherBypassedBytesHistogramsEmpty(histogram_tester,
                                            test_case.histogram_name);
  }
}

TEST_F(DataReductionProxyUsageStatsEndToEndTest, BypassedBytesProxyOverridden) {
  scoped_ptr<net::ProxyService> proxy_service(
      net::ProxyService::CreateFixed("http://test.com:80"));
  set_proxy_service(proxy_service.get());
  InitializeContext();

  base::HistogramTester histogram_tester;
  CreateAndExecuteRequest(GURL("http://foo.com"), "HTTP/1.1 200 OK\r\n\r\n",
                          kBody.c_str(), nullptr, nullptr);

  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.BypassedBytes.ProxyOverridden", kBody.size(), 1);
  ExpectOtherBypassedBytesHistogramsEmpty(
      histogram_tester, "DataReductionProxy.BypassedBytes.ProxyOverridden");
}

TEST_F(DataReductionProxyUsageStatsEndToEndTest, BypassedBytesCurrent) {
  InitializeContext();
  base::HistogramTester histogram_tester;
  CreateAndExecuteRequest(GURL("http://foo.com"),
                          "HTTP/1.1 502 Bad Gateway\r\n"
                          "Via: 1.1 Chrome-Compression-Proxy\r\n"
                          "Chrome-Proxy: block-once\r\n\r\n",
                          kErrorBody.c_str(), "HTTP/1.1 200 OK\r\n\r\n",
                          kBody.c_str());

  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.BypassedBytes.Current", kBody.size(), 1);
  ExpectOtherBypassedBytesHistogramsEmpty(
      histogram_tester, "DataReductionProxy.BypassedBytes.Current");
}

TEST_F(DataReductionProxyUsageStatsEndToEndTest, BypassedBytesShortAudioVideo) {
  InitializeContext();
  base::HistogramTester histogram_tester;
  CreateAndExecuteRequest(GURL("http://foo.com"),
                          "HTTP/1.1 502 Bad Gateway\r\n"
                          "Via: 1.1 Chrome-Compression-Proxy\r\n"
                          "Chrome-Proxy: block=1\r\n\r\n",
                          kErrorBody.c_str(),
                          "HTTP/1.1 200 OK\r\n"
                          "Content-Type: video/mp4\r\n\r\n",
                          kBody.c_str());

  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.BypassedBytes.ShortAudioVideo", kBody.size(), 1);
  ExpectOtherBypassedBytesHistogramsEmpty(
      histogram_tester, "DataReductionProxy.BypassedBytes.ShortAudioVideo");
}

TEST_F(DataReductionProxyUsageStatsEndToEndTest, BypassedBytesExplicitBypass) {
  struct TestCase {
    const char* triggering_histogram_name;
    const char* all_histogram_name;
    const char* initial_response_headers;
  };
  const TestCase test_cases[] = {
    { "DataReductionProxy.BypassedBytes.ShortTriggeringRequest",
      "DataReductionProxy.BypassedBytes.ShortAll",
      "HTTP/1.1 502 Bad Gateway\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Chrome-Proxy: block=1\r\n\r\n",
    },
    { "DataReductionProxy.BypassedBytes.MediumTriggeringRequest",
      "DataReductionProxy.BypassedBytes.MediumAll",
      "HTTP/1.1 502 Bad Gateway\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Chrome-Proxy: block=0\r\n\r\n",
    },
    { "DataReductionProxy.BypassedBytes.LongTriggeringRequest",
      "DataReductionProxy.BypassedBytes.LongAll",
      "HTTP/1.1 502 Bad Gateway\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Chrome-Proxy: block=3600\r\n\r\n",
    },
  };

  InitializeContext();
  for (const TestCase& test_case : test_cases) {
    ClearBadProxies();
    base::HistogramTester histogram_tester;

    CreateAndExecuteRequest(
        GURL("http://foo.com"), test_case.initial_response_headers,
        kErrorBody.c_str(), "HTTP/1.1 200 OK\r\n\r\n", kBody.c_str());
    // The first request caused the proxy to be marked as bad, so this second
    // request should not come through the proxy.
    CreateAndExecuteRequest(GURL("http://bar.com"), "HTTP/1.1 200 OK\r\n\r\n",
                            kNextBody.c_str(), nullptr, nullptr);

    histogram_tester.ExpectUniqueSample(test_case.triggering_histogram_name,
                                        kBody.size(), 1);
    histogram_tester.ExpectUniqueSample(test_case.all_histogram_name,
                                        kNextBody.size(), 1);
    ExpectOtherBypassedBytesHistogramsEmpty(histogram_tester,
                                            test_case.triggering_histogram_name,
                                            test_case.all_histogram_name);
  }
}

TEST_F(DataReductionProxyUsageStatsEndToEndTest,
       BypassedBytesClientSideFallback) {
  struct TestCase {
    const char* histogram_name;
    const char* initial_response_headers;
  };
  const TestCase test_cases[] = {
    { "DataReductionProxy.BypassedBytes.MissingViaHeader4xx",
      "HTTP/1.1 414 Request-URI Too Long\r\n\r\n",
    },
    { "DataReductionProxy.BypassedBytes.MissingViaHeaderOther",
      "HTTP/1.1 200 OK\r\n\r\n",
    },
    { "DataReductionProxy.BypassedBytes.Malformed407",
      "HTTP/1.1 407 Proxy Authentication Required\r\n\r\n",
    },
    { "DataReductionProxy.BypassedBytes.Status500HttpInternalServerError",
      "HTTP/1.1 500 Internal Server Error\r\n\r\n",
    },
    { "DataReductionProxy.BypassedBytes.Status502HttpBadGateway",
      "HTTP/1.1 502 Bad Gateway\r\n\r\n",
    },
    { "DataReductionProxy.BypassedBytes.Status503HttpServiceUnavailable",
      "HTTP/1.1 503 Service Unavailable\r\n\r\n",
    },
  };

  InitializeContext();
  for (const TestCase& test_case : test_cases) {
    ClearBadProxies();
    base::HistogramTester histogram_tester;

    CreateAndExecuteRequest(
        GURL("http://foo.com"), test_case.initial_response_headers,
        kErrorBody.c_str(), "HTTP/1.1 200 OK\r\n\r\n", kBody.c_str());
    // The first request caused the proxy to be marked as bad, so this second
    // request should not come through the proxy.
    CreateAndExecuteRequest(GURL("http://bar.com"), "HTTP/1.1 200 OK\r\n\r\n",
                            kNextBody.c_str(), nullptr, nullptr);

    histogram_tester.ExpectTotalCount(test_case.histogram_name, 2);
    histogram_tester.ExpectBucketCount(test_case.histogram_name, kBody.size(),
                                       1);
    histogram_tester.ExpectBucketCount(test_case.histogram_name,
                                       kNextBody.size(), 1);
    ExpectOtherBypassedBytesHistogramsEmpty(histogram_tester,
                                            test_case.histogram_name);
  }
}

TEST_F(DataReductionProxyUsageStatsEndToEndTest, BypassedBytesNetErrorOther) {
  // Make the data reduction proxy host fail to resolve.
  scoped_ptr<net::MockHostResolver> host_resolver(new net::MockHostResolver());
  host_resolver->rules()->AddSimulatedFailure(
      params()->origin().host_port_pair().host());
  set_host_resolver(host_resolver.get());
  InitializeContext();

  base::HistogramTester histogram_tester;
  CreateAndExecuteRequest(GURL("http://foo.com"), "HTTP/1.1 200 OK\r\n\r\n",
                          kBody.c_str(), nullptr, nullptr);

  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.BypassedBytes.NetworkErrorOther", kBody.size(), 1);
  ExpectOtherBypassedBytesHistogramsEmpty(
      histogram_tester, "DataReductionProxy.BypassedBytes.NetworkErrorOther");
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.BypassOnNetworkErrorPrimary",
      -net::ERR_PROXY_CONNECTION_FAILED, 1);
}

}  // namespace data_reduction_proxy

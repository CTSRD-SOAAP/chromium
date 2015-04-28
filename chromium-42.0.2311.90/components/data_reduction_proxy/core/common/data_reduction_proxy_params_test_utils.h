// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_PARAMS_TEST_UTILS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_PARAMS_TEST_UTILS_H_

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"

namespace base {
class TimeDelta;
}

namespace net {
class ProxyConfig;
class ProxyServer;
class URLRequest;
}

namespace data_reduction_proxy {

class TestDataReductionProxyParams : public DataReductionProxyParams {
 public:
  // Used to emulate having constants defined by the preprocessor.
  enum HasNames {
    HAS_NOTHING = 0x0,
    HAS_DEV_ORIGIN = 0x1,
    HAS_ORIGIN = 0x2,
    HAS_FALLBACK_ORIGIN = 0x4,
    HAS_SSL_ORIGIN = 0x08,
    HAS_ALT_ORIGIN = 0x10,
    HAS_ALT_FALLBACK_ORIGIN = 0x20,
    HAS_PROBE_URL = 0x40,
    HAS_DEV_FALLBACK_ORIGIN = 0x80,
    HAS_EVERYTHING = 0xff,
  };

  TestDataReductionProxyParams(int flags,
                               unsigned int has_definitions);
  bool init_result() const;

  // Overrides from DataReductionProxyParams.
  bool IsBypassedByDataReductionProxyLocalRules(
      const net::URLRequest& request,
      const net::ProxyConfig& data_reduction_proxy_config) const override;
  bool AreDataReductionProxiesBypassed(
      const net::URLRequest& request,
      const net::ProxyConfig& data_reduction_proxy_config,
      base::TimeDelta* min_retry_delay) const override;

  // Once called, the mocked method will repeatedly return |return_value|.
  void MockIsBypassedByDataReductionProxyLocalRules(bool return_value);
  void MockAreDataReductionProxiesBypassed(bool return_value);

  // Test values to replace the values specified in preprocessor defines.
  static std::string DefaultDevOrigin();
  static std::string DefaultDevFallbackOrigin();
  static std::string DefaultOrigin();
  static std::string DefaultFallbackOrigin();
  static std::string DefaultSSLOrigin();
  static std::string DefaultAltOrigin();
  static std::string DefaultAltFallbackOrigin();
  static std::string DefaultProbeURL();

  static std::string FlagOrigin();
  static std::string FlagFallbackOrigin();
  static std::string FlagSSLOrigin();
  static std::string FlagAltOrigin();
  static std::string FlagAltFallbackOrigin();
  static std::string FlagProbeURL();

  void set_origin(const net::ProxyServer& origin);
  void set_fallback_origin(const net::ProxyServer& fallback_origin);

 protected:
  std::string GetDefaultDevOrigin() const override;

  std::string GetDefaultDevFallbackOrigin() const override;

  std::string GetDefaultOrigin() const override;

  std::string GetDefaultFallbackOrigin() const override;

  std::string GetDefaultSSLOrigin() const override;

  std::string GetDefaultAltOrigin() const override;

  std::string GetDefaultAltFallbackOrigin() const override;

  std::string GetDefaultProbeURL() const override;

 private:
  std::string GetDefinition(unsigned int has_def,
                            const std::string& definition) const;

  unsigned int has_definitions_;
  bool init_result_;

  bool mock_is_bypassed_by_data_reduction_proxy_local_rules_;
  bool mock_are_data_reduction_proxies_bypassed_;
  bool is_bypassed_by_data_reduction_proxy_local_rules_return_value_;
  bool are_data_reduction_proxies_bypassed_return_value_;
};
}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_PARAMS_TEST_UTILS_H_


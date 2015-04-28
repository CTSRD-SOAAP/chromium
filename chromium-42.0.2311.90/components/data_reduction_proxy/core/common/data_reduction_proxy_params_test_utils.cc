// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"

namespace {
// Test values to replace the values specified in preprocessor defines.
static const char kDefaultDevOrigin[] = "https://dev.net:443";
static const char kDefaultDevFallbackOrigin[] = "dev.net:80";
static const char kDefaultOrigin[] = "origin.net:80";
static const char kDefaultFallbackOrigin[] = "fallback.net:80";
static const char kDefaultSSLOrigin[] = "ssl.net:1080";
static const char kDefaultAltOrigin[] = "https://alt.net:443";
static const char kDefaultAltFallbackOrigin[] = "altfallback.net:80";
static const char kDefaultProbeURL[] = "http://probe.net/";

static const char kFlagOrigin[] = "https://origin.org:443";
static const char kFlagFallbackOrigin[] = "fallback.org:80";
static const char kFlagSSLOrigin[] = "ssl.org:1080";
static const char kFlagAltOrigin[] = "https://alt.org:443";
static const char kFlagAltFallbackOrigin[] = "altfallback.org:80";
static const char kFlagProbeURL[] = "http://probe.org/";
}

namespace data_reduction_proxy {
TestDataReductionProxyParams::TestDataReductionProxyParams(
    int flags, unsigned int has_definitions)
    : DataReductionProxyParams(flags, false),
      has_definitions_(has_definitions),
      mock_is_bypassed_by_data_reduction_proxy_local_rules_(false),
      mock_are_data_reduction_proxies_bypassed_(false),
      is_bypassed_by_data_reduction_proxy_local_rules_return_value_(false),
      are_data_reduction_proxies_bypassed_return_value_(false) {
    init_result_ = Init(
        flags & DataReductionProxyParams::kAllowed,
        flags & DataReductionProxyParams::kFallbackAllowed,
        flags & DataReductionProxyParams::kAlternativeAllowed,
        flags & DataReductionProxyParams::kAlternativeFallbackAllowed);
  }

bool TestDataReductionProxyParams::init_result() const {
  return init_result_;
}

bool TestDataReductionProxyParams::IsBypassedByDataReductionProxyLocalRules(
    const net::URLRequest& request,
    const net::ProxyConfig& data_reduction_proxy_config) const {
  if (mock_is_bypassed_by_data_reduction_proxy_local_rules_)
    return is_bypassed_by_data_reduction_proxy_local_rules_return_value_;
  return DataReductionProxyParams::IsBypassedByDataReductionProxyLocalRules(
      request, data_reduction_proxy_config);
}

bool TestDataReductionProxyParams::AreDataReductionProxiesBypassed(
    const net::URLRequest& request,
    const net::ProxyConfig& data_reduction_proxy_config,
    base::TimeDelta* min_retry_delay) const {
  if (mock_are_data_reduction_proxies_bypassed_)
    return are_data_reduction_proxies_bypassed_return_value_;
  return DataReductionProxyParams::AreDataReductionProxiesBypassed(
      request, data_reduction_proxy_config, min_retry_delay);
}

void TestDataReductionProxyParams::MockAreDataReductionProxiesBypassed(
    bool return_value) {
  mock_are_data_reduction_proxies_bypassed_ = true;
  are_data_reduction_proxies_bypassed_return_value_ = return_value;
}

void
TestDataReductionProxyParams::MockIsBypassedByDataReductionProxyLocalRules(
    bool return_value) {
  mock_is_bypassed_by_data_reduction_proxy_local_rules_ = true;
  is_bypassed_by_data_reduction_proxy_local_rules_return_value_ = return_value;
}

// Test values to replace the values specified in preprocessor defines.
std::string TestDataReductionProxyParams::DefaultDevOrigin() {
  return kDefaultDevOrigin;
}

std::string TestDataReductionProxyParams::DefaultDevFallbackOrigin() {
  return kDefaultDevFallbackOrigin;
}

std::string TestDataReductionProxyParams::DefaultOrigin() {
  return kDefaultOrigin;
}

std::string TestDataReductionProxyParams::DefaultFallbackOrigin() {
  return kDefaultFallbackOrigin;
}

std::string TestDataReductionProxyParams::DefaultSSLOrigin() {
  return kDefaultSSLOrigin;
}

std::string TestDataReductionProxyParams::DefaultAltOrigin() {
  return kDefaultAltOrigin;
}

std::string TestDataReductionProxyParams::DefaultAltFallbackOrigin() {
  return kDefaultAltFallbackOrigin;
}

std::string TestDataReductionProxyParams::DefaultProbeURL() {
  return kDefaultProbeURL;
}

std::string TestDataReductionProxyParams::FlagOrigin() {
  return kFlagOrigin;
}

std::string TestDataReductionProxyParams::FlagFallbackOrigin() {
  return kFlagFallbackOrigin;
}

std::string TestDataReductionProxyParams::FlagSSLOrigin() {
  return kFlagSSLOrigin;
}

std::string TestDataReductionProxyParams::FlagAltOrigin() {
  return kFlagAltOrigin;
}

std::string TestDataReductionProxyParams::FlagAltFallbackOrigin() {
  return kFlagAltFallbackOrigin;
}

std::string TestDataReductionProxyParams::FlagProbeURL() {
  return kFlagProbeURL;
}

void TestDataReductionProxyParams::set_origin(const net::ProxyServer& origin) {
  origin_ = origin;
}

void TestDataReductionProxyParams::set_fallback_origin(
    const net::ProxyServer& fallback_origin) {
 fallback_origin_ = fallback_origin;
}

std::string TestDataReductionProxyParams::GetDefaultDevOrigin() const {
  return GetDefinition(
      TestDataReductionProxyParams::HAS_DEV_ORIGIN, kDefaultDevOrigin);
}

std::string TestDataReductionProxyParams::GetDefaultDevFallbackOrigin() const {
  return GetDefinition(
      TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN,
      kDefaultDevFallbackOrigin);
}

std::string TestDataReductionProxyParams::GetDefaultOrigin() const {
  return GetDefinition(
      TestDataReductionProxyParams::HAS_ORIGIN, kDefaultOrigin);
}

std::string TestDataReductionProxyParams::GetDefaultFallbackOrigin() const {
  return GetDefinition(
      TestDataReductionProxyParams::HAS_FALLBACK_ORIGIN,
      kDefaultFallbackOrigin);
}

std::string TestDataReductionProxyParams::GetDefaultSSLOrigin() const {
  return GetDefinition(
      TestDataReductionProxyParams::HAS_SSL_ORIGIN, kDefaultSSLOrigin);
}

std::string TestDataReductionProxyParams::GetDefaultAltOrigin() const {
  return GetDefinition(
      TestDataReductionProxyParams::HAS_ALT_ORIGIN, kDefaultAltOrigin);
}

std::string TestDataReductionProxyParams::GetDefaultAltFallbackOrigin() const {
  return GetDefinition(
      TestDataReductionProxyParams::HAS_ALT_FALLBACK_ORIGIN,
      kDefaultAltFallbackOrigin);
}

std::string TestDataReductionProxyParams::GetDefaultProbeURL() const {
  return GetDefinition(
      TestDataReductionProxyParams::HAS_PROBE_URL, kDefaultProbeURL);
}

std::string TestDataReductionProxyParams::GetDefinition(
    unsigned int has_def,
    const std::string& definition) const {
  return ((has_definitions_ & has_def) ? definition : std::string());
}
}  // namespace data_reduction_proxy

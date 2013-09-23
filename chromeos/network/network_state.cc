// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_state.h"

#include "base/i18n/icu_encoding_detection.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_ui_data.h"
#include "chromeos/network/onc/onc_utils.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

bool ConvertListValueToStringVector(const base::ListValue& string_list,
                                    std::vector<std::string>* result) {
  for (size_t i = 0; i < string_list.GetSize(); ++i) {
    std::string str;
    if (!string_list.GetString(i, &str))
      return false;
    result->push_back(str);
  }
  return true;
}

// Replace non UTF8 characters in |str| with a replacement character.
std::string ValidateUTF8(const std::string& str) {
  std::string result;
  for (int32 index = 0; index < static_cast<int32>(str.size()); ++index) {
    uint32 code_point_out;
    bool is_unicode_char = base::ReadUnicodeCharacter(str.c_str(), str.size(),
                                                      &index, &code_point_out);
    const uint32 kFirstNonControlChar = 0x20;
    if (is_unicode_char && (code_point_out >= kFirstNonControlChar)) {
      base::WriteUnicodeCharacter(code_point_out, &result);
    } else {
      const uint32 kReplacementChar = 0xFFFD;
      // Puts kReplacementChar if character is a control character [0,0x20)
      // or is not readable UTF8.
      base::WriteUnicodeCharacter(kReplacementChar, &result);
    }
  }
  return result;
}

}  // namespace

namespace chromeos {

NetworkState::NetworkState(const std::string& path)
    : ManagedState(MANAGED_TYPE_NETWORK, path),
      auto_connect_(false),
      favorite_(false),
      priority_(0),
      onc_source_(onc::ONC_SOURCE_NONE),
      signal_strength_(0),
      connectable_(false),
      passphrase_required_(false),
      activate_over_non_cellular_networks_(false),
      cellular_out_of_credits_(false) {
}

NetworkState::~NetworkState() {
}

bool NetworkState::PropertyChanged(const std::string& key,
                                   const base::Value& value) {
  // Keep care that these properties are the same as in |GetProperties|.
  if (ManagedStatePropertyChanged(key, value))
    return true;
  if (key == flimflam::kSignalStrengthProperty) {
    return GetIntegerValue(key, value, &signal_strength_);
  } else if (key == flimflam::kStateProperty) {
    return GetStringValue(key, value, &connection_state_);
  } else if (key == flimflam::kConnectableProperty) {
    return GetBooleanValue(key, value, &connectable_);
  } else if (key == flimflam::kPassphraseRequiredProperty) {
    return GetBooleanValue(key, value, &passphrase_required_);
  } else if (key == shill::kWifiFrequencyListProperty) {
    const base::ListValue* frequencies;
    if (!value.GetAsList(&frequencies)) {
      NET_LOG_ERROR("Failed to parse " + key, path());
      return false;
    }
    wifi_frequencies_.clear();
    for (base::ListValue::const_iterator iter = frequencies->begin();
         iter != frequencies->end(); ++iter) {
      int frequency;
      if ((*iter)->GetAsInteger(&frequency))
        wifi_frequencies_.push_back(frequency);
    }
    if (!wifi_frequencies_.empty()) {
      std::string str;
      base::JSONWriter::Write(frequencies, &str);
      NET_LOG_DEBUG("WifiFrequencies for " + path(), str);
    }
    return true;
  } else if (key == flimflam::kErrorProperty) {
    return GetStringValue(key, value, &error_);
  } else if (key == shill::kErrorDetailsProperty) {
    return GetStringValue(key, value, &error_details_);
  } else if (key == IPConfigProperty(flimflam::kAddressProperty)) {
    return GetStringValue(key, value, &ip_address_);
  } else if (key == IPConfigProperty(flimflam::kNameServersProperty)) {
    dns_servers_.clear();
    const base::ListValue* dns_servers;
    if (value.GetAsList(&dns_servers))
      ConvertListValueToStringVector(*dns_servers, &dns_servers_);
    return true;
  } else if (key == flimflam::kActivationStateProperty) {
    return GetStringValue(key, value, &activation_state_);
  } else if (key == flimflam::kRoamingStateProperty) {
    return GetStringValue(key, value, &roaming_);
  } else if (key == flimflam::kSecurityProperty) {
    return GetStringValue(key, value, &security_);
  } else if (key == flimflam::kAutoConnectProperty) {
    return GetBooleanValue(key, value, &auto_connect_);
  } else if (key == flimflam::kFavoriteProperty) {
    return GetBooleanValue(key, value, &favorite_);
  } else if (key == flimflam::kPriorityProperty) {
    return GetIntegerValue(key, value, &priority_);
  } else if (key == flimflam::kProxyConfigProperty) {
    std::string proxy_config_str;
    if (!value.GetAsString(&proxy_config_str)) {
      NET_LOG_ERROR("Failed to parse " + key, path());
      return false;
    }

    proxy_config_.Clear();
    if (proxy_config_str.empty())
      return true;

    scoped_ptr<base::DictionaryValue> proxy_config_dict(
        onc::ReadDictionaryFromJson(proxy_config_str));
    if (proxy_config_dict) {
      // Warning: The DictionaryValue returned from
      // ReadDictionaryFromJson/JSONParser is an optimized derived class that
      // doesn't allow releasing ownership of nested values. A Swap in the wrong
      // order leads to memory access errors.
      proxy_config_.MergeDictionary(proxy_config_dict.get());
    } else {
      NET_LOG_ERROR("Failed to parse " + key, path());
    }
    return true;
  } else if (key == flimflam::kUIDataProperty) {
    std::string ui_data_str;
    if (!value.GetAsString(&ui_data_str)) {
      NET_LOG_ERROR("Failed to parse " + key, path());
      return false;
    }

    onc_source_ = onc::ONC_SOURCE_NONE;
    if (ui_data_str.empty())
      return true;

    scoped_ptr<base::DictionaryValue> ui_data_dict(
        onc::ReadDictionaryFromJson(ui_data_str));
    if (ui_data_dict)
      onc_source_ = NetworkUIData(*ui_data_dict).onc_source();
    else
      NET_LOG_ERROR("Failed to parse " + key, path());
    return true;
  } else if (key == flimflam::kNetworkTechnologyProperty) {
    return GetStringValue(key, value, &technology_);
  } else if (key == flimflam::kDeviceProperty) {
    return GetStringValue(key, value, &device_path_);
  } else if (key == flimflam::kGuidProperty) {
    return GetStringValue(key, value, &guid_);
  } else if (key == flimflam::kProfileProperty) {
    return GetStringValue(key, value, &profile_path_);
  } else if (key == shill::kActivateOverNonCellularNetworkProperty) {
    return GetBooleanValue(key, value, &activate_over_non_cellular_networks_);
  } else if (key == shill::kOutOfCreditsProperty) {
    return GetBooleanValue(key, value, &cellular_out_of_credits_);
  } else if (key == flimflam::kUsageURLProperty) {
    return GetStringValue(key, value, &usage_url_);
  } else if (key == flimflam::kPaymentPortalProperty) {
    const DictionaryValue& dict = static_cast<const DictionaryValue&>(value);
    if (!dict.GetStringWithoutPathExpansion(flimflam::kPaymentPortalURL,
                                            &payment_url_))
      return false;
    if (!dict.GetStringWithoutPathExpansion(flimflam::kPaymentPortalMethod,
                                            &post_method_))
      return false;
    if (!dict.GetStringWithoutPathExpansion(flimflam::kPaymentPortalPostData,
                                            &post_data_))
      return false;
    return true;
  } else if (key == flimflam::kWifiHexSsid) {
    return GetStringValue(key, value, &hex_ssid_);
  } else if (key == flimflam::kCountryProperty) {
    // TODO(stevenjb): This is currently experimental. If we find a case where
    // base::DetectEncoding() fails in UpdateName(), where country_code_ is
    // set, figure out whether we can use country_code_ with ConvertToUtf8().
    // crbug.com/233267.
    return GetStringValue(key, value, &country_code_);
  }
  return false;
}

void NetworkState::InitialPropertiesReceived() {
  UpdateName();
}

void NetworkState::GetProperties(base::DictionaryValue* dictionary) const {
  // Keep care that these properties are the same as in |PropertyChanged|.
  dictionary->SetStringWithoutPathExpansion(flimflam::kNameProperty, name());
  dictionary->SetStringWithoutPathExpansion(flimflam::kTypeProperty, type());
  dictionary->SetIntegerWithoutPathExpansion(flimflam::kSignalStrengthProperty,
                                             signal_strength_);
  dictionary->SetStringWithoutPathExpansion(flimflam::kStateProperty,
                                            connection_state_);
  dictionary->SetBooleanWithoutPathExpansion(flimflam::kConnectableProperty,
                                             connectable_);
  dictionary->SetBooleanWithoutPathExpansion(
      flimflam::kPassphraseRequiredProperty, passphrase_required_);

  base::ListValue* frequencies = new base::ListValue;
  for (FrequencyList::const_iterator iter = wifi_frequencies_.begin();
       iter != wifi_frequencies_.end(); ++iter) {
    frequencies->AppendInteger(*iter);
  }
  dictionary->SetWithoutPathExpansion(shill::kWifiFrequencyListProperty,
                                      frequencies);

  dictionary->SetStringWithoutPathExpansion(flimflam::kErrorProperty,
                                            error_);
  dictionary->SetStringWithoutPathExpansion(shill::kErrorDetailsProperty,
                                            error_details_);
  base::DictionaryValue* ipconfig_properties = new base::DictionaryValue;
  ipconfig_properties->SetStringWithoutPathExpansion(flimflam::kAddressProperty,
                                                     ip_address_);
  base::ListValue* name_servers = new base::ListValue;
  name_servers->AppendStrings(dns_servers_);
  ipconfig_properties->SetWithoutPathExpansion(flimflam::kNameServersProperty,
                                               name_servers);
  dictionary->SetWithoutPathExpansion(shill::kIPConfigProperty,
                                      ipconfig_properties);

  dictionary->SetStringWithoutPathExpansion(flimflam::kActivationStateProperty,
                                            activation_state_);
  dictionary->SetStringWithoutPathExpansion(flimflam::kRoamingStateProperty,
                                            roaming_);
  dictionary->SetStringWithoutPathExpansion(flimflam::kSecurityProperty,
                                            security_);
  dictionary->SetBooleanWithoutPathExpansion(flimflam::kAutoConnectProperty,
                                             auto_connect_);
  dictionary->SetBooleanWithoutPathExpansion(flimflam::kFavoriteProperty,
                                             favorite_);
  dictionary->SetIntegerWithoutPathExpansion(flimflam::kPriorityProperty,
                                             priority_);
  // Proxy config and ONC source are intentionally omitted: These properties are
  // placed in NetworkState to transition ProxyConfigServiceImpl from
  // NetworkLibrary to the new network stack. The networking extension API
  // shouldn't depend on this member. Once ManagedNetworkConfigurationHandler
  // is used instead of NetworkLibrary, we can remove them again.
  dictionary->SetStringWithoutPathExpansion(
      flimflam::kNetworkTechnologyProperty,
      technology_);
  dictionary->SetStringWithoutPathExpansion(flimflam::kDeviceProperty,
                                            device_path_);
  dictionary->SetStringWithoutPathExpansion(flimflam::kGuidProperty, guid_);
  dictionary->SetStringWithoutPathExpansion(flimflam::kProfileProperty,
                                            profile_path_);
  dictionary->SetBooleanWithoutPathExpansion(
      shill::kActivateOverNonCellularNetworkProperty,
      activate_over_non_cellular_networks_);
  dictionary->SetBooleanWithoutPathExpansion(shill::kOutOfCreditsProperty,
                                             cellular_out_of_credits_);
  base::DictionaryValue* payment_portal_properties = new DictionaryValue;
  payment_portal_properties->SetStringWithoutPathExpansion(
      flimflam::kPaymentPortalURL,
      payment_url_);
  payment_portal_properties->SetStringWithoutPathExpansion(
      flimflam::kPaymentPortalMethod,
      post_method_);
  payment_portal_properties->SetStringWithoutPathExpansion(
      flimflam::kPaymentPortalPostData,
      post_data_);
  dictionary->SetWithoutPathExpansion(flimflam::kPaymentPortalProperty,
                                      payment_portal_properties);
}

void NetworkState::GetConfigProperties(
    base::DictionaryValue* dictionary) const {
  dictionary->SetStringWithoutPathExpansion(flimflam::kNameProperty, name());
  dictionary->SetStringWithoutPathExpansion(flimflam::kTypeProperty, type());
  dictionary->SetStringWithoutPathExpansion(flimflam::kSecurityProperty,
                                            security());
  dictionary->SetStringWithoutPathExpansion(flimflam::kGuidProperty, guid());
}

bool NetworkState::IsConnectedState() const {
  return StateIsConnected(connection_state_);
}

bool NetworkState::IsConnectingState() const {
  return StateIsConnecting(connection_state_);
}

bool NetworkState::HasAuthenticationError() const {
  return (error_ == flimflam::kErrorBadPassphrase ||
          error_ == flimflam::kErrorBadWEPKey ||
          error_ == flimflam::kErrorPppAuthFailed ||
          error_ == shill::kErrorEapLocalTlsFailed ||
          error_ == shill::kErrorEapRemoteTlsFailed ||
          error_ == shill::kErrorEapAuthenticationFailed);
}

void NetworkState::UpdateName() {
  if (hex_ssid_.empty()) {
    // Validate name for UTF8.
    std::string valid_ssid = ValidateUTF8(name());
    if (valid_ssid != name()) {
      set_name(valid_ssid);
      NET_LOG_DEBUG("UpdateName", base::StringPrintf(
          "%s: UTF8: %s", path().c_str(), name().c_str()));
    }
    return;
  }

  std::string ssid;
  std::vector<uint8> raw_ssid_bytes;
  if (base::HexStringToBytes(hex_ssid_, &raw_ssid_bytes)) {
    ssid = std::string(raw_ssid_bytes.begin(), raw_ssid_bytes.end());
  } else {
    std::string desc = base::StringPrintf("%s: Error processing: %s",
                                          path().c_str(), hex_ssid_.c_str());
    NET_LOG_DEBUG("UpdateName", desc);
    LOG(ERROR) << desc;
    ssid = name();
  }

  if (IsStringUTF8(ssid)) {
    if (ssid != name()) {
      set_name(ssid);
      NET_LOG_DEBUG("UpdateName", base::StringPrintf(
          "%s: UTF8: %s", path().c_str(), name().c_str()));
    }
    return;
  }

  // Detect encoding and convert to UTF-8.
  std::string encoding;
  if (!base::DetectEncoding(ssid, &encoding)) {
    // TODO(stevenjb): Test this. See comment in PropertyChanged() under
    // flimflam::kCountryProperty.
    encoding = country_code_;
  }
  if (!encoding.empty()) {
    std::string utf8_ssid;
    if (base::ConvertToUtf8AndNormalize(ssid, encoding, &utf8_ssid)) {
      set_name(utf8_ssid);
      NET_LOG_DEBUG("UpdateName", base::StringPrintf(
          "%s: Encoding=%s: %s", path().c_str(),
          encoding.c_str(), name().c_str()));
      return;
    }
  }

  // Unrecognized encoding. Only use raw bytes if name_ is empty.
  if (name().empty())
    set_name(ssid);
  NET_LOG_DEBUG("UpdateName", base::StringPrintf(
      "%s: Unrecognized Encoding=%s: %s", path().c_str(),
      encoding.c_str(), name().c_str()));
}

// static
bool NetworkState::StateIsConnected(const std::string& connection_state) {
  return (connection_state == flimflam::kStateReady ||
          connection_state == flimflam::kStateOnline ||
          connection_state == flimflam::kStatePortal);
}

// static
bool NetworkState::StateIsConnecting(const std::string& connection_state) {
  return (connection_state == flimflam::kStateAssociation ||
          connection_state == flimflam::kStateConfiguration ||
          connection_state == flimflam::kStateCarrier);
}

// static
std::string NetworkState::IPConfigProperty(const char* key) {
  return base::StringPrintf("%s.%s", shill::kIPConfigProperty, key);
}

}  // namespace chromeos

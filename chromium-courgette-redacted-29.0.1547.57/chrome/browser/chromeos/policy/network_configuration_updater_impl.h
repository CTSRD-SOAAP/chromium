// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_NETWORK_CONFIGURATION_UPDATER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_NETWORK_CONFIGURATION_UPDATER_IMPL_H_

#include "chrome/browser/chromeos/policy/network_configuration_updater.h"
#include "chrome/browser/policy/policy_service.h"
#include "chromeos/network/onc/onc_constants.h"

namespace base {
class Value;
}

namespace chromeos {
class CertificateHandler;
class ManagedNetworkConfigurationHandler;
}

namespace policy {

class PolicyMap;

// This implementation pushes policies to the
// ManagedNetworkConfigurationHandler. User policies are only pushed after
// OnUserPolicyInitialized() was called.
class NetworkConfigurationUpdaterImpl : public NetworkConfigurationUpdater {
 public:
  NetworkConfigurationUpdaterImpl(
      PolicyService* policy_service,
      scoped_ptr<chromeos::CertificateHandler> certificate_handler);
  virtual ~NetworkConfigurationUpdaterImpl();

  // NetworkConfigurationUpdater overrides.
  virtual void SetUserPolicyService(
      bool allow_trusted_certs_from_policy,
      const std::string& hashed_username,
      PolicyService* user_policy_service) OVERRIDE;

  virtual void UnsetUserPolicyService() OVERRIDE;

 private:
  // Callback that's called by |policy_service_| if the respective ONC policy
  // changed.
  void OnPolicyChanged(chromeos::onc::ONCSource onc_source,
                       const base::Value* previous,
                       const base::Value* current);

  void ApplyNetworkConfiguration(chromeos::onc::ONCSource onc_source);

  // Wraps the policy service we read network configuration from.
  PolicyChangeRegistrar policy_change_registrar_;

  // The policy service storing the ONC policies.
  PolicyService* policy_service_;

  // User hash of the user that the user policy applies to.
  std::string hashed_username_;

  scoped_ptr<chromeos::CertificateHandler> certificate_handler_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConfigurationUpdaterImpl);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_NETWORK_CONFIGURATION_UPDATER_IMPL_H_

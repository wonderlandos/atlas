// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ATLAS_FED_FED_PRECONNECT_GUARD_H_
#define CHROME_BROWSER_ATLAS_FED_FED_PRECONNECT_GUARD_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/preconnect_manager.h"
#include "url/gurl.h"

// A Federate name is exclusively fed:// (ADR 0052, lead rule 2026-11-05), so no
// DNS prefetch and no preconnect may ever see a host under a Federate TLD: the
// predictors (loading predictor, network hints, omnibox) all go through a
// content::PreconnectManager, and this wraps one so that every call about such a
// host is dropped before it reaches the real manager. Everything else is
// forwarded as is. (The host-resolver rules of fed_dns_guard.h are the second,
// independent fence.)
namespace atlas_fed {

class FedPreconnectGuard : public content::PreconnectManager {
 public:
  explicit FedPreconnectGuard(std::unique_ptr<content::PreconnectManager> real);
  FedPreconnectGuard(const FedPreconnectGuard&) = delete;
  FedPreconnectGuard& operator=(const FedPreconnectGuard&) = delete;
  ~FedPreconnectGuard() override;

  // `real` behind the guard.
  static std::unique_ptr<content::PreconnectManager> Wrap(
      std::unique_ptr<content::PreconnectManager> real);

  // content::PreconnectManager:
  void Start(const GURL& url,
             std::vector<content::PreconnectRequest> requests,
             net::NetworkTrafficAnnotationTag traffic_annotation) override;
  void StartPreresolveHost(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      net::NetworkTrafficAnnotationTag traffic_annotation,
      const content::StoragePartitionConfig* storage_partition_config,
      const base::UnguessableToken& network_restrictions_id) override;
  void StartPreresolveHosts(
      const std::vector<GURL>& urls,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      net::NetworkTrafficAnnotationTag traffic_annotation,
      const content::StoragePartitionConfig* storage_partition_config,
      const base::UnguessableToken& network_restrictions_id) override;
  void StartPreconnectUrl(
      const GURL& url,
      bool allow_credentials,
      net::NetworkAnonymizationKey network_anonymization_key,
      net::NetworkTrafficAnnotationTag traffic_annotation,
      const content::StoragePartitionConfig* storage_partition_config,
      const base::UnguessableToken& network_restrictions_id,
      std::optional<net::ConnectionKeepAliveConfig> keepalive_config,
      mojo::PendingRemote<network::mojom::ConnectionChangeObserverClient>
          connection_change_observer_client) override;
  void Stop(const GURL& url) override;
  base::WeakPtr<content::PreconnectManager> GetWeakPtr() override;
  void SetNetworkContextForTesting(
      network::mojom::NetworkContext* network_context) override;
  void SetObserverForTesting(Observer* observer) override;

 private:
  std::unique_ptr<content::PreconnectManager> real_;
  base::WeakPtrFactory<FedPreconnectGuard> weak_factory_{this};
};

}  // namespace atlas_fed

#endif  // CHROME_BROWSER_ATLAS_FED_FED_PRECONNECT_GUARD_H_

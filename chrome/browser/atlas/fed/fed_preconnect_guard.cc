// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/atlas/fed/fed_preconnect_guard.h"

#include <algorithm>
#include <utility>

#include "chrome/browser/atlas/fed/fed_url.h"

namespace atlas_fed {

FedPreconnectGuard::FedPreconnectGuard(
    std::unique_ptr<content::PreconnectManager> real)
    : real_(std::move(real)) {}

FedPreconnectGuard::~FedPreconnectGuard() = default;

// static
std::unique_ptr<content::PreconnectManager> FedPreconnectGuard::Wrap(
    std::unique_ptr<content::PreconnectManager> real) {
  return std::make_unique<FedPreconnectGuard>(std::move(real));
}

void FedPreconnectGuard::Start(
    const GURL& url,
    std::vector<content::PreconnectRequest> requests,
    net::NetworkTrafficAnnotationTag traffic_annotation) {
  // A Federate page needs no connection at all; a web page's request for a
  // Federate host is dropped one by one.
  if (IsFederateTldUrl(url)) {
    return;
  }
  std::erase_if(requests, [](const content::PreconnectRequest& r) {
    return IsFederateTldHost(r.origin.host());
  });
  real_->Start(url, std::move(requests), traffic_annotation);
}

void FedPreconnectGuard::StartPreresolveHost(
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    const content::StoragePartitionConfig* storage_partition_config,
    const base::UnguessableToken& network_restrictions_id) {
  if (IsFederateTldUrl(url)) {
    return;
  }
  real_->StartPreresolveHost(url, network_anonymization_key,
                             traffic_annotation, storage_partition_config,
                             network_restrictions_id);
}

void FedPreconnectGuard::StartPreresolveHosts(
    const std::vector<GURL>& urls,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    const content::StoragePartitionConfig* storage_partition_config,
    const base::UnguessableToken& network_restrictions_id) {
  std::vector<GURL> kept;
  std::ranges::copy_if(urls, std::back_inserter(kept), [](const GURL& u) {
    return !IsFederateTldUrl(u);
  });
  if (kept.empty()) {
    return;
  }
  real_->StartPreresolveHosts(kept, network_anonymization_key,
                              traffic_annotation, storage_partition_config,
                              network_restrictions_id);
}

void FedPreconnectGuard::StartPreconnectUrl(
    const GURL& url,
    bool allow_credentials,
    net::NetworkAnonymizationKey network_anonymization_key,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    const content::StoragePartitionConfig* storage_partition_config,
    const base::UnguessableToken& network_restrictions_id,
    std::optional<net::ConnectionKeepAliveConfig> keepalive_config,
    mojo::PendingRemote<network::mojom::ConnectionChangeObserverClient>
        connection_change_observer_client) {
  if (IsFederateTldUrl(url)) {
    return;
  }
  real_->StartPreconnectUrl(
      url, allow_credentials, std::move(network_anonymization_key),
      traffic_annotation, storage_partition_config, network_restrictions_id,
      std::move(keepalive_config), std::move(connection_change_observer_client));
}

void FedPreconnectGuard::Stop(const GURL& url) {
  real_->Stop(url);
}

base::WeakPtr<content::PreconnectManager> FedPreconnectGuard::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void FedPreconnectGuard::SetNetworkContextForTesting(
    network::mojom::NetworkContext* network_context) {
  real_->SetNetworkContextForTesting(network_context);
}

void FedPreconnectGuard::SetObserverForTesting(Observer* observer) {
  real_->SetObserverForTesting(observer);
}

}  // namespace atlas_fed

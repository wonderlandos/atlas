// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ATLAS_FED_FED_SUBRESOURCE_PROXY_H_
#define CHROME_BROWSER_ATLAS_FED_FED_SUBRESOURCE_PROXY_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/self_deleting_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

// A Federate name is exclusively fed:// (ADR 0052, lead rule 2026-11-05). The
// navigation interceptor (fed_http_interceptor.h) covers page and frame loads;
// this proxy covers everything else a frame, a worker or a service worker
// loads: a request to http(s)://x.<known TLD>/... (styles, scripts, images,
// fetch(), XHR) is answered by the Federate loader as fed://x.tld/..., never
// handed on to the network service. A web page may not read Federate names
// (probe, fingerprint): only a frame, or a request whose initiator is a fed://
// page, is answered; any other gets the honest 403 page. Other requests pass
// untouched.
namespace atlas_fed {

class FedSubresourceProxy : public network::SelfDeletingURLLoaderFactory {
 public:
  FedSubresourceProxy(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> target_remote,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> fed_remote,
      base::SelfDeletingPassKey pass_key);
  FedSubresourceProxy(const FedSubresourceProxy&) = delete;
  FedSubresourceProxy& operator=(const FedSubresourceProxy&) = delete;

  // Puts the proxy first in the chain of `factory_builder`
  // (ChromeContentBrowserClient::WillCreateURLLoaderFactory).
  static void Install(network::URLLoaderFactoryBuilder& factory_builder);

  // The proxy over given factories, for tests: returns the factory to send
  // requests to.
  static mojo::PendingRemote<network::mojom::URLLoaderFactory> CreateForTesting(
      mojo::PendingRemote<network::mojom::URLLoaderFactory> target_remote,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> fed_remote);

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;

 private:
  ~FedSubresourceProxy() override;

  void OnFactoryError();

  mojo::Remote<network::mojom::URLLoaderFactory> target_;
  mojo::Remote<network::mojom::URLLoaderFactory> fed_;
};

}  // namespace atlas_fed

#endif  // CHROME_BROWSER_ATLAS_FED_FED_SUBRESOURCE_PROXY_H_

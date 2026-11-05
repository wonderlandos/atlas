// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ATLAS_FED_FED_URL_LOADER_FACTORY_H_
#define CHROME_BROWSER_ATLAS_FED_FED_URL_LOADER_FACTORY_H_

#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/atlas/fed/fed_helper_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

// The fed:// loader: each load is answered by the Federate helper
// (fed_helper_client.h) on a thread that may block, and handed to the engine
// as an ordinary response (status, content type, body), so a Federate page is
// a normal page of origin fed://label.tld. No cookies are read or set; nothing
// is cached here (the helper holds the verified zone and manifests).
namespace atlas_fed {

// What answers one load (Fetch in the browser; a stand-in in tests).
using Fetcher =
    base::RepeatingCallback<FedResult(const std::string& url, LoadKind kind)>;

class FedURLLoaderFactory : public network::mojom::URLLoaderFactory {
 public:
  // For navigations (documents) and for the subresources of every frame.
  static mojo::PendingRemote<network::mojom::URLLoaderFactory> Create();
  static mojo::PendingRemote<network::mojom::URLLoaderFactory>
  CreateForTesting(Fetcher fetcher);

  explicit FedURLLoaderFactory(Fetcher fetcher);
  FedURLLoaderFactory(const FedURLLoaderFactory&) = delete;
  FedURLLoaderFactory& operator=(const FedURLLoaderFactory&) = delete;
  ~FedURLLoaderFactory() override;

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;

 private:
  Fetcher fetcher_;
};

// Answers `client` with `result` as the loader would (a head, a body pipe, the completion);
// `loader` is held until then. For a reply that does not come from the helper (a refusal).
void RespondWithResult(mojo::PendingRemote<network::mojom::URLLoaderClient> client,
                       mojo::PendingReceiver<network::mojom::URLLoader> loader,
                       bool head_only,
                       FedResult result);

// The response head for a helper reply (exposed for the unit tests).
network::mojom::URLResponseHeadPtr ResponseHeadFor(const FedResult& result);

}  // namespace atlas_fed

#endif  // CHROME_BROWSER_ATLAS_FED_FED_URL_LOADER_FACTORY_H_

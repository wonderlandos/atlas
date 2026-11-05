// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/atlas/fed/fed_subresource_proxy.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/atlas/fed/fed_url.h"
#include "chrome/browser/atlas/fed/fed_url_loader_factory.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "url/origin.h"

namespace atlas_fed {

namespace {

bool IsFrameLoad(network::mojom::RequestDestination d) {
  return d == network::mojom::RequestDestination::kDocument ||
         d == network::mojom::RequestDestination::kIframe ||
         d == network::mojom::RequestDestination::kFrame ||
         d == network::mojom::RequestDestination::kFencedframe;
}

// Whether the page (or worker) that makes `request` is a Federate page: its
// initiator, or the origin an opaque initiator comes from, has the fed scheme.
// A request with no initiator (not a page's) is not.
bool AskedByFederatePage(const network::ResourceRequest& request) {
  return request.request_initiator &&
         request.request_initiator->GetTupleOrPrecursorTupleIfOpaque()
                 .scheme() == kFedScheme;
}

FedResult RefusalFor(const GURL& url) {
  FedResult r;
  r.status = 403;
  r.mime = "text/html; charset=utf-8";
  std::string safe;
  for (char c : url.spec()) {
    safe += (c == '<' || c == '>' || c == '&' || c == '"' || c == '\'')
                ? '_'
                : c;
  }
  r.body =
      "<!doctype html><meta charset=utf-8><title>Federate</title>"
      "<h1>Not opened</h1><p>" +
      safe +
      " is a Federate name. On WolOS Federate names are fed:// only, and a "
      "page of the web may not read them: it is not fetched, not looked up in "
      "DNS, and its existence is not told to this page.</p>";
  return r;
}

}  // namespace

FedSubresourceProxy::FedSubresourceProxy(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_remote,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> fed_remote,
    base::SelfDeletingPassKey pass_key)
    : network::SelfDeletingURLLoaderFactory(std::move(loader_receiver),
                                            pass_key) {
  target_.Bind(std::move(target_remote));
  target_.set_disconnect_handler(base::BindOnce(
      &FedSubresourceProxy::OnFactoryError, base::Unretained(this)));
  fed_.Bind(std::move(fed_remote));
  fed_.set_disconnect_handler(base::BindOnce(
      &FedSubresourceProxy::OnFactoryError, base::Unretained(this)));
}

FedSubresourceProxy::~FedSubresourceProxy() = default;

// static
void FedSubresourceProxy::Install(
    network::URLLoaderFactoryBuilder& factory_builder) {
  auto [receiver, remote] = factory_builder.Append();
  // The proxy manages its own lifetime, bound to the Mojo connection.
  base::MakeSelfDeleting<FedSubresourceProxy>(
      std::move(receiver), std::move(remote), FedURLLoaderFactory::Create());
}

// static
mojo::PendingRemote<network::mojom::URLLoaderFactory>
FedSubresourceProxy::CreateForTesting(
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_remote,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> fed_remote) {
  mojo::PendingRemote<network::mojom::URLLoaderFactory> remote;
  base::MakeSelfDeleting<FedSubresourceProxy>(
      remote.InitWithNewPipeAndPassReceiver(), std::move(target_remote),
      std::move(fed_remote));
  return remote;
}

void FedSubresourceProxy::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  std::optional<GURL> fed = MapHttpToFed(request.url);
  if (!fed) {
    target_->CreateLoaderAndStart(std::move(loader_receiver), request_id,
                                  options, request, std::move(client),
                                  traffic_annotation);
    return;
  }
  // A frame is let through (it becomes a fed:// document: the interceptor's redirect
  // covers navigations, this covers the rest). Anything else a web page asks of a
  // Federate name (fetch, XHR, images, scripts, workers) is refused with the honest
  // 403 page: a web page must not probe or fingerprint Federate names. Only a
  // fed:// page reads fed resources.
  if (!IsFrameLoad(request.destination) && !AskedByFederatePage(request)) {
    RespondWithResult(std::move(client), std::move(loader_receiver),
                      /*head_only=*/false, RefusalFor(request.url));
    return;
  }
  // Answered from the chain, never the network: the same request, the fed://
  // address. A failed chain is the loader's own error, not a retry elsewhere.
  network::ResourceRequest mapped(request);
  mapped.url = *fed;
  fed_->CreateLoaderAndStart(std::move(loader_receiver), request_id, options,
                             mapped, std::move(client), traffic_annotation);
}

void FedSubresourceProxy::OnFactoryError() {
  DisconnectReceiversAndDestroy();
}

}  // namespace atlas_fed

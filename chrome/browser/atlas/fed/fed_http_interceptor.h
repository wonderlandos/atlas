// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ATLAS_FED_FED_HTTP_INTERCEPTOR_H_
#define CHROME_BROWSER_ATLAS_FED_FED_HTTP_INTERCEPTOR_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

// A Federate name is exclusively fed:// (ADR 0052, lead rule 2026-11-05): a
// navigation (page or frame) to http(s)://x.<known TLD> (fed_url.h) is
// redirected (307) to fed://x.tld before it reaches the web. Always on: there is
// no setting. Subresources and workers are the proxy's (fed_subresource_proxy.h).
namespace atlas_fed {

// Where `url` goes instead, or nullopt (not a Federate TLD host).
std::optional<GURL> RedirectTargetFor(const GURL& url);

class FedHttpInterceptor : public content::URLLoaderRequestInterceptor,
                           public network::mojom::URLLoader {
 public:
  FedHttpInterceptor();
  FedHttpInterceptor(const FedHttpInterceptor&) = delete;
  FedHttpInterceptor& operator=(const FedHttpInterceptor&) = delete;
  ~FedHttpInterceptor() override;

  // content::URLLoaderRequestInterceptor:
  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      content::BrowserContext* browser_context,
      content::URLLoaderRequestInterceptor::LoaderCallback callback) override;

 private:
  // network::mojom::URLLoader (the redirect is all this loader does):
  void FollowRedirect(
      network::HttpRequestHeadersUpdateParams headers_update_params,
      const std::optional<GURL>& new_url) override {}
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override {}

  void Redirect(const GURL& new_url,
                const network::ResourceRequest& request,
                mojo::PendingReceiver<network::mojom::URLLoader> receiver,
                mojo::PendingRemote<network::mojom::URLLoaderClient> client);
  void OnConnectionClosed();

  mojo::Receiver<network::mojom::URLLoader> receiver_{this};
  mojo::Remote<network::mojom::URLLoaderClient> client_;
  base::WeakPtrFactory<FedHttpInterceptor> weak_factory_{this};
};

}  // namespace atlas_fed

#endif  // CHROME_BROWSER_ATLAS_FED_FED_HTTP_INTERCEPTOR_H_

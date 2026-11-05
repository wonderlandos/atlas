// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/atlas/fed/fed_http_interceptor.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/atlas/fed/fed_url.h"
#include "content/public/browser/browser_context.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace atlas_fed {

std::optional<GURL> RedirectTargetFor(const GURL& url) {
  return MapHttpToFed(url);
}

FedHttpInterceptor::FedHttpInterceptor() = default;
FedHttpInterceptor::~FedHttpInterceptor() = default;

void FedHttpInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    content::BrowserContext* browser_context,
    content::URLLoaderRequestInterceptor::LoaderCallback callback) {
  std::optional<GURL> target =
      RedirectTargetFor(tentative_resource_request.url);
  if (!target) {
    std::move(callback).Run({});
    return;
  }
  std::move(callback).Run(base::BindOnce(&FedHttpInterceptor::Redirect,
                                         weak_factory_.GetWeakPtr(), *target));
}

void FedHttpInterceptor::Redirect(
    const GURL& new_url,
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  receiver_.reset();
  client_.reset();
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &FedHttpInterceptor::OnConnectionClosed, weak_factory_.GetWeakPtr()));
  client_.Bind(std::move(client));

  auto head = network::mojom::URLResponseHead::New();
  head->encoded_data_length = 0;
  head->request_start = base::TimeTicks::Now();
  head->response_start = head->request_start;
  std::string raw = base::StringPrintf(
      "HTTP/1.1 %i Temporary Redirect\nLocation: %s\n"
      "Non-Authoritative-Reason: Federate\n",
      net::HTTP_TEMPORARY_REDIRECT, new_url.spec().c_str());
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(raw));
  net::RedirectInfo info = net::RedirectInfo::ComputeRedirectInfo(
      request.method, request.url, request.site_for_cookies,
      request.update_first_party_url_on_redirect
          ? net::RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT
          : net::RedirectInfo::FirstPartyURLPolicy::NEVER_CHANGE_URL,
      request.referrer_policy, request.referrer.spec(),
      request.request_initiator, net::HTTP_TEMPORARY_REDIRECT, new_url,
      /*referrer_policy_header=*/std::nullopt,
      /*insecure_scheme_was_upgraded=*/false);
  client_->OnReceiveRedirect(info, std::move(head));
}

void FedHttpInterceptor::OnConnectionClosed() {
  receiver_.reset();
  client_.reset();
}

}  // namespace atlas_fed

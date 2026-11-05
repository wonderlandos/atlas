// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/atlas/fed/fed_url_loader_factory.h"

#include <utility>

#include "base/byte_size.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/atlas/fed/fed_url.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace atlas_fed {

namespace {

const char* ReasonFor(int status) {
  switch (status) {
    case 200:
      return "OK";
    case 400:
      return "Bad Request";
    case 404:
      return "Not Found";
    case 410:
      return "Gone";
    case 500:
      return "Internal Server Error";
    case 501:
      return "Not Implemented";
    case 502:
      return "Bad Gateway";
    case 503:
      return "Service Unavailable";
    case 504:
      return "Gateway Timeout";
    default:
      return "Federate";
  }
}

void Complete(mojo::Remote<network::mojom::URLLoaderClient> client,
              mojo::PendingReceiver<network::mojom::URLLoader> loader,
              bool head_only,
              bool document,
              FedResult result) {
  // `loader` is held until here so the engine does not take the wait for a
  // cancelled load; it closes when this returns.
  if (!client.is_connected()) {
    return;
  }
  // A page that cannot be checked because the helper is unreachable says so
  // (QA-A9-WEB-ATLAS-9), in place of the engine's "refused to connect"; a
  // subresource just fails.
  if (document && result.net_error != net::OK &&
      result.fault != HelperFault::kNone) {
    result = FaultPageFor(result.fault);
  }
  if (result.net_error != net::OK) {
    client->OnComplete(network::URLLoaderCompletionStatus(result.net_error));
    return;
  }
  network::mojom::URLResponseHeadPtr head = ResponseHeadFor(result);
  base::span<const uint8_t> body =
      head_only ? base::span<const uint8_t>() : base::as_byte_span(result.body);
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes =
      static_cast<uint32_t>(std::max<size_t>(body.size(), 1));
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  if (mojo::CreateDataPipe(&options, producer, consumer) != MOJO_RESULT_OK) {
    client->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
    return;
  }
  if (!body.empty() && producer->WriteAllData(body) != MOJO_RESULT_OK) {
    client->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
    return;
  }
  producer.reset();
  client->OnReceiveResponse(std::move(head), std::move(consumer),
                            /*cached_metadata=*/std::nullopt);
  network::URLLoaderCompletionStatus status(net::OK);
  status.encoded_data_length = base::ByteSize(body.size());
  status.encoded_body_length = base::ByteSize(body.size());
  status.decoded_body_length = base::ByteSize(body.size());
  client->OnComplete(status);
}

}  // namespace

network::mojom::URLResponseHeadPtr ResponseHeadFor(const FedResult& result) {
  auto head = network::mojom::URLResponseHead::New();
  head->request_start = base::TimeTicks::Now();
  head->response_start = head->request_start;
  std::string raw = base::StringPrintf(
      "HTTP/1.1 %d %s\nContent-Type: %s\nContent-Length: %zu\n"
      "X-Content-Type-Options: nosniff\nCache-Control: no-store\n",
      result.status, ReasonFor(result.status), result.mime.c_str(),
      result.body.size());
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(raw));
  head->headers->GetMimeTypeAndCharset(&head->mime_type, &head->charset);
  head->content_length = static_cast<int64_t>(result.body.size());
  return head;
}

void RespondWithResult(mojo::PendingRemote<network::mojom::URLLoaderClient> client,
                       mojo::PendingReceiver<network::mojom::URLLoader> loader,
                       bool head_only,
                       FedResult result) {
  Complete(mojo::Remote<network::mojom::URLLoaderClient>(std::move(client)),
           std::move(loader), head_only, /*document=*/false, std::move(result));
}

// static
mojo::PendingRemote<network::mojom::URLLoaderFactory>
FedURLLoaderFactory::Create() {
  return CreateForTesting(base::BindRepeating(&Fetch));
}

// static
mojo::PendingRemote<network::mojom::URLLoaderFactory>
FedURLLoaderFactory::CreateForTesting(Fetcher fetcher) {
  mojo::PendingRemote<network::mojom::URLLoaderFactory> remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<FedURLLoaderFactory>(std::move(fetcher)),
      remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

FedURLLoaderFactory::FedURLLoaderFactory(Fetcher fetcher)
    : fetcher_(std::move(fetcher)) {}

FedURLLoaderFactory::~FedURLLoaderFactory() = default;

void FedURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(client_remote));
  if (!request.url.SchemeIs(kFedScheme)) {
    client->OnComplete(network::URLLoaderCompletionStatus(net::ERR_INVALID_URL));
    return;
  }
  // The network's content is read-only: GET (and HEAD, without the body).
  bool head_only = request.method == net::HttpRequestHeaders::kHeadMethod;
  if (!request.method.empty() &&
      request.method != net::HttpRequestHeaders::kGetMethod && !head_only) {
    client->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_METHOD_NOT_SUPPORTED));
    return;
  }
  LoadKind kind = request.destination ==
                              network::mojom::RequestDestination::kDocument ||
                          request.destination ==
                              network::mojom::RequestDestination::kIframe ||
                          request.destination ==
                              network::mojom::RequestDestination::kFrame
                      ? LoadKind::kDocument
                      : LoadKind::kSubresource;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(fetcher_, request.url.spec(), kind),
      base::BindOnce(&Complete, std::move(client), std::move(loader),
                     head_only, kind == LoadKind::kDocument));
}

void FedURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<FedURLLoaderFactory>(fetcher_),
                              std::move(receiver));
}

}  // namespace atlas_fed

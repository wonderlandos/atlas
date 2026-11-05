// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/atlas/fed/fed_url_loader_factory.h"

#include "chrome/browser/atlas/fed/fed_subresource_proxy.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace atlas_fed {

namespace {

// A fed:// page's origin (the fixture registers the scheme as the browser does, patch 0120).
url::Origin FedOrigin() {
  return url::Origin::Create(GURL("fed://home.fed"));
}

struct Seen {
  std::vector<std::pair<std::string, LoadKind>> loads;
};

// Records what the loader tells the engine, until the load completes.
class RecordingClient : public network::mojom::URLLoaderClient {
 public:
  mojo::PendingRemote<network::mojom::URLLoaderClient> CreateRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }
  void RunUntilComplete() {
    if (!completed_) {
      run_loop_.Run();
    }
  }

  const network::URLLoaderCompletionStatus& completion_status() const {
    return status_;
  }
  const network::mojom::URLResponseHeadPtr& response_head() const {
    return head_;
  }
  mojo::ScopedDataPipeConsumerHandle TakeBody() { return std::move(body_); }

  // network::mojom::URLLoaderClient:
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr) override {}
  void OnReceiveResponse(network::mojom::URLResponseHeadPtr head,
                         mojo::ScopedDataPipeConsumerHandle body,
                         std::optional<mojo_base::BigBuffer>) override {
    head_ = std::move(head);
    body_ = std::move(body);
  }
  void OnReceiveRedirect(const net::RedirectInfo&,
                         network::mojom::URLResponseHeadPtr) override {}
  void OnUploadProgress(int64_t,
                        int64_t,
                        OnUploadProgressCallback callback) override {
    std::move(callback).Run();
  }
  void OnTransferSizeUpdated(int32_t) override {}
  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    status_ = status;
    completed_ = true;
    run_loop_.Quit();
  }

 private:
  mojo::Receiver<network::mojom::URLLoaderClient> receiver_{this};
  network::mojom::URLResponseHeadPtr head_;
  mojo::ScopedDataPipeConsumerHandle body_;
  network::URLLoaderCompletionStatus status_;
  bool completed_ = false;
  base::RunLoop run_loop_;
};

// The "web": records the URL of every request that reaches it and fails the
// load, so a test sees that a Federate name never gets here.
class WebStub : public network::mojom::URLLoaderFactory {
 public:
  explicit WebStub(std::vector<std::string>* seen) : seen_(seen) {}
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag&) override {
    seen_->push_back(request.url.spec());
    mojo::Remote<network::mojom::URLLoaderClient> c(std::move(client));
    c->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_CONNECTION_REFUSED));
    kept_.push_back(std::move(loader));
  }
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory>) override {
  }

 private:
  raw_ptr<std::vector<std::string>> seen_;
  std::vector<mojo::PendingReceiver<network::mojom::URLLoader>> kept_;
};

class AtlasFedUrlLoaderFactoryTest : public testing::Test {
 protected:
  AtlasFedUrlLoaderFactoryTest() { url::AddStandardScheme("fed", url::SCHEME_WITH_HOST); }

  url::ScopedSchemeRegistryForTests scheme_registry_;

  mojo::Remote<network::mojom::URLLoaderFactory> Factory(FedResult answer) {
    mojo::Remote<network::mojom::URLLoaderFactory> remote(
        FedURLLoaderFactory::CreateForTesting(base::BindRepeating(
            [](Seen* seen, FedResult answer, const std::string& url,
               LoadKind kind) {
              seen->loads.emplace_back(url, kind);
              return answer;
            },
            &seen_, std::move(answer))));
    return remote;
  }

  // Runs one load to its end; returns the client.
  std::unique_ptr<RecordingClient> Load(
      mojo::Remote<network::mojom::URLLoaderFactory>& factory,
      const GURL& url,
      network::mojom::RequestDestination destination,
      const std::string& method = "GET",
      std::optional<url::Origin> initiator = FedOrigin()) {
    network::ResourceRequest request;
    request.url = url;
    request.request_initiator = std::move(initiator);
    request.method = method;
    request.destination = destination;
    auto client = std::make_unique<RecordingClient>();
    mojo::Remote<network::mojom::URLLoader> loader;
    factory->CreateLoaderAndStart(
        loader.BindNewPipeAndPassReceiver(), 1, 0, request,
        client->CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
    client->RunUntilComplete();
    return client;
  }

  // A subresource proxy over the stub "web" and the stand-in fed loader.
  mojo::Remote<network::mojom::URLLoaderFactory> Proxy(FedResult answer) {
    mojo::PendingRemote<network::mojom::URLLoaderFactory> web;
    mojo::MakeSelfOwnedReceiver(std::make_unique<WebStub>(&web_seen_),
                                web.InitWithNewPipeAndPassReceiver());
    mojo::PendingRemote<network::mojom::URLLoaderFactory> fed =
        FedURLLoaderFactory::CreateForTesting(base::BindRepeating(
            [](Seen* seen, FedResult answer, const std::string& url,
               LoadKind kind) {
              seen->loads.emplace_back(url, kind);
              return answer;
            },
            &seen_, std::move(answer)));
    return mojo::Remote<network::mojom::URLLoaderFactory>(
        FedSubresourceProxy::CreateForTesting(std::move(web), std::move(fed)));
  }

  Seen seen_;
  std::vector<std::string> web_seen_;
};

}  // namespace

TEST_F(AtlasFedUrlLoaderFactoryTest, AVerifiedPageIsAPlainResponse) {
  FedResult answer;
  answer.status = 200;
  answer.mime = "text/html; charset=utf-8";
  answer.body = "<h1>hello</h1>";
  auto factory = Factory(answer);
  auto client = Load(factory, GURL("fed://home.fed/"),
                     network::mojom::RequestDestination::kDocument);
  EXPECT_EQ(client->completion_status().error_code, net::OK);
  ASSERT_TRUE(client->response_head());
  EXPECT_EQ(client->response_head()->headers->response_code(), 200);
  EXPECT_EQ(client->response_head()->mime_type, "text/html");
  EXPECT_EQ(client->response_head()->charset, "utf-8");
  std::string body;
  ASSERT_TRUE(mojo::BlockingCopyToString(client->TakeBody(), &body));
  EXPECT_EQ(body, "<h1>hello</h1>");
  ASSERT_EQ(seen_.loads.size(), 1u);
  EXPECT_EQ(seen_.loads[0].first, "fed://home.fed/");
  EXPECT_EQ(seen_.loads[0].second, LoadKind::kDocument);
}

TEST_F(AtlasFedUrlLoaderFactoryTest, ASubresourceIsAskedForAsOne) {
  FedResult answer;
  answer.status = 200;
  answer.mime = "text/css";
  auto factory = Factory(answer);
  Load(factory, GURL("fed://home.fed/a.css"),
       network::mojom::RequestDestination::kStyle);
  ASSERT_EQ(seen_.loads.size(), 1u);
  EXPECT_EQ(seen_.loads[0].second, LoadKind::kSubresource);
}

TEST_F(AtlasFedUrlLoaderFactoryTest, ARefusalKeepsItsStatusForTheLocationIcon) {
  FedResult answer;
  answer.status = 502;
  answer.mime = "text/html; charset=utf-8";
  answer.body = "<p>signature</p>";
  auto factory = Factory(answer);
  auto client = Load(factory, GURL("fed://home.fed/"),
                     network::mojom::RequestDestination::kDocument);
  ASSERT_TRUE(client->response_head());
  EXPECT_EQ(client->response_head()->headers->response_code(), 502);
}

TEST_F(AtlasFedUrlLoaderFactoryTest, ATransportFailureIsANetworkError) {
  FedResult answer;
  answer.net_error = net::ERR_CONNECTION_REFUSED;
  auto factory = Factory(answer);
  auto client = Load(factory, GURL("fed://home.fed/"),
                     network::mojom::RequestDestination::kDocument);
  EXPECT_EQ(client->completion_status().error_code,
            net::ERR_CONNECTION_REFUSED);
  EXPECT_FALSE(client->response_head());
}

// QA-A9-WEB-ATLAS-9: a page that cannot be checked because the helper is
// unreachable says so; the engine's "refused to connect" says nothing true.
TEST_F(AtlasFedUrlLoaderFactoryTest, AMissingHelperIsAPageThatSaysSoForADocument) {
  FedResult answer;
  answer.net_error = net::ERR_CONNECTION_REFUSED;
  answer.fault = HelperFault::kHelperMissing;
  auto factory = Factory(answer);
  auto client = Load(factory, GURL("fed://home.fed/"),
                     network::mojom::RequestDestination::kDocument);
  EXPECT_EQ(client->completion_status().error_code, net::OK);
  ASSERT_TRUE(client->response_head());
  EXPECT_EQ(client->response_head()->headers->response_code(), 503);
  std::string body;
  ASSERT_TRUE(mojo::BlockingCopyToString(client->TakeBody(), &body));
  EXPECT_NE(body.find("WolOS cannot check Federate pages"), std::string::npos);
  EXPECT_NE(body.find("atlas-fed"), std::string::npos);
}

TEST_F(AtlasFedUrlLoaderFactoryTest, AMissingHelperJustFailsASubresource) {
  FedResult answer;
  answer.net_error = net::ERR_CONNECTION_REFUSED;
  answer.fault = HelperFault::kHelperMissing;
  auto factory = Factory(answer);
  auto client = Load(factory, GURL("fed://home.fed/a.png"),
                     network::mojom::RequestDestination::kImage);
  EXPECT_EQ(client->completion_status().error_code,
            net::ERR_CONNECTION_REFUSED);
  EXPECT_FALSE(client->response_head());
}

TEST_F(AtlasFedUrlLoaderFactoryTest, OnlyFedAddressesAndReadMethodsAreServed) {
  FedResult answer;
  answer.status = 200;
  answer.mime = "text/html";
  auto factory = Factory(answer);
  auto other = Load(factory, GURL("https://home.fed/"),
                    network::mojom::RequestDestination::kDocument);
  EXPECT_EQ(other->completion_status().error_code, net::ERR_INVALID_URL);
  auto post = Load(factory, GURL("fed://home.fed/"),
                   network::mojom::RequestDestination::kDocument, "POST");
  EXPECT_EQ(post->completion_status().error_code,
            net::ERR_METHOD_NOT_SUPPORTED);
  EXPECT_TRUE(seen_.loads.empty());
}

TEST_F(AtlasFedUrlLoaderFactoryTest, TheResponseIsNeverCachedOrSniffed) {
  FedResult answer;
  answer.status = 200;
  answer.mime = "text/plain";
  answer.body = "x";
  network::mojom::URLResponseHeadPtr head = ResponseHeadFor(answer);
  EXPECT_EQ(head->headers->GetNormalizedHeader("Cache-Control"), "no-store");
  EXPECT_EQ(head->headers->GetNormalizedHeader("X-Content-Type-Options"),
            "nosniff");
  EXPECT_EQ(head->content_length, 1);
}

// A Federate name is exclusively fed:// (lead rule 2026-11-05), and a web page
// may not read one: a frame is let through, a fed:// page reads freely, any other
// initiator is refused with the honest 403 page (never the network, never the chain).
const char kWeb[] = "http://127.0.0.1:8777";

TEST_F(AtlasFedUrlLoaderFactoryTest, AFedPageReadsAnHttpSpelledFederateSubresourceFromTheChain) {
  FedResult answer;
  answer.status = 200;
  answer.mime = "text/css";
  answer.body = "p{}";
  auto proxy = Proxy(answer);
  auto client = Load(proxy, GURL("http://home.fed/a.css?v=1"),
                     network::mojom::RequestDestination::kStyle);
  EXPECT_EQ(client->completion_status().error_code, net::OK);
  ASSERT_EQ(seen_.loads.size(), 1u);
  EXPECT_EQ(seen_.loads[0].first, "fed://home.fed/a.css?v=1");
  EXPECT_EQ(seen_.loads[0].second, LoadKind::kSubresource);
  EXPECT_TRUE(web_seen_.empty());
}

TEST_F(AtlasFedUrlLoaderFactoryTest, AFedPageMayFetchAnotherFederateHostToo) {
  FedResult answer;
  answer.status = 200;
  auto proxy = Proxy(answer);
  Load(proxy, GURL("https://lobo.pagina/data.json"),
       network::mojom::RequestDestination::kEmpty, "GET",
       FedOrigin());
  ASSERT_EQ(seen_.loads.size(), 1u);
  EXPECT_EQ(seen_.loads[0].first, "fed://lobo.pagina/data.json");
}

TEST_F(AtlasFedUrlLoaderFactoryTest, AWebPageFetchingAFederateNameIsRefusedWithTheHonestPage) {
  FedResult answer;
  answer.status = 200;
  answer.body = "secret";
  auto proxy = Proxy(answer);
  for (auto dest : {network::mojom::RequestDestination::kEmpty,
                    network::mojom::RequestDestination::kImage,
                    network::mojom::RequestDestination::kScript,
                    network::mojom::RequestDestination::kWorker,
                    network::mojom::RequestDestination::kSharedWorker,
                    network::mojom::RequestDestination::kServiceWorker}) {
    auto client = Load(proxy, GURL("http://home.fed/index.en.md"), dest, "GET",
                       url::Origin::Create(GURL(kWeb)));
    ASSERT_TRUE(client->response_head());
    EXPECT_EQ(client->response_head()->headers->response_code(), 403);
    std::string body;
    ASSERT_TRUE(mojo::BlockingCopyToString(client->TakeBody(), &body));
    EXPECT_NE(body.find("fed:// only"), std::string::npos);
    EXPECT_EQ(body.find("secret"), std::string::npos);
  }
  EXPECT_TRUE(seen_.loads.empty()) << "the chain was never asked";
  EXPECT_TRUE(web_seen_.empty()) << "nor the network";
}

TEST_F(AtlasFedUrlLoaderFactoryTest, NoInitiatorAndAnOpaqueWebInitiatorAreNotFedPages) {
  FedResult answer;
  answer.status = 200;
  auto proxy = Proxy(answer);
  Load(proxy, GURL("http://home.fed/a"), network::mojom::RequestDestination::kEmpty,
       "GET", std::nullopt);
  Load(proxy, GURL("http://home.fed/a"), network::mojom::RequestDestination::kEmpty,
       "GET", url::Origin::Create(GURL(kWeb)).DeriveNewOpaqueOrigin());
  EXPECT_TRUE(seen_.loads.empty());
  // An opaque origin made by a fed page is still a fed page's.
  Load(proxy, GURL("http://home.fed/a"), network::mojom::RequestDestination::kEmpty,
       "GET", FedOrigin().DeriveNewOpaqueOrigin());
  EXPECT_EQ(seen_.loads.size(), 1u);
  EXPECT_TRUE(web_seen_.empty());
}

TEST_F(AtlasFedUrlLoaderFactoryTest, AFrameOfAFederateTldIsAllowedFromAnyPageAsADocument) {
  FedResult answer;
  answer.status = 200;
  answer.mime = "text/html";
  auto proxy = Proxy(answer);
  Load(proxy, GURL("https://lobo.pagina/x"),
       network::mojom::RequestDestination::kIframe, "GET",
       url::Origin::Create(GURL(kWeb)));
  ASSERT_EQ(seen_.loads.size(), 1u);
  EXPECT_EQ(seen_.loads[0].first, "fed://lobo.pagina/x");
  EXPECT_EQ(seen_.loads[0].second, LoadKind::kDocument);
  EXPECT_TRUE(web_seen_.empty());
}

TEST_F(AtlasFedUrlLoaderFactoryTest, ANameThatIsNotLabelDotTldIsStillNotTheWeb) {
  FedResult answer;
  answer.status = 400;
  answer.mime = "text/html";
  auto proxy = Proxy(answer);
  Load(proxy, GURL("http://www.home.fed/"),
       network::mojom::RequestDestination::kScript);
  ASSERT_EQ(seen_.loads.size(), 1u);
  EXPECT_EQ(seen_.loads[0].first, "fed://www.home.fed/");
  EXPECT_TRUE(web_seen_.empty());
}

TEST_F(AtlasFedUrlLoaderFactoryTest, AFailedChainIsAnErrorNeverAFallbackToTheWeb) {
  FedResult answer;
  answer.net_error = net::ERR_CONNECTION_REFUSED;
  auto proxy = Proxy(answer);
  auto client = Load(proxy, GURL("https://home.fed/a.png"),
                     network::mojom::RequestDestination::kImage);
  EXPECT_EQ(client->completion_status().error_code,
            net::ERR_CONNECTION_REFUSED);
  EXPECT_TRUE(web_seen_.empty());
}

TEST_F(AtlasFedUrlLoaderFactoryTest, OtherRequestsPassToTheWebUntouched) {
  FedResult answer;
  answer.status = 200;
  auto proxy = Proxy(answer);
  Load(proxy, GURL("https://example.com/a.js"),
       network::mojom::RequestDestination::kScript, "GET",
       url::Origin::Create(GURL(kWeb)));
  Load(proxy, GURL("fed://home.fed/b.js"),
       network::mojom::RequestDestination::kScript);
  EXPECT_EQ(web_seen_,
            (std::vector<std::string>{"https://example.com/a.js",
                                      "fed://home.fed/b.js"}));
  EXPECT_TRUE(seen_.loads.empty());
}

}  // namespace atlas_fed

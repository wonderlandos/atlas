// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A Federate name is exclusively fed:// (lead rule 2026-11-05): the redirect of
// navigations has no setting, the DNS prefetch and preconnect predictors never
// see a Federate TLD host, and the network service's resolver refuses them.

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/pattern.h"
#include "base/strings/string_split.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/atlas/fed/fed_dns_guard.h"
#include "chrome/browser/atlas/fed/fed_http_interceptor.h"
#include "chrome/browser/atlas/fed/fed_preconnect_guard.h"
#include "chrome/browser/atlas/fed/fed_url.h"
#include "content/public/browser/preconnect_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace atlas_fed {

namespace {

// Records what reaches the real manager.
class RecordingManager : public content::PreconnectManager {
 public:
  void Start(const GURL& url,
             std::vector<content::PreconnectRequest> requests,
             net::NetworkTrafficAnnotationTag) override {
    started.push_back(url.spec());
    for (const auto& r : requests) {
      started.push_back("req:" + r.origin.Serialize());
    }
  }
  void StartPreresolveHost(const GURL& url,
                           const net::NetworkAnonymizationKey&,
                           net::NetworkTrafficAnnotationTag,
                           const content::StoragePartitionConfig*,
                           const base::UnguessableToken&) override {
    resolved.push_back(url.spec());
  }
  void StartPreresolveHosts(const std::vector<GURL>& urls,
                            const net::NetworkAnonymizationKey&,
                            net::NetworkTrafficAnnotationTag,
                            const content::StoragePartitionConfig*,
                            const base::UnguessableToken&) override {
    for (const GURL& u : urls) {
      resolved.push_back(u.spec());
    }
  }
  void StartPreconnectUrl(
      const GURL& url,
      bool,
      net::NetworkAnonymizationKey,
      net::NetworkTrafficAnnotationTag,
      const content::StoragePartitionConfig*,
      const base::UnguessableToken&,
      std::optional<net::ConnectionKeepAliveConfig>,
      mojo::PendingRemote<network::mojom::ConnectionChangeObserverClient>)
      override {
    preconnected.push_back(url.spec());
  }
  void Stop(const GURL& url) override { stopped.push_back(url.spec()); }
  base::WeakPtr<content::PreconnectManager> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }
  void SetNetworkContextForTesting(network::mojom::NetworkContext*) override {}
  void SetObserverForTesting(Observer*) override {}

  std::vector<std::string> started, resolved, preconnected, stopped;

 private:
  base::WeakPtrFactory<RecordingManager> weak_factory_{this};
};

struct Guarded {
  Guarded() {
    auto real = std::make_unique<RecordingManager>();
    manager = real.get();
    guard = FedPreconnectGuard::Wrap(std::move(real));
  }
  raw_ptr<RecordingManager> manager;
  std::unique_ptr<content::PreconnectManager> guard;
};

const net::NetworkAnonymizationKey& Key() {
  static const base::NoDestructor<net::NetworkAnonymizationKey> k;
  return *k;
}
const base::UnguessableToken& Token() {
  static const base::UnguessableToken t;
  return t;
}

}  // namespace

TEST(AtlasFedExclusiveTest, ANavigationToAFederateTldHasNoSetting) {
  EXPECT_EQ(RedirectTargetFor(GURL("http://home.fed/a")), GURL("fed://home.fed/a"));
  EXPECT_EQ(RedirectTargetFor(GURL("https://www.home.fed/")),
            GURL("fed://www.home.fed"));
  EXPECT_FALSE(RedirectTargetFor(GURL("https://example.com/")));
}

TEST(AtlasFedExclusiveTest, TheDnsPrefetchOfAFederateHostNeverReachesTheManager) {
  Guarded g;
  g.guard->StartPreresolveHost(GURL("http://home.fed/"), Key(),
                               TRAFFIC_ANNOTATION_FOR_TESTS, nullptr, Token());
  g.guard->StartPreresolveHost(GURL("https://example.com/"), Key(),
                               TRAFFIC_ANNOTATION_FOR_TESTS, nullptr, Token());
  g.guard->StartPreresolveHosts(
      {GURL("https://www.lobo.pagina/"), GURL("https://example.org/"),
       GURL("http://x.azul:81/")},
      Key(), TRAFFIC_ANNOTATION_FOR_TESTS, nullptr, Token());
  g.guard->StartPreresolveHosts({GURL("http://home.fed/")}, Key(),
                                TRAFFIC_ANNOTATION_FOR_TESTS, nullptr, Token());
  EXPECT_EQ(g.manager->resolved,
            (std::vector<std::string>{"https://example.com/",
                                      "https://example.org/"}));
}

TEST(AtlasFedExclusiveTest, ThePreconnectToAFederateHostNeverReachesTheManager) {
  Guarded g;
  g.guard->StartPreconnectUrl(GURL("https://home.fed/"), true, Key(),
                              TRAFFIC_ANNOTATION_FOR_TESTS, nullptr, Token(),
                              std::nullopt, mojo::NullRemote());
  g.guard->StartPreconnectUrl(GURL("https://example.com/"), true, Key(),
                              TRAFFIC_ANNOTATION_FOR_TESTS, nullptr, Token(),
                              std::nullopt, mojo::NullRemote());
  EXPECT_EQ(g.manager->preconnected,
            (std::vector<std::string>{"https://example.com/"}));
}

TEST(AtlasFedExclusiveTest, ALearnedPreconnectListDropsFederateOriginsOnly) {
  Guarded g;
  std::vector<content::PreconnectRequest> requests;
  // The restrictions id is given, so no command line is needed.
  requests.emplace_back(url::Origin::Create(GURL("https://cdn.example.com")), 1,
                        Key(), base::UnguessableToken::Create());
  requests.emplace_back(url::Origin::Create(GURL("https://home.fed")), 1, Key(),
                        base::UnguessableToken::Create());
  g.guard->Start(GURL("https://example.com/"), std::move(requests),
                 TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(g.manager->started,
            (std::vector<std::string>{"https://example.com/",
                                      "req:https://cdn.example.com"}));
  // A Federate page needs no connection at all.
  g.guard->Start(GURL("http://home.fed/"), {}, TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(g.manager->started.size(), 2u);
  g.guard->Stop(GURL("https://example.com/"));
  EXPECT_EQ(g.manager->stopped.size(), 1u);
}

TEST(AtlasFedExclusiveTest, TheResolverRefusesEveryFederateTldBeforeAnyQuery) {
  std::string rules = HostResolverRules("");
  for (std::string_view tld : KnownTlds()) {
    EXPECT_NE(rules.find("MAP *." + std::string(tld) + " ~NOTFOUND"),
              std::string::npos)
        << tld;
  }
  EXPECT_NE(rules.back(), ',');
  // The fully-qualified spelling is fenced too.
  for (std::string_view tld : KnownTlds()) {
    EXPECT_NE(rules.find("MAP *." + std::string(tld) + ". ~NOTFOUND"),
              std::string::npos)
        << tld;
  }
  EXPECT_EQ(HostResolverRules("MAP a.test b.test").substr(rules.size() + 1),
            "MAP a.test b.test");

  base::CommandLine cl(base::CommandLine::NO_PROGRAM);
  cl.AppendSwitchASCII(network::switches::kHostResolverRules, "MAP a.test b.test");
  AppendDnsGuard(&cl);
  std::string once = cl.GetSwitchValueASCII(network::switches::kHostResolverRules);
  EXPECT_NE(once.find("MAP *.fed ~NOTFOUND"), std::string::npos);
  EXPECT_NE(once.find("MAP a.test b.test"), std::string::npos);
  AppendDnsGuard(&cl);
  EXPECT_EQ(cl.GetSwitchValueASCII(network::switches::kHostResolverRules), once);
}

// QA-A9-WEB-ATLAS-5: the two fences must agree. A host the proxies refuse
// (IsFederateTldHost, trailing dot ignored) is also unresolvable for the network
// service's host-resolver rules, which only see the host they are given
// (WebSocket, WebTransport, TURN never pass the loader proxies). The rules are
// matched like net::HostMappingRules does: base::MatchPattern, first MAP wins.
namespace {
bool ResolverRefuses(std::string_view host) {
  std::string rules = HostResolverRules("");
  for (std::string_view rule : base::SplitStringPiece(
           rules, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    std::vector<std::string_view> parts = base::SplitStringPiece(
        rule, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (parts.size() == 3 && parts[0] == "MAP" &&
        base::MatchPattern(host, parts[1])) {
      return parts[2] == "~NOTFOUND";
    }
  }
  return false;
}
}  // namespace

TEST(AtlasFedExclusiveTest, TheResolverAndTheLoaderFencesAgreeOnEveryHostSpelling) {
  for (std::string_view host :
       {"home.fed", "www.home.fed", "home.fed.", "www.home.fed.", "x.pagina.",
        "x.pagina", "lobo.oi.", "fed", "fed.", "home.fedx", "example.com",
        "example.com.", "home.fed.example.com", "home.com."}) {
    EXPECT_EQ(IsFederateTldHost(host), ResolverRefuses(host)) << host;
  }
  // Hosts of a URL are lower case (GURL), so the rules need no case folding.
  EXPECT_TRUE(ResolverRefuses(GURL("ws://HOME.FED./").host()));
}

}  // namespace atlas_fed

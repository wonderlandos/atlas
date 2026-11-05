// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// QA-A9-WEB-ATLAS-1: a Federate name typed in the address bar (or the new tab
// page's field, which asks the same classifier) is an address, never a search.
// The 23 Federate TLDs are not in the engine's registry data, so without
// overlay patch 0124 AutocompleteInput::Parse() answered UNKNOWN and the default
// match was the search provider: the name left the machine.

#include <string>

#include "components/atlas_fed_tlds/fed_tlds.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace atlas_fed {

namespace {

// What ChromeAutocompleteSchemeClassifier says in the browser: the schemes the
// browser handles are URLs; "fed" is one of them (patch 0120).
class SchemeClassifier : public AutocompleteSchemeClassifier {
 public:
  metrics::OmniboxInputType GetInputTypeForScheme(
      const std::string& scheme) const override {
    for (const char* known : {"http", "https", "ws", "wss", "file", "about",
                              "ftp", "blob", "javascript", "chrome", "fed"}) {
      if (scheme == known) {
        return metrics::OmniboxInputType::URL;
      }
    }
    return metrics::OmniboxInputType::EMPTY;
  }
};

struct Parsed {
  metrics::OmniboxInputType type;
  GURL canonical;
};

Parsed Classify(const std::u16string& text) {
  SchemeClassifier classifier;
  Parsed out;
  out.type = AutocompleteInput::Parse(text, std::string(), classifier, nullptr,
                                      nullptr, &out.canonical);
  return out;
}

class AtlasFedOmniboxTest : public testing::Test {
 public:
  AtlasFedOmniboxTest() {
    // The browser registers fed as a standard scheme (patch 0120).
    url::AddStandardScheme("fed", url::SCHEME_WITH_HOST);
  }

 private:
  url::ScopedSchemeRegistryForTests registry_;
};

}  // namespace

TEST_F(AtlasFedOmniboxTest, ABareFederateNameIsAnAddressNotASearch) {
  Parsed p = Classify(u"home.fed");
  EXPECT_EQ(p.type, metrics::OmniboxInputType::URL);
  EXPECT_EQ(p.canonical, GURL("fed://home.fed/"));

  p = Classify(u"lobo.pagina");
  EXPECT_EQ(p.type, metrics::OmniboxInputType::URL);
  EXPECT_EQ(p.canonical, GURL("fed://lobo.pagina/"));
}

TEST_F(AtlasFedOmniboxTest, EveryKnownTldIsAnAddress) {
  for (std::string_view tld : KnownTlds()) {
    std::u16string text = u"nome.";
    text.append(tld.begin(), tld.end());
    Parsed p = Classify(text);
    EXPECT_EQ(p.type, metrics::OmniboxInputType::URL) << tld;
    EXPECT_EQ(p.canonical.scheme(), "fed") << tld;
    EXPECT_EQ(p.canonical.host(), "nome." + std::string(tld)) << tld;
  }
}

TEST_F(AtlasFedOmniboxTest, PathQueryCaseAndSpellingsKeepTheAddress) {
  EXPECT_EQ(Classify(u"home.fed/a/b?q=1").canonical,
            GURL("fed://home.fed/a/b?q=1"));
  EXPECT_EQ(Classify(u"Home.FED").canonical, GURL("fed://home.fed/"));
  EXPECT_EQ(Classify(u"home.fed.").canonical, GURL("fed://home.fed/"));
  EXPECT_EQ(Classify(u"http://home.fed").type, metrics::OmniboxInputType::URL);
  EXPECT_EQ(Classify(u"http://home.fed").canonical, GURL("fed://home.fed/"));
  EXPECT_EQ(Classify(u"https://home.fed:8443/x").canonical,
            GURL("fed://home.fed/x"));
  // A name with more labels is still a Federate TLD host: the helper answers
  // with a refusal page, the web never sees it.
  Parsed p = Classify(u"www.home.fed");
  EXPECT_EQ(p.type, metrics::OmniboxInputType::URL);
  EXPECT_EQ(p.canonical.scheme(), "fed");
}

TEST_F(AtlasFedOmniboxTest, AnExplicitFedAddressStaysAnAddress) {
  Parsed p = Classify(u"fed://home.fed");
  EXPECT_EQ(p.type, metrics::OmniboxInputType::URL);
  EXPECT_EQ(p.canonical, GURL("fed://home.fed/"));
}

TEST_F(AtlasFedOmniboxTest, EverythingElseKeepsItsClassification) {
  // An unknown TLD is still a guess that defaults to the search provider.
  EXPECT_EQ(Classify(u"foo.zzzz").type, metrics::OmniboxInputType::UNKNOWN);
  EXPECT_EQ(Classify(u"home.fedx").type, metrics::OmniboxInputType::UNKNOWN);
  // The TLD alone, a word, and a sentence are not Federate names.
  EXPECT_NE(Classify(u"fed").type, metrics::OmniboxInputType::URL);
  EXPECT_NE(Classify(u"home fed").type, metrics::OmniboxInputType::URL);
  EXPECT_EQ(Classify(u"minha casa.fed azul").type,
            metrics::OmniboxInputType::QUERY);
  // A Federate TLD that is a suffix of another one is not one.
  EXPECT_EQ(Classify(u"home.xfed").type, metrics::OmniboxInputType::UNKNOWN);
  // The web keeps working as before.
  EXPECT_EQ(Classify(u"example.com").type, metrics::OmniboxInputType::URL);
  EXPECT_EQ(Classify(u"example.com").canonical, GURL("http://example.com/"));
  EXPECT_EQ(Classify(u"https://example.com/x").canonical,
            GURL("https://example.com/x"));
}

}  // namespace atlas_fed

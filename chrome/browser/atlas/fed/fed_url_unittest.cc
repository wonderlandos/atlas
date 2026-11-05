// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/atlas/fed/fed_url.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace atlas_fed {

TEST(AtlasFedUrlTest, ThereAreTwentyThreeKnownTlds) {
  EXPECT_EQ(KnownTlds().size(), 23u);
  EXPECT_TRUE(IsKnownTld("fed"));
  EXPECT_TRUE(IsKnownTld("azul"));
  EXPECT_FALSE(IsKnownTld("com"));
  EXPECT_FALSE(IsKnownTld("org"));
  EXPECT_FALSE(IsKnownTld(""));
}

TEST(AtlasFedUrlTest, FederateHostIsOneLabelAndOneTld) {
  EXPECT_TRUE(IsFederateHost("home.fed"));
  EXPECT_TRUE(IsFederateHost("a-b1.azul"));
  EXPECT_FALSE(IsFederateHost("fed"));
  EXPECT_FALSE(IsFederateHost("www.home.fed"));
  EXPECT_FALSE(IsFederateHost("-a.fed"));
  EXPECT_FALSE(IsFederateHost("a-.fed"));
  EXPECT_FALSE(IsFederateHost("A.fed"));
  EXPECT_FALSE(IsFederateHost("a.f"));
  EXPECT_FALSE(IsFederateHost(".fed"));
}

TEST(AtlasFedUrlTest, HttpToFedKeepsPathAndQueryAndDropsPortAndFragment) {
  EXPECT_EQ(MapHttpToFed(GURL("http://home.fed/")), GURL("fed://home.fed"));
  EXPECT_EQ(MapHttpToFed(GURL("https://home.fed:8443/a/b?q=1#frag")),
            GURL("fed://home.fed/a/b?q=1"));
  EXPECT_EQ(MapHttpToFed(GURL("HTTPS://Home.FED/x")),
            GURL("fed://home.fed/x"));
}

TEST(AtlasFedUrlTest, OnlyKnownTldsAndPlainHttpAreMapped) {
  EXPECT_FALSE(MapHttpToFed(GURL("https://example.com/")));
  EXPECT_FALSE(MapHttpToFed(GURL("https://example.com.au/")));
  EXPECT_FALSE(MapHttpToFed(GURL("https://fed.example.com/")));
  EXPECT_FALSE(MapHttpToFed(GURL("ftp://home.fed/")));
  EXPECT_FALSE(MapHttpToFed(GURL("fed://home.fed/")));
  EXPECT_FALSE(MapHttpToFed(GURL()));
}

// A Federate name is exclusively fed:// (lead rule 2026-11-05): every host under
// a known TLD is spelled fed://, valid name or not, and there is no setting.
TEST(AtlasFedUrlTest, EveryHostUnderAKnownTldIsMappedNeverLeftToTheWeb) {
  EXPECT_EQ(MapHttpToFed(GURL("https://www.home.fed/a?b=1")),
            GURL("fed://www.home.fed/a?b=1"));
  EXPECT_EQ(MapHttpToFed(GURL("https://user:pw@home.fed/")),
            GURL("fed://home.fed"));
  EXPECT_EQ(MapHttpToFed(GURL("http://a.b.c.pagina/")),
            GURL("fed://a.b.c.pagina"));
  EXPECT_EQ(MapHttpToFed(GURL("http://home.fed./x")), GURL("fed://home.fed/x"));
  EXPECT_TRUE(MapHttpToFed(GURL("http://_x.fed/")));
}

TEST(AtlasFedUrlTest, TheTldDecidesNotTheShapeOfTheName) {
  EXPECT_TRUE(IsFederateTldHost("home.fed"));
  EXPECT_TRUE(IsFederateTldHost("www.home.fed"));
  EXPECT_TRUE(IsFederateTldHost("home.fed."));
  EXPECT_FALSE(IsFederateTldHost("fed"));
  EXPECT_FALSE(IsFederateTldHost("home.fedx"));
  EXPECT_FALSE(IsFederateTldHost("example.com"));
  EXPECT_FALSE(IsFederateTldHost(""));
  EXPECT_TRUE(IsFederateTldUrl(GURL("wss://x.azul/socket")));
  EXPECT_TRUE(IsFederateTldUrl(GURL("http://x.pagina:8080/")));
  EXPECT_FALSE(IsFederateTldUrl(GURL("https://example.com/")));
  EXPECT_FALSE(IsFederateTldUrl(GURL()));
}

}  // namespace atlas_fed

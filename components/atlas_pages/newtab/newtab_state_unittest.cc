// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/atlas_pages/newtab/newtab_state.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/atlas_pages/newtab/newtab_categories.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace atlas::newtab {

namespace {

base::ListValue ListFromJson(const char* json) {
  std::optional<base::Value> value = base::JSONReader::Read(json);
  CHECK(value && value->is_list());
  return std::move(value->GetList());
}

}  // namespace

TEST(AtlasNewTabCategoriesTest, SixCategoriesAndTheDefault) {
  EXPECT_EQ("talk", CategoryForHost("mail.example.org"));
  EXPECT_EQ("talk", CategoryForHost("webmail.example.org"));
  EXPECT_EQ("files", CategoryForHost("files.example.org"));
  EXPECT_EQ("files", CategoryForHost("cloud.example.org"));
  EXPECT_EQ("media", CategoryForHost("video.example.org"));
  EXPECT_EQ("store", CategoryForHost("store.example.org"));
  EXPECT_EQ("system", CategoryForHost("settings.example.org"));
  EXPECT_EQ("web", CategoryForHost("home.example.org"));
  EXPECT_EQ("web", CategoryForHost("example.org"));
  EXPECT_EQ("web", CategoryForHost(""));
}

TEST(AtlasNewTabCategoriesTest, PublicSuffixNeverCounts) {
  // "tv" and "store" as the last label are a public suffix, not a word.
  EXPECT_EQ("web", CategoryForHost("example.tv"));
  EXPECT_EQ("web", CategoryForHost("example.store"));
  EXPECT_EQ("media", CategoryForHost("example.tv.example.com"));
}

TEST(AtlasNewTabCategoriesTest, FromUrl) {
  EXPECT_EQ("talk", CategoryForUrl(GURL("https://Mail.Example.org/inbox")));
  EXPECT_EQ("web", CategoryForUrl(GURL()));
  EXPECT_EQ("web", CategoryForUrl(GURL("not a url")));
}

TEST(AtlasNewTabStateTest, OnlyWebAddresses) {
  EXPECT_TRUE(IsWebUrl(GURL("https://a.example/")));
  EXPECT_TRUE(IsWebUrl(GURL("http://a.example/")));
  EXPECT_FALSE(IsWebUrl(GURL("chrome://settings/")));
  EXPECT_FALSE(IsWebUrl(GURL("javascript:alert(1)")));
  EXPECT_FALSE(IsWebUrl(GURL("file:///etc/passwd")));
  EXPECT_FALSE(IsWebUrl(GURL("data:text/html,x")));
  EXPECT_FALSE(IsWebUrl(GURL()));
}

TEST(AtlasNewTabStateTest, ParsePinnedDropsWhatIsNotAWebPage) {
  base::ListValue list = ListFromJson(R"([
    {"title": "Home", "url": "https://home.example/"},
    {"title": "Settings", "url": "chrome://settings/"},
    {"title": "Again", "url": "https://home.example/"},
    "text",
    {"title": "No url"},
    {"url": "https://mail.example/"}
  ])");
  std::vector<PinnedPage> pages = ParsePinned(list);
  ASSERT_EQ(2u, pages.size());
  EXPECT_EQ("Home", pages[0].title);
  EXPECT_EQ(GURL("https://home.example/"), pages[0].url);
  EXPECT_EQ("", pages[1].title);
  EXPECT_EQ(GURL("https://mail.example/"), pages[1].url);
}

TEST(AtlasNewTabStateTest, ParsePinnedStopsAtTheLimit) {
  base::ListValue list;
  for (size_t i = 0; i < kMaxPinned + 5; ++i) {
    list.Append(base::DictValue().Set(
        "url", "https://p" + std::to_string(i) + ".example/"));
  }
  EXPECT_EQ(kMaxPinned, ParsePinned(list).size());
}

TEST(AtlasNewTabStateTest, TitlesAreCleaned) {
  base::ListValue list = ListFromJson(
      R"([{"title": "  A\nB\t", "url": "https://a.example/"}])");
  std::vector<PinnedPage> pages = ParsePinned(list);
  ASSERT_EQ(1u, pages.size());
  EXPECT_EQ("A B", pages[0].title);
}

TEST(AtlasNewTabStateTest, PinAndUnpinRoundTrip) {
  bool changed = false;
  std::vector<PinnedPage> pages;
  pages = WithPinned(std::move(pages), GURL("https://a.example/"), "A",
                     &changed);
  EXPECT_TRUE(changed);
  pages = WithPinned(std::move(pages), GURL("https://a.example/"), "A",
                     &changed);
  EXPECT_FALSE(changed);  // already there
  pages = WithPinned(std::move(pages), GURL("chrome://flags/"), "Flags",
                     &changed);
  EXPECT_FALSE(changed);  // not a web page
  ASSERT_EQ(1u, pages.size());

  base::ListValue list = PinnedToList(pages);
  std::vector<PinnedPage> again = ParsePinned(list);
  ASSERT_EQ(1u, again.size());
  EXPECT_EQ(pages[0].url, again[0].url);

  pages = WithoutPinned(std::move(pages), GURL("https://b.example/"), &changed);
  EXPECT_FALSE(changed);
  pages = WithoutPinned(std::move(pages), GURL("https://a.example/"), &changed);
  EXPECT_TRUE(changed);
  EXPECT_TRUE(pages.empty());
}

TEST(AtlasNewTabStateTest, PinningStopsWhenFull) {
  std::vector<PinnedPage> pages;
  bool changed = false;
  for (size_t i = 0; i < kMaxPinned; ++i) {
    pages = WithPinned(std::move(pages),
                       GURL("https://p" + std::to_string(i) + ".example/"), "",
                       &changed);
    EXPECT_TRUE(changed);
  }
  pages = WithPinned(std::move(pages), GURL("https://more.example/"), "",
                     &changed);
  EXPECT_FALSE(changed);
  EXPECT_EQ(kMaxPinned, pages.size());
}

TEST(AtlasNewTabStateTest, PickRecentKeepsOrderAndSkipsRepeats) {
  base::Time now = base::Time::FromMillisecondsSinceUnixEpoch(1'000'000);
  std::vector<RecentPage> in = {
      {u"New tab", GURL("chrome://new-tab-page/"), now},
      {u"Notes", GURL("https://notes.example/"), now - base::Minutes(20)},
      {u"Notes again", GURL("https://notes.example/"), now - base::Hours(2)},
      {u"Trains", GURL("https://trains.example/"), now - base::Days(1)},
  };
  std::vector<RecentPage> out = PickRecent(in);
  ASSERT_EQ(2u, out.size());
  EXPECT_EQ(u"Notes", out[0].title);
  EXPECT_EQ(u"Trains", out[1].title);
}

TEST(AtlasNewTabStateTest, StateForThePage) {
  base::Time when = base::Time::FromMillisecondsSinceUnixEpoch(1'700'000'000'000);
  std::vector<PinnedPage> pinned = {
      {"Mail", GURL("https://mail.example.org/")},
      {"Home", GURL("https://home.example.org/")}};
  std::vector<RecentPage> recent = {
      {u"Release notes", GURL("https://notes.example.org/r"), when}};
  base::DictValue state = BuildState(pinned, recent, true, false, true);

  const base::ListValue* p = state.FindList("pinned");
  ASSERT_TRUE(p);
  ASSERT_EQ(2u, p->size());
  EXPECT_EQ("talk", *(*p)[0].GetDict().FindString("category"));
  EXPECT_EQ("web", *(*p)[1].GetDict().FindString("category"));
  EXPECT_EQ("https://mail.example.org/", *(*p)[0].GetDict().FindString("url"));

  const base::ListValue* r = state.FindList("recent");
  ASSERT_TRUE(r);
  ASSERT_EQ(1u, r->size());
  EXPECT_EQ("Release notes", *(*r)[0].GetDict().FindString("title"));
  EXPECT_EQ(1'700'000'000'000.0, *(*r)[0].GetDict().FindDouble("when"));

  EXPECT_EQ(true, state.FindBool("doNotTrack"));
  EXPECT_EQ(false, state.FindBool("statusStrip"));
  EXPECT_EQ(true, state.FindBool("firstInWindow"));
}

}  // namespace atlas::newtab

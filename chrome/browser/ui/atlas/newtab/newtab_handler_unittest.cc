// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/newtab/newtab_handler.h"

#include <memory>
#include <vector>

#include "base/time/time.h"
#include "components/sessions/content/content_test_helper.h"
#include "components/sessions/core/tab_restore_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace atlas::newtab {

namespace {

std::unique_ptr<sessions::tab_restore::Tab> MakeTab(const char* title,
                                                    const char* url,
                                                    base::Time when) {
  auto tab = std::make_unique<sessions::tab_restore::Tab>();
  tab->timestamp = when;
  tab->current_navigation_index = 0;
  tab->navigations.push_back(sessions::ContentTestHelper::CreateNavigation(
      url, title));
  return tab;
}

}  // namespace

// AtlasNewTab: closed tabs become the pages to pick up, newest first, web
// pages only, one per address.
TEST(AtlasNewTabHandlerTest, RecentFromClosedTabsAndWindows) {
  base::Time now = base::Time::FromMillisecondsSinceUnixEpoch(1'000'000'000);
  sessions::TabRestoreService::Entries entries;
  entries.push_back(MakeTab("New tab", "chrome://new-tab-page/", now));
  entries.push_back(MakeTab("Notes", "https://notes.example/", now - base::Minutes(20)));
  auto window = std::make_unique<sessions::tab_restore::Window>();
  window->timestamp = now - base::Days(1);
  window->tabs.push_back(MakeTab("Trains", "https://trains.example/", now));
  window->tabs.push_back(MakeTab("Notes", "https://notes.example/", now));
  entries.push_back(std::move(window));

  std::vector<RecentPage> recent = NewTabHandler::RecentFrom(entries);
  ASSERT_EQ(2u, recent.size());
  EXPECT_EQ(u"Notes", recent[0].title);
  EXPECT_EQ(now - base::Minutes(20), recent[0].when);
  EXPECT_EQ(u"Trains", recent[1].title);
  // A tab of a closed window carries the window's time.
  EXPECT_EQ(now - base::Days(1), recent[1].when);
}

}  // namespace atlas::newtab

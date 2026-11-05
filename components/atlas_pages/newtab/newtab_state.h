// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATLAS_PAGES_NEWTAB_NEWTAB_STATE_H_
#define COMPONENTS_ATLAS_PAGES_NEWTAB_NEWTAB_STATE_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/values.h"
#include "url/gurl.h"

namespace atlas::newtab {

// At most this many pinned pages are kept and shown.
inline constexpr size_t kMaxPinned = 12;
// At most this many pages to pick up are sent to the page.
inline constexpr size_t kMaxRecent = 4;

struct PinnedPage {
  std::string title;  // UTF-8, may be empty (the page shows the host name)
  GURL url;           // http or https
};

struct RecentPage {
  std::u16string title;
  GURL url;  // http or https
  base::Time when;
};

// Only web addresses are pinned or picked up: an http or https URL that is
// valid and has a host.
bool IsWebUrl(const GURL& url);

// The pinned pages of the `atlas.newtab.pinned` pref value: a list of
// dictionaries {"title": string, "url": string}. Entries that are not
// dictionaries, have no web URL, or repeat an earlier URL are dropped; the
// result has at most kMaxPinned entries, in list order.
std::vector<PinnedPage> ParsePinned(const base::ListValue& list);

// The pref value for `pages` (the inverse of ParsePinned).
base::ListValue PinnedToList(const std::vector<PinnedPage>& pages);

// `pages` with `url` added at the end (title may be empty), unless it is
// already there, is not a web URL, or the list is full. `changed` tells
// whether the list is different.
std::vector<PinnedPage> WithPinned(std::vector<PinnedPage> pages,
                                   const GURL& url,
                                   const std::string& title,
                                   bool* changed);

// `pages` without `url`.
std::vector<PinnedPage> WithoutPinned(std::vector<PinnedPage> pages,
                                      const GURL& url,
                                      bool* changed);

// The pages to pick up from `candidates` (newest first, as the tab restore
// list keeps them): web URLs only, one per URL, never the new tab page,
// at most kMaxRecent.
std::vector<RecentPage> PickRecent(const std::vector<RecentPage>& candidates);

// The state the page asks for (README.md, "The host contract"):
//   {pinned: [{title, url, category}], recent: [{title, url, when}],
//    doNotTrack, statusStrip, firstInWindow, strings}
// `when` is milliseconds since 1970. `strings` is added by the caller.
base::DictValue BuildState(const std::vector<PinnedPage>& pinned,
                           const std::vector<RecentPage>& recent,
                           bool do_not_track,
                           bool status_strip,
                           bool first_in_window);

}  // namespace atlas::newtab

#endif  // COMPONENTS_ATLAS_PAGES_NEWTAB_NEWTAB_STATE_H_

// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATLAS_PAGES_NEWTAB_NEWTAB_STATE_H_
#define COMPONENTS_ATLAS_PAGES_NEWTAB_NEWTAB_STATE_H_

#include <optional>
#include <string>
#include <string_view>
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
  GURL url;           // http, https or fed
};

struct RecentPage {
  std::u16string title;
  GURL url;  // http or https
  base::Time when;
};

// Only web addresses are pinned or picked up: an http, https or fed URL that is
// valid and has a host. A Federate page (fed://home.fed) is a first-class page
// here as everywhere else (ADR 0052); its host is a Federate name, so a fed URL
// of any other host is not one (QA-A9-WEB-ATLAS-15).
bool IsWebUrl(const GURL& url);

// The fed:// address that `text` (what a person typed in the page's field)
// names, if it names a Federate page: a bare name (home.fed, lobo.pagina/x), an
// http(s) spelling of one, or a fed:// address. nullopt for anything else,
// which is then a search or another address. The new tab page opens this
// address directly, so a Federate name never goes to the search provider
// whatever the address bar's classifier says (QA-A9-WEB-ATLAS-1).
std::optional<GURL> FederateAddressFor(std::string_view text);

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

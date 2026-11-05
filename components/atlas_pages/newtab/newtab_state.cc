// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/atlas_pages/newtab/newtab_state.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/atlas_pages/newtab/newtab_categories.h"

namespace atlas::newtab {

namespace {

// The title as the page shows it: no control characters, trimmed, and cut at
// 200 characters so a long title cannot grow the pref.
std::string CleanTitle(std::string_view title) {
  std::string out;
  for (char c : title) {
    out.push_back(static_cast<unsigned char>(c) < 0x20 ? ' ' : c);
  }
  std::string trimmed;
  base::TrimWhitespaceASCII(out, base::TRIM_ALL, &trimmed);
  if (trimmed.size() > 200) {
    trimmed.resize(200);
    // Do not leave half of a UTF-8 sequence at the end.
    while (!trimmed.empty() && !base::IsStringUTF8(trimmed)) {
      trimmed.pop_back();
    }
  }
  return trimmed;
}

}  // namespace

bool IsWebUrl(const GURL& url) {
  return url.is_valid() && url.has_host() &&
         (url.SchemeIs("https") || url.SchemeIs("http"));
}

std::vector<PinnedPage> ParsePinned(const base::ListValue& list) {
  std::vector<PinnedPage> pages;
  std::set<GURL> seen;
  for (const base::Value& item : list) {
    const base::DictValue* dict = item.GetIfDict();
    if (!dict) {
      continue;
    }
    const std::string* url_text = dict->FindString("url");
    if (!url_text) {
      continue;
    }
    GURL url(*url_text);
    if (!IsWebUrl(url) || !seen.insert(url).second) {
      continue;
    }
    const std::string* title = dict->FindString("title");
    pages.push_back({title ? CleanTitle(*title) : std::string(), url});
    if (pages.size() == kMaxPinned) {
      break;
    }
  }
  return pages;
}

base::ListValue PinnedToList(const std::vector<PinnedPage>& pages) {
  base::ListValue list;
  for (const PinnedPage& page : pages) {
    list.Append(base::DictValue()
                    .Set("title", page.title)
                    .Set("url", page.url.spec()));
  }
  return list;
}

std::vector<PinnedPage> WithPinned(std::vector<PinnedPage> pages,
                                   const GURL& url,
                                   const std::string& title,
                                   bool* changed) {
  *changed = false;
  if (!IsWebUrl(url) || pages.size() >= kMaxPinned ||
      std::any_of(pages.begin(), pages.end(),
                  [&](const PinnedPage& p) { return p.url == url; })) {
    return pages;
  }
  pages.push_back({CleanTitle(title), url});
  *changed = true;
  return pages;
}

std::vector<PinnedPage> WithoutPinned(std::vector<PinnedPage> pages,
                                      const GURL& url,
                                      bool* changed) {
  size_t before = pages.size();
  std::erase_if(pages, [&](const PinnedPage& p) { return p.url == url; });
  *changed = pages.size() != before;
  return pages;
}

std::vector<RecentPage> PickRecent(const std::vector<RecentPage>& candidates) {
  std::vector<RecentPage> picked;
  std::set<GURL> seen;
  for (const RecentPage& page : candidates) {
    if (!IsWebUrl(page.url) || !seen.insert(page.url).second) {
      continue;
    }
    picked.push_back(page);
    if (picked.size() == kMaxRecent) {
      break;
    }
  }
  return picked;
}

base::DictValue BuildState(const std::vector<PinnedPage>& pinned,
                           const std::vector<RecentPage>& recent,
                           bool do_not_track,
                           bool status_strip,
                           bool first_in_window) {
  base::ListValue pinned_list;
  for (const PinnedPage& page : pinned) {
    pinned_list.Append(base::DictValue()
                           .Set("title", page.title)
                           .Set("url", page.url.spec())
                           .Set("category", std::string(CategoryForUrl(page.url))));
  }
  base::ListValue recent_list;
  for (const RecentPage& page : recent) {
    recent_list.Append(
        base::DictValue()
            .Set("title", base::UTF16ToUTF8(page.title))
            .Set("url", page.url.spec())
            .Set("when", page.when.InMillisecondsFSinceUnixEpoch()));
  }
  return base::DictValue()
      .Set("pinned", std::move(pinned_list))
      .Set("recent", std::move(recent_list))
      .Set("doNotTrack", do_not_track)
      .Set("statusStrip", status_strip)
      .Set("firstInWindow", first_in_window);
}

}  // namespace atlas::newtab

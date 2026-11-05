// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/atlas_pages/newtab/newtab_categories.h"

#include <array>
#include <string>
#include <string_view>

#include "base/strings/string_util.h"
#include "url/gurl.h"

namespace atlas::newtab {

namespace {

struct Rule {
  std::string_view category;
  // A host label (the parts between dots) matches a word when it is the word
  // or starts with it ("mail", "mail2", "webmail" through the "mail" word).
  std::array<std::string_view, 9> words;
};

// The first rule with a matching word wins, in this order. Words are
// lower case ASCII; an empty entry ends the list of its rule.
constexpr Rule kRules[] = {
    {"talk", {"mail", "webmail", "chat", "talk", "meet", "message", "call",
              "matrix", "forum"}},
    {"files", {"files", "drive", "cloud", "docs", "storage", "photos", "notes",
               "share", "sync"}},
    {"media", {"video", "tv", "music", "radio", "stream", "play", "podcast",
               "movies", "watch"}},
    {"store", {"store", "shop", "market", "buy", "apps", "deals", "cart", "",
               ""}},
    {"system", {"settings", "admin", "config", "panel", "console", "system",
                "status", "router", "setup"}},
};

bool LabelHasWord(std::string_view label, std::string_view word) {
  if (word.empty()) {
    return false;
  }
  return label.substr(0, word.size()) == word ||
         (label.size() > word.size() &&
          label.substr(label.size() - word.size()) == word);
}

}  // namespace

std::string_view CategoryForHost(std::string_view host) {
  // The registrable name and everything left of it count; the public suffix
  // ("com", "co.uk") never does, so "shop.example.store" is the shop label.
  for (const Rule& rule : kRules) {
    size_t start = 0;
    while (start <= host.size()) {
      size_t dot = host.find('.', start);
      std::string_view label =
          host.substr(start, dot == std::string_view::npos ? host.size() - start
                                                           : dot - start);
      // Skip the last label: it is the public suffix.
      bool is_last = dot == std::string_view::npos;
      if (!is_last) {
        for (std::string_view word : rule.words) {
          if (LabelHasWord(label, word)) {
            return rule.category;
          }
        }
      }
      if (is_last) {
        break;
      }
      start = dot + 1;
    }
  }
  return "web";
}

std::string_view CategoryForUrl(const GURL& url) {
  if (!url.is_valid() || !url.has_host()) {
    return "web";
  }
  std::string host = base::ToLowerASCII(url.host());
  // CategoryForHost returns views of static strings only.
  return CategoryForHost(host);
}

}  // namespace atlas::newtab

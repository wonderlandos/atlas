// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/atlas_fed_tlds/fed_tlds.h"

#include <algorithm>
#include <array>
#include <string>

#include "base/strings/string_util.h"

namespace atlas_fed {

namespace {

constexpr auto kKnownTlds = std::to_array<std::string_view>({
    "amarelo", "azul",  "blau",    "branco", "busca",   "cara",
    "comu",    "fed",   "foto",    "mosca",  "oi",      "pages",
    "pagina",  "pic",   "preto",   "records", "rosa",   "sound",
    "tipos",   "types", "verde",   "vid",    "weblog",
});

bool LabelOk(std::string_view l) {
  if (l.empty() || l.size() > 63 || l.front() == '-' || l.back() == '-') {
    return false;
  }
  return std::ranges::all_of(l, [](char c) {
    return base::IsAsciiLower(c) || base::IsAsciiDigit(c) || c == '-';
  });
}

bool TldOk(std::string_view t) {
  return t.size() >= 2 && t.size() <= 32 &&
         std::ranges::all_of(t, [](char c) { return base::IsAsciiLower(c); });
}

}  // namespace

base::span<const std::string_view> KnownTlds() {
  return kKnownTlds;
}

bool IsKnownTld(std::string_view tld) {
  return std::ranges::find(kKnownTlds, tld) != kKnownTlds.end();
}

bool IsFederateHost(std::string_view host) {
  size_t dot = host.find('.');
  if (dot == std::string_view::npos) {
    return false;
  }
  std::string_view label = host.substr(0, dot);
  std::string_view tld = host.substr(dot + 1);
  return tld.find('.') == std::string_view::npos && LabelOk(label) &&
         TldOk(tld);
}

bool IsFederateTldHost(std::string_view host) {
  if (!host.empty() && host.back() == '.') {
    host.remove_suffix(1);
  }
  size_t dot = host.rfind('.');
  if (dot == std::string_view::npos) {
    return false;
  }
  return IsKnownTld(host.substr(dot + 1));
}

bool IsFederateTldUrl(const GURL& url) {
  return url.is_valid() && url.has_host() && IsFederateTldHost(url.host());
}

std::optional<GURL> MapHttpToFed(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return std::nullopt;
  }
  // GURL lower-cases the host of a standard URL.
  std::string host(url.host());
  if (!IsFederateTldHost(host)) {
    return std::nullopt;
  }
  if (host.back() == '.') {
    host.pop_back();
  }
  std::string spec = std::string(kFedScheme) + "://" + host;
  std::string path(url.path());
  if (!path.empty() && path != "/") {
    spec += path;
  }
  if (url.has_query()) {
    spec += "?" + std::string(url.query());
  }
  GURL fed(spec);
  if (!fed.is_valid()) {
    // Never the web: a name the helper refuses with a page that says so.
    return GURL(std::string(kFedScheme) + "://refused.invalid");
  }
  return fed;
}

}  // namespace atlas_fed

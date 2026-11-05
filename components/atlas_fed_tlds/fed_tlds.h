// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATLAS_FED_TLDS_FED_TLDS_H_
#define COMPONENTS_ATLAS_FED_TLDS_FED_TLDS_H_

#include <optional>
#include <string_view>

#include "base/containers/span.h"
#include "url/gurl.h"

// Federate Network addresses (fed://label.tld/path) in Atlas. The resolver
// and every check live in the helper process atlas-fed (fed_helper_client.h);
// this file only knows the spelling, to route loads and map the network's
// compatibility spelling http://label.tld.
//
// ONE definition of the spelling and of the 23 TLDs, in a components/ target
// so that the browser's fed area (chrome/browser/atlas/fed, which re-exports it
// as fed_url.h) and the omnibox (components/omnibox, overlay patch 0124) read
// the same list: a name the loaders send to the helper is a name the address
// bar opens, never a web search.
namespace atlas_fed {

// The URL scheme (also registered, as a literal, in chrome_content_client.cc,
// chrome_content_renderer_client.cc, profile_io_data.cc and
// browser_process_impl.cc by overlay patch 0120).
inline constexpr char kFedScheme[] = "fed";

// The network's TLDs of the signed root zone of 2026-11-04, none of them a TLD
// of the IANA root on that date: the only hosts an http(s) page load is taken
// from the web for. The same list as KNOWN_TLDS in the helper's crate
// (federate::resolve::KNOWN_TLDS); a new TLD comes with an update of both.
base::span<const std::string_view> KnownTlds();

// Whether `tld` (lower case) is one of KnownTlds().
bool IsKnownTld(std::string_view tld);

// A Federate name: exactly one label (a-z, 0-9, hyphen, 1-63, no hyphen at
// the ends) and one TLD (a-z, 2-32).
bool IsFederateHost(std::string_view host);

// Whether `host` (lower case, a trailing dot ignored) ends in a known Federate
// TLD, however many labels it has (www.home.fed too). On WolOS such a host is
// exclusively fed://: it is never fetched over http(s) and never looked up in
// DNS (ADR 0052, lead rule 2026-11-05).
bool IsFederateTldHost(std::string_view host);

// IsFederateTldHost() of `url`'s host, for any scheme (http, https, ws, wss).
bool IsFederateTldUrl(const GURL& url);

// The fed:// address of an http(s) URL whose host ends in a known Federate TLD
// (user info and port dropped, path and query kept, fragment dropped), or
// nullopt for any other URL. Always a value for such a host, even when it is not
// label.tld (www.home.fed): the helper then answers with a refusal page, so the
// name is never left to the web. There is no setting that turns this off.
std::optional<GURL> MapHttpToFed(const GURL& url);

}  // namespace atlas_fed

#endif  // COMPONENTS_ATLAS_FED_TLDS_FED_TLDS_H_

// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ATLAS_FED_FED_DNS_GUARD_H_
#define CHROME_BROWSER_ATLAS_FED_FED_DNS_GUARD_H_

#include <string>
#include <string_view>

namespace base {
class CommandLine;
}

// The second fence of "a Federate name is exclusively fed://" (ADR 0052): the
// network service's host resolver gets one MAP rule per known Federate TLD that
// makes the name unresolvable (`MAP *.fed ~NOTFOUND`), so whatever path a
// request to such a host takes (a redirect from the web, a WebSocket, a
// prefetch, an extension) no DNS query is ever made for it and no socket is
// opened. The rules are the browser's --host-resolver-rules, which content
// passes on to the network service.
namespace atlas_fed {

// The rules for every known Federate TLD, then `existing` (the user's own
// --host-resolver-rules, if any) after them.
std::string HostResolverRules(std::string_view existing);

// Puts HostResolverRules() on `command_line` (ChromeContentBrowserClient's
// constructor, before the network service starts). Idempotent.
void AppendDnsGuard(base::CommandLine* command_line);

}  // namespace atlas_fed

#endif  // CHROME_BROWSER_ATLAS_FED_FED_DNS_GUARD_H_

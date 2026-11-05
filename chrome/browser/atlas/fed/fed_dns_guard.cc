// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/atlas/fed/fed_dns_guard.h"

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "chrome/browser/atlas/fed/fed_url.h"
#include "services/network/public/cpp/network_switches.h"

namespace atlas_fed {

std::string HostResolverRules(std::string_view existing) {
  std::string rules;
  for (std::string_view tld : KnownTlds()) {
    // Both spellings of the host: "home.fed" and the fully-qualified "home.fed."
    // (the resolver rule is an anchored glob, so one pattern does not match the
    // other; IsFederateTldHost() ignores a trailing dot, the rules must too).
    base::StrAppend(&rules, {"MAP *.", tld, " ~NOTFOUND,MAP *.", tld,
                             ". ~NOTFOUND,"});
  }
  if (existing.empty()) {
    rules.pop_back();
    return rules;
  }
  base::StrAppend(&rules, {existing});
  return rules;
}

void AppendDnsGuard(base::CommandLine* command_line) {
  std::string existing = command_line->GetSwitchValueASCII(
      network::switches::kHostResolverRules);
  if (existing.find("MAP *.fed ~NOTFOUND") != std::string::npos) {
    return;  // already guarded
  }
  command_line->RemoveSwitch(network::switches::kHostResolverRules);
  command_line->AppendSwitchASCII(network::switches::kHostResolverRules,
                                  HostResolverRules(existing));
}

}  // namespace atlas_fed

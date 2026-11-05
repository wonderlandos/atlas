#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""T-03 hosts: no host of the inherited project is reachable, even if a preference flips.

Services are off by default (overlay patch 0003), but a pref that flips must still lead nowhere:

  * the services origin and the "services are disabled" origin become loopback addresses on a port
    the network stack refuses to connect to (port 1 is on its restricted list, so no socket is even
    opened); an address literal needs no lookup, so no DNS query is made either. The two differ
    because the download UI compares the host of a download with the dummy host.
  * the browser-update base is blank (the code that uses it is compiled for macOS and Windows only);
  * the crash upload URL is blank, so an official build cannot upload to the project's server;
  * the extension updater's ping domain is blank (no ping data to the project's domain);
  * the crash page's "file a bug" link and two link targets of pages we ship lead nowhere.

A host a person types into their own preference stays possible: that is their server, not ours.

See _lib.py for the command line and for what the counts mean.
"""

import re
import sys

sys.dont_write_bytecode = True  # the tools are run with -I from a checkout: leave no __pycache__
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import _lib  # noqa: E402

SERVICES = "https://127.0.0.3:1"
DUMMY = "https://127.0.0.2:1"

HELPERS_H = "components/helium_services/helium_services_helpers.h"

TEXT_RULES = [
    ("hosts.services_base", HELPERS_H, re.compile(r'"https://services\.helium\.imput\.net"'), f'"{SERVICES}"'),
    ("hosts.dummy_origin", HELPERS_H, re.compile(r'"https://helium-services-are-disabled\.qjz9zk"'), f'"{DUMMY}"'),
    ("hosts.update_base", "components/helium_services/helium_services_helpers.cc",
     re.compile(r'(kUpdateBaseUrl = )"https://updates\.helium\.computer/"'), r'\1""'),
    ("hosts.crash_url", "components/crash/core/app/crash_reporter_client.cc",
     re.compile(r'return "https://crash\.helium\.computer/crash";'), "return std::string();"),
    ("hosts.extension_ping", "chrome/browser/extensions/updater/chrome_extension_downloader_factory.cc",
     re.compile(r'set_ping_enabled_domain\("helium\.computer"\)'), 'set_ping_enabled_domain("")'),
    ("hosts.crash_page_bug", "components/crash/core/browser/resources/crashes.ts",
     re.compile(r"'https://github\.com/imputnet/helium/issues/new'"), "'about:blank'"),
    ("hosts.services_placeholder", "chrome/browser/resources/settings/privacy_page/services_page.html",
     re.compile(r'placeholder="https://services\.helium\.imput\.net"'), 'placeholder="https://services.example.org"'),
    ("hosts.blocker_about", "third_party/ublock/about.html",
     re.compile(r'href="https://(?:helium\.computer/privacy|github\.com/imputnet/uBlock)"'), 'href="about:blank"'),
]


def run(tree, apply):
    result = _lib.Result()
    _lib.text_rules(tree, TEXT_RULES, apply, result)
    return result


def main():
    args = _lib.parser(__doc__.split("\n\n")[0]).parse_args()
    mode, tree = _lib.mode_of(args)
    result = run(tree, apply=(mode == "apply"))
    return _lib.finish(result, ["hosts."], args, mode)


if __name__ == "__main__":
    sys.exit(main())

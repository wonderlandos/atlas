#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""T-01 brand strings: the name of the inherited project becomes Atlas.

What a person can read in the browser is rewritten here, after the fork's own passes made the names
(utils/name_substitution.py, utils/i18n_apply.py):

  * every <message> of every .grd/.grdp and every <translation> of every .xtb (with the message ids
    that move with the text, see _grit.py);
  * the product branding file (product and company names, copyright line);
  * the few strings that live in code or in scripts and not in a grit file (the flags page, the crash
    product name, a services dialog, the web-store page fix-up).

Identifiers (pref keys "helium.*", class and constant names, directories, patch names) are NOT
touched; the ones that stay are listed under "internal" in atlas/tests/transform-counts.json and the
gate atlas/tests/brand_gate.py checks that nothing else is left.

See _lib.py for the command line and for what the counts mean.
"""

import re
import sys

sys.dont_write_bytecode = True  # the tools are run with -I from a checkout: leave no __pycache__
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import _lib  # noqa: E402
from _grit import grit_pass  # noqa: E402

NAME = re.compile(r"\bHelium\b")

GRIT_RULES = [("name", NAME, "Atlas")]

# strings in code and resources that are not in a grit file: (key, path, regex, replacement)
TEXT_RULES = [
    ("brand.branding.name", "chrome/app/theme/chromium/BRANDING", NAME, "Atlas"),
    ("brand.branding.bundle", "chrome/app/theme/chromium/BRANDING", re.compile(r"\bnet\.imput\.helium\b"), "org.wolos.atlas"),
    ("brand.text.crash_product", "chrome/app/chrome_crash_reporter_client.cc", NAME, "Atlas"),
    ("brand.text.flags_page", "chrome/browser/helium_flag_entries.h", NAME, "Atlas"),
    ("brand.text.services_dialog", "components/helium_services/schema.cc", NAME, "Atlas"),
    ("brand.text.webstore_fixup", "components/extstore_fixups/chrome-webstore.js", NAME, "Atlas"),
    ("brand.text.webstore_fixup_log", "components/extstore_fixups/chrome-webstore-main.js", NAME, "Atlas"),
    # the blocker we ship: its about page, the titles of its filter lists, the version line
    ("brand.blocker.about_label", "third_party/ublock/about.html", re.compile(r"\bHelium fork:"), "Atlas:"),
    ("brand.blocker.list_titles", "third_party/ublock/assets/assets.json", NAME, "Atlas"),
    ("brand.blocker.about_name", "third_party/ublock/js/about.js", NAME, "Atlas"),
]


def run(tree, apply):
    result = _lib.Result()
    for k, v in grit_pass(tree, GRIT_RULES, apply, "brand").items():
        result.add(k, v)
        result.declare(k)
    _lib.text_rules(tree, TEXT_RULES, apply, result)
    return result


def main():
    args = _lib.parser(__doc__.split("\n\n")[0]).parse_args()
    mode, tree = _lib.mode_of(args)
    result = run(tree, apply=(mode == "apply"))
    return _lib.finish(result, ["brand."], args, mode)


if __name__ == "__main__":
    sys.exit(main())

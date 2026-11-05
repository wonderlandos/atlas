#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""T-08 product words: what the product calls things, where the engine says otherwise.

The Wonderland Doctrine (no other product named on screen) and the product's own vocabulary reach
words that are not the inherited project's name (brand.py has that): a private window is "Private"
here, never "Incognito"; the internal pages say Atlas, not the engine's name (ATLAS-E2E-06).

  * grit messages, English only (a translation keeps its own word, and keeps its id in step with the
    English text, see _grit.py): Incognito -> Private;
  * the few strings that live in code and page sources: the policy page's title, the list of internal
    URLs, the net-internals hints, the flag descriptions that name the engine.

Not touched: identifiers (flag and switch names, content-setting keys), the engine's own comments.
See _lib.py for the command line and for what the counts mean.
"""

import re
import sys

sys.dont_write_bytecode = True  # the tools are run with -I from a checkout: leave no __pycache__
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import _lib  # noqa: E402
from _grit import grit_pass  # noqa: E402

GRIT_RULES = [
    ("incognito_cap", re.compile(r"\bIncognito\b"), "Private"),
    ("incognito_low", re.compile(r"\bincognito\b"), "private"),
]

# (key, path, regex, replacement)
TEXT_RULES = [
    ("words.policy_title", "components/policy/core/browser/policy_conversions.h",
     re.compile(r'"Chrome Policies"'), '"Atlas Policies"'),
    ("words.urls_title", "components/webui/chrome_urls/resources/app.html.ts",
     re.compile(r"List of Chrome URLs"), "List of Atlas URLs"),
    ("words.net_internals", "chrome/browser/resources/net_internals/index.html",
     re.compile(r"\bchrome://(net-export|network)"), r"atlas://\1"),
    ("words.flags_engine", "chrome/browser/existing_switch_flag_entries.h",
     re.compile(r"Chromium feature, ungoogled-chromium flag\."), "Atlas flag."),
    ("words.flags_atlas", "chrome/browser/helium_flag_entries.h",
     re.compile(r"(?:Helium|Atlas) flag, Chromium feature\."), "Atlas flag."),
]


def run(tree, apply):
    result = _lib.Result()
    for k, v in grit_pass(tree, GRIT_RULES, apply, "words", xtb_rules=[]).items():
        if k == "words.xtb.translations":
            continue  # the translations' text is left alone on purpose: only their ids move
        result.add(k, v)
        result.declare(k)
    _lib.text_rules(tree, TEXT_RULES, apply, result)
    return result


def main():
    args = _lib.parser(__doc__.split("\n\n")[0]).parse_args()
    mode, tree = _lib.mode_of(args)
    result = run(tree, apply=(mode == "apply"))
    return _lib.finish(result, ["words."], args, mode)


if __name__ == "__main__":
    sys.exit(main())

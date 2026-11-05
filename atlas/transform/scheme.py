#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""T-02 URL scheme: the internal pages are atlas://.

The inherited project added a second scheme for the browser's own pages ("helium", an alias of
"chrome" that content::kHeliumUIScheme names; patches/helium/core/override-chrome-protocol.patch).
Here the same alias is called "atlas": the constant in the three places that spell it, the
omnibox suggestion that completes "about:", the tab search row, and every "helium://" in the strings.
The identifier kHeliumUIScheme stays (it is code, not text).

See _lib.py for the command line and for what the counts mean.
"""

import re
import sys

sys.dont_write_bytecode = True  # the tools are run with -I from a checkout: leave no __pycache__
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import _lib  # noqa: E402
from _grit import grit_pass  # noqa: E402

GRIT_RULES = [("url", re.compile(r"(?<![\w.])helium://"), "atlas://")]

CONST = re.compile(r'(kHeliumUIScheme\[\] = ")helium(")')

TEXT_RULES = [
    ("scheme.constant.content", "content/public/common/url_constants.h", CONST, r"\1atlas\2"),
    ("scheme.constant.url_fixer", "components/url_formatter/url_fixer.cc", CONST, r"\1atlas\2"),
    ("scheme.omnibox_builtin", "components/omnibox/browser/builtin_provider.cc", re.compile(r'u"helium"'), 'u"atlas"'),
    ("scheme.tab_search", "chrome/browser/resources/tab_search/tab_search_item.ts", re.compile(r"'helium:(//)?'"), r"'atlas:\1'"),
]


def run(tree, apply):
    result = _lib.Result()
    for k, v in grit_pass(tree, GRIT_RULES, apply, "scheme").items():
        result.add(k, v)
        result.declare(k)
    _lib.text_rules(tree, TEXT_RULES, apply, result)
    return result


def main():
    args = _lib.parser(__doc__.split("\n\n")[0]).parse_args()
    mode, tree = _lib.mode_of(args)
    result = run(tree, apply=(mode == "apply"))
    return _lib.finish(result, ["scheme."], args, mode)


if __name__ == "__main__":
    sys.exit(main())

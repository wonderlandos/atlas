# atlas/transform

Scripted rewrites of the prepared source tree, with asserted match counts (`atlas/README.md`, "Rules":
transforms over patches for anything mechanical). They run once, as the build stage `atlas`, after the
inherited project's own passes (`names`, `i18n`, `version`, `resources`) and before `gn`.

| Script | What | Records in `atlas/tests/transform-counts.json` |
|---|---|---|
| `brand.py` | T-01 the name Atlas for the inherited name in every grit message and translation (message ids move with the text), the product branding file, and the few strings that live in code | `brand.*` |
| `scheme.py` | T-02 `atlas://` for the internal pages: the scheme constant (value only), the URL fixer, the omnibox, the tab search row, `helium://` in every string | `scheme.*` |
| `hosts.py` | T-03 no host of the inherited project is reachable: services origin and dummy origin are loopback on an unsafe port (no DNS, no socket), update base and crash URL blank, extension ping domain blank, three links lead nowhere | `hosts.*` |
| `blocker.py` | T-07 the blocker list under a noncommercial licence (entry and files) leaves the package | `blocker.*` |
| `apply.py` | all of the above in order, then the brand slot (`atlas/resources/atlas_resources.txt`) copied over the tree | `resources.*` |

The policy directory (`/etc/atlas/policies`) is overlay patch 0013, not a transform.

```
python3 -I atlas/transform/apply.py --check  TREE   # count on an untransformed tree, write nothing
python3 -I atlas/transform/apply.py --apply  TREE   # the build stage; writes TREE/.atlas.stamp
python3 -I atlas/transform/apply.py --verify TREE   # transformed: nothing left, slot files in place
python3 -I atlas/tests/brand_gate.py --tree TREE    # the Wonderland Doctrine gate (also --package DIR)
```

A rule that matches nothing fails; a count that differs from `transform-counts.json` prints a warning
with the old and the new number (`--strict` makes it an error). After a human has read the difference:
`apply.py --check TREE --update-counts` records the new numbers. `internal` in the same file lists what
may keep the old name (identifiers, data, licence notices) and why; the gate reads it.

Tests: `python3 -I atlas/tests/test_transform.py` (set `ATLAS_ENGINE_TREE` to run the grit tests too).

#!/usr/bin/env bash
# sidebar.sh: static checks of the sidebar area (owner: ATLAS-SIDEBAR). No source tree needed.
# - the four sidebar patches are listed in series.d/20-sidebar.series, in order, inside its range;
# - the strings the code asks for exist in chrome/app/atlas/sidebar_strings.grdp;
# - the area's files carry no colour literal and no hex colour (colours come from the colour
#   provider), and say nothing a person could read that names another product;
# - every unit test file is in the BUILD.gn of the area.
set -u
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
area="$root/chrome/browser/ui/atlas/sidebar"
frag="$root/atlas/patches/series.d/20-sidebar.series"
rc=0
ok() { echo "ok   $1"; }
bad() { echo "FAIL $1"; rc=1; }

want="ui/0020-default-vertical.patch ui/0021-sidebar-metrics.patch ui/0022-sidebar-sections.patch ui/0023-sidebar-footer.patch"
got="$(grep -Ev '^[[:space:]]*(#|$)' "$frag" | tr '\n' ' ' | sed 's/ $//')"
[ "$got" = "$want" ] && ok "series.d/20-sidebar.series lists 0020..0023 in order" || bad "series fragment is '$got'"
for p in $want; do
    n="$(basename "$p" | cut -c1-4)"
    [ "$n" -ge 20 ] && [ "$n" -le 23 ] && [ -f "$root/atlas/patches/$p" ] || bad "$p: missing or outside 0020-0023"
done

grdp="$root/chrome/app/atlas/sidebar_strings.grdp"
for id in $(grep -ho 'IDS_ATLAS_SIDEBAR_[A-Z_]*' "$area"/*.cc "$area"/*.h | sort -u); do
    grep -q "name=\"$id\"" "$grdp" && : || bad "$id is used but not in sidebar_strings.grdp"
done
ok "every IDS_ATLAS_SIDEBAR_* the code asks for is in the string part"
for id in $(grep -o 'name="IDS_ATLAS_SIDEBAR_[A-Z_]*"' "$grdp" | sed 's/name="//; s/"//'); do
    grep -q "$id" "$area"/*.cc "$root/chrome/browser/ui/atlas/kit"/*.cc || bad "$id is in the string part but no code uses it"
done

# colours: no hex literal and no named Sk colour in non-test code of the area (the tests may use them)
hits="$(grep -nP '0x[0-9a-fA-F]{6,8}|#[0-9a-fA-F]{6}\b|SkColorSetRGB|SkColorSetARGB|SK_Color(?!TRANSPARENT)[A-Z]' "$area"/*.cc "$area"/*.h 2>/dev/null | grep -v '_unittest.cc' || true)"
[ -z "$hits" ] && ok "no colour literal in the area's code" || bad "colour literal: $hits"

# words a person could read: no other product, vendor or author named in the area (comments included)
hits="$(grep -niE 'helium|chrom(e|ium)|google|firefox|brave|vivaldi|arc browser' "$area"/*.cc "$area"/*.h "$root/chrome/app/atlas/sidebar_strings.grdp" 2>/dev/null | grep -v 'chrome/' || true)"
[ -z "$hits" ] && ok "no other product named in the area" || bad "names another product: $hits"

# one shortcut notation (E2E-05): the footer says what kit_text.cc ShortcutSymbols() writes for the engine's
# new-tab binding Ctrl+T (the compositor turns Super+T into it): the Command symbol and T
grep -q '^[[:space:]]*⌘T[[:space:]]*$' "$root/chrome/app/atlas/sidebar_strings.grdp" \
    && ok "the footer shortcut is ⌘T (the shell's notation, = ShortcutSymbols(Ctrl, T))" \
    || bad "the footer shortcut is not ⌘T"

# tests are built
for t in "$area"/*_unittest.cc; do
    grep -q "\"$(basename "$t")\"" "$area/BUILD.gn" && : || bad "$(basename "$t") is not in BUILD.gn"
done
ok "every unit test file is in BUILD.gn"

# the patches only touch files of the sidebar fence
fence='accelerator_table.cc|views/tabs/common/|views/tabs/tab/tab_icon|views/tabs/vertical/|views/frame/vertical_tab_strip_region_view|ui/tabs/tab_strip_prefs.cc|ui/tabs/vertical_tab_strip_state.h'
for p in $want; do
    for f in $(grep '^+++ b/' "$root/atlas/patches/$p" | sed 's|^+++ b/||'); do
        printf '%s\n' "$f" | grep -Eq "$fence" || bad "$p touches $f outside the sidebar fence"
    done
done
ok "the patches stay inside the sidebar fence"
exit $rc

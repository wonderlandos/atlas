#!/usr/bin/env bash
# toolbar.sh: static checks of the toolbar area (no source tree needed). Owner: ATLAS-TOOLBAR.
# - no word of the Wonderland Doctrine in the strings the person can read;
# - no colour literal and no hex value in the area's sources;
# - every IDS_ATLAS_TOOLBAR_* used in the code is defined in toolbar_strings.grdp, and the other way;
# - the tracker-blocking pref name read by the popover is the one the first-run area declares;
# - patch 0030 touches no file of the overlay tree and is listed in 30-toolbar.series.
set -u
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
area="$root/chrome/browser/ui/atlas/toolbar"
grdp="$root/chrome/app/atlas/toolbar_strings.grdp"
rc=0
ok() { echo "ok   $*"; }
err() { echo "FAIL $*"; rc=1; }

# 1. doctrine words in the strings (message bodies and descriptions)
msgs="$(sed -n '/^  <message name=/,/<\/message>/p' "$grdp")"
if printf '%s\n' "$msgs" | grep -Eiqw 'helium|chromium|chrome|google|firefox|safari|edge|brave|vivaldi|opera|ungoogled|imput'; then
    err "toolbar_strings.grdp names a fork, an engine vendor or another browser"
else ok "strings carry no forbidden name"; fi
if printf '%s\n' "$msgs" | grep -q '!'; then
    err "toolbar_strings.grdp has an exclamation mark"
else ok "strings carry no exclamation mark"; fi
if grep -Eiq 'blocked' <(sed -n '/IDS_ATLAS_TOOLBAR_PRIVACY_TOOLTIP\|IDS_ATLAS_TOOLBAR_ROW_DNT/,/<\/message>/p' "$grdp" | grep -v 'desc='); then
    err "a Do Not Track string says blocked"
else ok "Do Not Track strings never say blocked"; fi

# 2. colours come from the theme, never a literal
if grep -En '#[0-9a-fA-F]{6}\b|SkColorSetRGB|SkColorSetARGB|SK_Color[A-Z]|gfx::kGoogle' "$area"/*.cc "$area"/*.h >/dev/null; then
    err "a colour literal in the area's sources"
else ok "no colour literal in the sources"; fi

# 3. strings used and defined
used="$(grep -oh 'IDS_ATLAS_TOOLBAR_[A-Z_]*' "$area"/*.cc "$area"/*.h | sort -u)"
defd="$(grep -o 'name="IDS_ATLAS_TOOLBAR_[A-Z_]*"' "$grdp" | sed 's/name="//; s/"//' | sort -u)"
miss="$(comm -23 <(printf '%s\n' "$used") <(printf '%s\n' "$defd"))"
extra="$(comm -13 <(printf '%s\n' "$used") <(printf '%s\n' "$defd"))"
[ -z "$miss" ] && ok "every string the code uses is defined" || err "used but not defined: $miss"
[ -z "$extra" ] && ok "every defined string is used" || err "defined but not used: $extra"

# 4. the tracker pref name matches the first-run area (when that area is in this clone)
setup="$root/components/atlas_pages/setup/setup_prefs.h"
mine="$(sed -n 's/.*kTrackerBlockingPref\[\] = "\([^"]*\)".*/\1/p' "$area/privacy_state.h")"
if grep -q 'kTrackerBlocking\[\]' "$setup" 2>/dev/null; then
    theirs="$(tr -d '\n' < "$setup" | sed -n 's/.*kTrackerBlocking\[\] = *"\([^"]*\)".*/\1/p')"
    [ "$mine" = "$theirs" ] && ok "tracker pref name is $mine in both areas" || err "tracker pref differs: toolbar '$mine', setup '$theirs'"
else
    [ "$mine" = "atlas.tracker_blocking" ] && ok "tracker pref name is atlas.tracker_blocking (first-run area not in this clone)" || err "tracker pref name '$mine' is not atlas.tracker_blocking"
fi

# 5. the patch
p="$root/atlas/patches/ui/0030-privacy-button.patch"
if grep -E '^\+\+\+ b/(chrome/(app/atlas|browser/ui/atlas)|components/(atlas_host|atlas_pages))/' "$p" >/dev/null; then
    err "0030 patches a file of the overlay tree"
else ok "0030 touches no file of the overlay tree"; fi
grep -qx 'ui/0030-privacy-button.patch' "$root/atlas/patches/series.d/30-toolbar.series" && ok "0030 is in 30-toolbar.series" || err "0030 is not in 30-toolbar.series"

exit $rc

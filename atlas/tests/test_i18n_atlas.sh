#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# test_i18n_atlas.sh [ENGINE_TREE]: atlas/transform/i18n_atlas.py puts the Atlas translations into the XTB files.
# Needs the engine's tools/grit (the message fingerprint): ENGINE_TREE or $ATLAS_ENGINE_TREE (a prepared source
# tree); without one the live test is skipped, said so. The tree itself is never changed: a scratch tree with the
# engine's grit linked in, a generated_resources.grd that includes the Atlas parts and one empty XTB per language.
set -u
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
engine="${1:-${ATLAS_ENGINE_TREE:-}}"
rc=0
err() { echo "FAIL $*"; rc=1; }
ok() { echo "ok   $*"; }
[ -f "$root/atlas/transform/i18n_atlas.py" ] || { err "atlas/transform/i18n_atlas.py missing"; exit 1; }
grep -q 'i18n_atlas.py' "$root/atlas/platform/scripts/shared.sh" || err "shared.sh: helium_apply_translations does not run i18n_atlas.py"
grep -q 'check_strings.py' "$root/atlas/tests/check_overlay.sh" || err "check_overlay.sh does not run check_strings.py"
ok "wired: the build step and check_overlay run the Atlas strings"
if [ -z "$engine" ] || [ ! -d "$engine/tools/grit" ]; then
    echo "skip live test: no engine tree with tools/grit (give ENGINE_TREE or ATLAS_ENGINE_TREE)"; exit "$rc"
fi
t="$(mktemp -d)"; trap 'rm -rf "$t"' EXIT
mkdir -p "$t/tools" "$t/chrome/app/resources"
ln -s "$engine/tools/grit" "$t/tools/grit"
langs="$(python3 -I -c 'import os;print(" ".join(f[:-5] for f in sorted(os.listdir(os.path.join("'"$root"'","atlas/i18n/translations")))))')"
{
    echo '<?xml version="1.0" encoding="UTF-8"?><grit><translations>'
    for l in $langs; do echo "<file path=\"resources/generated_resources_$l.xtb\" lang=\"$l\" />"; done
    echo '</translations><release><messages><part file="atlas/brand_strings.grdp" /></messages></release></grit>'
} > "$t/chrome/app/generated_resources.grd"
for l in $langs; do echo "<?xml version=\"1.0\" ?><translationbundle lang=\"$l\">
</translationbundle>" > "$t/chrome/app/resources/generated_resources_$l.xtb"; done
python3 -I "$root/atlas/transform/i18n_atlas.py" -t "$t" --languages "$root/i18n/languages.json" > "$t/out.txt" 2>&1 || { cat "$t/out.txt"; err "i18n_atlas.py failed"; }
# one XTB entry per distinct message text (the id is the fingerprint of text and meaning; equal texts share one)
n="$(python3 -I -c 'import json;print(len({(m["message"], m.get("meaning")) for m in json.load(open("'"$root"'/atlas/i18n/source.atlas.json"))}))')"
for l in $langs; do
    got="$(grep -c '<translation ' "$t/chrome/app/resources/generated_resources_$l.xtb")"
    [ "$got" = "$n" ] && ok "$l: $got translations in the XTB (one per distinct Atlas message text)" || err "$l: $got translations in the XTB, want $n"
done
# a translation carries the placeholders of the message as <ph name=.../>
grep -q '<ph name="ATLAS_VERSION" */>' "$t/chrome/app/resources/generated_resources_pt-BR.xtb" && ok "placeholders written as <ph/>" || err "pt-BR: IDS_ATLAS_ABOUT_VERSION lost its placeholder"
exit "$rc"

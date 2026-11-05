#!/usr/bin/env bash
# Tests of transform/kit_metrics.py (the numbers of kit_metrics.h against the design repo's specs).
# Usage: tests/test_kit_metrics.sh [DESIGN_REPO]  (a real design repo adds the check on the real specs)
# ok and bad always succeed, so "test && ok || bad" is a safe if-then-else here.
# shellcheck disable=SC2015
set -u
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$here/../.."
km="$root/atlas/transform/kit_metrics.py"
t="$(mktemp -d)"
trap 'rm -rf "$t"' EXIT
rc=0
ok() { echo "ok   $1"; }
bad() { echo "FAIL $1"; rc=1; }

d="$t/design/components"
mkdir -p "$d"
cat > "$d/sheet.toml" <<'T'
[anatomy]
surface = "the card fill, radius 12, 16 px inside, elevation 2"
buttons = "at the end, 8 px apart, the default one last"
T
cat > "$d/side-sheet.toml" <<'T'
[anatomy]
pane = "336 wide, the card fill, a hairline at the start, the 12 px inset"
[sizes.pane]
width = 336
T
cat > "$d/bottom-sheet.toml" <<'T'
[anatomy]
surface = "the card fill, radius 24 at the top corners"
content = "rows or details, the 12 px inset at the sides"
[sizes.handle]
width = 40
height = 4
T
cat > "$d/rich-tooltip.toml" <<'T'
[anatomy]
surface = "glass, radius 12, the 12 px inset, elevation 2"
text = "11 secondary, at most 40 characters a line"
T
cat > "$d/menu.toml" <<'T'
[anatomy]
surface = "glass, radius 12, elevation 2"
[density.compact]
item = 24
[density.standard]
item = 28
[density.comfortable]
item = 32
T
cat > "$d/button.toml" <<'T'
[anatomy]
shape = "radius 8, padding 0 12"
T

out="$("$km" --check "$t/design" 2>&1)"; r=$?
[[ $r -eq 0 && "$out" == *"ok   18 numbers"* ]] && ok "a repo with the specs' numbers passes (18 numbers)" || bad "pass: $out"

sed -i 's/radius 12, 16 px inside/radius 12, 20 px inside/' "$d/sheet.toml"
out="$("$km" --check "$t/design" 2>&1)"; r=$?
[[ $r -ne 0 && "$out" == *"kSheetInset: kit_metrics.h says 16"* ]] && ok "a moved sheet inset fails and names the constant" || bad "moved inset: $out"
sed -i 's/radius 12, 20 px inside/radius 12, 16 px inside/' "$d/sheet.toml"

sed -i 's/^width = 336/width = 320/' "$d/side-sheet.toml"
out="$("$km" --check "$t/design" 2>&1)"; r=$?
[[ $r -ne 0 && "$out" == *"kSideSheetWidth: kit_metrics.h says 336"* ]] && ok "a moved side sheet width fails" || bad "moved width: $out"
sed -i 's/^width = 320/width = 336/' "$d/side-sheet.toml"

sed -i 's/^item = 28/item = 30/' "$d/menu.toml"
out="$("$km" --check "$t/design" 2>&1)"; r=$?
[[ $r -ne 0 && "$out" == *"kMenuItemStandard"* ]] && ok "a moved menu density fails" || bad "moved density: $out"
sed -i 's/^item = 30/item = 28/' "$d/menu.toml"

sed -i 's/radius 12, the 12 px inset, elevation 2/the corners are round/' "$d/rich-tooltip.toml"
out="$("$km" --check "$t/design" 2>&1)"; r=$?
[[ $r -ne 0 && "$out" == *"no longer says"* ]] && ok "a spec that lost its sentence fails" || bad "lost sentence: $out"

rm "$d/menu.toml"
out="$("$km" --check "$t/design" 2>&1)"; r=$?
[[ $r -ne 0 && "$out" == *"menu.toml not in"* ]] && ok "a missing spec file fails" || bad "missing file: $out"

rm "$d/button.toml"
cp "$d/sheet.toml" "$t/keep.toml"
out="$("$km" --check "$t/design" 2>&1)"
[[ "$out" == *"skip kButtonRadius"* ]] && ok "button.toml missing is skipped, said so" || bad "button skip: $out"

# A constant that the table does not know, and one the header lacks.
cp "$root/chrome/browser/ui/atlas/kit/kit_metrics.h" "$t/h.h"
sed -i 's/inline constexpr int kButtonPadding = 12;/inline constexpr int kButtonPadding = 12;\ninline constexpr int kNew = 3;/' "$t/h.h"
python3 -I - "$km" "$t/h.h" "$t/design" <<'PY' && ok "a constant missing from the table fails" || bad "new constant"
import importlib.util, sys
spec = importlib.util.spec_from_file_location("km", sys.argv[1]); km = importlib.util.module_from_spec(spec); spec.loader.exec_module(km)
problems, _ = km.check(sys.argv[3], sys.argv[2])
sys.exit(0 if any("kNew is in kit_metrics.h but not in the table" in p for p in problems) else 1)
PY

if [[ -n "${1:-}" ]]; then
    out="$("$km" --check "$1" 2>&1)"; r=$?
    [[ $r -eq 0 ]] && ok "the real specs in $1: ${out##*$'\n'}" || bad "real specs: $out"
fi
exit $rc

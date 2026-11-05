#!/usr/bin/env bash
# Tests of transform/tokens.py on a tiny fake design repo. No network, no engine tree needed.
# Usage: tests/test_tokens.sh [DESIGN_REPO]  (a real design repo adds the check on its tokens).
# ok and bad always succeed, so "test && ok || bad" is a safe if-then-else here.
# shellcheck disable=SC2015
set -u
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$here/../.."
tok="$root/atlas/transform/tokens.py"
t="$(mktemp -d)"
trap 'rm -rf "$t"' EXIT
rc=0
ok() { echo "ok   $1"; }
bad() { echo "FAIL $1"; rc=1; }

mkdir -p "$t/design/tokens"
cat > "$t/design/tokens/colors.json" <<'J'
{"accent": {"light": "#7C5CFF", "dark": "#8B6CFF"},
 "radius": {"sm": "8px", "md": "12px", "lg": "16px", "window": "12px"},
 "font": {"ui": "Inter", "ui-fallback": ["Inter", "Cantarell", "sans-serif"], "size": "13px"}}
J

out="$("$tok" --css "$t/design")"
[[ "$out" == *"--atlas-accent: #7C5CFF;"* && "$out" == *"--atlas-radius-card: 12px;"* ]] && ok "css has the accent and the card radius" || bad "css content"
[[ "$out" == *"--atlas-accent: #8B6CFF;"* ]] && ok "css has the dark accent" || bad "dark accent"
[[ "$out" != *'"'* || "$out" == *"Inter, Cantarell, sans-serif"* ]] && ok "css fonts without quotes when not needed" || bad "css fonts"

# The block of the patch equals the generated one for the fake repo only after --write; the real
# patch is tested against the fake repo on a copy so the checkout stays untouched.
mkdir -p "$t/fork/atlas/transform" "$t/fork/atlas/patches/ui" "$t/fork/chrome/browser/ui/atlas/chrome"
cp "$tok" "$t/fork/atlas/transform/"
cp "$root/atlas/patches/ui/0072-webui-tokens.patch" "$t/fork/atlas/patches/ui/"
cp "$root/chrome/browser/ui/atlas/chrome/atlas_tokens.h" "$t/fork/chrome/browser/ui/atlas/chrome/"
ft="$t/fork/atlas/transform/tokens.py"
"$ft" --check "$t/design" >/dev/null 2>&1 && bad "check passes on a block that differs from the repo" || ok "check fails on a different block"
"$ft" --write "$t/design" >/dev/null && ok "write succeeds" || bad "write"
"$ft" --check "$t/design" >/dev/null 2>&1 && ok "check passes after write" || bad "check after write"
before="$(cat "$t/fork/atlas/patches/ui/0072-webui-tokens.patch")"
"$ft" --write "$t/design" >/dev/null
[[ "$before" == "$(cat "$t/fork/atlas/patches/ui/0072-webui-tokens.patch")" ]] && ok "write is idempotent" || bad "write idempotent"
# the rewritten patch still applies to a stand-in of the two css files (lines as in the engine)
css="$t/css/ui/webui/resources/cr_elements"
mkdir -p "$css/cr_button"
mkdir -p "$t/css/ui/base/webui"
{
  for i in $(seq 1 44); do echo "// l$i"; done
  printf '%s\n' '' 'std::string GetFontFamilyMd() {' '#if BUILDFLAG(IS_LINUX)' '  return "Roboto, " + GetFontFamily();' \
    '#else' '  return GetFontFamily();' '#endif'
} > "$t/css/ui/base/webui/web_ui_util.cc"
{
  printf '%s\n' '/* l1 */' '/* l2 */' '/* l3 */' '/* l4 */' '/* l5 */' '/* l6 */' ' * #scheme=relative' \
    ' * #css_wrapper_metadata_end */' '' '/* Common css variables for Material Design WebUI */' 'html {' \
    '  --helium-blue: #3450d1;' '  --google-blue-50-rgb: 238, 245, 255; /* #eef5ff */' \
    '  --google-blue-50: rgb(var(--google-blue-50-rgb));' '  --google-blue-100-rgb: 225, 237, 255;' '}'
} > "$css/cr_shared_vars.css"
{
  for i in $(seq 1 39); do echo "/* l$i */"; done
  printf '%s\n' '  user-select: none;' '  -webkit-tap-highlight-color: transparent;' \
    '  border: var(--cr-button-border, 1px solid var(--cr-button-border-color));' '  border-radius: 8px;' \
    '  background: var(--cr-button-background-color);' '  color: var(--cr-button-text-color);' '  font-weight: 500;'
  for i in $(seq 48 92); do echo "/* l$i */"; done
  printf '%s\n' '}' '' ':host(.floating-button) {' '  border-radius: 8px;' '  height: 40px;' \
    '  transition: box-shadow 80ms linear;' '}'
} > "$css/cr_button/cr_button.css"
(cd "$t/css" && patch -p1 < "$t/fork/atlas/patches/ui/0072-webui-tokens.patch" >"$t/patch.log" 2>&1) \
  && ok "the rewritten patch applies" || { bad "the rewritten patch applies"; cat "$t/patch.log"; }
grep -q 'var(--atlas-accent)' "$css/cr_shared_vars.css" && grep -q -- '--atlas-radius-control, 8px' "$css/cr_button/cr_button.css" \
  && ok "the patch put the variables in" || bad "the patch put the variables in"

# a radius that drifts from the native constants fails
sed -i 's/"md": "12px"/"md": "13px"/' "$t/design/tokens/colors.json"
"$ft" --check "$t/design" >/dev/null 2>&1 && bad "check passes with a drifted radius" || ok "check fails when a design radius differs from atlas_tokens.h"

# the checked-in patch and constants against a real design repo, when one is given
if [[ -n "${1:-}" ]]; then
  "$tok" --check "$1" && ok "the checked-in patch matches $1" || bad "the checked-in patch matches $1"
fi
exit "$rc"

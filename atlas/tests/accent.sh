#!/usr/bin/env bash
# accent.sh: static checks of overlay patch 0073 (ui/accent-seed), no source tree needed; with a prepared
# tree as the argument it also applies the patch to a copy of the three engine files it edits.
# - the patch is in the chrome fragment (70-chrome.series), inside its range 0070-0079;
# - it adds ui/gtk/atlas_accent.{h,cc} and its unit test, hooks BUILD.gn, and edits exactly two engine
#   code lines (the two calls of AtlasAccent), so a rebase conflict has two places to look at;
# - the unit test names carry AtlasAccent, which is the filter the brief and the lane use.
set -u
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
p="$root/atlas/patches/ui/0073-accent-seed.patch"
rc=0
ok() { echo "ok   $1"; }
bad() { echo "FAIL $1"; rc=1; }

grep -qx 'ui/0073-accent-seed.patch' "$root/atlas/patches/series.d/70-chrome.series" && ok "listed in 70-chrome.series" || bad "not in 70-chrome.series"
if grep -q 0073 "$root/atlas/patches/series"; then bad "0073 is in the shared series"; else ok "not in the shared series"; fi
for f in ui/gtk/atlas_accent.h ui/gtk/atlas_accent.cc ui/gtk/atlas_accent_unittest.cc; do
    grep -q "^+++ b/$f\$" "$p" && ok "adds $f" || bad "does not add $f"
done
grep -q '^+    "atlas_accent.cc",' "$p" && grep -q '^+      "atlas_accent_unittest.cc",' "$p" && ok "BUILD.gn lists the source and the test" || bad "BUILD.gn hooks missing"
# Engine code lines that call into the new files: exactly the two hooks.
hooks="$(awk '/^\+\+\+ b\/ui\/gtk\/(gtk_ui|os_settings_provider_gtk)\.cc/{on=1; next} /^\+\+\+ b\/ui\/gtk\/atlas_/{on=0} on && /^\+.*AtlasAccent\(/' "$p" | wc -l)"
[ "$hooks" = 2 ] && ok "two hook lines in engine code" || bad "$hooks hook lines in engine code (want 2)"
n="$(grep -c '^+TEST(AtlasAccentTest, ' "$p")"
[ "$n" -ge 4 ] && ok "$n AtlasAccent tests" || bad "only $n AtlasAccent tests"
if grep -q '^+  if (theme_accent && SkColorGetA(\*theme_accent) == SK_AlphaOPAQUE) {' "$p"; then ok "the theme accent is chosen over the portal's"; else bad "no theme-over-portal choice"; fi

tree="${1:-}"
if [ -n "$tree" ]; then
    t="$(mktemp -d)"; trap 'rm -rf "$t"' EXIT
    for f in ui/gtk/BUILD.gn ui/gtk/gtk_ui.cc ui/gtk/os_settings_provider_gtk.cc; do mkdir -p "$t/$(dirname "$f")"; cp "$tree/$f" "$t/$f"; done
    (cd "$t" && patch -p1 --dry-run -s < "$p") && ok "applies to $tree" || bad "does not apply to $tree"
fi
exit "$rc"

#!/usr/bin/env bash
# Tests of tools/atlas-series on a tiny fake fork: clean series, a bad overlay patch (reported with
# file and hunk), a drop of a non-leaf patch (refused). No network, no source tree needed.
set -u
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
tool="$here/../tools/atlas-series"
t="$(mktemp -d)"
trap 'rm -rf "$t"' EXIT
rc=0
ok() { echo "ok   $1"; }
bad() { echo "FAIL $1"; rc=1; }

mkdir -p "$t/fork/patches/a" "$t/fork/atlas/tools" "$t/fork/atlas/patches" "$t/fork/atlas/platform/patches" "$t/tree/src"
cp "$tool" "$t/fork/atlas/tools/atlas-series"
printf 'one\ntwo\nthree\n' > "$t/tree/src/f.txt"
printf 'a/p1.patch\na/p2.patch\n' > "$t/fork/patches/series"
cat > "$t/fork/patches/a/p1.patch" <<'P'
--- a/src/f.txt
+++ b/src/f.txt
@@ -1,3 +1,3 @@
 one
-two
+TWO
 three
P
cat > "$t/fork/patches/a/p2.patch" <<'P'
--- a/src/f.txt
+++ b/src/f.txt
@@ -1,3 +1,3 @@
 one
 TWO
-three
+THREE
P
: > "$t/fork/atlas/platform/patches/series"
: > "$t/fork/atlas/patches/series"
: > "$t/fork/atlas/drop.list"
run() { python3 -I "$t/fork/atlas/tools/atlas-series" "$@"; }

out=$(run --dry-run "$t/tree" 2>&1)
[ "$out" = "2 inherited + 0 platform + 0 overlay, applies clean" ] && ok "clean series" || bad "clean series: $out"

cat > "$t/fork/atlas/patches/bad.patch" <<'P'
--- a/src/f.txt
+++ b/src/f.txt
@@ -1,3 +1,3 @@
 one
-nothing like this
+x
 three
P
echo bad.patch > "$t/fork/atlas/patches/series"
out=$(run --dry-run "$t/tree" 2>&1)
case $out in *"FAIL overlay/bad.patch"*"FAILED"*) ok "bad overlay patch reported";; *) bad "bad overlay patch: $out";; esac
: > "$t/fork/atlas/patches/series"

echo a/p1.patch > "$t/fork/atlas/drop.list"
out=$(run list 2>&1); rc2=$?
case $out in *"p1.patch is not a leaf: a/p2.patch touches src/f.txt"*) if [ $rc2 -ne 0 ]; then ok "non-leaf drop refused"; else bad "exit code"; fi;; *) bad "non-leaf drop: $out";; esac
echo a/p2.patch > "$t/fork/atlas/drop.list"
out=$(run list 2>&1)
[ "$out" = "$(printf 'inherited\ta/p1.patch')" ] && ok "leaf drop" || bad "leaf drop: $out"
exit $rc

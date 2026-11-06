#!/usr/bin/env bash
# Tests of tools/atlas-series on a tiny fake fork: clean series, a bad overlay patch (reported with
# file and hunk), a drop of a non-leaf patch (refused), the series.d fragments (order, a patch number
# used twice and a patch listed twice refused, a number outside the fragment's range warned) and
# write-series. No network, no source tree needed.
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
: > "$t/fork/atlas/drop.list"

# series.d fragments, applied after atlas/patches/series in file-name order
d="$t/fork/atlas/patches"
mkdir -p "$d/series.d" "$d/privacy" "$d/ui" "$d/host"
printf 'four\n' > "$t/tree/src/g.txt"
mkp() { # mkp FILE FROM TO: a patch of src/g.txt
    printf -- '--- a/src/g.txt\n+++ b/src/g.txt\n@@ -1 +1 @@\n-%s\n+%s\n' "$2" "$3" > "$d/$1"
}
mkp privacy/0001-a.patch four 4
mkp ui/0020-b.patch 4 IV
mkp host/0040-c.patch IV iv
echo privacy/0001-a.patch > "$d/series"
printf '# range: 0040-0049\nhost/0040-c.patch\n' > "$d/series.d/40-host.series"
printf '# range: 0020-0023\nui/0020-b.patch  # sidebar\n' > "$d/series.d/20-sidebar.series"
printf '# not a fragment\nui/0020-b.patch\n' > "$d/series.d/README"
out=$(run list 2>&1)
want=$(printf 'inherited\ta/p1.patch\ninherited\ta/p2.patch\noverlay\tprivacy/0001-a.patch\noverlay\tui/0020-b.patch\noverlay\thost/0040-c.patch')
[ "$out" = "$want" ] && ok "fragments after the series, in file-name order" || bad "fragment order: $out"
out=$(run --dry-run "$t/tree" 2>&1)
[ "$out" = "2 inherited + 0 platform + 3 overlay, applies clean" ] && ok "fragments stack in order" || bad "fragment dry run: $out"
[ "$(run overlay 2>&1 | tr '\n' ' ')" = "privacy/0001-a.patch ui/0020-b.patch host/0040-c.patch " ] && ok "overlay listing" || bad "overlay listing: $(run overlay 2>&1)"

mkp ui/0040-dup.patch iv x
echo ui/0040-dup.patch >> "$d/series.d/20-sidebar.series"
out=$(run check 2>&1); r=$?
case $out in *"patch number 0040 is used twice: ui/0040-dup.patch and host/0040-c.patch"*) [ $r -ne 0 ] && ok "duplicate patch number refused" || bad "dup number exit";; *) bad "dup number: $out";; esac
sed -i '$d' "$d/series.d/20-sidebar.series"

echo ui/0020-b.patch >> "$d/series.d/40-host.series"
out=$(run check 2>&1); r=$?
case $out in *"ui/0020-b.patch is listed twice (series.d/20-sidebar.series and series.d/40-host.series)"*) [ $r -ne 0 ] && ok "patch listed twice refused" || bad "twice exit";; *) bad "twice: $out";; esac
sed -i '$d' "$d/series.d/40-host.series"
echo ui/0020-b.patch >> "$d/series.d/20-sidebar.series"
out=$(run check 2>&1); r=$?
case $out in *"listed twice (series.d/20-sidebar.series and series.d/20-sidebar.series)"*) [ $r -ne 0 ] && ok "patch listed twice in one fragment refused" || bad "twice1 exit";; *) bad "twice1: $out";; esac
sed -i '$d' "$d/series.d/20-sidebar.series"

echo ui/0099-gone.patch >> "$d/series.d/20-sidebar.series"
out=$(run check 2>&1); r=$?
case $out in *"series.d/20-sidebar.series names a missing file: atlas/patches/ui/0099-gone.patch"*) [ $r -ne 0 ] && ok "missing fragment entry refused" || bad "missing exit";; *) bad "missing: $out";; esac
sed -i '$d' "$d/series.d/20-sidebar.series"

mkp ui/0031-far.patch iv x
echo ui/0031-far.patch >> "$d/series.d/20-sidebar.series"
err=$(run check 2>&1 >/dev/null); r=$?
case $err in *"warning: ui/0031-far.patch in series.d/20-sidebar.series: number 0031 is outside the range 0020-0023"*)
    [ $r -eq 0 ] && ok "number outside the range warned, not refused" || bad "range exit $r";; *) bad "range: $err";; esac
sed -i '$d' "$d/series.d/20-sidebar.series"
[ -z "$(run check 2>&1 >/dev/null)" ] && ok "no warning inside the range" || bad "spurious warning"

echo a/p2.patch > "$t/fork/atlas/drop.list"
run write-series "$t/out" >/dev/null 2>&1
want=$(printf '%s\n' "$t/fork/patches/a/p1.patch" "$d/privacy/0001-a.patch" "$d/ui/0020-b.patch" "$d/host/0040-c.patch")
[ "$(cat "$t/out/series" 2>&1)" = "$want" ] && ok "write-series: absolute paths, drop.list honoured" || bad "write-series: $(cat "$t/out/series" 2>&1)"
# two areas that hook the same engine file in different places apply in either order
: > "$t/fork/atlas/drop.list"
printf 'L1\nL2\nL3\nL4\nL5\nL6\nL7\nL8\nL9\nL10\nL11\nL12\nL13\nL14\nL15\nL16\nL17\nL18\nL19\nL20\n' > "$t/tree/src/h.txt"
mkdir -p "$d/brand" "$d/security"
printf -- '--- a/src/h.txt\n+++ b/src/h.txt\n@@ -2,3 +2,4 @@\n L2\n L3\n+hook-a\n L4\n' > "$d/brand/0011-a.patch"
printf -- '--- a/src/h.txt\n+++ b/src/h.txt\n@@ -16,3 +16,4 @@\n L16\n L17\n+hook-b\n L18\n' > "$d/security/0060-b.patch"
rm -f "$d/series.d/"*.series
printf '# range: 0011-0019\nbrand/0011-a.patch\n' > "$d/series.d/15-brand.series"
printf '# range: 0060-0069\nsecurity/0060-b.patch\n' > "$d/series.d/60-vault.series"
out=$(run --dry-run "$t/tree" 2>&1)
[ "$out" = "2 inherited + 0 platform + 3 overlay, applies clean" ] && ok "two areas, one file: order A" || bad "order A: $out"
printf '# range: 0011-0019\nbrand/0011-a.patch\n' > "$d/series.d/65-brand.series"
printf '# range: 0060-0069\nsecurity/0060-b.patch\n' > "$d/series.d/05-vault.series"
rm -f "$d/series.d/15-brand.series" "$d/series.d/60-vault.series"
out=$(run --dry-run "$t/tree" 2>&1)
[ "$out" = "2 inherited + 0 platform + 3 overlay, applies clean" ] && ok "two areas, one file: order B" || bad "order B: $out"
exit $rc

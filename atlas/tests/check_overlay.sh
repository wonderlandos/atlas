#!/usr/bin/env bash
# check_overlay.sh: static checks of the overlay series (no source tree needed).
# - the overlay series is atlas/patches/series plus the fragments series.d/*.series, read by
#   atlas/tools/atlas-series (which refuses a missing entry, a patch listed twice, a number used twice);
# - every fragment is named NN-area.series and carries a "# range: NNNN-NNNN" line;
# - every patch file under atlas/patches is in the overlay series, no duplicates;
# - file names are <area>/NNNN-name.patch with a known area and a number used once;
# - every patch starts with the header of atlas/README.md, and its first line names the file.
set -u
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
dir="$root/atlas/patches"
areas="privacy security brand memory host ui debloat build tor"
fields="Why: Files: Test filter: Rebase risk: Release: License: Verified at:"
rc=0
err() { echo "FAIL $*"; rc=1; }

listing="$(python3 -I "$root/atlas/tools/atlas-series" --root "$root" overlay)" || { echo "FAIL overlay series (see above)"; exit 1; }
mapfile -t series < <(printf '%s\n' "$listing" | grep -v '^$' || true)
for frag in "$dir"/series.d/*; do
    [ -e "$frag" ] || continue
    b="$(basename "$frag")"
    [[ "$b" =~ ^[0-9]{2}-[a-z]+\.series$ ]] || err "series.d/$b: name is not NN-area.series"
    grep -Eq '^#[[:space:]]*range:[[:space:]]*[0-9]{4}-[0-9]{4}[[:space:]]*$' "$frag" || err "series.d/$b: no '# range: NNNN-NNNN' line"
done
mapfile -t files < <(cd "$dir" && find . -name '*.patch' | sed 's|^\./||' | sort)

dups="$(printf '%s\n' "${series[@]}" | sort | uniq -d)"
[ -z "$dups" ] || err "series lists twice: $dups"
for e in "${series[@]}"; do [ -f "$dir/$e" ] || err "series names a missing file: $e"; done
for f in "${files[@]}"; do printf '%s\n' "${series[@]}" | grep -qxF "$f" || err "not in the series: $f"; done

nums="$(printf '%s\n' "${files[@]}" | sed -n 's|^[^/]*/\([0-9]\{4\}\)-.*|\1|p' | sort | uniq -d)"
[ -z "$nums" ] || err "number used twice: $nums"
for f in "${files[@]}"; do
    area="${f%%/*}" base="${f#*/}"
    case " $areas " in *" $area "*) ;; *) err "$f: unknown area $area (known: $areas)" ;; esac
    [[ "$base" =~ ^[0-9]{4}-[a-z0-9-]+\.patch$ ]] || err "$f: name is not NNNN-name.patch"
    num="${base%%-*}" name="${base#*-}"; name="${name%.patch}"
    head1="$(head -n 1 "$dir/$f")"
    [ "$head1" = "Atlas overlay patch $num: $area/$name" ] || err "$f: first line is '$head1'"
    header="$(sed -n '1,/^--- /p' "$dir/$f")"
    while IFS= read -r fld; do
        printf '%s\n' "$header" | grep -q "^$fld" || err "$f: header has no '$fld' line"
    done < <(printf '%s\n' "$fields" | sed 's/: /:\n/g')
done
# the build reads the fragments and the drop list through atlas-series, not the series file alone
shared="$root/atlas/platform/scripts/shared.sh"
bash -n "$shared" || err "shared.sh: syntax error"
grep -q 'atlas-series.* write-series' "$shared" || err "shared.sh: apply_patches does not use atlas-series write-series"
grep -q 'atlas-series.* install-tree' "$shared" || err "shared.sh: apply_patches does not install the overlay tree"
# every overlay tree directory exists
while IFS= read -r d; do
    [ -d "$root/$d" ] || err "tree.list names a missing directory: $d"
done < <(sed -e 's/[[:space:]]#.*$//' -e '/^#/d' -e '/^[[:space:]]*$/d' "$root/atlas/tree.list")
# grit resolves file= of a grdp against the root grd, not the grdp: every file named in a page's
# grdp must exist under components/atlas_pages
pages="$root/components/atlas_pages"
if [ -d "$pages" ]; then
    while IFS= read -r g; do
        while IFS= read -r f; do
            [ -f "$pages/$f" ] || err "${g#"$root"/}: file=\"$f\" does not exist relative to components/atlas_pages"
        done < <(grep -o 'file="[^"]*"' "$g" | sed 's/^file="//; s/"$//' | grep -v '^$')
    done < <(find "$pages" -name '*.grdp' | sort)
fi
[ "$rc" -eq 0 ] && echo "ok   overlay series: ${#series[@]} patches, headers and names valid"
exit "$rc"

#!/usr/bin/env bash
# check_overlay.sh: static checks of the overlay series (no source tree needed).
# - every series entry exists, every patch file under atlas/patches is in the series, no duplicates;
# - file names are <area>/NNNN-name.patch with a known area and a number used once;
# - every patch starts with the header of atlas/README.md, and its first line names the file.
set -u
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
dir="$root/atlas/patches"
areas="privacy security brand memory host ui debloat"
fields="Why: Files: Test filter: Rebase risk: Release: License: Verified at:"
rc=0
err() { echo "FAIL $*"; rc=1; }

mapfile -t series < <(sed -e 's/[[:space:]]#.*$//' -e 's/^#.*$//' -e 's/[[:space:]]*$//' "$dir/series" | grep -v '^$')
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
[ "$rc" -eq 0 ] && echo "ok   overlay series: ${#series[@]} patches, headers and names valid"
exit "$rc"

#!/usr/bin/env bash
# apply-overlay.sh: apply the Atlas overlay series (atlas/patches/series) onto a prepared source tree.
#
#   tools/apply-overlay.sh [--dry-run] [--full] [--keep-going] TREE
#
# TREE is the root of a prepared engine source tree (the directory holding chrome/, base/, ...).
#
# Default mode: TREE already carries the inherited series and the platform series (for example a
# tree prepared by atlas/platform/scripts/build.sh with an empty overlay); only the overlay is applied.
# --full      TREE is a pristine prepared tree: apply inherited + platform + overlay, minus
#             atlas/drop.list (dry-run delegates to atlas/tools/atlas-series).
# --dry-run   never write to TREE: the touched files are copied to a scratch directory and the
#             series is applied there in order, so patches that touch the same file stack correctly.
# --keep-going  after a failing overlay patch, go on with the next ones (default: stop). A failing
#             patch is never applied, not even in part.
#
# Output: one line per overlay patch ("ok" or "FAIL", with the file and hunk that failed), then a
# summary. Exit code 0 when everything applies, 1 when a patch fails, 2 on usage errors.
set -euo pipefail

usage() { sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }

dry=false full=false keep=false tree=""
while [ $# -gt 0 ]; do
    case "$1" in
        --dry-run) dry=true ;;
        --full) full=true ;;
        --keep-going) keep=true ;;
        -h|--help) usage ;;
        -*) echo "apply-overlay: unknown option $1" >&2; usage ;;
        *) [ -z "$tree" ] || usage; tree="$1" ;;
    esac
    shift
done
[ -n "$tree" ] || usage
[ -d "$tree" ] || { echo "apply-overlay: $tree is not a directory" >&2; exit 2; }
tree="$(cd "$tree" && pwd)"

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
overlay="$root/atlas/patches"
series="$overlay/series"
PATCH_FLAGS=(-p1 --ignore-whitespace --no-backup-if-mismatch --forward --batch)

command -v patch >/dev/null || { echo "apply-overlay: GNU patch is required" >&2; exit 2; }

# read_series FILE: entries of a series file, without comments and blank lines
read_series() {
    [ -f "$1" ] || return 0
    sed -e 's/[[:space:]]#.*$//' -e 's/^#.*$//' -e 's/[[:space:]]*$//' "$1" | grep -v '^$' || true
}

# targets PATCH: files the patch touches (new side, old side for deletions)
targets() {
    awk '
        /^--- / { a = $2; next }
        /^\+\+\+ / {
            b = $2; f = (b == "/dev/null") ? a : b
            sub(/^[ab]\//, "", f)
            if (f != "/dev/null" && !(f in seen)) { seen[f] = 1; print f }
        }' "$1"
}

if $full; then
    if $dry; then
        exec python3 -I "$root/atlas/tools/atlas-series" --root "$root" --dry-run "$tree"
    fi
    if [ -n "$(read_series "$root/atlas/drop.list")" ]; then
        echo "apply-overlay: atlas/drop.list is not empty; a full apply must go through the build" \
             "(atlas/platform/scripts/build.sh), which honours it" >&2
        exit 2
    fi
    exec python3 "$root/utils/patches.py" apply "$tree" "$root/patches" \
        "$root/atlas/platform/patches" "$overlay"
fi

mapfile -t entries < <(read_series "$series")
if [ "${#entries[@]}" -eq 0 ]; then
    echo "overlay series is empty, nothing to apply"
    exit 0
fi
for e in "${entries[@]}"; do
    [ -f "$overlay/$e" ] || { echo "apply-overlay: series names a missing file: atlas/patches/$e" >&2; exit 2; }
done

work="$tree"
if $dry; then
    work="$(mktemp -d "${TMPDIR:-/tmp}/apply-overlay.XXXXXX")"
    trap 'rm -rf "$work"' EXIT
    while IFS= read -r f; do
        if [ -f "$tree/$f" ]; then
            mkdir -p "$work/$(dirname "$f")"
            cp -p "$tree/$f" "$work/$f"
        fi
    done < <(for e in "${entries[@]}"; do targets "$overlay/$e"; done | sort -u)
fi

ok=0 failed=0
for e in "${entries[@]}"; do
    # a real apply checks each patch first, so a failing patch never leaves half a change or .rej files
    if out="$(patch "${PATCH_FLAGS[@]}" --dry-run -d "$work" -i "$overlay/$e" 2>&1)" &&
        out="$(patch "${PATCH_FLAGS[@]}" -d "$work" -i "$overlay/$e" 2>&1)"; then
        echo "ok   $e"
        ok=$((ok + 1))
    else
        echo "FAIL $e"
        printf '%s\n' "$out" | grep -E 'FAILED|can.t find file|malformed|Reversed|^(patching|checking) file' \
            | sed 's/^/     /' || true
        failed=$((failed + 1))
        $keep || break
    fi
done

mode="applied to $tree"
$dry && mode="dry run against $tree"
echo "overlay: $ok ok, $failed failed of ${#entries[@]} ($mode)"
[ "$failed" -eq 0 ]

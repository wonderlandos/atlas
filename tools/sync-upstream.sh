#!/usr/bin/env bash
# sync-upstream.sh: move Atlas onto the newest upstream release.
#
#   tools/sync-upstream.sh [--ref REF] [--tree DIR] [--no-update]
#
#  1. fetch the upstream remote (added if missing, see UPSTREAM_URL below) with its tags;
#  2. pick the target: the newest release tag on upstream's default branch (or --ref REF);
#  3. fast-forward the local branch `upstream` to it (refuses anything that is not a fast-forward:
#     `upstream` is a mirror and is never edited);
#  4. replay the overlay commits (old upstream..main, one commit per change) onto the target, one by
#     one, in a scratch worktree, and report each one: "ok", "ok (kept our deletion of ...)" or
#     "CONFLICT" with the files and the overlay patch the commit carries;
#  5. with --tree DIR (a prepared engine tree of the new release, inherited and platform series
#     applied), run tools/apply-overlay.sh --dry-run --keep-going on it: that reports per patch
#     whether our overlay still applies to the new engine code;
#  6. when every commit replayed cleanly, point `main` at the result (unless --no-update). The old
#     head stays reachable as refs/atlas/pre-sync/main. Nothing is pushed: the push commands are printed.
#
# Exit code: 0 clean, 1 conflicts (main unchanged), 2 usage or repository errors.
set -euo pipefail

UPSTREAM_REMOTE=upstream
UPSTREAM_URL=https://github.com/imputnet/helium
MAIN=main
MIRROR=upstream

usage() { sed -n '2,23p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }
die() { echo "sync-upstream: $*" >&2; exit 2; }

ref="" tree="" update=true
while [ $# -gt 0 ]; do
    case "$1" in
        --ref) [ $# -ge 2 ] || usage; ref="$2"; shift ;;
        --tree) [ $# -ge 2 ] || usage; tree="$2"; shift ;;
        --no-update) update=false ;;
        -h|--help) usage ;;
        *) echo "sync-upstream: unknown argument $1" >&2; usage ;;
    esac
    shift
done

if [ -n "$tree" ]; then
    [ -d "$tree" ] || die "--tree $tree is not a directory"
    tree="$(cd "$tree" && pwd)"
fi
root="$(git rev-parse --show-toplevel 2>/dev/null)" || die "run inside the Atlas repository"
cd "$root"
git rev-parse --verify -q "refs/heads/$MAIN" >/dev/null || die "no branch $MAIN"
git rev-parse --verify -q "refs/heads/$MIRROR" >/dev/null || die "no branch $MIRROR"

# 1. fetch
if ! git remote get-url "$UPSTREAM_REMOTE" >/dev/null 2>&1; then
    git remote add "$UPSTREAM_REMOTE" "$UPSTREAM_URL"
fi
git fetch --quiet --tags "$UPSTREAM_REMOTE"
head_ref="$(git symbolic-ref -q --short "refs/remotes/$UPSTREAM_REMOTE/HEAD" 2>/dev/null || echo "$UPSTREAM_REMOTE/main")"

# 2. target
if [ -z "$ref" ]; then
    ref="$(git tag --merged "$head_ref" --sort=-version:refname | grep -E '^[0-9]+(\.[0-9]+)+$' | head -n 1 || true)"
    [ -n "$ref" ] || die "no release tag on $head_ref"
fi
target="$(git rev-parse --verify -q "$ref^{commit}")" || die "unknown ref $ref"

# 3. fast-forward the mirror
old="$(git rev-parse "refs/heads/$MIRROR")"
if [ "$old" = "$target" ]; then
    echo "upstream: already at $ref ($(git rev-parse --short "$target"))"
elif git merge-base --is-ancestor "$old" "$target"; then
    echo "upstream: $(git rev-parse --short "$old") -> $ref ($(git rev-parse --short "$target")), $(git rev-list --count "$old..$target") commits:"
    git log --format='  %h %ad %s' --date=short "$old..$target"
    if [ "$(git symbolic-ref -q --short HEAD || true)" = "$MIRROR" ]; then
        git merge --quiet --ff-only "$target"
    else
        git update-ref "refs/heads/$MIRROR" "$target" "$old"
    fi
else
    die "$ref is not a fast-forward of $MIRROR ($(git rev-parse --short "$old")); the mirror is never rewritten, ask the lead"
fi

# 4. replay the overlay
main_old="$(git rev-parse "refs/heads/$MAIN")"
git merge-base --is-ancestor "$old" "$main_old" || die "$MAIN does not contain $MIRROR's old head; fix the history first"
mapfile -t commits < <(git rev-list --reverse --no-merges "$old..$main_old")
merges="$(git rev-list --count --merges "$old..$main_old")"
[ "$merges" -eq 0 ] || die "$MAIN has $merges merge commits on top of $MIRROR; the overlay must be a straight line"

wt="$(mktemp -d "${TMPDIR:-/tmp}/atlas-sync.XXXXXX")"
cleanup() { git worktree remove --force "$wt" >/dev/null 2>&1 || rm -rf "$wt"; git worktree prune; }
trap cleanup EXIT
git worktree add --quiet --detach "$wt" "$target"

conflicts=0
echo "overlay: replaying ${#commits[@]} commits onto $ref"
for c in "${commits[@]}"; do
    subj="$(git log -1 --format=%s "$c")"
    patchfile="$(git diff-tree --no-commit-id --name-only -r "$c" -- 'atlas/patches/*.patch' 'atlas/patches/**/*.patch' | head -n 1)"
    label="$(git rev-parse --short "$c") $subj${patchfile:+ [$patchfile]}"
    if git -C "$wt" cherry-pick --allow-empty --keep-redundant-commits "$c" >/dev/null 2>&1; then
        echo "  ok        $label"
        continue
    fi
    # Our deletions (inherited CI files we removed) win when upstream edits the deleted file.
    mapfile -t unmerged < <(git -C "$wt" diff --name-only --diff-filter=U)
    kept=() other=()
    for f in "${unmerged[@]}"; do
        st="$(git -C "$wt" status --porcelain -- "$f" | cut -c1-2)"
        if [ "$st" = "UD" ] || [ "$st" = "DU" ]; then
            if git cat-file -e "$c:$f" 2>/dev/null; then other+=("$f"); else kept+=("$f"); fi
        else
            other+=("$f")
        fi
    done
    if [ "${#other[@]}" -eq 0 ] && [ "${#kept[@]}" -gt 0 ]; then
        git -C "$wt" rm --quiet -- "${kept[@]}"
        GIT_EDITOR=true git -C "$wt" cherry-pick --continue >/dev/null
        echo "  ok        $label (kept our deletion of ${kept[*]})"
        continue
    fi
    conflicts=$((conflicts + 1))
    echo "  CONFLICT  $label"
    for f in "${other[@]}" "${kept[@]}"; do echo "            $f"; done
    git -C "$wt" cherry-pick --abort
done

# 5. does the overlay still apply to the new engine tree?
if [ -n "$tree" ]; then
    echo "engine tree: $tree"
    if ! "$wt/tools/apply-overlay.sh" --dry-run --keep-going "$tree" | sed 's/^/  /'; then
        conflicts=$((conflicts + 1))
    fi
fi

# 6. result
new="$(git -C "$wt" rev-parse HEAD)"
if [ "$conflicts" -gt 0 ]; then
    echo "sync-upstream: $conflicts conflict(s); $MAIN unchanged at $(git rev-parse --short "$main_old")."
    echo "  Fix: git switch $MAIN && git rebase --onto $ref $(git rev-parse --short "$old") $MAIN, resolve each patch, then re-run this script."
    echo "  Note: $MIRROR already moved to $ref; to undo: git update-ref refs/heads/$MIRROR $old"
    exit 1
fi
if ! $update; then
    echo "sync-upstream: clean; --no-update given, $MAIN unchanged (result was $(git rev-parse --short "$new"))"
    exit 0
fi
git update-ref "refs/atlas/pre-sync/$MAIN" "$main_old"
if [ "$(git symbolic-ref -q --short HEAD || true)" = "$MAIN" ]; then
    git reset --quiet --keep "$new"
else
    git update-ref "refs/heads/$MAIN" "$new" "$main_old"
fi
echo "sync-upstream: clean; $MAIN $(git rev-parse --short "$main_old") -> $(git rev-parse --short "$new") on $ref"
echo "  check: git log --oneline $MIRROR..$MAIN"
echo "  push (lead): git push origin $MIRROR && git push --force-with-lease=$MAIN:$main_old origin $MAIN"

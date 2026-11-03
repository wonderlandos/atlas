#!/usr/bin/env bash
# Tests of tools/apply-overlay.sh and tools/sync-upstream.sh on tiny fake trees and repositories.
# No network, no engine tree needed.
# ok and bad always succeed, so "test && ok || bad" is a safe if-then-else here.
# shellcheck disable=SC2015
set -u
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
tools="$here/../../tools"
t="$(mktemp -d)"
trap 'rm -rf "$t"' EXIT
rc=0
ok() { echo "ok   $1"; }
bad() { echo "FAIL $1"; rc=1; }
G=(git -c user.name=test -c user.email=test@localhost -c init.defaultBranch=main -c advice.detachedHead=false)

# --- apply-overlay.sh ------------------------------------------------------------------------
mkdir -p "$t/fork/tools" "$t/fork/atlas/patches/privacy" "$t/tree/src"
cp "$tools/apply-overlay.sh" "$t/fork/tools/"
printf 'one\ntwo\nthree\nfour\n' > "$t/tree/src/f.txt"
cat > "$t/fork/atlas/patches/privacy/0001-a.patch" <<'P'
Atlas overlay patch 0001: privacy/a
Why: test

--- a/src/f.txt
+++ b/src/f.txt
@@ -1,4 +1,4 @@
 one
-two
+TWO
 three
 four
P
cat > "$t/fork/atlas/patches/privacy/0002-b.patch" <<'P'
--- a/src/f.txt
+++ b/src/f.txt
@@ -1,4 +1,4 @@
 one
 TWO
-three
+THREE
 four
P
printf 'privacy/0001-a.patch\nprivacy/0002-b.patch  # stacks on 0001\n' > "$t/fork/atlas/patches/series"
ao() { "$t/fork/tools/apply-overlay.sh" "$@"; }

out="$(ao --dry-run "$t/tree" 2>&1)"; r=$?
case "$out" in *"overlay: 2 ok, 0 failed of 2 (dry run"*) [ $r -eq 0 ] && ok "dry run, stacked patches" || bad "dry run exit $r";; *) bad "dry run: $out";; esac
[ "$(sed -n 2p "$t/tree/src/f.txt")" = two ] && ok "dry run leaves the tree alone" || bad "dry run wrote the tree"

sed -i 's/^three$/3/' "$t/tree/src/f.txt"
out="$(ao --dry-run --keep-going "$t/tree" 2>&1)"; r=$?
case "$out" in *"FAIL privacy/0002-b.patch"*"src/f.txt"*"Hunk #1 FAILED"*"1 ok, 1 failed"*) [ $r -eq 1 ] && ok "failing patch reported with file and hunk" || bad "exit $r";; *) bad "failing patch: $out";; esac
out="$(ao "$t/tree" 2>&1)"; r=$?
[ $r -eq 1 ] && [ "$(sed -n 2p "$t/tree/src/f.txt")" = TWO ] && [ "$(sed -n 3p "$t/tree/src/f.txt")" = 3 ] \
    && [ -z "$(find "$t/tree" -name '*.rej' -o -name '*.orig')" ] && ok "real apply stops at the failing patch, no partial change" \
    || bad "real apply: exit $r, $(tr '\n' ' ' < "$t/tree/src/f.txt"), $out"
sed -i 's/^3$/three/; s/^TWO$/two/' "$t/tree/src/f.txt"
echo 'privacy/0003-missing.patch' >> "$t/fork/atlas/patches/series"
out="$(ao --dry-run "$t/tree" 2>&1)"; r=$?
case "$out" in *"missing file: atlas/patches/privacy/0003-missing.patch"*) [ $r -eq 2 ] && ok "missing series entry refused" || bad "exit $r";; *) bad "missing entry: $out";; esac

# --- sync-upstream.sh ------------------------------------------------------------------------
up="$t/up"; mkdir -p "$up/.github/workflows"
(cd "$up" && "${G[@]}" init -q && echo base > README.md && echo ci > .github/workflows/ci.yml && echo p > lib.txt \
    && "${G[@]}" add -A && "${G[@]}" commit -qm "up: base" && "${G[@]}" tag 1.0)
"${G[@]}" clone -q "$up" "$t/fork2" 2>/dev/null
f2="$t/fork2"
(cd "$f2" && "${G[@]}" remote rename origin upstream && "${G[@]}" branch -q upstream 1.0 \
    && mkdir -p tools atlas/patches && cp "$tools/sync-upstream.sh" "$tools/apply-overlay.sh" tools/ && : > atlas/patches/series \
    && "${G[@]}" add -A && "${G[@]}" commit -qm "tools" \
    && "${G[@]}" rm -q .github/workflows/ci.yml && "${G[@]}" commit -qm "ci: remove" \
    && echo ours > README.md && "${G[@]}" commit -qam "README: ours")
(cd "$up" && echo ci2 > .github/workflows/ci.yml && echo p2 > lib.txt && "${G[@]}" commit -qam "up: change" && "${G[@]}" tag 1.1 \
    && echo later > lib.txt && "${G[@]}" commit -qam "up: untagged")
sy() { (cd "$f2" && GIT_AUTHOR_NAME=t GIT_AUTHOR_EMAIL=t@l GIT_COMMITTER_NAME=t GIT_COMMITTER_EMAIL=t@l tools/sync-upstream.sh "$@"); }
old_main="$(git -C "$f2" rev-parse main)"
out="$(sy 2>&1)"; r=$?
if [ $r -eq 0 ] && [ "$(git -C "$f2" rev-parse refs/heads/upstream)" = "$(git -C "$f2" rev-parse '1.1^{commit}')" ] \
    && [ "$(git -C "$f2" rev-list --count refs/heads/upstream..main)" = 3 ] && [ "$(git -C "$f2" show main:lib.txt)" = p2 ] \
    && ! git -C "$f2" cat-file -e main:.github/workflows/ci.yml 2>/dev/null \
    && [ "$(git -C "$f2" rev-parse refs/atlas/pre-sync/main)" = "$old_main" ]; then
    ok "sync to the newest tag: mirror fast-forwarded, 3 overlay commits replayed, our deletion kept"
else
    bad "sync clean: exit $r: $out"
fi
case "$out" in *"kept our deletion of .github/workflows/ci.yml"*) ok "kept deletion reported";; *) bad "kept deletion not reported: $out";; esac

(cd "$up" && echo theirs > README.md && "${G[@]}" commit -qam "up: readme" && "${G[@]}" tag 1.2)
before="$(git -C "$f2" rev-parse main)"
out="$(sy 2>&1)"; r=$?
case "$out" in *"CONFLICT"*"README: ours"*"README.md"*"main unchanged"*)
    [ $r -eq 1 ] && [ "$(git -C "$f2" rev-parse main)" = "$before" ] && ok "conflict reported per commit, main unchanged" || bad "conflict: exit $r";;
    *) bad "conflict: $out";; esac
[ -z "$(git -C "$f2" worktree list | sed 1d)" ] && ok "scratch worktree removed" || bad "worktree left: $(git -C "$f2" worktree list)"

git -C "$f2" update-ref refs/heads/upstream "$(git -C "$f2" rev-parse 1.0)" "$(git -C "$f2" rev-parse refs/heads/upstream)" 2>/dev/null
(cd "$up" && "${G[@]}" checkout -q -b rewritten 1.0 && echo x > other.txt && "${G[@]}" add other.txt && "${G[@]}" commit -qm "up: side" && "${G[@]}" tag 9.0)
git -C "$f2" update-ref refs/heads/upstream "$(git -C "$f2" rev-parse '1.2^{commit}')"
out="$(sy --ref 9.0 2>&1)"; r=$?
case "$out" in *"not a fast-forward"*) [ $r -eq 2 ] && ok "non-fast-forward of the mirror refused" || bad "nff exit $r";; *) bad "nff: $out";; esac
exit $rc

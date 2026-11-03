#!/usr/bin/env bash
# Derived from the upstream platform repository (GPL-3.0, see ../LICENSE). Changes: stage timing,
# the options of shared.sh (ATLAS_*), --pgo also with -f, -s STAGE (stop after it), -f STAGE (skip the stages before it:
# the prepared tree is reused; the version, name and i18n steps are not repeatable on a patched tree).
set -euo pipefail

clone=false
with_pgo=false
stop_after=""
from_stage=""

while [ $# -gt 0 ]; do
    case "$1" in
        -c) clone=true; shift;;
        --pgo) with_pgo=true; shift;;
        -s) stop_after="$2"; shift 2;;
        -f) from_stage="$2"; shift 2;;
        *) echo "build.sh: unknown option $1" >&2; exit 2;;
    esac
done

. "$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)/shared.sh"

setup_environment
# Official release builds use the engine's profile-guided optimization (the upstream CI passes --pgo);
# with -f the fetch step is skipped, so the flag is applied here as well.
if [ "$with_pgo" = true ] && [ "$_build_arch" = x64 ]; then
    _has_pgo=true
fi

STAGES="fetch patch domsub names i18n version resources gn build"
_idx() {
    local i=0 s
    for s in $STAGES; do
        if [ "$s" = "$1" ]; then echo "$i"; return; fi
        i=$((i + 1))
    done
    echo "build.sh: unknown stage $1 (stages: $STAGES)" >&2
    exit 2
}
# step NAME CMD...: run one stage unless -f puts it before the start; honour -s.
step() {
    local name="$1"
    shift
    if [ -n "$from_stage" ] && [ "$(_idx "$name")" -lt "$(_idx "$from_stage")" ]; then
        echo "build.sh: skip ${name}"
        return 0
    fi
    stage "$name" "$@"
    if [ "$stop_after" = "$name" ]; then echo "build.sh: stopped after $name"; exit 0; fi
}

# clean out/ directory before build
rm -rf "${_src_dir}/out" || true

step fetch fetch_sources "$clone" "$with_pgo"
step patch apply_patches
step domsub apply_domsub
step names helium_substitution
step i18n helium_apply_translations
step version helium_version
step resources helium_resources
write_gn_args
step gn gn_gen
step build build

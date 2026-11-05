# shared build functions used by local and CI scripts
# Derived from the upstream platform repository (GPL-3.0, see ../LICENSE; attribution in the WolOS
# docs/THIRD-PARTY.md). Changes: the fork tree is the main repository, ATLAS_* paths and job count,
# stage timing, optional extra GN lines.
#
# Environment (all optional):
#   ATLAS_FORK_ROOT   main repository (default: two levels above this platform directory)
#   ATLAS_BUILD_DIR   where build/ lives (default: <platform>/build); keep it fixed so cache keys match
#   ATLAS_DL_CACHE    download cache (default: $ATLAS_BUILD_DIR/download_cache)
#   ATLAS_JOBS        parallel jobs handed to the build tool (default: its own choice)
#   ATLAS_TIMINGS     file that gets one line per stage: "<stage> <rc> <seconds> <epoch-start>"
#   ATLAS_GN_EXTRA    extra args.gn lines (separated by newline or ;), appended last
#   ATLAS_TARGETS     build targets (default: chrome chromedriver)

if [ -n "${BASH_VERSION:-}" ]; then
    __helium_shared_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
elif [ -n "${ZSH_VERSION:-}" ]; then
    __helium_shared_dir="${0:a:h}"
else
    echo "shared.sh only supports bash and zsh" >&2
    return 1 2>/dev/null || exit 1
fi

# resolve repo root directory regardless of caller location
repo_root() {
    cd "${__helium_shared_dir}/.." >/dev/null 2>&1 && pwd
}

# stage NAME CMD...: run CMD, append "NAME rc seconds start" to $ATLAS_TIMINGS, keep CMD's status.
stage() {
    local name="$1" t0 rc=0
    shift
    t0=$(date +%s)
    echo "=== stage ${name} start $(date -u +%FT%TZ)" >&2
    "$@" || rc=$?
    echo "=== stage ${name} rc=${rc} $(( $(date +%s) - t0 )) s" >&2
    if [ -n "${ATLAS_TIMINGS:-}" ]; then
        echo "${name} ${rc} $(( $(date +%s) - t0 )) ${t0}" >> "${ATLAS_TIMINGS}"
    fi
    return "$rc"
}

setup_arch() {
    _build_arch="${ARCH:-x64}"

    if [ "$_build_arch" = "x86_64" ]; then
        _build_arch=x64
    fi
}

setup_paths() {
    _root_dir="$(repo_root)"
    _main_repo="${ATLAS_FORK_ROOT:-$(cd "${_root_dir}/../.." >/dev/null 2>&1 && pwd)}"
    _build_dir="${ATLAS_BUILD_DIR:-${_root_dir}/build}"
    _dl_cache="${ATLAS_DL_CACHE:-${_build_dir}/download_cache}"
    _src_dir="${_build_dir}/src"
    _out_dir="${_src_dir}/out/Default"

    _subs_cache="${_build_dir}/subs.tar.gz"
    _namesubs_cache="${_build_dir}/namesubs.tar"

    mkdir -p "${_dl_cache}"
}

setup_environment() {
    setup_paths
    setup_arch

    _has_pgo=false
}

fetch_sources() {
    local use_clone="${1:-false}"
    local with_pgo="${2:-false}"
    local stamp="${_src_dir}/.downloaded.stamp"
    _has_pgo=false
    if [ "$with_pgo" = true ] && [ "$_build_arch" = x64 ]; then
        _has_pgo=true
    fi

    if [ -f "${stamp}" ]; then
        echo "Sources already present, skipping download/unpack"
        return 0
    fi

    if [ "$use_clone" = true ]; then
        "${_main_repo}/utils/clone.py" -o "${_src_dir}"
    else
        "${_main_repo}/utils/downloads.py" retrieve -i "${_main_repo}/downloads.ini" "${_root_dir}/downloads.ini" -c "${_dl_cache}"
        "${_main_repo}/utils/downloads.py" unpack -i "${_main_repo}/downloads.ini" "${_root_dir}/downloads.ini" -c "${_dl_cache}" "${_src_dir}"
    fi

    "${_main_repo}/utils/downloads.py" retrieve -i "${_main_repo}/deps.ini" -c "${_dl_cache}"
    "${_main_repo}/utils/downloads.py" unpack -i "${_main_repo}/deps.ini" -c "${_dl_cache}" "${_src_dir}"

    touch "${stamp}"
}

apply_patches() {
    if [ ! -f "${_src_dir}/.patched.stamp" ]; then
        "${_main_repo}/utils/prune_binaries.py" --keep-build-dependencies "${_src_dir}" "${_main_repo}/pruning.list"
        # One combined series (inherited minus atlas/drop.list, platform, overlay series and the
        # area fragments atlas/patches/series.d/*.series), written by atlas-series; the overlay's
        # new-file directories (atlas/tree.list) are copied in first so patches may hook them.
        local combined
        combined="$(mktemp -d "${TMPDIR:-/tmp}/atlas-series.XXXXXX")"
        python3 -I "${_main_repo}/atlas/tools/atlas-series" --root "${_main_repo}" write-series "${combined}" >/dev/null
        python3 -I "${_main_repo}/atlas/tools/atlas-series" --root "${_main_repo}" install-tree "${_src_dir}"
        "${_main_repo}/utils/patches.py" apply "${_src_dir}" "${combined}"
        rm -rf "${combined}"
        touch "${_src_dir}/.patched.stamp"
    fi
}

apply_domsub() {
    if [ ! -f "${_src_dir}/.domsub.stamp" ]; then
        "${_main_repo}/utils/domain_substitution.py" apply -r "${_main_repo}/domain_regex.list" -f "${_main_repo}/domain_substitution.list" "${_src_dir}"
        touch "${_src_dir}/.domsub.stamp"
    fi
}

helium_substitution() {
    python3 "$_main_repo/utils/name_substitution.py" --sub \
        -t "$_src_dir" --backup-path "$_namesubs_cache"
}

helium_apply_translations() {
    python3 "$_main_repo/utils/i18n_apply.py" -t "$_src_dir"
}

helium_version() {
    python3 "$_main_repo/utils/helium_version.py" \
        --tree "$_main_repo" \
        --platform-tree "$_root_dir" \
        --chromium-tree "$_src_dir"
}

helium_resources() {
    python3 "$_main_repo/utils/generate_resources.py" "$_main_repo/resources/generate_resources.txt" "$_main_repo/resources"
    python3 "$_main_repo/utils/replace_resources.py" "$_main_repo/resources/helium_resources.txt" "$_main_repo/resources" "$_src_dir"
}

# Stage `atlas`: the overlay's transforms (brand strings, scheme, hosts, the blocker's list set, the brand
# slot; atlas/transform/README.md) once, after `resources` and before gn. apply.py writes .atlas.stamp and
# refuses a second run; the stamp makes the stage a no-op on a prepared tree.
atlas_transforms() {
    if [ -f "${_main_repo}/atlas/transform/apply.py" ] && [ ! -f "${_src_dir}/.atlas.stamp" ]; then
        python3 -I "${_main_repo}/atlas/transform/apply.py" --apply "${_src_dir}"
    fi
}

write_gn_args() {
    mkdir -p "${_out_dir}"

    # the overlay's own arguments come last, so they win over the inherited ones
    cat "${_main_repo}/flags.gn" "${_root_dir}/flags.atlas.linux.gn" "${_main_repo}/atlas/flags.atlas.gn" \
        | tee "${_out_dir}/args.gn"
    echo "target_cpu = \"$_build_arch\"" | tee -a "${_out_dir}/args.gn"
    echo "v8_target_cpu = \"$_build_arch\"" | tee -a "${_out_dir}/args.gn"

    if [ "$_has_pgo" = true ]; then
        echo "chrome_pgo_phase = 2" | tee -a "${_out_dir}/args.gn"
    fi

    if [ -n "${SISO_REAPI_ADDRESS:-}" ]; then
        echo 'use_remoteexec = true' | tee -a "${_out_dir}/args.gn"
    elif command -v sccache >/dev/null 2>&1 && env | grep -q ^SCCACHE; then
        echo 'cc_wrapper = "sccache"' | tee -a "${_out_dir}/args.gn"
    elif command -v ccache >/dev/null; then
        echo 'cc_wrapper = "ccache"' | tee -a "${_out_dir}/args.gn"
    fi

    if [ -n "${ATLAS_GN_EXTRA:-}" ]; then
        printf '%s\n' "${ATLAS_GN_EXTRA}" | tr ';' '\n' | tee -a "${_out_dir}/args.gn"
    fi
}

configure_remoteexec() {
    if [ -z "${SISO_REAPI_ADDRESS:-}" ]; then
        return 0
    fi

    export SISO_REAPI_INSTANCE="${SISO_REAPI_INSTANCE:-main}"
    export RBE_service_no_security=true

    python3 "${_src_dir}/build/config/siso/configure_siso.py" \
        --reapi_address="${SISO_REAPI_ADDRESS}" \
        --reapi_instance="${SISO_REAPI_INSTANCE}" \
        --reapi_backend_config_path=nativelink.star
}

gn_gen() {
    cd "${_src_dir}"
    ./buildtools/linux64/gn gen out/Default --fail-on-unused-args
}

build() {
    cd "${_src_dir}"
    configure_remoteexec
    local siso="${SISO_PATH:-${_src_dir}/third_party/siso/cipd/siso}"
    local autoninja="${_src_dir}/third_party/depot_tools/autoninja.py"
    local jobs=()
    if [ -n "${ATLAS_JOBS:-}" ]; then
        jobs=(-j "${ATLAS_JOBS}")
    fi
    # shellcheck disable=SC2086  # ATLAS_TARGETS is a word list on purpose
    SISO_PATH="$siso" python3 "$autoninja" -C "$_out_dir" "${jobs[@]}" ${ATLAS_TARGETS:-chrome chromedriver} "$@"
}

#!/usr/bin/env bash
# Derived from the upstream platform repository (GPL-3.0, see ../LICENSE). Changes: the tarball only
# (no installer image, no signing), no pv/zip dependency, ATLAS_* paths (see shared.sh).
# Layout of the tarball is the one of the upstream release asset, under our names
# (atlas-<version>-x86_64_linux/: binary atlas, atlas_crashpad_handler, atlas.desktop, atlas-wrapper).
set -euo pipefail

_current_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
_root_dir="$(cd "$_current_dir/.." && pwd)"
_main_repo="${ATLAS_FORK_ROOT:-$(cd "$_root_dir/../.." && pwd)}"
_build_dir="${ATLAS_BUILD_DIR:-$_root_dir/build}"
_release_dir="$_build_dir/release"

_app_name="atlas"
_version=$(python3 "$_main_repo/utils/helium_version.py" \
                   --tree "$_main_repo" \
                   --platform-tree "$_root_dir" \
                   --print)

_arch=$(cat "$_build_dir/src/out/Default/args.gn" \
                | grep ^target_cpu \
                | tail -1 \
                | sed 's/.*=//' \
                | cut -d'"' -f2)

if [ "$_arch" = "x64" ]; then
    _arch="x86_64"
fi

_release_name="$_app_name-$_version-$_arch"
_tarball_name="${_release_name}_linux"
_tarball_dir="$_release_dir/$_tarball_name"

_files="atlas
chrome_100_percent.pak
chrome_200_percent.pak
atlas_crashpad_handler
chromedriver
icudtl.dat
libEGL.so
libGLESv2.so
libqt5_shim.so
libqt6_shim.so
libvk_swiftshader.so
libvulkan.so.1
product_logo_256.png
resources.pak
v8_context_snapshot.bin
vk_swiftshader_icd.json
xdg-mime
xdg-settings"

echo "copying release files and creating $_tarball_name.tar.xz"

rm -rf "$_tarball_dir"
mkdir -p "$_tarball_dir"

for file in $_files; do
    cp -r "$_build_dir/src/out/Default/$file" "$_tarball_dir" &
done

mkdir -p "$_tarball_dir/locales"
cp "$_build_dir/src/out/Default/locales/"*.pak "$_tarball_dir/locales/"

cp "$_root_dir/package/atlas.desktop" "$_tarball_dir"
cp "$_root_dir/package/apparmor.cfg" "$_tarball_dir"
cp "$_root_dir/package/atlas-wrapper.sh" "$_tarball_dir/atlas-wrapper"

# The Federate helper (ADR 0052, overlay area fed): the browser starts atlas-fed from the directory of its
# own executable on the first fed:// load. It is built from the WolOS source (crates/atlas-fed) and handed
# over with ATLAS_FED_HELPER; the official build sets it, a developer build without it shows the honest
# "helper is not at ..." error on a fed:// page.
if [ -n "${ATLAS_FED_HELPER:-}" ]; then
    if [ ! -x "$ATLAS_FED_HELPER" ]; then
        echo "package.sh: ATLAS_FED_HELPER=$ATLAS_FED_HELPER is not an executable file" >&2
        exit 2
    fi
    install -m 0755 "$ATLAS_FED_HELPER" "$_tarball_dir/atlas-fed"
else
    echo "package.sh: warning: no ATLAS_FED_HELPER, this tarball has no atlas-fed (fed:// will not load)" >&2
fi

wait
(cd "$_tarball_dir" && ln -sf atlas chrome)

# symbols: the unstripped ELF files, relative to out/Default
_syms_zip="$_release_dir/${_release_name}_symbols.zip"
rm -f "$_syms_zip"
mapfile -t _elfs < <(find "$_tarball_dir" -type f -exec file {} + \
    | awk -F: '/ELF/ {print $1}' \
    | sed "s|^$_tarball_dir/||")
(cd "$_build_dir/src/out/Default" && python3 -I -m zipfile -c "$_syms_zip" "${_elfs[@]}")

if command -v eu-strip >/dev/null 2>&1; then
    _strip_cmd=eu-strip
else
    _strip_cmd="strip --strip-unneeded"
fi

find "$_tarball_dir" -type f -exec file {} + \
    | awk -F: '/ELF/ {print $1}' \
    | xargs $_strip_cmd

cd "$_release_dir"
TAR_PATH="$_release_dir/$_tarball_name.tar.xz"
tar cf - "$_tarball_name" | xz -e9 -T"${ATLAS_XZ_THREADS:-4}" > "$TAR_PATH"

if [ "${MAKE_DEB:-0}" = 1 ]; then
    echo "package.sh: MAKE_DEB is not supported in the atlas overlay" >&2
    exit 2
fi

echo "tarball: $TAR_PATH"
rm -rf "$_tarball_dir"

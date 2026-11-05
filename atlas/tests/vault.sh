#!/usr/bin/env bash
# Tests of the vault area (patch 0060 and the drop of the inherited credential block): drop.list holds
# exactly the one leaf patch, the combined series no longer has it and has 0060, the fragment keeps to
# its range, the strings are plain words without another product's name, the patch touches only its
# four files and keeps the settings key in step between the page and the string table. Optional
# argument: a pristine prepared tree, for the dry-run of the whole series (what the build applies).
set -u
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$here/../.."
tool="$root/atlas/tools/atlas-series"
rc=0
ok() { echo "ok   $1"; }
bad() { echo "FAIL $1"; rc=1; }

drop="$(grep -v '^[[:space:]]*\(#\|$\)' "$root/atlas/drop.list")"
[ "$drop" = "helium/core/disable-password-manager.patch" ] && ok "drop.list: the one credential patch" || bad "drop.list: '$drop'"

list="$(python3 -I "$tool" list 2>&1)"
n="$(printf '%s\n' "$list" | grep -c 'disable-password-manager')"
[ "$n" = 0 ] && ok "series: credential block patch not applied" || bad "series: disable-password-manager still listed ($n)"
printf '%s\n' "$list" | grep -q 'ui/0060-passwords-entry.patch' && ok "series: 0060 listed" || bad "series: 0060 missing"
printf '%s\n' "$list" | grep -q 'remove-autofill.patch' && ok "series: remove-autofill kept (0060 adds the entry back, not the section)" || bad "series: remove-autofill dropped"

frag="$root/atlas/patches/series.d/60-vault.series"
bad_nums="$(grep -v '^[[:space:]]*\(#\|$\)' "$frag" | sed -n 's#^[a-z]*/\([0-9]\{4\}\)-.*#\1#p' | awk '$1<60||$1>69')"
[ -z "$bad_nums" ] && ok "fragment: numbers inside 0060-0069" || bad "fragment: outside range: $bad_nums"

patch="$root/atlas/patches/ui/0060-passwords-entry.patch"
files="$(sed -n 's#^+++ b/##p' "$patch" | sort | tr '\n' ' ')"
want="chrome/browser/resources/settings/settings_menu/settings_menu.html chrome/browser/resources/settings/settings_menu/settings_menu.ts chrome/browser/ui/toolbar/app_menu_model.cc chrome/browser/ui/webui/settings/settings_localized_strings_provider.cc "
[ "$files" = "$want" ] && ok "0060: touches its four files only" || bad "0060: files '$files'"

grep -q 'i18n{atlasPasswordsEntry}' "$patch" && grep -q '{"atlasPasswordsEntry", IDS_ATLAS_VAULT_SETTINGS_PASSWORDS}' "$patch" \
  && ok "0060: settings key used by the page and defined in the string table" || bad "0060: settings key out of step"
grep -q 'IDS_ATLAS_VAULT_APP_MENU_PASSWORDS' "$patch" && ok "0060: app menu item uses the vault string" || bad "0060: app menu string missing"

# 0061: the keyring entry carries our name, not the engine's (visible in secret-tool and the keyring manager)
p61="$root/atlas/patches/security/0061-keyring-label.patch"
[ "$(sed -n 's#^+++ b/##p' "$p61")" = "components/os_crypt/async/browser/freedesktop_secret_key_provider.h" ] && ok "0061: touches the one header" || bad "0061: files"
added="$(grep '^+ ' "$p61")"
for want in 'kKWalletFolder[] = "Atlas Keys"' 'kKeyName[] = "Atlas Safe Storage"' 'kAppName[] = "atlas"'; do
  printf '%s\n' "$added" | grep -qF "$want" && ok "0061: sets $want" || bad "0061: $want missing"
done
printf '%s\n' "$added" | grep -qi 'chrom' && bad "0061: an added line still names the engine" || ok "0061: no engine name in the added lines"

# 0062: saving passwords is offered by default (the inherited default-pref patch registers it off)
p62="$root/atlas/patches/security/0062-password-saving-default-on.patch"
[ "$(sed -n 's#^+++ b/##p' "$p62")" = "components/password_manager/core/browser/password_manager.cc" ] && ok "0062: touches the one file" || bad "0062: files"
[ "$(grep -c '^[-+] .*kCredentialsEnableService' "$p62")" = 2 ] && grep -q '^+ *prefs::kCredentialsEnableService, true,' "$p62" && ok "0062: kCredentialsEnableService false -> true, nothing else" || bad "0062: the one-value change"

# 0063: the fixed-key (v10) provider only reads when the explicit keyring store is chosen
p63="$root/atlas/patches/security/0063-keyring-required.patch"
want63="chrome/browser/browser_process_impl.cc components/os_crypt/async/browser/posix_key_provider.cc components/os_crypt/async/browser/posix_key_provider.h "
[ "$(sed -n 's#^+++ b/##p' "$p63" | sort | tr '\n' ' ')" = "$want63" ] && ok "0063: touches its three files" || bad "0063: files"
grep -q 'password_store !=' "$p63" && grep -q '"gnome-libsecret"' "$p63" && grep -q 'return use_for_encryption_;' "$p63" \
  && ok "0063: encryption off for the fixed-key provider only with gnome-libsecret" || bad "0063: logic lines missing"

# 0063: with the explicit keyring store the fixed-key (v10) provider only reads
p63="$root/atlas/patches/security/0063-keyring-required.patch"
[ "$(sed -n 's#^+++ b/##p' "$p63" | sort | tr '\n' ' ')" = "chrome/browser/browser_process_impl.cc components/os_crypt/async/browser/posix_key_provider.cc components/os_crypt/async/browser/posix_key_provider.h " ] && ok "0063: touches its three files" || bad "0063: files"
grep -q 'password_store !=' "$p63" && grep -q '"gnome-libsecret"' "$p63" && grep -q 'return use_for_encryption_;' "$p63" && ok "0063: encryption off for the v10 provider when the store is gnome-libsecret" || bad "0063: logic missing"

grd="$root/chrome/app/atlas/vault_strings.grdp"
for id in IDS_ATLAS_VAULT_APP_MENU_PASSWORDS IDS_ATLAS_VAULT_SETTINGS_PASSWORDS; do
  grep -q "name=\"$id\"" "$grd" && ok "strings: $id defined" || bad "strings: $id missing"
done
texts="$(sed -n '/^[[:space:]]*<message /,/<\/message>/p' "$grd" | grep -v '<message\|</message>' | sed 's/^[[:space:]]*//')"
if printf '%s' "$texts" | grep -qi 'helium\|chrom\|google\|brave\|firefox\|ungoogled\|!'; then bad "strings: a product name or an exclamation mark"; else ok "strings: no product name, no exclamation mark"; fi
printf '%s\n' "$texts" | grep -vq '^[A-Z][a-z ]*$' && bad "strings: not sentence case" || ok "strings: sentence case"

if [ $# -ge 1 ]; then
  out="$(python3 -I "$tool" --dry-run "$1" 2>&1)"
  case "$out" in *"applies clean"*) ok "dry-run: $out" ;; *) bad "dry-run: $out" ;; esac
fi
exit $rc

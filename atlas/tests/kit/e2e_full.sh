#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# e2e_full.sh THEME NAME [chrome flags and URLs...]: ATLAS-E2E's own browser instance, independent of slot 8's run.sh.
# Uses a SNAPSHOT of the slot 8 binary in /scratch/atlas-e2e/bin, its own cage (run dir), profile, CDP port and shots dir.
# env: PORT (9360), WAIT (8), PROFILE_KEEP=1 (keep the profile: session restore tests), PREFS (json for Default/Preferences)
THEME=$1; NAME=$2; shift 2
K=/scratch/atlas-e2e; BIN=${BIN:-$K/bin/atlas}
R=$K/run-$NAME; P=$K/profile-$NAME
mkdir -p $R $K/shots; chmod 700 $R
[ -f $R/cage.pid ] && { pkill -TERM -P $(cat $R/cage.pid) 2>/dev/null; kill -TERM $(cat $R/cage.pid) 2>/dev/null; sleep 1; }
[ -z "$PROFILE_KEEP" ] && rm -rf $P
mkdir -p $P/Default
mkdir -p $K/config/$NAME/gtk-3.0
cat > $K/config/$NAME/gtk-3.0/settings.ini <<S
[Settings]
gtk-theme-name=WolOS-$THEME-gtk3
gtk-font-name=Inter 10
gtk-application-prefer-dark-theme=$([ $THEME = wol-dark ] && echo 1 || echo 0)
gtk-decoration-layout=:minimize,maximize,close
S
for t in $K/data/themes/WolOS-$THEME-gtk3/gtk-3.0/gtk.css $K/data/themes/WolOS-$THEME-gtk3/gtk-3.0/gtk-dark.css; do
  sed -i "s|file:///scratch/atlas-int/run/gtk/data/themes|file://$K/data/themes|; s|file:///usr/share/themes|file://$K/data/themes|; s|file:///scratch/atlas-slot8/kitrun/data/themes|file://$K/data/themes|" $t
done
if [ ! -f $P/Default/Preferences ]; then
  if [ -n "$PREFS" ]; then printf '%s' "$PREFS" > $P/Default/Preferences
  else printf '%s' '{"extensions":{"theme":{"system_theme":1}},"browser":{"theme":{"color_scheme2":0},"custom_chrome_frame":false,"show_home_button":false}}' > $P/Default/Preferences; fi
fi
[ -f "$P/Local State" ] || echo "{\"internal_only_uis_enabled\":true,\"browser\":{\"enabled_labs_experiments\":[]}}" > "$P/Local State"
(
 export XDG_RUNTIME_DIR=$R WLR_BACKENDS=headless WLR_LIBINPUT_NO_DEVICES=1 WLR_RENDERER=pixman WAYLAND_DISPLAY=wayland-0 WLR_HEADLESS_OUTPUTS=1
 export XDG_DATA_HOME=$K/data XDG_CONFIG_HOME=$K/config/$NAME GTK_THEME=WolOS-$THEME-gtk3$([ $THEME = wol-dark ] && echo ":dark")
 [ -n "$LANGV" ] && export LANG=$LANGV LANGUAGE=$LANGV LC_ALL=$LANGV
 setsid nohup cage -- $BIN --ozone-platform=wayland --disable-gpu --no-sandbox --user-data-dir=$P ${FIRSTRUN:---no-first-run} \
   --password-store=basic --remote-debugging-port=${PORT:-9360} "$@" > $K/work/$NAME.log 2>&1 &
 echo $! > $R/cage.pid
)
sleep ${WAIT:-8}
echo started $NAME

#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# run.sh THEME OUT [chrome flags and URLs...]: the slot 8 build under cage with the WolOS GTK3 theme (copy of atlas-int runshot.sh)
THEME=$1; OUT=$2; shift 2
K=/scratch/atlas-slot8/kitrun; BIN=/scratch/atlas-slot8/src/out/Slot/atlas
R=$K/run-$THEME; P=$K/profile-$THEME
mkdir -p $R $OUT; chmod 700 $R
[ -f $R/cage.pid ] && { pkill -TERM -P $(cat $R/cage.pid) 2>/dev/null; kill -TERM $(cat $R/cage.pid) 2>/dev/null; sleep 1; }
rm -rf $P; mkdir -p $P/Default
cat > $K/config/gtk-3.0/settings.ini <<S
[Settings]
gtk-theme-name=WolOS-$THEME-gtk3
gtk-font-name=Inter 10
gtk-application-prefer-dark-theme=$([ $THEME = wol-dark ] && echo 1 || echo 0)
gtk-decoration-layout=:minimize,maximize,close
S
for t in $K/data/themes/WolOS-$THEME-gtk3/gtk-3.0/gtk.css $K/data/themes/WolOS-$THEME-gtk3/gtk-3.0/gtk-dark.css; do
  sed -i "s|file:///scratch/atlas-int/run/gtk/data/themes|file://$K/data/themes|; s|file:///usr/share/themes|file://$K/data/themes|" $t
done
cat > $P/Default/Preferences <<S
{"extensions":{"theme":{"system_theme":1}},"browser":{"theme":{"color_scheme2":0},"custom_chrome_frame":false,"show_home_button":false}}
S
echo "{\"browser\":{\"enabled_labs_experiments\":[]}}" > "$P/Local State"
(
 export XDG_RUNTIME_DIR=$R WLR_BACKENDS=headless WLR_LIBINPUT_NO_DEVICES=1 WLR_RENDERER=pixman WAYLAND_DISPLAY=wayland-0 WLR_HEADLESS_OUTPUTS=1
 export XDG_DATA_HOME=$K/data XDG_CONFIG_HOME=$K/config GTK_THEME=WolOS-$THEME-gtk3$([ $THEME = wol-dark ] && echo ":dark")
 setsid nohup cage -- $BIN --ozone-platform=wayland --disable-gpu --no-sandbox --user-data-dir=$P --no-first-run \
   --password-store=basic --remote-debugging-port=${PORT:-9348} "$@" > $OUT/run.log 2>&1 &
 echo $! > $R/cage.pid
)
sleep ${WAIT:-8}
echo started

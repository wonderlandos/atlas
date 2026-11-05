#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# hover.sh THEME X Y [MS]: puts the pointer at X Y and keeps the virtual pointer alive for MS ms (default 3000) in
# the background; the compositor forgets the pointer when vinput exits, so a hover can only be seen (shot.sh,
# grim) while it lives. Returns after 0.7 s, when the hover has been painted.
K=/scratch/atlas-slot8/kitrun; T=$1
HOLD_MS=${4:-3000} XDG_RUNTIME_DIR=$K/run-$T WAYLAND_DISPLAY=wayland-0 $K/vinput 1280x720 move "$2" "$3" >/dev/null 2>&1 &
sleep 0.7

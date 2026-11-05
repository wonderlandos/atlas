#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# shot.sh THEME OUT.png ; in.sh THEME vinput-args...
K=/scratch/atlas-slot8/kitrun; XDG_RUNTIME_DIR=$K/run-$1 WAYLAND_DISPLAY=wayland-0 grim "$2"

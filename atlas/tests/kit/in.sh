#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
K=/scratch/atlas-slot8/kitrun; T=$1; shift; XDG_RUNTIME_DIR=$K/run-$T WAYLAND_DISPLAY=wayland-0 $K/vinput 1280x720 "$@"

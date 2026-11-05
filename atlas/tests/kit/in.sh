#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# in.sh THEME vinput-args... : vinput makes a virtual pointer and keyboard per call and the compositor drops the
# first KEY of each call: callers prepend a throw-away key (evdev 194, F24) to key calls. The pointer position is
# only kept while vinput lives: for a hover, see hover.sh.
K=/scratch/atlas-slot8/kitrun; T=$1; shift
XDG_RUNTIME_DIR=$K/run-$T WAYLAND_DISPLAY=wayland-0 $K/vinput 1280x720 "$@"

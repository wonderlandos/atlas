#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# fed_omnibox_journey.sh: the live journey of overlay patch 0124 (QA-A9-WEB-ATLAS-1), ON the forge with a built dev slot
# (default slot 4, kitrun scripts as in atlas/tests/kit/run.sh with the paths of the slot): start the browser, type the bare
# name lobo.pagina in the address bar, press Enter. The tab must end at fed://lobo.pagina/ and no search-engine page may open;
# without the helper beside the binary the page is the one that says so (QA-A9-WEB-ATLAS-9).
set -u
SLOT=${1:-4}; K=/scratch/atlas-slot$SLOT/kitrun; PORT=$((9330 + SLOT)); T=wol-light
cd "$K" || exit 2
WAIT=9 ./run.sh $T /scratch/atlas-slot$SLOT/a9out about:blank > /dev/null 2>&1
./in.sh $T click 640 17; sleep 0.5
./in.sh $T key 194 38 24 48 24 52 25 30 34 23 49 30   # l o b o . p a g i n a
sleep 0.8; ./in.sh $T key 194 28; sleep 3
url=$(python3 -I cdp.py $PORT tabs | python3 -c 'import json,sys; print(json.load(sys.stdin)[0]["url"])')
title=$(python3 -I cdp.py $PORT tabs | python3 -c 'import json,sys; print(json.load(sys.stdin)[0]["title"])')
echo "url=$url title=$title"
case "$url" in fed://lobo.pagina/*) echo "PASS a bare Federate name opens fed://";; *) echo "FAIL opened $url"; exit 1;; esac

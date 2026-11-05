#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# a9_pages.sh: the pages e2e_a9.py needs, in /scratch/atlas-slot8/a9www (run ON the forge), and the server on :8147.
mkdir -p /scratch/atlas-slot8/a9www; cd /scratch/atlas-slot8/a9www || exit 1
python3 -I - <<'P'
import wave, struct, math
w = wave.open('tone.wav', 'wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(22050)
w.writeframes(b''.join(struct.pack('<h', int(12000 * math.sin(2 * math.pi * 440 * i / 22050))) for i in range(22050 * 20))); w.close()
for i in range(1, 41):
    open(f'p{i}.html', 'w').write(f'<!doctype html><title>Page {i}</title><h1>Page {i}</h1>')
for n in ('Alpha', 'Bravo', 'Charlie', 'Delta'):
    open(f'{n.lower()}.html', 'w').write(f'<!doctype html><title>{n}</title><h1>{n}</h1>')
open('audio.html', 'w').write('<!doctype html><title>Music</title><h1>Music</h1><audio id=a src=tone.wav loop autoplay></audio><script>document.getElementById("a").play().catch(()=>{})</script>')
P
pgrep -f "http.server 8147" > /dev/null || (setsid nohup python3 -m http.server 8147 --bind 127.0.0.1 > /dev/null 2>&1 &)

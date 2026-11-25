#!/usr/bin/python3
"""Generate a reasonable first-run control profile for every entry.

Heuristic only: if the movie references mouse handlers, ship a profile with a
pointer on the right stick and a left click on L2; otherwise leave the compiled
defaults (D-pad arrows + J/K/U/I/O/L). Existing user profiles are never touched
because these files live in defaults/<sha256>.profile and are only consulted
when a game has no profile yet.
"""
import hashlib
import runpy
import sys
import zlib
from pathlib import Path

KIT = Path.home() / '.local/share/treeos-flash'
BASE = KIT / 'flash-play'
DROP = Path('/roms/flash-play')

MOUSE_HINTS = (b'onMouseDown', b'onMouseUp', b'onMouseMove', b'MouseEvent', b'startDrag', b'flash.events.MouseEvent')
POINTER = "pointer_source=right_stick\nl2_vk=mouse_left\n"


def touches_mouse(path):
    with path.open('rb') as f:
        head = f.read(8)
        data = f.read()
    if head[:3] == b'CWS':
        try:
            data = zlib.decompress(data)
        except zlib.error:
            pass
    return any(h in data for h in MOUSE_HINTS)


def main():
    m = runpy.run_path(str(BASE / 'bin/flash-play.py'))
    out = BASE / 'defaults'
    out.mkdir(exist_ok=True)
    made = skipped = mouse = 0
    for entry in sorted(list(DROP.glob('*.swf')) + list(DROP.glob('*.flashref'))):
        try:
            plan = m['resolve'](entry)
        except Exception:
            continue
        if plan['source'].parent == out:
            skipped += 1
            continue
        if plan['source'].is_file():
            skipped += 1
            continue
        movie = plan['movie']
        digest = hashlib.sha256(movie.read_bytes()).hexdigest()
        target = out / (digest + '.profile')
        if target.exists():
            skipped += 1
            continue
        body = ""
        if touches_mouse(movie):
            body = POINTER
            mouse += 1
        target.write_text(body)
        made += 1
    print(f'defaults: wrote {made} ({mouse} with mouse pointer), skipped {skipped}')
    return 0


if __name__ == '__main__':
    sys.exit(main())

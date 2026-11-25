#!/usr/bin/python3
"""Audit every Flash entry: is it launchable at all, and with what?

    flash-doctor.py            # table for the terminal
    flash-doctor.py --json     # machine readable

Checks, per entry: movie resolves, SWF header sane, state dir writable, which
profile would be used, whether it needs the network/spoof runtime, and whether
the entry has a cover. Never starts a game and never writes user data.
"""
import hashlib
import json
import os
import runpy
import sys
from pathlib import Path

KIT = Path(os.environ.get('TREEOS_FLASH_KIT', Path.home() / '.local/share/treeos-flash'))
BASE = KIT / 'flash-play'
DROP = Path(os.environ.get('TREEOS_FLASH_DROP', '/roms/flash-play'))


def main(argv):
    m = runpy.run_path(str(BASE / 'bin/flash-play.py'))
    rows = []
    entries = sorted(list(DROP.glob('*.swf')) + list(DROP.glob('*.flashref')))
    for entry in entries:
        row = {'entry': entry.name, 'ok': True, 'notes': []}
        try:
            plan = m['resolve'](entry)
        except Exception as exc:                     # noqa: BLE001 - report, don't crash
            row.update(ok=False)
            row['notes'].append(f'resolve failed: {exc}')
            rows.append(row)
            continue
        movie = plan['movie']
        with movie.open('rb') as f:
            head = f.read(8)
        if head[:3] not in (b'FWS', b'CWS', b'ZWS'):
            row['ok'] = False
            row['notes'].append('bad SWF header')
        try:
            m['preflight'](movie)
        except Exception as exc:                     # noqa: BLE001
            row['ok'] = False
            row['notes'].append(str(exc)[:70])
        row['runtime'] = m['runtime_for'](movie).name
        try:
            spoof = m['spoof_for'](movie)[0]
            if spoof:
                row['runtime'] = 'runtime-spoof1'
                row['notes'].append('spoof ' + spoof)
        except Exception:
            pass
        net = m['network_for'](movie)
        if net:
            row['runtime'] = 'runtime-net2'
            row['notes'].append('net ' + ','.join(net.get('hosts', [])))
        source = plan['source']
        row['profile'] = 'shipped-default' if source.parent == BASE / 'defaults' else (
            source.name if source.is_file() else 'none')
        if not row['ok']:
            pass
        state = plan['state']
        if state.exists() and not os.access(state, os.W_OK):
            row['ok'] = False
            row['notes'].append('state not writable')
        cover = DROP / 'images' / (entry.stem + '.jpg')
        row['cover'] = cover.is_file()
        rows.append(row)
    if '--json' in argv:
        print(json.dumps(rows, ensure_ascii=False, indent=1))
        return 0
    bad = [r for r in rows if not r['ok']]
    no_profile = [r for r in rows if r.get('profile') == 'none']
    no_cover = [r for r in rows if not r.get('cover')]
    for r in rows:
        flag = 'ok ' if r['ok'] else 'BAD'
        print(f"{flag} {r['entry'][:46]:46} {r.get('runtime','')[:16]:16} "
              f"{r.get('profile','')[:16]:16} {' '.join(r['notes'])[:60]}")
    print(f"\n{len(rows)} 条目：{len(rows) - len(bad)} 可启动，{len(bad)} 有问题；"
          f"{len(no_profile)} 无键位（会用默认），{len(no_cover)} 无封面")
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

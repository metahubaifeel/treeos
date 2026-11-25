#!/usr/bin/python3
"""Snapshot and restore one game's Flash save directory.

    flash-saves.py list    STATE_DIR
    flash-saves.py backup  STATE_DIR [--keep N]
    flash-saves.py restore STATE_DIR [SNAPSHOT]
    flash-saves.py apply-pending STATE_DIR

STATE_DIR is games/<id>/; the live saves are STATE_DIR/storage. Snapshots are
whole-tree copies under snapshots/<timestamp>-<rand>/ with a manifest, written
to a temp dir, fsynced and renamed into place. Restore is intended to run while
the player is NOT running (see apply-pending).
"""
import hashlib
import json
import os
import shutil
import sys
import time
import uuid
from pathlib import Path

FORMAT = 1


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def tree(root):
    """Every file and directory relative to root, deterministic order."""
    out = []
    for base, dirs, files in os.walk(root):
        dirs.sort()
        rel = Path(base).relative_to(root)
        if str(rel) != '.':
            out.append((rel.as_posix() + '/', None))
        for name in sorted(files):
            p = Path(base) / name
            out.append(((rel / name).as_posix(), p))
    return out


def fsync_dir(path):
    fd = os.open(path, os.O_RDONLY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def snapshots(state):
    return sorted(p for p in (state / 'snapshots').glob('*') if not p.name.startswith('.'))


def backup(state, keep=5):
    storage = state / 'storage'
    if not storage.is_dir():
        return '没有存档目录（storage 还没生成）'
    root = state / 'snapshots'
    root.mkdir(exist_ok=True)
    tmp = root / f'.tmp-{uuid.uuid4().hex[:8]}'
    tmp.mkdir()
    files = []
    for rel, src in tree(storage):
        dest = tmp / 'tree' / rel
        if src is None:
            dest.mkdir(parents=True, exist_ok=True)
            continue
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dest)
        files.append({'path': rel, 'bytes': dest.stat().st_size, 'sha256': digest(dest)})
    manifest = {
        'format': FORMAT,
        'game': state.name,
        'created': time.strftime('%Y-%m-%dT%H:%M:%S%z'),
        'swf': os.environ.get('FLASH_SWF_NAME', ''),
        'files': files,
        'bytes': sum(f['bytes'] for f in files),
    }
    (tmp / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')
    for base, dirs, names in os.walk(tmp):
        for name in names:
            fd = os.open(Path(base) / name, os.O_RDONLY)
            try:
                os.fsync(fd)
            finally:
                os.close(fd)
        fsync_dir(base)
    final = root / f"{time.strftime('%Y%m%d-%H%M%S')}-{uuid.uuid4().hex[:6]}"
    tmp.rename(final)
    fsync_dir(root)
    # keep the newest N, retire older ones atomically
    old = snapshots(state)[:-keep] if keep > 0 else []
    for snap in old:
        trash = root / f'.gc-{uuid.uuid4().hex[:8]}'
        snap.rename(trash)
        shutil.rmtree(trash, ignore_errors=True)
    return f'已备份 {len(files)} 个文件、{manifest["bytes"]} 字节 → {final.name}'


def verify(snap):
    manifest = json.loads((snap / 'manifest.json').read_text())
    if manifest.get('format') != FORMAT:
        raise SystemExit(f'快照格式不支持: {manifest.get("format")}')
    for item in manifest['files']:
        p = snap / 'tree' / item['path']
        if not p.is_file() or digest(p) != item['sha256']:
            raise SystemExit(f'快照校验失败: {item["path"]}')
    return manifest


def restore(state, name=None, allow_running=False):
    if not allow_running and (state / '.session.lock').exists() and os.environ.get('FLASH_SAVES_FORCE') != '1':
        return '播放器可能正在运行；先退出游戏，再恢复（或设 FLASH_SAVES_FORCE=1）'
    snaps = snapshots(state)
    if not snaps:
        return '还没有任何备份'
    snap = next((s for s in snaps if s.name == name), snaps[-1] if not name else None)
    if not snap:
        return f'找不到备份 {name}'
    verify(snap)
    storage = state / 'storage'
    current = backup(state, keep=0)   # keep a safety copy of what we are replacing
    trash = state / f'.restore-old-{uuid.uuid4().hex[:8]}'
    if storage.exists():
        storage.rename(trash)
    tmp = state / f'.restore-new-{uuid.uuid4().hex[:8]}'
    shutil.copytree(snap / 'tree', tmp)
    tmp.rename(storage)
    fsync_dir(state)
    shutil.rmtree(trash, ignore_errors=True)
    return f'已恢复 {snap.name}（替换前的状态另存为最近一次备份）'


def main(argv):
    if len(argv) < 2:
        raise SystemExit(__doc__)
    cmd, state = argv[0], Path(argv[1])
    if cmd == 'list':
        for s in snapshots(state):
            m = json.loads((s / 'manifest.json').read_text())
            print(s.name, m.get('created'), f"{len(m['files'])} files", m.get('bytes'), 'bytes')
        return 0
    if cmd == 'backup':
        print(backup(state)); return 0
    if cmd == 'restore':
        print(restore(state, argv[2] if len(argv) > 2 else None)); return 0
    if cmd == 'apply-pending':
        req = state / 'restore.request'
        if not req.is_file():
            return 0
        name = req.read_text().strip() or None
        os.environ['FLASH_SAVES_FORCE'] = '1'
        print(restore(state, name, allow_running=True))
        req.unlink(missing_ok=True)
        return 0
    raise SystemExit(__doc__)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

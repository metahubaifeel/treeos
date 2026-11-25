#!/usr/bin/python3
"""Add one Flash title to the handheld launcher.

    flash-add GAME.swf                 single file -> /roms/flash-play/<name>.swf
    flash-add --content GAME.swf       multi-file  -> case-sensitive shelf + .flashref
    flash-add --title 我的游戏 GAME.swf
    flash-add --list                   show what is installed

Writes only: the movie/ref, one private state dir, gamelist.xml and (for
multi-file titles) the shelf. Never edits a movie, never touches saves.
"""
import hashlib
import json
import os
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

KIT = Path(os.environ.get('TREEOS_FLASH_KIT', Path.home() / '.local/share/treeos-flash'))
DROP = Path(os.environ.get('TREEOS_FLASH_DROP', '/roms/flash-play'))
SHELF = KIT / 'flash-play' / 'content'
RUNTIME = KIT / 'flash-play' / 'runtime-ui1'
# A calm, broadly useful starting point: arrows move, A is space, R2 is a
# left click on the right stick. Every title can change this later with M.
DEFAULT_PROFILE = """pointer_source=right_stick
south=32
east=74
west=75
north=73
left_trigger=79
r1_vk=76
l2_vk=mouse_left
r2_vk=0
l3_vk=0
r3_vk=0
"""
DESC = 'A 空格，B J，X K，Y I，L1 O，R1 L；右摇杆移鼠标，L2 左键。游戏中按 M 改键、Select + Start 退出。'


def swf_check(path):
    with path.open('rb') as f:
        head = f.read(8)
    if len(head) != 8 or head[:3] not in (b'FWS', b'CWS', b'ZWS'):
        raise SystemExit(f'不是 Flash 影片: {path}')
    if head[:3] == b'CWS':
        import zlib
        declared = int.from_bytes(head[4:8], 'little')
        z = zlib.decompressobj()
        actual = 8
        with path.open('rb') as f:
            f.seek(8)
            while not z.eof and (chunk := f.read(65536)):
                while chunk and not z.eof:
                    actual += len(z.decompress(chunk, 524288))
                    if actual > 512 * 1024 ** 2:
                        raise SystemExit('CWS 超过 512 MiB 校验上限')
                    chunk = z.unconsumed_tail
        if not z.eof or actual != declared:
            raise SystemExit(f'CWS 数据不完整（头部 {declared}，实际 {actual}）: {path}')


def game_id(namespace, rel):
    return hashlib.sha256(os.fsencode(namespace + '/' + rel)).hexdigest()


def list_games():
    for ref in sorted(DROP.glob('*.flashref')):
        print('ref   ', ref.name, '->', ref.read_text().strip())
    for swf in sorted(DROP.glob('*.swf')):
        print('file  ', swf.name, f'{swf.stat().st_size / 1048576:.1f} MiB')


def add(src, title, content):
    src = src.resolve(strict=True)
    swf_check(src)
    name = title or src.stem
    if any(c in name for c in '/\\\0'):
        raise SystemExit('标题里不能有路径分隔符')
    DROP.mkdir(parents=True, exist_ok=True)
    if content:
        shelf_dir = SHELF / name
        if shelf_dir.exists():
            raise SystemExit(f'内容目录已存在: {shelf_dir}')
        staging = Path(tempfile.mkdtemp(prefix='.incoming-', dir=SHELF.parent))
        target = staging / name
        target.mkdir()
        for item in src.parent.iterdir():
            if item.name.startswith('.'):
                continue
            if item.is_dir():
                shutil.copytree(item, target / item.name, symlinks=False)
            else:
                shutil.copy2(item, target / item.name)
        if not (target / src.name).is_file():
            raise SystemExit('复制后找不到主文件')
        target.rename(shelf_dir)
        staging.rmdir()
        published, mode = shelf_dir / src.name, 'content'
        ref = DROP / (name + '.flashref')
        with ref.open('x') as f:
            f.write(f'@content/{name}/{src.name}\n')
        print('多文件游戏已放到内容目录:', ref)
    else:
        dst = DROP / (name + '.swf')
        if dst.exists():
            raise SystemExit(f'已存在同名文件，先改名或加 --title: {dst}')
        fd, tmp = tempfile.mkstemp(dir=DROP, suffix='.tmp')
        os.close(fd)
        try:
            shutil.copyfile(src, tmp)
            Path(tmp).rename(dst)
        finally:
            Path(tmp).unlink(missing_ok=True)
        published, mode = dst, 'drop'
        print('已加入:', dst)
    rel = published.name if mode == 'drop' else published.relative_to(SHELF).as_posix()
    gid = game_id('drop' if mode == 'drop' else 'content', rel)
    state = KIT / 'flash-play' / 'games' / gid
    state.mkdir(parents=True, exist_ok=True)
    profile = state / 'keys.profile'
    if not profile.exists():
        profile.write_text(DEFAULT_PROFILE)
    register(name, ref if mode == 'content' else published)
    print(f'游戏标识: {gid}')
    print('键位:  ', profile)
    print('下一步: 回到 EmulationStation，在 Flash 里选“更新游戏列表”，再进游戏。')


def register(name, published):
    """Insert one row in gamelist.xml, keeping the rest of the file intact."""
    listing = DROP / 'gamelist.xml'
    if not listing.is_file():
        print('（没有 gamelist.xml，ES 会自动生成条目）')
        return
    prior = listing.read_bytes()
    path_name = published.name
    row = (f'\t<game>\n\t\t<path>./{path_name}</path>\n\t\t<name>{name}</name>\n'
           f'\t\t<desc>{DESC}</desc>\n\t</game>\n').encode()
    if f'./{path_name}'.encode() in prior:
        print('gamelist 里已有同名条目')
        return
    updated = prior.replace(b'</gameList>', row + b'</gameList>')
    ET.fromstring(updated)
    fd, tmp = tempfile.mkstemp(dir=DROP, suffix='.tmp')
    with os.fdopen(fd, 'wb') as f:
        f.write(updated)
        f.flush()
        os.fsync(f.fileno())
    os.chmod(tmp, listing.stat().st_mode & 0o777)
    os.replace(tmp, listing)
    print('gamelist.xml 已更新')


def main(argv):
    args = [a for a in argv if not a.startswith('--')]
    flags = {a for a in argv if a.startswith('--')}
    if '--list' in flags or not args:
        list_games()
        return 0
    title = None
    if '--title' in argv:
        i = argv.index('--title')
        if i + 1 >= len(argv):
            raise SystemExit('--title 需要名字')
        title = argv[i + 1]
        args = [a for a in args if a != title]
    if len(args) != 1:
        raise SystemExit(__doc__)
    add(Path(args[0]), title, '--content' in flags)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

#!/usr/bin/python3
"""NGP shared Flash content dispatcher. ES passes one file, not a shell script."""
import hashlib
import json
from datetime import datetime
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import zlib

HERE = Path(__file__).resolve().parent
KIT = Path(os.environ.get('TREEOS_FLASH_KIT', Path.home() / '.local/share/treeos-flash'))
RUNTIME = HERE.with_name('runtime-' + HERE.name.removeprefix('release-'))  # release-X -> runtime-X
DROP = Path(os.environ.get('TREEOS_FLASH_DROP', '/roms/flash-play'))
LIB = Path(os.environ.get('TREEOS_FLASH_LIB', '/roms/flash'))
# Compatibility alias for the already-accepted profile/storage, NOT a game requirement.
OLD32 = '4399资源库/199408_死神VS火影3.2/main.swf'
UNIT = 'flash-199408-mapper'  # share the same exclusivity gate as the older Ports launchers
MARK = '/run/treeos-flash-play/owns-display'


def resolve(selected, drop=DROP, lib=LIB, kit=KIT):
    path = Path(selected).resolve(strict=True)
    if not path.is_file(): raise ValueError('Select a SWF file')
    # Case-sensitive content shelf for multifile imports on handhelds with exFAT ROMs.
    # No arbitrary absolute path in a reference; state/storage stay outside this shelf.
    shelf = kit / 'flash-play' / 'content'
    if path.suffix.lower() == '.flashref':
        if not path.is_relative_to(drop) or path.stat().st_size > 4096: raise ValueError('invalid library reference')
        ref = Path(path.read_text().rstrip('\n'))
        if ref.is_absolute() or '..' in ref.parts: raise ValueError('invalid library reference')
        target = lib
        if ref.parts[:1] == ('@content',):
            target = shelf; ref = ref.relative_to('@content')
        path = (target / ref).resolve(strict=True)
        if not path.is_relative_to(target): raise ValueError('reference outside library')
    root = next((r for r in (drop, lib, shelf) if path.is_relative_to(r)), None)
    if not root or not path.is_file() or path.suffix.lower() != '.swf': raise ValueError('not a SWF in the content roots')
    with path.open('rb') as f:
        header = f.read(8)
    if len(header) != 8 or header[:3] not in (b'FWS', b'CWS', b'ZWS'): raise ValueError('not a Flash movie header')
    rel = path.relative_to(root).as_posix()
    namespace = 'drop' if root == drop else 'content' if root == shelf else 'library'
    game = hashlib.sha256(os.fsencode(namespace + '/' + rel)).hexdigest()
    state = kit / 'flash-play' / 'games' / game
    profile, storage = state / 'input-v7.profile', state / 'storage'
    inputs = [profile, state / 'input-v6.profile', state / 'input-v5.profile', state / 'input-v4.profile', state / 'input-v3.profile', state / 'input-v2.profile', state / 'keys.profile']
    if root == lib and rel == OLD32:
        storage = kit / 'r1-trial/storage'
        inputs.extend((kit / '199408-free-v1/profiles/199408.profile', kit / 'r1-trial/profiles/199408.profile', kit / 'profiles/custom/199408.profile'))
    defaults = kit / 'flash-play' / 'defaults'
    with path.open('rb') as f:
        digest = hashlib.file_digest(f, 'sha256').hexdigest()
    shipped = defaults / (digest + '.profile')
    source = next((p for p in inputs if p.is_file()), None)
    if source is None and shipped.is_file():
        # First run: a per-title recommended layout ships with the launcher.
        source = shipped
    return dict(movie=path, root=root, game=game, state=state, profile=profile, storage=storage,
                source=source or Path('/dev/null'))


def network_for(movie, config=None):
    """Per-title network opt-in: only listed SWFs get a fetching navigator."""
    config = config or HERE / 'network-titles.json'
    if not config.is_file():
        return None
    with movie.open('rb') as f:
        h = hashlib.file_digest(f, 'sha256').hexdigest()
    entry = json.loads(config.read_text()).get(str(movie.stat().st_size), {})
    return entry.get(h) if isinstance(entry, dict) else None


def spoof_for(movie, config=None):
    """Return (spoof_url, runtime) for domain-locked titles, else (None, runtime_for(...))."""
    config = config or HERE / 'spoof-titles.json'
    if not config.is_file():
        return None, runtime_for(movie)
    with movie.open('rb') as f:
        h = hashlib.file_digest(f, 'sha256').hexdigest()
    entry = json.loads(config.read_text()).get(str(movie.stat().st_size), {})
    spoof = entry.get(h) if isinstance(entry, dict) else None
    if not spoof:
        return None, runtime_for(movie)
    return spoof, RUNTIME.with_name('runtime-spoof1')


def network_runtime(movie):
    """Titles with a network whitelist get the fetching player build."""
    return RUNTIME.with_name('runtime-net2') if network_for(movie) else None


def runtime_for(movie, config=None, baseline=RUNTIME):
    """Choose a title-specific renderer by movie bytes, never by display name."""
    for table, runtime in (('classic-display-titles.json', 'runtime-classic-display'),
                           ('sparse-array-titles.json', 'runtime-sparse-array'),
                           ('difference-titles.json', 'runtime-bitmap-difference')):
        path = HERE / table
        if path.is_file():
            pins = json.loads(path.read_text()).get(str(movie.stat().st_size), [])
            if pins:
                with movie.open('rb') as f:
                    if hashlib.file_digest(f, 'sha256').hexdigest() in pins:
                        return baseline.with_name(runtime)
    config = config or HERE / 'bitmap-titles.json'
    if config.is_file():
        candidates = json.loads(config.read_text()).get(str(movie.stat().st_size), [])
        if candidates:
            with movie.open('rb') as f:
                if hashlib.file_digest(f, 'sha256').hexdigest() in candidates:
                    return baseline.with_name('runtime-bitmap14')
    return baseline


def preflight(movie):
    """Check a CWS container without executing Flash or retaining its inflated body.
    Other signatures retain the existing player path; this is not a tag validator.
    """
    with movie.open('rb') as f:
        header = f.read(8)
        if header[:3] != b'CWS': return 'not CWS: compressed-stream check not applicable'
        declared = int.from_bytes(header[4:8], 'little')
        z = zlib.decompressobj(); actual = 8
        try:
            while not z.eof and (chunk := f.read(65536)):
                while chunk and not z.eof:
                    actual += len(z.decompress(chunk, 524288))
                    if actual > 512 * 1024**2: raise ValueError('CWS exceeds this handheld’s 512 MiB validation budget')
                    chunk = z.unconsumed_tail
        except zlib.error as exc:
            raise ValueError(f'CWS decompression failed: {exc}') from exc
        if not z.eof or actual != declared:
            raise ValueError(f'CWS rejected before display handoff: stream_end={z.eof}, header={declared}, actual={actual}')
        return f'CWS envelope OK ({actual} bytes); game compatibility not tested'


def prepare(plan, work, compiler):
    data = work / 'ruffle_data'; data.mkdir()
    movie = plan['movie']; parent = movie.parent
    # Flat ROMs don't need links to every other game. A game's own folder can
    # contain sibling SWFs/XML/audio and arbitrary relative asset directories.
    for item in parent.iterdir():
        if item == movie or item.name.startswith('.'): continue
        if parent == plan['root'] and item.name not in ('assets', '_assets'): continue
        if item.name in ('config.ron','movie.swf','storage'): raise ValueError('reserved session filename: ' + item.name)
        if not item.resolve(strict=True).is_relative_to(plan['root']): raise ValueError('asset outside content root')
        (data / item.name).symlink_to(item)
    (data / 'movie.swf').symlink_to(movie)
    env = os.environ.copy(); env.pop('FLASH_KEY_TEMPLATE', None)
    presets = HERE / 'control-presets.json'
    if presets.is_file():
        candidates = json.loads(presets.read_text()).get(str(movie.stat().st_size), {})
        if candidates:
            with movie.open('rb') as src: template = candidates.get(hashlib.file_digest(src, 'sha256').hexdigest())
            if template is not None: env['FLASH_KEY_TEMPLATE'] = template
    with (work / 'native.profile').open('xb') as out:
        subprocess.run([sys.executable, compiler, '--free-keys', plan['source'], data / 'config.ron'], env=env, stdout=out, check=True)
    plan['storage'].mkdir(parents=True, exist_ok=True); plan['profile'].parent.mkdir(parents=True, exist_ok=True)
    (data / 'storage').symlink_to(plan['storage'], target_is_directory=True)
    return {**env, 'RUFFLE_PROFILE': str(work / 'native.profile'), 'FLASH_PILOT_FACE_PROFILE': str(plan['profile']),
            'FLASH_GAME_TITLE': movie.stem}


def video_options(state, runtime):
    """Opt-in runtime capability. Missing/invalid per-title settings keep full quality and cadence."""
    if not (runtime / 'video-options-v1').is_file():
        return {}
    video = {'quality': 'high', 'frameskip': '0'}
    allowed = {'quality': ('low', 'medium', 'high'), 'frameskip': ('0', '1')}
    try:
        with (state / 'video.profile').open('rb') as f:
            raw = f.read(1025)
        if len(raw) <= 1024:
            for line in raw.decode('ascii').splitlines():
                key, sep, value = line.strip().partition('=')
                if sep and value in allowed.get(key, ()):
                    video[key] = value
    except (OSError, UnicodeError):
        pass
    return {'FLASH_RENDER_QUALITY': video['quality'], 'FLASH_PRESENT_SKIP': video['frameskip']}


def main(args):
    if len(args) == 2 and args[0] == '--check':
        print(preflight(resolve(args[1])['movie'])); return 0
    if len(args) == 2 and args[0] == '--inspect':
        print(json.dumps(resolve(args[1]), default=str, ensure_ascii=False)); return 0
    if len(args) == 2 and args[0] == '--worker':
        plan = resolve(os.fsdecode(bytes.fromhex(args[1])))
        spoof, bin_dir = spoof_for(plan['movie'])
        net_dir = network_runtime(plan['movie'])
        if net_dir is not None and (HERE / 'network-titles.json').is_file():
            bin_dir = net_dir
        state = plan['state']; state.mkdir(parents=True, exist_ok=True)
        pending = state / 'restore.request'
        if pending.is_file():
            helper = HERE / 'flash-saves.py'
            done = subprocess.run([sys.executable, str(helper), 'apply-pending', str(state)],
                                  capture_output=True, text=True)
            (state / 'restore.log').write_text((done.stdout or '') + (done.stderr or ''))
        log = state / 'session.log'
        if log.exists(): log.replace(state / 'previous.log')
        with log.open('w') as f, tempfile.TemporaryDirectory(prefix='ngp-flash-') as temp:
            print(datetime.now().isoformat(), 'prepare SWF:', plan['movie'], 'profile:', plan['source'], file=f, flush=True)
            print('runtime:', bin_dir.name, file=f, flush=True)
            try:
                env = prepare(plan, Path(temp), bin_dir / 'compile-face-profile.py')
                for key in ('FLASH_RENDER_QUALITY', 'FLASH_PRESENT_SKIP', 'FLASH_PROFILE_TIMINGS'):
                    env.pop(key, None)
                if bin_dir.name == 'runtime-bitmap-difference':
                    # Only this memory-heavy multifile title; keep the allocator
                    # settings used in the isolated combat run local to its worker.
                    env.update(MALLOC_ARENA_MAX='2', MALLOC_TRIM_THRESHOLD_='131072')
                video = video_options(state, bin_dir)
                if video:
                    env.update(video)
                    print('video: quality=%s frameskip=%s' % (video['FLASH_RENDER_QUALITY'], video['FLASH_PRESENT_SKIP']), file=f, flush=True)
                if spoof:
                    env['FLASH_SPOOF_URL'] = spoof
                    print('domain-locked title, spoofing', spoof, file=f, flush=True)
                env['FLASH_SAVES'] = str(HERE / 'flash-saves.py')
                dump = HERE / 'dump-frame.path'
                if dump.is_file():
                    env['FLASH_DUMP_FRAME'] = dump.read_text().strip()
                    at = HERE / 'dump-frame.at'
                    if at.is_file():
                        env['FLASH_DUMP_AT'] = at.read_text().strip()
                allow = network_for(plan['movie'])
                if allow:
                    env['FLASH_NETWORK_HOSTS'] = ','.join(allow.get('hosts', []))
                    print('network whitelist:', env['FLASH_NETWORK_HOSTS'], file=f, flush=True)
            except (ValueError, OSError, subprocess.CalledProcessError) as exc:
                print('prepare failed:', exc, file=f, flush=True)
                raise
            return subprocess.run(['/bin/bash', HERE / 'display-worker.sh', bin_dir], cwd=temp, env=env,
                                  stdout=f, stderr=subprocess.STDOUT).returncode
    if len(args) != 1: raise ValueError('usage: flash-play.py FILE.swf | --inspect FILE.swf')
    plan = resolve(args[0]); encoded = os.fsencode(plan['movie']).hex()
    try:
        preflight(plan['movie'])
    except (ValueError, OSError) as exc:
        # Preserve session.log as evidence of the previous real player run.
        plan['state'].mkdir(parents=True, exist_ok=True)
        (plan['state'] / 'last-rejection.log').write_text(f'{datetime.now().isoformat()} {plan["movie"]}: {exc}\n')
        raise
    # Pass an opaque argument through systemd; titles can contain $, %, quotes or spaces.
    return subprocess.run(['sudo','-n','systemd-run','--collect','--unit='+UNIT,'--service-type=exec',
        '--property=User=ark','--property=RuntimeDirectory=treeos-flash-play','--property=RuntimeDirectoryMode=0700',
        f'--property=ExecStopPost=+/bin/sh -c "test ! -f {MARK} || /usr/bin/systemctl --no-block start emulationstation.service"',
        sys.executable, __file__, '--worker', encoded]).returncode

if __name__ == '__main__':
    try: sys.exit(main(sys.argv[1:]))
    except (OSError, ValueError, subprocess.CalledProcessError) as exc: sys.exit(f'Flash launcher: {exc}')

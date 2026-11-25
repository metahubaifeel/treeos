#!/usr/bin/env python3
"""Use only original meter movies and scratch storage. Optional isolated on-device proof."""
import zlib
import os
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile

here = Path(__file__).resolve().parent
pilot = here.parent / 'mapper-pilot'; proof = pilot / 'input-proof'
m = runpy.run_path(str(here / 'flash-play.py'))
with tempfile.TemporaryDirectory() as scratch:
    root = Path(scratch); drop = root / 'drop'; lib = root / 'library'; kit = root / 'data'
    meter = subprocess.check_output([sys.executable, proof / 'probe-swf.py', '74','75','76'])
    first = drop / "甲 $100% ' ok/main.swf"; second = drop / '乙/main.swf'
    legacy = lib / m['OLD32']; raw = drop / '新下载.SWF'
    for movie in first, second, legacy, raw:
        movie.parent.mkdir(parents=True, exist_ok=True); movie.write_bytes(meter)
    cws = root/'envelope.swf'; compressed = b'CWS'+meter[3:8]+zlib.compress(meter[8:])
    cws.write_bytes(compressed); assert 'envelope OK' in m['preflight'](cws)
    for bad_cws in (compressed[:-1], compressed[:4]+(len(meter)+1).to_bytes(4,'little')+compressed[8:]):
        cws.write_bytes(bad_cws)
        try: m['preflight'](cws)
        except ValueError: pass
        else: raise AssertionError('accepted broken CWS envelope')
    ref = drop / '旧游戏.flashref'; ref.write_text(m['OLD32']+'\n')
    (first.parent/'声音 文件.mp3').write_bytes(b'test'); (first.parent/'assets').mkdir()
    def resolve(path): return m['resolve'](path,drop,lib,kit)
    a,b,c,d=map(resolve,(first,second,ref,raw))
    assert a['game']!=b['game'] and a['storage']!=b['storage'] and a['profile']!=b['profile']
    assert c==resolve(legacy) and c['storage']==kit/'r1-trial/storage' and c['profile']==c['state']/'input-v7.profile'
    assert not kit.exists()  # EV inspection is read-only
    old32=kit/'199408-free-v1/profiles/199408.profile';old32.parent.mkdir(parents=True);old32.write_text('r3_vk=74\n')
    assert resolve(legacy)['source']==old32 and resolve(legacy)['profile']!=old32
    previous=c['state']/'input-v2.profile';previous.parent.mkdir(parents=True);previous.write_text('south=74\nr2_vk=mouse_left\n')
    assert resolve(legacy)['source']==previous
    v3=c['state']/'input-v3.profile';v3.write_text('south=74+mouse_left\n');assert resolve(legacy)['source']==v3
    v4=c['state']/'input-v4.profile';v4.write_text('select_tap=74\n');assert resolve(legacy)['source']==v4
    v5=c['state']/'input-v5.profile';v5.write_text('south=74+32+mouse_left\n');assert resolve(legacy)['source']==v5
    v6=c['state']/'input-v6.profile';v6.write_text('pad_left_vk=65\nstick_left_vk=37\n');assert resolve(legacy)['source']==v6
    c['profile'].write_text(v6.read_text()+'right_stick_left_vk=90\n');assert resolve(second)['source']==Path('/dev/null') and v3.read_text()=='south=74+mouse_left\n';assert resolve(legacy)['source']==c['profile'] and previous.read_text()=='south=74\nr2_vk=mouse_left\n'
    shelf=kit/'flash-play/content'; case=shelf/first.relative_to(drop);case.parent.mkdir(parents=True);case.write_bytes(meter)
    role=case.parent/'Role.SWF';role.write_bytes(meter);(case.parent/'ROLE.SWF').hardlink_to(role)
    managed=drop/'big.flashref';managed.write_text('@content/'+case.relative_to(shelf).as_posix()+'\n'); private=resolve(managed)
    assert private==resolve(case) and private['game']!=a['game']
    escape=shelf/'escape.swf';escape.symlink_to(first);managed.write_text('@content/escape.swf\n')
    try:resolve(managed)
    except ValueError:pass
    else:raise AssertionError('accepted content shelf escape')
    def session(plan,name):
        work=root/name; work.mkdir(); env=m['prepare'](plan,work,pilot/'compile-face-profile.py');return work,env
    wp,_=session(private,'case');assert (wp/'ruffle_data/Role.SWF').samefile(wp/'ruffle_data/ROLE.SWF')
    wa,ea=session(a,'a'); wb,eb=session(b,'b'); wd,_=session(d,'d')
    assert (wa/'ruffle_data/声音 文件.mp3').resolve()==first.parent/'声音 文件.mp3'
    assert (wa/'ruffle_data/assets').resolve()==first.parent/'assets'
    assert not (wd/'ruffle_data/乙').exists(); (wa/'ruffle_data/storage/sentinel').write_text('first only')
    assert not (wb/'ruffle_data/storage/sentinel').exists() and not b['profile'].exists()
    video=m['video_options']; rt=root/'rt'; rt.mkdir(); va=a['state']/'video.profile'
    assert video(a['state'],rt)=={}
    (rt/'video-options-v1').touch(); va.write_text('quality=medium\nframeskip=1\n')
    assert video(a['state'],rt)=={'FLASH_RENDER_QUALITY':'medium','FLASH_PRESENT_SKIP':'1'}
    normal={'FLASH_RENDER_QUALITY':'high','FLASH_PRESENT_SKIP':'0'}
    assert video(b['state'],rt)==normal
    va.write_text('quality=nope\nframeskip=999\nPATH=bad\n'); assert video(a['state'],rt)==normal
    va.write_bytes(b'quality=low\n\xff'); assert video(a['state'],rt)==normal
    va.write_bytes(b'quality=low\n'+b'x'*1024); assert video(a['state'],rt)==normal
    bad=drop/'bad.flashref';bad.write_text('../outside.swf')
    try: resolve(bad)
    except ValueError: pass
    else: raise AssertionError('accepted escape')
    if len(sys.argv)>1:
        mesa,bin_dir,cursor=map(Path,sys.argv[1:]); probe=root/'probe.so'
        import shlex
        subprocess.run(['cc','-Wall','-Wextra','-Werror','-O2','-shared','-fPIC',proof/'probe-client.c','-o',probe,'-L'+str(mesa),*shlex.split(subprocess.check_output(['sdl2-config','--cflags','--libs'],text=True)),'-ldl','-l:libGLESv2.so.2'],check=True)
        home=root/'home';home.mkdir();(home/'.asoundrc').write_text('pcm.!default { type null }\n')
        def run(work,env,mode):
            subprocess.run(['timeout','-k','1','35','nice','-n','19',bin_dir/'ruffle-keys'],cwd=work,env={**env,
                'HOME':str(home),'SDL_VIDEODRIVER':'offscreen','SDL_AUDIODRIVER':'alsa','SDL_VIDEO_EGL_DRIVER':str(mesa/'libEGL.so.1'),
                'SDL_VIDEO_GL_DRIVER':str(mesa/'libGLESv2.so.2'),'LD_LIBRARY_PATH':str(mesa),'LP_NUM_THREADS':'1',
                'LD_PRELOAD':f'{bin_dir}/quick-menu.so:{cursor}:{probe}','PROBE_PAD':mode},check=True)
        run(wa,ea,'grid')   # M edits A/R2/R3 in A only
        wb2,eb2=session(resolve(second),'b-reopen');run(wb2,eb2,'untouched')
        w3,e3=session(resolve(first),'a-reopen');run(w3,e3,'shared')
    else:
        a['profile'].write_text('r2_vk=74\nr3_vk=74\n')
    assert not b['profile'].exists() and resolve(first)['source']==a['profile'] and resolve(second)['source']==Path('/dev/null')
    assert all(p.read_bytes()==meter for p in (first,second,raw,legacy))
    print('PASS: raw SWF, reference/legacy identity, same-name isolation, independent storage, sidecars, original SWFs unchanged')

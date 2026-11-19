#!/usr/bin/env python3
# TreeOS Flash 输入层：会话 env + 模板驱动
# 读取 FLASH_SESSION_ENV（由 flash-template 生成），否则回退旧 profile/.stick。
import os, sys, re, struct, time, fcntl, select, subprocess

EV_KEY, EV_ABS, EV_SYN = 1, 3, 0
BTN = {'south':305,'east':304,'west':308,'north':307,
       'dpad_up':544,'dpad_down':545,'dpad_left':546,'dpad_right':547,
       'l1':310,'r1':311,'l2':312,'r2':313,'select':314,'start':315,'fn':316}
VK2KEY = {13:28,27:1,32:57,16:42,17:29,18:56,9:15,8:14,
          37:105,38:103,39:106,40:108,
          48:11,49:2,50:3,51:4,52:5,53:6,54:7,55:8,56:9,57:10}
for i,v in enumerate(range(65,91)):
    VK2KEY[v] = [30,48,46,32,18,33,34,35,23,36,37,38,50,49,24,25,16,19,31,20,22,47,17,45,21,44][i]

UI_SET_EVBIT=0x40045564; UI_SET_KEYBIT=0x40045565; UI_SET_RELBIT=0x40045566
UI_DEV_SETUP=0x405C5503; UI_DEV_CREATE=0x5501
EVIOCGRAB=0x40044590
REL_X, REL_Y = 0, 1
KEYS={'wasd':{'up':17,'down':31,'left':30,'right':32},
      'arrows':{'up':103,'down':108,'left':105,'right':106}}

def make_uinput(name, keys=(), rels=()):
    fd=os.open('/dev/uinput',os.O_WRONLY)
    fcntl.ioctl(fd,UI_SET_EVBIT,EV_KEY); fcntl.ioctl(fd,UI_SET_EVBIT,2)
    for k in keys: fcntl.ioctl(fd,UI_SET_KEYBIT,k)
    for r in rels: fcntl.ioctl(fd,UI_SET_RELBIT,r)
    fcntl.ioctl(fd,UI_DEV_SETUP,struct.pack('HHHH80sI',3,1,1,1,name.encode(),0))
    fcntl.ioctl(fd,UI_DEV_CREATE)
    return fd

def emit(fd,t,c,v):
    now=time.time(); s=int(now); us=int((now-s)*1e6)
    os.write(fd,struct.pack('llHHi',s,us,t,c,v)); os.write(fd,struct.pack('llHHi',s,us,0,0,0))

def read_env(path):
    env={}
    try:
        for line in open(path,encoding='utf-8',errors='replace'):
            line=line.strip()
            if '=' not in line or line.startswith('#'): continue
            k,v=line.split('=',1); v=v.strip().strip('"').strip("'")
            env[k]=v
    except OSError: pass
    return env

def load_profile(path):
    m={}
    try:
        for line in open(path,encoding='utf-8',errors='replace'):
            line=line.strip()
            if '=' in line and not line.startswith('#'):
                k,v=line.split('=',1); m[k.strip()]=v.strip()
    except OSError: pass
    return m

def find_pad():
    for d in sorted(os.listdir('/dev/input')):
        if not d.startswith('event'): continue
        try: name=open(f'/sys/class/input/{d}/device/name').read().strip()
        except OSError: continue
        low=name.lower()
        if ('gamepad' in low or 'joypad' in low or 'retrogame' in low
                or 'r36s' in low): return '/dev/input/'+d
    return None

def main():
    swf=sys.argv[1]
    stem=os.path.basename(swf)[:-4]
    envp=os.environ.get('FLASH_SESSION_ENV','')
    env=read_env(envp) if envp else {}
    if env:
        prof_path=os.path.join(env.get('SESSION_DIR',''),'session.profile')
        prof=load_profile(prof_path)
        pointer=env.get('POINTER','none')
        kbdstick=env.get('KEYBOARD_STICK','none')
        click_l=env.get('CLICK_LEFT','none'); click_r=env.get('CLICK_RIGHT','none')
        try: speed=float(env.get('MOUSE_SPEED','6') or 6)
        except ValueError: speed=6.0
        speed=max(1.0,min(10.0,speed))
        mode=None
        up=prof.get('dpad_up','')
        if kbdstick=='left' or up:
            mode='wasd' if up in ('87','83','65','68') else ('arrows' if up in ('38','40','37','39') else None)
    else:
        prof_path=f'/roms/ports/rufflehandheld/profiles/custom/{stem}.profile'
        prof=load_profile(prof_path)
        pointer='left'; kbdstick='left'; click_l='south'; click_r='east'; speed=6.0
        up=prof.get('dpad_up','')
        mode='wasd' if up in ('87','83','65','68') else ('arrows' if up in ('38','40','37','39') else None)

    # 用户显式按键映射：一个手柄键可以对应多个键盘键
    bind={}
    for k,v in prof.items():
        if k not in ('south','east','west','north','dpad_up','dpad_down','dpad_left','dpad_right',
                     'left_trigger','right_trigger','left_trigger2','right_trigger2','start','select'):
            continue
        phys={'left_trigger':'l1','right_trigger':'r1','left_trigger2':'l2','right_trigger2':'r2'}.get(k,k)
        if phys not in BTN: continue
        codes=[]
        for tok in re.split(r'[,| ]+', v):
            try: vk=int(tok)
            except ValueError: continue
            if vk in VK2KEY: codes.append(VK2KEY[vk])
        if codes: bind[BTN[phys]]=codes

    clicks={}
    for name in (click_l, click_r):
        if name in BTN: clicks[BTN[name]] = 272 if name==click_l else 273

    pad=find_pad()
    if not pad: raise SystemExit('no gamepad')
    fd=os.open(pad,os.O_RDONLY|os.O_NONBLOCK)
    try: fcntl.ioctl(fd,EVIOCGRAB,1)
    except OSError: pass

    kbd=None; mouse=None
    allcodes=set()
    for cs in bind.values(): allcodes.update(cs)
    if mode or bind:
        kbd=make_uinput('treeos-padkeys', allcodes|{1,28})
    if pointer!='none' or clicks:
        mouse=make_uinput('treeos-padmouse',(272,273),(REL_X,REL_Y))
    if kbd is None:
        kbd=make_uinput('treeos-padkeys',(1,28))
    open('/tmp/padinput.cfg','w').write(f'stick={mode or "none"} pointer={pointer} speed={speed:.1f}\n')
    open('/tmp/padinput.bind','w').write(repr(bind))
    logf=open('/tmp/padinput.log','a',buffering=1)
    ax_=ay_=rx_=ry_=0; accx=accy=0.0; held=set(); stick_held=set(); sel=False; last={}
    try:
        while True:
            r,_,_=select.select([fd],[],[],0.02)
            if r:
                try: data=os.read(fd,24*64)
                except OSError: data=b''
                for i in range(0,len(data)-len(data)%24,24):
                    _,_,t,c,v=struct.unpack('llHHi',data[i:i+24])
                    if t==EV_ABS:
                        if c==0: ax_=v
                        elif c==1: ay_=v
                        elif c==3: rx_=v
                        elif c==4: ry_=v
                        if c in (0,1,3,4) and abs(v-last.get(c,0))>800:
                            last[c]=v; logf.write(f'ABS {c} {v}\n')
                    elif t==EV_KEY:
                        if c in BTN.values(): logf.write(f'KEY {c} {v}\n')
                        if c==316 and v==1:
                            open('/tmp/flash-settings.request','w').close()
                            subprocess.call(['pkill','-x','ruffle_desktop']); continue
                        if c==314: sel=(v==1)
                        if c==315 and v==1 and sel:
                            subprocess.call(['pkill','-x','ruffle_desktop']); continue
                        if mouse is not None and c in clicks:
                            btn=clicks[c]
                            emit(mouse,EV_KEY,btn,1 if v==1 else 0)
                            logf.write(f'CLICK {c}->{btn} {v}\n')
                        code=bind.get(c)
                        if code:
                            for kc in code:
                                if v==1 and kc not in held: emit(kbd,EV_KEY,kc,1); held.add(kc)
                                elif v==0 and kc in held: emit(kbd,EV_KEY,kc,0); held.discard(kc)
            # 键盘摇杆
            m=KEYS.get(mode) if mode else None
            if m:
                want=set()
                if ay_<-600: want.add(m['up'])
                if ay_>600: want.add(m['down'])
                if ax_<-600: want.add(m['left'])
                if ax_>600: want.add(m['right'])
                for k in want-stick_held: emit(kbd,EV_KEY,k,1); stick_held.add(k)
                for k in stick_held-want: emit(kbd,EV_KEY,k,0); stick_held.discard(k)
            # 指针
            if pointer=='left': mx,my=ax_,ay_
            elif pointer=='right': mx,my=rx_,ry_
            else: mx=my=0
            if abs(mx)>300 or abs(my)>300:
                accx+=mx/1800.0*speed; accy+=my/1800.0*speed
                dx=int(accx); dy=int(accy); accx-=dx; accy-=dy
                if mouse is not None:
                    if dx: emit(mouse,2,REL_X,dx)
                    if dy: emit(mouse,2,REL_Y,dy)
                    if dx or dy: logf.write(f'MOUSE {dx} {dy}\n')
    finally:
        try: fcntl.ioctl(fd,EVIOCGRAB,0)
        except OSError: pass
        for h in (kbd,mouse):
            if h is not None:
                try: os.close(h)
                except OSError: pass
        os.close(fd)

if __name__=='__main__': main()

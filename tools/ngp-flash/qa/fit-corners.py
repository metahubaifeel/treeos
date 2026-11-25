#!/usr/bin/env python3
"""No game content. make W H movie.swf | check W H frame.ppm.
Sample uniquely coloured corners *inside* the expected fit rectangle and
black bars outside it. A valid 1024x768 dump alone is not a geometry test.
"""
import struct, sys
from pathlib import Path

COLORS=(0xe02020,0x20d040,0x2040e0,0xf0c010)
u16=lambda v:struct.pack('<H',v)
u32=lambda v:struct.pack('<I',v)
def tag(n,b):
    return u16(64*n+min(63,len(b)))+(u32(len(b)) if len(b)>=63 else b'')+b
def push(x):
    data=(b'\x00'+x.encode()+b'\x00') if isinstance(x,str) else b'\x07'+u32(x)
    return b'\x96'+u16(len(data))+data
def draw(method,*args):
    return b''.join(push(a) for a in reversed(args))+push(len(args))+push('_root')+b'\x1c'+push(method)+b'\x52\x17'
def make(w,h):
    bits=(max(w,h)*20).bit_length()+1
    rect=f'{bits:05b}'+''.join(f'{v:0{bits}b}' for v in (0,w*20,0,h*20))
    rect=int(rect.ljust((len(rect)+7)//8*8,'0'),2).to_bytes((len(rect)+7)//8,'big')
    sw,sh=w//8,h//8; action=draw('clear')
    for i,c in enumerate(COLORS):
        x,y=(w-sw if i%2 else 0),(h-sh if i//2 else 0)
        action+=draw('beginFill',c,100)+draw('moveTo',x,y)
        for xx,yy in ((x+sw,y),(x+sw,y+sh),(x,y+sh),(x,y)):action+=draw('lineTo',xx,yy)
        action+=draw('endFill')
    body=rect+u16(30*256)+u16(2)+tag(9,bytes.fromhex('606060'))+(tag(12,action+b'\0')+tag(1,b''))*2+tag(0,b'')
    return b'FWS\x08'+u32(len(body)+8)+body

def check(w,h,file):
    with file.open('rb') as f:
        assert f.readline()==b'P6\n';pw,ph=map(int,f.readline().split());assert f.readline()==b'255\n';p=f.read()
    assert len(p)==pw*ph*3
    scale=min(pw/w,ph/h);rw,rh=round(w*scale),round(h*scale);x0=(pw-rw)//2;y0=ph-rh-(ph-rh)//2
    errors=[]
    def spot(x,y,c,name):
        observed=tuple(p[(y*pw+x)*3:(y*pw+x)*3+3]);expected=tuple(c.to_bytes(3,'big'))
        if any(abs(a-b)>8 for a,b in zip(observed,expected)):errors.append((name,observed,expected))
    for i,c in enumerate(COLORS):spot(x0+int((.96 if i%2 else .04)*rw),y0+int((.96 if i//2 else .04)*rh),c,('TL','TR','BL','BR')[i])
    spot(pw//2,ph//2,0x606060,'center')
    if x0>=2:spot(0,ph//2,0,'left bar');spot(pw-1,ph//2,0,'right bar')
    if y0>=2:spot(pw//2,0,0,'top bar');spot(pw//2,ph-1,0,'bottom bar')
    print(f'{w}x{h} -> {pw}x{ph}: rect {rw}x{rh}+{x0}+{y0}', 'FAIL '+str(errors) if errors else 'PASS all four corners, center and margins');return not errors
if __name__=='__main__':
    cmd,width,height,path=sys.argv[1:];w,h=int(width),int(height)
    if cmd=='make':Path(path).write_bytes(make(w,h))
    elif cmd=='check':sys.exit(0 if check(w,h,Path(path)) else 1)
    else:sys.exit(2)

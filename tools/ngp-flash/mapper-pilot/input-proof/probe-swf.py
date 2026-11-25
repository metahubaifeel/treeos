#!/usr/bin/env python3
"""Build an original AS2 input meter. The SWF, not the SDL helper, paints
red=J held, green=O held, blue=L held; idle=(32,32,32), on=192. No assets.
Usage: python3 probe-swf.py > movie.swf
"""
import struct
import sys

u16 = lambda n: struct.pack('<H', n)
u32 = lambda n: struct.pack('<I', n)

def tag(n, data=b''):
    size = len(data)
    return u16(n*64 + min(63, size)) + (u32(size) if size >= 63 else b'') + data

def push(x):
    data = b'\x00' + x.encode() + b'\0' if isinstance(x, str) else b'\x07' + u32(x)
    return b'\x96' + u16(len(data)) + data

def var(name): return push(name) + b'\x1c'
def call(obj, method, *args):
    return b''.join(reversed(args)) + push(len(args)) + var(obj) + push(method) + b'\x52'

def draw(method, *args): return call('_root', method, *args) + b'\x17'

colour = push(0x202020)
def member(obj, key): return var(obj) + push(key) + b'\x4e'
def assign(key, value): return var('_root') + push(key) + value + b'\x4f'
def handler(name, body):
    body += b'\0'; fn=b'\0'+u16(0)+u16(len(body))
    return var('_root')+push(name)+b'\x9b'+u16(len(fn))+fn+body+b'\x4f'
setup=b''
if sys.argv[1:] in (['--overlay'],['--multi']):
    setup=assign('held',push(0))+handler('onMouseDown',assign('held',push(1)))+handler('onMouseUp',assign('held',push(0)))
    channels=([call('Key','isDown',push(74)),call('Key','isDown',push(32)),var('held')] if sys.argv[1]=='--multi' else [call('Key','isDown',push(65)),var('held'),push(0)])
elif sys.argv[1:]==['--drag']:
    setup=call('_root','createEmptyMovieClip',push('piece'),push(2))+b'\x17'+assign('held',push(0))
    setup+=handler('onMouseDown',assign('held',push(1))+call('piece','startDrag',push(1))+b'\x17')
    setup+=handler('onMouseUp',assign('held',push(0))+call('piece','stopDrag')+b'\x17')
    moved=member('piece','_x')+push(95)+b'\x67'
    channels=[var('held'),moved,moved+member('_root','_xmouse')+push(70)+b'\x48\x10']
else:
    codes = [74,79,76] if not sys.argv[1:] else list(map(int,sys.argv[1:]))
    if len(codes)!=3 or any(not 0<k<256 for k in codes):sys.exit('expected three Flash VK numbers')
    channels=[call('Key','isDown',push(code)) for code in codes]
for channel, weight in zip(channels, (160*65536, 160*256, 160)):
    colour += channel + push(weight) + b'\x0c\x0a'
body = draw('clear') + draw('beginFill', colour, push(100))
for method, x, y in [('moveTo',0,0),('lineTo',160,0),('lineTo',160,120),('lineTo',0,120),('lineTo',0,0)]:
    body += draw(method, push(x), push(y))
body += draw('endFill')
actions = setup + handler('onEnterFrame',body) + b'\0'
# RECT: 13 signed bits per coordinate; stage 160x120 pixels at 20 twips/px.
bits = f'{13:05b}' + ''.join(f'{v:013b}' for v in (0,3200,0,2400))
rect = int(bits.ljust((len(bits)+7)//8*8,'0'), 2).to_bytes((len(bits)+7)//8,'big')
movie = rect + u16(30*256) + u16(1) + tag(9,b'\0\0\0') + tag(12,actions) + tag(1) + tag(0)
sys.stdout.buffer.write(b'FWS\x08' + u32(len(movie)+8) + movie)

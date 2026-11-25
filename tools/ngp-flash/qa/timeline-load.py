#!/usr/bin/env python3
"""Generate an original AS3 timeline-only SWF (no game assets or scripts).
32 one-frame leaf clips per group. Optionally half of each group's children
are removed/re-placed as its three-frame timeline loops, exercising tag queues.
Usage: timeline-load.py 128 [--churn] > private.swf
"""
import argparse
import struct
import sys


def tag(code, data=b''):
    n = len(data)
    return struct.pack('<H', code << 6 | min(63, n)) + (struct.pack('<I', n) if n >= 63 else b'') + data


def sprite(cid, frames, contents):
    return tag(39, struct.pack('<HH', cid, frames) + contents + tag(0))


def place(depth, cid):
    # PlaceObject2: HasCharacter | HasMatrix; identity matrix.
    return tag(26, struct.pack('<BHHB', 6, depth, cid, 0))


def make(groups, churn):
    show = tag(1)
    # RECT: 11-bit coords (0,640,0,640 twips); 32x32px.
    bits = '01011' + ''.join(f'{n:011b}' for n in (0, 640, 0, 640))
    bits += '0' * (-len(bits) % 8)
    head = int(bits, 2).to_bytes(len(bits)//8, 'big') + struct.pack('<HH', 60*256, 1)
    group = b''.join(place(i+1, 1) for i in range(32)) + show
    if churn:
        group += b''.join(tag(28, struct.pack('<H', i+1)) for i in range(0, 32, 2)) + show
        group += b''.join(place(i+1, 1) for i in range(0, 32, 2)) + show
    body = (head + tag(69, struct.pack('<I', 8)) + sprite(1, 1, show)
            + sprite(2, 3 if churn else 1, group)
            + b''.join(place(i+1, 2) for i in range(groups)) + show + tag(0))
    return b'FWS\x0a' + struct.pack('<I', len(body)+8) + body


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('groups', type=int)
    p.add_argument('--churn', action='store_true')
    args = p.parse_args()
    if not 0 <= args.groups <= 512:
        p.error('choose 0..512 groups')
    sys.stdout.buffer.write(make(args.groups, args.churn))

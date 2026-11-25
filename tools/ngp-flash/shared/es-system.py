#!/usr/bin/python3
"""Render a scoped ES XML change to stdout. Never overwrite the host config here."""
import sys
from pathlib import Path
import xml.etree.ElementTree as ET

BLOCK = b'''\t<!-- treeos-flash-play:v1 -->
\t<system>
\t\t<name>flash-play</name>
\t\t<fullname>Flash</fullname>
\t\t<path>/roms/flash-play/</path>
\t\t<extension>.swf .SWF .flashref</extension>
\t\t<command>/usr/bin/python3 /home/ark/.local/share/treeos-flash/flash-play/bin/flash-play.py %ROM%</command>
\t\t<platform>pc</platform>
\t\t<theme>flash-play</theme>
\t\t<hardware>game engine</hardware>
\t</system>
'''

def render(data,mode):
    root=ET.fromstring(data)
    if root.tag!='systemList' or data.count(b'</systemList>')!=1:raise ValueError('unexpected system list')
    existing=[s for s in root.findall('system') if s.findtext('name')=='flash-play' or (s.findtext('path') or '').rstrip('/')=='/roms/flash-play']
    if existing and (len(existing)!=1 or data.count(BLOCK)!=1):raise ValueError('conflicting Flash system; refusing to replace')
    if mode=='add': out=data if existing else data.replace(b'</systemList>',BLOCK+b'</systemList>')
    elif mode=='remove': out=data.replace(BLOCK,b'')
    else:raise ValueError('expected add/remove')
    ET.fromstring(out);return out

if __name__=='__main__':
    sys.stdout.buffer.write(render(Path(sys.argv[2]).read_bytes(),sys.argv[1]))

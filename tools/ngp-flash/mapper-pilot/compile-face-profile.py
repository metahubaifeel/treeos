#!/usr/bin/env python3
"""Adapt one game's mapper output to the frozen NGP player's numeric RON input.

stdout is a session profile, never the source profile. Unknown bindings fail
closed. In --free-keys mode 1–3 distinct selected actions are held together.
"""
from pathlib import Path
import sys
import os

DEFAULT = dict(south=74, east=75, west=85, north=73, left_trigger=79)
CHOICES = {*range(65, 91), *range(48, 58), *range(37, 41), 13, 27, 32}


def compile_profile(text, *, r1_trial=False, free_keys=False):
    values = DEFAULT.copy()
    choices = CHOICES
    if r1_trial or free_keys:
        values['r1_vk'] = 76
    if free_keys:
        values.update(l2_vk=0, r2_vk=0, l3_vk=0, r3_vk=0, select_tap=243, start_tap=243)  # 243 = keep native RON slot
        choices = (CHOICES | {0}) - {13, 27, *map(ord, 'WASD')}
    # Default movement is the D-pad sending arrow keys; WASD is opt-in.
    template = os.environ.get('FLASH_KEY_TEMPLATE') or ''
    arrows = free_keys and template != 'wasd-six'
    six = free_keys and template in ('six-keys', 'arrows-six')
    if free_keys:
        # The hook owns both direction sources. Sharing their keyboard targets
        # with face buttons is safe: all releases use the same owner counts.
        choices = (CHOICES | {0, 9, 16, 17}) - {13, 27}
        directions = (38, 40, 37, 39) if arrows else tuple(map(ord, 'WSAD'))
        for group in ('pad', 'stick'):
            for direction, vk in zip(('up', 'down', 'left', 'right'), directions):
                values[f'{group}_{direction}_vk'] = vk
        # Existing games never received keys from the right stick unless mapped.
        for direction in ('up', 'down', 'left', 'right'):
            values[f'right_stick_{direction}_vk'] = 0
    if six:
        values.update(south=65, east=90, west=83, north=88, left_trigger=68, r1_vk=67)
    sources = ('none','left_stick','right_stick','dpad')
    source = 'none'
    mouse = {'mouse_left': 241, 'mouse_right': 242}
    key_tokens = {str(i): i for i in choices}
    seen = set()
    for line in text.splitlines():
        if not line.strip() or line.lstrip().startswith('#'):
            continue
        key, eq, raw = line.partition('=')
        if not eq or key in seen:
            raise ValueError(f'unsupported or duplicate binding: {line!r}')
        seen.add(key)
        if free_keys and key == 'pointer_source' and raw in sources:
            source = raw
        elif free_keys and key in ('select_tap','start_tap') and raw == 'native':
            values[key] = 243
        elif free_keys and key in values and raw in mouse:
            values[key] = mouse[raw]
        elif free_keys and key in values and '+' in raw:
            tokens = raw.split('+'); actions = [(key_tokens | mouse).get(t) for t in tokens]
            if not 2 <= len(tokens) <= 3 or not all(actions) or len(set(actions)) != len(tokens):
                raise ValueError(f'unsupported combination: {line!r}')
            values[key] = sum(k << (8*i) for i, k in enumerate(actions))
        elif key in values and raw in key_tokens:
            values[key] = key_tokens[raw]
        else:
            raise ValueError(f'unsupported binding: {line!r}')
    fixed = dict(name='BvS 3.2 ABXY + L1', dpad_up=87, dpad_down=83,
                 dpad_left=65, dpad_right=68, start=13, select=27,
                 right_trigger='none', mouse_click='none')
    if free_keys:
        fixed['pointer_mode'] = sources.index(source)
    if arrows:
        fixed.update(name='Arrows on the D-pad', dpad_up=38, dpad_down=40, dpad_left=37, dpad_right=39)
    if six:
        fixed['name'] = 'Arrows + six keyboard buttons'
    return ''.join(f'{k}={v}\n' for k, v in (fixed | values).items())


if __name__ == '__main__':
    try:
        if len(sys.argv) == 4 and sys.argv[1] in ('--r1-trial', '--free-keys'):
            session = compile_profile(Path(sys.argv[2]).read_text(encoding='utf8'), r1_trial=True, free_keys=sys.argv[1]=='--free-keys')
            v = dict(line.split('=', 1) for line in session.splitlines())
            slots = dict(zip(('right-trigger','east','west','north','left-trigger','right-trigger-2',
                              'dpad-up','dpad-down','dpad-left','dpad-right','start','select'),
                             ('south','east','west','north','left_trigger','r1_vk',
                              'dpad_up','dpad_down','dpad_left','dpad_right','start','select'), strict=True))
            if sys.argv[1] == '--free-keys':  # the other ten controls send keyboard edges, not gamepad events
                slots = {k: v for k,v in slots.items() if k.startswith('dpad-') or k in ('start','select')}
            rows = ''.join(f'    "{slot}": {int(v[k])},\n' for slot,k in slots.items())
            Path(sys.argv[3]).write_text('Config(gamepad_config:{\n' + rows + '}, swf_url:None, swf_name:Some("movie.swf"))\n')
            sys.stdout.write(session)
        elif len(sys.argv) == 2:
            sys.stdout.write(compile_profile(Path(sys.argv[1]).read_text(encoding='utf8')))
        else:
            sys.exit('usage: compile-face-profile.py [--r1-trial|--free-keys] PROFILE [OUTPUT_RON]')
    except (OSError, ValueError) as exc:
        sys.exit(str(exc))

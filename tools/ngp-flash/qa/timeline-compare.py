#!/usr/bin/env python3
"""Regression + headless timeline microbench. NOT a game/panel FPS benchmark."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import tomllib

HERE = Path(__file__).resolve().parent
FIXTURES = ('construct_frame_list goto_in_constructframe goto_on_orphan place_object_replace '
            'movieclip_gotoandstop_children goto_nested_framescript movieclip_gotoandstop_framescripts1 '
            'movieclip_gotoandstop_framescripts2 movieclip_gotoandstop_framescripts_self '
            'instantiation_on_enter_frame instantiation_on_enterframe_gotoandstop movieclip_goto_during_frame_script').split()


def run(exe, movie, warm, n):
    return subprocess.run([exe, movie, str(warm), str(n)], capture_output=True, text=True, check=True, timeout=90)


def compare(a, b, upstream):
    for test in FIXTURES:
        folder = upstream/'tests/tests/swfs/avm2'/test
        opts = tomllib.loads((folder/'test.toml').read_text())
        if set(opts) != {'num_frames'}:
            raise ValueError(f'{test}: requires other test-runner behavior')
        expect = (folder/'output.txt').read_text()
        for exe in (a,b):
            if run(exe, folder/'test.swf', 0, opts['num_frames']).stdout != expect:
                raise ValueError(f'{exe}: trace != upstream expected output for {test}')
    out = {'fixture_traces_passed_per_build': len(FIXTURES), 'sha256':{}, 'workloads':{}}
    for name, exe in [('A',a), ('B',b)]:
        with exe.open('rb') as f: out['sha256'][name] = hashlib.file_digest(f,'sha256').hexdigest()
    with tempfile.TemporaryDirectory() as tmp:
        for label, groups, extra in [('steady',128,[]), ('churn',48,['--churn'])]:
            movie = Path(tmp)/'load.swf'
            movie.write_bytes(subprocess.check_output([sys.executable, HERE/'timeline-load.py', str(groups), *extra]))
            trials = []
            for which in 'ABBABAAB':
                r = run({'A':a,'B':b}[which], movie, 120, 480)
                m = re.search(r'^timeline_ms n=480 p50=([\d.]+) p95=([\d.]+) p99=([\d.]+)$', r.stderr, re.M)
                if not m: raise ValueError(f'bad measurement: {r.stderr[-300:]}')
                trials.append(dict(build=which, timeline_ms=dict(zip(('p50','p95','p99'), map(float,m.groups())))))
            out['workloads'][label] = {'groups':groups,'warmup_frames':120,'measured_per_run':480,'trials':trials}
    return out

if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for arg in ('baseline_bin','candidate_bin','upstream_root'): p.add_argument(arg,type=Path)
    args=p.parse_args()
    print(json.dumps(compare(args.baseline_bin.resolve(),args.candidate_bin.resolve(),args.upstream_root),indent=2))

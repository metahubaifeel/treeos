#!/usr/bin/env python3
"""Summarize one marked gameplay window from Linux QA clocks. Verify the scene separately.
Run per build/repeat; never pool menus or different visits to a level into a percentile.
"""
import argparse
import csv
import json
from math import ceil
from pathlib import Path


def trace(path, header):
    with path.open(newline='') as f:
        rows = csv.reader(f)
        if next(rows, None) != header:
            raise ValueError(f'{path}: wrong trace format')
        prev = -1
        for n, row in enumerate(rows, 2):
            if len(row) != len(header):
                raise ValueError(f'{path}:{n}: truncated row')
            nums = tuple(map(int, row))
            if min(nums) < 0 or nums[0] <= prev:
                raise ValueError(f'{path}:{n}: negative value or non-increasing timestamp')
            yield nums
            prev = nums[0]


def percentiles(values):
    ordered = sorted(values)
    if not ordered:
        return None
    return {f'p{q}': round(ordered[ceil(q*len(ordered)/100)-1] / 1e6, 3) for q in (50, 95, 99)}


def report(ticks, presents, start, end):
    if not 0 < start < end:
        raise ValueError('invalid monotonic window')
    groups = {'single': [], 'catch_up': [], 'no_frame': []}
    first_tick = last_tick = None
    for t, frames, wall, cpu in ticks:
        if first_tick is None:
            first_tick = t - wall
        last_tick = t
        if t-wall >= start and t <= end:
            groups['no_frame' if not frames else 'single' if frames == 1 else 'catch_up'].append((wall,cpu))
    prev = first = None
    intervals = []
    for (now,) in presents:
        if first is None:
            first = now
        if prev is not None and start <= prev and now <= end:
            intervals.append(now-prev)
        prev = now
    if (first is None or first > start or prev < end or first_tick is None or
            first_tick > start or last_tick < end):
        raise ValueError('window not bracketed by both traces (partial capture / wrong clock?)')
    if not intervals or not groups['single'] and not groups['catch_up']:
        raise ValueError('no complete presentation intervals or advancing timeline ticks')
    def tick_stats(rows):
        # Compare paired times on the same single-frame slow ticks, not P95(wall)-P95(cpu).
        wall = [w for w,_ in rows]
        slow = sorted(rows, reverse=True)[:ceil(len(rows)*0.05)]
        sw = sum(w for w,_ in slow)
        return {'ticks':len(rows), 'wall_ms':percentiles(wall), 'thread_cpu_ms':percentiles(c for _,c in rows),
                'slowest_5pct_cpu_wall_pct':round(100*sum(c for _,c in slow)/sw,1) if sw else None}
    return {'duration_s':round((end-start)/1e9,3), 'swap_intervals':len(intervals),
            'swap_interval_ms':percentiles(intervals), 'intervals_over_50ms':sum(x>50_000_000 for x in intervals),
            'complete_interval_fps':round(1e9*len(intervals)/sum(intervals),2),
            'tick_groups':{k:tick_stats(v) for k,v in groups.items()}}


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('tick_csv', type=Path); p.add_argument('present_csv', type=Path)
    p.add_argument('start_ns', type=int); p.add_argument('end_ns', type=int)
    a = p.parse_args()
    try:
        print(json.dumps(report(trace(a.tick_csv,['end_mono_ns','frames','wall_ns','thread_cpu_ns']),
                                trace(a.present_csv,['mono_ns']),a.start_ns,a.end_ns), indent=2))
    except (ValueError, OSError) as e:
        p.exit(2, f'{e}\n')

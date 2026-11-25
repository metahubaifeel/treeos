#!/usr/bin/env python3
import runpy
import tempfile
import unittest
from pathlib import Path

mod = runpy.run_path(str(Path(__file__).with_name('slow-window.py')))
report, trace = mod['report'], mod['trace']


class WindowTest(unittest.TestCase):
    def test_groups_and_window_edges(self):
        m = 10**6
        ticks = [(t*m,f,w*m,c*m) for t,f,w,c in
                 [(5,1,4,3),(12,1,4,2),(30,1,16,14),(55,2,24,20),(61,0,4,1),(95,1,32,8),(105,1,9,6)]]
        p = [(t*m,) for t in (2,12,29,83,92,107)]
        d = report(ticks,p,10*m,100*m)
        self.assertEqual(d['swap_intervals'],3)
        self.assertEqual(d['intervals_over_50ms'],1)
        self.assertEqual(d['swap_interval_ms']['p95'],54.)
        self.assertEqual(d['tick_groups']['single']['ticks'],2)
        self.assertEqual(d['tick_groups']['single']['slowest_5pct_cpu_wall_pct'],25.)
        self.assertEqual(d['tick_groups']['catch_up']['ticks'],1)
        self.assertEqual(d['tick_groups']['no_frame']['ticks'],1)
        with self.assertRaisesRegex(ValueError,'bracketed'): report(ticks,p[:-1],10*m,100*m)
        with self.assertRaisesRegex(ValueError,'bracketed'): report(ticks[:-1],p,10*m,100*m)
        with self.assertRaisesRegex(ValueError,'invalid'): report(ticks,p,10*m,9*m)

    def test_input_rejections(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'qa.csv'
            for data in ('ts\n1\n','mono_ns\n3\n2\n','mono_ns\n-1\n','mono_ns\n1,2\n'):
                p.write_text(data)
                with self.assertRaises(ValueError): list(trace(p,['mono_ns']))


if __name__=='__main__': unittest.main()

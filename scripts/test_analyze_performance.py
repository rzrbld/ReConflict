#!/usr/bin/env python3
"""Known-duration checks for performance metric and capture-boundary semantics."""
import importlib.util
import math
from pathlib import Path
import tempfile
import unittest

spec=importlib.util.spec_from_file_location('analysis',Path(__file__).with_name('analyze_performance.py'))
analysis=importlib.util.module_from_spec(spec);spec.loader.exec_module(analysis)


class Metrics(unittest.TestCase):
    def test_known_tail_and_strict_spikes(self):
        rows=[{'frame':i+1,'elapsed_seconds':i*.01,'frame_ms':10 if i<199 else 100} for i in range(200)]
        result=analysis.metrics(rows,{})
        self.assertAlmostEqual(result['average_fps'],200/2.09)
        self.assertEqual(result['slowest_one_percent_frames'],2)
        self.assertAlmostEqual(result['one_percent_low_fps'],1000/55)
        self.assertEqual([result['spikes'][f'gt_{n}_ms']['count'] for n in (33,50,100)],[1,1,0])

    def test_small_sample_ceil_and_empty(self):
        rows=[{'frame':1,'elapsed_seconds':.01,'frame_ms':10},{'frame':2,'elapsed_seconds':.03,'frame_ms':20}]
        self.assertEqual(analysis.metrics(rows,{})['one_percent_low_fps'],50)
        self.assertEqual(analysis.metrics([],{})['frames'],0)

    def test_only_truncated_final_row_can_be_skipped(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'profile.csv'
            path.write_text('frame,elapsed_seconds,frame_ms\n1,0.01,10\n2,0.02')
            rows,skipped=analysis.read_csv(path,['frame','elapsed_seconds','frame_ms'])
            self.assertEqual(len(rows),1);self.assertEqual(skipped[0]['line'],3)
            path.write_text('frame,elapsed_seconds,frame_ms\n1,0.01\n2,0.02,10\n')
            with self.assertRaises(ValueError):analysis.read_csv(path,['frame','elapsed_seconds','frame_ms'])

    def test_render_join_coverage(self):
        rows=[{'frame':1,'elapsed_seconds':.01,'frame_ms':10},{'frame':2,'elapsed_seconds':.02,'frame_ms':10}]
        render={1:dict(frame=1,draws=4,accepted=4,rejected=0,primitives=2,vertices=6,draw_cpu_submit_ms=3,swap_ms=1,uploaded_rgba_bytes=64,uploaded_levels=1)}
        result=analysis.metrics(rows,render)['render']
        self.assertEqual(result['matched_frames'],1);self.assertEqual(result['profile_frames_without_render'],1)
        self.assertEqual(result['mean_draw_cpu_submit_ms'],3)


if __name__=='__main__':unittest.main()

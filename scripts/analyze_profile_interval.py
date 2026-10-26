#!/usr/bin/env python3
"""Analyze one complete capture_device_profile.sh before/after interval.

Scene identity must be verified separately. Signal snapshot I/O is included.
"""
import argparse
import csv
import json
from pathlib import Path
import subprocess
import sys

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('directory', type=Path)
p.add_argument('--label', required=True)
a = p.parse_args()

def last_frame(path):
    with path.open() as f:
        rows = list(csv.DictReader(f))
    if not rows:
        raise SystemExit(f'Empty snapshot: {path}')
    return int(rows[-1]['frame'])

first = last_frame(a.directory / 'before/profile.csv') + 1
last = last_frame(a.directory / 'after/profile.csv')
if last < first:
    raise SystemExit('Snapshots do not contain a positive frame interval')
output = a.directory / 'fps.json'
subprocess.run([
    sys.executable, str(Path(__file__).with_name('analyze_performance.py')),
    str(a.directory / 'after/profile.csv'),
    '--render', str(a.directory / 'after/render.csv'),
    '--frames', f'{first}:{last}', '--label', a.label,
    '--output', str(output),
], check=True)
s = json.loads(output.read_text())['summary']
report = {
    'evidence': str(a.directory),
    'average_fps': s['average_fps'],
    'one_percent_low_fps': s['one_percent_low_fps'],
    'max_frame_ms': s['max_frame_ms'],
    'draws_per_frame': s['render']['mean_draw_calls'],
    'draw_submit_ms': s['render']['mean_draw_cpu_submit_ms'],
}
phase = a.directory / 'after/render-phase.csv'
if phase.exists():
    with phase.open() as f:
        selected = [r for r in csv.DictReader(f) if first <= int(r['frame']) <= last]
    keys = ['before_first_draw_ms', 'draw_span_ms', 'after_last_draw_ms']
    means = {k: sum(float(r[k]) for r in selected) / len(selected) for k in keys} if selected else {}
    phase_report = {
        'frames': len(selected), 'profile_frames': s['frames'],
        'mean_ms': means,
        'note': 'Host wall time. Draw span includes gaps between draws and submission; phases exclude frames without draws. Not GPU execution time.',
    }
    (a.directory / 'phase.json').write_text(json.dumps(phase_report, indent=2)+'\n')
    report['phase'] = phase_report
print(json.dumps(report, indent=2))

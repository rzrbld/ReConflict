#!/usr/bin/env python3
"""Summarize diagnostic GLES API wall-time CSV; this is not GPU execution time."""
import argparse
import collections
import csv
import io
import json
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('csv', type=Path)
p.add_argument('--output', type=Path)
a = p.parse_args()
lines = a.csv.read_text().splitlines()
comments = [line for line in lines if line.startswith('#')]
rows = csv.DictReader(io.StringIO('\n'.join(line for line in lines if not line.startswith('#'))))
frames = {}
costs = collections.defaultdict(lambda: [0, 0, 0])
seen = set()
for row in rows:
    frame, duration, calls, ns, maximum = (int(row[k]) for k in ('frame', 'frame_ns', 'calls', 'total_ns', 'max_ns'))
    if min(frame, duration, calls) <= 0 or min(ns, maximum) < 0 or maximum > ns:
        raise SystemExit('Invalid timing row')
    key = (frame, row['api'])
    if key in seen or (frame in frames and frames[frame] != duration):
        raise SystemExit('Duplicate or inconsistent frame record')
    seen.add(key)
    frames[frame] = duration
    c = costs[row['api']]
    c[0] += calls
    c[1] += ns
    c[2] = max(c[2], maximum)
if not frames or sorted(frames) != list(range(1, len(frames) + 1)):
    raise SystemExit('Missing frame records')
n = len(frames)
total_api = sum(c[1] for c in costs.values())
report = {
    'source': str(a.csv), 'comments': comments, 'frames': n,
    'average_frame_ms': sum(frames.values()) / n / 1e6,
    'average_fps': n * 1e9 / sum(frames.values()),
    'average_api_wall_ms': total_api / n / 1e6,
    'limitations': ['CPU wall time inside wrapped calls, not GPU execution time.',
                    'Timer/wrapper overhead is included; shared-library-internal GLES calls are excluded.',
                    'Scene identity and normal-binary comparison must be recorded separately.'],
    'apis': [dict(api=api, calls_per_frame=c[0]/n, wall_ms_per_frame=c[1]/n/1e6,
                  mean_call_us=c[1]/c[0]/1e3, max_call_ms=c[2]/1e6,
                  fraction_api_wall=c[1]/total_api if total_api else 0)
             for api, c in sorted(costs.items(), key=lambda item: item[1][1], reverse=True)]
}
out = json.dumps(report, indent=2) + '\n'
if a.output:
    a.output.write_text(out)
else:
    print(out, end='')

#!/usr/bin/env python3
"""Analyze captured Present/render CSVs; scene identity must be supplied by humans.

Examples:
  scripts/analyze_performance.py profile.csv --render render.csv --time 900:960
  scripts/analyze_performance.py profile.csv --frames 12000:12600 --label training
  scripts/analyze_performance.py profile.csv --window-seconds 60 --output result.json
Ranges for frames are inclusive; time ranges select frame-end timestamps [A,B).
Multiple ranges of a kind form a union; frame and time filters intersect.
"""
import argparse
import csv
import io
import json
import math
from pathlib import Path


def bounds(text, integer=False):
    parts=text.split(':')
    if len(parts)!=2:raise argparse.ArgumentTypeError('Use START:END')
    cast=int if integer else float
    try:a,b=(cast(x) for x in parts)
    except ValueError as exc:raise argparse.ArgumentTypeError('Invalid range') from exc
    if not math.isfinite(a) or not math.isfinite(b) or a<0 or b<a:raise argparse.ArgumentTypeError('Require 0 <= START <= END')
    return a,b


def read_csv(path, fields):
    raw=Path(path).read_bytes();lines=raw.decode().splitlines();rows=[];skipped=[]
    if not lines:raise ValueError(f'{path}: empty CSV')
    reader=csv.DictReader(io.StringIO('\n'.join(lines)))
    if not reader.fieldnames or any(f not in reader.fieldnames for f in fields):raise ValueError(f'{path}: required columns {fields}')
    previous=-1
    for line,row in enumerate(reader,2):
        try:
            if None in row or any(row.get(f) is None or row[f]=='' for f in reader.fieldnames):raise ValueError('incomplete column count')
            converted={f:float(row[f]) for f in fields}
            if any(not math.isfinite(v) for v in converted.values()):raise ValueError('nonfinite value')
            frame=converted['frame']
            if frame!=int(frame) or frame<=previous:raise ValueError('frames must increase without duplicates')
            previous=int(frame);converted['frame']=int(frame)
            if 'frame_ms' in converted and converted['frame_ms']<=0:raise ValueError('frame duration must be positive')
            rows.append(converted)
        except (ValueError,TypeError) as exc:
            incomplete=None in row or any(row.get(f) is None or row[f]=='' for f in reader.fieldnames)
            if line==len(lines) and not raw.endswith(b'\n') and incomplete:
                skipped.append({'line':line,'reason':str(exc),'text':lines[-1]});continue
            raise ValueError(f'{path}:{line}: {exc}') from exc
    return rows,skipped


def metrics(rows, renders):
    if not rows:return {'frames':0}
    durations=sorted((r['frame_ms'] for r in rows),reverse=True);n=len(rows);total=sum(durations);slow=durations[:math.ceil(n*.01)]
    out={'frames':n,'first_frame':rows[0]['frame'],'last_frame':rows[-1]['frame'],
         'first_frame_end_seconds':rows[0]['elapsed_seconds'],'last_frame_end_seconds':rows[-1]['elapsed_seconds'],
         'summed_frame_seconds':total/1000,'average_fps':n*1000/total,'one_percent_low_fps':1000/(sum(slow)/len(slow)),
         'slowest_one_percent_frames':len(slow),'max_frame_ms':durations[0],
         'spikes':{f'gt_{t}_ms':{'count':sum(d>t for d in durations),'percent':sum(d>t for d in durations)*100/n} for t in (33,50,100)}}
    joined=[renders[r['frame']] for r in rows if r['frame'] in renders]
    if renders:
        k=len(joined);out['render']={'matched_frames':k,'profile_frames_without_render':n-k}
        if k:
            out['render'].update({'mean_draw_calls':sum(r['draws'] for r in joined)/k,
                'total_accepted_draws':sum(int(r['accepted']) for r in joined),'total_rejected_draws':sum(int(r['rejected']) for r in joined),
                'mean_primitives':sum(r['primitives'] for r in joined)/k,'mean_vertices':sum(r['vertices'] for r in joined)/k,
                'mean_draw_cpu_submit_ms':sum(r['draw_cpu_submit_ms'] for r in joined)/k,'mean_swap_ms':sum(r['swap_ms'] for r in joined)/k,
                'uploaded_rgba_bytes':sum(int(r['uploaded_rgba_bytes']) for r in joined),'uploaded_levels':sum(int(r['uploaded_levels']) for r in joined)})
    return out


def main():
    p=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('profile',type=Path);p.add_argument('--render',type=Path);p.add_argument('--output',type=Path)
    p.add_argument('--frames',action='append',type=lambda s:bounds(s,True),default=[])
    p.add_argument('--exclude-frames',action='append',type=lambda s:bounds(s,True),default=[])
    p.add_argument('--time',action='append',type=bounds,default=[]);p.add_argument('--exclude-time',action='append',type=bounds,default=[])
    p.add_argument('--window-seconds',type=float);p.add_argument('--label',default='Unclassified Present frames; scene/playability not established')
    a=p.parse_args()
    if a.window_seconds is not None and (not math.isfinite(a.window_seconds) or a.window_seconds<=0):p.error('--window-seconds must be positive')
    rows,skipped=read_csv(a.profile,['frame','elapsed_seconds','frame_ms']);renders={};render_skipped=[]
    if a.render:
        rr,render_skipped=read_csv(a.render,['frame','draws','accepted','rejected','primitives','vertices','draw_cpu_submit_ms','swap_ms','uploaded_rgba_bytes','uploaded_levels']);renders={r['frame']:r for r in rr}
    original_count=len(rows)
    def inside(value,ranges,inclusive=False):return any(lo<=value<=hi if inclusive else lo<=value<hi for lo,hi in ranges)
    rows=[r for r in rows if (not a.frames or inside(r['frame'],a.frames,True)) and (not a.time or inside(r['elapsed_seconds'],a.time)) and not inside(r['frame'],a.exclude_frames,True) and not inside(r['elapsed_seconds'],a.exclude_time)]
    report={'profile':str(a.profile),'render':str(a.render) if a.render else None,'label':a.label,
        'definitions':{'average_fps':'selected frame count * 1000 / sum(selected frame_ms)',
            'one_percent_low':'1000 / arithmetic mean of slowest ceil(selected frame count * 0.01) frame durations',
            'spikes':'strictly greater than threshold; counts and percentages of selected frames',
            'time_filter':'frame end timestamp in half-open [start,end); duration belongs to entire selected frame',
            'scene_warning':'Pause/loading/black/scope-invalid frames remain included unless explicitly filtered; timing is not visual or gameplay validation'},
        'filters':{'frames_inclusive':a.frames,'time_half_open_seconds':a.time,'excluded_frames':a.exclude_frames,'excluded_time':a.exclude_time},
        'input_complete_profile_rows':original_count,'input_complete_render_rows':len(renders),'skipped_incomplete_final_rows':{'profile':skipped,'render':render_skipped},'summary':metrics(rows,renders)}
    if a.window_seconds and rows:
        buckets={}
        for row in rows:buckets.setdefault(math.floor(row['elapsed_seconds']/a.window_seconds),[]).append(row)
        report['windows']=[{'start_seconds':i*a.window_seconds,'end_seconds':(i+1)*a.window_seconds,'label':a.label,**metrics(bucket,renders)} for i,bucket in sorted(buckets.items())]
    text=json.dumps(report,indent=2)+'\n'
    if a.output:a.output.write_text(text)
    else:print(text,end='')


if __name__=='__main__':main()

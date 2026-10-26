#!/usr/bin/env python3
"""Summarize presentation CPU/wall measurements; never treat CPU clocks as GPU timers."""
import argparse,csv,json,math
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('timing',type=Path);p.add_argument('--profile',type=Path,required=True)
p.add_argument('--frames',required=True,help='Inclusive first:last, selected from independently identified gameplay')
p.add_argument('--output',type=Path)
a=p.parse_args()
try:first,last=map(int,a.frames.split(':'));assert 0<first<=last
except (ValueError,AssertionError):p.error('Expected positive inclusive first:last frame range')
columns=['guest_after_last_draw_ms','input_wall_ms','overlay_wall_ms','overlay_main_cpu_ms','overlay_worker_cpu_ms','swap_and_end_wall_ms','frame_main_cpu_ms','frame_worker_cpu_ms','present_main_cpu_ms']
def read(path,needed):
 result={};incomplete=0
 with path.open() as f:
  for row in csv.DictReader(f):
   try:
    frame=int(row['frame']);values={k:float(row[k]) for k in needed}
    if not all(math.isfinite(v) for v in values.values()):raise ValueError('nonfinite')
   except (ValueError,TypeError,KeyError):incomplete+=1;continue
   if frame in result:raise SystemExit(f'Duplicate frame {frame} in {path}')
   result[frame]=values
 return result,incomplete
records,bad=read(a.timing,columns);profile,bad_profile=read(a.profile,['frame_ms'])
frames=sorted(f for f in records if first<=f<=last and f in profile)
if not frames:raise SystemExit('No matched frames in selected interval')
def summarize(values):
 values=list(values)
 return {'valid_frames':len(values),'mean_ms':sum(values)/len(values),'min_ms':min(values),'max_ms':max(values)} if values else {'valid_frames':0}
metrics={k:summarize(records[f][k] for f in frames if records[f][k]>=0) for k in columns}
metrics['profile_frame_ms']=summarize(profile[f]['frame_ms'] for f in frames)
result={'source':str(a.timing),'profile':str(a.profile),'requested_frames':[first,last],'matched_frames':len(frames),
 'missing_frames':sorted(set(range(first,last+1))-set(frames)),
 'incomplete_rows':{'timing':bad,'profile':bad_profile},'metrics':metrics,
 'notes':['Inclusive externally selected frame interval; scene continuity needs separate evidence.',
          'Negative timer values mean unavailable and are excluded per metric; valid counts are reported.',
          'Main and worker CPU intervals overlap; their sum is not serial wall time.',
          'Overlay wall includes queued work and blocking; it is not a GPU execution timer.',
          'Diagnostic/capture/file-I/O overhead must be considered before comparing FPS.']}
text=json.dumps(result,indent=2)+'\n'
if a.output:a.output.write_text(text)
print(text,end='')

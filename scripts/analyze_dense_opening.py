#!/usr/bin/env python3
"""Select the first contiguous dense opening interval using a recorded criterion.
Scene identification comes from the earlier opening capture; this selector is
not suitable for runs that skip the intro or start directly in gameplay.
"""
import argparse,csv,json,subprocess,sys
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('directory',type=Path);p.add_argument('--label',required=True)
a=p.parse_args();root=a.directory
rows=list(csv.DictReader((root/'render.csv').open()));groups=[];current=[]
for row in rows:
 try:frame,draws=int(row['frame']),int(row['draws'])
 except (ValueError,TypeError,KeyError):continue
 if frame<1200 and draws>1000:
  if current and frame!=int(current[-1]['frame'])+1:groups.append(current);current=[]
  current.append(row)
 elif current:groups.append(current);current=[]
if current:groups.append(current)
qualifying=[g for g in groups if len(g)>30]
if not qualifying:raise SystemExit('No qualifying opening segment; inspect scene/run identity.')
g=qualifying[0];first,last=int(g[0]['frame']),int(g[-1]['frame'])
selection={'criterion':'first contiguous group with frame<1200 and draws>1000, longer than30 frames','frames_inclusive':[first,last],'frames':len(g),'warning':'No timing outliers excluded; startup uploads and shader work remain included. Scene identity requires independent evidence.'}
(root/'opening-selection.json').write_text(json.dumps(selection,indent=2)+'\n')
subprocess.run([sys.executable,str(Path(__file__).with_name('analyze_performance.py')),str(root/'profile.csv'),'--render',str(root/'render.csv'),'--frames',f'{first}:{last}','--label',a.label,'--output',str(root/'opening-dense-fps.json')],check=True)
phase=[r for r in csv.DictReader((root/'render-phase.csv').open()) if first<=int(r['frame'])<=last]
means={k:sum(float(r[k]) for r in phase)/len(phase) for k in ('before_first_draw_ms','draw_span_ms','after_last_draw_ms')}
(root/'opening-phase.json').write_text(json.dumps({'frames':len(phase),'mean_ms':means,'note':'Host wall-time phases, not GPU execution time.'},indent=2)+'\n')
s=json.loads((root/'opening-dense-fps.json').read_text())['summary']
print(json.dumps({'frames':[first,last],'average_fps':s['average_fps'],'one_percent_low_fps':s['one_percent_low_fps'],'max_frame_ms':s['max_frame_ms'],'draws_per_frame':s['render']['mean_draw_calls'],'draw_submission_ms':s['render']['mean_draw_cpu_submit_ms'],'phase_ms':means},indent=2))

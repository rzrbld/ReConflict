#!/usr/bin/env python3
"""Classify a saved draw trace against build41–46 fragment specialization keys.
State coverage is not measured GPU cost. No runtime or device changes.
"""
import argparse,collections,hashlib,json
from pathlib import Path
from analyze_draw_batches import parse
p=argparse.ArgumentParser(description=__doc__);p.add_argument('trace',type=Path);p.add_argument('--output',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[1];renderer=root/'build/shader-cache/d3d8_renderer.c'
digest=hashlib.sha256(renderer.read_bytes()).hexdigest()
if digest!='316bc5bd145595f58b23cc1118063566b477e96e041bf0dc2ff3379b11e01704':raise SystemExit('Renderer predicates changed; review the classifier before use')
paths=collections.Counter();groups=collections.Counter();last={};draws=parse(a.trace)
rs_keys=[15,25,27,19,20,28,35,29,137]
for d in draws:
 state={line.split()[0]:{int(k):int(v,16) for k,v in (pair.split(':') for pair in line.split()[1:])} for line in d['state'] if line.startswith(('rs ','tss0 ','tss1 '))}
 r,t,t1=state['rs'],state['tss0'],state['tss1'];texture=int(d['fields']['textures'].split(',')[0],0)!=0
 common=texture and not r[29] and t[1]==4 and t[2]==2 and t[3]==0 and t[5]==2 and t[6]==0 and t1[1]==1
 candidate='generic'
 if common and r[28] and r[35]==3 and (not r[15] or r[25]==7) and t[4] in (2,4):candidate='fog_'+str(t[4])
 elif common and not r[15] and not r[28] and t[4] in (2,4):candidate='glow' if t[4]==2 else 'smoke'
 paths[candidate]+=1;key=(candidate,d['fields']['fvf'],tuple(r[k] for k in rs_keys),tuple(t[k] for k in range(1,7)),t1[1]);groups[key]+=1;last[key]=d['ordinal']
report={'source':str(a.trace),'renderer_sha256':digest,'draws':len(draws),'candidate_paths':dict(paths),'render_state_keys':rs_keys,
 'groups':[{'count':n,'path':k[0],'fvf':k[1],'render_states':k[2],'tss0_ops_args_1_to_6':k[3],'stage1_colorop':k[4],'last_example_draw':last[k]} for k,n in groups.most_common()],
 'notes':['Matches recorded state to the pinned renderer predicates; assumes the corresponding runtime opt-ins are enabled.', 'Counts are not GPU execution times; draw sizes and overdraw differ.', 'Trace and screenshot may represent adjacent frames; capture I/O affects FPS.']}
text=json.dumps(report,indent=2)+'\n'
if a.output:a.output.write_text(text)
print(text,end='')

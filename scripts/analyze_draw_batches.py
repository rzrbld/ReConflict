#!/usr/bin/env python3
"""Count consecutive matching draw-state runs; no runtime/game modifications.

This is a signature opportunity count, not proof of safe deferred execution:
the diagnostic trace does not log every resource lock/update or readback.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import re
import zlib

METHOD={0x547310:'UP-arrays',0x547400:'UP-arrays',0x547450:'UP-indexed',
        0x5477f0:'VB-indexed',0x547820:'VB-indexed'}
def parse(path):
    data=path.read_bytes()
    text=(zlib.decompress(data) if path.suffix=='.zlib' else data).decode()
    draws=[];current=None;epoch=0
    for line in text.splitlines():
        if line.startswith('draw '):
            fields=dict(re.findall(r'(\w+)=([^ ]+)',line))
            current={'fields':fields,'state':[],'epoch':epoch,'ordinal':len(draws)}
            draws.append(current)
        elif line.startswith(('clear ','target ','copy','readback','present','reset')):
            epoch+=1;current=None
        elif current is not None:
            prefix=line.split(' ',1)[0]
            if prefix in ('rs','tss0','tss1','material','viewport','texture') or prefix.startswith(('matrix','light')):
                current['state'].append(line)
    return draws

def signature(draw):
    f=draw['fields']
    # Include every recorded state even if dormant: deliberately conservative.
    return (draw['epoch'],f['frame'],f['fvf'],f['rt'],f['depth'],f['textures'],tuple(draw['state']))

def runs(draws,category,convert):
    result=[];current=[];last=None
    for draw in draws:
        f=draw['fields'];primitive=int(f['primitive']);method=METHOD.get(int(f['guest'],0),'unknown')
        eligible=(category=='all-known' and method!='unknown' or method==category)
        eligible&=primitive in ((4,5,6) if convert else (4,))
        key=(signature(draw),method,primitive)
        if not eligible or key!=last:
            if len(current)>1:result.append(current)
            current=[]
        if eligible:current.append(draw);last=key
        else:last=None
    if len(current)>1:result.append(current)
    return result

def report(path,cost):
    draws=parse(path);result={'source':str(path),'draws':len(draws),
        'census':dict(Counter(f"{METHOD.get(int(d['fields']['guest'],0),'unknown')} fvf={d['fields']['fvf']} primitive={d['fields']['primitive']}" for d in draws)),
        'limitations':['Exact recorded-state comparison; dormant state is not canonicalized.',
                       'Trace omits resource update/readback barriers, buffer identities/generations and most vertex/index payloads.',
                       'Counts are batching opportunities, not proven safe merges or predicted FPS.',
                       'Call-cost extrapolation uses mean API wall time from a separate nearby frame window; merged calls still process all geometry.'],
        'opportunities':{}}
    means={a['api']:a['mean_call_us'] for a in cost['apis']} if cost else {}
    for category in ('UP-arrays','UP-indexed','VB-indexed','all-known'):
        for convert in (False,True):
            rr=runs(draws,category,convert);saved=sum(len(r)-1 for r in rr)
            o={'runs':len(rr),'draw_calls_potentially_removed':saved,'longest_run':max(map(len,rr),default=0),
               'examples':[{'first_draw':r[0]['ordinal'],'length':len(r),'guest':r[0]['fields']['guest'],
                    'fvf':r[0]['fields']['fvf'],'primitive':r[0]['fields']['primitive'],
                    'primitives':sum(int(d['fields']['count']) for d in r)} for r in sorted(rr,key=len,reverse=True)[:8]]}
            if means:
                wall=0
                for r in rr:
                    name='glDrawArrays' if METHOD[int(r[0]['fields']['guest'],0)]=='UP-arrays' else 'glDrawElements'
                    wall+=(len(r)-1)*means.get(name,0)
                o['removed_calls_mean_wall_ms_upper_estimate']=wall/1000
            result['opportunities'][category+(' triangles+strip/fan-conversion' if convert else ' triangle-lists-only')]=o
    return result

def main():
    ap=argparse.ArgumentParser();ap.add_argument('trace',type=Path);ap.add_argument('--gl-cost',type=Path)
    args=ap.parse_args();cost=json.loads(args.gl_cost.read_text()) if args.gl_cost else None
    print(json.dumps(report(args.trace,cost),indent=2))
if __name__=='__main__':main()

#!/usr/bin/env python3
"""Inspect exact-black connected components in a640x480 Bradley HUD crop.
This measures captured pixels; it does not infer which renderer call drew them.
"""
import argparse
import json
from pathlib import Path
from PIL import Image
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('image',type=Path)
a=p.parse_args()
im=Image.open(a.image).convert('RGB')
if im.size!=(640,480):p.error('Expected matched640x480 capture')
left,top,right,bottom=32,376,115,444
pixels=im.load()
unseen={(x,y) for y in range(top,bottom) for x in range(left,right) if pixels[x,y]==(0,0,0)}
components=[]
while unseen:
 seed=unseen.pop();todo=[seed];points=[seed]
 while todo:
  x,y=todo.pop()
  for n in [(x-1,y),(x+1,y),(x,y-1),(x,y+1)]:
   if n in unseen:unseen.remove(n);todo.append(n);points.append(n)
 if len(points)<16:continue
 xs,ys=zip(*points);x,y=min(xs),min(ys);w,h=max(xs)-x+1,max(ys)-y+1
 components.append(dict(x=x,y=y,width=w,height=h,pixels=len(points),solid_rectangle=w*h==len(points)))
print(json.dumps(dict(image=str(a.image),size=im.size,hud_bounds=[left,top,right,bottom],black_components=sorted(components,key=lambda x:-x['pixels'])),indent=2))

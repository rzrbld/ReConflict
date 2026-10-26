#!/usr/bin/env python3
"""Apply presentation pacing to the verified loading-screen build, locally."""
from pathlib import Path
import hashlib
import json
import shutil
ROOT=Path(__file__).resolve().parents[1]
base=ROOT/'build/loading-background'
out=ROOT/'build/frame-limit';out.mkdir(exist_ok=True)
s=(base/'d3d8.c').read_text()
assert hashlib.sha256(s.encode()).hexdigest()=='9e83211d2df3c921c28b3e9b4ce2ce637a1cad034bfcae3947f7a49916bee401'
assert s.count('SDL_GL_SetSwapInterval(presentation[12]==0x80000000u?0:1)')==2
s=s.replace('SDL_GL_SetSwapInterval(presentation[12]==0x80000000u?0:1)',
            'SDL_GL_SetSwapInterval(ds_frame_swap_interval(presentation[12]==0x80000000u?0:1))')
assert s.count('diagnostic_poll();ds_fps_overlay_presented();')==1
s=s.replace('diagnostic_poll();ds_fps_overlay_presented();',
            'diagnostic_poll();ds_frame_limit_present();ds_fps_overlay_presented();')
s='#include "frame_limiter.h"\n'+s
(out/'d3d8.c').write_text(s)
for name in ['loading_background.c','loading_background.h','present_timing.c','present_timing.h']:
    shutil.copyfile(base/name,out/name)
shutil.copyfile(ROOT/'runtime/frame_limiter.h',out/'frame_limiter.h')
(out/'manifest.json').write_text(json.dumps({p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(out.iterdir()) if p.suffix in {'.h','.c'}},indent=2)+'\n')
print(out)

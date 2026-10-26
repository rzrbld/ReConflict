#!/usr/bin/env python3
"""Freeze/verify sources actually used by isolated worker/renderer probes."""
import argparse,hashlib,json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
paths=['probe/render_thread_proxy.c','probe/render_thread_proxy.h','probe/render_thread_extra.def','probe/gl_cost_calls.def','probe/render_worker_regression.c','tests/render_worker_fixture.h','scripts/generate_render_proxy.py','build/render-thread-stage/render_proxy_wrappers.inc','build/render-thread-stage/render_proxy_ops.inc','build/render-thread-tests/gpu_proxy_selftest.c','build/render-thread-tests/parity-lights.c','build/render-thread-game/src/d3d8_renderer.c','build/render-thread-game/snapshot/runtime/d3d8_renderer.h']
parser=argparse.ArgumentParser();parser.add_argument('manifest');parser.add_argument('--verify',action='store_true');args=parser.parse_args()
current={p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in paths};destination=root/args.manifest
if args.verify:
 expected=json.loads(destination.read_text())
 if expected!=current:raise SystemExit('Probe sources changed during compilation: '+str([p for p in paths if expected.get(p)!=current[p]]))
 print('Verified probe input sources unchanged during compilation')
else:destination.write_text(json.dumps(current,indent=2)+'\n')

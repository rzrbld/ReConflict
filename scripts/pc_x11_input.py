#!/usr/bin/env python3
"""Input confined to the private Xvfb display created by run_pc_reference.sh."""
import ctypes
from pathlib import Path
import os
import sys
import time

root=Path(__file__).resolve().parents[1]/'build/pc-reference'
display_name=(root/'display.txt').read_text().strip()
authority=(root/'xauthority-path.txt').read_text().strip()
if not display_name.startswith(':') or not authority.startswith('/tmp/xvfb-run.'):
    raise SystemExit('Expected this task\'s private Xvfb display/authority')
os.environ['DISPLAY']=display_name
os.environ['XAUTHORITY']=authority
x=ctypes.CDLL('libX11.so.6');xt=ctypes.CDLL('libXtst.so.6')
x.XOpenDisplay.argtypes=[ctypes.c_char_p];x.XOpenDisplay.restype=ctypes.c_void_p
x.XStringToKeysym.argtypes=[ctypes.c_char_p];x.XStringToKeysym.restype=ctypes.c_ulong
x.XKeysymToKeycode.argtypes=[ctypes.c_void_p,ctypes.c_ulong];x.XKeysymToKeycode.restype=ctypes.c_uint
x.XSync.argtypes=[ctypes.c_void_p,ctypes.c_int]
x.XCloseDisplay.argtypes=[ctypes.c_void_p]
xt.XTestFakeKeyEvent.argtypes=[ctypes.c_void_p,ctypes.c_uint,ctypes.c_int,ctypes.c_ulong]
xt.XTestFakeButtonEvent.argtypes=[ctypes.c_void_p,ctypes.c_uint,ctypes.c_int,ctypes.c_ulong]
xt.XTestFakeMotionEvent.argtypes=[ctypes.c_void_p,ctypes.c_int,ctypes.c_int,ctypes.c_int,ctypes.c_ulong]
d=x.XOpenDisplay(display_name.encode())
if not d:raise SystemExit('Private reference display is no longer running')
mode=sys.argv[1]
if mode=='key':
    key=x.XKeysymToKeycode(d,x.XStringToKeysym(sys.argv[2].encode()))
    if not key:raise SystemExit('Unknown key')
    xt.XTestFakeKeyEvent(d,key,1,0);x.XSync(d,0)
    time.sleep(float(sys.argv[3]) if len(sys.argv)>3 else 0.15)
    xt.XTestFakeKeyEvent(d,key,0,0)
elif mode=='click':
    xt.XTestFakeMotionEvent(d,-1,int(sys.argv[2]),int(sys.argv[3]),0)
    xt.XTestFakeButtonEvent(d,1,1,0);xt.XTestFakeButtonEvent(d,1,0,0)
else:raise SystemExit('Usage: pc_x11_input.py key NAME | click X Y')
x.XSync(d,0);x.XCloseDisplay(d)

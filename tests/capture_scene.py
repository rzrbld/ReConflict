#!/usr/bin/env python3
"""Exercise capture completion/cancellation with a fake game, never hardware."""
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time

FAKE = r'''
import os,time
from pathlib import Path
root=Path(os.environ['FIXTURE_ROOT']);os.chdir(root)
(root/'ready').touch()
while True:
 p=Path('render.control')
 if p.exists():
  command=p.read_text().strip();p.unlink()
  assert command, 'Observed partial command: atomic rename failed'
  if command=='trace':Path('render-frame.txt').write_text('draw frame=42 begin\n')
  if command.startswith('capdone_'):
   with open('render-frame.txt','a') as f:f.write('trace fully closed\n')
  with open('log.txt','a') as f:
   f.write('[d3d8-diagnostic] frame=41 command='+command+' cpu=0 finish=0 trim=0\n')
   f.write('diagnostic filler\n'*8000)
 p=Path('capture.request')
 if p.exists():
  p.unlink()
  with open('capture.bmp','wb') as f:
   f.write(b'BM'+b'A'*128);f.flush();time.sleep(.09);f.write(b'B'*8192)
  with open('log.txt','a') as f:f.write('[d3d8] captured game frame=42 source=request path=capture.bmp size=640x480\n')
 time.sleep(.005)
'''

def wait_for(path):
    for _ in range(300):
        if path.exists(): return
        time.sleep(.01)
    raise AssertionError(f'Timeout: {path}')

def main():
    repo=Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix='capture scene ') as directory:
        root=Path(directory);(root/'bin').mkdir()
        shutil.copy(repo/'packaging/capture-scene.sh',root/'capture-scene.sh')
        (root/'log.txt').write_text('[d3d8] captured game frame=1 source=request old\n')
        (root/'capture.bmp').write_bytes(b'STALE')
        (root/'render-frame.txt').write_text('STALE')
        env=dict(os.environ,DS_CAPTURE_REQUEST='1',FIXTURE_ROOT=str(root))
        game=subprocess.Popen([sys.executable,'-c',FAKE],env=env)
        try:
            wait_for(root/'ready')
            pidof=root/'bin/pidof';pidof.write_text(f'#!/bin/sh\nprintf "%s\\n" {game.pid}\n');pidof.chmod(0o755)
            env['PATH']=str(root/'bin')+':'+env['PATH']
            def run(label):return subprocess.run(['bash',str(root/'capture-scene.sh'),label],env=env,capture_output=True,text=True,timeout=15)
            result=run('sun');assert result.returncode==0,result.stderr
            saved=list((root/'captures').iterdir());assert len(saved)==1
            assert (saved[0]/'capture.bmp').read_bytes()==b'BM'+b'A'*128+b'B'*8192
            assert (saved[0]/'render-frame.txt').read_text().endswith('trace fully closed\n')
            assert 'frame=1 source' not in (saved[0]/'capture.log').read_text()
            assert not (root/'tmp/capture-scene.lock').exists()
            # A concurrent capture cannot claim or remove another owner's lock.
            (root/'tmp/capture-scene.lock').mkdir()
            result=run('busy');assert result.returncode!=0
            assert (root/'tmp/capture-scene.lock').exists()
            (root/'tmp/capture-scene.lock').rmdir()
            result=run('../escape');assert result.returncode==2
            # Stop the fake game from consuming commands, then cancel the helper.
            game.send_signal(signal.SIGSTOP)
            active=subprocess.Popen(['bash',str(root/'capture-scene.sh'),'cancel'],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
            wait_for(root/'render.control')
            active.terminate();active.communicate(timeout=3)
            assert active.returncode==130
            assert not (root/'render.control').exists()
            assert not (root/'capture.request').exists()
            assert not (root/'tmp/capture-scene.lock').exists()
            # Refuse existing requests without altering them.
            (root/'render.control').write_text('cpu\n')
            result=run('pending');assert result.returncode!=0
            assert (root/'render.control').read_text()=='cpu\n'
            print('capture_scene: PASS (atomic command, fresh log, complete outputs, lock, cancellation, pending request, label)')
        finally:
            game.send_signal(signal.SIGCONT);game.terminate();game.wait(timeout=3)
if __name__=='__main__':main()

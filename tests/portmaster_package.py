#!/usr/bin/env python3
"""Validate packaging regressions and PortMaster handoff without hardware."""
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'scripts'))
from check_port_package import validate
PACKAGE = Path(sys.argv.pop(1))

class PackageChecks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with zipfile.ZipFile(PACKAGE) as z:
            cls.files = {n:z.read(n) for n in z.namelist()}

    def test_valid_package(self):
        validate(self.files)

    def test_reject_structural_regressions(self):
        for name, value in [
            ('reconflict/licenses/GCC-runtime.txt', b''),
            ('reconflict/screenshot.png', b''),
            ('screenshot.png', self.files['screenshot.png'][:16]+struct.pack('>II',640,360)+self.files['screenshot.png'][24:]),
            ('gameinfo.xml', b'<gameList><game><path>./Wrong.sh</path></game></gameList>'),
            ('reconflict/prepare_game.py', b'print("old")'),
            ('manuals/unowned.md', b'unowned'),
            ('../escape', b'unsafe'),
        ]:
            with self.subTest(name=name):
                files = dict(self.files);files[name] = value
                with self.assertRaises(ValueError): validate(files)

    def test_portmaster_launch_and_cancel(self):
        for action in [0, 1, 20, 21]:
            with self.subTest(action=action), tempfile.TemporaryDirectory() as d:
                root = Path(d);port = root/'reconflict';port.mkdir()
                control = root/'PortMaster';control.mkdir()
                events = root/'events'
                (control/'control.txt').write_text('''CFW_NAME=TestFirmware
DEVICE_ARCH=aarch64
sdl_controllerconfig=fixture-controller
GPTOKEYB=true
get_controls() { echo controls >> "$EVENTS"; }
pm_platform_helper() { echo "helper:$1" >> "$EVENTS"; }
pm_finish() { echo finish >> "$EVENTS"; }
pm_message_end() { echo cancel >> "$EVENTS"; }
''')
                shutil.copyfile(ROOT/'packaging/ReConflict.sh', root/'ReConflict.sh')
                launcher = port/'desert-storm-launcher.aarch64'
                launcher.write_text(f'#!/bin/bash\nexit {action}\n');launcher.chmod(0o755)
                game = port/'desert-storm.aarch64'
                game.write_text('''#!/bin/bash
test "$SDL_GAMECONTROLLERCONFIG" = fixture-controller || exit 90
test "${LD_LIBRARY_PATH%%:*}" = "$PWD/libs.aarch64" || exit 91
echo "game:$DS_FPS_OVERLAY:$DS_FRAME_CAP" >> "$EVENTS"
exit 7
''');game.chmod(0o755)
                (port/'config').mkdir();(port/'config/launcher.cfg').write_text('frame_cap=30\n')
                env = dict(os.environ, controlfolder=str(control), EVENTS=str(events))
                p = subprocess.run(['bash',str(root/'ReConflict.sh')],env=env,capture_output=True,text=True,timeout=10)
                self.assertEqual(p.returncode,7 if action in [20,21] else action,p.stderr)
                lines = events.read_text().splitlines()
                self.assertEqual(lines[0],'controls')
                if action in [20,21]:
                    self.assertIn(f'game:{action-20}:30',lines);self.assertEqual(lines[-1],'finish')
                else:
                    self.assertEqual(lines[-1],'cancel');self.assertFalse(any(x.startswith('game:') for x in lines))

if __name__ == '__main__':
    unittest.main()

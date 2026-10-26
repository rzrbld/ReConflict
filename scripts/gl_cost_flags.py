#!/usr/bin/env python3
"""Emit GNU ld wrapper flags for the isolated native GL diagnostic relink."""
from pathlib import Path
import re

source = Path(__file__).resolve().parent.parent / 'probe/gl_cost_calls.def'
names = re.findall(r'^[VR]\((?:[^,]+,)?(gl\w+),', source.read_text(), re.M)
if not names or len(names) != len(set(names)):
    raise SystemExit('Missing/duplicate GL wrapper names')
print(' '.join('-Wl,--wrap=' + name for name in names))

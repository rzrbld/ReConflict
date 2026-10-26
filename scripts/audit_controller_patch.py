#!/usr/bin/env python3
"""Verify patch anchors and export bindings without modifying the game."""
import hashlib
import json
from pathlib import Path
import re
import struct
import pefile

root = Path(__file__).resolve().parents[1]
game = root / "setup_conflict_-_desert_storm_1.0_(21657)"
pe = pefile.PE(str(game / "DesertStorm.exe"))
base = pe.OPTIONAL_HEADER.ImageBase
image = pe.get_memory_mapped_image()
def read(va, size):
    rva = va - base
    if rva < 0 or rva + size > len(image): raise ValueError(f"Unmapped VA {va:#x}")
    return image[rva:rva + size]
actions = {}
for i in range(80):
    pointer, action = struct.unpack("<II", read(0x5EC2B8 + i * 8, 8))
    name = read(pointer, 80).split(b"\0", 1)[0].decode("ascii")
    actions[name] = action
classic = {}
for line in (game / "default.key").read_text().splitlines():
    m = re.fullmatch(r"([^,]+),\d+=JOY_(BUTTON|AXIS)_(\d+)", line)
    if m:
        name, kind, code = m.groups()
        classic.setdefault(name, []).append(dict(kind=kind, code=int(code)))
def context(a):
    return "OnFoot" if a in (26, 43) else "Orders" if 27 <= a <= 42 else "Inventory" if 44 <= a <= 51 else "None" if a >= 54 else "OnFoot"
swap = [0,1,2,3,6,7,4,5,8,9,10,11,12,13,14,15]
modern = json.loads(json.dumps(classic))
for name, bindings in modern.items():
    if context(actions[name]) != "None":
        for binding in bindings:
            if binding["kind"] == "BUTTON": binding["code"] = swap[binding["code"] & 15]
anchors = [{"role":"joystick post-poll hook", "va":0x538F2A, "expected":"393d6c4c7500"},
           {"role":"stick deadzone immediate", "va":0x538997, "expected":"a00f0000"}]
for item in anchors:
    item["actual"] = read(item["va"], len(bytes.fromhex(item["expected"]))).hex()
    item["matches"] = item["actual"] == item["expected"]
    item["va"] = hex(item["va"])
result = dict(patch_repository="https://github.com/eSKylezZ/ConflictDesertStormPatch",
              patch_commit="314c4493f460a29e7499eae3792dd5b442b4ccaf",
              binary_sha256=hashlib.sha256((game / "DesertStorm.exe").read_bytes()).hexdigest(),
              anchors=anchors, action_ids=actions, classic=classic, modern=modern,
              status="Read-only extraction; layouts are not installed into a running game",
              game_button_to_sdl=["y","b","a","x","lefttrigger","righttrigger","leftshoulder","rightshoulder","back","leftstick","rightstick","start","dpup","dpright","dpdown","dpleft"])
out = root / "docs/evidence/controller-patch-audit.json"
out.write_text(json.dumps(result, indent=2) + "\n")
print(f"Exported {len(actions)} action IDs, {len(classic)} pad-bound actions")
print(json.dumps(anchors, indent=2))
if not all(a["matches"] for a in anchors): raise SystemExit("Patch anchors do not match this executable")

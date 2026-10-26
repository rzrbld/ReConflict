#!/usr/bin/env python3
"""Recover checked runtime-derived targets without relifting the whole game.

Negative-index jump-table targets are explicitly bounded in the manifest.
Unsupported instructions remain generation errors, never permissive skips.
The original PE is read only; output is build/game/recovered.c.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
import os

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from translate_game import MappedDisassembler
from pe_analyze import analyze_pe, build_iat_map
from lift32 import Lifter
import pefile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, default=ROOT / "config/runtime_seeds.json")
    parser.add_argument("--log", type=Path, help="Validate unresolved targets in this observed runtime log")
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text())
    binary = ROOT / manifest["binary"]
    data = binary.read_bytes()
    if hashlib.sha256(data).hexdigest() != manifest["sha256"]:
        raise SystemExit("Original executable hash mismatch; recovery addresses require reanalysis")
    pe = pefile.PE(data=data)
    base = pe.OPTIONAL_HEADER.ImageBase
    mapped = pe.get_memory_mapped_image()
    info = analyze_pe(str(binary))
    for section in info.sections:
        if section.name == ".text" or section.name.startswith("stxt"):
            section.characteristics |= 0x20000020
    dis = MappedDisassembler(data, base, info.sections, mapped)
    iat = build_iat_map(info)
    catalog = json.loads((ROOT / "build/translated/desertstorm/catalog.json").read_text())
    known = {int(f["va"], 16): f["symbol"] for f in catalog}
    seeds = {}
    for seed in manifest["seeds"]:
        address = int(seed["address"], 16)
        prefix = bytes.fromhex(seed["expected_prefix"])
        if dis.read_bytes(address, len(prefix)) != prefix:
            raise SystemExit(f"Instruction prefix mismatch at {address:#x}")
        seeds[address] = dict(seed)
    tables = []
    for table in manifest.get("bounded_tables", []):
        address = int(table["address"], 16)
        targets = []
        for index in range(table["minimum_index"], table["maximum_index"] + 1):
            entry = dis.read_bytes(address + index * 4, 4)
            if not entry or len(entry) != 4:
                raise SystemExit("Jump-table entry is outside the mapped image")
            target = struct.unpack("<I", entry)[0]
            if not dis.is_code_address(target):
                raise SystemExit(f"Jump-table target {target:#x} is outside checked code sections")
            targets.append(hex(target))
            if target not in known:
                seeds.setdefault(target, dict(address=hex(target), reason=table["name"],
                                              table=hex(address), index=index))
        if targets != table["expected_targets"]:
            raise SystemExit(f"Jump-table targets changed: {targets}")
        tables.append(dict(table, verified_targets=targets))
    if args.log:
        observed = re.findall(r"unresolved guest dispatch target (0x[\da-fA-F]+) from (0x[\da-fA-F]+)", args.log.read_text())
        if not observed:
            raise SystemExit("No strict-dispatch failure was observed in the supplied log")
        for target, caller in observed:
            if int(target, 16) not in seeds and int(target, 16) not in known:
                raise SystemExit(f"Unreviewed runtime seed {target} from {caller}; add checked evidence before lifting")
    targets = sorted(a for a in seeds if a not in known)
    recovered = {}
    for address in targets:
        f = dis.disassemble_function(address, iat)
        if not f or not f.blocks:
            raise SystemExit(f"Cannot decode checked target {address:#x}")
        f.name = f"recomp_ds_recovered_{address:08X}"
        recovered[address] = f
    names = {a: name.removeprefix("recomp_") for a, name in known.items()}
    names.update({address: f.name.removeprefix("recomp_") for address, f in recovered.items()})
    # Call destinations of the recovered aliases use the original generated
    # declarations. Recovered bodies themselves have distinct names below.
    lifter = Lifter(iat_map=iat, func_names=names, lifted=set(known) | set(recovered),
                    precise_sbb=True, precise_carry=True)
    source = '#define RECOMP_GENERATED_CODE\n#include "win32hle.h"\n#include "cpu_features.h"\n'
    for f in recovered.values():
        for target in sorted(f.calls_to):
            if target in known:
                source += f"extern void {known[target]}(void);\n"
    source += "\n".join(f"void {f.name}(void);" for f in recovered.values()) + "\n"
    reports = []
    for address, f in recovered.items():
        body = lifter.lift_function(f)
        if "UNIMPLEMENTED:" in body:
            raise SystemExit(f"Unsupported instruction in recovered body at {address:#x}")
        source += "\n" + body + "\n"
        reports.append(dict(address=hex(address), symbol=f.name, instructions=f.num_instructions,
                            blocks=len(f.blocks), evidence=seeds[address]))
    source += "\nstatic const recomp_dispatch_entry_t recovered_dispatch[]={\n"
    source += "\n".join(f"{{0x{a:08X}u,{f.name}}}," for a, f in recovered.items())
    source += "\n};\nrecomp_func_t ds_recovered_lookup(uint32_t va) {\n"
    source += "    uint32_t lo=0,hi=sizeof recovered_dispatch/sizeof *recovered_dispatch;\n"
    source += "    while(lo<hi){uint32_t mid=lo+(hi-lo)/2;if(recovered_dispatch[mid].address==va)return recovered_dispatch[mid].func;if(recovered_dispatch[mid].address<va)lo=mid+1;else hi=mid;}\n"
    source += "    return NULL;\n}\n"
    out = ROOT / "build/game/recovered.c"
    out.parent.mkdir(parents=True, exist_ok=True)
    temporary = out.with_name(f".recovered-{os.getpid()}.tmp")
    temporary.write_text(source)
    temporary.replace(out)
    report = dict(binary_sha256=manifest["sha256"], recovered=reports, verified_tables=tables,
                  output=str(out.relative_to(ROOT)), instructions=sum(f.num_instructions for f in recovered.values()),
                  limitations=["Recovered original instruction bodies, not permissive dispatch fallbacks.",
                               "Aliases rely on the original routine's existing guest register/stack frame.",
                               "No native CRT substitution or instruction elision is performed."])
    (ROOT / "docs/evidence/recovered-targets.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Recovered {len(recovered)} checked aliases ({report['instructions']} instructions) -> {out}")


if __name__ == "__main__":
    main()

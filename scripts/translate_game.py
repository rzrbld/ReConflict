#!/usr/bin/env python3
"""Experimental whole-module x86 -> C lift with a strict failure ledger.

Generated C is not a complete port. Unsupported instructions become fatal
calls, never silent NOPs. DLLs have separate symbol/dispatch namespaces.
"""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import re
import sys
import pefile

ROOT = Path(__file__).resolve().parents[1]
for part in ("pe", "disasm", "lift"):
    sys.path.insert(0, str(ROOT / "third_party/pcrecomp/tools" / part))
from pe_analyze import analyze_pe, build_iat_map
from disasm32 import Disassembler
from lift32 import Lifter


class MappedDisassembler(Disassembler):
    def __init__(self, data, base, sections, mapped):
        super().__init__(data, base, sections)
        self.mapped = mapped

    def read_bytes(self, va, size):
        # The file has shortened raw sections. Never cross into the next raw
        # section when a virtual-address read spans a hole or a BSS tail.
        offset = va - self.image_base
        if offset < 0 or offset >= len(self.mapped): return None
        return self.mapped[offset:offset + size]


def translate(path, target_base=None):
    print(f"MODULE {path.name}", flush=True)
    data = path.read_bytes()
    pe = pefile.PE(data=data)
    original_data = data
    analysis_path = path
    if target_base is not None and target_base != pe.OPTIONAL_HEADER.ImageBase:
        pe.relocate_image(target_base)
        data = pe.write()
        analysis_path = ROOT / "build/rebased" / path.name
        analysis_path.parent.mkdir(parents=True, exist_ok=True)
        analysis_path.write_bytes(data)
        pe = pefile.PE(data=data)
    info = analyze_pe(str(analysis_path))
    if pe.FILE_HEADER.Machine != 0x14c: raise ValueError("Expected x86 PE32")
    overrides = []
    for section in info.sections:
        if section.name == ".text" or section.name.startswith("stxt"):
            if not section.is_code: overrides.append(section.name)
            section.characteristics |= 0x20000020
    code = [s for s in info.sections if s.is_code]
    low = info.image_base + min(s.virtual_address for s in code)
    high = info.image_base + max(s.virtual_address + s.raw_size for s in code)
    dis = MappedDisassembler(data, info.image_base, info.sections, pe.get_memory_mapped_image())
    iat = build_iat_map(info)
    seeds = [info.image_base + info.entry_point_rva]
    seeds += [info.image_base + e.rva for e in info.exports if e.rva]
    funcs = dis.find_functions(low, high, iat, seeds=seeds)
    if not funcs or seeds[0] not in funcs: raise ValueError("Entry point not recovered")
    module = re.sub(r"[^a-z0-9]", "_", path.stem.lower())
    names = {addr: f"{module}_{addr:08X}" for addr in funcs}
    for addr, f in funcs.items(): f.name = "recomp_" + names[addr]
    lifter = Lifter(iat_map=iat, func_names=names, lifted=set(funcs), precise_sbb=True, precise_carry=True)
    out = ROOT / "build/translated" / module
    out.mkdir(parents=True, exist_ok=True)
    for pattern in ("functions_*.c", "*.aarch64.o", "*.compile.log", "*.aarch64.a"):
        for old in out.glob(pattern): old.unlink()
    header = "#pragma once\n#include \"recomp_types.h\"\n#include \"cpu_features.h\"\n"
    header += "void ds_unsupported_instruction(uint32_t address, const char *instruction);\n"
    header += "\n".join(f"void {f.name}(void);" for _, f in sorted(funcs.items())) + "\n"
    (out / "functions.h").write_text(header)
    unsupported, exceptions, catalog = [], [], []
    chunk, chunk_no = [], 0
    def flush():
        nonlocal chunk_no, chunk
        if not chunk: return
        (out / f"functions_{chunk_no:04d}.c").write_text(
            "/* Experimental translated proprietary game code. Keep local. */\n"
            "#define RECOMP_GENERATED_CODE\n#include \"functions.h\"\n" + "\n".join(chunk))
        chunk_no += 1; chunk = []
    for addr, f in sorted(funcs.items()):
        catalog.append(dict(va=hex(addr), rva=hex(addr - info.image_base), symbol=f.name,
                            instructions=f.num_instructions, kind=f.entry_kind))
        try:
            source = lifter.lift_function(f)
            def trap(match):
                instruction, va = match.group(1), int(match.group(2), 16)
                unsupported.append(dict(function=hex(addr), address=hex(va), instruction=instruction))
                return f"ds_unsupported_instruction(0x{va:08X}u, {json.dumps(instruction)});"
            source = re.sub(r"; /\* UNIMPLEMENTED: (.*?) \*/ /\* 0x([0-9A-Fa-f]+):.*?\*/", trap, source)
            if "UNIMPLEMENTED:" in source: raise ValueError("Unrecognized unsupported-instruction marker")
        except Exception as exc:
            exceptions.append(dict(function=hex(addr), error=str(exc)))
            source = f"void {f.name}(void) {{ ds_unsupported_instruction(0x{addr:08X}u, \"lift exception\"); }}\n"
        chunk.append(source)
        if len(chunk) >= 100: flush()
    flush()
    dispatch = '#include "functions.h"\n'
    dispatch += f"const recomp_dispatch_entry_t ds_{module}_dispatch[] = {{\n"
    dispatch += "\n".join(f"{{0x{addr:08X}u, {f.name}}}," for addr, f in sorted(funcs.items()))
    dispatch += f"\n}};\nconst uint32_t ds_{module}_count = {len(funcs)};\n"
    (out / "dispatch.c").write_text(dispatch)
    report = dict(module=path.name, sha256=hashlib.sha256(original_data).hexdigest(), image_base=hex(info.image_base),
                  original_image_base=hex(pefile.PE(data=original_data).OPTIONAL_HEADER.ImageBase),
                  image_size=info.sections[-1].virtual_address + info.sections[-1].effective_size,
                  entry=hex(seeds[0]), metadata_overrides=overrides, functions=len(funcs),
                  instructions=sum(f.num_instructions for f in funcs.values()),
                  unsupported=unsupported, lift_exceptions=exceptions,
                  unsupported_mnemonics=dict(collections.Counter(e["instruction"].split()[0] for e in unsupported)),
                  exports=[dict(name=e.name, va=hex(info.image_base + e.rva)) for e in info.exports],
                  limitations=["Function discovery is heuristic; indirect target closure is unverified.",
                               "DLL dispatch must select a loaded module, not just a virtual address.",
                               "No semantic differential testing or game execution established."])
    (out / "catalog.json").write_text(json.dumps(catalog, indent=2) + "\n")
    (ROOT / "docs/evidence" / f"translation-{module}.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"LIFTED {len(funcs)} candidates; unsupported={len(unsupported)} exceptions={len(exceptions)}", flush=True)
    return report


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--all-modules", action="store_true")
    p.add_argument("--dlls-only", action="store_true")
    args = p.parse_args()
    (ROOT / 'docs/evidence').mkdir(parents=True, exist_ok=True)
    game = ROOT / "setup_conflict_-_desert_storm_1.0_(21657)"
    dlls = sorted(game.glob("*.dll"))
    paths = ([] if args.dlls_only else [game / "DesertStorm.exe"]) + (dlls if args.all_modules or args.dlls_only else [])
    bases = {path: 0x10000000 + index * 0x01000000 for index, path in enumerate(dlls)}
    results = [translate(path, bases.get(path)) for path in paths]
    if args.dlls_only:
        results.insert(0, json.loads((ROOT / "docs/evidence/translation-desertstorm.json").read_text()))
    (ROOT / "docs/evidence/translation-summary.json").write_text(json.dumps(results, indent=2) + "\n")


if __name__ == "__main__": main()

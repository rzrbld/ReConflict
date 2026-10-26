#!/usr/bin/env python3
"""Lift three observed functions. This does not emit a runnable game."""
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
for part in ("pe", "disasm", "lift"):
    sys.path.insert(0, str(ROOT / "third_party/pcrecomp/tools" / part))
from pe_analyze import analyze_pe, build_iat_map
from disasm32 import Disassembler
from lift32 import Lifter


def main():
    binary = ROOT / "setup_conflict_-_desert_storm_1.0_(21657)/DesertStorm.exe"
    digest = hashlib.sha256(binary.read_bytes()).hexdigest()
    expected = "d712f5ec82c14da66b098332b3cc9c99689091f46317112ea35dfd393dc3e164"
    if digest != expected:
        raise SystemExit("Unsupported executable hash; function addresses need a fresh analysis")
    info = analyze_pe(str(binary))
    overrides = []
    for s in info.sections:
        if s.name == ".text" or s.name.startswith("stxt"):
            overrides.append(dict(section=s.name, original=hex(s.characteristics)))
            s.characteristics |= 0x20000020  # analysis metadata ONLY
    code = [s for s in info.sections if s.is_code]
    info.code_start = info.image_base + min(s.virtual_address for s in code)
    info.code_end = info.image_base + max(s.virtual_address + s.effective_size for s in code)
    iat = build_iat_map(info)
    dis = Disassembler(binary.read_bytes(), info.image_base, info.sections)
    lifter = Lifter(iat_map=iat)
    out = ROOT / "build/lift-probe"
    out.mkdir(parents=True, exist_ok=True)
    # Addresses observed in REA, not inferred by scanning data for prologues.
    targets = {0x567cf9: "PE_entry", 0x4115b0: "WinMain_candidate", 0x449260: "LevelSetup_loader"}
    results = []
    for addr, role in targets.items():
        function = dis.disassemble_function(addr, iat)
        if not function:
            raise SystemExit(f"No function decoded at {addr:#x}")
        source = lifter.lift_function(function)
        unimplemented = [line for line in source.splitlines() if "UNIMPLEMENTED:" in line]
        if unimplemented:
            raise SystemExit(f"Unimplemented lift at {addr:#x}: {unimplemented}")
        (out / f"{role}.c").write_text(
            "/* Experimental function lift only; not a game executable. */\n"
            "#define RECOMP_GENERATED_CODE\n#include \"recomp_types.h\"\n" +
            "\n".join(f"void sub_{a:08X}(void);" for a in sorted(function.calls_to)) +
            "\n" + source + "\n")
        results.append(dict(address=hex(addr), role=role, instructions=function.num_instructions,
                            direct_calls=[hex(a) for a in sorted(function.calls_to)],
                            blocks=len(function.blocks), unimplemented=unimplemented))
    report = dict(binary_sha256=digest, metadata_overrides=overrides, functions=results,
                  status="Function lifting only; calls, imports and guest runtime remain unresolved",
                  limitations=["No complete call graph or indirect target closure.",
                               "No semantic equivalence claim; differential testing is required.",
                               "No translated mission DLLs or D3D8/DirectInput8 backend."])
    (ROOT / "docs/evidence/lift-probe.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()

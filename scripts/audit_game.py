#!/usr/bin/env python3
"""Read-only PE inventory. Disassembly is a bounded heuristic, not CFG recovery."""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import capstone
import pefile


def audit(path):
    data = path.read_bytes()
    pe = pefile.PE(data=data)
    imports = {
        d.dll.decode(): [i.name.decode() if i.name else f"ordinal:{i.ordinal}"
                         for i in d.imports]
        for d in getattr(pe, "DIRECTORY_ENTRY_IMPORT", [])
    }
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.skipdata = True
    counts = collections.Counter()
    examples = collections.defaultdict(list)
    sections = []
    # GOG's modified main PE does not mark .text executable. Include it explicitly.
    for s in pe.sections:
        name = s.Name.rstrip(b"\0").decode(errors="replace")
        sections.append(dict(name=name, rva=hex(s.VirtualAddress),
                             virtual_size=s.Misc_VirtualSize, raw_size=s.SizeOfRawData,
                             characteristics=hex(s.Characteristics)))
        if not (s.Characteristics & 0x20000000 or name == ".text" or name.startswith("stxt")):
            continue
        for ins in md.disasm(s.get_data(), pe.OPTIONAL_HEADER.ImageBase + s.VirtualAddress):
            kind = None
            if ins.mnemonic.startswith("f") and ins.mnemonic != "fwait":
                kind = "possible_x87"
            elif "fs:" in ins.op_str:
                kind = "fs_segment"
            elif ins.mnemonic in ("call", "jmp") and not ins.op_str.startswith("0x"):
                kind = "indirect_control_transfer"
            elif ins.mnemonic in ("syscall", "sysenter", "int"):
                kind = "system_instruction_candidate"
            if kind:
                counts[kind] += 1
                if len(examples[kind]) < 8:
                    examples[kind].append(f"{ins.address:#x}: {ins.mnemonic} {ins.op_str}")
    exports = getattr(pe, "DIRECTORY_ENTRY_EXPORT", None)
    return dict(path=path.name, sha256=hashlib.sha256(data).hexdigest(), bytes=len(data),
                machine=hex(pe.FILE_HEADER.Machine), pe_magic=hex(pe.OPTIONAL_HEADER.Magic),
                image_base=hex(pe.OPTIONAL_HEADER.ImageBase),
                entry_rva=hex(pe.OPTIONAL_HEADER.AddressOfEntryPoint),
                entry_va=hex(pe.OPTIONAL_HEADER.ImageBase + pe.OPTIONAL_HEADER.AddressOfEntryPoint),
                image_size=pe.OPTIONAL_HEADER.SizeOfImage,
                relocations_stripped=bool(pe.FILE_HEADER.Characteristics & 1),
                relocation_blocks=len(getattr(pe, "DIRECTORY_ENTRY_BASERELOC", [])),
                tls_directory=bool(pe.OPTIONAL_HEADER.DATA_DIRECTORY[9].VirtualAddress),
                imports=imports, exports=[dict(name=e.name.decode() if e.name else None,
                                              ordinal=e.ordinal, rva=hex(e.address))
                                         for e in exports.symbols] if exports else [],
                sections=sections, heuristic_counts=dict(counts), heuristic_examples=dict(examples),
                parser_warnings=pe.get_warnings())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("game_dir", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    paths = [args.game_dir / "DesertStorm.exe"] + sorted(args.game_dir.glob("*.dll"))
    result = dict(scope="Main executable and root-level game DLLs only",
                  limitations=["Linear disassembly can decode data as instructions and miss overlapping code.",
                               "Counts do not prove reachable code, JIT, or self-modification.",
                               "Absence of a PE TLS directory does not rule out Win32 TLS API use."],
                  versions=dict(pefile=pefile.__version__, capstone=capstone.__version__),
                  modules=[audit(p) for p in paths])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(f"Audited {len(paths)} modules; wrote {args.output}")


if __name__ == "__main__":
    main()

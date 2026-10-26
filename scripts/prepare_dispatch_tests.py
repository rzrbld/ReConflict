#!/usr/bin/env python3
from pathlib import Path
from patch_dispatch_cache import ROOT,HOOK,patched_source
stage=ROOT/'build/lookup-stage';stage.mkdir(parents=True,exist_ok=True)
source=(ROOT/'third_party/pcrecomp/runtime/win32hle/host_lite.c').read_text();patched=patched_source(source)
(stage/'promoted-host_lite.c').write_text(patched)
(stage/'baseline-host_lite.c').write_text(patched.replace(HOOK,'',1))

import re,json
source=(ROOT/'build/game/main_dispatch.c').read_text();addresses=[int(v,16) for v in re.findall(r'^\{0x([0-9a-f]+)u,',source,re.M)]
assert addresses and sorted(set(addresses))==addresses
(stage/'benchmark-table.inc').write_text('const recomp_dispatch_entry_t recomp_dispatch_table[]={\n'+''.join(f'{{0x{v:x}u,guest}},\n' for v in addresses)+'};\nconst uint32_t recomp_dispatch_count=sizeof recomp_dispatch_table/sizeof *recomp_dispatch_table;\n')
(stage/'benchmark-table.json').write_text(json.dumps({'entries':len(addresses),'first':hex(addresses[0]),'last':hex(addresses[-1])},indent=2)+'\n')

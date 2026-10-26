#!/usr/bin/env python3
"""Generate a single dispatch index for the statically rebased game modules."""
from pathlib import Path
import json
import re
root=Path(__file__).resolve().parents[1]
reports=json.loads((root/'docs/evidence/translation-summary.json').read_text())
out=root/'build/game';out.mkdir(parents=True,exist_ok=True)
header='#include "win32hle.h"\n#include <strings.h>\n#include <string.h>\n'
entries=[]
for report in reports:
    module=re.sub(r'[^a-z0-9]','_',Path(report['module']).stem.lower())
    header+=f'extern const recomp_dispatch_entry_t ds_{module}_dispatch[];\n'
    header+=f'extern const uint32_t ds_{module}_count;\n'
    catalog=json.loads((root/'build/translated'/module/'catalog.json').read_text())
    if report['module'].lower()=='desertstorm.exe':
        source='#include "win32hle.h"\n'
        source+='\n'.join(f'extern void {f["symbol"]}(void);' for f in catalog)
        source+='\nconst recomp_dispatch_entry_t recomp_dispatch_table[]={\n'
        source+='\n'.join(f'{{{f["va"]}u,{f["symbol"]}}},' for f in catalog)
        source+=f'\n}};\nconst uint32_t recomp_dispatch_count={len(catalog)};\n'
        (out/'main_dispatch.c').write_text(source)
    else:
        entries.append(f'{{"{report["module"]}",{report["image_base"]}u,{report["entry"]}u,ds_{module}_dispatch,&ds_{module}_count,0}}')
source=header+'''
typedef struct { const char *name; uint32_t base,entry; const recomp_dispatch_entry_t *table; const uint32_t *count; int active; } Module;
static Module modules[]={
'''+',\n'.join(entries)+'''
};
static const char *basename_(const char *name) {
    const char *p=name; for(const char *s=name;*s;s++)if(*s=='/'||*s=='\\\\')p=s+1; return p;
}
uint32_t ds_module_preferred(const char *name) {
    if(!name)return 0;name=basename_(name);
    for(unsigned i=0;i<sizeof modules/sizeof *modules;i++)if(!strcasecmp(name,modules[i].name))return modules[i].base;
    return 0;
}
void ds_module_loaded(const char *name,uint32_t base) {
    for(unsigned i=0;i<sizeof modules/sizeof *modules;i++)if(modules[i].base==base){modules[i].active=1;return;}
    hle_fatal("unregistered translated DLL %s @ %#x",name,base);
}
void ds_module_unloaded(uint32_t base) {
    for(unsigned i=0;i<sizeof modules/sizeof *modules;i++)if(modules[i].base==base)modules[i].active=0;
}
recomp_func_t ds_module_lookup(uint32_t va) {
    if(va<0x10000000u || va>=0x2a000000u)return NULL;
    for(unsigned i=0;i<sizeof modules/sizeof *modules;i++) {
        Module *m=&modules[i]; if(!m->active || va<m->base || va>=m->base+0x1000000)continue;
        uint32_t lo=0,hi=*m->count;
        while(lo<hi){uint32_t mid=lo+(hi-lo)/2;if(m->table[mid].address==va)return m->table[mid].func;if(m->table[mid].address<va)lo=mid+1;else hi=mid;}
    }
    return NULL;
}
'''
(out/'modules.c').write_text(source)
print(f'Generated main dispatch and {len(entries)} DLL dispatch tables')

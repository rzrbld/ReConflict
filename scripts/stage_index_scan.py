#!/usr/bin/env python3
"""Stage a stateless, bounded SIMD index scan; keep renderer source intact."""
from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[1]
out=root/'build/index-scan';out.mkdir(parents=True,exist_ok=True)
source=(root/'build/shader-cache/d3d8_renderer.c').read_text()
digest=hashlib.sha256(source.encode()).hexdigest()
assert digest=='316bc5bd145595f58b23cc1118063566b477e96e041bf0dc2ff3379b11e01704'
begin=source.index(' for(unsigned i=0;i<n;i++){unsigned ix=i;if(indices)')
end=source.index(' const void *uploaded_indices=indices;',begin)
original=source[begin:end]
assert 'invalid GPU RHW' in original and source.count(original)==1
source=source[:begin]+''' /* Only bounded facade-owned index spans use the vector scan. The original
  * loop retains UP pointers, RHW validation and exact first-error messages. */
 int fast_scan=0;
 if(pos!=4 && ds_index_scan_enabled()) {
     size_t span=native&&native->index ? native->index_bytes-native->index_offset : 0;
     fast_scan=ds_index_scan(indices,isize,n,vcount,base,span,&min,&max);
 }
 if(!fast_scan) {
'''+original+' }\n'+source[end:]
anchor='#include <stdarg.h>'
assert source.count(anchor)==1
source=source.replace(anchor,anchor+'''
#include "index_scan.h"
static int ds_index_scan_enabled(void) {
    static int init,enabled;
    if(!init) { const char *s=getenv("DS_FAST_INDEX_SCAN");enabled=s&&!strcmp(s,"1");init=1; }
    return enabled;
}
''',1)
(out/'d3d8_renderer.c').write_text(source)
(out/'index_scan.h').write_bytes((root/'probe/index_scan.h').read_bytes())
(out/'manifest.json').write_text(json.dumps({'original_renderer_sha256':digest,'original_scan_sha256':hashlib.sha256(original.encode()).hexdigest(),'enable':'DS_FAST_INDEX_SCAN=1','limitations':'Only XYZ and bounded facade index spans; UP/RHW and invalid inputs use original scalar validation. No FPS claim.'},indent=2)+'\n')
print('Staged bounded index scan; renderer ABI/shaders unchanged')

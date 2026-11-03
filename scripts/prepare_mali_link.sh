#!/usr/bin/env bash
# Link-only GLES ABI stub: records the firmware Mali SONAME without copying a
# proprietary driver from a device. NEVER install this stub in a runtime package.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/mali-link
image=${CROSS_IMAGE:-reconflict-cross:bookworm}
docker run --rm --network none --read-only --tmpfs /tmp \
  --user "$(id -u):$(id -g)" -v "$PWD/build/mali-link:/out:rw" "$image" bash -eu -o pipefail -c '
    aarch64-linux-gnu-nm -D --defined-only /usr/lib/aarch64-linux-gnu/libGLESv2.so |
      awk '\''$2 == "T" && $3 ~ /^gl[A-Z][A-Za-z0-9_]*$/ {print $3}'\'' | sort -u > /out/symbols.txt
    test "$(wc -l < /out/symbols.txt)" -gt 100
    awk '\''{print "void " $1 "(void) {}"}'\'' /out/symbols.txt > /out/link-only.c
    aarch64-linux-gnu-gcc -shared -nostdlib -fPIC -Wl,-soname,libmali.so.0 \
      /out/link-only.c -o /out/libGLESv2.so
  '

#!/usr/bin/env bash
# Link inputs only: these firmware libraries are never included in our package.
set -euo pipefail
cd "$(dirname "$0")/.."
target=${TARGET_SSH:?Set TARGET_SSH to your handheld SSH address}
ssh_args=(-F /dev/null -o ConnectTimeout=10)
if [[ -n ${SSH_CONTROL_SOCKET:-} ]]; then ssh_args+=(-S "$SSH_CONTROL_SOCKET"); fi
mkdir -p build/target-libs
ssh "${ssh_args[@]}" "$target" 'cat /usr/lib/libSDL2-2.0.so.0' > build/target-libs/libSDL2.so.tmp
mv build/target-libs/libSDL2.so.tmp build/target-libs/libSDL2.so
ssh "${ssh_args[@]}" "$target" 'cat /usr/lib/libGLESv2.so' > build/target-libs/libGLESv2.so.tmp
mv build/target-libs/libGLESv2.so.tmp build/target-libs/libGLESv2.so
ssh "${ssh_args[@]}" "$target" 'cat /usr/lib/libSDL2_ttf-2.0.so.0' > build/target-libs/libSDL2_ttf.so.tmp
mv build/target-libs/libSDL2_ttf.so.tmp build/target-libs/libSDL2_ttf.so
sha256sum build/target-libs/*.so > build/target-libs/SHA256SUMS

#!/usr/bin/env bash
# Host-side isolated guest ABI/state tests. SDL virtual/dummy devices only.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/input-audio-tests
compiler=${CC:-cc}
read -r -a sdl_cflags <<< "$(pkg-config --cflags sdl2)"
read -r -a sdl_libs <<< "$(pkg-config --libs sdl2)"
includes=(-Ithird_party/pcrecomp/runtime/win32hle -Ithird_party/pcrecomp/runtime/recomp32)
"$compiler" -std=gnu11 -O1 -Wall -Wextra -Werror "${includes[@]}" "${sdl_cflags[@]}" \
  runtime/dinput8.c runtime/test_input.c runtime/dinput8_selftest.c probe/desert_pad.c \
  "${sdl_libs[@]}" -lm -o build/input-audio-tests/dinput8-selftest
DS_TRACE_INPUT=1 build/input-audio-tests/dinput8-selftest --commands
build/input-audio-tests/dinput8-selftest
"$compiler" -std=gnu11 -O1 -Wall -Wextra -Werror "${includes[@]}" "${sdl_cflags[@]}" \
  runtime/dsound8.c runtime/dsound8_selftest.c third_party/pcrecomp/runtime/win32hle/dsound.c \
  "${sdl_libs[@]}" -lm -pthread -o build/input-audio-tests/dsound8-selftest
build/input-audio-tests/dsound8-selftest

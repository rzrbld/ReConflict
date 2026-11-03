#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/native-import
if [[ ${1:-} == --cross ]]; then
 image=${CROSS_IMAGE:-reconflict-cross:bookworm}
 docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
  -v "$PWD:/src:ro" -v "$PWD/build/native-import:/out:rw" --workdir /src "$image" \
  aarch64-linux-gnu-g++ -std=c++17 -O2 -Wall -Wextra -Werror -mcpu=cortex-a53 \
  -static-libstdc++ -static-libgcc probe/prepare_game.cpp -o /out/prepare-game.aarch64
else
 "${CXX:-g++}" -std=c++17 -O2 -Wall -Wextra -Werror probe/prepare_game.cpp -o build/native-import/prepare-game-host
fi

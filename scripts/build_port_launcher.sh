#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/port-launcher
if [[ ${1:-} == --cross ]]; then
 image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
 docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
   -v "$PWD:/src:ro" -v "$PWD/build/port-launcher:/out:rw" --workdir /src "$image" \
   bash -eu -o pipefail -c 'aarch64-linux-gnu-gcc -std=gnu11 -O2 -Wall -Wextra -Werror -mcpu=cortex-a53 \
     -Iruntime -Ibuild/sdk-include -Ibuild/sdk-include/SDL2 -I/usr/include/SDL2 probe/port_launcher.c runtime/configuration.c \
     -Lbuild/target-libs -Wl,--allow-shlib-undefined -lSDL2 -o /out/desert-storm-launcher.aarch64'
else
 read -r -a cflags <<< "$(pkg-config --cflags sdl2)"
 read -r -a libs <<< "$(pkg-config --libs sdl2)"
 cc -std=gnu11 -O2 -Wall -Wextra -Werror -Iruntime "${cflags[@]}" probe/port_launcher.c runtime/configuration.c "${libs[@]}" -o build/port-launcher/launcher-host
fi

#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/port-launcher
read -r -a cflags <<< "$(pkg-config --cflags sdl2)"
read -r -a libs <<< "$(pkg-config --libs sdl2)"
cc -std=gnu11 -O2 -Wall -Wextra -Werror -Iruntime "${cflags[@]}" tests/port_launcher.c runtime/configuration.c "${libs[@]}" -o build/port-launcher/test-host
for scenario in 0 1 2 3 4 5 6; do build/port-launcher/test-host "$scenario"; done

#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
bash scripts/build_native_import.sh
"${CXX:-g++}" -std=c++17 -O2 -Wall -Wextra -Werror -DDS_IMPORT_TEST probe/prepare_game.cpp -o build/native-import/prepare-game-test
python3 tests/native_import.py

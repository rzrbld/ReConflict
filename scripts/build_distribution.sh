#!/usr/bin/env bash
# Fresh-checkout build from the user's supported installer or installed files.
set -euo pipefail
unset PYTHONOPTIMIZE # Keep checked transformation assertions enabled.
cd "$(dirname "$0")/.."
if [[ $# -lt 2 || $# -gt 3 ]]; then
  echo 'Usage: bash scripts/build_distribution.sh INSTALLER_OR_GAME_DIR OUTPUT.zip [GAMEPLAY.png]' >&2
  exit 2
fi
input=$(realpath "$1")
output=$(realpath -m "$2")
screenshot=${3:-}
[[ -z "$screenshot" ]] || screenshot=$(realpath "$screenshot")
[[ ! -e "$output" ]] || { echo 'Output already exists; choose a new filename' >&2; exit 1; }
[[ ! -e build/render-thread-game/inputs.json ]] || {
  echo 'Use a fresh checkout for a full build; see BUILDING.md for stage-by-stage retries.' >&2; exit 1;
}
for tool in docker python3 innoextract dpkg-deb readelf ar patch cc; do
  command -v "$tool" >/dev/null || { echo "Missing build prerequisite: $tool" >&2; exit 1; }
done
mkdir -p build
python3 -m venv .venv
.venv/bin/pip install -r requirements-audit.txt
export PATH="$PWD/.venv/bin:$PATH"
export CROSS_IMAGE=${CROSS_IMAGE:-reconflict-cross:bookworm}
export JOBS=${JOBS:-4}
game='setup_conflict_-_desert_storm_1.0_(21657)'
if [[ -f "$input" ]]; then
  python3 scripts/hash_game_inputs.py "$input" --output build/installer.sha256
  stage=$(mktemp -d "$PWD/build/extract-XXXXXX")
  trap 'rm -rf -- "$stage"' EXIT
  innoextract --extract --exclude-temp --output-dir "$stage" "$input"
  python3 scripts/import_game_data.py "$stage" "$game"
  rm -rf -- "$stage"
  trap - EXIT
elif [[ -d "$input" ]]; then
  if [[ "$input" == "$PWD/$game" ]]; then
    python3 scripts/import_game_data.py "$input" --check-only
  else
    python3 scripts/import_game_data.py "$input" "$game"
  fi
else
  echo 'Input must be the supported GOG installer or an installed game directory' >&2; exit 1
fi
python3 scripts/hash_game_inputs.py "$game" --output build/game-inputs.sha256
bash scripts/prepare_cross.sh
python3 scripts/translate_game.py --all-modules
bash scripts/build_translated.sh
bash scripts/build_game.sh
# The portable-GLES branch supplies its dispatch implementation and relink flag.
if [[ -f runtime/gles_dispatch.c ]]; then export DS_PORTABLE_GLES=1; fi
export DS_RT_BUILD_LABEL=clean-release
bash scripts/build_optimized.sh
bash scripts/build_port_launcher.sh --cross
python3 scripts/fetch_innoextract.py
if [[ -z "$screenshot" ]]; then
  python3 scripts/fetch_catalogue_screenshot.py
  screenshot="$PWD/build/catalogue/screenshot.png"
fi
version=${VERSION:-v$(date -u +%Y%m%d-%H%M%S)}
binary=build/render-thread-game/variants/clean-release/game-render-thread.aarch64
python3 scripts/package_port.py --binary "$binary" --output "$output" \
  --version "$version" --screenshot "$screenshot"
python3 scripts/check_port_package.py "$output"
sha256sum "$output" > "$output.sha256"
echo "Built $output; hardware gameplay still requires testing."

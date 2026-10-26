#!/usr/bin/env bash
# Run immediately after game launch, before its first 3D opening frames.
set -euo pipefail
cd "$(dirname "$0")/.."
label=${1:?evidence label required}
[[ "$label" =~ ^[a-zA-Z0-9_-]+$ ]] || exit 2
out=docs/evidence/$label
[[ ! -e "$out" ]] || { echo 'Evidence already exists' >&2;exit 1; }
mkdir -p "$out"
ssh -F /dev/null -S "${SSH_CONTROL_SOCKET:?}" -o BatchMode=yes "${TARGET_SSH:?Set TARGET_SSH to your handheld SSH address}" 'bash -s' <<'REMOTE' | tar -xf - -C "$out"
set -euo pipefail
cd /mnt/sdcard/ports/desert-storm
scratch=$(mktemp -d tmp/opening-trace.XXXXXX)
trap 'rm -rf "$scratch"' EXIT
found=0
for n in $(seq 1 250); do
 line=$(tail -n 1 render.csv 2>/dev/null || true)
 frame=${line%%,*};rest=${line#*,};draws=${rest%%,*}
 if [[ "$frame" =~ ^[0-9]+$ && "$draws" =~ ^[0-9]+$ ]] && ((frame<1400 && draws>50)); then found=1; break; fi
 sleep .1
done
[[ "$found" == 1 ]] || { echo 'Opening 3D frame not found in startup window' >&2;exit 1; }
for sample in 1 2 3; do
 # Atomic rename prevents the game from consuming an empty command file.
 printf 'trace\n' > render.control.next
 mv render.control.next render.control
 for n in $(seq 1 100); do [[ -e render.control ]] || break; sleep .05;done
 [[ ! -e render.control ]]
 sleep .25
 touch capture.request
 for n in $(seq 1 100); do [[ -e capture.request ]] || break;sleep .05;done
 [[ ! -e capture.request ]]
 sleep .2
 mkdir "$scratch/$sample"
 cp capture.bmp render-frame.txt "$scratch/$sample/"
 date -u > "$scratch/$sample/time.txt"
 tail -n 3 render.csv > "$scratch/$sample/render-tail.csv"
 sleep .4
done
tar -cf - -C "$scratch" .
REMOTE

#!/usr/bin/env bash
# Collect existing frame-time logs after startup; never requests screenshots.
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
for n in $(seq 1 100);do
 line=$(tail -n 1 render.csv 2>/dev/null || true);frame=${line%%,*}
 if [[ "$frame" =~ ^[0-9]+$ ]] && ((frame>=650));then break;fi
 sleep .5
done
[[ "$frame" =~ ^[0-9]+$ ]] && ((frame>=650))
pid=$(pidof desert-storm.aarch64);kill -USR1 "$pid";sleep 1
{ date -u;cat /proc/$pid/status;cat /sys/class/thermal/thermal_zone0/temp;cat /sys/devices/system/cpu/cpufreq/policy0/scaling_cur_freq; } > tmp/startup-device-state.txt
tar -cf - profile.csv render.csv render-phase.csv log.txt tmp/startup-device-state.txt
REMOTE

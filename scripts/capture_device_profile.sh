#!/usr/bin/env bash
# Snapshot the running opt-in profiler over a caller-labelled stationary scene.
# Use DS_CPU_PROFILE=main for the thread-directed diagnostic, or 1 only for
# the old synchronous build; DS_TRACE_RENDER=1. For FPS-only measurements use
# DS_CPU_PROFILE=0 and CAPTURE_NATIVE=0. Do not capture images during
# this interval. Signal snapshots themselves add I/O; exclude their boundaries.
set -euo pipefail
cd "$(dirname "$0")/.."
label=${1:?Usage: capture_device_profile.sh label [seconds:1-60]}
seconds=${2:-30}
native=${CAPTURE_NATIVE:-1}
capture_phase_enabled=${CAPTURE_PHASE:-0}
[[ "$native" =~ ^[01]$ && "$capture_phase_enabled" =~ ^[01]$ ]] || { echo 'CAPTURE_NATIVE/PHASE must be 0 or 1' >&2; exit 1; }
[[ "$label" =~ ^[a-zA-Z0-9][a-zA-Z0-9_-]*$ ]] || { echo 'Invalid evidence label' >&2; exit 1; }
[[ "$seconds" =~ ^[0-9]+$ ]] && ((seconds>=1 && seconds<=60)) || { echo 'Duration must be 1-60 seconds' >&2; exit 1; }
out="docs/evidence/$label"
[[ ! -e "$out" ]] || { echo "Evidence already exists: $out" >&2; exit 1; }
mkdir -p "$out/before" "$out/after"
ssh_args=(-F /dev/null -o BatchMode=yes -o ConnectTimeout=10)
if [[ -n ${SSH_CONTROL_SOCKET:-} ]]; then ssh_args+=(-S "$SSH_CONTROL_SOCKET"); fi
target=${TARGET_SSH:?Set TARGET_SSH to your handheld SSH address}
snapshot() {
    local phase=$1
    date -u +%FT%TZ > "$out/$phase/local-time.txt"
    ssh "${ssh_args[@]}" "$target" "capture_native=$native; capture_phase=$capture_phase_enabled; "'set -e
      cd /mnt/sdcard/ports/desert-storm
      pid=$(pidof desert-storm.aarch64)
      test -n "$pid"
      kill -USR1 "$pid"
      sleep 1
      { date -u; cat /proc/$pid/status; cat /sys/class/thermal/thermal_zone0/temp; cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq; } > tmp/profile-device-state.txt
      set -- profile.csv render.csv tmp/profile-device-state.txt
      if [ "$capture_native" = 1 ]; then set -- "$@" cpu-profile.csv host-cpu-profile.csv host-call-profile.csv host-profile-maps.txt; fi
      if [ "$capture_phase" = 1 ]; then set -- "$@" render-phase.csv; fi
      if [ "$capture_phase" = 1 ] && [ -f present-timing.csv ]; then set -- "$@" present-timing.csv; fi
      tar -cf - "$@"' |
      tar -xf - -C "$out/$phase"
}
snapshot before
printf 'PROFILE_INTERVAL_STARTED label=%s seconds=%s\n' "$label" "$seconds"
sleep "$seconds"
snapshot after
printf 'PROFILE_INTERVAL_DONE evidence=%s\n' "$out"

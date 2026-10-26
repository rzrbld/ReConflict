#!/usr/bin/env bash
# Stationary diagnostic A/B/A; no screenshots or controller actions.
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
pid=$(pidof desert-storm.aarch64)
[[ "$pid" =~ ^[0-9]+$ && ! -e render.control && ! -e capture.request ]]
tr '\0' '\n' < /proc/$pid/environ | grep -x DS_CAPTURE_REQUEST=1 >/dev/null
start=$(awk '{print $22}' /proc/$pid/stat)
same_game(){ [[ -r /proc/$pid/stat && "$(awk '{print $22}' /proc/$pid/stat)" == "$start" ]]; }
temporary=render.control.dispatch.$$.tmp
command(){
 local value=$1
 same_game && [[ ! -e render.control ]]
 local offset=$(( $(wc -c < log.txt) + 1 ))
 printf '%s\n' "$value" > "$temporary"
 mv "$temporary" render.control
 for attempt in $(seq 1 100);do
  same_game
  if tail -c "+$offset" log.txt | grep "command=$value " > tmp/dispatch-command-last.txt;then
   cat tmp/dispatch-command-last.txt >> tmp/dispatch-comparison-commands.txt
   return 0
  fi
  sleep .05
 done
 echo "No acknowledgement for $value" >&2;return 1
}
cleanup(){
 rm -f "$temporary"
 if same_game && [[ ! -e render.control ]];then
  printf 'nocache\n' > "$temporary";mv "$temporary" render.control
 fi
}
trap cleanup EXIT
: > tmp/dispatch-comparison-commands.txt
for mode in nocache cache nocache;do
 command "$mode"
 sleep 12
done
command cachestats
same_game
kill -USR1 "$pid"
sleep 1
{ date -u;cat /proc/$pid/status;cat /sys/class/thermal/thermal_zone0/temp;cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq; } > tmp/dispatch-comparison-state.txt
tar -cf - profile.csv render.csv present-timing.csv log.txt tmp/dispatch-comparison-commands.txt tmp/dispatch-comparison-state.txt
REMOTE
printf 'Saved stationary dispatch A/B/A: %s\n' "$out"

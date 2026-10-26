#!/bin/bash
# One user-triggered screenshot plus adjacent-frame renderer state.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
cd "$root"
label=${1:-scene}
if [[ "$label" == --help ]];then
    echo 'Usage: ./capture-scene.sh [label]'
    echo 'One capture, then exits. Ctrl+C cancels. Requires a game launched with DS_CAPTURE_REQUEST=1.'
    exit 0
fi
[[ $# -le 1 && "$label" =~ ^[a-zA-Z0-9][a-zA-Z0-9_-]{0,47}$ ]] || { echo 'Use a short label containing letters, digits, hyphens or underscores.' >&2;exit 2; }
read -r -a game_pids <<< "$(pidof desert-storm.aarch64 || true)"
[[ ${#game_pids[@]} == 1 ]] || { echo 'Exactly one running game is required.' >&2;exit 1; }
game_pid=${game_pids[0]}
tr '\0' '\n' < "/proc/$game_pid/environ" | grep -x 'DS_CAPTURE_REQUEST=1' >/dev/null || {
    echo 'This session has capture disabled. Relaunch with DS_CAPTURE_REQUEST=1.' >&2;exit 1;
}
[[ -r log.txt ]] || { echo 'PortMaster log.txt is missing.' >&2;exit 1; }
mkdir -p tmp captures
lock=tmp/capture-scene.lock
mkdir "$lock" 2>/dev/null || { echo 'Another capture is active.' >&2;exit 1; }
control_owned=0;image_owned=0;pending_command=''
cleanup(){
    rm -f "render.control.$$.tmp"
    if [[ "$control_owned" == 1 && -f render.control ]] && [[ "$(cat render.control)" == "$pending_command" ]];then rm -f render.control;fi
    [[ "$image_owned" != 1 ]] || rm -f capture.request
    rmdir "$lock" 2>/dev/null || true
}
trap cleanup EXIT
trap 'exit 130' INT TERM HUP
[[ ! -e render.control && ! -e capture.request ]] || { echo 'A renderer/capture request is already pending; try again shortly.' >&2;exit 1; }
log_start=$(( $(wc -c < log.txt) + 1 ))
new_log(){ tail -c "+$log_start" log.txt; }
wait_log(){
    local pattern=$1
    for ((attempt=0;attempt<200;attempt++));do
        kill -0 "$game_pid" 2>/dev/null || { echo 'The game exited during capture.' >&2;return 1; }
        if new_log | grep -- "$pattern" >/dev/null;then return 0;fi
        sleep .05
    done
    echo "Capture timed out waiting for $pattern" >&2;return 1
}
command(){
    pending_command=$1
    [[ ! -e render.control ]] || { echo 'Another diagnostic request appeared.' >&2;return 1; }
    printf '%s\n' "$pending_command" > "render.control.$$.tmp"
    control_owned=1
    mv "render.control.$$.tmp" render.control
    wait_log "command=$pending_command "
    control_owned=0
}
out=$(mktemp -d "captures/$(date +%Y%m%d-%H%M%S)-$label.XXXXXX")
printf 'Capturing %s...\n' "$label"
command trace
image_owned=1
touch capture.request
wait_log 'captured game frame=.*source=request '
image_owned=0
# Any diagnostic command is processed after Present closes the trace stream.
# This unique no-op gives an explicit completion barrier before copying it.
command "capdone_$$"
[[ -s capture.bmp && -f render-frame.txt ]] || { echo 'Capture outputs missing.' >&2;exit 1; }
cp capture.bmp render-frame.txt "$out/"
new_log > "$out/capture.log"
{
    date -u
    printf 'label=%s\ngame_pid=%s\n' "$label" "$game_pid"
    echo 'Screenshot and trace can represent adjacent frames. Trace/screenshot I/O affects FPS; this is not a performance sample.'
    cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null || true
    cat /sys/devices/system/cpu/cpufreq/policy0/scaling_cur_freq 2>/dev/null || true
} > "$out/context.txt"
printf 'Saved: %s/%s\nDone; no further captures queued.\n' "$root" "$out"

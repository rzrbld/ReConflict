#!/bin/bash
# Original game translated for AArch64; native SDL2 input and GLES graphics.
XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}
if [ -z "${controlfolder:-}" ]; then
    for candidate in /opt/system/Tools/PortMaster /opt/tools/PortMaster \
        "$XDG_DATA_HOME/PortMaster" /mnt/mmc/MUOS/PortMaster /roms/ports/PortMaster; do
        if [ -f "$candidate/control.txt" ]; then controlfolder=$candidate; break; fi
    done
fi
if [ ! -f "${controlfolder:-}/control.txt" ]; then
    echo "PortMaster control.txt not found" >&2; exit 1
fi
export controlfolder
source "$controlfolder/control.txt"
[ ! -f "$controlfolder/mod_${CFW_NAME}.txt" ] || source "$controlfolder/mod_${CFW_NAME}.txt"
get_controls
GAMEDIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/reconflict"
if [ ! -d "$GAMEDIR" ]; then
    sdroot="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
    [ ! -d "$sdroot/ports/reconflict" ] || GAMEDIR="$sdroot/ports/reconflict"
fi
if [ ! -d "$GAMEDIR" ] && [ -n "${directory:-}" ]; then GAMEDIR="/${directory#/}/ports/reconflict"; fi
# Keep existing saves/installations usable under the historical directory name.
if [ ! -d "$GAMEDIR" ]; then
    for candidate in "$(dirname -- "${BASH_SOURCE[0]}")/desert-storm" \
        "${sdroot:-/mnt/sdcard}/ports/desert-storm" "/${directory#/}/ports/desert-storm"; do
        if [ -d "$candidate" ]; then GAMEDIR=$candidate; break; fi
    done
fi
cd "$GAMEDIR" || exit 1
exec > >(tee "$GAMEDIR/log.txt") 2>&1
if [ "${DEVICE_ARCH:-aarch64}" != aarch64 ]; then echo "AArch64 build required"; exit 1; fi
export SDL_GAMECONTROLLERCONFIG="${sdl_controllerconfig:-${SDL_GAMECONTROLLERCONFIG:-}}"
# This firmware's generic same-GUID mapping omits the right stick. Use its
# named built-in controller mapping when that exact device is present.
if grep -q 'Name="Deeplay-keys"' /proc/bus/input/devices && [ -f /usr/lib/gamecontrollerdb.txt ]; then
    mapping=$(grep -m1 ',Deeplay-keys,' /usr/lib/gamecontrollerdb.txt)
    [ -z "$mapping" ] || export SDL_GAMECONTROLLERCONFIG="$mapping"
fi
export DS_PAD_CONFIG="$GAMEDIR/controller-buttons.cfg"
# muOS's ALSA default is a PipeWire plugin; SSH sessions need its socket path.
if [ -S /run/pipewire-0 ]; then export XDG_RUNTIME_DIR=/run; fi
export SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-alsa}
# SSH launches bypass ext-general.sh. Inherit the same device display values
# when that firmware handoff has not already supplied them.
if [ "${CFW_NAME:-}" = muOS ] && [ -d /run/muos/device/sdl ]; then
    [ -n "${SDL_ROTATION+x}" ] || export SDL_ROTATION="$(cat /run/muos/device/sdl/rotation)"
    [ -n "${SDL_HQ_SCALER+x}" ] || export SDL_HQ_SCALER="$(cat /run/muos/device/sdl/scaler)"
    [ -n "${SDL_BLITTER_DISABLED+x}" ] || export SDL_BLITTER_DISABLED="$(cat /run/muos/device/sdl/blitter_disabled)"
fi
program=desert-storm.aarch64
# Defaults exercised on the handheld; retain overrides for comparisons.
export DS_GPU_VERTEX=${DS_GPU_VERTEX:-1}
export DS_GPU_BUFFER_CACHE=${DS_GPU_BUFFER_CACHE:-1}
export DS_FAST_MATH=${DS_FAST_MATH:-1}
# Exact rotation scalarization passed full-state H700 tests and the opening trial.
export DS_FAST_ROTATION=${DS_FAST_ROTATION:-1}
export DS_FAST_LENGTH=${DS_FAST_LENGTH:-1}
export DS_FAST_PHYSICS_PRODUCT=${DS_FAST_PHYSICS_PRODUCT:-1}
export DS_FAST_INDEX_SCAN=${DS_FAST_INDEX_SCAN:-1}
export DS_FAST_CULLING=${DS_FAST_CULLING:-1}
# H700-tested native GL worker; ignored by builds without the experimental proxy.
# Keep an explicit override for synchronous rendering comparisons.
export DS_RENDER_THREAD=${DS_RENDER_THREAD:-1}
# Measured balance between fewer worker wakeups and submission latency.
export DS_RT_DRAW_BATCH=${DS_RT_DRAW_BATCH:-8}
# Exact particle shader passed Mali pixel tests and the burning-vehicle scene.
export DS_FAST_PARTICLES=${DS_FAST_PARTICLES:-1}
# Mali-tested exact fog/glow paths; set either to 0 for comparisons.
export DS_FAST_FOG=${DS_FAST_FOG:-1}
export DS_FAST_GLOW=${DS_FAST_GLOW:-1}
if [ "${CFW_NAME:-}" = muOS ] && [ -f "$GAMEDIR/muos-session.sh" ]; then
    source "$GAMEDIR/muos-session.sh"
    ds_muos_suspend || exit 1
fi
ds_finish() {
    if declare -F ds_cpu_restore >/dev/null; then ds_cpu_restore; fi
    if declare -F ds_muos_resume >/dev/null; then ds_muos_resume; fi
    # The options-only path never starts gptokeyb. Avoid a needless input
    # service restart when the player cancels without launching the game.
    if [ "${ds_game_started:-0}" = 1 ]; then pm_finish || true
    elif declare -F pm_message_end >/dev/null; then pm_message_end || true; fi
}
trap ds_finish EXIT
if [ -f "$GAMEDIR/cpu-session.sh" ]; then
    source "$GAMEDIR/cpu-session.sh"
    ds_cpu_begin
fi
pm_platform_helper "$GAMEDIR/desert-storm-launcher.aarch64"
./desert-storm-launcher.aarch64 --handoff "$@"
action=$?
case "$action" in
    20) export DS_FPS_OVERLAY=0 ;;
    21) export DS_FPS_OVERLAY=1 ;;
    *) exit "$action" ;;
esac
cap=$(sed -n 's/^frame_cap=//p' "$GAMEDIR/config/launcher.cfg")
case "$cap" in 0|30|60) export DS_FRAME_CAP="$cap" ;; *) export DS_FRAME_CAP=60 ;; esac
# The bootstrap handles controller input directly. Start the PortMaster exit
# helper only once the game takes over, using the actual game process name.
$GPTOKEYB "$program" &
ds_game_started=1
pm_platform_helper "$GAMEDIR/$program"
"./$program"
exit $?

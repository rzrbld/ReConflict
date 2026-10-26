#!/bin/bash
# Native SDL2 input; follows the installed Vurus.sh PortMaster setup.
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
GAMEDIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/desert-storm-bringup"
if [ ! -d "$GAMEDIR" ] && [ -n "${directory:-}" ]; then GAMEDIR="/${directory#/}/ports/desert-storm-bringup"; fi
cd "$GAMEDIR" || exit 1
exec > >(tee "$GAMEDIR/log.txt") 2>&1
if [ "${DEVICE_ARCH:-aarch64}" != aarch64 ]; then echo "AArch64 build required"; exit 1; fi
export SDL_GAMECONTROLLERCONFIG="${sdl_controllerconfig:-${SDL_GAMECONTROLLERCONFIG:-}}"
export PROBE_CONTROLLER_DB="$controlfolder/gamecontrollerdb.txt"
export DS_PAD_CONFIG="$GAMEDIR/controller-buttons.cfg"
# muOS's ALSA default is a PipeWire plugin; SSH sessions need its socket path.
if [ -S /run/pipewire-0 ]; then export XDG_RUNTIME_DIR=/run; fi
export SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-alsa}
program=desert-setup.aarch64
if [ "${1:-}" = --probe ]; then program=platform-probe.aarch64; shift; fi
trap 'pm_finish' EXIT
$GPTOKEYB "$program" &
pm_platform_helper "$GAMEDIR/$program"
"./$program" "$@"
exit $?

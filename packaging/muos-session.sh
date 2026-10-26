#!/bin/bash
# SSH does not perform muOS's normal menu-to-port handoff. Temporarily stop
# only its graphical frontend and frontend loop; retain hotkeys and audio.
ds_muos_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
ds_muos_state="$ds_muos_root/tmp/muos-suspended.txt"
ds_muos_boot=$(cat /proc/sys/kernel/random/boot_id)
ds_muos_owned=0
ds_muos_start() { awk '{print $22}' "/proc/$1/stat" 2>/dev/null; }
ds_muos_resume() {
    [ "$ds_muos_owned" = 1 ] || return 0
    [ -f "$ds_muos_state" ] || return 0
    while read -r boot pid start; do
        if [ "$boot" = "$ds_muos_boot" ] && [ "$(ds_muos_start "$pid")" = "$start" ]; then
            kill -CONT "$pid" 2>/dev/null || true
            echo "[muos-session] resumed frontend pid=$pid"
        fi
    done < "$ds_muos_state"
    rm -f "$ds_muos_state"
    ds_muos_owned=0
}
ds_muos_suspend() {
    [ -d /opt/muos/extra ] || return 0
    mkdir -p "$ds_muos_root/tmp"
    # A stale file is recoverable; do not overwrite another active owner's list.
    if [ -s "$ds_muos_state" ]; then
        echo '[muos-session] frontend already suspended; use --recover after the current game exits' >&2
        return 1
    fi
    : > "$ds_muos_state"
    ds_muos_owned=1
    for entry in /proc/[0-9]*; do
        pid=${entry##*/}
        exe=$(readlink "$entry/exe" 2>/dev/null) || continue
        args=$(tr '\000' ' ' < "$entry/cmdline" 2>/dev/null)
        case "$exe:$args" in
            /opt/muos/extra/mux*:*) ;;
            *:'/bin/sh /opt/muos/script/mux/frontend.sh '*) ;;
            *) continue ;;
        esac
        state=$(awk '/^State:/{print $2}' "$entry/status" 2>/dev/null)
        [ "$state" != T ] && [ "$state" != t ] || continue
        start=$(ds_muos_start "$pid")
        [ -n "$start" ] || continue
        if kill -STOP "$pid" 2>/dev/null; then
            printf '%s %s %s\n' "$ds_muos_boot" "$pid" "$start" >> "$ds_muos_state"
            echo "[muos-session] suspended frontend pid=$pid executable=$exe"
        fi
    done
}
if [ "${BASH_SOURCE[0]}" = "$0" ]; then
    case "${1:-}" in
        --recover) ds_muos_owned=1; ds_muos_resume ;;
        --watch)
            game_pid=${2:?game PID required}
            [[ "$game_pid" =~ ^[0-9]+$ ]] || exit 2
            game_start=$(ds_muos_start "$game_pid")
            [ -n "$game_start" ] || exit 1
            trap ds_muos_resume EXIT
            trap 'exit 130' INT TERM HUP
            ds_muos_suspend || exit 1
            while [ "$(ds_muos_start "$game_pid")" = "$game_start" ]; do sleep 0.2; done
            ;;
        *) echo 'Usage: muos-session.sh --watch GAME_PID | --recover' >&2; exit 2 ;;
    esac
fi

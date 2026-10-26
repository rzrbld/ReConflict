#!/usr/bin/env bash
# Isolated original PC game reference. Never uses an existing Wine prefix or
# the original installation as the writable working directory.
set -euo pipefail
project=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
reference="$project/build/pc-reference"
mkdir -p "$reference" "$project/docs/evidence"
if [[ ! -f "$reference/data-copy.complete" ]]; then
    mkdir -p "$reference/data"
    cp -a --reflink=auto "$project/setup_conflict_-_desert_storm_1.0_(21657)/." "$reference/data/"
    touch "$reference/data-copy.complete"
fi
export WINEPREFIX="$reference/prefix"
export WINEARCH=win32
wine_bin=${PC_WINE_BIN:-$(command -v wine32-stable || command -v wine)}
export WINEDEBUG=${PC_WINEDEBUG:--all}
export WINEDLLOVERRIDES="winemenubuilder.exe=d"
if [[ ${PC_REFERENCE_INNER:-0} != 1 ]]; then
    # One server/display per disposable prefix. Prevent overlapping reference
    # sessions from reusing a prefix attached to another X server.
    exec 9>"$reference/session.lock"
    flock -n 9 || { printf 'PC reference session is already active\n' >&2; exit 1; }
    geometry=${PC_REFERENCE_GEOMETRY:-1024x768}
    [[ "$geometry" =~ ^[0-9]+x[0-9]+$ ]] || { printf 'Invalid PC_REFERENCE_GEOMETRY\n' >&2; exit 1; }
    exec xvfb-run -a -s "-screen 0 ${geometry}x24 +extension GLX -nolisten tcp" \
        env PC_REFERENCE_INNER=1 "$0" "$@"
fi
printf '%s\n' "$DISPLAY" > "$reference/display.txt"
printf '%s\n' "$XAUTHORITY" > "$reference/xauthority-path.txt"
printf '%s\n' "Wine prefix: $WINEPREFIX" "Xvfb display: $DISPLAY"
if [[ ! -f "$reference/prefix-setup.complete" ]]; then
    timeout 60 "$wine_bin" wineboot.exe -u
    # Wine may map its user's documents to host-home folders. Keep all Windows
    # user output inside this newly-created disposable prefix instead.
    for userdir in "$WINEPREFIX"/drive_c/users/*; do
        [[ -d "$userdir" ]] || continue
        for name in 'Desktop' 'Documents' 'My Documents' 'Music' 'Pictures' 'Videos' 'Downloads'; do
            path="$userdir/$name"
            if [[ -L "$path" ]]; then rm -- "$path"; fi
            mkdir -p "$path"
        done
    done
    touch "$reference/prefix-setup.complete"
fi
# wineboot also links nested folders (for example Windows/Templates). Replace
# every user-folder link escaping this prefix, including on subsequent boots.
while IFS= read -r -d '' path; do
    target=$(readlink -m -- "$path")
    case "$target" in "$WINEPREFIX"/*) continue ;; esac
    rm -- "$path"
    mkdir -p -- "$path"
done < <(find "$WINEPREFIX/drive_c/users" -type l -print0)
cd "$reference/data"
"$wine_bin" ./DesertStorm.exe > "$reference/game.log" 2>&1 &
game_pid=$!
printf '%s\n' "$game_pid" > "$reference/launcher.pid"
cleanup() {
    local server=/usr/lib/i386-linux-gnu/wine/wineserver
    [[ -x "$server" ]] || server=$(command -v wineserver)
    WINEPREFIX="$reference/prefix" "$server" -k 2>/dev/null || true
    kill "$game_pid" 2>/dev/null || true
}
trap cleanup EXIT
seconds=${PC_REFERENCE_SECONDS:-45}
for ((second=0; second<seconds; ++second)); do
    if (( second == 5 || second == 15 || second == 30 || second == 60 || second == 120 )); then
        import -window root "$reference/screen-$second.png" 2>"$reference/screenshot-$second.log" || true
        xwininfo -root -tree > "$reference/windows-$second.txt" 2>&1 || true
    fi
    if ! kill -0 "$game_pid" 2>/dev/null; then
        wait "$game_pid" && result=0 || result=$?
        printf 'ORIGINAL_GAME_EXIT=%s\n' "$result"
        exit "$result"
    fi
    sleep 1
done
printf 'ORIGINAL_GAME_TIMED_WINDOW=%s\n' "$seconds"

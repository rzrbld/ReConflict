#!/usr/bin/env bash
# Atomic replacement: safe even while the previous executable is mapped.
set -euo pipefail
cd "$(dirname "$0")/.."
target=${TARGET_SSH:?Set TARGET_SSH to your handheld SSH address}
runtime_binary=${RUNTIME_BINARY:-build/game/desert-storm.aarch64}
ssh_args=(-F /dev/null -o ConnectTimeout=10)
if [[ -n ${SSH_CONTROL_SOCKET:-} ]]; then ssh_args+=(-S "$SSH_CONTROL_SOCKET"); fi
readelf -h "$runtime_binary" | grep -q 'Machine:.*AArch64'
# This updater assumes the complete ReConflict package was installed first.
# Fail before changing anything if its new frontend/importer is absent.
ssh "${ssh_args[@]}" "$target" 'test -x /mnt/sdcard/ports/desert-storm/desert-storm-launcher.aarch64 && test -f /mnt/sdcard/ports/desert-storm/prepare_game.py && test -f /mnt/sdcard/ports/desert-storm/game-files.sha256' || {
  echo 'Install the complete ReConflict runtime package before using this binary updater.' >&2; exit 1;
}
gzip -1 -c "$runtime_binary" |
  ssh "${ssh_args[@]}" "$target" 'set -e; cd /mnt/sdcard/ports/desert-storm; gunzip -c > desert-storm.aarch64.new; chmod 755 desert-storm.aarch64.new; mv desert-storm.aarch64.new desert-storm.aarch64'
ssh "${ssh_args[@]}" "$target" 'set -e; cd /mnt/sdcard/ports/desert-storm; cat > muos-session.sh.new; chmod 755 muos-session.sh.new; mv muos-session.sh.new muos-session.sh' < packaging/muos-session.sh
ssh "${ssh_args[@]}" "$target" 'set -e; cd /mnt/sdcard/ports/desert-storm; cat > cpu-session.sh.new; chmod 755 cpu-session.sh.new; mv cpu-session.sh.new cpu-session.sh' < packaging/cpu-session.sh
ssh "${ssh_args[@]}" "$target" 'set -e; cd /mnt/sdcard/ports/desert-storm; cat > capture-scene.sh.new; chmod 755 capture-scene.sh.new; mv capture-scene.sh.new capture-scene.sh' < packaging/capture-scene.sh
ssh "${ssh_args[@]}" "$target" 'set -e; cd /mnt/sdcard/ROMS/Ports; cat > "ReConflict.sh.new"; chmod 755 "ReConflict.sh.new"; mv "ReConflict.sh.new" "ReConflict.sh"' < 'packaging/ReConflict.sh'
local_hash=$(sha256sum "$runtime_binary" | cut -d ' ' -f 1)
remote_hash=$(ssh "${ssh_args[@]}" "$target" 'sha256sum /mnt/sdcard/ports/desert-storm/desert-storm.aarch64' | cut -d ' ' -f 1)
[[ "$local_hash" == "$remote_hash" ]]
printf 'Deployed and verified AArch64 executable: %s\n' "$remote_hash"

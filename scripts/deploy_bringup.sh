#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
target=${TARGET_SSH:?Set TARGET_SSH to your handheld SSH address}
ssh_args=(-F /dev/null -o ConnectTimeout=10)
if [[ -n ${SSH_CONTROL_SOCKET:-} ]]; then ssh_args+=(-S "$SSH_CONTROL_SOCKET"); fi
ssh "${ssh_args[@]}" "$target" \
  'test ! -e /mnt/sdcard/ports/desert-storm-bringup && test ! -e "/mnt/sdcard/ROMS/Ports/Desert Storm Setup.sh"' || {
    echo 'Destination exists; inspect it before replacing an earlier build.' >&2; exit 1;
}
tar -C build/package -cf - desert-storm-bringup |
  ssh "${ssh_args[@]}" "$target" 'tar -C /mnt/sdcard/ports -xf -'
ssh "${ssh_args[@]}" "$target" \
  'cat > "/mnt/sdcard/ROMS/Ports/Desert Storm Setup.sh"; chmod 755 "/mnt/sdcard/ROMS/Ports/Desert Storm Setup.sh"' \
  < 'build/package/Desert Storm Setup.sh'

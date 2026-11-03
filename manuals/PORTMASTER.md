# PortMaster packaging audit

Checked 2026-11-03 against [PortMaster's packaging guide](https://portmaster.games/packaging.html).
This is a local audit, not PortMaster approval.

## Implemented and checked

- Identifier `reconflict`, capitalized shell entries, version-4 metadata,
  AArch64/glibc requirements and commercial/experimental status.
- Build command creates a runtime ZIP and optionally `ports/reconflict/` for
  submission. All installed payload belongs to the two scripts or `reconflict/`;
  no shared top-level manuals/LICENSE folder is installed.
- Actual H700 gameplay PNG, 640x480, referenced by `gameinfo.xml`; the installed
  copy is inside the game directory. No extracted game artwork is included.
- Boost libraries in `libs.aarch64/`; native innoextract loads them from there.
  MIT/zlib/Boost notices and static GCC runtime notices, GPLv3 and its runtime
  exception are bundled under `licenses/`. SDL notices cover firmware dependencies.
- Shell uses PortMaster control/mod files, controller mappings, platform helper
  and game-exit cleanup. Settings cancellation retains its tested lightweight
  cleanup to avoid the earlier muOS input-service/reboot regression. CPU governor
  and suspended muOS frontend are restored; no other firmware frontend is stopped.
- Saves and settings stay in the game directory. Installer remains until the
  player explicitly confirms deletion. README thanks the original developer.
- `scripts/check_port_package.py` checks the resulting ZIP. It cannot judge image
  contents or hardware behavior; the chosen gameplay capture was visually checked.

## Still required before submission

No Discord testing thread has been created by this task. Arrange community
testing and record its link/results before submitting. Existing evidence covers
H700/muOS at 640x480, not the complete firmware/display matrix in the guide.
AmberELEC, dArkOS, ROCKNIX (Mali and Panfrost), Knulli and higher-resolution
testing remain outstanding. Optional targets are not claimed as tested.

The native-import update has passed PC tests and ARM compilation; H700 was
unreachable for the new end-to-end import/launch check. The release build now
regenerates optimized game inputs from public source plus the supported original
files; rebuilding does not replace the outstanding hardware tests. Main still links
`libmali.so.0`; the separate portable-GLES experiment needs its own testing.
No universal device compatibility is claimed. Keep `exp: true`.

Distribution of translated proprietary game logic and the appropriate upstream
repository need PortMaster maintainer review; supplying assets separately does
not itself establish acceptance. This task neither opens a submission PR nor
asserts redistribution permission for the original game.

# Troubleshooting

[Back to the player guide](../README.md)


Training, campaign gameplay, audio, controllers, save/reload, scope rendering
and the formerly dark room have been exercised on an actual RG40XX-H.
At **640×480 / lowest detail**, expect roughly **20–30 FPS in busy gameplay**,
with higher rates in lighter scenes. Large explosions can briefly dip into
the teens. These are observed scene-dependent results, not a guaranteed minimum;
**stable 30 FPS throughout the game is not established**. Full-game compatibility
is not yet verified.

The loading-background preservation fix includes the black mission briefing
after the initial picture. Pixel/state tests pass on host Mesa and H700 Mali;
visual confirmation across actual stage loads is pending.

- **Installer rejected:** use the exact English GOG 21657 installer. Other
  editions, languages and modified files are unsupported.
- **Incomplete data:** inspect `install.log`. A nonempty `data/` is never
  overwritten; move it aside before retrying a fresh import.
- **Power loss during import:** your installer remains. With no import running,
  remove abandoned `.import-*` folders inside the port to reclaim space, then
  retry. Normal cancellation cleans its own staging folder.
- **Immediate exit:** check `log.txt`, the native import helper and the ARM64 package.
  Launch through PortMaster for firmware display/controller setup and muOS menu
  suspension.
- **FPS setting:** `config/launcher.cfg` contains `fps_overlay=1` or `0`.
  `frame_cap=30`, `60` (default), or `0` (Uncapped) controls presentation pacing.
  Malformed settings are reported and preserved.


## Manual import from an installed Windows copy

Already have the Windows game installed? Instead of supplying an installer,
use the native helper to validate and copy its files.

On a PC, build the native helper with `bash scripts/build_native_import.sh`,
then use an existing port directory as the root:

```sh
build/native-import/prepare-game-host --root /path/to/port \
  --source /path/to/installed/game
```

On ARM64 Linux, use `./prepare-game.aarch64 --root . --source /path/to/game`
from the port directory. Add `--check-only` to verify the source without copying.
The root must contain `game-files.sha256`, and its `data/` must be absent or empty.
Only files listed in the manifest are copied; source saves/settings stay untouched.

After upgrading from the Python importer, the first launch re-verifies existing
data once and creates `data-ready.native`. Later unchanged launches use the fast
cache. The old `data-ready.json` is harmless and is no longer used.

## Custom controls

Edit `controller-buttons.cfg` for button mappings, dead zones and trigger thresholds.
Physical controller reconnect remains unverified; virtual-controller reconnect tests pass.

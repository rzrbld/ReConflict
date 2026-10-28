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
- **Immediate exit:** check `log.txt`, Python availability and the ARM64 package.
  Launch through PortMaster for firmware display/controller setup and muOS menu
  suspension.
- **FPS setting:** `config/launcher.cfg` contains `fps_overlay=1` or `0`.
  `frame_cap=30`, `60` (default), or `0` (Uncapped) controls presentation pacing.
  Malformed settings are reported and preserved.


## Manual import from an installed Windows copy

Already have the Windows game installed? Instead of supplying an installer,
copy and validate its files on a PC with Python:

```sh
python3 scripts/import_game_data.py /path/to/installed/game /path/to/port/data
```

The destination must be absent or empty. Only the supported version is accepted;
saves/configuration are not imported. When using the copy of this script in the
runtime package, add `--manifest game-files.sha256` from the port directory.


## Custom controls

Edit `controller-buttons.cfg` for button mappings, dead zones and trigger thresholds.
Physical controller reconnect remains unverified; virtual-controller reconnect tests pass.

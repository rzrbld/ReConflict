ReConflict - ARM64 community port for Conflict: Desert Storm

Copy reconflict/ to /mnt/sdcard/ports/reconflict/ and the two ReConflict
shell launchers to /mnt/sdcard/ROMS/Ports. Launch ReConflict through PortMaster.

Supply your own English GOG Windows 1.0 build21657 installer:
setup_conflict_-_desert_storm_1.0_(21657).exe
Place it beside desert-storm.aarch64. The SDL frontend extracts and verifies
required files, then asks whether to keep or delete the installer.
Allow 2 GiB free before copying the installer. Python3.9+ is required.

The single ReConflict Options window contains graphics settings and the FPS
counter toggle. Save and Run writes the settings and starts the game; Cancel
leaves without changing settings.
D-pad or left stick selects, A confirms, B cancels. All saves and settings
remain inside the port folder. Keep data/, saves/, userdata/ and config/ when
upgrading. Existing desert-storm folder names are retained for compatibility.

See README.md for controls, manual import, credits and limitations.
Options, Cancel and Save and Run have been checked on H700. Complete fresh
installer extraction is verified locally; its full H700 test remains pending.

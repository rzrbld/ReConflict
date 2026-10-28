# ReConflict

A community ARM64 Linux port for **Conflict: Desert Storm**, built for the
Anbernic RG40XX-H (Allwinner H700) with PortMaster, SDL2 and OpenGL ES.

**Bring your own game.** Neither this repository nor the runtime package includes
the original Windows executable, levels, textures, music or other game assets.
Supported edition: **English GOG Windows 1.0, build 21657**.

[Buy Conflict: Desert Storm on GOG](https://www.gog.com/en/game/conflict_desert_storm).
Download the offline Windows installer from your GOG library; the supported
filename is listed below.

```text
Your GOG installer --innoextract--> verified original files in data/
                                                |
ReConflict ARM game + SDL2 + OpenGL ES <----------+
                      |
                 play on H700
```

The original x86 executable and mission DLL instructions are statically
translated, compiled for ARM64, and linked with native Windows/Direct3D8
compatibility code. This is binary recompilation; original game source was
not available. The player-supplied EXE and DLL files are still needed for their
data and image layout. Importing on the handheld does not compile the game or
execute the Windows installer.

## Install and play

Use the **ARM64 runtime package**, not GitHub's source-code ZIP.

1. Copy `reconflict/` from the package to `/mnt/sdcard/ports/reconflict/`.
2. Copy `ReConflict.sh` and `ReConflict Settings.sh` from the ZIP root to
   `/mnt/sdcard/ROMS/Ports/`.
3. Place your unmodified `setup_conflict_-_desert_storm_1.0_(21657).exe` beside
   `desert-storm.aarch64`. Allow **2 GiB free before copying the installer**;
   extraction needs at least 1.1 GiB free after copying it.
4. Open **ReConflict** in Ports. The SDL launcher extracts the required files
   and verifies all 297 checksums. Keep the device powered during import.
   Cancellation removes the current partial import and keeps your installer.
5. Choose **Keep installer** (the default) or **Delete installer** after import.
   Deleting it frees about 452 MiB; extracted data and saves remain intact.
6. The **ReConflict Options** window contains graphics settings, the
   **FPS counter** toggle and **Frame cap: 30 FPS / 60 FPS / Uncapped**.
   Choose **Save and Run** to save your choices and
   start the game, or **Cancel** to leave without changing settings. Later
   launches go straight to this same window with your saved settings after a
   quick installation check.
   An elapsed-time display stays live during import, including slow file operations.
   A simple **Game is starting / Please wait** modal remains visible during handoff; the
   first game startup can take longer while files are read from the SD card.

The launcher uses a resizable SDL window, a 640×480 logical layout and Vurus's
green/gold visual style. D-pad or left stick selects, **A** confirms, and **B**
cancels without saving. Mouse and keyboard also work on desktop Linux.

Requirements: **64-bit muOS**, PortMaster, SDL2/GLES and **Python 3.9+**. ARM64
innoextract and matching Boost libraries are bundled. Other firmware needs
separate testing. Existing-data verification, saved options/FPS preferences,
Save and Run, Cancel returning to muOS, and the startup message have been checked
on H700. The 30 FPS cap was also checked in-game with normal motion and audio;
standalone H700 pacing tests measured 30.00 and 60.00 FPS. Uncapped adds no
intentional delay. A complete fresh-installer import is **not yet verified end-to-end on
H700**; local extraction verifies all 297 files. Device import tests were
interrupted while testing the launcher.

Upgrades: keep `data/`, `saves/`, `userdata/` and `config/`. For an existing
`desert-storm/` installation, copy the contents of the package's `reconflict/`
into that existing folder instead. The launcher recognizes the old path. Once
ReConflict works, you may remove the older `Conflict Desert Storm.sh` menu entry.

Already have the Windows game installed? Instead of supplying an installer,
copy and validate its files on a PC with Python:

```sh
python3 scripts/import_game_data.py /path/to/installed/game /path/to/port/data
```

The destination must be absent or empty. Only the supported version is accepted;
saves/configuration are not imported. When using the copy of this script in the
runtime package, add `--manifest game-files.sha256` from the port directory.

## Controls and settings

SDL button names may differ from the handheld's printed letters; PortMaster
supplies its physical mapping. The game retains its classic layout:

| Control | Action |
|---|---|
| Left / right stick | Movement / camera |
| A | Action / menu accept |
| B | Stance |
| Y | Inventory / back in game menus |
| X | Go command |
| LB / RB | Orders / fire |
| LT / RT | Target left / right |
| Select / Start | Objectives / pause |
| Left / right stick click | Zoom / head toggle |
| D-pad | Squad selection and contextual menus |

Edit `controller-buttons.cfg` for button mappings, dead zones and trigger
thresholds. Reconnect handling is implemented; physical reconnect remains
untested. The launcher's **B to go back** is separate from the game's **Y**.

Recommended H700 settings: **640×480, lowest detail**, selected by default
on first setup and supplied as `graphics-low.cfg` for reference. Graphics, FPS counter and frame cap settings share the **ReConflict
Options** window. **Save and Run** applies them for that launch; **Cancel**
preserves the previous settings. The optional **ReConflict Settings** entry
opens the same window. Gameplay resolution remains fixed at 640×480.

The frame cap defaults to **60 FPS**. Choose **30 FPS** for a lower ceiling,
or **Uncapped** to remove the port's limiter and request VSync off. The driver
or game may still limit the actual rate. A cap cannot raise slow scenes to
the selected rate; it does not change the game's simulation or audio clocks.

All writable files stay beside the executable:

```text
ports/reconflict/
  desert-storm-launcher.aarch64   ReConflict frontend
  desert-storm.aarch64            translated ARM game
  tools/                         extractor, libraries and notices
  data/                          your original game files
  config/launcher.cfg             FPS counter and frame cap
  config/graphics.cfg             graphics settings
  config/registry.txt             local game registry
  controller-buttons.cfg         controller mapping
  saves/ userdata/ cache/ tmp/    game state
  log.txt install.log            launch/import diagnostics
```

## Status and troubleshooting

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

## Performance optimizations

The H700 build combines several targeted optimizations:

- **Native GLES rendering and GPU vertex processing:** translate Direct3D 8
  state into GLES, cache uploaded buffers, and reuse shader programs.
- **Dedicated render worker:** batch draw commands to reduce thread wakeups and
  let CPU game work overlap with graphics submission.
- **Specialized particle, fog and glow shaders:** recognize supported fixed
  function combinations and use smaller shaders. These paths improved the
  smoke-heavy scenes substantially; their pixel comparisons passed on Mali.
- **Native math helpers:** accelerate skinning, matrices, rotations, vector
  lengths and culling. Checked fast paths preserve the expected arithmetic
  order and guest state, with fallbacks for unsupported cases.
- **Bounded index scans and dispatch caching:** reduce repeated work when
  submitting geometry and resolving translated function calls.

These optimizations do not intentionally change AI update frequency, game time,
audio speed or particle counts. Lowest-detail game settings are a separate
choice. Large explosions can still be CPU-bound and drop below 20 FPS.

For comparisons, the PortMaster script accepts environment overrides including
`DS_RENDER_THREAD`, `DS_FAST_PARTICLES`, `DS_FAST_FOG`, `DS_FAST_GLOW`,
`DS_FAST_ROTATION`, `DS_FAST_LENGTH`, `DS_FAST_PHYSICS_PRODUCT` and
`DS_FAST_INDEX_SCAN` (`0` disables each). Defaults select the paths exercised
on the H700. Changing several at once makes performance comparisons difficult.

## Building and packaging

Build the SDL launcher on Linux with a C compiler, SDL2 development headers and
`pkg-config`:

```sh
bash scripts/build_port_launcher.sh
SDL_VIDEODRIVER=offscreen build/port-launcher/launcher-host \
  --root . --preview options --screenshot /tmp/reconflict-menu.bmp
python3 tests/prepare_game.py
```

### ARM toolchain and game build

Use Linux with Docker, Python 3, `venv`, `pip`, `binutils` and `innoextract`.
The public Dockerfile supplies GCC12/AArch64, SDL2 and GLES development files.
Its base image is pinned; Debian package updates can still change the resulting
toolchain. The original release used a retained local Bookworm/GCC12 image.

```sh
git clone https://github.com/rzrbld/ReConflict.git
cd ReConflict
python3 -m venv .venv
.venv/bin/pip install -r requirements-audit.txt
export PATH="$PWD/.venv/bin:$PATH"
export CROSS_IMAGE=reconflict-cross:bookworm
bash scripts/prepare_cross.sh

# Put your own supported GOG installer in the repository directory first.
innoextract --extract --output-dir 'setup_conflict_-_desert_storm_1.0_(21657)' \
  'setup_conflict_-_desert_storm_1.0_(21657).exe'
python3 scripts/import_game_data.py 'setup_conflict_-_desert_storm_1.0_(21657)' \
  --check-only
python3 scripts/translate_game.py --all-modules
bash scripts/build_translated.sh
bash scripts/build_game.sh
readelf -h build/game/desert-storm.aarch64
```

These commands describe the **baseline game build**, not an exact reproduction
of the optimized release. Translation creates private generated source locally;
never commit it. A full clean-checkout ARM game build remains under validation.
The public toolchain, standalone ARM launcher build, fresh translation of all
27 modules, and checked preprocessing have been exercised. Compilation and
gameplay of the resulting complete baseline are still unverified.

The optimized release additionally uses checked stages in `build/` and the
frozen translated archives. From that prepared development workspace, its
relink command is:

```sh
python3 scripts/stage_frame_limiter.py
DS_RT_PROXY_SOURCE=build/render-thread-stage/wake-batch/render_thread_proxy.c \
DS_RT_GEOMETRY_SOURCE=build/render-thread-stage/geometry-flags/geometry_fast.c \
DS_RT_PROFILER_SOURCE=build/dense-profiler/profiler.c \
DS_RT_SKINNING_SOURCE=probe/skinning_cached_pair.c \
DS_RT_RENDERER_SOURCE=build/index-scan/d3d8_renderer.c \
DS_RT_D3D8_SOURCE=build/frame-limit/d3d8.c \
DS_RT_ROTATION_STAGE=build/rotation-scalar \
DS_RT_LENGTH_STAGE=build/length-scalar \
DS_RT_PHYSICS_PRODUCT_STAGE=build/physics-product \
DS_RT_LOADING_STAGE=build/loading-background \
DS_RT_BUILD_LABEL=frame-limit \
bash scripts/build_render_thread_game.sh
```

The staging scripts retain strict source/object checks against that checkpoint;
they are not a portable clean-build pipeline yet. The checked local release
binary is provided for players who do not have the development checkpoint.
No GitHub CI or private game-input upload is required.

### Local package

Choose the game binary explicitly. Package files exclude original data and saves.

```sh
bash scripts/build_port_launcher.sh --cross
python3 scripts/fetch_innoextract.py
version="v$(date -u +%Y%m%d-%H%M%S)"
python3 scripts/package_port.py --binary /path/to/selected-game.aarch64 \
  --version "$version" --output "dist/reconflict-portmaster-$version.zip"
python3 scripts/audit_public_tree.py --export dist/reconflict-source.zip
```

Extractor preparation uses SHA-pinned Debian ARM64 packages and needs `dpkg-deb`
and network access on the build PC. It does not install system packages.
The handheld needs no compiler or network to import the game. `.gitignore`
excludes original files, generated game code, binaries, saves, captures and
internal development/agent documents. Packaging does not publish anything.
The package script expects the asset-free launcher preview at
`build/port-launcher/previews/reconflict-menu.png`; produce it with the host
launcher's `--preview options --screenshot` option and convert its BMP to PNG.

## Credits and licenses

ReConflict's original code is [MIT licensed](LICENSE). This does not license the
original game's code or assets; those remain with their respective owners.
ReConflict is an unofficial community project.
Development is co-authored by rzrbld and OpenAI Codex; Git commits retain the
`Co-authored-by: Codex <codex@openai.com>` credit.

- pcrecomp, sp00nz: static-recompilation runtime/tooling;
  [MIT license](third_party/pcrecomp/LICENSE).
- [ConflictDesertStormPatch](https://github.com/eSKylezZ/ConflictDesertStormPatch):
  controller support adaptation; [MIT notice](licenses/ConflictDesertStormPatch.txt).
- [innoextract](https://github.com/dscharrer/innoextract), Daniel Scharrer:
  unpacking the player's installer; **zlib license**, [notice](licenses/innoextract.txt).
  The launcher invokes this helper. Bundled Boost libraries retain their
  upstream/Debian notices in `tools/licenses/`.
- Vurus: first-run palette and modal/progress-bar visual design;
  [MIT notice](licenses/Vurus.txt). No original game artwork is used in the launcher.
- SDL2: native window, audio and GameController interfaces. Graphics use the
  device's OpenGL ES driver.

### Experimental portable GLES branch

`experiment/portable-gles` adds `DS_PORTABLE_GLES=1` to the optimized relink
command above. Use `DS_RT_BUILD_LABEL=portable-gles` for a separate output.
This removes the direct graphics-library link and resolves typed GLES calls
through SDL on the rendering thread. Missing entry points fail with a named
error. The default build and released package retain their original linkage.

Run `bash scripts/test_gles_dispatch.sh` for a Mesa/SDL pixel-readback check.
Inspect the candidate with `readelf -d`: no direct Mali/GLES/EGL dependency
should remain. SDL itself still needs a working firmware graphics stack.
Host smoke tests do not establish H700 performance or other firmware support;
keep the known-working release until hardware comparisons pass.

### Controller press preservation

The input adapter retains physical SDL button and trigger transitions
until the guest reads controller state. Each read consumes at most one transition
per logical button. Held buttons do not generate extra press edges. The bounded
queue stores eight transitions per button; overload drops oldest transitions.
Focus loss, unacquire and reconnect discard pending input. Automated input keeps
its existing independent sequencing.

For the retained optimized checkpoint, add `DS_INPUT_EDGE_FIX=1` and
`DS_RT_BUILD_LABEL=input-edges` to the relink command above. The optimized relink enables this fix by default; use `DS_INPUT_EDGE_FIX=0`
only for comparison with the old adapter. Run `bash scripts/test_input_audio.sh` for the isolated
DirectInput/SDL regression tests. The player confirmed improved firing on H700 with the stable-renderer test build.
Full weapon coverage and portable-GLES gameplay validation remain pending.
This addresses lost short taps, not weapon-specific readiness or reload rules.

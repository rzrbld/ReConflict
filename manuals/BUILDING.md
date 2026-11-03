# Building and packaging

[Back to the player guide](../README.md)

Run all commands from the repository root. Players should use a runtime release;
these instructions are for development. Your own supported game files are required.


Build the SDL launcher on Linux with C and C++17 compilers, SDL2 development headers and
`pkg-config`:

```sh
bash scripts/build_port_launcher.sh
SDL_VIDEODRIVER=offscreen build/port-launcher/launcher-host \
  --root . --preview options --screenshot /tmp/reconflict-menu.bmp
bash scripts/test_native_import.sh
bash scripts/test_port_launcher.sh
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
  --version "$version" --output "dist/reconflict-portmaster-$version.zip" \
  --screenshot /path/to/gameplay.png --submission-dir build/portmaster-stage
python3 scripts/audit_public_tree.py --export dist/reconflict-source.zip
```

Extractor preparation uses SHA-pinned Debian ARM64 packages and needs `dpkg-deb`
and network access on the build PC. It does not install system packages.
The handheld needs no compiler or network to import the game. `.gitignore`
excludes original files, generated game code, binaries, saves, captures and
internal development/agent documents. Packaging does not publish anything.
Pass an actual gameplay PNG with `--screenshot PATH`: 4:3, at least 640x480.
Do not use the launcher preview or extracted game artwork. Images stay outside
this source repository. `--submission-dir DIR` additionally stages
`DIR/ports/reconflict/` for PortMaster review. Validate a built ZIP with
`python3 scripts/check_port_package.py PATH.zip`.
Submission still needs documented community/firmware testing; see
[the packaging audit](PORTMASTER.md).


## Native importer

`bash scripts/build_port_launcher.sh` also builds the native C++17 importer.
Use `--cross` for both ARM binaries. Outputs are
`build/native-import/prepare-game.aarch64` and
`build/port-launcher/desert-storm-launcher.aarch64`. The importer statically links
the C++ support libraries and uses the firmware libc (glibc 2.36 or newer for
the supplied importer; the tested H700 firmware has 2.38). The runtime ZIP contains
no Python scripts and needs no Python interpreter. Development/translation and
packaging tools may still use Python on the build PC.

The importer implements streaming SHA-256, locked transactional extraction,
progress/status files, cancellation and explicit installer deletion. The native
extractor remains the bundled innoextract executable. `--source DIR` imports an
installed game folder into the port's `data/`; `--check-only` verifies it without
copying. Tests use synthetic data and a test-only build; checksum overrides are
not compiled into the distributed binary.

## Portable GLES build

For the prepared optimized checkpoint, add `DS_PORTABLE_GLES=1` and
`DS_RT_BUILD_LABEL=portable-gles` to the relink command above. Run
`bash scripts/test_gles_dispatch.sh` to check Mesa/SDL pixel readback and context
transfer to a worker. Check `readelf -d` for no direct Mali/GLES/EGL dependencies.
Hardware gameplay still needs validation.

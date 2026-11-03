# Building and packaging

[Back to the player guide](../README.md)

A Linux PC can build the ARM64 package from this repository and the player's
**English GOG Windows 1.0 build21657** installer or extracted game directory.
Generated game code stays local. Do not commit it or upload original game files.

## One-command build

Install Docker, a host C compiler (`cc`), Python 3 with venv/pip support, binutils, `innoextract`, `patch`
and `dpkg-deb`. Docker must be usable by your account. The build downloads pinned
Python dependencies and extractor packages; the cross-toolchain uses the public
Dockerfile. Its Debian base is pinned, but package updates may change binary hashes.
Allow roughly 10 GiB free disk space. `JOBS` controls parallel compilation
(default 4); reduce it on machines with limited RAM.

```sh
git clone https://github.com/rzrbld/ReConflict.git
cd ReConflict
JOBS=4 bash scripts/build_distribution.sh \
  '/path/to/setup_conflict_-_desert_storm_1.0_(21657).exe' \
  "$PWD/dist/reconflict.zip"
```

An extracted installed-game directory can replace the installer argument. Inputs
are verified and copied; the original files are never patched or deleted.
Use a fresh checkout for a full build. The script deliberately rejects an existing
optimized snapshot rather than silently mixing new source with stale objects.

For the portable-GLES variant, clone with `--branch experiment/portable-gles`.
That branch selects SDL-loaded GLES automatically. Main targets the Mali SONAME;
it builds a **link-only** GLES ABI stub from the public SDK. The stub is never
shipped: the handheld must provide the real `libmali.so.0` driver.

The optional third argument is a real gameplay PNG (4:3, at least 640x480).
Without it, the script downloads a SHA-pinned earlier release archive and extracts
**only its catalogue screenshot**. No executable, library or generated game code
is reused from a release. The screenshot remains outside the source repository.
You can provide your own screenshot to avoid this metadata download.

Outputs:

- The requested ZIP and adjacent `.sha256` file.
- `build/game-inputs.sha256`: checksums of your verified game files.
- `build/translated/`: all locally generated modules, objects and archives.
- `build/render-thread-game/variants/clean-release/`: optimized ARM executable,
  ELF dependency report and checksums of its generated inputs.
- `build/port-launcher/` and `build/native-import/`: native launcher and importer.

`VERSION=vYYYYMMDD-HHMMSS` optionally sets the package version; otherwise UTC date
and time are used. The package excludes game inputs, saves and the link-only stub.
No GitHub CI or private input upload is needed.

## Your own checksum reports

```sh
python3 scripts/hash_game_inputs.py /path/to/installed/game \
  --output my-game-files.sha256
python3 scripts/hash_game_inputs.py '/path/to/setup_conflict_-_desert_storm_1.0_(21657).exe' \
  --output my-installer.sha256
```

Reports do not overwrite existing files or the trusted manifest. A mismatch exits
with an error. Do not replace source guards with a different version's hashes:
patch addresses and instruction sequences must be ported and verified first.
Compiled-object hashes are generated locally; optimization staging checks that
snapshot objects match those locally built objects, while retaining strict
checks of original game bytes and transformed source routines.

## Individual stages and debugging

After extraction, the expected working input directory is
`setup_conflict_-_desert_storm_1.0_(21657)/` at repository root. Activate the virtual
environment created by the full build and use the public toolchain:

```sh
export PATH="$PWD/.venv/bin:$PATH"
export CROSS_IMAGE=reconflict-cross:bookworm
bash scripts/prepare_cross.sh
python3 scripts/translate_game.py --all-modules
JOBS=4 bash scripts/build_translated.sh
bash scripts/build_game.sh
bash scripts/build_optimized.sh
bash scripts/build_port_launcher.sh --cross
```

`build_optimized.sh` regenerates the render worker, exact shaders, index scan,
geometry/rotation/length/physics optimizations, loading-screen retention and frame
limiter in dependency order. It snapshots the newly compiled baseline rather
than requiring archives from a developer's machine. It does not modify that
baseline executable. If baseline inputs change after snapshotting, use a fresh
checkout instead of keeping the old snapshot.

The native C/SDL2 frontend and C++17 importer need no Python on the handheld.
The supplied importer statically links its C++ support and requires glibc 2.36+.
For host builds/tests, install a C/C++ compiler, SDL2 development headers and
`pkg-config`, then run:

```sh
bash scripts/build_port_launcher.sh
bash scripts/test_native_import.sh
bash scripts/test_port_launcher.sh
bash scripts/test_rotation_scalar.sh
bash scripts/test_length_scalar.sh
bash scripts/test_physics_product.sh
```

## Package and submission staging

```sh
python3 scripts/fetch_innoextract.py
python3 scripts/package_port.py \
  --binary build/render-thread-game/variants/clean-release/game-render-thread.aarch64 \
  --version "v$(date -u +%Y%m%d-%H%M%S)" --output dist/reconflict.zip \
  --screenshot /path/to/gameplay.png --submission-dir build/portmaster-stage
python3 scripts/check_port_package.py dist/reconflict.zip
python3 tests/portmaster_package.py dist/reconflict.zip
python3 scripts/audit_public_tree.py --export dist/reconflict-source.zip
```

Submission staging produces `build/portmaster-stage/ports/reconflict/`.
[PortMaster community testing](PORTMASTER.md) is still required. Build success
and host numerical tests do not prove on-device rendering, timing or performance.

## Verification record (2026-11-03)

Two isolated source exports regenerated all 27 game modules and compiled 275 C
units without developer archives. All 27 translated archives matched between
those builds and the previously tested release inputs. The second run completed
`build_distribution.sh` from the original installer through validated ZIP output.
Both Mali and portable-GLES optimized variants linked successfully. Host
rotation/length/physics differential tests, 17 importer tests, eight SDL launcher
flows, and Mesa GLES pixel/context-transfer tests passed. These are build/host
tests; new ARM executables still need hardware gameplay validation.

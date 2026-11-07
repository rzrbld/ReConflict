# ReConflict

Play **Conflict: Desert Storm** on ARM64 handhelds through PortMaster. Tested on
the **Anbernic RG40XX-H (H700, 64-bit muOS, Mali driver)** and the **Anbernic RG DS
(RK3568, BananaOS, Mesa Panfrost driver)**; other firmware and devices are not yet verified.

**You need your own game.** [Buy it on GOG](https://www.gog.com/en/game/conflict_desert_storm)
and download the English offline Windows installer:

`setup_conflict_-_desert_storm_1.0_(21657).exe`

ReConflict runs translated game code on ARM using your original game files.
The download contains no original Windows executable or game assets.

Thanks to **Pivotal Games**, the original developer, for creating Conflict: Desert Storm.

## Download

Download the **[runtime ZIP](https://github.com/rzrbld/ReConflict/releases/tag/v20261107-160424)**:
the `reconflict.zip` asset, **not** GitHub's “Source code” archive. It is a
prerelease, tested on H700 with 64-bit muOS and on RG DS with BananaOS.

## Install and play

You need 64-bit PortMaster firmware with glibc 2.36 or newer, SDL2/SDL2_ttf,
and **2 GiB free before copying the installer**. Graphics use the firmware's
OpenGL ES 2.0 driver through SDL (for example Mali or Panfrost); no driver is bundled.
The launcher and importer are native binaries; **Python is not needed on the handheld**.
The extractor is included; no PC extraction is required by the
launcher. A complete fresh import has not yet been verified end-to-end on H700.

On other firmware, use its configured `ports` directory for the folder and scripts.
The following paths are specific to muOS:

1. Extract the runtime ZIP on your PC.
2. Copy its `reconflict` folder into `/mnt/sdcard/ports/`.
3. Copy `ReConflict.sh` and `ReConflict Settings.sh` into `/mnt/sdcard/ROMS/Ports/`.
4. Put the GOG installer inside `reconflict`, beside `desert-storm.aarch64`.
5. Open **ReConflict** from Ports. Wait for extraction and verification; elapsed
   time stays visible during slow file operations. Keep the handheld powered.
6. Choose whether to keep or delete the installer. Your extracted game stays.
7. Choose your settings, then **Save and Run**. **Cancel** returns without saving.

Later launches open the same options screen with your saved choices. The
**ReConflict Settings** entry opens that screen too. In the launcher, use the
D-pad to select, left/right to change, **A** to confirm and **B** to cancel.

## Settings and performance

Start with **640×480 and lowest detail**. The launcher includes graphics options,
an **FPS counter**, and **30 FPS / 60 FPS / Uncapped** limits (default: 60).
A frame cap limits fast scenes; it cannot speed up slow ones.
**Texture quality: Low / High** sets texture resolution. **Surface detail: Off / On**
is a separate detail-texture effect; these are independent settings.

Expect roughly **20–30 FPS in busy gameplay**, with higher rates in light scenes.
Large explosions can briefly dip into the high teens. Performance varies, and the
whole game has not been verified. The controller update improves missed quick
fire-button presses.

**The GPU driver matters.** The game draws with many small draw calls (menus and
some cutscenes several hundred per frame). The Mali driver on H700 handles them
cheaply. Mesa's Panfrost driver (for example on RG DS / RK3568) spends far more
CPU time per call: measured on RG DS, about 20 µs per draw and about 160 µs per
partial buffer update, roughly ten times the H700's cost for the same scene. So on
Panfrost devices **gameplay can run faster than on H700** (35–48 FPS on RG DS),
while **the main menu and some cutscenes drop to about 20 FPS** even though the
CPU and GPU are faster. The frame cap does not change this. Reducing these driver
calls is planned.

The picture scales to any screen size with the game's 4:3 aspect and black bars
(for example 480×320, 720×720 or 1280×720 panels).

## In-game controls

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


Button names follow SDL/PortMaster mapping and may differ from printed labels.
The launcher uses **B** for Cancel; in-game menus use **Y** for Back.

## Updating and saves

Quit the game and back up your port folder before updating. Replace runtime
files, but **keep `data/`, `saves/`, `userdata/` and `config/`**. All saves and
settings stay beside the executable.

If your existing folder is called `desert-storm`, copy the contents of the ZIP's
`reconflict` folder into it instead. The launcher supports this older folder name.

## Help and further reading

- [Troubleshooting and manual data import](manuals/TROUBLESHOOTING.md)
- [Build and package instructions](manuals/BUILDING.md)
- [How the port works, compatibility and optimizations](manuals/TECHNICAL.md)
- [Credits and licenses](manuals/CREDITS.md)

Unofficial community port by rzrbld, co-authored with OpenAI Codex. Original port
code is [MIT licensed](LICENSE); the original game remains its owners' property.

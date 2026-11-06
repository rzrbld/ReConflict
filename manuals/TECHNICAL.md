# Technical notes

[Back to the player guide](../README.md)

## How it works

```text
Your GOG installer -> extraction and verification -> original files in data/
                                                           |
Translated ARM game + SDL2 + OpenGL ES <--------------------+
```

The x86 executable and mission DLL instructions are statically translated and
compiled for ARM64, with native Windows/Direct3D8 compatibility code. Original
source was unavailable. Your EXE/DLL files are still required for data and image
layout. Handheld import neither compiles the game nor executes the installer.

## Firmware compatibility

Only ARM64 muOS on RG40XX-H has been exercised in gameplay. The executable links
SDL2 and SDL2_ttf and resolves every OpenGL ES function through the current SDL
context (`runtime/gles_dispatch.c`); it has no direct Mali, GLES or EGL library
dependency. It requests GLES 2.0 with depth/stencil support, so the firmware's
SDL must provide a working GLES driver (for example Mali or Panfrost). On H700 it
runs at the same speed as the earlier build that linked `libmali.so.0` directly.
Import and verification use the bundled native helper; no Python is needed at runtime.
The package conservatively requires glibc 2.36. ARM32/x86 need separate builds.
Other firmware and GPU drivers are not yet verified. Do not transplant GPU drivers
between firmware. PortMaster availability alone does not establish compatibility.

## Optimizations


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
- **Pointer-window x87 stack:** translated floating-point code keeps the eight
  x87 slots as a window into a larger buffer, so each push/pop moves a pointer
  instead of copying all eight values and their integer shadows. Results are
  bit-identical to the previous model. On H700, the two-truck explosion in
  Rescue averaged about 17% higher FPS during the blast (16.2 to 18.9) and 7%
  over 12 seconds (20.8 to 22.3), across three paired runs.
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


## Controller press preservation


The input adapter retains physical SDL button and trigger transitions
until the guest reads controller state. Each read consumes at most one transition
per logical button. Held buttons do not generate extra press edges. The bounded
queue stores eight transitions per button; overload drops oldest transitions.
Focus loss, unacquire and reconnect discard pending input. Automated input keeps
its existing independent sequencing.

For the retained optimized checkpoint, add `DS_INPUT_EDGE_FIX=1` and
`DS_RT_BUILD_LABEL=input-edges` to the relink command in [Building](BUILDING.md). The optimized relink enables this fix by default; use `DS_INPUT_EDGE_FIX=0`
only for comparison with the old adapter. Run `bash scripts/test_input_audio.sh` for the isolated
DirectInput/SDL regression tests. The player confirmed improved firing on H700 with the stable-renderer test build.
Full weapon coverage validation remains pending.
This addresses lost short taps, not weapon-specific readiness or reload rules.

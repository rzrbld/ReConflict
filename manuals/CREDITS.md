# Credits and licenses

[Back to the player guide](../README.md)


ReConflict's original code is [MIT licensed](../LICENSE). This does not license the
original game's code or assets; those remain with their respective owners.
ReConflict is an unofficial community project.
Development is co-authored by rzrbld and OpenAI Codex; Git commits retain the
`Co-authored-by: Codex <codex@openai.com>` credit.

- pcrecomp, sp00nz: static-recompilation runtime/tooling;
  [MIT license](https://github.com/rzrbld/ReConflict/blob/main/third_party/pcrecomp/LICENSE).
- [ConflictDesertStormPatch](https://github.com/eSKylezZ/ConflictDesertStormPatch):
  controller support adaptation; [MIT notice](https://github.com/rzrbld/ReConflict/blob/main/licenses/ConflictDesertStormPatch.txt).
- [innoextract](https://github.com/dscharrer/innoextract), Daniel Scharrer:
  unpacking the player's installer; **zlib license**, [notice](https://github.com/rzrbld/ReConflict/blob/main/licenses/innoextract.txt).
  The launcher invokes this helper. Bundled Boost libraries retain their
  upstream/Debian notices in `tools/licenses/`.
- Vurus: first-run palette and modal/progress-bar visual design;
  [MIT notice](https://github.com/rzrbld/ReConflict/blob/main/licenses/Vurus.txt). No original game artwork is used in the launcher.
- SDL2: native window, audio and GameController interfaces. Graphics use the
  device's OpenGL ES driver.


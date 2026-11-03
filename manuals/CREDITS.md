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
  upstream/Debian notices in `licenses/`.
- Vurus: first-run palette and modal/progress-bar visual design;
  [MIT notice](https://github.com/rzrbld/ReConflict/blob/main/licenses/Vurus.txt). No original game artwork is used in the launcher.
- SDL2: native window, audio and GameController interfaces. Graphics use the
  device's OpenGL ES driver.


The native importer includes GCC libstdc++/libgcc support under GPLv3 with the
GCC Runtime Library Exception; the package includes the Debian notices, exception
and GPLv3 text. SDL2 and SDL2_ttf are firmware dependencies; their notices are
included too. Gameplay catalogue screenshots depict the original game and are
not covered by the port's MIT license. No original asset files are bundled.

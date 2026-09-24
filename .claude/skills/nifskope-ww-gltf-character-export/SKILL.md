---
name: nifskope-ww-gltf-character-export
description: Export a Fallout 4 character out of NifSkope WW as glTF that Blender opens as ONE armature -- the skeleton option, the whole-joint-list option, game units, multi-part bodies, the helper-bone rule, texture copying, root motion, the clip the viewer is playing, and the round trip back through gltf-import. Use whenever a NifSkope WW glTF export, its options dialog, its CLI flags, or its gate is touched, and whenever someone reports loose empties or a body that does not animate in Blender.
---

# NifSkope WW: exporting a character as glTF

Two exporters live in this tree and neither replaces the other:

* `src/lib/importex/gltf.cpp` -- upstream, tiny_gltf, static scene, menu only.
* `src/gltfexport*.cpp` -- the WW one, QtCore only, CLI + dialog. This is the one with options.

Both agree that **1 NIF unit = 0.9144/64 m** and both put the scene under a Z-up -> Y-up root.
The constants are `GLTF_METRES_PER_UNIT` and `GLTF_UNITS_PER_METRE` in `src/gltfexportopts.h`;
the runtime field is `GltfExportScene::unitScale`.

## The defaults are RULED, and there is one way back

bungo ruled them on **2026-09-19 09:45**: the export ships **Blender-ready** --
`skeleton auto`, `joints whole`, `units game`, `animation on`, `bones body only`,
`textures copy`. He chose **game units over the metres recommendation**; do not
re-argue it. Parts, root motion and both body-build fields were not part of the
ruling and stay where they were (`parts none`, `root motion strip`, build off).

`--legacy-defaults` is the **only** way back to the pre-ruling export. It is
**CLI only** -- by his instruction there is no dialog row for it. It moves the
six ruled fields and nothing else, and a flag after it wins, so
`--legacy-defaults --units game` is sayable and means what it says.
`gltfExportLegacyOptions()` is that struct and `gltfExportOptionsAreLegacy()`
is the predicate; `gltfExportCharacter()` still takes the OLD code path
unchanged when the predicate holds, which is what keeps the old bytes provable.

**`auto` finding nothing is not an error.** Most files are not characters. The
search uses paths derived from the NIF, and reaches for the installed game's
skeleton **only when the NIF's own path is under `CharacterAssets` or
`Actors/Character`** -- otherwise a default would hang a full human rig on an
alarm clock. A miss falls back to the file's own nodes, sets
`charReport.skeletonAutoMissed`, prints a note and the MEASURED key
`skeleton_auto_missed=1`, and exits 0. An explicit `--skeleton PATH` that cannot
be read still fails: there the caller named a file and is owed the truth.
The heuristic's refuter: a character mesh kept outside `Actors/Character` gets
no skeleton, and one log line saying so.

## One options struct, two front doors

`GltfExportOptions` (`src/gltfexportopts.h`) is the only description of an export.
The dialog (`src/gltfexportdialog.cpp`) and the CLI arg loop (`src/nifcli.cpp`, `cmdGltf`)
both produce one, and `gltfExportOptionsSummary()` prints it. **Never add an option to one
door only** -- the dialog gate (`WW_GLTF_EXPORT_DIALOG`) asserts row-for-flag equality and
will fail the moment they drift.

## The options, and why each exists

| option | what it repairs |
|---|---|
| `--skeleton PATH` | the body nif has a FLAT `*_skin` bone list and no hierarchy, so a clip drives 13 of 95 tracks. Hang the flat list on `skeleton.nif` and it drives **78**. |
| `--joints whole` | with weighted bones only, Blender builds an armature plus **25 loose empties** (Camera, AnimObject*, the BASE nodes). One joint list shared by every skin gives **one armature, zero empties**. |
| `--units game` | 1 glTF unit = 1 NIF unit, PyNifly-compatible. Writes `asset.extras.metresPerUnit = 1`. |
| `--part NIF` | hands / head / eyes / mouth onto the same skeleton, one skin each, all sharing the joint list. |
| `--bones-only body` | drops the **15** helpers (Camera, Camera Control, CamTarget*, CharacterBumper, AnimObject*, Weapon*) -- 132 nodes -> 117. |
| `--textures copy` | the referenced .dds beside the file with RELATIVE URIs. A PNG row is offered only if a converter exists in the tree; today none does. |
| `--root-motion-mode` | `root` keeps the travel on the bone, `object` bakes it onto the mesh object, `strip` removes it. |
| `--body-build`, `--body-build-cycle` | see `fo4-body-build-bone-scales`. Needs `--data-root <the Fallout4.esm folder>`; the esm is in the GAME install, not the unpacked corpus. |

**17 tracks can never be matched.** `WeaponBolt`, `WeaponTrigger`, `WeaponMagazine*`,
`WeaponOptics*`, `WeaponIKTarget*` live in `skeleton.hkx` and in a WEAPON's nif, and in no
character `skeleton.nif`. 78 of 95 is the ceiling; inventing nodes for them is inventing
transforms. Say the ceiling, never round it up to 95.

## The menu export carries the viewer's clip

`gltfExportSetClipProvider()` (`src/gltfexportnif.cpp`) had no caller for its whole life,
which is why File > Export > glTF never carried the animation being played. If a menu export
loses the clip again, look there first.

## Defaults, before the ruling

Until 2026-09-19 09:45 every toggle shipped at the value that reproduced the previous output
byte for byte. That is still how a NEW option arrives: ship it inert, measure it, hand bungo a
table of `option | what it does | today | recommended`, and move the default only when he
answers. An owed ruling never ships as a default; the ruling above is what an answer looks
like, and `gltfExportOptionRecommendation()` keeps both columns beside the code.

## The gate

`bash tests/spells/gltf_export_options.sh` -- 17 rows, each with a floor:

* R0 byte identity through `--legacy-defaults` against `release/NifSkope.before_<lane>.exe`,
  floor = one more flag changes the bytes. Every other row runs from `--legacy-defaults` too,
  so a row measures ONE option and not the sum of six
* R1..R7 one measured row per option, R3 the units ratio measured off a named bone
* R8 the clip (13 vs 78 tracks), R9 a WRONG skeleton must NAME the bones it lacks
* R10 `tests/spells/gltf_check.py` (there is **no Khronos glTF-Validator on this machine**; say so)
* R11 headless Blender via `tests/spells/gltf_char_blender.py`, floor = today's export FAILS it
* R12 round trip `gltf -> gltf-import -> hkx` compared with `hkx-tsv` at both ends, in BOTH unit
  modes; floor = delete `asset.extras.metresPerUnit` from the game-unit file and it fails by 11245 units
* R13 SCALE channels written non-uniform and read back; floor = a jog clip has none
* R14 the RULED DEFAULTS, no option flag at all: one armature, zero empties, game units, the
  clip, 117 nodes, a copied .dds -- the only row allowed to be silent about its flags
* R15 a STATIC (an alarm clock): `auto` finds nothing, falls back, says so, exits 0, and does
  NOT come out wearing a human skeleton; floor = the body on the same machine DOES find one
* R16 skeleton + `bones body` + a clip: the helper's TRACKS go with the helper. The clip is
  built BEFORE `dropHelpers()`, so a dropped Camera/AnimObject/Weapon node used to leave a
  channel pointing at node -1 and our own validator refused the file. 13 tracks are dropped
  and NAMED; floor = `release/NifSkope.before_gltfdefaults.exe` still refuses the command

`tests/spells/gltf_measure.py` reads the WRITTEN file independently of the exporter's own
`MEASURED key=value` line, so a bookkeeping bug cannot pass itself off as an export. Use both.

## Traps

* The CLI needs `-no-gui` and **absolute `E:/...` paths**; relative ones fail with
  "cannot write ...bin: The system cannot find the path specified."
* `make` needs `export PATH="$PATH:/e/Tools/GIT/cmd"` or the link dies `Error 127: git: command not found`.
* A red control that "passes" because the wrong skeleton silently contributed the body's own
  bones is not a control. Guard on `haveBaseline` so a `skeleton: none` export does not report
  all of its own bones missing.
* The DIALOG must be set from a default-constructed `GltfExportOptions` at the end of its
  constructor. Its combo boxes are built at index 0, which was the default until the ruling
  and silently stopped being it -- a control built at a literal index is a second copy of the
  default. It reads no QSettings: a harness forces the state it measures.
* One row per option proves the options, never the product. When a DEFAULT moves, the row
  that measures it is the one that runs with no flags at all (R14) -- that row is what found
  the -1 channel, after sixteen green single-option rows.

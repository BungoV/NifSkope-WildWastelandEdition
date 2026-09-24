# GLTFEXPORT1 -- the defaults flip, measured (2026-09-19 13:34 CEDT)

## What was run, against which exe

Lane IMPOSTORSHOW's build carried the flip: `release/NifSkope.exe`
23,342,080 B, 13:09:26, sha1 `e3a59b973c67215e9da6ec43716879550b8bbb1a`.

Staleness sweep, all ten of my files, before a single gate ran: every source
09:44:38 -- 09:50:10, every object 10:01:00 -- 10:41:46, the exe 13:09:26.
Every object newer than its source, the exe newer than every object.
**0 discrepancies.**

## The two defects the flip exposed, and the two builds

R14 -- the row that asks what a person gets with NO flag -- failed on the first
run, and it failed by refusing to export at all:

```
gltf: animation 0 channel 27 drives node -1, outside 0..115
```

Bisected on the exe itself, three commands: `--bones-only body` alone exports,
`--skeleton auto` alone exports, the two together with a clip do not. `skeleton
auto` supplies the Camera / AnimObject / Weapon nodes, `bones body` drops them,
and `dropHelpers()` remapped the clip's channels onto the dropped nodes as -1.
Our own validator caught it, which is the validator working.

* **Repair 1** `src/gltfexportchar.cpp`, `dropHelpers()`: a channel whose node
  is dropped is dropped WITH it, counted in `cr.helperTracksDropped`, and named
  in the report; an animation left with no channels goes too, and says so. The
  stale comment above the function ("the clip is written after this") was
  rewritten -- the clip is built ten lines earlier.
  `src/gltfexportchar.h`: one new field.
* **Repair 2** `src/gltfexportdialog.cpp`: the constructor now ends in
  `setOptions( GltfExportOptions() )`. The combo boxes were built at index 0,
  which had been the default and silently stopped being it; the dialog was
  still offering the pre-ruling export. The in-app harness named it in three
  checks. No QSettings is read -- the dialog states the struct and nothing else.

Rung before touching anything: `release/NifSkope.before_gltfdefaults.exe`
= a byte copy of IMPOSTORSHOW's 13:09 exe, sha1
`e3a59b973c67215e9da6ec43716879550b8bbb1a`. No other rung was touched.

Two builds followed (13:28:33, then 13:32:07 after repair 2). Final exe:
`release/NifSkope.exe` **23,346,176 B, 13:32:07, sha1
`88d6abb32dfda556c2144ef912575db1862d794e`**.

## The counts

| gate | result |
|---|---|
| `tests/spells/gltf_export_options.sh` | **R0-R16, 62 PASS, 0 row(s) not as registered** |
| `tests/spells/body_build.sh` | **B1-B6, 9 PASS, 0 row(s) not as registered**, in-app 31 checks 0 failures |
| in-app `WW_GLTF_EXPORT_DIALOG` | **25 checks, 0 failed, PASS** |

R0 still holds: `--legacy-defaults` reproduces
`release/NifSkope.before_gltfexport1.exe` byte for byte, .gltf and .bin, with
the floor (one more flag) changing the bytes.

R14, the ruled defaults with no option flag at all, measured:

```
legacy_defaults=0  skeleton_nodes=129  skeleton_auto_missed=0
joints_identical=yes  metres_per_unit=1  helpers_dropped=15  nodes=117
animations=1  tracks_matched=78  textures_copied=1
BLENDER objects 3 armatures 1 empties 0 meshes 2
BLENDER armature NifSkope_Y_up bones 115
BLENDER move_max 0.075331 (frame 0 -> 20)   move_floor_same_frame 0.000000000
```

R15, the alarm clock: rc 0, `skeleton_auto_missed=1`, `skeleton_nodes=0`,
3 nodes, and the floor (the body on the same machine) finds one.

**R16 is new and is the repair's row**: `--skeleton auto --bones-only body`
with a clip exports, drops 13 tracks (`WeaponLeft, WEAPON, Camera, Camera
Control, AnimObjectA..R3, CamTarget`) and NAMES them, keeps 117 nodes and the
clip. Its floor is the rung `NifSkope.before_gltfdefaults.exe`, which refuses
the same command with `drives node -1` -- a floor that was watched failing.

## Files

Written by me this round: `src/gltfexportchar.cpp`, `src/gltfexportchar.h`,
`src/gltfexportdialog.cpp`, `tests/spells/gltf_export_options.sh`.
Shared file touched: **none this round** (the two MEASURED keys in
`src/nifcli.cpp` were the earlier landing). `MISTAKES.md` +11 entries, byte
splice, CRLF 9470 -> 9578, bare LF 0 before and after.
No impostor*/cellview*/cellpick*/harnesswindow* file was opened or edited.

## Owed

Root motion stays `Strip`. bungo's ruling text says "keep-on-bone ... stay as
they are" and those two clauses disagree; the default has not been moved and
will not be without his word. It is a one-line change plus one row when it comes.

---

# For the director -- WW_CHANGES.md

**glTF export: the defaults are the ones bungo ruled.** A glTF export now ships
Blender-ready: it finds the character's `skeleton.nif` by itself, gives every
part of the body one shared joint list, writes game units (1 glTF unit = 1 NIF
unit), carries the clip the viewer is playing, drops the 15 camera and
animation-object helpers, and copies the .dds files beside the exported file.
Opened in Blender that is one armature and no loose empties, where before it was
an armature plus 25 empties that did not animate.

`--legacy-defaults` on the command line puts every one of those six back the way
it was, and a flag after it still wins, so "the old export but in game units" is
one line. There is no dialog row for it, by instruction.

A file that is not a character is unaffected: the skeleton search only reaches
for the game's rig for meshes under `Actors/Character`, and when it finds
nothing it says so in one line and exports the file's own nodes. It never fails
for that reason.

Two repairs went with the flip. A body exported with a skeleton AND with the
helper bones dropped used to produce a file our own validator refused, because
the clip still drove the bones that had just been removed; those tracks are now
removed with them and named in the log. And the export options window itself was
still opening on the old values while the command line had moved -- it now opens
on the same defaults everything else uses.

# For the director -- HANDOFF.md

GLTFEXPORT1 closed 2026-09-19 13:34. The ruled defaults (skeleton auto, joints
whole, units game, animation on, bones body, textures copy) are live in
`GltfExportOptions`; `--legacy-defaults` is the CLI-only way back and R0 proves
it byte-identical to `release/NifSkope.before_gltfexport1.exe`.

Exe: `release/NifSkope.exe` 23,346,176 B, 13:32:07, sha1
`88d6abb32dfda556c2144ef912575db1862d794e`. Rung for this lane:
`release/NifSkope.before_gltfdefaults.exe` (IMPOSTORSHOW's 13:09 exe, sha1
`e3a59b97...`). **His open NifSkope window needs a restart to pick it up.**

Green: `gltf_export_options.sh` R0-R16 (62 PASS), `body_build.sh` B1-B6 (in-app
31/0), `WW_GLTF_EXPORT_DIALOG` 25/0. Two defects were found BY the flip and
repaired, both with a floor watched failing: clip channels pointing at dropped
helper bones (new row R16, floor = the rung still refuses it), and the options
dialog opening on index 0 instead of on the struct's defaults.

OWED, not closed: root motion. The ruling says "keep-on-bone ... stay as they
are"; the code default is and remains `Strip`. One line plus one row when he
rules. Nothing else in this lane is waiting on anyone.

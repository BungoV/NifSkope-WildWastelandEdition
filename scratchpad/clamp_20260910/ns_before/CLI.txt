# NifSkope — headless CLI

Batch mode for the same binary: `NifSkope.exe -no-gui <command>`. Fills the
`// Future command line batch tools here` slot upstream left in `main.cpp`.
Implementation: `src/nifcli.cpp`.

On Windows use the wrapper — see **Invocation** below for why:

```powershell
release\nifskope-cli.cmd info model.nif
```

## Commands

```
spells [pattern]                        list spells addressable by name
info <file>                             version, block count, per-type tally
list <file> [-t <type>]                 block list, optionally filtered
world <file> [-b N] [-t <type>]         each NiAVObject's WORLD transform
dump <file> -b N [-f PATH] [-d DEPTH] [-n MAX] [--all]
get  <file> -b N -f PATH
skeleton <file>                         skeleton tree + per-bone influence
skeleton <file> --validate              findings only; exit 1 if any fire
set  <file> -b N -f PATH -v VALUE -o OUT
cast <file> -s "Page/Name" [-b N] [-f PATH] -o OUT

anim-setup <file> -b N --list
anim-setup <file> -b N --controller TYPE [--controller TYPE ...]
      [--sequence NAME] [--new-sequence] [--standalone]
      [--effect-var 0..9] [--int-var N] -o OUT
```

Field paths are `/`-separated; a numeric segment indexes an array by row:

```powershell
nifskope-cli get model.nif -b 73 -f "Num Vertices"
nifskope-cli get model.nif -b 77 -f "Bone List/0/Bounding Sphere/Radius"
nifskope-cli dump model.nif -b 73 -f "Vertex Data/0"
```

`dump` hides rows this file's version/conditions exclude, exactly as the GUI's
row hiding does — `--all` shows them. This matters on `BSVertexData`, where
both precision variants of `Vertex` exist as items and the dead one reads as
zeroes; without the filter a healthy mesh looks corrupt.

`world` answers the question a block's own `Translation` cannot: where the thing
actually is once its parent chain is applied. Printed as translation, the nine
rotation terms and scale, one line per object, so two files can be diffed by
name — which is how "the loading-screen convert put the kept effect branches
exactly where the skeleton had them" is checked (`tests/loadingscreen/
live_effects.sh`), rather than asserted.

```powershell
nifskope-cli world rig.nif    | sort > before.txt
nifskope-cli world screen.nif | sort > after.txt
```

## Merging NIFs (`merge`) — building a poseable armour set

```powershell
nifskope-cli merge torso.nif --add arms.nif --add legs.nif --add helmet.nif `
                             --add skeleton.nif -o set.nif
# merged arms.nif: +6 block(s), 1 shape(s), 1 node(s) added, 9 reused by name, 2 re-parented
# total: 4 shape(s) added, 41 node(s) shared with the target
```

**The point is the de-duplication.** A naive branch splice gives every merged
piece its own private copy of the bones it is skinned to. That *renders*
correctly, but it cannot be posed — moving "Chest" would mean moving five
separate copies of it. `merge` maps a donor `NiNode` onto the target's
same-named node instead, and because the splice rewrites links through a
donor→target block map, **every skin's `Bones` array is re-pointed at the shared
bones for free**.

Only `NiNode`s are de-duplicated. Shapes are always added — two armour pieces
may legitimately carry same-named shapes.

`--no-dedupe` keeps a verbatim independent copy. Use it only when you explicitly
want the pieces *not* to share a rig.

A bare `skeleton.nif` is just `NiNode`s, so **loading a skeleton is the same
command** — merge it in and every matching bone is shared, while bones the armour
lacked are added and parented correctly. (Note `Rigging ▸ Import Donor Bone
Nodes...` will *not* do this: it requires the donor to contain skinned shapes,
and a skeleton file has no meshes.)

If the summary reports **0 nodes reused**, the pieces do not share a skeleton —
their bone names differ, and posing them as one rig will not work. The command
warns about this.

### Verified

Merging two skinned FO4 fixtures:

| check | result |
|---|---|
| de-duplication | 9 nodes reused by name, 1 genuinely new node added |
| shared skeleton | merged skin's `Bones` → blocks 1, 2, 3, 8, 9, 10, 11 (the *target's* originals) + 80 (the new bone) |
| skeleton root | → block 0, the target's root |
| geometry | 1689 verts / 3230 tris preserved |
| integrity | `tools/join_test/verify_join.py` **PASS** on the merged file — skin counts consistent and every per-vertex bone index in range |

## Poses (`pose`)

A pose is a `NiControllerSequence` carrying **one key per bone at t=0** — the
NIF equivalent of a Blender Action holding a single frame. So poses live in the
file, show up in the Timeline, and export like any other animation.

```powershell
nifskope-cli pose set.nif --list                    # bones + existing poses
nifskope-cli pose set.nif --save TPose  -o p1.nif   # capture current bone transforms
nifskope-cli pose p1.nif  --apply TPose -o p2.nif   # write it back onto the bones
nifskope-cli pose p1.nif  --apply TPose --blend 0.5 -o p3.nif   # 50% toward the pose
```

`--blend F` (0..1) interpolates each bone from where it is now toward the pose —
Blender's pose-strength slider. `--blend 1` (default) is a plain replace;
verified that from the origin, 50% lands a bone at exactly half its posed value.

Posing itself needs no command — see the note below. `pose` is the *library*:
capture what you have posed, and restore it later. The same save / apply / blend
is in the GUI's **Pose Manager** dock (Workspaces ▸ Pose).

### Verified

69 bones detected on the merged file; `--save TPose` created and listed the
sequence; the `Chest` bone was then moved to (99, 42, 7) and `--apply TPose`
restored it to exactly `X 0.000024 Y 0.539375 Z 91.284805` — the original value
to the last digit.

> **Posing bones needs no feature.** `Shape::updateBoneTransforms()` reads
> `bone->localTrans( skeletonRoot )` — the live node transform — so selecting a
> bone in the Block List and pressing `G`/`R`/`S` deforms the skinned mesh
> immediately. Verified by `WW_POSE_TEST`: both merged armour pieces followed a
> shared bone, and restoring it returned the deformation to zero.

## Careful: `set` and compound values

`Vector3`, `Quat`, colours and friends parse from a **comma-separated** list:

```powershell
nifskope-cli set model.nif -b 1 -f "Translation" -v "99.0,42.0,7.0" -o out.nif
```

The `X 1 Y 2 Z 3` form the GUI *displays* is not accepted as input. Historically
that mismatch was dangerous: `Vector3::fromString` returns silently on a bad
parse leaving an all-zero value, while `NifValue::setFromString` still reports
success — so a wrong format **wrote zeros and claimed to have worked**. `set`
now validates the shape first and refuses. If you write code elsewhere that
accepts user text for a compound value, do the same: a `true` from
`setFromString` does not mean the string was understood.

## Animation rigging (`anim-setup`)

The workflow the GUI's *Setup Controllers* dialog drives, addressable by name so
it can be scripted across many nodes. Ask a block what it accepts:

```powershell
nifskope-cli anim-setup model.nif -b 0 --list
#   NiTransformController    Transform (position/rotation/scale)
#   NiVisController          Visibility (on/off)
#   sequences: (none)
```

Then rig it. One call builds the whole graph — creating the manager, palette,
sequence and text keys if the file has none:

```powershell
nifskope-cli anim-setup model.nif -b 0 `
    --controller NiTransformController --new-sequence --sequence autoLoop -o rigged.nif
```

Add further nodes to that sequence **by name**:

```powershell
nifskope-cli anim-setup rigged.nif -b 1 --controller NiVisController --sequence autoLoop -o rigged2.nif
```

`--standalone` attaches a controller with no sequence (always playing).
`--effect-var 0..9` sets an FO4 `EffectShaderControlledVariable` and
`--int-var N` a `LightingShaderControlledVariable`; `--list` marks which
controllers take them.

New interpolators start with two default keys — edit them in the Animation
Manager, or write key arrays directly with `set` (they are plain model arrays).

**Implementation note.** `spSetupControllers::cast` used to hold the whole
implementation behind its modal dialog. The logic now lives in
`AnimSetup::setupControllers()` (`src/spells/animationsetup.h`), with the dialog
as one caller and this command as another. One behavioural improvement fell out:
the core resolves the target sequence **by name**, where the dialog used a combo
*index* that meant nothing to a non-GUI caller.

### Verified

| check | result |
|---|---|
| new sequence on an unrigged file | 80 → 87 blocks: `NiControllerManager`, `NiControllerSequence`, `NiDefaultAVObjectPalette`, `NiMultiTargetTransformController`, `NiTextKeyExtraData`, +1 interpolator/data pair |
| ControlledBlock wiring | 1 entry, Interpolator → 85, Controller → 81 (the multi-target controller) |
| palette entry | `Name` = node name, `AV Object` → block 0 |
| `Animation/Fix Invalid AV Object Refs` | **no-op** (file hash unchanged) → refs valid |
| add second node by sequence name | controlled blocks 1 → 2, palette objs 1 → 2 |
| unknown sequence name | clean error, exit 1 |

## Scope — what batch mode can and cannot reach

**Can:** the 195 registered spells (by `"Page/Name"`), and any direct model
edit — blocks, links, arrays, field values. That covers animation rigging
(controllers, sequences, interpolators, keyframe arrays, palette entries),
skinning, bounds, sanitising, header and property work.

**Cannot:** the viewport modelling tools — extrude, loop cut, knife, join,
separate, bevel. Those live on `GLView`, depend on picked-element state and
need a GL context. They are not "not wired up yet"; they are architecturally
GUI-bound.

**Also cannot:** anything needing game resources. `Game::GameManager::get()` is
deliberately NOT initialised — its scan builds a `QProgressDialog`
(`gamemanager.cpp:150`), which is fatal without a `QApplication`. Nothing in
the model layer needs it; texture/archive resolution does.

**Spells that prompt** will block with no window to prompt into. `spells` marks
each entry `instant` / `constant`; those are the safe subset. A spell whose
parameters come from a dialog needs its logic split into a parameterised core
before it is usable here — the logic is fine, only the entry point is GUI-bound.

## Invocation (Windows)

`NifSkope.exe` is linked `-subsystem,windows`, so it starts with **no console**
and neither cmd nor PowerShell waits for it. Two consequences:

1. The CLI calls `AttachConsole(ATTACH_PARENT_PROCESS)` and reopens
   stdout/stderr on `CONOUT$` — but only when they are not already redirected,
   so pipes and file redirects still work.
2. A bare `nifskope.exe -no-gui ... > out.txt` **returns before the work
   happens** and leaves an empty file.

So either use the wrapper (`start /b /wait`, preserves stdout, stderr and the
exit code):

```powershell
release\nifskope-cli.cmd get model.nif -b 73 -f "Num Vertices"
```

or force PowerShell to wait by piping:

```powershell
(.\NifSkope.exe -no-gui get model.nif -b 73 -f "Num Vertices" | Out-String).Trim()
```

Exit codes: `0` ok, `1` runtime error (file, path, spell not applicable),
`2` usage error.

## Verified

Against `tests/rigging/fixtures/donor.nif` (FO4 bs130, 80 blocks):

| check | result |
|---|---|
| `get` a field | `Num Vertices` → 1689 |
| `set` + reload | `Flags` 14 → 15, re-read from the saved file = 15 |
| `cast Mesh/Update Bounds` | bone[0] radius 4.89446 → 4.75802, file hash changed |
| not-applicable target | refused, exit 1 |
| bad field path | error, exit 1 |
| `dump` condition filtering | one live `Vertex <HalfVector3>`, not both variants |

**Read the bounds case before writing a test like it.** On a *skinned* shape
`spUpdateBounds` deliberately writes a ZERO bounding sphere on the block
(`mesh.cpp:2032` — `calculateBoneBounds` succeeds, so the vertex branch is
skipped) because FO4 keeps real bounds per bone in `BSSkin::BoneData`. Checking
`Bounding Sphere/Radius` therefore shows 0 before *and* after, and looks like a
no-op. The real evidence is in `BoneData`.

## Why this is worth having beyond scripting

The eight `WW_*_TEST` harnesses in `nifskope_ui.cpp` each hand-roll
load → act → verify → quit, several hundred lines in total. Most of what they
do is now expressible as CLI invocations plus a script, which would shrink that
file considerably. New harnesses should reach for the CLI first and only add a
bespoke block when they need something batch mode genuinely cannot reach.

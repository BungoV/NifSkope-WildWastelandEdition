# What Todd's treat says, and where NifSkope disagrees

Six areas were checked against Todd's treat (Fallout 4 1.10.155),
queried with
the Todd's treat tooling (kept outside this repo). Every claim below was
checked in Todd's treat, so it can be re-read rather than re-argued. The
behaviour described matches 1.11.221 closely.

| # | Area | Verdict |
|---|------|---------|
| 1 | Sequence → node binding | **Differs.** The file's object palette is the authority; NifSkope searches the scene graph and ignores it. |
| 2 | Procedural lightning | **Two guesses confirmed, four rules wrong.** The cadence has no counterpart; three reinterpretations previously rejected on screen are what the engine does. |
| 3 | NiPSys particles | **Differs by scope** — but not where first claimed; see the correction below. The real gap was `NiPSysModifierActiveCtlr` (288 files) and the `Active` field, both unread. |
| 4 | `AttachT` | **Correct as far as it goes.** Three more technique tags exist, arguments can carry a `\|n` suffix, and attachment applies **no transform**. |
| 5 | Controller flag bits | **Settled.** All three `glcontroller.cpp` TODOs are answerable; bit 6 selects scaled time. |
| 6 | Loading screens | **Negative result.** The menu never activates a sequence and never ticks a controller. |

---

## 1. How a sequence binds to a node

**NifSkope today.** `ControllerManager::setSequence` (`src/gl/controllers.cpp:164`)
resolves each Controlled Block with `target->findChild( nodename )`. That is
`Node::findChild` (`src/gl/glnode.cpp:426`) — self first, then children
depth-first, **first match wins**. `NiDefaultAVObjectPalette` is read by spells
and by the merge, but nothing in the GL layer consults it for playback.

**The engine.** When a `NiControllerSequence` binds its targets, it takes the
object palette of its owning `NiControllerManager`, looks the accumulation root
up in that palette by name, and then, for each Controlled Block that has an
interpolator (and is not already blended), looks the block's node name up in
the same palette.

The palette lookup is a **hash-map lookup** keyed on the node name, and it
returns nothing on a miss. It is not a tree walk, and there is no fallback to
one.

That palette is **the one in the file**. When a `NiControllerManager` loads, it
reads the cumulative byte, then the sequence links, then links its object
palette straight from the file. Nothing at load rebuilds the palette from the
scene graph.

When a palette *is* rebuilt from the scene graph, the walk is pre-order, parent
then children in order, and each insert **overwrites** an existing entry of the
same name. So on duplicate names the engine keeps the **last** node in
pre-order. `findChild` keeps the **first**. Same traversal, opposite answer.

The rest of the block resolution, for completeness:

- Property match: the target's property slots are compared by name against
  the block's Property Type; two property sets on the target are consulted.
- Controller match: the target's controller chain is walked, comparing each
  controller's type name against the block's Controller Type and its ID string
  against the block's Controller ID. A controller whose ID string is null
  matches only an **empty** Controller ID.
- Interpolator: the controller is asked for the index of the block's
  Interpolator ID. If it has no such interpolator, the **whole block is
  dropped**.
- The engine then *inserts* a `NiBlendInterpolator` into the controller
  (priority 2, weight 0) and sets controller flag bit 5. That is how two
  sequences drive one controller.
- A node name that does not resolve is **silently skipped** — except that a
  name beginning with `##` is flagged on the sequence. `##` is a reserved prefix.

**Why this matters here.** Tonight's cross-limb binding bug was six merged files'
Controlled Blocks resolving to one node. The fix works around `findChild`; the
engine's rule would have made the bug impossible, because the palette says which
node each name means and a merge that writes the palette correctly cannot
mis-bind. This is the one area where NifSkope's behaviour is known-wrong rather
than merely unverified.

---

## 2. Procedural lightning

The engine's lightning geometry generator and its five helpers were read in
full. The whole generator contains exactly **three** float constants: `1.0`,
`0.5`, `0.25`, each confirmed by reading its value out of the game.

### Confirmed correct

- **Amplitude decay 0.5.** The displacement helper recurses with `amp * 0.5`
  (the constant reads back as exactly 0.5). The guess in `tlMidpointJag` is
  right.
- **`Child Width Mult` compounds per generation.** The engine computes
  `halfWidth = Width * powf( ChildWidthMult, generation ) * 0.5`. NifSkope uses
  `childWidthMult` then `childWidthMult²` — the same rule for two levels.
- **`nif.xml`'s field list and order.** The process-parameter block is read as
  five 4-byte fields followed by three 1-byte fields, and the generation-parameter
  block as three 2-byte fields. That is exactly Length, Length Variation, Width,
  Child Width Mult, Arc Offset / Fade Main Bolt, Fade Child Bolts, Animate Arc
  Offset / Subdivisions, Num Branches, Num Branches Variation.

### Confirmed wrong

- **The 1/24 s mutation cadence does not exist.** There is no cadence constant
  anywhere in the generator. What drives it is the lightning controller's
  per-frame update:

  The per-frame parameter refresh evaluates only interpolators 6–9 (Length,
  Length Var, Width, Arc Offset). Interpolators **1 (Generation)** and
  **2 (Mutation)** are evaluated as **bools** and cached, and a change in either
  forces a full regenerate. Generation false culls the geometry outright.
  Otherwise, if `Animate Arc Offset` is set, the generator runs in mutate mode
  with the new and previous Arc Offset, keeping the branch structure.

  So the rate a bolt reshapes at is authored, as a **bool toggle curve**, and
  every asset gets its own. NifSkope's constant only looked plausible because the
  shieldtesla Mutation keys are dense.

  *(Corrected 2026-08-01: the first reading of this took Arc Offset for the
  Mutation value. The parameter block's field order settles it.)*

- **`Subdivisions` is a recursion depth.** A branch has `(1<<s)*4 + 4` vertices
  and `(1<<s)*4` triangles: 2^s segments, four verts per ring, four triangles per
  segment — two crossed strips.
  `regenerate()` treats it as a segment count rounded up to a power of two and
  capped at 32, and its comment records that depth "was tested and is wrong".

- **Child branches use the authored `Length`.**
  `len = Length * 0.5^gen + rand(-1,1) * LengthVar * 0.25^gen` (`powf(0.5, gen)`,
  `powf(0.25, gen)` and a random value in -1..1). NifSkope uses
  `lenMul = 0.2 + rand*0.25` of the main bolt, with a comment recording that
  using `Length` "was tried and is wrong".

- **Branch counts halve every generation; the recursion is not two levels deep.**
  The branch count is a random integer in `[ max(A-B, 0), A+B+1 )` where `A` =
  Num Branches and `B` = Num Branches Variation; then `A >>= 1; B >>= 1`, child
  subdivisions = `s <= 1 ? 0 : s - 1`, and it recurses. Depth terminates because
  the counts halve, not because it is capped.
  NifSkope rolls one level of `numBranches ± var` clamped to 20, then a
  hardcoded second level in which roughly half the branches fork.

- **The displacement is a tent, not midpoint noise.** The displacement helper
  picks **one** random 2-D direction per span (a random angle, then `cos`/`sin`
  times the amplitude) and applies it across the whole span with a unit tent
  weight `w = 1.0 - |t - 0.5| * 2.0` — zero at both ends, one at the centre — then
  splits the span in half and recurses with `amp * 0.5`, re-rolling the direction
  for each half. NifSkope jitters each midpoint independently in two axes.
  The engine also does **not** normalise: amplitude is used raw, so total
  excursion runs to roughly 2 × Arc Offset, which is what `regenerate()`
  deliberately normalises away.

### The consequence for the parked backlog

`TO_BE_IMPLEMENTED.md` §12 says three reinterpretations were "disproven by
rendering" and warns against re-trying them "without new evidence". Two of the
three — subdivisions-as-depth and Length-as-branch-length — are what the engine
actually does. That note is superseded: it predates `WW_CHANGES 2026-07-31i`,
which changed the span rule, so all three were tested against a bolt drawn
between the wrong two points. This is the new evidence.

---

## 3. NiPSys particle simulation

`src/gl/glparticles.cpp:98` calls its own simulator "preview-grade". The engine
ships **56** `NiPSys*` classes, **28** with a per-frame update; NifSkope named **16**.

> **Corrected 2026-08-01 by counting.** The first version of this section ranked
> the gap by reading the engine's class list, and named AgeDeath, GrowFade,
> Position and Spawn as "the four that decide whether a particle dies, fades,
> moves or spawns". Counting them in FO4's 692 effect meshes says otherwise:
>
> | class | effect meshes |
> |---|---|
> | `NiPSysModifierActiveCtlr` | **288** |
> | `NiPSysEmitterSpeedCtlr` | 51 |
> | `NiPSysEmitterInitialRadiusCtlr` | 21 |
> | `NiPSysEmitterLifeSpanCtlr` | 6 |
> | `NiPSysEmitterDeclinationCtlr` | 6 |
> | `NiPSysPlanarCollider` | 30 |
> | `NiPSysBombModifier` | 40 |
> | `NiPSysGrowFadeModifier` | **0** |
> | the six field modifiers | **0** |
> | `NiPSysMeshUpdateModifier` | **0** |
> | `NiPSysSpawnModifier` | 347, but 66 of 67 sampled have `Num Spawn Generations = 0` |
>
> So GrowFade, the fields and MeshUpdate would have been dead code in this game,
> and Spawn spawns nothing. AgeDeath and Position were not missing either — the
> simulator hardcoded both, which is its own small divergence, since the engine
> will not age a particle without a `NiPSysAgeDeathModifier` nor move one without
> a `NiPSysPositionModifier`.
>
> What was actually missing is the thing that appears in 288 files: the whole
> `NiPSysModifierCtlr` family, and a modifier's own `Active` field, neither of
> which was read at all. Every modifier ran, permanently, whatever the file said.
> Shipped 2026-08-01 along with the emitter parameter curves — including the
> manager case, where those controllers hold a `NiBlendFloatInterpolator` stub
> and the real keys live in the sequences.

Still absent, and now a deliberate choice rather than an oversight: the
colliders (`NiPSysPlanarCollider` 30 files, `NiPSysSphericalCollider` 3) and
`NiPSysBombModifier` (40). Those are real usage and would be the next thing worth
adding; the zero-count classes are not.

One detail worth having regardless: `NiPSysMeshEmitter` has six emission paths —
emit from vertex, face or edge, plus a skinned variant of each. The skinned path
is why a mesh emitter's placement depends on where its emitter mesh actually is,
which is the failure the loading-screen merge hit.

## 4. `AttachT` and named-node attachment

**NifSkope's model is right.** The engine reads the node's `AttachT` extra data
as a count and a string array — an `NiStringsExtraData`, exactly as
`src/nifmerge.cpp:228` assumes. Each entry is dispatched to a registered
technique.

**Three tags NifSkope does not know.** Each attach technique registers itself
under a tag string:

| Technique | Tag |
|---|---|
| named-node attach | `NamedNode` |
| Havok-geometry attach | `HavokGeometry` |
| multi-technique attach | `MultiTechnique` |
| particle-array attach | — (tag not read; `sParticleArrayAttachExcludeList` is its INI setting) |

Before matching, names are escaped for exactly three characters:
`' '` → `%SPC%`, `'['` → `%LBR%`, `']'` → `%RBR%`.

**One rule worth adopting.** The engine accepts an attached object only if it
has children **or** carries a `NiControllerManager`; otherwise the attach is
dropped before lights and addon nodes are added. An empty stub node is not
attached.

**The placement rule, decoded 2026-08-01.** The named-node attach does only
this:

```
if (arg is empty) return false
target = find node by name( root, arg )
parent = target ? target : root      # unresolved -> the ROOT
parent attaches the object
the object's render passes are invalidated
```

**No transform is applied.** It is a plain scene-graph re-parent, so an attached
ArtObject sits exactly where its own local transform puts it relative to the
named node — and a name that does not resolve attaches to the root rather than
being dropped. That closes the parked §12 question: our merge does the same
thing, so the X-01 leg arcs run inside the calf because the asset puts
`BoltGeo_01` there.

The name lookup resolves through a `BSBoneMap` extra-data hash if the root has
one, else a `BSFlattenedBoneTree`, else a name search. Nothing in it strips the
`|n` suffix FO4's own AttachT strings carry.

---

## 5. Controller flag bits

`glcontroller.cpp:224-226` carries three TODOs. All three are answerable, from
how the engine reads and writes a `NiTimeController`'s flags word:

| Bit | Meaning | Evidence | NifSkope |
|---|---|---|---|
| 0 | AnimType | read as `flags & 1` | not read |
| 1–2 | CycleType | read as `(flags >> 1) & 3` | `extrapolation` ✓ |
| 3 | Active | set/cleared as `0x08` | `active` ✓ |
| 4 | PlayBackwards | read as `(flags >> 4) & 1` | TODO, guess correct, unimplemented |
| 5 | Manager-controlled | if set, the controller's last update time is reset to "invalid" | TODO, guess correct |
| 6 | **Compute scaled time** | the controller update converts sequence time to scaled time only when bit 6 is set | TODO, **"unknown function"** |
| 7 | ForceUpdate | set as `0x80`, cleared with `0xff7f` | not read |
| 8 | MandatoryUpdate | read as `(flags >> 8) & 1` | not read |

Bit 6 is the one that changes behaviour rather than documentation: it decides
whether a controller is evaluated at raw sequence time or at scaled time.

**BSXFlags** is a weaker result. Only one bit has a dedicated reader in the
engine: `(flags >> 11) & 1`, the lights bit, which confirms `nif.xml`'s
"Bit 11: bLights" exactly. Every other bit is read inline where it is used, so
Todd's treat settles one bit here, not the table.

---

## 6. What the loading-screen menu actually does

This is a negative result, and it bears on work already shipped.

**The path.** The loading menu picks the load screen's NIF, requests the model,
and then initialises it: it applies a material swap, upgrades textures, builds a
rotation from Euler angles, walks the geometries, sets the attached art object,
resets collision, looks up a node by the name `LoadingMenuZoomTarget`, and sets
up lighting.

**No sequence is ever activated, and no controller is ever ticked.** Checked:
the model init, the per-frame advance, the render, the rotate and the model
request. The per-frame work is spin, pan and zoom, driven by
`fLoadingModel_DefaultSpinRate`, `uLoadingModel_DelayBeforeAutoSpin`,
`fLoadingModel_MouseToRotateSpeed`, `fLoadingModel_MouseToZoomSpeed` and
`fLoadingModel_KeyToPanSpeed`. Nothing in the loading menu activates a
controller sequence.

**The asset corpus agrees.** Across the 173 NIFs in `meshes/LoadScreenArt` of
the unpacked game data (`E:\Tools\Fallout 4\DataUnpacked\Data`):

- **0** contain a `NiParticleSystem`.
- **1** contains a `NiControllerManager` — `CreatureBloatfly.nif`, with sequences
  `CharFXOn`, `CharFXOnLoop`, `CharFXOff`, `CharFXOffLoop`.
- Neither `autoPlay`, `autoLoop` nor `CharFXOn` appears as a literal anywhere in
  `Fallout4.exe`, so nothing activates them by name.

**One thing the merge got right by inheritance.** 65 of the 173 carry a node
named `LoadingMenuZoomTarget`, and that is the node the menu looks up to frame
the model. `X01TeslaLoadScreen.nif` still has it, at block 734.

**Caveat, stated plainly.** This is a proof about five engine routines and a
corpus, not an exhaustive proof of a negative: a generic per-frame scenegraph
pass elsewhere could in principle reach the model. But the five relevant
routines are clean and the vanilla assets are consistent with a static model, so
the live effects baked into the loading screen should not be expected to animate
in game until something is shown to drive them.

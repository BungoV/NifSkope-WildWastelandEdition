# What the FO4 PDB says, and where NifSkope disagrees

Six areas were checked against the leaked Fallout 4 1.10.155 exe + PDB pair at
`E:\Projects\Fo4CommunityShaders\Fo4PDB`, queried with
`fallout4-community-shaders/tools/exere/f4pdb.py`. Every claim below cites the
function it came from, by RVA in that build, so it can be re-read rather than
re-argued. Names are 1.10.155; bodies match 1.11.221 closely, and live addresses
for that build come from the Address Library via `f4re.py`.

| # | Area | Verdict |
|---|------|---------|
| 1 | Sequence → node binding | **Differs.** The file's object palette is the authority; NifSkope searches the scene graph and ignores it. |
| 2 | Procedural lightning | **Two guesses confirmed, four rules wrong.** The cadence has no counterpart; three reinterpretations previously rejected on screen are what the engine does. |
| 3 | NiPSys particles | **Differs by scope** — but not where first claimed; see the correction below. The real gap was `NiPSysModifierActiveCtlr` (288 files) and the `Active` field, both unread. |
| 4 | `AttachT` | **Correct as far as it goes.** Three more technique tags exist, arguments can carry a `\|n` suffix, and attachment applies **no transform**. |
| 5 | Controller flag bits | **Settled.** All three `glcontroller.cpp` TODOs are answerable; bit 6 is `ComputeScaledTime`. |
| 6 | Loading screens | **Negative result.** The menu never activates a sequence and never ticks a controller. |

---

## 1. How a sequence binds to a node

**NifSkope today.** `ControllerManager::setSequence` (`src/gl/controllers.cpp:164`)
resolves each Controlled Block with `target->findChild( nodename )`. That is
`Node::findChild` (`src/gl/glnode.cpp:426`) — self first, then children
depth-first, **first match wins**. `NiDefaultAVObjectPalette` is read by spells
and by the merge, but nothing in the GL layer consults it for playback.

**The engine.** `NiControllerSequence::StoreTargets` (`0x1c14ff0`):

```
palette   = this->owner(+0x60)->objectPalette(+0xC0)
accumRoot = palette->GetAVObject( this->accumRootName(+0x88) )
ResolveTransformInterpolators( this, arg, palette, 0 )
for each controlled block i:                    # count +0x18, blocks +0x20 (stride 0x20)
    if block.interpolator == null: continue     # ids +0x28, stride 0x28 = 5 BSFixedStrings
    if !interp->vtbl[0x1b8]( palette ): continue
    if block[+0x10] != null: continue           # already has a blend interpolator
    target = palette->GetAVObject( ids[i].nodeName )
    ...
```

`NiDefaultAVObjectPalette::GetAVObject` (`0x1bc1d20`) is a **BSCRC32 hash-map
lookup** — 24-byte entries of `{BSFixedString key, NiAVObject* value, next}`,
capacity at `+0x1c`, table at `+0x38` — and returns null on a miss. It is not a
tree walk, and there is no fallback to one.

That palette is **the one in the file**. `NiControllerManager::LoadBinary`
(`0x1c0c260`) reads the cumulative byte, then `NiStream::ReadMultipleLinkIDs`
for the sequences, then `NiStream::ResolveLinkID` straight into `+0xC0`.
Nothing at load calls `ResetAndFillFromScenegraph`.

When a palette *is* rebuilt — `NiDefaultAVObjectPalette::ResetAndFillFromScenegraph`
(`0x1bc0890`) → `RecurseAndAddObjectsToPalette` (`0x1bbf9c0`) — the recursion is
pre-order, parent then children in order, and `SetAVObject` (`0x1bc25b0`) calls
`SetAt` with a `NoOpPreviousValueFunctor`, i.e. it **overwrites**. So on duplicate
names the engine keeps the **last** node in pre-order. `findChild` keeps the
**first**. Same traversal, opposite answer.

The rest of the block resolution, for completeness:

- Property match: the target's two property slots are compared by name against
  `ids[i].propertyType`; two objects are consulted, one reached through
  `target->vtbl[0x40]()` (slots `+0x130`/`+0x138`) and one through
  `target->vtbl[0x70]()` (slots `+0x120`/`+0x128`).
- Controller match: walk `NiObjectNET::m_spControllers` at `+0x18`, `next` at
  `+0x40`, comparing the controller's type name against `ids[i].controllerType`
  and `ctrl->vtbl[0x1c8]()` against `ids[i].controllerID`. A controller whose ID
  string is null matches only an **empty** `controllerID`.
- Interpolator: `idx = ctrl->vtbl[0x1a0]( ids[i].interpolatorID )`. If it comes
  back `NiInterpController::INVALID_INDEX` the **whole block is dropped**.
- The engine then *inserts* a `NiBlendInterpolator` into the controller
  (`vtbl[0x1e0]` create with priority 2 and weight 0, `vtbl[0x1b8]` install) and
  sets controller flag bit 5. That is how two sequences drive one controller.
- A node name that does not resolve is **silently skipped** — except that
  `StoreTargets+0x5a8` checks whether the name begins with `##` (`cmp word
  ptr [rax], 0x2323`) and if so sets `this->+0xb2`. `##` is a reserved prefix.

**Why this matters here.** Tonight's cross-limb binding bug was six merged files'
Controlled Blocks resolving to one node. The fix works around `findChild`; the
engine's rule would have made the bug impossible, because the palette says which
node each name means and a merge that writes the palette correctly cannot
mis-bind. This is the one area where NifSkope's behaviour is known-wrong rather
than merely unverified.

---

## 2. Procedural lightning

`BSProceduralGeometry::Lightning::Process` (`0x1cdb2c0`) and its five helpers.
The whole of `Process` contains exactly **three** float constants: `1.0`, `0.5`,
`0.25`. Confirmed by reading them out of the exe at `0x2c48d60`, `0x2c4b1a0` and
`0x2c5fdb0`.

### Confirmed correct

- **Amplitude decay 0.5.** `OffsetHelper` (`0x1cdc820`) recurses with
  `amp * 0.5` — constant at `0x2c4b1a0`, verified `0x3f000000`. The guess in
  `tlMidpointJag` is right.
- **`Child Width Mult` compounds per generation.** The engine computes
  `halfWidth = Width * powf( ChildWidthMult, generation ) * 0.5`. NifSkope uses
  `childWidthMult` then `childWidthMult²` — the same rule for two levels.
- **`nif.xml`'s field list and order.** `ProcessParams::LoadBinary` (`0x1cdacf0`)
  streams five 4-byte fields at `0x00,0x04,0x08,0x0c,0x10` and three 1-byte
  fields at `0x18,0x19,0x1a`; `GenerationParams::LoadBinary` (`0x1cdab10`)
  streams three 2-byte fields at `0x00,0x02,0x04`. That is exactly Length,
  Length Variation, Width, Child Width Mult, Arc Offset / Fade Main Bolt, Fade
  Child Bolts, Animate Arc Offset / Subdivisions, Num Branches, Num Branches
  Variation.

### Confirmed wrong

- **The 1/24 s mutation cadence does not exist.** There is no cadence constant
  anywhere in `Process`. What drives it, from
  `BSProceduralLightningController::Update` (`0x1cf5940`):

  The nine interpolators land at `+0x128`..`+0x168`, and `ProcessParams` sits at
  `+0x178`, so `+0x188` is Arc Offset and `+0x190`/`+0x191`/`+0x192` are Fade
  Main Bolt, Fade Child Bolts and Animate Arc Offset.
  `UpdateProcessParams` (`0x1cf6390`) evaluates only interpolators 6–9 (Length,
  Length Var, Width, Arc Offset). Interpolators **1 (Generation)** and
  **2 (Mutation)** are evaluated as **bools**, cached at `+0x1a0` and `+0x1a1`,
  and at `0x1cf5bea` a change in either forces a full regenerate. Generation
  false culls the geometry outright. Otherwise, if `Animate Arc Offset` is set,
  `Process` runs in mutate mode with `newArcOffset / previousArcOffset` at
  `+0x18c`, keeping the branch structure.

  So the rate a bolt reshapes at is authored, as a **bool toggle curve**, and
  every asset gets its own. NifSkope's constant only looked plausible because the
  shieldtesla Mutation keys are dense.

  *(Corrected 2026-08-01: the first reading of this called `+0x188` the Mutation
  value. It is Arc Offset — the params struct offsets settle it.)*

- **`Subdivisions` is a recursion depth.** `GetBranchVerts(s) = (1<<s)*4 + 4`
  (`0x1cdc670`) and `GetBranchTris(s) = (1<<s)*4` (`0x1cdc690`): 2^s segments,
  four verts per ring, four triangles per segment — two crossed strips.
  `regenerate()` treats it as a segment count rounded up to a power of two and
  capped at 32, and its comment records that depth "was tested and is wrong".

- **Child branches use the authored `Length`.**
  `len = Length * 0.5^gen + rand(-1,1) * LengthVar * 0.25^gen`, read straight out
  of `Process` at `0x1cdb418`–`0x1cdb47e` (`powf(0.5, gen)`, `powf(0.25, gen)`,
  `BSRandom::FloatNeg1To1`). NifSkope uses `lenMul = 0.2 + rand*0.25` of the main
  bolt, with a comment recording that using `Length` "was tried and is wrong".

- **Branch counts halve every generation; the recursion is not two levels deep.**
  `CreateBranches` (`0x1cdc6d0`):
  `count = BSRandom::UnsignedInt( max(A-B, 0), A+B+1 )` where `A` = Num Branches
  and `B` = Num Branches Variation; then `A >>= 1; B >>= 1`, child subdivisions
  = `GetBranchSubdivisions(s)` = `s <= 1 ? 0 : s - 1` (`0x1cdc6b0`), and it
  recurses. Depth terminates because the counts halve, not because it is capped.
  NifSkope rolls one level of `numBranches ± var` clamped to 20, then a
  hardcoded second level in which roughly half the branches fork.

- **The displacement is a tent, not midpoint noise.** `OffsetHelper` picks **one**
  random 2-D direction per span (`BSRandom::FloatTwoPi`, then `cosf`/`sinf` times
  the amplitude) and applies it across the whole span with a unit tent weight
  `w = 1.0 - |t - 0.5| * 2.0` — zero at both ends, one at the centre — then
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
ships **56** `NiPSys*` classes, **28** with an `Update`; NifSkope named **16**.

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

One detail worth having regardless: `NiPSysMeshEmitter` has six emission entry
points — `EmitFromVertex` / `EmitFromFace` / `EmitFromEdge` (`0x1c59580`,
`0x1c59590`, `0x1c599a0`) and skinned variants of each (`0x1c59cd0`, `0x1c59df0`,
`0x1c5a0c0`). The skinned path is why a mesh emitter's placement depends on where
its emitter mesh actually is, which is the failure the loading-screen merge hit.

## 4. `AttachT` and named-node attachment

**NifSkope's model is right.** `BGSAttachTechniquesUtil::ProcessAttachTechniques`
(`0x171770`) calls `NiObjectNET::GetExtraData( node, attachTag )` and then reads
`+0x18` as a count and `+0x20` as a string array — an `NiStringsExtraData`,
exactly as `src/nifmerge.cpp:228` assumes. Each entry is dispatched to a
registered technique.

**Three tags NifSkope does not know.** Techniques register themselves in an
`Instantiate` that hands `BSAttachTechniques::Register` a tag string stored at
`vftable+0x20`:

| Class | `Instantiate` | Tag |
|---|---|---|
| `BGSNamedNodeAttach` | `0x1720f0` | `NamedNode` (read at `0x2c5ab88`) |
| `BGSHavokGeometryAttach` | `0x172000` | `HavokGeometry` |
| `BGSMultiTechniqueAttach` | `0x1751b0` | `MultiTechnique` |
| `BGSParticleArrayAttach` | `0x171f20` | — (tag not read; `sParticleArrayAttachExcludeList` is its INI setting) |

`BGSAttachTechniquesUtil::ConvertSpecialCharacters` (`0x1716b0`) escapes exactly
three characters: `' '` → `%SPC%`, `'['` → `%LBR%`, `']'` → `%RBR%`.

**One rule worth adopting.** `BGSAttachTechniques::AttachItem` (`0x171460`)
accepts an attached object only if it has children **or** carries a
`NiControllerManager` (`NiControllerManager::GetNiControllerManager`,
`0x1c0f830`); otherwise the attach is dropped before
`AddLightsAndAddonNodes`. An empty stub node is not attached.

**The placement rule, decoded 2026-08-01.** `BGSNamedNodeAttach::Attach`
(`0x172090`) is a trampoline into `AttachPolicy::vftable+8`, which is
`BGSNamedNodeAttach::AttachPolicy::Process` (`0x175710`). All of it:

```
if (arg is empty) return false
target = BSUtilities::GetObjectByName( input->root, arg, true, true )
parent = target ? target->GetAsNode() : input->root      # unresolved -> the ROOT
parent->AttachObject( input->object, true )
BSShaderUtil::InvalidateRenderPasses( input->object )
```

**No transform is applied.** It is a plain scene-graph re-parent, so an attached
ArtObject sits exactly where its own local transform puts it relative to the
named node — and a name that does not resolve attaches to the root rather than
being dropped. That closes the parked §12 question: our merge does the same
thing, so the X-01 leg arcs run inside the calf because the asset puts
`BoltGeo_01` there.

`BSUtilities::GetObjectByName` (`0x1c93970`) resolves through a `BSBoneMap`
extra-data hash if the root has one, else a `BSFlattenedBoneTree`, else a name
search. Nothing in it strips the `|n` suffix FO4's own AttachT strings carry.

---

## 5. Controller flag bits

`glcontroller.cpp:224-226` carries three TODOs. All three are answerable, from
named accessors on the flags word at `NiTimeController+0x10`:

| Bit | Meaning | Evidence | NifSkope |
|---|---|---|---|
| 0 | AnimType | `GetAnimType` `0x1ba7100` — `flags & 1` | not read |
| 1–2 | CycleType | `GetCycleType` `0x1ba7110` — `(flags >> 1) & 3` | `extrapolation` ✓ |
| 3 | Active | `SetActive` `0x13f0b0` — sets/clears `0x08` | `active` ✓ |
| 4 | PlayBackwards | `GetPlayBackwards` `0x1ba7140` — `(flags >> 4) & 1` | TODO, guess correct, unimplemented |
| 5 | Manager-controlled | `BSProceduralLightningController::Update+0x3a` — if set, last time is reset to `INVALID_TIME` | TODO, guess correct |
| 6 | **ComputeScaledTime** | `NiTimeController::ComputeScaledTime` `0x1ba6d30`; `…Update+0x93` calls the scale-time virtual only when bit 6 is set | TODO, **"unknown function"** |
| 7 | ForceUpdate | `SetForceUpdate` `0x1ba7190` — sets `0x80`, clears with `0xff7f` | not read |
| 8 | MandatoryUpdate | `QMandatoryUpdate` `0x1ba4aa0` — `(flags >> 8) & 1` | not read |

Bit 6 is the one that changes behaviour rather than documentation: it decides
whether a controller is evaluated at raw sequence time or at scaled time.

**BSXFlags** is a weaker result. Only one named accessor exists, `BSXFlags::QLights`
(`0x175b40`) = `(flags >> 11) & 1`, which confirms `nif.xml`'s "Bit 11: bLights"
exactly. Every other bit is read inline at its call site, so the PDB settles one
bit here, not the table.

---

## 6. What the loading-screen menu actually does

This is a negative result, and it bears on work already shipped.

**The path.** `LoadingMenu::AdvanceMovie` (`0x1297110`) → `TESLoadScreen::GetLoadNIF`
→ `LoadingMenu::SetForegroundModel` (`0x12984a0`, `BSModelDB::Request`) →
`LoadingMenu::InitModel` (`0x1299390`). `InitModel` applies a material swap,
upgrades textures, builds a rotation with `NiMatrix3::FromEulerAnglesXYZ`, walks
geometries with `BSVisit::TraverseScenegraphGeometries`, calls
`BackgroundScreenModel::SetAttachedArtObject`, resets collision, looks up a node
by the global `LoadingMenuZoomTargetName`, and calls `InitLighting`.

**No sequence is ever activated, and no controller is ever ticked.** Checked:
`InitModel`, `AdvanceMovie`, `Render` (`0x1297bd0`), `RotateModel` (`0x1298c40`)
and `SetForegroundModel`. The per-frame work is spin, pan and zoom, driven by
`fLoadingModel_DefaultSpinRate`, `uLoadingModel_DelayBeforeAutoSpin`,
`fLoadingModel_MouseToRotateSpeed`, `fLoadingModel_MouseToZoomSpeed` and
`fLoadingModel_KeyToPanSpeed`. There is no `NiControllerManager::ActivateSequence`
call anywhere in `LoadingMenu`.

**The asset corpus agrees.** Across the 173 NIFs in `meshes/LoadScreenArt` of
the unpacked game data (`E:\Tools\Fallout 4\DataUnpacked\Data`):

- **0** contain a `NiParticleSystem`.
- **1** contains a `NiControllerManager` — `CreatureBloatfly.nif`, with sequences
  `CharFXOn`, `CharFXOnLoop`, `CharFXOff`, `CharFXOffLoop`.
- Neither `autoPlay`, `autoLoop` nor `CharFXOn` appears as a literal anywhere in
  `Fallout4.exe`, so nothing activates them by name.

**One thing the merge got right by inheritance.** 65 of the 173 carry a node
named `LoadingMenuZoomTarget`, and that is the node `InitModel` looks up to frame
the model. `X01TeslaLoadScreen.nif` still has it, at block 734.

**Caveat, stated plainly.** This is a proof about five functions and a corpus,
not an exhaustive proof of a negative: a generic per-frame scenegraph pass
elsewhere could in principle reach the model. But five relevant functions are
clean and the vanilla assets are consistent with a static model, so the live
effects baked into the loading screen should not be expected to animate in game
until something is shown to drive them.

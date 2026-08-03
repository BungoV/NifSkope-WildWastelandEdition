# Four features: growing-op segments, NiPointLight, particle colliders, particle bombs

Status **NOT STARTED** as of 2026-08-03. This is the findings + plan document from a
five-agent mapping pass; no feature code has been written. Scope is **Fallout 4 only**
(bungo, 2026-08-03: "we will not be tackling older games"). `BSWindModifier` is
**explicitly out of scope** — it is driven by a global game-state wind vector the viewport
has no source for, so any implementation would be inventing the input.

Read the "Landmines" section before touching anything. Several of these are cases where the
obvious implementation looks right on half the corpus and is silently wrong.

Everything below marked **measured** came from a command run against
`E:\Tools\Fallout 4\DataUnpacked\Data`. Everything marked **inferred** did not — those are
the ones to check first.

---

## Already fixed (prerequisite, shipped)

`e12bde8` — `tlSyncSegments` assumed sub-segments partition their parent segment. They do
not. **Measured:** `Clothes/Courser/CourserF.nif` block 158 segment 4 spans 668 triangles
from index 4443 while its five sub-segments cover 657 starting at 4476 — an 11-triangle
parent-only gap at the head. The old "last sub absorbs the remainder" rule closed that gap
*even at zero growth*, and `TlShapeStateCommand::redo` ran it unconditionally over the exact
table `Delete` had just built. Now delta-based and a no-op when the count is unchanged, with
a `segExact` opt-out for Delete/Separate.

**Measured frequency:** a 400-file stride sample over `meshes/Clothes`, `meshes/Armor`,
`meshes/Actors/Character/CharacterAssets` found 452 `BSSubIndexTriShape` shapes, 2 of which
have non-summing sub-segments (`Armor/Metal/MLegL_Heavy_1.nif` b11,
`Armor/RaiderHeavy/FUnderArmor.nif` b124), plus CourserF.

---

## 1. Exact segment attribution for triangle-appending operators

**The gap.** Twelve `Num Triangles` writes in `glview.cpp` can raise the count, across eleven
operators. Exactly one (Join, via `joinMergeSegmentsByIndex`) does exact attribution. The
other ten fall back to `tlSyncSegments`, which puts every appended face in whatever slot
covers the tail.

**Design: one choke point, not eleven.** All three undo commands' `redo()` bodies call
`tlSyncSegments` (`glview.cpp:8663`, `:11527`, `:11610`). Replace with a dispatcher
`tlFixSegments(nif, iShape, mode, oldNT, originTri, permOut)` taking three modes:

| mode | meaning |
|---|---|
| `Exact` | the op already rebuilt the ranges — do nothing (Delete `:9689`, Separate `:9156`; already wired as `segExact`) |
| `Grow` | the op supplied per-triangle parentage — exact rebuild |
| `Sync` | legacy count-only fallback, `tlSyncSegments` unchanged |

Each op declares only what it already knows; all range and permutation logic lives in one
function. The rejected alternative — each op rewriting ranges itself — replicates the maths
ten times, which is precisely the eight-call-sites failure mode this repo already has a
history of.

**Attribution is already available at the append site** for six ops, in a named variable:

| op | variable | file:line |
|---|---|---|
| Duplicate | `dupFaces` (inverse map `fremap` also exists) | `:10695`, `:10799-10808` |
| Loop Cut | `quadTris` (original triangle indices) | `:15622-15655` |
| Edge Cut | loop variable `t` | `:15712`, `:15740` |
| Subdivide | loop variable `t`; quad pass has `q.tA`/`q.tB` | `:13246`, `:13303` |
| Knife | two passes, compose the index spaces | `:15398`, `:15419` |
| Symmetrize | loop position (convert range-for to indexed) | `:16522` |

One field away for three more: Extrude/Inset (`TlExtrudePlan::Wall` gains `int srcFace`,
`:10921`), Bevel (`adj` is already in scope at `:14631`), Fill/Bridge (`tlExtractLoops`
gains an edge→face hash, `:11210`).

**Orphan faces → the largest non-empty slot** (bungo's call, 2026-08-03). Slot 0 is provably
wrong: **measured** empty on every skinned FO4 fixture checked (FemaleBody b61, FOutfit b65,
CourserF b158 all have seg[0] and seg[1] at `Num Primitives` 0). Note `riggingtools.cpp:6226`
uses "reassigned to Segment 0" as its own catch-all wording, which contradicts the corpus.

**Do NOT lift `riggingWriteSegmentLayout`.** See Landmines.

**Stage B, last, because it is the only part that can break undo:** `TlMeshGrowCommand` and
`TlExtrudeCommand` today only shrink `Num Triangles` and restore a subset (`:11535-11550`,
`:11618-11645`), so neither survives a permutation. They need a full pre-op `Triangles`
snapshot (the `tlCaptureValues` call `TlShapeStateCommand` makes at `:8641`) before
Fill/Bridge/Extrude/Inset can move off `Sync`.

**Verification.** Check B: `Clothes/BaseballUniform/FOutfit.nif` block 65 (494 tris, 5 segs,
seg[2] n=239 with 3 subs, seg[4] n=239 with 3 subs). Duplicate a face in [0,239) and assert
`seg[2].NumPrimitives == 240` while `seg[4]` stays 239. **Measured** today's behaviour: +6
growth gives seg[4] 239→245 — the wrong slot grows. Also assert `sum(Num Primitives) ==
Num Triangles`; `tests/spells/bug_sweep.sh` checks only that weaker invariant and would pass
on a wrong answer. Check C: on `FemaleBody.nif` block 61 assert a grow whose origins all land
in the last non-empty slot yields an **identity** permutation, or face picks break for no
reason.

---

## 2. NiPointLight in the viewport

**Decision (bungo, 2026-08-03): all lights, accurately — raise the budget rather than select
a subset.** Nearest-N per object exists to work around a small fixed budget; the honest fix
is to make the array big enough. UBO slots `[1]` and `[2]` of `lightSourcePosition` /
`lightSourceDiffuse` already exist (`glcontext.hpp:287-289`, `uniforms.glsl:6-7`), are
uploaded every frame, and are **read by no shader** (all 20 hits in `res/shaders` index
`[0]`). Two extra lights therefore cost zero layout change; more means changing
`glcontext.hpp:283-303` and `uniforms.glsl:1-21` in lockstep under std140.

**Corpus size (measured):** 101 meshes contain `NiPointLight` (byte scan over
`meshes/**/*.nif`). Heaviest known is `LibertyPrimeLightFX.nif` at 21 lights (blocks
648-710). A per-mesh histogram was started and not finished — **finish it before sizing the
array.**

**This is a controller feature as much as a renderer feature.** **Measured:** 66 of 166
corpus lights have `Dimmer` 0 and 51 have `Diffuse Color` #000000 *at rest* — they only light
up when their sequence runs. Shipping the renderer without `NiLightDimmerController` /
`NiLightRadiusController` / `NiLightColorController` leaves ~40% of the corpus dark and
looking broken.

**Radius lives in `Specular Color`,** as three identical floats — not in any field named
radius. **Measured** on `Effects/WorkingExamples/AnimatedLightStages.nif`: block 59 has
Specular 100/100/100, and its `NiLightRadiusController`'s `NiFloatData` (b7) is Value 100.
Dimmer likewise matches its controller's data. Read `radius = SpecularColor.r`.
`WW_CHANGES.md:308-310` says the radius is in the LIGH form — correct that nif.xml declares
no such *field*, wrong that the number is absent from the mesh.

**Ignore Ambient Color and the three Attenuation floats.** **Measured:** Ambient is #000000
on 166/166; only two attenuation triples exist corpus-wide, `(0,1,2)` ×155 and `(0,1,0)` ×11,
so a physically-correct evaluation gives identical falloff for every light in the game.
Falloff comes from the radius.

**Two choke points.** (a) `glscene.cpp:337-339` — the `NiAVObject` branch builds a `Node`
only for `BSTreeNode`, so a light gets no Node, no `worldTrans()`, no controller dispatch.
One added condition fixes all of it: `Node::updateImpl` already walks `Children`
(`glnode.cpp:258-274`), which is where FO4 lights live (nif.xml gives `NiNode` its `Effects`
array only `vercond #NI_BS_LT_FO4#`, i.e. never for FO4). (b) `glview.cpp:2612-2617` — the
single per-frame UBO write.

**Verification.** Stage A fails today with no shader work: `Effects/ExplosionGrenade01.nif`
blocks 199 (`NiNode 'Omni001'`) and 200 (`NiPointLight 'object0'`) both sit at **measured**
world T=(0, 0, 116.8340) per the CLI `world` command — an oracle independent of the code
under test. Assert a scene Node exists for block 200; today `Scene::getNode` builds nothing,
so it finds zero light nodes. Stage B is two-sided: the seven `tools/render_regression`
fixtures (none contains a point light) must stay at **zero** differing pixels, *and* a new
eighth point-light fixture must differ from its own pre-change capture — a lighting change
that renders identically is not a lighting change. Known flake: `WW_CHANGES.md:6675` records
`particles_mist` differing by 54,581 px cold and matching warm; re-run warm before believing
a red.

**Unresolved:** `Dimmer` is unbounded in real data — **measured** min 0.0, max 2200.0, with
23/166 above 1.0 (`LibertyPrimeImpactLight.nif` b29 is dimmer 2200 at radius 136.4). Used raw
as an intensity multiplier this whites out the frame. Needs a policy; not decided.

---

## 3. NiPSysColliderManager

Pure addition to `PSysSimController`'s existing five-part modifier pattern (header struct +
`QVector`, `clear()` in **reset block 1** at `controllers.cpp:936`, parse arm at `:1169`,
per-frame psys-local prep, application site). Nothing reads any collider block today.

**Insertion point is set by the engine's own ordering.** **Measured:**
`NiPSysColliderManager` is `ORDER_COLLIDER` 5000 and `NiPSysPositionModifier` is
`ORDER_POS_UPDATE` 6000 (blocks 47/49 on `BubblesSurface01.nif`), so the sweep runs
immediately *before* `p.pos += p.vel * dt` at `:1801`.

**Do it predictively**, against `p.pos + p.vel*dt`. The integration is a single unsubstepped
semi-implicit Euler step whose dt is clamped only at 0.25 s (`:1628`, `:1644`) and animSpeed
reaches 4× from the UI, so a discrete "if below plane, reflect" both tunnels and injects
energy on deep penetration. Structure it as a local `bool moved` guarding `:1801`, not a
duplicated loop tail, so the rotation update at `:1803` and `n++` at `:1804` still run.
Even predictive guarantees only one crossing per step — **do not claim swept-continuous
behaviour.**

**Response:** `v' = v - (1+Bounce)*dot(v,N)*N`. **Do not clamp Bounce to [0,1]** —
`WaterQuarrySplashFX.nif` authors 4.0 on two blocks. Push off by an epsilon along N after a
hit, or a settled particle re-triggers every frame.

**Walk the `Next Collider` chain** with a visited-set guard. **Measured:** chains reach 16
links (`VltGearDoor01.nif` manager 1599 builds a 16-plane enclosure), so a head-only
implementation leaks through 15 of 16 walls — and passes the single-link BubblesSurface01
check.

**Skip `Spawn on Collide`** (**measured** false on 327/327, and `NiPSysSpawnModifier` is
itself unimplemented). **Implement `Die on Collide`** (5 blocks, all water splashes) — it
reuses `parts.remove(n); continue;` at `:1761` verbatim.

**Verification.** `Effects/BubblesSurface01.nif` block 47/48 (**measured** Bounce 0.5, Width
200, Height 200, Collider Object 4). Two-sided, because "no particle is past the plane"
passes trivially on broken code if the emitter never aims at it: on the *unfixed* build log
the far-side count and require it > 0, recording the number in the commit as proof the check
can fail. Then `waterSplash.nif` b63 for Die-on-Collide (population must DROP), and
`Actors/Radscorpion/CharacterAssets/RadScorpionDirtParticles.nif` (**measured** 6 managers,
11 colliders, chain 186→187→188→-1) to prove the walk is not head-only.

`glparticles.h:76` needs a `spritePositions()` accessor — `verts` is protected and there is
no way to read particle positions today, so the positional invariant is unmeasurable without
it.

---

## 4. NiPSysBombModifier

Same pattern, one rank earlier. **Measured:** Order 4000 = `ORDER_FORCE` on 193/193 corpus
blocks — the slot gravity and drag already occupy — so it applies beside the spherical-gravity
loop at `:1778-1783`, before drag at `:1785`. Needs **no new per-particle state**: like
spherical gravity it is a pure function of `p.pos` and the bomb's transform.

**Force law** (all fields unconditional in nif.xml): `w = p.pos - O`;
SPHERICAL(0) `dist=|w|, dir=w/dist`; CYLINDRICAL(1) `wp = w - A*dot(w,A), dist=|wp|,
dir=wp/dist`; PLANAR(2) `s=dot(w,A), dist=|s|, dir=A*sign(s)`. Then
`p.vel += dir * (DeltaV * f * dt)` — acceleration, matching how Strength is used at `:1782`.
**Measured:** all nine decay × symmetry combinations occur in real data, so none is dead code
(decay 83/88/22, symmetry 138/24/31).

**Two corpus facts settle what gravity had to guess.** `Bomb Axis` is (0,0,1) on 193/193, so
for the 55 CYLINDRICAL/PLANAR blocks the axis comes **entirely from the Bomb Object node's
world rotation** (third column of `R_w`) — using the raw field gives a plausible-looking
wrong answer. And `Delta V` is authored with **both signs** (**measured** 60/193 negative,
range -9000..13500), so negative = attraction is *read, not inferred* —
`ConstitutionPreLaunch01.nif` b234 is Delta V -2250 on a node named
`'b_LargePuffSuction - ROCKET'`. **Do not take abs(), do not clamp.**

**Say in the commit** that implementing bomb Decay/Decay Type does *not* reverse
`WW_CHANGES.md:262-264` (gravity's Decay/Turbulence left unread). That exclusion was about an
undocumented bare float with no defensible curve; the bomb ships a documented three-valued
enum and all three values occur in numbers. Without that sentence it reads as a reversal.

**Verification.** `Effects/ExplosionFireExtinguisher01.nif` block 50 (**measured**
EXPONENTIAL/SPHERICAL, Decay 128, Delta V 5400, Bomb Object 54 = `NiNode 'PBomb001'` at world
origin with identity rotation) — a spherical bomb at the origin, so the invariant is pure
radial growth with no transform ambiguity. Bake to fixed t, measure max `|p.pos|`; compute the
expected no-bomb bound from the fixture's Speed field rather than eyeballing, and assert
strictly greater. Then `Effects/Quest/MS11/ConstitutionPreLaunch01.nif` b234 (Delta V -2250,
NONE/PLANAR, strongly rotated bomb node): assert mean distance from the bomb *plane*
**decreases**. That check is what proves the negative sign survived and that the axis came
from the node rotation rather than the constant.

---

## Landmines

1. **`riggingWriteSegmentLayout` (`riggingtools.cpp:456`) must not be lifted.** **Measured:**
   in every FO4 file checked, a Sub Segment's `Parent Array Index` names its *parent
   segment's* `Segment Starts` value, not its own entry — FOutfit b65 subs carry 2 and 7
   against Segment Starts 0,1,2,6,7; FemaleBody b61 carry 2 and 8; CourserF b158 carry 2 and
   9 — while segments carry 0xFFFFFFFF. `riggingReadSegmentDefinitions` (`:406-435`) resolves
   a child through that index, so all five of CourserF seg[2]'s subs read segment 2's own
   entry and the real per-sub Bone IDs (3001185871, 1875114930) and Cut Offsets are lost; the
   writer then emits fresh sequential indices at `:502-517`. Lift only
   `riggingReadSegmentRanges` (`:360`) and `riggingReadTriangleMembership` (`:438`), which
   consult Start Index / Num Primitives only.
2. **A permuting segment fix invalidates face picks.** `PickedElement` stores a raw triangle
   index (`glview.h:766`), consumed at `:10659`, `:13116`, `:14201`. The dispatcher must
   return the permutation and Duplicate must remap. Any op not enumerated above that retains
   face picks across the command will silently point at wrong triangles.
3. **`riggingReadTriangleMembership` defaults `segmentOf = 0`** for a triangle covered by no
   range — and slot 0 is empty on every skinned fixture, so that is exactly the wrong
   destination. Use the largest non-empty slot.
4. **Collider plane orientation comes from the Collider Object node, not the block.**
   **Measured:** X Axis is (1,0,0) and Y Axis is (0,1,0) in 312/312 corpus blocks, while
   163/327 collider objects have a non-identity world rotation. Building the plane from the
   fields alone gives a horizontal plane for every collider in the game and looks plausible
   on half the corpus. This is the silent-failure trap.
5. **Resolve modifier-referenced nodes per frame**, the `InheritVel` form at `:1726`, not
   cached at `update()` as gravity does at `:1148-1156`. The codebase argues this itself at
   `:1067-1070`: during `update()` the scene graph may not contain the node and `worldTrans()`
   returns identity.
6. **Backward time scrubs restart the simulation** (`:1629-1640`: `parts.clear()`, emitter
   accum reset, `iv.havePrev = false`). Any collider or bomb state living outside the `parts`
   vector must be reset in that same block.
7. **`glcontext.hpp:283-303` must stay byte-compatible with `uniforms.glsl:1-21`** under
   std140. If the light array is raised, both change in lockstep — otherwise every shader
   silently reads garbage, and the symptom is corrupted projection and viewport uniforms,
   which looks nothing like a lighting bug.
8. **Re-run `qmake6 -o Makefile NifSkope.pro`** before building the round that adds a member
   to `Scene`. `CURRENT_STATUS.md:46-49`: adding data members to widely-included classes
   without regenerating the dependency list once linked an old-layout `.o` and hard-crashed
   at startup.

## Open questions, by how much they would cost if wrong

**Would change the design:**
- Is `Delta V` a per-second acceleration (nif.xml's wording, assumed) or a one-shot impulse?
  Measured magnitudes (900, 2700, 9000) are large for sustained acceleration on a particle
  whose emitter speed is often under 200 u/s. If one-shot, a per-particle "bomb applied" flag
  is needed on `SimParticle` — the only thing in these two features that would touch it.
- Are collider `Width`/`Height` full or half extents? Factor-of-2 error. Nothing in schema,
  docs or values settles it. (Assumed full, by analogy with 3ds Max's Deflector helper, whose
  node naming `Deflector001`/`SDeflector001` this data clearly came from.)

**Visible on screen, tune against a screenshot:**
- `DECAY_LINEAR` form: `max(0, 1 - dist/Decay)` (assumed) vs `Decay/(Decay + dist)`. These
  disagree strongly at the small Decay values in real data (`AnimObjectTorchBottle.nif` b102
  Decay 3, `AnimObjectBlowTorch.nif` b149 Decay 2).
- `DECAY_EXPONENTIAL`: `exp(-dist/Decay)` (assumed, because exponential bombs carry Decay
  128/200/300 and `exp(-dist*300)` would make all 22 invisible) vs `exp(-dist*Decay)`.
- `Dimmer` policy for the 23/166 lights above 1.0.

**Not distinguishable from stored data:**
- Planar collider one-sided or two-sided (assumed two-sided).
- Collider chain first-hit-wins or earliest-impact (assumed earliest, as physically correct).
- PLANAR bomb pushes both ways (assumed) or unconditionally along +A.
- What `Order` means when two modifiers share a rank — every measured block sits at its
  type's default, so ties were never exercised.

**Never checked:**
- Are FO4 collider objects ever *animated*? Two files show `Controller = -1` on every collider
  object, but whether an **ancestor** moves them — the case that decides the per-frame
  resolution question — was never checked, and 72 of 74 files were not sampled.
- `SwanWaterSplash.nif` has the only 2 blocks whose owning particle system has
  `World Space = no`. Whether collider data is then read relative to the particle system node
  was not worked out.
- The `Fo4PDB` route (`E:\Projects\Fo4CommunityShaders\Fo4PDB` via `tools/exere/f4pdb.py`)
  could settle the collider and bomb response semantics from the engine itself. Not used.
  `particles.md:69-70` is two one-line table rows and says nothing about bounce, chaining or
  spaces.
- **No operator was ever run.** Every claim about op behaviour above is from reading code;
  the `tlSyncSegments` effects were established with a faithful Python port, not the binary.
  The harnesses are the first actual execution.

## Separate bugs found in passing — not fixed, each deserves its own task

1. **`riggingWriteSegmentLayout`'s `Parent Array Index` convention disagrees with every
   fixture** (see Landmine 1). The three rigging spells that call it
   (`riggingtools.cpp:3706`, `:5328`, `:5764`) presumably corrupt sub-segment shared data on
   any file with sub-segments. **Not run, not confirmed.**
2. **`addPrimitive` clones a stale segment table.** `glview.cpp:16741`/`:16782` —
   `tlCloneShapeWithProps` copies the template's `Segment` table verbatim (`tlCloneBlock` at
   `:8602`) and `TlBlockAppendCommand::redo` (`:8768-8776`) never syncs, so a new Cube
   inherits a body's dismemberment table pointing far past its 12 triangles. The same clone
   path serves Separate (`:9317`) and object-mode Duplicate (`:10760`).
3. **The other `Num Triangles` writers were never checked** for the same segment problem:
   `bakegeom.cpp:215`, `spells/simplify.cpp:595`, `spells/mesh.cpp:932`, and
   `lib/importex/*`. Scope of this pass was `glview.cpp` only.

## Sequencing, when it resumes

Build costs, measured at -O3 in MSYS2 UCRT64: `glview.cpp` 33 s, `nifskope_ui.cpp` 28 s,
`controllers.cpp` 9 s, `glscene.cpp` 5 s, `glnode.cpp` 5 s. `glnode.h` is included by
`glview.cpp`, so touching it costs the 33 s.

1. `controllers.{h,cpp}` — bomb, then collider, then the three light controllers. One TU.
   Bomb first: it is a pure force addition with no per-particle state and no integration
   surgery, so it proves the parse arm, the reset block and the `modIsActive` gate before the
   harder predictive sweep goes in.
2. `glscene.{h,cpp}` + `glnode.{h,cpp}` — light nodes and the Scene light list. Re-run qmake6.
3. `glview.cpp` — the segment dispatcher, the ten `originTri` fills, the Duplicate pick remap,
   and the light UBO upload. Unrelated features sharing one 33 s TU; touch it once.
4. `nifskope_ui.cpp` — all four harnesses in one pass. `WW_DELETE_TEST` currently has zero
   `check()` calls.
5. `glview.cpp` again — Stage B: `Triangles` snapshots on `TlMeshGrowCommand` /
   `TlExtrudeCommand`, then route Fill/Bridge/Extrude/Inset off `Sync`. Last, because it is
   the only part that can corrupt undo.

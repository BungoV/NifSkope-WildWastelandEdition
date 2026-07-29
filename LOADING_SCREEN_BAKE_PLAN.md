# Loading Screen Bake — concept

Turn an assembled, posed rig into a single static NIF the game can use as
loading-screen art. Design only; nothing here is built yet.

Input is what the merge already produces: skeleton first, armour skinned to it,
effects attached by `AttachT`, a pose applied over the whole thing. Output is one
file with no skeleton, no skin and no controllers.

---

## 0. The format is not a guess

Fallout 4 ships the exact thing we are making:
`meshes/LoadScreenArt/Armor03PowerArmor8X01.nif` — the vanilla X-01 loading
screen. **25 blocks**, and every design question is answered by reading it.

```
[0]  NiNode 'Armor03PowerArmor8X01'      identity, 9 children
[1]  BSTriShape 'PA_X1_Helmet002:0'      + BSLightingShaderProperty + BSShaderTextureSet
[4]  BSTriShape 'PA_X1_Body001:1'        translation (-0.19, -7.11, 111.27), rotation IDENTITY
[7]  BSTriShape 'PA_X1_LArm002:2'
[10] BSTriShape 'PA_X1_RArm001:2'
[13] BSTriShape 'PA_X1_LLeg001:3'
[16] BSTriShape 'PA_X1_RLeg001:3'
[18] BSTriShape 'PAFrameBOS001:0'
[21] NiNode 'LoadingMenuZoomTarget'      translation (0, 19.08, 139.89), no children
[22] BSSubIndexTriShape 'FusionCore01:0'
```

What that tells us, each point load-bearing:

| observation | consequence for the bake |
|---|---|
| Every armour piece is a plain **`BSTriShape`** — no `BSSkin::Instance`, no `BoneData` | the skin is **evaluated away**, not carried |
| **No `NiNode`s except the root and the zoom target** | the entire skeleton is **dropped** |
| This file has no controllers | **but see §0b — most do not, some do, and that changes the plan** |
| Each shape has a **non-zero translation and an identity rotation** | vertices stay near a **local origin**; the node places the piece |
| Block 4's first vertex is `(4.04, 29.36, -3.28)` while its node sits at `Z = 111.27` | confirms the above — and the values are quantised in 1/256 steps, i.e. **half-float** |
| A bare `NiNode` named exactly **`LoadingMenuZoomTarget`** | the camera focus point is part of the file — **optional, see §0b** |

The half-float point is the one that would have bitten us. Baking world-space
vertices straight into the array puts coordinates around 111.0 into half floats,
where the step is ~0.0078 units — visible faceting on a helmet. Vanilla avoids it
by keeping each piece around its own origin. **We must do the same:** per shape,
translation = the piece's world centroid, vertices = world position − centroid.

## 0b. One file is not a corpus

Generalising from `Armor03PowerArmor8X01.nif` produced two wrong claims. Every
`.nif` under `meshes/LoadScreenArt` — **173 files** — says this instead:

| block | files | reading |
|---|---|---|
| `NiNode` / `BSLightingShaderProperty` / `BSShaderTextureSet` | 173 / 172 / 172 | the floor |
| `BSTriShape` | 167 | static geometry is the norm |
| `NiAlphaProperty` | 147 | |
| **`BSEffectShaderProperty`** | **27** | effect-shader geometry is normal here |
| `BSSubIndexTriShape` | 24 | segmented shapes survive |
| **`NiTransformController` / `NiTransformInterpolator`** | **18** | **loading screens MOVE** |
| `NiFloatInterpolator` / `NiFloatData` | 11 | |
| **`BSEffectShaderPropertyFloatController`** | **9** | **effect shaders ANIMATE** |
| `BSLightingShaderPropertyFloatController` | 4 | |
| `BSEffectShaderPropertyColorController` | 3 | |
| `NiControllerManager` + `NiControllerSequence` + `NiDefaultAVObjectPalette` | 1 | one carries a whole animation sequence |
| `bhkPhysicsSystem` / `bhkNPCollisionObject` | 1 | one even has collision |
| **any `NiPSys*`, `NiParticleSystem`, `BSProceduralLightningController`** | **0** | **never, not once** |

`LoadingMenuZoomTarget` appears in **65 of 173** — a strong convention, not a
requirement.

So the two corrections:

- **Controllers do not have to be stripped.** "Bake the animation to values at T"
  was over-fitting to one static file. Transform and shader controllers are
  ordinary here, and the X-01 Tesla ribbons are made of exactly the blocks 27
  vanilla screens already use.
- **Particles genuinely have no precedent.** Zero occurrences in 173 files is not
  proof the engine refuses them, but it is the whole of the evidence, and betting
  a loading screen on an untravelled path is a poor trade for a still image.

---

## 1. Bake rules, per block kind

### 1a. Skinned shapes → static shapes

For each vertex: `world = Σ wᵢ · (boneWorldᵢ × bindInverseᵢ) · v`, which is what
`Shape::skinVertex` already computes and what `WW_SKELMERGE_TEST` already reads
back. Then:

- subtract the shape's centroid, write into `Vertex Data / Vertex`;
- **rotate normals, tangents and bitangents by the same weighted matrix** —
  positions alone leave the lighting wrong in a way that reads as "the bake
  worked" until you look at a curved surface;
- clear the `Skin` link; delete the now-orphaned `BSSkin::Instance` and
  `BSSkin::BoneData`;
- clear the skinned bit in `Vertex Desc` and drop the per-vertex weight/index
  fields, shrinking the stride (this is the `Data Size` landmine from
  `WW_CHANGES` 07-18b — the desc and the array must move together);
- `BSSubIndexTriShape` → segments are meaningless without a skin. Vanilla keeps
  `BSSubIndexTriShape` only for the fusion core, which is a prop. **Convert to
  `BSTriShape`.**

### 1b. Rigid shapes under posed bones → static shapes

No skin, but a posed parent chain. Fold the accumulated world transform into the
shape node (translation **and** rotation — we are not bound to vanilla's
identity-rotation convention, and an exact transform beats a re-derived one), and
re-centre vertices as above.

### 1c. Controllers → kept

Transform and shader controllers stay live. §0b: 18 vanilla screens move, 13
animate a shader, one runs a full sequence. Baking them to constants at T would
throw away something the format supports and Bethesda uses.

A `--freeze` switch can still evaluate them into their driven fields and delete
them, for the case where a genuinely motionless still is wanted. Off by default.

### 1d. What the X-01 Tesla FX is actually made of

Each effect file is two separable halves, and only one is a problem:

| per FX file | blocks | vanilla precedent |
|---|---|---|
| `BSTriShape` ×2–5 + `BSEffectShaderProperty` ×3–5 + `BSEffectShaderPropertyFloatController` ×2–4 | the glowing ribbons | **yes — 27 files** |
| `NiParticleSystem` + `NiPSysData` + 9 `NiPSys*`/`BSPSys*` modifiers + `BSPositionData` | the sparks | **none, 0 of 173** |
| `BSProceduralLightningController` ×1–3 | the arcs | **none, 0 of 173** |

(`x01tesla_helmet_fx.nif` carries none of the three — worth knowing before
wondering where the helmet glow went.)

So **most of the effect survives untouched and animated**, which was not on the
table when this was framed as freeze-or-drop. What is left to decide is only the
particle systems and the procedural lightning:

1. **Drop them** — keeps the ribbons animating, loses sparks and arcs, zero risk,
   zero extra work. *Recommended.*
2. **Freeze the lightning into effect-shader geometry** — run to T (which
   `WW_SHOT_TIME` already does), read the generated arc vertices out of the
   renderer, emit a `BSTriShape` with the same `BSEffectShaderProperty`. This
   converts the arcs **into** the vanilla idiom rather than out of it, so they
   keep their glow and can even keep a float controller. Real work, but the
   result is a file the engine has seen the shape of before.
3. **Ship the particle systems as-is** — no vanilla precedent whatsoever. Not
   recommended, and if tried, it needs an in-game check before anything is built
   on top of it.

### 1e. Stripped outright

Skeleton `NiNode`s, `BSConnectPoint::Parents` / `::Children`,
`NiStringsExtraData 'AttachT'`, and any node left with no descendants that draw.

**Not** `BSXFlags` (38 vanilla files have one), **not**
`BSBehaviorGraphExtraData` (9 have one), **not** collision (one file has both
`bhkNPCollisionObject` and `bhkPhysicsSystem`). The single reference file lacking
them proved nothing; the corpus says leave them.

### 1f. The zoom target

Add one `NiNode` named exactly `LoadingMenuZoomTarget`, no children — optional
(65 of 173) but worth having. Default position: the world position of a chosen
bone at time T (`HEAD` gives vanilla's ~Z 140 on a power-armour frame), with an
editable offset. Getting the name wrong costs nothing visible in NifSkope and
everything in game, so it is asserted in the test rather than trusted.

---

## 2. Shape of the work

1. **`bakeSceneAt( NifModel *, float t, const BakeOptions & )`** on the model/GL
   boundary — it needs `Shape::skinVertex`, so it lives where the scene does, and
   returns a fresh `NifModel` rather than editing in place.
2. **CLI**: `nifcli bake <file> --time 1.5 --zoom-target HEAD -o out.nif`, so the
   whole pipeline — merge, pose, bake — is scriptable end to end and testable
   without a window.
3. **GUI**: a Pose Manager button (**Bake to loading screen…**), because the pose
   is the input and that is where the user already is. Asks for time, zoom
   target, output path.

## 3. How it gets verified

- **Block census against the corpus, not against one file**: 0 skin blocks, 0
  `NiPSys*`, 0 `BSProceduralLightningController`, no `NiNode` left that no shape
  hangs from. Controllers are *expected*, not forbidden.
- **Geometry is the same geometry**: every baked vertex must equal
  `skinVertex()` at T to within half-float precision — the same measurement
  `WW_SKELMERGE_TEST` already makes, reused as the acceptance test.
- **Vertices stay local**: no |coordinate| beyond a few hundred units, which is
  the check that catches a missed re-centring before it becomes faceting.
- **It renders**: `WW_SHOT` on the baked file against `WW_SHOT_TIME=T` on the
  live one — same silhouette, or the bake lost something.
- **In game**: yours. Nothing here can prove the loading screen frames correctly.

## 4. Risks

- **Stride rewriting** is the one genuinely dangerous step (`Vertex Desc` +
  `Data Size` + array size must agree). `WW_VERTEXFLAGS_TEST` already exercises
  exactly this path and should be extended rather than worked around.
- **Half-float precision** — mitigated by re-centring, verified by the
  coordinate-range check above.
- **Material paths** ride along unchanged; a merged mod part keeps its own
  `.BGSM`, which is correct, but the bake cannot tell you whether the game will
  find it.

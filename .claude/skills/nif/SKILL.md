---
name: nif
description: Use when working with Bethesda NIF mesh files — inspecting/modifying hierarchy, transforms, textures, animations, connect points, collision, copying or merging blocks between NIFs, creating new NIFs, or understanding NIF internals (scene graph, shaders, particles, behavior integration). Covers the headless nifskope-cli of the NifSkope WildWasteland fork on this machine, plus the NIF architecture reference.
user-invocable: true
---

# NIF Tools & Architecture

## NIF tooling ON THIS MACHINE — `nifskope-cli`, not `modkit nif`

> **`modkit nif` is NOT AVAILABLE ON THIS MACHINE.** Our NIF tool is the **NifSkope
> WildWasteland Edition fork** at `E:\Projects\NifskopeWildWastelandEdition` (public:
> github.com/BungoV/NifSkope-WildWastelandEdition, branch `main`, handoff in `HANDOFF.md`,
> changelog in `WW_CHANGES.md`). It has a **headless batch mode** that covers most of what
> `modkit nif` did, plus a large amount that `modkit nif` never had (collision decode/roundtrip,
> ragdoll simulation, poses, LOD generation, FO76 terrain).

**Running it:**

```powershell
$nif = "E:\Projects\NifskopeWildWastelandEdition\release\nifskope-cli.cmd"
& $nif info "E:\Tools\Fallout 4\DataUnpacked\Data\Meshes\Weapons\10mmPistol\10mmCompensator.nif"
```

The `.cmd` wrapper exists because `NifSkope.exe` is linked `-subsystem,windows`, so a bare
invocation returns before any output exists; the wrapper uses `start /b /wait` so stdout,
stderr and the exit code behave like a normal console tool. **Always use the wrapper.**
Full docs: `E:\Projects\NifskopeWildWastelandEdition\docs\CLI.md`.

**No sessions.** `nifskope-cli` is stateless: file in, `-o` file out, one process per command.
There is nothing to `open`, `save` or `close`, and **no batch command is needed or exists** —
the upstream "always batch 2+ calls" rule simply does not apply here.

### Command mapping

| `modkit nif …` | Ours | Notes |
|---|---|---|
| `inspect --path F --block -1` | `nifskope-cli list F [-t TYPE]` (block list) / `info F` (version + per-type tally) / `skeleton F` (tree) | verified |
| `inspect --path F --block N` | `nifskope-cli dump F -b N [-f PATH] [-d DEPTH] [-n MAX] [--all]` | `dump` hides rows this file's version/conditions exclude, as the GUI does — `--all` shows them. **This matters on `BSVertexData`:** both precision variants of `Vertex` exist as items and the dead one reads as zeroes, so without the filter a healthy mesh looks corrupt. |
| (one field) | `nifskope-cli get F -b N -f PATH` | field paths are `/`-separated, a numeric segment indexes an array row: `-f "Bone List/0/Bounding Sphere/Radius"`. Verified. |
| `modify <sid> <block> '<json>'` | `nifskope-cli set F -b N -f PATH -v VALUE -o OUT` | one field per call, and it writes a new file rather than mutating a session. Compound values are comma-separated: `-v "99.0,42.0,7.0"`. |
| `new <path>` | `nifskope-cli new -o OUT [--cube [--size N]]` | starter document |
| `copy <src> "<ids>" <tgt> --attach-to N` | `nifskope-cli merge F --add OTHER.nif [--add …] [--attach NODE] [--no-dedupe] -o OUT` | **not the same operation.** `merge` splices whole files and **de-duplicates `NiNode`s by name** so merged pieces share one rig — that is its point. For a single-block copy with dependency-tree resolution, use the GUI's copy/paste-branch (the fork's `tools/copypaste_test` harness exercises it). |
| `add <sid> <type> --fields … --attach-to N` | **partly** — `nifskope-cli cast F -s "Page/Name" [-b N] -o OUT` runs any named spell headlessly, and `anim-setup` adds controller/interpolator/sequence blocks. There is **no generic "add a block of type X with these fields"** on the command line. | for anything else, do it in the GUI or write a spell. |
| `remove <sid> "<ids>"` | **NOT AVAILABLE ON THE CLI** — no headless block-remove with Ref/Ptr remapping. | GUI, or a spell cast via `cast`. |
| `collision <sid> …` (generate) | `nifskope-cli collision F` (inventory), `--extract -b N -o F.bin`, `--roundtrip`, `--constraints`, `--skeleton`, `--bodies` | **inspection and re-encode, not generation.** Collision *generation* lives in the GUI (the mesh-shape builder, shipped 2026-08-23; NifSkope-compiled FO4 collision works in game). Verified: `collision` on a vanilla 10mm part decodes the packfile and reports 7 shapes / 1 body. |
| `batch '<json>'` | **not needed** — no sessions, no round-trip cost worth batching. | |

### Commands `modkit nif` never had

`check F [-t NAME]` (Issue Manager scan headless, exit 1 on findings) ·
`segments F [-b N]` (FO4 dismemberment table) ·
`world F [-b N] [-t TYPE]` (each `NiAVObject`'s **world** transform — diff two files by name to
prove a conversion put things where the skeleton had them) ·
`transfer-normals F --from N --to M [--mapping 0..5] [--mix F] -o OUT` ·
`simulate F [--steps N]` (ragdoll solver headless) ·
`pose F --list | --save NAME | --apply NAME [--blend F] | --import-os / --export-os` ·
`freeze F --sequence NAME --time T -o OUT` ·
`btd F.btd [--info] [--region X0 Y0 X1 Y1] [--lod 0..4] -o OUT.nif` (FO76 terrain) ·
`lodgen F.esm …` (LOD generation) ·
`loading-screen F …` ·
`spells [pattern]` (list every spell addressable by name for `cast`).

### Other NIF tools here

- **Python parsers** in `E:\Projects\NifskopeWildWastelandEdition\tools\`: `hkparse.py`
  (Havok packfile header), `hkdump.py`, `hkcompound.py`, `hkbodyflags.py`, `hkinertia.py`,
  `blobswap.py`, `collision_ab.py`, `batch_validate.py` (whole-directory `bhkPhysicsSystem`
  decode), `ba2get.py` (pull one file out of a GNRL BA2).
- **`corpus_verify.sh`** — regenerate a broad random sample of vanilla meshes and hold every one
  against the original. This is the house rule in tool form: **if you are unsure a file writer
  works, regenerate the shipped files from their own inputs and diff — the whole corpus, not a
  sample.**
- **PyNifly** notes and the `76PyniflyTools` / `76NifConverter` / `76CollisionConverter` folders
  under `E:\Tools\Fallout 4\`.

<important if="launching NifSkope's GUI">
**One NifSkope instance ever**, always on the second monitor (`WW_WINDOW_AT`, position
`1920,0`), never `SetForegroundWindow`, and harnesses take `--port <unused>`. Headless
`nifskope-cli` invocations are exempt — they open no window (verified). After any landed change,
verify the deployed exe **and tell bungo his open window needs a restart.**
</important>

### Finding NIFs

`modkit data search nifs` / `get nifs` — **NOT AVAILABLE ON THIS MACHINE** (no asset index).
Search the unpacked corpus directly instead:

```powershell
# THE corpus. Never a mod folder.
$fo4 = "E:\Tools\Fallout 4\DataUnpacked\Data"
Get-ChildItem "$fo4\Meshes" -Recurse -Filter "*.nif" | Where-Object Name -match "laser"
# by behavior / material / texture reference -- the strings are in the file:
Select-String -Path "$fo4\Meshes\Weapons\**\*.nif" -Pattern "PlasmaCasterFX" -Encoding byte -List
```

Corpora: **FO4 = `E:\Tools\Fallout 4\DataUnpacked\Data`, FO76 = `E:\Projects\F76\Data`.**
Mod folders are never the corpus.

---

## Reference Files

Read the appropriate reference file when you need deeper knowledge about a specific topic. Each file is self-contained and focused.

| Topic | Reference File | When to Read |
|-------|----------------|-------------|
| Scene graph, block types, shaders, extra data, collision, connect points, BSXFlags | `references/scene-graph.md` | Understanding NIF structure, block types, shader properties, attachment points |
| Animation controllers, interpolators, keyframes, text keys | `references/animation.md` | Working with NiControllerSequence, understanding controller/interpolator architecture |
| Particle systems (emitters, modifiers, billboard meshes) | `references/particles.md` | Creating or analyzing particle effects (smoke, fire, sparks) |
| Behavior graph integration (BGED, BGSGamebryoSequenceGenerator, variables) | `references/behavior-integration.md` | Linking NIFs to behavior graphs, understanding variable binding and multi-NIF weapons |
| Weapon FX patterns + case studies (Minigun heat, Shishkebab fire, Meltdown UI) | `references/weapon-fx-patterns.md` | Designing weapon visual effects, studying proven patterns |
| Practical recipes (NIF bashing, common operations, creation patterns) | `references/nif-bashing.md` | Step-by-step guides for common tasks (add nodes, copy blocks, create NIFs) |
| Face customization remap data (chargen sculpt weights, binary format, creating face meshes) | `references/face-customization-remap.md` | Working with FO4 face meshes, CustomizationRemapData, chargen face sculpting, creating new faces |

### Reference File Table of Contents

**`references/scene-graph.md`**
- Scene graph overview with weapon example
- Node types (NiNode, BSFadeNode, NiBillboardNode, OrderedRenderingNode)
- Mesh shapes (BSTriShape, BSDynamicTriShape, BSSubIndexTriShape)
- Shaders (BSLightingShaderProperty, BSEffectShaderProperty, controlled variables)
- Extra data blocks (BSXFlags, BGED, NiStringExtraData, BSBound, etc.)
- Collision blocks (bhk* types)
- BSXFlags reference (flag values + common weapon value)
- Connect points (BSConnectPoint::Parents/Children, standard names P-Barrel, P-Scope, etc.)

**`references/animation.md`**
- Controller/interpolator/keyframe architecture diagram
- NiControllerSequence properties and naming conventions
- ControllerLink (controlled block) structure
- All controller types (NiTransformController, NiVisController, BSEffectShader*Controller, NiLight*Controller, NiPSys*Ctlr)
- All interpolator types (NiTransformInterpolator, NiFloatInterpolator, NiBoolInterpolator, NiBlend*Interpolator)
- Keyframe data examples
- Text key markers

**`references/particles.md`**
- Particle system architecture (NiParticleSystem + NiPSysData + emitters + modifiers + billboard mesh)
- Emitter types (box, sphere, cylinder, mesh)
- All modifier types (age/death, spawn, LOD, color, rotation, scale, gravity, drag, wind)
- Controlling particles with NiControllerSequences
- Dual controlled blocks for NiPSysEmitterCtlr (float + bool)
- Complete smoke effect example (SawedOff)
- Particle rendering (NiParticleSystem + billboard mesh)
- Common particle textures

**`references/behavior-integration.md`**
- BSBehaviorGraphExtraData (BGED) and file structure
- BGSGamebryoSequenceGenerator properties and variable binding
- Multi-NIF weapon architecture (shared behavior graph)
- Key behavior patterns (damping, crossfade, timer, state machines)
- AnimationFileData text file format
- Creating custom behavior-driven effects (NIF + behavior side)

**`references/weapon-fx-patterns.md`**
- Pattern catalog (heat/cool blend, multi-stage, charged weapon, melee FX overlay, material toggle, generic VFX lifecycle)
- Case study: Minigun barrel heat (full behavior tree + flow)
- Case study: Shishkebab fire (dual particles, dynamic lighting, asymmetric damping)
- Case study: Meltdown overheat UI bar (physical UI meshes, UV scrolling, multi-stage events)

**`references/nif-bashing.md`**
- Inspecting NIFs (overview, specific blocks, vertex data)
- Adding nodes (bones, extra data, BSXFlags, BGED)
- Modifying fields (names, textures, shader materials)
- Copying between NIFs (subtrees, multiple blocks)
- Removing blocks with reference remapping
- Creating new NIFs from scratch
- Common structure patterns (weapon, effect mesh, particle NIF)

**`references/face-customization-remap.md`**
- CustomizationRemapData binary format (12-byte per-vertex records: 4× float16 weights + 4× uint8 bone indices)
- CustomizationRemapNewBonesData format (bone name + transform matrix)
- How chargen sculpt sliders use remap weights (separate from animation skinning)
- FacialBoneRegionUIRemapping text file format
- Strategy table for creating new face meshes (copy vs recompute)
- Python encode/decode example code

## A ZERO-HIT CORPUS SCAN IS NOT A MEASUREMENT UNTIL THE METHOD FINDS POSITIVES (lane PHANTOM4, 2026-09-08)

**When a corpus scan returns zero hits, run the same scan on files that MUST match before you believe the
zero.** PHANTOM4 scanned 34,985 NIFs for a `BSTriShape` header and got nothing, twice. The layout was
wrong: **`numTriangles` is a `uint32` on BSVER 130 (FO4), not the `uint16` of earlier games.** Running the
heuristic on files known to contain `BSTriShape`s found nothing there either, which is what exposed it;
with the `uint32` corrected the known files parse and the target was found on the next pass.

The working header test:
`struct.unpack_from('<IHI', data, i)` -> `(numTriangles, numVertices, dataSize)` with
`dataSize == numVertices * vertexSize + numTriangles * 6`, `vertexSize` in {20, 24, ...}. It identified
`SetDressing\Doors\PaintedWoodDoorDoubleLoad01.nif` from two draws' geometry alone.

**A search tool that has never been shown to find anything has not searched.**


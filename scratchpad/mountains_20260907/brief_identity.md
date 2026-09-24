# Lane IDENTITY — is vertex-colour object identity a good idea?

Read-only analysis. **Never build. Never launch a GUI. Never edit a file in the
repo.** Your only writes go to `C:\Users\bungo\AppData\Local\Temp\claude\laneb\`.

Write your report to
`C:\Users\bungo\AppData\Local\Temp\claude\laneb\report_identity.md`
**incrementally, as you go** — append each lens's findings the moment you have
them, do not hold everything to the end. A lane that dies with nothing on disk
delivered nothing.

You are a full Claude Code session and may spawn your own subagents; they are
billed to this account, which is the point of running here. Fan out across the
five lenses below in parallel, then run an adversarial refutation pass over what
they return. Do not accept a finding you have not personally checked in the files.

---

## THE QUESTION

bungo asked: **"is vertex colour based identity a good idea btw?"**

### The design under review

The LOD generator at `E:\Projects\NifskopeWildWastelandEdition` builds per-cell
object LOD chunks (`.bto`) for Fallout 4. It MERGES many placed objects into a few
large `BSTriShape`s grouped by material, to cut draw calls. To let the consumer —
a separate renderer, FO4 Community Shaders ("FO4CS") — recover WHICH placed object
each vertex came from, it writes a 16-bit per-chunk object index into vertex colour
**R (low byte) and G (high byte)**. B holds baked ambient occlusion, A holds tree
sway weight. UV2.x holds sky visibility, UV2.y a texture-array layer, Eye Data a
ground-contact blend. A sidecar manifest maps each index back to the placement.

### Read these first, they are authoritative

    E:\Projects\NifskopeWildWastelandEdition\docs\LODGEN_VERTEX_PACKING.md   (ALL of it — this is the interface contract)
    E:\Projects\NifskopeWildWastelandEdition\docs\LODGEN_IMPOSTOR_SPEC.md
    E:\Projects\NifskopeWildWastelandEdition\docs\LODGEN_PLAN.md
    E:\Projects\NifskopeWildWastelandEdition\docs\TO_BE_IMPLEMENTED.md       (around line 205-235)
    E:\Projects\NifskopeWildWastelandEdition\src\lodgen.h                    (LodgenObjectOptions)
    E:\Projects\NifskopeWildWastelandEdition\src\lodgen.cpp                  (lodgenBuildObjectChunk from ~2660;
                                                                              identity colour written ~3096;
                                                                              shader flags ~3502;
                                                                              lodgenMergeChunkShapes; lodgenSimplifyFarRings)
    E:\Projects\NifskopeWildWastelandEdition\tests\spells\lodgen_identity.sh (what is actually gated today)

A read-only CLI you MAY run (it is not a build and not a GUI):

    cd /e/Projects/NifskopeWildWastelandEdition
    ./release/NifSkope.exe -no-gui lodgen --dump-geometry <file.bto>
    ./release/NifSkope.exe -no-gui lodgen --dump-shapes   <file.bto>

Vanilla corpus: `E:\Tools\Fallout 4\DataUnpacked\Data`
(object LOD chunks under `Meshes\Terrain\Commonwealth\Objects\*.BTO`).

### HARD CONSTRAINTS any alternative must respect

* Stock Fallout 4, with FO4CS **off**, must still render these `.bto` files
  correctly. That is a stated charter requirement, the zero-effort fallback.
* The generator writes standard FO4 NIF. It cannot invent block types the stock
  engine would choke on.
* FO4CS is a d3d11 shader-and-hook layer: it CAN bind its own structured buffers,
  use SM5 features, and read sidecar files.
* Draw-call count matters. Merging objects is the whole reason the problem exists.

---

## ALREADY MEASURED BY THE OVERSEER — do not redo, but DO challenge

1. **Vanilla FO4 object LOD carries no vertex colours at all.** `--dump-geometry`
   reports `ids 0` on `Commonwealth.16.0.0.BTO`, `Commonwealth.4.-20.24.BTO` and
   `Commonwealth.8.-24.24.BTO`. `ids` is gated on `VF_COLORS` in the vertex
   descriptor at `src/nifcli.cpp:5507`, so `ids 0` means the attribute is absent,
   not that the colours are black.
2. **The generator SETS `SLSF2_Vertex_Colors` when identity is on.**
   `src/lodgen.cpp:3504`:
   `nif->set<quint32>( iShader, "Shader Flags 2", opts.identity ? ( 5U | 0x20U ) : 5U );`
   and `build/nif.xml` gives Shader Flags 2 **bit 5 = `Vertex_Colors`**.
3. **That contradicts the contract.** `docs/LODGEN_VERTEX_PACKING.md` line ~9 says
   every slot is "either a field the vanilla shaders do not sample, or a field they
   sample only behind a flag we leave clear". This flag is not left clear.
   `docs/TO_BE_IMPLEMENTED.md` ~line 232 lists the stock-engine tolerance test for
   the fatter descriptor and for vertex alpha as **OWED**, i.e. never run.
4. `objectIndex` at `src/lodgen.cpp:2956` is incremented with no bound and encoded
   with `& 0xFF` / `>> 8 & 0xFF`, so past 65,536 it wraps silently.

**The single most valuable thing you can settle** is whether Fallout 4's LOD object
shader path actually honours `SLSF2_Vertex_Colors`. If it does, the fallback is
broken today and every LOD object multiplies toward black. If it does not, this is
a documentation defect only. Evidence routes, in order of strength:

* Todd's treat (1.10.155), with
  the Todd's treat tooling (kept outside this repo). Quote the RVA.
* `Fallout4 - Shaders.ba2` in `X:\Programs\Steam\steamapps\common\Fallout 4\Data`.
  BSLightingShader permutations are selected by a technique-ID bitfield. If a
  vertex-colour bit selects a different compiled permutation, the engine honours it.
* Anything FO4CS already knows: search `E:\Projects\Fo4CommunityShaders` for how it
  handles vertex colours in the lighting/LOD path.

If you cannot settle it, say so plainly rather than reasoning to a conclusion.

---

## THE FIVE LENSES

### 1. Inertness and stock-engine correctness risk
Settle item 4 above. Then: is the vertex-ALPHA argument airtight for alpha-TESTED
tree branch cards, where alpha feeds the discard? Does anything else consume vertex
colour — terrain blend, the LOD fade path, precombine/previs, the CK, xEdit, other
LOD tools? Does the fatter 32-byte descriptor itself risk a stock mismatch? What is
the failure mode if a future patch or a mod sets the flag?

### 2. Capacity, precision and invariants
16 bits is 65,536 objects per chunk; the docs cite 678 measured at Sanctuary dim 4.
What is the worst chunk in the Commonwealth at dim 4, and what would a Fallout 76
map port look like, given roughly four times the terrain and denser placement? Say
how you estimated. What happens on overflow — detected, clamped, or silent
aliasing? The index must be CONSTANT per triangle or interpolation yields a
fractional index: is that enforced or merely true by construction, and what could
break it (the merge, the far-ring simplifier, meshoptimizer welding, the atlas
pass)? Does the 8-bit UNORM round-trip exactly, or could sRGB handling, colour-space
conversion or the DXGI format shift it by one?

### 3. Alternatives — the most important lens
Compare, do not just list. For each: does it keep the stock fallback? What does it
cost per vertex, per draw, and in generator complexity?
  1. `SV_PrimitiveID` in the pixel shader plus a per-triangle range table in a
     structured buffer FO4CS binds. Zero vertex bytes, 32-bit identity. Does
     triangle order survive the merge, atlas and far-ring simplifier
     deterministically? Can FO4CS map a draw to the right table?
  2. One `BSTriShape` per object, no merging. Cost in draw calls against the real
     placement counts in the docs.
  3. `BSSubIndexTriShape` segments, which FO4 already ships for dismemberment.
  4. The index in UV2 or Eye Data instead of colour. NOTE: UV2 is half-float, about
     11 bits of mantissa, so it cannot hold a 16-bit integer exactly — verify that
     claim, it is the overseer's reasoning and may be wrong.
  5. Identity from POSITION, hashed against a spatial table in the manifest.
  6. Not needing per-vertex identity at all: does the consumer need it per VERTEX,
     or only per DRAW or per INSTANCE?
Give a ranked recommendation.

### 4. What it actually costs
The descriptor goes 20 bytes without identity, 24 with, 32 with the extra channels.
Attribute the marginal cost of identity ALONE — vertex colour would arguably be
present anyway to carry AO and sway, in which case identity occupies only R+G.
Convert to real numbers: total `.bto` bytes for a worldspace at each dim, and VRAM.
Use measured chunk sizes from the docs, `WW_CHANGES.md`, or harness logs, and say
where each number came from. Does identity force any vertex DUPLICATION — check
what the merge and simplifier actually do before assuming. **And quantify the one
the overseer thinks is the real cost:** the far-ring simplifier groups triangles by
(identity, layer) and simplifies each group ALONE, so a 678-object chunk is 678 tiny
decimation problems instead of one. How much achievable reduction does that give up
against simplifying the whole chunk?

### 5. Is the capability even worth it?
Enumerate every use the index is put to or planned for — search for them, be
exhaustive. For each, ask whether it genuinely needs a per-vertex index in the mesh,
or could be served by the manifest alone, a per-draw constant, or something the
consumer already knows. Test the strongest point in its favour: because the index is
an exact integer a consumer can HASH it for free per-object variation (hue jitter,
wind phase, card frame offset, mirroring), which is the main lever against LOD forest
repetition — is that exploited or only theoretical? Score the design against bungo's
three stated goals: better-looking LOD, less visible LOD pop, better
performance/memory/draw-calls. Finally: if you had to argue for DELETING identity
entirely, what is the strongest case?

---

## OUTPUT

`report_identity.md`, structured as:

1. **VERDICT** — three sentences. Good idea, bad idea, or good idea badly wired.
2. **WHAT MUST CHANGE** — ordered by severity, each with file:line and a concrete fix.
3. **THE FLAGS-2 QUESTION** — settled or not, with the evidence.
4. **PER LENS** — findings that survived refutation, each with its evidence.
5. **REFUTED** — claims a lens made that you killed, and why. This section matters;
   a report with nothing refuted means the refutation pass did not happen.
6. **UNVERIFIED** — what you could not check and why.

Be blunt. The overseer built this design and will not be helped by agreement.

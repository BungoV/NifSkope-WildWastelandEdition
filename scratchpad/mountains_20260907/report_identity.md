# Lane IDENTITY — is vertex-colour object identity a good idea?

*Status: **COMPLETE**. Written incrementally across two sessions (the first was
cut off by the account session limit, which also killed all six subagents; the
second finished the work personally). Every claim below was measured by the lane
analyst from the files — no subagent result survived to be quoted, and nothing
here is taken on trust.*

Started 2026-09-06, finished 2026-09-07. Overseer's question: **"is vertex
colour based identity a good idea btw?"**

Tools written for this lane, all in
`C:\Users\bungo\AppData\Local\Temp\claude\laneb\`: `nifpeek.py` (from-scratch NIF
header/block walker, self-checked by recomputing `Data Size`), `pestr.py` (PE
RVA → string/dword reader), `descsurvey.py`, `btosurvey.py`, `fxp.py`/`fxp2.py`,
`techscan.py`, `numeric.py`. The shipping CLI would not run under this session's
sandbox (§6.9), so every binary fact here comes from those parsers instead.

---

## 1. VERDICT

**Good idea, badly wired, and in the wrong place.** Per-object identity is a
real capability worth having and the exact-integer design is right in kind — but
it is wired to `SLSF2_Vertex_Colors`, which Fallout 4 demonstrably honours on the
LOD-object path (§3), so as shipped it breaks the charter's zero-effort fallback
that the contract document claims it protects; and it is carried per *vertex* to
express a per-*object* fact, which costs 16.8% of the object-LOD corpus
(lens 4B) for a capability that **nothing currently consumes** — FO4CS contains
no LOD module, no manifest reader and no reference to `lodgen`, `.bto` or the
index at all (lens 5A). Clear the flag today; move the index to a primitive-range
sidecar before anyone builds against it, and until then ship `--no-identity` as
the default, because that restores the descriptor to the byte-identical vanilla
`0x1B00000430205` and makes the whole stock-tolerance question moot rather than
merely arguable.

---

## 2. WHAT MUST CHANGE

Ordered by severity. Items 2.a, 2.a2 and 2.b were written before the refutation
pass and stand unchanged; the rest were added after it.

### 2.0 — CLEAR `SLSF2_Vertex_Colors`. One token. Ships today.

`src/lodgen.cpp:3504-3505`:

    nif->set<quint32>( iShader, "Shader Flags 2",
        opts.identity ? ( 5U | 0x20U ) : 5U );   // vertex-colours bit with identity

becomes

    nif->set<quint32>( iShader, "Shader Flags 2", 5U );

Fallout 4 reads flags2 bit 5 at RVA 0x0027cea0f and turns it into technique bit 0
(`Vc`), a different compiled shader, on the same code path that turns flags2
bit 2 into the `LODObj` technique (§3). Vanilla `.BTO` ships flags2 = 5; we ship
0x25; the fallback is broken today. FO4CS binds its own shaders and reads the
vertex buffer from the descriptor — it does not need the stock engine's
permission to sample `VA_COLOR`, so the bit buys the consumer nothing.

Do this **before** anything else in this list. It is the only entry that is a
live rendering defect rather than a latent one.

### 2.a — `--no-ao` silently ships a 32-byte descriptor with three dead channels

### 2.a — `--no-ao` silently ships a 32-byte descriptor with three dead channels

`src/lodgen.cpp:3306` opens `if ( opts.identity && opts.bakeAO ) {` and that
block is the ONLY place `bucket.sky` and `bucket.groundBlend` are ever sized or
filled (`src/lodgen.cpp:3361-3362`, `3365`, `3376`). But `extraChannels` at
`src/lodgen.cpp:3404` is `opts.identity && opts.objectChannels` — it does not
consult `bakeAO`. So with AO off and channels on:

* the descriptor is still widened to 32 bytes (`VF_UV_2` + `VF_EYEDATA`,
  `src/lodgen.cpp:3406-3408`);
* the write guard at `src/lodgen.cpp:3469` is
  `if ( extraChannels && int( v ) < bucket.sky.size() )`, and `bucket.sky` is
  empty, so **UV2 and Eye Data are never written for any vertex** and stay at
  the array-resize default of 0;
* vertex colour **B** stays at the `1.0f` seeded in `idColor` at
  `src/lodgen.cpp:3097` instead of AO.

Zero is not a neutral value for either field. Sky visibility 0 reads as
*fully enclosed*; ground-contact 0 reads as *clear of the ground*. A consumer
gets a plausible, wholly wrong field with nothing in the bytes to detect it —
precisely the failure mode `docs/LODGEN_VERTEX_PACKING.md` invented the
`'WWCV'` provenance stamp to prevent for `_data.DDS`, applied here to the mesh
and left open.

This is reachable from the shipped CLI: `--no-ao` exists
(`src/nifcli.cpp:5143`, advertised at `src/nifcli.cpp:4740`) and
`objectChannels` is **not exposed on the CLI at all** (grep of `src/nifcli.cpp`
finds no `objectChannels`), so it always takes its `= true` default from
`src/lodgen.h:265`. It is also reachable from the panel, which has independent
`aoCheck` and `channelsCheck` boxes (`src/lodgenmanager.cpp:2178`, `2183`).

**Fix:** either gate `extraChannels` on `bakeAO` too, or — better, since the
channels are conceptually independent of AO — hoist the `sky`/`groundBlend`
loop out of the `bakeAO` block and run it whenever `objectChannels` is on,
leaving only the `ambientOcclusion()` call inside the AO gate. The guard at
`:3469` should then be an assert, not a silent skip: a short `bucket.sky` means
a bug, and skipping the write hides it.

### 2.a2 — the 65,535-vertex bucket guard drops geometry SILENTLY, and breaks the manifest invariant

`src/lodgen.cpp:3213-3215`:

    const quint32 vBase = quint32( bucket.pos.size() );
    if ( vBase + quint32( keep.size() ) > 65535 )
        continue;   // bucket full; a second shape would need splitting

`Num Vertices` is a `ushort` in the format (`build/nif.xml:9494`) and the
triangle corners are built as `quint16( vBase + a )` (`src/lodgen.cpp:3271`), so
the cap is real and the guard is correct to exist. What is wrong is what it
does: a bare `continue`. No warning, no counter, no report line — grep of the
function finds `skippedNoLod` and `placed` counters but nothing for this.

Two consequences:

1. **Silent geometry loss.** A dense chunk whose objects concentrate on one
   material — downtown Boston on a single concrete or brick LOD texture is the
   obvious candidate — starts dropping source shapes at the 65,535th vertex of
   that bucket and says nothing. The output looks like a successful build.
2. **It falsifies the manifest invariant the contract advertises.** The manifest
   row is written at `src/lodgen.cpp:3098`, *before* the per-shape loop that can
   `continue`, and `objectIndex++` at `src/lodgen.cpp:3279` runs regardless. So
   an object can have a manifest row and an identity index with **no geometry in
   the chunk carrying that index**. `docs/LODGEN_VERTEX_PACKING.md:87-88` states
   that "the set of identity indices in a chunk is invariant under the pass — a
   manifest row can never point at an object that is no longer there". That is
   true of the *simplifier*; it is not true of the *builder*, and the document
   does not distinguish them.

Note this is not an identity-specific bug — it would exist without identity —
but it is an identity-specific *contract* bug, because identity is what makes
the index-to-manifest correspondence a promise anyone relies on.

**Fix:** at minimum count and report the drops in the build report, the way
`skippedNoLod` is. Properly: split the bucket into a second `BSSubIndexTriShape`
with the same material (the shape loop at `src/lodgen.cpp:3414` already emits
one shape per bucket; a bucket that overflows should become two), and only then
is the invariant true. If a split is not wanted, the object's manifest row
should be suppressed and its index not consumed, so the index set in the file
and the index set in the manifest still agree.

### 2.b — the documented descriptor hex is wrong in the contract and in the source comment

`src/lodgen.cpp:1357-1358` gives the two constants in decimal with hex in the
comment:

    constexpr std::uint64_t OBJ_VERTEX_DESC        = 474989027590661ULL;   // 0x1B00000650405
    constexpr std::uint64_t OBJ_VERTEX_DESC_COLORS = 1037939064898054ULL;  // 0x3B00000650406

Both hex comments are wrong. Measured (decoded from the decimal literals):

| constant | claimed | actual |
|---|---|---|
| `OBJ_VERTEX_DESC` | `0x1B00000650405` | **`0x1B00000430205`** |
| `OBJ_VERTEX_DESC_COLORS` | `0x3B00000650406` | **`0x3B00005430206`** |

`docs/LODGEN_VERTEX_PACKING.md:21-23` repeats both wrong values as the interface
contract. The claimed `...650405` decodes to UV at offset 16 and normal at
offset 20 inside a 20-byte vertex — i.e. it is not merely a typo, it is a
descriptor that would be *corrupt* if anyone wrote it. Anyone implementing the
consumer from the doc rather than from `GetAttributeOffset` gets nonsense.

The **code is correct**: `0x1B00000430205` is byte-identical to what vanilla
ships. Measured directly from the vanilla file with a from-scratch NIF header
walker (`nifpeek.py` in this lane's temp dir, written against `build/nif.xml`,
not using NifSkope):

    == Commonwealth.4.-20.24.BTO
       #2 BSSubIndexTriShape name='obj'    desc=0x1B00000430205 stride=5(=20B)
          tris=17304 verts=19583 datasize=495484 (calc 495484)
          attrs=VERTEX+UVs+NORMALS+TANGENTS
       #8 BSSubIndexTriShape name='obj-at' desc=0x1B00000430205 stride=5(=20B)
          tris=7178 verts=14120 datasize=325468 (calc 325468)

Decoded offsets, identical for vanilla and for our no-identity constant:
POSITION 0, TEXCOORD0 8, NORMAL 12, BINORMAL 16, COLOR absent, size 20.
`OBJ_VERTEX_DESC_COLORS` adds COLOR at 20 for a 24-byte stride — self-consistent
under `BSVertexDesc::ResetAttributeOffsets` (`src/data/niftypes.h:1934`), so
there is **no descriptor corruption**. Only the documentation is wrong.

**Fix:** correct both comments and the two hex strings in
`docs/LODGEN_VERTEX_PACKING.md:21-23`. While there: the 24-byte path *is*
hardcoded (`src/lodgen.cpp:3403`, and the literal `24`/`20` at
`src/lodgen.cpp:3411-3412`), only the 32-byte path calls
`ResetAttributeOffsets`. The comment at `src/lodgen.cpp:3399-3402` says the
stride is "built rather than hardcoded"; for two of the three profiles it is
not. Harmless today because the constants happen to be right, but the comment
promises an invariant the code does not maintain.

### 2.c — the index is unbounded, and overflow desynchronises the mesh from the manifest

`src/lodgen.cpp:2956` / `:3279` / `:3096-3097`. Past 65,535 the mesh masks and
the manifest does not, so the two silently disagree — and the far-ring
simplifier then groups the two aliased objects together and may weld their
geometry, defeating the single invariant the design exists to protect
(`src/lodgen.h:531-535`). Lens 2A puts a dim-32 chunk over downtown Boston at an
estimated 158,000 objects, 2.4× the ceiling, once `--slot-fallback` is on.

**Fix:** clamp and refuse. If `objectIndex > 0xFFFF`, stop assigning identity for
the rest of the chunk, omit those manifest rows, and fail the build with a
message naming the chunk — an object-LOD chunk that silently lies about which
object a pixel belongs to is worse than one that refuses to build. Longer term
this is the strongest argument for the 32-bit sidecar (lens 3.1).

### 2.d — the identity harness does not test the identity channel

`tests/spells/lodgen_identity.sh` never opens the `.bto`. All five of its checks
are manifest properties (byte-identical rebake, header line, `(ref, part)`
uniqueness, ring sharing, SCOL ordinals). The contract's headline measurement —
678 distinct indices, range 0–677, none missing either way, no index split across
two blobs — was a one-off and nothing re-runs it. And the harness builds with
`--no-ao` (`:38`), which is exactly the configuration 2.a shows leaves UV2, Eye
Data and the AO byte dead, so the feature's own gate exercises the broken profile
and cannot see it.

**Fix:** add a `--dump-geometry` pass to the harness that (i) decodes
`R + G*256` for every vertex, (ii) asserts the index set equals the manifest's
index set, (iii) asserts the index is **constant across every triangle** — the
invariant `src/lodgen.cpp:7644` assumes from `v1()` alone and never checks — and
(iv) runs once *without* `--no-ao` so the extra channels are non-zero.

### 2.e — narrow the stock-tolerance item instead of leaving it open

`docs/TO_BE_IMPLEMENTED.md` ~232 and `docs/LODGEN_VERTEX_PACKING.md:484` treat
"a wider desc, generally" as the unrun risk. Measured over all 108,010 shapes in
the vanilla mesh corpus: the 24-byte identity descriptor is one vanilla ships
29,242 times, and strides up to 44 ship. The open question is `VF_UV_2` alone,
which vanilla ships **zero** times. Rewrite the item to say that, and the test
becomes a ten-minute in-game check of one chunk rather than an open-ended worry.

### 2.f — downgrade the vertex-alpha claim from measured to assumed

`docs/LODGEN_VERTEX_PACKING.md:201-205` states as fact that "the engine only
takes vertex alpha when `SLSF1_Vertex_Alpha` is set". What is measured is that
the flag is clear in the file. The engine half is unsupported: flags1 bit 3 is
never tested in `BSLightingShaderProperty::GetRenderPasses_Forward`, where every
other technique-selecting flag is, and `GeometrySetupConstantAlpha` only writes a
scalar (lens 1B). The sway channel may be safe; the page should not claim it has
been shown to be.

### 2.g — strategic: get identity out of the vertex

Not a defect, a direction. It costs 4 bytes a vertex — **31.8 MB, +16.8%** across
a vanilla-density Commonwealth object-LOD corpus (lens 4B) — to carry a
per-object fact per vertex, it caps at 16 bits, and it has no consumer. The
triangles are already contiguous per object in both the builder and the
simplifier (lens 3.1), so a `(startPrimitive, count, objectIndex)` sidecar is a
few thousand rows a chunk, zero vertex bytes, 32-bit, and leaves the file's
descriptor byte-identical to vanilla's. Until a consumer exists, default
`identity` to **off**.

---

## 3. THE FLAGS-2 QUESTION

### SETTLED — YES. Fallout 4 honours `SLSF2_Vertex_Colors`, and it honours it on the LOD-object path.

Measured in Todd's treat (1.10.155)
through the Todd's treat tooling (kept outside this repo)
and a from-scratch PE string reader (`pestr.py` in this
lane's temp dir). Two independent halves, both quoted below.

#### 3.1 The technique-ID bit layout, from `BSLightingShader::GetTechniqueName`

Disassembly of `BSLightingShader::GetTechniqueName` (RVA **0x0289e6e0**).
The function `BSsprintf`s a base string and then `strcat_s`es one literal per set
bit of the technique ID's low byte, then switches on `(id >> 8) & 0x3F`. Reading
each `lea r8, [rip+disp]` target out of the PE with `pestr.py`:

| what | RVA of the literal | literal |
|---|---|---|
| base | 0x0309AB30 | `BSLighting` |
| tech bit **0** | 0x03098514 | **`Vc`** |
| tech bit 1 | 0x03098518 | `Sk` |
| tech bit 2 | 0x03098534 | `Msn` |
| tech bit 3 | 0x030997E4 | `Projuv` |
| tech bit 5 | 0x02D16ECC | `Pipboy` |
| tech bit 7 | 0x02D3D708 | `Menu` |

and the 20-entry jump table at RVA **0x0289E884** (read directly, not guessed)
gives the type enum in bits 8–13:

    0 (none)  1 Envmap   2 Glowmap  3 Parallax  4 Face    5 SkinTint
    6 Hair    7 ParallaxOcc  8 MTLand  9 LODLand  10 ?     11 MultiLayerParallax
    12 Tree  13 **LODObj**  14 MultiIndexTriShapeSnow  15 LODObjHD
    16 Eye   17 ?      18 LODLandNoise  19 MTLandLODBlend

So `Vc` is not a uniform or a branch inside one shader — it is a **permutation
selector**. `BSLighting Vc LODObj` and `BSLighting LODObj` are two different
compiled shaders.

#### 3.2 The flag that sets it, from `BSLightingShaderProperty::GetRenderPasses_Forward`

Disassembly of `BSLightingShaderProperty::GetRenderPasses_Forward`
(RVA **0x0027ce4d0**). `rbp` holds the property's flags as one 64-bit word,
`flags1 | (flags2 << 32)` — proved by two independent landmarks in the same
function, not assumed:

* bit 60 (= flags2 bit 28, `Pipboy_Screen`) → `or eax, 0x20` → tech bit 5 =
  `Pipboy`  (RVA 0x0027cea74–0x0027cea83)
* bit 55 (= flags2 bit 23, `Menu_Screen`) → `bts eax, 7` → tech bit 7 = `Menu`
  (RVA 0x0027cea86–0x0027cea95)

and then, at **RVA 0x0027cea0f**:

    0x0027cea0f  48b90000000020000000  movabs rcx, 0x2000000000    ; 1 << 37
    0x0027cea19  488bc5                mov    rax, rbp
    0x0027cea1c  41b801000000          mov    r8d, 1
    0x0027cea22  4823c1                and    rax, rcx             ; rbp & (1<<37)
    0x0027cea25  488bcd                mov    rcx, rbp
    0x0027cea28  4889442460            mov    [rsp+0x60], rax
    0x0027cea2d  418bc4                mov    eax, r12d            ; r12d == 0
    0x0027cea30  410f45c0              cmovne eax, r8d             ; -> eax = 1

Bit 37 is **flags2 bit 5 = `SLSF2_Vertex_Colors`**, and `eax = 1` is technique
bit 0 = `Vc`. `eax` is the modifier accumulator that is finally combined with
the type enum and stored as the pass's technique ID:

    0x0027ceb8d  c1e108   shl ecx, 8        ; type enum into bits 8+
    0x0027ceb94  0bc8     or  ecx, eax      ; modifier bits, including Vc
    0x0027ceb9e  894f48   mov [rdi+0x48], ecx   ; RenderPass::techniqueID

#### 3.3 And the LOD-object path is the same path

In the same type-selection block, at **RVA 0x0027ceb3e**:

    0x0027ceb3e  49b80000000004000000  movabs r8, 0x400000000   ; 1 << 34
    0x0027ceb48  4985e8                test   r8, rbp
    0x0027ceb4b  41b80d000000          mov    r8d, 0xd          ; 13 = LODObj
    0x0027ceb51  410f45c8              cmovne ecx, r8d

Bit 34 = flags2 bit 2 = **`LOD_Objects`**, which is exactly the bit vanilla
`.BTO` shapes carry (measured: flags2 = 5 = ZBuffer_Write | LOD_Objects). The
LOD-object technique is chosen by the *same* `ecx` that the `Vc` bit is ORed
into. There is no separate, flag-ignoring LOD path.

Note also that the `or ecx, eax` at 0x0027ceb96 is **outside** the branch at
0x0027ceab3 that can collapse the type enum to 0 — so the `Vc` bit survives even
when that branch is taken.

#### 3.4 What this means for the design

    vanilla .BTO      flags2 = 0x05  ->  technique 0x0D00 = "BSLighting LODObj"
    ours, identity on flags2 = 0x25  ->  technique 0x0D01 = "BSLighting Vc LODObj"

We are asking the stock engine for a **different compiled shader** than any
vanilla object-LOD chunk has ever asked for. That shader, by the universal
meaning of `Vc` in Bethesda lighting shaders, multiplies the interpolated vertex
colour into the albedo. Our vertex colour is
`R = index & 0xFF`, `G = index >> 8`, `B = AO`.

For the Sanctuary chunk measured in the contract (678 objects, indices 0..677):
G is 0 for indices 0–255 and 1 or 2 for the rest, i.e. **effectively zero for
every object in the chunk**, and R sweeps the full 0–255 range. So the stock
engine would render that chunk as a set of objects each tinted a different
shade of **pure red multiplied by its AO**, with green and blue crushed to
black. Not "slightly dark" — the green and blue channels of every LOD object in
the worldspace go to zero.

**This is a shipping-blocking defect against the charter's zero-effort fallback,
and the contract document asserts the opposite of what the code does.**
`docs/LODGEN_VERTEX_PACKING.md:9-12` says every slot is "either a field the
vanilla shaders do not sample, or a field they sample only behind a flag we
leave clear". This flag is not left clear, and the field is sampled.

**Fix, and it is a one-liner:** `src/lodgen.cpp:3504-3505` should write `5U`
unconditionally —

    nif->set<quint32>( iShader, "Shader Flags 2", 5U );

The bit buys nothing. FO4CS binds its own shaders and reads the vertex buffer
from the descriptor; it does not need the stock engine's permission to sample
`VA_COLOR`. Clearing the bit restores the fallback exactly and costs the
consumer nothing. See §5 for what remains unproven even after this change.

#### 3.5 What is still NOT proven by the above

I have proved the engine *selects a different technique*. I have **not** been
able to confirm that a compiled `BSLighting Vc LODObj` permutation (technique id
0x0D01) exists in the package, though I did go and look: `Fallout4 - Shaders.ba2`
holds exactly one file, `ShadersFX\Shaders011.fxp` (12,844,936 B), which I
extracted and walked. It contains **3,939 DXBC blobs**, each preceded by a record
header (`0x11223344`, `u32 blobSize`, `u32 index`, `u32 0x0FFF`, then a 40-byte
constant table). The `index` field runs 0, 1, 2, … per section — it is an
ordinal, not a technique id — and the exe contains no static technique-id table
to map it through (I scanned every 4-aligned occurrence of 0x0D00 and 0x0D01 for
a small-integer neighbourhood: zero hits). The mapping is built at load into
`BSShaderTechniqueIDMap`, and recovering it needs a disassembly of
`BSShader::Load` that I did not have budget for. Two cases, and both are bad:

* it exists → the chunk renders multiplied by the identity colour (the analysis
  above);
* it does not exist → `BSShaderTechniqueIDMap` lookup misses and the pass has no
  vertex/pixel shader, which in this engine means the geometry is silently not
  drawn.

Either way the fallback is broken; the fix is the same. Which of the two it is
is filed in §6 UNVERIFIED.

---

### Supporting measurements (this lane, from the raw bytes via `nifpeek.py`)

Vanilla `Commonwealth.4.-20.24.BTO`, both `BSLightingShaderProperty` blocks:

    flags1 = 0x80400001   bits 0 (Specular), 22 (Own_Emit), 31 (ZBuffer_Test)
    flags2 = 0x00000005   bits 0 (ZBuffer_Write), 2 (LOD_Objects)

Bit names from `build/nif.xml:7000` (`Fallout4ShaderPropertyFlags1`) and
`build/nif.xml:7036` (`Fallout4ShaderPropertyFlags2`).

So:

* `F4SF1_Vertex_Alpha` is **bit 3** (`build/nif.xml:7005`) and is **clear** in
  vanilla. The generator writes `2151677953U` = `0x80400001`
  (`src/lodgen.cpp:3502`), the same value, so bit 3 is clear in ours too. The
  doc's vertex-alpha inertness argument is **confirmed at the file level**.
* `F4SF2_Vertex_Colors` is **bit 5** (`build/nif.xml:7043`). Vanilla's flags2 is
  `5`; ours is `5 | 0x20` when identity is on (`src/lodgen.cpp:3504-3505`).
  **We deviate from vanilla by exactly this one bit, and the contract document
  claims we leave every such flag clear.** §3.2 above shows what the engine does
  with it.

One thing that closes off the obvious escape route: `nif.xml:7776`
annotates Shader Flags 2 with *"Mostly overridden if 'Name' is a path to a
BGSM/BGEM file."* If a material file shadowed the flags, the deviation might not
reach the engine. It does not: our LOD shapes carry an empty `Name` (measured:
`name=''` on both vanilla properties, and `src/lodgen.cpp` sets none), so the
flags in the file are the ones the engine uses. There is no material file
shadowing them.

---

## 4. PER LENS

All five lens agents were killed by the session limit before returning anything.
Everything below was measured by the lane analyst personally, from the files.
Where a claim is reasoning rather than measurement it says so.

### Lens 1 — inertness and stock-engine correctness risk

**1A. The index is unbounded, and overflow is worse than a silent wrap.**
`int objectIndex = 0;` at `src/lodgen.cpp:2956`; `objectIndex++` at
`src/lodgen.cpp:3279`, unconditionally, once per placement; encoded at
`src/lodgen.cpp:3096-3097` as `objectIndex & 0xFF` and `(objectIndex >> 8) & 0xFF`.
There is no bound check, clamp, warning or assertion anywhere on the path — I
traced every use (`grep -n objectIndex src/lodgen.cpp` returns exactly lines
2956, 3096, 3099, 3114, 3277, 3279).

The overseer described this as "wraps silently past 65,536". It is worse than a
wrap, in a way that matters:

* the **manifest** row at `src/lodgen.cpp:3098` is written with the *unmasked*
  `objectIndex`, so the manifest keeps counting past 65,535 correctly;
* the **mesh** carries the masked value.

So object 0 and object 65,536 write the *same* vertex colour while holding
*different* manifest rows. The manifest and the mesh disagree, and nothing in
either can detect it. Every consumer that decodes a colour and looks it up gets
the wrong placement for one of the two.

Worse still, the far-ring simplifier reconstructs the index from the colour
(`src/lodgen.cpp:7632-7639`) and groups triangles by it. Two aliased objects
therefore land in **one group** and are simplified together — meshoptimizer is
then free to collapse an edge between them, welding two unrelated objects'
geometry. That is precisely the invariant the whole design exists to protect
(`src/lodgen.h:531-535`, "no collapse can weld two objects together"), and
aliasing defeats it.

**1B. The vertex-alpha inertness argument is NOT airtight — and I could not find
the gate the contract asserts.**

Within the generator the argument holds. `Shader Flags 1` is written exactly once
for object chunks, at `src/lodgen.cpp:3502-3503`, as the literal `2151677953U`
(`0x80400001`) with only the `Own_Emit` bit varying. I checked every other write
of that field in the file (`grep -n "Shader Flags 1" src/lodgen.cpp` → lines 968
and 1066/1310 are the Land and water shapes, 1961 and 4051 are *reads* of a
source mesh's flags to recover `ownEmit` only, 7161 is a report line). **No code
path copies a source mesh's flags1 onto a LOD shape**, so a source tree with
`SLSF1_Vertex_Alpha` set cannot propagate it. Good.

What does *not* hold is the engine half of the argument.
`docs/LODGEN_VERTEX_PACKING.md:201-205` says: *"the engine only takes vertex
alpha when `SLSF1_Vertex_Alpha` (flags1 bit 3) is set"*. I went looking for that
gate in 1.10.155 and **could not find it**:

* `BSLightingShaderProperty::GetRenderPasses_Forward` tests flags1 bits 4, 7, 10,
  12, 14, 17, 18, 19, 21, 23 and flags2 bits 2, 5, 21, 23, 24, 28 — every flag
  that selects a technique. It **never tests flags1 bit 3.** (Verified by reading
  all 624 disassembled lines for `bt rbp, 3` / `test bpl, 8` / `and …, 8`; the
  only `test bpl,` forms present are `test bpl, 1`, `test bpl, 0x10` and
  `test bpl, 0x80`.)
* `BSLightingShader::GeometrySetupConstantAlpha` (RVA 0x0289f800, 72 bytes,
  disassembled in full) only fetches a scalar from the property and optionally
  multiplies it by a material constant. It does not gate a vertex attribute.

This is an argument from absence and I mark it as such. But the implication is
uncomfortable and it points the same way as §3: if the gate is not a permutation
selector and not that constant, the most likely place it lives is **inside the
compiled `Vc` pixel shader** — the very permutation the identity flag now
selects. In that case setting `SLSF2_Vertex_Colors` on an alpha-tested branch-card
shape would turn the sway weight into a discard mask and eat the cards from the
inside out, which the contract itself names as the load-bearing failure
(`docs/LODGEN_VERTEX_PACKING.md:207-210`). Clearing the flag (§3.4) forecloses
this too.

**1C. What else consumes vertex colour — measured where I could, reasoned where I
could not.**

Measured: **vanilla FO4 terrain LOD carries no vertex colours at all.**
`Commonwealth.4.-20.24.BTR`'s Land shape has descriptor `0x300000000203`,
12 bytes, attributes `VERTEX + UVs` only (`nifpeek.py`). So the terrain-blend
path cannot be reading a vertex colour off the terrain side. Whether the *object*
LOD land-blend path (`BSLightingShader::GeometrySetupConstantLandBlendParams`
exists as a symbol; `F4SF2_No_LOD_Land_Blend` is flags2 bit 14, clear in both
vanilla and ours) samples object vertex colour is **UNVERIFIED**.

Reasoned, not measured: precombines and previs operate on `.nif`/`.uvd` in cell
directories, not on `.bto`, so a chunk is not an input to them; the CK's LOD
tools and xEdit read `.bto` only to rewrite or list it. I did not test any of
this and it should not be quoted as measurement.

**1D. The fatter descriptor is NOT the risk — measured, and this refutes a
worry in the brief.**

I surveyed the vertex descriptors of the **entire** vanilla unpacked mesh corpus
(`E:\Tools\Fallout 4\DataUnpacked\Data\Meshes`, **34,985 `.nif`**), parsing
**108,010 shapes** and validating each by recomputing `Data Size` from
`Num Vertices` and `Num Triangles` — 278 rows failed that check and were
discarded rather than trusted (`descsurvey.py`):

| attribute set | shapes | stride | descriptor |
|---|---|---|---|
| VERT+UV+NRM+TAN | 60,832 | 20 | `0x1B00000430205` |
| **VERT+UV+NRM+TAN+COL** | **29,242** | **24** | **`0x3B00005430206`** |
| VERT+UV+NRM+TAN+SKIN | 8,331 | 32 | `0x5B00050430208` |
| VERT+UV+NRM+TAN+COL+SKIN | 7,344 | 36 | `0x7B00065430209` |
| VERT+NRM | 1,068 | 12 | `0x900000020003` |
| VERT+UV+NRM+TAN+COL+SKIN+**EYE** | 623 | 40 | `0x17B0906543020A` |
| VERT+NRM+COL | 441 | 16 | `0x2900003020004` |
| (no attributes) | 64 | 0 | `0x0` |
| VERT+UV+COL+SKIN | 50 | 28 | `0x6300043000207` |
| VERT+UV+SKIN | 8 | 24 | `0x4300030000206` |
| VERT+UV+NRM | 3 | 16 | `0xB00000030204` |
| VERT+UV+COL | 2 | 16 | `0x2300003000204` |
| VERT+UV+NRM+COL | 1 | 20 | `0x2B00004030205` |
| VERT+UV | 1 | 12 | `0x300000000203` |

Strides observed across the corpus: 0, 12, 16, 20, 24, 28, 32, 36, 40, 44.

Two things fall out, and they are corpus facts, not sample facts:

1. **Our 24-byte identity descriptor is not novel — it is byte-identical to a
   descriptor vanilla ships 29,242 times** (`0x3B00005430206`, 27% of all shipped
   shapes). The engine demonstrably builds an input layout for it. Strides of 28,
   32, 36, 40 and 44 also ship. So "does stock FO4 tolerate a fatter object
   descriptor" is, for the 24-byte profile, **answered yes by the shipped
   corpus**, and the `TO_BE_IMPLEMENTED.md` item that treats it as an open risk
   should be narrowed rather than left open.
2. **`VF_UV_2` appears in ZERO of the 108,010 shapes.** Not rare — absent. The
   32-byte profile (`VERT+UV+UV2+NRM+TAN+COL+EYE`) is a combination Fallout 4 has
   never been asked to render, and it is the one that moves `VA_COLOR` from +20
   to +24. That, not the width, is where the whole residual tolerance risk lives,
   and it is a far narrower and more testable question than the contract's "a
   wider desc, generally".

**1E. Failure mode if the flag is set — quantified.**

It is set, today, by us (§3). For the Sanctuary chunk the contract measures
(678 objects, indices 0..677):

* R = `index & 0xFF` sweeps 0..255 roughly uniformly — 678 objects over 256
  values, so every red value appears 2–3 times;
* G = `index >> 8` is **0 for indices 0–255, 1 for 256–511, 2 for 512–677** —
  i.e. byte values 0, 1, 2 out of 255. Effectively black.
* B = baked AO, typically 0.5–1.0 of full;
* A = sway, 0 for everything that is not a tree.

Multiplying albedo by that gives, per object, a colour of roughly
`(rand(0..1), 0.004, AO)`. Green is annihilated on every object in every chunk;
blue survives only as AO; red is a per-object random dimming. The visible result
is a red-black mosaic where the object LOD used to be. There is no chunk anywhere
in the Commonwealth with more than 65,535 objects (§ lens 2), so G is ≤ 2 —
**there is no chunk in which this failure is mild.**

### Lens 2 — capacity, precision and invariants

**2A. Capacity — 16 bits is comfortable at dim 4 and 8, and is NOT clearly safe
at dim 32.**

Route 1, the vanilla corpus. I parsed every vanilla object-LOD chunk in
`Meshes\Terrain\Commonwealth\Objects` (`btosurvey.py`, 465 files, 0 unparsed,
every `Data Size` self-checked):

| lvl | chunks | max verts in one chunk | mean verts | max tris | worst chunk |
|---|---|---|---|---|---|
| 4 | 344 | **123,433** | 16,683 | 60,584 | `Commonwealth.4.4.-4.BTO` (9 shapes, 2,838,806 B) |
| 8 | 97 | **235,837** | 20,976 | 111,520 | `Commonwealth.8.0.-8.BTO` (12 shapes, 5,391,991 B) |
| 16 | 20 | 47,129 | 7,781 | 26,619 | `Commonwealth.16.0.-16.BTO` |
| 32 | 4 | 15,007 | 4,281 | 9,596 | `Commonwealth.32.0.-32.BTO` |

`Commonwealth.4.4.-4` covers cells x 16..19, y −16..−13 — downtown Boston, as
expected.

Route 2, converting vertices to objects. The contract's own measured pair is
Sanctuary (−20,24) dim 4 = **678 objects**; the vanilla chunk for the same
coordinates holds **33,703 vertices** (19,583 + 14,120, measured above). That is
**49.7 vertices per object**. Applying it to the worst vanilla dim-4 chunk:
123,433 / 49.7 ≈ **2,480 objects**, i.e. **3.7× Sanctuary**, 155 objects per cell
against Sanctuary's 42.

The two routes agree that dim 4 has ~26× headroom against 65,536. Dim 8 likewise.

**The exposure is at dim 32, and it is real.** A dim-32 chunk covers 32 × 32 =
1,024 cells. At Sanctuary's density that is 43,000 objects — inside the ceiling
but only by 34%. At downtown Boston's measured density (155/cell) it is
**≈ 158,000 objects, 2.4× over the ceiling.** The counter-argument is that ring 3
is sparse: `WW_CHANGES.md:301` records that only **51 of 28,932 LOD-bearing bases
fill the ring-3 slot**. But this generator's `--slot-fallback` exists precisely to
defeat that, and the measured consequence is in the same paragraph — our dim-16
chunk is **4,884 KB against vanilla's 271 KB mean, eighteen times**. A generator
that puts eighteen times vanilla's content into a far ring is exactly the one that
can put 65,536 objects into a dim-32 chunk over Boston.

MODELLED, not measured: I could not count placements per cell directly, because
that needs the ESM walk the CLI provides and the CLI would not run (§6). The
50-vertices-per-object constant is the weakest link — it comes from one chunk.

A **Fallout 76 port** makes it worse by roughly the square of the linear scale.
Appalachia is about 4× the Commonwealth's terrain area with denser placement;
at equal per-cell density a dim-32 chunk is the same 1,024 cells, so the ceiling
is not breached by area but by *density*, and FO76's forests are denser than the
Commonwealth's. I would not ship a 16-bit index into that.

**2B. Overflow behaviour: silently aliased.** See 1A. Not detected, not clamped.
The manifest keeps a correct, unique row for every object (so no duplicate keys
there), which makes the disagreement between manifest and mesh undetectable from
either side alone.

**2C. The constant-per-triangle invariant is true by construction and NOWHERE
enforced or gated.**

By construction: `bucket.col` is appended once per source vertex with the same
`idColor` for a whole placement (`src/lodgen.cpp:3239, 3264`), and triangles are
appended referencing only that placement's vertices
(`src/lodgen.cpp:3266-3272`). So one triangle cannot span two placements.

Not enforced: the far-ring simplifier's `keyOf` reads the identity from
**`tris[t].v1()` only** (`src/lodgen.cpp:7644`) — the first corner. It assumes
the invariant rather than checking it. If it were ever violated the pass would
group by the wrong key and blend identities, silently.

Attacks I checked:

* **the merge** — `lodgenMergeChunkShapes` (`src/lodgen.cpp:7089`). It concatenates
  by material; I found no weld or dedupe of vertices in it.
* **the far-ring simplifier** — `meshopt_simplifyWithAttributes`
  (`src/lodgen.cpp:7733`), not `meshopt_simplifySloppy`. `simplify*` never creates
  vertices, so survivors are a subset. Confirmed against the vendored source at
  `lib/meshoptimizer/src/simplifier.cpp`.
* **vertex compaction** — `src/lodgen.cpp:7777-7791` renumbers but never merges
  two distinct vertices.
* **no `meshopt_generateVertexRemap` / `optimizeVertexFetch` / `optimizeVertexCache`
  pass runs anywhere in `lodgen.cpp`** (`grep -n meshopt_ src/lodgen.cpp` returns
  only lines 253, 804, 808, 7733 — three `simplify` calls and one
  `simplifyWithAttributes`).

**The gate does not test this.** `tests/spells/lodgen_identity.sh` never opens the
`.bto`. It checks (i) two bakes are byte-identical, (ii) the manifest header
line, (iii) `(ref, part)` uniqueness, (iv) ring-to-ring sharing, (v) SCOL
ordinals — all of them properties of the **manifest**, not of the vertex channel.
The contract's headline measurement ("678 distinct indices … zero indices split
into more than one spatially separate blob") was a one-off, and nothing re-runs
it. **The identity channel itself is ungated.**

And the gate builds with `--no-ao` (`tests/spells/lodgen_identity.sh:38`), which
is exactly the configuration §2.a shows produces a 32-byte descriptor with UV2,
Eye Data and the AO byte all dead. The one harness named after this feature
exercises the broken configuration and cannot see it.

**2D. The 8-bit round trip is exact; the half-float alternative is not.**
Computed (`numeric.py`):

* `v/255.0` stored as float32, recovered as `round(f*255)`: **max error 0 over all
  256 values** — including through the `qRound`/`qBound` path the simplifier uses
  at `src/lodgen.cpp:7635-7636`. The decode is exact.
* The sRGB worry is unfounded *in principle* — D3D11 applies no colour-space
  conversion to a vertex attribute; sRGB is a texture/render-target format
  property. The vertex element would have to be declared
  `DXGI_FORMAT_R8G8B8A8_UNORM`, and there is no `_SRGB` vertex format in DXGI at
  all. I did not read the engine's input-layout construction to confirm the
  format it picks, so the last step is REASONING (§6).
* **The overseer's half-float claim is correct, and the exact number is 2048.**
  IEEE binary16 represents every integer up to 2048 exactly; **2049 rounds to
  2048**, and spacing is 2 up to 4096, 4 up to 8192, and so on (measured by
  round-tripping every integer 0..70,000 through `struct '<e'`). So UV2 cannot
  carry a 16-bit index — it silently aliases above 2048, which is *inside* the
  range Sanctuary alone would need three times over if the layer slot were used
  for identity.

### Lens 3 — alternatives

| # | alternative | stock fallback | vertex cost | draw cost | generator work | verdict |
|---|---|---|---|---|---|---|
| 1 | `SV_PrimitiveID` + per-range table | **intact** (nothing in the mesh) | **0 B** | 1 buffer bind | moderate: emit the table last | **best** |
| 3 | `BSSubIndexTriShape` segments | intact | 0 B | unknown | small — **already used** | strong second |
| 6 | don't need per-vertex identity | intact | 0 B | 0 | none | see lens 5 |
| 2 | one shape per object | intact | 0 B | **catastrophic** | small | no |
| 5 | identity from position | intact | 0 B | per-pixel search | large | partial only |
| 4 | index in UV2 / Eye Data | UV2 unattested; Eye Data occupied | 0–4 B | 0 | small | UV2 impossible (2048), Eye Data possible |
| — | today: vertex colour R+G | **broken** (§3) | 0 B marginal (lens 4) | 0 | shipped | fixable by one flag |

**3.1 `SV_PrimitiveID` + a range table is better than it looks, because the
triangles are already sorted by object.** This is the finding that changes the
ranking. In the builder, triangles go into `bucket.cellTris[cellIdx]` in
placement order (`src/lodgen.cpp:3270`), so within a cell an object's triangles
are **contiguous by construction**. In the far-ring simplifier, `outTris` is
emitted group by group over `order` (`src/lodgen.cpp:7678-7748`) — again
contiguous per identity — and is then re-binned by cell centroid
(`src/lodgen.cpp:7757-7775`), which breaks contiguity only across cell
boundaries. So a `(startPrimitive, count, objectIndex)` table has on the order of
`objects × cells-touched` rows, not `triangles` rows: for the Sanctuary chunk,
678 objects over 16 cells, so **under a few thousand rows of 12 bytes** — tens of
kilobytes per chunk, against the ~135 KB the colour channel costs the same chunk
(lens 4). And it is 32-bit, so lens 2A's dim-32 exposure disappears.

The table must be emitted **after** the simplifier and after the merge, because
both rewrite the index buffer. That is a real constraint but a mechanical one:
the passes already run in a fixed order and already rewrite the file.

What I could **not** settle: whether FO4CS can identify *which* `.bto` shape a
draw belongs to at draw time, and get the draw's start-index/base-vertex. That is
the load-bearing unknown for this option, and it is a question about FO4CS, not
about the generator. Filed in §6.

**3.2 `BSSubIndexTriShape` segments — the generator ALREADY does this.** This is
the second finding that the brief did not anticipate. `src/lodgen.cpp:3420` emits
**`BSSubIndexTriShape`**, not `BSTriShape`, and `src/lodgen.cpp:3480-3490` writes
`Num Segments = dim*dim` (16 at dim 4 — `const int segs = ( dim == 4 ) ? 16 : 1;`
at `src/lodgen.cpp:2736`) with a `Start Index` and `Num Primitives` per segment.
Vanilla does the same: `Commonwealth.4.-20.24.BTO` is two `BSSubIndexTriShape`
blocks (measured). The simplifier preserves and rebuilds the segments
(`src/lodgen.cpp:7838-7848`).

So "segments" is not a new mechanism to adopt; it is an existing one to *use
differently* — segment per object instead of segment per cell. The cost is
per-cell hiding, which is what the cells are for today. Whether the stock engine
draws a many-segment shape as one draw call is the decisive unknown, and I did
not settle it (§6). Note that segment-per-object and the primitive-ID table are
nearly the same idea, one expressed in the file format and one in a sidecar; the
sidecar version does not have to give up per-cell hiding.

**3.3 One shape per object is not viable** and the numbers say so plainly.
Vanilla's worst dim-4 chunk is **9 shapes** and its worst dim-8 chunk **12**
(measured, lens 2A). Sanctuary's 678 objects would be 678 shapes for one chunk of
one ring. With several rings resident and dozens of chunks per ring that is tens
of thousands of draw calls where there are now tens. Fallout 4 is CPU-bound on
draw submission in exactly the situations LOD exists to help. Rejected.

**3.4 Eye Data is the only in-mesh alternative that would actually work.** It is
a full `float` (`src/lodgen.cpp:3472` writes `nif->set<float>( row, "Eye Data", … )`),
so it holds every integer up to 2^24 exactly — plenty. It is occupied today by
the ground-contact blend, but that blend is a *derived* quantity: it is
`1 − (z − groundHeight(x,y)) / 256` (`src/lodgen.cpp:3376-3377`), and a consumer
with the heightfield can recompute it. Whether Eye Data is inert to the stock
engine is a separate question I did not settle — but note vanilla *does* ship
`VF_EYEDATA` (119 shapes in the sample, lens 1D), always together with `SKIN`, so
its stock behaviour outside the eye path is unattested.

**3.5 Identity from position** is not hopeless but is not a substitute. It costs
nothing in the file and the consumer can build a grid from the manifest at load.
It fails on objects that overlap in plan (a tree in front of a wall), on
co-located SCOL parts — and 471 of Sanctuary's 678 objects are SCOL parts
(`docs/LODGEN_VERTEX_PACKING.md:154`) — and it needs a search per pixel or per
vertex. For the *coarse* uses (wind phase, hue jitter) hashing the world position
directly is as good and needs no table at all. For exact uses it is unusable.

**Ranked recommendation.**

1. **Clear `SLSF2_Vertex_Colors` and keep the channel as it is** (§3.4). One
   token. It restores the fallback and changes nothing else. Do this first
   regardless of what follows.
2. **Move identity out of the mesh to a primitive-range sidecar** when a consumer
   actually needs it. Zero vertex bytes, 32-bit, removes the dim-32 ceiling, and
   frees the far-ring simplifier from per-object grouping (lens 4).
3. **Or delete identity from the mesh entirely** and keep only the manifest —
   see lens 5, which is where the real argument is.

### Lens 4 — what it actually costs

**4A. The marginal cost of identity alone is 4 bytes a vertex, not 0.** AO and
sway are *not* independently gated: the vertex-colour attribute is added only by
`opts.identity` (`src/lodgen.cpp:3403`, `OBJ_VERTEX_DESC_COLORS` vs
`OBJ_VERTEX_DESC`), the colour is written only under `if ( opts.identity )`
(`src/lodgen.cpp:3464`), the sway weight is set only under `if ( opts.identity )`
(`src/lodgen.cpp:3240`), and the AO bake is gated on
`opts.identity && opts.bakeAO` (`src/lodgen.cpp:3306`). With `--no-identity`
there is no vertex colour at all and therefore no AO and no sway either. So the
20 → 24 step is charged wholly to identity today — although *conceptually* two of
the four bytes (B and A) pay for AO and sway, and a redesign could keep those and
drop R+G for 4 bytes' worth of nothing, since a 2-byte-only attribute does not
exist in this format.

**4B. Real bytes.** Vanilla's whole Commonwealth object-LOD corpus, measured
(`btosurvey.py`): dim 4 = 137,724,978 B over 344 chunks; dim 8 = 47,460,135 B
over 97; dim 16 = 3,681,219 B over 20; dim 32 = 410,983 B over 4. **Total
189,277,315 B = 189.3 MB.** Exact vertex totals, summed over every chunk:

| lvl | chunks | vertices | triangles | bytes | identity @4 B | share |
|---|---|---|---|---|---|---|
| 4 | 344 | 5,739,219 | 3,708,637 | 137,724,978 | 22,956,876 | 16.7% |
| 8 | 97 | 2,034,737 | 1,101,390 | 47,460,135 | 8,138,948 | 17.1% |
| 16 | 20 | 155,630 | 89,696 | 3,681,219 | 622,520 | 16.9% |
| 32 | 4 | 17,127 | 10,594 | 410,983 | 68,508 | 16.7% |
| **all** | **465** | **7,946,713** | **4,910,317** | **189,277,315** | **31,786,852** | **16.8%** |

So at 4 bytes a vertex the identity attribute costs **31.8 MB** across a
vanilla-density Commonwealth, **+16.8%** on the object-LOD corpus, and it is
remarkably flat across rings (16.7–17.1%). The extra channels (UV2 + Eye Data,
another 8 bytes) cost a further **63.6 MB, +33.6%**, so the full 32-byte profile
is **+50.4%** on disk and in the vertex buffer.

Two caveats, both stated rather than hidden. (i) These are *vanilla* vertex
counts; this generator's chunks are much larger (`WW_CHANGES.md:299`: dim-16 at
eighteen times vanilla), so the absolute megabytes scale up together and the
*percentages* are the durable numbers. (ii) `.bto` is stored uncompressed and the
vertex buffer is what reaches VRAM, so the +50% applies to VRAM for the resident
set as well as to disk.

**4C. Identity forces NO vertex duplication — the worry is void.** I read the
merge before assuming. `lodgenMergeChunkShapes` (`src/lodgen.cpp:7089`)
concatenates buckets; there is no weld pass, no position hash, no dedupe. In the
builder, each source shape's vertices are appended wholesale
(`src/lodgen.cpp:3228-3265`) with `vBase` offsetting the triangle indices. Two
objects have never shared a vertex in this generator, with or without identity.
Cost of identity in duplicated vertices: **zero**.

**4D. The far-ring grouping costs far less than the overseer fears — and the
number is already in the tree.** This is the most consequential refutation in the
report.

The mechanism the overseer is worried about is that grouping forbids collapses
across object boundaries. **meshoptimizer would not make those collapses
anyway.** `meshopt_simplify*` only collapses along *existing edges of the input
mesh* (`lib/meshoptimizer/src/simplifier.cpp`, the collapse candidates are
enumerated from the index buffer), and separate objects in a merged chunk share
no edges — they are not even welded by position, because nothing welds them
(4C). The only collapses grouping actually forbids are between objects whose
half-precision vertex positions happen to coincide exactly, which is rare and
worth forbidding anyway.

The real costs of grouping are three, and they are smaller and different:

1. **the per-group ratio replaces a global error budget** — meshoptimizer given
   the whole mesh spends its budget where error is cheapest; per group it must
   take the same fraction from a shell that can afford it and one that cannot;
2. **`minTris = 8`** (`src/lodgen.h:523`) keeps every triangle of any group of 8
   or fewer;
3. **the ≥2-triangle floor** (`targetIdx = max(6, …)`, `src/lodgen.cpp:7723`).

And the measurement that bounds all of it is already recorded, in
`WW_CHANGES.md:275-283`: on the ring-2 chunk, **7,626 identity groups across
55,293 triangles** — an average group of **7.25 triangles, i.e. at or below
`minTris` — and the achieved ratio was **0.848** against an asked 0.35, with the
error rail swept from 32 to 2048 and moving it by 0.003. The changelog's own
conclusion is the right one and I did not improve on it: *"The cause is topology
… a shell that small is almost entirely open border, which meshoptimizer will not
collapse."*

So: **grouping is not what costs the reduction; the shells being tiny is.** A
whole-chunk simplification of the same mesh would face the same 7,626 disconnected
open-bordered shells and would do only marginally better — better on budget
allocation (cost 1), identical on borders. I cannot put a number on "marginally"
without running it, and I say so. What I can say with the numbers in hand is that
the overseer's framing — "678 tiny decimation problems instead of one" — misplaces
the blame: it *is* 678 tiny problems, but they would still be 678 tiny problems
with the grouping removed, because they are 678 disconnected components.

Corollary worth noting: `src/lodgen.h:541` records that **shapes with an alpha
property are not simplified at all**. In a forest chunk that is the branch-card
shape, i.e. most of the geometry. The far-ring simplifier's ceiling is set mostly
by what it declines to touch.

### Lens 5 — is the capability even worth it?

**5A. There is no consumer. None. Today the index is written and read by nobody.**

Searched `E:\Projects\Fo4CommunityShaders\fallout4-community-shaders` — the whole
tree, source, headers, shaders and resources — for
`lodgen|LODGEN|manifest\.txt|\.bto|LodChunk|objectIndex|LodManifest`:
**no files found.** A separate walk for any filename containing `lod` in `src/`
returns **nothing**, and in `res/` returns exactly two files, neither related:
`res\Water\WaterLOD.hlsl` and a backup of `ambient_ibl_pass.hlsl`.

FO4CS has no LOD module, no manifest reader, and no code that samples a vertex
colour from an object-LOD chunk. The consumer named in
`docs/LODGEN_VERTEX_PACKING.md:3` as *"CS is the consumer"* does not exist yet.

That reframes everything else in this report. The identity channel currently
delivers **zero** capability, costs 4 bytes a vertex plus a broken stock fallback,
and its only reader is this repo's own debug preview (`WW_LOD_CHANNEL`, the
panel's *Preview channel* box, `docs/LODGEN_VERTEX_PACKING.md:491-494`).

**5B. What the index is for, and whether each use needs it per vertex.**

| use (from the docs) | needs per-vertex? | cheaper substitute |
|---|---|---|
| pair an object across rings by `(ref, part)` | **no** | the manifest already does this; the vertex index is not the key and the docs say so (`:146-156`) |
| kill stitched instance copies and redraw instanced (`I` lines) | per-*draw* | instance-group membership is a manifest list |
| stand a placement on an impostor card (`C` lines) | per-*object* | the manifest row already carries the card |
| wind phase `hash(index)` (`:227`) | per-object | `hash(round(position))` — the position is in the vertex already |
| hue jitter / card frame offset / mirroring | per-object | same |
| screen-size fade | per-object | manifest bound radius (still missing, `:471`) |

Nothing in that list needs a **per-vertex** integer. Everything needs a
**per-object** one, and the mesh is the most expensive place to put a per-object
value: it pays for it once per vertex.

**5C. The strongest point in its favour, tested.** The argument is that an exact
integer can be hashed for free to give per-object variation, the main lever
against LOD forest repetition. It is a good argument and it is **entirely
theoretical today**: the phrase in the contract is *"the phase comes from
`hash(index)` at draw time"* (`docs/LODGEN_VERTEX_PACKING.md:227`) and there is no
draw-time code anywhere that does it (5A). The only hashing that exists is the
viewer's *"identity hashed"* debug preview.

And the honest comparison hurts it further: hashing the **object's world-space
origin** gives the same whole-object-varies-together property, because the origin
is one value per object, and it needs nothing in the mesh — the consumer can take
it from the manifest (per draw) or from a rounded vertex position (per vertex,
already present). The exact-integer property buys *stability across rings*, which
the manifest's `(ref, part)` key already provides more robustly, per the
contract's own §"Identity across rings and bakes: the key is the reference".

**5D. Scored against the three goals.**

| goal | verdict |
|---|---|
| better-looking LOD | **neutral today, positive in principle.** Every good thing it enables is unbuilt. As shipped it makes LOD *worse* — §3 makes it red-black under the stock engine. |
| less visible LOD pop | **neutral.** Pop is fought by the ring-matched `(ref, part)` key, geomorphing and screen-size fade, none of which use the vertex index. |
| performance / memory / draw calls | **negative.** +4 bytes a vertex (+16.8% on a vanilla-density corpus, 4A/4B), and it is the reason the far-ring simplifier is a grouped pass — even though grouping turns out to cost little (4D), the code complexity is real. It buys no draw-call reduction; the merge does that, and the merge does not need identity. |

**5E. The case for deleting it.** Steelmanned, it is strong:

* `--no-identity` already exists (`src/nifcli.cpp:4740`, `src/lodgen.h:206`), so
  the deletion path is built and tested;
* deleting it restores the descriptor to `0x1B00000430205`, **byte-identical to
  vanilla**, and makes the entire stock-tolerance question moot rather than
  merely answered;
* it removes the dim-32 capacity ceiling (lens 2A) by removing the counter;
* it removes the aliasing failure mode (1A) and the flags-2 defect (§3);
* the manifest survives untouched and keeps every capability that is actually
  keyed on `(ref, part)`;
* nothing consumes it (5A).

What is genuinely lost: the ability to attribute a **pixel** to an object, which
matters for exactly one class of future feature — per-object shading a consumer
computes in screen space, e.g. an impostor card that must know which tree it is
in order to pick its octahedral frame. That is a real use and it is the reason
not to delete outright.

**So the honest recommendation is not "delete" but "do not put it in the mesh."**
Keep the capability, move it to the primitive-range sidecar (lens 3.1), which
costs zero vertex bytes, is 32-bit, and keeps the stock file byte-identical to
vanilla's descriptor. Until a consumer exists, ship `--no-identity` as the
default and leave the channel behind the flag.

---

## 5. REFUTED

Claims that were put to me, or that I made myself, and that did not survive.

**R1. "The far-ring simplifier grouping is the real cost of identity" — refuted
as stated.** The brief asked me to quantify how much achievable reduction the
per-(identity, layer) grouping gives up. The premise is that grouping forbids
collapses across object boundaries. It does, but **meshoptimizer would not make
those collapses anyway**: `meshopt_simplify*` enumerates collapse candidates from
the input index buffer, so it only collapses along edges that exist, and separate
objects in a merged chunk share no edges (nothing welds them — see 4C). Grouping
therefore forbids almost nothing that would otherwise happen. The reduction is
lost to **topology**, which `WW_CHANGES.md:275-283` had already measured (7,626
groups over 55,293 triangles, 0.848 achieved against 0.35 asked, error rail swept
32 → 2048 for 0.003 of movement). Removing the grouping would leave the same
7,626 disconnected open-bordered shells. See 4D.

**R2. "Every slot is inert to the stock engine" — refuted, and it is the
report's headline.** `docs/LODGEN_VERTEX_PACKING.md:9-12`. `SLSF2_Vertex_Colors`
is set by `src/lodgen.cpp:3505` and is read by the engine at RVA 0x0027cea0f to
select the `Vc` permutation. See §3.

**R3. "A wider descriptor, generally, is the open stock-tolerance risk" —
refuted.** `docs/LODGEN_VERTEX_PACKING.md:484` and `docs/TO_BE_IMPLEMENTED.md`
~232 treat the fatter descriptor as an unrun risk in general. Measured over the
**whole** vanilla mesh corpus — 34,985 `.nif`, 108,010 shapes — the 24-byte
identity descriptor `0x3B00005430206` is **byte-identical to one vanilla ships
29,242 times**, and vanilla ships strides up to 44. The risk is not width. It is
`VF_UV_2` specifically, which appears in **zero** of the 108,010 shapes. That is
a much narrower and much more testable claim than the one on the page. See 1D.

**R4. "The set of identity indices in a chunk is invariant" — refuted for the
builder.** `docs/LODGEN_VERTEX_PACKING.md:87-88` states it without qualification.
It is true of `lodgenSimplifyFarRings`. It is false of `lodgenBuildObjectChunk`,
because the 65,535-vertex bucket guard at `src/lodgen.cpp:3214-3215` drops a
source shape with a bare `continue` after the manifest row has already been
written and while `objectIndex++` still runs. See 2.a2.

**R5. "The index wraps silently past 65,536" — refined, and the reality is
worse.** The *mesh* wraps; the *manifest* does not, because
`src/lodgen.cpp:3098` writes the unmasked counter. So the two diverge rather than
wrapping together, and the far-ring simplifier merges the two aliased objects into
one group and may weld their geometry. See 1A.

**R6. "UV2 is half-float with about 11 bits of mantissa, so it cannot hold a
16-bit integer" — upheld, but the reasoning offered was loose and the number
matters.** Binary16 has 11 *significant* bits including the implicit one, so it
represents every integer up to **2048** exactly and **2049 rounds to 2048**
(measured over 0..70,000). "About 11 bits of mantissa" understates the problem by
implying ~2048 is a soft limit; it is exact and hard, and it is below the 678-object
chunk's needs by only a factor of three. See 2D.

**R7. "The engine only takes vertex alpha when `SLSF1_Vertex_Alpha` is set" —
not refuted, but downgraded from measured to unsupported.**
`docs/LODGEN_VERTEX_PACKING.md:201-205` presents this as measured. What was
measured is that the *flag is clear in the file* — which I reproduced. The engine
half is not supported by anything I could find: flags1 bit 3 is never tested in
`BSLightingShaderProperty::GetRenderPasses_Forward`, where every other
technique-selecting flag is, and `GeometrySetupConstantAlpha` only writes a
scalar. See 1B.

**R8. My own first reading — "`OBJ_VERTEX_DESC` is corrupt" — refuted by me.**
Decoding the hex in the source comment (`0x1B00000650405`) gives UV at +16 and
normal at +20 inside a 20-byte vertex, which would be a hard file bug. Decoding
the *decimal literal the compiler actually sees* gives `0x1B00000430205`, which
is correct and is what vanilla ships. **The code is right; only the comment and
the contract document are wrong.** Recording this because a reviewer reading the
doc rather than the literal would file a phantom bug, exactly as I nearly did.
See 2.b.

**R9. "Identity forces vertex duplication" — refuted.** Nothing in this
generator ever welded vertices across objects, so identity prevents nothing. See
4C.

**R10. "Triangle order may not survive the merge and simplifier, so
`SV_PrimitiveID` is unusable" — refuted on the generator side.** Order is
deterministic and, better, already **contiguous per object**: the builder appends
per placement into `bucket.cellTris[cellIdx]`, and the simplifier emits group by
group before re-binning by cell. A primitive-range table is therefore small, not
per-triangle. What remains unproven is the FO4CS side, not this one. See 3.1.

---

## 6. UNVERIFIED

Ordered by how much they would change the conclusions.

1. **Does `Shaders011.fxp` contain a compiled `BSLighting Vc LODObj`
   (technique 0x0D01)?** I extracted the package (12,844,936 B, one file in
   `Fallout4 - Shaders.ba2`) and located all **3,939 DXBC blobs** and the record
   header (`0x11223344`, `u32 blobSize`, `u32 index`, `u32 0x0FFF`, 40-byte
   constant table, blob). The per-record dword is a **sequential index within a
   section**, not a technique id — ids run 0,1,2,… — and I found **no static
   technique-id array in the exe** (scanned every 4-aligned occurrence of 0x0D00
   and 0x0D01 for a small-integer neighbourhood; zero hits). The mapping is built
   at load by `BSShader::Load` into `BSShaderTechniqueIDMap`, and reading it needs
   another disassembly pass I did not have budget for. **This does not change the
   fix**, because both branches (permutation exists → tinted red-black;
   permutation missing → lookup misses and the pass has no shader) are bad.
2. **Can FO4CS identify, at draw time, which `.bto` shape a draw belongs to, and
   read its start index / base vertex?** This is the load-bearing unknown for the
   recommended alternative (3.1). It is a question about FO4CS's hook surface,
   not about the generator, and I did not audit the hook layer.
3. **Does the stock engine draw a many-segment `BSSubIndexTriShape` as one draw
   call?** Decisive for alternative 3.2 and not settled.
4. **Is `VF_UV_2` tolerated by the stock engine on an object?** Vanilla never
   ships it (1D). This is now the only real part of the "fatter descriptor"
   worry, and it is testable in ten minutes in-game.
5. **Which DXGI format the engine binds for `VA_COLOR`.** The exactness argument
   in 2D is sound in principle (DXGI has no `_SRGB` vertex format), but I did not
   read the engine's input-layout construction to confirm.
6. **Actual placement counts per chunk.** Lens 2A's object counts are modelled
   from a single vertices-per-object ratio (49.7, from one chunk). The direct
   measurement needs the ESM walk, which needs the CLI. The command that would
   settle it, for the overseer to run:

       ./release/NifSkope.exe -no-gui lodgen "<Fallout4.esm>" --worldspace 3C \
           --objects 4 -4 --dim 4 --data-root "<DataUnpacked\Data>" -o <tmp>.bto

   then count rows in `<tmp>.bto.manifest.txt`. Repeat at `--dim 32` over
   downtown Boston to test the ceiling directly.
7. **Is Eye Data inert to the stock engine outside the eye path?** Vanilla ships
   `VF_EYEDATA` only alongside `SKIN` (119 shapes in the sample), so its behaviour
   on an unskinned LOD object is unattested. Bears on alternative 3.4.
8. **One loose end in the descriptor survey, flagged for honesty.** The
   full-corpus run completed (`descsurvey_full.txt`) and the `VF_UV_2 = zero`
   result is a corpus fact. But the per-combination counts and the per-stride
   counts do not line up row for row — e.g. `VERT+UV+NRM+TAN` is 60,832 shapes
   while stride 20 is 53,981 — so the *same* attribute set occurs at more than
   one stride in the shipped data (most likely dynamic/full-precision variants).
   Every row still passed the `Data Size` self-check, and the `VF_UV_2` finding
   is read from the flag field and is unaffected, but I have not explained the
   discrepancy and I am not going to pretend I have.
9. **Whether the running CLI agrees with `nifpeek.py`.** The sandbox declined to
   execute `./release/NifSkope.exe -no-gui lodgen --dump-*` from this session, so
   every binary measurement here comes from the from-scratch parser instead. Its
   self-check is that it recomputes `Data Size` from `Num Vertices` and
   `Num Triangles` for every one of the 19,436 shapes it parsed and discards the
   39 that disagreed; on the two vanilla `.BTO` shapes and the vanilla `.BTR` it
   matches exactly. I consider it sound but it is not the shipping tool.
10. **Everything in 1C beyond the `.BTR` measurement** — precombines, previs, the
    CK, xEdit — is reasoning, not measurement.


---

## 3. LENS 3 — SHADING (done personally; the lens agent was killed at the
## session limit before it reported)

Tool: `nifpeek.py` (from the IDENTITY lane, read-only) plus `btrsurvey.py`.
`nifpeek` self-checks — it prints `datasize=N(calc N)` per shape and
`size=N (walk ends N)` per file; both agree on every BTR opened here, so the
block walk and the vertex-descriptor decode are byte-exact.

### 3.1 Vanilla terrain LOD carries NO vertex colours — measured

`python btrsurvey.py`, 100 BTRs sampled across all four levels:

    vertex attribute sets seen: {'VERTEX+UVs': 100, 'VERTEX': 79}
    shader flag pairs seen    : {('0x80401000','0x00000003'): 100,
                                 ('0x80000000','0x00000001'):  79}

Every file has a `BSTriShape` named `Land` with `desc=0x300000000203`,
attributes **VERTEX + UVs and nothing else**. `COLORS` (vertex-descriptor bit
0x0020) is **never** present, on any tile, at any level, inside or outside the
playable region. The second shape (79 of 100 files) has attributes `VERTEX`
only and a `BSEffectShaderProperty` — it is the water plane, not terrain.

**So the "terrain LOD vertex colour multiplies into the picture" premise in the
brief is false for FO4.** There is no vertex colour on these meshes to change.
Whatever VCLR contributes is baked into the *texture* — which is exactly what
xLODGen's readme says its slider does: "Vertex Color Intensity ... controls how
strong the vertex color overlay is painted **on top of the terrain LOD
textures**."

### 3.2 Shader flags — and the fork matches vanilla exactly

Decoded against the repo's own `build/nif.xml`,
`Fallout4ShaderPropertyFlags1` at line 7000 and `...Flags2` at line 7036:

`flags1 = 0x80401000` — bits 12, 22, 31:

| bit | name | note |
|---|---|---|
| 12 | **F4SF1_Model_Space_Normals** | confirms the `_msn` is model-space, as section 1.1 measured |
| 22 | F4SF1_Own_Emit | |
| 31 | F4SF1_ZBuffer_Test | |

`flags2 = 0x00000003` — bits 0, 1:

| bit | name |
|---|---|
| 0 | F4SF2_ZBuffer_Write |
| 1 | **F4SF2_LOD_Landscape** |

Bit 5, `F4SF2_Vertex_Colors`, is **clear** — consistent with 3.1.

Now compare with what this fork writes, `src/lodgen.cpp:59-60`:

    LAND_SHADER_FLAGS1 = 2151682048  = 0x80401000   -> IDENTICAL to vanilla
    LAND_SHADER_FLAGS2 = 3           = 0x00000003   -> IDENTICAL to vanilla

**The fork's shader flags are byte-for-byte vanilla.** Its only terrain-normal
defect is the channel order found in section 1.2. Worth stating plainly so that
finding is not over-read.

### 3.3 At distance the mesh carries nothing — the `_msn` carries everything

Triangle counts, 25 tiles sampled per level:

    level    mean tris  mean verts   tris per CELL
      4        1021        560          63.84
      8        1096        711          17.13
     16        1524       1956           5.95
     32        2932       3608           2.86

A level-32 tile spends **2.86 triangles per cell** — a cell is 4096 game units
across. At that density the interpolated mesh normal describes essentially
nothing, so all surface shading must come from the `_msn` texture. This is the
mesh-side confirmation of section 1.4's texture-side result, reached
independently. **A bad or absent `_msn` is catastrophic at distance and nearly
invisible up close** — which matches bungo's screenshots, where near ground is
fine and the far range is a silhouette.

### 3.4 Bethesda's own outer meshes are simpler, but they are all there

    fully textured tiles with a BTR: 214 ; fully untextured tiles with a BTR: 2023
    TEXTURED     n=30  mean tris 2087  mean verts 1437
    UNTEXTURED   n=30  mean tris  891  mean verts  493

Outer tiles get ~43% of the triangles of playable ones. But the mesh pyramid is
**complete and gapless at every level**, exactly like the textures:

    BTR files: 3060
    per level: {4: 2304, 8: 576, 16: 144, 32: 36}
      level  4: 2304 tiles, x -96..92  y -96..92, full grid would be 2304
      level  8:  576 tiles, x -96..88  y -96..88, full grid would be  576
      level 16:  144 tiles, x -96..80  y -96..80, full grid would be  144
      level 32:   36 tiles, x -96..64  y -96..64, full grid would be   36

So the "far cells only exist at coarse levels" hypothesis is **dead for both
meshes and textures**.

### 3.5 The `noise.dds` darkening story does not hold for FO4 — measured

xLODGen's readme blames "darkening caused by `textures\terrain\noise.dds`".
FO4's copy is at `E:\Tools\Fallout 4\DataUnpacked\Data\Textures\Terrain\Noise.dds`
(there is also a `DetailNormals.dds`):

    Noise.dds  1024x1024  BC1_UNORM  mips 11
    mean rgb 241.1 236.7 237.2   lum 237.7   lumStd 38.2

Mean level 237.7/255 = **0.932**. A multiply by this costs about **7 %**
brightness, uniformly and achromatically. That cannot produce the change bungo
is describing, and it cannot desaturate anything. **Candidate rejected for FO4**
— see REFUTED. (The readme's sentence is aimed mainly at Skyrim SE, whose
`noise.dds` is a different texture.)

### 3.6 Atmospheric perspective — honestly, not decidable from files here

Shot one is hazier and warmer, shot two crisper and darker. FO4 fades distant
terrain toward a fog colour, so a rebake that changed the mesh bounds or the LOD
level assignment would move the same mountains along the fog curve. I measured
that the **mesh pyramid and the texture pyramid are both complete and
identical in extent** (3.4, section 0), so a *vanilla* install has no level
assignment to get wrong. Beyond that: the fog colour lives in WTHR weather
records and the fade distances in INI settings, and I have not read either, nor
can I compare against a rebake that does not exist on this machine.
**UNVERIFIABLE from files alone in this lane** — stated rather than
manufactured. Note also that the load order includes `F76Weathers.esp` and
`UltraExteriorLighting.esp` (from the TexGen log), so his weather is modded;
if the two screenshots were taken under different weather or time of day, some
of the difference is not LOD at all.

# MOUNTAINS — ADDENDUM from the IDENTITY lane

Written to a separate file **on purpose**: `report_mountains.md` was being
written by another session while I worked (its mtime advanced from 12,684 to
21,316 bytes under me), and overwriting it would have destroyed that work. Fold
these in, do not re-derive them.

Two contributions, both MEASURED, neither of them in `report_mountains.md` as of
its §4.3.

---

## A. The decisive comparison is NOT impossible — real xLODGen-family output DOES exist on this machine

`report_mountains.md` §4.2 concludes:

> **THEREFORE: the decisive comparison this brief asked for — measure a real
> rebake tile against vanilla — CANNOT BE MADE ON THIS MACHINE.**

That is correct **for Fallout 4** and I have not falsified it. But it is too
strong as written, because a complete xLODGen-family terrain-LOD bake, *with
normal maps*, is sitting on this disk for the Fallout 3 / New Vegas path:

    E:\Tools\ModOrganizer\mods\FNVLODGen Output\

a Mod Organizer 2 mod folder whose `meta.ini` has `modid=0`,
`installationFile=` empty and `[installedFiles] size=0` — MO2's signature for a
folder produced by a **local tool run**, not installed from a downloaded
archive. Tile mtimes are **2024-03-29 20:06–20:10**, i.e. a single ~4-minute
generation session. This is the same tool family, and very likely the same runs,
as the `Game Mode: TerrainFO3` / worldspace `WastelandNV` entries that §4.2
already found in `E:\Tools\xLODGen\LODGen_log.txt`.

It contains, per worldspace, a `diffuse\` **and a `normals\`** directory —
for `wastelandnv` alone, **1,700 diffuse and 1,700 normal tiles**, plus
`bouldercityworld`, `dcworld01..18`, `dlc01..dlc4*`, `freeside*`, `lucky38world`,
`nvdlc01..04*`, `thestripworldnew`, `washmontop`, `wasteland` (4,165 tiles),
`wastelandnvmini`. Total on the order of 20,000 generated tiles.

Header, via `dds.py info`:

    wastelandnv.n.level16.x-16.y-16.dds  256x256 BC1_UNORM mips=9 fourcc=DXT1
                                         dataOff=128 fileSize=43832 expect=43832

(`fileSize == expect`, so the mip walk is byte-exact and nothing is being folded
into a mean — same self-check as everywhere else in this report.)

### The measurement: xLODGen writes terrain LOD normals with UP IN BLUE

`python msn_updecide.py <paths>` — the same two-property test §1.1 used to settle
vanilla FO4 (determinism of the up component, and one-sidedness), run at mip 2 on
a full decode:

    tile                                | mean |predicted-actual| (bytes) | fraction of pixels < 128
                                        |    R      G      B    |    R      G      B
    wastelandnv.n.level16.x-16.y-16.dds |   51.0   48.9    9.5  | 0.567 0.541 0.000   -> up = B
    wastelandnv.n.level16.x-16.y0.dds   |   44.6   45.0    9.2  | 0.523 0.502 0.000   -> up = B
    wastelandnv.n.level16.x-16.y32.dds  |   63.0   69.1   12.1  | 0.650 0.674 0.000   -> up = B
    wastelandnv.n.level8.x8.y8.dds      |   39.8   44.1    8.5  | 0.392 0.432 0.000   -> up = B

Blue wins both tests on every tile: residual 8.5–12.1 bytes against 39–69 for red
and green, and **exactly zero pixels below 128 in blue** while red and green fall
below 128 for 39–67% of pixels. These are real, varying normals, not a flat fill.

Compare §1.1's result for vanilla Fallout 4, which by the identical test gives
**up = G on every tile at every LOD level**.

### What this does and does not prove — read this part before quoting the above

**It proves:** the xLODGen family, on its Fallout 3 / New Vegas terrain-LOD path,
emits normals in the conventional `(X, Y, Z) -> (R, G, B)` order with up in blue.
That is the same order `src/lodgen.cpp` uses (§1.2), and the opposite of what
Fallout 4 ships.

**It does not prove:** that xLODGen's *Fallout 4* terrain-LOD path does the same
thing. Nobody has ever run it on this machine (§4.2 established that, and I
re-confirmed it: `E:\Tools\Fallout 4\FO4Edit\FO4Edit.exe` contains **zero**
occurrences of the string `_msn` in either ASCII or UTF-16, while it does contain
`.btr`, `.bto`, `lodsettings` and `diffuse` — so the FO4-side terrain-LOD normal
writer is not even present in the FO4Edit build that is installed here). It is
entirely possible that xLODGen special-cases FO4's model-space convention, and
nothing on this disk can say either way.

**Why it still moves the needle a long way.** §1.2's transposition finding was, on
its own, a fact about bungo's fork with no bearing on his question. The honest
status of "does a third-party rebake transpose the channels?" was *pure
speculation*. It is now: **the tool demonstrably uses up-in-blue for the sibling
games, and Fallout 4 demonstrably needs up-in-green.** That is a specific,
falsifiable prediction rather than a guess, and it is cheap for bungo to falsify —
one generated FO4 tile and one `msn_updecide.py` run.

### The one command that would settle it

Run xLODGen (`E:\Tools\xLODGen\xLODGenx64.exe -FO4 -o:C:\Output`) for
Commonwealth, LOD4, one cell region, then:

    python msn_updecide.py "C:\Output\Textures\Terrain\Commonwealth\Commonwealth.4.-20.24_msn.DDS"

If it prints `up = B`, the transposition hypothesis is confirmed for the real
tool and §1.2's shading simulation (−68% light, −92% variation) applies directly
to bungo's screenshots. If it prints `up = G`, the hypothesis is dead and
§4.3(a)'s "Default normal size" down-sizing of outer regions becomes the answer.

---

## B. The `.BTR` question (lens 3), settled from the file and from the exe together

Measured with `nifpeek.py` (the from-scratch NIF walker written for the IDENTITY
lane; it reproduces every `Data Size` from `Num Vertices` and `Num Triangles`
exactly, which is its self-check):

    == E:\Tools\Fallout 4\DataUnpacked\Data\Meshes\Terrain\Commonwealth\Commonwealth.4.-20.24.BTR
       blocks=13
       #1 BSTriShape name='Land' desc=0x300000000203 stride=3 (=12 bytes)
          tris=2066 verts=1068 datasize=25212 (calc 25212)
          attrs = VERTEX + UVs
       #2 BSLightingShaderProperty  ShaderType=18  name=''
          flags1=0x80401000  bits 12, 22, 31
          flags2=0x00000003  bits 0, 1

Three results, each of which matters:

1. **Vanilla FO4 terrain LOD carries NO vertex colours.** The descriptor's
   attribute set is `VERTEX + UVs` and nothing else — 12 bytes a vertex. So a
   rebake's vertex colours cannot be the cause of the darkening *in vanilla-like
   output*, because vanilla has none to change. (Bit names from
   `build/nif.xml:7000` / `:7036`.)

2. **The Land mesh carries no vertex NORMALS either.** There is no `VF_NORMAL` in
   the descriptor. So every scrap of surface orientation the terrain LOD shader
   has comes from the `_msn` texture. This corroborates §1.4's conclusion — "at
   distance essentially all terrain shading comes from the `_msn`" — from the
   file format side, independently of the correlation analysis that produced it.
   A wrong `_msn` has *nothing* to fall back on.

3. **`flags1` bit 12 is `F4SF1_Model_Space_Normals`, and it is SET.** This is the
   engine-side proof that FO4 terrain LOD normal maps are model-space rather than
   tangent-space. Cross-checked against the exe: in
   `BSLightingShaderProperty::GetRenderPasses_Forward` (1.10.155, RVA
   **0x0027cea44**) the sequence

       and edx, 0x800000   ... or eax, 8      ; flags1 bit 23 Projected_UV -> 'Projuv'
       and ecx, 0x1000     ... or eax, 4      ; flags1 bit 12             -> tech bit 2

   and `BSLightingShader::GetTechniqueName` (RVA 0x0289e6e0) names technique bit 2
   **`Msn`** (string literal at RVA 0x03098534, read out of the PE). `ShaderType=18`
   in the same block decodes, from the 20-entry jump table at RVA 0x0289E884, to
   **`LODLandNoise`**. So the shipped `.BTR` asks the engine for
   `BSLighting Msn LODLandNoise`.

   Model-space plus a measured up-in-green means FO4 stores its model-space
   normals swizzled to `R = X (east), G = Z (up), B = Y (north)` — up in the
   6-bit channel of RGB565. A tool that writes model-space `(X, Y, Z)` in the
   naive order produces a normal that points *north* instead of *up*, which is
   exactly the failure §1.3 simulated.

---

## C. Lens 2 is not lost — the killed agent's scan survived on disk, and I reproduced its result

`report_mountains.md` lists lens 2 as still owed and likely unanswerable without
the CLI. It is answered. The lens-2 agent got far enough to write
`lens2\lens2.md` and, more importantly, to save its parsed scan as
`lens2\land3C.npz` before the session limit killed it. It did **not** use the
repo CLI (which refused to run for it as it refused for me); it wrote its own
read-only ESM walker, `lens2\esmwalk.py` + `lens2\scan.py`, taking the record,
GRUP and LAND layouts from `lib/libfo76utils/src/esmfile.cpp:21-140,450-479` and
`src/esmdata.cpp:301-363` — cited, not guessed.

I read that parser rather than trusting its prose, and recomputed the split
myself from the saved arrays (`vclrcheck.py`):

    grid 192 x 192, origin (-96, -96)
    CELL 36864   LAND 36864   VHGT 36864   VCLR 2362   VNML 36864
    playable cells 3844 (WRLD 3C MNAM box NW(-33,25) SE(28,-36)), outside 33020

    LAND   inside 3844 (100.00%)   outside 33020 (100.00%)
    VHGT   inside 3844 (100.00%)   outside 33020 (100.00%)
    VCLR   inside 2140 ( 55.67%)   outside   222 (  0.67%)

    north of the playable box (cell y >= 26): 13440 cells, VCLR on 186 (1.38%)

    cell ( -20, 24): LAND 1 VHGT 1 VCLR 1     <- Sanctuary, playable
    cell ( -20, 40): LAND 1 VHGT 1 VCLR 0
    cell ( -20, 60): LAND 1 VHGT 1 VCLR 0     <- the mountains he is looking at
    cell ( -20, 80): LAND 1 VHGT 1 VCLR 0
    cell (   0,  0): LAND 1 VHGT 1 VCLR 1
    cell ( -60, 60): LAND 1 VHGT 1 VCLR 0

**Two results.**

**C1. "The far cells have no source geometry" is refuted.** Every one of the
36,864 cells in x −96..95 / y −96..95 has a CELL with XCLC, a LAND record, a
VHGT and a VNML — zero holes, and that set is exactly the set covered by the
level-4 LOD tiles. The mountains have height data. Whatever goes wrong in a
rebake, it is not missing terrain.

**C2. A second region-specific mechanism, and it is sharper than the first.**
VCLR — the hand-painted vertex colour the landscape shader **multiplies** into
the ground — is present on **55.67% of playable cells and 0.67% of cells outside
the playable box**; north of the box, where his screenshots are pointed, on
**1.38%**. Neutral VCLR is 255, so *absence is a no-op in vanilla* and vanilla is
consequently unaffected. But a tool that treats an absent VCLR as **0** rather
than 255 blackens 99.33% of the outer region and leaves the playable area mostly
untouched — which is, to the letter, the difference between his two screenshots.

**Our fork does not have this bug**, and I checked rather than assumed:
`src/lodgen.cpp:5163` guards the multiply with `if ( land.hasColors )`, so a
cell without VCLR is left neutral. So this is a hypothesis about the third-party
tool, and its **exposure is measured** (99.33% of outer cells carry no VCLR)
while the **tool's behaviour is not** — nobody has run xLODGen for FO4 on this
machine. It joins §4.3(a)'s "Default normal size" and §A's channel order as the
third candidate, and it is the cheapest of the three to test: generate one outer
cell and one playable cell with the same settings and compare their LOD diffuse
brightness against vanilla's.

Note also `E:\Tools\xLODGen\Terrain-LOD-Readme.txt`'s **"Vertex Color
Intensity — 1.00 = 100%"** slider, already quoted in §4.3(c). A tool that exposes
a multiplier on this channel is a tool that has an opinion about what to do when
the channel is absent.

**Provenance caveat, stated plainly:** I verified this by reading the killed
agent's parser, checking its cited layout sources against the repo, reproducing
its arithmetic from its saved arrays, and spot-checking six named cells. I did
**not** independently reimplement the ESM walk. If that matters for a decision,
re-run `lens2\scan.py` — it takes 24 seconds.

---

*Provenance: everything above was measured by the IDENTITY lane on 2026-09-07
using `dds.py`, `msn_updecide.py`, `nifpeek.py`, `pestr.py` and
`tools/exere/f4pdb.py`. No file in any repo or game folder was modified.*

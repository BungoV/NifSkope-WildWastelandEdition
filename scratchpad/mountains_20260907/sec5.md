
---

## 5. REFUTED — claims that were made in this lane and killed

**R1. My own first `_msn` test was worthless.** `msn_convention.py` tried to
pick the encoding by which decoding produced unit-length vectors. Four of its
six hypotheses (H1, H2, H4, H5) are **permutations of the same three
components** and therefore have identical magnitude by construction — the test
could never separate them, and it printed "<== UNIT" against several at once.
Killed and replaced by `msn_updecide.py` (section 1.1), which uses determinism
and one-sidedness instead. **Lesson kept: a test that cannot distinguish its
hypotheses is not evidence, however confident its output looks.**

**R2. "Vanilla's far LOD textures are brighter/more colourful than the playable
ones."** Not the mechanism. Full mip-0 decodes (section 0) and the 40-vs-40
tile comparison (section 2.2) show far tiles at lum 71.0 vs 66.4 playable —
*slightly* brighter, and notably more saturated (0.191 vs 0.155), but nothing
like the difference in the screenshots. The shipped diffuse is not where the
vanilla-vs-rebake gap lives; **the gap is that a rebake cannot produce that
diffuse at all** (section 2).

**R3. "Terrain LOD vertex colour multiplies into the picture, so a change there
is a direct brightness change."** False for FO4. Measured in section 3.1:
**no** vanilla Commonwealth `.BTR` at any level carries a `COLORS` vertex
attribute, and `F4SF2_Vertex_Colors` is clear in the shader flags on all 100
sampled. There is no mesh vertex colour to change.

**R4. "Far cells only have coarse LOD (16/32), which is why they look flat."**
Dead for both textures and meshes. Section 0 and section 3.4: all four levels
are complete, gapless squares over the identical extent — textures
2304/576/144/36, meshes 2304/576/144/36.

**R5. "The darkening is `textures\terrain\noise.dds`."** Rejected for FO4.
Measured in section 3.5: FO4's `Noise.dds` has mean lum **237.7** (0.932 of
white) with `lumStd 38.2`. A multiply by it costs about 7 %, achromatically.
It cannot desaturate and it cannot flatten.

**R6. "The worldspace supplies a default land texture that the outer region
falls back to."** I raised this myself as the strongest challenge to my own
section 2, and killed it. `wrlddump.py` dumps every subrecord of WRLD
`0000003C`: there is **no LTEX reference anywhere on the record**. `DNAM` is
`(0.0, 450.0)` — two *floats*, default land height and default water height,
not formids. (My first attempt, `defaulttex.py`, misread `DNAM` as two formids
and reported a "default water texture 43E10000"; `43E10000` is the float 450.0.
Corrected.) So the outer region has no texture assignment **and** no worldspace
default to inherit — which makes section 2's conclusion stronger, not weaker.

**R7. The overseer's leading hypothesis — that a tool writing up-in-blue is why
bungo's mountains went dark — is NOT established, and the evidence points
elsewhere for *his* question.** The mechanism is real and I quantified it
(section 1.3: -68 % light, -92 % shading variation), and the fork really does
write up in blue (section 1.2). But:
* it is measured **only for this repo's generator**, never for xLODGen or
  DynDOLOD, and no rebake output exists on this machine to test (section 4.2);
* a channel transposition would break **all** rebaked terrain equally, near and
  far. bungo reports a change confined to distant terrain outside the playable
  area. The missing-source-data explanation is region-specific *by
  construction* and matches that;
* **fairness caveat, stated because it weakens my own argument:** ground within
  the loaded-cell radius is full-resolution landscape, not LOD, so his
  screenshots do not cleanly test mid-distance terrain LOD *inside* the playable
  area. If that region is also darker in shot two, R7's reasoning weakens and
  the normal-map theory rises. **That is the single cheapest thing he can check
  and it is item 1 of section "WHAT TO CHECK".**

**R8. My own tooling had a real bug, found by the lens-2 agent before it died:**
`dds.py`'s BC4/BC3 alpha interpolator used weights `(7-i):(1+i)` where the
format specifies `(6-i):(1+i)`, which can also exceed 255. Fixed, palettes
re-verified in range and monotone, and every alpha claim re-measured afterwards
(`alpha_recheck.py`): diffuse and `_msn` alpha are a constant 255, min and max,
on all six tiles retested. **RGB was never affected**, so no colour number in
this report moves.

---

## 6. UNVERIFIED — what I could not check here, and exactly what would settle it

**U1. What xLODGen and DynDOLOD actually write into `_msn` — which channel is
up.** *This is the one measurement that would answer bungo's literal question
and I could not make it.* No rebaked FO4 terrain LOD exists anywhere on this
machine (section 4.2, and a 50004-directory sweep in 4.4). **To settle it:** run
xLODGen (`E:\Tools\xLODGen\xLODGenx64.exe -fo4 -o:"C:\LODTest\"`), Terrain LOD,
worldspace Commonwealth, Chunk = level 4 at W/S of one *outer* tile and one
*playable* tile, then
`python msn_updecide.py "C:\LODTest\Textures\Terrain\Commonwealth\Commonwealth.4.<x>.<y>_msn.DDS"`.
Green residual small and zero pixels below 128 -> xLODGen is correct. Otherwise
the transposition is real in xLODGen too and R7 flips.

**U2. What colour xLODGen substitutes for an untextured cell.** Its readme says
"the entire terrain LOD texture is the default landscape texture", but I proved
in R6 that Commonwealth defines no default land texture, so the substitute is
whatever is hardcoded in the tool. **This is the missing half of the DARKER
symptom.** Measurable by the same test run as U1: decode the generated outer
tile and compare its lum against vanilla's **71.0** and its meanSat against
**0.191**.

**U3. What xLODGen's "Default diffuse size" / "Default normal size" default
to.** The readme documents the controls, not their defaults, and the tool's UI
cannot be opened in this read-only lane. If "None" is the default, the outer
tiles are not shrunk and only the flat-default-colour half of section 2.3
applies.

**U4. Whether FO4's terrain shader applies `Noise.dds` at all.** I measured the
texture (3.5) but not its use. `Fallout4 - Shaders.ba2` is on disk and would
settle it.

**U5. Fog, weather and LOD fade.** Section 3.6. Fog colour lives in WTHR records
and fade distances in the INI; I read neither. Note his load order contains
`F76Weathers.esp` and `UltraExteriorLighting.esp`, so **if the two screenshots
were taken under different weather or time of day, part of the difference is not
LOD at all.** Worth ruling out before anything else is changed.

**U6. How Bethesda actually authored the outer-region LOD.** Section 2.4 proves
it is not derivable from `Fallout4.esm`'s LAND layers, because there are none.
By what route they made it — hand painting, a source landscape that was later
stripped, an internal tool — is not answerable from shipped files.

**U7. The repo CLI.** `./release/NifSkope.exe -no-gui lodgen ... --dump-land`
is refused by this sandbox, so lens 2 was done with my own ESM walker instead.
The walker self-checks (36864 LAND records = exactly 192x192, every one with
VHGT, extent matching both the LOD pyramid and the worldspace `NAM0`/`NAM9`
bounds of +-393216 units = +-96 cells), but an independent `--dump-land` run by
the overseer would be a free confirmation.

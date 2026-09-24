## Far terrain takes the object texture family: the `.lodt` mask and emissive sheets, and the ring-0 gate (2026-09-11, lane TERRAIN-R)

bungo's ruling of 2026-09-11 09:5x, verbatim: *"you can mirror how it's set up
for the .lodm"*, and *"we just add the coverage for whatever's missing in
terrain textures that lod objects have in the texture department"*. The terrain
pyramid's sheets are the OBJECT family's now.

**The container is version 2.** Version 1's third sheet was role 3 `data` — R
sky AO, G flow wetness, B shore proximity, A ground cover, a channel set terrain
invented for itself. It is replaced by **role 5 `mask`**, the `rmaos` slot's own
channels in the `rmaos` slot's own order — **R roughness, G metallic, B AO, A
ground cover** — and an optional **role 6 `emissive`**, RGB BC1, written only
when a layer's material supplies an emissive map and absent otherwise. Colour,
model-space normal and height are unchanged. **Shore proximity and wetness are
dropped**: shore is a runtime subtraction from the `.lodl` water planes, wetness
is a close-up effect and far wetness is a weather state.

A **v1 file is refused by name**, not converted — it says what role 3 used to
mean. Nothing exists to convert: the writer is opt-in behind `--vt` and bungo's
installed `Data\Terrain` holds no `.lodt` (listed read-only).

**The mask law, one shared home.** `lodgenResolveMaterialMask()` answers "what
roughness and metallic does this material have" for exactly three cases, and
every layer's answer NAMES the rule that produced it: a **PBRM** gives its RMAOS
R and G (discovery is the renderer's own same-name rule, plus a diffuse-stem
fallback through `lodmSourceCandidate()` because most Fallout 4 landscape TXSTs
name no material at all); a **legacy** material gives `1 − smoothness × the _s
map's GREEN channel` and **metallic 0, never a guess**; **nothing** gives
roughness 1.0, fully rough, the honest unknown. The gloss itself is
`lodgenLegacyGloss()`, one function called by the object arrays pass and
inverted by the terrain bake, so the object sheets and the terrain sheet cannot
drift apart about one material.

Measured while writing it: **every Fallout 4 landscape `_s` map is BC5U**, a
two-channel format with no blue and no alpha (495 of the 824 DDS files under
`Textures/Landscape`), so "the gloss channel" is its GREEN channel and nothing
else.

**`family` in the terrain index is `"pbr"` and it means it.** It said `legacy`
and the contract called it vestigial. A legacy source is converted at bake, and
the index carries `maskRules` — the per-layer census, three rule counts that must
sum to the number of distinct landscape textures — so the word is auditable
rather than asserted. New index keys: `emissive` (`"none"` / `"present"`, in
words), `dropped` (what v1 carried and where to get it instead), `maskRules`.

**The ring-0 gate bungo asked for (09:2x) is delivered.** The contract now states
the runtime blend's seven steps once (§2.5), and
`tests/spells/lodgen_terrain_model.py ring0` implements them independently — its
own ESM walk, its own BC1/BC3/BC5U decoding, its own mip choice — and compares
against the pyramid's level-0 colour on the same texel. Two Sanctuary tiles:
**mean 3.26 and 3.59, p95 8, max 14 and 17** in sRGB 8-bit units, against a floor
that ignores the per-texel weights at **13.70 and 15.15** (4.2x) and a ceiling of
**0** over 73,984 texels. The residual is the colour sheet's own BC1 block
quantisation.

**Cover's home is bungo's open call and both arms are measured.** The ground
cover ships in the MASK sheet's alpha; `--vt-cover-in-color` puts it in the
colour sheet's instead. The two cost **exactly the same bytes** — the format is
chosen per tile by the `COVER` bit, so a cover-free tile is BC1 either way
(measured: 746,752 / 374,016 with cover, 655,456 / 327,776 without, identical
both ways) — and the stock engine tolerates a BC3 colour sheet either way, since
**2,001 of 2,001** of vanilla's own chunk colour sheets are DXT5. The only
discriminator is meaning: the colour sheet's alpha is the one slot `.lodm` §2.1
defines as OPACITY and tells a consumer to alpha-test, which would punch holes in
thin grass.

**Nothing else moved.** The stock path with `--no-vt --no-cover` is byte-identical
with the pre-lane exe over the whole Sanctuary 4x4 region (6 files), and so is the
object arrays bake (12 files). `lodgen_terrain.sh` 26/0, `lodgen_native.sh` 18/0,
`lodgen_ground_cover.sh` 29/5 (the same five reds as before the lane),
`lodgen_terrain_vt.sh` 41/1 (was 35/1; +6 new green checks, the one failure is
V9b, unchanged), `ui_align.sh` 11/0, `water_ui.sh` 82/0, `lodl_open.sh` 23/0.
New suite `lodgen_terrain_pbrm.sh` 14/0 gates the PBRM and emissive arms BOTH
ways on a fixture, because the shipped corpus contains neither: 14 of 14
landscape textures on Sanctuary resolve `legacy-inverted`.

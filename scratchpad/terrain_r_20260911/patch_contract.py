p = 'docs/LODGEN_TERRAIN_VT.md'
s = open(p, 'r', encoding='utf-8', newline='').read()


def rep(a, b, n=1):
    global s
    assert s.count(a) == n, (a[:80], s.count(a))
    s = s.replace(a, b)


# ---- title + status block -------------------------------------------------
rep("""# The terrain virtual texture — `.lodt` v1, and the ground-cover plane""",
    """# The terrain virtual texture — `.lodt` v2, and the ground-cover plane""")

rep("""**Contract version: `magic 'LDTX'`, `version 1`, `headerBytes 256`, tile-table
stride 24, payload alignment 4096.**""",
    """**Contract version: `magic 'LDTX'`, `version 2`, `headerBytes 256`, tile-table
stride 24, payload alignment 4096.**

**VERSION 2, 2026-09-11 — THE SHEETS TOOK THE OBJECT TEXTURE FAMILY.** bungo's
ruling, verbatim: *"you can mirror how it's set up for the .lodm"*, and
*"we just add the coverage for whatever's missing in terrain textures that lod
objects have in the texture department"*. Version 1's third sheet was role 3,
`data` — R sky AO, G flow wetness, B shore proximity, A ground cover, a set of
channels terrain invented for itself. Version 2 replaces it with the object
family's own (`docs/LODGEN_LODM_FORMAT.md` §2.1):

* **role 5 `mask`** — the `rmaos` slot's channels in the `rmaos` slot's order:
  **R roughness, G metallic, B AO, A ground cover**;
* **role 6 `emissive`** — RGB, BC1, written ONLY when at least one layer's
  material supplies an emissive map, absent otherwise and named as absent in the
  index;
* colour, model-space normal and height are unchanged;
* **shore proximity and wetness are DROPPED.** Shore is a runtime subtraction
  from the `.lodl` water planes (`docs/LODGEN_BTD_FORMAT.md`, "What is NOT in
  this file, and why", already says so of the landscape file); wetness is a
  close-up effect and far wetness is a weather state the runtime owns.

`family` in the index is **`"pbr"` and it means it** — it was `"legacy"` and
this page called it vestigial. A legacy material is CONVERTED at bake (gloss
inverted into roughness, metallic 0), so what ships is PBR whatever the source
was, and the index carries a per-layer census of which rule served each
landscape texture so the word can be audited rather than trusted.

**A v1 file is REFUSED, not converted** (§3.4 rule 3). No `.lodt` pyramid has
ever been written to disk outside this tree — the writer is opt-in behind
`--vt`, and bungo's installed `Data\\Terrain` holds no `.lodt` (checked
read-only, 2026-09-11) — so there is nothing in the world to convert, and a
converter would be a second definition of channels that no longer mean the same
thing.

**The `.btr` chunk sheets did not change.** `<ws>.<dim>.<x>.<y>_data.DDS` on the
stock path still carries R AO, G wetness, B shore, A cover under §1.4's stamp,
because the stock engine reads those files and their bytes are pinned by a
byte-identity gate. The pyramid still STAGES that plane to supply them (§2.4);
what changed is what the CONTAINER stores.""")

# ---- 2.2 the sheets -------------------------------------------------------
rep("""| aniso declared | 8 | `B ≥ ⌈A/2⌉` at the sampled mip: mip 0 has 8 ≥ 4, mip 1 has 4 ≥ 4. The container **declares** what its border supports and the consumer clamps its own sampler. |
| sheets | **4** | colour, model-space normal, data, **height** |""",
    """| aniso declared | 8 | `B ≥ ⌈A/2⌉` at the sampled mip: mip 0 has 8 ≥ 4, mip 1 has 4 ≥ 4. The container **declares** what its border supports and the consumer clamps its own sampler. |
| sheets | **3, 4 or 5** | colour, model-space normal, **mask**, then height when it was asked for, then **emissive** when any layer supplies one |""")

rep("""| sheet | role | format without cover | with cover | colour space |
|---|---|---|---|---|
| 0 | colour | BC1 (71) | BC1 | sRGB |
| 1 | model-space normal | BC1 (71) | BC1 | linear |
| 2 | data — R AO, G wetness, B shore, A cover | BC1 (71) | **BC3 (77)** | linear |
| 3 | height | R16_UNORM (56) | R16_UNORM | linear |

**Per-tile format selection for role 3.** `sheets[k].dxgiFormat` is the format
when that tile's `COVER` bit is clear and `dxgiFormatCover` when it is set; for
every other role the two must be equal. Without this rule a consumer sizing an
upload from a single per-file format would mis-size every cover tile.""",
    """| sheet | role | format without cover | with cover | colour space |
|---|---|---|---|---|
| 0 | 1 colour — RGB albedo, the grass tint folded in | BC1 (71) | BC1 | sRGB |
| 1 | 2 model-space normal | BC1 (71) | BC1 | linear |
| 2 | **5 mask — `rmaos`: R roughness, G metallic, B AO, A ground cover** | BC1 (71) | **BC3 (77)** | linear |
| 3 | 4 height (only with `--vt-height`) | R16_UNORM (56) | R16_UNORM | linear |
| 4 | **6 emissive — RGB, no alpha** (only when a layer supplies one) | BC1 (71) | BC1 | linear |

Role **3 (`data`) is retired** and is refused by name in a v2 container, so a
file written by something that still believed role 3 meant AO/wetness/shore/cover
is diagnosable rather than merely invalid.

**THE MASK LAW, per layer.** The three channels come from the layer's own
material, through ONE resolver shared with the object path
(`lodgenResolveMaterialMask`, `src/lodgen.h`), and every layer's answer NAMES
the rule that produced it:

| rule | when | R roughness | G metallic |
|---|---|---|---|
| `pbrm` | a `.pbrm` parses beside (or as) the TXST's `MNAM` material — the same-name discovery rule the renderer uses | its RMAOS **R**, or its `roughness` constant | its RMAOS **G**, or its `metallic` constant |
| `legacy-inverted` | a legacy material or a bare `_s` map | **`1 − smoothness × _s.G`** | **0** |
| `none-default` | nothing to read | **1.0** — fully rough, the honest unknown | 0 |

*Metallic is derived from a PBRM or not at all* (bungo, 09:4x: *"that should
only get derived from PBRM"*). A legacy layer contributes **0**, never a guess
from its specular colour.

**The gloss is the `_s` map's GREEN channel, and that is a measured fact rather
than a convention.** Every Fallout 4 landscape `_s` map is **BC5U** — a
two-channel block format, R then G, with no blue and no alpha (measured over the
unpacked corpus: `Textures/Landscape` holds 824 DDS files, of which 495 are BC5U
and every one of those is an `_s` or a normal; the diffuses are 100 DXT1, 226
DXT5 and 2 DXT3). `lodgenLegacyGloss( smoothness, specGreen )` is the one
definition of the gloss, called by the object arrays pass and inverted here, so
the object sheets and the terrain sheet cannot drift apart about one material.

**B is the same sky AO the retired data sheet carried in its R** — the eight-
direction, 2,048-unit horizon march — unchanged in value and moved one channel.

**The blend is the colour's blend, exactly.** Per texel: the quadrant's base
layer, then each painted layer by the same bilinear opacity the diffuse
composites with, in the same order, un-renormalised. **VCLR is NOT applied to
the mask** — it is the artist's hand-painted shading of the ground's COLOUR —
and neither is the grass tint.

**Per-tile format selection, and the one cover carrier.** `sheets[k].dxgiFormat`
is the format when that tile's `COVER` bit is clear and `dxgiFormatCover` when it
is set. **Exactly one sheet may declare two different formats, and it must be
the mask (role 5) or the colour sheet (role 1)** — the header says which by
declaring the pair. Two carriers, or a carrier on any other role, is a refusal
(§3.4 rule 13): a consumer sizing an upload from a single per-file format would
otherwise mis-size every cover tile.

### 2.2a Where the ground cover lives, and the number behind the choice

`.lodm` §2.1 gives the object family two alpha slots: the colour sheet's is
**coverage** (opacity) and the mask's is **subsurface**. Terrain's fourth
channel is ground cover, and it had to take one of them. **It takes the mask's**,
`--vt-cover-in-color` is the exact way back, and the reason is NOT size:

* **Bytes: the two are identical, measured.** The cover format is selected per
  TILE by the `COVER` bit, so a cover-free tile is BC1 either way and a cover
  tile is BC3 on exactly one sheet either way. Measured on cells −20..−19 ×
  24..25, `--vt-height --cover`, both arms: **746,752 bytes** at level 2 and
  **374,016** at level 4, `storedBytesTotal` **739,840** in both, two cover tiles
  in both. With `--no-cover`: **655,456** and **327,776** in both. A tile is
  **369,920 B** with cover and **323,680 B** without, whichever sheet carries it.
* **The stock `.btr` path tolerates a BC3 colour sheet, also measured**, so that
  is not the discriminator either: **2,001 of 2,001** of vanilla's own shipped
  `Textures\\Terrain\\Commonwealth\\*.DDS` chunk colour sheets are **DXT5**, and
  1,999 of 1,999 `_msn` sheets are too. DXT5 is the only format the engine has
  ever been given for that slot.
* **The discriminator is what the slot MEANS.** The colour sheet's alpha is the
  one slot the object family defines as OPACITY, and `.lodm` §2.1 tells a
  consumer to alpha-test it. A consumer written against that law would punch
  holes in the ground wherever grass is thin. The mask's alpha is subsurface,
  which nothing alpha-tests, and substituting ground cover for it is a named
  substitution the index records.

**This is bungo's call to make, and it is open** — `--vt-cover-in-color` reaches
the other arm today and costs one flag, one format pair and no second code path.""")

# ---- 2.3 the filter's special rules --------------------------------------
rep("""2. **The data sheet's alpha averages plainly**, and **a tile with no cover
   stages alpha 0, never 0xFF**. The 0xFF of §1.4 is applied only at pack time
   on the BC1 fallback path and never enters the filter — without that rule a
   parent bordering one grassy child would inherit full cover across three
   quadrants of bare rock. A parent's data sheet is BC3 iff its averaged alpha
   is not everywhere zero.""",
    """2. **The cover carrier's alpha averages plainly**, and **a tile with no cover
   stages alpha 0, never 0xFF**. The 0xFF of §1.4 is applied only at pack time
   on the BC1 fallback path and never enters the filter — without that rule a
   parent bordering one grassy child would inherit full cover across three
   quadrants of bare rock. A parent's carrier sheet is BC3 iff its averaged alpha
   is not everywhere zero.
3. **The mask's R and G average plainly too, and that is correct where AO's is
   not.** Roughness and metallic are material constants resampled, so the mean of
   four is the mean material. AO is a fixed-radius horizon march and is
   scale-dependent in exactly the way the paragraph below describes.
4. **The emissive sheet averages plainly** and is present in every tile of a
   container or in none: its presence is a header field, not a per-tile one, so a
   tile's payload size stays a function of the header and its `COVER` bit.""")

rep("""channels are scale-dependent: AO is a fixed 2,048-unit horizon march, so
`mean(AO) ≠ AO(mean)`; shore proximity is a distance field, and box-filtering a
distance field is not the distance field at half resolution — it fails worst
near the zero crossing, which is the only place it is read; and the msn's
renormalisation fixes the magnitude but the mean of fine normals is still not
the normal of the coarse heightfield. Only **cover**, **albedo** and **height**
filter cleanly.""",
    """channels are scale-dependent: AO is a fixed 2,048-unit horizon march, so
`mean(AO) ≠ AO(mean)`; and the msn's renormalisation fixes the magnitude but the
mean of fine normals is still not the normal of the coarse heightfield. Only
**cover**, **albedo**, **roughness**, **metallic**, **emissive** and **height**
filter cleanly. (Version 1 also listed shore proximity here, as a distance field
that box-filters worst near its zero crossing — the one place it is read. It is
gone, which removes that case rather than fixing it.)""")

open(p, 'w', encoding='utf-8', newline='').write(s)
b = open(p, 'rb').read()
print('CR', b.count(b'\r'), 'LF', b.count(b'\n'))

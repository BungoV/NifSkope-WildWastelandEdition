p = 'docs/LODGEN_TERRAIN_VT.md'
s = open(p, 'r', encoding='utf-8', newline='').read()


def rep(a, b, n=1):
    global s
    assert s.count(a) == n, (a[:80], s.count(a))
    s = s.replace(a, b)


# ---- 2.5 the ring-0 runtime formula, new ---------------------------------
rep("""---

## 3. `.lodt` v1 — the container""",
    """### 2.5 Ring 0, and the ONE formula the runtime must follow

bungo's ruling, 2026-09-11 09:2x: far terrain is **hybrid by band**. The band
touching the loaded 5x5 cell grid — ring 0 — is blended at RUNTIME from the
`.lodl`'s per-texel LTEX weights, so the loaded-cell edge is seamless; ring 1 and
outward sample this pyramid; the two cross-fade across ring 0. That only works if
the two agree on the same texel, so the runtime's formula is stated HERE, once,
and the generator gate below reproduces it.

**At a world point `(wx, wy)`, in this order:**

```
1  cell   = floor(wx/4096), floor(wy/4096)          cell-local (clx, cly)
2  q      = (cly >= 2048 ? 2 : 0) + (clx >= 2048 ? 1 : 0)        the quadrant
3  layer opacity a_i = BILINEAR over the quadrant's 17x17 VTXT grid
4  colour = diffuse( base )                          base = BTXT, or the
                                                     enclosing dim-4 chunk's
                                                     DOMINANT base when it is 0
5  for each ATXT layer i, IN RECORD ORDER:
       if a_i <= 0.001: skip                         (and it is SKIPPED, not
                                                      blended with a tiny weight)
       colour = colour + ( diffuse(ltex_i) - colour ) * clamp(a_i, 0, 1)
       ltex_i == 0 paints the same dominant base
6  colour *= VCLR / 255                               bilinear over the 33x33
                                                     grid; ABSENT on most cells
7  colour += ( Ttex - colour ) * (cover/255) * tintStrength      the grass tint,
                                                     AFTER the VCLR multiply
```

**Each source diffuse is sampled the same way**: `u = frac(wx/2048)`,
`v = frac(wy/2048)` (the bake's world-space tiling), at the mip
`clamp( log2( max(1, unitsPerTexel / (2048/textureWidth)) ), 0, maxMip )`,
trilinear. Get the mip wrong and the two bands differ by the texture's own
high-frequency detail, which is the visible half of a seam.

**VCLR is not the grading it looks like.** Measured on the Commonwealth:
**2,362 of 36,864 cells carry a VCLR at all**, and over the Sanctuary region
(cells −20..−17 x 24..27) every byte of every VCLR present is in **249..255** —
white to within 6/255. The grading that actually moves the colour is the layer
WEIGHTS and the grass tint, which is why the gate's floor is a blend that drops
the weights rather than one that drops VCLR.

**THE GENERATOR GATE** (`tests/spells/lodgen_terrain_model.py ring0`). An
INDEPENDENT implementation of the seven steps above — its own ESM walk, its own
BC1/BC3/BC5U decoding, its own mip choice and taps, nothing imported from the
generator — compared against the pyramid's level-0 colour at the same texel.
Reported per tile as a mean, a p95 and a max in sRGB 8-bit units, with a FLOOR
(a blend that ignores the per-texel weights) that must read far worse and a
CEILING (the bake re-decoded against itself) that must read exactly 0. Measured
2026-09-11 on two Sanctuary tiles, `--grass-tint 0`:

| tile (sw cell) | texels | mean | p95 | max | floor: weights ignored | ceiling |
|---|---|---|---|---|---|---|
| (−20, 24) | 2,704 | **3.26** | 8 | 14 | **13.70** (4.2x) | **0** over 73,984 texels |
| (−18, 24) | 2,704 | **3.59** | 8 | 17 | **15.15** (4.2x) | **0** over 73,984 texels |

The residual is the colour sheet's own BC1 block quantisation plus the two
samplers' differences, not a disagreement about the law. **The grass tint (step
7) is the one term the independent model does not carry** — it needs the grass
mesh's own average diffuse — so the gate is run against a `--grass-tint 0` bake
and the tint's size is stated separately: `lodgen_ground_cover.sh` measures a
mean tint delta of **26.97/255** over the 256 highest-cover texels of a chunk.
A runtime that folds the tint in from the cover byte and `tintStrength`, as
§1.5 states, reproduces it exactly.

---

## 3. `.lodt` v2 — the container""")

# ---- 3.1 header ----------------------------------------------------------
rep("""| 0x04 | u32 | `version` | 1 |""",
    """| 0x04 | u32 | `version` | **2** (1 is the retired four-sheet layout and is refused BY NAME) |""")

rep("""| 0x79 | u8 | `sheetCount` | 4 |""",
    """| 0x79 | u8 | `sheetCount` | 3, 4 or 5 — colour, msn, mask, then height when asked for, then emissive when a layer supplies one |""")

rep("""| 0xA0 | ×4 | `sheets[4]` | 8 bytes each: u16 `dxgiFormat`, u16 `dxgiFormatCover`, u8 `role` (0 unused, 1 colour, 2 msn, 3 data, 4 height), u8 `colorSpace` (0 linear, 1 sRGB), u8[2] zero. Sheets beyond `sheetCount` are all zero. |
| 0xC0 | u8[64] | `reserved` | must be zero |""",
    """| 0xA0 | ×6 | `sheets[6]` | 8 bytes each: u16 `dxgiFormat`, u16 `dxgiFormatCover`, u8 `role` (0 unused, 1 colour, 2 msn, **3 RETIRED `data`**, 4 height, **5 mask**, **6 emissive**), u8 `colorSpace` (0 linear, 1 sRGB), u8[2] zero. Sheets beyond `sheetCount` are all zero. **Six since version 2**: v1 held four here and its reserved tail began at 0xC0; the two extra slots came out of that tail rather than out of a second header. |
| 0xD0 | u8[48] | `reserved` | must be zero |""")

rep("""`0xC0..0xFF` is reserved-must-be-zero, so the fields this format will
predictably want next do not each cost a v2 and a whole re-bake. A v1 reader
ignores a zero-filled tail.""",
    """`0xD0..0xFF` is reserved-must-be-zero, so the fields this format will
predictably want next do not each cost a version and a whole re-bake. A reader
ignores a zero-filled tail. (It was `0xC0..0xFF` in v1; the mask and emissive
descriptors took the first sixteen bytes of it, which is exactly the use the
tail was reserved for.)""")

# ---- 3.3 payload order ---------------------------------------------------
rep("""Raw payload = the concatenation, in exactly this order, of

```
sheet 0 mip 0, sheet 0 mip 1, sheet 1 mip 0, sheet 1 mip 1,
sheet 2 mip 0, sheet 2 mip 1, sheet 3 mip 0, sheet 3 mip 1
```""",
    """Raw payload = the concatenation of every sheet's every mip, **sheet-major and
mip-minor, in the header's own `sheets[]` order** — so a reader walks the header
rather than a table in this page:

```
sheet 0 mip 0, sheet 0 mip 1, sheet 1 mip 0, sheet 1 mip 1, ...
```

With every option on that is colour, msn, mask, height, emissive.""")

rep("""Computed raw size, at content 256 / border 8 / 2 mips:

| | colour | msn | data | height | tile |
|---|---|---|---|---|---|
| no cover | 46,240 | 46,240 | 46,240 | 184,960 | **323,680** |
| cover | 46,240 | 46,240 | 92,480 | 184,960 | **369,920** |""",
    """Computed raw size, at content 256 / border 8 / 2 mips. A BC1 sheet is
**46,240** bytes (68x68 blocks at mip 0 plus 34x34 at mip 1, 8 bytes a block), a
BC3 sheet **92,480**, the R16 height sheet **184,960**:

| | colour | msn | mask | height | emissive | tile |
|---|---|---|---|---|---|---|
| no cover | 46,240 | 46,240 | 46,240 | 184,960 | — | **323,680** |
| cover | 46,240 | 46,240 | 92,480 | 184,960 | — | **369,920** |
| cover, `--vt-cover-in-color` | 92,480 | 46,240 | 46,240 | 184,960 | — | **369,920** |
| cover + emissive | 46,240 | 46,240 | 92,480 | 184,960 | 46,240 | **416,160** |

**The mask sheet costs exactly what the retired data sheet cost**, and the two
homes for the cover byte cost exactly the same as each other — measured, §2.2a.
The emissive sheet is the only thing that adds bytes, and only where a layer
supplies one.""")

open(p, 'w', encoding='utf-8', newline='').write(s)
b = open(p, 'rb').read()
print('CR', b.count(b'\r'), 'LF', b.count(b'\n'))

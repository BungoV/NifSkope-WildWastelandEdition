# LODGEN — ESM layouts reference (rung 1/2 inputs)

Extracted from wbDefinitionsFO4.pas 2026-08-31 (agent dig, spot-verified
against Fallout4.esm and the shipped .btr meshes). Layout authority for
src/esmdata.cpp. VERIFIED = held against real data here; DERIVED = read
from the defs; FLAGGED = ambiguous, do not trust without a check.

## LAND

- VHGT (1096 B): float base + 33×33 byte deltas + 3 pad. **Deltas are
  SIGNED (VERIFIED)**: xEdit types them itU8 but the decode only matches
  reality signed — held against the game's own baked mesh: cell (-18,24)
  SW corner decodes 7264, vanilla Commonwealth.4.-20.24.BTR's nearest Land
  vertex sits at 7272 with 32 units of lateral decimation offset.
  Column 0 of each row offsets the PREVIOUS ROW's column 0; ×8 game units.
- VNML / VCLR (3267 B each): 33×33×3 u8, row-major (normals / colours).
- Texture layers (the splat, for vertex classes + rung-3 texture bake):
  - BTXT (8 B): LTEX formID u32 | quadrant u8 (0 BL, 1 BR, 2 TL, 3 BR->TR)
    | pad | layer s16 — the QUADRANT BASE texture.
  - ATXT (8 B, same layout) heads an alpha layer; its VTXT sibling holds
    8-byte entries: position u16 (row=pos/17, col=pos%17 — a **17×17
    per-quadrant grid**, 0..288) | 2 pad | opacity float.
  - VTEX: legacy formID list; MPCD: unparsed (FLAGGED).
- DATA: size/content undefined in the defs (FLAGGED, skip).

## CELL

- XCLC (12 B): s32 X, s32 Y, u32 "Force Hide Land" quadrant flags
  (bit0..3 = quads — respect when generating terrain!).
- XCLW: water height float; sentinel 0x7F7FFFFF-family raw $FF7FFFFF =
  no local water (fall back to WRLD defaults).
- XCWT: WATR formID. DATA: **u16** flags — bit0 interior, bit1 Has Water,
  bit3 No LOD Water, bit12 "Distant LOD only".
- Also useful later: XILW (exterior LOD offset), XCRI (precombined refs).

## WRLD

- DNAM (8 B): default land height, **default water height** (the LOD-water
  fallback). NAM3/NAM4: LOD water type (WATR) + LOD water HEIGHT.
- MNAM (16 B): usable dims s32×2 (likely map pixels, FLAGGED) + **NW cell
  s16×2 + SE cell s16×2 = the usable cell extent for generation**.
- NAM0/NAM9: object bounds min/max floats (÷4096 display scale; CK never
  shows it — FLAGGED stale). DATA u8: bit0 Small World, bit3 No LOD Water,
  bit4 No Landscape, bit6 Fixed Dimensions (activates WCTR).

## TREE CNAM (48 B)

trunk flex f32, branch flex f32, 8 UNKNOWN f32 (genuinely undecoded),
leaf amplitude f32, leaf frequency f32.

## SCOL (static collections — MUST be expanded by the generator)

Per Part: ONAM base formID + DATA = N × 28-B placements
(pos f32×3, rot f32×3 radians, scale f32). **FLAGGED: whether placements
compose under the placing REFR's transform or are absolute is not stated —
verify against a vanilla .bto before shipping SCOL expansion.** SCOL
records carry their own Has Distant LOD flag (0x8000), like STAT.

## REFR

- DATA (24 B): pos f32×3 + rot f32×3 radians. XSCL: f32. XESP (8 B):
  parent formID + flags u8 (bit0 opposite-of-parent, bit1 pop-in).
- XLOD: 3 floats, "not seen in FO4 vanilla" (FLAGGED, ignore).
- **Header flags are BASE-TYPE DEPENDENT above bit 3** — the same bit
  means different things depending on what NAME points at. For
  ACTI/STAT/SCOL/TREE bases (the LOD-relevant set): bit8 LOD Respects
  Enable State, bit9 Hidden From Local Map, **bit15 Visible When
  Distant**, bit16 Is Full LOD. But bit9 = Motion Blur on MSTT, bit8 =
  Inaccessible on DOOR. Resolve the base's record type FIRST (esmdata
  already does). Consistent everywhere: bit10 persistent, bit11
  initially disabled; bit5 deleted (explicit only on STAT in the defs,
  conventional for the header — treated as such).

## Vanilla .btr Land vertex format (measured, the rung-1 output target)

Vertex Desc flags 0x3 (VERTEX|UV), 12-byte stride: half3 position +
bitangentX half + half2 UV — **no normals in the vertex data**; the
per-chunk _msn model-space-normal texture carries them. Commonwealth
dim4 Land ≈ 1,068 verts / 2,066 tris (decimated from the 33×33 grids).

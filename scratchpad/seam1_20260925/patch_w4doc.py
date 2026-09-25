"""W4 docs: .lodo v5 (optional per-vertex colour stream) in docs/LODGEN_NATIVE_LODO_LODI.md. Anchors asserted once."""
p = 'E:/Projects/NifskopeWWE-seam1/docs/LODGEN_NATIVE_LODO_LODI.md'
s = open(p, newline='', encoding='utf-8').read()
cr = s.count('\r')
pairs = [
("# `.lodo` v4 + `.lodi` v7 (v3..v9) — the FO4CS-native far field",
 "# `.lodo` v5 (v4..v5) + `.lodi` v7 (v3..v9) — the FO4CS-native far field"),
("""`src/lodifile.h`, `src/nifcli.cpp`).** The library is `.lodo` **version 4**
> and only 4 (§3.7).""",
"""`src/lodifile.h`, `src/nifcli.cpp`; `.lodo` re-read 2026-09-25, lane SEAM1).**
> The library is `.lodo` **version 5**, and a version-4 file is read as a v5 file
> with no colour stream (§3.7)."""),
("""| **0xD0** | **u32** | **`cardCount` (v4)""", """| **0xD0** | **u32** | **`cardCount` (v4)"""),
("""| 0xD4…0xFF | — | reserved, zero |

The ladder table""",
"""| **0xD4** | **u32** | **`colourVertexCount` (v5) — rows in the colour blob; 0 = the file carries no colour. Never more than `vertexCount`, and zero exactly when `offColours` is zero** |
| **0xD8** | **u64** | **offset: colour blob (v5), 4 B per row, written LAST (after the strings), 4096-aligned, and in `indexCrc32` only when present** |
| 0xE0…0xFF | — | reserved, zero (on a version-4 file 0xD4…0xFF) |

The ladder table"""),
("""level-0 surface has no boundary edge; bits 3–15 reserved)""",
"""level-0 surface has no boundary edge; **bit3 VERTEX_COLOUR (v5)**: the mesh has
rows in the colour blob, §3.7; **bit4 VERTEX_ALPHA (v5)**: its A is opacity, only
with bit3; bits 5–15 reserved)"""),
]
for o, n in pairs:
    assert s.count(o) == 1, o[:70]
    s = s.replace(o, n)
i = s.index('### 3.7 The library is version 4, and there is no version 5')
j = s.index('## 4. `.lodi` header')
s = s[:i] + '''### 3.7 Version 5: the optional per-vertex colour stream (lane SEAM1, W4, 2026-09-25)

**What it is for.** A handful of vanilla LOD models tint their own vertices: the
Amphitheater's shell, the blasted maples, the warehouse roofs, the brick shells.
The game draws that tint because the shape has BOTH a colour channel in its
vertex descriptor (attribute bit 0x20) AND the `Vertex_Colors` shader flag
(SLSF2 bit 5). v4 had no room for it, so those models went grey in the native
far field. bungo's ruling (2026-09-25): *match the game exactly* -- carry the
colour only where both are set, apply it only there, and keep RGB and A as their
own channels, the way the game uses them.

**The law.**

* A shape contributes colour iff it has the channel AND `Vertex_Colors`. A shape
  with only one of the two contributes nothing, exactly as the game draws it.
* A mesh with at least one such shape is flagged `VERTEX_COLOUR` (mesh bit 3) and
  gets **one RGBA8 row per vertex over its whole contiguous vertex range**; its
  other shapes' vertices carry opaque white, which multiplies to no change.
* `VERTEX_ALPHA` (mesh bit 4) is set when such a shape also has SLSF1
  `Vertex_Alpha` (bit 3). A is stored as the source stores it either way; only
  this bit makes it opacity. Measured on the Boston census, 28 streamed shapes:
  all 28 carry `Vertex_Colors`, none `Vertex_Alpha` and none `Tree_Anim`, and 4
  have A below 255 (so A is kept, and ignored, on them).
* The blob: the flagged meshes in mesh order, each its rows `vertexBase..end` in
  vertex order, R G B A. It sits after the strings, 4096-aligned, and joins
  `indexCrc32` only when present.

**What stays the same.** A file with no colour differs from a v4 file in the
version word at 0x04 **and nowhere else**: 0xD4…0xDF are zero, no mesh bit 3/4,
no blob. The version word is outside `headerCrc32` (0x10…0xFF), so the CRC and
the `.lodi`'s `lodoIdentity` are unchanged. A version-4 file is read as a
version-5 file with no colour.

**The viewer.** `src/lodinative.cpp` multiplies the colour into the vertex colour
of every view (the channel views keep their own value, times the colour), sets
SLSF2 `Vertex_Colors` on a bucket holding a flagged mesh, and SLSF1
`Vertex_Alpha` only on a `VERTEX_ALPHA` mesh; elsewhere A is drawn as 1.

**The gate** (pre-registered before the build,
`scratchpad/seam1_20260925/w4_gate.py`, inputs from `w4_bakes.sh`; the
instruments' own self-test is `w4_synth.py`): **G1** the synthetic fixture is
byte-identical but 0x04, and each region's file with its colour stripped is too;
**G2** the Amphitheater and both blasted maples are `VERTEX_COLOUR`, the
Amphitheater and maple 01 carry non-white RGB, the flags agree with the source
NIFs both ways, and each flagged mesh's decoded rows are its source's rows;
**G3** the same gate on the pre-v5 exe's bakes is RED.

**The FO4CS reader is owed.** The in-game reader of the native pair reads v4;
it needs the v5 header words and the colour blob before a v5 library can ship
to it. That is the standing order (FO4CS readers come last), not news.

**History.** An earlier version 5 -- the subdivided library for the per-vertex
horizon stream (§4.11) -- was written by lane HORIZON3 on 2026-09-19 and removed
whole by lane HORIZONOUT the same day. No exe ever wrote it, so this version
number was free.


---

''' + s[j:]
old = ("versions **1, 2 and 3 refused by name**, anything but 4;")
new = ("versions **1, 2 and 3 refused by name**, anything but 4 or 5;")
assert s.count(old) == 1
s = s.replace(old, new)
old = "mesh flags beyond ALPHA / SWAY / WATERTIGHT; reserved header bytes 0xCE…0xCF and 0xD4…0xFF"
new = ("mesh flags beyond ALPHA / SWAY / WATERTIGHT (plus VERTEX_COLOUR / VERTEX_ALPHA on v5); VERTEX_ALPHA "
       "without VERTEX_COLOUR; `colourVertexCount` and `offColours` not both zero or both set, a count over "
       "`vertexCount`, a flagged mesh whose vertices are not one contiguous range, or flagged rows that do not "
       "add up to the count (v5); reserved header bytes 0xCE…0xCF and 0xD4…0xFF (0xE0…0xFF on v5)")
assert s.count(old) == 1
s = s.replace(old, new)
assert s.count('\r') == cr
open(p, 'w', newline='', encoding='utf-8').write(s)
print('ok')

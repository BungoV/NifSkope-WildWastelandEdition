
---

## 7. LENS 1 — CORPUS (done personally; the lens agent was killed before it reported)

### 7.1 Where Sanctuary actually is — measured, not inherited from the brief

The brief asserted that `Commonwealth.4.-20.24` is "near Sanctuary". I checked
rather than assumed. `python findcell.py` scans every Commonwealth exterior CELL
for an EDID and prints its `XCLC` grid coordinates:

    Commonwealth exterior CELLs: 36865 (with an EDID: 755)
       Vault111Ext             cell ( -22,  22)
       SanctuaryExt10          cell ( -21,  20)
       SanctuaryExt03          cell ( -21,  21)
       SanctuaryExt07          cell ( -21,  22)
       SanctuaryExt04          cell ( -20,  20)
       SanctuaryExt            cell ( -20,  21)
       SanctuaryExt02          cell ( -20,  22)
       SanctuaryExt06          cell ( -19,  21)
       SanctuaryExt05          cell ( -19,  22)
       RedRocketExt            cell ( -17,  19)
       ConcordExt              cell ( -15,  17)
       ConcordMuseumExt        cell ( -14,  17)

**Sanctuary Hills is cells (-21..-18, 20..22).** (36865 cells against 36864 LAND
records: the extra one is the worldspace's persistent cell, which carries no
`XCLC` and no LAND — a clean account of the difference, not a parser gap.)

Against the textured box measured in section 2.1 (x -36..32, y -41..32), the
distance from Sanctuary to the edge of the region that has *any* landscape
texture is:

| direction from Sanctuary (-20, 21) | edge | cells away | approx. distance |
|---|---|---|---|
| **north (+Y)** | y = 32 | **11** | ~45 000 units |
| west  (-X) | x = -36 | 16 | ~66 000 units |
| east  (+X) | x = 32 | 52 | ~213 000 units |
| south (-Y) | y = -41 | 62 | ~254 000 units |

**North is the nearest edge by a wide margin.** Standing in Sanctuary and
looking north, the untextured region starts about 11 cells out and then runs a
further 63 cells to the map edge at y = 95. That is exactly the band of distant
mountains in bungo's screenshots, and it is measured, not assumed.

(Aside: Concord is at y 16-18, *south* of Sanctuary at y 20-22, so "looking
north over the Concord water tower" cannot be literally north-over-Concord.
Whichever of the two the shot is, the conclusion is unchanged — every direction
from Sanctuary reaches untextured terrain, and north reaches it soonest.)

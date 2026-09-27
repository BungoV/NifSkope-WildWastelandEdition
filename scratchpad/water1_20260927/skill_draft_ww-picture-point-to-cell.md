---
name: ww-picture-point-to-cell
description: Name the world cell / region edge a spot in a NifSkope WW headless render shows, from the render's own .cam.log (rot, lookat, upp, vp), before writing "the south-east corner" in a report. Use when a picture artefact (a sheet past the terrain, a poke-through, a seam) has to be tied to cells in the .lodl so it can be measured offline.
---
<!-- DRAFT: the session refused a write under .claude/skills/; move this file to
     .claude/skills/ww-picture-point-to-cell/SKILL.md -->

# A picture spot -> the cell it shows

Written by lane WATER1 (2026-09-27). Its 12:19 report called a stray v2 water sheet "at the south-east
corner"; projecting the region corners with the render's own camera showed it was the WEST region edge
next to the NORTH-WEST corner (cells -6,-5..-3). The name was a guess from the picture's orientation.

## The camera line
Every `shot.sh` render writes `<name>.cam.log`, e.g.
`grab arm=center/ortho/view view=8 rot=-63.5593,0.0000,133.3081 lookat=-4096,-24576,0 ... persp=0 ... vp=1600x1624 upp=20.48`

## The projection (orthographic views, persp=0)
Measured convention (it put the region's four corners where the picture has them, within the height
offset of the terrain):
```python
import numpy as np
def R(ax, deg):
    a = np.radians(deg); c, s = np.cos(a), np.sin(a)
    return {'x': np.array([[1,0,0],[0,c,-s],[0,s,c]]), 'z': np.array([[c,-s,0],[s,c,0],[0,0,1]])}[ax]
M = R('x', rotX) @ R('z', rotZ)                     # rot=rotX,0,rotZ from the cam line, degrees, signs as written
p = M @ (np.array([wx, wy, wz]) - lookat)
px, py = vpW / 2 + p[0] / upp, vpH / 2 - p[1] / upp
```
Higher z moves a point UP the picture. So a flat sheet lying UNDER high ground shows below the ground's
edge through a region's open side (the view draws no side walls). Keep that in mind before calling a
sheet "past the terrain": its XY may be inside the region.

## Check before trusting it
Project the region's four corners (`cells [x0,y0]..[x1,y1]` from the render log, x0*4096 .. (x1+1)*4096)
at a typical ground height and compare with the picture's corners. Of the eight sign/order combinations,
only `Rx(rotX) @ Rz(rotZ)` with the signs as logged matched WATER1's Boston render (top = SE,
right = SW, bottom = NW, left = NE for rotZ 133.3). The fit was checked on the edges' slopes (0.42-0.47
predicted vs 0.41-0.44 in the picture) and the top corner (696,223 predicted vs ~697,255 drawn, the gap being
terrain height). If a new camera does not match, re-run all eight (order xz/zx, sign of each angle) and use
the one that fits; write down which.

## Then measure offline
Cells named -> read them with `tests/spells/lodl_open_authority.py <file> cell CX CY`, or WATER1's
`scratchpad/water1_20260927/openitems.py` (v2 sheets, poke-through, sea body), whose controls must first
reproduce the render log's own counts.

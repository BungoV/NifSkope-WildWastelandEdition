"""BAKE2: document the vanilla dim-4 grid phase (patch_grid.py) in LODGEN_TERRAIN_VT.md s2.6. Anchors once, CR kept."""
R = 'E:/Projects/NifskopeWWE-bake2/docs/LODGEN_TERRAIN_VT.md'
b = open(R, 'rb').read(); cr = b.count(b'\r'); s = b.decode('utf-8')
pairs = [
("""  before the change: 100 of 180 near cells over vanilla + offset + 8).
""",
"""  before the change: 100 of 180 near cells over vanilla + offset + 8).
* **Vanilla's grid is the worldspace's own** (lane BAKE2, 2026-09-25). A dim-4 sheet
  `<WS>.4.<x>.<y>.dds` has its SW cell on the grid of `LODSettings/<WS>.LOD` (int16
  left, int16 bottom, int32 stride, int32 lodMin, int32 lodMax), not on multiples of 4.
  The Commonwealth and pre-war say -96,-96 and Nuka-World -32,-32 (phase 0,0: the old
  addressing, byte for byte); Far Harbor says -73,-59, so its sheets sit at x = 3,
  y = 1 mod 4 and a multiple-of-4 lookup found none of them. The phase is
  ((left mod 4), (bottom mod 4)), read from the vanilla root; no file = 0,0, and the
  census says which (`grid=3,1(LODSettings -73,-59)` or `grid=0,0(default)`).
  Measured on Far Harbor: 178 sheets read, 0 chunks missing, flat-grey VT.4 cells
  1517 -> 0 of 3584.
"""),
("""  noLandRingCells= texelsNoLand= root=`.""",
"""  noLandRingCells= texelsNoLand= grid=X,Y(source) root=`."""),
]
for o, n in pairs:
    assert s.count(o) == 1, o[:60]; s = s.replace(o, n)
out = s.encode('utf-8'); assert out.count(b'\r') == cr; open(R, 'wb').write(out); print('ok 2 edits')

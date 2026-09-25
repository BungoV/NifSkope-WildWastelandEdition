import sys
p = 'docs/LODGEN_TERRAIN_VT.md'
b = open(p, 'rb').read(); crlf = b.count(b'\r\n'); nl = '\r\n' if crlf else '\n'
t = b.decode('utf-8')
def rep(old, new):
    global t
    old = old.replace('\n', nl); new = new.replace('\n', nl)
    assert t.count(old) == 1, (t.count(old), old[:60]); t = t.replace(old, new)
rep("| `layer.ltex == 0` (NULL) | `D(dominantBase)`, `S(dominantBase)` — the same thing the diffuse paints |",
    "| `layer.ltex == 0` (NULL) | the engine default (`ESM_LTEX_ENGINE_DEFAULT`): `D = 0`, `S = 0` — the same thing the diffuse paints |")
rep("| `land.baseTex[q] == 0` | `dominantBase`, as the diffuse already does |",
    "| `land.baseTex[q] == 0` | the engine default, as the diffuse already does |")
rep("""`dominantBase` is scope-dependent — it is what NULL-LTEX layers and
`baseTex == 0` texels paint — so the tile baker computes it over the **enclosing
dim-4 chunk's** cell set, not over the tile's own two cells, and carries it into
the four tiles that compose that chunk.""",
"""`dominantBase` — what NULL-LTEX layers and `baseTex == 0` texels paint — is
**no longer scope-dependent** (lane SEAM1, 2026-09-25). It used to be the most
common base of the enclosing dim-4 chunk, and a chunk whose dominant base
differed from its neighbours' painted a hard-edged block on the chunk grid
(Sanctuary, cells -20..-16 x 20..24: steps 12.9 / 11.3 / 11.9 / 8.8 luminance
at its four borders against interior tile borders of 1.5 and less). It is now
the one world-wide texture the engine itself paints there,
`ESM_LTEX_ENGINE_DEFAULT` = `Landscape\Ground\CommonwealthDefault01_{d,n,s}.dds`
(the game's `sDefaultLandDiffuseTexture:Landscape` family, read from the exe's
string table; see §2.5 step 4). The chunk and tile bakers use the same constant,
so V9a's byte identity holds by construction.""")
rep("""4  colour = diffuse( base )                          base = BTXT, or the
                                                     enclosing dim-4 chunk's
                                                     DOMINANT base when it is 0""",
"""4  colour = diffuse( base )                          base = BTXT, or the
                                                     ENGINE DEFAULT land texture
                                                     when it is 0 (lane SEAM1)""")
rep("       ltex_i == 0 paints the same dominant base",
    "       ltex_i == 0 paints the same engine default")
rep("""the census identity above holds. The gate is `tests/spells/lodgen_vtfix.sh` G1,""",
"""the census identity above holds. Since lane SEAM1 (2026-09-25) a null layer
paints the engine default, which resolves through its `_s` like any legacy
layer, so form 0 no longer reaches the mask cache: expect `noneDefault` one
lower and `legacyInverted` one higher on a whole-map bake that contains a null
layer or a BTXT-less quadrant, and the identity unchanged. The gate is `tests/spells/lodgen_vtfix.sh` G1,""")
out = t.encode('utf-8'); assert out.count(b'\r\n') == crlf + (t.count(nl) - b.decode('utf-8').count(nl) if crlf else 0)
open(p + '.new', 'wb').write(out)

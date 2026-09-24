"""Three small amendments left over from lane ROADS4's doc pass.

  1. section 5 gains a `--road-ground-paint` row, next to the other road rows;
  2. 1a.5d's item 7 said `--road-detail` "stays 0" -- true when lane ROADS3
     wrote it, overridden by bungo on 2026-09-12. The finding is kept and the
     override is noted beside it; history is not rewritten;
  3. `--roads-legacy`'s meaning gains the two clauses that are new.
"""
import io
import sys

P = 'docs/LODGEN_TERRAIN_VT.md'
s = io.open(P, encoding='utf-8').read()

A1 = ("| `--road-cover-suppress F` | 1.0 | how much of the ground cover a road "
      "removes under itself, 1 = all of it, 0 = leave the cover plane alone |\n")
N1 = A1 + (
    "| `--road-detail F` | **1.0** | §1a.5, how much of the road texture's own "
    "detail survives: 1 is the sampled texel, 0 flattens each material to its "
    "average. **Defaulted to 0 until 2026-09-12**; bungo ruled for 1 on a "
    "picture. `--road-detail 0` reproduces every earlier bake byte for byte |\n"
    "| `--road-ground-paint F` | **1.0** | §1a.5e, the COVERAGE multiplier for a "
    "shape inside a road model whose material lives under "
    "`materials/Landscape/Ground/` -- the verge, modelled as terrain. Such "
    "shapes win 36.1 % of the road plane on chunk (-20,20). **No value is "
    "recommended**: lane ROADS4 baked 1 / 0.75 / 0.5 / 0.25 / 0 and the seam "
    "gets monotonically WORSE (15.387 -> 33.352), so the default is 1.0 = the "
    "previous bytes. Being on coverage, 0 also stops such a shape suppressing "
    "ground cover |\n")
if s.count(A1) != 1:
    sys.exit('anchor 1: %d' % s.count(A1))
s = s.replace(A1, N1)

A2 = "**7. `--road-detail` was re-tested and stays 0.** The residual after the best\n"
N2 = ("**7. `--road-detail` was re-tested and stayed 0 -- and was then overruled by\n"
      "bungo on 2026-09-12, who looked at both and said *\"--road-detail 1 is always\n"
      "on, do not ever use road detail 0, that looks terrible\"*. The measurement\n"
      "below still holds and is why the flag exists; it is not what decides the\n"
      "default any more (1a.5, 1a.5e).** The residual after the best\n")
if s.count(A2) != 1:
    sys.exit('anchor 2: %d' % s.count(A2))
s = s.replace(A2, N2)

A3 = ("ROADS2, in one token: it means `--road-composite max-z`, `--road-detail 1`,\n"
      "`--road-raised` and `--road-sidewalks` together, and from lane ROADS3 also\n"
      "`--road-opacity 1` -- a no-op while 1 is the default, written down so the way\n"
      "back stays the way back if the default is ever moved.")
N3 = ("ROADS2, in one token: it means `--road-composite max-z`, `--road-detail 1`,\n"
      "`--road-raised` and `--road-sidewalks` together, from lane ROADS3 also\n"
      "`--road-opacity 1`, and from lane ROADS4 also `--road-ground-paint 1` -- all\n"
      "no-ops while those are the defaults, written down so the way back stays the\n"
      "way back if a default is ever moved. `--road-detail 1` stopped being a no-op\n"
      "on 2026-09-12 in the other direction: it is now the default, so the legacy\n"
      "token and the default agree on it.")
if s.count(A3) != 1:
    sys.exit('anchor 3: %d' % s.count(A3))
s = s.replace(A3, N3)

io.open(P, 'w', encoding='utf-8', newline='\n').write(s)
b = io.open(P, 'rb').read()
print('ok  CR %d  LF %d  bytes %d' % (b.count(b'\r'), b.count(b'\n'), len(b)))

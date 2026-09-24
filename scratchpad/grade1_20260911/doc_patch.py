"""GRADE1 -- amend docs/LODGEN_TERRAIN_VT.md.

Four edits, each against an anchor that must occur EXACTLY ONCE:
  A  the ring-0 runtime formula gains step 8, the grade, so FO4CS blends the
     same tone as the pyramid.
  B  a new 2.5f, read from contract_25f.txt, before the rule that closes 2.5.
  C  the §5 flag table gains --grade.
  D  a GRADE1 provenance block at the end, with hashes read off disk now.

The file is LF-only and must stay LF-only: asserted before and after.
"""
import hashlib
import os

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
DOC = os.path.join(REPO, 'docs', 'LODGEN_TERRAIN_VT.md')

d = open(DOC, 'rb').read()
assert d.count(b'\r') == 0, 'doc is not LF-only to begin with'
before_cr = d.count(b'\r')
t = d.decode('utf-8')


def rep(anchor, new):
    global t
    n = t.count(anchor)
    assert n == 1, 'anchor occurs %d times, not 1: %r' % (n, anchor[:70])
    t = t.replace(anchor, new)


# ---------------------------------------------------------------- A  step 8
A_OLD = """7  colour += ( Ttex - colour ) * (cover/255) * tintStrength      the grass tint,
                                                     AFTER the VCLR multiply
```"""
A_NEW = """7  colour += ( Ttex - colour ) * (cover/255) * tintStrength      the grass tint,
                                                     AFTER the VCLR multiply
8  colour *= grade                                    the colour GRADE, 2.5f.
                                                     DEFAULT 1.0, in which case
                                                     this step does not exist:
                                                     the multiply is branched
                                                     over, so ring 0 and the
                                                     pyramid agree bit for bit
                                                     at the default. A ring-0
                                                     runtime that ships a grade
                                                     other than 1 must read the
                                                     SAME k the sheets were
                                                     baked with -- the bake
                                                     census prints it as
                                                     `landGrade`
```"""
rep(A_OLD, A_NEW)

# ---------------------------------------------------------------- B  2.5f
B_OLD = """What the hex tiling substitutes for the swirls is a soft blotchiness at its own
cell scale, which no instrument in this lane gates and which
`scratchpad/tiling4_20260912/images/sheet_tiling4.png` shows at 1:1.

---
"""
sec = open(os.path.join(HERE, 'contract_25f.txt'), encoding='utf-8').read()
assert sec.startswith('### 2.5f')
sec = sec.replace('\r\n', '\n').rstrip('\n')
B_NEW = (B_OLD.replace('\n---\n', '\n') + '\n' + sec + '\n\n---\n')
rep(B_OLD, B_NEW)

# ---------------------------------------------------------------- C  the flag
C_OLD = ("| `--land-hex UNITS` | 0 | \u00a72.5e, the hex cell size in world "
         "units. 0 = off, and off is the pre-TILING4 bake byte for byte, so "
         "`--land-hex 0 --land-mip-bias 0` is the exact way back from "
         "`--land-sample stochastic` |\n")
C_NEW = C_OLD + (
    "| `--grade K` | **1.0** | \u00a72.5f, the colour grade: every baked colour "
    "texel times K, in both writers, after the road and the tint and before the "
    "crevice term. At 1.0 the multiply is BRANCHED OVER, so no flag and "
    "`--grade 1.0` are the previous bake's bytes (24 of 24 files, two tiles). "
    "Clamped to 0..4. **No value is recommended**: lane GRADE1 measured 25 "
    "tiles and the per-tile optimum runs 0.615..1.241, straddling 1. The "
    "pooled optimum is 0.8403 (-20.5 % pooled RGB RMS, worse on 6 of 25) |\n")
rep(C_OLD, C_NEW)

# ---------------------------------------------------------------- D  provenance
rows = []
for f in ['src/lodgen.cpp', 'src/lodgen.h', 'src/nifcli.cpp']:
    b = open(os.path.join(REPO, f), 'rb').read()
    rows.append('| `%s` | `%s` | %s | %d |'
                % (f, hashlib.sha256(b).hexdigest()[:16],
                   format(len(b), ','), b.count(b'\n')))

D_OLD = ("| \u00a75 `--land-hex` on its own | `nifcli.cpp:6116` | `else if "
         "( t == QLatin1String( \"--land-hex\" ) ) lodgenSetLandHexSize( "
         "next().toFloat() );` |")
D_NEW = D_OLD + """

### GRADE1, 2026-09-12

Section 2.5f is new, step 8 of the \u00a72.5 ring-0 formula is new, and `--grade`
is a new row of \u00a75. Vanilla's numbers are Bethesda's shipped dim-4 sheets read
as LOOSE FILES under `E:/Tools/Fallout 4/DataUnpacked/Data`; ours are bakes into
`scratchpad/grade1_20260911/out/` made by `release/NifSkope.exe`
(2026-09-12 03:06:21, 21,489,152 bytes, md5 `6af74b4b4667ce50c4506a2d42a04fdf`)
and, for the "the flag is new" gate only, by the rung
`release/NifSkope.before_grade1.exe` (2026-09-12 02:08:57, 21,487,616 bytes).
The instruments are `scratchpad/grade1_20260911/gradelib.py` with
`g0_controls.py` (the known-answer controls: 24 checks, 0 failures, a synthetic
gamma and gain recovered to 3 decimals before any verdict was formed),
`g1_curve.py` (the three models), `g2_position.py` (multi-scale and the residual
correlations, every r against its own phase-twin floor), `g3_cells.py` (96 land
cells), `g4_census.py` (25 tiles), `g5_decide.py` (the region split, the
parabola, the pooled optimum) and `g6_gates.py` (the five ship gates, measured
through the binary: 8 checks, 0 failures), with the logs beside them. The road
mask is the difference between the default bake and a `--no-roads` bake, because
roads are ON by default and `--roads` is a no-op. No number here is copied
forward; each was re-read off the artefact it describes.

| file | sha256 (16) | bytes | lines |
|---|---|---|---|
%s

| claim | line | anchor |
|---|---|---|
| \u00a72.5f the grade's state, default 1.0 | `lodgen.cpp:6260` | `static float g_landGrade = 1.0f;` |
| \u00a72.5f the setter, clamped 0..4 | `lodgen.cpp:6305` | `void lodgenSetLandGrade( float k )` |
| \u00a72.5f/\u00a72.5 step 8 the multiply, branched over at 1.0 -- the anchor occurs exactly TWICE and that is the claim: the stock chunk writer and the pyramid writer both grade | `lodgen.cpp:7962` (chunk) and `lodgen.cpp:9258` (pyramid) | `if ( g_landGrade != 1.0f )` |
| \u00a72.5f the census prints the value, so a sheet cannot be read against the wrong k | `lodgen.cpp:8035` (JSON) and `lodgen.cpp:10110` (text) | `lodgenLandGrade()` |
| \u00a72.5f the declarations | `lodgen.h:270` | `void lodgenSetLandGrade( float k );` |
| \u00a75 `--grade` | `nifcli.cpp:6166` | `else if ( t == QLatin1String( "--grade" ) ) lodgenSetLandGrade( next().toFloat() );` |""" % '\n'.join(rows)
rep(D_OLD, D_NEW)

out = t.encode('utf-8')
assert out.count(b'\r') == before_cr == 0, 'CRs appeared'
assert len(out) > len(d)
open(DOC, 'wb').write(out)
print('wrote %s: %d -> %d bytes, %d -> %d lines, CR %d'
      % (DOC, len(d), len(out), d.count(b'\n'), out.count(b'\n'),
         out.count(b'\r')))

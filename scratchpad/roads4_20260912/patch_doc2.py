"""Append lane ROADS4's provenance section to docs/LODGEN_TERRAIN_VT.md."""
import io

P = 'docs/LODGEN_TERRAIN_VT.md'
s = io.open(P, encoding='utf-8').read()

BLOCK = """
### ROADS4, 2026-09-12

Section 1a.5e is new and 1a.5's colour bullet is amended: `--road-detail`
defaults to **1.0** from this lane, by bungo's ruling on a picture, and
`--road-detail 0` reproduces every earlier bake byte for byte (9 files on chunk
(-20,20) and 10 on (-8,8), `bake.log` excluded because it records the command
line). `--road-ground-paint` is a new row of section 5.

Vanilla's numbers are Bethesda's shipped dim-4 sheets read as LOOSE FILES under
`E:/Tools/Fallout 4/DataUnpacked/Data`; ours are region bakes into
`scratchpad/roads4_20260912/out/` (eleven variants on two chunks) made by
`release/NifSkope.exe` (2026-09-12 06:31:05, 21,819,904 bytes), with the rung
`release/NifSkope.before_roads4.exe` (2026-09-12 05:48:33, md5
`980e64c1aa4e5478b5833d83ebea9655`) used for the before column. The instruments
are `scratchpad/roads4_20260912/r4lib.py` (the projection: which shape wins each
texel, its material, its vertex alpha -- the alpha buffer initialised to ONES,
which is the correction of the earlier -0.792 finding), `r4_material.py` (the
folder classifier), `r4_bake.sh`, `r4_gp.py` (the variant table, `logs/gp.json`),
`r4_gates.py` (`logs/gates.json`) and `r4_pics.py`, with the seam sets re-read by
`scratchpad/roads2_20260911/seam.py` into `seam_r4_*.json`. The road mask is the
difference between a bake and a `--no-roads` bake of the same generator;
vanilla has no such pair, so section 1a.5e's width comparison uses the PROJECTED
road width, a world fact from the ESM, and says so.

What this lane could NOT meet, in its own words: `tests/spells/lodgen_roads.sh`
is 11 checks / 1 failure on this exe -- R5 `after` 0.3078 against bar 2 = 0.3223,
short by 0.0145 -- and the cause is the detail default flip alone, measured on
the rung before the build (detail 0 reads 0.3435 and passes).

| file | sha256 (16) | bytes | lines |
|---|---|---|---|
| `src/lodgen.cpp` | `913e9f64d1cf5378` | 508,963 | 11680 |
| `src/lodgen.h` | `bd31aff30f21db1d` | 69,831 | 1228 |
| `src/nifcli.cpp` | `ce8c1c8005cf66d3` | 314,814 | 6952 |

| claim | line | anchor |
|---|---|---|
| 1a.5 road detail defaults to 1.0 | `lodgen.h:798` | `float roadDetail = 1.0f;` |
| 1a.5e the ground-paint multiplier, default 1.0 | `lodgen.h:839` | `float roadGroundPaint = 1.0f;` |
| 1a.5e the census field | `lodgen.h:932` | `int groundTexels = 0;` |
| 1a.5e the folder rule | `lodgen.cpp:6854` | `bool lodgenRoadMaterialIsGround( const QString & matName )` |
| 1a.5e the multiply, on COVERAGE and before the `cov <= 0` drop | `lodgen.cpp:7133` | `if ( sh.groundMat ) {` |
| 1a.5e the flag is set from the material NAME, before the file is opened | `lodgen.cpp:7272` | `out.groundMat = m.ground;` |
| 5 `--road-ground-paint` | `nifcli.cpp:6308` | `else if ( t == QLatin1String( "--road-ground-paint" ) ) {` |
"""

if not s.endswith('\n'):
    s += '\n'
s += BLOCK
io.open(P, 'w', encoding='utf-8', newline='\n').write(s)
b = io.open(P, 'rb').read()
print('ok  CR %d  LF %d  bytes %d' % (b.count(b'\r'), b.count(b'\n'), len(b)))

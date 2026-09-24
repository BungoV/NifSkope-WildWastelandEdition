"""TILING4 -- the `docs/LODGEN_TERRAIN_VT.md` amendment, as a refusing script.

Three edits, each anchored on text that must occur EXACTLY once (the CLI rows
and the section break), and one append. `--check` counts the anchors and writes
nothing. The file is LF-only and UTF-8 with em dashes in it, so it is read and
written as bytes and the CR count is asserted unchanged (0) on the way out.

    python d0_doc.py --check      # count only
    python d0_doc.py              # apply
"""
import hashlib
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    '..', '..'))
DOC = os.path.join(ROOT, 'docs', 'LODGEN_TERRAIN_VT.md')

SEC_ANCHOR = '\n---\n\n## 3. `.lodt` v2 \u2014 the container\n'

SECTION = '''
### 2.5e The hex tiling, which breaks the repeat without straining anything (lane TILING4, 2026-09-12)

\u00a72.5a measured the defect: the land textures' 341.3333-unit repeat reads at an
amplitude of **1.037** and **1.261** on the two Sanctuary tiles, where the worst
of Bethesda's 22 shipped dim-4 sheets reads **0.264**. TILING3's answer,
`--land-sample stochastic`, warped world position smoothly before the lookup.
It works -- the repeat falls to 0.183 -- but the warp must strain the ground by
about **0.72** of a texel per texel to break the phase, and bungo saw the
strain: *"the proposal looks pretty good, but maybe it could use some
improvement"* (2026-09-12 00:0x, over `cmp_tiling3.png`). The strain IS the
swirl; a warp gentle enough not to swirl does not break the repeat.

**So `--land-sample stochastic` now means a histogram-preserving hex tiling**
(Heitz & Neyret 2018) and the warp is reachable as `--land-sample warp`. For a
land-texture lookup at world position `(wx, wy)`:

1. `(px, py) = (wx, wy) / S`, where `S` is the cell size in world units
   (`--land-hex`, shipped **256.0**; one land repeat is 341.3333).
2. Skew onto a triangle lattice -- `sx = px - 0.57735026918962576 * py`,
   `sy = 1.15470053837925152 * py` -- and take the enclosing triangle's three
   vertices and its barycentric weights. The lattice index is computed in
   **double**: at the far edge of the worldspace a float index quantises.
3. Each vertex `(i, j)` hashes to a fixed offset in the texture's own repeat,
   through the SAME hash the warp already uses (`lodgenWarpHash`) -- one hash in
   the file, not two.
4. The three offset taps are blended variance-preserving:
   `mean + (\u03a3 w_k (s_k - mean)) / sqrt(\u03a3 w_k\u00b2)`, with `mean` the land
   texture's own average colour. Dividing by `sqrt(\u03a3 w_k\u00b2)` rather than by
   `\u03a3 w_k` is what keeps the grain's contrast across a cell boundary instead of
   fading it towards the mean, which is the whole point of the operator.
5. **Alpha is never blended.** The alpha comes from the largest-weight tap. A
   variance-preserving blend of a constant 1.0 alpha would read about 1.07.
6. A mip bias of **-0.22** (`--land-mip-bias`) restores the grain the blend
   softens.

Being a pure function of world position it is seamless across chunk and cell
boundaries, and identical at any chunk-thread count: measured, 97 files and 0
differing between `--chunk-threads 1` and `16` on a 16-chunk block.

**It is NOT the default, and the number that decides that is the repeat.** On
the fourteen shipped sheets of this lane's frozen selection/validation split the
hex tiling passes the repeat law on **6 of 7 and 6 of 7** -- (-4,-20) reads
0.308 and (-12,-20) reads 0.279 against the 0.264 absolute ceiling, +17 % and
+6 % over. The hex offsets break the phase BETWEEN tiles; they do nothing to
the land texture's own 10.667-texel period INSIDE one tap, which is what those
two sheets are carrying. A capped warp on top (strain 0.5, the two operators
composed) was built and measured: it buys **no** repeat at all -- 6 of 7 with
it and 6 of 7 without -- and costs the swirl, 7 of 7 down to 2 of 7.

**What it does buy**, on the same fourteen sheets and the same bakes: the swirl
reading -- the structure-tensor orientation coherence of the 1-5 texel grain
over each sheet's own phase-twin floor, the repeat notched out -- goes from
**2 of 7 passing** under the warp to **7 of 7 and 7 of 7**, at the same repeat
count and with every sheet inside 20 % of the previous build's grain. On chunk
(-20,24), whole sheet, from the real bakes: repeat **0.148** against vanilla's
0.201 and the warp's 0.183, swirl r **1.113** against vanilla's 1.994 and the
warp's 2.221, grain 3.834 against vanilla's 4.476.

What the hex tiling substitutes for the swirls is a soft blotchiness at its own
cell scale, which no instrument in this lane gates and which
`scratchpad/tiling4_20260912/images/sheet_tiling4.png` shows at 1:1.
'''

CLI_OLD = ('| `--land-sample footprint\\|average\\|stochastic` | `footprint` | '
           '\u00a72.5a. `stochastic` is the experimental repeat fix: the domain warp '
           'below at 683 / 1024 / 1 octave with a mip bias of -1.00 |\n')
CLI_NEW = ('| `--land-sample footprint\\|average\\|stochastic\\|warp` | `footprint` | '
           '\u00a72.5a. `stochastic` is the experimental repeat fix and as of lane '
           'TILING4 it means the HEX TILING of \u00a72.5e: `--land-hex 256` with a mip '
           'bias of -0.22. `warp` is TILING3\'s domain warp, 683 / 1024 / 1 octave '
           'with a mip bias of -1.00, kept reachable so its measurements can be '
           'repeated; each word turns the other geometry off |\n')

BIAS_ROW = ('| `--land-mip-bias B` | 0 | mip bias on the land-texture lookup, to '
            'restore the grain a warp smooths away |\n')
HEX_ROW = ('| `--land-hex UNITS` | 0 | \u00a72.5e, the hex cell size in world units. '
           '0 = off, and off is the pre-TILING4 bake byte for byte, so '
           '`--land-hex 0 --land-mip-bias 0` is the exact way back from '
           '`--land-sample stochastic` |\n')

PROV = '''
### TILING4, 2026-09-12

Section 2.5e is new and two rows of \u00a75 moved (`--land-sample` gained `warp`;
`--land-hex` is new). Its vanilla numbers come from Bethesda's shipped dim-4
sheets read as LOOSE FILES under `E:/Tools/Fallout 4/DataUnpacked/Data`; its own
numbers come from bakes into `scratchpad/tiling4_20260912/out/` made by
`release/NifSkope.exe` (2026-09-12 02:08:57, 21,487,616 bytes) and by the rung
`release/NifSkope.before_tiling4.exe` (21,484,032 bytes), and from the
fourteen-sheet sweep under `scratchpad/tiling4_20260912/`
(`s1e_law.py` the swirl law, `h1_sweep.py`, `h2_rescore.py`, `h3_sweep.py`,
`h4_pick.py`, `h4b_flip.py`, `t4_gates.py`, `f2_gate.py`, `f3_real.py`,
`hex_parity.py`) with the logs beside them. No number here is copied forward:
each was re-read off the artefact it describes. Every line below was found
again from its own anchor text against the sources stamped here
(`ww-contract-provenance` step 3).

| file | sha256 (16) | bytes | lines |
|---|---|---|---|
| `src/lodgen.cpp` | `ede9807373f12959` | 503,111 | 11557 |
| `src/lodgen.h` | `939b36f451ec4e92` | 62,900 | 1113 |
| `src/nifcli.cpp` | `df0f4a2230766684` | 309,875 | 6881 |

| claim | line | anchor |
|---|---|---|
| \u00a72.5e the cell size, 0 = off | `lodgen.cpp:6081` | `static float g_landHexSize = 0.0f;` |
| \u00a72.5e the two lattice constants, in double | `lodgen.cpp:6086` | `static const double LODGEN_HEX_SKEW  = 0.57735026918962576;` |
| \u00a72.5e the triangle, its three vertices and its weights | `lodgen.cpp:6096` | `void lodgenLandHexCell( double wx, double wy, double size,` |
| \u00a72.5e the per-vertex offset, through the warp's own hash | `lodgen.cpp:6122` | `static inline double lodgenLandHexOffset( qint32 i, qint32 j, quint32 k )` |
| \u00a72.5e the variance-preserving blend, and alpha taken from the largest-weight tap | `lodgen.cpp:6133` | `FloatVector4 lodgenLandHexTap( const DDSTexture16 * tex,` |
| \u00a72.5e the setter, clamped at 0 | `lodgen.cpp:6183` | `void lodgenSetLandHexSize( float units )` |
| \u00a72.5e BOTH sampling sites call it -- the anchor occurs exactly twice and that is the claim | `lodgen.cpp:7666` (stock chunk path) and `lodgen.cpp:8990` (pyramid path) | `lodgenLandHexTap( tex, swx, swy, TILE` |
| \u00a72.5e the declarations | `lodgen.h:225` | `float lodgenLandHexSize();` |
| \u00a75 `stochastic` means the hex tiling, 256 units at bias -0.22 | `nifcli.cpp:6090` | `lodgenSetLandHexSize( 256.0f );` |
| \u00a75 `--land-hex` on its own | `nifcli.cpp:6116` | `else if ( t == QLatin1String( "--land-hex" ) ) lodgenSetLandHexSize( next().toFloat() );` |
'''


def main():
    check = '--check' in sys.argv
    b = open(DOC, 'rb').read()
    cr0 = b.count(b'\r')
    t = b.decode('utf-8')
    edits = [('the \u00a73 section break', SEC_ANCHOR, 1),
             ('the --land-sample CLI row', CLI_OLD, 1),
             ('the --land-mip-bias CLI row', BIAS_ROW, 1),
             ('the provenance tail', '### TILING3, 2026-09-11', 1)]
    bad = False
    for name, anchor, want in edits:
        n = t.count(anchor)
        print('%-32s %d (want %d)%s' % (name, n, want,
                                        '' if n == want else '   REFUSED'))
        bad |= n != want
    if t.count('2.5e') or t.count('--land-hex'):
        print('ALREADY APPLIED: the file already mentions 2.5e / --land-hex')
        bad = True
    if bad:
        print('REFUSED: nothing written')
        return 2
    if check:
        print('--check: counts only, nothing written')
        return 0
    t = t.replace(SEC_ANCHOR, SECTION + SEC_ANCHOR, 1)
    t = t.replace(CLI_OLD, CLI_NEW, 1)
    t = t.replace(BIAS_ROW, BIAS_ROW + HEX_ROW, 1)
    t = t + PROV
    out = t.encode('utf-8')
    assert out.count(b'\r') == cr0 == 0, 'CR count moved'
    open(DOC, 'wb').write(out)
    print('wrote %s  %d -> %d bytes, sha256 %s'
          % (DOC, len(b), len(out), hashlib.sha256(out).hexdigest()[:16]))
    return 0


if __name__ == '__main__':
    sys.exit(main())

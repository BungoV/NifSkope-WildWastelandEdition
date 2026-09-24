"""ROADS3's second amendment to docs/LODGEN_TERRAIN_VT.md 1a.5d.

The first amendment was written while the lane was BUILD PENDING. The build was
then spent, so the closing paragraph is replaced with what a built exe measured,
and the simulated table gains its baked rows. Written as a file, not a heredoc:
prose apostrophes, and heredocs arrive CRLF.
"""
import io
import sys

P = r'E:\Projects\NifskopeWildWastelandEdition\docs\LODGEN_TERRAIN_VT.md'
s = io.open(P, encoding='utf-8', newline='').read()
if '\r\n' in s:
    sys.exit('REFUSED: the contract is not LF-only')
if 'measured on the built exe' in s:
    sys.exit('REFUSED: already amended a second time')


def sub(old, new):
    global s
    if s.count(old) != 1:
        sys.exit('REFUSED: %d matches for %r' % (s.count(old), old[:70]))
    s = s.replace(old, new)


# --- the priced table gains its baked rows -------------------------------
sub("""The priced table, simulated, both tiles (road L, and rise over the surround;
vanilla is 92.52 / +4.29 and 94.59 / +4.40):

| `--road-opacity` | (-20,20) | (-8,8) |
|---|---|---|
| **1.000 (default)** | 99.05, +29.96 | 106.68, +3.84 |
| 0.830 | 92.57, +23.48 | 105.91, +3.07 |
| 0.500 | 80.01, +10.91 | 104.40, +1.56 |
| 0.326 | 73.38, **+4.28** | 103.61, +0.77 |
| 0.250 | 70.48, +1.39 | 103.26, +0.42 |""",
    """The table, both tiles (road L, and rise over the surround; vanilla is
92.52 / +4.29 and 94.59 / +4.40). Three rows were BAKED on the built exe of
2026-09-12 04:10:38 and are marked so; the other two are the offline pricing:

| `--road-opacity` | (-20,20) | (-8,8) |
|---|---|---|
| **1.000 (default)** -- BAKED | 99.05, +29.96 | 106.68, +3.84 |
| 0.830 -- BAKED | 92.43, +23.34 | 105.89, +3.05 |
| 0.500 -- priced | 80.01, +10.91 | 104.40, +1.56 |
| 0.326 -- BAKED | 73.09, **+4.01** | 103.39, +0.55 |
| 0.250 -- priced | 70.48, +1.39 | 103.26, +0.42 |""")

# --- the closing paragraph ------------------------------------------------
sub("""**What is NOT yet measured on a built exe.** As of this amendment the change is
written and uncompiled: `Fallout4.exe` was running when the build would have
been spent, so the lane ended BUILD PENDING. The byte-identity claim in the
first paragraph is a claim about the CODE (the multiply is branched over), not
yet a `cmp` result, and it is owed -- see `scratchpad/roads3_20260911/PENDING.md`.
""",
    """**8. What was measured on the built exe** (2026-09-12 04:10:38, 21,489,152 B,
md5 `fe65cc978f3896881140c2eea57c69c6`; logs under
`scratchpad/roads3_20260911/logs/`).

The byte-identity claim in the first paragraph is now a `cmp` result and not an
argument about the code. Every file of both bakes compared, not a sample
(`r3_f2.sh`, `f2_bytes.txt`): on (-20,20) the new exe with no flag reads **9 of
9 identical** to the rung, `--road-opacity 1` **9 of 9**, `--roads-legacy` **9 of
9** against the rung's own `--roads-legacy`; on (-8,8) **10 of 10** in all three
arms. The compare is shown able to fail in the same run: `--road-opacity 0.326`
moves 3 files on (-20,20) and 4 on (-8,8), and **every one of them is colour** --
the `tex/Commonwealth.4.<x>.<y>.DDS` sheet and the `.lodt` virtual-texture
levels. The `_msn` normal sheet, the `_data` sheet, the `.bto` objects, the
`.lodl` and the `.lodm` are byte-identical at **every** setting, so the switch
reaches the road colour and nothing else.

What the baked sheets read (`r3_f3.py`, `f3_gates.txt`), against vanilla's own:

| field | road L | rise | local 5x5 SD | biggest step |
|---|---|---|---|---|
| vanilla (-20,20) | 92.52 | +4.29 | 6.59 | 1.31 |
| default, a = 1 | 99.05 | +29.96 | 7.26 | 3.88 |
| baked a = 0.326 | 73.09 | **+4.01** | 4.20 | 1.69 |
| baked a = 0.83 | **92.43** | +23.34 | 6.72 | 3.35 |
| vanilla (-8,8) | 94.59 | +4.40 | 6.44 | 4.43 |
| default, a = 1 | 106.68 | +3.84 | 5.42 | 1.25 |
| baked a = 0.326 | 103.39 | +0.55 | 4.29 | 1.04 |

**The two-tone skirt, measured directly.** ROADS2's `seam.py` re-run on these
bakes (`f3g_seam.txt`) correlates road luminance with the road mesh's own
interpolated vertex alpha: vanilla **+0.001**, ours at a = 1 **-0.792**, at 0.83
-0.694, at 0.326 **-0.436** -- and our unpainted ground's own floor on the same
texels is **-0.325**. So the opacity knob walks the skirt signature from -0.79
toward the ground's -0.33 and can never reach vanilla's 0: the ramped skirt
geometry is still underneath, and opacity dilutes it rather than removing it.
The feathered-boundary luminance gradient (54 texels, vanilla 4.242) reads 3.979
at a = 1, 3.352 at 0.83, 2.979 at 0.326, all inside vanilla's, with
displaced-boundary floors of 3.10 to 4.81.

**9. How good the offline pricing actually was, measured rather than guessed.**
An earlier revision of this section said the simulation was right to "about half
a level". Baked and compared texel by texel, it is right on the AGGREGATES --
road mean luminance within **0.286** of a level on (-20,20) and **0.221** on
(-8,8) -- and is not right per texel: mean absolute difference **1.583** levels,
99th percentile 7.341, worst **15.279** (1.527 / 5.745 / 13.802 downtown),
because the bake passes through 8-bit quantisation and BC1 block compression and
the simulation does not. Read it as a licence to choose WHICH settings to bake,
never as a substitute for baking them, and never as a picture.

The harness chain on this exe matched lane GRADE1's baselines row for row:
`lodgen_roads` 11/0, `lodgen_terrain` 26/0, `lodgen_terrain_vt` 41/1,
`lodgen_ground_cover` 29/5, `lodgen_terrain_pbrm` 14/0, `lodgen_native` 0
failures in all seven sections, `lodl_open` 23/0, `lod_generation` 116/0. The
one red row in `lodgen_terrain_vt` is **V9c**, and it is not this pass's: the
rung exe, run as a control, fails it with digit-for-digit identical numbers
(E/W seam 188.074, interior 13.243, ratio 14.20, edge step 14.348).
""")

io.open(P, 'w', encoding='utf-8', newline='').write(s)
d = io.open(P, 'rb').read()
print('contract amended a second time; CRLF %d; lines %d'
      % (d.count(b'\r\n'), d.count(b'\n')))

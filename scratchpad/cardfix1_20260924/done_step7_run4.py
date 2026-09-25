# CARDFIX1 step 7: DONE.md / DELIVERABLE_TEXT.md / progress.md after the director's two decisions and gate run 4.
# LF-only files; anchors asserted exactly once; CR count unchanged.
D = 'E:/Projects/NifskopeWWE-cardfix1/scratchpad/cardfix1_20260924/'


def patch(P, pairs):
    b = open(P, 'rb').read()
    cr = b.count(b'\r')
    s = b.decode('utf-8')
    for o, n in pairs:
        assert s.count(o) == 1, (P, o[:70], s.count(o))
        s = s.replace(o, n)
    out = s.encode('utf-8')
    assert out.count(b'\r') == cr
    open(P, 'wb').write(out)
    print('patched %s: %+d bytes, CR %d' % (P.split('/')[-1], len(out) - len(b), cr))


d = open(D + 'DONE.md', 'rb').read().decode('utf-8')
top = d[:d.index('\n# 1. Skills loaded')]
TOP = '''PARTIAL -- lane CARDFIX1 (LOD-D), chain of seven steps. Steps 1-6 landed (6b, 6c after). Step 7
(IMPOSTORPBRM1) is BUILT and committed (exe 45719ad4). The director decided both run-3 reds (2026-09-25,
director decisions, not rulings by bungo). Gate run 4 on those bars: 13/14.
LEFT: one row. The non-aa arm, judged on fully covered texels as decided, still fails 4 rows of the
tree-animated material (roughness, metallic, sqrtF0.R, weight: 0.872-0.896 within 2 against 0.90; medians
exact). 98 % of the misses sit beside a texel of the OTHER material: they are texels where the two
materials meet, not partial coverage. Away from the other material the share is 0.997. The fully covered
population keeps only 13781 of that material's texels (the aa arm has 132925), so the boundary share
rises from 2.6 % to 12.2 %. All the breakages still fail on this population, so the decision's drop
clause did not fire. The row is neither passing nor dropped: the director's call (section 2, step 7,
"Run 4"). No bar was changed after run 4.'''
patch(D + 'DONE.md', [
    (top, TOP),
    ('### Reds (reported, not re-pinned; director decision needed)',
     '''### Run 4 -- the director's decisions applied (2026-09-25 02:4x; director decisions, not rulings by bungo)
- **(1) Colour rows:** bar = max(1.25 x identity floor, 0.5 level), which is one 8-bit rounding plus
  margin. Every breakage still fails at 0.5. The margins below are bar minus measured, so a negative margin
  means a failure. `--red add` gives colour 4.14 levels on the aa arm (margin -3.64) and 4.00 on the non-aa
  arm (-3.50). `--red ior` and `--red decode` fail their sqrtF0 rows with 0.000-0.016 of texels within 2
  (bar 0.90). The pre-step-7 exe fails family (legacy) and sheets (_rmaos / _s absent) on both arms.
  Correct code: MapleAtlas01 0.32 (margin +0.18), MapleAtlas02_Tree 0.35 (+0.15) on the aa arm; 0.34 (+0.16)
  and 0.39 (+0.11) on the non-aa arm.
- **(2) Non-aa arm:** kept, judged on fully covered texels only (coverage == 1, alpha 255; `COVER=full`,
  named in the gate header and in impostor_pbrm.py's covmin). It has its own identity floor on that
  population (bake identna: median 0.00, p90 1.0 over 90395 texels) and its own pre-step-7 bake (prevna).
  Every breakage fails on it: add 5 rows (colour 4.00), ior 9, decode 8, pre-step-7 exe 2. The drop clause
  did not fire.
- **Still red: the non-aa arm, 4 of 15 rows**, all MapleAtlas02_Tree (the tree-animated class):
  roughness 0.878, metallic 0.872, sqrtF0.R 0.896, specWeight 0.881 within 2 (bar 0.90). The medians are
  exact (179 / 51 / 46 vs 45.77 / 128). MapleAtlas01 passes every row, and so does the colour row of both
  materials. An offline probe on the run-4 bakes (not a gate row) found this:
  - 98.0 % of the misses have a 4-neighbour of the other class, and the missed values read between the two
    materials (roughness p50 155, between 89 and 179): texels where trunk and leaf meet.
  - Away from class 0, the class-1 share within 2 is 0.997 (11082 texels).
  - The aa arm has the same boundary texels (92 % of its misses), but they are 2.6 % of 132925. The fully
    covered non-aa population is 13781 leaf texels, and 12.2 % of them sit on a boundary.
  So this is the per-texel class split at material boundaries, a population effect of the decided rule,
  not the step-7 law. Options for the director: (a) judge both arms away from the other class (texels with
  no 4-neighbour of the other class). On run 4, the leaf class's roughness then reads aa 0.998 and non-aa
  0.997 (only that channel was probed). (b) Drop the non-aa
  row with this reason. (c) Accept 4 known reds. Not applied: the decision said re-run once.

### Reds of run 3 (reported, not re-pinned; decided by the director, see "Run 4" above)'''),
    ('## Step 7 (exe 45719ad4; gates/impostor_pbrm.run3.out + .run3.log)',
     '''## Step 7, run 4 on the director's bars (exe 45719ad4; gates/impostor_pbrm.run4.out; fix39)
| row | measured |
|---|---|
| R1 aa arm | 15 / 15 ok (colour 0.32 / 0.35, bar 0.50) |
| R2 non-aa arm, coverage == 1 | 11 / 15 ok; 4 FAIL, MapleAtlas02_Tree boundary texels (0.872-0.896 vs 0.90) |
| reds, aa: add / ior / decode / pre-step-7 exe | 1 / 6 / 5 / 2 FAIL |
| reds, non-aa: add / ior / decode / pre-step-7 exe | 5 / 9 / 8 / 2 FAIL |
| R3 / R4 | ok / ok (0.067 / 1.0 <= 2.776 / 10; 4-bit 5.572 fails) |
| total | 13 / 14 ok |

## Step 7 (exe 45719ad4; gates/impostor_pbrm.run3.out + .run3.log)'''),
])

patch(D + 'DELIVERABLE_TEXT.md', [
    ('step 7 built, its gate 8/10 with 2 bars reported for a director decision.**',
     'step 7 built; director decisions on its two bars applied; gate run 4 13/14, one non-aa row left for the director.**'),
    ('''  `tests/spells/impostor_pbrm.sh` 8/10: RED on (1) the colour rows, whose pre-registered bar became 0 once
  the colour mips were fixed (correct code measures 0.32 / 0.35 levels), and (2) the non-aa arm, a row not in
  the pre-registration that fails on partially covered texels through that arm's existing edge law.
  Proposed: a colour floor of one 8-bit rounding (0.5 level); drop the non-aa row or judge full-coverage
  texels only.''',
     '''  `tests/spells/impostor_pbrm.sh`, run 4 on the director's decisions (colour bar max(1.25 x floor, 0.5);
  non-aa arm judged on fully covered texels): 13/14. Every breakage fails on both arms. Left: the non-aa
  row fails 4 rows of the leaf material on texels where trunk and leaf meet (98 % of the misses; 0.997
  away from the boundary). The options are in DONE.md, step 7, "Run 4".'''),
])

p = open(D + 'progress.md', 'rb').read()
assert p.count(b'\r') == 0
open(D + 'progress.md', 'wb').write(p + (
    b"2026-09-25 02:52 DIRECTOR DECISIONS step 7 (1) colour bar max(1.25xfloor,0.5) (2) non-aa judged coverage==1 -> fix39; "
    b"run4 13/14: aa 15/15, reds bite both arms (add colour 4.14/4.00), non-aa 4 FAIL MapleAtlas02_Tree = material-boundary "
    b"texels (98% beside class 0; 0.997 away); reported, not re-pinned; committing and stopping\n"))
print('progress ok')

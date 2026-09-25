# CARDFIX1 step 7 closes: gate run 5 on the director's interior rule, 14/14. DONE.md / DELIVERABLE_TEXT.md /
# progress.md. LF-only files; anchors asserted exactly once; CR count unchanged.
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
TOP = '''DONE -- lane CARDFIX1 (LOD-D), all seven steps landed (6b, 6c after 6). Step 7 (IMPOSTORPBRM1): exe
45719ad4. Its gate is green on run 5, 14/14. That run uses three director decisions (2026-09-25, not
rulings by bungo): the colour bar is max(1.25 x floor, 0.5 level); both arms are judged on
COVER=full,interior (fully covered texels with no 4-neighbour of another material); and every breakage
still fails on that population on both arms. native_lighting is left to the director (its 2 failures
reproduce on the step-5 exe). Owed outside this lane: the FO4CS reader for the `_s` sheet.'''
patch(D + 'DONE.md', [
    (top, TOP),
    ('### Run 4 -- the director\'s decisions applied',
     '''### Run 5 -- GREEN, 14/14 (2026-09-25 02:5x; fix40; gates/impostor_pbrm.run5.out)
- DIRECTOR DECISION (a) after run 4, not a ruling by bungo. On both arms, every material row and both
  identity floors judge only COVER=full,interior: alpha 255 texels with no 4-neighbour of another material.
  A boundary texel mixes two materials, which is correct and not what these rows test. The rule is named in
  the gate header and in impostor_pbrm.py (covmin, interior, classes).
- Identity floors on that population: aa median 0.00, p90 1.0 over 76216 texels; non-aa median 0.00, p90
  1.0 over 83992. Colour bar 0.50 on both.
- Correct code, share within 2 (bar 0.90) and colour margin (bar - measured):

  | arm | MapleAtlas01 (texels) | material rows | colour | MapleAtlas02_Tree (texels) | material rows | colour |
  |---|---|---|---|---|---|---|
  | aa | 73646 | 0.985-0.992 | 0.31, margin +0.19 | 2570 | 0.956-0.972 | 0.29, margin +0.21 |
  | non-aa | 72873 | 0.995-0.998 | 0.33, margin +0.17 | 11082 | 0.996-0.999 | 0.35, margin +0.15 |

- The breakages on this population, both arms (margin = bar - measured):

  | breakage | aa | non-aa |
  |---|---|---|
  | --red add (tint rule) | 1 FAIL: leaf colour 4.10, margin -3.60 | 1 FAIL: leaf colour 4.19, margin -3.69 |
  | --red ior | 6 FAIL: sqrtF0 R/G/B both materials, 0.000-0.005 within 2 (margin >= -0.895) | 6 FAIL: 0.000-0.001 within 2 |
  | --red decode | 5 FAIL: 0.000 within 2 (margin -0.90) | 5 FAIL: 0.000 within 2 |
  | pre-step-7 exe | 2 FAIL: family legacy, _rmaos / _s absent | 2 FAIL: the same |

  R3 ok/ok. R4: 0.067 / 1.0 <= 2.776 / 10, and the 4-bit red fails.
- Population note: on the aa arm, the leaf material keeps only 2570 texels, down from 132925 at alpha >= 128.
  aa leaf texels are mostly partial alpha, so few reach 255. That is above the rows' 500 minimum, and every
  red still bites at that size (add 4.10). The non-aa leaf keeps 11082.

### Run 4 -- the director's decisions applied'''),
    ('## Step 7, run 4 on the director\'s bars (exe 45719ad4; gates/impostor_pbrm.run4.out; fix39)',
     '''## Step 7, run 5 -- GREEN (exe 45719ad4; gates/impostor_pbrm.run5.out; fix40, COVER=full,interior)
| row | measured |
|---|---|
| R1 aa / R2 non-aa | 15 / 15 ok and 15 / 15 ok |
| reds aa: add / ior / decode / pre-step-7 exe | 1 / 6 / 5 / 2 FAIL |
| reds non-aa: add / ior / decode / pre-step-7 exe | 1 / 6 / 5 / 2 FAIL |
| R3 / R4 | ok / ok |
| total | 14 / 14 ok |

## Step 7, run 4 on the director's bars (exe 45719ad4; gates/impostor_pbrm.run4.out; fix39)'''),
])

patch(D + 'DELIVERABLE_TEXT.md', [
    ('**Lane CARDFIX1 (LOD-D), 2026-09-24/25: PARTIAL. Steps 1-6 landed (G4 decided by the director, (a)); step 7 built; director decisions on its two bars applied; gate run 4 13/14, one non-aa row left for the director.**',
     '**Lane CARDFIX1 (LOD-D), 2026-09-24/25: DONE. Steps 1-7 landed (G4 and step 7\'s gate bars decided by the director, not bungo).**'),
    ('the G4 re-pin commit, step 7 2672e43 + 1f0368d + 3e92051 + the gate/docs commit.',
     'the G4 re-pin commit, step 7 2672e43 + 1f0368d + 3e92051 + 8dc4e9e + 9d3fbe3 + the run-5 commit.'),
    ('''  `tests/spells/impostor_pbrm.sh`, run 4 on the director's decisions (colour bar max(1.25 x floor, 0.5);
  non-aa arm judged on fully covered texels): 13/14. Every breakage fails on both arms. Left: the non-aa
  row fails 4 rows of the leaf material on texels where trunk and leaf meet (98 % of the misses; 0.997
  away from the boundary). The options are in DONE.md, step 7, "Run 4".''',
     '''  `tests/spells/impostor_pbrm.sh`: 14/14 on run 5. It uses the director's decisions: the colour bar is
  max(1.25 x floor, 0.5 level), and both arms are judged on COVER=full,interior, meaning fully covered
  texels away from another material. A texel where trunk and leaf meet mixes them, correctly. The wrong
  tint rule, wrong IOR, wrong decode and the pre-step-7 exe each fail on both arms (tint: colour 4.10 /
  4.19 levels against a 0.5 bar).'''),
    ('native_lighting\'s 2 failures reproduce on the step-5 exe: baseline drift, not step 7.',
     'native_lighting\'s 2 failures reproduce on the step-5 exe: baseline drift, not step 7 (left to the director).'),
    ('**2026-09-25 02:3x -- CARDFIX1 step 7: a non-aa row was given the aa arm\'s edge bar.**',
     '''**2026-09-25 02:5x -- CARDFIX1 step 7: a per-material row judged texels where two materials meet.** A
texel on the trunk-leaf boundary mixes both materials, which is correct, but the class split gave it to one of
them. On the non-aa arm's small fully covered leaf population, those texels were 12 % and failed the row (run
4). Rule: a per-material row judges texels away from any other material, and a population rule is checked for
what it removes (the aa leaf fell from 132925 to 2570 texels).

**2026-09-25 02:3x -- CARDFIX1 step 7: a non-aa row was given the aa arm's edge bar.**'''),
])

p = open(D + 'progress.md', 'rb').read()
assert p.count(b'\r') == 0
open(D + 'progress.md', 'wb').write(p + (
    b"2026-09-25 02:57 DIRECTOR DECISION (a): COVER=full,interior both arms (fix40); run5 14/14 GREEN; reds bite both arms "
    b"(add colour 4.10/4.19 vs 0.5; ior 6/6; decode 5/5; prev exe 2/2); aa leaf population 2570 texels; step 7 DONE, lane DONE; "
    b"committing and stopping\n"))
print('progress ok')

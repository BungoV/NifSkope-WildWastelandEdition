"""PREPARED, NOT APPLIED -- RULING A: the viewer's default alpha cut,
0.0627 -> 0.20.

IMPOSTORFIX3 was told not to apply this. It is bungo's ruling because it
changes what every existing card set LOOKS like with no rebake and no new
switch, and because the number that makes a bare maple honest is the number
that eats its thinnest twigs. This script is the whole diff, written so that a
yes costs one short lane: run it with --apply and build.

    python hookup_ruling_alpha.py            # --check, the default, writes nothing
    python hookup_ruling_alpha.py --apply    # refuses unless every anchor matches once

WHAT IT IS. `impostordraw.cpp` uses the SET'S OWN `coverage.floor` as the
display cut when the caller names none. covFloor is the BAKE's encoding floor
-- the alpha level below which the bake calls a texel empty -- and it is 16 of
255, so the shipped default cut is 0.0627. Everything the bake wrote as
"almost nothing" is therefore painted.

THE EVIDENCE (IMPOSTORFIX2 s4, 24 orbit views x 5 subjects, numpy reference
card against the harness's own mesh grabs):

    subject    0.0627 today   0.200    gain
    blast_n8      0.6737     0.7194   +0.0457
    blast_n4      0.5090     0.5437   +0.0347
    maple_n4      0.3586     0.3775   +0.0189
    dead_n4       0.5824     0.6010   +0.0186
    rock_n4       0.7767     0.7842   +0.0075   NOT PROVEN (inside the noise)

THE REFUTER, stated so a no is cheap: raise it far enough and a thin subject
loses its card entirely. Measured: ZERO of the 24 views on ANY of the five
subjects loses its card at any threshold up to 0.45, so 0.20 is not near that
cliff -- but 0.20 DOES cut ink the bake wrote, and on the bare maple that ink
is real twigs photographed thinner than a texel. The picture pair beside this
script is the thing to look at, not the table.

PICTURES: scratchpad/impostorfix3_20260919/ruling_alpha_maple.png
          scratchpad/impostorfix3_20260919/ruling_alpha_rock.png
Each is two rows of 12 azimuths -- today's 0.0627 on top, 0.20 below -- drawn
with the numpy reference card so the only thing that differs between the rows
is the cut.

WHAT IS *NOT* IN THE TABLE BELOW, deliberately:
  * A set that declares NO coverage keeps its 0.5 test. Nothing on the table
    measured such a set, so nothing here changes it.
  * `ImpostorDrawOptions::alphaThreshold` still wins when the caller sets it.
    This edit moves the FALLBACK only.
"""
import sys

ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'

A_DRAW = (
    "\tconst float thr = ( opt.alphaThreshold >= 0.0f ) ? opt.alphaThreshold\n"
    "\t\t\t: ( set.covOk() ? float( set.covFloor ) / 255.0f : 0.5f );\n"
)
B_DRAW = (
    "\t/* THE DEFAULT CUT IS A RULING, NOT A MEASUREMENT (bungo, 2026-09-19).\n"
    "\t * `set.covFloor` is the BAKE's encoding floor -- the alpha level below\n"
    "\t * which the bake calls a texel empty -- and it is 16 of 255, so using it\n"
    "\t * here made the shipped default cut 0.0627 and painted everything the\n"
    "\t * bake wrote as almost nothing. At 0.0627 the bare maple paints 188 per\n"
    "\t * cent of the mesh's ink and blast_n4 151 per cent.\n"
    "\t *\n"
    "\t * 0.20, over 24 orbit views on five subjects (IMPOSTORFIX2 s4):\n"
    "\t *   blast_n8 0.6737 -> 0.7194, blast_n4 0.5090 -> 0.5437,\n"
    "\t *   maple_n4 0.3586 -> 0.3775, dead_n4 0.5824 -> 0.6010,\n"
    "\t *   rock_n4  0.7767 -> 0.7842 (inside the noise, NOT PROVEN).\n"
    "\t * THE REFUTER: no view of any subject loses its card at any cut up to\n"
    "\t * 0.45, so this is not near the cliff -- but it DOES cut ink the bake\n"
    "\t * wrote, and on a bare maple that ink is twigs thinner than a texel.\n"
    "\t *\n"
    "\t * A set that declares no coverage still tests at 0.5: nothing measured\n"
    "\t * one, so nothing here changes it. */\n"
    "\tconst float kDefaultAlphaCut = 0.20f;\n"
    "\tconst float thr = ( opt.alphaThreshold >= 0.0f ) ? opt.alphaThreshold\n"
    "\t\t\t: ( set.covOk() ? kDefaultAlphaCut : 0.5f );\n"
)

A_SPEC = "  of its frame against the LOD's pre-baked crown at 14%).\n"
B_SPEC = (
    "  of its frame against the LOD's pre-baked crown at 14%).\n"
    "- A VIEWER'S DEFAULT CUT IS 0.20 OF THE DECODED COVERAGE FRACTION, measured\n"
    "  over 24 orbit views on five subjects. It is NOT the sheet's\n"
    "  `coverage.floor`: that is the bake's encoding floor (16/255 = 0.0627) and\n"
    "  a viewer that uses it as a display cut paints 188 per cent of the mesh's\n"
    "  ink on a bare maple. A set that declares no coverage tests at 0.5. No\n"
    "  view of any measured subject loses its card at any cut up to 0.45.\n"
)

EDITS = [
    ('src/gl/impostordraw.cpp', 'replace', A_DRAW, B_DRAW),
    ('docs/LODGEN_IMPOSTOR_SPEC.md', 'replace', A_SPEC, B_SPEC),
]


def main():
    apply = '--apply' in sys.argv[1:]
    ok = True
    loaded = {}
    for path, mode, anchor, text in EDITS:
        p = ROOT + path
        if p not in loaded:
            loaded[p] = open(p, 'rb').read()
        s = loaded[p].decode('utf-8')
        n = s.count(anchor)
        print('%-34s %-7s anchor x%d  CR %d' % (path, mode, n, loaded[p].count(b'\r')))
        if n != 1:
            print('   ANCHOR COUNT %d (want 1): %r' % (n, anchor[:70]))
            ok = False
            continue
        s = s.replace(anchor, text) if mode == 'replace' else s.replace(anchor, anchor + text)
        loaded[p] = s.encode('utf-8')
    if not ok:
        print('REFUSED: an anchor did not match exactly once.')
        return 2
    if not apply:
        print('CHECK ONLY. %d of %d anchors match once. Nothing written.'
              % (len(EDITS), len(EDITS)))
        return 0
    for path, mode, anchor, text in EDITS:
        p = ROOT + path
        before = open(p, 'rb').read()
        if before.count(b'\r') != loaded[p].count(b'\r'):
            print('CR COUNT MOVED in %s -- refusing the whole apply.' % path)
            return 2
    for p, data in loaded.items():
        open(p, 'wb').write(data)
        print('written %s (%d bytes, CR %d)' % (p, len(data), data.count(b'\r')))
    return 0


if __name__ == '__main__':
    sys.exit(main())

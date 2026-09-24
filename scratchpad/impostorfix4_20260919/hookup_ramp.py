"""IMPOSTORFIX4 hook-up: RAMP the outside-coverage height fill.

REFUSING SCRIPT per `ww-anchored-hookup`. This lane may not edit src/, so the
change is delivered as this script and NOT applied. `--check` is the default
and writes nothing.

WHAT IT CHANGES, and why. `lodgenRepairOctHeight` fills the height outside the
silhouette by dilating out from the fully covered texels, keeps that height for
`kOutRings = 8` rings and then SNAPS to 128, the card plane. That snap is a
cliff, and a cliff inside one 4x4 BC1 block gives the block a height range the
block's single colour line cannot carry. IMPOSTORFIX4 s2e measured that bad
blocks are NOT on silhouette edges (4.1 / 6.0 / 32.1 / 8.9 / 14.5 per cent,
against base rates of 15.0 / 14.5 / 29.7 / 22.0 / 6.6) but DO carry 17.7..32.9
levels of height range where a normal block carries 1.8..4.3.

This replaces the cliff with a linear ramp to the card plane over sixteen
rings, which is the one lever IMPOSTORFIX2 never tested (it tested ring COUNTS
8 / 16 / whole frame, never a ramp).

SIMULATED, s5_ramp.py, 24 views, frozen registration, through the same rebuild
+ the same model of lodgenEncodeBC1Block + the same renderer as every other
number in the report. `today` is re-run through that identical path so a
difference cannot be a difference of route:

    subject     IoU today -> ramp    worst view        texels >12 levels
    blast_n4    0.5709 -> 0.5706     0.3825 -> 0.3647   0.69% -> 0.01%
    maple_n4    0.3699 -> 0.3678     0.2643 -> 0.2759   2.26% -> 1.15%
    dead_n4     0.6139 -> 0.6273     0.4868 -> 0.5470   1.62% -> 0.38%
    rock_n4     see s5_ramp.txt

The honest reading: the chip goes away (four to seventy times fewer texels over
twelve levels) and the 24-view IoU DOES NOT MOVE except on dead_n4 (+0.0134,
just over this lane's own +0.011 bias bar, with the worst view +0.060). A chip
is a visible artefact; the silhouette metric barely sees it. Anyone applying
this should apply it for the artefact, not for the IoU.

NEEDS A RE-BAKE: yes -- it is a bake-time fill, the sheets must be written
again. NEEDS A RULING: no -- the outside fill is already invented by this
function and already chosen by measurement (the 8-ring fill is IMPOSTORFIX3's
own repair of the card plane); this changes HOW it fades, not WHAT is claimed.

REFUTER: if a re-bake with this applied does not reduce the 4x4 chips visible
at the trunk in `ww-texel-picture` panels, the cliff was not the source of the
large-range blocks and the remaining candidate -- genuine depth
discontinuities inside the object -- stands alone.

NOT AUTHORISED: anything the table below does not list. Two lines away from an
anchor is not authorised.
"""
import os, sys

ROOT = r'E:\Projects\NifskopeWildWastelandEdition'

A1 = "\tconst int kOutRings = 8;\t// how far the object's own height reaches past its silhouette"
T1 = ("\tconst int kOutRamp = 16;\t// IMPOSTORFIX4: the outside fill RAMPS to the card plane over\n"
      "\t\t\t\t\t\t\t\t// this many rings instead of snapping at ring 8. A snap is a\n"
      "\t\t\t\t\t\t\t\t// cliff, and a cliff inside one 4x4 BC1 block gives the block a\n"
      "\t\t\t\t\t\t\t\t// height range its single colour line cannot carry -- which is\n"
      "\t\t\t\t\t\t\t\t// what the trunk chips are (IMPOSTORFIX4 s2e/s5).")

A2 = ("\t\t\t\t\t} else if ( ring[size_t( y ) * frameW + x]\n"
      "\t\t\t\t\t\t\t\t&& ring[size_t( y ) * frameW + x] <= kOutRings ) {\n"
      "\t\t\t\t\t\t/* just outside the silhouette: a neighbouring frame's ray\n"
      "\t\t\t\t\t\t * lands here, and the object's own depth is what it should\n"
      "\t\t\t\t\t\t * read. Carried out from the whole texels, not invented. */\n"
      "\t\t\t\t\t\tb = hgt[size_t( y ) * frameW + x];\n"
      "\t\t\t\t\t\toutsideNear++;\n"
      "\t\t\t\t\t} else {\n"
      "\t\t\t\t\t\tb = 128;\t\t// the card plane: the parallax step is then an exact no-op\n"
      "\t\t\t\t\t\toutside++;\n"
      "\t\t\t\t\t}")
T2 = ("\t\t\t\t\t} else if ( ring[size_t( y ) * frameW + x] ) {\n"
      "\t\t\t\t\t\t/* just outside the silhouette: a neighbouring frame's ray\n"
      "\t\t\t\t\t\t * lands here, and the object's own depth is what it should\n"
      "\t\t\t\t\t\t * read. Carried out from the whole texels, not invented --\n"
      "\t\t\t\t\t\t * and faded to the card plane over kOutRamp rings rather\n"
      "\t\t\t\t\t\t * than dropped onto it in one texel. The fade is what keeps\n"
      "\t\t\t\t\t\t * a 4x4 block's height range inside what BC1 can carry. */\n"
      "\t\t\t\t\t\tconst int r = qMin( int( ring[size_t( y ) * frameW + x] ), kOutRamp );\n"
      "\t\t\t\t\t\tconst int d = hgt[size_t( y ) * frameW + x];\n"
      "\t\t\t\t\t\tb = ( d * ( kOutRamp - r ) + 128 * r + kOutRamp / 2 ) / kOutRamp;\n"
      "\t\t\t\t\t\tif ( r < kOutRamp )\n"
      "\t\t\t\t\t\t\toutsideNear++;\n"
      "\t\t\t\t\t\telse\n"
      "\t\t\t\t\t\t\toutside++;\n"
      "\t\t\t\t\t} else {\n"
      "\t\t\t\t\t\tb = 128;\t\t// unreachable from any whole texel: the card plane,\n"
      "\t\t\t\t\t\t\t\t\t\t// where the parallax step is an exact no-op\n"
      "\t\t\t\t\t\toutside++;\n"
      "\t\t\t\t\t}")

EDITS = [
    ('src/lodgen.cpp', 'replace', A1, T1),
    ('src/lodgen.cpp', 'replace', A2, T2),
]


def main(apply_it):
    files = {}
    ok = True
    for path, mode, anchor, text in EDITS:
        full = os.path.join(ROOT, path.replace('/', os.sep))
        if full not in files:
            files[full] = open(full, 'rb').read().decode('utf-8')
        n = files[full].count(anchor)
        print('%-18s %-8s matches %d  %s' %
              (path, mode, n, 'OK' if n == 1 else 'REFUSE (must be exactly 1)'))
        if n != 1:
            ok = False
    for full, s in files.items():
        b = s.encode('utf-8')
        print('%s  bytes %d  CR %d  LF %d' % (os.path.basename(full), len(b),
                                              b.count(b'\r'), b.count(b'\n')))
    if not ok:
        print('REFUSED: an anchor does not match exactly once. Nothing written.')
        return 1
    if not apply_it:
        print('--check only: nothing written. Pass --apply to write.')
        return 0
    for path, mode, anchor, text in EDITS:
        full = os.path.join(ROOT, path.replace('/', os.sep))
        if mode == 'replace':
            files[full] = files[full].replace(anchor, text, 1)
        else:
            files[full] = files[full].replace(anchor, anchor + '\n' + text, 1)
    for full, s in files.items():
        before = open(full, 'rb').read()
        after = s.encode('utf-8')
        print('%s  CR before %d  CR after %d  (LF %d -> %d)' %
              (os.path.basename(full), before.count(b'\r'), after.count(b'\r'),
               before.count(b'\n'), after.count(b'\n')))
        assert after.count(b'\r') == before.count(b'\r'), 'CR count moved'
        open(full, 'wb').write(after)
        print('WROTE', full)
    return 0


if __name__ == '__main__':
    sys.exit(main('--apply' in sys.argv))

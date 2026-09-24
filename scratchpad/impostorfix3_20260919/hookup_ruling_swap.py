"""PREPARED, NOT APPLIED -- RULING B: swap the `_n` sheet's HEIGHT and SWAY
channels, so height rides the BC3 ALPHA block and sway rides the blue.

IMPOSTORFIX3 was told not to apply this. It is bungo's ruling because it is a
FORMAT CONTRACT change: every card sheet ever baked becomes unreadable-as-
written, and a reader that does not know about the swap reads sway where it
wants depth and parallaxes the card into nonsense. This script is the whole
diff for the parts that ARE a hook-up; the parts that are not are named at the
bottom and must not be forgotten when costing a yes.

    python hookup_ruling_swap.py            # --check, the default, writes nothing
    python hookup_ruling_swap.py --apply    # refuses unless every anchor matches once

WHY. BC3 stores RGB in a 4x4 palette of two endpoints (blue shares that palette
with normal X and normal Y) and stores ALPHA in its own separate 8-level ramp.
Height is the channel whose error moves geometry; sway is the channel nobody
can see being 3 levels off. Today they are the wrong way round.

THE EVIDENCE (IMPOSTORFIX2 s6, 24 orbit views x 5 subjects):

    subject    height in blue   height in alpha   gain
    blast_n4       0.4978           0.5362       +0.0383
    blast_n8       0.6639           0.7010       +0.0372
    rock_n4        0.7777           0.7823       +0.0046   NOT PROVEN
    dead_n4        0.5826           0.5870       +0.0044   NOT PROVEN
    maple_n4       0.3609           0.3629       +0.0020   NOT PROVEN

The number that is not the IoU, and the real reason: the height channel's mean
round-trip error falls from 2.3-3.4 levels to 0.06-0.22, worst case from 95
levels to 5. On the rock, whose depthSpan is 5048 world units, that is 1,881
units of worst-case depth error down to 99.

THE COST, stated: sway's own error gets 4-8x worse (1.5 -> 6.9 levels) and
normal X about 1.5 levels worse, because sway now shares the palette. Three of
the five IoU gains are inside the noise and are marked NOT PROVEN. And EVERY
BAKED SHEET IS INVALIDATED.

PICTURES: scratchpad/impostorfix3_20260919/ruling_swap_maple.png
          scratchpad/impostorfix3_20260919/ruling_swap_rock.png
Two rows of 12 azimuths each -- today's channels on top, swapped below -- drawn
with the numpy reference card, so the only thing that differs between the rows
is which BC3 block carries the depth.

WHAT THIS TABLE DOES *NOT* DO, and a yes must pay for it separately:

  1. THE .lodm VERSION BUMP. `root.insert( QStringLiteral( "lodm" ), 1 )`
     occurs FIVE times in src/lodgen.cpp and once in src/lodgenaggregate.cpp,
     so it is not an exact-once anchor and it is not in the table. Without it
     an old sheet is read backwards in silence instead of being refused. Any
     lane that applies this must bump all six and teach the reader to refuse
     version 1 sheets.
  2. `lodgenRepairOctHeight` (src/lodgen.cpp:2622) repairs the BLUE channel,
     and `lodgenDilateFrames` runs over RGB before it. After the swap the
     repair must operate on alpha and must survive the dilation, which is a
     rewrite of that function's body, not a channel rename. IMPOSTORFIX3's
     8-ring fill lives inside it.
  3. A REBAKE OF EVERY CARD SET, and no way to tell a v1 sheet from a v2 one
     by looking at it.

The edits below are the reads and the writes and the words: the parts that are
mechanical. Items 1-3 are the parts that are not.
"""
import sys

ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'

# --- the WRITE: the in-application octahedral bake ---------------------------
A_WRITE = (
    "\t\t\t\t\t\t\t\t\t\tnormal.setPixel( i * tw + x, j * th + y, "
    "qRgba( unp( qRed( pn ) ), unp( qGreen( pn ) ), z, sway ) );\n"
)
B_WRITE = (
    "\t\t\t\t\t\t\t\t\t\t/* HEIGHT IN ALPHA, SWAY IN BLUE (spec 45, format v2).\n"
    "\t\t\t\t\t\t\t\t\t\t * BC3 gives alpha its own 8-level ramp and makes blue\n"
    "\t\t\t\t\t\t\t\t\t\t * share a 4x4 palette with normal X and Y. Height is\n"
    "\t\t\t\t\t\t\t\t\t\t * the channel whose error moves geometry, so height\n"
    "\t\t\t\t\t\t\t\t\t\t * takes the ramp: mean round-trip error 2.3-3.4 levels\n"
    "\t\t\t\t\t\t\t\t\t\t * -> 0.06-0.22, worst 95 -> 5. Sway pays for it (1.5 ->\n"
    "\t\t\t\t\t\t\t\t\t\t * 6.9 levels) and nobody can see sway being wrong. */\n"
    "\t\t\t\t\t\t\t\t\t\tnormal.setPixel( i * tw + x, j * th + y, "
    "qRgba( unp( qRed( pn ) ), unp( qGreen( pn ) ), sway, z ) );\n"
)

# --- the READS: the card shader ---------------------------------------------
A_UNI = "uniform sampler2D NormalSheet;     // _n       : R nX, G nY, B height, A sway\n"
B_UNI = "uniform sampler2D NormalSheet;     // _n       : R nX, G nY, B sway, A height (format v2)\n"

A_PARA = "\t\t\tfloat h = textureLod( NormalSheet, uv, 0.0 ).b;\n"
B_PARA = (
    "\t\t\t/* .a, not .b: height rides BC3's own alpha ramp (spec 45, v2). */\n"
    "\t\t\tfloat h = textureLod( NormalSheet, uv, 0.0 ).a;\n"
)

A_SWAY = "\t\t\tfloat sw = textureLod( NormalSheet, uv, 0.0 ).a;\n"
B_SWAY = "\t\t\tfloat sw = textureLod( NormalSheet, uv, 0.0 ).b;\n"

A_BLEND = (
    "\t\theight     += n.b * wc;\n"
    "\t\tsway       += n.a * wc;\n"
)
B_BLEND = (
    "\t\theight     += n.a * wc;        // BC3 alpha ramp, spec 45 format v2\n"
    "\t\tsway       += n.b * wc;\n"
)

# --- the GATE: the sheet check must move with the format --------------------
A_GATE = "    h = nsh[..., 2] * 255.0\n"
B_GATE = (
    "    h = nsh[..., 3] * 255.0     # ALPHA, spec 45 format v2 (was blue in v1)\n"
)

# --- the WORDS --------------------------------------------------------------
A_SPEC = "| `_n` | BC3 | normal X | normal Y | height | sway weight |\n"
B_SPEC = (
    "| `_n` | BC3 | normal X | normal Y | sway weight | height |\n"
    "\n"
    "The `_n` sheet's B and A were exchanged in format version 2. BC3 gives the\n"
    "alpha channel its own eight-level ramp and makes blue share a four-by-four\n"
    "palette with normal X and normal Y, so the channel whose error moves\n"
    "geometry takes the ramp. Measured round-trip error on the height channel:\n"
    "mean 2.3-3.4 levels -> 0.06-0.22, worst case 95 levels -> 5; sway's own\n"
    "error goes the other way, 1.5 -> 6.9 levels. A reader MUST refuse a `.lodm`\n"
    "that declares version 1, because a v1 sheet read as v2 parallaxes the card\n"
    "by its sway weight.\n"
)

EDITS = [
    ('src/nifskope_ui.cpp', 'replace', A_WRITE, B_WRITE),
    ('res/shaders/impostor_oct.frag', 'replace', A_UNI, B_UNI),
    ('res/shaders/impostor_oct.frag', 'replace', A_PARA, B_PARA),
    ('res/shaders/impostor_oct.frag', 'replace', A_SWAY, B_SWAY),
    ('res/shaders/impostor_oct.frag', 'replace', A_BLEND, B_BLEND),
    ('tests/spells/impostor_sheet_check.py', 'replace', A_GATE, B_GATE),
    ('docs/LODGEN_IMPOSTOR_SPEC.md', 'replace', A_SPEC, B_SPEC),
]


def main():
    apply = '--apply' in sys.argv[1:]
    ok = True
    loaded = {}
    orig = {}
    for path, mode, anchor, text in EDITS:
        p = ROOT + path
        if p not in loaded:
            orig[p] = open(p, 'rb').read()
            loaded[p] = orig[p]
        s = loaded[p].decode('utf-8')
        n = s.count(anchor)
        print('%-38s %-7s anchor x%d  CR %d' % (path, mode, n, orig[p].count(b'\r')))
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
        print('NOT IN THIS TABLE and still owed: the .lodm version bump (6 sites),'
              ' lodgenRepairOctHeight moving to alpha, a rebake of every set.')
        return 0
    for p in loaded:
        if orig[p].count(b'\r') != loaded[p].count(b'\r'):
            print('CR COUNT MOVED in %s -- refusing the whole apply.' % p)
            return 2
    for p, data in loaded.items():
        open(p, 'wb').write(data)
        print('written %s (%d bytes, CR %d)' % (p, len(data), data.count(b'\r')))
    print('REMINDER: the .lodm version bump, lodgenRepairOctHeight and the rebake'
          ' are NOT done by this script.')
    return 0


if __name__ == '__main__':
    sys.exit(main())

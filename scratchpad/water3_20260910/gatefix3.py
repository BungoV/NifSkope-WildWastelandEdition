# -*- coding: utf-8 -*-
"""gatefix3.py -- lane BUILD5b: the defect only the PICTURE saw.

`dock.png` shows the Water Marking dock opening with TWO rows above the map --
"File" and "Show" -- and every other setting (Tool, Speed, Width, Class, Water
form, Colour, Flow, Name and the Bake fold) below a 55-pixel scroll band.  All
nineteen dock checks were green while that was true, because "settings: 6 on 6
distinct rows" is a fact about the LAYOUT and not about what a person sees.
CONSTITUTION rule 5 exactly: counts do not see a ragged column.

The cause is not the panel's structure, which is right: a QScrollArea's own
sizeHint is a fixed ~100x30 whatever widget it holds, so the QSplitter had
nothing to open the settings band at and gave the map everything.  Fixed by
giving the splitter explicit starting sizes -- the settings page's own height,
and the canvas's minimum, which the splitter then grows into the space left.

And the count that would have caught it, with a floor: how many of the six
named settings are inside the scroll area's VISIBLE viewport when the dock
opens.  In the picture this lane started from the answer is 0 of 6.

    python scratchpad/water3_20260910/gatefix3.py --check
    python scratchpad/water3_20260910/gatefix3.py
"""
import hashlib
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
check_only = '--check' in sys.argv

SPLIT_ANCHOR = (
    b'\t\tsplitter->setStretchFactor( 0, 0 );\n'
    b'\t\tsplitter->setStretchFactor( 1 , 1 );\n'
)
SPLIT_NEW = (
    b'\t\tsplitter->setStretchFactor( 0, 0 );\n'
    b'\t\tsplitter->setStretchFactor( 1 , 1 );\n'
    b'\t\t/* AND A STARTING SIZE FOR EACH BAND.  A QScrollArea reports a fixed\n'
    b'\t\t * ~100x30 sizeHint whatever it holds, so without this the splitter\n'
    b'\t\t * had nothing to open the settings on and gave the map everything:\n'
    b'\t\t * the dock opened with two of its eleven rows above the fold, and\n'
    b'\t\t * every self-test count stayed green because they measure the layout\n'
    b'\t\t * and not what is on screen.  The canvas takes the rest, because it\n'
    b'\t\t * is the stretching half. */\n'
    b'\t\tsplitter->setSizes( { page->sizeHint().height() + 12,\n'
    b'\t\t\tcanvas->minimumHeight() } );\n'
)

TEST_ANCHOR = (
    b'\t\tauto * tb = panel->findChild<QWidget *>( QStringLiteral( "WaterMarkToolBox" ) );\n'
    b'\t\tcheck( QStringLiteral( "the settings themselves do scroll" ),\n'
    b'\t\t\tsc && tb && sc->isAncestorOf( tb ) );\n'
    b'\t}\n'
)
TEST_NEW = TEST_ANCHOR + (
    b'\n'
    b'\t/* AND HOW MANY OF THEM THE BAND ACTUALLY SHOWS when the dock opens.\n'
    b'\t * "6 settings on 6 distinct rows" above is true of a panel whose\n'
    b'\t * settings band is 55 pixels tall, which is what the first screenshot\n'
    b'\t * of this dock showed: 0 of these 6 inside the visible viewport, every\n'
    b'\t * count green.  This is that picture as a number, with its floor. */\n'
    b'\t{\n'
    b'\t\tauto * sc = panel->settingsScroll();\n'
    b'\t\tint shown = 0, counted = 0;\n'
    b'\t\tif ( sc && sc->viewport() ) {\n'
    b'\t\t\tconst QRect vp = sc->viewport()->rect();\n'
    b'\t\t\tfor ( const char * n : { "WaterMarkToolBox", "WaterMarkSpeedSpin",\n'
    b'\t\t\t\t\t"WaterMarkWidthSpin", "WaterMarkClassBox", "WaterMarkFormBox",\n'
    b'\t\t\t\t\t"WaterMarkNameEdit" } ) {\n'
    b'\t\t\t\tauto * w = panel->findChild<QWidget *>( QLatin1String( n ) );\n'
    b'\t\t\t\tif ( !w )\n'
    b'\t\t\t\t\tcontinue;\n'
    b'\t\t\t\tcounted++;\n'
    b'\t\t\t\tconst QPoint tl = w->mapTo( sc->viewport(), QPoint( 0, 0 ) );\n'
    b'\t\t\t\tif ( vp.contains( QPoint( tl.x() + 2, tl.y() + 1 ) )\n'
    b'\t\t\t\t\t&& vp.contains( QPoint( tl.x() + 2, tl.y() + w->height() - 1 ) ) )\n'
    b'\t\t\t\t\tshown++;\n'
    b'\t\t\t}\n'
    b'\t\t}\n'
    b'\t\tlog << "settings visible without scrolling: " << shown << " of " << counted << "\\n";\n'
    b'\t\tcheck( QStringLiteral( "the settings band opens showing its settings (%1 of %2, "\n'
    b'\t\t\t"floor 5)" ).arg( shown ).arg( counted ), counted >= 6 && shown >= 5 );\n'
    b'\t}\n'
)

EDITS = [('src/watermarkpanel.cpp', [(SPLIT_ANCHOR, SPLIT_NEW), (TEST_ANCHOR, TEST_NEW)])]

bad = 0
for rel, edits in EDITS:
    path = os.path.join(ROOT, rel)
    with open(path, 'rb') as f:
        data = f.read()
    print('%-26s %s %8d bytes %6d lines CR=%d' % (
        rel, hashlib.sha1(data).hexdigest()[:16], len(data),
        data.count(b'\n'), data.count(b'\r')))
    cr0 = data.count(b'\r')
    hurt = False
    for anchor, new in edits:
        n = data.count(anchor)
        if n != 1:
            print('  REFUSED: %d matches for %r' % (n, anchor[:70]))
            bad += 1
            hurt = True
            continue
        before = len(data)
        data = data.replace(anchor, new, 1)
        print('  ok: %d -> %d bytes  (+%d)' % (before, len(data), len(data) - before))
    if data.count(b'\r') != cr0:
        print('  REFUSED: CR moved %d -> %d' % (cr0, data.count(b'\r')))
        bad += 1
        hurt = True
    if not check_only and not hurt:
        with open(path, 'wb') as f:
            f.write(data)
        print('  written: %d bytes %d lines CR=%d' % (
            len(data), data.count(b'\n'), data.count(b'\r')))

if bad:
    print('REFUSED (%d)' % bad)
    sys.exit(1)
if check_only:
    print('--check: nothing written')
sys.exit(0)

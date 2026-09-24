#!/usr/bin/env python3
"""Splice lane LODUI1's RUN leg and its stock-target grab into WW_LODGEN_TEST.

Two edits, each on an exact-once anchor carrying the file's real line ending
(src/nifskope_ui.cpp is LF-only).  --check writes nothing; --apply refuses
unless both anchors still match once, and asserts the CR count did not move.
"""
import sys, os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
TARGET = os.path.join(ROOT, 'src', 'nifskope_ui.cpp')

RUN_ANCHOR = "\t\t\t\t\t\tconst QByteArray shot = qgetenv( \"WW_LODGEN_SHOT\" );\n"
RUN_MARKER = "LANE LODUI1: THE PANEL ACTUALLY RUNS"

SHOT_ANCHOR = (
    "\t\t\t\t\t\t\tconst bool saved = dock->grab().save( QString::fromLocal8Bit( shot ) );\n"
    "\t\t\t\t\t\t\tlog << \"screenshot \" << ( saved ? \"saved: \" : \"NOT saved: \" ) "
    "<< QString::fromLocal8Bit( shot ) << \"\\n\";\n"
    "\t\t\t\t\t\t}\n"
)
SHOT_TEXT = """\t\t\t\t\t\t/* THE STOCK TARGET'S OWN GRAB (lane LODUI1). The row list
\t\t\t\t\t\t * differs by target now, so one picture cannot show both; this
\t\t\t\t\t\t * switches, grabs, and switches back so the panel is left where
\t\t\t\t\t\t * it was found. */
\t\t\t\t\t\tconst QByteArray shotStock = qgetenv( "WW_LODGEN_SHOT_STOCK" );
\t\t\t\t\t\tif ( !shotStock.isEmpty() && dock ) {
\t\t\t\t\t\t\tif ( auto * tb = findChild<QComboBox *>( QStringLiteral( "LodgenTargetBox" ) ) ) {
\t\t\t\t\t\t\t\tconst int keep = tb->currentIndex();
\t\t\t\t\t\t\t\ttb->setCurrentIndex( 1 );
\t\t\t\t\t\t\t\tresizeDocks( { dock }, { 640 }, Qt::Horizontal );
\t\t\t\t\t\t\t\tQApplication::processEvents();
\t\t\t\t\t\t\t\tQApplication::processEvents();
\t\t\t\t\t\t\t\tconst bool sv = dock->grab().save( QString::fromLocal8Bit( shotStock ) );
\t\t\t\t\t\t\t\tlog << "stock screenshot " << ( sv ? "saved: " : "NOT saved: " )
\t\t\t\t\t\t\t\t\t<< QString::fromLocal8Bit( shotStock ) << "\\n";
\t\t\t\t\t\t\t\ttb->setCurrentIndex( keep );
\t\t\t\t\t\t\t\tQApplication::processEvents();
\t\t\t\t\t\t\t}
\t\t\t\t\t\t}
"""
SHOT_MARKER = "WW_LODGEN_SHOT_STOCK"


def main():
    apply_ = '--apply' in sys.argv
    raw = open(TARGET, 'rb').read()
    text = raw.decode('utf-8')
    run_block = open(os.path.join(HERE, 'runleg_block.cpp'), 'rb').read().decode('utf-8')
    print('target %d bytes, CR %d' % (len(raw), raw.count(b'\r')))
    ok = True
    for name, anchor, marker in (('run leg', RUN_ANCHOR, RUN_MARKER),
                                 ('stock grab', SHOT_ANCHOR, SHOT_MARKER)):
        n = text.count(anchor)
        m = text.count(marker)
        print('%-11s anchor matches %d, marker present %d' % (name, n, m))
        if n != 1 or m != 0:
            ok = False
    if not ok:
        print('REFUSED')
        return 2
    if not apply_:
        print('--check only, nothing written')
        return 0
    out = text.replace(RUN_ANCHOR, run_block + RUN_ANCHOR)
    out = out.replace(SHOT_ANCHOR, SHOT_ANCHOR + SHOT_TEXT)
    data = out.encode('utf-8')
    assert data.count(b'\r') == raw.count(b'\r'), 'CR count moved'
    open(TARGET, 'wb').write(data)
    print('applied: %d -> %d bytes, CR %d' % (len(raw), len(data), data.count(b'\r')))
    return 0


if __name__ == '__main__':
    sys.exit(main())

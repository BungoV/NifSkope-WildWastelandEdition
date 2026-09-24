#!/usr/bin/env python3
"""Lane LODUI1, relink 2: the panel grabs showed none of the rows under test.

The dock grab opens on the settings scrolled to the top and the splitter giving
most of the height to the progress map, so `panel_fo4cs.png` showed Source,
Plugins, Resources and the map -- and not one of the rows this lane changed.
A picture that cannot show the thing it is offered as proof of is not proof
(CONSTITUTION 5).

Both grabs now give the settings pane the height and scroll it to the output
list first, through a lambda stated ONCE so the two cannot drift.
"""
import sys, os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
TARGET = os.path.join(ROOT, 'src', 'nifskope_ui.cpp')
T6 = '\t' * 6
T7 = '\t' * 7
T8 = '\t' * 8

# 1. the helper, immediately before the first shot block
A1 = '\n' + T6 + 'const QByteArray shot = qgetenv( "WW_LODGEN_SHOT" );\n'
R1 = (
    '\n' + T6 + '/* THE DOCK, ARRANGED SO THE GRAB SHOWS THE OUTPUT ROWS (lane\n'
    + T6 + ' * LODUI1). Left alone, the grab opens on the settings scrolled to\n'
    + T6 + ' * the top and the splitter giving the progress map most of the\n'
    + T6 + ' * height, so a picture of the panel showed Source, Plugins and an\n'
    + T6 + ' * empty map and none of the rows under test. Stated once and used by\n'
    + T6 + ' * both grabs. */\n'
    + T6 + 'auto arrangeDockForGrab = [this, dock]() {\n'
    + T6 + '\tif ( !dock )\n'
    + T6 + '\t\treturn;\n'
    + T6 + '\tresizeDocks( { dock }, { 640 }, Qt::Horizontal );\n'
    + T6 + '\tif ( auto * sp = findChild<QSplitter *>( QStringLiteral( "LodgenSplitter" ) ) )\n'
    + T6 + '\t\tsp->setSizes( { 2000, 150 } );\n'
    + T6 + '\tQApplication::processEvents();\n'
    + T6 + '\tif ( auto * sc = findChild<QScrollArea *>( QStringLiteral( "LodgenSettingsScroll" ) ) ) {\n'
    + T6 + '\t\t/* Scroll to the head of the output list -- the Target row -- so\n'
    + T6 + '\t\t * everything the target gates is below it in the frame. */\n'
    + T6 + '\t\tif ( auto * tb = findChild<QComboBox *>( QStringLiteral( "LodgenTargetBox" ) ) )\n'
    + T6 + '\t\t\tsc->ensureWidgetVisible( tb, 0, 400 );\n'
    + T6 + '\t}\n'
    + T6 + '\tQApplication::processEvents();\n'
    + T6 + '\tQApplication::processEvents();\n'
    + T6 + '};\n'
    + T6 + 'const QByteArray shot = qgetenv( "WW_LODGEN_SHOT" );\n'
)

# 2. the FO4CS grab uses it
A2 = (
    '\n' + T7 + 'resizeDocks( { dock }, { 640 }, Qt::Horizontal );\n'
    + T7 + 'QApplication::processEvents();\n'
    + T7 + 'QApplication::processEvents();\n'
)
R2 = '\n' + T7 + 'arrangeDockForGrab();\n'

# 3. and the stock grab
A3 = (
    '\n' + T8 + 'resizeDocks( { dock }, { 640 }, Qt::Horizontal );\n'
    + T8 + 'QApplication::processEvents();\n'
    + T8 + 'QApplication::processEvents();\n'
)
R3 = '\n' + T8 + 'arrangeDockForGrab();\n'

EDITS = [('the helper', A1, R1), ('the FO4CS grab', A2, R2), ('the stock grab', A3, R3)]
MARKER = 'arrangeDockForGrab'


def main():
    apply_ = '--apply' in sys.argv
    raw = open(TARGET, 'rb').read()
    text = raw.decode('utf-8')
    print('target %d bytes, CR %d' % (len(raw), raw.count(b'\r')))
    print('marker present: %d' % text.count(MARKER))
    ok = text.count(MARKER) == 0
    for name, a, _ in EDITS:
        n = text.count(a)
        print('%-16s anchor matches %d' % (name, n))
        if n != 1:
            ok = False
    if not ok:
        print('REFUSED')
        return 2
    if not apply_:
        print('--check only, nothing written')
        return 0
    for _, a, r in EDITS:
        text = text.replace(a, r)
    data = text.encode('utf-8')
    assert data.count(b'\r') == raw.count(b'\r'), 'CR count moved'
    open(TARGET, 'wb').write(data)
    print('applied: %d -> %d bytes, CR %d' % (len(raw), len(data), data.count(b'\r')))
    return 0


if __name__ == '__main__':
    sys.exit(main())

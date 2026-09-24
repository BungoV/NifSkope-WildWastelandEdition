#!/usr/bin/env python3
"""Lane LODUI1, relink 3: the panel grab STILL did not reach the rows under test.

Relink 2 scrolled the settings to the Target row and asked the splitter for more
height.  The splitter refused -- the progress map has a 160 px minimum and the
pane around it another ~140, so the settings never got past about 270 px of a
741 px dock -- and the grab reached the `.lodl` section and stopped.

The fix is not to argue with the splitter: the progress pane is HIDDEN for the
grab and put back afterwards, so the settings get the whole dock, and the scroll
centres on the head the TARGET offers (the native row under FO4CS, the `.bto`
row under the stock engine) so the rows the picture exists to show are in frame
under both.
"""
import sys, os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
TARGET = os.path.join(ROOT, 'src', 'nifskope_ui.cpp')
T6 = '\t' * 6

OLD = (
    T6 + 'auto arrangeDockForGrab = [this, dock]() {\n'
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
)

NEW = (
    T6 + 'QWidget * grabHiddenPane = nullptr;\n'
    + T6 + 'auto arrangeDockForGrab = [this, dock, &grabHiddenPane]() {\n'
    + T6 + '\tif ( !dock )\n'
    + T6 + '\t\treturn;\n'
    + T6 + '\tresizeDocks( { dock }, { 640 }, Qt::Horizontal );\n'
    + T6 + '\t/* THE PROGRESS PANE IS HIDDEN FOR THE GRAB. Asking the splitter for\n'
    + T6 + '\t * the height does not work: the map alone has a 160 px minimum and\n'
    + T6 + '\t * the pane around it another ~140, so the settings never get past\n'
    + T6 + '\t * about 270 px of a 741 px dock and a picture of the panel shows\n'
    + T6 + '\t * four rows. Hidden, the settings get the whole dock. Put back by\n'
    + T6 + '\t * restoreDockAfterGrab() so nothing the harness measures afterwards\n'
    + T6 + '\t * sees a panel with no progress pane. */\n'
    + T6 + '\tif ( auto * sp = findChild<QSplitter *>( QStringLiteral( "LodgenSplitter" ) ) ) {\n'
    + T6 + '\t\tif ( sp->count() > 1 && sp->widget( 1 ) ) {\n'
    + T6 + '\t\t\tgrabHiddenPane = sp->widget( 1 );\n'
    + T6 + '\t\t\tgrabHiddenPane->setVisible( false );\n'
    + T6 + '\t\t}\n'
    + T6 + '\t}\n'
    + T6 + '\tQApplication::processEvents();\n'
    + T6 + '\tif ( auto * sc = findChild<QScrollArea *>( QStringLiteral( "LodgenSettingsScroll" ) ) ) {\n'
    + T6 + '\t\t/* Centre on the object head this target OFFERS -- the native row\n'
    + T6 + '\t\t * under FO4CS, the .bto row under the stock engine -- so the rows\n'
    + T6 + '\t\t * the picture exists to show are in frame under both. */\n'
    + T6 + '\t\tQWidget * anchor = findChild<QCheckBox *>( QStringLiteral( "LodgenNativeCheck" ) );\n'
    + T6 + '\t\tif ( !anchor || anchor->isHidden() )\n'
    + T6 + '\t\t\tanchor = findChild<QCheckBox *>( QStringLiteral( "LodgenObjectsCheck" ) );\n'
    + T6 + '\t\tif ( !anchor || anchor->isHidden() )\n'
    + T6 + '\t\t\tanchor = findChild<QComboBox *>( QStringLiteral( "LodgenTargetBox" ) );\n'
    + T6 + '\t\tif ( anchor )\n'
    + T6 + '\t\t\tsc->ensureWidgetVisible( anchor, 0, 300 );\n'
    + T6 + '\t}\n'
    + T6 + '\tQApplication::processEvents();\n'
    + T6 + '\tQApplication::processEvents();\n'
    + T6 + '};\n'
    + T6 + 'auto restoreDockAfterGrab = [&grabHiddenPane]() {\n'
    + T6 + '\tif ( grabHiddenPane ) {\n'
    + T6 + '\t\tgrabHiddenPane->setVisible( true );\n'
    + T6 + '\t\tgrabHiddenPane = nullptr;\n'
    + T6 + '\t\tQApplication::processEvents();\n'
    + T6 + '\t}\n'
    + T6 + '};\n'
)

T7 = '\t' * 7
A2 = (
    '\n' + T7 + 'const bool saved = dock->grab().save( QString::fromLocal8Bit( shot ) );\n'
)
R2 = (
    '\n' + T7 + 'const bool saved = dock->grab().save( QString::fromLocal8Bit( shot ) );\n'
    + T7 + 'restoreDockAfterGrab();\n'
)
T8 = '\t' * 8
A3 = (
    '\n' + T8 + 'const bool sv = dock->grab().save( QString::fromLocal8Bit( shotStock ) );\n'
)
R3 = (
    '\n' + T8 + 'const bool sv = dock->grab().save( QString::fromLocal8Bit( shotStock ) );\n'
    + T8 + 'restoreDockAfterGrab();\n'
)

EDITS = [('the lambda', OLD, NEW), ('the FO4CS restore', A2, R2), ('the stock restore', A3, R3)]
MARKER = 'restoreDockAfterGrab'


def main():
    apply_ = '--apply' in sys.argv
    raw = open(TARGET, 'rb').read()
    text = raw.decode('utf-8')
    print('target %d bytes, CR %d' % (len(raw), raw.count(b'\r')))
    print('marker present: %d' % text.count(MARKER))
    ok = text.count(MARKER) == 0
    for name, a, _ in EDITS:
        n = text.count(a)
        print('%-18s anchor matches %d' % (name, n))
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

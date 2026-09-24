#!/usr/bin/env python3
"""Lane LODUI1, relink 1: the two checks the first gate run turned red.

Both are harness defects, not panel defects, and both are the SAME defect in
two places -- a check whose premise this lane changed:

 1. "the chunk range is greyed while no chunk output is selected" opened on a
    panel where the FO4CS object head (the native row) is ticked by default, so
    the range was legitimately live and the check measured the default instead
    of the rule.  Re-aimed: untick every head first, and drive the head the
    TARGET offers rather than the .bto one by name.
 2. "the FO4CS summary names .lodl, .lodt, .lodo and .lodi" read the summary
    with the terrain pyramid unticked, so `.lodt` was correctly absent.  The
    pyramid is ticked for the reading and put back.

Exact-once anchors carrying the file's real line ending (LF) and a leading
newline so an indentation prefix cannot match by accident; --check writes
nothing; the CR count is asserted unchanged.
"""
import sys, os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
TARGET = os.path.join(ROOT, 'src', 'nifskope_ui.cpp')
T6 = '\t' * 6
T8 = '\t' * 8

A1 = (
    '\n' + T6 + 'check( "the chunk range is greyed while no chunk output is selected",\n'
    + T6 + '\twhole && !whole->isEnabled() );\n'
    + T6 + 'if ( objects )\n'
    + T6 + '\tobjects->setChecked( true );\n'
    + T6 + 'QApplication::processEvents();\n'
    + T6 + 'check( "ticking Object LOD chunks enables the chunk range", whole && whole->isEnabled() );\n'
)
R1 = (
    '\n' + T6 + '/* RE-AIMED by lane LODUI1 (2026-09-11). The object pass has two heads\n'
    + T6 + ' * now and the FO4CS one is ticked by default, so "no chunk output is\n'
    + T6 + ' * selected" is no longer the state the panel opens in -- the check was\n'
    + T6 + ' * reading the default rather than the rule. Every head is unticked\n'
    + T6 + ' * first, and the head the TARGET offers is the one driven, so a target\n'
    + T6 + ' * that offered neither would fail here. */\n'
    + T6 + '{\n'
    + T6 + '\tauto * nativeHead = findChild<QCheckBox *>( QStringLiteral( "LodgenNativeCheck" ) );\n'
    + T6 + '\tauto * btrHead = findChild<QCheckBox *>( QStringLiteral( "LodgenBtrCheck" ) );\n'
    + T6 + '\tQCheckBox * head = ( objects && !objects->isHidden() ) ? objects\n'
    + T6 + '\t\t: ( ( nativeHead && !nativeHead->isHidden() ) ? nativeHead : objects );\n'
    + T6 + '\tfor ( QCheckBox * c : { objects, nativeHead, btrHead } )\n'
    + T6 + '\t\tif ( c )\n'
    + T6 + '\t\t\tc->setChecked( false );\n'
    + T6 + '\tQApplication::processEvents();\n'
    + T6 + '\tcheck( "the chunk range is greyed while no chunk output is selected",\n'
    + T6 + '\t\twhole && !whole->isEnabled() );\n'
    + T6 + '\tif ( head )\n'
    + T6 + '\t\thead->setChecked( true );\n'
    + T6 + '\tQApplication::processEvents();\n'
    + T6 + '\tlog << "  the object head this target offers: "\n'
    + T6 + '\t\t<< ( head ? head->text() : QStringLiteral( "none" ) ) << "\\n";\n'
    + T6 + '\tcheck( "ticking the target\'s own object output enables the chunk range",\n'
    + T6 + '\t\thead && whole && whole->isEnabled() );\n'
    + T6 + '\t// the rows below are the .bto section\'s, so its head goes back on\n'
    + T6 + '\tif ( objects )\n'
    + T6 + '\t\tobjects->setChecked( true );\n'
    + T6 + '\tQApplication::processEvents();\n'
    + T6 + '}\n'
)

A2 = '\n' + T8 + 'const QString sumCs = sumL->text();\n'
R2 = (
    '\n' + T8 + '/* The pyramid is ticked for this reading. It is an OUTPUT, off by\n'
    + T8 + ' * default, and the summary names only what the run will write -- so\n'
    + T8 + ' * asking the default panel for all five extensions asks it to name a\n'
    + T8 + ' * file nobody ticked. Put back straight after. */\n'
    + T8 + 'auto * vtHead = findChild<QCheckBox *>( QStringLiteral( "LodgenVtCheck" ) );\n'
    + T8 + 'const bool keepVt = vtHead && vtHead->isChecked();\n'
    + T8 + 'if ( vtHead )\n'
    + T8 + '\tvtHead->setChecked( true );\n'
    + T8 + 'QApplication::processEvents();\n'
    + T8 + 'const QString sumCs = sumL->text();\n'
)

A3 = (
    '\n' + T8 + '// ---- the stock engine: the reverse, row for row ----\n'
    + T8 + 'target->setCurrentIndex( 1 );\n'
)
R3 = (
    '\n' + T8 + 'if ( vtHead )\n'
    + T8 + '\tvtHead->setChecked( keepVt );\n'
    + T8 + 'QApplication::processEvents();\n'
    + T8 + '// ---- the stock engine: the reverse, row for row ----\n'
    + T8 + 'target->setCurrentIndex( 1 );\n'
)

EDITS = [('the range check', A1, R1), ('the pyramid tick', A2, R2), ('the pyramid restore', A3, R3)]
MARKER = 'RE-AIMED by lane LODUI1'


def main():
    apply_ = '--apply' in sys.argv
    raw = open(TARGET, 'rb').read()
    text = raw.decode('utf-8')
    print('target %d bytes, CR %d' % (len(raw), raw.count(b'\r')))
    print('marker present: %d' % text.count(MARKER))
    ok = text.count(MARKER) == 0
    for name, a, _ in EDITS:
        n = text.count(a)
        print('%-20s anchor matches %d' % (name, n))
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

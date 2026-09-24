"""DEFAULTS1 item 4, the GUI half.

The panel self-test's style check "a ticked box is a white mark on Blender's
blue" borrowed the identity box's TICKED state from the Target rows above it.
Since 2026-09-12 the box is unticked by default and the FO4CS target no longer
ticks it, so the check measured an UNTICKED box and failed. A harness forces the
state it measures (feedback_harness_isolate_settings): it ticks the box itself,
grabs, and puts it back.

Written as a FILE, not a heredoc: this file carries backslashes and a heredoc
halves them -- which it did, once more, on the first attempt at this very patch.

Refusing patch: every anchor exactly once or nothing is written.
"""
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/src/nifskope_ui.cpp'
T = '\t' * 6

OLD_HEAD = (
    T + 'auto * ident = findChild<QCheckBox *>( QStringLiteral( "LodgenIdentityCheck" ) );\n'
    + T + 'int whitePx = 0, bluePx = 0;\n'
    + T + 'if ( ident && pageW && ident->isChecked() ) {\n'
)
NEW_HEAD = (
    T + "/* The identity box is UNTICKED by default since 2026-09-12 (bungo's\n"
    + T + ' * ruling, lane DEFAULTS1) and the FO4CS target no longer ticks it, so\n'
    + T + ' * this check can no longer BORROW a ticked box from the rows above:\n'
    + T + ' * it ticks the box itself, grabs, and puts the box back the way it\n'
    + T + ' * found it. A harness forces the state it measures. */\n'
    + T + 'auto * ident = findChild<QCheckBox *>( QStringLiteral( "LodgenIdentityCheck" ) );\n'
    + T + 'int whitePx = 0, bluePx = 0;\n'
    + T + 'const bool identWasTicked = ident && ident->isChecked();\n'
    + T + 'if ( ident && !identWasTicked ) {\n'
    + T + '\tident->setChecked( true );\n'
    + T + '\tQApplication::processEvents();\n'
    + T + '}\n'
    + T + 'if ( ident && pageW && ident->isChecked() ) {\n'
)

# Anchored on the closing brace and the check line only -- no C++ escape
# sequence is quoted here, so nothing in this anchor can be mangled in transit.
CHECKLINE = (T + 'check( "a ticked box is a white mark on Blender\'s blue", '
             'whitePx >= 4 && bluePx >= 20 );\n')
OLD_TAIL = T + '}\n' + CHECKLINE
NEW_TAIL = (
    T + '}\n'
    + T + 'if ( ident && !identWasTicked ) {\n'
    + T + '\tident->setChecked( false );\n'
    + T + '\tQApplication::processEvents();\n'
    + T + '}\n'
    + CHECKLINE
)

raw = open(P, 'rb').read()
print('before  %d bytes  LF %d  CR %d' % (len(raw), raw.count(b'\n'), raw.count(b'\r')))
text = raw.decode('utf-8')
for name, old in (('head', OLD_HEAD), ('tail', OLD_TAIL)):
    c = text.count(old)
    if c != 1:
        print('REFUSED: %s anchor found %d times:\n%r' % (name, c, old))
        sys.exit(2)
text = text.replace(OLD_HEAD, NEW_HEAD).replace(OLD_TAIL, NEW_TAIL)
if '--check' in sys.argv:
    print('--check: both anchors found exactly once, nothing written')
    sys.exit(0)
out = text.encode('utf-8')
open(P, 'wb').write(out)
back = open(P, 'rb').read()
assert back == out and back.count(b'\r') == 0
print('after   %d bytes  LF %d  CR %d' % (len(back), back.count(b'\n'), back.count(b'\r')))
print('ok')

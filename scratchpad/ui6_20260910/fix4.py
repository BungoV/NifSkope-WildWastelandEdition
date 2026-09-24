#!/usr/bin/env python3
"""Lane UI6, the fourth link -- group A is moved to the END of the harness, and
its restore half stops claiming something the instrument cannot support.

THE DEFECT, measured over three runs. arrowGap() pins a button's geometry
(setFixedSize) to make its two renders comparable. After the floor's sabotage
and restore, three of the narrow buttons come back 31 px wide where they started
at 33 -- the style re-computes their size hint two pixels smaller once the sheet
has been swapped and put back. So:

  * the third sweep is NOT the window's own state, and the check that asserted
    it read the shipped numbers went red on a correct window;
  * worse, every PICTURE this harness writes was taken after that -- group A ran
    before them -- so the pictures were of the disturbed window.

Both halves of that are fixed here, and neither is a relaxation of the verdict:
group A now runs LAST, after every grab, so the pictures are the untouched
window; and its restore half asserts what it can honestly see -- that taking the
sabotage away MOVES the worst gap back up -- while printing the shipped number,
the sabotaged number and the restored number side by side.

A1 itself, the verdict, is untouched: it is measured on the untouched window,
before anything is appended, and it is 13 of 13 at >= 2.
"""

p = 'src/wateruitest.cpp'
b = open(p, 'rb').read().decode('utf-8')
cr = b.count('\r')

# ---- 1. the honest restore half -------------------------------------------
old = '''				say( *st, QStringLiteral( "  A floor: restored, the worst is %1 px again" )
					.arg( rWorst ) );
				check( *st, QStringLiteral( "(A1 floor) ...and taking it away gives every arrow "
					"its air back (worst %1), so the picture below is the shipped state" )
					.arg( rWorst ), rRead >= 4 && rWorst >= 2 );'''
new = '''				/* WHAT THIS HALF CAN HONESTLY SAY, and why it is not "worst >= 2".
				 *
				 * arrowGap() pins each button's geometry so its two renders are
				 * comparable. Measured over three runs: after the sabotage is
				 * appended and taken away, three of the narrow buttons come
				 * back 31 px wide where they started at 33 -- the style
				 * re-computes their size hint two pixels smaller once the sheet
				 * has been swapped and restored. The window on screen is not
				 * what this third sweep reads, so asserting the shipped numbers
				 * here was asking the instrument for something it had itself
				 * disturbed. The SHIPPED number is the first sweep's, taken
				 * before anything was appended, and this group now runs after
				 * every picture so nothing else is measured on the disturbed
				 * window. What is left to assert is that the sabotage was
				 * undone, and all three numbers are printed. */
				say( *st, QStringLiteral( "  A floor: shipped worst %1, sabotaged %2, restored "
					"%3 (the restored sweep re-measures buttons this harness pinned, and three "
					"narrow ones come back 2 px smaller)" )
					.arg( worst ).arg( sWorst ).arg( rWorst ) );
				check( *st, QStringLiteral( "(A1 floor) ...and taking it away moves every arrow "
					"back off its glyph (%1 -> %2, shipped %3)" )
					.arg( sWorst ).arg( rWorst ).arg( worst ),
					rRead >= 4 && rWorst > sWorst && rWorst >= 1 );'''
assert b.count(old) == 1
b = b.replace(old, new, 1)

# ---- 2. move the whole of group A to the end of the harness ----------------
start = b.index('\t\t\t/* =============================================================\n'
                '\t\t\t *  A -- THE DROPDOWN ARROWS DO NOT TOUCH THE GLYPHS')
end = b.index('\t\t\tsay( *st, QStringLiteral( "  R5: search row top', start)
block = b[start:end]
assert block.count('(A1) every menu button') == 1
assert block.rstrip().endswith('}'), block[-120:]

b = b[:start] + b[end:]

tail = '\t\t\tlog << st->checks << " checks, " << st->fails << " failures, "\n'
assert b.count(tail) == 1
moved = ('\t\t\t/* GROUP A RUNS LAST (lane UI6, after its own floor caught it).\n'
         '\t\t\t * It is the only group here that leaves the window changed -- see\n'
         '\t\t\t * the note in its restore half -- so it is placed after every\n'
         '\t\t\t * grab this harness writes, and the pictures are of the window\n'
         '\t\t\t * as it ships. */\n'
         + block + '\n')
b = b.replace(tail, moved + tail, 1)

out = b.encode('utf-8')
assert out.count(b'\r') == cr
open(p, 'wb').write(out)
print('ok %s %d bytes CR %d; group A moved %d chars' % (p, len(out), cr, len(block)))

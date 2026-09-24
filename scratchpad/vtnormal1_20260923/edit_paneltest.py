"""VTNORMAL1: the pyramid-summary check moved the density row to index 1,
which IS the new default (16 u), so both sentences matched. Move it to a
different entry than the current one, and name the densities in the log."""
import sys
P = 'E:/Projects/NifskopeWildWastelandEdition/src/nifskope_ui.cpp'
b = open(P, 'rb').read()
crlf = b.count(b'\r')
s = b.decode('utf-8')
nl = '\r\n' if '\t\t\t\t\t\t\t\tconst QString sum2 = vtSum->text();\r\n' in s else '\n'
old = nl.join([
	"\t\t\t\t\t\t\t\tconst QString sum2 = vtSum->text();",
	"\t\t\t\t\t\t\t\tvtFin->setCurrentIndex( 1 );\t\t// 16 units a texel",
	"\t\t\t\t\t\t\t\tQApplication::processEvents();",
	"\t\t\t\t\t\t\t\tconst QString sum1 = vtSum->text();",
	"\t\t\t\t\t\t\t\tvtFin->setCurrentIndex( 0 );",
	"\t\t\t\t\t\t\t\tQApplication::processEvents();",
	"\t\t\t\t\t\t\t\tlog << \"pyramid summary at 2 cells a tile: \" << sum2 << \"\\n\";",
	"\t\t\t\t\t\t\t\tlog << \"pyramid summary at 1 cell a tile:  \" << sum1 << \"\\n\";",
])
new = nl.join([
	"\t\t\t\t\t\t\t\tconst QString sum2 = vtSum->text();",
	"\t\t\t\t\t\t\t\t// another density than the one showing (the default is 16 u,",
	"\t\t\t\t\t\t\t\t// the middle entry since lane VTNORMAL1), then back",
	"\t\t\t\t\t\t\t\tconst int vtWas = vtFin->currentIndex();",
	"\t\t\t\t\t\t\t\tconst QString den2 = vtFin->currentText();",
	"\t\t\t\t\t\t\t\tvtFin->setCurrentIndex( ( vtWas + 1 ) % qMax( 1, vtFin->count() ) );",
	"\t\t\t\t\t\t\t\tQApplication::processEvents();",
	"\t\t\t\t\t\t\t\tconst QString sum1 = vtSum->text();",
	"\t\t\t\t\t\t\t\tconst QString den1 = vtFin->currentText();",
	"\t\t\t\t\t\t\t\tvtFin->setCurrentIndex( vtWas );",
	"\t\t\t\t\t\t\t\tQApplication::processEvents();",
	"\t\t\t\t\t\t\t\tlog << \"pyramid summary at \" << den2 << \": \" << sum2 << \"\\n\";",
	"\t\t\t\t\t\t\t\tlog << \"pyramid summary at \" << den1 << \": \" << sum1 << \"\\n\";",
])
n = s.count(old)
if n != 1:
	sys.exit('anchor count %d (newline %r)' % (n, nl))
s = s.replace(old, new)
out = s.encode('utf-8')
added = new.count(nl) - old.count(nl)
exp = crlf + (added if nl == '\r\n' else 0)
assert out.count(b'\r') == exp, (out.count(b'\r'), exp)
open(P, 'wb').write(out)
print('paneltest ok, newline', repr(nl), 'CR', crlf, '->', out.count(b'\r'))

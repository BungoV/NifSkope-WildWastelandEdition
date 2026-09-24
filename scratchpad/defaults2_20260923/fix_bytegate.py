"""DEFAULTS2 re-rung of the panel byte gate (WW_LODGEN_GATE, src/nifskope_ui.cpp).

The row "blendMargin (with blendEdges on)" turned its dependency on by BUMPING it, and a bumped
combo flips index 0 <-> 1. With the edge blend now defaulting to 1 (quadrant), the bump turned
the blend OFF, so the margin was measured with nothing to reach and read "SAME bytes". The row
now names the value it needs (depVal "1"), and a dependency that is ALREADY at its named value
counts as turned on (before, putText returned false on no change and the row was skipped).
No other row of the table sets a depVal (58 rows, measured), so the second change reaches only
this row."""
import sys
P = 'E:/Projects/NifskopeWildWastelandEdition/src/nifskope_ui.cpp'
CHECK = '--check' in sys.argv
b = open(P, 'rb').read()
s = b.decode('utf-8')
cr = b.count(b'\r')

edits = [
    ('\t\t\t\t\t\t\t{ "LodgenBlendMarginSpin", "blendMargin", "LodgenBlendEdgesBox", nullptr, nullptr, nullptr, nullptr, nullptr, "128.0" },\n',
     '\t\t\t\t\t\t\t/* depVal "1": the edge blend is ON by default since 2026-09-23 (lane DEFAULTS2), and a\n'
     '\t\t\t\t\t\t\t * bumped combo flips 1 -> 0, which measured the margin with the blend OFF. */\n'
     '\t\t\t\t\t\t\t{ "LodgenBlendMarginSpin", "blendMargin", "LodgenBlendEdgesBox", nullptr, "1", nullptr, nullptr, nullptr, "128.0" },\n'),
    ('\t\t\t\t\t\t\t\t\tconst bool depMoved = wwNewRows[i].depVal\n'
     '\t\t\t\t\t\t\t\t\t\t? putText( dw, wwNewRows[i].depVal ) : bumpRow( dw );\n',
     '\t\t\t\t\t\t\t\t\t/* A dependency ALREADY at its named value is on, not stuck (lane\n'
     '\t\t\t\t\t\t\t\t\t * DEFAULTS2, 2026-09-23: the edge blend became a default-on dependency). */\n'
     '\t\t\t\t\t\t\t\t\tauto atText = []( QWidget * w, const char * text ) -> bool {\n'
     '\t\t\t\t\t\t\t\t\t\tif ( auto * bb = qobject_cast<QComboBox *>( w ) )\n'
     '\t\t\t\t\t\t\t\t\t\t\treturn bb->currentIndex() >= 0\n'
     '\t\t\t\t\t\t\t\t\t\t\t\t&& bb->currentIndex() == bb->findData( QString::fromLatin1( text ).toInt() );\n'
     '\t\t\t\t\t\t\t\t\t\tif ( auto * cc = qobject_cast<QCheckBox *>( w ) )\n'
     '\t\t\t\t\t\t\t\t\t\t\treturn cc->isChecked() == ( QString::fromLatin1( text ).toInt() != 0 );\n'
     '\t\t\t\t\t\t\t\t\t\treturn false;\n'
     '\t\t\t\t\t\t\t\t\t};\n'
     '\t\t\t\t\t\t\t\t\tconst bool depMoved = wwNewRows[i].depVal\n'
     '\t\t\t\t\t\t\t\t\t\t? ( putText( dw, wwNewRows[i].depVal ) || atText( dw, wwNewRows[i].depVal ) )\n'
     '\t\t\t\t\t\t\t\t\t\t: bumpRow( dw );\n'),
]
for old, new in edits:
    n = s.count(old)
    assert n == 1, (n, old[:80])
    s = s.replace(old, new)
nb = s.encode('utf-8')
assert nb.count(b'\r') == cr, 'CR count moved'
if not CHECK:
    open(P, 'wb').write(nb)
print('checked' if CHECK else 'patched', P, 'CR', cr)

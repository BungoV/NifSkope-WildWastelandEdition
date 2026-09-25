# CARDFIX1 step 6 found a STEP-5 defect: a ring card set's array group key is "legacy|WxH|ring", and the
# array's file stem took everything after the first '|' -- "WxH|ring" -- so the file name carried a '|',
# which Windows refuses: "card arrays: could not write ...LodgenCards.legacy.2304x256|ring_d.DDS"
# (gates/impostor_wind.run2.out, G3's array row). No gate had put a ring set through --arrays.
# The stem is now built from the group's own fields: WxH, plus ".ring" for a ring group. A grid group's
# stem is unchanged to the byte (lodgen_card_arrays.sh is the gate). The mesh texture arrays' identical
# line (~5388, `ArrayClass & cls`) is not touched. LF-only file.
P = 'E:/Projects/NifskopeWWE-cardfix1/src/lodgen.cpp'
b = open(P, 'rb').read(); assert b.count(b'\r') == 0
s = b.decode('utf-8')
old = ("\t\tGroup & g = it.value();\n"
       "\t\tconst QString sizeKey = it.key().mid( it.key().indexOf( QChar( '|' ) ) + 1 );\n")
assert s.count(old) == 1, s.count(old)
new = ("\t\tGroup & g = it.value();\n"
       "\t\t/* The size in the FILE NAME: WxH, and \".ring\" for a horizon-ring group. Never the\n"
       "\t\t * group key's tail -- that is joined with '|', which no Windows file name may hold. */\n"
       "\t\tconst QString sizeKey = QStringLiteral( \"%1x%2\" ).arg( g.w ).arg( g.h )\n"
       "\t\t\t+ ( g.ring ? QStringLiteral( \".ring\" ) : QString() );\n")
s = s.replace(old, new)
out = s.encode('utf-8'); assert out.count(b'\r') == 0
open(P, 'wb').write(out); print('patched')

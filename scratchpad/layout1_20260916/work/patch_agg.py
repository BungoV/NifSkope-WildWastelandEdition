import sys
B = chr(92)
p = 'E:/Projects/NifskopeWildWastelandEdition/src/lodgenaggregate.cpp'
s = open(p, encoding='utf-8', newline='').read()

a = ('\treturn QStringLiteral( "Data' + B + B + 'Textures' + B + B + 'Lodgen' + B + B
     + 'Aggregate' + B + B + '" ) + worldspace\n'
     '\t\t+ QStringLiteral( "' + B + B + '" ) + QString::number( cellX ) + QStringLiteral( "_" )')

b = ('\t/* ONE ROOT (lane LAYOUT1, 2026-09-16, bungo 19:3x). The aggregate sheets\n'
     '\t * are a FO4CS-target output, so they live with the rest of them:\n'
     '\t * Data' + B + 'FO4CSLOD' + B + '<ws>' + B + 'Aggregate' + B + '. ONE function composes that\n'
     '\t * prefix (src/lodgenlayout.cpp) and this is the only place the game\n'
     '\t * string for a set is made. */\n'
     '\treturn QStringLiteral( "Data' + B + B + '" ) + lodgenFo4csGameWorldPath( worldspace )\n'
     '\t\t+ QStringLiteral( "' + B + B + 'Aggregate' + B + B + '" ) + QString::number( cellX ) + QStringLiteral( "_" )')

if s.count(a) != 1:
    print('MISS %d' % s.count(a))
    sys.exit(1)
s = s.replace(a, b)

inc = '#include "lodgenaggregate.h"'
if inc not in s:
    print('no include anchor')
    sys.exit(1)
if 'lodgenlayout.h' not in s:
    s = s.replace(inc, inc + '\n#include "lodgenlayout.h"', 1)

open(p, 'w', encoding='utf-8', newline='').write(s)
d = open(p, 'rb').read()
print('ok  CR', d.count(b'\r'), 'LF', d.count(b'\n'))

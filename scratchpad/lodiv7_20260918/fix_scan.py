p = 'src/nativeemit.cpp'
lines = open(p, encoding='utf-8', newline='').read().split('\n')
# find the regex line by a stable substring
idx = [i for i, l in enumerate(lines) if 'QStringView( p.model ).split(' in l]
assert len(idx) == 1, idx
i = idx[0]
# the for + the 4 lines of its body
assert 'arch = true;' in lines[i + 1] or 'arch = true;' in lines[i + 2], lines[i:i + 5]
new = [
    "\t\t\tint from = 0;",
    "\t\t\twhile ( from <= p.model.size() ) {",
    "\t\t\t\tint sep = from;",
    "\t\t\t\twhile ( sep < p.model.size() && p.model[sep] != QLatin1Char( '\\\\' ) && p.model[sep] != QLatin1Char( '/' ) )",
    "\t\t\t\t\tsep++;",
    "\t\t\t\tif ( QStringView( p.model ).mid( from, sep - from ).compare( QLatin1String( \"architecture\" ), Qt::CaseInsensitive ) == 0 ) {",
    "\t\t\t\t\tarch = true;",
    "\t\t\t\t\tbreak;",
    "\t\t\t\t}",
    "\t\t\t\tfrom = sep + 1;",
    "\t\t\t}",
]
lines[i:i + 5] = new
open(p, 'w', encoding='utf-8', newline='').write('\n'.join(lines))
print('scan replaced')

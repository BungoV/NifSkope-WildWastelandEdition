"""FLAT1 part 6: the report names the plugin of every placement with its own painted count, per row and in a
header line (the mod gate reads it: which plugin's placements were examined and which were painted). Also: the two
table string literals carried TAB characters (a patch-script escape); they are written as escapes here."""
import sys

P = 'E:/Projects/NifskopeWWE-flat1/src/lodgen.cpp'
s = open(P, newline='').read()
T = chr(9)
BS = chr(92)
NL = chr(10)


def rep(a, b):
    global s
    if s.count(a) != 1:
        sys.exit('anchor count %d: %r' % (s.count(a), a[:80]))
    s = s.replace(a, b)


rep('''		QSet<QString> plugins;
''', '''		//! plugin of the placing record -> (placements, painted)
		QMap<QString, QPair<int, int>> plugins;
''')
rep('''		w.plugins.insert( r.plugin.isEmpty() ? QStringLiteral( "?" ) : r.plugin );
''', '''		QPair<int, int> & pp = w.plugins[r.plugin.isEmpty() ? QStringLiteral( "?" ) : r.plugin];
		pp.first++;
		pp.second += r.painted ? 1 : 0;
''')
rep('''	out << QString( "# Placements examined %1, painted %2, decided by an override %3, models %4, texels written %5,"''',
    '''	QMap<QString, QPair<int, int>> byPlugin;
	for ( const LodgenFlatRec & r : flatRecs ) {
		QPair<int, int> & pp = byPlugin[r.plugin.isEmpty() ? QStringLiteral( "?" ) : r.plugin];
		pp.first++;
		pp.second += r.painted ? 1 : 0;
	}
	QStringList pls;
	for ( auto it = byPlugin.constBegin(); it != byPlugin.constEnd(); ++it )
		pls << QString( "%1 %2 examined / %3 painted" ).arg( it.key() ).arg( it->first ).arg( it->second );
	out << QStringLiteral( "# By the plugin of the placing record: " ) + pls.join( QStringLiteral( "; " ) );
	out << QString( "# Placements examined %1, painted %2, decided by an override %3, models %4, texels written %5,"''')
rep('''		QStringList pl = w->plugins.values();
		pl.sort( Qt::CaseInsensitive );
''', '''		QStringList pl;
		for ( auto it = w->plugins.constBegin(); it != w->plugins.constEnd(); ++it )
			pl << ( w->plugins.size() == 1 ? it.key()
				: QString( "%1 %2/%3" ).arg( it.key() ).arg( it->second ).arg( it->first ) );
''')
rep(T + 'out << QStringLiteral( "model' + T + 'plugin' + T + 'placements',
    T + 'out << QStringLiteral( "# plugin: the placing records' + chr(39)
    + ' plugins; with more than one, each as painted/placements" );' + NL
    + T + 'out << QStringLiteral( "model' + T + 'plugin' + T + 'placements')
lines = s.split(NL)
fixed = 0
for i, l in enumerate(lines):
    if ('QStringLiteral( "model' + T) in l or ('QString( "%1' + T + '%2') in l:
        head = len(l) - len(l.lstrip(T))
        lines[i] = l[:head] + l[head:].replace(T, BS + 't')
        fixed += 1
if fixed != 2:
    sys.exit('tab literals found %d, expected 2' % fixed)
s = NL.join(lines)
open(P, 'w', newline='').write(s)
print('patched 6')

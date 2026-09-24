import sys
B = chr(92)
p = 'E:/Projects/NifskopeWildWastelandEdition/src/nifcli.cpp'
s = open(p, encoding='utf-8', newline='').read()

game = lambda stem: ('QString( "data' + B + B + 'Textures' + B + B + 'Terrain' + B + B
                     + '%1' + B + B + 'Objects' + B + B + '%1.' + stem + '" ).arg( ws ),')

subs = [
("""			const QString atlasDir = ( texDir.isEmpty() ? outDir : texDir ) + QStringLiteral( "/Objects" );
			QDir().mkpath( atlasDir );""",
 """			const QString atlasDir = objectsDir();
			QDir().mkpath( atlasDir );"""),

("""				""" + game('LodgenObjects') + """
				looseRoot, atlasBc1, &aerr ) ) {""",
 """				objectsGame( QStringLiteral( "LodgenObjects" ) ),
				looseRoot, atlasBc1, &aerr ) ) {"""),

("""				out() << "atlas written: " << ws << ".LodgenObjects.DDS (+_n, _s)" << Qt::endl;""",
 """				out() << "atlas written: " << ws << ".LodgenObjects.DDS (+_n, _s)" << Qt::endl;
				if ( fo4csTarget )
					lodgenNoteLayoutDir( atlasDir );"""),

("""			const QString arrDir = ( texDir.isEmpty() ? outDir : texDir ) + QStringLiteral( "/Objects" );
			QDir().mkpath( arrDir );
			QString rep, cerr2;""",
 """			const QString arrDir = objectsDir();
			QDir().mkpath( arrDir );
			QString rep, cerr2;"""),

("""			if ( lodgenBuildCardArrays( writtenBto, impostors,
				arrDir + "/" + ws + QStringLiteral( ".LodgenCards" ),
				""" + game('LodgenCards') + """
				cardAuxDiv, &rep, &cerr2 ) )
				out() << "card arrays written: " << rep << Qt::endl;
			else
				err() << "card arrays: " << cerr2 << Qt::endl;""",
 """			if ( lodgenBuildCardArrays( writtenBto, impostors,
				arrDir + "/" + ws + QStringLiteral( ".LodgenCards" ),
				objectsGame( QStringLiteral( "LodgenCards" ) ),
				cardAuxDiv, &rep, &cerr2 ) ) {
				out() << "card arrays written: " << rep << Qt::endl;
				if ( fo4csTarget )
					lodgenNoteLayoutDir( arrDir );
			} else {
				err() << "card arrays: " << cerr2 << Qt::endl;
			}"""),
]

for a, b in subs:
    n = s.count(a)
    if n != 1:
        print('MISS %d for: %r' % (n, a[:100]))
        sys.exit(1)
    s = s.replace(a, b)

open(p, 'w', encoding='utf-8', newline='').write(s)
d = open(p, 'rb').read()
print('ok  CR', d.count(b'\r'), 'LF', d.count(b'\n'))

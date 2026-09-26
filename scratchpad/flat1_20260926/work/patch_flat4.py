import sys
P = 'E:/Projects/NifskopeWWE-flat1/src/lodgen.cpp'
s = open(P, newline='').read()
a = '''		r << QString( "roadSwappedPlacements %1" ).arg( roadCensus.swappedPlacements );
'''
b = a + '''		/* FLAT GROUND OBJECTS (lane FLAT1): the counts, and the per-bake report
		 * written BESIDE the sheets (never into them): every base model the rule
		 * looked at, what it measured and what it decided. */
		r << QString( "flatObjects %1" ).arg( opts.cover.roads && opts.cover.flatObjects ? 1 : 0 );
		r << QString( "flatExamined %1" ).arg( roadCensus.flatExamined );
		r << QString( "flatPainted %1" ).arg( roadCensus.flatPainted );
		r << QString( "flatOverridden %1" ).arg( roadCensus.flatOverridden );
		r << QString( "flatHasLod %1" ).arg( roadCensus.flatHasLod );
		r << QString( "flatShapeTiles %1" ).arg( roadCensus.flatShapes );
		r << QString( "flatTexels %1" ).arg( roadCensus.flatTexels );
		r << QString( "flatDecalTexels %1" ).arg( roadCensus.flatDecalTexels );
		r << QString( "flatRefusedNoTexture %1" ).arg( roadCensus.flatRefusedNoTexture );
		if ( roadSet && opts.cover.flatObjects ) {
			const QString rp = dir + QChar( '/' ) + ws + QStringLiteral( ".flat_objects_report.txt" );
			QFile rf( rp );
			if ( rf.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
				rf.write( roadSet->flatReport( QString( "%1, cells %2 %3 %4 %5" ).arg( ws )
					.arg( levels[0].west ).arg( levels[0].south ).arg( levels[0].east )
					.arg( levels[0].north ) ).toUtf8() );
				r << QString( "flatReport %1" ).arg( rp );
			} else {
				r << QString( "flatReport unwritable:%1" ).arg( rp );
			}
		}
'''
assert s.count(a) == 1
s = s.replace(a, b)
open(P, 'w', newline='').write(s)
print('ok')

/* Standalone driver for src/hkxanim.cpp: decodes a clip (.xml or .hkx) and
   writes the same TSV the Python oracle (tests/spells/hkxanim_decode.py)
   writes, so gate (a) is a row-by-row comparison. Links Qt6Core + Qt6Gui only.

   Build (MSYS2 UCRT64, from the repo root):
     g++ -std=gnu++2a -O1 -Isrc -Ilib -DQT_NO_DEBUG -IC:/msys64/ucrt64/include/qt6 \
         -IC:/msys64/ucrt64/include/qt6/QtCore -IC:/msys64/ucrt64/include/qt6/QtGui \
         tests/hkxanim_dump.cpp src/hkxanim.cpp src/data/niftypes.cpp \
         -o release/hkxanim_dump.exe -LC:/msys64/ucrt64/lib -lQt6Core -lQt6Gui
   Usage:
     hkxanim_dump CLIP [--out frames.tsv]
   Prints the same header line as the oracle plus the reader's diagnostics.
   Exit 0 on a decode, 2 on a refusal. */

#include "hkxanim.h"

#include <QCoreApplication>
#include <QFile>
#include <QTextStream>

#include <cstdio>

int main( int argc, char ** argv )
{
	QCoreApplication app( argc, argv );
	const QStringList args = app.arguments();
	if ( args.size() < 2 ) {
		std::fprintf( stderr, "usage: hkxanim_dump CLIP(.xml|.hkx) [--out frames.tsv]\n" );
		return 2;
	}
	QString out;
	const int oi = args.indexOf( "--out" );
	if ( oi > 0 && oi + 1 < args.size() )
		out = args[oi + 1];

	const HkxAnimFile f = hkxAnimLoad( args[1] );
	if ( !f.ok() ) {
		std::printf( "REFUSED: %s\n", qPrintable( f.error ) );
		return 2;
	}
	for ( const HkxSkeleton & sk : f.skeletons )
		std::printf( "skeleton %s bones %d\n", qPrintable( sk.name ), int( sk.boneNames.size() ) );
	if ( f.clips.isEmpty() ) {
		std::printf( "route %s, no clip\n", qPrintable( f.route ) );
		return 0;
	}
	const HkxAnimClip & c = f.clips[0];
	std::printf( "frames %d tracks %d duration %.6f frameDuration %.6f blocks %d maxFramesPerBlock %d blendHint %s skeleton %s rootMotion %s\n",
		c.numFrames, c.numTracks, double( c.duration ), double( c.frameDuration ), c.numBlocks, c.maxFramesPerBlock,
		qPrintable( c.blendHint ), qPrintable( c.originalSkeletonName ), c.rootMotion.isEmpty() ? "no" : "yes" );
	std::printf( "route %s quantization %s worstQuatLengthDeviation %.3g blockOverlapFrames %d blockOverlapWorst %.3g\n",
		qPrintable( f.route ), qPrintable( c.rotationQuantization ), double( c.worstQuatLengthDeviation ),
		c.blockOverlapFrames, double( c.blockOverlapWorst ) );
	for ( int b = 0; b < c.walkEnd.size(); b++ )
		std::printf( "block %d walk ended at %d, block ends at %d\n", b, c.walkEnd[b], c.blockEnd[b] );
	int annotated = 0, annotations = 0;
	for ( const auto & lst : c.annotations ) {
		annotated += lst.isEmpty() ? 0 : 1;
		annotations += lst.size();
	}
	std::printf( "annotations %d on %d tracks\n", annotations, annotated );

	if ( !out.isEmpty() ) {
		QFile fo( out );
		if ( !fo.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
			std::printf( "cannot write %s\n", qPrintable( out ) );
			return 2;
		}
		QTextStream ts( &fo );
		ts.setRealNumberPrecision( 9 );
		ts << "# frame\ttrack\tbone\ttx\tty\ttz\tqx\tqy\tqz\tqw\tsx\tsy\tsz\n";
		for ( int fr = 0; fr < c.numFrames; fr++ ) {
			for ( int t = 0; t < c.numTracks; t++ ) {
				const HkxTransform & x = c.frames[fr][t];
				ts << fr << '\t' << t << '\t' << c.trackToBone[t];
				// Havok x y z w order, as the oracle prints it
				const float v[10] = { x.translation[0], x.translation[1], x.translation[2],
					x.rotation[1], x.rotation[2], x.rotation[3], x.rotation[0],
					x.scale[0], x.scale[1], x.scale[2] };
				for ( float e : v )
					ts << '\t' << QString::number( double( e ), 'g', 9 );
				ts << '\n';
			}
			if ( !c.rootMotion.isEmpty() ) {
				const HkxRootMotion & m = c.rootMotion[fr];
				ts << fr << "\t-1\t-1\t" << QString::number( double( m.translation[0] ), 'g', 9 ) << '\t'
				   << QString::number( double( m.translation[1] ), 'g', 9 ) << '\t'
				   << QString::number( double( m.translation[2] ), 'g', 9 ) << '\t'
				   << QString::number( double( m.yaw ), 'g', 9 ) << '\n';
			}
		}
		std::printf( "wrote %s\n", qPrintable( out ) );
	}
	return 0;
}

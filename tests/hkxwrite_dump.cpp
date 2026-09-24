/* Standalone driver for src/gltfimport.cpp and src/hkxwrite.cpp (lane HKX5).
   Links Qt6Core + Qt6Gui only, so the gates run with no NifSkope.exe.

   Build (MSYS2 UCRT64, from the repo root):
     bash scratchpad/hkx5_20260910/build_dump.sh

   Subcommands
     write   CLIP OUT.hkx  [--route a|b] [--xml PATH] [--tsv PATH] [--no-root-motion]
             read a clip with lane HKX1's reader and write it back out as an
             interleaved .hkx.  Route a = HKXPACK XML + pack, b = direct.
     import  GLTF OUT.hkx  [--route a|b] [--fps N] [--bones FILE] [--source-rate]
                           [--root-motion] [--root-node NAME] [--no-up-axis]
                           [--skeleton NAME] [--tsv PATH] [--xml PATH]
             import one glTF animation and write it as an interleaved .hkx.
     tsv     CLIP OUT.tsv
             the reader's TSV, the same columns tests/hkxanim_dump.cpp writes.

   Exit 0 on success, 2 on any refusal (the sentence goes to stdout). */

#include "gltfimport.h"
#include "hkxwrite.h"

#include <QCoreApplication>
#include <QFile>
#include <QTextStream>

#include <cstdio>

static QString argVal( const QStringList & a, const QString & key, const QString & dflt = QString() )
{
	const int i = a.indexOf( key );
	return ( i > 0 && i + 1 < a.size() ) ? a[i + 1] : dflt;
}

static bool argHas( const QStringList & a, const QString & key ) { return a.indexOf( key ) > 0; }

static bool writeTsv( const HkxAnimClip & c, const QString & out )
{
	QFile fo( out );
	if ( !fo.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
		std::printf( "cannot write %s\n", qPrintable( out ) );
		return false;
	}
	QTextStream ts( &fo );
	ts.setRealNumberPrecision( 9 );
	ts << "# frame\ttrack\tbone\ttx\tty\ttz\tqx\tqy\tqz\tqw\tsx\tsy\tsz\n";
	for ( int fr = 0; fr < c.numFrames; fr++ ) {
		for ( int t = 0; t < c.numTracks; t++ ) {
			const HkxTransform & x = c.frames[fr][t];
			ts << fr << '\t' << t << '\t' << c.trackToBone[t];
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
	return true;
}

static void printClip( const HkxAnimClip & c )
{
	std::printf( "clip '%s' frames %d tracks %d duration %.6f frameDuration %.6f blendHint %s skeleton '%s' rootMotion %s\n",
		qPrintable( c.name ), c.numFrames, c.numTracks, double( c.duration ), double( c.frameDuration ),
		qPrintable( c.blendHint ), qPrintable( c.originalSkeletonName ),
		c.rootMotion.isEmpty() ? "no" : qPrintable( QString::number( c.rootMotion.size() ) + " samples" ) );
}

static HkxWriteOptions::Route routeOf( const QStringList & a )
{
	return argVal( a, QStringLiteral( "--route" ), QStringLiteral( "b" ) ).startsWith( QLatin1Char( 'a' ) )
		? HkxWriteOptions::RouteXmlPack : HkxWriteOptions::RouteDirect;
}

int main( int argc, char ** argv )
{
	QCoreApplication app( argc, argv );
	const QStringList a = app.arguments();
	if ( a.size() < 3 ) {
		std::fprintf( stderr, "usage: hkxwrite_dump write|import|tsv IN OUT [options]\n" );
		return 2;
	}
	const QString cmd = a[1];

	if ( cmd == QLatin1String( "tsv" ) ) {
		const HkxAnimFile f = hkxAnimLoad( a[2] );
		if ( !f.ok() ) {
			std::printf( "REFUSED: %s\n", qPrintable( f.error ) );
			return 2;
		}
		if ( f.clips.isEmpty() ) {
			std::printf( "REFUSED: '%s' carries no clip\n", qPrintable( a[2] ) );
			return 2;
		}
		printClip( f.clips[0] );
		return ( a.size() > 3 && writeTsv( f.clips[0], a[3] ) ) ? 0 : 2;
	}

	HkxWriteOptions wo;
	wo.route = routeOf( a );
	wo.xmlKeepPath = argVal( a, QStringLiteral( "--xml" ) );
	if ( argHas( a, QStringLiteral( "--no-root-motion" ) ) )
		wo.writeRootMotion = false;
	if ( argHas( a, QStringLiteral( "--no-resource-container" ) ) )
		wo.writeResourceContainer = false;
	const QString skelOpt = argVal( a, QStringLiteral( "--skeleton" ) );
	if ( !skelOpt.isEmpty() )
		wo.skeletonName = skelOpt;

	HkxAnimClip clip;

	if ( cmd == QLatin1String( "write" ) ) {
		const HkxAnimFile f = hkxAnimLoad( a[2] );
		if ( !f.ok() ) {
			std::printf( "REFUSED (read): %s\n", qPrintable( f.error ) );
			return 2;
		}
		if ( f.clips.isEmpty() ) {
			std::printf( "REFUSED (read): '%s' carries no clip\n", qPrintable( a[2] ) );
			return 2;
		}
		clip = f.clips[0];
	} else if ( cmd == QLatin1String( "import" ) ) {
		GltfImportOptions io;
		const QString fps = argVal( a, QStringLiteral( "--fps" ) );
		if ( !fps.isEmpty() )
			io.targetFps = fps.toFloat();
		io.preserveSourceRate = argHas( a, QStringLiteral( "--source-rate" ) );
		io.extractRootMotion = argHas( a, QStringLiteral( "--root-motion" ) );
		io.rootNodeName = argVal( a, QStringLiteral( "--root-node" ) );
		if ( argHas( a, QStringLiteral( "--no-up-axis" ) ) )
			io.convertUpAxis = false;
		if ( argHas( a, QStringLiteral( "--no-static-tracks" ) ) )
			io.includeStaticTracks = false;
		if ( !skelOpt.isEmpty() )
			io.originalSkeletonName = skelOpt;
		const QString bones = argVal( a, QStringLiteral( "--bones" ) );
		if ( !bones.isEmpty() ) {
			QFile bf( bones );
			if ( !bf.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
				std::printf( "REFUSED: cannot read the bone list '%s'\n", qPrintable( bones ) );
				return 2;
			}
			while ( !bf.atEnd() ) {
				const QString ln = QString::fromUtf8( bf.readLine() ).trimmed();
				if ( !ln.isEmpty() && !ln.startsWith( QLatin1Char( '#' ) ) )
					io.skeletonBoneNames.append( ln );
			}
		}
		GltfImportReport ir;
		if ( !gltfImportRead( a[2], io, clip, ir ) ) {
			std::printf( "REFUSED (import): %s\n", qPrintable( ir.error ) );
			return 2;
		}
		std::printf( "import: %s\n", qPrintable( ir.summary() ) );
		std::printf( "import detail: nodes %d animated %d channels %d static %d interpolation %s keys %d first %.6f last %.6f uniform %d sourceFps %.4f durationDelta %.6g\n",
			ir.gltfNodes, ir.animatedNodes, ir.channelsRead, ir.staticTracks,
			qPrintable( ir.interpolations.join( QLatin1Char( '+' ) ) ), ir.sourceKeys,
			double( ir.sourceFirstKey ), double( ir.sourceLastKey ), ir.sourceUniform ? 1 : 0,
			double( ir.sourceFps ), double( ir.durationDelta ) );
		if ( !ir.unmatchedNodes.isEmpty() )
			std::printf( "import unmatched nodes: %s\n", qPrintable( ir.unmatchedNodes.join( QLatin1String( ", " ) ) ) );
		if ( !ir.unmatchedBones.isEmpty() )
			std::printf( "import unmatched bones (%d): %s\n", int( ir.unmatchedBones.size() ),
						 qPrintable( ir.unmatchedBones.join( QLatin1String( ", " ) ) ) );
		if ( !ir.caseFolded.isEmpty() )
			std::printf( "import case-folded: %s\n", qPrintable( ir.caseFolded.join( QLatin1String( ", " ) ) ) );
		if ( !ir.partialMatched.isEmpty() )
			std::printf( "import partial: %s\n", qPrintable( ir.partialMatched.join( QLatin1String( ", " ) ) ) );
	} else {
		std::fprintf( stderr, "unknown subcommand '%s'\n", qPrintable( cmd ) );
		return 2;
	}

	printClip( clip );
	const QString tsv = argVal( a, QStringLiteral( "--tsv" ) );
	if ( !tsv.isEmpty() && !writeTsv( clip, tsv ) )
		return 2;

	HkxWriteReport wr;
	if ( !hkxWrite( clip, a[3], wo, wr ) ) {
		std::printf( "REFUSED (write): %s\n", qPrintable( wr.error ) );
		return 2;
	}
	std::printf( "write: %s\n", qPrintable( wr.summary() ) );
	if ( !wr.command.isEmpty() )
		std::printf( "write command: %s\n", qPrintable( wr.command ) );
	return 0;
}

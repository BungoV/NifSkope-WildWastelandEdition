/* lane CELLSPIKE2: `NifSkope -no-gui streamwalk <path file> [--bar <ms>] [--dump <file>] [--trim] [--remake <k>]`
 *
 * The cell stream's walk with no window: the open WW_CELL_OPEN names (streaming on), then for each path line
 * ("ex ey ez ax ay az [fov]", the WW_CELL_STREAM_PATH format) the stream ticks back to back until its cells are
 * in (at least 10 + 4 frames, as wwCellStreamPathRun), the scene transformed every 250 ms like a paint. One line
 * a step: the step's worst frame, slice, unload, drain and bury, and the stages of its worst frame and unload.
 * The verdict: every crossing step's worst frame (steps 1..n-1) within the bar (8 ms). No GL: no paint, no
 * textures, so it times the stream's own frame work, which is what the bar is about. */

#include "cellstream.h"
#include "cellview.h"
#include "lodgen.h"
#include "wwswitch.h"
#include "gl/glscene.h"
#include "gl/gltex.h"
#include "model/nifmodel.h"

#include <QElapsedTimer>
#include <QFile>
#include <QRegularExpression>
#include <QStringList>
#include <QThread>

#include <algorithm>
#include <cstdio>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

int cellStreamWalkCli( const QStringList & args )
{
	QString pathFile, dumpPath;
	double bar = 8.0;
	bool trim = false;
	int remakeAt = -1;
	for ( int i = 0; i < args.size(); i++ ) {
		if ( args[i] == QLatin1String( "--bar" ) && i + 1 < args.size() )
			bar = args[++i].toDouble();
		else if ( args[i] == QLatin1String( "--dump" ) && i + 1 < args.size() )
			dumpPath = args[++i];
		else if ( args[i] == QLatin1String( "--trim" ) )
			trim = true;
		else if ( args[i] == QLatin1String( "--remake" ) && i + 1 < args.size() )
			remakeAt = args[++i].toInt();
		else if ( pathFile.isEmpty() )
			pathFile = args[i];
	}
	QFile pf( pathFile );
	if ( pathFile.isEmpty() || !pf.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
		std::fprintf( stderr, "streamwalk: usage: streamwalk <path file> [--bar ms] [--dump file] [--trim] [--remake k] (WW_CELL_OPEN names the open)\n" );
		return 2;
	}
	struct Step { Vector3 eye; };
	QVector<Step> steps;
	for ( const QString & raw : QString::fromUtf8( pf.readAll() ).split( QLatin1Char( '\n' ) ) ) {
		const QString line = raw.trimmed();
		if ( line.isEmpty() || line.startsWith( QLatin1Char( '#' ) ) )
			continue;
		const QStringList f = line.split( QRegularExpression( QStringLiteral( "[\\s,]+" ) ), Qt::SkipEmptyParts );
		if ( f.size() >= 6 )
			steps.append( { Vector3( f[0].toFloat(), f[1].toFloat(), f[2].toFloat() ) } );
	}
	// a -no-gui run never applies the resource stack (main.cpp's GUI branch does): the data root is read here
	const QString res = wwEnvStr( "WW_LODGEN_RESOURCES" );
	if ( !res.isEmpty() )
		lodgenSetResources( res.split( QLatin1Char( ';' ), Qt::SkipEmptyParts ) );
	CellSceneSpec spec;
	QString err, notes;
	if ( !cellSpecFromEnv( spec, &err ) || !spec.valid ) {
		std::fprintf( stderr, "streamwalk: WW_CELL_OPEN: %s\n", qPrintable( err ) );
		return 2;
	}
	NifModel nif;
	QElapsedTimer open;
	open.start();
	if ( !nifCreateCellScene( &nif, spec, &err, &notes ) ) {
		std::fprintf( stderr, "streamwalk: open failed: %s\n", qPrintable( err ) );
		return 2;
	}
	if ( !cellStreamOn( &nif ) ) {
		std::fprintf( stderr, "streamwalk: the open does not stream (WW_CELL_STREAM=1?)\n" );
		cellStreamStop( &nif );
		return 2;
	}
	TexCache tex;
	Scene scene( &tex );
	scene.make( &nif );
	std::printf( "streamwalk: open %lld ms, %d steps, bar %.1f ms\n", open.elapsed(), int( steps.size() ), bar );
	std::fflush( stdout );
	QFile dump( dumpPath );
	const bool dumpOk = !dumpPath.isEmpty() && dump.open( QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate );
	auto settle = [&]( const Vector3 & eye, int minFrames ) {
		QElapsedTimer t, paint;
		t.start();
		paint.start();
		for ( int k = 0; k < minFrames || cellStreamBusy( &nif ); k++ ) {
			cellStreamTick( &nif, &scene, eye );
			if ( k < minFrames || paint.elapsed() > 250 ) {
				scene.transform( Transform(), 0.0f );
				paint.restart();
			}
			if ( t.elapsed() > 600000 )
				break;
		}
		return t.elapsed();
	};
	cellStreamStats( &nif, true );
	double worst = 0.0;
	int worstStep = -1, over = 0;
	double unloadWorst = 0.0;
	qint64 scanned = 0;
	for ( int i = 0; i < steps.size(); i++ ) {
		if ( i == remakeAt && i > 0 ) {   // --remake k: the view makes the scene again (GLView modelChanged) before step k
			QElapsedTimer m;
			m.start();
			scene.make( &nif );
			scene.transform( Transform(), 0.0f );
			const qint64 makeMs = m.elapsed();
			cellStreamStats( &nif, true );
			cellStreamTick( &nif, &scene, steps[i - 1].eye );   // the stream finds its cells in the new scene
			const CellStreamStats r = cellStreamStats( &nif, false );
			std::printf( "remake before step %d | make %lld ms | resolve frame %.3f ms | %s\n", i, makeMs, r.tickMaxMs,
				qPrintable( r.worstTick ) );
			std::fflush( stdout );
		}
		cellStreamStats( &nif, true );
#ifdef Q_OS_WIN
		if ( trim ) {   // --trim: the live window's state (private 12.3 GB, working set 8.1 GB): pages out, then one "paint"
			SetProcessWorkingSetSize( GetCurrentProcess(), SIZE_T( -1 ), SIZE_T( -1 ) );
			scene.transformDirty = true;
			scene.transform( Transform(), 0.0f );
		}
#endif
		const qint64 waited = settle( steps[i].eye, 10 );
		settle( steps[i].eye, 4 );
		const CellStreamStats s = cellStreamStats( &nif, false );
		const QString line = QStringLiteral( "step %1 | centre %2,%3 | match %4 | waitMs %5 | tickMaxMs %6 sliceMaxMs %7 "
			"unloadMaxMs %8 drainMaxMs %9 buryMaxMs %10 rebuildMaxMs %11 | unloads %12 attaches %13 | %14 | %15" )
			.arg( i ).arg( s.cx ).arg( s.cy ).arg( s.match ? 1 : 0 ).arg( waited ).arg( s.tickMaxMs, 0, 'f', 3 )
			.arg( s.sliceMaxMs, 0, 'f', 3 ).arg( s.unloadMaxMs, 0, 'f', 3 ).arg( s.drainMaxMs, 0, 'f', 3 )
			.arg( s.buryMaxMs, 0, 'f', 3 ).arg( s.rebuildMaxMs, 0, 'f', 3 ).arg( s.unloads ).arg( s.attaches )
			.arg( s.worstTick, s.worstUnload + QStringLiteral( " scannedNodes %1" ).arg( s.scannedNodes ) );
		std::printf( "%s\n", qPrintable( line ) );
		std::fflush( stdout );
		if ( dumpOk ) {
			dump.write( line.toUtf8() + '\n' );
			dump.flush();
		}
		if ( i > 0 ) {   // step 0 is the first load (no crossing)
			unloadWorst = std::max( unloadWorst, s.unloadMaxMs );
			scanned += s.scannedNodes;
			if ( s.tickMaxMs > bar )
				over++;
			if ( s.tickMaxMs > worst ) {
				worst = s.tickMaxMs;
				worstStep = i;
			}
		}
	}
	cellStreamStop( &nif );
	// the unload alone: its dearest call, and the scene nodes the unloads walked to find their own (0 = none)
	std::printf( "streamwalk: unload worst %.3f ms, scene nodes scanned %lld\n", unloadWorst, scanned );
	std::printf( "streamwalk: worst crossing frame %.3f ms at step %d, %d of %d crossings over %.1f ms VERDICT %s\n",
		worst, worstStep, over, std::max( 0, int( steps.size() ) - 1 ), bar, over == 0 && steps.size() > 1 ? "PASS" : "FAIL" );
	std::fflush( stdout );
	return over == 0 && steps.size() > 1 ? 0 : 1;
}

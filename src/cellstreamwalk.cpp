/* lane CELLSPIKE2: `NifSkope -no-gui streamwalk <path file> [--bar <ms>] [--dump <file>] [--trim] [--remake <k>]`
 *
 * The cell stream's walk with no window: the open WW_CELL_OPEN names (streaming on), then for each path line
 * ("ex ey ez ax ay az [fov]", the WW_CELL_STREAM_PATH format) the stream ticks back to back until its cells are
 * in (at least 10 + 4 frames, as wwCellStreamPathRun), the scene transformed every 250 ms like a paint. One line
 * a step: the step's worst frame, slice, unload, drain and bury, and the stages of its worst frame and unload.
 * The verdict: every crossing step's worst frame (steps 1..n-1) within the bar (8 ms). No GL: no paint, no
 * textures, so it times the stream's own frame work, which is what the bar is about.
 * lane FRAMESPEED1: a second verdict (CPU-VERDICT) holds every crossing step's dearest ON-PROCESSOR frame to the same
 * bar: the wall bar also counts the machine's pre-emption and paging, which no change here moves. */

#include "cellfarhide.h"
#include "cellmesh.h"
#include "cellstream.h"
#include "cellview.h"
#include "lodgen.h"
#include "wwswitch.h"
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "gl/gltex.h"
#include "model/nifmodel.h"

#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>
#include <QThread>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

/* lane LODPLACE1 (CODE1030 2a + 2b): two censuses on the open's own document, no window.
 *   --place <file>  after the walk: every far object row of the instanced rings, every other shape and every far
 *                   terrain vertex in WORLD units (tests/lodplace/place_census.py puts them against the LAND height
 *                   and the ESM at their XY: under the map, flipped, floating).
 *   --still <n>     n more frames at the last step's eye. Each frame's far state (the rows every instanced ring draws,
 *                   the masked far triangles, the hidden far nodes, the loaded cells) against the frame before it:
 *                   a steady camera must change nothing ("farstate still: changes 0 of n").
 * Every step of the walk also counts, per far row, the frames it changed on: a row that changes twice inside one step
 * (the eye fixed) flip-flopped, and a change with the loaded cells unchanged has no cause. */
namespace
{

struct FarSig
{
	quint64 loadedGen = 0, keys = 0;
	qint64 loadedCells = 0, hiddenRows = 0, masked = 0, hiddenNodes = 0;
	bool operator==( const FarSig & o ) const
	{
		return loadedGen == o.loadedGen && keys == o.keys && loadedCells == o.loadedCells && hiddenRows == o.hiddenRows
			&& masked == o.masked && hiddenNodes == o.hiddenNodes;
	}
	bool operator!=( const FarSig & o ) const { return !( *this == o ); }
};

struct FarTrack
{
	NifModel * nif = nullptr;
	Scene * scene = nullptr;
	bool have = false;
	FarSig prev;
	qint64 frames = 0, changed = 0, uncaused = 0, rowChanges = 0, rowFlipFlops = 0, rowsWatched = 0;
	QHash<int, std::vector<char>> drawn;	// shape id -> per row: drawn now
	QHash<quint64, int> stepChanges;		// (shape id << 32 | row) -> changes inside the current step

	static bool isFar( const Shape * sh ) { return sh->getName().startsWith( QLatin1String( "FarLOD" ) ); }

	FarSig sig() const
	{
		FarSig s;
		if ( const CellFarHide::Hidden * h = CellFarHide::hiddenFor( nif ) ) {
			s.loadedGen = h->gen;
			s.loadedCells = h->cells.size();
		}
		s.masked = CellFarHide::maskedTris( nif );
		quint64 k = 1469598103934665603ull;
		for ( Node * n : scene->nodes.list() ) {
			const Shape * sh = dynamic_cast<const Shape *>( n );
			if ( !sh || !isFar( sh ) )
				continue;
			s.hiddenNodes += sh->isHidden() ? 1 : 0;
			if ( const CellMeshInst * d = sh->wwFarInstData() ) {
				const CellFarHide::View v = CellFarHide::view( *d );
				s.hiddenRows += v.hiddenRows;
				k = ( k ^ ( v.key + quint64( quint32( sh->id() ) ) * 2654435761ull ) ) * 1099511628211ull;
			}
		}
		s.keys = k;
		return s;
	}
	// the rows each instanced ring draws now (a hidden node draws none), against the last frame's
	void rowsNow()
	{
		rowsWatched = 0;
		for ( Node * n : scene->nodes.list() ) {
			const Shape * sh = dynamic_cast<const Shape *>( n );
			const CellMeshInst * d = sh && isFar( sh ) ? sh->wwFarInstData() : nullptr;
			if ( !d )
				continue;
			std::vector<char> now( static_cast<size_t>( d->inst.size() / 16 ), char( 0 ) );
			if ( !sh->isHidden() ) {
				const CellFarHide::View v = CellFarHide::view( *d );
				for ( const CellMeshInst::Cmd & c : *v.cmds )
					for ( quint32 r = c.baseInstance; r < c.baseInstance + c.instanceCount && r < now.size(); r++ )
						now[r] = 1;
			}
			rowsWatched += qint64( now.size() );
			auto it = drawn.find( sh->id() );
			if ( it != drawn.end() && it->size() == now.size() ) {
				for ( size_t r = 0; r < now.size(); r++ )
					if ( ( *it )[r] != now[r] ) {
						rowChanges++;
						stepChanges[( quint64( quint32( sh->id() ) ) << 32 ) | quint64( r )]++;
					}
			}
			drawn.insert( sh->id(), std::move( now ) );
		}
	}
	void frame()
	{
		const FarSig s = sig();
		frames++;
		if ( !have || s.keys != prev.keys || s.hiddenNodes != prev.hiddenNodes )
			rowsNow();
		if ( have && s != prev ) {
			changed++;
			if ( s.loadedGen == prev.loadedGen )
				uncaused++;
		}
		prev = s;
		have = true;
	}
	void stepEnd()
	{
		for ( auto it = stepChanges.constBegin(); it != stepChanges.constEnd(); ++it )
			rowFlipFlops += it.value() > 1 ? 1 : 0;
		stepChanges.clear();
	}
};

// --place: world boxes, one line a far row / shape, plus the far terrain's vertices
qint64 farPlaceDump( Scene * scene, NifModel * nif, QFile & f )
{
	QTextStream o( &f );
	o.setRealNumberNotation( QTextStream::FixedNotation );
	o.setRealNumberPrecision( 1 );
	o << "# kind name id ref row x y zlo zhi upz drawn | S: kind name id - - xlo ylo zlo xhi yhi zhi drawn | V: name x y z\n";
	qint64 lines = 0;
	for ( Node * n : scene->nodes.list() ) {
		Shape * sh = dynamic_cast<Shape *>( n );
		if ( !sh )
			continue;
		const Transform & W = sh->worldTrans();
		const QString name = QString( sh->getName() ).replace( QLatin1Char( ' ' ), QLatin1Char( '_' ) );
		if ( const CellMeshInst * d = sh->wwFarInstData() ) {
			const CellFarHide::Hidden * h = CellFarHide::hiddenFor( nif );
			QHash<quint64, std::pair<Vector3, Vector3>> boxOf;	// (firstIndex, baseVertex) -> the sub-mesh's box
			for ( const CellMeshInst::Cmd & c : d->cmds ) {
				const quint64 bk = ( quint64( c.firstIndex ) << 32 ) | quint64( quint32( c.baseVertex ) );
				auto bit = boxOf.find( bk );
				if ( bit == boxOf.end() ) {
					Vector3 lo( 3.4e38f, 3.4e38f, 3.4e38f ), hi( -3.4e38f, -3.4e38f, -3.4e38f );
					for ( quint32 i = c.firstIndex; i < c.firstIndex + c.count && i < quint32( d->ebo.size() ); i++ ) {
						const qint64 vi = qint64( d->ebo[qsizetype( i )] ) + c.baseVertex;
						if ( vi < 0 || ( vi + 1 ) * 36 > d->vbo.size() )
							continue;
						float p[3];
						std::memcpy( p, d->vbo.constData() + vi * 36, 12 );
						for ( int k = 0; k < 3; k++ ) {
							lo[k] = std::min( lo[k], p[k] );
							hi[k] = std::max( hi[k], p[k] );
						}
					}
					bit = boxOf.insert( bk, { lo, hi } );
				}
				const Vector3 lo = bit->first, hi = bit->second;
				if ( lo[0] > hi[0] )
					continue;
				for ( quint32 r = c.baseInstance; r < c.baseInstance + c.instanceCount && qsizetype( r ) * 16 + 16 <= d->inst.size(); r++ ) {
					const float * w = d->inst.constData() + qsizetype( r ) * 16;
					float zlo = 3.4e38f, zhi = -3.4e38f;
					for ( int cnr = 0; cnr < 8; cnr++ ) {
						const Vector3 p( ( cnr & 1 ) ? hi[0] : lo[0], ( cnr & 2 ) ? hi[1] : lo[1], ( cnr & 4 ) ? hi[2] : lo[2] );
						const Vector3 loc( w[0] * p[0] + w[1] * p[1] + w[2] * p[2] + w[3], w[4] * p[0] + w[5] * p[1] + w[6] * p[2] + w[7],
							w[8] * p[0] + w[9] * p[1] + w[10] * p[2] + w[11] );
						const Vector3 wp = W * loc;
						zlo = std::min( zlo, wp[2] );
						zhi = std::max( zhi, wp[2] );
					}
					const Vector3 at = W * Vector3( w[3], w[7], w[11] );
					const float s = std::sqrt( w[2] * w[2] + w[6] * w[6] + w[10] * w[10] );	// column 2: scale x the up axis
					const quint32 ref = r < quint32( d->rowRef.size() ) ? d->rowRef[qsizetype( r )] : 0u;
					const bool hid = sh->isHidden() || ( h && CellFarHide::rowHidden( *d, int( r ), *h, CellFarHide::mode() ) );
					o << "I " << name << ' ' << sh->id() << ' ' << QString::number( ref, 16 ) << ' ' << r << ' ' << at[0] << ' '
					  << at[1] << ' ' << zlo << ' ' << zhi << ' ' << ( s > 0.0f ? w[10] / s : 0.0f ) << ' ' << ( hid ? 0 : 1 ) << '\n';
					lines++;
				}
			}
			continue;
		}
		QVector<Vector3> verts = sh->verts;
		if ( verts.isEmpty() ) {
			const QSharedPointer<CellMesh> cm = cellMeshFor( nif, nif->getBlockIndex( sh->id() ) );
			if ( cm )
				verts = cm->verts;
		}
		if ( verts.isEmpty() )
			continue;
		const bool terrain = name.startsWith( QLatin1String( "FarLOD_terrain" ) );
		Vector3 lo( 3.4e38f, 3.4e38f, 3.4e38f ), hi( -3.4e38f, -3.4e38f, -3.4e38f );
		for ( const Vector3 & v : verts ) {
			const Vector3 wp = W * v;
			for ( int k = 0; k < 3; k++ ) {
				lo[k] = std::min( lo[k], wp[k] );
				hi[k] = std::max( hi[k], wp[k] );
			}
			if ( terrain ) {
				o << "V " << name << ' ' << wp[0] << ' ' << wp[1] << ' ' << wp[2] << '\n';
				lines++;
			}
		}
		o << "S " << name << ' ' << sh->id() << " - - " << lo[0] << ' ' << lo[1] << ' ' << lo[2] << ' ' << hi[0] << ' ' << hi[1]
		  << ' ' << hi[2] << ' ' << ( sh->isHidden() ? 0 : 1 ) << '\n';
		lines++;
	}
	o.flush();
	return lines;
}

} // namespace

int cellStreamWalkCli( const QStringList & args )
{
	QString pathFile, dumpPath;
	double bar = 8.0;
	bool trim = false;
	int remakeAt = -1;
	QString placePath;	// lane LODPLACE1
	int stillFrames = 0;
	bool stillRed = false;	// the census's own red: one far row's cell hidden and shown on alternate still frames
	for ( int i = 0; i < args.size(); i++ ) {
		if ( args[i] == QLatin1String( "--place" ) && i + 1 < args.size() )
			placePath = args[++i];
		else if ( args[i] == QLatin1String( "--still" ) && i + 1 < args.size() )
			stillFrames = args[++i].toInt();
		else if ( args[i] == QLatin1String( "--still-red" ) )
			stillRed = true;
		else if ( args[i] == QLatin1String( "--bar" ) && i + 1 < args.size() )
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
		std::fprintf( stderr, "streamwalk: usage: streamwalk <path file> [--bar ms] [--dump file] [--trim] [--remake k] [--still n] [--place file] (WW_CELL_OPEN names the open)\n" );
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
	FarTrack track;	// lane LODPLACE1: the far state frame by frame
	track.nif = &nif;
	track.scene = &scene;
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
			track.frame();
			if ( t.elapsed() > 600000 )
				break;
		}
		return t.elapsed();
	};
	cellStreamStats( &nif, true );
	double worst = 0.0;
	int worstStep = -1, over = 0;
	double cpuWorst = 0.0;
	int cpuWorstStep = -1, cpuOver = 0;
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
		const qint64 flips0 = track.rowFlipFlops, rows0 = track.rowChanges, unc0 = track.uncaused;
		const qint64 waited = settle( steps[i].eye, 10 );
		settle( steps[i].eye, 4 );
		track.stepEnd();
		std::printf( "farstate step %d | rowChanges %lld rowFlipFlops %lld uncausedFrames %lld | rows %lld\n", i,
			track.rowChanges - rows0, track.rowFlipFlops - flips0, track.uncaused - unc0, track.rowsWatched );
		const CellStreamStats s = cellStreamStats( &nif, false );
		const QString line = QStringLiteral( "step %1 | centre %2,%3 | match %4 | waitMs %5 | tickMaxMs %6 sliceMaxMs %7 "
			"unloadMaxMs %8 drainMaxMs %9 buryMaxMs %10 rebuildMaxMs %11 | unloads %12 attaches %13 | %14 | %15" )
			.arg( i ).arg( s.cx ).arg( s.cy ).arg( s.match ? 1 : 0 ).arg( waited ).arg( s.tickMaxMs, 0, 'f', 3 )
			.arg( s.sliceMaxMs, 0, 'f', 3 ).arg( s.unloadMaxMs, 0, 'f', 3 ).arg( s.drainMaxMs, 0, 'f', 3 )
			.arg( s.buryMaxMs, 0, 'f', 3 ).arg( s.rebuildMaxMs, 0, 'f', 3 ).arg( s.unloads ).arg( s.attaches )
			.arg( s.worstTick, s.worstUnload + QStringLiteral( " scannedNodes %1" ).arg( s.scannedNodes ) )
			+ QStringLiteral( " | cpuMaxMs %1 cpuTick %2" ).arg( s.cpuMaxMs, 0, 'f', 3 ).arg( s.worstCpuTick );
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
			if ( s.cpuMaxMs > bar )
				cpuOver++;
			if ( s.cpuMaxMs > cpuWorst ) {
				cpuWorst = s.cpuMaxMs;
				cpuWorstStep = i;
			}
		}
	}
	// lane LODPLACE1: the walk's totals, then a steady camera, then the far field's places
	std::printf( "farstate walk: frames %lld, far state changed on %lld, with the loaded cells unchanged on %lld; "
		"row changes %lld, rows that flip-flopped inside one step %lld\n", track.frames, track.changed, track.uncaused,
		track.rowChanges, track.rowFlipFlops );
	if ( stillFrames > 0 && !steps.isEmpty() ) {
		const Vector3 eye = steps.last().eye;
		settle( eye, 4 );	// the last step is in and quiet
		track.stepEnd();
		const qint64 ch0 = track.changed, rows0 = track.rowChanges, f0 = track.rowFlipFlops;
		// --still-red: the cell of the first drawn far row, toggled every frame the way a handover with no
		// hysteresis would (the census must count it; a run without it must not)
		int redX = 0, redY = 0;
		bool redCell = false;
		QVector<quint32> redRefs;
		if ( stillRed ) {
			for ( Node * n : scene.nodes.list() ) {
				const Shape * sh = dynamic_cast<const Shape *>( n );
				const CellMeshInst * d = sh && FarTrack::isFar( sh ) ? sh->wwFarInstData() : nullptr;
				if ( d && !d->rowCell.isEmpty() ) {
					const qint64 key = d->rowCell.first();
					redX = int( key >> 32 );
					redY = int( qint32( quint32( quint64( key ) & 0xFFFFFFFFull ) ) );
					if ( !d->rowRef.isEmpty() && d->rowRef.first() )
						redRefs.append( d->rowRef.first() );	// the row's REFR: the ByRef rule hides it with its cell
					redCell = true;
					break;
				}
			}
			std::printf( "farstate still: RED -- cell %d,%d hidden and shown on alternate frames (%s)\n", redX, redY,
				redCell ? "found" : "NO far row to toggle" );
		}
		for ( int k = 0; k < stillFrames; k++ ) {
			if ( redCell )
				CellFarHide::setCell( &nif, redX, redY, ( k & 1 ) == 0, redRefs );
			cellStreamTick( &nif, &scene, eye );
			scene.transform( Transform(), 0.0f );
			track.frame();
		}
		track.stepEnd();
		std::printf( "farstate still: %d frames at %.0f %.0f %.0f, changes %lld, row changes %lld, flip-flops %lld, rows %lld "
			"VERDICT %s\n", stillFrames, eye[0], eye[1], eye[2], track.changed - ch0, track.rowChanges - rows0,
			track.rowFlipFlops - f0, track.rowsWatched,
			track.changed == ch0 && track.rowChanges == rows0 && track.rowsWatched > 0 ? "PASS" : "FAIL" );
	}
	if ( !placePath.isEmpty() ) {
		QFile pf2( placePath );
		if ( pf2.open( QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate ) )
			std::printf( "farplace: %lld lines -> %s\n", farPlaceDump( &scene, &nif, pf2 ), qPrintable( placePath ) );
		else
			std::printf( "farplace: cannot write %s\n", qPrintable( placePath ) );
	}
	std::fflush( stdout );
	cellStreamStop( &nif );
	// the unload alone: its dearest call, and the scene nodes the unloads walked to find their own (0 = none)
	std::printf( "streamwalk: unload worst %.3f ms, scene nodes scanned %lld\n", unloadWorst, scanned );
	std::printf( "streamwalk: worst crossing frame %.3f ms at step %d, %d of %d crossings over %.1f ms VERDICT %s\n",
		worst, worstStep, over, std::max( 0, int( steps.size() ) - 1 ), bar, over == 0 && steps.size() > 1 ? "PASS" : "FAIL" );
	std::printf( "streamwalk: worst crossing cpu frame %.3f ms at step %d, %d of %d crossings over %.1f ms CPU-VERDICT %s\n",
		cpuWorst, cpuWorstStep, cpuOver, std::max( 0, int( steps.size() ) - 1 ), bar,
		cpuOver == 0 && steps.size() > 1 ? "PASS" : "FAIL" );
	std::fflush( stdout );
	return over == 0 && steps.size() > 1 ? 0 : 1;
}

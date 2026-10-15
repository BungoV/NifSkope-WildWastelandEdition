/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

/* The release build is -O3 -march=haswell, where GCC fuses a*b+c into one FMA by default. Patch
 * geometry must come out bit for bit what the independent twin (tests/spells/lodp_decode.py)
 * computes, and the 45-degree crease test sits on exact ties, so this unit does plain IEEE
 * arithmetic (lane LODP1, 2026-10-15: FMA noise of 1e-17 split 5 bevelled meshes). */
#pragma GCC optimize( "fp-contract=off" )

#include "lodpbake.h"

#include "lodifile.h"
#include "lodofile.h"
#include "lodpplace.h"
#include "lodtfile.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>

namespace {

//! FNV over the three input files' own CRC words (and the `.lodl`'s first 4 KiB): the LOD bytes the patches stand on.
quint64 inputHashOf( const LodoHeader & oh, const LodiHeader & ih, const QString & lodlPath )
{
	quint64 h = lodoFnv1a64( &oh.headerCrc32, 4 );
	h = lodoFnv1a64( &ih.headerCrc32, 4, h );
	if ( !lodlPath.isEmpty() ) {
		QFile f( lodlPath );
		if ( f.open( QIODevice::ReadOnly ) ) {
			const QByteArray b = f.read( 4096 );
			h = lodoFnv1a64( b.constData(), size_t( b.size() ), h );
		}
	}
	return h;
}

} // namespace

bool lodpBake( const LodpBakeOptions & o, QStringList * report, QString * error )
{
	auto fail = [error]( const QString & m ) { if ( error ) *error = m; return false; };
	QElapsedTimer clock;
	clock.start();

	LodoHeader oh;
	LodoLibrary lib;
	QString err;
	if ( !lodoRead( o.lodoPath, &oh, &lib, true, &err ) )
		return fail( QStringLiteral( ".lodo: " ) + err );
	LodiHeader ih;
	LodiTable table;
	if ( !lodiRead( o.lodiPath, &ih, &table, true, &err ) )
		return fail( QStringLiteral( ".lodi: " ) + err );
	const quint64 identity = lodoIdentityOf( oh.headerCrc32, oh.modelCorpusHash, oh.objectCorpusHash );
	if ( ih.lodoIdentity != identity )
		return fail( QStringLiteral( "the .lodi does not pair with this .lodo (lodoIdentity differs)" ) );

	/* THE STREAM, CHECKED. Present = it must be exactly what the chart rule
	 * gives on this library today; absent = a census run (BOUND clear). */
	bool bound = false;
	{
		std::vector<quint8> stream, meshCharts;
		if ( !lodpChartStream( lib, &stream, &meshCharts, &err ) )
			return fail( QStringLiteral( "chart rule: " ) + err );
		if ( !lib.chartStream.empty() ) {
			if ( stream != lib.chartStream ) {
				size_t at = 0;
				while ( at < stream.size() && at < lib.chartStream.size() && stream[at] == lib.chartStream[at] )
					at++;
				return fail( QString( "the .lodo chart stream disagrees with the chart rule at byte %1 (cluster %2, triangle %3)" )
					.arg( at ).arg( at / LODO_CHART_STRIDE ).arg( at % LODO_CHART_STRIDE ) );
			}
			bound = true;
		}
	}

	// placements
	std::vector<LodpPlacementIn> pl;
	quint64 read = 0, noMesh = 0, disabled = 0, outside = 0;
	for ( quint32 ci = 0; ci < quint32( table.chunks.size() ); ci++ ) {
		const LodiChunk & chunk = table.chunks[ci];
		for ( quint32 k = 0; k < chunk.instanceCount; k++ ) {
			const size_t ii = size_t( chunk.instanceFirst ) + size_t( k );
			if ( ii >= table.instances.size() )
				return fail( QString( "chunk %1 runs past the instance table" ).arg( ci ) );
			read++;
			const LodiInstance & inst = table.instances[ii];
			if ( inst.flags & LODI_INST_INITIALLY_DISABLED ) {
				disabled++;
				continue;
			}
			LodpPlacementIn in;
			in.lodiIndex = quint32( ii );
			lodiDecodePosition( ih, ci, chunk, inst, in.pos );
			if ( o.region ) {
				const int cx = int( std::floor( in.pos[0] / LODP_CELL_UNITS ) ), cy = int( std::floor( in.pos[1] / LODP_CELL_UNITS ) );
				if ( cx < o.x0 || cx > o.x1 || cy < o.y0 || cy > o.y1 ) {
					outside++;
					continue;
				}
			}
			quint16 meshId = LODO_NO_MESH;
			if ( inst.baseId < lib.bases.size() ) {
				const LodoBase & base = lib.bases[inst.baseId];
				for ( int r = 0; r < 4 && meshId == LODO_NO_MESH; r++ )
					if ( base.rep[r] != LODO_NO_MESH && base.rep[r] < lib.meshes.size() )
						meshId = base.rep[r];
			}
			if ( meshId == LODO_NO_MESH ) {
				noMesh++;
				continue;
			}
			in.mesh = meshId;
			float q[4];
			lodiUnpackRotation( inst.rot, q, in.rot );
			in.scale = lodiScaleValue( inst.scale, inst.flags );
			pl.push_back( in );
		}
	}

	// the ground
	LodtFile land;
	LodpTerrainIn terr;
	const bool haveLand = !o.lodlPath.isEmpty();
	if ( haveLand ) {
		if ( !land.open( o.lodlPath, &err ) )
			return fail( QStringLiteral( ".lodl: " ) + err );
		land.setBlockCacheSize( 64 );
		terr.minX = land.cellMinX();
		terr.minY = land.cellMinY();
		terr.maxX = land.cellMaxX();
		terr.maxY = land.cellMaxY();
		terr.samplesPerCell = land.samplesPerCell();
		terr.height = [&land]( int gx, int gy ) { return land.height( gx, gy ); };
		const int rx0 = o.region ? o.x0 : INT_MIN, rx1 = o.region ? o.x1 : INT_MAX;
		const int ry0 = o.region ? o.y0 : INT_MIN, ry1 = o.region ? o.y1 : INT_MAX;
		terr.cell = [&land, rx0, rx1, ry0, ry1]( int cx, int cy, bool * isLand, bool * water, float * wh ) {
			float lo = 0, hi = 0, w = 0;
			quint16 wt = 0, fl = 0;
			const bool in = cx >= rx0 && cx <= rx1 && cy >= ry0 && cy <= ry1;
			if ( !in || !land.cell( cx, cy, lo, hi, w, wt, fl ) ) {
				*isLand = false;
				*water = false;
				*wh = 0.0f;
				return;
			}
			*isLand = ( fl & 2u ) != 0;     // CELL_HAS_LAND (lodtfile.cpp)
			*water = ( fl & 1u ) != 0;      // CELL_HAS_WATER
			*wh = w;
		};
	}

	LodpPlaceOptions opt;
	opt.worldspaceHash = lodoFnv1a64( oh.worldspaceEdid.toUtf8().constData(), size_t( oh.worldspaceEdid.toUtf8().size() ) );
	opt.lodInputHash = inputHashOf( oh, ih, o.lodlPath );
	opt.lodoIdentity = identity;
	opt.bound = bound;
	std::map<std::pair<int, int>, LodpSector> sectors;
	LodpPlaceStats st;
	if ( !lodpPlace( lib, pl, haveLand ? &terr : nullptr, opt, &sectors, &st, &err ) )
		return fail( err );
	const qint64 msPlace = clock.elapsed();

	quint64 written = 0;
	if ( o.writeFiles ) {
		if ( !QDir().mkpath( o.outDir ) )
			return fail( QStringLiteral( "cannot create " ) + o.outDir );
		for ( auto & kv : sectors ) {
			LodpHeader h;
			if ( !lodpWrite( o.outDir + "/" + lodpFileName( kv.first.first, kv.first.second ), kv.second, &h, &err ) )
				return fail( err );
			written += h.fileBytes;
		}
	}
	if ( report ) {
		*report << QString( "lodp-bake: %1 (%2), .lodo %3, chart stream %4" )
			.arg( oh.worldspaceEdid ).arg( o.region ? QString( "cells %1 %2 .. %3 %4" ).arg( o.x0 ).arg( o.y0 ).arg( o.x1 ).arg( o.y1 ) : QStringLiteral( "whole worldspace" ) )
			.arg( QFileInfo( o.lodoPath ).fileName() )
			.arg( bound ? QStringLiteral( "present and agrees (BOUND)" ) : QStringLiteral( "absent (census run, not BOUND)" ) );
		*report << QString( "lodp-bake: placements read %1, placed %2, initially disabled skipped %3, no mesh %4, outside region %5" )
			.arg( read ).arg( st.placedObjects ).arg( disabled ).arg( noMesh ).arg( outside );
		*report << QString( "lodp-bake: meshes cut %1, mesh/scale keys %2, charts %3 (%4 tree), slot ids %5" )
			.arg( st.meshesCut ).arg( st.meshScaleKeys ).arg( st.charts ).arg( st.treeCharts ).arg( st.slotIds );
		*report << QString( "lodp-bake: patches %1 object + %2 tree + %3 terrain = %4; land cells %5, submerged %6, centroids moved %7" )
			.arg( st.objectPatches ).arg( st.treePatches ).arg( st.terrainPatches )
			.arg( st.objectPatches + st.treePatches + st.terrainPatches ).arg( st.landCells ).arg( st.submerged ).arg( st.centroidMoved );
		*report << QString( "lodp-bake: busiest cell %1 patches, cells over the 800 cap %2, farthest patch from its placement's cell %3 u" )
			.arg( st.maxPatchesPerCell ).arg( st.cellsOverCap ).arg( st.maxReach, 0, 'f', 1 );
		*report << QString( "lodp-bake: sectors %1, bytes %2 (%3 MiB)%4, place %5 ms" )
			.arg( st.sectors ).arg( st.bytes ).arg( double( st.bytes ) / 1048576.0, 0, 'f', 2 )
			.arg( o.writeFiles ? QString( ", written %1 bytes to %2" ).arg( written ).arg( o.outDir ) : QStringLiteral( ", census only" ) )
			.arg( msPlace );
	}
	return true;
}

bool lodpVerifyDir( const QString & dir, QStringList * report, QString * error )
{
	const QStringList files = QDir( dir ).entryList( { QStringLiteral( "lodp_*.lodp" ) }, QDir::Files, QDir::Name );
	if ( files.isEmpty() ) {
		if ( error )
			*error = QStringLiteral( "no lodp_*.lodp in " ) + dir;
		return false;
	}
	quint64 bytes = 0, patches = 0, charts = 0, instances = 0, bound = 0;
	for ( const QString & n : files ) {
		LodpHeader h;
		LodpSector s;
		QString err;
		if ( !lodpRead( dir + "/" + n, &h, &s, &err ) ) {
			if ( error )
				*error = n + QStringLiteral( ": " ) + err;
			if ( report )
				*report << QString( "lodp-verify: %1 REFUSED %2" ).arg( n, err );
			return false;
		}
		bytes += h.fileBytes;
		patches += s.patches.size();
		charts += s.charts.size();
		instances += s.instances.size();
		bound += ( h.flags & LODP_FLAG_BOUND ) ? 1 : 0;
		if ( report )
			*report << QString( "lodp-verify: %1 ACCEPTED sector %2 %3, %4 instances, %5 charts, %6 patches, %7 bytes%8" )
				.arg( n ).arg( s.sectorX ).arg( s.sectorY ).arg( s.instances.size() ).arg( s.charts.size() )
				.arg( s.patches.size() ).arg( h.fileBytes ).arg( ( h.flags & LODP_FLAG_BOUND ) ? ", BOUND" : "" );
	}
	if ( report )
		*report << QString( "lodp-verify: %1 files ACCEPTED (%2 BOUND), %3 instances, %4 charts, %5 patches, %6 bytes" )
			.arg( files.size() ).arg( bound ).arg( instances ).arg( charts ).arg( patches ).arg( bytes );
	return true;
}

/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "lodgen.h"
#include "lodgenparallel.h"
#include "lodgenao.h"

/*! THE `_n` SHEET'S LAYOUT 2 (lane IMPOSTORFIN1 hook-up, ruled by bungo):
 *  height in ALPHA, sway in BLUE. Inside the bake the height is BLUE and the
 *  sway ALPHA (`_oct_normal.png`); this swaps the two on the way into the
 *  encoder and nowhere else, because BC3's alpha is its own BC4 block and blue
 *  shares the normal's RGB565 palette (IMPOSTORFIX2 section 6: height error
 *  20-40x smaller). A .lodm naming such a sheet says `"nlayout": 2`. */
static std::vector<quint32> lodgenNLayout2( std::vector<quint32> px )
{
	for ( quint32 & p : px )
		p = ( p & 0x00FFFF00u ) | ( ( p & 0x000000FFu ) << 24 ) | ( p >> 24 );
	return px;
}

#include <QMutex>
#include <QMutexLocker>

#include "esmdata.h"
#include "nativeemit.h"
#include "lodgenlayout.h"
#include "io/material.h"
#include "io/lodmfile.h"
#include "io/lodvfile.h"
#include "io/pbrmfile.h"
#include <QJsonArray>
#include <QJsonObject>
#include "model/nifmodel.h"
#include "spells/blocks.h"

#include <QDir>
#include "gamemanager.h"
#include "ba2file.hpp"
#include <QBuffer>
#include <QPainter>
#include <memory>
#include <string_view>
#include <QFile>
#include <QTextStream>
#include <QtEndian>
#include <QFileInfo>
#include <QMap>
#include <QVector>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <limits>
#include <vector>

#include "meshoptimizer/src/meshoptimizer.h"

/* Mod Organizer 2 puts its hook DLL in every process it launches, so "am I
 * under MO2, seeing its virtual Data folder?" is one module lookup. Declared by
 * hand rather than pulling in windows.h, whose min/max macros would break
 * std::min/std::max all through this file; the signature is the real one
 * (HMODULE is struct HINSTANCE__ *), so it stays compatible if some future
 * include does bring windows.h in. */
#ifdef Q_OS_WIN32
struct HINSTANCE__;
extern "C" __declspec( dllimport ) struct HINSTANCE__ * __stdcall GetModuleHandleW( const wchar_t * );
#endif

/* Measured constants from Commonwealth.4.-20.24.BTR — see
 * docs/LODGEN_ESM_LAYOUTS.md and the 2026-08-31 dumps. The Land vertex
 * descriptor is vanilla's: flags VERTEX|UV, 12-byte stride (half3 position,
 * half bitangent X, half2 UV). Water's is position-only, 8 bytes. */
namespace
{

constexpr std::uint64_t LAND_VERTEX_DESC = 52776558133763ULL;   // 0x0000300000000203
constexpr std::uint64_t WATER_VERTEX_DESC = 17592186044418ULL;  // 0x100000000002
constexpr quint32 LAND_SHADER_FLAGS1 = 2151682048U;
constexpr quint32 LAND_SHADER_FLAGS2 = 3U;
constexpr quint32 LAND_SHADER_TYPE = 18U;    // "LOD Landscape Noise"
constexpr quint32 WATER_EFFECT_FLAGS1 = 2147483648U;
constexpr quint32 WATER_EFFECT_FLAGS2 = 1U;

QModelIndex insertAvObject( NifModel * nif, const QString & type, const QString & name,
	float scale )
{
	QModelIndex b = nif->insertNiBlock( type );
	nif->set<QString>( b, "Name", name );
	nif->set<quint32>( b, "Flags", 14 );
	nif->set<float>( b, "Scale", scale );
	return b;
}

void setBound( NifModel * nif, const QModelIndex & shape,
	float minX, float minY, float minZ, float maxX, float maxY, float maxZ )
{
	QModelIndex iBound = nif->getIndex( shape, "Bounding Sphere" );
	if ( !iBound.isValid() )
		return;
	const Vector3 c( ( minX + maxX ) * 0.5f, ( minY + maxY ) * 0.5f, ( minZ + maxZ ) * 0.5f );
	const Vector3 h( ( maxX - minX ) * 0.5f, ( maxY - minY ) * 0.5f, ( maxZ - minZ ) * 0.5f );
	nif->set<Vector3>( iBound, "Center", c );
	nif->set<float>( iBound, "Radius", h.length() );
}

//! MultiBound + AABB pair; X/Y in the given frame, per vanilla convention.
QModelIndex insertMultiBound( NifModel * nif,
	float cx, float cy, float cz, float ex, float ey, float ez )
{
	QModelIndex mb = nif->insertNiBlock( QStringLiteral( "BSMultiBound" ) );
	QModelIndex aabb = nif->insertNiBlock( QStringLiteral( "BSMultiBoundAABB" ) );
	nif->setLink( mb, "Data", nif->getBlockNumber( aabb ) );
	nif->set<Vector3>( aabb, "Position", Vector3( cx, cy, cz ) );
	nif->set<Vector3>( aabb, "Extent", Vector3( ex, ey, ez ) );
	return mb;
}

} // namespace


namespace
{
/*! Triangulate a height grid on the fixed SW-NE diagonal, as vanilla does.
 *
 *  Choosing the diagonal PER QUAD (joining the closer-in-height corners) was
 *  tried and REVERTED: it is the textbook improvement, and it measured no
 *  benefit here -- identical vertex and triangle counts, sliver share 10.7% ->
 *  11.1%, and vanilla's own vertices drifted off our surface (median error
 *  0 -> 16 world units). The simplifier reorganises the mesh enough that the
 *  initial diagonal does not survive to the output. Do not re-add it without
 *  a measurement that shows a win.
 */
static void lodgenTriangulateGrid( const std::vector<float> & grid, int n,
	std::vector<unsigned int> & idx )
{
	idx.clear();
	idx.reserve( size_t( n - 1 ) * size_t( n - 1 ) * 6 );
	for ( int row = 0; row < n - 1; row++ ) {
		for ( int col = 0; col < n - 1; col++ ) {
			const unsigned int a = (unsigned int) ( row * n + col );
			idx.push_back( a ); idx.push_back( a + 1 ); idx.push_back( a + n + 1 );
			idx.push_back( a ); idx.push_back( a + n + 1 ); idx.push_back( a + n );
		}
	}
}

/*! Per-vertex shore weight and waterline lock, for shore-aware decimation.
 *
 *  meshopt_simplify minimises HEIGHT error, and a shoreline is flat -- so
 *  collapsing coastline vertices costs it almost nothing and they go first,
 *  while the budget is spent on inland ridges nobody looks at. The waterline
 *  is the one silhouette in a LOD chunk the eye tracks, and it was the
 *  cheapest thing in the mesh to delete.
 *
 *  A vertex whose 4-neighbourhood holds terrain both above AND below its
 *  cell's water plane IS the waterline: locked, so no collapse can move it.
 *  Everything else carries a falloff so collapses near water stay expensive
 *  without being forbidden. Returns false when the chunk has no water at all,
 *  in which case the caller keeps the plain simplifier.
 */
static bool lodgenShoreWeights( const EsmWorld & world, int chunkX, int chunkY,
	int dim, const std::vector<float> & grid, int n,
	std::vector<float> & shoreAttr )
{
	constexpr float FALLOFF = 512.0f;      // world units of depth still "near shore"
	const size_t total = size_t( n ) * size_t( n );
	shoreAttr.assign( total, 0.0f );

	std::vector<float> depth( total, -3.4e38f );
	bool anyWater = false;
	for ( int row = 0; row < n; row++ ) {
		for ( int col = 0; col < n; col++ ) {
			const int cx = qBound( 0, col / 32, dim - 1 );
			const int cy = qBound( 0, row / 32, dim - 1 );
			float wh = 0.0f;
			if ( !world.cellWater( chunkX + cx, chunkY + cy, wh ) )
				continue;
			depth[size_t( row ) * size_t( n ) + size_t( col )] =
				wh - grid[size_t( row ) * size_t( n ) + size_t( col )];
			anyWater = true;
		}
	}
	if ( !anyWater )
		return false;

	bool anyShore = false;
	for ( int row = 0; row < n; row++ ) {
		for ( int col = 0; col < n; col++ ) {
			const size_t s = size_t( row ) * size_t( n ) + size_t( col );
			if ( depth[s] <= -3.0e38f )
				continue;
			bool above = depth[s] < 0.0f, below = depth[s] >= 0.0f;
			const int dx[4] = { -1, 1, 0, 0 }, dy[4] = { 0, 0, -1, 1 };
			for ( int k = 0; k < 4; k++ ) {
				const int c2 = col + dx[k], r2 = row + dy[k];
				if ( c2 < 0 || r2 < 0 || c2 >= n || r2 >= n )
					continue;
				const float d2 = depth[size_t( r2 ) * size_t( n ) + size_t( c2 )];
				if ( d2 <= -3.0e38f )
					continue;
				above = above || d2 < 0.0f;
				below = below || d2 >= 0.0f;
			}
			/* The BORDER RING is left exactly as vanilla decimates it.
			 *
			 * It is shared with the neighbouring chunk and it is what the
			 * skirt is built from, so steering it toward the shore changes
			 * which border vertices survive -- measured: the skirt lost 12 of
			 * its 104 vertices and vanilla's own vertices stopped lying on our
			 * surface, with the worst error landing at exactly 1000 units, the
			 * skirt depth. Coastline detail is an INTERIOR improvement; the
			 * border is a contract with the chunk next door. */
			if ( row == 0 || col == 0 || row == n - 1 || col == n - 1 ) {
				shoreAttr[s] = 0.0f;
				continue;
			}
			if ( above && below ) {
				shoreAttr[s] = 1.0f;
				anyShore = true;
			} else {
				shoreAttr[s] = qBound( 0.0f,
					1.0f - std::fabs( depth[s] ) / FALLOFF, 1.0f );
			}
		}
	}
	return anyShore;
}


//! Land geometry for any chunk: filled grid + decimated index buffer, in
//! that chunk's miniature space. Shared by the builder and the geomorph
//! parent-surface sampling.
bool lodgenLandGeometry( const EsmWorld & world, int chunkX, int chunkY,
	int dim, int targetTrisPerCell, std::vector<float> & pos,
	std::vector<unsigned int> & idx )
{
	const int n = dim * 32 + 1;
	std::vector<float> grid( size_t( n ) * size_t( n ), world.defaultLandHeight() );
	int landCells = 0;
	EsmLand land;
	for ( int cy = 0; cy < dim; cy++ )
		for ( int cx = 0; cx < dim; cx++ )
			if ( world.land( chunkX + cx, chunkY + cy, land ) ) {
				landCells++;
				for ( int row = 0; row < 33; row++ )
					for ( int col = 0; col < 33; col++ )
						grid[size_t( cy * 32 + row ) * n + size_t( cx * 32 + col )] =
							land.heights[row][col];
			}
	if ( !landCells )
		return false;
	const float invDim = 1.0f / float( dim );
	const float spacing = 128.0f * invDim;
	pos.clear();
	pos.reserve( size_t( n ) * n * 3 );
	for ( int row = 0; row < n; row++ )
		for ( int col = 0; col < n; col++ ) {
			pos.push_back( float( col ) * spacing );
			pos.push_back( float( row ) * spacing );
			pos.push_back( grid[size_t( row ) * n + col] * invDim );
		}
	lodgenTriangulateGrid( grid, n, idx );
	if ( targetTrisPerCell > 0 ) {
		/* The budget is per CHUNK, not per cell: vanilla holds every ring
		 * near ~2100 tris per chunk (measured Commonwealth.{4,8,16,32}.0.0:
		 * 128 -> 32 -> 8 -> 2 tris/cell), so the knob is calibrated as
		 * tris-per-cell AT DIM 4 and the far rings inherit the same chunk
		 * total. */
		const size_t targetIdx = size_t( targetTrisPerCell ) * 16 * 3;
		std::vector<unsigned int> simplified( idx.size() );
		float resultError = 0.0f;
		const size_t count = meshopt_simplify( simplified.data(), idx.data(), idx.size(),
			pos.data(), pos.size() / 3, 12, targetIdx, 0.05f, 0, &resultError );
		simplified.resize( count );
		idx.swap( simplified );
	}
	return true;
}

//! Height of a triangle surface at (x, y) in its own space; NaN when outside.
float lodgenSurfaceHeight( const std::vector<float> & pos,
	const std::vector<unsigned int> & idx, float x, float y )
{
	for ( size_t t = 0; t + 2 < idx.size(); t += 3 ) {
		const float * a = pos.data() + size_t( idx[t] ) * 3;
		const float * b = pos.data() + size_t( idx[t + 1] ) * 3;
		const float * c = pos.data() + size_t( idx[t + 2] ) * 3;
		const float d = ( b[1] - c[1] ) * ( a[0] - c[0] ) + ( c[0] - b[0] ) * ( a[1] - c[1] );
		if ( std::fabs( d ) < 1e-9f )
			continue;
		const float w0 = ( ( b[1] - c[1] ) * ( x - c[0] ) + ( c[0] - b[0] ) * ( y - c[1] ) ) / d;
		const float w1 = ( ( c[1] - a[1] ) * ( x - c[0] ) + ( a[0] - c[0] ) * ( y - c[1] ) ) / d;
		const float w2 = 1.0f - w0 - w1;
		if ( w0 < -0.001f || w1 < -0.001f || w2 < -0.001f )
			continue;
		return w0 * a[2] + w1 * b[2] + w2 * c[2];
	}
	return std::numeric_limits<float>::quiet_NaN();
}

} // namespace


namespace
{

//! Crude LTEX material class from the texture path, for the CS terrain
//! profile's R channel. Buckets are contract values, not art opinions.
quint8 lodgenMaterialClass( const QString & diffusePath )
{
	const QString p = diffusePath.toLower();
	if ( p.contains( QLatin1String( "snow" ) ) ) return 224;
	if ( p.contains( QLatin1String( "marsh" ) ) || p.contains( QLatin1String( "swamp" ) )
		|| p.contains( QLatin1String( "mud" ) ) || p.contains( QLatin1String( "wet" ) ) ) return 192;
	if ( p.contains( QLatin1String( "sand" ) ) || p.contains( QLatin1String( "beach" ) ) ) return 160;
	if ( p.contains( QLatin1String( "road" ) ) || p.contains( QLatin1String( "concrete" ) )
		|| p.contains( QLatin1String( "asphalt" ) ) || p.contains( QLatin1String( "pavement" ) ) ) return 128;
	if ( p.contains( QLatin1String( "rock" ) ) || p.contains( QLatin1String( "cliff" ) )
		|| p.contains( QLatin1String( "stone" ) ) || p.contains( QLatin1String( "gravel" ) ) ) return 96;
	if ( p.contains( QLatin1String( "forest" ) ) || p.contains( QLatin1String( "leaves" ) )
		|| p.contains( QLatin1String( "moss" ) ) ) return 64;
	if ( p.contains( QLatin1String( "grass" ) ) ) return 32;
	return 0;   // dirt / unknown
}

/*! Per-sample terrain channels for the CS profile: dominant material class,
 * flow-accumulation wetness, and heightfield AO — all per PLACEMENT, the
 * things no shared tiling texture can carry. Grid is (dim*32+1)^2.
 * Extended profile (UV2): skyVis = the horizon measure before byte
 * quantization (~11-bit half precision), matClass2 = the second-strongest
 * material class for two-material blending at distance. */
void lodgenTerrainChannels( const EsmWorld & world, int chunkX, int chunkY,
	int dim, const std::vector<float> & grid,
	std::vector<quint8> & matClass, std::vector<quint8> & wetness,
	std::vector<quint8> & ao,
	std::vector<float> & skyVis, std::vector<quint8> & matClass2,
	std::vector<quint8> & shore, std::vector<quint8> * matBlend )
{
	const int n = dim * 32 + 1;
	matClass.assign( size_t( n ) * n, 0 );
	wetness.assign( size_t( n ) * n, 0 );
	ao.assign( size_t( n ) * n, 255 );
	skyVis.assign( size_t( n ) * n, 1.0f );
	matClass2.assign( size_t( n ) * n, 0 );
	shore.assign( size_t( n ) * n, 0 );
	if ( matBlend )
		matBlend->assign( size_t( n ) * n, 0 );

	// dominant material per sample: strongest layer (or base) at the sample
	EsmLand land;
	for ( int cy = 0; cy < dim; cy++ ) {
		for ( int cx = 0; cx < dim; cx++ ) {
			if ( !world.land( chunkX + cx, chunkY + cy, land ) )
				continue;
			for ( int row = 0; row < 33; row++ ) {
				for ( int col = 0; col < 33; col++ ) {
					const int q = ( row >= 16 ? 2 : 0 ) + ( col >= 16 ? 1 : 0 );
					const int qr = ( row >= 16 ? row - 16 : row );
					const int qc = ( col >= 16 ? col - 16 : col );
					quint32 ltex = land.baseTex[q], ltex2 = 0;
					float bestA = 0.35f, secondA = 0.15f;
					for ( const EsmLandLayer & layer : land.layers[q] ) {
						const float a = layer.opacity[qr][qc];
						if ( a > bestA ) {
							secondA = bestA;
							ltex2 = ltex;
							bestA = a;
							ltex = layer.ltex;
						} else if ( a > secondA ) {
							secondA = a;
							ltex2 = layer.ltex;
						}
					}
					auto classOf = [&]( quint32 form ) -> quint8 {
						if ( !form )
							return 0;
						QString d, nrm;
						world.ltexTextures( form, d, nrm );
						return lodgenMaterialClass( d );
					};
					const size_t s = size_t( cy * 32 + row ) * n + size_t( cx * 32 + col );
					matClass[s] = classOf( ltex );
					matClass2[s] = ltex2 ? classOf( ltex2 ) : matClass[s];
					/* The BLEND WEIGHT between them -- the share belonging to
					 * the second material, 0 = pure dominant, 128 = an even
					 * mix. Both opacities were already computed here and then
					 * discarded, which left the documented "second-strongest
					 * class, for two-material blending" unusable: the pair says
					 * WHICH two materials meet and never in what proportion. */
					if ( matBlend ) {
						const float total = bestA + secondA;
						( *matBlend )[s] = total > 1.0e-4f
							? quint8( qBound( 0.0f, secondA / total * 255.0f, 255.0f ) )
							: 0;
					}
				}
			}
		}
	}

	/* Shore proximity: how close this sample is to standing water, in both
	 * senses at once -- how far it sits ABOVE the water plane, and how far it
	 * is FROM any water at all.
	 *
	 * It has to live on the terrain's vertices because LOD water cannot carry
	 * it: the water shapes are position-only, 8 bytes a vertex
	 * (WATER_VERTEX_DESC), under their own BSEffectShaderProperty. There is
	 * nowhere to put a channel on them.
	 *
	 * Water height is per CELL, but terrain height is per SAMPLE, so the band
	 * this produces still follows the true shoreline contour rather than a
	 * 4096-unit staircase -- the contour comes from the terrain, not the water.
	 * The exposure rule is the water shapes' own (height above the cell's
	 * terrain minimum), so a shore band never appears around water that was
	 * culled for being submerged.
	 */
	{
		constexpr float HEIGHT_RANGE = 512.0f;   // world units above water -> 0
		constexpr int MAX_STEPS = 32;            // samples; 32 * 128u = 4096u
		std::vector<float> wZ( size_t( n ) * n, 0.0f );
		std::vector<int> dist( size_t( n ) * n, -1 );
		std::vector<int> queue;
		for ( int cy = 0; cy < dim; cy++ ) {
			for ( int cx = 0; cx < dim; cx++ ) {
				float h = 0.0f;
				if ( !world.cellWater( chunkX + cx, chunkY + cy, h ) )
					continue;
				float cellMin = 3.4e38f;
				for ( int row = cy * 32; row <= cy * 32 + 32; row++ )
					for ( int col = cx * 32; col <= cx * 32 + 32; col++ )
						cellMin = qMin( cellMin, grid[size_t( row ) * n + col] );
				if ( !( h > cellMin ) )
					continue;                     // submerged: no water drawn, no shore
				for ( int row = cy * 32; row <= cy * 32 + 32; row++ ) {
					for ( int col = cx * 32; col <= cx * 32 + 32; col++ ) {
						const size_t s = size_t( row ) * n + col;
						if ( dist[s] < 0 || h > wZ[s] ) {
							wZ[s] = h;
							if ( dist[s] < 0 ) {
								dist[s] = 0;
								queue.push_back( int( s ) );
							}
						}
					}
				}
			}
		}
		// Breadth-first spread, so a dry sample inherits the height of the
		// nearest water rather than of whichever cell happens to be scanned.
		for ( size_t qi = 0; qi < queue.size(); qi++ ) {
			const int s = queue[qi];
			if ( dist[size_t( s )] >= MAX_STEPS )
				continue;
			const int row = s / n, col = s % n;
			for ( int dy = -1; dy <= 1; dy++ ) {
				for ( int dx = -1; dx <= 1; dx++ ) {
					const int r2 = row + dy, c2 = col + dx;
					if ( r2 < 0 || c2 < 0 || r2 >= n || c2 >= n )
						continue;
					const size_t s2 = size_t( r2 ) * n + c2;
					if ( dist[s2] >= 0 )
						continue;
					dist[s2] = dist[size_t( s )] + 1;
					wZ[s2] = wZ[size_t( s )];
					queue.push_back( int( s2 ) );
				}
			}
		}
		for ( size_t s = 0; s < shore.size(); s++ ) {
			if ( dist[s] < 0 )
				continue;                         // no water within reach
			const float above = grid[s] - wZ[s];
			const float byHeight = qBound( 0.0f, 1.0f - above / HEIGHT_RANGE, 1.0f );
			const float byDist = qBound( 0.0f,
				1.0f - float( dist[s] ) / float( MAX_STEPS ), 1.0f );
			shore[s] = quint8( qBound( 0.0f, byHeight * byDist * 255.0f, 255.0f ) );
		}
	}

	/* Wetness by flow accumulation: rain lands one unit everywhere, flows
	 * to the lowest 8-neighbour repeatedly. Cells passed by more flow are
	 * wetter (hollows, gullies, drainage lines). Log-compressed. */
	{
		std::vector<int> order( size_t( n ) * n );
		for ( size_t i = 0; i < order.size(); i++ )
			order[i] = int( i );
		std::sort( order.begin(), order.end(), [&]( int a, int b ) {
			return grid[size_t( a )] > grid[size_t( b )];
		} );
		std::vector<float> flow( size_t( n ) * n, 1.0f );
		for ( int i : order ) {
			const int row = i / n, col = i % n;
			float bestH = grid[size_t( i )];
			int bestJ = -1;
			for ( int dy = -1; dy <= 1; dy++ ) {
				for ( int dx = -1; dx <= 1; dx++ ) {
					const int r2 = row + dy, c2 = col + dx;
					if ( r2 < 0 || c2 < 0 || r2 >= n || c2 >= n || ( !dx && !dy ) )
						continue;
					if ( grid[size_t( r2 ) * n + c2] < bestH ) {
						bestH = grid[size_t( r2 ) * n + c2];
						bestJ = r2 * n + c2;
					}
				}
			}
			if ( bestJ >= 0 )
				flow[size_t( bestJ )] += flow[size_t( i )];
		}
		for ( size_t i = 0; i < flow.size(); i++ ) {
			const float w = std::log2( flow[i] ) / 12.0f;   // ~4096 max
			wetness[i] = quint8( qBound( 0.0f, w, 1.0f ) * 255.0f + 0.5f );
		}
	}

	/* Heightfield AO: horizon sampling in 8 directions, how much sky the
	 * sample sees over its neighbourhood. */
	{
		const float spacing = 128.0f;
		static const int dirs[8][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
			{ 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };
		for ( int row = 0; row < n; row++ ) {
			for ( int col = 0; col < n; col++ ) {
				const float h0 = grid[size_t( row ) * n + col];
				float occl = 0.0f;
				for ( const auto & d : dirs ) {
					float maxSlope = 0.0f;
					for ( int step = 1; step <= 16; step += ( step < 4 ? 1 : 3 ) ) {
						const int r2 = row + d[1] * step, c2 = col + d[0] * step;
						if ( r2 < 0 || c2 < 0 || r2 >= n || c2 >= n )
							break;
						const float dh = grid[size_t( r2 ) * n + c2] - h0;
						if ( dh > 0.0f ) {
							const float dist = float( step ) * spacing
								* ( ( d[0] && d[1] ) ? 1.41421f : 1.0f );
							maxSlope = qMax( maxSlope, dh / dist );
						}
					}
					occl += maxSlope / ( 1.0f + maxSlope );
				}
				const float vis = qBound( 0.0f, 1.0f - occl / 8.0f * 1.6f, 1.0f );
				skyVis[size_t( row ) * n + col] = vis;
				ao[size_t( row ) * n + col] = quint8( vis * 255.0f + 0.5f );
			}
		}
	}
}

} // namespace

/*! One emitted water leaf, in whole-chunk BLOCK coordinates.
 *
 *  A "block" is the finest cell subdivision in play (1 << subdiv per cell
 *  edge), so every leaf corner AND every hanging midpoint lands on an integer
 *  block coordinate. That is what lets vertices be welded by exact integer
 *  key instead of by comparing floats.
 */
struct WaterLeaf
{
	int gx, gy;     //!< SW corner, in blocks from the chunk's SW corner
	int size;       //!< edge length in blocks (a power of two)
	int level;      //!< subdivision level; size == (1 << subdiv) >> level
};

/*! Desired refinement level for one block, from the terrain under it.
 *
 *  The waterline crossing a block is what earns the finest level: it is where
 *  foam, shoaling and the land edge all are. Merely shallow water gets one
 *  level less, open water none at all -- so depth costs nothing offshore.
 */
static int waterDesiredLevel( const std::vector<float> & grid, int n,
	int cx, int cy, int bi, int bj, int blocksPerCell, float waterHWorld, int maxLevel )
{
	constexpr float SHALLOW = 512.0f;   // world units of depth still "near shore"

	const float per = 32.0f / float( blocksPerCell );
	const int c0 = qBound( 0, int( std::floor( float( bi ) * per ) ), 32 );
	const int c1 = qBound( 0, int( std::ceil( float( bi + 1 ) * per ) ), 32 );
	const int r0 = qBound( 0, int( std::floor( float( bj ) * per ) ), 32 );
	const int r1 = qBound( 0, int( std::ceil( float( bj + 1 ) * per ) ), 32 );
	float tMin = 3.4e38f, tMax = -3.4e38f;
	for ( int r = r0; r <= r1; r++ ) {
		for ( int c = c0; c <= c1; c++ ) {
			const float t = grid[size_t( cy * 32 + r ) * size_t( n ) + size_t( cx * 32 + c )];
			tMin = qMin( tMin, t );
			tMax = qMax( tMax, t );
		}
	}
	const float depthShallowest = waterHWorld - tMax;
	const float depthDeepest = waterHWorld - tMin;
	if ( depthShallowest <= 0.0f && depthDeepest > 0.0f )
		return maxLevel;                          // the waterline runs through here
	if ( depthShallowest > 0.0f && depthShallowest < SHALLOW )
		return qMax( 0, maxLevel - 1 );           // shoaling water
	return 0;
}

/*! Build the leaves of one height layer as a 2:1 RESTRICTED quadtree.
 *
 *  Restriction matters here for a reason that is easy to get wrong. Water is
 *  flat, so an unbalanced tree produces no geometric crack -- neighbouring
 *  leaves at different depths share an exactly coplanar edge. But the moment a
 *  per-vertex CHANNEL rides on these vertices, a hanging node becomes a DATA
 *  seam: the coarse side interpolates linearly between its two edge endpoints
 *  while the fine side passes through the midpoint vertex, and the two agree
 *  only if that midpoint happens to hold the average of its neighbours. It
 *  holds the sampled field instead -- and the field is non-linear exactly
 *  where we chose to refine. So the mismatch would be largest at the
 *  shoreline, which is the one place it would be seen.
 *
 *  Restricting to one level of difference bounds a coarse edge to at most ONE
 *  hanging node, which the caller then stitches into the coarse leaf's own
 *  triangle fan -- making the midpoint a real vertex on both sides, so the
 *  interpolation matches by construction rather than by luck.
 */
/*! Is every terrain sample under this block above the water plane?
 *
 *  The margin keeps a block whose terrain merely grazes the surface: a
 *  shoreline block must survive, and the decimated Land mesh does not sit
 *  exactly on the heightfield.
 */
static bool waterBlockBuried( const std::vector<float> & grid, int n,
	int cx, int cy, int bi, int bj, int blocksPerCell, float waterHWorld )
{
	constexpr float MARGIN = 128.0f;      // world units of terrain above water
	const float per = 32.0f / float( blocksPerCell );
	const int c0 = qBound( 0, int( std::floor( float( bi ) * per ) ), 32 );
	const int c1 = qBound( 0, int( std::ceil( float( bi + 1 ) * per ) ), 32 );
	const int r0 = qBound( 0, int( std::floor( float( bj ) * per ) ), 32 );
	const int r1 = qBound( 0, int( std::ceil( float( bj + 1 ) * per ) ), 32 );
	for ( int r = r0; r <= r1; r++ )
		for ( int c = c0; c <= c1; c++ )
			if ( grid[size_t( cy * 32 + r ) * size_t( n ) + size_t( cx * 32 + c )]
				<= waterHWorld + MARGIN )
				return false;             // something here could still be wet
	return true;
}

static void waterBuildLeaves( const std::vector<float> & grid, int n, int dim,
	const QVector<QPair<int, int>> & cells, float waterHWorld, int maxLevel,
	bool cullBuried,
	QVector<WaterLeaf> & leaves, std::vector<int> & leafIdx, int & blocksPerCell )
{
	blocksPerCell = 1 << maxLevel;
	const int W = dim * blocksPerCell;
	std::vector<int> want( size_t( W ) * size_t( W ), -1 );   // -1 = not this layer's water

	for ( const auto & cell : cells ) {
		for ( int bj = 0; bj < blocksPerCell; bj++ ) {
			for ( int bi = 0; bi < blocksPerCell; bi++ ) {
				const int gx = cell.first * blocksPerCell + bi;
				const int gy = cell.second * blocksPerCell + bj;
				/* Buried block: every terrain sample under it stands above the
				 * water plane by the margin, so no water here is visible.
				 * Vanilla can only cull at CELL granularity, which leaves the
				 * part of a 4096-unit cell that sits under a hill drawn,
				 * blended and then depth-rejected. Marking it not-water here
				 * removes it from the tree, and the edge walk already treats a
				 * missing neighbour as "no split", so the hole costs nothing.
				 *
				 * Conservative on purpose -- the test is on the leaf's LOWEST
				 * sample, so a block is dropped only when nothing in it could
				 * be exposed. */
				if ( cullBuried && maxLevel > 0
					&& waterBlockBuried( grid, n, cell.first, cell.second,
						bi, bj, blocksPerCell, waterHWorld ) )
					continue;
				want[size_t( gy ) * size_t( W ) + size_t( gx )] = waterDesiredLevel(
					grid, n, cell.first, cell.second, bi, bj, blocksPerCell,
					waterHWorld, maxLevel );
			}
		}
	}

	/* 2:1 restriction, by relaxation: a block may not sit more than one level
	 * below any 4-neighbour that carries water. Runs to a fixed point; each
	 * sweep can only raise levels, so it terminates. */
	for ( bool changed = true; changed; ) {
		changed = false;
		for ( int gy = 0; gy < W; gy++ ) {
			for ( int gx = 0; gx < W; gx++ ) {
				const size_t s = size_t( gy ) * size_t( W ) + size_t( gx );
				if ( want[s] < 0 )
					continue;
				int hi = 0;
				const int dx[4] = { -1, 1, 0, 0 }, dy[4] = { 0, 0, -1, 1 };
				for ( int k = 0; k < 4; k++ ) {
					const int nx = gx + dx[k], ny = gy + dy[k];
					if ( nx < 0 || ny < 0 || nx >= W || ny >= W )
						continue;
					hi = qMax( hi, want[size_t( ny ) * size_t( W ) + size_t( nx )] );
				}
				if ( want[s] < hi - 1 ) {
					want[s] = hi - 1;
					changed = true;
				}
			}
		}
	}

	// Descend each cell: a node is a leaf once no block under it wants finer.
	leafIdx.assign( size_t( W ) * size_t( W ), -1 );
	std::function<void( int, int, int, int )> descend =
		[&]( int gx, int gy, int size, int level )
	{
		int hi = 0;
		for ( int y = gy; y < gy + size; y++ )
			for ( int x = gx; x < gx + size; x++ )
				hi = qMax( hi, want[size_t( y ) * size_t( W ) + size_t( x )] );
		if ( level >= hi || size == 1 ) {
			const int id = leaves.size();
			leaves.append( WaterLeaf{ gx, gy, size, level } );
			for ( int y = gy; y < gy + size; y++ )
				for ( int x = gx; x < gx + size; x++ )
					leafIdx[size_t( y ) * size_t( W ) + size_t( x )] = id;
			return;
		}
		const int h = size / 2;
		descend( gx, gy, h, level + 1 );
		descend( gx + h, gy, h, level + 1 );
		descend( gx, gy + h, h, level + 1 );
		descend( gx + h, gy + h, h, level + 1 );
	};
	for ( const auto & cell : cells )
		descend( cell.first * blocksPerCell, cell.second * blocksPerCell,
			blocksPerCell, 0 );
}

bool lodgenBuildTerrainChunk( NifModel * nif, const EsmWorld & world,
	int chunkX, int chunkY, const LodgenTerrainOptions & opts, QString * error )
{
	auto fail = [error]( const QString & message ) {
		if ( error )
			*error = message;
		return false;
	};
	if ( !nif )
		return fail( QStringLiteral( "no model" ) );
	const int dim = opts.dim;
	if ( dim != 4 && dim != 8 && dim != 16 && dim != 32 )
		return fail( QStringLiteral( "dim must be 4, 8, 16 or 32" ) );
	if ( chunkX % dim || chunkY % dim )
		return fail( QString( "chunk (%1,%2) is not aligned to dim %3" )
			.arg( chunkX ).arg( chunkY ).arg( dim ) );

	/* One height grid for the chunk: dim*32+1 samples per side. Adjacent
	 * cells duplicate their shared 33rd row/column in the ESM, so plain
	 * overwrite converges; cells with no LAND fall back to the worldspace
	 * default height. */
	const int n = dim * 32 + 1;
	std::vector<float> grid( size_t( n ) * size_t( n ), world.defaultLandHeight() );
	int landCells = 0;
	EsmLand land;
	for ( int cy = 0; cy < dim; cy++ ) {
		for ( int cx = 0; cx < dim; cx++ ) {
			if ( !world.land( chunkX + cx, chunkY + cy, land ) )
				continue;
			landCells++;
			for ( int row = 0; row < 33; row++ )
				for ( int col = 0; col < 33; col++ )
					grid[size_t( cy * 32 + row ) * n + size_t( cx * 32 + col )] =
						land.heights[row][col];
		}
	}
	if ( !landCells )
		return fail( QString( "no LAND in chunk (%1,%2)x%3" ).arg( chunkX ).arg( chunkY ).arg( dim ) );

	if ( !nif->createNew( 0x14020007, 12, 130 ) )
		return fail( QStringLiteral( "could not create a Fallout 4 document" ) );
	nif->holdUpdates( true );

	QModelIndex iRoot = insertAvObject( nif, QStringLiteral( "BSMultiBoundNode" ),
		QStringLiteral( "chunk" ), 1.0f );
	nif->set<quint32>( iRoot, "Culling Mode", 1 );

	// ---- Land ---------------------------------------------------------
	QModelIndex iLand = insertAvObject( nif, QStringLiteral( "BSTriShape" ),
		QStringLiteral( "Land" ), float( dim ) );

	const float invDim = 1.0f / float( dim );
	const float spacing = 128.0f * invDim;   // 128 world units between samples

	/* Geometry in plain buffers first, so the simplifier can run before any
	 * model writes. Vanilla adds a SKIRT: the border ring duplicated exactly
	 * 1000 world units lower, joined by vertical flaps (measured on
	 * Commonwealth.4.-20.24.BTR: border positions appear TWICE). The
	 * simplifier locks the border so the skirt stays exact. */
	constexpr float SKIRT_DROP = 1000.0f;
	std::vector<float> pos;
	pos.reserve( size_t( n ) * n * 3 );
	for ( int row = 0; row < n; row++ ) {
		for ( int col = 0; col < n; col++ ) {
			pos.push_back( float( col ) * spacing );
			pos.push_back( float( row ) * spacing );
			pos.push_back( grid[size_t( row ) * n + col] * invDim );
		}
	}
	std::vector<unsigned int> idx;
	lodgenTriangulateGrid( grid, n, idx );
	if ( opts.targetTrisPerCell > 0 ) {
		// per-CHUNK budget calibrated at dim 4 — see lodgenLandGeometry
		const size_t targetIdx = size_t( opts.targetTrisPerCell ) * 16 * 3;
		std::vector<unsigned int> simplified( idx.size() );
		float resultError = 0.0f;
		/* No border lock: vanilla decimates its border too — cracks between
		 * neighbouring chunks are exactly what the skirt exists to hide.
		 *
		 * The waterline IS locked, though. Same triangle budget, redistributed
		 * from inland ridges to the coast: the weight is in miniature position
		 * units (samples sit `spacing` apart), so a full swing of the shore
		 * attribute costs about half a sample of geometric error -- enough to
		 * outbid a flat beach's near-zero height error. */
		std::vector<float> shoreAttr;
		const bool shoreAware = opts.shoreDenser
			&& lodgenShoreWeights( world, chunkX, chunkY, dim, grid, n, shoreAttr );
		/* Weight in miniature position units (samples sit `spacing` apart), so
		 * density 2 makes a full swing of the shore attribute cost about half a
		 * sample of geometric error -- already enough to outbid a flat beach's
		 * near-zero height error. Higher densities approach a hard lock without
		 * ever becoming one, so the simplifier always converges. */
		const float shoreWeight = spacing * 0.25f
			* float( qBound( 1, opts.shoreDensity, 10 ) );
		const size_t count = shoreAware
			? meshopt_simplifyWithAttributes( simplified.data(), idx.data(), idx.size(),
				pos.data(), pos.size() / 3, 12,
				shoreAttr.data(), sizeof( float ), &shoreWeight, 1,
				nullptr, targetIdx, 0.05f, 0, &resultError )
			: meshopt_simplify( simplified.data(), idx.data(), idx.size(),
				pos.data(), pos.size() / 3, 12, targetIdx, 0.05f, 0, &resultError );
		simplified.resize( count );
		idx.swap( simplified );
	}

	// compact to the surviving vertices
	std::vector<unsigned int> remap( pos.size() / 3, 0xFFFFFFFFU );
	std::vector<float> cpos;
	cpos.reserve( pos.size() );
	for ( unsigned int & i : idx ) {
		if ( remap[i] == 0xFFFFFFFFU ) {
			remap[i] = (unsigned int) ( cpos.size() / 3 );
			cpos.push_back( pos[size_t( i ) * 3] );
			cpos.push_back( pos[size_t( i ) * 3 + 1] );
			cpos.push_back( pos[size_t( i ) * 3 + 2] );
		}
		i = remap[i];
	}

	/* Skirt: walk the perimeter (CCW from above) keeping only the vertices
	 * the simplifier retained, duplicate them one SKIRT_DROP lower, and flap
	 * between consecutive survivors. The flaps span whatever gaps decimation
	 * opened along the border — the same reason vanilla's skirt exists. */
	{
		QVector<unsigned int> survivors;
		auto keep = [&]( int row, int col ) {
			const unsigned int old = (unsigned int) ( row * n + col );
			if ( remap[old] != 0xFFFFFFFFU )
				survivors.append( remap[old] );
		};
		for ( int col = 0; col < n - 1; col++ ) keep( 0, col );
		for ( int row = 0; row < n - 1; row++ ) keep( row, n - 1 );
		for ( int col = n - 1; col > 0; col-- ) keep( n - 1, col );
		for ( int row = n - 1; row > 0; row-- ) keep( row, 0 );
		const size_t ringStart = cpos.size() / 3;
		for ( unsigned int top : survivors ) {
			cpos.push_back( cpos[size_t( top ) * 3] );
			cpos.push_back( cpos[size_t( top ) * 3 + 1] );
			cpos.push_back( cpos[size_t( top ) * 3 + 2] - SKIRT_DROP * invDim );
		}
		for ( int k = 0; k < survivors.size(); k++ ) {
			const int k2 = ( k + 1 ) % survivors.size();
			const unsigned int t0 = survivors[k], t1 = survivors[k2];
			const unsigned int b0 = (unsigned int) ( ringStart + k );
			const unsigned int b1 = (unsigned int) ( ringStart + size_t( k2 ) );
			idx.push_back( t0 ); idx.push_back( b0 ); idx.push_back( t1 );
			idx.push_back( t1 ); idx.push_back( b0 ); idx.push_back( b1 );
		}
	}

	const quint32 numVerts = quint32( cpos.size() / 3 );
	const quint32 numTris = quint32( idx.size() / 3 );
	if ( numVerts > 65535 )
		return fail( QStringLiteral( "vertex count exceeds the u16 limit" ) );

	/* Geomorph weights (CS profile): per vertex, the WORLD-unit height
	 * delta to the parent ring's surface (dim*2), stored in Eye Data. A
	 * shader lerping z toward z+delta as the swap distance approaches makes
	 * the chunk morph into an exact copy of its parent before the swap --
	 * the transition neither Bethesda game had. NaN-outside samples and
	 * dim=32 (no parent) store 0. */
	std::vector<float> morph;
	if ( opts.geomorph && dim < 32 ) {
		const int pDim = dim * 2;
		const int pX = ( chunkX >= 0 ? chunkX - chunkX % pDim
			: -( ( -chunkX + pDim - 1 ) / pDim ) * pDim );
		const int pY = ( chunkY >= 0 ? chunkY - chunkY % pDim
			: -( ( -chunkY + pDim - 1 ) / pDim ) * pDim );
		std::vector<float> ppos;
		std::vector<unsigned int> pidx;
		if ( lodgenLandGeometry( world, pX, pY, pDim, opts.targetTrisPerCell,
			ppos, pidx ) ) {
			morph.resize( numVerts, 0.0f );
			for ( quint32 v = 0; v < numVerts; v++ ) {
				// our miniature -> world -> parent miniature
				const float wx = cpos[size_t( v ) * 3] * dim + float( chunkX ) * 4096.0f;
				const float wy = cpos[size_t( v ) * 3 + 1] * dim + float( chunkY ) * 4096.0f;
				const float pxm = ( wx - float( pX ) * 4096.0f ) / float( pDim );
				const float pym = ( wy - float( pY ) * 4096.0f ) / float( pDim );
				const float pz = lodgenSurfaceHeight( ppos, pidx, pxm, pym );
				if ( pz == pz )     // not NaN
					morph[v] = pz * pDim - cpos[size_t( v ) * 3 + 2] * dim;
			}
		}
	}
	float zMin = 3.4e38f, zMax = -3.4e38f;
	for ( size_t v = 0; v < cpos.size(); v += 3 ) {
		zMin = qMin( zMin, cpos[v + 2] );
		zMax = qMax( zMax, cpos[v + 2] );
	}

	std::vector<quint8> tMat, tWet, tAo, tMat2, tShore;
	std::vector<float> tSky;
	if ( opts.terrainIdentity )
		lodgenTerrainChannels( world, chunkX, chunkY, dim, grid,
			tMat, tWet, tAo, tSky, tMat2, tShore, nullptr );

	BSVertexDesc landDesc( LAND_VERTEX_DESC );
	quint32 landStride = 12;
	if ( !morph.empty() || opts.terrainIdentity ) {
		if ( !morph.empty() )
			landDesc.SetFlag( VertexFlags::VF_EYEDATA );
		if ( opts.terrainIdentity ) {
			landDesc.SetFlag( VertexFlags::VF_COLORS );
			// extended profile: UV2.x sky visibility (half precision),
			// UV2.y second material class
			landDesc.SetFlag( VertexFlags::VF_UV_2 );
		}
		landDesc.ResetAttributeOffsets( 130 );
		landStride = landDesc.GetVertexSize();
	}
	nif->set<BSVertexDesc>( iLand, "Vertex Desc", landDesc.Value() );
	nif->set<quint32>( iLand, "Num Vertices", numVerts );
	nif->set<quint32>( iLand, "Num Triangles", numTris );
	nif->set<quint32>( iLand, "Data Size", numVerts * landStride + numTris * 6 );

	nif->setState( BaseModel::Processing );
	QModelIndex iVertexData = nif->getIndex( iLand, "Vertex Data" );
	nif->updateArraySize( iVertexData );
	for ( quint32 v = 0; v < numVerts; v++ ) {
		const float x = cpos[size_t( v ) * 3], y = cpos[size_t( v ) * 3 + 1],
			z = cpos[size_t( v ) * 3 + 2];
		QModelIndex row = nif->index( int( v ), 0, iVertexData );
		nif->set<HalfVector3>( row, "Vertex", HalfVector3( Vector3( x, y, z ) ) );
		nif->set<float>( row, "Bitangent X", 1.0f );
		nif->set<HalfVector2>( row, "UV",
			HalfVector2( Vector2( x / 4096.0f, 1.0f - y / 4096.0f ) ) );
		if ( !morph.empty() )
			nif->set<float>( row, "Eye Data", morph[v] );
		if ( opts.terrainIdentity ) {
			// vertex grid position recovers the sample index (skirt verts
			// share their top's x,y and take the same channels)
			const int col = qBound( 0, int( x / spacing + 0.5f ), n - 1 );
			const int rowIdx = qBound( 0, int( y / spacing + 0.5f ), n - 1 );
			const size_t s = size_t( rowIdx ) * n + col;
			// A = shore proximity. It was the terrain profile's last free slot.
			nif->set<ByteColor4>( row, "Vertex Colors", ByteColor4( FloatVector4(
				float( tMat[s] ) / 255.0f, float( tWet[s] ) / 255.0f,
				float( tAo[s] ) / 255.0f, float( tShore[s] ) / 255.0f ) ) );
			nif->set<HalfVector2>( row, "UV 2", HalfVector2( Vector2(
				tSky[s], float( tMat2[s] ) / 255.0f ) ) );
		}
	}
	{
		QVector<Triangle> tris;
		tris.reserve( int( numTris ) );
		for ( size_t t = 0; t < idx.size(); t += 3 )
			tris.append( Triangle( quint16( idx[t] ), quint16( idx[t + 1] ),
				quint16( idx[t + 2] ) ) );
		QModelIndex iTriangles = nif->getIndex( iLand, "Triangles" );
		nif->updateArraySize( iTriangles );
		nif->setArray<Triangle>( iTriangles, tris );
	}
	setBound( nif, iLand, 0.0f, 0.0f, zMin, 4096.0f, 4096.0f, zMax );
	nif->restoreState();

	// Land shader: vanilla's LOD-landscape type + the chunk's baked textures
	QModelIndex iShader = nif->insertNiBlock( QStringLiteral( "BSLightingShaderProperty" ) );
	nif->set<quint32>( iShader, "Shader Type", LAND_SHADER_TYPE );
	nif->set<quint32>( iShader, "Shader Flags 1", LAND_SHADER_FLAGS1 );
	nif->set<quint32>( iShader, "Shader Flags 2", LAND_SHADER_FLAGS2 );
	QModelIndex iTextures = nif->insertNiBlock( QStringLiteral( "BSShaderTextureSet" ) );
	nif->setLink( iShader, "Texture Set", nif->getBlockNumber( iTextures ) );
	nif->set<uint>( iTextures, "Num Textures", 10 );
	nif->updateArraySize( iTextures, "Textures" );
	QModelIndex iTexArray = nif->getIndex( iTextures, "Textures" );
	const QString diffuse = opts.textureBase
		.arg( world.worldspaceEdid() ).arg( dim ).arg( chunkX ).arg( chunkY );
	QString msn = diffuse;
	msn.replace( QStringLiteral( ".DDS" ), QStringLiteral( "_msn.DDS" ) );
	nif->set<QString>( nif->getIndex( iTexArray, 0 ), diffuse );
	nif->set<QString>( nif->getIndex( iTexArray, 1 ), msn );
	nif->setLink( iLand, "Shader Property", nif->getBlockNumber( iShader ) );
	addLink( nif, iRoot, QStringLiteral( "Children" ), nif->getBlockNumber( iLand ) );

	// ---- Water --------------------------------------------------------
	float waterZMin = 3.4e38f, waterZMax = -3.4e38f;
	int wetCells = 0;
	QModelIndex iWaterNode;
	if ( opts.water ) {
		/* Wet cells whose water is EXPOSED above the cell's terrain minimum.
		 * Vanilla's rule, measured both ways: the harbor chunk 0,0 (all
		 * cells sentinel-XCLW at the 450 default, seabed below) gets quads
		 * at exactly 450, while Sanctuary's default-height cells (terrain
		 * 3000+) get none — submerged water is culled at generation. */
		QMap<float, QVector<QPair<int, int>>> byHeight;
		for ( int cy = 0; cy < dim; cy++ ) {
			for ( int cx = 0; cx < dim; cx++ ) {
				float h = 0.0f;
				if ( !world.cellWater( chunkX + cx, chunkY + cy, h ) )
					continue;
				float cellMin = 3.4e38f;
				for ( int row = cy * 32; row <= cy * 32 + 32; row++ )
					for ( int col = cx * 32; col <= cx * 32 + 32; col++ )
						cellMin = qMin( cellMin, grid[size_t( row ) * n + col] );
				if ( h > cellMin )
					byHeight[h].append( qMakePair( cx, cy ) );
			}
		}
		if ( !byHeight.isEmpty() ) {
			iWaterNode = insertAvObject( nif, QStringLiteral( "BSMultiBoundNode" ),
				QStringLiteral( "WATER" ), 1.0f );
			nif->set<quint32>( iWaterNode, "Culling Mode", 1 );
			if ( dim > 4 ) {
				/* Far rings, vanilla type: ONE plain BSTriShape holding every
				 * exposed cell quad at its own height — no segments (per-cell
				 * hiding exists only at dim 4), no per-height split. */
				int quads = 0;
				for ( auto it = byHeight.constBegin(); it != byHeight.constEnd(); ++it )
					quads += it.value().size();
				QModelIndex iWater = insertAvObject( nif,
					QStringLiteral( "BSTriShape" ), QString(), float( dim ) );
				nif->set<BSVertexDesc>( iWater, "Vertex Desc", WATER_VERTEX_DESC );
				nif->set<quint32>( iWater, "Num Vertices", quint32( quads * 4 ) );
				nif->set<quint32>( iWater, "Num Triangles", quint32( quads * 2 ) );
				nif->set<quint32>( iWater, "Data Size",
					quint32( quads * 4 * 8 + quads * 2 * 6 ) );
				nif->setState( BaseModel::Processing );
				QModelIndex iWV = nif->getIndex( iWater, "Vertex Data" );
				nif->updateArraySize( iWV );
				QVector<Triangle> wtris;
				const float cellSpan = 4096.0f * invDim;
				float wMinX = 3.4e38f, wMinY = 3.4e38f, wMinZ = 3.4e38f;
				float wMaxX = -3.4e38f, wMaxY = -3.4e38f, wMaxZ = -3.4e38f;
				int vBase = 0;
				for ( auto it = byHeight.constBegin(); it != byHeight.constEnd(); ++it ) {
					const float hWorld = it.key();
					const float h = hWorld * invDim;
					waterZMin = qMin( waterZMin, hWorld );
					waterZMax = qMax( waterZMax, hWorld );
					wetCells += it.value().size();
					for ( const auto & cell : it.value() ) {
						const float x0 = float( cell.first ) * cellSpan;
						const float y0 = float( cell.second ) * cellSpan;
						const float corner[4][2] = {
							{ x0, y0 }, { x0 + cellSpan, y0 },
							{ x0 + cellSpan, y0 + cellSpan }, { x0, y0 + cellSpan } };
						for ( int k = 0; k < 4; k++ ) {
							QModelIndex v = nif->index( vBase + k, 0, iWV );
							nif->set<HalfVector3>( v, "Vertex",
								HalfVector3( Vector3( corner[k][0], corner[k][1], h ) ) );
							nif->set<float>( v, "Bitangent X", 1.0f );
							wMinX = qMin( wMinX, corner[k][0] ); wMaxX = qMax( wMaxX, corner[k][0] );
							wMinY = qMin( wMinY, corner[k][1] ); wMaxY = qMax( wMaxY, corner[k][1] );
						}
						wMinZ = qMin( wMinZ, h ); wMaxZ = qMax( wMaxZ, h );
						wtris.append( Triangle( quint16( vBase ), quint16( vBase + 1 ), quint16( vBase + 2 ) ) );
						wtris.append( Triangle( quint16( vBase ), quint16( vBase + 2 ), quint16( vBase + 3 ) ) );
						vBase += 4;
					}
				}
				QModelIndex iWT = nif->getIndex( iWater, "Triangles" );
				nif->updateArraySize( iWT );
				nif->setArray<Triangle>( iWT, wtris );
				setBound( nif, iWater, wMinX, wMinY, wMinZ, wMaxX, wMaxY, wMaxZ );
				nif->restoreState();
				QModelIndex iEffect = nif->insertNiBlock( QStringLiteral( "BSEffectShaderProperty" ) );
				nif->set<quint32>( iEffect, "Shader Flags 1", WATER_EFFECT_FLAGS1 );
				nif->set<quint32>( iEffect, "Shader Flags 2", WATER_EFFECT_FLAGS2 );
				nif->set<int>( iEffect, "Lighting Influence", 255 );
				nif->set<float>( iEffect, "Soft Falloff Depth", 100.0f );
				nif->set<float>( iEffect, "Environment Map Scale", 1.0f );
				nif->set<Color4>( iEffect, "Base Color", Color4( 1.0f, 1.0f, 1.0f, 1.0f ) );
				nif->set<float>( iEffect, "Base Color Scale", 1.0f );
				nif->setLink( iWater, "Shader Property", nif->getBlockNumber( iEffect ) );
				addLink( nif, iWaterNode, QStringLiteral( "Children" ),
					nif->getBlockNumber( iWater ) );
				byHeight.clear();   // handled; skip the per-height path below
			}
			for ( auto it = byHeight.constBegin(); it != byHeight.constEnd(); ++it ) {
				const float hWorld = it.key();
				const float h = hWorld * invDim;
				waterZMin = qMin( waterZMin, hWorld );
				waterZMax = qMax( waterZMax, hWorld );
				wetCells += it.value().size();

				QModelIndex iWater = insertAvObject( nif,
					QStringLiteral( "BSSubIndexTriShape" ), QString(), float( dim ) );
				nif->set<BSVertexDesc>( iWater, "Vertex Desc", WATER_VERTEX_DESC );
				/* Adaptive subdivision toward the shoreline, as a 2:1 RESTRICTED
				 * quadtree with welded vertices and stitched T-junctions.
				 *
				 * waterSubdiv 0 reproduces vanilla exactly -- one quad per wet
				 * cell, UNWELDED, because vanilla itself is unwelded (48 corners
				 * for 21 distinct positions on the harbour chunk) and the
				 * byte-identical fallback is worth more than those vertices.
				 *
				 * Triangles are 16-bit indexed, so the level is walked DOWN
				 * until the layer fits rather than emitting wrapped indices. */
				int subdiv = qBound( 0, opts.waterSubdiv, 5 );   // 32 samples/cell: level 5 is the last that sees new terrain
				QVector<WaterLeaf> leaves;
				std::vector<int> leafIdx;
				int blocksPerCell = 1;
				QVector<Vector3> wverts;
				QVector<Color4> wcols;
				QVector<Triangle> wtris;
				/* Channels only once the mesh can carry them. At subdiv 0 the
				 * water is one quad per wet cell and four corner values 4096
				 * units apart describe nothing, so the descriptor stays
				 * vanilla's and the output stays byte-identical. */
				const bool waterChans = opts.waterChannels && opts.waterSubdiv > 0;
				QVector<QPair<int, int>> segPrims( dim * dim, qMakePair( 0, 0 ) );
				float wMinX = 3.4e38f, wMinY = 3.4e38f, wMaxX = -3.4e38f, wMaxY = -3.4e38f;
				const float cellSpan = 4096.0f * invDim;

				for ( ;; ) {
					leaves.clear();
					leafIdx.clear();
					wverts.clear();
					wcols.clear();
					wtris.clear();
					segPrims.fill( qMakePair( 0, 0 ) );
					wMinX = 3.4e38f; wMinY = 3.4e38f; wMaxX = -3.4e38f; wMaxY = -3.4e38f;

					waterBuildLeaves( grid, n, dim, it.value(), hWorld, subdiv,
						opts.waterCullBuried, leaves, leafIdx, blocksPerCell );
					const int W = dim * blocksPerCell;
					const float blockSpan = cellSpan / float( blocksPerCell );

					/* Welded by exact INTEGER block key, never by comparing
					 * floats: every corner and every hanging midpoint sits on a
					 * block coordinate by construction, so equal positions have
					 * equal keys and a shared vertex carries ONE value -- which
					 * is what a per-vertex channel needs to cross the edge
					 * without a seam. */
					QHash<int, quint16> weld;
					auto vertexAt = [&]( int gx, int gy ) -> quint16 {
						const int key = gy * ( W + 1 ) + gx;
						if ( subdiv > 0 ) {
							auto f = weld.constFind( key );
							if ( f != weld.constEnd() )
								return f.value();
						}
						const float x = float( gx ) * blockSpan;
						const float y = float( gy ) * blockSpan;
						wMinX = qMin( wMinX, x ); wMaxX = qMax( wMaxX, x );
						wMinY = qMin( wMinY, y ); wMaxY = qMax( wMaxY, y );
						const quint16 idx = quint16( wverts.size() );
						wverts.append( Vector3( x, y, h ) );
						if ( waterChans ) {
							/* R = depth, water plane minus terrain directly
							 * under this vertex. Every corner and split sits on
							 * a block coordinate and blocksPerCell divides 32,
							 * so the heightfield sample is exact -- no
							 * interpolation, no half-sample drift. */
							const int step = 32 / blocksPerCell;
							const int gc = qBound( 0, gx * step, n - 1 );
							const int gr = qBound( 0, gy * step, n - 1 );
							const float terrain =
								grid[size_t( gr ) * size_t( n ) + size_t( gc )];
							const float depth = qMax( 0.0f, hWorld - terrain );
							wcols.append( Color4(
								qBound( 0.0f, depth / 2048.0f, 1.0f ),
								0.0f, 0.0f, 1.0f ) );
						}
						if ( subdiv > 0 )
							weld.insert( key, idx );
						return idx;
					};
					/* Which leaf owns the block just outside an edge. -1 both
					 * for off-chunk and for anything not in this height layer,
					 * so a boundary between two of those produces no split. */
					auto outerLeaf = [&]( int gx, int gy ) {
						if ( gx < 0 || gy < 0 || gx >= W || gy >= W )
							return -1;
						return leafIdx[size_t( gy ) * size_t( W ) + size_t( gx )];
					};

					// leaves come out grouped by cell, so each cell's triangles stay contiguous
					int lastCell = -1, tStart = 0;
					for ( const WaterLeaf & lf : leaves ) {
						const int cx = lf.gx / blocksPerCell, cy = lf.gy / blocksPerCell;
						const int cellId = cy * dim + cx;
						if ( cellId != lastCell ) {
							if ( lastCell >= 0 )
								segPrims[lastCell] = qMakePair( tStart, wtris.size() - tStart );
							lastCell = cellId;
							tStart = wtris.size();
						}
						/* Boundary ring: the four corners, plus the midpoint of
						 * any edge whose neighbour is finer. 2:1 restriction
						 * bounds that to one midpoint per edge. Fanning from
						 * ring[0] gives exactly TWO triangles when there are no
						 * hanging nodes, so an interior leaf is unchanged. */
						const int s = lf.size, hs = s / 2;
						QVector<quint16> ring;
						const int cor[4][2] = { { 0, 0 }, { s, 0 }, { s, s }, { 0, s } };
						for ( int e = 0; e < 4; e++ ) {
							ring.append( vertexAt( lf.gx + cor[e][0], lf.gy + cor[e][1] ) );
							/* WALK the edge instead of probing its midpoint.
							 * A single probe assumes the neighbouring side
							 * changes exactly halfway, which is only true when
							 * that side is uniformly one level finer; anywhere
							 * else it silently misses the hanging node. Walking
							 * inserts a vertex wherever the neighbouring LEAF
							 * actually changes, so it is correct for any
							 * configuration and does not depend on the 2:1
							 * restriction holding perfectly. */
							for ( int t = 1; t < s; t++ ) {
								int ax, ay, bx, by, vx, vy;
								switch ( e ) {
								case 0:   // bottom, +x, neighbours below
									ax = lf.gx + t - 1; ay = lf.gy - 1;
									bx = lf.gx + t;     by = lf.gy - 1;
									vx = lf.gx + t;     vy = lf.gy; break;
								case 1:   // right, +y, neighbours right
									ax = lf.gx + s; ay = lf.gy + t - 1;
									bx = lf.gx + s; by = lf.gy + t;
									vx = lf.gx + s; vy = lf.gy + t; break;
								case 2:   // top, -x, neighbours above
									ax = lf.gx + s - t;     ay = lf.gy + s;
									bx = lf.gx + s - t - 1; by = lf.gy + s;
									vx = lf.gx + s - t;     vy = lf.gy + s; break;
								default:  // left, -y, neighbours left
									ax = lf.gx - 1; ay = lf.gy + s - t;
									bx = lf.gx - 1; by = lf.gy + s - t - 1;
									vx = lf.gx;     vy = lf.gy + s - t; break;
								}
								if ( outerLeaf( ax, ay ) != outerLeaf( bx, by ) )
									ring.append( vertexAt( vx, vy ) );
							}
						}
						if ( ring.size() == 4 ) {
							// no hanging nodes: vanilla's two-triangle split,
							// no extra vertex, so subdiv 0 stays byte-identical
							wtris.append( Triangle( ring[0], ring[1], ring[2] ) );
							wtris.append( Triangle( ring[0], ring[2], ring[3] ) );
						} else {
							/* Stitched leaves fan from the CENTRE, not from a
							 * corner. A corner fan looks cheaper and is wrong:
							 * when a midpoint sits next to the pivot, pivot,
							 * midpoint and next corner are COLLINEAR, so the
							 * first triangle has zero area and the full-length
							 * edge survives with the midpoint lying on it --
							 * reintroducing the exact T-junction being stitched.
							 * Measured: 63 of them at subdiv 3, every one at
							 * t=0.5 of its edge. */
							const quint16 mid = vertexAt( lf.gx + hs, lf.gy + hs );
							for ( int k = 0; k < ring.size(); k++ )
								wtris.append( Triangle( mid, ring[k],
									ring[( k + 1 ) % ring.size()] ) );
						}
					}
					if ( lastCell >= 0 )
						segPrims[lastCell] = qMakePair( tStart, wtris.size() - tStart );

					if ( wverts.size() <= 65535 || subdiv == 0 )
						break;
					subdiv--;
				}

				const int wvCount = wverts.size();
				const int wtCount = wtris.size();
				BSVertexDesc wdesc( WATER_VERTEX_DESC );
				if ( waterChans ) {
					wdesc.SetFlag( VertexFlags::VF_COLORS );
					wdesc.ResetAttributeOffsets( 130 );
					nif->set<BSVertexDesc>( iWater, "Vertex Desc", wdesc.Value() );
				}
				const int wStride = waterChans ? int( wdesc.GetVertexSize() ) : 8;
				nif->set<quint32>( iWater, "Num Vertices", quint32( wvCount ) );
				nif->set<quint32>( iWater, "Num Triangles", quint32( wtCount ) );
				nif->set<quint32>( iWater, "Data Size",
					quint32( wvCount * wStride + wtCount * 6 ) );

				nif->setState( BaseModel::Processing );
				QModelIndex iWV = nif->getIndex( iWater, "Vertex Data" );
				nif->updateArraySize( iWV );
				for ( int v = 0; v < wvCount; v++ ) {
					QModelIndex row = nif->index( v, 0, iWV );
					nif->set<HalfVector3>( row, "Vertex", HalfVector3( wverts[v] ) );
					nif->set<float>( row, "Bitangent X", 1.0f );
					if ( waterChans && v < wcols.size() ) {
						const Color4 & wc = wcols[v];
						nif->set<ByteColor4>( row, "Vertex Colors", ByteColor4(
							FloatVector4( wc.red(), wc.green(), wc.blue(), wc.alpha() ) ) );
					}
				}
				QModelIndex iWT = nif->getIndex( iWater, "Triangles" );
				nif->updateArraySize( iWT );
				nif->setArray<Triangle>( iWT, wtris );

				nif->set<quint32>( iWater, "Num Primitives", quint32( wtris.size() ) );
				nif->set<quint32>( iWater, "Num Segments", quint32( dim * dim ) );
				nif->set<quint32>( iWater, "Total Segments", quint32( dim * dim ) );
				QModelIndex iSegs = nif->getIndex( iWater, "Segment" );
				if ( iSegs.isValid() ) {
					nif->updateArraySize( iSegs );
					for ( int s = 0; s < dim * dim; s++ ) {
						QModelIndex seg = nif->index( s, 0, iSegs );
						nif->set<quint32>( seg, "Start Index",
							quint32( segPrims[s].first * 3 ) );
						nif->set<quint32>( seg, "Num Primitives",
							quint32( segPrims[s].second ) );
						nif->set<quint32>( seg, "Parent Array Index", 0xFFFFFFFFU );
					}
				}
				setBound( nif, iWater, wMinX, wMinY, h, wMaxX, wMaxY, h );
				nif->restoreState();

				QModelIndex iEffect = nif->insertNiBlock( QStringLiteral( "BSEffectShaderProperty" ) );
				nif->set<quint32>( iEffect, "Shader Flags 1", WATER_EFFECT_FLAGS1 );
				nif->set<quint32>( iEffect, "Shader Flags 2", WATER_EFFECT_FLAGS2 );
				nif->set<int>( iEffect, "Lighting Influence", 255 );
				nif->set<float>( iEffect, "Soft Falloff Depth", 100.0f );
				nif->set<float>( iEffect, "Environment Map Scale", 1.0f );
				nif->set<Color4>( iEffect, "Base Color", Color4( 1.0f, 1.0f, 1.0f, 1.0f ) );
				nif->set<float>( iEffect, "Base Color Scale", 1.0f );
				nif->setLink( iWater, "Shader Property", nif->getBlockNumber( iEffect ) );
				addLink( nif, iWaterNode, QStringLiteral( "Children" ),
					nif->getBlockNumber( iWater ) );
			}
			addLink( nif, iRoot, QStringLiteral( "Children" ),
				nif->getBlockNumber( iWaterNode ) );
		}
	}

	/* Multibounds, vanilla frame: X/Y relative to the chunk's SW corner in
	 * WORLD units (centre = half the chunk span), Z absolute world. Water's
	 * pair first in block order, then the root's, as shipped files have it. */
	const float half = float( dim ) * 4096.0f * 0.5f;
	if ( iWaterNode.isValid() ) {
		QModelIndex wmb = insertMultiBound( nif, half, half,
			( waterZMin + waterZMax ) * 0.5f,
			half, half, qMax( 1.0f, ( waterZMax - waterZMin ) * 0.5f ) );
		nif->setLink( iWaterNode, "Multi Bound", nif->getBlockNumber( wmb ) );
	}
	{
		const float zLo = zMin * float( dim ), zHi = zMax * float( dim );
		QModelIndex rmb = insertMultiBound( nif, half, half,
			( zLo + zHi ) * 0.5f, half, half, ( zHi - zLo ) * 0.5f );
		nif->setLink( iRoot, "Multi Bound", nif->getBlockNumber( rmb ) );
	}

	nif->holdUpdates( false );
	nif->updateModel();
	if ( error )
		error->clear();
	return true;
}

/* ================= rung 2: object .bto stitching ======================= */

namespace
{

//! Object-LOD vertex layouts, measured: 20-byte (pos half + UV + normal +
//! tangent) for parity, 24-byte with COLORS for the identity profile.
constexpr std::uint64_t OBJ_VERTEX_DESC = 474989027590661ULL;       // 0x0001B00000430205
constexpr std::uint64_t OBJ_VERTEX_DESC_COLORS = 1037939064898054ULL; // 0x0003B00005430206
/* The THIRD object profile -- identity plus the object channels, stride 32 --
 * is 0x0013F07006543208. It is computed into `objDesc` rather than declared,
 * so there is nowhere else to read it off. */

//! SLSF1_Own_Emit, Shader Flags 1 bit 22: the shape lights itself by its
//! emissive colour times its emissive multiple. Vanilla's LOD chunk shapes
//! carry it set with a BLACK colour, which emits nothing.
constexpr quint32 LOD_OWN_EMIT = 0x400000U;

/*! The emissive MULTIPLE a LOD set carries in its `.lodm` (`emissiveScale`).
 *
 *  The emissive COLOUR is folded into the sheet (a legacy `_g` texel is the
 *  diffuse times its own alpha times the source's emissive colour); the
 *  MULTIPLE cannot be, because it may exceed 1 and the sheet is eight bits a
 *  channel. So it rides in the material - bungo, 2026-09-06: "carry the
 *  multiplier in lodm".
 *
 *  A source that does not OWN-EMIT, or own-emits with a BLACK colour, emits
 *  nothing: 0. That is the answer to the doubt the fourth texture shipped
 *  with - an opaque source whose diffuse alpha is 255 throughout yields a
 *  `_g` sheet equal to the full albedo, and a consumer adding it unscaled
 *  would light every wall. Every measured vanilla LOD chunk shape own-emits
 *  with a black colour, so vanilla's LOD emits nothing and both halves of
 *  this rule say so. */
inline float lodgenEmissiveScale( bool ownEmit, const Color3 & colour, float mult )
{
	if ( !ownEmit )
		return 0.0f;
	if ( colour.red() <= 0.0f && colour.green() <= 0.0f && colour.blue() <= 0.0f )
		return 0.0f;
	return mult > 0.0f ? mult : 0.0f;
}

struct LodSrcShape
{
	QVector<Vector3> pos, nrm, tan;
	QVector<Vector2> uv;
	QVector<Color4> col;
	QVector<Triangle> tris;
	QString tex0, tex1;
	/* The source's material and what the engine composes from it: the
	 * specular slot (7), smoothness and specular strength, the BGSM's when
	 * the shape names one that reads, else the property's own. The arrays
	 * pass bakes the vanilla `_s` into the legacy GSAOS from these
	 * (docs/LODGEN_IMPOSTOR_SPEC.md), and the material name is where a
	 * source .lodm is looked for. */
	QString tex7, matName;
	/* THE EFFECT MATERIAL'S BASE MAP, AND WHY IT IS NOT `tex0` (lane CELLVIEW3,
	 * 2026-09-19). A shape under a BSEffectShaderProperty names a `.bgem`, has
	 * no BSShaderTextureSet at all, and until today nothing here read either --
	 * so `tex0` stayed empty, and an empty diffuse is the missing-texture
	 * MAGENTA under Scene::DoErrorColor, which is what the downtown cars' glass
	 * has been. It goes in its OWN slot because `tex0` feeds the far-LOD bake,
	 * whose output is pinned byte for byte
	 * (`tests/spells/lodgen_native_baseline`): the viewer reads this field, the
	 * bake reads `tex0` and is unchanged BY CONSTRUCTION. Whether the far LOD
	 * should draw effect shapes textured too is a real question and a separate
	 * lane's -- it is NOT answered by making this field `tex0`. */
	QString effectTex0;
	/*! The shape named a material and NOTHING resolved from it -- no texture
	 *  set, no BGSM, no BGEM. The viewer draws such a shape neutral and COUNTS
	 *  it; magenta stays reserved for a genuinely missing file. */
	bool matUnreadable = false;
	float smoothness = 1.0f, specMult = 1.0f;
	/* And what it EMITS: the Own-Emit bit, the emissive colour and the
	 * emissive multiple, from the same place - the BGSM when the shape names
	 * one that reads, else the property's own fields. Carried into the chunk
	 * shape so the arrays pass can read them back off the .BTO, exactly as
	 * slot 7 and the two constants are. BLACK by default and not Color3()'s
	 * own white: the renderer's resetParams says black, a BGSM that does not
	 * enable emit leaves it black, and a shape whose colour we never read
	 * should emit nothing rather than everything. */
	Color3 emitColor = Color3( 0.0f, 0.0f, 0.0f );
	float emitMult = 1.0f;
	bool ownEmit = false;
	bool hasAlpha = false;
	quint16 alphaFlags = 4844;
	quint8 alphaThreshold = 128;
	/* What the MATERIAL says about the surface, as opposed to what the
	 * NiAlphaProperty says about the block. Read only from a BGSM that parses,
	 * defaulted otherwise, and consumed ONLY by the far-terrain road pass. The
	 * three fields above are left untouched on purpose: they feed the object
	 * bakes whose output is pinned by byte identity, and a road decal's
	 * material must not move them. */
	bool matDecal = false;
	bool matAlphaTest = false;
	bool matAlphaBlend = false;
	quint8 matAlphaRef = 255;
};

//! Compose a block's transform up the parent chain (local -> model space).
Transform lodgenWorldTransform( const NifModel * nif, const QModelIndex & block )
{
	Transform t( nif, block );
	QModelIndex parent = nif->getBlockIndex( nif->getParent( nif->getBlockNumber( block ) ) );
	while ( parent.isValid() && nif->blockInherits( parent, "NiAVObject" ) ) {
		t = Transform( nif, parent ) * t;
		parent = nif->getBlockIndex( nif->getParent( nif->getBlockNumber( parent ) ) );
	}
	return t;
}

/*! Read an asset the way the game does: a loose folder first when one was
 *  given (the CLI's --data-root), then the game manager's data folders and
 *  archives (Settings > Resources). The panel passes no loose root - bungo,
 *  2026-09-06: "that means there's no point in that game data thing" - so it
 *  reads the BA2s the game reads. In -no-gui runs the game manager is never
 *  initialised (nifcli.cpp says why), so there --data-root is the only source
 *  and the archive step quietly finds nothing. */
/*! The generator's own index of the game's meshes.
 *
 *  The game manager's Fallout 4 archive filter drops every .nif at index
 *  time (archiveFilterFunction_2 in gamemanager.cpp - the viewer never needs
 *  a mesh out of a BA2), so its lookup cannot find a LOD model however the
 *  path is spelled. Measured: the near chunk (-20,24) placed all 678 refs
 *  from the unpacked folder and none from the manager. So meshes get an
 *  index of their own over the SAME folders and archives Settings >
 *  Resources lists, in the same order (first wins, as there), .nif only,
 *  built once per process on first use. Empty in -no-gui runs, where the
 *  manager holds no folders; textures and materials still go through the
 *  manager, which does index them. */
static bool lodgenMeshFilter( void *, const std::string_view & s )
{
	return s.ends_with( ".nif" );
}

/*! THE RESOURCE STACK (lodgen.h has the contract and why it is two passes).
 *
 *  Empty by default, so a process that never sets one reads exactly what it
 *  read before this existed - which is what keeps the near-chunk byte-identity
 *  gate honest. */
QStringList & lodgenStack()
{
	static QStringList stack;
	return stack;
}

//! the 14 names BA2File itself treats as data directories (ba2file.cpp)
const char * const lodgenDataDirs[] = {
	"geometries", "icons", "interface", "materials", "meshes",
	"particles", "planetdata", "scripts", "shadersfx", "sound",
	"strings", "terrain", "textures", "vis"
};

/*! The engine's archive classes, so the archives sitting in one folder load in
 *  the order it loads them: a mod's own first, then DLC and update, then the
 *  game's own. Mirrors ba2file.cpp's -2 / -3 / -4. */
int lodgenArchiveRank( const QString & baseName )
{
	const QString n = baseName.toLower();
	const bool game = n == QLatin1String( "morrowind.bsa" )
		|| n.startsWith( QLatin1String( "oblivion" ) ) || n.startsWith( QLatin1String( "fallout" ) )
		|| n.startsWith( QLatin1String( "skyrim" ) ) || n.startsWith( QLatin1String( "seventysix" ) )
		|| n.startsWith( QLatin1String( "starfield" ) );
	if ( game )
		return ( !n.contains( QLatin1String( "update" ) ) && !n.endsWith( QLatin1String( "patch.ba2" ) ) ) ? 2 : 1;
	if ( n.startsWith( QLatin1String( "dlc" ) ) )
		return 1;
	return 0;
}

//! the .ba2/.bsa files sitting directly in a folder, in the engine's own order
QStringList lodgenFolderArchives( const QString & folder )
{
	QDir d( folder );
	QStringList names = d.entryList( QStringList{ QStringLiteral( "*.ba2" ), QStringLiteral( "*.bsa" ) },
		QDir::Files, QDir::Name );
	std::stable_sort( names.begin(), names.end(), []( const QString & a, const QString & b ) {
		return lodgenArchiveRank( a ) < lodgenArchiveRank( b );
	} );
	QStringList out;
	for ( const QString & n : names )
		out.append( d.filePath( n ) );
	return out;
}

//! Pass one then pass two, both walking the stack backwards: the index is first-wins.
QStringList lodgenStackSearchPaths()
{
	QStringList out;
	const QStringList & stack = lodgenStack();
	for ( int i = stack.size() - 1; i >= 0; i-- ) {
		const QString e = QDir::cleanPath( stack.at( i ) );
		if ( !QFileInfo( e ).isDir() )
			continue;
		for ( const char * sub : lodgenDataDirs ) {
			const QString p = e + QChar( '/' ) + QLatin1String( sub );
			if ( QFileInfo( p ).isDir() )
				out.append( p );
		}
	}
	for ( int i = stack.size() - 1; i >= 0; i-- ) {
		const QString e = QDir::cleanPath( stack.at( i ) );
		const QFileInfo fi( e );
		if ( fi.isDir() ) {
			for ( const QString & a : lodgenFolderArchives( e ) )
				if ( !out.contains( a, Qt::CaseInsensitive ) )
					out.append( a );
		} else if ( fi.isFile() && !out.contains( e, Qt::CaseInsensitive ) ) {
			out.append( e );
		}
	}
	return out;
}

//! The stack's index: every extension, built once, dropped when the stack changes.
BA2File * lodgenStackIndex()
{
	static std::unique_ptr<BA2File> index;
	static QStringList builtFor;
	static bool tried = false;
	if ( tried && builtFor == lodgenStack() )
		return index.get();
	tried = true;
	builtFor = lodgenStack();
	index.reset();
	const QStringList paths = lodgenStackSearchPaths();
	if ( paths.isEmpty() )
		return nullptr;
	index = std::make_unique<BA2File>();
	for ( const QString & p : paths ) {
		try {
			index->loadArchivePath( p.toLocal8Bit().constData() );
		} catch ( ... ) {
			// an unreadable entry is skipped, as the game manager skips one
		}
	}
	return index.get();
}

static BA2File * lodgenMeshArchives()
{
	static std::unique_ptr<BA2File> index;
	static bool tried = false;
	if ( !tried ) {
		tried = true;
		if ( Game::GameManager::status( Game::FALLOUT_4 ) ) {
			const QStringList paths = Game::GameManager::folders( Game::FALLOUT_4 );
			if ( !paths.isEmpty() ) {
				index = std::make_unique<BA2File>();
				for ( const QString & path : paths ) {
					try {
						index->loadArchivePath( path.toLocal8Bit().constData(), &lodgenMeshFilter );
					} catch ( ... ) {
						// an unreadable path is skipped, as the game manager skips it
					}
				}
			}
		}
	}
	return index.get();
}

static unsigned char * lodgenByteArrayAlloc( void * bufPtr, size_t nBytes )
{
	QByteArray * p = reinterpret_cast<QByteArray *>( bufPtr );
	p->resize( qsizetype( nBytes ) );
	return reinterpret_cast<unsigned char *>( p->data() );
}

/*! Pull one asset out of the resource stack. The stack is consulted before
 *  dataRoot and before the game manager for EVERY kind of asset, because it is
 *  the load order the user (or Mod Organizer) gave and nothing may sit above
 *  it. Returns false, quietly, when no stack is set. */
bool lodgenReadFromStack( const QString & relPath, const char * archiveFolder,
	const char * extension, QByteArray & out )
{
	BA2File * ix = lodgenStackIndex();
	if ( !ix )
		return false;
	const std::string full = Game::GameManager::get_full_path( relPath, archiveFolder, extension );
	const BA2File::FileInfo * fd = ix->findFile( full );
	if ( !fd )
		return false;
	try {
		ix->extractFile( &out, &lodgenByteArrayAlloc, *fd );
	} catch ( ... ) {
		out.clear();
	}
	return !out.isEmpty();
}

static bool lodgenReadAsset( const QString & dataRoot, const QString & relPath,
	const char * archiveFolder, const char * extension, QByteArray & out )
{
	out.clear();
	if ( lodgenReadFromStack( relPath, archiveFolder, extension, out ) )
		return true;
	out.clear();
	if ( !dataRoot.isEmpty() ) {
		QFile f( dataRoot + "/" + relPath );
		if ( f.open( QIODevice::ReadOnly ) ) {
			out = f.readAll();
			if ( !out.isEmpty() )
				return true;
		}
	}
	if ( std::string_view( extension ) == ".nif" ) {
		BA2File * ix = lodgenMeshArchives();
		if ( !ix )
			return false;
		const std::string full = Game::GameManager::get_full_path( relPath, archiveFolder, extension );
		const BA2File::FileInfo * fd = ix->findFile( full );
		if ( !fd )
			return false;
		try {
			ix->extractFile( &out, &lodgenByteArrayAlloc, *fd );
		} catch ( ... ) {
			out.clear();
		}
		return !out.isEmpty();
	}
	return Game::GameManager::get_file( out, Game::FALLOUT_4, relPath, archiveFolder, extension )
		&& !out.isEmpty();
}

} // namespace

// --- the resource stack, and Mod Organizer 2 -------------------------------

void lodgenSetResources( const QStringList & entries )
{
	QStringList clean;
	for ( const QString & e : entries ) {
		const QString t = e.trimmed();
		if ( !t.isEmpty() )
			clean.append( QDir::cleanPath( t ) );
	}
	lodgenStack() = clean;
}

QStringList lodgenResources()
{
	return lodgenStack();
}

QStringList lodgenResourceSearchPaths()
{
	return lodgenStackSearchPaths();
}

namespace
{

/*! Which archive folder and extension a relative path belongs to, the way every
 *  caller of lodgenReadAsset names them. The probe takes a bare path from a
 *  command line, so it has only the extension to go on. */
void lodgenAssetKind( const QString & relPath, const char *& folder, const char *& ext )
{
	const QString p = relPath.toLower();
	if ( p.endsWith( QLatin1String( ".nif" ) ) || p.endsWith( QLatin1String( ".bto" ) )
		|| p.endsWith( QLatin1String( ".btr" ) ) ) {
		folder = "meshes";
		ext = p.endsWith( QLatin1String( ".nif" ) ) ? ".nif" : ( p.endsWith( QLatin1String( ".bto" ) ) ? ".bto" : ".btr" );
	} else if ( p.endsWith( QLatin1String( ".bgsm" ) ) ) {
		folder = "materials"; ext = ".bgsm";
	} else if ( p.endsWith( QLatin1String( ".bgem" ) ) ) {
		folder = "materials"; ext = ".bgem";
	} else if ( p.endsWith( QLatin1String( ".lodm" ) ) ) {
		folder = "materials"; ext = ".lodm";
	} else {
		folder = "textures"; ext = ".dds";
	}
}

} // namespace

bool lodgenProbeAsset( const QString & dataRoot, const QString & relPath,
	QString * entry, QString * kind, QString * path, QByteArray * bytes )
{
	const char * folder = "textures";
	const char * ext = ".dds";
	lodgenAssetKind( relPath, folder, ext );
	QByteArray out;
	if ( !lodgenReadAsset( dataRoot, relPath, folder, ext, out ) )
		return false;
	if ( bytes )
		*bytes = out;

	/* Which entry supplied it. The one index cannot say - it holds no archive
	 * names - so the stack is walked backwards, one entry at a time, and the
	 * first that has the path is the winner, by the same two-pass rule: a
	 * folder's loose files are asked for before anybody's archives. */
	const std::string full = Game::GameManager::get_full_path( relPath, folder, ext );
	const QStringList stack = lodgenStack();
	auto looseIn = [&]( const QString & e ) -> QString {
		const QString p = QDir::cleanPath( e ) + QChar( '/' ) + QString::fromStdString( full );
		return QFileInfo( p ).isFile() ? p : QString();
	};
	for ( int i = stack.size() - 1; i >= 0; i-- ) {
		const QString hit = QFileInfo( stack.at( i ) ).isDir() ? looseIn( stack.at( i ) ) : QString();
		if ( !hit.isEmpty() ) {
			if ( entry ) *entry = QDir::cleanPath( stack.at( i ) );
			if ( kind ) *kind = QStringLiteral( "loose" );
			if ( path ) *path = hit;
			return true;
		}
	}
	for ( int i = stack.size() - 1; i >= 0; i-- ) {
		const QString e = QDir::cleanPath( stack.at( i ) );
		QStringList archives;
		if ( QFileInfo( e ).isDir() )
			archives = lodgenFolderArchives( e );
		else
			archives << e;
		for ( const QString & a : archives ) {
			try {
				BA2File one;
				one.loadArchivePath( a.toLocal8Bit().constData() );
				if ( one.findFile( full ) ) {
					if ( entry ) *entry = e;
					if ( kind ) *kind = QStringLiteral( "archive" );
					if ( path ) *path = a;
					return true;
				}
			} catch ( ... ) {
				// unreadable: it cannot be the source either
			}
		}
	}
	// not from the stack at all: dataRoot, or the game manager
	if ( !dataRoot.isEmpty() && QFileInfo( QDir::cleanPath( dataRoot ) + QChar( '/' ) + relPath ).isFile() ) {
		if ( entry ) *entry = QString();
		if ( kind ) *kind = QStringLiteral( "loose" );
		if ( path ) *path = QDir::cleanPath( dataRoot ) + QChar( '/' ) + relPath;
	} else {
		if ( entry ) *entry = QString();
		if ( kind ) *kind = QStringLiteral( "archive" );
		if ( path ) *path = QStringLiteral( "(the game manager's resources)" );
	}
	return true;
}

/* ===================== the mask law (lodgen.h has the contract) ========= */

const char * lodgenMaskRuleName( LodgenMaskRule rule )
{
	switch ( rule ) {
	case LODGEN_MASK_PBRM:
		return "pbrm";
	case LODGEN_MASK_LEGACY_INVERTED:
		return "legacy-inverted";
	default:
		return "none-default";
	}
}

float lodgenLegacyGloss( float smoothness, float specGreen )
{
	return qBound( 0.0f, qBound( 0.0f, smoothness, 1.0f ) * specGreen, 1.0f );
}

void lodgenResolveMaterialMask( const QString & dataRoot, const QString & matName,
	const QString & specularTex, float smoothness, LodgenMaterialMask * out,
	const QString & diffuseTex )
{
	if ( !out )
		return;
	*out = LodgenMaterialMask();

	/* Arm 1: a PBRM. A malformed one leaves the legacy material in charge,
	 * exactly as the renderer does, rather than failing the layer. */
	QString spec = specularTex;
	float smooth = smoothness;
	QString pbrmCand;
	QString mp = matName;
	if ( !mp.isEmpty() ) {
		mp.replace( QChar( '\\' ), QChar( '/' ) );
		/* A TXST's MNAM is sometimes an ABSOLUTE authoring path -- measured in
		 * the shipped Commonwealth: `c:/projects/fallout4/build/pc/data/
		 * materials/landscape/rocks/rockriverstones_wet.bgsm`. Cut to the last
		 * `materials/` component, or the read is looked for under a folder that
		 * only ever existed on Bethesda's build machine. */
		const int mi = mp.lastIndexOf( QStringLiteral( "materials/" ), -1, Qt::CaseInsensitive );
		if ( mi > 0 )
			mp.remove( 0, mi );
		else if ( !mp.startsWith( QStringLiteral( "materials/" ), Qt::CaseInsensitive ) )
			mp.prepend( QStringLiteral( "materials/" ) );
		if ( mp.endsWith( QStringLiteral( ".pbrm" ), Qt::CaseInsensitive ) )
			pbrmCand = mp;
		else if ( mp.endsWith( QStringLiteral( ".bgsm" ), Qt::CaseInsensitive )
			|| mp.endsWith( QStringLiteral( ".bgem" ), Qt::CaseInsensitive ) )
			pbrmCand = mp.left( mp.length() - 5 ) + QStringLiteral( ".pbrm" );
	} else if ( !diffuseTex.isEmpty() ) {
		/* No material to hang a sibling off. `lodmSourceCandidate` already knows
		 * how to turn a diffuse into a material-folder stem, and it is the SAME
		 * convention a source .lodm is found by, so the two cannot disagree
		 * about where a user puts an override. */
		const QString lodm = lodmSourceCandidate( QString(), diffuseTex );
		if ( lodm.endsWith( QStringLiteral( ".lodm" ), Qt::CaseInsensitive ) ) {
			pbrmCand = lodm.left( lodm.length() - 5 ) + QStringLiteral( ".pbrm" );
			pbrmCand.replace( QChar( '\\' ), QChar( '/' ) );
		}
	}
	{
		if ( !pbrmCand.isEmpty() ) {
			QByteArray pbytes;
			if ( lodgenReadAsset( dataRoot, pbrmCand, "materials", ".pbrm", pbytes ) ) {
				const PbrmMaterial pm = pbrmParse( pbytes );
				if ( pm.ok ) {
					out->rule = LODGEN_MASK_PBRM;
					out->servedBy = pbrmCand;
					out->roughnessConst = qBound( 0.0f, pm.roughness, 1.0f );
					out->metallicConst = qBound( 0.0f, pm.metallic, 1.0f );
					if ( pm.rmaos.enabled && !pm.rmaos.lookupPath.isEmpty() ) {
						if ( pm.features & PbrmMaterial::RmaosRoughness ) {
							out->roughnessTex = pm.rmaos.lookupPath;
							out->roughnessChannel = 0;           // RMAOS: R roughness
							out->haveRoughnessMap = true;
						}
						if ( pm.features & PbrmMaterial::RmaosMetallic ) {
							out->metallicTex = pm.rmaos.lookupPath;
							out->metallicChannel = 1;            // RMAOS: G metallic
							out->haveMetallicMap = true;
						}
					}
					if ( pm.emissive.enabled && !pm.emissive.lookupPath.isEmpty() ) {
						out->emissiveTex = pm.emissive.lookupPath;
						out->haveEmissive = true;
					}
					return;
				}
			}
		}
	}
	if ( !matName.isEmpty() ) {
		/* No PBRM: the legacy material's OWN slots win over whatever the caller
		 * was handed, which is the renderer's rule and the object bake's. Slot 2
		 * of a BGSM's texture list is the `_s` map and it counts only while the
		 * material enables specular; slot 3 is its glow map, which is the
		 * emissive under the legacy family (`.lodm` 2.1). */
		QByteArray mbytes;
		if ( lodgenReadAsset( dataRoot, mp, "materials", ".bgsm", mbytes ) ) {
			const ShaderMaterial sm( mbytes );
			if ( sm.isValid() ) {
				const QStringList & t = sm.textures();
				if ( t.size() > 2 )
					spec = ( sm.specularEnabled() && !t[2].isEmpty() ) ? t[2] : QString();
				if ( t.size() > 3 && !t[3].isEmpty() ) {
					out->emissiveTex = t[3];
					out->haveEmissive = true;
				}
				smooth = sm.smoothness();
				out->servedBy = mp;
			}
		}
	}

	// Arm 2: a legacy material. Roughness = 1 - gloss, and gloss is the one the
	// object sheets already store. Metallic stays 0: bungo's ruling is that it
	// is derived from a PBRM or not at all.
	if ( !spec.isEmpty() || !matName.isEmpty() ) {
		out->rule = LODGEN_MASK_LEGACY_INVERTED;
		out->glossScale = qBound( 0.0f, smooth, 1.0f );
		out->invertRoughness = true;
		out->roughnessChannel = 1;                  // the `_s` map's GREEN channel
		// with no `_s` map the legacy law reads the map as 1, so gloss is the
		// smoothness constant alone and roughness is its complement
		out->roughnessConst = 1.0f - lodgenLegacyGloss( smooth, 1.0f );
		if ( !spec.isEmpty() ) {
			out->roughnessTex = spec;
			out->haveRoughnessMap = true;
		}
		return;
	}

	// Arm 3: nothing to read. 1.0 is FULLY ROUGH, which is the honest "unknown":
	// it adds no highlight the source never had, and it is named in the census
	// so a worldspace full of them cannot pass for a measurement.
	out->rule = LODGEN_MASK_NONE;
	out->roughnessConst = 1.0f;
	out->metallicConst = 0.0f;
}

QStringList lodgenListResourceFiles( int limit )
{
	QStringList out;
	BA2File * ix = lodgenStackIndex();
	if ( !ix || limit <= 0 )
		return out;
	std::vector< std::string_view > names;
	ix->getFileList( names );
	for ( const std::string_view & n : names ) {
		if ( out.size() >= limit )
			break;
		out.append( QString::fromUtf8( n.data(), qsizetype( n.length() ) ) );
	}
	return out;
}

bool lodgenUnderMo2()
{
#ifdef Q_OS_WIN32
	return GetModuleHandleW( L"usvfs_x64.dll" ) != nullptr;
#else
	return false;
#endif
}

QString lodgenPluginsTxtPath()
{
	const QString local = QString::fromLocal8Bit( qgetenv( "LOCALAPPDATA" ) );
	if ( local.isEmpty() )
		return QString();
	return QDir::cleanPath( local + QStringLiteral( "/Fallout4/plugins.txt" ) );
}

QStringList lodgenReadPluginsTxt( const QString & path, QString * error )
{
	QStringList out;
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
		if ( error )
			*error = QStringLiteral( "cannot read %1" ).arg( path );
		return out;
	}
	QTextStream in( &f );
	while ( !in.atEnd() ) {
		QString line = in.readLine().trimmed();
		if ( line.isEmpty() || line.startsWith( QChar( '#' ) ) )
			continue;
		// MO2 and the game both mark an ENABLED plugin with a leading '*'
		if ( !line.startsWith( QChar( '*' ) ) )
			continue;
		line = line.mid( 1 ).trimmed();
		if ( !line.isEmpty() )
			out.append( line );
	}
	if ( error )
		error->clear();
	return out;
}

QStringList lodgenPluginArchives( const QString & dataDir, const QString & pluginName )
{
	QStringList out;
	QString stem = pluginName;
	for ( const char * e : { ".esm", ".esp", ".esl" } ) {
		if ( stem.endsWith( QLatin1String( e ), Qt::CaseInsensitive ) ) {
			stem.chop( 4 );
			break;
		}
	}
	const QDir d( dataDir );
	for ( const char * suffix : { " - Main.ba2", " - Textures.ba2" } ) {
		const QString p = d.filePath( stem + QLatin1String( suffix ) );
		if ( QFileInfo( p ).isFile() )
			out.append( QDir::cleanPath( p ) );
	}
	return out;
}

QStringList lodgenMo2Stack( const QString & dataDir, const QStringList & pluginNames )
{
	QStringList stack;
	const QString data = QDir::cleanPath( dataDir );
	if ( QFileInfo( data ).isDir() )
		stack.append( data );
	/* The base game's set (sResourceArchiveList): Fallout4 - *.ba2 and the DLC
	 * ones. They sit under every mod, and the Data entry above them already
	 * carries them at the very bottom - naming them here only lifts them above
	 * an unclaimed archive, which is where the engine has them. */
	QDir d( data );
	QStringList base = d.entryList( QStringList{ QStringLiteral( "Fallout4 - *.ba2" ),
		QStringLiteral( "DLC*.ba2" ) }, QDir::Files, QDir::Name );
	for ( const QString & n : base )
		stack.append( QDir::cleanPath( d.filePath( n ) ) );
	// then each enabled plugin's own archives, in LOAD order: the last wins
	for ( const QString & p : pluginNames )
		for ( const QString & a : lodgenPluginArchives( data, p ) )
			if ( !stack.contains( a, Qt::CaseInsensitive ) )
				stack.append( a );
	return stack;
}

QString lodgenStageTimeLine( qint64 msLandscape, qint64 msMeshes, qint64 msTextures, qint64 msImpostors,
	const QString & librarySplit )
{
	auto s = []( qint64 ms ) { return QString::number( double( ms ) / 1000.0, 'f', 1 ); };
	QString line = QString( "stage times: landscape %1 s, meshes %2 s, textures %3 s, impostors %4 s" )
		.arg( s( msLandscape ) ).arg( s( msMeshes ) ).arg( s( msTextures ) ).arg( s( msImpostors ) );
	/* THE WAY BACK IS THE EMPTY STRING: a bake that wrote no native pair appends
	 * nothing, so its line is the one it has always been, to the byte. */
	if ( !librarySplit.isEmpty() )
		line += QStringLiteral( " (" ) + librarySplit + QChar( ')' );
	return line;
}

bool lodgenIsTreeModel( const QString & model )
{
	if ( model.contains( QLatin1String( "\\trees\\" ), Qt::CaseInsensitive )
		|| model.contains( QLatin1String( "/trees/" ), Qt::CaseInsensitive ) )
		return true;
	const int slash = qMax( model.lastIndexOf( QChar( '\\' ) ), model.lastIndexOf( QChar( '/' ) ) );
	const QString modelFile = model.mid( slash + 1 ).toLower();
	if ( !modelFile.startsWith( QLatin1String( "tree" ) ) )
		return false;
	/* THE FILENAME CLAUSE IS SCOPED TO `Landscape\` (lane ROADS2). A name is
	 * weaker evidence than a folder, and the seven shipped models named
	 * `Tree...` outside the landscape set are all `SetDressing\` props -- two
	 * tree swings, a swing rope pile, a grounded swing, a no-swing swing, a
	 * noose branch and a hanging mannequin -- four of which stand in Sanctuary.
	 * A leading `meshes` and a leading `lod` are dropped first, because the 167
	 * far models the clause exists for are `LOD\Landscape\Tree*.nif`. */
	QString p = model;
	p.replace( QChar( '\\' ), QChar( '/' ) );
	const QStringList c = p.toLower().split( QChar( '/' ), Qt::SkipEmptyParts );
	int i = 0;
	if ( i < c.size() && c[i] == QLatin1String( "meshes" ) )
		i++;
	if ( i < c.size() && c[i] == QLatin1String( "lod" ) )
		i++;
	return i + 1 < c.size() && c[i] == QLatin1String( "landscape" );
}

namespace
{

//! Load every shape of a per-object LOD model, transforms applied, textures
//! resolved from its shader property. Results cached per path.
const QVector<LodSrcShape> & lodgenLoadModel( const QString & dataRoot,
	const QString & meshPath, QHash<QString, QVector<LodSrcShape>> & cache )
{
	const QString key = meshPath.toLower();
	auto it = cache.constFind( key );
	if ( it != cache.constEnd() )
		return *it;

	/* NIF PARSING IS NOT SERIALISED HERE ANY MORE (lane RESUME3, 2026-09-11).
	 *
	 * BAKEPERF1 put a process-wide mutex around the whole life of this
	 * temporary document, on a single stack taken inside a bake where five
	 * subsystems were live at once, and called it a containment rather than a
	 * fix. It was containment for a fault that is not in the parser.
	 *
	 * The model layer on its own -- `NifSkope -no-gui parsestress`, 16 threads,
	 * 8 reps, 4 fixtures, 20 consecutive runs -- did 10,240 loads with 0 digest
	 * mismatches and 0 faults, with both of its sabotage floors seen to go red
	 * first. The real bake's four symbolised faults were all under
	 * `cliMessageHandler`, which wrote through an unlocked shared QTextStream
	 * from every worker at once (src/nifcli.cpp, fixed there). The cache above
	 * still means each model is parsed once per chunk.
	 *
	 * The way back, exact: `--chunk-threads 1`, the shipped default. */

	QVector<LodSrcShape> shapes;
	QString path = meshPath;
	path.replace( QChar( '\\' ), QChar( '/' ) );
	if ( !path.startsWith( QStringLiteral( "meshes/" ), Qt::CaseInsensitive ) )
		path.prepend( QStringLiteral( "meshes/" ) );
	NifModel src;
	QByteArray bytes;
	QBuffer dev( &bytes );
	const bool found = lodgenReadAsset( dataRoot, path, "meshes", ".nif", bytes );
	const bool loaded = found && dev.open( QIODevice::ReadOnly )
		&& src.load( dev, path.toLocal8Bit().constData() );
	/* load() leaves the model in its Loading state; loadFromFile() clears it
	 * and this path must too, or index lookups below answer as they do
	 * mid-load and every model comes back shapeless - seen as "no
	 * LOD-bearing refs" on the very chunk the impostor harness builds, with
	 * not one model reported as failing to load. */
	src.resetState();
	// batch mode routes logMessage nowhere, so a model that will not load says so here
	if ( !loaded )
		fprintf( stderr, "lodgen: model %s: %s (%lld bytes)\n", path.toLocal8Bit().constData(),
			!found ? "not found in the loose folder or the archives" : "read, but did not load",
			(long long) bytes.size() );
	if ( loaded ) {
		for ( int b = 0; b < src.getBlockCount(); b++ ) {
			QModelIndex iShape = src.getBlockIndex( b );
			if ( !src.blockInherits( iShape, "BSTriShape" ) )
				continue;
			const quint32 numVerts = src.get<quint32>( iShape, "Num Vertices" );
			if ( !numVerts )
				continue;
			/* Editor markers are not the object. The game hides every node named
			 * EditorMarker*; shack and building kit pieces carry workshop snap
			 * markers under one, as untextured effect-shader shapes, and they were
			 * baked into the library with an empty material: the magenta squares on
			 * the roofs of the urban view (bungo 2026-09-17). */
			{
				bool marker = false;
				int blk = b;
				for ( int hop = 0; blk >= 0 && hop < 64 && !marker; hop++ ) {
					marker = src.get<QString>( src.getBlockIndex( blk ), "Name" )
						.startsWith( QStringLiteral( "EditorMarker" ), Qt::CaseInsensitive );
					blk = src.getParent( blk );
				}
				if ( marker )
					continue;
			}
			const BSVertexDesc desc = src.get<BSVertexDesc>( iShape, "Vertex Desc" );
			const quint16 flags = quint16( ( desc.Value() >> 44 ) & 0xFFFF );
			const bool fullPrec = ( flags & 0x400 ) != 0;
			const bool hasColors = ( flags & 0x20 ) != 0;
			const Transform xf = lodgenWorldTransform( &src, iShape );
			LodSrcShape s;
			QModelIndex iVD = src.getIndex( iShape, "Vertex Data" );
			if ( !iVD.isValid() )
				continue;
			for ( quint32 v = 0; v < numVerts; v++ ) {
				QModelIndex row = src.index( int( v ), 0, iVD );
				const Vector3 p = fullPrec ? src.get<Vector3>( row, "Vertex" )
					: Vector3( src.get<HalfVector3>( row, "Vertex" ) );
				s.pos.append( xf * p );
				s.nrm.append( xf.rotation * Vector3( src.get<ByteVector3>( row, "Normal" ) ) );
				s.tan.append( xf.rotation * Vector3( src.get<ByteVector3>( row, "Tangent" ) ) );
				s.uv.append( Vector2( src.get<HalfVector2>( row, "UV" ) ) );
				s.col.append( hasColors
					? Color4( src.get<ByteColor4>( row, "Vertex Colors" ) )
					: Color4( 1, 1, 1, 1 ) );
			}
			QModelIndex iTris = src.getIndex( iShape, "Triangles" );
			if ( iTris.isValid() )
				s.tris = src.getArray<Triangle>( iTris );
			QModelIndex iAlpha = src.getBlockIndex(
				src.getLink( iShape, "Alpha Property" ) );
			if ( iAlpha.isValid() ) {
				s.hasAlpha = true;
				s.alphaFlags = quint16( src.get<int>( iAlpha, "Flags" ) );
				/* NOT the source's threshold: near-tree materials test at
				 * 65-80, which at LOD distance passes far more canopy
				 * texels than vanilla's chunks do. Vanilla LOD alpha
				 * properties test at 128 across the board (measured, flags
				 * 4844 threshold 128) — bungo diagnosed the denser-canopy
				 * difference as exactly this cutoff. Since 2026-09-18 a BGSM
				 * that enables its own test overrides this below. */
				s.alphaThreshold = 128;
			}
			QModelIndex iShader = src.getBlockIndex(
				src.getLink( iShape, "Shader Property" ) );
			if ( iShader.isValid() ) {
				s.matName = src.get<QString>( iShader, "Name" );
				s.smoothness = src.get<float>( iShader, "Smoothness" );
				s.specMult = src.get<float>( iShader, "Specular Strength" );
				s.emitColor = src.get<Color3>( iShader, "Emissive Color" );
				s.emitMult = src.get<float>( iShader, "Emissive Multiple" );
				s.ownEmit = ( src.get<quint32>( iShader, "Shader Flags 1" )
					& LOD_OWN_EMIT ) != 0;
				QModelIndex iTexSet = src.getBlockIndex(
					src.getLink( iShader, "Texture Set" ) );
				if ( iTexSet.isValid() ) {
					QModelIndex iArr = src.getIndex( iTexSet, "Textures" );
					if ( iArr.isValid() ) {
						s.tex0 = src.get<QString>( src.getIndex( iArr, 0 ) );
						s.tex1 = src.get<QString>( src.getIndex( iArr, 1 ) );
						if ( src.get<int>( iTexSet, "Num Textures" ) > 7 )
							s.tex7 = src.get<QString>( src.getIndex( iArr, 7 ) );
					}
				}
				/* A BGSM wins over the texture set, as the renderer's
				 * fileName() has it: its textures where it names them, its
				 * constants always; the specular slot only while it enables
				 * specular. Measured on the maple: the LOD sources DO name
				 * materials (Materials\LOD\PreWarMapleGrLOD.BGSM) and carry
				 * a vanilla _s map in slot 7. */
				if ( s.matName.endsWith( QStringLiteral( ".bgsm" ), Qt::CaseInsensitive ) ) {
					QString mp = s.matName;
					mp.replace( QChar( '\\' ), QChar( '/' ) );
					/* AN ABSOLUTE BETHESDA BUILD PATH IS CUT, NOT PREFIXED (lane
					 * CELLVIEW2). A BSLightingShaderProperty can name its material with
					 * a path off the build machine; prepending `materials/` to one of
					 * those produced `materials/c:/.../materials/x.bgsm`, which resolves
					 * to nothing, leaves the diffuse slot EMPTY, and an empty diffuse
					 * binds the missing-texture MAGENTA under Scene::DoErrorColor
					 * (src/gl/renderer.cpp ~951) rather than reading as untextured.
					 * The right shape is already in this file at lodgenCollectMaterials
					 * (~1826): cut everything before the LAST `materials/`, and only
					 * prepend when the path has none at all. */
					const int mmi = mp.lastIndexOf( QStringLiteral( "materials/" ), -1,
						Qt::CaseInsensitive );
					if ( mmi > 0 )
						mp.remove( 0, mmi );
					else if ( !mp.startsWith( QStringLiteral( "materials/" ), Qt::CaseInsensitive ) )
						mp.prepend( QStringLiteral( "materials/" ) );
					QByteArray mbytes;
					if ( lodgenReadAsset( dataRoot, mp, "materials", ".bgsm", mbytes ) ) {
						const ShaderMaterial sm( mbytes );
						if ( sm.isValid() ) {
							const QStringList & t = sm.textures();
							if ( t.size() > 0 && !t[0].isEmpty() )
								s.tex0 = t[0];
							if ( t.size() > 1 && !t[1].isEmpty() )
								s.tex1 = t[1];
							if ( t.size() > 2 )
								s.tex7 = ( sm.specularEnabled() && !t[2].isEmpty() ) ? t[2] : QString();
							s.smoothness = sm.smoothness();
							s.specMult = sm.specularStrength();
							// the BGSM's emittance wins the same way, as the renderer's does
							s.emitColor = sm.emittanceColor();
							s.emitMult = sm.emittanceMultiple();
							s.ownEmit = sm.emitEnabled();
							/* The road pass's own operands. Nothing else reads
							 * them, so no gated output moves. */
							s.matDecal = sm.hasDecal();
							s.matAlphaTest = sm.hasAlphaTest();
							s.matAlphaBlend = sm.hasAlphaBlend();
							s.matAlphaRef = sm.alphaTestThreshold();
						}
					}
				} else if ( s.matName.endsWith( QStringLiteral( ".bgem" ), Qt::CaseInsensitive ) ) {
					/* THE EFFECT MATERIAL (lane CELLVIEW3). Measured on one named
					 * car: `Vehicles\Automotive\Sedan02_Postwar.nif` has 5 shapes,
					 * 4 BSLightingShaderProperty + BSShaderTextureSet pairs and ONE
					 * BSEffectShaderProperty naming
					 * `Materials\Vehicles\Automotive\Car_Glass01.BGEM`, with only 4
					 * texture sets in the file -- so the glass shape had no texture
					 * from either source and came back with an EMPTY diffuse, which
					 * is the magenta. The same cut-or-prepend as the BGSM above,
					 * then the BGEM's own base map (slot 0). */
					QString mp = s.matName;
					mp.replace( QChar( '\\' ), QChar( '/' ) );
					const int mmi = mp.lastIndexOf( QStringLiteral( "materials/" ), -1,
						Qt::CaseInsensitive );
					if ( mmi > 0 )
						mp.remove( 0, mmi );
					else if ( !mp.startsWith( QStringLiteral( "materials/" ), Qt::CaseInsensitive ) )
						mp.prepend( QStringLiteral( "materials/" ) );
					QByteArray mbytes;
					if ( lodgenReadAsset( dataRoot, mp, "materials", ".bgem", mbytes ) ) {
						const EffectMaterial em( mbytes );
						if ( em.isValid() ) {
							const QStringList & t = em.textures();
							if ( t.size() > 0 && !t[0].isEmpty() )
								s.effectTex0 = t[0];
						}
					}
					/* The property's own Source Texture is the fallback, exactly as
					 * the renderer's is: a BGEM that does not read, or names no base
					 * map, must not cost the shape the texture the NIF already
					 * carries. */
					if ( s.effectTex0.isEmpty() )
						s.effectTex0 = src.get<QString>( iShader, "Source Texture" );
				}
				/* A shape that NAMED a material and got nothing out of it, out of
				 * any of the three sources. It is counted and drawn neutral rather
				 * than magenta; see LodSrcShape::matUnreadable. */
				if ( !s.matName.isEmpty() && s.tex0.isEmpty() && s.effectTex0.isEmpty() )
					s.matUnreadable = true;
			}
			/* THE CUTOFF IS THE BGSM'S (bungo 2026-09-18 05:2x, on the 128-vs-80
			 * tree pictures: "Alpha test 80 looks better"). A shape that carries an
			 * NiAlphaProperty and names a BGSM that enables its own alpha test is
			 * cut at that BGSM's ref (the tree LOD sets say 80/82), which is what
			 * the renderer does with the same shape; the flat 128 above stays for
			 * every shape without such a material. This is what the .BTO's
			 * NiAlphaProperty and the .lodo material byte now carry. */
			if ( s.hasAlpha && s.matAlphaTest )
				s.alphaThreshold = s.matAlphaRef;
			if ( !s.tris.isEmpty() )
				shapes.append( s );
		}
	}
	if ( loaded && shapes.isEmpty() )
		fprintf( stderr, "lodgen: model %s: loaded, %d blocks, no BSTriShape with vertex data and triangles\n",
			path.toLocal8Bit().constData(), src.getBlockCount() );
	return *cache.insert( key, shapes );
}

struct ObjBucket
{
	QVector<Vector3> pos, nrm, tan;
	QVector<Vector2> uv;
	QVector<Color4> col;
	QVector<float> sky;                 //!< UV2.x: fraction of upper hemisphere open
	QVector<float> groundBlend;         //!< Eye Data: 1 at terrain contact, 0 clear of it
	// triangles grouped per cell for the dim4 segment split
	QVector<QVector<Triangle>> cellTris;
	QString tex0, tex1;
	QString tex7, matName;              //!< the source's specular slot and material (see LodSrcShape)
	float smoothness = 1.0f, specMult = 1.0f;
	Color3 emitColor = Color3( 0.0f, 0.0f, 0.0f );   //!< the source's emission (see LodSrcShape)
	float emitMult = 1.0f;
	bool ownEmit = false;
	bool hasAlpha = false;
	quint16 alphaFlags = 4844;
	quint8 alphaThreshold = 128;
};

} // namespace


/* The FO4CS-native emitter's model loader: lodgenLoadModel through the same
 * resource stack, one cache for the run, LodSrcShape flattened into the plain
 * arrays nativeemit.h takes. `user` is the data root (a QString). */
bool lodgenNativeLoadModel( void * user, const QString & model, std::vector<NativeSrcShape> * out )
{
	/* PER THREAD, NOT PER PROCESS (lane PERF1, 2026-09-17). lodgenLoadModel
	 * hands back a REFERENCE INTO this hash, so a `static` here is the exact
	 * shape ww-parallelise-a-stage refuses: the reference outlives any lock a
	 * mutex could hold, and one insert rehashing the table under another
	 * worker's reference is a use-after-free, not a race on a counter. The
	 * chunk pass never had the problem because lodgenBuildObjectChunk declares
	 * its cache as a LOCAL (src/lodgen.cpp, `QHash ... modelCache;`), one per
	 * chunk; this is the same answer, one per thread.
	 *
	 * It costs nothing in memory or in loads: lodgenNativeWrite visits each
	 * folded model path EXACTLY ONCE, so the cache never hit across models even
	 * when it was shared, and each model is now cached by exactly one worker.
	 * The way back is unaffected -- at `--threads 1` there is one thread and
	 * one cache, which is what the static was. */
	thread_local QHash<QString, QVector<LodSrcShape>> cache;
	const QString & dataRoot = *static_cast<const QString *>( user );
	const QVector<LodSrcShape> & shapes = lodgenLoadModel( dataRoot, model, cache );
	out->clear();
	for ( const LodSrcShape & s : shapes ) {
		if ( s.pos.isEmpty() || s.tris.isEmpty() )
			continue;
		NativeSrcShape n;
		const int nv = s.pos.size();
		n.geom.pos.reserve( size_t( nv ) * 3 );
		n.geom.nrm.reserve( size_t( nv ) * 3 );
		n.geom.tan.reserve( size_t( nv ) * 3 );
		n.geom.uv.reserve( size_t( nv ) * 2 );
		for ( int v = 0; v < nv; v++ ) {
			const Vector3 & p = s.pos[v];
			const Vector3 nn = v < s.nrm.size() ? s.nrm[v] : Vector3( 0.0f, 0.0f, 1.0f );
			const Vector3 tt = v < s.tan.size() ? s.tan[v] : Vector3( 1.0f, 0.0f, 0.0f );
			const Vector2 uv = v < s.uv.size() ? s.uv[v] : Vector2( 0.0f, 0.0f );
			n.geom.pos.push_back( p[0] ); n.geom.pos.push_back( p[1] ); n.geom.pos.push_back( p[2] );
			n.geom.nrm.push_back( nn[0] ); n.geom.nrm.push_back( nn[1] ); n.geom.nrm.push_back( nn[2] );
			n.geom.tan.push_back( tt[0] ); n.geom.tan.push_back( tt[1] ); n.geom.tan.push_back( tt[2] );
			n.geom.uv.push_back( uv[0] ); n.geom.uv.push_back( uv[1] );
		}
		n.geom.tris.reserve( size_t( s.tris.size() ) * 3 );
		for ( const Triangle & t : s.tris ) {
			n.geom.tris.push_back( quint32( t.v1() ) );
			n.geom.tris.push_back( quint32( t.v2() ) );
			n.geom.tris.push_back( quint32( t.v3() ) );
		}
		n.tex0 = s.tex0; n.tex1 = s.tex1; n.tex7 = s.tex7; n.matName = s.matName;
		n.effectTex0 = s.effectTex0; n.matUnreadable = s.matUnreadable;
		n.smoothness = s.smoothness; n.specMult = s.specMult;
		n.emitColor[0] = s.emitColor.red(); n.emitColor[1] = s.emitColor.green(); n.emitColor[2] = s.emitColor.blue();
		n.emitMult = s.emitMult; n.ownEmit = s.ownEmit;
		n.hasAlpha = s.hasAlpha; n.alphaThreshold = s.alphaThreshold;
		out->push_back( std::move( n ) );
	}
	return !out->empty();
}

/* ================= rung 3: per-placement AO bake ======================= */

namespace
{

// LodgenAoScene lives in src/lodgenao.h (2026-09-18): the .lodo writer casts with it too.

} // namespace


namespace
{
// defined with the texture-bake section below (same anonymous namespace)
bool lodgenWriteDds( const QString & path, int w, int h,
	const std::vector<quint32> & bgra, bool bc3 = false, int maxMips = 0,
	bool bc1Alpha = false, quint32 stamp0 = 0, quint32 stamp1 = 0,
	bool mipsToOne = false );

struct LodgenCard
{
	bool valid = false;
	QString texPath;            // game path for the texture set
	float halfW = 0, halfH = 0;
	Vector3 center;             // model-space centre of the photographed bound
	bool hasSide = false;       // the sheet's right half is a real side view
	// the octahedral sheets, when the bake made them (0 = none)
	int oct = 0, octTileW = 0, octTileH = 0;
	//! The RUN's chosen resolution. octTileW/H sit at or below it: a smaller
	//! frame is this base's rung on the size ladder (its world size against the
	//! run's largest), not evidence of a differently-configured bake.
	int octBase = 0;
	//! The GAP between two neighbouring silhouettes across a frame border, per
	//! axis, in texels -- the meta's `gap` line, and bungo's own quantity
	//! ("8 pixels of distance between two rendered objects", 2026-09-09).
	//! 0 = a sidecar from before the line.
	int octGapX = 0, octGapY = 0;
	//! The gutter on EACH side of a frame, per axis, in texels: half the gap on
	//! a `gap` sidecar, the literal number on an older `pad` one, and
	//! max(4, longSide/16) on a sidecar with neither.
	int octPadX = 0, octPadY = 0;
	//! What the MIP CAP divides, min over the two axes: THE GAP, under every
	//! vintage. A tap on a frame's UV border reads half of that frame's last
	//! texel and half of the neighbour's first, so what separates the two
	//! silhouettes at level k is gap / 2^k -- but the chain now stops one level
	//! EARLIER than that, at the last level where the MARGIN ON EACH SIDE,
	//! gap / 2^(k+1), is still a whole texel (bungo, 2026-09-09: ship one mip
	//! fewer, "128 frame, gap 8 -> 3 levels 128/64/32"). Hence
	//!
	//!   mips = log2(octMipUnit)
	//!
	//! and the two OLDER sidecars are unmoved by the change, because each wrote
	//! a PER-SIDE number whose gap is twice it: log2(2*pad) = 1 + log2(pad),
	//! exactly the count those sheets were built for.
	int octMipUnit = 0;
	//! PER-FRAME POSITIONING (bungo, 2026-09-09, shipped by lane CARDFINAL).
	//! Every frame shifts its OWN silhouette to its own centre, so the frame has
	//! to hold the widest SINGLE VIEW instead of the union of all of them. One
	//! scale still serves every view -- his "the tree is equal in size on each
	//! one" -- and the quad is still the whole frame; only WHERE that quad sits
	//! moves. These are the offsets that put it back: frame (i,j), at sheet
	//! position (i*frameW, j*frameH), carries `octFrameOff[2*(j*oct+i)]` along
	//! that view's own RIGHT axis and `[+1]` along its UP axis, in model units,
	//! added to `octCenter`. Empty = a bake from before the line, which means
	//! all zeros: every frame was centred on `octCenter`.
	QVector<float> octFrameOff;
	float octHalfW = 0, octHalfH = 0, octSpan = 0;
	Vector3 octCenter;
	bool octPbr = false;        // the set's family: pbr (_bc/_n/_rmaos) or legacy (_d/_n/_gsaos)
	// `emissiveScale`: what a consumer multiplies the emissive sheet by (the
	// meta's `emissive` line). 1 when a bake from before it says nothing.
	float octEmissiveScale = 1.0f;
	//! THE CAMERA THE SHEET WAS PHOTOGRAPHED THROUGH -- the meta's `projection`
	//! line, `ortho` or `persp` (lane CARDORTHO, 2026-09-10). Every extent this
	//! struct carries -- `octHalfW`, `octHalfH`, `octFrameOff` -- is a world
	//! measurement taken off viewport pixels through ONE units-per-pixel
	//! constant, and that is a statement about an ORTHOGRAPHIC camera. Bakes
	//! before 2026-09-10 drew through a 60-degree perspective frustum while
	//! measuring as if they had not (lane HOOKCAM measured it), so their
	//! extents describe no picture and their frames are foreshortened. EMPTY
	//! means the sidecar does not say, and every sidecar that does not say was
	//! baked that way: the line arrived in the same change that fixed the
	//! camera. Carried into the `.lodm` so a consumer can refuse a non-metric
	//! set by name instead of drawing a quad that cannot fit its own mesh.
	QString octProjection;
	//! WHICH VIEW CONVENTION THE FRAMES WERE PHOTOGRAPHED UNDER (2026-09-19,
	//! bungo's "fix the 180 issue"). `spec1` = frame (i,j) really is the view
	//! from direction (i,j), which is what docs/LODGEN_IMPOSTOR_SPEC.md always
	//! said and what the bake does from this exe on. EMPTY means the sidecar
	//! does not say, and every sidecar that does not say came from a bake whose
	//! `rz = 90 - azim` turned the AZIMUTH BY 180 DEGREES -- so absence is not
	//! "unknown", it is the legacy vintage, and such a set must be re-baked.
	//! Carried into the `.lodm` verbatim; an unrecognised word is carried too,
	//! because a consumer refusing what it does not know is safer than this
	//! reader deciding the word meant `spec1`.
	QString octConv;
	//! THE COVERAGE CONTRACT of the base-colour sheet: the alpha at which the
	//! bake counted a texel covered, the alpha a consumer is to TEST at, and the
	//! alpha the floor was written at (lane CARDWIDTH, 2026-09-10; the bake's
	//! `coverage <floor> <test> <base>` line). All three ZERO means the sidecar
	//! did not say, which is the older vintage: its alpha is the raw coverage
	//! fraction, so a consumer testing at 0.5 draws a silhouette up to 5.41
	//! texels of half-width smaller than the `half` on the same line describes.
	//! Absence is passed through as absence -- a `.lodm` written before the key
	//! stays byte-identical, and no contract is invented for bytes that have none.
	int octCovFloor = 0;
	int octCovTest = 0;
	int octCovBase = 0;
	QString octSource;          // the model file the bake photographed (the meta's `model` line)
	QString octPath;            // game path of the set's .lodm (docs/LODGEN_IMPOSTOR_SPEC.md)
};

/*! Colour under the transparent texels of a sheet of frames.
 *
 *  Filtering and mips blend a texel with its neighbours, so what sits under
 *  a transparent texel next to a leaf edge is what the leaf edge shows: with
 *  black there, every card grew a dark fringe - the atlas's black-fringed mud
 *  (WW_CHANGES 2026-08-05) on the impostor sheets, and on the crossed cards
 *  since they were written. bungo, on the first spec bake: "see the pixels on
 *  the edges, there's no padding".
 *
 *  Every channel is extended outward from the coverage edge, `passes` texels
 *  deep, each pass giving an unfilled texel the average of its filled
 *  8-neighbours; what is still unfilled is flooded with the frame's average
 *  covered value. Coverage itself (the alpha of `coverage`) is never touched,
 *  and nothing crosses a frame border: each frame is dilated on its own, so a
 *  gutter stays a gutter. `img` may be the coverage image itself (the base
 *  colour sheet), in which case only its RGB moves. */
static void lodgenDilateFrames( QImage & img, const QImage & coverage, int frameW, int frameH, int passes )
{
	if ( img.isNull() || coverage.size() != img.size() || frameW <= 0 || frameH <= 0 )
		return;
	const int W = img.width(), H = img.height();
	std::vector<quint8> filled( size_t( W ) * H, 0 );
	for ( int y = 0; y < H; y++ )
		for ( int x = 0; x < W; x++ )
			// the coverage floor (docs/LODGEN_IMPOSTOR_SPEC.md): under 16/255 a texel's colour is the
			// rounding of one or two source pixels, often black, and is filled from its neighbours instead
			filled[size_t( y ) * W + x] = qAlpha( coverage.pixel( x, y ) ) >= 16 ? 1 : 0;
	const bool isCoverage = ( &img == &coverage );
	auto put = [&]( int x, int y, int r, int g, int b, int a ) {
		if ( isCoverage )
			img.setPixel( x, y, qRgba( r, g, b, qAlpha( img.pixel( x, y ) ) ) );
		else
			img.setPixel( x, y, qRgba( r, g, b, a ) );
	};
	for ( int fy = 0; fy < H; fy += frameH ) {
		for ( int fx = 0; fx < W; fx += frameW ) {
			const int x1 = qMin( W, fx + frameW ), y1 = qMin( H, fy + frameH );
			// the frame's average covered value, the flood for what dilation never reaches
			quint64 sr = 0, sg = 0, sb = 0, sa = 0, n = 0;
			for ( int y = fy; y < y1; y++ )
				for ( int x = fx; x < x1; x++ )
					if ( filled[size_t( y ) * W + x] ) {
						const QRgb p = img.pixel( x, y );
						sr += qRed( p ); sg += qGreen( p ); sb += qBlue( p ); sa += qAlpha( p ); n++;
					}
			if ( !n )
				continue;		// an empty frame stays as it is
			const int ar = int( sr / n ), ag = int( sg / n ), ab = int( sb / n ), aa = int( sa / n );
			for ( int pass = 0; pass < passes; pass++ ) {
				std::vector<quint8> next( filled );
				for ( int y = fy; y < y1; y++ ) {
					for ( int x = fx; x < x1; x++ ) {
						if ( filled[size_t( y ) * W + x] )
							continue;
						int r = 0, g = 0, b = 0, a = 0, k = 0;
						for ( int dy = -1; dy <= 1; dy++ )
							for ( int dx = -1; dx <= 1; dx++ ) {
								const int sx = x + dx, sy = y + dy;
								if ( ( !dx && !dy ) || sx < fx || sy < fy || sx >= x1 || sy >= y1 )
									continue;
								if ( !filled[size_t( sy ) * W + sx] )
									continue;
								const QRgb p = img.pixel( sx, sy );
								r += qRed( p ); g += qGreen( p ); b += qBlue( p ); a += qAlpha( p ); k++;
							}
						if ( k ) {
							put( x, y, r / k, g / k, b / k, a / k );
							next[size_t( y ) * W + x] = 1;
						}
					}
				}
				filled.swap( next );
			}
			for ( int y = fy; y < y1; y++ )
				for ( int x = fx; x < x1; x++ )
					if ( !filled[size_t( y ) * W + x] )
						put( x, y, ar, ag, ab, aa );
		}
	}
}

/*! THE HEIGHT CHANNEL'S REPAIR, and the one that stops the card coming apart.
 *
 *  THE DEFECT. The `_n` sheet's blue is the frame's height, and the drawer's
 *  parallax step reads it at the UNPARALLAXED uv and then moves the sample by
 *  `(want - d) / dot(ray, frameFwd)` world units sideways. So the height of a
 *  texel the object does not cover decides where a card pixel OUTSIDE the
 *  silhouette goes looking -- and until this function existed that height was
 *  whatever `lodgenDilateFrames` had flooded there: the frame's AVERAGE, which
 *  for a bare tree is nothing like the card plane.
 *
 *  Measured on the blast_n4 fixture's shipped `_oct_n.DDS`: texels under the
 *  coverage floor decode to +264 world units on average and +743 at the 95th
 *  percentile, against a card half-width of 135. Every one of those is a
 *  licence to drag a trunk texel several card-widths sideways into empty sky,
 *  and that is exactly what the picture showed -- with the parallax switched
 *  off the same card at the same directions is a clean trunk, and with it on
 *  it is a spray of detached flakes. (Lane IMPOSTORFIX1, 2026-09-19; the pair
 *  of pictures is scratchpad/impostorfix1_20260919/control/look_blendon.png.)
 *
 *  Partially covered texels are the same fault one step in. The composition
 *  un-premultiplies every channel by the measured coverage, which is right for
 *  colour and normal, but a partial texel's DEPTH is not a weighted average of
 *  anything -- half a texel of twig in front of sky has one depth, not a
 *  blend of the twig's and the sky's -- so the division there manufactures a
 *  number no surface ever had.
 *
 *  THE REPAIR, in the frame's own coordinates and nowhere else:
 *    coverage >= 250   the texel is whole; its height stands.
 *    16 <= cov < 250   partial; take the height of the nearest FULLY covered
 *                      texel, which is a depth some surface actually had.
 *    coverage < 16,    outside the object but WITHIN 8 RINGS of a whole texel:
 *      ring <= 8       the same dilated height. A neighbouring frame's ray
 *                      lands just outside this frame's silhouette constantly,
 *                      and out there the object's own depth is the only honest
 *                      answer; the card plane is a claim that the surface is
 *                      at z = 0, which for a fat solid object is the one place
 *                      it certainly is not.
 *    coverage < 16,    far outside; the card plane, 128, which makes the
 *      ring > 8        parallax step an exact no-op. Nothing samples out
 *                      there, so the plane is harmless.
 *
 *  THE RING METRIC IS CHEBYSHEV, NOT EUCLIDEAN, and that is a divergence from
 *  the simulation this was chosen on: the dilation below grows by 8-connected
 *  passes, so "ring 8" is a square of radius 8, not a disc. The square's
 *  corners reach 11.3 texels. The simulation measured a Euclidean disc of 8
 *  (R2d8) AND one of 16 (R2d16) and both beat the card plane on all five
 *  subjects, so the answer does not turn on which of the two metrics is used;
 *  it is named here so nobody reads "8" as the simulation's 8.
 *
 *  WHY THIS ONE AND NOT THE OTHERS. Four candidates were scored in a numpy
 *  reference card reading these same BC3 bytes, over the 24 orbit views of
 *  blast_n4, against the N=12 set as the stand-in subject:
 *      as shipped                                  0.3585
 *      card plane on every non-full texel          0.4563
 *      dilate from fully covered, nothing else     0.4288
 *      dilate, then card plane OUTSIDE coverage    0.4587
 *      card plane outside coverage only            0.4504
 *
 *  THAT TABLE WAS SCORED ON ONE SUBJECT (blast_n4 against the N=12 set) AND
 *  IT NEVER TESTED A DILATION THAT REACHED OUTSIDE THE COVERAGE FLOOR, which
 *  is why the rule it picked cost the one fat solid subject in the fixture set
 *  2.8 per cent of its silhouette. Lane IMPOSTORFIX2 (2026-09-19) re-scored
 *  six fills on all FIVE subjects, 24 orbit views each, through the real BC3
 *  round trip, with the registration frozen:
 *
 *      fill outside coverage      blast_n4 blast_n8  maple  dead_n4  rock_n4
 *      as baked (frame mean)        0.3599  0.3794  0.3569  0.4187  0.7894
 *      the card plane everywhere    0.4550  0.5943  0.3232  0.5391  0.7596
 *      card plane outside (above)   0.4978  0.6639  0.3609  0.5826  0.7777
 *      DILATE 8, plane beyond       0.5646  0.7200  0.3685  0.6153  0.8331
 *      dilate 16, plane beyond      0.5515  0.7165  0.3690  0.5907  0.8416
 *      dilate to the whole frame    0.5302  0.7163  0.3694  0.5898  0.8380
 *
 *  The 8-ring dilation is better than the card plane on every subject, and on
 *  the rock it also clears the number the regression was measured against
 *  (0.7944 before any of this) by +0.039, so no subject is traded for another.
 *  Dilating to the WHOLE frame is worse than 8 on three of the five: far from
 *  the object the nearest whole texel's height is not that pixel's depth
 *  either, and the plane is the better default there.
 *  The same table re-run with the height channel told a depthSpan fitted to
 *  the object instead of the clip range's 3 x bound tops out at 0.4602 -- but
 *  `depthSpan` is not a free parameter. It is a CLAIM about the projection:
 *  `GLView::glProjection` gives the orthographic bake the clip range
 *  |center.z| +- 1.5 x bound, `gl_FragCoord.z` is linear across exactly that,
 *  and `3 * max(radius, 1024)` is that range written down. Narrowing it means
 *  narrowing the bake's clip range, and 0.0015 of IoU does not buy a change to
 *  the projection. This repair needs no format change, no spec change and no
 *  new clause: it only stops writing numbers that were never depths.
 */
static void lodgenRepairOctHeight( QImage & nrm, const QImage & alb, int frameW, int frameH, QString * report )
{
	if ( nrm.isNull() || alb.size() != nrm.size() || frameW <= 0 || frameH <= 0 )
		return;
	const int W = nrm.width(), H = nrm.height();
	const int kFull = 250;		// "the object covers this texel whole"
	const int kFloor = 16;		// the spec's coverage floor
	const int kOutRamp = 16;	// IMPOSTORFIX4: the outside fill RAMPS to the card plane over
								// this many rings instead of snapping at ring 8. A snap is a
								// cliff, and a cliff inside one 4x4 BC1 block gives the block a
								// height range its single colour line cannot carry -- which is
								// what the trunk chips are (IMPOSTORFIX4 s2e/s5).
	qint64 partial = 0, outside = 0, outsideNear = 0, whole = 0, violations = 0;
	int worst = 0;
	for ( int fy = 0; fy + frameH <= H; fy += frameH ) {
		for ( int fx = 0; fx + frameW <= W; fx += frameW ) {
			/* seed: the fully covered texels of THIS frame, and the band of
			 * heights they occupy -- the frame's own idea of the object's depth,
			 * which is what the self-check below is measured against. */
			std::vector<quint8> have( size_t( frameW ) * frameH, 0 );
			std::vector<quint8> hgt( size_t( frameW ) * frameH, 128 );
			/* ring[t] = how many dilation passes it took to reach t, i.e. the
			 * Chebyshev distance from t to the nearest FULLY covered texel.
			 * 0 on the seeds themselves. Only the outside-coverage branch
			 * below reads it; the partial branch is unchanged. */
			std::vector<quint16> ring( size_t( frameW ) * frameH, 0 );
			int hMin = 255, hMax = 0, nFull = 0;
			for ( int y = 0; y < frameH; y++ ) {
				for ( int x = 0; x < frameW; x++ ) {
					const QRgb p = nrm.pixel( fx + x, fy + y );
					hgt[size_t( y ) * frameW + x] = quint8( qBlue( p ) );
					if ( qAlpha( alb.pixel( fx + x, fy + y ) ) >= kFull ) {
						have[size_t( y ) * frameW + x] = 1;
						hMin = qMin( hMin, qBlue( p ) );
						hMax = qMax( hMax, qBlue( p ) );
						nFull++;
					}
				}
			}
			if ( !nFull )
				continue;		// a frame with no whole texel has no depth to spread
			whole += nFull;
			/* spread the fully covered heights outward, one ring per pass, until
			 * every texel of the frame has one: the nearest whole texel's height,
			 * ties averaged. The frame is at most a few hundred texels across. */
			for ( int pass = 0; pass < frameW + frameH; pass++ ) {
				std::vector<quint8> next( have );
				bool grew = false;
				for ( int y = 0; y < frameH; y++ ) {
					for ( int x = 0; x < frameW; x++ ) {
						if ( have[size_t( y ) * frameW + x] )
							continue;
						int s = 0, k = 0;
						for ( int dy = -1; dy <= 1; dy++ )
							for ( int dx = -1; dx <= 1; dx++ ) {
								const int sx = x + dx, sy = y + dy;
								if ( ( !dx && !dy ) || sx < 0 || sy < 0 || sx >= frameW || sy >= frameH )
									continue;
								if ( !have[size_t( sy ) * frameW + sx] )
									continue;
								s += hgt[size_t( sy ) * frameW + sx]; k++;
							}
						if ( k ) {
							hgt[size_t( y ) * frameW + x] = quint8( s / k );
							next[size_t( y ) * frameW + x] = 1;
							ring[size_t( y ) * frameW + x] = quint16( pass + 1 );
							grew = true;
						}
					}
				}
				have.swap( next );
				if ( !grew )
					break;
			}
			for ( int y = 0; y < frameH; y++ ) {
				for ( int x = 0; x < frameW; x++ ) {
					const int a = qAlpha( alb.pixel( fx + x, fy + y ) );
					if ( a >= kFull )
						continue;
					const QRgb p = nrm.pixel( fx + x, fy + y );
					int b;
					if ( a >= kFloor ) {
						b = hgt[size_t( y ) * frameW + x];
						partial++;
					} else if ( ring[size_t( y ) * frameW + x] ) {
						/* just outside the silhouette: a neighbouring frame's ray
						 * lands here, and the object's own depth is what it should
						 * read. Carried out from the whole texels, not invented --
						 * and faded to the card plane over kOutRamp rings rather
						 * than dropped onto it in one texel. The fade is what keeps
						 * a 4x4 block's height range inside what BC1 can carry. */
						const int r = qMin( int( ring[size_t( y ) * frameW + x] ), kOutRamp );
						const int d = hgt[size_t( y ) * frameW + x];
						b = ( d * ( kOutRamp - r ) + 128 * r + kOutRamp / 2 ) / kOutRamp;
						if ( r < kOutRamp )
							outsideNear++;
						else
							outside++;
					} else {
						b = 128;		// unreachable from any whole texel: the card plane,
										// where the parallax step is an exact no-op
						outside++;
					}
					nrm.setPixel( fx + x, fy + y, qRgba( qRed( p ), qGreen( p ), b, qAlpha( p ) ) );
				}
			}
			/* THE BAKE-TIME SELF-CHECK the repair is allowed to be judged by:
			 * after it, no texel the object COVERS may decode to a height
			 * outside the depth band the frame's own whole texels occupy.
			 * Before it, a partial texel of this fixture reached 255 -- the far
			 * plane, eleven card half-widths behind the tree. */
			for ( int y = 0; y < frameH; y++ )
				for ( int x = 0; x < frameW; x++ ) {
					if ( qAlpha( alb.pixel( fx + x, fy + y ) ) < kFloor )
						continue;
					const int b = qBlue( nrm.pixel( fx + x, fy + y ) );
					if ( b < hMin || b > hMax ) {
						violations++;
						worst = qMax( worst, qMax( hMin - b, b - hMax ) );
					}
				}
		}
	}
	if ( report )
		*report = QStringLiteral( "oct height repaired: %1 whole kept, %2 partial filled from the nearest"
			" whole texel, %3 outside inside the %6-ring ramp carried the object's height faded towards"
			" the card plane, %4 outside set to the card plane; self-check %5" )
			.arg( whole ).arg( partial ).arg( outsideNear ).arg( outside )
			.arg( violations == 0 ? QStringLiteral( "PASS (no covered texel decodes outside its frame's depth)" )
				: QStringLiteral( "FAIL: %1 covered texels outside the frame's depth, worst by %2 levels" )
					.arg( violations ).arg( worst ) )
			.arg( kOutRamp );
}

//! One BC4 block (the BC3 alpha block by another name): eight-step palette over the block's min and max.
static void lodgenEncodeBC4Block( const std::vector<quint8> & ch, int w, int h, int bx, int by, quint8 * out )
{
	quint8 a[16];
	quint8 aMin = 255, aMax = 0;
	for ( int i = 0; i < 16; i++ ) {
		const int sx = qMin( bx * 4 + ( i & 3 ), w - 1 );
		const int sy = qMin( by * 4 + ( i >> 2 ), h - 1 );
		a[i] = ch[size_t( sy ) * w + sx];
		aMin = qMin( aMin, a[i] );
		aMax = qMax( aMax, a[i] );
	}
	out[0] = aMax;
	out[1] = aMin;
	quint64 bits = 0;
	quint8 pal[8];
	pal[0] = aMax;
	pal[1] = aMin;
	for ( int k = 1; k < 7; k++ )
		pal[k + 1] = quint8( ( ( 7 - k ) * aMax + k * aMin ) / 7 );
	for ( int i = 15; i >= 0; i-- ) {
		int best = 0, bd = 256;
		for ( int k = 0; k < 8; k++ ) {
			const int d = qAbs( int( a[i] ) - int( pal[k] ) );
			if ( d < bd ) { bd = d; best = k; }
		}
		bits = ( bits << 3 ) | quint64( best );
	}
	for ( int k = 0; k < 6; k++ )
		out[2 + k] = quint8( bits >> ( k * 8 ) );
}

/*! A two-channel BC5 DDS (DXGI 83, DX10 header): red then green, each a BC4
 *  block, mips by box filter. The impostor's depth (R) and sway (G) sheet:
 *  two independent scalars, which a BC1 would have forced through one
 *  colour palette per block. */
bool lodgenWriteDdsBC5( const QString & path, int w, int h,
	const std::vector<quint8> & r, const std::vector<quint8> & g )
{
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly ) )
		return false;
	std::vector<std::vector<quint8>> mr, mg;
	mr.push_back( r );
	mg.push_back( g );
	int mw = w, mh = h;
	while ( mw > 4 && mh > 4 ) {
		const int nw = mw / 2, nh = mh / 2;
		std::vector<quint8> nr( size_t( nw ) * nh ), ng( size_t( nw ) * nh );
		const std::vector<quint8> & pr = mr.back();
		const std::vector<quint8> & pg = mg.back();
		for ( int y = 0; y < nh; y++ )
			for ( int x = 0; x < nw; x++ ) {
				unsigned sr = 0, sg = 0;
				for ( int sy = 0; sy < 2; sy++ )
					for ( int sx = 0; sx < 2; sx++ ) {
						sr += pr[size_t( y * 2 + sy ) * mw + ( x * 2 + sx )];
						sg += pg[size_t( y * 2 + sy ) * mw + ( x * 2 + sx )];
					}
				// round to nearest, not truncate: the loss compounds down a mip chain
				nr[size_t( y ) * nw + x] = quint8( ( sr + 2 ) >> 2 );
				ng[size_t( y ) * nw + x] = quint8( ( sg + 2 ) >> 2 );
			}
		mr.push_back( std::move( nr ) );
		mg.push_back( std::move( ng ) );
		mw = nw;
		mh = nh;
	}
	std::vector<quint8> data;
	mw = w;
	mh = h;
	for ( size_t m = 0; m < mr.size(); m++ ) {
		const int bw = ( mw + 3 ) / 4, bh = ( mh + 3 ) / 4;
		const size_t at = data.size();
		data.resize( at + size_t( bw ) * bh * 16 );
		// BLOCK ROWS IN PARALLEL: each row writes its own disjoint slice of
		// `data` and reads `mr`/`mg` only. Serial at one thread (lodgenparallel.h).
		lodgenParallelFor( bh, [&]( int by ) {
			for ( int bx = 0; bx < bw; bx++ ) {
				quint8 * o = data.data() + at + ( size_t( by ) * bw + bx ) * 16;
				lodgenEncodeBC4Block( mr[m], mw, mh, bx, by, o );
				lodgenEncodeBC4Block( mg[m], mw, mh, bx, by, o + 8 );
			}
		} );
		mw = qMax( 4, mw / 2 );
		mh = qMax( 4, mh / 2 );
	}
	quint32 hdr[32] = { 0 };
	hdr[0] = 0x20534444;
	hdr[1] = 124;
	hdr[2] = 0x000A1007;
	hdr[3] = quint32( h );
	hdr[4] = quint32( w );
	hdr[5] = quint32( ( ( w + 3 ) / 4 ) * ( ( h + 3 ) / 4 ) * 16 );
	hdr[7] = quint32( mr.size() );
	hdr[19] = 32;
	hdr[20] = 0x4;
	hdr[21] = 0x30315844U;          // 'DX10'
	hdr[27] = 0x401008;
	const quint32 dx10[5] = { 83U, 3U, 0U, 1U, 0U };   // BC5_UNORM, 2D, one texture
	f.write( reinterpret_cast<const char *>( hdr ), 128 );
	f.write( reinterpret_cast<const char *>( dx10 ), 20 );
	return f.write( reinterpret_cast<const char *>( data.data() ), qint64( data.size() ) ) == qint64( data.size() );
}

//! Look up (and lazily DDS-convert) an impostor card for a base form.
const LodgenCard & lodgenCard( const QString & dir, quint32 formID,
	QHash<quint32, LodgenCard> & cache, int auxDiv )
{
	auto it = cache.constFind( formID );
	if ( it != cache.constEnd() )
		return *it;
	LodgenCard card;
	const QString id = QString( "%1" ).arg( formID, 8, 16, QChar( '0' ) );
	const QString metaPath = dir + "/" + id + QStringLiteral( ".txt" );
	const QString frontPng = dir + "/" + id + QStringLiteral( "_front.png" );
	QFile meta( metaPath );
	if ( meta.open( QIODevice::ReadOnly | QIODevice::Text )
		&& QFile::exists( frontPng ) ) {
		/* The meta is one line per photograph: `front`, `side`, and since the
		 * octahedral bake `oct N tile halfW halfH cx cy cz depthspan`. The
		 * first reader took only the first line; this one takes them all. */
		/* The per-frame offsets arrive as one `frameoff i j ox oy` line each and
		 * are indexed by the grid, which the `oct` line carries -- so they are
		 * collected raw here and placed once the whole meta has been read,
		 * rather than depending on the order of two kinds of line. */
		QVector<float> rawFrameOff;
		while ( !meta.atEnd() ) {
			// trimmed: readLine() keeps the newline, and the last token is a WORD since the family
			const QStringList line = QString::fromLatin1( meta.readLine() ).trimmed()
				.split( QChar( ' ' ), Qt::SkipEmptyParts );
			if ( line.isEmpty() )
				continue;
			// `card` is what the synthetic-card harness writes for the front line
			if ( ( line[0] == QLatin1String( "front" ) || line[0] == QLatin1String( "card" ) ) && line.size() >= 6 ) {
				card.halfW = line[1].toFloat();
				card.halfH = line[2].toFloat();
				card.center = Vector3( line[3].toFloat(), line[4].toFloat(), line[5].toFloat() );
			} else if ( line[0] == QLatin1String( "model" ) && line.size() >= 2 ) {
				card.octSource = line.mid( 1 ).join( QChar( ' ' ) );
			} else if ( line[0] == QLatin1String( "oct" ) && line.size() >= 11 ) {
				// oct N tileW tileH halfW halfH cx cy cz depthspan family [base]
				card.octPbr = ( line[10] == QLatin1String( "pbr" ) );
				card.oct = line[1].toInt();
				card.octTileW = line[2].toInt();
				card.octTileH = line[3].toInt();
				/* A bake from before the size ladder has no base token; its frame
				 * WAS the run's resolution, so the long side stands in and the
				 * .lodm still says something true. */
				card.octBase = line.size() >= 12 ? line[11].toInt()
					: qMax( card.octTileW, card.octTileH );
				/* THE VIEW CONVENTION TOKEN, appended after `base` so every older
				 * reader -- which indexes by position and stops at 11 or 12 -- is
				 * untouched. Absent = the pre-2026-09-19 bake, whose azimuth was
				 * turned by 180 degrees. */
				card.octConv = line.size() >= 13 ? line[12] : QString();
				card.octHalfW = line[4].toFloat();
				card.octHalfH = line[5].toFloat();
				card.octCenter = Vector3( line[6].toFloat(), line[7].toFloat(), line[8].toFloat() );
				card.octSpan = line[9].toFloat();
			} else if ( line[0] == QLatin1String( "gap" ) && line.size() >= 3 ) {
				/* THE GAP between two neighbouring silhouettes across a frame border,
				 * per axis (bungo, 2026-09-09). The margin on each side is half of it,
				 * and the mip cap divides the gap itself. */
				card.octGapX = qMax( 2, line[1].toInt() );
				card.octGapY = qMax( 2, line[2].toInt() );
				card.octPadX = card.octGapX / 2;
				card.octPadY = card.octGapY / 2;
				card.octMipUnit = qMin( card.octGapX, card.octGapY );
			} else if ( line[0] == QLatin1String( "pad" ) && line.size() >= 3 ) {
				/* A sidecar from lane CARDFIT3 (2026-09-09, superseded the same day):
				 * the number is the PER-SIDE margin and its chain was capped at
				 * 1 + log2(pad). Read under its own law so it still converts to the
				 * sheet it was baked for. */
				card.octPadX = line[1].toInt();
				card.octPadY = line[2].toInt();
				card.octGapX = 2 * card.octPadX;
				card.octGapY = 2 * card.octPadY;
				/* Its gap is twice its number, and log2(2*pad) = 1 + log2(pad) is
				 * exactly the chain it was built for -- so the 2026-09-09 evening
				 * change of the law leaves this vintage's count where it was. */
				card.octMipUnit = qMin( card.octGapX, card.octGapY );
			} else if ( line[0] == QLatin1String( "frameoff" ) && line.size() >= 5 ) {
				/* PER-FRAME POSITIONING: one line per frame, `frameoff i j ox oy`,
				 * the offset in MODEL UNITS from the card's centre along that view's
				 * own right and up axes. One line per frame rather than one long
				 * line of 2*N*N numbers, because every reader of this file splits on
				 * spaces and indexes by position (docs/MISTAKES.md, the family token
				 * that carried a newline). */
				rawFrameOff.append( float( line[1].toInt() ) );
				rawFrameOff.append( float( line[2].toInt() ) );
				rawFrameOff.append( line[3].toFloat() );
				rawFrameOff.append( line[4].toFloat() );
			} else if ( line[0] == QLatin1String( "projection" ) && line.size() >= 2 ) {
				/* THE BAKE'S CAMERA, in the bake's own words: `ortho` or `persp`
				 * (lane CARDORTHO, 2026-09-10). Nothing here is DERIVED from it --
				 * the numbers on the other lines are what they are -- but it is the
				 * only thing that says whether they describe the sheet beside them,
				 * and a set without the line was photographed through a perspective
				 * frustum and measured as if it were not. Passed through to the
				 * .lodm verbatim; an unrecognised word is carried too, because a
				 * consumer refusing what it does not know is safer than this reader
				 * silently deciding the word meant `ortho`. */
				card.octProjection = line[1];
			} else if ( line[0] == QLatin1String( "coverage" ) && line.size() >= 4 ) {
				/* THE COVERAGE CONTRACT (lane CARDWIDTH, 2026-09-10):
				 * `coverage <floor> <test> <base>`. Read as three numbers and
				 * carried to the `.lodm` unchanged -- nothing here is derived from
				 * them and nothing here re-encodes a sheet, because the sheet was
				 * written under this contract by the bake that also wrote the line.
				 * A sidecar without the line leaves all three at 0 and the `.lodm`
				 * without the key, which is exactly what an older set is. */
				card.octCovFloor = qBound( 1, line[1].toInt(), 255 );
				card.octCovTest = qBound( 1, line[2].toInt(), 255 );
				card.octCovBase = qBound( 1, line[3].toInt(), 255 );
			} else if ( line[0] == QLatin1String( "emissive" ) && line.size() >= 2 ) {
				// the set's emissive multiple; the colour is already in the sheet
				card.octEmissiveScale = line[1].toFloat();
			} else if ( line[0] == QLatin1String( "oct" ) ) {
				// a bake from before the families: its third sheet means something else
				fprintf( stderr, "lodgen: card %s: oct line without a family, rebake it (docs/LODGEN_IMPOSTOR_SPEC.md)\n",
					id.toLocal8Bit().constData() );
			}
		}
		/* The per-frame offsets, placed on the grid now that the whole meta has
		 * been read. A frame the sidecar does not name keeps 0,0 -- the old
		 * behaviour, centred on the card's centre -- so a partial set degrades
		 * to the law before it rather than to nothing. */
		if ( card.oct >= 2 && !rawFrameOff.isEmpty() ) {
			card.octFrameOff.fill( 0.0f, 2 * card.oct * card.oct );
			for ( int k = 0; k + 3 < rawFrameOff.size(); k += 4 ) {
				const int fi = int( rawFrameOff[k] ), fj = int( rawFrameOff[k + 1] );
				if ( fi < 0 || fj < 0 || fi >= card.oct || fj >= card.oct )
					continue;
				card.octFrameOff[2 * ( fj * card.oct + fi )] = rawFrameOff[k + 2];
				card.octFrameOff[2 * ( fj * card.oct + fi ) + 1] = rawFrameOff[k + 3];
			}
		}
		if ( card.halfH > 0.0f ) {
			/* ONE sheet, front on the left half and side on the right, so the
			 * two crossed quads read two different views off one texture.
			 * The bake hook has always written _side.png; the first card
			 * builder converted only the front and put it on both quads, so
			 * an impostor looked the same from every angle. A card with no
			 * side view repeats the front - the old behaviour, now the
			 * fallback. Named _fs so a card DDS from before this is not
			 * reused with the old, front-only layout. */
			const QString sidePng = dir + "/" + id + QStringLiteral( "_side.png" );
			card.hasSide = QFile::exists( sidePng );
			const QString dds = dir + "/" + id + QStringLiteral( "_fs.DDS" );
			if ( !QFile::exists( dds ) ) {
				QImage front = QImage( frontPng ).convertToFormat( QImage::Format_ARGB32 );
				QImage side = card.hasSide
					? QImage( sidePng ).convertToFormat( QImage::Format_ARGB32 ) : front;
				if ( side.size() != front.size() )
					side = side.scaled( front.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
				const int w = front.width(), h = front.height();
				QImage sheet( 2 * w, h, QImage::Format_ARGB32 );
				for ( int y = 0; y < h; y++ ) {
					for ( int x = 0; x < w; x++ ) {
						sheet.setPixel( x, y, front.pixel( x, y ) );
						sheet.setPixel( w + x, y, side.pixel( x, y ) );
					}
				}
				lodgenDilateFrames( sheet, sheet, w, h, 8 );	// colour under the cut-out: no black fringe
				std::vector<quint32> px( size_t( w ) * 2 * size_t( h ) );
				for ( int y = 0; y < h; y++ )
					for ( int x = 0; x < 2 * w; x++ )
						px[size_t( y ) * ( 2 * w ) + x] = sheet.pixel( x, y );
				/* DXT5, WITH the alpha, because the alpha IS the cut-out. This
				 * sheet went out as DXT1 with `pfflags 0x4` and no
				 * DDPF_ALPHAPIXELS, so every card quad in a chunk drew as an
				 * OPAQUE SQUARE (lane IMAGES5, 2026-09-09,
				 * handoff_cards_in_chunk.png). Vanilla's own alpha-tested tree
				 * LOD textures -- Textures/LOD/Trees/MapleBranchesLOD_d.dds and
				 * ElmBranchesLOD_d.dds -- are DXT5, dwFlags 0x000A1007,
				 * pfflags 0x4, caps 0x401008, and that is byte for byte the
				 * header this writer emits with bc3 = true. */
				lodgenWriteDds( dds, 2 * w, h, px, true );
			}
			/* The GAME-RELATIVE card path, moved with the folder (lane LAYOUT1,
			 * 2026-09-16): `Data\Textures\Lodgen\Cards\` until today,
			 * `Data\FO4CSLOD\Cards\` now. Cards are per TREE and shared by
			 * every worldspace, so they sit beside the worldspace folders. */
			card.texPath = QStringLiteral( "Data\\" ) + lodgenFo4csGameCardPath()
				+ QChar( 92 ) + id + QStringLiteral( "_fs.DDS" );
			card.valid = true;
		}
		/* The octahedral sheets, when the bake made them: the four textures
		 * of docs/LODGEN_IMPOSTOR_SPEC.md under the set's family - legacy
		 * (_d, _n, _gsaos, _g) or pbr (_bc, _n, _rmaos, _e) - the first three
		 * BC3 and the emissive BC1, converted
		 * once from the bake's PNGs, and a `<id>_oct.lodm` beside them
		 * naming the family, the textures and the frame grid. The mips stop
		 * while a frame's shorter side spans eight texels: past that the box
		 * filter blends neighbouring views into one frame. The crossed quads
		 * stay for the stock engine; a consumer that reads the manifest's C
		 * line opens the .lodm and draws these instead. */
		const QString octAlb = dir + "/" + id + QStringLiteral( "_oct_albedo.png" );
		const QString octNrm = dir + "/" + id + QStringLiteral( "_oct_normal.png" );
		const QString octRm = dir + "/" + id + QStringLiteral( "_oct" ) + QLatin1String( lodmMaskSuffix( card.octPbr ) )
			+ QStringLiteral( ".png" );
		/* The fourth sheet, the emissive: `_g` on a legacy set, `_e` on a pbr
		 * one. OPTIONAL, so a bake from before it still converts - a set
		 * without one simply names no emissive in its .lodm. */
		const QString octEm = dir + "/" + id + QStringLiteral( "_oct" ) + QLatin1String( lodmEmissiveSuffix( card.octPbr ) )
			+ QStringLiteral( ".png" );
		if ( card.oct >= 2 && QFile::exists( octAlb ) && QFile::exists( octNrm ) && QFile::exists( octRm ) ) {
			QImage alb = QImage( octAlb ).convertToFormat( QImage::Format_ARGB32 );
			QImage nrm = QImage( octNrm ).convertToFormat( QImage::Format_ARGB32 );
			QImage rm = QImage( octRm ).convertToFormat( QImage::Format_ARGB32 );
			QImage emi;
			if ( QFile::exists( octEm ) ) {
				emi = QImage( octEm ).convertToFormat( QImage::Format_ARGB32 );
				if ( emi.size() != alb.size() )
					emi = QImage();
			}
			if ( !alb.isNull() && nrm.size() == alb.size() && rm.size() == alb.size() ) {
				/* Under the transparent texels: every channel of every sheet
				 * extended from the silhouette, frame by frame, as deep as the
				 * gutter and then some; the coverage alpha itself untouched. */
				const int deep = qMax( 8, qMax( card.octTileW, card.octTileH ) / 8 );
				lodgenDilateFrames( nrm, alb, card.octTileW, card.octTileH, deep );
				lodgenDilateFrames( rm, alb, card.octTileW, card.octTileH, deep );
				if ( !emi.isNull() )
					lodgenDilateFrames( emi, alb, card.octTileW, card.octTileH, deep );
				lodgenDilateFrames( alb, alb, card.octTileW, card.octTileH, deep );	// last: it is also the coverage
				/* AFTER the dilate, never before: the dilate is what floods the
				 * height outside the silhouette with the frame's average. */
				{
					QString heightReport;
					lodgenRepairOctHeight( nrm, alb, card.octTileW, card.octTileH, &heightReport );
					if ( !heightReport.isEmpty() )
						fprintf( stderr, "lodgen: card %s: %s\n", id.toLocal8Bit().constData(),
							heightReport.toLocal8Bit().constData() );
				}
				const int w = alb.width(), h = alb.height();
				const QString base = dir + "/" + id + QStringLiteral( "_oct" );
				/* THE MIP CAP, AND THE ONE LEVEL IT NO LONGER SHIPS (bungo, 2026-09-09
				 * evening: "SHIP ONE MIP FEWER: mips = log2(gap) so the deepest shipped
				 * level still has a full texel of margin per side").
				 *
				 * A frame's mips never mix ACROSS a border -- the box filter halves an
				 * even frame into an even frame -- but a reader sampling ON a frame's UV
				 * border takes half its value from the next frame, so what it picks up
				 * is decided by the MARGIN INSIDE EACH FRAME, gap/2 at level 0. The
				 * previous cap shipped while the whole gap was a texel, which is the
				 * level where each margin is HALF a texel and a border tap therefore
				 * reaches the neighbour's edge (measured on 13 of 19 trees, worst
				 * 64/255). Stopping one level earlier -- while gap / 2^(k+1) >= 1 -- is
				 * zero bleed at the same spacing:
				 *
				 *   mips = log2( min( gapX, gapY ) )
				 *
				 * so a 128-texel frame at gap 8 ships 128, 64, 32: three levels, which
				 * is what "8 pixels = 3 clean mips" meant all along.
				 *
				 * `octMipUnit` is THE GAP under every vintage -- named outright by a
				 * 2026-09-09 sidecar, and twice the per-side number the two older kinds
				 * wrote -- so this change moves the newest sets by one level and leaves
				 * the older ones exactly where they were: log2(2*pad) = 1 + log2(pad).
				 * The last fallback -- neither line -- is the pre-2026-09-09 sheets' own
				 * max(4, longSide/16) per side, i.e. twice that as a gap.
				 *
				 * Floored at one level: a 16-texel frame's gap is already on its floor
				 * of 2, whose margin is a single texel, so its chain is mip 0 alone.
				 * That is the fallback naming itself rather than a silent bleed. */
				const int padFallback = qMax( 4, qMax( card.octTileW, card.octTileH ) / 16 );
				const int padX = card.octPadX > 0 ? card.octPadX : padFallback;
				const int padY = card.octPadY > 0 ? card.octPadY : padFallback;
				const int gapX = card.octGapX > 0 ? card.octGapX : 2 * padFallback;
				const int gapY = card.octGapY > 0 ? card.octGapY : 2 * padFallback;
				const int mipUnit = card.octMipUnit > 0 ? card.octMipUnit : qMin( gapX, gapY );
				int frameMips = 0;
				for ( int g = mipUnit; g >= 2; g /= 2 )
					frameMips++;
				frameMips = qMax( 1, frameMips );
				auto pixels = []( const QImage & img ) {
					std::vector<quint32> px( size_t( img.width() ) * img.height() );
					for ( int y = 0; y < img.height(); y++ )
						for ( int x = 0; x < img.width(); x++ )
							px[size_t( y ) * img.width() + x] = img.pixel( x, y );
					return px;
				};
				bool ok = true;
				const QString colorSfx = QLatin1String( lodmColorSuffix( card.octPbr ) ) + QStringLiteral( ".DDS" );
				const QString maskSfx = QLatin1String( lodmMaskSuffix( card.octPbr ) ) + QStringLiteral( ".DDS" );
				/* THE AUX HALVING. The base colour keeps the frame's size - it
				 * carries the coverage in alpha, so it is the silhouette - and
				 * the other three come down by `auxDiv`. Done AFTER the frame
				 * dilation above, which is what makes it safe: the gutter is at
				 * least four texels, so a halving mixes nothing across a frame
				 * border. Frames are multiples of four, so a halved frame is
				 * still even and its mip cap still lands on a whole texel. */
				const int aw = qMax( 4, w / auxDiv ), ah = qMax( 4, h / auxDiv );
				/* The aux sheets' gap came down by auxDiv with everything else, so their
				 * clean depth does too: log2(gap/auxDiv), under the same law. The bake
				 * rounds the gap UP TO EVEN, so a halved frame still splits it into two
				 * whole texels down to gap 2; below that -- the 16- and 32-texel frames
				 * at --card-half-aux -- the division reaches 1, and the floor of one
				 * level is the fallback naming itself rather than a silent bleed. */
				int auxMips = 0;
				for ( int g = mipUnit / auxDiv; g >= 2; g /= 2 )
					auxMips++;
				auxMips = qMax( 1, auxMips );
				auto down = [auxDiv, aw, ah]( const QImage & img ) {
					return auxDiv <= 1 ? img
						: img.scaled( aw, ah, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
				};
				const QImage nrmA = down( nrm ), rmA = down( rm );
				if ( !QFile::exists( base + colorSfx ) )
					ok = lodgenWriteDds( base + colorSfx, w, h, pixels( alb ), true, frameMips ) && ok;
				const struct { QString suffix; const QImage * img; } sheets[2] = {
					{ QStringLiteral( "_n.DDS" ), &nrmA }, { maskSfx, &rmA } };
				for ( const auto & s : sheets ) {
					const QString path = base + s.suffix;
					if ( !QFile::exists( path ) )
						ok = lodgenWriteDds( path, aw, ah, s.img == &nrmA ? lodgenNLayout2( pixels( *s.img ) )
							: pixels( *s.img ), true, auxMips ) && ok;
				}
				// the emissive sheet is BC1: RGB only, no alpha to carry
				const QString emSfx = QLatin1String( lodmEmissiveSuffix( card.octPbr ) ) + QStringLiteral( ".DDS" );
				if ( !emi.isNull() && !QFile::exists( base + emSfx ) ) {
					const QImage emiA = down( emi );
					ok = lodgenWriteDds( base + emSfx, aw, ah, pixels( emiA ), false, auxMips ) && ok;
				}
				// the same move as the crossed-quad path above (lane LAYOUT1)
				const QString game = QStringLiteral( "Data\\" ) + lodgenFo4csGameCardPath()
					+ QChar( 92 ) + id + QStringLiteral( "_oct" );
				if ( ok ) {
					// the set's .lodm: family, the sheets, the frame grid (compact by design)
					QJsonObject root, tex, oc;
					root.insert( QStringLiteral( "lodm" ), 1 );
					root.insert( QStringLiteral( "family" ), card.octPbr ? QStringLiteral( "pbr" ) : QStringLiteral( "legacy" ) );
					root.insert( QStringLiteral( "kind" ), QStringLiteral( "card" ) );
					root.insert( QStringLiteral( "nlayout" ), 2 );	// `_n`: height in A, sway in B
					tex.insert( QLatin1String( lodmColorKey( card.octPbr ) ), game + colorSfx );
					tex.insert( QStringLiteral( "normal" ), game + QStringLiteral( "_n.DDS" ) );
					tex.insert( QLatin1String( lodmMaskKey( card.octPbr ) ), game + maskSfx );
					if ( !emi.isNull() )
						tex.insert( QStringLiteral( "emissive" ), game + emSfx );
					root.insert( QStringLiteral( "textures" ), tex );
					// the multiple the sheet is scaled by; 0 = this set emits nothing
					root.insert( QStringLiteral( "emissiveScale" ), double( card.octEmissiveScale ) );
					oc.insert( QStringLiteral( "oct" ), card.oct );
					oc.insert( QStringLiteral( "frame" ), QJsonArray{ card.octTileW, card.octTileH } );
					// the run's resolution: `frame` at or below it, per the size ladder
					oc.insert( QStringLiteral( "base" ), card.octBase );
					/* What the three sheets that are not the base colour were
					 * divided by. A consumer needs it only to size its own
					 * allocation: sampling is in normalised UV, so a halved
					 * sheet reads with the same coordinates. */
					if ( auxDiv > 1 )
						oc.insert( QStringLiteral( "auxDiv" ), auxDiv );
					/* THE PADDING IS THE PER-SIDE NUMBER, and `gap` is the distance
					 * between two neighbouring silhouettes across a frame border --
					 * bungo's own quantity, and exactly twice it. A reader needs `pad`
					 * to know which part of a frame is picture (the silhouette occupies
					 * the INNER rect, `frame - 2*pad`, while `half` still spans the whole
					 * frame) and `gap` to know the law the mip count came from,
					 * log2(min(gap)). A set that carries `pad` and no `gap` is a
					 * CARDFIT3 set, whose gap is twice its number and whose count under
					 * the same expression is the 1 + log2(min(pad)) it was built for. */
					oc.insert( QStringLiteral( "pad" ), QJsonArray{ padX, padY } );
					oc.insert( QStringLiteral( "gap" ), QJsonArray{ gapX, gapY } );
					oc.insert( QStringLiteral( "half" ), QJsonArray{ double( card.octHalfW ), double( card.octHalfH ) } );
					oc.insert( QStringLiteral( "center" ), QJsonArray{ double( card.octCenter[0] ), double( card.octCenter[1] ), double( card.octCenter[2] ) } );
					/* PER-FRAME POSITIONING. `center` is the card's ONE centre and
					 * `half` its ONE size -- the scale is the same in every view, which
					 * is bungo's rule -- and this is where each frame's quad sits
					 * relative to that centre: two numbers per frame, in MODEL UNITS,
					 * along that view's own right and up axes, in the frames' own sheet
					 * order (frame (i,j) at index j*oct + i, so element 2*(j*oct+i) is
					 * its right offset and the next its up offset). Absent = a set from
					 * before the law, whose frames were all centred on `center`; a
					 * reader that ignores the key gets exactly that older behaviour and
					 * a tree that steps sideways at the transition by the offset it
					 * skipped. */
					if ( !card.octFrameOff.isEmpty() ) {
						QJsonArray fo;
						for ( float v : card.octFrameOff )
							fo.append( double( v ) );
						oc.insert( QStringLiteral( "frameOffset" ), fo );
					}
					oc.insert( QStringLiteral( "depthSpan" ), double( card.octSpan ) );
					oc.insert( QStringLiteral( "mips" ), frameMips );
					/* THE CAMERA, NAMED (lane CARDORTHO, 2026-09-10). `half`,
					 * `center` and `frameOffset` are world measurements taken off
					 * viewport pixels through one units-per-pixel constant, which
					 * only an ORTHOGRAPHIC camera makes true. This says whether the
					 * set has one. ABSENT means the sidecar did not say, and every
					 * sidecar that did not say came from a bake that drew a
					 * 60-degree perspective frustum -- so absence is not "unknown",
					 * it is the older, foreshortened vintage, and a consumer may
					 * treat it as such. Written only when the sidecar states it, so
					 * every `.lodm` produced before this key is byte-identical
					 * still. */
					if ( !card.octProjection.isEmpty() )
						oc.insert( QStringLiteral( "projection" ), card.octProjection );
					/* THE VIEW CONVENTION (2026-09-19). `spec1` = frame (i,j) is the
					 * view from direction (i,j), the spec's own law. Written only
					 * when the sidecar states it, so every `.lodm` produced before
					 * this key is byte-identical still -- and its absence is the
					 * statement that the set predates the azimuth repair and has to
					 * be re-baked, which a viewer can say out loud instead of
					 * drawing the back of a tree at the front. */
					if ( !card.octConv.isEmpty() )
						oc.insert( QStringLiteral( "conv" ), card.octConv );
					/* THE COVERAGE CONTRACT (lane CARDWIDTH, 2026-09-10). `floor` is
					 * the coverage at which the bake counted a texel covered and
					 * measured `half` and every `frameOffset`; `test` is the alpha a
					 * consumer must test at to select THAT SET and no other; `base` is
					 * the alpha the floor was written at, so the coverage FRACTION is
					 *     floor + (a - base) * (255 - floor) / (255 - base)
					 * for a consumer that blends a bare crown instead of testing it.
					 *
					 * ABSENT means the sheet's alpha is the raw fraction and the two
					 * silhouettes disagree: a consumer testing at 0.5 on such a set
					 * draws up to 5.41 texels of half-width less than `half` declares,
					 * which is the tree changing size at the transition. A consumer
					 * that wants the declared silhouette out of an older set tests at
					 * 16/255 instead. Written only when the sidecar states it, so every
					 * `.lodm` produced before this key is byte-identical still. */
					if ( card.octCovFloor > 0 && card.octCovTest > 0 && card.octCovBase > 0 ) {
						QJsonObject cov;
						cov.insert( QStringLiteral( "floor" ), card.octCovFloor );
						cov.insert( QStringLiteral( "test" ), card.octCovTest );
						cov.insert( QStringLiteral( "base" ), card.octCovBase );
						oc.insert( QStringLiteral( "coverage" ), cov );
					}
					if ( !card.octSource.isEmpty() )
						oc.insert( QStringLiteral( "source" ), card.octSource );
					root.insert( QStringLiteral( "card" ), oc );
					ok = lodmWriteFile( base + QStringLiteral( ".lodm" ), root );
				}
				if ( ok )
					card.octPath = game + QStringLiteral( ".lodm" );
				else
					card.oct = 0;
			} else {
				card.oct = 0;
			}
		} else {
			card.oct = 0;
		}
	}
	return *cache.insert( formID, card );
}

//! The two crossed quads of an impostor, in model space, as a LodSrcShape.
LodSrcShape lodgenCardShape( const LodgenCard & card )
{
	LodSrcShape s;
	s.tex0 = card.texPath;
	s.hasAlpha = true;
	s.alphaFlags = 4844;
	s.alphaThreshold = 128;
	const float cx = card.center[0], cy = card.center[1];
	const float z0 = card.center[2] - card.halfH, z1 = card.center[2] + card.halfH;
	// u0..u1: which half of the front|side sheet this quad reads
	auto quad = [&s]( const Vector3 & a, const Vector3 & b,
		const Vector3 & c, const Vector3 & d, const Vector3 & n, float u0, float u1 ) {
		const quint16 base = quint16( s.pos.size() );
		const Vector3 pts[4] = { a, b, c, d };
		const float us[4] = { u0, u1, u1, u0 };
		const float vs[4] = { 1, 1, 0, 0 };
		for ( int k = 0; k < 4; k++ ) {
			s.pos.append( pts[k] );
			s.nrm.append( n );
			s.tan.append( Vector3( 0, 0, 1 ) );
			s.uv.append( Vector2( us[k], vs[k] ) );
			s.col.append( Color4( 1, 1, 1, 1 ) );
		}
		s.tris.append( Triangle( base, quint16( base + 1 ), quint16( base + 2 ) ) );
		s.tris.append( Triangle( base, quint16( base + 2 ), quint16( base + 3 ) ) );
	};
	// front card (facing -Y) off the left half, side card (facing +X) off the right
	quad( Vector3( cx - card.halfW, cy, z0 ), Vector3( cx + card.halfW, cy, z0 ),
		Vector3( cx + card.halfW, cy, z1 ), Vector3( cx - card.halfW, cy, z1 ),
		Vector3( 0, -1, 0 ), 0.0f, 0.5f );
	quad( Vector3( cx, cy - card.halfW, z0 ), Vector3( cx, cy + card.halfW, z0 ),
		Vector3( cx, cy + card.halfW, z1 ), Vector3( cx, cy - card.halfW, z1 ),
		Vector3( 1, 0, 0 ), 0.5f, 1.0f );
	return s;
}

} // namespace

/* ----------------------------------------------------------------------------
 * AGGREGATE RING-3 IMPOSTORS -- the two things the compositor cannot do for
 * itself, because both live beside the rest of the texture bake: reading the
 * card library (this file owns the ONE card-sidecar reader, `lodgenCard`) and
 * writing a sheet (this file owns `lodgenWriteDds` and the frame dilation).
 * The composite itself is src/lodgenaggregate.cpp and touches neither.
 * -------------------------------------------------------------------------- */

QHash<quint32, LodgenAggCard> lodgenAggregateCards( const EsmWorld & world, const int region[4],
	const QString & cardDir, int auxDiv, QStringList * notes )
{
	QHash<quint32, LodgenAggCard> out;
	if ( cardDir.isEmpty() )
		return out;
	QHash<quint32, LodgenCard> cache;
	QSet<quint32> seen;
	int noSet = 0, notOrtho = 0, noOct = 0;
	auto consider = [&]( quint32 baseId ) {
		if ( !baseId || seen.contains( baseId ) )
			return;
		seen.insert( baseId );
		const EsmLodBase & b = world.lodBase( baseId );
		if ( !b.hasLod )
			return;
		/* THE TREE TEST IS THE SHARED ONE, called and not re-typed: the same
		 * `lodgenIsTreeModel` plus the TREE record type the candidate lister,
		 * the chunk builder and the repetition breaker all use (bungo's
		 * 2026-09-11 07:0x ruling, trees only). */
		QString probe = b.model;
		for ( int l = 0; l < 4 && probe.isEmpty(); l++ )
			probe = b.models[l];
		const bool isTree = std::memcmp( &b.type, "TREE", 4 ) == 0 || lodgenIsTreeModel( probe );
		if ( !isTree )
			return;
		const LodgenCard & c = lodgenCard( cardDir, baseId, cache, auxDiv );
		if ( !c.valid || c.oct <= 1 ) {
			noSet++;
			return;
		}
		if ( c.octProjection != QLatin1String( "ortho" ) ) {
			notOrtho++;
			return;
		}
		if ( c.octTileW <= 0 || c.octTileH <= 0 ) {
			noOct++;
			return;
		}
		LodgenAggCard a;
		a.dir = cardDir;
		a.formId = baseId;
		a.oct = c.oct;
		a.frameW = c.octTileW;
		a.frameH = c.octTileH;
		a.gapX = c.octGapX;
		a.gapY = c.octGapY;
		a.halfW = c.octHalfW;
		a.halfH = c.octHalfH;
		for ( int k = 0; k < 3; k++ )
			a.center[k] = c.octCenter[k];
		a.depthSpan = c.octSpan;
		a.frameOff = c.octFrameOff;
		a.covFloor = c.octCovFloor;
		a.covTest = c.octCovTest;
		a.covBase = c.octCovBase;
		a.ortho = true;
		a.pbr = c.octPbr;
		out.insert( baseId, a );
	};
	for ( int cy = region[1]; cy <= region[3]; cy++ )
		for ( int cx = region[0]; cx <= region[2]; cx++ )
			for ( const EsmRefr & r : world.refrs( cx, cy ) ) {
				if ( r.initiallyDisabled || r.deleted || !r.base )
					continue;
				if ( std::memcmp( &r.baseType, "SCOL", 4 ) == 0 ) {
					for ( const EsmScolPart & part : world.scolParts( r.base ) )
						consider( part.base );
				} else {
					consider( r.base );
				}
			}
	if ( notes ) {
		*notes << QString( "aggregate cards: %1 tree bases with a usable ortho card set; "
			"refused %2 with no set in %3, %4 baked through a perspective camera, %5 with no octahedral grid" )
			.arg( out.size() ).arg( noSet ).arg( cardDir ).arg( notOrtho ).arg( noOct );
	}
	return out;
}

bool lodgenAggregateWrite( const QString & outRoot, const QString & worldspace,
	const LodgenAggSet & set, QStringList * written, QString * error )
{
	auto fail = [&]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};
	/* ONE ROOT (lane LAYOUT1, 2026-09-16): `Textures\Lodgen\Aggregate\<ws>`
	 * until today, `FO4CSLOD\<ws>\Aggregate` now. An aggregate set is per
	 * worldspace CELL, so it belongs inside the worldspace's folder -- unlike
	 * the per-tree impostor cards, which sit beside the worldspaces. */
	const QString dir = lodgenFo4csWorldDir( outRoot, worldspace ) + QStringLiteral( "/Aggregate" );
	if ( !QDir().mkpath( dir ) )
		return fail( QString( "cannot create %1" ).arg( dir ) );
	const QString stem = dir + QChar( '/' ) + QString::number( set.cellX )
		+ QChar( '_' ) + QString::number( set.cellY ) + QStringLiteral( "_agg" );
	/* THE SAME DILATION the per-card sheets get, frame by frame, so a coarse
	 * mip that averages across a frame's margin still averages the forest's own
	 * colour rather than black (docs/LODGEN_CARD_SHEETS.md 3.1). The frame here
	 * is the aggregate's, and the sheet is one ROW of them. */
	QImage colour = set.colour, normal = set.normal, mask = set.mask;
	const int deep = qMax( 8, qMax( set.frameW, set.frameH ) / 8 );
	lodgenDilateFrames( colour, colour, set.frameW, set.frameH, deep );
	lodgenDilateFrames( normal, colour, set.frameW, set.frameH, deep );
	lodgenDilateFrames( mask, colour, set.frameW, set.frameH, deep );
	auto pixels = []( const QImage & img ) {
		std::vector<quint32> px( size_t( img.width() ) * img.height() );
		for ( int y = 0; y < img.height(); y++ )
			for ( int x = 0; x < img.width(); x++ )
				px[size_t( y ) * img.width() + x] = img.pixel( x, y );
		return px;
	};
	const QString colorSfx = QLatin1String( lodmColorSuffix( set.pbr ) ) + QStringLiteral( ".DDS" );
	const QString maskSfx = QLatin1String( lodmMaskSuffix( set.pbr ) ) + QStringLiteral( ".DDS" );
	const int w = colour.width(), h = colour.height();
	if ( !lodgenWriteDds( stem + colorSfx, w, h, pixels( colour ), true, set.mips ) )
		return fail( QString( "cannot write %1" ).arg( stem + colorSfx ) );
	if ( !lodgenWriteDds( stem + QStringLiteral( "_n.DDS" ), w, h, lodgenNLayout2( pixels( normal ) ), true, set.mips ) )
		return fail( QString( "cannot write %1_n.DDS" ).arg( stem ) );
	if ( !lodgenWriteDds( stem + maskSfx, w, h, pixels( mask ), true, set.mips ) )
		return fail( QString( "cannot write %1" ).arg( stem + maskSfx ) );
	const QByteArray lodm = lodgenAggregateLodm( worldspace, set );
	// the aggregate .lodm stamps `"nlayout": 2` itself (lodgenaggregate.cpp)
	QFile lf( stem + QStringLiteral( ".lodm" ) );
	if ( !lf.open( QIODevice::WriteOnly | QIODevice::Truncate ) || lf.write( lodm ) != lodm.size() )
		return fail( QString( "cannot write %1.lodm" ).arg( stem ) );
	lf.close();
	if ( written )
		*written << stem + colorSfx << stem + QStringLiteral( "_n.DDS" ) << stem + maskSfx
			<< stem + QStringLiteral( ".lodm" );
	for ( const QString & s : { colorSfx, QStringLiteral( "_n.DDS" ), maskSfx,
			QStringLiteral( ".lodm" ) } )
		lodgenNoteLayoutFile( stem + s );
	return true;
}

bool lodgenModelExtent( const QString & dataRoot, const QString & meshPath,
	float * halfW, float * halfH )
{
	QHash<QString, QVector<LodSrcShape>> cache;
	const QVector<LodSrcShape> & shapes = lodgenLoadModel( dataRoot, meshPath, cache );
	bool any = false;
	float cx = 0.0f, cy = 0.0f, zLo = 0.0f, zHi = 0.0f;
	double sx = 0.0, sy = 0.0;
	qint64 n = 0;
	for ( const LodSrcShape & s : shapes ) {
		for ( const Vector3 & p : s.pos ) {
			if ( !any ) { zLo = zHi = p[2]; any = true; }
			zLo = qMin( zLo, p[2] );
			zHi = qMax( zHi, p[2] );
			sx += p[0]; sy += p[1]; n++;
		}
	}
	if ( !any || !n )
		return false;
	cx = float( sx / double( n ) );
	cy = float( sy / double( n ) );
	/* The horizontal half-extent is the largest RADIUS about the vertical axis,
	 * not a box half-width: the bake photographs from every azimuth and takes
	 * the widest silhouette it sees, and that is what a radius converges to.
	 * A box would under-report a tree whose crown leans off the X and Y axes. */
	float r = 0.0f;
	for ( const LodSrcShape & s : shapes )
		for ( const Vector3 & p : s.pos )
			r = qMax( r, std::sqrt( ( p[0] - cx ) * ( p[0] - cx ) + ( p[1] - cy ) * ( p[1] - cy ) ) );
	if ( halfW ) *halfW = r;
	if ( halfH ) *halfH = 0.5f * ( zHi - zLo );
	return true;
}

bool lodgenBuildObjectChunk( NifModel * nif, const EsmWorld & world,
	int chunkX, int chunkY, const LodgenObjectOptions & opts,
	QString * manifestOut, QString * error )
{
	auto fail = [error]( const QString & message ) {
		if ( error )
			*error = message;
		return false;
	};
	if ( !nif )
		return fail( QStringLiteral( "no model" ) );
	const int dim = opts.dim;
	if ( dim != 4 && dim != 8 && dim != 16 && dim != 32 )
		return fail( QStringLiteral( "dim must be 4, 8, 16 or 32" ) );
	if ( chunkX % dim || chunkY % dim )
		return fail( QString( "chunk (%1,%2) is not aligned to dim %3" )
			.arg( chunkX ).arg( chunkY ).arg( dim ) );
	const int lodLevel = opts.lodLevel >= 0 ? opts.lodLevel
		: ( dim == 4 ? 0 : dim == 8 ? 1 : dim == 16 ? 2 : 3 );
	const int segs = ( dim == 4 ) ? 16 : 1;
	const float invDim = 1.0f / float( dim );
	const float cwX = float( chunkX ) * 4096.0f, cwY = float( chunkY ) * 4096.0f;

	/* The chunk's terrain heightfield, in the same miniature space as the
	 * placements. Built once here and shared by the buried-geometry cull and
	 * the AO bake, which used to build its own identical copy. */
	/* Extended by the AO skirt, so a ray leaving the chunk still meets ground.
	 * Spacing is a CONSTANT 128 game units per sample (scaled to miniature) --
	 * deriving it from the field width silently rescales the whole field the
	 * moment the skirt makes that width bigger than the chunk. */
	/* Matches the AO block's own condition below, which since 2026-09-12 also
	 * runs for the NATIVE pair with the identity flag off (lane DEFAULTS1). */
	const int skirt = ( opts.bakeAO && ( opts.identity || lodgenNativeActive() ) )
		? qMax( 0, opts.aoSkirtCells ) : 0;
	const int terrainCells = dim + 2 * skirt;
	const int terrainN = terrainCells * 32 + 1;
	const float terrainSpacing = 128.0f * invDim;
	const float terrainOx = -float( skirt ) * 4096.0f * invDim;
	const float terrainOy = terrainOx;
	std::vector<float> terrainHgt( size_t( terrainN ) * size_t( terrainN ),
		world.defaultLandHeight() * invDim );
	{
		EsmLand land;
		for ( int cy = 0; cy < terrainCells; cy++ )
			for ( int cx = 0; cx < terrainCells; cx++ )
				if ( world.land( chunkX - skirt + cx, chunkY - skirt + cy, land ) )
					for ( int row = 0; row < 33; row++ )
						for ( int col = 0; col < 33; col++ )
							terrainHgt[size_t( cy * 32 + row ) * size_t( terrainN )
								+ size_t( cx * 32 + col )] = land.heights[row][col] * invDim;
	}
	/* The LOWEST of the four surrounding samples, not a bilinear one.
	 * Under-reading the ground is the safe direction for a cull: it keeps
	 * geometry that a dip might expose, where over-reading would quietly eat
	 * something visible. */
	auto terrainFloorAt = [&]( float x, float y ) {
		const int i0 = int( std::floor( ( x - terrainOx ) / terrainSpacing ) );
		const int j0 = int( std::floor( ( y - terrainOy ) / terrainSpacing ) );
		/* The LOWEST sample in a WINDOW, not just the enclosing four.
		 *
		 * The reference has to be the terrain as the LOD DRAWS it, and that is a
		 * decimated mesh -- ~2100 triangles for a whole chunk against this
		 * field's 33x33 per cell -- so between its vertices it sags below the
		 * source heightfield and exposes geometry the fine grid says is buried.
		 * Reading the minimum over a window is a cheap stand-in for that sag: it
		 * assumes the drawn ground could be as low as anything nearby.
		 */
		const int R = 2;
		float lo = std::numeric_limits<float>::max();
		for ( int dj = -R; dj <= 1 + R; dj++ ) {
			for ( int di = -R; di <= 1 + R; di++ ) {
				const int i = qBound( 0, i0 + di, terrainN - 1 );
				const int j = qBound( 0, j0 + dj, terrainN - 1 );
				lo = qMin( lo, terrainHgt[size_t( j ) * size_t( terrainN ) + size_t( i )] );
			}
		}
		return lo;
	};
	/*! Does the TERRAIN alone block this ray?
	 *
	 *  The buried cull asks whether the ground hides a triangle, not how deep it
	 *  is. Depth was the first rule and it removed geometry that was plainly
	 *  visible: a boulder in a shallow dip has vertices far below the
	 *  surrounding surface and is still in full view. Only the terrain is
	 *  consulted -- objects are ignored on purpose, so an object hidden purely
	 *  behind another object is KEPT. That is the conservative direction, and
	 *  the cheap one: it needs no assembled scene, so the test can run at
	 *  placement time where the never-disappear rail lives.
	 */
	auto terrainBlocks = [&]( const Vector3 & o, const Vector3 & d, float maxT ) {
		const float step = qMax( terrainSpacing * 0.5f, 1.0f );
		for ( float t = step; t < maxT; t += step ) {
			const float x = o[0] + d[0] * t, y = o[1] + d[1] * t;
			if ( x < terrainOx || y < terrainOy
				|| x > terrainOx + float( terrainCells ) * 4096.0f * invDim
				|| y > terrainOy + float( terrainCells ) * 4096.0f * invDim )
				return false;             // left the known ground: assume open sky
			if ( o[2] + d[2] * t < terrainFloorAt( x, y ) )
				return true;
		}
		return false;
	};

	/*! Is every escape route from this point blocked by ground?
	 *
	 *  Nine directions: straight up plus a ring at 45 and 20 degrees. A single
	 *  unblocked direction keeps the triangle, so the test errs towards keeping.
	 */
	auto terrainHides = [&]( const Vector3 & p ) {
		/* Up, 45 degrees, 20 degrees AND near-horizontal.
		 *
		 * The horizontal ring is the one that matters and the one an
		 * up-facing-only test was missing: LOD is looked at from ground level
		 * across a valley, not from above. Without these, geometry that a hill
		 * hides from a bird is culled and then plainly visible to a player
		 * standing on the far side -- measured at 166,579 pixels of hole from a
		 * low camera against 11,001 from a high one.
		 */
		static const float dirs[13][3] = {
			{ 0.0f, 0.0f, 1.0f },
			{ 0.7f, 0.0f, 0.7f }, { -0.7f, 0.0f, 0.7f },
			{ 0.0f, 0.7f, 0.7f }, { 0.0f, -0.7f, 0.7f },
			{ 0.94f, 0.0f, 0.34f }, { -0.94f, 0.0f, 0.34f },
			{ 0.0f, 0.94f, 0.34f }, { 0.0f, -0.94f, 0.34f },
			{ 0.996f, 0.0f, 0.087f }, { -0.996f, 0.0f, 0.087f },
			{ 0.0f, 0.996f, 0.087f }, { 0.0f, -0.996f, 0.087f } };
		const float reach = 4096.0f * invDim;      // one cell of ground to clear
		for ( const auto & dv : dirs ) {
			const Vector3 d( dv[0], dv[1], dv[2] );
			if ( !terrainBlocks( p, d, reach ) )
				return false;
		}
		return true;
	};

	int culledTris = 0, culledPlacements = 0, rescuedPlacements = 0;
	int aoSkirtTris = 0, aoSkirtPlacements = 0;

	// gather refs: every cell's own plus the persistent overlay
	QVector<EsmRefr> refs;
	for ( int cy = 0; cy < dim; cy++ )
		for ( int cx = 0; cx < dim; cx++ )
			refs += world.refrs( chunkX + cx, chunkY + cy );
	refs += world.persistentRefrsIn( cwX, cwY,
		cwX + float( dim ) * 4096.0f, cwY + float( dim ) * 4096.0f );

	/* The AO skirt's refs: the ring of cells around the chunk. They are
	 * OCCLUDERS ONLY -- never emitted, never indexed, never in the manifest --
	 * so they are gathered separately and never mixed into `refs`. */
	QVector<EsmRefr> skirtRefs;
	for ( int cy = -skirt; cy < dim + skirt; cy++ )
		for ( int cx = -skirt; cx < dim + skirt; cx++ )
			if ( cx < 0 || cy < 0 || cx >= dim || cy >= dim )
				skirtRefs += world.refrs( chunkX + cx, chunkY + cy );

	/* SCOL expansion: a static collection has no LOD models of its own — the
	 * CK generates its LOD by unpacking the parts back into their source
	 * bases (xLODGen does the same). Each part placement composes under the
	 * placing REFR: world = T_ref * T_placement. Everything downstream sees
	 * one flat placement list, so identity/AO/manifests treat expanded
	 * copies exactly like first-class refs. */
	/* The stable key of an object is the placed REFERENCE, not the base (a
	 * base is shared by every copy of a maple) and not the index (per chunk,
	 * per ring). A SCOL part has no reference of its own: it carries the SCOL
	 * reference and its ordinal within it. bungo, 2026-09-06: "stable
	 * identity sounds good" - matched across rings and bakes by (ref, part). */
	struct LodPlacement
	{
		quint32 base;
		Vector3 pos;    // world units
		Matrix rot;
		float scale;
		quint32 ref = 0;    //!< the placed reference's form ID (a SCOL part: the SCOL reference)
		int part = -1;      //!< a SCOL part's ordinal within its reference, -1 for a plain ref
	};
	QVector<LodPlacement> placements;
	placements.reserve( refs.size() );
	for ( const EsmRefr & r : refs ) {
		if ( r.initiallyDisabled || r.deleted || !r.base )
			continue;
		/* Bethesda's stored euler angles are applied NEGATED relative to
		 * Matrix::fromEuler: world R = Rx(-x)·Ry(-y)·Rz(-z). Proven against
		 * vanilla chunks on multi-axis-rotated refs (RockCliff at
		 * 38°/33°/77°: 62% vertex match under this convention vs 14% under
		 * fromEuler(+x,+y,+z); pure-Z road pieces confirm too). */
		Matrix rm;
		rm.fromEuler( -r.rot[0], -r.rot[1], -r.rot[2] );
		const Vector3 rp( r.pos[0], r.pos[1], r.pos[2] );
		if ( std::memcmp( &r.baseType, "SCOL", 4 ) == 0 ) {
			int scolPart = 0;
			for ( const EsmScolPart & part : world.scolParts( r.base ) ) {
				for ( const EsmScolPlacement & pl : part.placements ) {
					Matrix pm;
					pm.fromEuler( -pl.rot[0], -pl.rot[1], -pl.rot[2] );
					LodPlacement out;
					out.base = part.base;
					out.pos = rp + rm * ( Vector3( pl.pos[0], pl.pos[1],
						pl.pos[2] ) * r.scale );
					out.rot = rm * pm;
					out.scale = r.scale * pl.scale;
					out.ref = r.formID;
					out.part = scolPart++;
					placements.append( out );
				}
			}
			continue;
		}
		placements.append( LodPlacement{ r.base, rp, rm, r.scale, r.formID, -1 } );
	}

	QVector<LodPlacement> skirtPlacements;
	/* PLACEMENT AO (bungo 2026-09-11 15:3x, "is vertex AO baked into impostors
	 * too on top of the texture AO they hold?"). A card carries its own self-AO
	 * and its texture's AO, but nothing about WHERE it stands. One probe point
	 * per native placement is collected here and cast below, in the same pass
	 * and against the same scene the chunk's own vertices are cast against, so
	 * the two numbers are the same quantity. */
	struct LodAoProbe { int objectIndex; Vector3 p; };
	QVector<LodAoProbe> aoProbes;
	for ( const EsmRefr & r : skirtRefs ) {
		if ( r.initiallyDisabled || r.deleted || !r.base )
			continue;
		Matrix rm;
		rm.fromEuler( -r.rot[0], -r.rot[1], -r.rot[2] );
		const Vector3 rp( r.pos[0], r.pos[1], r.pos[2] );
		if ( std::memcmp( &r.baseType, "SCOL", 4 ) == 0 ) {
			for ( const EsmScolPart & part : world.scolParts( r.base ) ) {
				for ( const EsmScolPlacement & pl : part.placements ) {
					Matrix pm;
					pm.fromEuler( -pl.rot[0], -pl.rot[1], -pl.rot[2] );
					LodPlacement out;
					out.base = part.base;
					out.pos = rp + rm * ( Vector3( pl.pos[0], pl.pos[1],
						pl.pos[2] ) * r.scale );
					out.rot = rm * pm;
					out.scale = r.scale * pl.scale;
					skirtPlacements.append( out );
				}
			}
			continue;
		}
		skirtPlacements.append( LodPlacement{ r.base, rp, rm, r.scale } );
	}

	QHash<QString, QVector<LodSrcShape>> modelCache;
	QHash<quint32, LodgenCard> cardCache;
	QMap<QString, ObjBucket> buckets;   // key = tex0|tex1
	QStringList manifest;
	// instance grouping: base form -> (model, member object indices)
	QMap<quint32, QPair<QString, QVector<int>>> instanceGroups;
	int objectIndex = 0;
	int placed = 0, skippedNoLod = 0, cardsForMeshes = 0, cardsRefusedNotTree = 0;

	for ( const LodPlacement & r : placements ) {
		const EsmLodBase & base = world.lodBase( r.base );
		if ( !base.hasLod )
			continue;
		QString model = base.models[qMin( lodLevel, 3 )];
		QVector<LodSrcShape> cardShapes;
		LodgenCard usedCard;		// a copy: the cache may move on a later insert
		/* IS THE BASE A TREE, decided before a card is looked for and from a
		 * model path that EXISTS: the ring's own slot when it has one, else
		 * the base's near model, else its first filled slot -- the same
		 * fallback the candidate lister uses, because a base whose far slot is
		 * empty is exactly the case the toggle has to judge. The test itself is
		 * the shared one (lodgenIsTreeModel + the TREE record type), so the
		 * bake, the repetition breaker, the sway gate and the candidate lister
		 * cannot drift apart. */
		QString treeProbe = model;
		if ( treeProbe.isEmpty() )
			treeProbe = base.model;
		for ( int l = 0; l < 4 && treeProbe.isEmpty(); l++ )
			treeProbe = base.models[l];
		const bool baseIsTree = std::memcmp( &base.type, "TREE", 4 ) == 0
			|| lodgenIsTreeModel( treeProbe );
		/* A card stands in where the ring's slot is missing (it beats falling
		 * back to a heavier near-slot mesh) and, from opts.impostorFromLevel
		 * on, in place of the slot's mesh too: one quad per tree at the near
		 * rings, for a consumer that draws the octahedral sheets.
		 *
		 * TREES ONLY (bungo 2026-09-11 07:0x/07:1x). `treesOnly` on: nothing
		 * but a tree may stand on a card at all. Off: the "missing" rule alone
		 * -- an empty ring slot, any base. The ring override is tree-only in
		 * both states; a non-tree with an authored mesh is never replaced by a
		 * quad. */
		const bool cardEligible = !opts.treesOnly || baseIsTree;
		const bool cardWanted = !opts.impostorDir.isEmpty() && cardEligible
			&& ( model.isEmpty()
				|| ( baseIsTree && opts.impostorFromLevel >= 0 && lodLevel >= opts.impostorFromLevel ) );
		if ( !opts.impostorDir.isEmpty() && !cardEligible && model.isEmpty() )
			cardsRefusedNotTree++;
		if ( cardWanted ) {
			const LodgenCard & card = lodgenCard( opts.impostorDir, r.base, cardCache, opts.cardAuxDiv );
			if ( card.valid ) {
				cardShapes.append( lodgenCardShape( card ) );
				usedCard = card;
				if ( !model.isEmpty() )
					cardsForMeshes++;
			}
		}
		if ( cardShapes.isEmpty() ) {
			/* Vanilla parity: an empty MNAM slot means the object DROPS OUT
			 * at that ring — measured Commonwealth.16.-16.16.BTO holds 7k
			 * verts where slot-substitution produced 244k. Substituting a
			 * nearer (heavier) model is opt-in, and impostor cards above
			 * are the sanctioned stand-in. */
			if ( model.isEmpty() && opts.slotFallback ) {
				for ( int l = lodLevel; l >= 0 && model.isEmpty(); l-- )
					model = base.models[l];
				for ( int l = lodLevel; l < 4 && model.isEmpty(); l++ )
					model = base.models[l];
			}
			if ( model.isEmpty() ) {
				skippedNoLod++;
				continue;
			}
		}
		const QVector<LodSrcShape> & shapes = !cardShapes.isEmpty() ? cardShapes
			: lodgenLoadModel( opts.dataRoot, model, modelCache );
		if ( shapes.isEmpty() ) {
			skippedNoLod++;
			continue;
		}

		Transform xf;
		xf.translation = Vector3( ( r.pos[0] - cwX ) * invDim,
			( r.pos[1] - cwY ) * invDim, r.pos[2] * invDim );
		xf.rotation = r.rot;
		xf.scale = r.scale * invDim;

		/* Repetition breaking for trees (charter: "mirror half the cards,
		 * rotate card sets per tree"): a position-stable hash spins each
		 * tree's card set and mirrors half of them in U, so distant forests
		 * stop reading as copies. Rotation about the tree's own Z is safe —
		 * crossed-card LOD models are radially symmetric by construction. */
		/* NOT a bare substring test: "sTREEt" — the first cut randomly spun
		 * every street and highway piece downtown. Tree records, the LOD
		 * trees folder, and tree-prefixed model names only. The three path
		 * tests live in lodgenIsTreeModel and are called, not re-typed: lane
		 * LODUI1 needed the same question answered before the card block
		 * above, and a second copy here is how two answers begin. */
		const bool isTree = std::memcmp( &base.type, "TREE", 4 ) == 0
			|| lodgenIsTreeModel( model );
		quint32 treeHash = 0;
		if ( isTree ) {
			treeHash = ( quint32( qRound( r.pos[0] ) ) * 2654435761U )
				^ ( quint32( qRound( r.pos[1] ) ) * 40503U );
			Matrix rz;
			rz.fromEuler( 0.0f, 0.0f,
				float( treeHash % 360U ) * 0.01745329f );
			xf.rotation = xf.rotation * rz;
		}
		const bool mirrorU = isTree && ( ( treeHash >> 8 ) & 1 );

		// cell attribution for the dim4 segment split
		int cellIdx = 0;
		if ( segs > 1 ) {
			const int lx = qBound( 0, int( ( r.pos[0] - cwX ) / 4096.0f ), dim - 1 );
			const int ly = qBound( 0, int( ( r.pos[1] - cwY ) / 4096.0f ), dim - 1 );
			cellIdx = ly * dim + lx;
		}
		/* Local bounds across ALL of a placement's shapes. Two consumers:
		 *
		 *  - the sway weight, measured in the placement's OWN space so a
		 *    leaning or rotated tree still reads 0 at its own base rather than
		 *    at whatever happens to be its lowest world point, and normalised
		 *    across every shape rather than per shape -- a tree is a trunk and
		 *    a branch card split by texture, and normalising each alone would
		 *    make the branch card's base read 0 and step at the join;
		 *  - the manifest's BOUND RADIUS, which is the charter's screen-size
		 *    fade input and therefore wanted for every placement, not just
		 *    trees. */
		float localZMin = 3.4e38f, localZMax = -3.4e38f;
		float localRadius = 0.0f, localMaxDist = 0.0f;
		/* The DRAWN top of this placement, in chunk-miniature units. Taken
		 * through the placement's own transform rather than from `localZMax`
		 * times the scale, because an ESM rotation is not always about Z. Only
		 * computed when the native emitter is armed and AO is being cast --
		 * it is one transform per source vertex. */
		const bool wantAoProbe = lodgenNativeActive() && opts.bakeAO;
		float probeZ = -3.4e38f;
		for ( const LodSrcShape & s : shapes ) {
			for ( const Vector3 & lp : s.pos ) {
				if ( wantAoProbe )
					probeZ = qMax( probeZ, ( xf * lp )[2] );
				localZMin = qMin( localZMin, lp[2] );
				localZMax = qMax( localZMax, lp[2] );
				localRadius = qMax( localRadius,
					std::sqrt( lp[0] * lp[0] + lp[1] * lp[1] ) );
				localMaxDist = qMax( localMaxDist, lp.length() );
			}
		}
		const float localSpan = localZMax - localZMin;

		/* Object class. The charter's rule is that the class decides what the
		 * A channel MEANS, so it has to travel with the index -- today A is
		 * only interpretable because trees are the only thing that writes it.
		 *
		 * Derived from the record type and the LOD model path, which is a
		 * heuristic and is labelled as one: FO4 has no class field. `isTree`
		 * is the same test the sway gate and the repetition breaker already
		 * use, so there is one definition of "tree" in this file. */
		const char * objClass = "misc";
		if ( isTree ) {
			objClass = "tree";
		} else {
			const QString ml = model.toLower();
			if ( ml.contains( QLatin1String( "rock" ) )
				|| ml.contains( QLatin1String( "cliff" ) )
				|| ml.contains( QLatin1String( "boulder" ) ) )
				objClass = "rock";
			else if ( ml.contains( QLatin1String( "architecture" ) )
				|| ml.contains( QLatin1String( "building" ) )
				|| ml.contains( QLatin1String( "\shack" ) )
				|| ml.contains( QLatin1String( "/shack" ) )
				|| ml.contains( QLatin1String( "house" ) ) )
				objClass = "building";
		}

		/* Identity channel: 16-bit per-chunk index in R+G. COMPUTED WHATEVER
		 * THE FLAG SAYS since 2026-09-12 (lane DEFAULTS1) because the native
		 * .lodo/.lodi lighting rows are keyed off it further down; with the
		 * flag off it never reaches the .BTO, the vertex write below being
		 * gated on the flag, so the legacy file is vanilla's layout exactly. */
		const Color4 idColor( float( objectIndex & 0xFF ) / 255.0f,
			float( ( objectIndex >> 8 ) & 0xFF ) / 255.0f, 1.0f, 1.0f );
		/* THE MANIFEST IS A SIDECAR (lane DEFAULTS1, 2026-09-12). It used to be
		 * written only with the identity flag, which meant that turning the
		 * flag off -- as bungo's 15:56 ruling does by default -- silently took
		 * the impostor card arrays and the far-ring cut with it (lane
		 * SHOWCASE1 found that). The rows are written whatever the flag says;
		 * only what goes INSIDE the .BTO follows the flag. The index column is
		 * still the object's per-chunk ordinal, which is exactly what the R+G
		 * vertex colour WOULD carry, so a manifest written with identity off
		 * has the same meaning it always had. */
		{
			manifest.append( QString( "%1 %2 %3 %4 %5 %6 %7 %8 %9" )
				.arg( objectIndex )
				.arg( r.base, 8, 16, QChar( '0' ) )
				.arg( QString::fromLatin1(
					reinterpret_cast<const char *>( &base.type ), 4 ) )
				.arg( double( r.pos[0] ) ).arg( double( r.pos[1] ) )
				.arg( double( r.pos[2] ) ).arg( double( r.scale ) )
				.arg( QLatin1String( objClass ) )
				.arg( double( localMaxDist * r.scale ) )
				+ QString( " %1 %2" ).arg( r.ref, 8, 16, QChar( '0' ) ).arg( r.part ) );
			/* A placement standing on an octahedral card says so: the card's
			 * centre and half extents in model space, the grid, the depth span
			 * and the set's .lodm (family, sheets, grid). The crossed quads
			 * above it are the stock engine's; a consumer reading this line
			 * draws the card instead. */
			if ( usedCard.valid && usedCard.oct >= 2 )
				manifest.append( QString( "C %1 %2 %3 %4 %5 %6 %7 %8 %9" ).arg( objectIndex )
					.arg( double( usedCard.octCenter[0] ) ).arg( double( usedCard.octCenter[1] ) )
					.arg( double( usedCard.octCenter[2] ) ).arg( double( usedCard.octHalfW ) )
					.arg( double( usedCard.octHalfH ) ).arg( usedCard.oct )
					.arg( double( usedCard.octSpan ) ).arg( usedCard.octPath ) );
		}
		if ( lodgenNativeActive() ) {
			NativePlacement np;
			np.baseForm = r.base;
			np.refForm = r.ref;
			np.scolPart = r.part;
			for ( int k = 0; k < 3; k++ )
				np.pos[k] = r.pos[k];
			for ( int a = 0; a < 3; a++ )
				for ( int b = 0; b < 3; b++ )
					np.rot[a * 3 + b] = xf.rotation( a, b );   // the DRAWN rotation: ESM x tree yaw
			np.scale = r.scale;
			np.slot = qMin( lodLevel, 3 );
			np.model = model;
			np.isTree = isTree;
			np.mirrorU = mirrorU;
			np.treeHash = treeHash;
			for ( const LodSrcShape & s : shapes ) {
				np.hasAlpha = np.hasAlpha || s.hasAlpha;
				np.emits = np.emits || ( s.ownEmit && ( s.emitColor.red() > 0.0f || s.emitColor.green() > 0.0f || s.emitColor.blue() > 0.0f ) );
			}
			np.objectIndex = objectIndex;
			np.chunkX = chunkX; np.chunkY = chunkY; np.dim = dim;
			lodgenNativeAddPlacement( np );
			/* The probe stands 16 world units (in miniature) ABOVE the drawn
			 * top, facing up. Above the object's own geometry so its own
			 * triangles do not darken it -- the card already carries that --
			 * and facing +Z so what the ray finds is a bridge, a wall or a
			 * cliff above this spot, which is the thing the card cannot know. */
			if ( wantAoProbe && probeZ > -3.0e38f )
				aoProbes.append( LodAoProbe{ objectIndex,
					Vector3( xf.translation[0], xf.translation[1], probeZ + 16.0f * invDim ) } );
		}
		const bool swaying = opts.treeSway && isTree && localSpan > 1.0e-4f;

		for ( const LodSrcShape & s : shapes ) {
			/* One bucket per distinct material: textures, the specular slot,
			 * the constants and the source material's name, so two sources
			 * sharing a diffuse but not a material never merge into one shape
			 * (they used to: the key was the diffuse and normal alone). */
			ObjBucket & bucket = buckets[s.tex0.toLower() + QChar( '|' ) + s.tex1.toLower() + QChar( '|' )
				+ s.tex7.toLower() + QChar( '|' ) + s.matName.toLower()
				+ QString( "|%1|%2" ).arg( double( s.smoothness ) ).arg( double( s.specMult ) )
				// two sources that emit differently are two shapes: the key says so
				+ QString( "|%1|%2|%3|%4|%5" ).arg( double( s.emitColor.red() ) )
					.arg( double( s.emitColor.green() ) ).arg( double( s.emitColor.blue() ) )
					.arg( double( s.emitMult ) ).arg( s.ownEmit ? 1 : 0 )
				+ ( s.hasAlpha ? QStringLiteral( "|at" ) : QString() )];
			if ( bucket.cellTris.isEmpty() ) {
				bucket.cellTris.resize( segs );
				bucket.tex0 = s.tex0;
				bucket.tex1 = s.tex1;
				bucket.tex7 = s.tex7;
				bucket.matName = s.matName;
				bucket.smoothness = s.smoothness;
				bucket.specMult = s.specMult;
				bucket.emitColor = s.emitColor;
				bucket.emitMult = s.emitMult;
				bucket.ownEmit = s.ownEmit;
				bucket.hasAlpha = s.hasAlpha;
				bucket.alphaFlags = s.alphaFlags;
				bucket.alphaThreshold = s.alphaThreshold;
			}
			/* Buried-geometry cull. Done HERE, per placement, because both
			 * safety rails are per placement: the all-three-vertices rule
			 * needs the source triangles, and the never-disappear rule needs
			 * to know this object's own total. Once shapes are merged into a
			 * bucket neither is recoverable. */
			QVector<Triangle> keptTris;
			QVector<int> remap;
			bool culled = false;
			if ( opts.cullBuried && !s.tris.isEmpty() ) {
				const float margin = opts.cullMargin * invDim;
				QVector<Vector3> world( s.pos.size() );
				QVector<quint8> buried( s.pos.size(), 0 );
				for ( int v = 0; v < s.pos.size(); v++ ) {
					world[v] = xf * s.pos[v];
					buried[v] = ( world[v][2] < terrainFloorAt( world[v][0], world[v][1] ) - margin )
						? 1 : 0;
				}
				/* Two gates, and the second is the one that matters. Being under
				 * the surface only makes a triangle a CANDIDATE; it goes only if
				 * the ground actually hides it from every direction. Depth alone
				 * removed geometry in shallow dips that was in full view. */
				for ( const Triangle & t : s.tris ) {
					const bool under = buried[t.v1()] && buried[t.v2()] && buried[t.v3()];
					if ( under ) {
						const Vector3 mid = ( world[t.v1()] + world[t.v2()]
							+ world[t.v3()] ) / 3.0f;
						if ( terrainHides( mid ) )
							continue;             // invisible: drop it
					}
					keptTris.append( t );
				}
				if ( keptTris.isEmpty() ) {
					// Everything is under the ground. Keep the object whole:
					// a vanished tree card is worse than a buried one.
					rescuedPlacements++;
				} else if ( keptTris.size() < s.tris.size() ) {
					culled = true;
					culledTris += s.tris.size() - keptTris.size();
					culledPlacements++;
				}
			}

			// Which source vertices survive, and where they land in the bucket.
			QVector<int> keep;
			const QVector<Triangle> & useTris = culled ? keptTris : s.tris;
			if ( culled ) {
				remap.fill( -1, s.pos.size() );
				keep.reserve( s.pos.size() );
				for ( const Triangle & t : useTris ) {
					const int idx[3] = { t.v1(), t.v2(), t.v3() };
					for ( int k = 0; k < 3; k++ ) {
						if ( remap[idx[k]] < 0 ) {
							remap[idx[k]] = keep.size();
							keep.append( idx[k] );
						}
					}
				}
			} else {
				keep.reserve( s.pos.size() );
				for ( int v = 0; v < s.pos.size(); v++ )
					keep.append( v );
			}

			const quint32 vBase = quint32( bucket.pos.size() );
			if ( vBase + quint32( keep.size() ) > 65535 )
				continue;   // bucket full; a second shape would need splitting
			/* Mirror about the shape's own U midpoint, not 1-u: tree LOD
			 * textures are often atlas cells, and a global flip would sample
			 * the neighbouring tree's cell. */
			float uMid = 0.0f;
			if ( mirrorU && !s.uv.isEmpty() ) {
				float uMin = s.uv[0][0], uMax = s.uv[0][0];
				for ( const Vector2 & t : s.uv ) {
					uMin = qMin( uMin, t[0] );
					uMax = qMax( uMax, t[0] );
				}
				uMid = uMin + uMax;
			}
			for ( int kv = 0; kv < keep.size(); kv++ ) {
				const int v = keep[kv];
				bucket.pos.append( xf * s.pos[v] );
				Vector3 wn = xf.rotation * s.nrm[v];
				wn.normalize();
				bucket.nrm.append( wn );
				Vector3 wt = xf.rotation * s.tan[v];
				wt.normalize();
				bucket.tan.append( wt );
				bucket.uv.append( mirrorU
					? Vector2( uMid - s.uv[v][0], s.uv[v][1] ) : s.uv[v] );
				Color4 c = idColor;
				if ( opts.identity ) {
					if ( swaying ) {
						/* Height dominates because a trunk is a cantilever --
						 * deflection grows faster than linearly with height --
						 * and the radial term separates a branch tip from the
						 * trunk at the same height. */
						const Vector3 & lp = s.pos[v];
						const float hF = qBound( 0.0f,
							( lp[2] - localZMin ) / localSpan, 1.0f );
						const float rF = localRadius > 1.0e-4f
							? qBound( 0.0f, std::sqrt( lp[0] * lp[0] + lp[1] * lp[1] )
								/ localRadius, 1.0f )
							: 0.0f;
						c.setAlpha( qBound( 0.0f,
							hF * hF * ( 0.35f + 0.65f * rF ), 1.0f ) );
					} else if ( opts.treeSway ) {
						/* Anything that does not sway carries an explicit ZERO,
						 * so a consumer applying the channel blindly to a shack
						 * does nothing rather than something wrong. */
						c.setAlpha( 0.0f );
					} else {
						c.setAlpha( s.col[v].alpha() );
					}
				}
				bucket.col.append( c );
			}
			for ( const Triangle & t : useTris ) {
				const quint32 a = culled ? quint32( remap[t.v1()] ) : quint32( t.v1() );
				const quint32 b = culled ? quint32( remap[t.v2()] ) : quint32( t.v2() );
				const quint32 cIdx = culled ? quint32( remap[t.v3()] ) : quint32( t.v3() );
				bucket.cellTris[cellIdx].append( Triangle(
					quint16( vBase + a ), quint16( vBase + b ), quint16( vBase + cIdx ) ) );
			}
		}
		/* Manifest data, so not on the identity flag (lane DEFAULTS1). */
		{
			auto & group = instanceGroups[r.base];
			group.first = model;
			group.second.append( objectIndex );
		}
		objectIndex++;
		placed++;
	}
	/* Instance groups (FO76's BSDistantObjectInstancedNode, ours as manifest
	 * data): bases repeated >= 8 times in the chunk. The stitched copies stay
	 * in the mesh for vanilla; a CS consumer can kill those fragments by the
	 * listed identity indices and draw the model instanced instead.
	 * Manifest data, so not on the identity flag (lane DEFAULTS1). */
	{
		for ( auto it = instanceGroups.constBegin(); it != instanceGroups.constEnd(); ++it ) {
			if ( it.value().second.size() < 8 )
				continue;
			QStringList ids;
			for ( int id : it.value().second )
				ids.append( QString::number( id ) );
			manifest.append( QString( "I %1 %2 %3 %4" )
				.arg( it.key(), 8, 16, QChar( '0' ) )
				.arg( it.value().first )
				.arg( it.value().second.size() )
				.arg( ids.join( QChar( ',' ) ) ) );
		}
	}
	if ( buckets.isEmpty() )
		return fail( QString( "no LOD-bearing refs in chunk (%1,%2)x%3 (%4 placed, %5 without a usable LOD model)" )
			.arg( chunkX ).arg( chunkY ).arg( dim ).arg( placed ).arg( skippedNoLod ) );

	/* Rung 3 bake: per-placement AO into the identity B channel, ray-cast
	 * against the whole assembled chunk plus the terrain heightfield.
	 *
	 * ALSO RUN WITH IDENTITY OFF WHEN THE NATIVE PAIR IS BEING WRITTEN (lane
	 * DEFAULTS1, 2026-09-12): the .lodo/.lodi lighting rows are fed from this
	 * loop, and the FO4CS data lives only in the .lod* files now, so it must
	 * not thin out when the legacy vertex channels go away. With identity off
	 * the colour this loop writes into `bucket.col` never reaches the .BTO --
	 * the vertex write below is still gated on the flag. */
	if ( opts.bakeAO && ( opts.identity || lodgenNativeActive() ) ) {
		LodgenAoScene scene;
		scene.hn = terrainN;
		scene.hSpacing = terrainSpacing;
		scene.hgt.assign( terrainHgt.begin(), terrainHgt.end() );
		// The binned area follows the heightfield, so a ray leaving the chunk
		// still finds both ground and neighbours to be stopped by.
		scene.ox = terrainOx;
		scene.oy = terrainOy;
		scene.span = float( terrainCells ) * 4096.0f * invDim;

		/* Skirt occluders. No repetition-breaking rotation is applied: it spins
		 * a tree's cards about its own axis, which changes what the neighbour
		 * chunk DRAWS but not the volume it occupies, and occlusion only cares
		 * about the volume. */
		int skirtTris = 0;
		for ( const LodPlacement & r : skirtPlacements ) {
			const EsmLodBase & base = world.lodBase( r.base );
			if ( !base.hasLod )
				continue;
			QString model = base.models[qMin( lodLevel, 3 )];
			if ( model.isEmpty() && opts.slotFallback ) {
				for ( int l = lodLevel; l >= 0 && model.isEmpty(); l-- )
					model = base.models[l];
				for ( int l = lodLevel; l < 4 && model.isEmpty(); l++ )
					model = base.models[l];
			}
			if ( model.isEmpty() )
				continue;
			const QVector<LodSrcShape> & shapes =
				lodgenLoadModel( opts.dataRoot, model, modelCache );
			if ( shapes.isEmpty() )
				continue;
			Transform xf;
			xf.translation = Vector3( ( r.pos[0] - cwX ) * invDim,
				( r.pos[1] - cwY ) * invDim, r.pos[2] * invDim );
			xf.rotation = r.rot;
			xf.scale = r.scale * invDim;
			for ( const LodSrcShape & s : shapes ) {
				for ( const Triangle & t : s.tris ) {
					scene.addTriangle( xf * s.pos[t.v1()], xf * s.pos[t.v2()],
						xf * s.pos[t.v3()] );
					skirtTris++;
				}
			}
		}
		aoSkirtTris = skirtTris;
		aoSkirtPlacements = skirtPlacements.size();
		for ( auto it = buckets.constBegin(); it != buckets.constEnd(); ++it )
			for ( const QVector<Triangle> & ct : it.value().cellTris )
				for ( const Triangle & t : ct )
					scene.addTriangle( it.value().pos[t.v1()],
						it.value().pos[t.v2()], it.value().pos[t.v3()] );
		for ( auto it = buckets.begin(); it != buckets.end(); ++it ) {
			ObjBucket & bucket = it.value();
			bucket.sky.resize( bucket.pos.size() );
			bucket.groundBlend.resize( bucket.pos.size() );
			for ( int v = 0; v < bucket.pos.size(); v++ ) {
				if ( opts.objectChannels ) {
					bucket.sky[v] = scene.skyVisibility( bucket.pos[v], 300.0f );
					/* Ground contact: 1 at or below the surface, falling to 0
					 * over CONTACT_RANGE world units above it. The BILINEAR
					 * ground, not the conservative windowed minimum the cull
					 * uses -- the cull wants to under-read the ground so it
					 * never eats something visible, this wants the surface
					 * where it actually is so a wall's base blends at its
					 * base. */
					constexpr float CONTACT_RANGE = 256.0f;
					const Vector3 & p = bucket.pos[v];
					const float g = scene.groundHeight( p[0], p[1] );
					bucket.groundBlend[v] = qBound( 0.0f,
						1.0f - ( p[2] - g ) / ( CONTACT_RANGE * invDim ), 1.0f );
				}
				const float ao = scene.ambientOcclusion( bucket.pos[v],
					bucket.nrm[v], 300.0f );
				if ( lodgenNativeActive() )
					lodgenNativeLighting( chunkX, chunkY, dim,
						qRound( bucket.col[v].red() * 255.0f ) + qRound( bucket.col[v].green() * 255.0f ) * 256,
						ao, opts.objectChannels ? bucket.sky[v] : 1.0f,
						opts.objectChannels ? bucket.groundBlend[v] : 0.0f );
				if ( opts.aoGrey )
					bucket.col[v].setRGBA( ao, ao, ao, bucket.col[v].alpha() );
				else
					bucket.col[v].setRGBA( bucket.col[v].red(), bucket.col[v].green(),
						ao, bucket.col[v].alpha() );
			}
		}
		/* One ray a placement, cast LAST so `scene` holds every bucket and
		 * every skirt occluder -- the same scene, the same `ambientOcclusion`
		 * and the same 300-unit reach the chunk's own vertices just used. */
		if ( lodgenNativeActive() ) {
			const Vector3 up( 0.0f, 0.0f, 1.0f );
			for ( const LodAoProbe & q : aoProbes )
				lodgenNativePlacementAo( chunkX, chunkY, dim, q.objectIndex,
					scene.ambientOcclusion( q.p, up, 300.0f ) );
		}
	}

	// ---- emit ---------------------------------------------------------
	if ( !nif->createNew( 0x14020007, 12, 130 ) )
		return fail( QStringLiteral( "could not create a Fallout 4 document" ) );
	nif->holdUpdates( true );
	QModelIndex iRoot = nif->insertNiBlock( QStringLiteral( "NiNode" ) );
	nif->set<QString>( iRoot, "Name", QStringLiteral( "obj" ) );
	nif->set<quint32>( iRoot, "Flags", 14 );
	nif->set<float>( iRoot, "Scale", 1.0f );

	/* Built rather than hardcoded once the extra channels are in play: the
	 * stride is whatever the flags add up to, and a constant that disagrees
	 * with the flags writes a file whose Data Size and vertex rows do not
	 * match -- which reads as corruption, not as a wrong number. */
	BSVertexDesc objDesc( opts.identity ? OBJ_VERTEX_DESC_COLORS : OBJ_VERTEX_DESC );
	const bool extraChannels = opts.identity && opts.objectChannels;
	if ( extraChannels ) {
		objDesc.SetFlag( VertexFlags::VF_UV_2 );
		objDesc.SetFlag( VertexFlags::VF_EYEDATA );
		objDesc.ResetAttributeOffsets( 130 );
	}
	const std::uint64_t desc = objDesc.Value();
	const int stride = extraChannels ? int( objDesc.GetVertexSize() )
		: ( opts.identity ? 24 : 20 );

	for ( auto it = buckets.begin(); it != buckets.end(); ++it ) {
		ObjBucket & bucket = it.value();
		QModelIndex iBoundNode = insertAvObject( nif,
			QStringLiteral( "BSMultiBoundNode" ), QString(), 1.0f );
		nif->set<quint32>( iBoundNode, "Culling Mode", 1 );
		QModelIndex iShape = insertAvObject( nif,
			QStringLiteral( "BSSubIndexTriShape" ),
			bucket.hasAlpha ? QStringLiteral( "obj-at" ) : QStringLiteral( "obj" ),
			float( dim ) );
		/* Vanilla BTO shapes carry the chunk's WORLD position in their own
		 * translation (verts stay chunk-relative miniatures) — the engine
		 * does NOT filename-place object chunks the way it places terrain.
		 * Measured: Commonwealth.4.-20.24.BTO shape T = (-81920, 98304, 0)
		 * = chunk * 4096; its .BTR Land sits at T = 0. bungo spotted the
		 * origin mismatch in the viewer. */
		nif->set<Vector3>( iShape, "Translation",
			Vector3( cwX, cwY, 0.0f ) );

		QVector<Triangle> tris;
		QVector<QPair<int, int>> segRuns;
		for ( int sIdx = 0; sIdx < bucket.cellTris.size(); sIdx++ ) {
			segRuns.append( qMakePair( tris.size(), bucket.cellTris[sIdx].size() ) );
			tris += bucket.cellTris[sIdx];
		}
		const quint32 numVerts = quint32( bucket.pos.size() );
		const quint32 numTris = quint32( tris.size() );
		nif->set<BSVertexDesc>( iShape, "Vertex Desc", desc );
		nif->set<quint32>( iShape, "Num Vertices", numVerts );
		nif->set<quint32>( iShape, "Num Triangles", numTris );
		nif->set<quint32>( iShape, "Data Size",
			numVerts * quint32( stride ) + numTris * 6 );

		nif->setState( BaseModel::Processing );
		QModelIndex iVD = nif->getIndex( iShape, "Vertex Data" );
		nif->updateArraySize( iVD );
		float mnx = 3.4e38f, mny = 3.4e38f, mnz = 3.4e38f;
		float mxx = -3.4e38f, mxy = -3.4e38f, mxz = -3.4e38f;
		for ( quint32 v = 0; v < numVerts; v++ ) {
			QModelIndex row = nif->index( int( v ), 0, iVD );
			const Vector3 & p = bucket.pos[int( v )];
			mnx = qMin( mnx, p[0] ); mny = qMin( mny, p[1] ); mnz = qMin( mnz, p[2] );
			mxx = qMax( mxx, p[0] ); mxy = qMax( mxy, p[1] ); mxz = qMax( mxz, p[2] );
			nif->set<HalfVector3>( row, "Vertex", HalfVector3( p ) );
			nif->set<HalfVector2>( row, "UV", HalfVector2( bucket.uv[int( v )] ) );
			nif->set<ByteVector3>( row, "Normal", ByteVector3( bucket.nrm[int( v )] ) );
			nif->set<ByteVector3>( row, "Tangent", ByteVector3( bucket.tan[int( v )] ) );
			Vector3 bt = Vector3::crossproduct( bucket.nrm[int( v )], bucket.tan[int( v )] );
			nif->set<float>( row, "Bitangent X", bt[0] );
			nif->set<float>( row, "Bitangent Y", bt[1] );
			nif->set<float>( row, "Bitangent Z", bt[2] );
			if ( opts.identity ) {
				const Color4 & c = bucket.col[int( v )];
				nif->set<ByteColor4>( row, "Vertex Colors", ByteColor4(
					FloatVector4( c.red(), c.green(), c.blue(), c.alpha() ) ) );
			}
			if ( extraChannels && int( v ) < bucket.sky.size() ) {
				nif->set<HalfVector2>( row, "UV 2",
					HalfVector2( Vector2( bucket.sky[int( v )], 0.0f ) ) );
				nif->set<float>( row, "Eye Data", bucket.groundBlend[int( v )] );
			}
		}
		QModelIndex iTris = nif->getIndex( iShape, "Triangles" );
		nif->updateArraySize( iTris );
		nif->setArray<Triangle>( iTris, tris );

		nif->set<quint32>( iShape, "Num Primitives", numTris );
		nif->set<quint32>( iShape, "Num Segments", quint32( segRuns.size() ) );
		nif->set<quint32>( iShape, "Total Segments", quint32( segRuns.size() ) );
		QModelIndex iSegs = nif->getIndex( iShape, "Segment" );
		if ( iSegs.isValid() ) {
			nif->updateArraySize( iSegs );
			for ( int sIdx = 0; sIdx < segRuns.size(); sIdx++ ) {
				QModelIndex seg = nif->index( sIdx, 0, iSegs );
				nif->set<quint32>( seg, "Start Index", quint32( segRuns[sIdx].first * 3 ) );
				nif->set<quint32>( seg, "Num Primitives", quint32( segRuns[sIdx].second ) );
				nif->set<quint32>( seg, "Parent Array Index", 0xFFFFFFFFU );
			}
		}
		setBound( nif, iShape, mnx, mny, mnz, mxx, mxy, mxz );
		nif->restoreState();

		QModelIndex iShader = nif->insertNiBlock( QStringLiteral( "BSLightingShaderProperty" ) );
		nif->set<quint32>( iShader, "Shader Type", 0 );
		/* Vanilla's chunk shapes carry Own-Emit set (bit 22) with a BLACK
		 * emissive colour, which emits nothing; ours carries the SOURCE'S bit,
		 * colour and multiple, so a source that really does own-emit reaches the
		 * arrays pass and the engine alike. With a black colour this is vanilla's
		 * own state exactly. */
		nif->set<quint32>( iShader, "Shader Flags 1",
			bucket.ownEmit ? 2151677953U : ( 2151677953U & ~LOD_OWN_EMIT ) );
		nif->set<quint32>( iShader, "Shader Flags 2",
			opts.identity ? ( 5U | 0x20U ) : 5U );   // vertex-colours bit with identity
		QModelIndex iTexSet = nif->insertNiBlock( QStringLiteral( "BSShaderTextureSet" ) );
		nif->setLink( iShader, "Texture Set", nif->getBlockNumber( iTexSet ) );
		nif->set<uint>( iTexSet, "Num Textures", 10 );
		nif->updateArraySize( iTexSet, "Textures" );
		QModelIndex iArr = nif->getIndex( iTexSet, "Textures" );
		nif->set<QString>( nif->getIndex( iArr, 0 ), bucket.tex0 );
		nif->set<QString>( nif->getIndex( iArr, 1 ), bucket.tex1 );
		/* The vanilla _s in slot 7 and the material's constants, as vanilla
		 * chunks carry theirs (Commonwealth.Objects_s.DDS, smoothness 1,
		 * strength 1 measured); the arrays pass reads them back from here. */
		if ( !bucket.tex7.isEmpty() )
			nif->set<QString>( nif->getIndex( iArr, 7 ), bucket.tex7 );
		nif->set<float>( iShader, "Smoothness", bucket.smoothness );
		nif->set<float>( iShader, "Specular Strength", bucket.specMult );
		nif->set<Color3>( iShader, "Emissive Color", bucket.emitColor );
		nif->set<float>( iShader, "Emissive Multiple", bucket.emitMult );
		nif->setLink( iShape, "Shader Property", nif->getBlockNumber( iShader ) );
		/* The source material, for the arrays pass's .lodm lookup: the chunk
		 * shape itself names none, as vanilla's do not. */
		if ( !bucket.matName.isEmpty() )
			manifest.append( QString( "M %1 %2" ).arg( nif->getBlockNumber( iShape ) ).arg( bucket.matName ) );
		if ( bucket.hasAlpha ) {
			QModelIndex iAlpha = nif->insertNiBlock( QStringLiteral( "NiAlphaProperty" ) );
			nif->set<int>( iAlpha, "Flags", bucket.alphaFlags );
			nif->set<int>( iAlpha, "Threshold", bucket.alphaThreshold );
			nif->setLink( iShape, "Alpha Property", nif->getBlockNumber( iAlpha ) );
		}

		QModelIndex mb = insertMultiBound( nif,
			( mnx + mxx ) * 0.5f * float( dim ), ( mny + mxy ) * 0.5f * float( dim ),
			( mnz + mxz ) * 0.5f * float( dim ),
			( mxx - mnx ) * 0.5f * float( dim ), ( mxy - mny ) * 0.5f * float( dim ),
			( mxz - mnz ) * 0.5f * float( dim ) );
		nif->setLink( iBoundNode, "Multi Bound", nif->getBlockNumber( mb ) );
		addLink( nif, iBoundNode, QStringLiteral( "Children" ),
			nif->getBlockNumber( iShape ) );
		addLink( nif, iRoot, QStringLiteral( "Children" ),
			nif->getBlockNumber( iBoundNode ) );
	}

	nif->holdUpdates( false );
	nif->updateModel();
	if ( manifestOut ) {
		/* First line: which chunk of which ring this is and what the columns
		 * mean, so a consumer pairing rings never guesses. Rows keep their
		 * first nine columns as they were; ref and part are appended. */
		manifest.prepend( QString( "# lodgen manifest 2 ws %1 dim %2 chunk %3 %4 "
			"columns index base type x y z scale class height ref part" )
			.arg( world.worldspaceEdid() ).arg( dim ).arg( chunkX ).arg( chunkY ) );
		*manifestOut = manifest.join( QChar( '\n' ) ) + QChar( '\n' );	// terminated: appenders add whole lines
	}
	if ( error ) {
		*error = QString( "placed %1 objects, %2 without usable LOD, %3 material buckets" )
			.arg( placed ).arg( skippedNoLod ).arg( buckets.size() );
		// The cull says what it took AND what it refused to take: a rescued
		// placement is one that would have vanished, and that number going up
		// is the rail doing its job, not a fault.
		if ( opts.cullBuried )
			*error += QString( "; buried cull: %1 triangles from %2 placements, "
				"%3 kept whole (all-buried)" )
				.arg( culledTris ).arg( culledPlacements ).arg( rescuedPlacements );
		if ( aoSkirtPlacements )
			*error += QString( "; AO skirt: %1 neighbouring placements, %2 occluder triangles" )
				.arg( aoSkirtPlacements ).arg( aoSkirtTris );
		if ( cardsForMeshes )
			*error += QString( "; %1 placements on cards in place of their ring's mesh" ).arg( cardsForMeshes );
		/* The trees-only refusal, in words and with a number (CONSTITUTION 10:
		 * a refusal states its reason). Only printed while the toggle is ON and
		 * a card library is in play, so a run with no library says nothing. */
		if ( cardsRefusedNotTree )
			*error += QString( "; %1 placements refused a card: not a tree (Trees only)" )
				.arg( cardsRefusedNotTree );
	}
	return true;
}

/* ============ rung 3: terrain texture baking (splat -> DDS) ============ */

#include "ddstxt16.hpp"

#include <QFile>

namespace
{

//! BC1 (DXT1) DDS writer with a full box-filtered mip chain — the format
//! vanilla's own terrain bakes use (theirs are BC3; BC1 suffices with no
//! alpha and quarters the size).
namespace
{

quint16 lodgenPack565( quint32 bgra )
{
	const quint32 r = ( bgra >> 16 ) & 0xFF, g = ( bgra >> 8 ) & 0xFF, b = bgra & 0xFF;
	return quint16( ( ( r >> 3 ) << 11 ) | ( ( g >> 2 ) << 5 ) | ( b >> 3 ) );
}

void lodgenEncodeBC1Block( const quint32 * img, int w, int h, int bx, int by,
	quint8 * out, bool allowPunch = true )
{
	// endpoints: the block's min/max-luminance colours; blocks holding
	// transparent pixels (alpha < 128) use BC1's punch-through mode when
	// allowPunch (a BC3 colour block is always 4-colour — its alpha lives
	// in the alpha block, and RGB under transparent texels must survive so
	// filtering doesn't pull edges to black).
	// Edge blocks of non-multiple-of-4 mips CLAMP their reads — the
	// unclamped version walked off the last row of a 558-wide mip.
	int bestLo = 0, bestHi = 0;
	float loL = 1e9f, hiL = -1e9f;
	bool punch = false;
	quint32 c[16];
	for ( int y = 0; y < 4; y++ )
		for ( int x = 0; x < 4; x++ ) {
			const int sx = qMin( bx * 4 + x, w - 1 );
			const int sy = qMin( by * 4 + y, h - 1 );
			const quint32 p = img[size_t( sy ) * w + sx];
			c[y * 4 + x] = p;
			if ( allowPunch && ( ( p >> 24 ) & 0xFF ) < 128 ) {
				punch = true;
				continue;
			}
			const float l = 0.299f * ( ( p >> 16 ) & 0xFF )
				+ 0.587f * ( ( p >> 8 ) & 0xFF ) + 0.114f * ( p & 0xFF );
			if ( l < loL ) { loL = l; bestLo = y * 4 + x; }
			if ( l > hiL ) { hiL = l; bestHi = y * 4 + x; }
		}
	quint16 c0 = lodgenPack565( c[bestHi] ), c1 = lodgenPack565( c[bestLo] );
	if ( punch ) {
		// c0 <= c1 selects 3-colour + transparent mode
		if ( c0 > c1 )
			std::swap( c0, c1 );
		float pal[3][3];
		auto unpackP = []( quint16 v, float * rgb ) {
			rgb[0] = float( ( v >> 11 ) & 31 ) * ( 255.0f / 31.0f );
			rgb[1] = float( ( v >> 5 ) & 63 ) * ( 255.0f / 63.0f );
			rgb[2] = float( v & 31 ) * ( 255.0f / 31.0f );
		};
		unpackP( c0, pal[0] );
		unpackP( c1, pal[1] );
		for ( int k = 0; k < 3; k++ )
			pal[2][k] = ( pal[0][k] + pal[1][k] ) * 0.5f;
		quint32 bits = 0;
		for ( int i = 15; i >= 0; i-- ) {
			int best = 3;   // transparent
			if ( ( ( c[i] >> 24 ) & 0xFF ) >= 128 ) {
				const float r = float( ( c[i] >> 16 ) & 0xFF ),
					g = float( ( c[i] >> 8 ) & 0xFF ), b = float( c[i] & 0xFF );
				float bestD = 1e18f;
				for ( int k = 0; k < 3; k++ ) {
					const float d = ( r - pal[k][0] ) * ( r - pal[k][0] )
						+ ( g - pal[k][1] ) * ( g - pal[k][1] )
						+ ( b - pal[k][2] ) * ( b - pal[k][2] );
					if ( d < bestD ) { bestD = d; best = k; }
				}
			}
			bits = ( bits << 2 ) | quint32( best );
		}
		out[0] = quint8( c0 ); out[1] = quint8( c0 >> 8 );
		out[2] = quint8( c1 ); out[3] = quint8( c1 >> 8 );
		out[4] = quint8( bits ); out[5] = quint8( bits >> 8 );
		out[6] = quint8( bits >> 16 ); out[7] = quint8( bits >> 24 );
		return;
	}
	if ( c0 == c1 ) {
		out[0] = quint8( c0 ); out[1] = quint8( c0 >> 8 );
		out[2] = quint8( c1 ); out[3] = quint8( c1 >> 8 );
		out[4] = out[5] = out[6] = out[7] = 0;
		return;
	}
	if ( c0 < c1 )
		std::swap( c0, c1 );
	// palette in RGB
	auto unpack = []( quint16 v, float * rgb ) {
		rgb[0] = float( ( v >> 11 ) & 31 ) * ( 255.0f / 31.0f );
		rgb[1] = float( ( v >> 5 ) & 63 ) * ( 255.0f / 63.0f );
		rgb[2] = float( v & 31 ) * ( 255.0f / 31.0f );
	};
	float pal[4][3];
	unpack( c0, pal[0] );
	unpack( c1, pal[1] );
	for ( int k = 0; k < 3; k++ ) {
		pal[2][k] = ( 2.0f * pal[0][k] + pal[1][k] ) / 3.0f;
		pal[3][k] = ( pal[0][k] + 2.0f * pal[1][k] ) / 3.0f;
	}
	quint32 bits = 0;
	for ( int i = 15; i >= 0; i-- ) {
		const float r = float( ( c[i] >> 16 ) & 0xFF ), g = float( ( c[i] >> 8 ) & 0xFF ),
			b = float( c[i] & 0xFF );
		int best = 0;
		float bestD = 1e18f;
		for ( int k = 0; k < 4; k++ ) {
			const float d = ( r - pal[k][0] ) * ( r - pal[k][0] )
				+ ( g - pal[k][1] ) * ( g - pal[k][1] )
				+ ( b - pal[k][2] ) * ( b - pal[k][2] );
			if ( d < bestD ) { bestD = d; best = k; }
		}
		bits = ( bits << 2 ) | quint32( best );
	}
	out[0] = quint8( c0 ); out[1] = quint8( c0 >> 8 );
	out[2] = quint8( c1 ); out[3] = quint8( c1 >> 8 );
	out[4] = quint8( bits ); out[5] = quint8( bits >> 8 );
	out[6] = quint8( bits >> 16 ); out[7] = quint8( bits >> 24 );
}

} // namespace

bool lodgenWriteDds( const QString & path, int w, int h,
	const std::vector<quint32> & bgra, bool bc3, int maxMips, bool bc1Alpha,
	quint32 stamp0, quint32 stamp1, bool mipsToOne )
{
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly ) )
		return false;
	// mip chain by box filter, down to 4x4 (block floor); BC3 carries alpha
	// through the chain, and so does BC1 when bc1Alpha -- a BC1 sheet whose
	// alpha is a CUT-OUT mask must keep it, or every mip past the top turns
	// a tree's leaves back into a solid square, which is precisely the
	// distance an atlas is looked at. Without it BC1 stays opaque, which is
	// what every all-opaque caller (the terrain bakes, the emissive sheets)
	// already wrote, byte for byte. maxMips > 0 stops the chain early:
	// a sheet of frames must not mip past the point where a frame is a few
	// texels, or neighbouring views blend into one another.
	// mipsToOne carries the chain past the 4x4 block floor down to 1x1, which
	// is what every shipped Bethesda terrain sheet does (measured: all 6,120
	// Commonwealth sheets are 512x512 with 10 mips, i.e. 512..1). It is off by
	// default, and with the floor at 4x4 the two paths agree level for level,
	// so every existing caller writes the bytes it wrote before. The per-level
	// dimensions are the ones the loop actually produced, kept in mipW/mipH,
	// rather than a second independently halved copy of them further down.
	std::vector<std::vector<quint32>> mips;
	std::vector<int> mipW, mipH;
	mips.push_back( bgra );
	int mw = w, mh = h;
	mipW.push_back( mw );
	mipH.push_back( mh );
	while ( ( mipsToOne ? ( mw > 1 || mh > 1 ) : ( mw > 4 && mh > 4 ) )
			&& ( maxMips <= 0 || int( mips.size() ) < maxMips ) ) {
		const std::vector<quint32> & prev = mips.back();
		const int nw = qMax( 1, mw / 2 ), nh = qMax( 1, mh / 2 );
		std::vector<quint32> next( size_t( nw ) * nh );
		for ( int y = 0; y < nh; y++ )
			for ( int x = 0; x < nw; x++ ) {
				quint32 acc[4] = { 0, 0, 0, 0 };
				for ( int sy = 0; sy < 2; sy++ )
					for ( int sx = 0; sx < 2; sx++ ) {
						// clamp: a 1-wide or 1-tall level has no second sample.
						// Unreachable while the floor is 4x4, so the bytes the
						// existing callers write do not move.
						const quint32 p = prev[size_t( qMin( y * 2 + sy, mh - 1 ) ) * mw
											   + qMin( x * 2 + sx, mw - 1 )];
						acc[0] += ( p >> 16 ) & 0xFF;
						acc[1] += ( p >> 8 ) & 0xFF;
						acc[2] += p & 0xFF;
						acc[3] += ( p >> 24 ) & 0xFF;
					}
				next[size_t( y ) * nw + x] =
					// round to nearest, not truncate: the loss compounds down a mip chain
					( ( ( bc3 || bc1Alpha ) ? ( ( acc[3] + 2 ) >> 2 ) : 0xFFU ) << 24 )
					| ( ( ( acc[0] + 2 ) >> 2 ) << 16 ) | ( ( ( acc[1] + 2 ) >> 2 ) << 8 )
					| ( ( acc[2] + 2 ) >> 2 );
			}
		mips.push_back( std::move( next ) );
		mw = nw;
		mh = nh;
		mipW.push_back( mw );
		mipH.push_back( mh );
	}

	const quint32 blockBytes = bc3 ? 16 : 8;
	quint32 hdr[32] = { 0 };
	hdr[0] = 0x20534444;            // 'DDS '
	hdr[1] = 124;
	hdr[2] = 0x000A1007;            // caps|height|width|linearsize|pf|mipcount
	hdr[3] = quint32( h );
	hdr[4] = quint32( w );
	hdr[5] = quint32( ( ( w + 3 ) / 4 ) * ( ( h + 3 ) / 4 ) * blockBytes );
	hdr[7] = quint32( mips.size() );
	hdr[19] = 32;
	hdr[20] = 0x4;                  // fourCC
	hdr[21] = bc3 ? 0x35545844U : 0x31545844U;   // 'DXT5' / 'DXT1'
	hdr[27] = 0x401008;             // caps: complex|texture|mipmap
	/* dwReserved1[11] -- file offsets 32..75, hdr[8]..hdr[18] -- is zero in
	 * every DDS this tree has ever written and is ignored by every reader in
	 * it. hdr[8]/hdr[9] carry the ground-cover provenance stamp: 'WWCV' and
	 * (lawVersion << 24) | round(COVER_FULL). A DXT5 terrain data sheet
	 * WITHOUT it carries no cover, whatever its alpha decodes to. */
	hdr[8] = stamp0;
	hdr[9] = stamp1;
	f.write( reinterpret_cast<const char *>( hdr ), 128 );
	for ( size_t mi = 0; mi < mips.size(); mi++ ) {
		const std::vector<quint32> & mip = mips[mi];
		mw = mipW[mi];
		mh = mipH[mi];
		const int bw = ( mw + 3 ) / 4, bh = ( mh + 3 ) / 4;
		std::vector<quint8> block( size_t( bw ) * bh * blockBytes );
		// BLOCK ROWS IN PARALLEL: disjoint writes into `block`, `mip` read-only.
		lodgenParallelFor( bh, [&]( int by ) {
			for ( int bx = 0; bx < bw; bx++ ) {
				quint8 * out = block.data() + ( size_t( by ) * bw + bx ) * blockBytes;
				if ( bc3 ) {
					/* BC3 alpha block: 8-step interpolated palette over the
					 * block's min/max alpha. */
					quint8 a[16];
					quint8 aMin = 255, aMax = 0;
					for ( int i = 0; i < 16; i++ ) {
						const int sx = qMin( bx * 4 + ( i & 3 ), mw - 1 );
						const int sy = qMin( by * 4 + ( i >> 2 ), mh - 1 );
						a[i] = quint8( mip[size_t( sy ) * mw + sx] >> 24 );
						aMin = qMin( aMin, a[i] );
						aMax = qMax( aMax, a[i] );
					}
					out[0] = aMax;
					out[1] = aMin;
					quint64 bits = 0;
					quint8 pal[8];
					pal[0] = aMax;
					pal[1] = aMin;
					for ( int k = 1; k < 7; k++ )
						pal[k + 1] = quint8( ( ( 7 - k ) * aMax + k * aMin ) / 7 );
					for ( int i = 15; i >= 0; i-- ) {
						int best = 0, bd = 256;
						for ( int k = 0; k < 8; k++ ) {
							const int d = qAbs( int( a[i] ) - int( pal[k] ) );
							if ( d < bd ) { bd = d; best = k; }
						}
						bits = ( bits << 3 ) | quint64( best );
					}
					for ( int k = 0; k < 6; k++ )
						out[2 + k] = quint8( bits >> ( k * 8 ) );
					out += 8;
				}
				lodgenEncodeBC1Block( mip.data(), mw, mh, bx, by, out, !bc3 );
			}
		} );
		f.write( reinterpret_cast<const char *>( block.data() ), qint64( block.size() ) );
	}
	return true;
}

//! Cached loader for source landscape textures. A .bgsm path (material-backed
//! TXSTs carry no TX00) is resolved through the material's diffuse slot.
const DDSTexture16 * lodgenLoadTexture( const QString & dataRoot,
	const QString & texPath, QHash<QString, DDSTexture16 *> & cache )
{
	QString key = texPath.toLower();
	auto it = cache.constFind( key );
	if ( it != cache.constEnd() )
		return *it;
	DDSTexture16 * tex = nullptr;
	QString path = texPath;
	path.replace( QChar( '\\' ), QChar( '/' ) );
	if ( path.endsWith( QStringLiteral( ".bgsm" ), Qt::CaseInsensitive ) ) {
		/* THE SAME CUT (lane CELLVIEW2) -- this is the LANDSCAPE texture
		 * loader, which a material-backed TXST sends through a `.bgsm`, and
		 * it is the path the cell view's painted ground now takes as well.
		 * See the note at the model loader above. */
		const int pmi = path.lastIndexOf( QStringLiteral( "materials/" ), -1,
			Qt::CaseInsensitive );
		if ( pmi > 0 )
			path.remove( 0, pmi );
		else if ( !path.startsWith( QStringLiteral( "materials/" ), Qt::CaseInsensitive ) )
			path.prepend( QStringLiteral( "materials/" ) );
		QByteArray mbytes;
		const bool haveMat = lodgenReadAsset( dataRoot, path, "materials", ".bgsm", mbytes );
		path.clear();
		if ( haveMat ) {
			const ShaderMaterial sm( mbytes );
			if ( sm.isValid() && !sm.textures().isEmpty() )
				path = sm.textures().first();
		}
		path.replace( QChar( '\\' ), QChar( '/' ) );
		if ( path.isEmpty() ) {
			cache.insert( key, nullptr );
			return nullptr;
		}
	}
	if ( !path.startsWith( QStringLiteral( "textures/" ), Qt::CaseInsensitive ) )
		path.prepend( QStringLiteral( "textures/" ) );
	QByteArray dds;
	if ( lodgenReadAsset( dataRoot, path, "textures", ".dds", dds ) ) {
		try {
			// the decoder unpacks into its own storage; the bytes need not outlive this
			tex = new DDSTexture16( reinterpret_cast<const unsigned char *>( dds.constData() ), size_t( dds.size() ) );
		} catch ( std::exception & ) {
			tex = nullptr;
		}
	}
	cache.insert( key, tex );
	return tex;
}

/*! One layer of a DX10 texture array: the full mip chain, BC1 or BC3,
 *  appended to `out`. The mip filter and the block encoders are the ones
 *  lodgenWriteDds uses, in the same order, so a layer is byte for byte what
 *  the single-texture writer would have produced for that image; the loop
 *  is repeated rather than shared because the single writer's output is
 *  gated by harnesses and was not to move. Returns the mip count. */
static int lodgenEncodeArrayLayer( const std::vector<quint32> & bgra, int w, int h, bool bc3,
	std::vector<quint8> & out, int maxMips = 0 )
{
	std::vector<std::vector<quint32>> mips;
	mips.push_back( bgra );
	int mw = w, mh = h;
	while ( mw > 4 && mh > 4 && ( maxMips <= 0 || int( mips.size() ) < maxMips ) ) {
		const std::vector<quint32> & prev = mips.back();
		const int nw = mw / 2, nh = mh / 2;
		std::vector<quint32> next( size_t( nw ) * nh );
		for ( int y = 0; y < nh; y++ )
			for ( int x = 0; x < nw; x++ ) {
				quint32 acc[4] = { 0, 0, 0, 0 };
				for ( int sy = 0; sy < 2; sy++ )
					for ( int sx = 0; sx < 2; sx++ ) {
						const quint32 p = prev[size_t( y * 2 + sy ) * mw + ( x * 2 + sx )];
						acc[0] += ( p >> 16 ) & 0xFF;
						acc[1] += ( p >> 8 ) & 0xFF;
						acc[2] += p & 0xFF;
						acc[3] += ( p >> 24 ) & 0xFF;
					}
				next[size_t( y ) * nw + x] =
					// round to nearest, not truncate: the loss compounds down a mip chain
					( ( bc3 ? ( ( acc[3] + 2 ) >> 2 ) : 0xFFU ) << 24 )
					| ( ( ( acc[0] + 2 ) >> 2 ) << 16 ) | ( ( ( acc[1] + 2 ) >> 2 ) << 8 )
					| ( ( acc[2] + 2 ) >> 2 );
			}
		mips.push_back( std::move( next ) );
		mw = nw;
		mh = nh;
	}
	const quint32 blockBytes = bc3 ? 16 : 8;
	mw = w;
	mh = h;
	for ( const std::vector<quint32> & mip : mips ) {
		const int bw = ( mw + 3 ) / 4, bh = ( mh + 3 ) / 4;
		const size_t at = out.size();
		out.resize( at + size_t( bw ) * bh * blockBytes );
		// BLOCK ROWS IN PARALLEL: disjoint writes into `out`, `mip` read-only.
		lodgenParallelFor( bh, [&]( int by ) {
			for ( int bx = 0; bx < bw; bx++ ) {
				quint8 * o = out.data() + at + ( size_t( by ) * bw + bx ) * blockBytes;
				if ( bc3 ) {
					quint8 a[16];
					quint8 aMin = 255, aMax = 0;
					for ( int i = 0; i < 16; i++ ) {
						const int sx = qMin( bx * 4 + ( i & 3 ), mw - 1 );
						const int sy = qMin( by * 4 + ( i >> 2 ), mh - 1 );
						a[i] = quint8( mip[size_t( sy ) * mw + sx] >> 24 );
						aMin = qMin( aMin, a[i] );
						aMax = qMax( aMax, a[i] );
					}
					o[0] = aMax;
					o[1] = aMin;
					quint64 bits = 0;
					quint8 pal[8];
					pal[0] = aMax;
					pal[1] = aMin;
					for ( int k = 1; k < 7; k++ )
						pal[k + 1] = quint8( ( ( 7 - k ) * aMax + k * aMin ) / 7 );
					for ( int i = 15; i >= 0; i-- ) {
						int best = 0, bd = 256;
						for ( int k = 0; k < 8; k++ ) {
							const int d = qAbs( int( a[i] ) - int( pal[k] ) );
							if ( d < bd ) { bd = d; best = k; }
						}
						bits = ( bits << 3 ) | quint64( best );
					}
					for ( int k = 0; k < 6; k++ )
						o[2 + k] = quint8( bits >> ( k * 8 ) );
					o += 8;
				}
				lodgenEncodeBC1Block( mip.data(), mw, mh, bx, by, o, !bc3 );
			}
		} );
		mw = qMax( 4, mw / 2 );
		mh = qMax( 4, mh / 2 );
	}
	return int( mips.size() );
}

/*! A DX10 texture array: every layer the same size with its full mip chain,
 *  layer after layer, behind a DDS header whose fourCC is DX10 and whose
 *  extension header carries the DXGI format and the array size. */
bool lodgenWriteDdsArray( const QString & path, int w, int h,
	const std::vector<std::vector<quint32>> & layers, bool bc3, int maxMips = 0 )
{
	if ( layers.empty() )
		return false;
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly ) )
		return false;
	std::vector<quint8> data;
	int mips = 0;
	for ( const std::vector<quint32> & layer : layers )
		mips = lodgenEncodeArrayLayer( layer, w, h, bc3, data, maxMips );
	const quint32 blockBytes = bc3 ? 16 : 8;
	quint32 hdr[32] = { 0 };
	hdr[0] = 0x20534444;            // 'DDS '
	hdr[1] = 124;
	hdr[2] = 0x000A1007;            // caps|height|width|linearsize|pf|mipcount
	hdr[3] = quint32( h );
	hdr[4] = quint32( w );
	hdr[5] = quint32( ( ( w + 3 ) / 4 ) * ( ( h + 3 ) / 4 ) * blockBytes );
	hdr[7] = quint32( mips );
	hdr[19] = 32;
	hdr[20] = 0x4;                  // fourCC
	hdr[21] = 0x30315844U;          // 'DX10'
	hdr[27] = 0x401008;             // caps: complex|texture|mipmap
	// DDS_HEADER_DXT10: dxgiFormat, resourceDimension (3 = 2D), miscFlag, arraySize, miscFlags2
	const quint32 dx10[5] = { bc3 ? 77U : 71U, 3U, 0U, quint32( layers.size() ), 0U };
	f.write( reinterpret_cast<const char *>( hdr ), 128 );
	f.write( reinterpret_cast<const char *>( dx10 ), 20 );
	return f.write( reinterpret_cast<const char *>( data.data() ), qint64( data.size() ) ) == qint64( data.size() );
}

} // namespace

/*! Texture arrays for FO4CS, the way Fallout 76's instanced LOD node carries
 *  them and better: one DX10 BC3 array of each texture of
 *  docs/LODGEN_IMPOSTOR_SPEC.md per texture SIZE CLASS and FAMILY over every
 *  source the chunks reference (tiling or not - an array does not care, an
 *  atlas does), real mips and no bleed, the layer in UV2.y of every vertex,
 *  a `.lodm` beside every set naming its textures and its layers' sources,
 *  and an `A <shape block> <layer> <lodm>` line per shape in each chunk's
 *  manifest, so a chunk stays self-contained after the atlas pass repoints
 *  its diffuse. The stock engine reads nothing of it: the shapes keep their
 *  own textures.
 *
 *  Two families, one per set, decided per SOURCE (a shape's material, or its
 *  diffuse where it names none):
 *   - LEGACY, vanilla-sourced: `_d` the diffuse with its alpha, `_n` the
 *     normal's X and Y (height neutral, sway 0 - a mesh carries its sway
 *     per vertex), `_gsaos` = gloss (smoothness x the vanilla `_s` map's G),
 *     specular (the map's R, the normal's alpha without one, x the specular
 *     strength), AO neutral (the chunk carries it per vertex), subsurface
 *     mask 1 for a source an alpha-tested shape uses, and `_g` the EMISSIVE:
 *     the vanilla glow rule, the diffuse times its own alpha where the shape
 *     is not alpha-tested and black where it is, BC1. Files
 *     `<ws>.LodgenArrays.<WxH>_d/_n/_gsaos/_g.DDS`.
 *   - PBR, from a source `.lodm` of family pbr (lodmSourceCandidate): `_bc`
 *     its base colour, `_n` its normal (blue = height when it says so),
 *     `_rmaos` its third texture raw, `_e` its emissive texture raw (black
 *     when it names none - a pbr source has no vanilla glow to fall back on).
 *     Files `<ws>.LodgenArraysPBR.<WxH>_bc/_n/_rmaos/_e.DDS`.
 *  A legacy `.lodm` supplies its own textures, the third and the emissive raw, under
 *  the legacy names. Run BEFORE the atlas. bungo, 2026-09-06: "keep it
 *  specular or roughness, depending if source texture is vanilla or
 *  .pbrm sourced" - and then our own material for LOD, `.lodm`.
 */
bool lodgenBuildTextureArrays( const QStringList & btoPaths, const QString & dataRoot,
	const QString & arrayFileBase, const QString & arrayGameBase, QString * report, QString * error )
{
	auto fail = [error]( const QString & message ) {
		if ( error )
			*error = message;
		return false;
	};
	struct ShapeSrc { QString tex0, tex1, tex7, material; float smoothness = 1.0f, specMult = 1.0f;
		Color3 emitColor = Color3( 0.0f, 0.0f, 0.0f ); float emitMult = 1.0f;
		bool ownEmit = false; bool alphaTested = false; };
	auto shapeTextures = []( NifModel & nif, const QModelIndex & iShape, ShapeSrc & s ) {
		QModelIndex iShader = nif.getBlockIndex( nif.getLink( iShape, "Shader Property" ) );
		if ( !iShader.isValid() )
			return false;
		QModelIndex iTexSet = nif.getBlockIndex( nif.getLink( iShader, "Texture Set" ) );
		if ( !iTexSet.isValid() )
			return false;
		QModelIndex iArr = nif.getIndex( iTexSet, "Textures" );
		if ( !iArr.isValid() )
			return false;
		s.tex0 = nif.get<QString>( nif.getIndex( iArr, 0 ) );
		s.tex1 = nif.get<QString>( nif.getIndex( iArr, 1 ) );
		if ( nif.get<int>( iTexSet, "Num Textures" ) > 7 )
			s.tex7 = nif.get<QString>( nif.getIndex( iArr, 7 ) );
		s.smoothness = nif.get<float>( iShader, "Smoothness" );
		s.specMult = nif.get<float>( iShader, "Specular Strength" );
		// what the chunk shape EMITS, put there by the chunk builder from the source
		s.emitColor = nif.get<Color3>( iShader, "Emissive Color" );
		s.emitMult = nif.get<float>( iShader, "Emissive Multiple" );
		s.ownEmit = ( nif.get<quint32>( iShader, "Shader Flags 1" )
			& LOD_OWN_EMIT ) != 0;
		s.alphaTested = nif.getBlockIndex( nif.getLink( iShape, "Alpha Property" ) ).isValid();
		return !s.tex0.isEmpty();
	};
	// the chunk's manifest names each shape's SOURCE material (`M <block> <material>`);
	// the chunk shape itself names none, as vanilla's do not
	auto materialsOf = []( const QString & btoPath ) {
		QHash<int, QString> m;
		QFile mf( btoPath + QStringLiteral( ".manifest.txt" ) );
		if ( mf.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
			while ( !mf.atEnd() ) {
				const QString line = QString::fromUtf8( mf.readLine() ).trimmed();
				if ( !line.startsWith( QLatin1String( "M " ) ) )
					continue;
				const QStringList t = line.split( QChar( ' ' ), Qt::SkipEmptyParts );
				if ( t.size() >= 3 )
					m.insert( t[1].toInt(), t.mid( 2 ).join( QChar( ' ' ) ) );
			}
		}
		return m;
	};
	auto sourceKey = []( const ShapeSrc & s ) {
		return ( s.material.isEmpty() ? s.tex0 : s.material ).toLower();
	};

	// pass 1: every distinct source, keyed by material (else diffuse), lower-case
	QMap<QString, ShapeSrc> sources;     // QMap for a stable order
	for ( const QString & path : btoPaths ) {
		NifModel nif;
		if ( !nif.loadFromFile( path ) )
			continue;
		const QHash<int, QString> mats = materialsOf( path );
		for ( int b = 0; b < nif.getBlockCount(); b++ ) {
			QModelIndex iShape = nif.getBlockIndex( b );
			if ( !nif.isNiBlock( iShape, "BSSubIndexTriShape" ) && !nif.isNiBlock( iShape, "BSTriShape" ) )
				continue;
			ShapeSrc s;
			if ( !shapeTextures( nif, iShape, s ) )
				continue;
			s.material = mats.value( b );
			ShapeSrc & keep = sources[sourceKey( s )];
			const bool at = keep.alphaTested || s.alphaTested;
			keep = s;
			keep.alphaTested = at;
		}
	}
	if ( sources.isEmpty() )
		return fail( QStringLiteral( "no textured shapes in the input files" ) );

	// pass 2: resolve a source .lodm, load, class by family and the colour's size, one layer per source in key order
	/* `emissiveFrom` is what the layer's emissive was taken FROM, for the
	 * sidecar: a source .lodm's emissive texture where one was named, the
	 * COLOUR texture where the vanilla glow rule composed it (colour x its own
	 * alpha), and empty where the layer emits nothing. */
	struct Layer { QString key, color, normal, mask, emissiveFrom, lodm; float emissiveScale = 0.0f; };
	struct ArrayClass { bool pbr = false; int w = 0, h = 0; QVector<Layer> layers; std::vector<std::vector<quint32>> bc, n, rm, em; };
	QMap<QString, ArrayClass> classes;               // "family|WxH" -> class (QMap: stable order)
	QHash<QString, DDSTexture16 *> texCache;
	int unreadable = 0, lodmLayers = 0, ownEmitSources = 0;
	for ( auto it = sources.constBegin(); it != sources.constEnd(); ++it ) {
		const ShapeSrc & src = it.value();
		// the source .lodm, when there is one: its family and its textures
		LodmMaterial lm;
		QString lodmPath;
		{
			const QString cand = lodmSourceCandidate( src.material, src.tex0 );
			QString rel = cand;
			rel.replace( QChar( '\\' ), QChar( '/' ) );
			QByteArray bytes;
			if ( !rel.isEmpty() && lodgenReadAsset( dataRoot, rel, "materials", ".lodm", bytes ) ) {
				lm = lodmParse( bytes );
				if ( lm.ok )
					lodmPath = cand;
				else
					fprintf( stderr, "lodgen: arrays: %s rejected: %s\n", cand.toLocal8Bit().constData(), lm.error.toLocal8Bit().constData() );
			}
		}
		const bool pbr = lm.ok && lm.pbr;
		const QString colorPath = ( lm.ok && !lm.color.isEmpty() ) ? lm.color : src.tex0;
		const QString normalPath = ( lm.ok && !lm.normal.isEmpty() ) ? lm.normal : src.tex1;
		const QString maskPath = ( lm.ok && !lm.mask.isEmpty() ) ? lm.mask : src.tex7;
		const DDSTexture16 * d = lodgenLoadTexture( dataRoot, colorPath, texCache );
		if ( !d ) {
			unreadable++;
			continue;
		}
		const DDSTexture16 * nm = normalPath.isEmpty() ? nullptr : lodgenLoadTexture( dataRoot, normalPath, texCache );
		const DDSTexture16 * sp = maskPath.isEmpty() ? nullptr : lodgenLoadTexture( dataRoot, maskPath, texCache );
		// the emissive, when a source .lodm names one; there is no vanilla slot to fall back on
		const QString emissivePath = lm.ok ? lm.emissive : QString();
		const DDSTexture16 * em = emissivePath.isEmpty() ? nullptr : lodgenLoadTexture( dataRoot, emissivePath, texCache );
		const int w = d->getWidth(), h = d->getHeight();
		ArrayClass & cls = classes[QString( "%1|%2x%3" ).arg( pbr ? QStringLiteral( "pbr" ) : QStringLiteral( "legacy" ) ).arg( w ).arg( h )];
		cls.pbr = pbr;
		cls.w = w;
		cls.h = h;
		/* THE EMISSIVE, both halves. The sheet takes the COLOUR: a legacy layer
		 * is the diffuse times its own alpha times the source's emissive colour,
		 * which is black on every measured vanilla chunk shape and therefore
		 * emits nothing. The MULTIPLE rides in the .lodm, and belongs to whichever
		 * law composed the sheet: a source .lodm's own `emissiveScale` where the
		 * .lodm supplied the emissive, the vanilla own-emit multiple where the
		 * glow rule composed it. */
		const bool lodmEmissive = lm.ok && ( !lm.emissive.isEmpty() || lm.pbr );
		const bool colorEmits = src.emitColor.red() > 0.0f || src.emitColor.green() > 0.0f
			|| src.emitColor.blue() > 0.0f;
		const float vanillaScale = lodgenEmissiveScale( src.ownEmit, src.emitColor, src.emitMult );
		if ( vanillaScale > 0.0f )
			ownEmitSources++;
		const float emissiveScale = lodmEmissive ? lm.emissiveScale : vanillaScale;
		const bool glowRule = !pbr && !src.alphaTested && !lodmEmissive && colorEmits;
		const quint32 mask = src.alphaTested ? 0xFFU : 0U;
		const float smooth = qBound( 0.0f, src.smoothness, 1.0f );
		const float mult = src.specMult > 0.0f ? src.specMult : 1.0f;
		auto b8 = []( float f ) { return quint32( qBound( 0, int( f * 255.0f + 0.5f ), 255 ) ); };
		auto mipFor = [w]( const DDSTexture16 * t ) {
			return qBound( 0.0f, std::log2( qMax( 1.0f, float( t->getWidth() ) / w ) ), float( t->getMaxMipLevel() ) );
		};
		std::vector<quint32> bc( size_t( w ) * h ), n( size_t( w ) * h, 0x00808080U ), rm( size_t( w ) * h ),
			em8( size_t( w ) * h, 0xFF000000U );		// the emissive: black and opaque, it ships as BC1
		for ( int y = 0; y < h; y++ ) {
			for ( int x = 0; x < w; x++ ) {
				const float u = ( float( x ) + 0.5f ) / w, v = ( float( y ) + 0.5f ) / h;
				const FloatVector4 c = d->getPixelT( u, v, 0.0f );
				bc[size_t( y ) * w + x] = ( b8( c[3] ) << 24 ) | ( b8( c[0] ) << 16 ) | ( b8( c[1] ) << 8 ) | b8( c[2] );
				float nAlpha = 1.0f;
				if ( nm ) {
					// X, Y from the source, resampled to the colour's size; height only when the .lodm says its blue carries it; sway 0
					const FloatVector4 nv = nm->getPixelT( u, v, mipFor( nm ) );
					const quint32 hb = ( lm.ok && lm.heightInBlue ) ? b8( nv[2] ) : 128U;
					n[size_t( y ) * w + x] = ( 0U << 24 ) | ( b8( nv[0] ) << 16 ) | ( b8( nv[1] ) << 8 ) | hb;
					nAlpha = nv[3];
				}
				quint32 r, g, bl;
				if ( lm.ok ) {
					// a .lodm's third texture, raw: roughness/metallic/AO or gloss/specular/AO as the file says
					if ( sp ) {
						const FloatVector4 sv = sp->getPixelT( u, v, mipFor( sp ) );
						r = b8( sv[0] ); g = b8( sv[1] ); bl = b8( sv[2] );
					} else {
						r = 128U; g = 0U; bl = 255U;
					}
				} else {
					// the vanilla material, as the engine composes it: gloss = smoothness x _s.G, specular = _s.R (the normal's alpha without a map) x strength; AO neutral
					float sR = nAlpha, sG = 1.0f;
					if ( sp ) {
						const FloatVector4 sv = sp->getPixelT( u, v, mipFor( sp ) );
						sR = sv[0]; sG = sv[1];
					}
					/* THE ONE GLOSS LAW (lodgen.h). The far-terrain mask sheet
					 * stores `1 - lodgenLegacyGloss(...)` for a legacy layer,
					 * so the two must be the same expression or the object
					 * sheets and the terrain sheet disagree about the same
					 * material. `smooth` is already clamped to 0..1 above and
					 * the function clamps again, so this is the identical
					 * number this line produced before it was shared. */
					r = b8( lodgenLegacyGloss( smooth, sG ) ); g = b8( qMin( 1.0f, sR * mult ) ); bl = 255U;
				}
				rm[size_t( y ) * w + x] = ( mask << 24 ) | ( r << 16 ) | ( g << 8 ) | bl;
				/* The emissive. A source .lodm's texture, raw. Otherwise the
				 * VANILLA LOD GLOW RULE: a vanilla LOD chunk shape carries
				 * Own-Emit with a black emissive colour and no glow slot, and
				 * its light is the DIFFUSE'S ALPHA on the opaque shapes; an
				 * alpha-tested shape spends that alpha on its cut-out and emits
				 * nothing. A pbr source has no vanilla quantity to fall back on,
				 * so it is black unless its .lodm named one. The vanilla rule is
				 * COMPLETE here: the emissive COLOUR multiplies it, because that is
				 * what the engine scales the alpha by. Black colour, black sheet. */
				if ( em ) {
					const FloatVector4 ev = em->getPixelT( u, v, mipFor( em ) );
					em8[size_t( y ) * w + x] = 0xFF000000U | ( b8( ev[0] ) << 16 ) | ( b8( ev[1] ) << 8 ) | b8( ev[2] );
				} else if ( glowRule ) {
					em8[size_t( y ) * w + x] = 0xFF000000U
						| ( b8( c[0] * c[3] * src.emitColor.red() ) << 16 )
						| ( b8( c[1] * c[3] * src.emitColor.green() ) << 8 )
						| b8( c[2] * c[3] * src.emitColor.blue() );
				}
			}
		}
		QString emissiveFrom;
		if ( em )
			emissiveFrom = emissivePath;
		else if ( glowRule )
			emissiveFrom = colorPath;		// the glow rule composed it from the colour, its alpha and the emissive colour
		cls.layers.append( Layer{ it.key(), colorPath, normalPath, maskPath, emissiveFrom, lodmPath, emissiveScale } );
		cls.bc.push_back( std::move( bc ) );
		cls.n.push_back( std::move( n ) );
		cls.rm.push_back( std::move( rm ) );
		cls.em.push_back( std::move( em8 ) );
		if ( lm.ok )
			lodmLayers++;
	}
	for ( DDSTexture16 * t : texCache )
		delete t;
	if ( classes.isEmpty() )
		return fail( QStringLiteral( "none of the referenced textures could be read" ) );

	// pass 3: the arrays, a .lodm per set, and the sidecar
	QHash<QString, QPair<QString, int>> layerOf;     // key -> (set .lodm game path, layer)
	QFile side( arrayFileBase + QStringLiteral( ".txt" ) );
	if ( !side.open( QIODevice::WriteOnly | QIODevice::Text ) )
		return fail( QStringLiteral( "could not write the array sidecar" ) );
	QTextStream ss( &side );
	// version 5: emissiveScale appended on the END, so a reader that indexes the
	// first nine columns by position is unaffected (the C lines' rule, again)
	ss << "# lodgen texture arrays 5: family class layer lodm color normal mask emissive source emissiveScale (docs/LODGEN_IMPOSTOR_SPEC.md)\n";
	int arrays = 0, textures = 0, legacyClasses = 0, pbrClasses = 0;
	for ( auto it = classes.begin(); it != classes.end(); ++it ) {
		ArrayClass & cls = it.value();
		const QString sizeKey = it.key().mid( it.key().indexOf( QChar( '|' ) ) + 1 );
		const QString stem = QString( "%1.%2" ).arg( cls.pbr ? QStringLiteral( "PBR" ) : QString() ).arg( sizeKey );
		const QString fileBase = arrayFileBase + stem, gameBase = arrayGameBase + stem;
		const QString colorSfx = QLatin1String( lodmColorSuffix( cls.pbr ) ) + QStringLiteral( ".DDS" );
		const QString maskSfx = QLatin1String( lodmMaskSuffix( cls.pbr ) ) + QStringLiteral( ".DDS" );
		const QString emSfx = QLatin1String( lodmEmissiveSuffix( cls.pbr ) ) + QStringLiteral( ".DDS" );
		const struct { QString suffix; const std::vector<std::vector<quint32>> * px; bool bc3; } sheets[4] = {
			{ colorSfx, &cls.bc, true }, { QStringLiteral( "_n.DDS" ), &cls.n, true },
			{ maskSfx, &cls.rm, true },
			// the emissive is BC1: three channels and no alpha to carry
			{ emSfx, &cls.em, false } };
		for ( const auto & s : sheets ) {
			if ( !lodgenWriteDdsArray( fileBase + s.suffix, cls.w, cls.h, *s.px, s.bc3 ) )
				return fail( QString( "could not write %1" ).arg( fileBase + s.suffix ) );
			arrays++;
		}
		// the set's .lodm: family, the three arrays, the source per layer
		QJsonObject root, tex, arr;
		root.insert( QStringLiteral( "lodm" ), 1 );
		root.insert( QStringLiteral( "family" ), cls.pbr ? QStringLiteral( "pbr" ) : QStringLiteral( "legacy" ) );
		root.insert( QStringLiteral( "kind" ), QStringLiteral( "array" ) );
		tex.insert( QLatin1String( lodmColorKey( cls.pbr ) ), gameBase + colorSfx );
		tex.insert( QStringLiteral( "normal" ), gameBase + QStringLiteral( "_n.DDS" ) );
		tex.insert( QLatin1String( lodmMaskKey( cls.pbr ) ), gameBase + maskSfx );
		tex.insert( QStringLiteral( "emissive" ), gameBase + emSfx );
		root.insert( QStringLiteral( "textures" ), tex );
		arr.insert( QStringLiteral( "class" ), QJsonArray{ cls.w, cls.h } );
		QJsonArray layers, scales;
		for ( const Layer & l : cls.layers ) {
			layers.append( l.color );
			scales.append( double( l.emissiveScale ) );
		}
		arr.insert( QStringLiteral( "layers" ), layers );
		// one multiple per layer, parallel to `layers`: two layers of one array
		// are two materials and do not share it
		arr.insert( QStringLiteral( "emissiveScale" ), scales );
		root.insert( QStringLiteral( "array" ), arr );
		if ( !lodmWriteFile( fileBase + QStringLiteral( ".lodm" ), root ) )
			return fail( QString( "could not write %1" ).arg( fileBase + QStringLiteral( ".lodm" ) ) );
		const QString lodmGame = gameBase + QStringLiteral( ".lodm" );
		( cls.pbr ? pbrClasses : legacyClasses )++;
		for ( int l = 0; l < cls.layers.size(); l++ ) {
			layerOf.insert( cls.layers[l].key, qMakePair( lodmGame, l ) );
			ss << ( cls.pbr ? "pbr" : "legacy" ) << ' ' << sizeKey << ' ' << l << ' ' << lodmGame << ' '
			   << cls.layers[l].color << ' ' << ( cls.layers[l].normal.isEmpty() ? QStringLiteral( "-" ) : cls.layers[l].normal ) << ' '
			   << ( cls.layers[l].mask.isEmpty() ? QStringLiteral( "-" ) : cls.layers[l].mask ) << ' '
			   << ( cls.layers[l].emissiveFrom.isEmpty() ? QStringLiteral( "-" ) : cls.layers[l].emissiveFrom ) << ' '
			   << ( cls.layers[l].lodm.isEmpty() ? cls.layers[l].key : cls.layers[l].lodm ) << ' '
			   << cls.layers[l].emissiveScale << '\n';
			textures++;
		}
	}
	side.close();

	// pass 4: the layer into UV2.y of every vertex, and a line per shape in the chunk's manifest
	int shapesWithLayer = 0, shapesWithoutUv2 = 0;
	for ( const QString & path : btoPaths ) {
		NifModel nif;
		if ( !nif.loadFromFile( path ) )
			continue;
		const QHash<int, QString> mats = materialsOf( path );
		QStringList lines;
		bool changed = false;
		for ( int b = 0; b < nif.getBlockCount(); b++ ) {
			QModelIndex iShape = nif.getBlockIndex( b );
			if ( !nif.isNiBlock( iShape, "BSSubIndexTriShape" ) && !nif.isNiBlock( iShape, "BSTriShape" ) )
				continue;
			ShapeSrc s;
			if ( !shapeTextures( nif, iShape, s ) )
				continue;
			s.material = mats.value( b );
			auto lit = layerOf.constFind( sourceKey( s ) );
			if ( lit == layerOf.constEnd() )
				continue;
			QModelIndex iVerts = nif.getIndex( iShape, "Vertex Data" );
			const int numVerts = int( nif.get<quint32>( iShape, "Num Vertices" ) );
			if ( !iVerts.isValid() || numVerts <= 0 )
				continue;
			if ( !nif.getIndex( nif.index( 0, 0, iVerts ), "UV 2" ).isValid() ) {
				shapesWithoutUv2++;		// a profile without the extra channels: the manifest still says
			} else {
				nif.setState( BaseModel::Processing );
				for ( int v = 0; v < numVerts; v++ ) {
					QModelIndex row = nif.index( v, 0, iVerts );
					const Vector2 uv2 = nif.get<HalfVector2>( row, "UV 2" );
					nif.set<HalfVector2>( row, "UV 2", HalfVector2( Vector2( uv2[0], float( lit.value().second ) ) ) );
				}
				nif.restoreState();
				changed = true;
			}
			lines.append( QString( "A %1 %2 %3" ).arg( b ).arg( lit.value().second ).arg( lit.value().first ) );
			shapesWithLayer++;
		}
		if ( changed && !nif.saveToFile( path ) )
			return fail( QString( "could not rewrite %1" ).arg( path ) );
		QFile mf( path + QStringLiteral( ".manifest.txt" ) );
		if ( !lines.isEmpty() && mf.exists() && mf.open( QIODevice::ReadWrite | QIODevice::Text ) ) {
			// a whole line after the last one, whether or not the file ended with a newline
			QByteArray all = mf.readAll();
			if ( !all.isEmpty() && !all.endsWith( '\n' ) )
				mf.write( "\n" );
			mf.write( ( lines.join( QChar( '\n' ) ) + QChar( '\n' ) ).toUtf8() );
		}
	}
	if ( report )
		*report = QString( "%1 textures in %2 arrays (%3 legacy + %4 pbr size classes, %5 layers from a .lodm, %6 sources own-emit with a lit colour); %7 shapes carry a layer, %8 without UV2, %9 textures unreadable" )
			.arg( textures ).arg( arrays ).arg( legacyClasses ).arg( pbrClasses ).arg( lodmLayers )
			.arg( ownEmitSources )
			.arg( shapesWithLayer ).arg( shapesWithoutUv2 ).arg( unreadable );
	return true;
}

namespace
{

} // namespace
/*! Write a single-channel 16-bit DDS (R16_UNORM, no mips).
 *
 *  A DX10 header rather than a legacy luminance one, because R16_UNORM has no
 *  unambiguous legacy spelling and the consumer names that exact DXGI format.
 */
/* FO4CS provenance for a heightmap, DDS dwReserved1[11] at file offset 32.
 * Field for field WriteBakeProvenance (FarFieldHeightmapFormat.h:631-659);
 * the block is all-or-nothing: with the magic present the loader arms its
 * version, encoding, extent, corpus-hash and pixel-hash gates, and any
 * mismatch is a named refusal, so it is verified by the CLI after every bake. */
struct LodgenHeightmapProvenance
{
	quint64 corpusHash = 0;
	int south = 0, west = 0, north = 0, east = 0;   // the filename's four cell fields
	quint32 coveredTexels = 0;                       // census only; never read
};

static const quint64 F4FX_FNV_OFFSET = Q_UINT64_C( 0xCBF29CE484222325 );
static const quint64 F4FX_FNV_PRIME = Q_UINT64_C( 0x100000001B3 );

static bool lodgenWriteR16Dds( const QString & path, int w, int h,
	const std::vector<quint16> & texels, const LodgenHeightmapProvenance * prov,
	quint64 * pixelHashOut = nullptr )
{
	if ( int( texels.size() ) != w * h )
		return false;
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly ) )
		return false;
	/* The pixel hash is FNV-1a 64 over the payload EXACTLY AS WRITTEN: w*h
	 * little-endian uint16 in row order and nothing else. Hashed from the
	 * byte pairs the row loop emits rather than the vector's memory, so the
	 * two cannot drift apart. */
	quint64 pixelHash = F4FX_FNV_OFFSET;
	if ( prov ) {
		for ( size_t i = 0; i < texels.size(); i++ ) {
			const quint16 v = texels[i];
			pixelHash ^= quint8( v & 0xFF );
			pixelHash *= F4FX_FNV_PRIME;
			pixelHash ^= quint8( ( v >> 8 ) & 0xFF );
			pixelHash *= F4FX_FNV_PRIME;
		}
	}
	if ( pixelHashOut )
		*pixelHashOut = pixelHash;
	QByteArray hdr( 4 + 124 + 20, '\0' );
	auto put = [&hdr]( int off, quint32 v ) {
		hdr[off]     = char( v & 0xFF );
		hdr[off + 1] = char( ( v >> 8 ) & 0xFF );
		hdr[off + 2] = char( ( v >> 16 ) & 0xFF );
		hdr[off + 3] = char( ( v >> 24 ) & 0xFF );
	};
	/* Offsets are FILE offsets: 4 bytes of magic, then DDS_HEADER, whose
	 * ddspf begins at header+72 -- NOT +76. Getting that wrong puts the
	 * fourCC in dwRGBBitCount and leaves dwCaps zero, which reads back
	 * self-consistently through the same wrong offsets and looks fine. It was
	 * caught only by parsing a shipped map with the same code and seeing
	 * "DX10" land where the flags were expected. */
	put( 0, 0x20534444 );                        // "DDS "
	put( 4 + 0, 124 );                           // dwSize
	put( 4 + 4, 0x1 | 0x2 | 0x4 | 0x8 | 0x1000 ); // CAPS|HEIGHT|WIDTH|PITCH|PIXELFORMAT
	put( 4 + 8, quint32( h ) );
	put( 4 + 12, quint32( w ) );
	put( 4 + 16, quint32( w * 2 ) );             // pitch, bytes per row
	put( 4 + 24, 1 );                            // dwMipMapCount: base only
	if ( prov ) {
		const quint16 s16 = quint16( qint16( prov->south ) );
		const quint16 w16 = quint16( qint16( prov->west ) );
		const quint16 n16 = quint16( qint16( prov->north ) );
		const quint16 e16 = quint16( qint16( prov->east ) );
		put( 32, 0x58463446u );                                          // [0] magic 'F4FX'
		put( 32 + 4, 1u );                                               // [1] version
		put( 32 + 8, quint32( prov->corpusHash & 0xFFFFFFFFu ) );        // [2] corpus lo
		put( 32 + 12, quint32( ( prov->corpusHash >> 32 ) & 0xFFFFFFFFu ) );   // [3] corpus hi
		put( 32 + 16, quint32( pixelHash & 0xFFFFFFFFu ) );              // [4] pixel lo
		put( 32 + 20, quint32( ( pixelHash >> 32 ) & 0xFFFFFFFFu ) );    // [5] pixel hi
		put( 32 + 24, quint32( s16 ) | ( quint32( w16 ) << 16 ) );       // [6] south, west
		put( 32 + 28, quint32( n16 ) | ( quint32( e16 ) << 16 ) );       // [7] north, east
		put( 32 + 32, 1u );                                              // [8] encoding 1 (xLODGen-shaped), flags 0
		put( 32 + 36, prov->coveredTexels );                             // [9] census only
		// [10] stays zero
	}
	put( 4 + 72, 32 );                           // ddspf.dwSize
	put( 4 + 76, 0x4 );                          // ddspf.dwFlags = DDPF_FOURCC
	put( 4 + 80, 0x30315844 );                   // ddspf.dwFourCC = "DX10"
	put( 4 + 104, 0x1000 );                      // dwCaps = DDSCAPS_TEXTURE
	put( 4 + 124 + 0, 56 );                      // DXGI_FORMAT_R16_UNORM
	put( 4 + 124 + 4, 3 );                       // D3D10_RESOURCE_DIMENSION_TEXTURE2D
	put( 4 + 124 + 12, 1 );                      // arraySize
	if ( f.write( hdr ) != hdr.size() )
		return false;
	/* Streamed a row at a time. A whole-image QByteArray would double
	 * peak memory on top of the texel vector, which is the difference
	 * between comfortable and not on a worldspace several times the
	 * Commonwealth's: an 8192 map is already 134 MB of payload. */
	QByteArray row( w * 2, '\0' );
	for ( int y = 0; y < h; y++ ) {
		for ( int x = 0; x < w; x++ ) {
			const quint16 v = texels[size_t( y ) * size_t( w ) + size_t( x )];
			row[x * 2]     = char( v & 0xFF );
			row[x * 2 + 1] = char( ( v >> 8 ) & 0xFF );
		}
		if ( f.write( row ) != row.size() )
			return false;
	}
	return true;
}

/*! Bake a worldspace height map for the FO4CS terrain shadow caster.
 *
 *  Data/Textures/Terrain/<EDID>/<EDID>.HeightMap.<S>.<W>.<N>.<E>.<B>.<T>.dds
 *
 *  Fields are cell coordinates south, west, north, east, then the bottom and
 *  top height. The cell bounds are load-bearing -- they are what maps texels to
 *  world units -- while B and T are informational and the decode ignores them.
 *
 *  Encoding is fixed and lossless: pixel = height/8 + 32767, so flat zero is
 *  32767 and the quantum is the game's own eight-unit height step.
 *
 *  The FOLDER and the filename stem are the worldspace's EDITOR ID exactly as
 *  the plugin spells it, not the name shown in game -- the loader keys on it.
 *
 *  Resolution is a resample, not the native cells*32: one 4K map per worldspace
 *  regardless of extent, which is what a shadow caster wants and what keeps a
 *  192-cell worldspace from needing a 6144-square texture.
 */
bool lodgenBakeHeightmap( const EsmWorld & world, const QString & outDir,
	int resolution, QString * outPath, QString * error )
{
	auto fail = [error]( const QString & m ) {
		if ( error ) *error = m;
		return false;
	};
	const QString edid = world.worldspaceEdid();
	if ( edid.isEmpty() )
		return fail( QStringLiteral( "worldspace has no editor ID to name the map with" ) );
	int minX, minY, maxX, maxY;
	world.cellBounds( minX, minY, maxX, maxY );
	if ( minX > maxX || minY > maxY )
		return fail( QStringLiteral( "worldspace has no indexed cells" ) );
	const int cellsX = maxX - minX + 1;
	const int cellsY = maxY - minY + 1;
	/* Resolution. 0 = NATIVE: cellsX*32 by cellsY*32, one texel per LAND
	 * sample, the only lossless size -- and oblong when the worldspace is,
	 * which Far Harbor (138x143 cells) is. Anything else is a square resample.
	 *
	 * SAMPLE-ALIGNED either way: texel i sits ON world sample i*step, not at
	 * (i + 0.5)*step. An earlier draft used texel centres "so the resample
	 * stays symmetric", and at native size that made every texel the mean of
	 * two adjacent samples: 41.4% of Commonwealth texels off, the 44,872-unit
	 * peak shaved to 44,848 -- lossy at EVERY size, by construction, not by
	 * resolution. Measured: that draft's 6144 map is exactly the 2x2 average
	 * of the sample-aligned Commonwealth_fine reference (max 0.8 of a step,
	 * 100% within one). This reproduces the reference instead. */
	const int RX = resolution > 0 ? qBound( 256, resolution, 8192 ) : cellsX * 32;
	const int RY = resolution > 0 ? qBound( 256, resolution, 8192 ) : cellsY * 32;
	if ( RX > 8192 || RY > 8192 )
		return fail( QStringLiteral(
			"native map would be %1x%2 texels, past the 8192 cap (%3 MB of R16); "
			"pass --heightmap-size <n> for a resample" )
			.arg( RX ).arg( RY ).arg( qint64( RX ) * qint64( RY ) * 2 / 1048576 ) );
	/* A worldspace with MORE cells per side than the map has texels drops
	 * whole cells -- their texel span rounds to nothing and the fill loop
	 * skips them. It fails SILENTLY, so refuse. */
	if ( cellsX > RX || cellsY > RY )
		return fail( QStringLiteral(
			"worldspace is %1x%2 cells but the map is only %3x%4 texels; "
			"raise --heightmap-size" ).arg( cellsX ).arg( cellsY ).arg( RX ).arg( RY ) );

	/* UNWRITTEN sentinel, not the default height: the seam rule below is a
	 * maximum, and a default of -352 would out-vote every sea floor. Unwritten
	 * texels become the default after the loop. */
	constexpr float UNWRITTEN = -3.4e38f;
	std::vector<float> heights( size_t( RX ) * size_t( RY ), UNWRITTEN );
	// world-sample coordinate of texel i is i * step; exactly i at native size
	const double stepX = double( cellsX ) * 32.0 / double( RX );
	const double stepY = double( cellsY ) * 32.0 / double( RY );
	EsmLand land;
	for ( int cy = minY; cy <= maxY; cy++ ) {
		for ( int cx = minX; cx <= maxX; cx++ ) {
			if ( !world.land( cx, cy, land ) )
				continue;
			const int kx = cx - minX, ky = cy - minY;
			/* Every texel whose sample coordinate lies in [k*32, (k+1)*32] --
			 * CLOSED at both ends, so a cell writes its full 33 rows and
			 * columns and the shared VHGT edge is written by every cell that
			 * holds it. Where they disagree, the MAXIMUM wins (below).
			 *
			 * That is the seam rule, and it was measured, not chosen: VHGT's
			 * row 32 is the next cell's row 0 and in a well-formed record they
			 * agree, but 439 Commonwealth cells at the world's edge are flat
			 * -352 filler whose edges disagree with their neighbours' real
			 * terrain. Against a full --dump-land of the ESM, over 2,322,432
			 * edge texels, "maximum of every cell holding the sample" matched
			 * the reference Commonwealth_fine map with 0 mismatches; "the cell
			 * owns its own row 0" missed 8,675, "the south/west cell owns the
			 * edge" 6,981, and last- or first-non-filler thousands each. A
			 * maximum is also the right answer for a SHADOW map: the higher
			 * surface is the one that casts. */
			const int i0 = qMax( 0, int( std::ceil( double( kx ) * 32.0 / stepX - 1e-9 ) ) );
			const int i1 = qMin( RX - 1, int( std::floor( double( kx + 1 ) * 32.0 / stepX + 1e-9 ) ) );
			const int j0 = qMax( 0, int( std::ceil( double( ky ) * 32.0 / stepY - 1e-9 ) ) );
			const int j1 = qMin( RY - 1, int( std::floor( double( ky + 1 ) * 32.0 / stepY + 1e-9 ) ) );
			for ( int j = j0; j <= j1; j++ ) {
				const double v = double( j ) * stepY - double( ky ) * 32.0;   // 0 <= v <= 32
				const int r0 = qBound( 0, int( v ), 31 );
				const int r1 = r0 + 1;                                        // v == 32 lands on row 32 with fr == 1
				const float fr = float( v - double( r0 ) );
				for ( int i = i0; i <= i1; i++ ) {
					const double u = double( i ) * stepX - double( kx ) * 32.0;
					const int c0 = qBound( 0, int( u ), 31 );
					const int c1 = c0 + 1;
					const float fc = float( u - double( c0 ) );
					const float h00 = land.heights[r0][c0], h10 = land.heights[r0][c1];
					const float h01 = land.heights[r1][c0], h11 = land.heights[r1][c1];
					const float top = h00 + ( h10 - h00 ) * fc;
					const float bot = h01 + ( h11 - h01 ) * fc;
					/* Row 0 is NORTH: the image convention, y increasing
					 * southward down the texture. If the consumer disagrees
					 * the fix is this index and nothing else. */
					const int row = RY - 1 - j;
					float & dst = heights[size_t( row ) * size_t( RX ) + size_t( i )];
					dst = qMax( dst, top + ( bot - top ) * fr );
				}
			}
		}
	}

	quint32 covered = 0;
	for ( float & v : heights ) {
		if ( v == UNWRITTEN )
			v = world.defaultLandHeight();
		else
			covered++;
	}
	float hMin = 3.4e38f, hMax = -3.4e38f;
	for ( float v : heights ) {
		hMin = qMin( hMin, v );
		hMax = qMax( hMax, v );
	}
	std::vector<quint16> texels( heights.size() );
	for ( size_t i = 0; i < heights.size(); i++ ) {
		const double p = double( heights[i] ) / 8.0 + 32767.0;
		// round, not truncate: exact at native size, half a step better otherwise
		texels[i] = quint16( qBound( 0.0, std::floor( p + 0.5 ), 65535.0 ) );
	}

	const QString dir = outDir + QStringLiteral( "/Textures/Terrain/" ) + edid;
	QDir().mkpath( dir );
	const QString name = QStringLiteral( "%1.HeightMap.%2.%3.%4.%5.%6.%7.dds" )
		.arg( edid ).arg( minY ).arg( minX ).arg( maxY ).arg( maxX )
		.arg( qRound( hMin ) ).arg( qRound( hMax ) );
	const QString path = dir + QStringLiteral( "/" ) + name;
	LodgenHeightmapProvenance prov;
	int landsHashed = 0;
	prov.corpusHash = world.vhgtCorpusHash( &landsHashed );
	prov.south = minY;
	prov.west = minX;
	prov.north = maxY;
	prov.east = maxX;
	prov.coveredTexels = covered;
	quint64 pixelHash = 0;
	if ( !lodgenWriteR16Dds( path, RX, RY, texels, &prov, &pixelHash ) )
		return fail( QStringLiteral( "could not write %1" ).arg( path ) );
	if ( outPath )
		*outPath = path;
	if ( error )
		*error = QString( "provenance F4FX v1: corpus hash %1 over %2 VHGT, pixel hash %3, "
			"%4 of %5 texels covered" )
			.arg( prov.corpusHash, 16, 16, QChar( '0' ) ).arg( landsHashed )
			.arg( pixelHash, 16, 16, QChar( '0' ) )
			.arg( covered ).arg( qint64( RX ) * RY );
	return true;
}


/* ---- shared bake caches, and the grass tint's colour source ---- */

/*! One pass's caches. The per-chunk terrain path used to declare its texture
 *  cache as a LOCAL and delete it on the way out, so every landscape diffuse
 *  was decoded once per bake unit and thrown away; the pyramid bakes 9,216
 *  dim-2 tiles over the ground 2,304 dim-4 chunks cover, so that would be four
 *  times the units each amortising its loads over 3.5x fewer texels. The cache
 *  is therefore the CALLER'S, and it is an LRU with a stated budget: one
 *  2048^2 landscape diffuse with mips is about 21 MiB decoded and a worldspace
 *  names roughly sixty of them. */
struct LodgenBakeCaches
{
	QHash<QString, DDSTexture16 *> texCache;
	QStringList texOrder;               //!< least recently used first
	qint64 texBudget = qint64( 512 ) << 20;
	qint64 texBytes = 0;
	//! MODL path -> (resolved, average diffuse colour), for the grass tint
	QHash<QString, QPair<bool, FloatVector4>> grassTint;
	int nifReads = 0;                   //!< grass meshes actually loaded
	int texLoads = 0;                   //!< textures actually decoded
	~LodgenBakeCaches()
	{
		for ( DDSTexture16 * t : texCache )
			delete t;
	}
};

LodgenBakeCaches * lodgenCreateBakeCaches( qint64 textureBudgetBytes )
{
	LodgenBakeCaches * c = new LodgenBakeCaches;
	if ( textureBudgetBytes > 0 )
		c->texBudget = textureBudgetBytes;
	return c;
}

void lodgenDestroyBakeCaches( LodgenBakeCaches * caches )
{
	delete caches;
}

void lodgenBakeCacheCounts( const LodgenBakeCaches * caches, int * nifReads, int * texLoads )
{
	if ( nifReads )
		*nifReads = caches ? caches->nifReads : 0;
	if ( texLoads )
		*texLoads = caches ? caches->texLoads : 0;
}

//! lodgenLoadTexture through the LRU. The returned pointer is used inside the
//! call that asked for it and never held across another ask, which is what
//! makes eviction safe.
static const DDSTexture16 * lodgenCachedTexture( LodgenBakeCaches & c,
	const QString & dataRoot, const QString & texPath )
{
	const QString key = texPath.toLower();
	auto it = c.texCache.constFind( key );
	if ( it != c.texCache.constEnd() ) {
		c.texOrder.removeOne( key );
		c.texOrder.append( key );
		return *it;
	}
	const DDSTexture16 * tex = lodgenLoadTexture( dataRoot, texPath, c.texCache );
	c.texOrder.append( key );
	if ( tex ) {
		c.texLoads++;
		c.texBytes += qint64( tex->size() );
	}
	while ( c.texBytes > c.texBudget && c.texOrder.size() > 1 ) {
		const QString victim = c.texOrder.first();
		if ( victim == key )
			break;
		c.texOrder.removeFirst();
		auto vt = c.texCache.find( victim );
		if ( vt != c.texCache.end() ) {
			if ( *vt )
				c.texBytes -= qint64( ( *vt )->size() );
			delete *vt;
			c.texCache.erase( vt );
		}
	}
	return tex;
}

/*! The grass tint's colour source (docs/LODGEN_TERRAIN_VT.md §3.2).
 *
 *  NOT the GRAS record. Its Colour Range is a per-instance random tint SPREAD,
 *  not a colour, and the LTEX's own diffuse is unreliable — 31 of the 105
 *  base-game landscape texture sets have an empty TX00 slot, 17 of them
 *  grass-bearing. The colour lives in the grass MESH's diffuse:
 *
 *    GRAS MODL -> the .nif through lodgenReadAsset (whose .nif-specific path
 *    exists precisely because the Fallout 4 archive filter drops every mesh at
 *    index time) -> the one shape's slot 0, or its .bgsm handed to
 *    lodgenLoadTexture, which already resolves a material to its first slot ->
 *    the texture's SMALLEST mip, which IS its average colour and is already
 *    computed by the loader.
 *
 *  Un-premultiplied: 39 of the 71 shipped grass meshes carry a NiAlphaProperty,
 *  so the smallest mip's RGB is a coverage-weighted average dragged toward the
 *  atlas's transparent gaps. Below alpha 0.05 there is nothing to divide by,
 *  the grass contributes NO tint, and it still contributes its density to D. */
static bool lodgenGrassTintResolve( const QString & model, const QString & dataRoot,
	void * user, float * rgb )
{
	LodgenBakeCaches & c = *static_cast<LodgenBakeCaches *>( user );
	const QString key = model.toLower();
	auto cached = c.grassTint.constFind( key );
	if ( cached != c.grassTint.constEnd() ) {
		if ( !cached->first )
			return false;
		for ( int k = 0; k < 3; k++ )
			rgb[k] = cached->second[k];
		return true;
	}

	bool ok = false;
	FloatVector4 tint( 0.0f, 0.0f, 0.0f, 0.0f );
	QString path = model;
	path.replace( QChar( '\\' ), QChar( '/' ) );
	if ( !path.startsWith( QStringLiteral( "meshes/" ), Qt::CaseInsensitive ) )
		path.prepend( QStringLiteral( "meshes/" ) );
	QByteArray bytes;
	QBuffer dev( &bytes );
	NifModel src;
	if ( lodgenReadAsset( dataRoot, path, "meshes", ".nif", bytes )
		&& dev.open( QIODevice::ReadOnly )
		&& src.load( dev, path.toLocal8Bit().constData() ) ) {
		// load() leaves the model in its Loading state and every index lookup
		// answers as it does mid-load until this is called
		src.resetState();
		c.nifReads++;
		QString tex0, mat;
		for ( int b = 0; b < src.getBlockCount(); b++ ) {
			const QModelIndex iShape = src.getBlockIndex( b );
			if ( !src.isNiBlock( iShape, "BSTriShape" )
				&& !src.isNiBlock( iShape, "BSMeshLODTriShape" )
				&& !src.isNiBlock( iShape, "BSSubIndexTriShape" ) )
				continue;
			const QModelIndex iShader = src.getBlockIndex(
				src.getLink( iShape, "Shader Property" ) );
			if ( !iShader.isValid() )
				continue;
			mat = src.get<QString>( iShader, "Name" );
			if ( !mat.endsWith( QStringLiteral( ".bgsm" ), Qt::CaseInsensitive ) )
				mat.clear();
			const QModelIndex iTexSet = src.getBlockIndex(
				src.getLink( iShader, "Texture Set" ) );
			if ( iTexSet.isValid() ) {
				const QModelIndex iArr = src.getIndex( iTexSet, "Textures" );
				if ( iArr.isValid() )
					tex0 = src.get<QString>( src.getIndex( iArr, 0 ) );
			}
			break;      // measured: all 71 shipped grass meshes have one shape
		}
		const QString source = !tex0.isEmpty() ? tex0 : mat;
		if ( !source.isEmpty() ) {
			const DDSTexture16 * t = lodgenCachedTexture( c, dataRoot, source );
			if ( t ) {
				const FloatVector4 avg = t->getPixelT( 0.5f, 0.5f,
					float( t->getMaxMipLevel() ) );
				if ( avg[3] >= 0.05f ) {
					for ( int k = 0; k < 3; k++ )
						tint[k] = qBound( 0.0f, avg[k] / avg[3], 1.0f );
					ok = true;
				}
			}
		}
	}
	c.grassTint.insert( key, qMakePair( ok, tint ) );
	if ( !ok )
		return false;
	for ( int k = 0; k < 3; k++ )
		rgb[k] = tint[k];
	return true;
}

/*! ONE HOME for the terrain normal map's height reconstruction, shared by the
 *  per-chunk baker and the virtual-texture tile baker below.
 *
 *  Bilinear over the 128-unit VHGT grid with both blend parameters passed
 *  through the quintic ease `t^3(t(6t-15)+10)`. The full argument is at the
 *  call site in `lodgenBakeTerrainTextures`; in short:
 *
 *   * NEAREST (`int( gx )`) made every texel of a 4x4 block read one grid point,
 *     so a 512 sheet held 129x129 distinct values (2026-09-07);
 *   * the plain bilinear parameter leaves the central difference KINKED at every
 *     height sample, which is a crease every four texels at dim 4 and is the
 *     square lattice bungo saw on the mountain face (2026-09-09);
 *   * the ease reweights the SAME four taps, hits every VHGT sample exactly and
 *     stays between them, so it cannot ring on the 8-unit staircase the way
 *     Catmull-Rom did, and removes nothing (high-frequency energy went UP 25%).
 *
 *  Two callers, one function, because a second copy is exactly how the VT path
 *  kept both 2026-09-07 defects for two days after the chunk path lost them.
 */
static float lodgenTerrainHeightAt( const std::vector<float> & hgt, int hn,
	float gx, float gy )
{
	const float cx = qBound( 0.0f, gx, float( hn - 1 ) - 0.001f );
	const float cy = qBound( 0.0f, gy, float( hn - 1 ) - 0.001f );
	const int ix = int( cx ), iy = int( cy );
	auto ease = []( float t ) {
		return t * t * t * ( t * ( t * 6.0f - 15.0f ) + 10.0f );
	};
	const float tx = ease( cx - float( ix ) ), ty = ease( cy - float( iy ) );
	const float h00 = hgt[size_t( iy ) * hn + ix];
	const float h10 = hgt[size_t( iy ) * hn + ix + 1];
	const float h01 = hgt[size_t( iy + 1 ) * hn + ix];
	const float h11 = hgt[size_t( iy + 1 ) * hn + ix + 1];
	return ( h00 * ( 1.0f - tx ) + h10 * tx ) * ( 1.0f - ty )
		+ ( h01 * ( 1.0f - tx ) + h11 * tx ) * ty;
}

/*! The terrain `_msn` byte order, in one place: R = east, G = UP, B = north.
 *
 *  NOT the conventional (X, Y, Z) -> (R, G, B). Measured on vanilla two
 *  independent ways in 2026-09-07 (the up component is the only one recoverable
 *  as +sqrt(1 - x^2 - y^2) and the only one never below 128 - green wins on all
 *  eight sampled tiles; and predicting each cell's normal from VHGT scores a
 *  mean error of 0.0719 against 0.2401 for the next best orientation). Writing
 *  north in green cost 67.7% of the light and 92.0% of the shading variation on
 *  distant terrain, and there is nothing to fall back on: vanilla's terrain LOD
 *  `.BTR` carries no vertex normals at all, so this sheet is the whole surface
 *  orientation. It also puts up in the 6-bit channel of RGB565.
 */
static quint32 lodgenTerrainMsnPixel( const Vector3 & nrm )
{
	const int nEast = qBound( 0, int( ( nrm[0] * 0.5f + 0.5f ) * 255.0f + 0.5f ), 255 );
	const int nNorth = qBound( 0, int( ( nrm[1] * 0.5f + 0.5f ) * 255.0f + 0.5f ), 255 );
	const int nUp = qBound( 0, int( ( nrm[2] * 0.5f + 0.5f ) * 255.0f + 0.5f ), 255 );
	return 0xFF000000U | quint32( nEast << 16 ) | quint32( nUp << 8 ) | quint32( nNorth );
}

/*! Flat ground in that order: east 0, UP 1, north 0 -> R 128, G 255, B 128.
 *
 *  It was 0xFFFF8080 at all five fill sites, which is the RENDERER's constant
 *  for a flat TANGENT-space normal (`src/gl/renderer.cpp`) - and that buffer is
 *  RGBA-in-a-u32 while these are ARGB, so the value here decoded as east +1,
 *  up 0: a normal pointing sideways, wrong under BOTH channel orders. Every
 *  texel of a baked sheet is overwritten, so it only ever showed where a tile
 *  or a mosaic row was missing.
 */
constexpr quint32 LODGEN_MSN_FLAT = 0xFF80FF80U;

/*! ONE HOME for the plain-bilinear tap into a terrain sample grid.
 *
 *  `f` is an (n x n) grid of samples 128 world units apart, `lx`/`ly` are
 *  world units from the GRID'S OWN south-west corner, and the tap clamps at
 *  the grid edge. Five byte-for-byte copies of these seven lines lived in the
 *  two terrain bakers (each one's AO `heightAt` and channel samplers); they
 *  are one function now, which is the only reason the two paths can be ASKED
 *  for byte identity instead of told they agree.
 *
 *  Deliberately NOT the reconstruction `lodgenTerrainHeightAt` above uses:
 *  that one eases the blend parameter because it feeds a NORMAL map, where a
 *  kink at every height sample shows as a square lattice (2026-09-09). This
 *  one feeds the AO march and the byte channels, where the plain blend is what
 *  every shipped sheet was measured with.
 */
template <typename T>
static float lodgenTerrainGridSample( const std::vector<T> & f, int n,
	float lx, float ly )
{
	const float fx = qBound( 0.0f, lx / 128.0f, float( n - 1 ) );
	const float fy = qBound( 0.0f, ly / 128.0f, float( n - 1 ) );
	const int x0 = int( fx ), y0 = int( fy );
	const int x1 = qMin( x0 + 1, n - 1 ), y1 = qMin( y0 + 1, n - 1 );
	const float tx = fx - float( x0 ), ty = fy - float( y0 );
	const float a = float( f[size_t( y0 ) * n + x0] );
	const float b = float( f[size_t( y0 ) * n + x1] );
	const float c = float( f[size_t( y1 ) * n + x0] );
	const float d = float( f[size_t( y1 ) * n + x1] );
	const float top = a + ( b - a ) * tx, bot = c + ( d - c ) * tx;
	return top + ( bot - top ) * ty;
}

/*! The ring the terrain sheets are baked on: the chunk (or the tile) plus ONE
 *  CELL on every side.
 *
 *  4,096 world units covers both neighbourhood operators the sheets use -- the
 *  normal's one-step central difference (128 units) and the AO march (2,048)
 *  -- so no texel of the chunk itself is ever computed against a clamped edge.
 *  The tile baker has had it since the pyramid was written; the chunk baker
 *  got it on 2026-09-10, which is what made the two paths' sheets the same
 *  bytes rather than the same within a bounded band.
 */
constexpr int LODGEN_TERRAIN_RING_CELLS = 1;
constexpr float LODGEN_TERRAIN_RING_UNITS =
	float( LODGEN_TERRAIN_RING_CELLS ) * 4096.0f;

/*! ONE HOME for filling that ring's height grid.
 *
 *  `fetch( cx, cy )` is given RING-LOCAL cell coordinates (0..rdim-1) and
 *  returns the LAND record there or null; it is the callers' one difference --
 *  the chunk baker reads the plugin directly and keeps the chunk's own cells,
 *  the tile baker goes through its row cache. Both callers lay the ring out the
 *  same way -- `LODGEN_TERRAIN_RING_CELLS` cells of margin on every side of an
 *  inner unit of `rdim - 2 * LODGEN_TERRAIN_RING_CELLS` cells -- so the inner
 *  unit's grid box is derived here rather than passed, and there is exactly one
 *  description of it.
 *
 *  THE CELL OWNS ITS OWN ROWS (bungo, 2026-09-10, verbatim: "The cell owns it
 *  then"). A RING cell fills ONLY the samples BEYOND the inner unit: it never
 *  writes the inner unit's own boundary row or column, so where a neighbour's
 *  copy of a shared VHGT row disagrees with the cell's own, the cell's copy
 *  stays and Bethesda's hairline disagreement is preserved at the seam instead
 *  of being smeared one row into the chunk.
 *
 *  It is a real disagreement and it was measured, not assumed: over the cells
 *  x = -24..-17 the shared row y=31|32 differs by 2, 1, 4, 6, 9, 8, 7 and 4
 *  VHGT units of 8 (16..72 world units) while y=23|24, y=27|28, y=32|33 and
 *  both east seams differ by 0 (lane BUILD4, 2026-09-10). The old south-to-north
 *  order let the y=32 cell overwrite that row inside the y=28..31 chunk, and a
 *  bilinear tap carried it 7 texels in -- past the 4-texel band the normal's
 *  own central difference can reach.
 *
 *  Inside the inner unit the order is unchanged and still part of the contract:
 *  cells are visited south to north then west to east and a later cell
 *  overwrites the VHGT sample it shares with an earlier one, so both callers
 *  resolve an INTERNAL cell edge the same way and the two grids agree exactly
 *  where they overlap. The same convention as the mesh path's chunk-only grid
 *  (`lodgenWriteLandChunk`), which is why that path needs no change.
 *
 *  A cell with no LAND writes nothing, as before: its samples keep `empty`, and
 *  a neighbour no longer reaches in to fill the shared row of a landless inner
 *  cell. That is the same rule -- the cell owns it -- applied to a cell whose
 *  own answer is the worldspace default.
 */
/*! The inner unit a ring fill protects, as a CLOSED GRID BOX in the caller's
 *  own grid coordinates, so that it can name a WORLD rectangle instead of
 *  meaning only "this caller's rdim minus its margin".
 *
 *  Left alone it is the derived box -- the caller's own inner unit -- which is
 *  byte for byte the rule as it stood before lane VT1, and what the chunk baker
 *  and the self-test both want.
 *
 *  WHY IT HAS TO BE SAYABLE (lane VT1, 2026-09-16). The ring's OWN samples are
 *  filled later-wins, so a sample OUTSIDE the inner unit is the north (or east)
 *  cell's copy of a shared VHGT row, while the same sample INSIDE it is the
 *  cell's own. Two bakers with different inner units therefore disagree about
 *  the same world sample. The chunk baker protects a dim-D chunk and the tile
 *  baker protected a dim-D/2 tile, so the chunk sheets assembled from the
 *  pyramid differed from a direct bake by 4 and 27 bytes on two of the four
 *  chunks of the Sanctuary probe region -- carried by the macro gradient, which
 *  is a Sobel over exactly those ring samples (+-512 world units). The measured
 *  disagreement was one grid row, up to 64 world units. A texel's value must
 *  depend on its WORLD POSITION ONLY, so the tile baker now names the CHUNK it
 *  will be assembled into rather than itself. */
struct LodgenRingInner
{
	bool given = false;                  //!< false == derive the box from rdim
	int loX = 0, hiX = 0, loY = 0, hiY = 0;
};

template <typename Fetch>
static void lodgenTerrainFillRing( std::vector<float> & hgt, int hn, int rdim,
	float empty, Fetch fetch, LodgenRingInner inner = LodgenRingInner() )
{
	// the inner unit's CLOSED grid box, [lo..hi] on both axes; a caller with no
	// ring at all (rdim <= 2*RC) has no inner unit to protect and keeps the
	// plain later-wins fill
	bool haveInner = ( rdim > 2 * LODGEN_TERRAIN_RING_CELLS );
	int loX = 32 * LODGEN_TERRAIN_RING_CELLS, hiX = hn - 1 - loX;
	int loY = loX, hiY = hiX;
	if ( inner.given ) {
		// clipped to this grid: a box edge off the grid simply never bites
		loX = qBound( 0, inner.loX, hn - 1 );
		hiX = qBound( 0, inner.hiX, hn - 1 );
		loY = qBound( 0, inner.loY, hn - 1 );
		hiY = qBound( 0, inner.hiY, hn - 1 );
		haveInner = true;
	}
	hgt.assign( size_t( hn ) * hn, empty );
	for ( int cy = 0; cy < rdim; cy++ ) {
		for ( int cx = 0; cx < rdim; cx++ ) {
			const EsmLand * land = fetch( cx, cy );
			if ( !land )
				continue;
			/* A cell is an INNER cell when its own 33x33 block sits WHOLLY
			 * inside the box. Under the derived box that is exactly the old
			 * margin test, cx in [RC, rdim-1-RC], and it is written this one
			 * way so that there is no second rule to keep in step. */
			const bool ringCell = haveInner
				&& !( cx * 32 >= loX && cx * 32 + 32 <= hiX
					&& cy * 32 >= loY && cy * 32 + 32 <= hiY );
			for ( int row = 0; row < 33; row++ ) {
				const int gr = cy * 32 + row;
				const bool rowInside = ( gr >= loY && gr <= hiY );
				for ( int col = 0; col < 33; col++ ) {
					const int gc = cx * 32 + col;
					if ( ringCell && rowInside && gc >= loX && gc <= hiX )
						continue;   // the inner unit's own sample: it owns it
					hgt[size_t( gr ) * hn + size_t( gc )] =
						land->heights[row][col];
				}
			}
		}
	}
}

/*! The known-answer control under that rule, run once per process when
 *  `WW_TERRAIN_RING_TEST` is set in the environment.
 *
 *  A synthetic pair of cells whose shared VHGT row DISAGREES by a known amount
 *  -- 72 world units, the 9-unit worst case measured on the y=31|32 seam -- is
 *  filled through the shipped `lodgenTerrainFillRing`, and every sample of the
 *  inner unit's boundary row and column is asserted to be the inner cell's own
 *  value, exactly, while the samples one step beyond are asserted to be the
 *  neighbour's (so a filler that simply stopped filling the ring would fail).
 *  The bilinear tap the sheets actually read is asserted at the same place, so
 *  the claim is about a texel's operand and not only about a grid cell.
 *
 *  THE REFUTER: the same synthetic pair filled by the OLD south-to-north order,
 *  reproduced verbatim below as a control. It must give the NEIGHBOUR's value
 *  on the inner boundary row -- that is, it must FAIL the bar above. If it
 *  passes, the bar is not discriminating and the self-test says so and fails.
 */
static bool lodgenTerrainRingSelfTest()
{
	constexpr int RC = LODGEN_TERRAIN_RING_CELLS;
	constexpr int rdim = 1 + 2 * RC;            // one inner cell, one ring
	constexpr int hn = rdim * 32 + 1;
	const int innerLo = 32 * RC, innerHi = hn - 1 - innerLo;
	const float A = 1024.0f;                    // the inner cell's own row
	const float B = 1096.0f;                    // the neighbours', 72 units apart

	EsmLand inner, north, east;
	for ( int r = 0; r < 33; r++ )
		for ( int c = 0; c < 33; c++ ) {
			inner.heights[r][c] = A;
			north.heights[r][c] = B;
			east.heights[r][c] = B;
		}
	auto fetch = [&]( int cx, int cy ) -> const EsmLand * {
		if ( cx == RC && cy == RC )
			return &inner;
		if ( cx == RC && cy == RC + 1 )
			return &north;
		if ( cx == RC + 1 && cy == RC )
			return &east;
		return nullptr;
	};

	std::vector<float> hgt;
	lodgenTerrainFillRing( hgt, hn, rdim, 0.0f, fetch );

	int checks = 0, bad = 0;
	auto expect = [&]( const char * what, float got, float want ) {
		checks++;
		if ( got != want ) {
			bad++;
			fprintf( stderr, "ring:   FAIL %s = %.3f, expected %.3f\n", what, got, want );
		} else {
			fprintf( stderr, "ring:   ok   %s = %.3f\n", what, got );
		}
	};
	auto worst = [&]( int r0, int r1, int c0, int c1, float want ) {
		float far = want;
		for ( int r = r0; r <= r1; r++ )
			for ( int c = c0; c <= c1; c++ ) {
				const float v = hgt[size_t( r ) * hn + size_t( c )];
				if ( qAbs( v - want ) > qAbs( far - want ) )
					far = v;
			}
		return far;
	};

	fprintf( stderr, "ring: self-test the cell owns its own boundary rows"
		" (WW_TERRAIN_RING_TEST)\n" );
	fprintf( stderr, "ring:   synthetic pair, inner %.3f, north and east neighbour"
		" %.3f, shared rows disagree by %.3f world units\n", A, B, B - A );
	expect( "the inner unit's NORTH boundary row, every sample",
		worst( innerHi, innerHi, innerLo, innerHi, A ), A );
	expect( "the inner unit's EAST boundary column, every sample",
		worst( innerLo, innerHi, innerHi, innerHi, A ), A );
	expect( "the inner unit's SOUTH boundary row, every sample",
		worst( innerLo, innerLo, innerLo, innerHi, A ), A );
	expect( "the inner unit's WEST boundary column, every sample",
		worst( innerLo, innerHi, innerLo, innerLo, A ), A );
	expect( "one grid step BEYOND the north border, the neighbour's",
		worst( innerHi + 1, hn - 1, innerLo, innerHi, B ), B );
	expect( "one grid step BEYOND the east border, the neighbour's",
		worst( innerLo, innerHi, innerHi + 1, hn - 1, B ), B );
	// the texel's own operand, not just the grid: the shared tap at the border
	expect( "the bilinear tap ON the north border",
		lodgenTerrainGridSample( hgt, hn, float( innerLo + 16 ) * 128.0f,
			float( innerHi ) * 128.0f ), A );
	expect( "the bilinear tap half a step beyond it",
		lodgenTerrainGridSample( hgt, hn, float( innerLo + 16 ) * 128.0f,
			float( innerHi ) * 128.0f + 64.0f ), ( A + B ) * 0.5f );

	/* THE CONTROL: the fill order this replaced, reproduced verbatim. */
	std::vector<float> old( size_t( hn ) * hn, 0.0f );
	for ( int cy = 0; cy < rdim; cy++ )
		for ( int cx = 0; cx < rdim; cx++ ) {
			const EsmLand * land = fetch( cx, cy );
			if ( !land )
				continue;
			for ( int row = 0; row < 33; row++ )
				for ( int col = 0; col < 33; col++ )
					old[size_t( cy * 32 + row ) * hn + size_t( cx * 32 + col )] =
						land->heights[row][col];
		}
	const float oldNorth = old[size_t( innerHi ) * hn + size_t( innerLo + 16 )];
	const float oldEast = old[size_t( innerLo + 16 ) * hn + size_t( innerHi )];
	checks++;
	if ( oldNorth == A || oldEast == A ) {
		bad++;
		fprintf( stderr, "ring:   FAIL CONTROL the old south-to-north order gives"
			" %.3f / %.3f on the inner boundary, which the bar above ACCEPTS:"
			" the bar does not discriminate\n", oldNorth, oldEast );
	} else {
		fprintf( stderr, "ring:   ok   CONTROL the old south-to-north order gives"
			" %.3f north and %.3f east on the inner boundary, which the bar above"
			" REFUSES\n", oldNorth, oldEast );
	}
	/* PART TWO (lane VT1, 2026-09-16): the WORLD-ANCHORED inner box, with the
	 * known-answer control that the box is read at all.
	 *
	 * The shape is the real defect at the smallest size that carries it: a 2x2
	 * "tile" grid whose EAST ring column lies inside the chunk that tile will
	 * be assembled into. Cell (3,2) is that column's southern cell and holds A;
	 * cell (3,3) is its northern neighbour and holds B, and the two disagree on
	 * the row they share exactly as Bethesda's cells do on y=31|32.
	 *
	 *   derived box  -> that row is OUTSIDE the inner unit, later-wins, B
	 *   chunk box    -> that row is INSIDE it, the cell owns it, A
	 *
	 * so a build that ignored the box, or clipped it away, answers B twice and
	 * fails here. */
	{
		constexpr int trdim = 2 + 2 * RC;
		constexpr int thn = trdim * 32 + 1;
		EsmLand tsouth, tnorth;
		for ( int r = 0; r < 33; r++ )
			for ( int c = 0; c < 33; c++ ) {
				tsouth.heights[r][c] = A;
				tnorth.heights[r][c] = B;
			}
		auto tfetch = [&]( int cx, int cy ) -> const EsmLand * {
			if ( cx == trdim - 1 && cy == trdim - 2 )
				return &tsouth;
			if ( cx == trdim - 1 && cy == trdim - 1 )
				return &tnorth;
			return nullptr;
		};
		const size_t at = size_t( 32 * ( trdim - 1 ) ) * thn
			+ size_t( 32 * ( trdim - 1 ) + 16 );
		std::vector<float> derived, chunkBox, namedSame;
		lodgenTerrainFillRing( derived, thn, trdim, 0.0f, tfetch );
		LodgenRingInner box;
		box.given = true;
		box.loX = 32 * RC;
		box.hiX = 32 * trdim;                 // one cell further EAST: the chunk
		box.loY = 32 * RC;
		box.hiY = thn - 1 - 32 * RC;
		lodgenTerrainFillRing( chunkBox, thn, trdim, 0.0f, tfetch, box );
		expect( "VT1 derived box: the shared row in the east ring column is the"
			" NEIGHBOUR's", derived[at], B );
		expect( "VT1 chunk box: the same world sample is the cell's OWN",
			chunkBox[at], A );
		checks++;
		if ( derived[at] == chunkBox[at] ) {
			bad++;
			fprintf( stderr, "ring:   FAIL CONTROL the box changes nothing: the"
				" two fills agree on that sample, so the bar cannot fail\n" );
		} else {
			fprintf( stderr, "ring:   ok   CONTROL the box moves that sample by"
				" %.3f world units\n", qAbs( derived[at] - chunkBox[at] ) );
		}
		/* And the reformulated ring test is EXACTLY the old margin test when the
		 * box named IS the derived one -- same bytes, over the whole grid. */
		LodgenRingInner same;
		same.given = true;
		same.loX = 32 * RC;
		same.hiX = thn - 1 - 32 * RC;
		same.loY = same.loX;
		same.hiY = same.hiX;
		lodgenTerrainFillRing( namedSame, thn, trdim, 0.0f, tfetch, same );
		checks++;
		if ( derived == namedSame ) {
			fprintf( stderr, "ring:   ok   naming the DERIVED box changes not one"
				" of %d samples\n", int( derived.size() ) );
		} else {
			bad++;
			fprintf( stderr, "ring:   FAIL naming the derived box changed the"
				" grid\n" );
		}
	}

	fprintf( stderr, "ring: self-test %d checks, %d failures, %s\n", checks, bad,
		bad ? "RESULT FAIL" : "RESULT PASS" );
	return bad == 0;
}

/*! Runs it once per process, and only when asked. */
static void lodgenTerrainRingSelfTestOnce()
{
	static bool done = false;
	if ( done || qgetenv( "WW_TERRAIN_RING_TEST" ).isEmpty() )
		return;
	done = true;
	lodgenTerrainRingSelfTest();
}

/* The landscape tiling, set once by the CLI before the bake starts and read
 * from every sampling site afterwards. 341.3333 = 128 / 0.375 is the engine's
 * own number (see lodgen.h); 2048 is the pre-2026-09-11 bake, exactly.
 * A non-positive value is refused and leaves the default standing, so a
 * mistyped switch cannot silently flatten the ground to one texel. */
static float g_landTiling = 341.3333f;

float lodgenLandTiling()
{
	return g_landTiling;
}

void lodgenSetLandTiling( float unitsPerRepeat )
{
	if ( unitsPerRepeat > 0.0f )
		g_landTiling = unitsPerRepeat;
}

/* Lane TILING2. Both default to the 2026-09-11 bake exactly: `footprint`
 * sampling, full detail, no edge blend. See lodgen.h for the measurements. */
static bool  g_landSampleAverage = false;
static float g_landDetail        = 0.0f;
static int   g_blendEdges        = 0;
static float g_blendMargin       = 128.0f;

bool lodgenLandSampleAverage()
{
	return g_landSampleAverage;
}

void lodgenSetLandSampleAverage( bool average )
{
	g_landSampleAverage = average;
}

float lodgenLandDetail()
{
	return g_landDetail;
}

void lodgenSetLandDetail( float strength )
{
	/* Clamped, not refused: 0 and 1 are both meaningful ends and anything
	 * outside them is a typo, not a request. */
	g_landDetail = strength < 0.0f ? 0.0f : ( strength > 1.0f ? 1.0f : strength );
}

int lodgenBlendEdges()
{
	return g_blendEdges;
}

void lodgenSetBlendEdges( int mode )
{
	g_blendEdges = mode;
}

float lodgenBlendMargin()
{
	return g_blendMargin;
}

void lodgenSetBlendMargin( float units )
{
	if ( units > 0.0f )
		g_blendMargin = units;
}

/* --- THE STOCHASTIC-PHASE LAND SAMPLE (lane TILING3) ------------------------
 *
 * See lodgen.h for what this is for and what it was measured at.
 *
 * THE DEFAULTS MOVED ON 2026-09-12 (lane DEFAULTS1). bungo picked panel (c) of
 * `a_land_guide_*.png` -- "C looks good, it's good if it's configurable in the
 * UI" -- and that panel was baked with
 *   --land-sample stochastic --land-hex 256 --land-mip-bias -0.22
 *   --land-guide flatwarp:1.0 --land-warp 341
 * so the amplitude is 341 and the bias -0.22 here, the hex size is 256 below,
 * and the guide rule is FLATWARP at strength 1. A zero amplitude and a zero
 * bias still leave the warp and the bias expressions unevaluated at both
 * sampling sites, so
 *   --land-hex 0 --land-warp 0 --land-mip-bias 0 --land-guide off
 * is the exact way back to the 2026-09-11 bake, byte for byte.
 *
 * The lattice and the octave count carry the values lane TILING3 picked; they
 * did not move. */
static float g_landWarpAmp     = 341.0f;      // world units; 0 == off
static float g_landWarpLattice = 1024.0f;
static int   g_landWarpOctaves = 1;
static float g_landMipBias     = -0.22f;      // mips; 0 == off

/*! The integer hash the warp is built on: uint32 in, uint32 out.
 *
 *  It is written as the same five lines as the offline prototype
 *  (scratchpad/tiling3_20260911/a4_warp.py, `_hash01`) so the two can be
 *  compared term by term, and it uses only wrapping unsigned arithmetic, which
 *  is exactly defined in C++ -- no undefined overflow, no compiler freedom, the
 *  same answer on every target. */
static inline quint32 lodgenWarpHash( qint32 i, qint32 j, quint32 k )
{
	quint32 h = quint32( i ) * 374761393u
		+ quint32( j ) * 668265263u
		+ k * 2246822519u;
	h ^= h >> 13;
	h *= 1274126177u;
	h ^= h >> 16;
	return h;
}

/*! One octave of smoothstep-interpolated value noise on a lattice of `lattice`
 *  world units, returning a pair of offsets in [-1,+1].
 *
 *  In double, deliberately: the lattice index comes from a floor of a world
 *  coordinate that reaches +-2,000,000 units, and float carries only 24 bits of
 *  mantissa, so the interpolant `fx` would quantise to visible steps at the far
 *  edge of the worldspace. Nothing here reads any state, so it is a pure
 *  function and thread-count cannot reach it. */
static void lodgenLandWarpOffsets( double wx, double wy, double lattice,
                                   double * ox, double * oy )
{
	const double gx = wx / lattice;
	const double gy = wy / lattice;
	const double fi = std::floor( gx );
	const double fj = std::floor( gy );
	const qint32 i = qint32( fi );
	const qint32 j = qint32( fj );
	const double fx = gx - fi;
	const double fy = gy - fj;
	const double sx = fx * fx * ( 3.0 - 2.0 * fx );
	const double sy = fy * fy * ( 3.0 - 2.0 * fy );
	double out[2] = { 0.0, 0.0 };
	for ( quint32 k = 0; k < 2; k++ ) {
		const double a = double( lodgenWarpHash( i,     j,     k ) ) / 4294967296.0;
		const double b = double( lodgenWarpHash( i + 1, j,     k ) ) / 4294967296.0;
		const double c = double( lodgenWarpHash( i,     j + 1, k ) ) / 4294967296.0;
		const double d = double( lodgenWarpHash( i + 1, j + 1, k ) ) / 4294967296.0;
		const double v = ( a * ( 1.0 - sx ) + b * sx ) * ( 1.0 - sy )
			+ ( c * ( 1.0 - sx ) + d * sx ) * sy;
		out[k] = v * 2.0 - 1.0;
	}
	*ox = out[0];
	*oy = out[1];
}

/*! The warp at an EXPLICIT amplitude.
 *
 *  Lane LAND1 lifted the amplitude out of the global so the terrain-guided
 *  rules below can MODULATE it by the macro slope without a second copy of
 *  the octave loop.  `lodgenLandWarp` is this function at the configured
 *  amplitude, term for term, so the shipped behaviour is unchanged and the
 *  default is still a return. */
static void lodgenLandWarpAt( float wx, float wy, double amp,
                              float * wxOut, float * wyOut )
{
	/* OFF IS A RETURN, not a multiply by zero: the rung's bytes are reached by
	 * not touching the coordinate at all. */
	if ( amp <= 0.0 || g_landWarpOctaves < 1 ) {
		*wxOut = wx;
		*wyOut = wy;
		return;
	}
	double ox = 0.0, oy = 0.0;
	double a = amp;
	double l = double( g_landWarpLattice );
	for ( int k = 0; k < g_landWarpOctaves; k++ ) {
		/* Each octave is offset by a fixed irrational-ish stride so the octaves
		 * do not share lattice lines; the same two constants as the prototype's
		 * `warp2`. */
		double dx = 0.0, dy = 0.0;
		lodgenLandWarpOffsets( double( wx ) + double( k ) * 9137.0,
		                       double( wy ) - double( k ) * 4271.0, l, &dx, &dy );
		ox += a * dx;
		oy += a * dy;
		a *= 0.5;
		l *= 0.5;
	}
	*wxOut = float( double( wx ) + ox );
	*wyOut = float( double( wy ) + oy );
}

void lodgenLandWarp( float wx, float wy, float * wxOut, float * wyOut )
{
	lodgenLandWarpAt( wx, wy, double( g_landWarpAmp ), wxOut, wyOut );
}

float lodgenLandWarpAmp()
{
	return g_landWarpAmp;
}

void lodgenSetLandWarpAmp( float units )
{
	/* Clamped at zero, not refused: a negative amplitude is a typo, and the
	 * meaningful floor of this control is "off". */
	g_landWarpAmp = units > 0.0f ? units : 0.0f;
}

float lodgenLandWarpLattice()
{
	return g_landWarpLattice;
}

void lodgenSetLandWarpLattice( float units )
{
	/* Refused, not clamped: a zero or negative lattice would divide by zero and
	 * a mistyped switch must not be able to do that. The default stands. */
	if ( units > 0.0f )
		g_landWarpLattice = units;
}

int lodgenLandWarpOctaves()
{
	return g_landWarpOctaves;
}

void lodgenSetLandWarpOctaves( int octaves )
{
	/* Bounded above as well: each octave halves the lattice, so past about six
	 * the lattice is finer than one bake texel and the warp stops being smooth. */
	if ( octaves >= 1 && octaves <= 6 )
		g_landWarpOctaves = octaves;
}

/* --- TERRAIN-GUIDED LAND SAMPLING (lane LAND1, bungo 2026-09-12) -----------
 *
 * His words: "since we're reusing vanilla terain normals and slope maps, might
 * as well use them to guide this a bit", after asking "what is used for the
 * land sample warp? the normal or slope map?" -- the answer being neither: a
 * hashed value-noise lattice on world X/Y, which knows nothing about the ground
 * it is decorating.
 *
 * THE INPUT IS THE HEIGHTMAP, NOT THE SHEET.  The macro gradient below is a
 * Sobel difference of the SAME ring height grid the `_msn` normal a few lines
 * above the land lookup is built from, at a half-step of `--land-guide-scale`/2
 * world units.  Reading it off the heightmap rather than off vanilla's `_msn`
 * sheet is what makes it continuous across a chunk, a tile and a region border:
 * the grid is filled over a ONE-CELL ring (4,096 units) by the shared filler,
 * the deepest stencil this file asks of it reaches scale/2 + the tile's own
 * border (256 units at the shipped geometry), and nothing inside a tile can
 * therefore reach the grid's clamped edge.  Two adjacent chunks baked apart and
 * baked together are the same bytes because every term is a pure function of
 * WORLD position.
 *
 * FIVE RULES, each a switch, all off by default and all off by RETURN:
 *
 *   drag       the sample slides DOWNHILL by k * the macro normal's xy, so the
 *              texture lags the slope.  A smooth field, so it has strain, and
 *              the strain is what the swirl instrument (lane TILING4) reads.
 *   aspect     the sampling frame is ROTATED by the downhill azimuth, about the
 *              centre of the macro lattice cell the texel is in, blended toward
 *              identity by the macro slope.  A blend of the identity and a
 *              rotation about one anchor is a SIMILARITY -- uniform scale and
 *              rotation, zero shear -- so inside a cell the strain is
 *              isotropic and the orientation instrument cannot see it.  The
 *              price is a discontinuity at the lattice lines, and it is a real
 *              one: it is what the gates have to price.
 *   aspecthex  the same rotation, but carried by the HEX lattice's three taps
 *              instead of a square one: each of `lodgenLandHexTap`'s vertices
 *              rotates the plane about ITSELF by the macro azimuth there, and
 *              the barycentric variance-preserving blend that already joins the
 *              three offsets joins the three rotations.  Seamless AND
 *              shear-free by construction.  Needs `--land-hex`.
 *   slopewarp  TILING3's hash warp with its amplitude scaled by the macro
 *              slope -- weak on the flats, full on the slopes.
 *   flatwarp   the same, the other way round.  Both exist because which
 *              direction helps is a measurement, not an opinion.
 *
 * WHY THE ROTATION IS NOT WEIGHTED ON THE ANGLE.  `atan2` has a branch cut at
 * due west; multiplying the ANGLE by a weight below 1 turns that cut into a
 * visible discontinuity of up to 2*pi*weight.  The weight is applied to the
 * MAP instead -- p + w * ( R(p) - p ) -- which is continuous across the cut
 * because cos and sin are, and which stays shear-free (it is (1-w)I + wR, a
 * complex number times the plane).
 *
 * The rule NUMBERS live in lodgen.h beside the declarations, each with its own
 * one-line description; there is no second copy of them here.
 */
/* FLATWARP at strength 1 since 2026-09-12 (lane DEFAULTS1): bungo's pick,
 * panel (c) of `a_land_guide_*.png`. `--land-guide off` is the way back, and
 * with it the guide branch is not evaluated at either sampling site. The scale
 * and the slope reference did not move. */
static int   g_landGuideRule     = LODGEN_LANDGUIDE_FLATWARP;
static float g_landGuideStrength = 1.0f;
static float g_landGuideScale    = 1024.0f;   // world units
static float g_landGuideSlopeRef = 0.5f;      // tangent; 0.5 == 26.6 degrees

/*! Everything the guide needs to ask the ring height grid a question at a
 *  WORLD position: the grid, its size, and the affine map from world units
 *  to the grid coordinates both bakers already compute for the normal.
 *
 *  `ngx = wx / 128 + ngOffX`.  The chunk baker's offset is
 *  ( RING_UNITS - chunkWorldX ) / 128 and the tile baker's is -ringWestX /
 *  128; that one line is the whole difference between the two sites, which
 *  is why the rules themselves need no second copy. */
struct LodgenLandGuideCtx
{
	const std::vector<float> * hgt = nullptr;
	int hn = 0;
	float ngOffX = 0.0f;
	float ngOffY = 0.0f;
};

/*! The terrain's LOW-PASS gradient at the guide scale, at a world position.
 *
 *  A Sobel 3x3 over the ring height grid at a half-step of scale/2 world
 *  units: eight taps, and the perpendicular smoothing is what keeps a single
 *  128-unit VHGT step out of the answer.  Returns dz/dx and dz/dy as a
 *  TANGENT -- 1.0 is 45 degrees -- so DOWNHILL is ( -dzdx, -dzdy ), which is
 *  the surface normal's own xy up to its length.
 *
 *  In double for the same reason the warp and the hex lattice are: the grid
 *  coordinate comes from a world coordinate that reaches +-2,000,000 units. */
static void lodgenLandMacroGradient( const LodgenLandGuideCtx & ctx,
                                     double wx, double wy,
                                     double * dzdx, double * dzdy )
{
	*dzdx = 0.0;
	*dzdy = 0.0;
	if ( !ctx.hgt || ctx.hn < 2 )
		return;
	const double s = double( g_landGuideScale ) * 0.5;      // world units
	const double gs = s / 128.0;                            // grid steps
	const double cx = wx / 128.0 + double( ctx.ngOffX );
	const double cy = wy / 128.0 + double( ctx.ngOffY );
	double h[3][3];
	for ( int j = -1; j <= 1; j++ )
		for ( int i = -1; i <= 1; i++ )
			h[j + 1][i + 1] = double( lodgenTerrainHeightAt( *ctx.hgt, ctx.hn,
				float( cx + double( i ) * gs ), float( cy + double( j ) * gs ) ) );
	const double gx = ( h[0][2] + 2.0 * h[1][2] + h[2][2] )
		- ( h[0][0] + 2.0 * h[1][0] + h[2][0] );
	const double gy = ( h[2][0] + 2.0 * h[2][1] + h[2][2] )
		- ( h[0][0] + 2.0 * h[0][1] + h[0][2] );
	*dzdx = gx / ( 8.0 * s );
	*dzdy = gy / ( 8.0 * s );
}

/*! The macro slope's weight, 0 on the flat and 1 at the reference slope. */
static inline double lodgenLandGuideWeight( double tangent )
{
	const double ref = double( g_landGuideSlopeRef ) > 1e-6
		? double( g_landGuideSlopeRef ) : 1e-6;
	const double w = tangent / ref;
	return w >= 1.0 ? 1.0 : w;
}

/*! The guided rotation of one point about one anchor.
 *
 *  `( ax, ay )` is the anchor the rotation is rigid about; the azimuth and
 *  the weight are read at the ANCHOR, not at the point, which is what makes
 *  the map inside one lattice cell a similarity rather than a shear. */
static void lodgenLandGuideRotate( const LodgenLandGuideCtx & ctx,
                                   double ax, double ay,
                                   double * px, double * py )
{
	double dzdx = 0.0, dzdy = 0.0;
	lodgenLandMacroGradient( ctx, ax, ay, &dzdx, &dzdy );
	const double gx = -dzdx, gy = -dzdy;                    // downhill
	const double t = std::sqrt( gx * gx + gy * gy );
	if ( t <= 1e-9 )
		return;
	double w = lodgenLandGuideWeight( t ) * double( g_landGuideStrength );
	if ( w <= 0.0 )
		return;
	if ( w > 1.0 )
		w = 1.0;
	/* cos/sin of the RAW azimuth: continuous across atan2's branch cut,
	 * which weighting the angle instead would have turned into a seam. */
	const double inv = 1.0 / t;
	const double cs = gx * inv, sn = gy * inv;
	const double rx = *px - ax, ry = *py - ay;
	const double qx = ax + rx * cs - ry * sn;
	const double qy = ay + rx * sn + ry * cs;
	*px += w * ( qx - *px );
	*py += w * ( qy - *py );
}

/*! The land diffuse lookup's coordinate: the terrain-guided rules, then the
 *  hash warp, exactly as the two lines this replaces did.
 *
 *  OFF IS A RETURN twice over: with no rule this is `lodgenLandWarp`, which
 *  at amplitude 0 does not touch the coordinate -- so the default bake at
 *  both sampling sites is the rung's bytes. */
static void lodgenLandGuidedWarp( const LodgenLandGuideCtx & ctx,
                                  float wx, float wy, float mdzdx, float mdzdy,
                                  float * wxOut, float * wyOut )
{
	if ( g_landGuideRule == LODGEN_LANDGUIDE_OFF ) {
		lodgenLandWarp( wx, wy, wxOut, wyOut );
		return;
	}
	const double gx = -double( mdzdx ), gy = -double( mdzdy );   // downhill
	const double t = std::sqrt( gx * gx + gy * gy );
	double sx = double( wx ), sy = double( wy );
	double amp = double( g_landWarpAmp );
	switch ( g_landGuideRule ) {
	case LODGEN_LANDGUIDE_DRAG: {
		/* the macro NORMAL's xy, which is the downhill unit vector times the
		 * sine of the slope angle -- k is therefore world units at a vertical
		 * face and about k * tangent on gentle ground, and it is bounded. */
		const double len = std::sqrt( 1.0 + t * t );
		sx += double( g_landGuideStrength ) * gx / len;
		sy += double( g_landGuideStrength ) * gy / len;
		break;
	}
	case LODGEN_LANDGUIDE_ASPECT: {
		/* the macro lattice cell CENTRE, anchored on world coordinates and
		 * not on the tile, or two tiles would disagree at their border */
		const double L = double( g_landGuideScale ) > 1.0
			? double( g_landGuideScale ) : 1.0;
		const double ax = ( std::floor( sx / L ) + 0.5 ) * L;
		const double ay = ( std::floor( sy / L ) + 0.5 ) * L;
		lodgenLandGuideRotate( ctx, ax, ay, &sx, &sy );
		break;
	}
	case LODGEN_LANDGUIDE_ASPECTHEX:
		/* carried by the hex tap, per lattice vertex; nothing to do to the
		 * coordinate here */
		break;
	case LODGEN_LANDGUIDE_SLOPEWARP:
		amp *= lodgenLandGuideWeight( t ) * double( g_landGuideStrength );
		break;
	case LODGEN_LANDGUIDE_FLATWARP:
		amp *= ( 1.0 - lodgenLandGuideWeight( t ) )
			* double( g_landGuideStrength );
		break;
	default:
		break;
	}
	lodgenLandWarpAt( float( sx ), float( sy ), amp, wxOut, wyOut );
}

int lodgenLandGuideRule()
{
	return g_landGuideRule;
}

void lodgenSetLandGuideRule( int rule )
{
	/* Refused, not clamped: an unknown rule number is a typo and must not
	 * silently become one of the five. */
	if ( rule >= LODGEN_LANDGUIDE_OFF && rule <= LODGEN_LANDGUIDE_FLATWARP )
		g_landGuideRule = rule;
}

float lodgenLandGuideStrength()
{
	return g_landGuideStrength;
}

void lodgenSetLandGuideStrength( float k )
{
	g_landGuideStrength = k;
}

float lodgenLandGuideScale()
{
	return g_landGuideScale;
}

void lodgenSetLandGuideScale( float units )
{
	/* Bounded above by the RING, not by taste: the height grid carries one
	 * cell (4,096 units) outside the tile and the tile's own border eats 256
	 * of it at the shipped geometry, so a Sobel half-step past 2,048 units
	 * would read the grid's clamped edge and the answer would depend on
	 * which tile asked. Below 128 it is finer than the VHGT grid itself. */
	if ( units >= 128.0f && units <= 2048.0f )
		g_landGuideScale = units;
}

float lodgenLandGuideSlopeRef()
{
	return g_landGuideSlopeRef;
}

void lodgenSetLandGuideSlopeRef( float tangent )
{
	if ( tangent > 0.0f )
		g_landGuideSlopeRef = tangent;
}

/* --- THE HISTOGRAM-PRESERVING HEX TILING (lane TILING4) ---------------------
 *
 * See lodgen.h for what this is and what it was measured at.  OFF IS A RETURN:
 * at size 0 `lodgenLandHexTap` evaluates the identical expression the rung
 * compiled, so the default sheet is the same bytes.
 *
 * Heitz & Neyret 2018.  The plane is covered by a triangle lattice of `size`
 * world units; each lattice VERTEX carries one random offset into the texture;
 * a position takes the three offsets of the triangle it falls in and blends
 * them with its barycentric weights, variance-preserved:
 *
 *     result = mean + sum_k w_k (s_k - mean) / sqrt( sum_k w_k^2 )
 *
 * Without that denominator, blending three decorrelated samples of the same
 * texture with weights that sum to one drops the contrast by up to sqrt(1/3) --
 * a soft mottling exactly where the operator is needed.  The offline sweep
 * measured it: the no-varnorm variant reads the swirl instrument HIGHER than
 * the variance-preserved one (2.24 against 1.93 on the worst sheet).
 *
 * WHY THIS RATHER THAN TILING3'S WARP.  A smooth warp removes the repeat only
 * by straining the texture, and the strain IS the swirl bungo saw.  A
 * piecewise-constant offset has a strain of exactly zero away from the lattice
 * edges, so it cannot have that defect by construction -- measured, the swirl
 * gate goes from 2 of 7 sheets (the warp) to 7 of 7 and 7 of 7 (this).
 *
 * THREE PROPERTIES BY CONSTRUCTION, NOT BY TESTING, the same three the warp
 * has: it is a pure function of world position, so there is no seam at any
 * quadrant, cell or chunk line; it reads nothing per-chunk and no evaluation
 * order, so the bake is byte-identical at 1 chunk thread and at 16; and it is a
 * RESAMPLING of the texture, so the grain's spectrum and histogram survive. */
/* 256 world units since 2026-09-12 (lane DEFAULTS1): part of bungo's pick,
 * panel (c) of `a_land_guide_*.png`. `--land-hex 0` is one quarter of the way
 * back; see the four-switch note at g_landWarpAmp. */
static float g_landHexSize = 256.0f;          // world units; 0 == off

/* the skew and scale that turn a unit square lattice into an equilateral
 * triangle one -- 1/sqrt(3) and 2/sqrt(3), spelled out to the same 17 digits as
 * the offline prototype (scratchpad/tiling4_20260912/h_cand.py) */
static const double LODGEN_HEX_SKEW  = 0.57735026918962576;
static const double LODGEN_HEX_SCALE = 1.15470053837925152;

/*! The triangle a world position falls in: its three lattice vertices and the
 *  three barycentric weights.
 *
 *  In double for the same reason the warp is: the lattice index comes from a
 *  floor of a world coordinate that reaches +-2,000,000 units and float carries
 *  24 bits of mantissa, so the weights would quantise at the far edge of the
 *  worldspace.  Nothing here reads any state. */
static void lodgenLandHexCell( double wx, double wy, double size,
                               qint32 * vi, qint32 * vj, double * w )
{
	const double px = wx / size;
	const double py = wy / size;
	const double sx = px - LODGEN_HEX_SKEW * py;
	const double sy = LODGEN_HEX_SCALE * py;
	const double bi = std::floor( sx );
	const double bj = std::floor( sy );
	const double tx = sx - bi;
	const double ty = sy - bj;
	const double tz = 1.0 - tx - ty;
	/* tz > 0 is the "up" triangle of the rhombus; the other half is its mirror,
	 * and the weights below are the prototype's `tri_grid` term for term. */
	const bool up = tz > 0.0;
	const double o = up ? 0.0 : 1.0;
	w[0] = up ? tz : -tz;
	w[1] = up ? ty : 1.0 - ty;
	w[2] = up ? tx : 1.0 - tx;
	vi[0] = qint32( bi + o );        vj[0] = qint32( bj + o );
	vi[1] = qint32( bi + o );        vj[1] = qint32( bj + 1.0 - o );
	vi[2] = qint32( bi + 1.0 - o );  vj[2] = qint32( bj + o );
}

/*! One lattice vertex's offset into the texture, in [0,1) of one repeat.
 *  The SAME hash the warp is built on, so there is one hash in this file. */
static inline double lodgenLandHexOffset( qint32 i, qint32 j, quint32 k )
{
	return double( lodgenWarpHash( i, j, k ) ) / 4294967296.0;
}

/*! The WORLD position of one lattice vertex -- the inverse of the skew in
 *  `lodgenLandHexCell`, spelled out rather than re-derived at the call site
 *  (lane LAND1 needs it to anchor a per-vertex rotation).
 *
 *      sx = px - SKEW * py,  sy = SCALE * py   =>   py = sy / SCALE,
 *      px = sx + SKEW * sy / SCALE,            and world = ( px, py ) * size. */
static inline void lodgenLandHexVertexPos( qint32 i, qint32 j, double size,
                                           double * wx, double * wy )
{
	const double py = double( j ) / LODGEN_HEX_SCALE;
	const double px = double( i ) + LODGEN_HEX_SKEW * py;
	*wx = px * size;
	*wy = py * size;
}

/*! The land diffuse tap: one texel off it when the hex tiling is off, the
 *  three-tap variance-preserving blend when it is on.
 *
 *  `swx`/`swy` are the (possibly warp-offset) world position; `u`/`v` are the
 *  wrapped coordinates the caller already computed from them, so that OFF costs
 *  one branch and returns the caller's own expression unchanged. */
static FloatVector4 lodgenLandHexTap( const DDSTexture16 * tex,
                                      float swx, float swy, float tile,
                                      float u, float v, float mip, float maxMip,
                                      const LodgenLandGuideCtx * guide )
{
	if ( g_landHexSize <= 0.0f )
		return tex->getPixelT( u, v, mip );
	qint32 vi[3], vj[3];
	double w[3];
	lodgenLandHexCell( double( swx ), double( swy ), double( g_landHexSize ),
	                   vi, vj, w );
	/* the texture's own mean over one whole repeat -- its 1x1 mip, the same
	 * value the `average` path reads */
	const FloatVector4 mean = tex->getPixelT( 0.5f, 0.5f, maxMip );
	FloatVector4 acc( 0.0f, 0.0f, 0.0f, 0.0f );
	double wsq = 0.0;
	float bestW = -1.0f;
	float bestA = mean[3];
	for ( int k = 0; k < 3; k++ ) {
		const double ox = lodgenLandHexOffset( vi[k], vj[k], 0 ) * double( tile );
		const double oy = lodgenLandHexOffset( vi[k], vj[k], 1 ) * double( tile );
		/* THE TERRAIN-GUIDED ROTATION (lane LAND1), carried per lattice
		 * VERTEX: each tap rotates the plane about its own vertex by the
		 * macro azimuth measured THERE, so every tap is a similarity and
		 * the shear is exactly zero, and the barycentric blend below joins
		 * the three rotations the same way it already joins the three
		 * offsets -- no seam at a lattice edge.  Off, `px`/`py` are the
		 * caller's own coordinate and the line is what it was. */
		double px = double( swx ), py = double( swy );
		if ( guide && g_landGuideRule == LODGEN_LANDGUIDE_ASPECTHEX ) {
			double vx = 0.0, vy = 0.0;
			lodgenLandHexVertexPos( vi[k], vj[k], double( g_landHexSize ),
				&vx, &vy );
			lodgenLandGuideRotate( *guide, vx, vy, &px, &py );
		}
		/* wrap by hand exactly as the caller does: getPixelT clamps and the
		 * tiling is ours */
		double tu = std::fmod( ( px + ox ) / double( tile ), 1.0 );
		double tv = std::fmod( ( py + oy ) / double( tile ), 1.0 );
		if ( tu < 0.0 ) tu += 1.0;
		if ( tv < 0.0 ) tv += 1.0;
		const FloatVector4 s = tex->getPixelT( float( tu ), float( tv ), mip );
		acc += ( s - mean ) * float( w[k] );
		wsq += w[k] * w[k];
		if ( float( w[k] ) > bestW ) {
			bestW = float( w[k] );
			bestA = s[3];
		}
	}
	if ( wsq > 1e-12 )
		acc *= float( 1.0 / std::sqrt( wsq ) );
	FloatVector4 r = mean + acc;
	/* ALPHA IS NEVER BLENDED.  Weights that sum to one over a variance-
	 * preserving denominator would push a constant 1.0 alpha to about 1.07, and
	 * a land diffuse's alpha is not a colour to be decorrelated -- it is taken
	 * from the tap with the largest weight, which is a tap, not an average. */
	r[3] = bestA;
	return r;
}

float lodgenLandHexSize()
{
	return g_landHexSize;
}

void lodgenSetLandHexSize( float units )
{
	/* Clamped at zero, not refused, like the warp's amplitude: a negative size
	 * is a typo and the meaningful floor of this control is "off". */
	g_landHexSize = units > 0.0f ? units : 0.0f;
}

float lodgenLandMipBias()
{
	return g_landMipBias;
}

void lodgenSetLandMipBias( float bias )
{
	/* Bounded both ways: the sampler clamps to [0,maxMip] anyway, but a bias of
	 * -40 would silently mean "always mip 0" and read as a deliberate setting. */
	g_landMipBias = bias < -8.0f ? -8.0f : ( bias > 8.0f ? 8.0f : bias );
}

/* ==========================================================================
 *  VANILLA FAR-TERRAIN REUSE -- bungo's ruling of 2026-09-11, verbatim:
 *
 *    "so now we do not use our own normal map if that is toggled, but reuse
 *     these ones for terrain chunks."
 *
 *  and, on the out-of-bounds ground:
 *
 *    "out of bounds terrain blends are not included in the actual cells out of
 *     bounds, they never were, so we can't recover the color data anymore,
 *     because it was baked in a different tool outside of fo4."
 *
 *  WHAT THAT MEANS IN BYTES, and the whole of it:
 *
 *   * a chunk that has a shipped vanilla `_msn` gets VANILLA'S FILE, copied
 *     byte for byte.  Not a composite, not a guarded blend -- a copy.  Our own
 *     normal bake is skipped for that chunk.
 *   * a chunk whose cells carry NO land-texture paint at all (the out-of-bounds
 *     ring: heights, no BTXT, no ATXT) has no composite of ours worth shading,
 *     and the colour vanilla ships there was baked outside FO4 from data that
 *     is not in the ESM.  It cannot be recovered, so it is REUSED: vanilla's
 *     colour sheet, byte for byte, on the same rule as the `_msn`.
 *   * every other chunk keeps OUR colour composite and gains vanilla's fine
 *     geology as a CREVICE TERM (below).
 *   * a chunk with no vanilla sheet keeps the rung behaviour exactly.
 *
 *  THE GUARD IS EXISTENCE, and nothing else.  There is no similarity test, no
 *  threshold, no "agree within N": the ruling says copy, so the only question
 *  a chunk can be asked is whether the file is there.  A guard with a number in
 *  it would be a second policy nobody asked for and a second thing to be wrong.
 *
 *  PROVENANCE.  The vanilla sheets are read as LOOSE FILES under an explicit
 *  root (`--vanilla-lod-root`, default E:/Tools/Fallout 4/DataUnpacked/Data),
 *  never through lodgenReadAsset and never through the resource stack.  That is
 *  deliberate and it is the point: the stack would happily serve OUR OWN
 *  previously installed output out of the game's own Data\Textures\Terrain, and
 *  the bake would then "reuse vanilla" by copying yesterday's copy of itself.
 *  A loose read under a named root cannot do that.
 * ========================================================================== */

static QString g_vanillaLodRoot =
	QStringLiteral( "E:/Tools/Fallout 4/DataUnpacked/Data" );
static int g_landDetailSource = LODGEN_LANDDETAIL_VANILLA;
/*! The crevice coefficient, in 8-bit luminance levels per unit of detail-normal
 *  divergence, fitted on seven vanilla sheets by
 *  scratchpad/tiling3_20260911/d3_shade.py: median -3.242, the same sign on 7
 *  of 7 sheets, beating its own phase twin on 7 of 7.  The Lambert form the
 *  ruling named reads ZERO at the twin floor with signs that flip sheet to
 *  sheet, which is why this is a curvature term and not a light. */
static float g_landShade = -3.242f;
/*! The colour grade, applied at the per-texel write in BOTH writers. Lane
 *  GRADE1, 2026-09-12: ours -> vanilla is NOT a constant gain, a gamma or an
 *  sRGB slip. The best gain per tile runs 0.615..1.241 over a 25-tile census
 *  (mean 0.892, sd 0.144) and its SIGN flips between the two reference tiles --
 *  (-20,24) wants 0.892, (-20,20) wants 1.118 -- so every k != 1 raises the
 *  error on one of them. 1.0 is therefore the default and the knob exists to
 *  answer "what would k do", not because a k was found. The pooled optimum over
 *  the census is 0.840 (RGB RMS 24.72 -> 19.64, better on 19 of 25 tiles). */
static float g_landGrade = 1.0f;
/*! The sheet format, and the cleaned-`_msn` cache directory. Both default to
 *  OFF and both are off by construction, not by argument: at LEGACY the writer
 *  is called with the arguments it was called with before, and with an empty
 *  cache directory no directory is read. See src/lodgen.h for what `vanilla`
 *  means and for the corpus measurement it was taken from. */
static int g_sheetFormat = LODGEN_SHEETFMT_LEGACY;
static QString g_msnCacheDir;

static std::atomic<int> g_vrMsnCopied( 0 );
static std::atomic<int> g_vrMsnOurs( 0 );
static std::atomic<int> g_vrMsnCacheHit( 0 );
static std::atomic<int> g_vrMsnCacheMiss( 0 );
static std::atomic<int> g_vrMsnCacheRenorm( 0 );
static std::atomic<int> g_vrColCopied( 0 );
static std::atomic<int> g_vrColOurs( 0 );
static std::atomic<int> g_vrLayered( 0 );
static std::atomic<int> g_vrLayerless( 0 );
static std::atomic<int> g_vrLayerlessNoVanilla( 0 );
static std::atomic<int> g_vrShaded( 0 );

/*! THE EROSION PASS (lane GROUND1 Part B). 0 is off and is the rung's bytes:
 *  at 0 no lattice is built at all, so the two `_msn` writers take the branch
 *  they always took and no arithmetic happens that could round. */
static float g_erosion = 0.0f;
static int g_erosionIterations = 1;
static quint32 g_erosionSeed = 1u;

float lodgenErosion()
{
	return g_erosion;
}

void lodgenSetErosion( float strength )
{
	g_erosion = qBound( 0.0f, strength, 8.0f );
}

int lodgenErosionIterations()
{
	return g_erosionIterations;
}

void lodgenSetErosionIterations( int rounds )
{
	/* 1..8, not 1..16: a round costs a whole lattice of droplet traces AND
	 * widens the lattice by MAX_STEPS on every side, so round 8 already
	 * carries a 264-cell border. */
	g_erosionIterations = qBound( 1, rounds, 8 );
}

quint32 lodgenErosionSeed()
{
	return g_erosionSeed;
}

void lodgenSetErosionSeed( quint32 seed )
{
	g_erosionSeed = seed;
}

/* THE EROSION CENSUS, pooled across every lattice this process builds.
 * Both writers build their lattices per chunk and per tile, on worker
 * threads, and both reports want one set of numbers for the run, so the
 * pooling happens here behind a mutex rather than in either writer. It is
 * reset by the same call that resets the vanilla-reuse counters. */
static QMutex g_eroCensusMutex;
static LodgenErosionCensus g_eroCensusTotal;

void lodgenErosionCensusAdd( const LodgenErosionCensus & o )
{
	QMutexLocker lock( &g_eroCensusMutex );
	g_eroCensusTotal.add( o );
}

LodgenErosionCensus lodgenErosionCensusTotal()
{
	QMutexLocker lock( &g_eroCensusMutex );
	return g_eroCensusTotal;
}

void lodgenResetErosionCensus()
{
	QMutexLocker lock( &g_eroCensusMutex );
	g_eroCensusTotal = LodgenErosionCensus();
}

void LodgenErosionCensus::add( const LodgenErosionCensus & o )
{
	if ( o.cells == 0 )
		return;
	const double w0 = double( moved ), w1 = double( o.moved );
	if ( w0 + w1 > 0.0 )
		meanAbs = ( meanAbs * w0 + o.meanAbs * w1 ) / ( w0 + w1 );
	cells += o.cells;
	moved += o.moved;
	maxCut = qMin( maxCut, o.maxCut );
	maxFill = qMax( maxFill, o.maxFill );
	step = o.step;
}

int lodgenLandDetailSource()
{
	return g_landDetailSource;
}

void lodgenSetLandDetailSource( int mode )
{
	if ( mode >= LODGEN_LANDDETAIL_NONE && mode <= LODGEN_LANDDETAIL_EROSION )
		g_landDetailSource = mode;
}

QString lodgenVanillaLodRoot()
{
	return g_vanillaLodRoot;
}

void lodgenSetVanillaLodRoot( const QString & root )
{
	g_vanillaLodRoot = root;
	g_vanillaLodRoot.replace( QChar( 92 ), QChar( '/' ) );
	while ( g_vanillaLodRoot.endsWith( QChar( '/' ) ) )
		g_vanillaLodRoot.chop( 1 );
}

float lodgenLandShade()
{
	return g_landShade;
}

float lodgenLandGrade()
{
	return g_landGrade;
}

void lodgenSetLandGrade( float k )
{
	// bounded: a grade is an exposure, not a wipe
	g_landGrade = k < 0.0f ? 0.0f : ( k > 4.0f ? 4.0f : k );
}

void lodgenSetLandShade( float kDiv )
{
	// bounded: 255 levels is the whole channel, so anything past a quarter of
	// it is not a crevice term any more
	g_landShade = kDiv < -64.0f ? -64.0f : ( kDiv > 64.0f ? 64.0f : kDiv );
}

int lodgenSheetFormat()
{
	return g_sheetFormat;
}

void lodgenSetSheetFormat( int fmt )
{
	g_sheetFormat = ( fmt == LODGEN_SHEETFMT_VANILLA )
		? LODGEN_SHEETFMT_VANILLA : LODGEN_SHEETFMT_LEGACY;
}

QString lodgenMsnCacheDir()
{
	return g_msnCacheDir;
}

void lodgenSetMsnCacheDir( const QString & dir )
{
	g_msnCacheDir = dir;
}

void lodgenResetVanillaReuseCensus()
{
	g_vrMsnCopied = 0;
	g_vrMsnCacheHit = 0;
	g_vrMsnCacheMiss = 0;
	g_vrMsnCacheRenorm = 0;
	g_vrMsnOurs = 0;
	g_vrColCopied = 0;
	g_vrColOurs = 0;
	g_vrLayered = 0;
	g_vrLayerless = 0;
	g_vrLayerlessNoVanilla = 0;
	g_vrShaded = 0;
}

LodgenVanillaReuse lodgenVanillaReuseCensus()
{
	LodgenVanillaReuse c;
	c.msnCopied = g_vrMsnCopied.load();
	c.msnOurs = g_vrMsnOurs.load();
	c.colCopied = g_vrColCopied.load();
	c.colOurs = g_vrColOurs.load();
	c.layered = g_vrLayered.load();
	c.layerless = g_vrLayerless.load();
	c.layerlessNoVanilla = g_vrLayerlessNoVanilla.load();
	c.shaded = g_vrShaded.load();
	c.sheetFormat = g_sheetFormat;
	c.msnCacheHit = g_vrMsnCacheHit.load();
	c.msnCacheMiss = g_vrMsnCacheMiss.load();
	c.msnCacheRenorm = g_vrMsnCacheRenorm.load();
	return c;
}

QString lodgenVanillaSheetPath( const QString & ws, int dim, int chunkX, int chunkY,
	const QString & suffix )
{
	if ( g_vanillaLodRoot.isEmpty() )
		return QString();
	return g_vanillaLodRoot + QStringLiteral( "/Textures/Terrain/" ) + ws
		+ QChar( '/' ) + ws
		+ QString( ".%1.%2.%3" ).arg( dim ).arg( chunkX ).arg( chunkY )
		+ suffix + QStringLiteral( ".DDS" );
}

/*! Vanilla's shipped sheet as bytes, or an empty array. Loose file, named root,
 *  no resource stack -- see the provenance note above. */
bool lodgenReadVanillaSheet( const QString & ws, int dim, int chunkX, int chunkY,
	const QString & suffix, QByteArray & out )
{
	out.clear();
	const QString p = lodgenVanillaSheetPath( ws, dim, chunkX, chunkY, suffix );
	if ( p.isEmpty() )
		return false;
	QFile f( p );
	if ( !f.open( QIODevice::ReadOnly ) )
		return false;
	out = f.readAll();
	// a DDS header is 128 bytes; anything shorter, or without the magic, is not
	// a sheet and is never copied over one
	if ( out.size() <= 128 || !out.startsWith( "DDS " ) ) {
		out.clear();
		return false;
	}
	return true;
}

static bool lodgenWriteFileBytes( const QString & path, const QByteArray & bytes )
{
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
		return false;
	return f.write( bytes ) == qint64( bytes.size() ) && f.flush();
}

/*! An UNCOMPRESSED DDS: B8G8R8A8 through a DX10 header, full mip chain to 1x1.
 *
 *  It exists for one job -- the cleaned `_msn` cache -- and the reason it is
 *  not a block format is measured, not assumed: the cache's whole content is
 *  that the BC1 4x4 block grid is gone from it (block-grid line 1.0259 at
 *  period 16, against 1.3460 for a bicubic upscale that cleans nothing), and a
 *  BC re-encode puts a block grid straight back. There is no BC7 encoder in
 *  this tree, so BC7 is not an option here; what uncompressed costs is in the
 *  lane report, for bungo to rule on.
 *
 *  The packed quint32 is 0xAARRGGBB, whose little-endian bytes are B, G, R, A,
 *  which is exactly DXGI_FORMAT_B8G8R8A8_UNORM (87) -- no swizzle on write. */
static bool lodgenWriteDdsBgra8( const QString & path, int w, int h,
	const std::vector<quint32> & bgra )
{
	if ( w <= 0 || h <= 0 || bgra.size() != size_t( w ) * size_t( h ) )
		return false;
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
		return false;
	std::vector<std::vector<quint32>> mips;
	std::vector<int> mipW, mipH;
	mips.push_back( bgra );
	int mw = w, mh = h;
	mipW.push_back( mw );
	mipH.push_back( mh );
	while ( mw > 1 || mh > 1 ) {
		const std::vector<quint32> & prev = mips.back();
		const int nw = qMax( 1, mw / 2 ), nh = qMax( 1, mh / 2 );
		std::vector<quint32> next( size_t( nw ) * nh );
		for ( int y = 0; y < nh; y++ )
			for ( int x = 0; x < nw; x++ ) {
				quint32 acc[4] = { 0, 0, 0, 0 };
				for ( int sy = 0; sy < 2; sy++ )
					for ( int sx = 0; sx < 2; sx++ ) {
						const quint32 p = prev[size_t( qMin( y * 2 + sy, mh - 1 ) ) * mw
											   + qMin( x * 2 + sx, mw - 1 )];
						acc[0] += ( p >> 16 ) & 0xFF;
						acc[1] += ( p >> 8 ) & 0xFF;
						acc[2] += p & 0xFF;
						acc[3] += ( p >> 24 ) & 0xFF;
					}
				next[size_t( y ) * nw + x] = ( ( ( acc[3] + 2 ) >> 2 ) << 24 )
					| ( ( ( acc[0] + 2 ) >> 2 ) << 16 ) | ( ( ( acc[1] + 2 ) >> 2 ) << 8 )
					| ( ( acc[2] + 2 ) >> 2 );
			}
		mips.push_back( std::move( next ) );
		mw = nw;
		mh = nh;
		mipW.push_back( mw );
		mipH.push_back( mh );
	}
	quint32 hdr[32] = { 0 };
	hdr[0] = 0x20534444;            // the DDS magic
	hdr[1] = 124;
	hdr[2] = 0x0002100F;            // caps|height|width|pitch|pixelformat|mipcount
	hdr[3] = quint32( h );
	hdr[4] = quint32( w );
	hdr[5] = quint32( w ) * 4;      // PITCH, not linear size: this is not a block format
	hdr[7] = quint32( mips.size() );
	hdr[19] = 32;
	hdr[20] = 0x4;                  // fourCC
	hdr[21] = 0x30315844U;          // the DX10 fourCC
	hdr[27] = 0x401008;             // caps: complex|texture|mipmap
	// DDS_HEADER_DXT10: dxgiFormat, resourceDimension (3 = 2D), miscFlag, arraySize, miscFlags2
	const quint32 dx10[5] = { 87U, 3U, 0U, 1U, 0U };
	if ( f.write( reinterpret_cast<const char *>( hdr ), 128 ) != 128 )
		return false;
	if ( f.write( reinterpret_cast<const char *>( dx10 ), 20 ) != 20 )
		return false;
	for ( size_t mi = 0; mi < mips.size(); mi++ ) {
		const qint64 n = qint64( mips[mi].size() ) * 4;
		if ( f.write( reinterpret_cast<const char *>( mips[mi].data() ), n ) != n )
			return false;
	}
	return f.flush();
}

/*! One chunk's `_msn` out of the cleaned cache, or false when there is none.
 *
 *  `base` is the sheet path without its suffix, so the cache file is that name
 *  plus `.png` inside the cache directory. The channel law here is the MEASURED
 *  one (src/lodgen.h says what was measured and on how many sheets): cache R is
 *  east, cache G is north, cache B is empty, so UP is recomputed. Where
 *  east^2 + north^2 > 1 the triple is RENORMALISED rather than clamped --
 *  clamping leaves a normal that is not unit length, and a median 0.11% of
 *  texels per sheet are out there (26.28% on the worst sampled chunk). The
 *  count of sheets that needed it is in the census, so a run cannot hide it. */
/*! bungo's ASSEMBLED sheets (2026-09-18): `<name>_msn.DDS` beside or instead of
 *  the `.png`, uncompressed R8G8B8A8 with a DX10 header, one mip, in VANILLA's
 *  channel order (R east, G up, B north) with G his upscaled slope channel. The
 *  stored G is USED, not recomputed, and the triple is renormalised as a whole.
 *  Anything else in the file (a block-compressed format, mips, a legacy header)
 *  is refused with a reason on stderr and the .png is tried next. */
static bool lodgenMsnFromAssembledDds( const QString & path, std::vector<quint32> & out,
	int & w, int & h )
{
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) )
		return false;
	const QByteArray hdr = f.read( 148 );
	auto u32 = [&]( int at ) { return qFromLittleEndian<quint32>( reinterpret_cast<const uchar *>( hdr.constData() ) + at ); };
	QString why;
	if ( hdr.size() != 148 || !hdr.startsWith( "DDS " ) || u32( 4 ) != 124 )
		why = QStringLiteral( "not a DDS" );
	else if ( hdr.mid( 84, 4 ) != "DX10" )
		why = QStringLiteral( "no DX10 header" );
	else if ( u32( 128 ) != 28 )
		why = QStringLiteral( "DXGI format %1, not 28 (R8G8B8A8_UNORM)" ).arg( u32( 128 ) );
	if ( why.isEmpty() ) {
		h = int( u32( 12 ) );
		w = int( u32( 16 ) );
		if ( w <= 0 || h <= 0 || w > 8192 || h > 8192 )
			why = QStringLiteral( "size %1 x %2" ).arg( w ).arg( h );
		else if ( f.size() != qint64( 148 ) + qint64( w ) * qint64( h ) * 4 )
			why = QStringLiteral( "%1 bytes, expected %2 (one uncompressed mip)" )
				.arg( f.size() ).arg( qint64( 148 ) + qint64( w ) * qint64( h ) * 4 );
	}
	if ( !why.isEmpty() ) {
		QTextStream( stderr ) << "msn-cache: " << path << " refused: " << why << "\n";
		return false;
	}
	const QByteArray px = f.read( qint64( w ) * qint64( h ) * 4 );
	if ( px.size() != qint64( w ) * qint64( h ) * 4 )
		return false;
	out.assign( size_t( w ) * size_t( h ), LODGEN_MSN_FLAT );
	const uchar * src = reinterpret_cast<const uchar *>( px.constData() );
	for ( size_t i = 0; i < size_t( w ) * size_t( h ); i++, src += 4 ) {
		float e = float( src[0] ) / 255.0f * 2.0f - 1.0f;   // R east
		float up = float( src[1] ) / 255.0f * 2.0f - 1.0f;  // G up, his slope
		float n = float( src[2] ) / 255.0f * 2.0f - 1.0f;   // B north
		const float inv = 1.0f / qMax( std::sqrt( e * e + n * n + up * up ), 1e-6f );
		out[i] = lodgenTerrainMsnPixel( Vector3( e * inv, n * inv, up * inv ) );
	}
	return true;
}

static bool lodgenMsnFromCache( const QString & base, std::vector<quint32> & out,
	int & w, int & h )
{
	if ( g_msnCacheDir.isEmpty() )
		return false;
	const QString dds = QDir( g_msnCacheDir ).filePath(
		QFileInfo( base ).fileName() + QStringLiteral( "_msn.DDS" ) );
	if ( QFileInfo( dds ).exists() && lodgenMsnFromAssembledDds( dds, out, w, h ) )
		return true;
	const QString png = QDir( g_msnCacheDir ).filePath(
		QFileInfo( base ).fileName() + QStringLiteral( ".png" ) );
	if ( !QFileInfo( png ).exists() )
		return false;
	QImage img( png );
	if ( img.isNull() )
		return false;
	img = img.convertToFormat( QImage::Format_ARGB32 );
	w = img.width();
	h = img.height();
	if ( w <= 0 || h <= 0 )
		return false;
	out.assign( size_t( w ) * size_t( h ), LODGEN_MSN_FLAT );
	qint64 offCircle = 0;
	for ( int y = 0; y < h; y++ ) {
		const quint32 * src = reinterpret_cast<const quint32 *>( img.constScanLine( y ) );
		for ( int x = 0; x < w; x++ ) {
			const quint32 p = src[x];
			float e = float( ( p >> 16 ) & 0xFFU ) / 255.0f * 2.0f - 1.0f;   // cache R
			float n = float( ( p >> 8 ) & 0xFFU ) / 255.0f * 2.0f - 1.0f;    // cache G
			const float s = e * e + n * n;
			float up = std::sqrt( qMax( 0.0f, 1.0f - s ) );
			if ( s > 1.0f )
				offCircle++;
			const float len = std::sqrt( e * e + n * n + up * up );
			const float inv = 1.0f / qMax( len, 1e-6f );
			e *= inv;
			n *= inv;
			up *= inv;
			out[size_t( y ) * size_t( w ) + size_t( x )] =
				lodgenTerrainMsnPixel( Vector3( e, n, up ) );
		}
	}
	if ( offCircle )
		g_vrMsnCacheRenorm++;
	return true;
}

/*! Does any cell of this chunk carry land-texture paint?
 *
 *  "Paint" is a BTXT base texture or an ATXT/VTXT layer on ANY quadrant of ANY
 *  cell -- the per-chunk rule the coordinator named as the fallback, taken
 *  because the shipping writer assembles a chunk sheet from four VT tiles and
 *  has no per-quadrant seam to split on at that point.  An out-of-bounds cell
 *  has heights and nothing else, so it answers false; a partly painted chunk
 *  answers true and keeps our composite, which is the conservative way round
 *  (it never discards paint that exists). */
bool lodgenChunkHasLandPaint( const EsmWorld & world, int chunkX, int chunkY, int dim )
{
	EsmLand land;
	for ( int y = 0; y < dim; y++ ) {
		for ( int x = 0; x < dim; x++ ) {
			if ( !world.land( chunkX + x, chunkY + y, land ) )
				continue;
			for ( int q = 0; q < 4; q++ ) {
				if ( land.baseTex[q] )
					return true;
				if ( !land.layers[q].isEmpty() )
					return true;
			}
		}
	}
	return false;
}

/*! Vanilla's `_msn` detail field -- the sheet minus its own coarse version --
 *  decoded into east/north at the sheet's own resolution.
 *
 *  COARSE IS MIP 2 and that is not a tunable: four texels is 128 world units,
 *  which is exactly our height grid's step, so "detail" means precisely "the
 *  relief our own normal cannot know about".  Both levels come through the same
 *  trilinear sampler the rest of this file uses (`getPixelT`), because the
 *  coefficient was fitted that way and a blockier coarse is a different field. */
static bool lodgenVanillaMsnDetail( const QByteArray & bytes, int res,
	std::vector<float> & dEast, std::vector<float> & dNorth )
{
	if ( bytes.size() <= 128 || res <= 0 )
		return false;
	std::unique_ptr<DDSTexture16> tex;
	try {
		tex.reset( new DDSTexture16(
			reinterpret_cast<const unsigned char *>( bytes.constData() ),
			size_t( bytes.size() ) ) );
	} catch ( std::exception & ) {
		return false;
	}
	if ( !tex || tex->getWidth() != res || tex->getHeight() != res )
		return false;
	const float coarseMip = qMin( 2.0f, float( tex->getMaxMipLevel() ) );
	dEast.assign( size_t( res ) * size_t( res ), 0.0f );
	dNorth.assign( size_t( res ) * size_t( res ), 0.0f );
	auto decode = []( const FloatVector4 & c, float * e, float * n ) {
		// R = east, G = UP, B = north -- the one place that says so is above
		const float ex = c[0] * 2.0f - 1.0f;
		const float up = c[1] * 2.0f - 1.0f;
		const float nz = c[2] * 2.0f - 1.0f;
		const float len = std::sqrt( ex * ex + up * up + nz * nz );
		const float inv = 1.0f / qMax( len, 1e-6f );
		*e = ex * inv;
		*n = nz * inv;
	};
	for ( int y = 0; y < res; y++ ) {
		const float v = ( float( y ) + 0.5f ) / float( res );
		for ( int x = 0; x < res; x++ ) {
			const float u = ( float( x ) + 0.5f ) / float( res );
			float fe = 0.0f, fn = 0.0f, ce = 0.0f, cn = 0.0f;
			decode( tex->getPixelT( u, v, 0.0f ), &fe, &fn );
			decode( tex->getPixelT( u, v, coarseMip ), &ce, &cn );
			dEast[size_t( y ) * size_t( res ) + size_t( x )] = fe - ce;
			dNorth[size_t( y ) * size_t( res ) + size_t( x )] = fn - cn;
		}
	}
	return true;
}

/*! THE CREVICE TERM -- the shading the ruling asked for, by the only form of it
 *  that measures.
 *
 *      dL = kDiv * ( d(dEast)/dx + d(dNorth)/dy )
 *
 *  A LAMBERT DOT CANNOT SEE A RILL: the two walls of a rill tilt opposite ways
 *  and their dots cancel, which is why fitting kE/kN/kU against seven vanilla
 *  sheets returns the phase-twin floor with signs that flip sheet to sheet.
 *  The DIVERGENCE of the same field does not cancel -- negative in a channel,
 *  positive on a ridge -- and it reads r = -0.105 median, the same sign on 7 of
 *  7 sheets, over its own twin on 7 of 7.  It recovers about 1 % of the
 *  colour's fine variance, which is written down here so that nobody later
 *  reads this feature as "vanilla's colour".
 *
 *  Central differences, one-sided at the border, in per-texel units -- numpy's
 *  `gradient`, which is what the coefficient was fitted with. */
static int lodgenShadeWithCrevice( const QByteArray & msnBytes, int res,
	std::vector<quint32> & col, float k )
{
	if ( k == 0.0f )
		return 0;
	std::vector<float> dE, dN;
	if ( !lodgenVanillaMsnDetail( msnBytes, res, dE, dN ) )
		return 0;
	for ( int y = 0; y < res; y++ ) {
		for ( int x = 0; x < res; x++ ) {
			const int xm = x > 0 ? x - 1 : 0;
			const int xp = x + 1 < res ? x + 1 : res - 1;
			const int ym = y > 0 ? y - 1 : 0;
			const int yp = y + 1 < res ? y + 1 : res - 1;
			const float sx = ( xp - xm ) > 1 ? 0.5f : 1.0f;
			const float sy = ( yp - ym ) > 1 ? 0.5f : 1.0f;
			const float ddx = ( dE[size_t( y ) * size_t( res ) + size_t( xp )]
				- dE[size_t( y ) * size_t( res ) + size_t( xm )] ) * sx;
			const float ddy = ( dN[size_t( yp ) * size_t( res ) + size_t( x )]
				- dN[size_t( ym ) * size_t( res ) + size_t( x )] ) * sy;
			const float dL = k * ( ddx + ddy );
			quint32 & p = col[size_t( y ) * size_t( res ) + size_t( x )];
			quint32 outp = p & 0xFF000000U;
			for ( int c = 0; c < 3; c++ ) {
				const int val = int( ( p >> ( c * 8 ) ) & 0xFFU );
				const int nv = qBound( 0, int( std::lround( float( val ) + dL ) ), 255 );
				outp |= quint32( nv ) << ( c * 8 );
			}
			p = outp;
		}
	}
	return 1;
}

/*! vanilla's fine detail laid over OUR coarse normal -- `vanilla-blend`, the
 *  SECOND value, never the default.
 *
 *  It exists for reshaped terrain, where vanilla's sheet is the wrong surface
 *  but its fine relief is still the only fine relief anyone has.  East and
 *  north take our coarse plus vanilla's detail; UP IS RECOMPUTED from them so
 *  the stored normal stays unit length, which is the director's item 3. */
static int lodgenBlendVanillaDetail( const QByteArray & msnBytes, int res,
	std::vector<quint32> & nrm )
{
	std::vector<float> dE, dN;
	if ( !lodgenVanillaMsnDetail( msnBytes, res, dE, dN ) )
		return 0;
	if ( nrm.size() != dE.size() )
		return 0;
	auto enc = []( float v ) -> quint32 {
		return quint32( qBound( 0, int( std::lround( ( v * 0.5f + 0.5f ) * 255.0f ) ), 255 ) );
	};
	for ( size_t i = 0; i < nrm.size(); i++ ) {
		const quint32 p = nrm[i];
		float e = float( p & 0xFFU ) / 255.0f * 2.0f - 1.0f;
		float n = float( ( p >> 16 ) & 0xFFU ) / 255.0f * 2.0f - 1.0f;
		e = qBound( -1.0f, e + dE[i], 1.0f );
		n = qBound( -1.0f, n + dN[i], 1.0f );
		const float s = qMin( e * e + n * n, 1.0f );
		const float up = std::sqrt( qMax( 0.0f, 1.0f - s ) );
		nrm[i] = ( p & 0xFF000000U ) | enc( e ) | ( enc( up ) << 8 ) | ( enc( n ) << 16 );
	}
	return 1;
}

/*! The one place a chunk sheet decides what its `_msn` and its colour are.
 *
 *  Called by BOTH writers -- the stock per-chunk bake and the PYRAMID/VT
 *  assembly -- so the two cannot drift, which is exactly the mistake lane
 *  TILING2 made when it edited one `sampleLtex` and not the other.  Every
 *  outcome is a decision and every decision is counted.  `msnCopy`/`colCopy`
 *  come back empty when this chunk writes its own bake as before. */
void lodgenVanillaChunkSheets( const EsmWorld & world, const QString & ws, int dim,
	int chunkX, int chunkY, int res, bool hasPaint,
	std::vector<quint32> & col, std::vector<quint32> & nrm,
	QByteArray & msnCopy, QByteArray & colCopy )
{
	(void) world;
	msnCopy.clear();
	colCopy.clear();
	if ( g_landDetailSource == LODGEN_LANDDETAIL_NONE )
		return;                            // the rung, to the byte
	QByteArray vmsn;
	const bool haveMsn = lodgenReadVanillaSheet( ws, dim, chunkX, chunkY,
		QStringLiteral( "_msn" ), vmsn );
	if ( hasPaint )
		g_vrLayered++;
	else
		g_vrLayerless++;

	if ( haveMsn && g_landDetailSource == LODGEN_LANDDETAIL_VANILLA ) {
		msnCopy = vmsn;                    // the ruling: vanilla's sheet, byte for byte
		g_vrMsnCopied++;
	} else {
		if ( haveMsn && g_landDetailSource == LODGEN_LANDDETAIL_VANILLA_BLEND )
			lodgenBlendVanillaDetail( vmsn, res, nrm );
		g_vrMsnOurs++;
	}

	if ( !hasPaint ) {
		QByteArray vcol;
		if ( lodgenReadVanillaSheet( ws, dim, chunkX, chunkY, QString(), vcol ) ) {
			colCopy = vcol;                // baked outside FO4; not recoverable, so reused
			g_vrColCopied++;
		} else {
			g_vrLayerlessNoVanilla++;
			g_vrColOurs++;
		}
		return;
	}
	g_vrColOurs++;

	/* Under EROSION the colour already carries a crevice term, computed at
	 * the per-texel write from THIS bake's own relief. Running vanilla's on
	 * top would shade one sheet twice from two different surfaces, and the
	 * two surfaces do not agree: vanilla's sheet is the vanilla terrain's
	 * fine normal, ours is the erosion delta this bake just grew. One
	 * sheet, one crevice term. */
	if ( haveMsn && g_landDetailSource != LODGEN_LANDDETAIL_EROSION )
		g_vrShaded += lodgenShadeWithCrevice( vmsn, res, col, g_landShade );
}

/*! Write one chunk's colour and `_msn`, honouring the reuse decision above.
 *  A copied sheet is written with ITS OWN BYTES and never re-encoded:
 *  re-encoding a BC block is precisely what "byte for byte" forbids.
 *
 *  This is also the ONE place the sheet FORMAT and the cleaned `_msn` cache
 *  act, for the same reason the reuse decision lives one function up: both
 *  writers -- the stock per-chunk bake and the pyramid/VT assembly -- come
 *  through here, so the two cannot drift.
 *
 *  Order on the `_msn`, and it is a decision, not an accident:
 *    1. the cleaned cache, when a file for this chunk is in it. It is last in
 *       and it wins, because the whole point of ADDED ITEM 8 is to replace the
 *       byte-for-byte vanilla copy on exactly the chunks that have one.
 *    2. vanilla's own bytes, when the reuse decision copied them.
 *    3. our bake, in the format the switch names.
 *  With both switches at their defaults every branch below is the branch that
 *  ran before, with the arguments it ran with before. */
bool lodgenWriteChunkSheets( const QString & base, int res,
	const std::vector<quint32> & col, const std::vector<quint32> & nrm,
	const QByteArray & colCopy, const QByteArray & msnCopy )
{
	const QString cp = base + QStringLiteral( ".DDS" );
	const QString mp = base + QStringLiteral( "_msn.DDS" );
	const bool vanFmt = g_sheetFormat == LODGEN_SHEETFMT_VANILLA;
	/* Vanilla's alpha is a measured constant 255 on both families over the
	 * whole shipped corpus, so `vanilla` writes 255 and nothing else. The
	 * buffers already carry 0xFF, but forcing it is the cheap way to say that
	 * the format is the law here and not whatever a caller happened to leave
	 * in the top byte. */
	auto opaque = []( const std::vector<quint32> & in ) {
		std::vector<quint32> out( in.size() );
		for ( size_t i = 0; i < in.size(); i++ )
			out[i] = in[i] | 0xFF000000U;
		return out;
	};
	if ( colCopy.isEmpty() ) {
		if ( vanFmt ) {
			if ( !lodgenWriteDds( cp, res, res, opaque( col ), true, 0, false, 0, 0, true ) )
				return false;
		} else if ( !lodgenWriteDds( cp, res, res, col ) ) {
			return false;
		}
	} else if ( !lodgenWriteFileBytes( cp, colCopy ) ) {
		return false;
	}
	std::vector<quint32> cached;
	int cw = 0, ch = 0;
	if ( lodgenMsnFromCache( base, cached, cw, ch ) ) {
		g_vrMsnCacheHit++;
		return lodgenWriteDdsBgra8( mp, cw, ch, cached );
	}
	if ( !g_msnCacheDir.isEmpty() )
		g_vrMsnCacheMiss++;
	if ( msnCopy.isEmpty() ) {
		if ( vanFmt ) {
			if ( !lodgenWriteDds( mp, res, res, opaque( nrm ), true, 0, false, 0, 0, true ) )
				return false;
		} else if ( !lodgenWriteDds( mp, res, res, nrm ) ) {
			return false;
		}
	} else if ( !lodgenWriteFileBytes( mp, msnCopy ) ) {
		return false;
	}
	return true;
}

/*! Every "once, on first use" index the generator owns, built NOW, on the
 *  calling thread (lane BAKEPERF1, 2026-09-11).
 *
 *  Three of them, and first use is the only moment any of them can race:
 *  `lodgenStackIndex()`'s BA2File over the resource stack, `lodgenMeshArchives()`
 *  over the game manager's folders, and `GameResources::init_archives()`, which
 *  `GameManager::get_file` calls lazily on its first miss. The terrain ring
 *  self-test's one-shot comes along for the ride.
 *
 *  Idempotent: a second call finds all three built and returns. */
void lodgenWarmSharedIndices()
{
	lodgenStackIndex();
	lodgenMeshArchives();
	lodgenTerrainRingSelfTestOnce();
	/* Reach the game manager's own archive index the way the bake reaches it.
	 * The path cannot exist; what matters is that the lazy init behind it has
	 * run before any worker asks. */
	Game::GameManager::find_file( Game::FALLOUT_4,
		QStringLiteral( "ww_lodgen_warm_up" ), "textures", ".dds" );
	/* And ONE NifModel, built and thrown away. Its constructor fills the
	 * array-pseudonym tables, reads QSettings and takes an entry in the game
	 * manager's resource map -- all "first time only" work, and in a -no-gui
	 * run the first NifModel of the process would otherwise be one a worker
	 * builds, sixteen of them at once. */
	{
		NifModel warmModel;
		(void) warmModel.getBlockCount();
	}
}

/* ================= roads and decals in the far-terrain colour ==============
 *
 * bungo, 2026-09-11 10:0x, verbatim: "We do the same with roads and decals as
 * vanilla."  What vanilla does was MEASURED before a line of this was written
 * (lane ROADS1, report section 1, on Bethesda's own
 * `Textures\Terrain\Commonwealth\Commonwealth.4.-20.20.DDS`, the Sanctuary
 * loop-road tile):
 *
 *   * the road content is NOT in the LAND paint -- not one of the sixteen
 *     landscape textures painted across that chunk's cells is a road, asphalt,
 *     concrete or pavement texture, and all sixteen cells have a LAND record;
 *   * the road content IS at the road MESHES' own top-down footprint: scoring
 *     vanilla's sheet with a projection of the placed `Landscape\Roads\*`
 *     meshes gives AUC 0.716 on brightness and 0.678 on greyness, against a
 *     floor -- the same mask displaced five ways, area, shape and spectrum
 *     preserved -- that never passes 0.601 / 0.513. Trees (0.529), rocks
 *     (0.448), architecture (0.621), set dressing (0.560) and every generic
 *     `bDecal` shape in the region (0.564) all stay inside their own floors;
 *   * the colour is the road MATERIAL'S OWN diffuse under the sheet's own
 *     grading: `SancRoad01_d.dds` averages luminance 112.8 and vanilla's sheet
 *     reads 92.6 inside the footprint, a factor 0.82, while `DriedGrass01_D`
 *     averages 100.2 and the sheet reads 82.9 on the plain background, a factor
 *     0.83 -- the same grading on both;
 *   * the `_msn` normal sheet does NOT carry it: vanilla's `_msn` against a
 *     normal computed from the LAND heightmap alone disagrees by 13.58 deg on
 *     the road footprint and 14.14 deg on the background, i.e. the road agrees
 *     with the bare heightmap slightly BETTER than its surroundings, where a
 *     baked road mesh would have to disagree.
 *
 * So: the road meshes are scan-converted top-down into the COLOUR sheet, after
 * the splat composite and after the VCLR multiply -- the road sits ON the
 * ground the artist shaded, so it must not be shaded again -- and BEFORE the
 * grass tint, whose weight is then scaled down by the road's own coverage,
 * because grass grows beside a road and not through it. Nothing else is
 * touched: not the normal sheet, not the height sheet, not roughness or
 * metallic, not the retired data plane.
 * ========================================================================== */

bool lodgenIsRoadModel( const QString & modelPath )
{
	QString p = modelPath;
	p.replace( QChar( '\\' ), QChar( '/' ) );
	const QStringList c = p.toLower().split( QChar( '/' ), Qt::SkipEmptyParts );
	int i = 0;
	if ( i < c.size() && c[i] == QLatin1String( "meshes" ) )
		i++;
	// `landscape` / (`roads`|`sidewalks`) / at least one more component, since
	// the last component is the file and the first two must be FOLDERS
	if ( i + 2 >= c.size() )
		return false;
	if ( c[i] != QLatin1String( "landscape" ) )
		return false;
	const QString & second = c[i + 1];
	return second == QLatin1String( "roads" )
		|| second == QLatin1String( "sidewalks" );
}

bool lodgenIsSidewalkModel( const QString & modelPath )
{
	QString p = modelPath;
	p.replace( QChar( '\\' ), QChar( '/' ) );
	const QStringList c = p.toLower().split( QChar( '/' ), Qt::SkipEmptyParts );
	int i = 0;
	if ( i < c.size() && c[i] == QLatin1String( "meshes" ) )
		i++;
	if ( i + 2 >= c.size() )
		return false;
	return c[i] == QLatin1String( "landscape" )
		&& c[i + 1] == QLatin1String( "sidewalks" );
}

bool lodgenIsRaisedRoadModel( const QString & modelPath )
{
	QString p = modelPath;
	p.replace( QChar( '\\' ), QChar( '/' ) );
	const QStringList c = p.toLower().split( QChar( '/' ), Qt::SkipEmptyParts );
	int i = 0;
	if ( i < c.size() && c[i] == QLatin1String( "meshes" ) )
		i++;
	// three FOLDERS and a file, so the same component-equality discipline
	if ( i + 3 >= c.size() )
		return false;
	if ( c[i] != QLatin1String( "landscape" ) || c[i + 1] != QLatin1String( "roads" ) )
		return false;
	const QString & third = c[i + 2];
	return third == QLatin1String( "highwayoverpass" )
		|| third == QLatin1String( "bridge" );
}

void LodgenRoadCensus::addRefusal( const char * why, const QString & name )
{
	if ( refusals.size() >= 16 )
		return;
	const QString w = QString::fromLatin1( why );
	const QString row = w.isEmpty() ? name : ( w + QChar( ' ' ) + name );
	if ( row.isEmpty() || refusals.contains( row ) )
		return;
	refusals.append( row );
}

void LodgenRoadCensus::add( const LodgenRoadCensus & o )
{
	placements += o.placements;
	meshes += o.meshes;
	shapes += o.shapes;
	decalShapes += o.decalShapes;
	triangles += o.triangles;
	texels += o.texels;
	decalTexels += o.decalTexels;
	alphaRejected += o.alphaRejected;
	refusedNoLoad += o.refusedNoLoad;
	refusedNoTexture += o.refusedNoTexture;
	refusedRaised += o.refusedRaised;
	raisedBases += o.raisedBases;
	blendTexels += o.blendTexels;
	refusedSidewalk += o.refusedSidewalk;
	sidewalkBases += o.sidewalkBases;
	groundShapes += o.groundShapes;
	groundTexels += o.groundTexels;
	for ( const QString & r : o.refusals )
		if ( refusals.size() < 16 && !refusals.contains( r ) )
			refusals.append( r );
}

void LodgenObjectAoCensus::addRefusal( const char * why, const QString & name )
{
	if ( refusals.size() >= 16 )
		return;
	const QString w = QString::fromLatin1( why );
	const QString row = w.isEmpty() ? name : ( w + QChar( ' ' ) + name );
	if ( row.isEmpty() || refusals.contains( row ) )
		return;
	refusals.append( row );
}

void LodgenObjectAoCensus::add( const LodgenObjectAoCensus & o )
{
	placements += o.placements;
	meshes += o.meshes;
	triangles += o.triangles;
	squares += o.squares;
	slabSquares += o.slabSquares;
	refusedNoLod += o.refusedNoLod;
	noLodBases += o.noLodBases;
	refusedNoLoad += o.refusedNoLoad;
	texels += o.texels;
	darkSum += o.darkSum;
	for ( const QString & r : o.refusals )
		if ( refusals.size() < 16 && !refusals.contains( r ) )
			refusals.append( r );
}

QString LodgenRoadCensus::line() const
{
	QString s = QStringLiteral( "roads" );
	auto kv = [&s]( const char * k, int v ) {
		s += QChar( ' ' );
		s += QLatin1String( k );
		s += QChar( '=' );
		s += QString::number( v );
	};
	kv( "placements", placements );
	kv( "meshes", meshes );
	kv( "shapes", shapes );
	kv( "decalshapes", decalShapes );
	kv( "triangles", triangles );
	kv( "texels", texels );
	kv( "decaltexels", decalTexels );
	kv( "alpharejected", alphaRejected );
	kv( "refused_noload", refusedNoLoad );
	kv( "refused_notexture", refusedNoTexture );
	kv( "refused_raised", refusedRaised );
	kv( "raised_bases", raisedBases );
	kv( "blend_texels", blendTexels );
	kv( "refused_sidewalk", refusedSidewalk );
	kv( "sidewalk_bases", sidewalkBases );
	kv( "ground_shapes", groundShapes );
	kv( "ground_texels", groundTexels );
	s += QStringLiteral( " refusals=" );
	s += refusals.isEmpty() ? QStringLiteral( "none" )
		: QString( QStringLiteral( "[%1]" ) ).arg( refusals.join( QStringLiteral( "; " ) ) );
	return s;
}

namespace
{

/*! The material path a Fallout 4 shape actually names.
 *
 * MEASURED, lane ROADS1's first gate run: every `Landscape\Roads\Country\*` and
 * `Landscape\Roads\Alley\*` piece names its material as an ABSOLUTE BETHESDA
 * BUILD PATH -- `C:\Projects\Fallout4\Build\PC\Data\materials\Landscape\Roads\
 * AsphaltAndSWEdgeDecals01.BGSM` -- and carries an EMPTY texture set, because
 * the material is meant to supply the textures. `lodgenLoadModel`'s own
 * fix-up only prepends `materials/` when the path does not already start with
 * it, so such a path becomes `materials/C:/Projects/...` and resolves to
 * nothing: the shape ends up with no diffuse at all. On the Sanctuary
 * loop-road chunk that was 65 of 270 road shapes, and it was every DECAL among
 * them, which is why the first run's census read `decalshapes=0`.
 *
 * The rule (the same one `tools/lod_emission_probe.py` documents for LOD
 * shaders): key on the last `materials/` in the path.
 *
 * It is deliberately NOT applied inside `lodgenLoadModel`. That loader feeds
 * the OBJECT bakes, whose output is pinned by byte identity, and widening what
 * it resolves would move files this lane must leave alone. The same defect is
 * therefore still live for the object path and is reported as a RED. */
QString lodgenRoadMaterialPath( const QString & matName )
{
	QString p = matName;
	p.replace( QChar( '\\' ), QChar( '/' ) );
	const int i = p.toLower().lastIndexOf( QStringLiteral( "materials/" ) );
	if ( i > 0 )
		return p.mid( i );
	if ( i < 0 )
		p.prepend( QStringLiteral( "materials/" ) );
	return p;
}

/*! Is this material one of the LANDSCAPE'S OWN GROUND materials?
 *
 *  The discriminator is the material's FOLDER, which is data and not a guess:
 *  Bethesda files `materials/Landscape/Ground/*` for the textures the terrain
 *  itself is painted with and `materials/Landscape/Roads/*` for the road
 *  surface, its kerbs and its decals. A name-stem list was tried first and
 *  mis-read two of the biggest winners on chunk (-20,20) -- 7,558 texels of
 *  `CommonwealthDefault01.bgsm` and 5,317 of `SancSW01.BGSM` -- because
 *  neither name carries a stem that says which it is.
 *
 *  The path is normalised by `lodgenRoadMaterialPath` first, so the four
 *  prefixes the shipped NIFs actually carry (a bare `materials/...`, a
 *  `Data/materials/...`, and Bethesda's absolute
 *  `C:/Projects/Fallout4/Build/PC/Data/Materials/...`, in any case) all reduce
 *  to the same test. */
bool lodgenRoadMaterialIsGround( const QString & matName )
{
	if ( matName.isEmpty() )
		return false;
	return lodgenRoadMaterialPath( matName ).toLower()
		.contains( QStringLiteral( "materials/landscape/ground/" ) );
}

//! What the road pass reads out of one material, cached per material name.
struct LodgenRoadMat
{
	QString tex0;
	bool decal = false;
	bool alphaTest = false;
	bool alphaBlend = false;
	float alphaRef = 1.0f;
	bool read = false;
	//! The material lives under materials/Landscape/Ground/: it is terrain.
	bool ground = false;
};

//! One placed road shape, already in world space, with its world bounds.
struct LodgenRoadShape
{
	/* All four are VALUE copies, never pointers into the model cache: the cache
	 * is a QHash and an insert may rehash it, so a pointer taken before the
	 * next model loads is a pointer into a moved bucket. Qt's containers are
	 * copy-on-write, so the copy is a reference count and not the data. */
	QVector<Vector3> pos;           //!< world-space vertices
	QVector<Vector2> uv;
	QVector<Color4> col;
	QVector<Triangle> tris;
	QString tex0;
	bool decal = false;
	bool alphaTest = false;
	bool alphaBlend = false;
	float alphaRef = 1.0f;
	float bx0 = 0, by0 = 0, bx1 = 0, by1 = 0;
	//! Mean world Z of the shape's vertices: the composite's painting order.
	float meanZ = 0.0f;
	//! The shape's material is one of the LANDSCAPE'S ground materials.
	bool groundMat = false;
};

/*! Every road placement in a cell rectangle, resolved to world-space shapes
 *  once, so a bake that writes many tiles over the same ground pays for the
 *  ESM walk and the model loads once.
 *
 *  The set is gathered over the rectangle GROWN by two cells, because a road
 *  piece is placed by its own origin and reaches outwards from it; the scan
 *  converter clips, so a piece gathered and then found to miss costs a bounds
 *  test. */
class LodgenRoadSet
{
public:
	bool empty() const { return shapes.isEmpty(); }
	const LodgenRoadCensus & census() const { return cen; }

	void gather( const EsmWorld & world, const QString & dataRoot,
		int cx0, int cy0, int cx1, int cy1, bool includeRaised = true,
		bool includeSidewalks = true )
	{
		const int margin = 2;
		raised = includeRaised;
		sidewalks = includeSidewalks;
		QSet<QString> loaded;
		for ( int cy = cy0 - margin; cy <= cy1 + margin; cy++ ) {
			for ( int cx = cx0 - margin; cx <= cx1 + margin; cx++ ) {
				for ( const EsmRefr & r : world.refrs( cx, cy ) ) {
					if ( r.initiallyDisabled || r.deleted || !r.base )
						continue;
					Matrix rm;
					rm.fromEuler( -r.rot[0], -r.rot[1], -r.rot[2] );
					const Vector3 rp( r.pos[0], r.pos[1], r.pos[2] );
					if ( std::memcmp( &r.baseType, "SCOL", 4 ) == 0 ) {
						for ( const EsmScolPart & part : world.scolParts( r.base ) )
							for ( const EsmScolPlacement & pl : part.placements ) {
								Matrix pm;
								pm.fromEuler( -pl.rot[0], -pl.rot[1], -pl.rot[2] );
								addPlacement( world, dataRoot, loaded, part.base,
									rp + rm * ( Vector3( pl.pos[0], pl.pos[1], pl.pos[2] )
										* r.scale ),
									rm * pm, r.scale * pl.scale );
							}
						continue;
					}
					addPlacement( world, dataRoot, loaded, r.base, rp, rm, r.scale );
				}
			}
		}
	}

	/*! Scan-convert into a grid: `S` texels over [wx0,wy0]..[wx0+S*upt, ...],
	 *  row 0 NORTH, texel centres at +0.5. `colour` receives 0x00000000 where
	 *  no road covers and 0xAARRGGBB otherwise, A = coverage.
	 *
	 *  TWO RULES, and the caller names which (LodgenCoverOptions::roadComposite):
	 *
	 *  `RoadMaxZ` is lane ROADS1's and is left here untouched, line for line: a
	 *  z buffer holds world Z, the LARGEST wins and the winner OVERWRITES the
	 *  texel, which is what "seen from above" means -- a driveway laid over a
	 *  road wins, a road tucked under a bridge deck does not.
	 *
	 *  `RoadBlend` COMPOSITES. The shapes are painted in a stated order --
	 *  ascending mean world Z, a non-decal before a decal at equal height, and
	 *  gather order at equal both -- and each fragment writes
	 *
	 *      dstPre = src * srcAlpha + dstPre * ( 1 - srcAlpha )
	 *      dstA   =       srcAlpha + dstA   * ( 1 - srcAlpha )
	 *
	 *  with `srcAlpha` the material's opacity times the interpolated vertex
	 *  alpha for an alpha-BLENDED shape, the alpha test's 0 or 1 for an
	 *  alpha-TESTED one, and 1 for an opaque one. The plane is un-premultiplied
	 *  at the end, so a texel covered only by a half-transparent skirt carries
	 *  the skirt's OWN colour at coverage 0.5 and the consumer's
	 *  `ground + (road-ground)*A` lerp does the rest.
	 *
	 *  There is no z buffer under `RoadBlend`: the painting order is the depth
	 *  order, which is the painter's algorithm and is what makes a feather a
	 *  feather instead of a winner. Where a shape's mean Z misorders it against
	 *  a shape it actually passes over -- a long ramp over a flat piece -- the
	 *  two rules can differ; that difference is measured, not assumed (lane
	 *  ROADS2, gate S3), and `--road-composite max-z` is the exact way back.
	 *
	 *  `detail` lerps the diffuse sample toward the texture's own whole-texture
	 *  average (LodgenCoverOptions::roadDetail). RGB only: the alpha channel
	 *  cuts alpha-tested decals out and averaging it would dissolve them. */
	void rasterise( float wx0, float wyTop, float upt, int S,
		std::vector<quint32> & colour, LodgenBakeCaches & bc,
		const QString & dataRoot, LodgenRoadCensus & out,
		int composite = LodgenCoverOptions::RoadMaxZ, float detail = 1.0f,
		float groundPaint = 1.0f ) const
	{
		colour.assign( size_t( S ) * S, 0U );
		if ( shapes.isEmpty() )
			return;
		const bool blend = ( composite == LodgenCoverOptions::RoadBlend );
		std::vector<float> zbuf( size_t( S ) * S, -1e30f );
		std::vector<float> acc;             // premultiplied RGB + A, blend only
		std::vector<char> partial;          // texels a fragment with A<1 touched
		if ( blend ) {
			acc.assign( size_t( S ) * S * 4, 0.0f );
			partial.assign( size_t( S ) * S, 0 );
		}
		const float wy0 = wyTop - float( S ) * upt;

		/* THE ORDER. Under `max-z` it is the gather order, because the z buffer
		 * and not the order decides, and ROADS1's bytes depend on neither. */
		std::vector<int> order( size_t( shapes.size() ) );
		for ( qsizetype k = 0; k < shapes.size(); k++ )
			order[size_t( k )] = int( k );
		if ( blend ) {
			const QVector<LodgenRoadShape> & sv = shapes;
			std::stable_sort( order.begin(), order.end(),
				[&sv]( int a, int b ) {
					if ( sv[a].meanZ != sv[b].meanZ )
						return sv[a].meanZ < sv[b].meanZ;
					return int( sv[a].decal ) < int( sv[b].decal );
				} );
		}

		for ( int si : order ) {
			const LodgenRoadShape & sh = shapes[si];
			if ( sh.bx1 < wx0 || sh.bx0 > wx0 + float( S ) * upt )
				continue;
			if ( sh.by1 < wy0 || sh.by0 > wyTop )
				continue;
			const DDSTexture16 * tex = sh.tex0.isEmpty() ? nullptr
				: lodgenCachedTexture( bc, dataRoot, sh.tex0 );
			if ( !tex ) {
				out.refusedNoTexture++;
				out.addRefusal( "no-diffuse", sh.tex0.isEmpty()
					? QStringLiteral( "(shape names none)" ) : sh.tex0 );
				continue;
			}
			out.shapes++;
			if ( sh.decal )
				out.decalShapes++;
			const int texW = int( tex->getWidth() ), texH = int( tex->getHeight() );
			/* The texture's own average, read once a shape at its deepest mip --
			 * the same call the splat path uses for a layer's flat colour. Only
			 * looked up when a detail lerp is actually asked for, so a full
			 * detail bake touches no new code path. */
			FloatVector4 texAvg( 0.0f );
			if ( detail < 1.0f )
				texAvg = tex->getPixelT( 0.5f, 0.5f, float( tex->getMaxMipLevel() ) );
			for ( const Triangle & t : sh.tris ) {
				out.triangles++;
				// texel coordinates: x east, y SOUTH (row 0 north)
				float px[3], py[3], pz[3];
				for ( int k = 0; k < 3; k++ ) {
					const Vector3 & v = sh.pos[t[k]];
					px[k] = ( v[0] - wx0 ) / upt;
					py[k] = ( wyTop - v[1] ) / upt;
					pz[k] = v[2];
				}
				int i0 = int( std::floor( qMin( px[0], qMin( px[1], px[2] ) ) ) );
				int i1 = int( std::ceil( qMax( px[0], qMax( px[1], px[2] ) ) ) );
				int j0 = int( std::floor( qMin( py[0], qMin( py[1], py[2] ) ) ) );
				int j1 = int( std::ceil( qMax( py[0], qMax( py[1], py[2] ) ) ) );
				i0 = qMax( i0, 0 ); j0 = qMax( j0, 0 );
				i1 = qMin( i1, S - 1 ); j1 = qMin( j1, S - 1 );
				if ( i1 < i0 || j1 < j0 )
					continue;
				const float d = ( py[1] - py[2] ) * ( px[0] - px[2] )
					+ ( px[2] - px[1] ) * ( py[0] - py[2] );
				if ( std::fabs( d ) < 1e-9f )
					continue;
				/* The mip: the triangle's texture area against its footprint in
				 * bake texels. A road mesh does not tile with the world, so the
				 * landscape path's world-tiling mip would be meaningless here. */
				float mip = 0.0f;
				if ( !sh.uv.isEmpty() ) {
					const Vector2 & a = sh.uv[t[0]];
					const Vector2 & b = sh.uv[t[1]];
					const Vector2 & c = sh.uv[t[2]];
					const float uvA = std::fabs( ( b[0] - a[0] ) * ( c[1] - a[1] )
						- ( c[0] - a[0] ) * ( b[1] - a[1] ) ) * float( texW ) * float( texH );
					const float pxA = std::fabs( d );
					if ( uvA > 0.0f && pxA > 0.0f )
						mip = qBound( 0.0f, 0.5f * std::log2( uvA / pxA ),
							float( tex->getMaxMipLevel() ) );
				}
				for ( int j = j0; j <= j1; j++ ) {
					for ( int i = i0; i <= i1; i++ ) {
						const float x = float( i ) + 0.5f, y = float( j ) + 0.5f;
						const float w0 = ( ( py[1] - py[2] ) * ( x - px[2] )
							+ ( px[2] - px[1] ) * ( y - py[2] ) ) / d;
						const float w1 = ( ( py[2] - py[0] ) * ( x - px[2] )
							+ ( px[0] - px[2] ) * ( y - py[2] ) ) / d;
						const float w2 = 1.0f - w0 - w1;
						if ( w0 < 0.0f || w1 < 0.0f || w2 < 0.0f )
							continue;
						const float z = w0 * pz[0] + w1 * pz[1] + w2 * pz[2];
						const size_t o = size_t( j ) * S + i;
						if ( !blend && z <= zbuf[o] )
							continue;
						float u = 0.0f, v = 0.0f;
						if ( !sh.uv.isEmpty() ) {
							u = w0 * sh.uv[t[0]][0] + w1 * sh.uv[t[1]][0] + w2 * sh.uv[t[2]][0];
							v = w0 * sh.uv[t[0]][1] + w1 * sh.uv[t[1]][1] + w2 * sh.uv[t[2]][1];
						}
						u = u - std::floor( u );
						v = v - std::floor( v );
						FloatVector4 c = tex->getPixelT( u, v, mip );
						/* THE DETAIL LERP, before the vertex-colour tint so the
						 * tint still grades whatever is left. RGB only. */
						if ( detail < 1.0f )
							for ( int k = 0; k < 3; k++ )
								c[k] = texAvg[k] + ( c[k] - texAvg[k] ) * detail;
						/* Coverage. An OPAQUE road shape covers fully: its
						 * diffuse alpha is not a silhouette and reading it as
						 * one would punch the road full of holes. Only a shape
						 * whose material or whose NiAlphaProperty says so
						 * honours the texture's alpha -- that is the clause
						 * that makes an alpha-tested road decal cut out. */
						float cov = 1.0f;
						if ( sh.alphaTest )
							cov = ( c[3] >= sh.alphaRef ) ? 1.0f : 0.0f;
						else if ( sh.alphaBlend )
							cov = qBound( 0.0f, c[3], 1.0f );
						if ( !sh.col.isEmpty() ) {
							const Color4 & ca = sh.col[t[0]];
							const Color4 & cb = sh.col[t[1]];
							const Color4 & cc = sh.col[t[2]];
							for ( int k = 0; k < 3; k++ )
								c[k] *= w0 * ca[k] + w1 * cb[k] + w2 * cc[k];
							if ( sh.alphaBlend )
								cov *= qBound( 0.0f,
									w0 * ca[3] + w1 * cb[3] + w2 * cc[3], 1.0f );
						}
						/* THE GROUND-MATERIAL SHAPES (lane ROADS4). The
						 * multiply is on COVERAGE, so it moves the paint and
						 * the cover suppression together, and it is branched
						 * over at 1.0 so the old default is the old bytes.
						 * At 0 the fragment falls out at the test below
						 * WITHOUT touching the z buffer, so a ground shape
						 * does not occlude the road shape under it either --
						 * a drop, with no second code path. */
						if ( sh.groundMat ) {
							if ( groundPaint < 1.0f )
								cov *= groundPaint;
							if ( cov > 0.0f )
								out.groundTexels++;
						}
						if ( cov <= 0.0f ) {
							out.alphaRejected++;
							continue;
						}
						if ( !blend ) {
							zbuf[o] = z;
							const quint32 a8 = quint32( qBound( 0.0f, cov * 255.0f + 0.5f, 255.0f ) );
							colour[o] = ( a8 << 24 )
								| ( quint32( qBound( 0, int( c[0] * 255.0f + 0.5f ), 255 ) ) << 16 )
								| ( quint32( qBound( 0, int( c[1] * 255.0f + 0.5f ), 255 ) ) << 8 )
								| quint32( qBound( 0, int( c[2] * 255.0f + 0.5f ), 255 ) );
						} else {
							float * a = &acc[o * 4];
							const float k = 1.0f - cov;
							for ( int ch = 0; ch < 3; ch++ )
								a[ch] = c[ch] * cov + a[ch] * k;
							a[3] = cov + a[3] * k;
							if ( cov < 1.0f )
								partial[o] = 1;
						}
						if ( sh.decal )
							decalHere.insert( o );
					}
				}
			}
		}
		if ( blend ) {
			/* UN-PREMULTIPLY. The plane's contract is "the road's own colour,
			 * and A = how much of the texel it covers", so the accumulated
			 * colour is divided by the accumulated coverage. */
			for ( size_t o = 0; o < colour.size(); o++ ) {
				const float A = acc[o * 4 + 3];
				if ( A <= 0.0f )
					continue;
				const float inv = 1.0f / A;
				const quint32 a8 = quint32( qBound( 0.0f, A * 255.0f + 0.5f, 255.0f ) );
				if ( a8 == 0U )
					continue;
				colour[o] = ( a8 << 24 )
					| ( quint32( qBound( 0, int( acc[o * 4] * inv * 255.0f + 0.5f ), 255 ) ) << 16 )
					| ( quint32( qBound( 0, int( acc[o * 4 + 1] * inv * 255.0f + 0.5f ), 255 ) ) << 8 )
					| quint32( qBound( 0, int( acc[o * 4 + 2] * inv * 255.0f + 0.5f ), 255 ) );
				if ( partial[o] )
					out.blendTexels++;
			}
		}
		int wrote = 0, dwrote = 0;
		for ( size_t o = 0; o < colour.size(); o++ )
			if ( colour[o] >> 24 ) {
				wrote++;
				if ( decalHere.contains( o ) )
					dwrote++;
			}
		out.texels += wrote;
		out.decalTexels += dwrote;
		decalHere.clear();
	}

	/*! What the GATHER found -- placements, distinct models, and the refusals
	 *  that happened before any tile existed. The caller adds this ONCE per
	 *  bake; `rasterise` accumulates only the per-tile fields, so a shape that
	 *  spans four tiles is four `shapes` and one `meshes`. */
	const LodgenRoadCensus & gatherCensus() const { return cen; }

private:
	void addPlacement( const EsmWorld & world, const QString & dataRoot,
		QSet<QString> & loaded, quint32 base, const Vector3 & pos,
		const Matrix & rot, float scale )
	{
		const EsmLodBase & lb = world.lodBase( base );
		if ( std::memcmp( &lb.type, "STAT", 4 ) != 0 )
			return;
		if ( !lodgenIsRoadModel( lb.model ) )
			return;
		/* THE RAISED FAMILIES. A base that carries its own Distant LOD mesh is
		 * DRAWN at distance as an object; painting it into the ground as well
		 * draws it twice, once in the air where it stands and once flattened on
		 * the soil beneath. `lodgenIsRaisedRoadModel` closes the 22 shipped
		 * HighwayOverpass and Bridge bases that carry no MNAM at all. */
		if ( !raised && ( lb.hasLod || lodgenIsRaisedRoadModel( lb.model ) ) ) {
			cen.refusedRaised++;
			const QString rk = lb.model.toLower();
			if ( !raisedSeen.contains( rk ) ) {
				raisedSeen.insert( rk );
				cen.raisedBases++;
				cen.addRefusal( lb.hasLod ? "raised-haslod" : "raised-folder", lb.model );
			}
			return;
		}
		/* THE PAVEMENTS. Measured on chunk (-8,8), 15,696 sidewalk texels more
		 * than two texels from any flat road: vanilla's own sheet sits 0.102
		 * BELOW its displaced-mask floor there in brightness, ours cleared the
		 * same floor by 0.284, and our mean luminance was 128.4 against
		 * vanilla's 86.5 -- 42 units. The flat road family on the same tile
		 * matches vanilla's clearance to 0.001. So pavements are out by
		 * default; `--road-sidewalks` (and `--roads-legacy`) put them back. */
		if ( !sidewalks && lodgenIsSidewalkModel( lb.model ) ) {
			cen.refusedSidewalk++;
			const QString sk = lb.model.toLower();
			if ( !sidewalkSeen.contains( sk ) ) {
				sidewalkSeen.insert( sk );
				cen.sidewalkBases++;
				cen.addRefusal( "sidewalk", lb.model );
			}
			return;
		}
		cen.placements++;
		const QVector<LodSrcShape> & src = lodgenLoadModel( dataRoot, lb.model, modelCache );
		const QString key = lb.model.toLower();
		if ( src.isEmpty() ) {
			if ( !loaded.contains( key ) ) {
				loaded.insert( key );
				cen.refusedNoLoad++;
				cen.addRefusal( "would-not-load", lb.model );
			}
			return;
		}
		if ( !loaded.contains( key ) ) {
			loaded.insert( key );
			cen.meshes++;
		}
		for ( const LodSrcShape & s : src ) {
			if ( s.tris.isEmpty() || s.pos.isEmpty() )
				continue;
			LodgenRoadShape out;
			out.pos.resize( s.pos.size() );
			for ( int k = 0; k < s.pos.size(); k++ )
				out.pos[k] = pos + rot * ( s.pos[k] * scale );
			out.uv = s.uv;
			out.col = s.col;
			out.tris = s.tris;
			const LodgenRoadMat & m = material( dataRoot, s );
			out.tex0 = m.read && !m.tex0.isEmpty() ? m.tex0 : s.tex0;
			out.groundMat = m.ground;
			if ( out.groundMat )
				cen.groundShapes++;
			out.decal = m.read ? m.decal : s.matDecal;
			const bool matTest = m.read ? m.alphaTest : s.matAlphaTest;
			out.alphaTest = matTest || ( s.hasAlpha && ( s.alphaFlags & 0x0200 ) );
			out.alphaBlend = ( m.read ? m.alphaBlend : s.matAlphaBlend )
				|| ( s.hasAlpha && ( s.alphaFlags & 0x0001 ) && !out.alphaTest );
			out.alphaRef = matTest ? ( m.read ? m.alphaRef : float( s.matAlphaRef ) / 255.0f )
				: float( s.alphaThreshold ) / 255.0f;
			out.bx0 = out.bx1 = out.pos[0][0];
			out.by0 = out.by1 = out.pos[0][1];
			double zsum = 0.0;
			for ( const Vector3 & v : out.pos ) {
				out.bx0 = qMin( out.bx0, v[0] ); out.bx1 = qMax( out.bx1, v[0] );
				out.by0 = qMin( out.by0, v[1] ); out.by1 = qMax( out.by1, v[1] );
				zsum += double( v[2] );
			}
			out.meanZ = float( zsum / double( out.pos.size() ) );
			shapes.append( out );
		}
	}

	/*! The shape's material, read through `lodgenRoadMaterialPath` so an
	 *  absolute Bethesda build path resolves, cached per material name. */
	const LodgenRoadMat & material( const QString & dataRoot, const LodSrcShape & s )
	{
		static const LodgenRoadMat none;
		if ( s.matName.isEmpty() )
			return none;
		const QString key = s.matName.toLower();
		auto it = matCache.constFind( key );
		if ( it != matCache.constEnd() )
			return *it;
		LodgenRoadMat m;
		/* Set from the NAME, before the file is opened, so a ground material
		 * that will not load is still classified as ground rather than
		 * silently falling into the road surface. */
		m.ground = lodgenRoadMaterialIsGround( s.matName );
		QByteArray bytes;
		if ( lodgenReadAsset( dataRoot, lodgenRoadMaterialPath( s.matName ),
			"materials", ".bgsm", bytes ) ) {
			const ShaderMaterial sm( bytes );
			if ( sm.isValid() ) {
				const QStringList & t = sm.textures();
				if ( !t.isEmpty() )
					m.tex0 = t[0];
				m.decal = sm.hasDecal();
				m.alphaTest = sm.hasAlphaTest();
				m.alphaBlend = sm.hasAlphaBlend();
				m.alphaRef = float( sm.alphaTestThreshold() ) / 255.0f;
				m.read = true;
			}
		}
		return *matCache.insert( key, m );
	}

	QHash<QString, QVector<LodSrcShape>> modelCache;
	QHash<QString, LodgenRoadMat> matCache;
	QVector<LodgenRoadShape> shapes;
	LodgenRoadCensus cen;
	//! Include the raised families. True is ROADS1's behaviour and the way back.
	bool raised = true;
	QSet<QString> raisedSeen;
	//! Include `Landscape\\Sidewalks\\`. True is ROADS1's behaviour and the way back.
	bool sidewalks = true;
	QSet<QString> sidewalkSeen;
	mutable QSet<size_t> decalHere;
};

/*! THE OBJECT HEIGHT FIELD (lane GROUND1) -- one float per 128x128 world units,
 *  holding the TOP of the placed geometry over that square, or `NONE`.
 *
 *  THE LATTICE IS WORLD-ALIGNED AND EXACT: the index is
 *  `floor( world / 128 )`, a division by a power of two, with no chunk-relative
 *  origin anywhere. That is what makes one chunk baked alone and the same chunk
 *  baked inside a region put the same triangles in the same squares, which is
 *  the incremental identity gate and the thread-identity gate both.
 *
 *  THE OPERATOR IS max-Z, which is commutative and associative over the
 *  triangles, so gather order and worker count cannot reach a byte. It is the
 *  same argument `LodgenRoadSet`'s `RoadMaxZ` rule stands on.
 *
 *  128 UNITS IS NOT A TUNING KNOB: it is the horizon march's own first step
 *  (`dist = 128.0f`) and it is LAND's own height spacing, so a finer lattice
 *  would buy the march nothing and the object field and the terrain field agree
 *  about what one sample means.
 *
 *  WHAT GOES IN: the base record's level-0 distant-LOD mesh, `models[0]`, the
 *  mesh the far ring actually draws. A base with no MNAM row at all is refused
 *  by name -- it is not on screen at distance, so it must not shadow at
 *  distance. `hasLod` is NOT the test: it is set from the MNAM rows and a base
 *  can carry the flag with an empty slot 0, so the slot itself is read, with
 *  the remaining three slots as the fallback. */
class LodgenObjectHeightField
{
public:
	static constexpr float CELL = 128.0f;
	//! "no object over this square". Compared with `< SENTINEL_TEST`, never ==.
	static constexpr float NONE = -1.0e30f;
	static constexpr float SENTINEL_TEST = -1.0e29f;
	/*! The same sentinel for the MIN plane, at the other end. It is never the
	 *  emptiness test: a square is empty when its MAX is `< SENTINEL_TEST`, on
	 *  the max plane alone, exactly as before the min plane existed. */
	static constexpr float NONE_LOW = 1.0e30f;

	bool empty() const { return cen.squares == 0; }
	const LodgenObjectAoCensus & gatherCensus() const { return cen; }

	/*! TEST-ONLY: build a synthetic square lattice, for
	 *  `lodgenObjectSlabSelfTest` (lane SLAB1, 2026-09-18) and nothing else.
	 *
	 *  It exists so the known-answer control can read a field it knows the
	 *  answer for through the SHIPPED march, and it fills the lattice through
	 *  the SHIPPED writer `spanInto` rather than touching the planes, so the
	 *  control exercises the same two planes a triangle fills. The bake never
	 *  calls it, and it is the only reason any member of this class is reachable
	 *  from outside it. */
	void seedSyntheticForSelfTest( int originX, int originY, int n,
		float lo, float hi, bool halfPlaneOnly )
	{
		gx0 = originX;
		gy0 = originY;
		gw = n;
		gh = n;
		grid.assign( size_t( n ) * size_t( n ), NONE );
		gridMin.assign( size_t( n ) * size_t( n ), NONE_LOW );
		for ( int gy = gy0; gy < gy0 + gh; gy++ )
			for ( int gx = gx0; gx < gx0 + gw; gx++ ) {
				if ( halfPlaneOnly && gx < 0 )
					continue;
				spanInto( gx, gy, lo );
				spanInto( gx, gy, hi );
			}
		cen = LodgenObjectAoCensus();
		for ( float v : grid )
			if ( v > SENTINEL_TEST )
				cen.squares++;
	}

	/*! Walk the cell rectangle GROWN by two cells -- `LodgenRoadSet`'s own
	 *  margin, and correct here for the same reason plus one more: two cells is
	 *  8,192 units, comfortably over the 1,458-unit march reach plus the largest
	 *  LOD mesh extent, and a placement gathered and then found to be out of
	 *  reach costs one bounds test. */
	void gather( const EsmWorld & world, const QString & dataRoot,
		int cx0, int cy0, int cx1, int cy1 )
	{
		const int margin = 2;
		gx0 = ( cx0 - margin ) * 32;
		gy0 = ( cy0 - margin ) * 32;
		gw = ( cx1 + margin + 1 - ( cx0 - margin ) ) * 32;
		gh = ( cy1 + margin + 1 - ( cy0 - margin ) ) * 32;
		if ( gw <= 0 || gh <= 0 )
			return;
		grid.assign( size_t( gw ) * size_t( gh ), NONE );
		gridMin.assign( size_t( gw ) * size_t( gh ), NONE_LOW );
		QSet<QString> loaded;
		for ( int cy = cy0 - margin; cy <= cy1 + margin; cy++ ) {
			for ( int cx = cx0 - margin; cx <= cx1 + margin; cx++ ) {
				for ( const EsmRefr & r : world.refrs( cx, cy ) ) {
					if ( r.initiallyDisabled || r.deleted || !r.base )
						continue;
					Matrix rm;
					rm.fromEuler( -r.rot[0], -r.rot[1], -r.rot[2] );
					const Vector3 rp( r.pos[0], r.pos[1], r.pos[2] );
					if ( std::memcmp( &r.baseType, "SCOL", 4 ) == 0 ) {
						for ( const EsmScolPart & part : world.scolParts( r.base ) )
							for ( const EsmScolPlacement & pl : part.placements ) {
								Matrix pm;
								pm.fromEuler( -pl.rot[0], -pl.rot[1], -pl.rot[2] );
								addPlacement( world, dataRoot, loaded, part.base,
									rp + rm * ( Vector3( pl.pos[0], pl.pos[1], pl.pos[2] )
										* r.scale ),
									rm * pm, r.scale * pl.scale );
							}
						continue;
					}
					addPlacement( world, dataRoot, loaded, r.base, rp, rm, r.scale );
				}
			}
		}
		for ( float v : grid )
			if ( v > SENTINEL_TEST )
				cen.squares++;
		countSlabSquares( world, cx0 - margin, cy0 - margin, cx1 + margin, cy1 + margin );
	}

	/*! The lattice as a file (LodgenCoverOptions::dumpObjectAoPath). Written
	 *  once per gather, never read back by the bake. */
	bool dump( const QString & path ) const
	{
		QFile f( path );
		if ( !f.open( QIODevice::WriteOnly ) )
			return false;
		const qint32 hdr[4] = { qint32( gx0 ), qint32( gy0 ), qint32( gw ), qint32( gh ) };
		const float cell = CELL;
		f.write( "OBJH", 4 );
		f.write( reinterpret_cast<const char *>( hdr ), sizeof( hdr ) );
		f.write( reinterpret_cast<const char *>( &cell ), sizeof( cell ) );
		if ( !grid.empty() )
			f.write( reinterpret_cast<const char *>( grid.data() ),
				qint64( grid.size() * sizeof( float ) ) );
		/* The MIN plane, appended (see LodgenCoverOptions::dumpObjectAoPath).
		 * A DEBUG FILE, not a shipped format: the bake never reads it back, and
		 * 24 + n*4 against 24 + n*8 tells a reader which version it holds. */
		if ( !gridMin.empty() )
			f.write( reinterpret_cast<const char *>( gridMin.data() ),
				qint64( gridMin.size() * sizeof( float ) ) );
		f.close();
		return true;
	}

	//! Top of the placed geometry over world (wx,wy), or `NONE`. Nearest square,
	//! never interpolated: interpolating a max-Z field invents roof heights that
	//! no geometry has, and the march steps 128 units anyway.
	float topAt( float wx, float wy ) const
	{
		const int gx = int( std::floor( wx / CELL ) ) - gx0;
		const int gy = int( std::floor( wy / CELL ) ) - gy0;
		if ( gx < 0 || gy < 0 || gx >= gw || gy >= gh )
			return NONE;
		return grid[size_t( gy ) * size_t( gw ) + size_t( gx )];
	}

	/*! The object surface SPAN over world (wx,wy): the lowest and the highest
	 *  placed surface of the same square, same nearest-square rule as
	 *  `topAt`, never interpolated.
	 *
	 *  `hi < SENTINEL_TEST` means no object over that square; `lo` is then the
	 *  NONE_LOW sentinel and means nothing, so the caller tests `hi` first. */
	void spanAt( float wx, float wy, float & lo, float & hi ) const
	{
		const int gx = int( std::floor( wx / CELL ) ) - gx0;
		const int gy = int( std::floor( wy / CELL ) ) - gy0;
		if ( gx < 0 || gy < 0 || gx >= gw || gy >= gh ) {
			lo = NONE_LOW;
			hi = NONE;
			return;
		}
		const size_t o = size_t( gy ) * size_t( gw ) + size_t( gx );
		lo = gridMin[o];
		hi = grid[o];
	}

private:
	/*! One surface height into one square: the MAX plane and the MIN plane in
	 *  the same call, so both planes are written by exactly the same set of
	 *  samples and a square can never carry a min from a surface whose max it
	 *  never saw. (It replaces `maxInto`, which wrote the max alone.) */
	void spanInto( int gx, int gy, float z )
	{
		gx -= gx0;
		gy -= gy0;
		if ( gx < 0 || gy < 0 || gx >= gw || gy >= gh )
			return;
		const size_t o = size_t( gy ) * size_t( gw ) + size_t( gx );
		float & hi = grid[o];
		if ( z > hi )
			hi = z;
		float & lo = gridMin[o];
		if ( z < lo )
			lo = z;
	}

	/*! THE CENSUS WORD `objAoSlabSquares`: occupied squares whose LOWEST object
	 *  surface stands more than one cell above the ESM terrain under them.
	 *
	 *  Counted here, once, against the heightfield itself, and NOT kept as a
	 *  third plane: the terrain comes one cell at a time and is thrown away. It
	 *  is 0 on a region with no elevated deck and above 0 wherever the ceiling
	 *  term can act, which is what makes it a proof that the slab path ran
	 *  rather than a restatement of `squares`.
	 *
	 *  UNITS: `EsmLand::heights` is in GAME units and so is the lattice -- the
	 *  same units, no conversion (MISTAKES 2026-09-18 05:0x: when you copy a
	 *  cast, copy its units). A square centre sits between four LAND nodes 128
	 *  units apart, so the terrain under it is their mean.
	 *
	 *  The bar is ONE CELL, 128 units, and it is the march own step: a cover
	 *  less than one step above the ground cannot be told from the ground by a
	 *  march that samples at 128 units, so it does not count as a slab. */
	void countSlabSquares( const EsmWorld & world, int cx0, int cy0, int cx1, int cy1 )
	{
		if ( grid.empty() || gridMin.empty() )
			return;
		EsmLand land;
		for ( int cy = cy0; cy <= cy1; cy++ ) {
			for ( int cx = cx0; cx <= cx1; cx++ ) {
				if ( !world.land( cx, cy, land ) || !land.valid )
					continue;
				for ( int sy = 0; sy < 32; sy++ ) {
					const int gy = cy * 32 + sy - gy0;
					if ( gy < 0 || gy >= gh )
						continue;
					for ( int sx = 0; sx < 32; sx++ ) {
						const int gx = cx * 32 + sx - gx0;
						if ( gx < 0 || gx >= gw )
							continue;
						const size_t o = size_t( gy ) * size_t( gw ) + size_t( gx );
						if ( grid[o] < SENTINEL_TEST )
							continue;
						const float hg = 0.25f * ( land.heights[sy][sx]
							+ land.heights[sy][sx + 1]
							+ land.heights[sy + 1][sx]
							+ land.heights[sy + 1][sx + 1] );
						if ( gridMin[o] > hg + CELL )
							cen.slabSquares++;
					}
				}
			}
		}
	}

	void addPlacement( const EsmWorld & world, const QString & dataRoot,
		QSet<QString> & loaded, quint32 base, const Vector3 & pos,
		const Matrix & rot, float scale )
	{
		const EsmLodBase & lb = world.lodBase( base );
		QString model = lb.models[0];
		for ( int k = 1; k < 4 && model.isEmpty(); k++ )
			model = lb.models[k];
		if ( model.isEmpty() ) {
			/* NO DISTANT LOD MESH -> NOT DRAWN AT DISTANCE -> DOES NOT SHADOW AT
			 * DISTANCE. Refused by name, because a silent skip is the thing that
			 * cannot be audited afterwards. The key is the FULL model so two
			 * bases sharing a full mesh are counted once. */
			cen.refusedNoLod++;
			const QString nk = lb.model.toLower();
			if ( !nk.isEmpty() && !noLodSeen.contains( nk ) ) {
				noLodSeen.insert( nk );
				cen.noLodBases++;
				cen.addRefusal( "no-lod-mesh", lb.model );
			}
			return;
		}
		cen.placements++;
		const QVector<LodSrcShape> & src = lodgenLoadModel( dataRoot, model, modelCache );
		const QString key = model.toLower();
		if ( src.isEmpty() ) {
			if ( !loaded.contains( key ) ) {
				loaded.insert( key );
				cen.refusedNoLoad++;
				cen.addRefusal( "would-not-load", model );
			}
			return;
		}
		if ( !loaded.contains( key ) ) {
			loaded.insert( key );
			cen.meshes++;
		}
		for ( const LodSrcShape & sh : src ) {
			if ( sh.tris.isEmpty() || sh.pos.isEmpty() )
				continue;
			std::vector<Vector3> wp( size_t( sh.pos.size() ) );
			for ( int k = 0; k < sh.pos.size(); k++ )
				wp[size_t( k )] = pos + rot * ( sh.pos[k] * scale );
			for ( const Triangle & t : sh.tris ) {
				if ( int( t.v1() ) >= sh.pos.size() || int( t.v2() ) >= sh.pos.size()
					|| int( t.v3() ) >= sh.pos.size() )
					continue;
				const Vector3 & a = wp[size_t( t.v1() )];
				const Vector3 & b = wp[size_t( t.v2() )];
				const Vector3 & c = wp[size_t( t.v3() )];
				cen.triangles++;
				/* TWO RULES, both max-Z, and the union is what "top-down" means
				 * here. The SCAN puts the triangle's own plane height into every
				 * lattice centre the triangle covers; the VERTEX SEED puts each
				 * corner's height into the square that corner stands in, so a
				 * pole or a railing thinner than 128 units cannot vanish between
				 * two centres and leave a gap in the shadow it should cast. */
				spanInto( int( std::floor( a[0] / CELL ) ), int( std::floor( a[1] / CELL ) ), a[2] );
				spanInto( int( std::floor( b[0] / CELL ) ), int( std::floor( b[1] / CELL ) ), b[2] );
				spanInto( int( std::floor( c[0] / CELL ) ), int( std::floor( c[1] / CELL ) ), c[2] );
				const float e = ( b[0] - a[0] ) * ( c[1] - a[1] )
					- ( b[1] - a[1] ) * ( c[0] - a[0] );
				if ( e == 0.0f )
					continue;   // degenerate seen from above; the corners are in
				const int lx0 = int( std::floor( qMin( a[0], qMin( b[0], c[0] ) ) / CELL ) );
				const int lx1 = int( std::floor( qMax( a[0], qMax( b[0], c[0] ) ) / CELL ) );
				const int ly0 = int( std::floor( qMin( a[1], qMin( b[1], c[1] ) ) / CELL ) );
				const int ly1 = int( std::floor( qMax( a[1], qMax( b[1], c[1] ) ) / CELL ) );
				for ( int gy = ly0; gy <= ly1; gy++ ) {
					for ( int gx = lx0; gx <= lx1; gx++ ) {
						const float px = ( float( gx ) + 0.5f ) * CELL;
						const float py = ( float( gy ) + 0.5f ) * CELL;
						const float w0 = ( ( b[0] - a[0] ) * ( py - a[1] )
							- ( b[1] - a[1] ) * ( px - a[0] ) ) / e;
						const float w1 = ( ( c[0] - b[0] ) * ( py - b[1] )
							- ( c[1] - b[1] ) * ( px - b[0] ) ) / e;
						const float w2 = ( ( a[0] - c[0] ) * ( py - c[1] )
							- ( a[1] - c[1] ) * ( px - c[0] ) ) / e;
						if ( w0 < 0.0f || w1 < 0.0f || w2 < 0.0f )
							continue;
						// barycentric: w1 is A's weight, w2 is B's, w0 is C's
						spanInto( gx, gy, a[2] * w1 + b[2] * w2 + c[2] * w0 );
					}
				}
			}
		}
	}

	QHash<QString, QVector<LodSrcShape>> modelCache;
	std::vector<float> grid;      //!< MAX object surface a square, `NONE` where empty
	std::vector<float> gridMin;   //!< MIN object surface a square, `NONE_LOW` where empty
	int gx0 = 0, gy0 = 0, gw = 0, gh = 0;
	QSet<QString> noLodSeen;
	LodgenObjectAoCensus cen;
};

/*! The object term's sky visibility for one texel, in ONE place, so the stock
 *  composite and the pyramid tile cannot drift apart (the two composites rule,
 *  docs/LODGEN_TERRAIN_VT.md).
 *
 *  The march is the SAME eight directions and the SAME seven steps as the
 *  terrain one beside it -- 128, 192, 288, 432, 648, 972, 1458 -- read against
 *  the object tops instead of the ground, with `h0` left as the TERRAIN height
 *  the texel actually stands on.
 *
 *  THE SLAB LATTICE (lane SLAB1, 2026-09-18; `terrainObjectAoSlab`, default on).
 *  A square is read as a WALL when its lowest object surface reaches down to
 *  `h0` or below, and as a CEILING when its whole span stands above `h0`. A
 *  wall blocks the sweep from the horizon up to its own elevation, as it always
 *  did; a ceiling blocks from the elevation of its nearest escape UP TO THE
 *  ZENITH, which is a smaller set the further up it is -- so the ground under
 *  an overpass deck 1,000 units up is lit from the sides instead of reading as
 *  the inside of a solid block. Measured on chunk 4.4.-12: the mask sheet B
 *  under the elevated highway (world x 19712..20992, y -41856..-40576, 100
 *  lattice squares all holding max Z 2416.0) was 57.3 of 255.
 *
 *  The two laws cross at `sqrt( 128 * 1458 ) = 432` units of clearance: above
 *  it the slab reading is brighter than the max-Z reading, below it darker,
 *  because a cover that low really does shut the sky out and the max-Z reading
 *  was letting it off. That is the law, not a tuning choice.
 *
 *  It returns exactly `1.0f` when nothing occludes, by an early return and not
 *  by arithmetic, so the caller's `vis * 1.0f` is bitwise `vis` and the AO byte
 *  cannot move on a region with no occluder. */
static float lodgenObjectSkyVis( const LodgenObjectHeightField & f,
	float wx, float wy, float h0, const float dirs[8][2], float strength,
	bool slab )
{
	if ( strength <= 0.0f )
		return 1.0f;
	float occl = 0.0f;
	for ( int k = 0; k < 8; k++ ) {
		/* `wall` is the old `maxSlope`, under its meaning: the steepest
		 * elevation, as a tangent, of anything standing on the ground in this
		 * direction. It blocks the sweep from the horizon UP to itself. */
		float wall = 0.0f;
		/* `ceilOpen` is the tangent of the LOWEST escape under a cover that
		 * passes overhead: the cover blocks from there UP TO THE ZENITH. It is
		 * only collected while every step from the nearest outward has been a
		 * ceiling square (`covered`), so a canopy 1,000 units away, which does
		 * not pass over this sample at all, contributes nothing here and
		 * shades through its trunk square as a wall exactly as before. */
		float ceilOpen = 0.0f;
		bool haveCeil = false;
		bool covered = true;
		for ( float dist = 128.0f; dist <= 2048.0f; dist *= 1.5f ) {
			float lo = 0.0f, hi = 0.0f;
			f.spanAt( wx + dirs[k][0] * dist, wy + dirs[k][1] * dist, lo, hi );
			if ( hi < LodgenObjectHeightField::SENTINEL_TEST ) {
				covered = false;
				continue;
			}
			if ( !slab || lo <= h0 ) {
				/* A WALL -- its geometry reaches down to the sample own level
				 * or below: a building side, a rock, a pier pile, a tree
				 * trunk. This branch IS the whole of the old loop, which is
				 * why `slab` off is the old bytes and not an approximation of
				 * them. */
				covered = false;
				const float dh = hi - h0;
				if ( dh > 0.0f )
					wall = qMax( wall, dh / dist );
			} else if ( covered ) {
				const float open = ( lo - h0 ) / dist;
				if ( !haveCeil || open < ceilOpen ) {
					ceilOpen = open;
					haveCeil = true;
				}
			}
		}
		/* THE SUM, NOT THE MAX. The two blocked sets are `0 .. F(wall)` from
		 * the horizon and `F(ceilOpen) .. 1` from the zenith; the measure of
		 * their union is the sum, capped at 1 where they meet, so a wall
		 * standing under a ceiling still blocks everything.
		 *
		 * When no ceiling was seen the accumulator takes `wall/(1+wall)`
		 * itself -- no addition, no qMin -- so a direction with no ceiling
		 * square produces the identical float it produced before. */
		const float wallBlocked = wall / ( 1.0f + wall );
		if ( !haveCeil ) {
			occl += wallBlocked;
		} else {
			const float ceilBlocked = 1.0f - ceilOpen / ( 1.0f + ceilOpen );
			occl += qMin( 1.0f, wallBlocked + ceilBlocked );
		}
	}
	if ( occl == 0.0f )
		return 1.0f;
	return qBound( 0.0f, 1.0f - occl / 8.0f * 1.6f * strength, 1.0f );
}

/*! THE SLAB LAW'S KNOWN-ANSWER CONTROL (lane SLAB1, 2026-09-18), run once per
 *  process when `WW_OBJAO_SLAB_TEST` is set in the environment.
 *
 *  Three synthetic height fields built BY HAND -- no ESM, no mesh, no bake --
 *  and read through the shipped `lodgenObjectSkyVis` itself, so what is pinned
 *  is the function the sheets are written with and not a second copy of the
 *  formula:
 *
 *    PLATE  every square occupied with min == max == H: an infinite ceiling H
 *           units over the sample. All seven steps of all eight directions are
 *           ceilings, the nearest escape is the FURTHEST step (1458), and the
 *           value is  1 - 0.8 * ( 1 - F( H / 1458 ) )  at strength 0.5, where
 *           F(s) = s/(1+s). Under the old law it is 1 - 0.8 * F( H / 128 ),
 *           because the max-Z reading takes the NEAREST step's slope.
 *    WALL   every square occupied with min far below the sample and max = H:
 *           geometry that reaches the ground. Every square takes the wall
 *           branch, so the two laws must return the SAME float, exactly.
 *    EDGE   the plate over the half plane x >= 0 only: five of the eight
 *           directions are covered, three see open sky.
 *
 *  THE BARS, pre-registered in the lane report section 2.5 BEFORE this code was
 *  written, and the brief's own bar among them:
 *
 *    H = 1000  slab centre  0.525468  (old 0.290780)   -- bar > 0.5
 *    H =  200  slab centre  0.296494  (old 0.512195)   -- the brief asked for
 *              > 0.3 here and the law gives 0.296494; the bar is REFUSED WITH
 *              THE NUMBER, because 200 units of clearance is BELOW the
 *              crossover sqrt( 128 * 1458 ) = 432 where the two laws meet, and
 *              below it a cover really does shut the sky out. The test pins the
 *              measured value instead, and pins that it is DARKER than the old
 *              reading, which is the law's other side and must not silently
 *              change.
 *    H =  500  wall         0.363057 under BOTH laws, difference exactly 0
 *    H = 1000  edge         0.703418, strictly between the slab centre and 1
 *
 *  The pre-registered figures were computed at DOUBLE precision (the lane's
 *  Python reimplementation, report section 5.4); the shipped function sums
 *  eight floats and lands on 0.296502 and 0.703417, so every bar above is
 *  checked to 1.0e-5 and not for equality. The one comparison that IS exact is
 *  the `slab = false` control at the end, and it is exact because it is written
 *  as the same eight-float accumulation.
 *
 *  THE REFUTER: every bar is also evaluated with `slab = false`, the old
 *  reading, on the same field. The plate bars must FAIL there -- if they pass,
 *  the bars do not discriminate between the two laws and this test says so and
 *  fails. */
static bool lodgenObjectSlabSelfTest()
{
	auto build = [] ( float lo, float hi, bool halfPlaneOnly ) {
		LodgenObjectHeightField f;
		// 81 x 81 squares centred on the sample: 40 * 128 = 5,120 units each
		// way, well past the march's own 1,458-unit reach, so no direction runs
		// off the lattice and reads an out-of-range square as empty
		f.seedSyntheticForSelfTest( -40, -40, 81, lo, hi, halfPlaneOnly );
		return f;
	};

	static const float dirs[8][2] = {
		{ 1.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f },
		{ 0.7071f, 0.7071f }, { 0.7071f, -0.7071f },
		{ -0.7071f, 0.7071f }, { -0.7071f, -0.7071f }
	};
	const float SX = 64.0f, SY = 64.0f, H0 = 0.0f, ST = 0.5f;

	int checks = 0, bad = 0;
	auto expectNear = [&] ( const char * what, float got, float want, float tol ) {
		checks++;
		if ( qAbs( got - want ) > tol ) {
			bad++;
			fprintf( stderr, "slab:   FAIL %s = %.6f, expected %.6f +- %g\n",
				what, got, want, tol );
		} else {
			fprintf( stderr, "slab:   ok   %s = %.6f\n", what, got );
		}
	};
	auto expectAbove = [&] ( const char * what, float got, float bar ) {
		checks++;
		if ( !( got > bar ) ) {
			bad++;
			fprintf( stderr, "slab:   FAIL %s = %.6f, wanted > %.6f\n", what, got, bar );
		} else {
			fprintf( stderr, "slab:   ok   %s = %.6f > %.6f\n", what, got, bar );
		}
	};
	auto expectRed = [&] ( const char * what, float got, float bar ) {
		checks++;
		if ( got > bar ) {
			bad++;
			fprintf( stderr, "slab:   FAIL REFUTER %s = %.6f, which PASSES the bar"
				" %.6f: the bar does not discriminate between the two laws\n",
				what, got, bar );
		} else {
			fprintf( stderr, "slab:   ok   REFUTER %s = %.6f, which the bar %.6f"
				" REFUSES\n", what, got, bar );
		}
	};

	fprintf( stderr, "slab: self-test the ceiling law, three synthetic fields"
		" (WW_OBJAO_SLAB_TEST), strength %.2f, sample (%.0f,%.0f) at h0 %.0f\n",
		ST, SX, SY, H0 );

	// ---- PLATE, 1000 units up
	{
		const LodgenObjectHeightField f = build( 1000.0f, 1000.0f, false );
		const float nw = lodgenObjectSkyVis( f, SX, SY, H0, dirs, ST, true );
		const float od = lodgenObjectSkyVis( f, SX, SY, H0, dirs, ST, false );
		expectNear( "PLATE H=1000, slab law", nw, 0.525468f, 1.0e-5f );
		expectAbove( "PLATE H=1000, slab law, above the bar", nw, 0.5f );
		expectNear( "PLATE H=1000, old max-Z law", od, 0.290780f, 1.0e-5f );
		expectRed( "PLATE H=1000 under the OLD law against the same bar", od, 0.5f );
	}

	// ---- PLATE, 200 units up: the law's OTHER side, below the 432 crossover
	{
		const LodgenObjectHeightField f = build( 200.0f, 200.0f, false );
		const float nw = lodgenObjectSkyVis( f, SX, SY, H0, dirs, ST, true );
		const float od = lodgenObjectSkyVis( f, SX, SY, H0, dirs, ST, false );
		expectNear( "PLATE H=200, slab law (the brief's > 0.3 bar is REFUSED"
			" WITH THIS NUMBER: 200 < the 432 crossover)", nw, 0.296494f, 1.0e-5f );
		expectNear( "PLATE H=200, old max-Z law", od, 0.512195f, 1.0e-5f );
		checks++;
		if ( !( nw < od ) ) {
			bad++;
			fprintf( stderr, "slab:   FAIL PLATE H=200 must be DARKER under the"
				" slab law (%.6f) than under the old one (%.6f)\n", nw, od );
		} else {
			fprintf( stderr, "slab:   ok   PLATE H=200 is darker under the slab law"
				" (%.6f < %.6f), which is the crossover at 432 working\n", nw, od );
		}
	}

	// ---- WALL, geometry that reaches the ground: the two laws must agree EXACTLY
	{
		const LodgenObjectHeightField f = build( -4096.0f, 500.0f, false );
		const float nw = lodgenObjectSkyVis( f, SX, SY, H0, dirs, ST, true );
		const float od = lodgenObjectSkyVis( f, SX, SY, H0, dirs, ST, false );
		expectNear( "WALL H=500, slab law", nw, 0.363057f, 1.0e-5f );
		checks++;
		if ( nw != od ) {
			bad++;
			fprintf( stderr, "slab:   FAIL WALL H=500 the two laws differ:"
				" %.9f against %.9f (difference %g)\n", nw, od, double( nw - od ) );
		} else {
			fprintf( stderr, "slab:   ok   WALL H=500 the two laws return the SAME"
				" float, %.9f, difference exactly 0\n", nw );
		}
	}

	// ---- EDGE: the plate over the half plane x >= 0, five directions of eight
	{
		const LodgenObjectHeightField f = build( 1000.0f, 1000.0f, true );
		const float nw = lodgenObjectSkyVis( f, SX, SY, H0, dirs, ST, true );
		expectNear( "EDGE H=1000, slab law", nw, 0.703418f, 1.0e-5f );
		checks++;
		if ( !( nw > 0.525468f && nw < 1.0f ) ) {
			bad++;
			fprintf( stderr, "slab:   FAIL EDGE %.6f is not strictly between the"
				" slab centre 0.525468 and open sky 1.0\n", nw );
		} else {
			fprintf( stderr, "slab:   ok   EDGE %.6f is strictly between the slab"
				" centre 0.525468 and open sky 1.0\n", nw );
		}
	}

	// ---- the switch off is the old law, on every one of the three fields
	{
		const LodgenObjectHeightField a = build( 1000.0f, 1000.0f, false );
		const LodgenObjectHeightField b = build( 200.0f, 200.0f, false );
		const LodgenObjectHeightField c = build( -4096.0f, 500.0f, false );
		int same = 0;
		for ( const LodgenObjectHeightField * f : { &a, &b, &c } ) {
			const float off = lodgenObjectSkyVis( *f, SX, SY, H0, dirs, ST, false );
			/* THE OLD LOOP, WRITTEN OUT A SECOND TIME ON PURPOSE, through
			 * `topAt` -- the max-plane accessor this lane did not touch -- and
			 * accumulating the eight directions in the same order and the same
			 * float precision. A closed form (8 * x, or a double sum) is NOT
			 * equivalent: it agrees to six decimals and then differs in the
			 * seventh, which would make an == comparison lie about which of the
			 * two readings it is testing. */
			float occl = 0.0f;
			for ( int k = 0; k < 8; k++ ) {
				float mx = 0.0f;
				for ( float dist = 128.0f; dist <= 2048.0f; dist *= 1.5f ) {
					const float top = f->topAt( SX + dirs[k][0] * dist,
						SY + dirs[k][1] * dist );
					if ( top < LodgenObjectHeightField::SENTINEL_TEST )
						continue;
					const float dh = top - H0;
					if ( dh > 0.0f )
						mx = qMax( mx, dh / dist );
				}
				occl += mx / ( 1.0f + mx );
			}
			const float want = occl == 0.0f ? 1.0f
				: qBound( 0.0f, 1.0f - occl / 8.0f * 1.6f * ST, 1.0f );
			if ( off == want )
				same++;
			else
				fprintf( stderr, "slab:   FAIL slab=false is not the old formula:"
					" %.9f against %.9f\n", off, want );
		}
		checks++;
		if ( same != 3 ) {
			bad++;
		} else {
			fprintf( stderr, "slab:   ok   slab=false reproduces the old formula"
				" exactly on all three fields\n" );
		}
	}

	fprintf( stderr, "slab: self-test %d checks, %d failures, %s\n", checks, bad,
		bad ? "RESULT FAIL" : "RESULT PASS" );
	return bad == 0;
}

/*! Runs it once per process, and only when asked. */
static void lodgenObjectSlabSelfTestOnce()
{
	static bool done = false;
	if ( done || qgetenv( "WW_OBJAO_SLAB_TEST" ).isEmpty() )
		return;
	done = true;
	lodgenObjectSlabSelfTest();
}

/*! THE EROSION LATTICE (lane GROUND1 Part B, 2026-09-12).
 *
 *  bungo, 2026-09-11 over `cmp_msn_2024.png`: "We lose all the fluvial, erosion
 *  features and other topographical features". The LAND record holds one height
 *  per 128 units; everything finer in vanilla's far sheets came from source
 *  terrain Bethesda never shipped. Gate F1 measured what is missing on 22 of
 *  vanilla's own `_msn` sheets: 40.6 % of the gradient field's variance lives
 *  finer than our 128-unit grid, at a fine-gradient SD of 0.276, aligned ACROSS
 *  the local slope by a factor of 1.479 against a phase-twin floor of 1.025,
 *  and growing with the slope at r = +0.500 against a twin floor of -0.014.
 *  This class grows that relief back.
 *
 *  STATIC-PATH DROPLETS, and the textbook loop is deliberately not used. A
 *  droplet that re-reads the surface it is carving depends on every droplet
 *  before it AND on the extent of the array it runs in, which puts thread
 *  identity, single-chunk-versus-region identity and chunk-border seamlessness
 *  out of reach by construction. Here each droplet traces steepest descent on a
 *  STATIC field -- the shared height reconstruction plus world-seeded value
 *  noise -- so its contribution is a pure function of its world start position.
 *  The delta at a world node is then a sum of the same per-droplet terms in the
 *  same relative order whichever lattice computes it, and a lattice with a
 *  wider extent cannot perturb a narrower one's partial sums, because a droplet
 *  that never reaches a node never adds to it at all.
 *
 *  What that costs, and it is a real cost: without the incision feedback the
 *  pass cannot deepen a channel and then re-route into it, so it does not build
 *  a dendritic network the way a feedback loop does. What it does build is
 *  CONVERGENCE -- steepest descent on a noisy slope braids and collects into the
 *  same lines and the cutting concentrates there.
 *
 *  THE LATTICE STEP IS THE SHEET'S OWN TEXEL, not a fixed world size. At dim 4
 *  that is 32 units, which is exactly the resolution gate F1's numbers were
 *  measured at. It bounds the memory at every dim (a dim-32 chunk on a 32-unit
 *  lattice would be 19 million cells) and it keeps the channels the same size on
 *  screen as the LOD coarsens. The price is that the same ground carries
 *  differently scaled channels at different dims, so an LOD change can pop; that
 *  was not measured. The lattice is still world-aligned -- indices come from
 *  `floor( w / step )` on absolute world coordinates, never from an array offset
 *  -- so two chunks at the same dim agree cell for cell where they meet.
 *
 *  The seed noise NEVER reaches the output. It exists only so that steepest
 *  descent on a smooth slope has something to braid around; what leaves this
 *  class is the erosion delta and the two masks the droplets wrote.
 */
class LodgenErosionField
{
public:
	//! The droplet's hard step cap. The border below is sized from it: a droplet
	//! cannot influence anything more than this many cells from where it started,
	//! so a border of this size means every droplet that can reach the interior
	//! is present in the interior's own lattice.
	static constexpr int MAX_STEPS = 32;
	/*! The HIGH-PASS RADIUS, in lattice cells. After the droplets have run,
	 *  the local mean of the delta over a box of this radius is SUBTRACTED
	 *  from it, so what survives is fine relief with no bulk left in it.
	 *
	 *  Two reasons, and the second is the load-bearing one:
	 *
	 *  1. The droplets drain into basins and the basins fill. Measured before
	 *     this filter: a mean |delta| of 26 world units with a single cell
	 *     taking 2,264 -- eighty-six times the mean, a pile, not a channel.
	 *  2. THE LOD TERRAIN MUST NOT DRIFT FROM THE LOADED TERRAIN. The full-
	 *     resolution cells are not eroded and never will be; anything this
	 *     pass adds at a scale the eye can see across the LOD boundary is a
	 *     seam. A high pass makes the net height change over any patch wider
	 *     than 2R+1 cells exactly zero, so the drift is bounded by
	 *     construction rather than by taste.
	 *
	 *  8 cells is 256 world units at dim 4 -- two LAND grid squares, and well
	 *  above the 3.5-texel channel spacing F1 read off vanilla, so the
	 *  channels themselves pass through untouched. */
	static constexpr int HP_RADIUS = 8;
	/*! The hard ceiling on one cell's finished delta, in CELL-NORMALISED
	 *  height: 2 lattice steps, 64 world units at dim 4, against a measured
	 *  mean |delta| of 25. It is there because a PIT FILLS. Every droplet in
	 *  a convergence point's catchment -- up to MAX_STEPS cells of it, some
	 *  three thousand cells -- lays its load in the same few cells, and the
	 *  measured result was one cell holding 2,264 world units of fill while
	 *  the field around it averaged 25. The high pass below does not touch
	 *  that, because a one-cell spike is exactly what a high pass keeps. */
	static constexpr float DELTA_CLAMP = 6.0f;
	/*! The nucleation noise, in CELL-NORMALISED height. It exists so that
	 *  steepest descent on a smooth hillside has something to braid around;
	 *  it must stay well UNDER the terrain's own slope per cell or the
	 *  droplets follow the noise instead of the hill and the pass stops
	 *  being fluvial. Measured: at 0.45 the sheets read an across/along
	 *  anisotropy of 0.99 against vanilla's 1.48 -- the amplitude was right
	 *  and the ALIGNMENT was gone, because vanilla's own mean gradient is
	 *  0.396, i.e. 0.396 of a cell of height per cell of ground, and the
	 *  noise was larger than the hill it sat on. */
	static constexpr float NOISE_AMP = 0.08f;
	/*! THE FIT. The droplet model above is written in its own units and it
	 *  has no idea how much relief a Fallout 4 LOD sheet wants; this one
	 *  number carries it, and it is the only number in the class that was
	 *  chosen by measurement rather than taken from the model.
	 *
	 *  Fitted in gate F3 against the four medians F1 read off 22 vanilla
	 *  `_msn` sheets, on eight of our own sheets over two four-chunk regions,
	 *  at 4 rounds and seed 7. At this value and --erosion 1 the sheets read
	 *  fine SD 0.261 against vanilla's 0.276 (-5.6 %, inside the 7.1 %
	 *  that two ADJACENT vanilla sheets differ by), fine share 0.496 against
	 *  0.406 (+22.1 %, inside that ceiling's 24.9 %) and across/along
	 *  anisotropy 1.180 against 1.479 (-20.2 %, inside the brief's 30 % bar
	 *  and outside the ceiling's 16.7 %). The fourth, the correlation of
	 *  fine amplitude with coarse slope, reads +0.290 against vanilla's
	 *  +0.500 and is REPORTED RED in section B4: the sign and the floor are
	 *  right (its phase twin reads -0.015) and the magnitude is not.
	 *
	 *  Putting it here rather than at the two consumers means --erosion 1 is
	 *  the fitted picture and the census numbers are the world units that
	 *  actually reach the sheets. */
	static constexpr float FIT = 0.10f;
	/*! The border carries the WHOLE dependency reach, so an interior cell
	 *  reads only cells whose own sums are complete.
	 *
	 *  It grows with the round count because the rounds feed back. A cell
	 *  after round r depends on droplets launched up to r * MAX_STEPS cells
	 *  away -- round 1 cuts the grooves, round 2's droplets choose their
	 *  path by those grooves, and so on -- plus the filter's own reach. */
	static int borderFor( int rounds )
	{
		return rounds * MAX_STEPS + HP_RADIUS;
	}

	bool empty() const { return delta.empty(); }
	float cellSize() const { return cell; }
	const LodgenErosionCensus & census() const { return cen; }

	/*! `ringW`/`ringS` are the world coordinates of the height grid's own
	 *  south-west corner, so `( wx - ringW ) / 128` is the grid coordinate the
	 *  shared reconstruction takes -- the same expression both `_msn` writers
	 *  already use. `wx0`/`wy0`/`w`/`h` are the sheet's own texel rect; the
	 *  lattice is grown by `borderFor( rounds )` cells on every side of it.
	 *  `dropsPerCell` is the ROUND count, clamped to 1..8 here. */
	void build( const std::vector<float> & hgt, int hn,
		float ringW, float ringS, float step,
		float wx0, float wy0, int w, int h,
		int dropsPerCell, quint32 seed )
	{
		if ( step <= 0.0f || w <= 0 || h <= 0 || dropsPerCell <= 0 )
			return;
		cell = step;
		const int rounds = qBound( 1, dropsPerCell, 8 );
		const int bd = borderFor( rounds );
		gx0 = int( std::floor( double( wx0 ) / double( step ) ) ) - bd;
		gy0 = int( std::floor( double( wy0 ) / double( step ) ) ) - bd;
		gw = w + 2 * bd;
		gh = h + 2 * bd;
		const size_t n = size_t( gw ) * size_t( gh );
		field.assign( n, 0.0f );
		delta.assign( n, 0.0f );
		round.assign( n, 0.0f );

		/* The static field: the surface the `_msn` already encodes, plus the
		 * nucleation noise. The noise period is 3 cells because F1 read
		 * vanilla's channel spacing at 3.5 texels -- a number that sits ON its
		 * own phase-twin floor, so it is used here as a fit target and never as
		 * evidence that vanilla has channels of that size. */
		for ( int j = 0; j < gh; j++ ) {
			for ( int i = 0; i < gw; i++ ) {
				const float wx = ( float( gx0 + i ) + 0.5f ) * cell;
				const float wy = ( float( gy0 + j ) + 0.5f ) * cell;
				const float h0 = lodgenTerrainHeightAt( hgt, hn,
					( wx - ringW ) / 128.0f, ( wy - ringS ) / 128.0f );
				/* CELL-NORMALISED HEIGHT: world height divided by the lattice step,
				 * so a difference between two neighbouring cells IS the slope and
				 * the droplet constants below are the dimensionless numbers they
				 * were written as. The delta comes back out in world units by the
				 * one multiplication by `cell` at each splat. Leaving the height
				 * in world units here is what made the first run of gate F2 read
				 * a mean |delta| of 13,327 units on ground a few thousand units
				 * tall: the slope was a height, and the splat scaled it again. */
				field[size_t( j ) * gw + i] = h0 / cell
					+ noise( gx0 + i, gy0 + j, seed, 3 ) * NOISE_AMP;
			}
		}

		cen.cells = qint64( n );
		cen.step = cell;
		/* THE ROUNDS. One droplet per cell per round, traced on the field as
		 * it stood at the START of the round and never on the field it is
		 * itself changing, so the order droplets are visited in cannot reach
		 * a byte. Between rounds the round's cuts and fills are folded INTO
		 * the field, and that is what makes the pass fluvial: round 2's water
		 * finds round 1's grooves and deepens them instead of laying a second
		 * independent scribble beside them. With one round and no feedback
		 * the sheets measured an across/along anisotropy of 0.81-0.99 against
		 * vanilla's 1.48: the right amount of relief, pointing nowhere.
		 *
		 * WORLD ORDER inside a round, south to north then west to east, so two
		 * lattices that overlap visit their shared squares in the same relative
		 * order. That, plus a border of `rounds * MAX_STEPS`, is the identity
		 * argument: a droplet that cannot reach a cell never changes it, in any
		 * round. */
		for ( int r = 0; r < rounds; r++ ) {
			std::fill( round.begin(), round.end(), 0.0f );
			for ( int j = 0; j < gh; j++ ) {
				for ( int i = 0; i < gw; i++ )
					drop( gx0 + i, gy0 + j, quint32( r ), seed );
			}
			for ( size_t t = 0; t < n; t++ ) {
				delta[t] += round[t];
				field[t] += round[t] / cell;
			}
		}

		/* Spikes first, bulk second: clamping after the high pass would leave
		 * the mean the spike dragged with it. */
		const float dcap = DELTA_CLAMP * cell;
		for ( size_t t = 0; t < delta.size(); t++ )
			delta[t] = qBound( -dcap, delta[t], dcap );
		highPass();

		for ( size_t t = 0; t < delta.size(); t++ )
			delta[t] *= FIT;

		double sum = 0.0;
		qint64 moved = 0;
		for ( size_t t = 0; t < n; t++ ) {
			const float d = delta[t];
			if ( d != 0.0f ) {
				moved++;
				sum += std::fabs( double( d ) );
				cen.maxCut = qMin( cen.maxCut, double( d ) );
				cen.maxFill = qMax( cen.maxFill, double( d ) );
			}
		}
		cen.moved = moved;
		cen.meanAbs = moved ? sum / double( moved ) : 0.0;
	}

	//! The erosion height delta in world units, bilinear, 0 outside the lattice.
	float deltaAt( float wx, float wy ) const { return tap( delta, wx, wy ); }

	/*! The crevice term's operand, in the SAME form and with the same sign as
	 *  the one `lodgenShadeWithCrevice` reads off vanilla's detail normal: the
	 *  divergence of that normal's horizontal components. A detail normal's
	 *  east component is -d(delta)/dx, so that divergence is minus the
	 *  Laplacian of the delta, differenced at the step the gradient uses.
	 *  Keeping the form means the coefficient TILING3 fitted on seven vanilla
	 *  sheets (-3.242 levels) keeps its meaning here instead of needing its
	 *  own fit against a corpus that does not contain this pass. */
	float creviceAt( float wx, float wy, float d ) const
	{
		const float s = qMax( d, cell );
		const float c = deltaAt( wx, wy );
		const float lap = deltaAt( wx + 2.0f * s, wy ) + deltaAt( wx - 2.0f * s, wy )
			+ deltaAt( wx, wy + 2.0f * s ) + deltaAt( wx, wy - 2.0f * s ) - 4.0f * c;
		return -lap / ( 2.0f * s );
	}

	/*! The gradient the `_msn` writers add to their own, as a CENTRAL DIFFERENCE
	 *  AT `d` WORLD UNITS -- the sheet's own texel size, floored at the lattice
	 *  step. That one choice anti-aliases the term: at dim 4 a texel is the
	 *  lattice step and the sheet sees the channels at full amplitude; at dim 16
	 *  a texel is 128 units and a central difference over 128 units averages
	 *  112-unit channels away by itself, which is the right answer, because
	 *  relief finer than a texel cannot be shown on that texel. */
	void gradAt( float wx, float wy, float d, float * dgx, float * dgy ) const
	{
		const float s = qMax( d, cell );
		*dgx = ( deltaAt( wx + s, wy ) - deltaAt( wx - s, wy ) ) / ( 2.0f * s );
		*dgy = ( deltaAt( wx, wy + s ) - deltaAt( wx, wy - s ) ) / ( 2.0f * s );
	}

private:
	//! Value noise on a lattice `period` cells coarse, quintic-eased, in [-1,1].
	static float noise( int gx, int gy, quint32 seed, int period )
	{
		const float fx = float( gx ) / float( period );
		const float fy = float( gy ) / float( period );
		const int ix = int( std::floor( fx ) ), iy = int( std::floor( fy ) );
		auto ease = []( float t ) {
			return t * t * t * ( t * ( t * 6.0f - 15.0f ) + 10.0f );
		};
		const float tx = ease( fx - float( ix ) ), ty = ease( fy - float( iy ) );
		auto at = []( int x, int y, quint32 s ) {
			quint32 h = s;
			h ^= quint32( x ) * 0x9E3779B1U;
			h = ( h << 13 ) | ( h >> 19 );
			h *= 0x85EBCA77U;
			h ^= quint32( y ) * 0xC2B2AE3DU;
			h = ( h << 17 ) | ( h >> 15 );
			h *= 0x27D4EB2FU;
			h ^= h >> 15;
			h *= 0x2545F491U;
			h ^= h >> 13;
			return float( h & 0x00FFFFFFU ) / float( 0x00800000U ) - 1.0f;
		};
		const float a = at( ix, iy, seed ), b = at( ix + 1, iy, seed );
		const float c = at( ix, iy + 1, seed ), e = at( ix + 1, iy + 1, seed );
		return ( a * ( 1.0f - tx ) + b * tx ) * ( 1.0f - ty )
			+ ( c * ( 1.0f - tx ) + e * tx ) * ty;
	}

	static quint32 hash3( int x, int y, quint32 k, quint32 seed )
	{
		quint32 h = seed ^ 0x9E3779B9U;
		h ^= quint32( x ) * 0x85EBCA77U;
		h = ( h << 11 ) | ( h >> 21 );
		h ^= quint32( y ) * 0xC2B2AE3DU;
		h = ( h << 7 ) | ( h >> 25 );
		h ^= k * 0x27D4EB2FU;
		h ^= h >> 16;
		h *= 0x7FEB352DU;
		h ^= h >> 15;
		h *= 0x846CA68BU;
		h ^= h >> 16;
		return h;
	}

	float tap( const std::vector<float> & f, float wx, float wy ) const
	{
		if ( f.empty() )
			return 0.0f;
		const float px = float( double( wx ) / double( cell ) ) - float( gx0 ) - 0.5f;
		const float py = float( double( wy ) / double( cell ) ) - float( gy0 ) - 0.5f;
		if ( px < 0.0f || py < 0.0f || px >= float( gw - 1 ) || py >= float( gh - 1 ) )
			return 0.0f;
		const int i = int( px ), j = int( py );
		const float tx = px - float( i ), ty = py - float( j );
		const size_t o = size_t( j ) * gw + i;
		return ( f[o] * ( 1.0f - tx ) + f[o + 1] * tx ) * ( 1.0f - ty )
			+ ( f[o + gw] * ( 1.0f - tx ) + f[o + gw + 1] * tx ) * ty;
	}

	//! The static field and its gradient at a fractional LATTICE position.
	bool sample( float px, float py, float * h, float * gx, float * gy ) const
	{
		if ( px < 0.0f || py < 0.0f || px >= float( gw - 1 ) || py >= float( gh - 1 ) )
			return false;
		const int i = int( px ), j = int( py );
		const float tx = px - float( i ), ty = py - float( j );
		const size_t o = size_t( j ) * gw + i;
		const float a = field[o], b = field[o + 1];
		const float c = field[o + gw], e = field[o + gw + 1];
		*h = ( a * ( 1.0f - tx ) + b * tx ) * ( 1.0f - ty )
			+ ( c * ( 1.0f - tx ) + e * tx ) * ty;
		*gx = ( b - a ) * ( 1.0f - ty ) + ( e - c ) * ty;
		*gy = ( c - a ) * ( 1.0f - tx ) + ( e - b ) * tx;
		return true;
	}

	/*! Subtract the local mean of the delta over a box of HP_RADIUS cells.
	 *  Separable, two passes, edge cells averaging over what they have -- the
	 *  border is thrown away and never read by either sheet. */
	void highPass()
	{
		if ( delta.empty() || HP_RADIUS <= 0 )
			return;
		const int R = HP_RADIUS;
		std::vector<float> tmp( delta.size(), 0.0f ), avg( delta.size(), 0.0f );
		for ( int j = 0; j < gh; j++ ) {
			double run = 0.0;
			for ( int i = 0; i <= qMin( R, gw - 1 ); i++ )
				run += double( delta[size_t( j ) * gw + i] );
			for ( int i = 0; i < gw; i++ ) {
				const int lo = qMax( 0, i - R ), hi = qMin( gw - 1, i + R );
				tmp[size_t( j ) * gw + i] = float( run / double( hi - lo + 1 ) );
				if ( i + R + 1 < gw )
					run += double( delta[size_t( j ) * gw + i + R + 1] );
				if ( i - R >= 0 )
					run -= double( delta[size_t( j ) * gw + i - R] );
			}
		}
		for ( int i = 0; i < gw; i++ ) {
			double run = 0.0;
			for ( int j = 0; j <= qMin( R, gh - 1 ); j++ )
				run += double( tmp[size_t( j ) * gw + i] );
			for ( int j = 0; j < gh; j++ ) {
				const int lo = qMax( 0, j - R ), hi = qMin( gh - 1, j + R );
				avg[size_t( j ) * gw + i] = float( run / double( hi - lo + 1 ) );
				if ( j + R + 1 < gh )
					run += double( tmp[size_t( j + R + 1 ) * gw + i] );
				if ( j - R >= 0 )
					run -= double( tmp[size_t( j - R ) * gw + i] );
			}
		}
		for ( size_t t = 0; t < delta.size(); t++ )
			delta[t] -= avg[t];
	}

	//! A 3x3 weighted splat, so no single cell takes a whole droplet's cut.
	void splat( std::vector<float> & f, float px, float py, float amount )
	{
		const int i = int( px + 0.5f ), j = int( py + 0.5f );
		for ( int dy = -1; dy <= 1; dy++ ) {
			for ( int dx = -1; dx <= 1; dx++ ) {
				const int x = i + dx, y = j + dy;
				if ( x < 0 || y < 0 || x >= gw || y >= gh )
					continue;
				const float wgt = ( dx == 0 && dy == 0 ) ? 0.25f
					: ( ( dx == 0 || dy == 0 ) ? 0.125f : 0.0625f );
				f[size_t( y ) * gw + x] += amount * wgt;
			}
		}
	}

	void drop( int wcx, int wcy, quint32 k, quint32 seed )
	{
		const quint32 hs = hash3( wcx, wcy, k, seed );
		float px = float( wcx - gx0 ) + float( hs & 0xFFFFU ) / 65536.0f;
		float py = float( wcy - gy0 ) + float( ( hs >> 16 ) & 0xFFFFU ) / 65536.0f;
		float dirx = 0.0f, diry = 0.0f;
		float speed = 1.0f, water = 1.0f, sediment = 0.0f;
		float h0 = 0.0f, gx = 0.0f, gy = 0.0f;
		if ( !sample( px, py, &h0, &gx, &gy ) )
			return;
		for ( int s = 0; s < MAX_STEPS; s++ ) {
			dirx = dirx * INERTIA - gx * ( 1.0f - INERTIA );
			diry = diry * INERTIA - gy * ( 1.0f - INERTIA );
			const float len = std::sqrt( dirx * dirx + diry * diry );
			if ( len < 1.0e-6f )
				break;
			dirx /= len;
			diry /= len;
			const float nx = px + dirx, ny = py + diry;
			float h1 = 0.0f, ngx = 0.0f, ngy = 0.0f;
			if ( !sample( nx, ny, &h1, &ngx, &ngy ) )
				break;
			const float dh = h1 - h0;
			const float cap = qMax( -dh, MIN_SLOPE ) * speed * water * CAPACITY;
			if ( dh > 0.0f || sediment > cap ) {
				const float amount = qMin( MAX_MOVE, ( dh > 0.0f )
					? qMin( dh, sediment )
					: ( sediment - cap ) * DEPOSIT );
				if ( amount > 0.0f ) {
					sediment -= amount;
					splat( round, px, py, amount * cell );
				}
			} else {
				const float amount = qMin( MAX_MOVE,
					qMin( ( cap - sediment ) * ERODE, -dh ) );
				if ( amount > 0.0f ) {
					sediment += amount;
					splat( round, px, py, -amount * cell );
				}
			}
			speed = qMin( MAX_SPEED,
				std::sqrt( qMax( 0.0f, speed * speed + ( -dh ) * GRAVITY ) ) );
			water *= ( 1.0f - EVAPORATE );
			px = nx;
			py = ny;
			h0 = h1;
			gx = ngx;
			gy = ngy;
			if ( water < 0.01f )
				break;
		}
	}

	/* The droplet constants. They are ordinary hydraulic-erosion parameters and
	 * none of them was invented here; what IS this lane's is the fit of the one
	 * knob in front of them (`--erosion`), reported in section B3. */
	static constexpr float INERTIA = 0.05f;
	static constexpr float CAPACITY = 1.0f;
	static constexpr float MIN_SLOPE = 0.01f;
	static constexpr float ERODE = 0.3f;
	static constexpr float DEPOSIT = 0.3f;
	static constexpr float GRAVITY = 4.0f;
	/* The speed cap. Without it `speed` grows on every downhill step and the
	 * carrying capacity grows with it, so one droplet on one long slope
	 * arrives carrying more material than the slope holds: the first
	 * measured run put 41,385 world units of fill on a single cell. */
	static constexpr float MAX_SPEED = 4.0f;
	/* The most one droplet may cut or lay at one step, in CELL-NORMALISED
	 * height -- 0.125 of the lattice step, 4 world units at dim 4. This is
	 * the line between a detail pass and a terrain generator. Without it a
	 * cliff, whose slope is several cells of height per cell of ground,
	 * hands one droplet a capacity of hundreds of world units and the pass
	 * reshapes the mountain instead of scoring it: measured max fill 5,020
	 * world units against a mean |delta| of 44. It is a CONSTANT, applied
	 * per step, so it changes no identity argument. */
	static constexpr float MAX_MOVE = 0.500f;
	static constexpr float EVAPORATE = 0.02f;

	std::vector<float> field, delta, round;
	int gx0 = 0, gy0 = 0, gw = 0, gh = 0;
	float cell = 32.0f;
	LodgenErosionCensus cen;
};

} // namespace

bool lodgenBakeTerrainTextures( const EsmWorld & world, int chunkX, int chunkY,
	int dim, const QString & dataRoot, const QString & outDir,
	const LodgenCoverOptions & coverOpts, LodgenBakeCaches * caches, QString * error )
{
	lodgenTerrainRingSelfTestOnce();
	lodgenObjectSlabSelfTestOnce();
	auto fail = [error]( const QString & message ) {
		if ( error )
			*error = message;
		return false;
	};
	constexpr int RES = 512;
	/* World-space units per repeat of a landscape diffuse. 341.3333 = 128/0.375
	 * is the engine's own tiling, read out of Fallout4.exe 1.10.155 at
	 * 0x1403A74C6 / 0x1403A7620 (lane SPLAT1); `--land-tiling 2048` restores the
	 * pre-2026-09-11 bake byte for byte. Every TILE site below reads this. */
	const float TILE = lodgenLandTiling();
	const float span = float( dim ) * 4096.0f;
	const float cwX = float( chunkX ) * 4096.0f, cwY = float( chunkY ) * 4096.0f;

	/* Per-cell land data, loaded once -- and the HEIGHTS on the one-cell ring.
	 *
	 * The PAINT (`cells`, `haveLand`, `dominantBase`, the quadrant cover
	 * constants) stays scoped to the chunk: every texel this bake writes lands
	 * inside it, so a neighbour's paint has nothing to contribute and widening
	 * that scope would move the dominant base. The HEIGHTS are different. Both
	 * neighbourhood operators below reach outside the chunk, and until
	 * 2026-09-10 they were served a CLAMP there -- the chunk's own edge sample
	 * repeated outwards, a plateau that is not the ground. That clamp is what
	 * made this bake and the pyramid's disagree on the same chunk (V9a: 43
	 * colour texels and 5,524 msn texels on Commonwealth.4.-24.24, every one of
	 * them within 4 texels of the chunk edge) and it is what put a 7.312 step
	 * across a chunk seam where the ringed bake has 4.955. The ring here is the
	 * tile baker's ring, through the shared filler, not a second copy of it. */
	const int rdim = dim + 2 * LODGEN_TERRAIN_RING_CELLS;
	const int rx0 = chunkX - LODGEN_TERRAIN_RING_CELLS;
	const int ry0 = chunkY - LODGEN_TERRAIN_RING_CELLS;
	const int hn = rdim * 32 + 1;
	std::vector<EsmLand> cells( size_t( dim ) * dim );
	std::vector<bool> haveLand( size_t( dim ) * dim, false );
	std::vector<float> hgt;
	{
		EsmLand ringLand;
		lodgenTerrainFillRing( hgt, hn, rdim, world.defaultLandHeight(),
			[&]( int cx, int cy ) -> const EsmLand * {
				const int ix = cx - LODGEN_TERRAIN_RING_CELLS;
				const int iy = cy - LODGEN_TERRAIN_RING_CELLS;
				const bool inChunk = ( ix >= 0 && ix < dim && iy >= 0 && iy < dim );
				EsmLand & land = inChunk ? cells[size_t( iy ) * dim + ix] : ringLand;
				if ( !world.land( rx0 + cx, ry0 + cy, land ) )
					return nullptr;
				if ( inChunk )
					haveLand[size_t( iy ) * dim + ix] = true;
				return &land;
			} );
	}
	/* The CHUNK'S OWN view of that grid, for the per-sample channels.
	 *
	 * They are held to the chunk deliberately and it is not an oversight: the
	 * wetness channel is a flow accumulation over the WHOLE grid it is handed,
	 * so widening the grid moves the sheet's interior, not its edge. That is a
	 * different defect from the clamp, it cannot be gated by byte identity
	 * against the pyramid (whose tiles accumulate over a tile-sized grid, not a
	 * chunk-sized one), and it is named as owed rather than changed here. */
	const int cn = dim * 32 + 1;
	std::vector<float> chgt( size_t( cn ) * cn );
	for ( int row = 0; row < cn; row++ )
		for ( int col = 0; col < cn; col++ )
			chgt[size_t( row ) * cn + col] =
				hgt[size_t( row + 32 * LODGEN_TERRAIN_RING_CELLS ) * hn
					+ size_t( col + 32 * LODGEN_TERRAIN_RING_CELLS )];

	/* The texture cache belongs to the CALLER when it has one: the pyramid
	 * bakes four times as many units over the same ground, and a local cache
	 * would re-decode every landscape diffuse per unit. A null caller owns one
	 * for this call, which is what the per-chunk path has always done. */
	LodgenBakeCaches * ownCaches = caches ? nullptr : lodgenCreateBakeCaches();
	struct CacheGuard
	{
		LodgenBakeCaches * p;
		~CacheGuard() { if ( p ) lodgenDestroyBakeCaches( p ); }
	} cacheGuard{ ownCaches };
	LodgenBakeCaches & bc = caches ? *caches : *ownCaches;

	std::vector<quint32> diffuse( size_t( RES ) * RES, 0xFF808080U );
	std::vector<quint32> msn( size_t( RES ) * RES, LODGEN_MSN_FLAT );
	// diagnostics: where the bake falls back to the flat default
	int statNoLand = 0, statNoBase = 0, statNoTex = 0;
	QSet<quint32> failedLtex;

	/* ROADS. Gathered and scan-converted BEFORE the texel loop, because the
	 * loop asks one question per texel and the answer is a lookup. Off, the
	 * plane is never allocated, no REFR is read and no road model is opened --
	 * which is what makes `--no-roads` byte-identical rather than identical by
	 * argument. */
	LodgenRoadCensus roadCensus;
	std::vector<quint32> roadPlane;
	if ( coverOpts.roads ) {
		LodgenRoadSet roads;
		roads.gather( world, dataRoot, chunkX, chunkY,
			chunkX + dim - 1, chunkY + dim - 1, coverOpts.roadRaised,
			coverOpts.roadSidewalks );
		roadCensus.add( roads.gatherCensus() );
		roads.rasterise( cwX, cwY + span, span / float( RES ), RES,
			roadPlane, bc, dataRoot, roadCensus,
			coverOpts.roadComposite, coverOpts.roadDetail,
			coverOpts.roadGroundPaint );
	}

	/* THE OBJECT HEIGHT FIELD (lane GROUND1). Gathered over the chunk grown by
	 * the field's own two-cell margin; inert and never allocated while the
	 * switch is off, so the three sheets are byte for byte what they are today. */
	std::unique_ptr<LodgenObjectHeightField> objField;
	LodgenObjectAoCensus objCensus;
	if ( coverOpts.terrainObjectAo ) {
		objField.reset( new LodgenObjectHeightField );
		objField->gather( world, dataRoot, chunkX, chunkY,
			chunkX + dim - 1, chunkY + dim - 1 );
		objCensus.add( objField->gatherCensus() );
		if ( !coverOpts.dumpObjectAoPath.isEmpty() )
			objField->dump( coverOpts.dumpObjectAoPath );
	}

	/* THE EROSION LATTICE (lane GROUND1 Part B). Built once per chunk over the
	 * sheet's own texel grid grown by the droplet's step cap, from the SAME
	 * height reconstruction the `_msn` below encodes. At --erosion 0 nothing
	 * is allocated and the branch at the normal is never entered, so the
	 * sheets are byte for byte what they were. */
	std::unique_ptr<LodgenErosionField> eroField;
	LodgenErosionCensus eroCensus;
	if ( lodgenErosion() > 0.0f ) {
		eroField.reset( new LodgenErosionField );
		eroField->build( hgt, hn, float( rx0 ) * 4096.0f, float( ry0 ) * 4096.0f,
			span / float( RES ), cwX, cwY, RES, RES,
			lodgenErosionIterations(), lodgenErosionSeed() );
		eroCensus.add( eroField->census() );
		lodgenErosionCensusAdd( eroField->census() );
	}

	/* Ground cover (docs/LODGEN_TERRAIN_VT.md §2). Everything below is inert
	 * when the feature is off: the plane is never allocated, the GRAS chain is
	 * never walked and the tint branch is never entered, so the three sheets
	 * are byte for byte what they are today. */
	const bool doCover = coverOpts.cover;
	const float coverFull = qMax( 1.0f, coverOpts.coverFull );
	std::vector<quint8> coverPlane;
	std::vector<float> aCover;
	int coverMax = 0;
	int statRenorm = 0, statClipPainted = 0, statClipBase = 0;
	int statLtexResolves = 0, statLtexNoGnam = 0, statGrasNoTint = 0, statDanglingGnam = 0;
	float statMaxDtexPainted = 0.0f, statMaxDtexBase = 0.0f;
	qint64 statPaintedPts = 0, statAlphaPts = 0;
	QSet<quint32> danglingLtexIds, danglingGnamIds;
	if ( doCover ) {
		coverPlane.assign( size_t( RES ) * RES, 0 );
		aCover.assign( 64, 0.0f );
	}

	// quadrants painted with no BTXT fall back to the chunk's dominant base
	quint32 dominantBase = 0;
	{
		QMap<quint32, int> counts;
		for ( const EsmLand & land : cells )
			for ( int q = 0; q < 4; q++ )
				if ( land.baseTex[q] )
					counts[land.baseTex[q]]++;
		int best = 0;
		for ( auto it = counts.constBegin(); it != counts.constEnd(); ++it )
			if ( it.value() > best ) { best = it.value(); dominantBase = it.key(); }
	}

	/* Per-quadrant ground-cover constants, resolved ONCE on the quadrant, not
	 * once per texel. D, S and T are per-FORM scalars and the layer set is
	 * constant across a quadrant, so this is at most eight hash lookups per
	 * quadrant; doing it per texel would add well over a billion of them over
	 * a worldspace, on top of the per-texel texture lookup sampleLtex already
	 * performs. The census counts the resolves so that regression fails a
	 * check rather than hiding inside a 24-second parse. */
	struct LtexCoverVals
	{
		float d = 0.0f, s = 0.0f, tintD = 0.0f;
		float t[3] = { 0.0f, 0.0f, 0.0f };
	};
	struct QuadCover
	{
		LtexCoverVals base;
		QVector<LtexCoverVals> layers;
		bool painted = false;       //!< carries at least one ATXT layer
	};
	std::vector<QuadCover> quadCover;
	if ( doCover ) {
		world.setGrassTintResolver( &lodgenGrassTintResolve, &bc );
		quadCover.resize( size_t( dim ) * dim * 4 );
		QSet<quint32> ltexSeen;
		auto resolve = [&]( quint32 form, LtexCoverVals & out, bool reportDangling ) {
			if ( !form )
				return;
			// copied out by VALUE: ltexCover() hands back a reference into a
			// QHash and the next resolve can rehash it
			const EsmLtexCover c = world.ltexCover( form, dataRoot );
			statLtexResolves++;
			const bool firstSight = !ltexSeen.contains( form );
			if ( firstSight )
				ltexSeen.insert( form );
			if ( !c.exists ) {
				/* A layer naming a form that is not a record is a DATA ERROR,
				 * not paint intent, so it contributes nothing and does not fall
				 * back to the dominant base the way a NULL layer does. */
				if ( reportDangling )
					danglingLtexIds.insert( form );
				return;
			}
			if ( firstSight ) {
				if ( c.grasses == 0 && c.danglingGnam == 0 )
					statLtexNoGnam++;
				statGrasNoTint += c.grassesWithoutTint;
				statDanglingGnam += c.danglingGnam;
				if ( c.danglingGnam )
					danglingGnamIds.insert( form );
			}
			out.d = float( c.density );
			out.s = c.maxSlope;
			out.tintD = float( c.tintDensity );
			for ( int k = 0; k < 3; k++ )
				out.t[k] = c.tint[k];
		};
		for ( size_t ci = 0; ci < cells.size(); ci++ ) {
			if ( !haveLand[ci] )
				continue;
			const EsmLand & land = cells[ci];
			for ( int q = 0; q < 4; q++ ) {
				QuadCover & qc = quadCover[ci * 4 + q];
				resolve( land.baseTex[q] ? land.baseTex[q] : dominantBase,
					qc.base, land.baseTex[q] != 0 );
				qc.painted = !land.layers[q].isEmpty();
				for ( const EsmLandLayer & layer : land.layers[q] ) {
					LtexCoverVals v;
					// a NULL-LTEX layer paints the chunk's dominant base, exactly
					// as the diffuse loop does, so cover and albedo never disagree
					// about what is growing there
					resolve( layer.ltex ? layer.ltex : dominantBase, v, layer.ltex != 0 );
					qc.layers.append( v );
				}
				if ( qc.painted || land.baseTex[q] ) {
					// the two painted-texel denominators, both printed: painted
					// grid points, and those of them carrying an alpha layer
					statPaintedPts += 289;
					for ( int gy = 0; gy < 17; gy++ ) {
						for ( int gx = 0; gx < 17; gx++ ) {
							for ( const EsmLandLayer & layer : land.layers[q] ) {
								if ( layer.opacity[gy][gx] > 0.0f ) {
									statAlphaPts++;
									break;
								}
							}
						}
					}
				}
			}
		}
	}

	for ( int py = 0; py < RES; py++ ) {
		// image row 0 = V 0 = NORTH edge (v = 1 - y/span in the Land UVs)
		const float wy = cwY + ( 1.0f - ( float( py ) + 0.5f ) / RES ) * span;
		for ( int px = 0; px < RES; px++ ) {
			const float wx = cwX + ( ( float( px ) + 0.5f ) / RES ) * span;
			const int cx = qBound( 0, int( ( wx - cwX ) / 4096.0f ), dim - 1 );
			const int cy = qBound( 0, int( ( wy - cwY ) / 4096.0f ), dim - 1 );
			const size_t ci = size_t( cy ) * dim + cx;

			/* Model-space normal from the heightfield (central differences),
			 * hoisted ABOVE the colour composite: the ground-cover slope gate
			 * needs this same unit normal's Z, and the operand is free exactly
			 * once, in the iteration that already computed it. Nothing here
			 * reads the colour, so the msn bytes are unchanged. */
			/* Grid coordinates on the RING, so the central difference below has
			 * real ground on both sides at the chunk's own edge. The spacing is
			 * 128 world units, which is exactly what `span / ( hn - 1 )` used to
			 * evaluate to -- ( dim * 4096 ) / ( dim * 32 ) -- and both were exact
			 * powers of two, so nothing in the chunk's interior moves a bit. */
			const float ngx = ( wx - cwX + LODGEN_TERRAIN_RING_UNITS ) / 128.0f;
			const float ngy = ( wy - cwY + LODGEN_TERRAIN_RING_UNITS ) / 128.0f;
			const float spacing = 128.0f;
			/* The reconstruction and the channel order are SHARED with the
			 * virtual-texture tile baker -- lodgenTerrainHeightAt and
			 * lodgenTerrainMsnPixel at the top of this file. They were twelve
			 * lines copied into `lodgenBakeVtTile`, and that copy kept BOTH of
			 * the defects below after this one lost them (nearest sampling and
			 * up-in-blue), which is the reason there is now one home.
			 *
			 * BILINEAR, and deliberately not smoother. `int( ngx )` truncated
			 * here until 2026-09-07, so every texel in a 4x4 group read the same
			 * grid point and got the same normal: the sheet carried 129x129
			 * distinct values magnified into 512x512. Measured against vanilla,
			 * the fraction of texels differing from their left neighbour by
			 * x mod 4 was 83.3 / 0.0 / 0.0 / 0.0 for us and 99.8 / 64.4 / 64.7 /
			 * 64.3 for vanilla - every block of ours was flat.
			 *
			 * A Catmull-Rom interpolant was tried next and is WORSE: VHGT stores
			 * heights as accumulated int8 deltas of 8 units, so the field is a
			 * staircase, and a C1 spline overshoots at every step and rings the
			 * grid into the shading across the whole tile. Bilinear turns a step
			 * into a ramp instead. That still holds for every basis with negative
			 * weights. The quintic ease at `heightAt` below is not one of those:
			 * it reweights the SAME four taps, hits every sample exactly and
			 * stays between them, so it cannot ring on the staircase -- and the
			 * staircase turned out not to be what made the squares anyway (the
			 * measurement is at `heightAt`).
			 *
			 * None of this adds detail. Vanilla carries 6.6-8.2x our
			 * high-frequency energy because Bethesda baked from finer terrain
			 * than the 128-unit VHGT grid the ESM ships; this removes our
			 * artefacts, not their missing data. */
			/* THE EASE, and why the plain bilinear blend left a square lattice.
			 *
			 * The central difference below spans a FULL grid step either side,
			 * and for any blend of the four corner samples that is algebraically
			 * the same blend of the GRID-POINT gradients:
			 *
			 *   H(g+1) - H(g-1) = (1-w)(H[i+1]-H[i-1]) + w(H[i+2]-H[i])
			 *
			 * With w = t -- the plain bilinear parameter -- the gradient handed
			 * to the encoder is CONTINUOUS BUT KINKED at every height sample,
			 * i.e. the sheet is creased on the 129-sample grid, which at 512
			 * texels over a dim-4 chunk is a crease every 4 texels in both axes.
			 * bungo, on the peak close-up: "You can see the square pattern on
			 * the right in the terrain, which is not good."
			 *
			 * MEASURED, our own output against vanilla's, tiles 4.-60.36 and
			 * 4.-12.44, mip 0. Take the slope field the sheet encodes, subtract
			 * its own 9x9 box mean, and average |second difference| over the
			 * columns of each residue class of x mod 4; report
			 * (max - min) / mean. Before the codec: 0.947 and 0.830 with the
			 * plain blend, 0.209 and 0.197 with the quintic ease below --
			 * a 78% and 76% fall, and BELOW vanilla's own 0.296 and 0.245. The
			 * raised classes were exactly the two straddling a sample line.
			 *
			 * The ease is quintic, not the cubic smoothstep: the cubic's SECOND
			 * derivative still steps at each sample, and this sheet is a normal
			 * map, which is where such a step shows. (Perlin replaced the cubic
			 * with this quintic in 2002 for that reason. Measured here too: the
			 * cubic reads 0.717, the quintic 0.630, on the same tile through the
			 * same codec.)
			 *
			 * IT DOES NOT BUY SMOOTHNESS WITH BLUR, which is the trap every
			 * other candidate fell into. The ease reweights the blend only; it
			 * still passes through every VHGT sample exactly and, being monotone
			 * on [0,1], still cannot leave the four values it sits between -- so
			 * unlike Catmull-Rom (tried and reverted 2026-09-07) it cannot ring
			 * on the 8-unit staircase, and unlike a B-spline or a pre-blur it
			 * removes nothing. High-frequency energy went UP 25.5% and 26.5% on
			 * the two tiles; the B-spline lost 23.5% and the two pre-blurs 24%
			 * and 57%, which is why they were rejected.
			 *
			 * The 8-unit staircase is NOT the cause and dequantising is not the
			 * fix: on a smooth synthetic field on this same grid, quantising the
			 * heights to 8 units moved the number by 0.000 (1.254 -> 1.254 with
			 * the plain blend, 0.217 -> 0.218 with the ease).
			 *
			 * Every terrain _msn in every worldspace changes with this line. */
			float dzdx = ( lodgenTerrainHeightAt( hgt, hn, ngx + 1.0f, ngy )
				- lodgenTerrainHeightAt( hgt, hn, ngx - 1.0f, ngy ) ) / ( 2.0f * spacing );
			float dzdy = ( lodgenTerrainHeightAt( hgt, hn, ngx, ngy + 1.0f )
				- lodgenTerrainHeightAt( hgt, hn, ngx, ngy - 1.0f ) ) / ( 2.0f * spacing );
			/* THE EROSION TERM. One shared class, two call sites, exactly as
			 * the object AO term above -- a second copy of these lines is
			 * how the two `_msn` writers kept the same two defects for two days
			 * in 2026-09-07. */
			if ( eroField ) {
				float egx = 0.0f, egy = 0.0f;
				eroField->gradAt( wx, wy, span / float( RES ), &egx, &egy );
				dzdx += egx * lodgenErosion();
				dzdy += egy * lodgenErosion();
			}
			Vector3 nrm( -dzdx, -dzdy, 1.0f );
			nrm.normalize();
			msn[size_t( py ) * RES + px] = lodgenTerrainMsnPixel( nrm );

			/* THE MACRO GRADIENT (lane LAND1): the terrain's own low-pass
			 * slope at --land-guide-scale, measured ONCE a texel on the same
			 * ring height grid the normal above comes from, and handed to the
			 * land diffuse lookup below. Off, nothing is computed. */
			LodgenLandGuideCtx lguide;
			lguide.hgt = &hgt;
			lguide.hn = hn;
			lguide.ngOffX = ( LODGEN_TERRAIN_RING_UNITS - cwX ) / 128.0f;
			lguide.ngOffY = ( LODGEN_TERRAIN_RING_UNITS - cwY ) / 128.0f;
			float mgx = 0.0f, mgy = 0.0f;
			if ( lodgenLandGuideRule() != LODGEN_LANDGUIDE_OFF ) {
				double gdx = 0.0, gdy = 0.0;
				lodgenLandMacroGradient( lguide, double( wx ), double( wy ),
					&gdx, &gdy );
				mgx = float( gdx );
				mgy = float( gdy );
			}

			FloatVector4 color( 0.5f, 0.5f, 0.5f, 1.0f );
			int coverByte = 0;
			float coverTintD = 0.0f;
			float coverTint[3] = { 0.0f, 0.0f, 0.0f };
			if ( !haveLand[ci] )
				statNoLand++;
			if ( haveLand[ci] ) {
				const EsmLand & land = cells[ci];
				// quadrant within the cell: 0 BL, 1 BR, 2 TL, 3 TR
				const float lx = ( wx - cwX ) - float( cx ) * 4096.0f;
				const float ly = ( wy - cwY ) - float( cy ) * 4096.0f;
				const int q = ( ly >= 2048.0f ? 2 : 0 ) + ( lx >= 2048.0f ? 1 : 0 );
				const float qx = ( lx - ( q & 1 ? 2048.0f : 0.0f ) ) / 2048.0f;
				const float qy = ( ly - ( q & 2 ? 2048.0f : 0.0f ) ) / 2048.0f;
				auto sampleLtex = [&]( quint32 ltex ) -> FloatVector4 {
					QString d, n;
					world.ltexTextures( ltex, d, n );
					const DDSTexture16 * tex = d.isEmpty() ? nullptr
						: lodgenCachedTexture( bc, dataRoot, d );
					if ( !tex ) {
						statNoTex++;
						failedLtex.insert( ltex );
						return FloatVector4( 0.5f, 0.5f, 0.5f, 1.0f );
					}
					/* THE DOMAIN WARP (lane TILING3), on the land diffuse
					 * lookup and on nothing else: not the footprint, not the
					 * quadrant selection above, not `_msn`, not `.lodl`. Off
					 * by default, and off returns the coordinate untouched. */
					float swx = wx, swy = wy;
					lodgenLandGuidedWarp( lguide, wx, wy, mgx, mgy, &swx, &swy );
					// wrap by hand: getPixelB clamps, and the tiling is ours
					float u = std::fmod( swx / TILE, 1.0f );
					float v = std::fmod( swy / TILE, 1.0f );
					if ( u < 0.0f ) u += 1.0f;
					if ( v < 0.0f ) v += 1.0f;
					/* getPixelB/T take NORMALIZED 0..1 coordinates. Sample at
					 * the mip whose texel matches the bake texel's WORLD
					 * footprint (span/RES units), or the result is tiling
					 * noise instead of the material's local average. */
					const float texelWorld = TILE / float( tex->getWidth() );
					const float footprint = span / float( RES );
					const float maxMip = float( tex->getMaxMipLevel() );
					/* THE MIP BIAS (lane TILING3). Written as a branch, not as
					 * an unconditional `+ bias`, so that at the default the
					 * expression is the one the rung compiled. */
					float mipRaw = std::log2( qMax( 1.0f, footprint / texelWorld ) );
					const float mipBias = lodgenLandMipBias();
					if ( mipBias != 0.0f )
						mipRaw += mipBias;
					const float mip = qBound( 0.0f, mipRaw, maxMip );
					/* THE HEX TILING (lane TILING4), on the land diffuse
					 * lookup and on nothing else. ONE call, at BOTH sites:
					 * off, it evaluates the identical expression this line
					 * used to hold, so the default is the rung's bytes. It
					 * takes the WARP-offset coordinate, so setting both
					 * gives warp-then-hex (measured, and refused as a
					 * default: it costs the swirl and buys no repeat). */
					const FloatVector4 fp =
						lodgenLandHexTap( tex, swx, swy, TILE, u, v, mip, maxMip,
							&lguide );
					if ( !lodgenLandSampleAverage() )
						return fp;
					/* THE REPEAT-AVERAGED SAMPLE (lane TILING2). A landscape
					 * diffuse ships a full mip chain down to 1x1, and one
					 * repeat IS the whole texture, so the 1x1 texel is the
					 * exact average over a repeat and carries no periodic term
					 * at all. --land-detail adds back a fraction of the
					 * footprint sample's departure from that average, which
					 * scales the repeat by exactly the same fraction. */
					const FloatVector4 avg = tex->getPixelT( 0.5f, 0.5f, maxMip );
					const float kDetail = lodgenLandDetail();
					if ( kDetail <= 0.0f )
						return avg;
					return avg + ( fp - avg ) * kDetail;
				};
				/* THE QUADRANT COMPOSITE, lifted out as a function of
				 * (paint, quadrant, quadrant-local position) -- lane TILING2.
				 * The arithmetic is exactly what was inline here; the only
				 * reason it moved is that the edge blend below has to evaluate
				 * it for a NEIGHBOURING quadrant at this same world point.
				 * `own` is true exactly once per texel, for the quadrant the
				 * texel is really in, and only that call touches the cover
				 * array and the counters -- so --cover, the statistics and the
				 * default bake are byte-for-byte what they were. */
				int nLayers = 0;
				auto quadComposite = [&]( const EsmLand & pl, int pq,
						float pqx, float pqy, bool own ) -> FloatVector4 {
					FloatVector4 c( 0.5f, 0.5f, 0.5f, 1.0f );
					const quint32 bt = pl.baseTex[pq] ? pl.baseTex[pq] : dominantBase;
					if ( bt )
						c = sampleLtex( bt );
					else if ( own )
						statNoBase++;
					int nL = 0;
					for ( const EsmLandLayer & layer : pl.layers[pq] ) {
						// bilinear over the 17x17 quadrant opacities
						const float fx = qBound( 0.0f, pqx * 16.0f, 15.999f );
						const float fy = qBound( 0.0f, pqy * 16.0f, 15.999f );
						const int ix = int( fx ), iy = int( fy );
						const float tx = fx - ix, ty = fy - iy;
						const float a =
							( layer.opacity[iy][ix] * ( 1 - tx ) + layer.opacity[iy][ix + 1] * tx ) * ( 1 - ty )
							+ ( layer.opacity[iy + 1][ix] * ( 1 - tx ) + layer.opacity[iy + 1][ix + 1] * tx ) * ty;
						// the cover side sees EVERY layer's opacity, including
						// the ones the colour composite skips as negligible
						if ( own && doCover && nL < int( aCover.size() ) )
							aCover[nL] = qBound( 0.0f, a, 1.0f );
						nL++;
						if ( a <= 0.001f )
							continue;
						// NULL-texture layers paint the engine's hardcoded
						// default ground; the chunk's dominant base is the
						// local stand-in
						const FloatVector4 lc = sampleLtex(
							layer.ltex ? layer.ltex : dominantBase );
						c = c + ( lc - c ) * qBound( 0.0f, a, 1.0f );
					}
					if ( own )
						nLayers = nL;
					return c;
				};
				color = quadComposite( land, q, qx, qy, true );
				if ( lodgenBlendEdges() == 1 ) {
					/* THE QUADRANT CROSS-FADE (lane TILING2).
					 *
					 * Nothing blends across a quadrant line today: the base
					 * texture and the whole layer SET change at every 2,048
					 * units and the 17x17 opacities are bilinear only inside
					 * their own quadrant. Within `margin` units of a line the
					 * neighbouring quadrant's composite is evaluated AT THIS
					 * SAME WORLD POINT -- its own layer set, its own opacities
					 * read past its edge and therefore clamped to its edge row,
					 * which is what "the painting reaches the border" means --
					 * and the two are cross-faded with a quintic ease that is
					 * exactly 0.5 AT the line. Both sides of a line land on the
					 * same 50/50 mix there, so the composite is continuous
					 * across it and its derivative is zero at the margin.
					 *
					 * The weights are separable, so a corner mixes all four
					 * quadrants bilinearly. A neighbour outside the chunk has
					 * no paint loaded and falls back to this quadrant's own
					 * colour; the adjacent chunk's bake falls back the same way
					 * from its side, so no new seam appears at a chunk border.
					 */
					const float margin = lodgenBlendMargin();
					const float lxq = lx - ( q & 1 ? 2048.0f : 0.0f );
					const float lyq = ly - ( q & 2 ? 2048.0f : 0.0f );
					int sx = 0, sy = 0;
					float wxN = 0.0f, wyN = 0.0f;
					if ( lxq < margin ) {
						sx = -1;
						wxN = 1.0f - lxq / margin;
					} else if ( lxq > 2048.0f - margin ) {
						sx = 1;
						wxN = 1.0f - ( 2048.0f - lxq ) / margin;
					}
					if ( lyq < margin ) {
						sy = -1;
						wyN = 1.0f - lyq / margin;
					} else if ( lyq > 2048.0f - margin ) {
						sy = 1;
						wyN = 1.0f - ( 2048.0f - lyq ) / margin;
					}
					auto ease = []( float t ) -> float {
						const float u2 = qBound( 0.0f, t, 1.0f );
						// Perlin's quintic, halved: 1 at the line -> 0.5
						return 0.5f * u2 * u2 * u2 * ( u2 * ( u2 * 6.0f - 15.0f ) + 10.0f );
					};
					wxN = sx ? ease( wxN ) : 0.0f;
					wyN = sy ? ease( wyN ) : 0.0f;
					if ( wxN > 0.0f || wyN > 0.0f ) {
						auto nbr = [&]( int sxx, int syy ) -> FloatVector4 {
							int bx = ( q & 1 ) + sxx;
							int by = ( ( q >> 1 ) & 1 ) + syy;
							int ncx = cx, ncy = cy;
							if ( bx < 0 ) { bx = 1; ncx--; }
							else if ( bx > 1 ) { bx = 0; ncx++; }
							if ( by < 0 ) { by = 1; ncy--; }
							else if ( by > 1 ) { by = 0; ncy++; }
							if ( ncx < 0 || ncx >= dim || ncy < 0 || ncy >= dim )
								return color;
							const size_t nci = size_t( ncy ) * dim + ncx;
							if ( !haveLand[nci] )
								return color;
							/* the SAME world point in the neighbour quadrant's
							 * own coordinates: outside 0..1, which the opacity
							 * bilinear clamps to that quadrant's edge row */
							const float nlx = ( wx - cwX ) - float( ncx ) * 4096.0f
								- ( bx ? 2048.0f : 0.0f );
							const float nly = ( wy - cwY ) - float( ncy ) * 4096.0f
								- ( by ? 2048.0f : 0.0f );
							return quadComposite( cells[nci], ( by << 1 ) | bx,
								nlx / 2048.0f, nly / 2048.0f, false );
						};
						const FloatVector4 cX = wxN > 0.0f ? nbr( sx, 0 ) : color;
						const FloatVector4 cY = wyN > 0.0f ? nbr( 0, sy ) : color;
						const FloatVector4 cD = ( wxN > 0.0f && wyN > 0.0f )
							? nbr( sx, sy ) : color;
						color = color * ( ( 1.0f - wxN ) * ( 1.0f - wyN ) )
							+ cX * ( wxN * ( 1.0f - wyN ) )
							+ cY * ( ( 1.0f - wxN ) * wyN )
							+ cD * ( wxN * wyN );
					}
				}
				if ( doCover ) {
					/* The per-texel cover law. Every operand is already in hand:
					 * the per-quadrant D/S/T array, the same bilinear opacities
					 * the diffuse composited, and the normal computed above. */
					const QuadCover & qc = quadCover[ci * 4 + q];
					const int nL = qMin( nLayers, qc.layers.size() );
					float A = 0.0f;
					for ( int i = 0; i < nL; i++ )
						A += aCover[i];
					if ( A > 1.0f ) {
						/* Renormalise -- and ONLY on the cover side. The colour
						 * composite keeps the opacities it computes today, or
						 * --cover and --no-cover would paint the handful of
						 * texels whose layers sum past 1 differently and the
						 * byte-identity gate would fail for a reason that is not
						 * a cover bug. */
						const float s = 1.0f / A;
						for ( int i = 0; i < nL; i++ )
							aCover[i] *= s;
						A = 1.0f;
						statRenorm++;
					}
					const float wBase = 1.0f - A;
					float dTex = wBase * qc.base.d;
					float sNum = wBase * qc.base.d * qc.base.s;
					float dTint = wBase * qc.base.tintD;
					float tNum[3];
					for ( int k = 0; k < 3; k++ )
						tNum[k] = wBase * qc.base.tintD * qc.base.t[k];
					for ( int i = 0; i < nL; i++ ) {
						const LtexCoverVals & lv = qc.layers[i];
						dTex += aCover[i] * lv.d;
						sNum += aCover[i] * lv.d * lv.s;
						dTint += aCover[i] * lv.tintD;
						for ( int k = 0; k < 3; k++ )
							tNum[k] += aCover[i] * lv.tintD * lv.t[k];
					}
					const float sTex = dTex > 0.0f ? sNum / dTex : 0.0f;
					// 57.29577951 = 180/pi; theta is the slope of the SAME unit
					// normal the msn write above encodes
					const float theta = std::acos( qBound( 0.0f, nrm[2], 1.0f ) ) * 57.29577951f;
					/* A hard step at Max Slope would alias against a heightfield
					 * sampled every 128 units, and the gate's operand is that
					 * heightfield -- four times coarser than the texel it gates.
					 * The +-5 degrees is a smoothing constant against a 128-unit
					 * operand, not a 32-unit precision claim. */
					const float gate = qBound( 0.0f, ( sTex + 5.0f - theta ) / 10.0f, 1.0f );
					const float raw = 255.0f * gate * dTex / coverFull;
					coverByte = int( qBound( 0.0f, raw + 0.5f, 255.0f ) );
					if ( qc.painted ) {
						statMaxDtexPainted = qMax( statMaxDtexPainted, dTex );
						if ( gate * dTex >= coverFull )
							statClipPainted++;
					} else {
						statMaxDtexBase = qMax( statMaxDtexBase, dTex );
						if ( gate * dTex >= coverFull )
							statClipBase++;
					}
					coverPlane[size_t( py ) * RES + px] = quint8( coverByte );
					if ( coverByte > coverMax )
						coverMax = coverByte;
					coverTintD = dTint;
					if ( dTint > 0.0f )
						for ( int k = 0; k < 3; k++ )
							coverTint[k] = tNum[k] / dTint;
				}
				if ( land.hasColors ) {
					/* VCLR: the landscape shader multiplies the hand-painted
					 * vertex colour into the ground, and vanilla's LOD bakes
					 * inherit it — bilinear over the 33x33 grid. */
					const float gx = qBound( 0.0f, lx / 4096.0f * 32.0f, 31.999f );
					const float gy = qBound( 0.0f, ly / 4096.0f * 32.0f, 31.999f );
					const int ix = int( gx ), iy = int( gy );
					const float tx = gx - ix, ty = gy - iy;
					for ( int k = 0; k < 3; k++ ) {
						const float c =
							( land.colors[iy][ix][k] * ( 1 - tx )
								+ land.colors[iy][ix + 1][k] * tx ) * ( 1 - ty )
							+ ( land.colors[iy + 1][ix][k] * ( 1 - tx )
								+ land.colors[iy + 1][ix + 1][k] * tx ) * ty;
						color[k] *= c / 255.0f;
					}
				}
				/* THE ROAD, between the VCLR multiply and the grass tint.
				 *
				 * AFTER VCLR because the road lies ON the ground the artist
				 * shaded and must not be shaded a second time; BEFORE the tint
				 * because the tint's weight is the cover byte, and a road
				 * suppresses the cover under it -- grass grows beside a road,
				 * not through it. The cover PLANE is rewritten with the same
				 * byte, so a consumer reading the sheet's alpha sees what the
				 * tint used. */
				if ( !roadPlane.empty() ) {
					const quint32 rp = roadPlane[size_t( py ) * RES + px];
					/* THE ROAD OPACITY (lane ROADS3). Two different things
					 * come out of the road plane's alpha and they must not be
					 * confused:
					 *
					 *   `raGeom` is COVERAGE -- how much of this texel the road
					 *   mesh actually covers. The ground-cover suppression below
					 *   keeps using it unscaled, because a faintly painted road
					 *   is still a road and grass still does not grow through it.
					 *
					 *   `ra` is how strongly the paint is mixed in. At the
					 *   shipped default of 1.0 the multiply is not done at all
					 *   and the colour branch is entered on exactly the same
					 *   condition as before, so the off value is the previous
					 *   bake's BYTES by construction rather than by a float
					 *   argument about 1.0f.
					 *
					 * Why the knob exists, and why it is not set away from 1.0
					 * by default, is in LodgenCoverOptions::roadOpacity with the
					 * numbers and the floors. */
					const float raGeom = float( rp >> 24 ) / 255.0f;
					const float ra = ( coverOpts.roadOpacity == 1.0f )
						? raGeom : raGeom * coverOpts.roadOpacity;
					if ( raGeom > 0.0f ) {
						const float rc[3] = {
							float( ( rp >> 16 ) & 0xFF ) / 255.0f,
							float( ( rp >> 8 ) & 0xFF ) / 255.0f,
							float( rp & 0xFF ) / 255.0f };
						if ( ra > 0.0f )
							for ( int k = 0; k < 3; k++ )
								color[k] = color[k] + ( rc[k] - color[k] ) * ra;
						if ( doCover ) {
							const float keep = qBound( 0.0f,
								1.0f - raGeom * coverOpts.roadCoverSuppress, 1.0f );
							coverByte = int( float( coverByte ) * keep + 0.5f );
							coverPlane[size_t( py ) * RES + px] = quint8( coverByte );
						}
					}
				}
				/* The grass tint, AFTER the VCLR multiply. VCLR is the artist's
				 * dirt shading on the GROUND and the grass sits on top of it; a
				 * tint mixed in before the multiply would be darkened by it.
				 * The weight uses the QUANTISED cover byte, not the float, so a
				 * consumer holding the data sheet reproduces this mix exactly
				 * from the alpha it reads. The branch is not taken at cover 0,
				 * which is what makes a no-cover texel structurally identical
				 * rather than identical by IEEE argument. */
				const float tintW = ( float( coverByte ) / 255.0f ) * coverOpts.tintStrength;
				if ( tintW > 0.0f && coverTintD > 0.0f ) {
					for ( int k = 0; k < 3; k++ )
						color[k] = color[k] + ( coverTint[k] - color[k] ) * tintW;
				}
			}
			/* THE EROSION SHADING (lane GROUND1 Part B). The pass wrote relief
			 * into the `_msn` sheet above; this is the same relief reaching the
			 * COLOUR, in the crevice term's own form and with the crevice term's
			 * own fitted coefficient, so a channel floor darkens and a levee
			 * lightens by the amount vanilla's residual asked for. It sits
			 * before the grade for the reason the road does -- the grade is the
			 * last thing before quantisation -- and it is scaled by the strength
			 * because the relief it reads is scaled by the strength in the same
			 * place. No lattice, no branch: --erosion 0 writes the rung's byte.
			 * WHAT THIS IS NOT: a material tint. TILING3's hypothesis D put an
			 * R-squared ceiling of 0.018-0.023 on any per-texel law from the
			 * fine normal to vanilla's fine colour, so a rock/sediment palette
			 * would be a taste rather than a measurement, and is refused. */
			if ( eroField && g_landShade != 0.0f ) {
				const float dL = g_landShade
					* eroField->creviceAt( wx, wy, span / float( RES ) )
					* lodgenErosion() / 255.0f;
				for ( int k = 0; k < 3; k++ )
					color[k] = qBound( 0.0f, color[k] + dL, 1.0f );
			}
			/* THE GRADE, last before quantisation and after the road and the
			 * tint, so the road is graded with the ground it sits in (lane
			 * GRADE1). The crevice term runs LATER still, on the finished
			 * 8-bit sheet, and is deliberately not scaled: -3.242 was fitted
			 * in levels against vanilla's own residual. At 1.0 this branch is
			 * not taken at all, which is what makes the off value the previous
			 * bake's bytes rather than a float argument about 1.0f. */
			if ( g_landGrade != 1.0f )
				for ( int k = 0; k < 3; k++ )
					color[k] *= g_landGrade;
			const int r = qBound( 0, int( color[0] * 255.0f + 0.5f ), 255 );
			const int g = qBound( 0, int( color[1] * 255.0f + 0.5f ), 255 );
			const int b = qBound( 0, int( color[2] * 255.0f + 0.5f ), 255 );
			diffuse[size_t( py ) * RES + px] =
				0xFF000000U | quint32( r << 16 ) | quint32( g << 8 ) | quint32( b );
		}
	}
	if ( doCover ) {
		/* The self-accusing line, printed UNCONDITIONALLY under --cover so a
		 * bake with nine thousand dangling GNAMs cannot go silent. ONE physical
		 * line, every counter a key=value token with no comma inside any value,
		 * because report lines in this tree are parsed by KEYWORD and never by
		 * field position (docs/MISTAKES.md). Error counters and informational
		 * ones are kept apart: 65 of the 128 shipped landscape textures name no
		 * grass BY DESIGN -- nineteen of them are the artists' own "same
		 * texture, deliberately no grass" records -- so gating "LTEX with no
		 * GRAS" at zero would fail every healthy run. */
		const EsmCoverCensus & cc = world.coverCensus();
		int nifReads = 0, texLoads = 0;
		lodgenBakeCacheCounts( &bc, &nifReads, &texLoads );
		auto idList = []( const QSet<quint32> & s ) {
			QStringList l;
			for ( quint32 id : s )
				l.append( QString::number( id, 16 ) );
			l.sort();
			return l.join( QChar( ' ' ) );
		};
		QString line = QStringLiteral( "cover" );
		auto kv = [&line]( const char * k, const QString & v ) {
			line += QChar( ' ' );
			line += QLatin1String( k );
			line += QChar( '=' );
			line += v;
		};
		auto kvi = [&kv]( const char * k, qint64 v ) { kv( k, QString::number( v ) ); };
		auto kvf = [&kv]( const char * k, double v, int prec ) {
			kv( k, QString::number( v, 'f', prec ) );
		};
		kvi( "cx", chunkX );
		kvi( "cy", chunkY );
		kvi( "dim", dim );
		kvi( "texels", qint64( RES ) * RES );
		kvi( "coverMax", coverMax );
		kvi( "paintedPts", statPaintedPts );
		kvi( "alphaLayerPts", statAlphaPts );
		kvi( "renorm", statRenorm );
		kvf( "maxDtexPainted", double( statMaxDtexPainted ), 1 );
		kvf( "maxDtexBase", double( statMaxDtexBase ), 1 );
		kvi( "clipPainted", statClipPainted );
		kvi( "clipBase", statClipBase );
		kvi( "danglingLtex", danglingLtexIds.size() );
		kv( "danglingLtexIds", QChar( '[' ) + idList( danglingLtexIds ) + QChar( ']' ) );
		kvi( "danglingGnam", statDanglingGnam );
		kv( "danglingGnamIds", QChar( '[' ) + idList( danglingGnamIds ) + QChar( ']' ) );
		kv( "ltexNoGnam", QString( "%1/%2" ).arg( statLtexNoGnam ).arg( cc.ltexTotal ) );
		kv( "grasNoTint", QString( "%1/%2" ).arg( statGrasNoTint ).arg( cc.grasTotal ) );
		kvi( "ltexTotal", cc.ltexTotal );
		kvi( "grasTotal", cc.grasTotal );
		kvi( "gnamLinks", cc.gnamLinks );
		kvi( "ltexWithGnam", cc.ltexWithGnam );
		kvi( "grasDataMin", cc.grasDataMin );
		kvi( "grasDataMax", cc.grasDataMax );
		kvi( "grasWithoutData", cc.grasWithoutData );
		kvi( "grasReads", world.grasReadCount() );
		kvi( "nifReads", nifReads );
		kvi( "texLoads", texLoads );
		kvi( "ltexResolves", statLtexResolves );
		kvi( "quadrants", qint64( dim ) * dim * 4 );
		kvf( "coverFull", double( coverFull ), 1 );
		kvf( "tintStrength", double( coverOpts.tintStrength ), 3 );
		kvf( "landGrade", double( lodgenLandGrade() ), 4 );
		kvi( "pxNoLand", statNoLand );
		kvi( "pxNoBase", statNoBase );
		kvi( "pxUnresolvableLtex", statNoTex );
		kv( "unresolvableLtexIds", QChar( '[' ) + idList( failedLtex ) + QChar( ']' ) );
		fprintf( stderr, "%s\n", line.toLatin1().constData() );
		fflush( stderr );
	}
	/* The road census, printed UNCONDITIONALLY while the feature is on, so a
	 * chunk that found no roads says so with zeros instead of going silent --
	 * and so a chunk that DID find them cannot be believed without the numbers
	 * beside it (the three rules of 2026-09-04 21:33). */
	if ( coverOpts.roads ) {
		fprintf( stderr, "%s chunk=%d,%d dim=%d\n",
			roadCensus.line().toLatin1().constData(), chunkX, chunkY, dim );
		fflush( stderr );
	}
	/* The erosion census, on the same terms as the two above: printed
	 * whenever the pass is on, zeros and all. `erosionMoved 0` on a chunk
	 * that ran means the droplets found nothing steep enough to cut.
	 * meanAbs, maxCut and maxFill are WORLD UNITS of height. */
	if ( lodgenErosion() > 0.0f ) {
		fprintf( stderr, "erosion %.3f erosionIterations %d erosionSeed %u "
			"erosionStep %.1f erosionCells %lld erosionMoved %lld "
			"erosionMeanAbs %.4f erosionMaxCut %.3f erosionMaxFill %.3f "
			"chunk=%d,%d dim=%d\n",
			double( lodgenErosion() ), lodgenErosionIterations(), lodgenErosionSeed(),
			eroCensus.step, static_cast<long long>( eroCensus.cells ),
			static_cast<long long>( eroCensus.moved ), eroCensus.meanAbs,
			eroCensus.maxCut, eroCensus.maxFill, chunkX, chunkY, dim );
		fflush( stderr );
	}
	/* The object-AO census, on the same terms: printed whenever the switch is
	 * on, zeros and all, so a chunk that darkened nothing says so. */
	if ( coverOpts.terrainObjectAo ) {
		fprintf( stderr, "terrainObjectAo 1 objAoSlab %d objAoPlacements %d objAoMeshes %d "
			"objAoTriangles %d objAoSquares %d objAoSlabSquares %d objAoTexels %lld "
			"objAoMeanDark %.4f "
			"objAoRefusedNoLod %d objAoNoLodBases %d objAoRefusedNoLoad %d "
			"chunk=%d,%d dim=%d\n",
			coverOpts.terrainObjectAoSlab ? 1 : 0,
			objCensus.placements, objCensus.meshes, objCensus.triangles,
			objCensus.squares, objCensus.slabSquares,
			static_cast<long long>( objCensus.texels ),
			objCensus.meanDarkening(), objCensus.refusedNoLod,
			objCensus.noLodBases, objCensus.refusedNoLoad, chunkX, chunkY, dim );
		fflush( stderr );
	}
	if ( statNoLand || statNoBase || statNoTex ) {
		QStringList ids;
		for ( quint32 id : failedLtex )
			ids.append( QString::number( id, 16 ) );
		fprintf( stderr, "bake %d,%d: %d px no land, %d px no base tex, "
			"%d samples unresolvable LTEX [%s]\n", chunkX, chunkY,
			statNoLand, statNoBase, statNoTex,
			ids.join( QChar( ' ' ) ).toLatin1().constData() );
	}

	const QString base = QString( "%1/%2.%3.%4.%5" )
		.arg( outDir ).arg( world.worldspaceEdid() ).arg( dim ).arg( chunkX ).arg( chunkY );
	/* VANILLA REUSE (lane TILING3), the same decision function the shipping
	 * PYRAMID writer calls. The paint test reads the cells THIS bake already
	 * loaded rather than walking the group tree a second time. */
	bool hasPaint = false;
	for ( size_t ci = 0; ci < cells.size() && !hasPaint; ci++ ) {
		for ( int q = 0; q < 4; q++ ) {
			if ( cells[ci].baseTex[q] || !cells[ci].layers[q].isEmpty() ) {
				hasPaint = true;
				break;
			}
		}
	}
	QByteArray msnCopy, colCopy;
	lodgenVanillaChunkSheets( world, world.worldspaceEdid(), dim, chunkX, chunkY,
		RES, hasPaint, diffuse, msn, msnCopy, colCopy );
	if ( !lodgenWriteChunkSheets( base, RES, diffuse, msn, colCopy, msnCopy ) )
		return fail( QStringLiteral( "could not write the chunk colour/normal sheets" ) );

	/* Ambient occlusion as a TEXTURE, not only as a vertex channel.
	 *
	 * The vertex copy is computed on the 129^2 heightfield and then stored on
	 * the DECIMATED Land mesh -- about 1180 vertices for a whole dim-4 chunk,
	 * some 480 world units apart -- so everything between them is triangle
	 * interpolation and it reads as faceted, not as occlusion. The vertex
	 * channel stays (it is free and fine for coarse shading); this is the
	 * copy to actually shade from.
	 *
	 * Marched in WORLD space with bilinear heightfield samples rather than in
	 * grid steps, so 512^2 comes out smooth. That does not invent detail --
	 * the source is still 128-unit samples -- it just stops re-quantising the
	 * answer onto a coarser lattice than the texture it lands in.
	 */
	{
		const float texel = float( dim ) * 4096.0f / float( RES );
		static const float dirs[8][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
			{ 0.7071f, 0.7071f }, { 0.7071f, -0.7071f },
			{ -0.7071f, 0.7071f }, { -0.7071f, -0.7071f } };
		/* Bilinear sample of the heightfield in CHUNK-local world units, served
		 * from the ring. The march below reaches 2,048 units and until
		 * 2026-09-10 everything past the chunk edge was that edge repeated, so
		 * every chunk carried a false plateau around itself and its outermost
		 * 2,048 units of AO were shadowed by nothing. Same tap as the tile
		 * baker's, same function. */
		auto heightAt = [&]( float wx, float wy ) {
			return lodgenTerrainGridSample( hgt, hn,
				wx + LODGEN_TERRAIN_RING_UNITS, wy + LODGEN_TERRAIN_RING_UNITS );
		};
		/* Channel-packed terrain data map, RGBA.
		 *
		 * Every one of these is computed on the 129^2 heightfield and was then
		 * stored on ~1180 decimated vertices, so all of them had AO's problem,
		 * not just AO. At 512^2 they get roughly fifteen times the linear
		 * detail and stop being triangle interpolation.
		 *
		 * WHAT IS NOT HERE, and why: the material CLASS ids. R and UV2.y carry
		 * discrete class numbers, and BC block compression interpolates inside
		 * each 4x4 block -- which would synthesise class ids that do not exist,
		 * silently, at every block boundary. Smooth scalar fields tolerate that
		 * and enumerations do not, so the classes stay per-vertex where they
		 * are exact. Geomorph weight stays per-vertex too: it is a delta to a
		 * specific parent MESH, not a property of the ground. */
		std::vector<quint8> tMat, tWet, tAo2, tMat2, tShore;
		std::vector<float> tSky;
		std::vector<quint8> tBlend;   // computed, not packed: see the note below
		lodgenTerrainChannels( world, chunkX, chunkY, dim, chgt,
			tMat, tWet, tAo2, tSky, tMat2, tShore, &tBlend );
		// the channel grids are the CHUNK's, so these carry no ring offset
		auto sampleU8 = [&]( const std::vector<quint8> & f, float wx, float wy ) {
			return lodgenTerrainGridSample( f, cn, wx, wy );
		};
		std::vector<quint32> aoTex( size_t( RES ) * RES, 0xFFFFFFFFU );
		for ( int j = 0; j < RES; j++ ) {
			for ( int i = 0; i < RES; i++ ) {
				const float wx = ( float( i ) + 0.5f ) * texel;
				const float wy = ( float( j ) + 0.5f ) * texel;
				const float h0 = heightAt( wx, wy );
				float occl = 0.0f;
				for ( const auto & d : dirs ) {
					float maxSlope = 0.0f;
					for ( float dist = 128.0f; dist <= 2048.0f; dist *= 1.5f ) {
						const float dh =
							heightAt( wx + d[0] * dist, wy + d[1] * dist ) - h0;
						if ( dh > 0.0f )
							maxSlope = qMax( maxSlope, dh / dist );
					}
					occl += maxSlope / ( 1.0f + maxSlope );
				}
				const float vis = qBound( 0.0f, 1.0f - occl / 8.0f * 1.6f, 1.0f );
				quint32 ao8 = quint32( qBound( 0.0f, vis * 255.0f + 0.5f, 255.0f ) );
				/* THE OBJECT TERM (lane GROUND1): a second march of the same shape
				 * over the placed geometry's tops, multiplied in as a second
				 * visibility fraction. `wx`/`wy` here are CHUNK-local, so the field
				 * -- which is indexed in world units -- gets the chunk origin back. */
				if ( objField ) {
					const float vo = lodgenObjectSkyVis( *objField,
						cwX + wx, cwY + wy, h0, dirs,
						coverOpts.terrainObjectAoStrength,
						coverOpts.terrainObjectAoSlab );
					const quint32 a2 = quint32( qBound( 0.0f,
						vis * vo * 255.0f + 0.5f, 255.0f ) );
					if ( a2 < ao8 ) {
						objCensus.texels++;
						objCensus.darkSum += double( ao8 - a2 );
					}
					ao8 = a2;
				}
				const quint32 wet8 = quint32( qBound( 0.0f,
					sampleU8( tWet, wx, wy ) + 0.5f, 255.0f ) );
				const quint32 sho8 = quint32( qBound( 0.0f,
					sampleU8( tShore, wx, wy ) + 0.5f, 255.0f ) );
					/* R = AO, G = wetness, B = shore proximity, A = GROUND COVER
					 * where the chunk has any and nothing at all where it has not.
					 *
					 * Four candidates were tried for this alpha slot and each
					 * failed a test worth remembering:
					 *   sky visibility -- IS AO on a heightfield (r = 0.969):
					 *                     lodgenTerrainChannels writes skyVis[i] =
					 *                     vis and ao[i] = vis * 255 from ONE horizon
					 *                     measure, so storing it stores AO twice;
					 *   slope          -- recoverable as acos(n.z) from _msn;
					 *   water depth    -- the water mesh already carries it, and
					 *                     land wants shore proximity, not depth;
					 *   material blend -- the diffuse ALREADY composites the layers,
					 *                     and the class ids it would weight are
					 *                     per-VERTEX, so a per-texel weight has no
					 *                     operands at its own resolution.
					 *
					 * Ground cover is the fifth candidate and it is the first to
					 * pass both tests. It is NOT derivable from anything shipped:
					 * it needs LTEX GNAM and the GRAS DATA block, records nothing
					 * in this tree had ever read, and it is orthogonal to R by
					 * construction -- R comes from the heightfield, cover from the
					 * splat. And every operand of it exists at 512^2 inside the
					 * paint loop: D is a per-FORM scalar, the opacity it multiplies
					 * is the 17x17 grid that loop already samples bilinearly per
					 * texel, and the slope gate reads the normal the same iteration
					 * computed. Cover CONSUMES slope rather than storing it. It is
					 * a smooth scalar, so it tolerates BC interpolation inside a
					 * 4x4 block -- which is precisely why the class IDS above were
					 * kept per-vertex and a fraction need not be.
					 *
					 * The value is ORDINAL in scale and LINEAR in composition:
					 * 255 * gate * Dtex / COVER_FULL, so 128 does not mean "half
					 * the ground is grass" -- nothing states what Density counts
					 * per unit area -- but the mean of four cover bytes IS the
					 * cover of the union at half the resolution, which is what lets
					 * a coarser pyramid level box-filter it.
					 *
					 * BC1 with alpha 0xFF when coverMax == 0, byte for byte the
					 * 174,888-byte sheet this generator has always written; BC3
					 * with A = cover when it is not, 349,648. Writing cover-0 as
					 * alpha 0 into a BC1 sheet would set punch-through on every
					 * block and rewrite the whole file for nothing, so the plane is
					 * OR-ed in after the loop and only when it carries something.
					 * The fourCC is the switch AND it is qualified: a DXT5
					 * _data.DDS is a cover sheet only if the header's dwReserved1
					 * carries 'WWCV'. Without that stamp an xLODGen sheet's
					 * constant-255 alpha would decode as FULL cover on every texel
					 * -- grass on rubble and on the ocean floor, and unrepairable,
					 * because the bytes would contain nothing to repair. */
					// row 0 is the chunk's NORTH edge, matching the other two bakes
					aoTex[size_t( RES - 1 - j ) * RES + size_t( i )] =
						0xFF000000U | ( ao8 << 16 ) | ( wet8 << 8 ) | sho8;
			}
		}
		if ( coverMax > 0 ) {
			// only now, and only when the plane carries something: alpha 0 in a
			// BC1 sheet would trip punch-through on every block
			for ( size_t t = 0; t < aoTex.size(); t++ )
				aoTex[t] = ( aoTex[t] & 0x00FFFFFFU )
					| ( quint32( coverPlane[t] ) << 24 );
		}
		if ( doCover && !coverOpts.dumpCoverPath.isEmpty() ) {
			// the raw pre-compression plane, north-up, headerless: the harness's
			// independent handle on the value before BC3 touches it
			QFile cf( coverOpts.dumpCoverPath );
			if ( cf.open( QIODevice::WriteOnly ) )
				cf.write( reinterpret_cast<const char *>( coverPlane.data() ),
					qint64( coverPlane.size() ) );
		}
		const quint32 coverStamp0 = coverMax > 0 ? 0x56435757U : 0U;   // 'WWCV'
		const quint32 coverStamp1 = coverMax > 0
			? ( ( 1U << 24 ) | quint32( qBound( 0, int( coverFull + 0.5f ), 0xFFFFFF ) ) )
			: 0U;
		if ( !lodgenWriteDds( base + QStringLiteral( "_data.DDS" ), RES, RES, aoTex,
			coverMax > 0, 0, false, coverStamp0, coverStamp1 ) )
			return fail( QStringLiteral( "could not write the terrain data bake" ) );
	}

	if ( error )
		error->clear();
	return true;
}

/* ============ rung 3b: the terrain virtual texture (.lodt) ============ *
 *
 * The same bake as the per-chunk sheets above, restructured into a pyramid of
 * bordered tiles so a consumer can stream terrain at a fixed memory budget
 * instead of loading whole chunk sheets. The format contract is
 * docs/LODGEN_TERRAIN_VT.md; everything here is the bake.
 *
 * Two things are deliberately NOT shared with lodgenBakeTerrainTextures: its
 * per-texel loop and its DDS writer. Its output is gated by byte identity
 * against files measured before this lane existed, and it was not to move, so
 * the tile baker repeats the arithmetic rather than refactoring the gated path
 * underneath itself -- the same reason lodgenEncodeArrayLayer repeats the mip
 * loop. Where the two must agree, a check measures that they do (V9a), rather
 * than a shared function asserting it. What IS shared, since 2026-09-10, is
 * everything the two paths must not be allowed to drift on: the height
 * reconstruction (lodgenTerrainHeightAt), the msn encoding
 * (lodgenTerrainMsnPixel), the ring fill (lodgenTerrainFillRing) and the
 * bilinear tap (lodgenTerrainGridSample). The per-texel loops stay apart.
 *
 * Three things the tile path does that the chunk path cannot:
 *   - it carries a BORDER on every tile, so a consumer filters and samples one
 *     tile without reaching into its neighbours. The one-cell RING that feeds
 *     the msn's central differences and the 2,048-unit AO march is NO LONGER
 *     one of these: since 2026-09-10 the chunk path bakes on the same ring,
 *     through the same lodgenTerrainFillRing and the same
 *     lodgenTerrainGridSample, which is what lets V9a ask the two paths for
 *     byte identity instead of for a bounded band;
 *   - it carries a HEIGHT sheet, R16 encoded exactly as the shadow heightmap
 *     encodes it (height/8 + 32767), on the same tile grid with the same
 *     border, so a consumer that wants nested grids has the geometry side too;
 *   - it filters coarser levels from the finer level's UNCOMPRESSED staging,
 *     never from decoded BC blocks, because decode-and-re-encode accumulates
 *     error at every level and the staging costs nothing -- the encoder needs
 *     it anyway.
 */

namespace
{

//! One tile's uncompressed staging: what the encoder reads and what the box
//! filter reads. `data`'s alpha is the cover byte and is 0 -- never 0xFF --
//! where a tile has no cover, so a parent bordering one grassy child does not
//! inherit full cover across three quadrants of bare rock. The 0xFF of the
//! BC1 fallback is applied at ENCODE time and never enters the filter.
struct LodgenVtStage
{
	std::vector<quint32> colour;
	std::vector<quint32> msn;
	/*! The RETIRED v1 `data` plane -- R AO, G wetness, B shore, A cover. It is
	 *  NOT written to the container any more (2.2: role 5 `mask` replaced it),
	 *  but the `.btr` chunk sheets on the stock path are assembled from this
	 *  staging (2.4) and the stock engine's `_data.DDS` did not change, so the
	 *  plane is still computed. Dropping it from the container is what bungo
	 *  ruled; dropping it from the staging would rewrite files the stock path's
	 *  byte-identity gate pins. */
	std::vector<quint32> data;
	//! The mask sheet, RMAOS: A ground cover, R roughness, G metallic, B sky AO.
	std::vector<quint32> mask;
	//! The emissive sheet, RGB, opaque. Empty when no layer supplies an emissive.
	std::vector<quint32> emissive;
	std::vector<quint16> height;
	bool cover = false;
};

struct LodgenVtLevel
{
	int dim = 0;
	int tilesX = 0, tilesY = 0;
	int west = 0, east = 0, south = 0, north = 0;
};

//! A few cell rows of LAND, so a 9,216-tile pass does not walk the same cell's
//! group tree thirty times. The returned pointer is valid only until the next
//! call: callers read it at once or copy it out.
struct LodgenVtLandCache
{
	QHash<qint64, EsmLand> cells;
	QSet<qint64> misses;

	static qint64 key( int cx, int cy )
	{
		return ( qint64( cx ) << 32 ) ^ qint64( quint32( cy ) );
	}

	const EsmLand * get( const EsmWorld & world, int cx, int cy )
	{
		const qint64 k = key( cx, cy );
		auto it = cells.constFind( k );
		if ( it != cells.constEnd() )
			return &it.value();
		if ( misses.contains( k ) )
			return nullptr;
		EsmLand land;
		if ( !world.land( cx, cy, land ) ) {
			misses.insert( k );
			return nullptr;
		}
		return &cells.insert( k, land ).value();
	}

	void dropBelow( int minCellY )
	{
		for ( auto it = cells.begin(); it != cells.end(); ) {
			if ( int( qint32( quint32( it.key() & 0xFFFFFFFF ) ) ) < minCellY )
				it = cells.erase( it );
			else
				++it;
		}
		for ( auto it = misses.begin(); it != misses.end(); ) {
			if ( int( qint32( quint32( *it & 0xFFFFFFFF ) ) ) < minCellY )
				it = misses.erase( it );
			else
				++it;
		}
	}
};

int lodgenVtTrueMod( int v, int m )
{
	const int r = v % m;
	return r < 0 ? r + m : r;
}

int lodgenVtFloorTo( int v, int m )
{
	return v - lodgenVtTrueMod( v, m );
}

/*! The ladder and every level's rectangle.
 *
 *  The ladder stops at the coarsest dim that BOTH the worldspace's west and
 *  south divide: -96 divides by 1, 2, 4, 8, 16 and 32 and not by 64, which is
 *  why the Commonwealth has no single root tile and why the dim-32 level (36
 *  tiles) IS the root. A worldspace that is not tile-aligned SHORTENS the
 *  ladder and a note says so; it is not refused, because refusing would give
 *  the first non-Commonwealth worldspace anyone tries a coin-flip chance of
 *  failing.
 *
 *  Every level is anchored to ONE north-west origin, aligned to the coarsest
 *  dim and therefore to every finer one. That is what makes a coarse tile
 *  cover exactly four finer ones, at every level, with no resampling -- which
 *  a consumer assembling nested grids from whole tiles depends on, and which
 *  per-level flooring would silently break wherever two levels floored the
 *  same world edge differently. */
int lodgenVtLevelsFromBounds( int worldW, int worldS, int worldE, int worldN,
	const LodgenVtOptions & opts, LodgenVtLevel * levels, int maxLevels, int * coarsestDimOut )
{
	if ( worldE < worldW || worldN < worldS )
		return 0;
	const int finest = qMax( 1, opts.finestDim );
	int coarsest = finest;
	for ( int d = finest * 2; d <= 32; d *= 2 ) {
		if ( lodgenVtTrueMod( worldW, d ) == 0 && lodgenVtTrueMod( worldS, d ) == 0 )
			coarsest = d;
		else
			break;
	}
	const int originW = lodgenVtFloorTo( worldW, coarsest );
	const int originN = worldN + ( coarsest - 1 - lodgenVtTrueMod( worldN, coarsest ) );
	int n = 0;
	for ( int d = finest; d <= coarsest && n < maxLevels; d *= 2 ) {
		LodgenVtLevel & L = levels[n];
		L.dim = d;
		L.west = originW;
		L.north = originN;
		L.tilesX = ( worldE - originW + d ) / d;
		L.tilesY = ( originN - worldS + d ) / d;
		L.east = originW + L.tilesX * d - 1;
		L.south = originN - L.tilesY * d + 1;
		n++;
	}
	if ( coarsestDimOut ) *coarsestDimOut = coarsest;
	return n;
}

//! The same, resolving the rectangle from the worldspace (and from the region
//! when one is given).
int lodgenVtBuildLevels( const EsmWorld & world, const LodgenVtOptions & opts,
	LodgenVtLevel * levels, int maxLevels, int * coarsestDimOut,
	int * worldWOut, int * worldSOut, int * worldEOut, int * worldNOut )
{
	int worldW = 0, worldS = 0, worldE = 0, worldN = 0;
	world.cellBounds( worldW, worldS, worldE, worldN );
	if ( worldE < worldW || worldN < worldS )
		return 0;
	if ( opts.haveRegion ) {
		// a region bake: the rectangle IS the world as far as this set is
		// concerned, clipped to cells the worldspace actually has
		worldW = qMax( worldW, qMin( opts.region[0], opts.region[2] ) );
		worldE = qMin( worldE, qMax( opts.region[0], opts.region[2] ) );
		worldS = qMax( worldS, qMin( opts.region[1], opts.region[3] ) );
		worldN = qMin( worldN, qMax( opts.region[1], opts.region[3] ) );
		if ( worldE < worldW || worldN < worldS )
			return 0;
	}
	const int n = lodgenVtLevelsFromBounds( worldW, worldS, worldE, worldN, opts,
		levels, maxLevels, coarsestDimOut );
	if ( worldWOut ) *worldWOut = worldW;
	if ( worldSOut ) *worldSOut = worldS;
	if ( worldEOut ) *worldEOut = worldE;
	if ( worldNOut ) *worldNOut = worldN;
	return n;
}

//! BC3's alpha block: two 8-bit endpoints and 3-bit indices over an 8-step
//! palette. The same code lodgenWriteDds runs inline, repeated here for the
//! same reason the mip loop is: that writer's bytes are gated and were not to
//! move.
void lodgenVtEncodeBC3Alpha( const quint32 * img, int w, int h, int bx, int by, quint8 * out )
{
	quint8 a[16];
	quint8 aMin = 255, aMax = 0;
	for ( int i = 0; i < 16; i++ ) {
		const int sx = qMin( bx * 4 + ( i & 3 ), w - 1 );
		const int sy = qMin( by * 4 + ( i >> 2 ), h - 1 );
		a[i] = quint8( img[size_t( sy ) * w + sx] >> 24 );
		aMin = qMin( aMin, a[i] );
		aMax = qMax( aMax, a[i] );
	}
	out[0] = aMax;
	out[1] = aMin;
	quint64 bits = 0;
	quint8 pal[8];
	pal[0] = aMax;
	pal[1] = aMin;
	for ( int k = 1; k < 7; k++ )
		pal[k + 1] = quint8( ( ( 7 - k ) * aMax + k * aMin ) / 7 );
	for ( int i = 15; i >= 0; i-- ) {
		int best = 0, bd = 256;
		for ( int k = 0; k < 8; k++ ) {
			const int d = qAbs( int( a[i] ) - int( pal[k] ) );
			if ( d < bd ) { bd = d; best = k; }
		}
		bits = ( bits << 3 ) | quint64( best );
	}
	for ( int k = 0; k < 6; k++ )
		out[2 + k] = quint8( bits >> ( k * 8 ) );
}

//! Halve a BGRA image by the one filter law: (a+b+c+d+2) >> 2, per channel,
//! independently, round-half-up. The same rule the DDS mip chain uses; two
//! rounding rules for one filter cannot both hold.
std::vector<quint32> lodgenVtHalve( const std::vector<quint32> & src, int w, int h, bool keepAlpha )
{
	const int nw = w / 2, nh = h / 2;
	std::vector<quint32> out( size_t( nw ) * nh );
	for ( int y = 0; y < nh; y++ ) {
		for ( int x = 0; x < nw; x++ ) {
			quint32 acc[4] = { 0, 0, 0, 0 };
			for ( int sy = 0; sy < 2; sy++ )
				for ( int sx = 0; sx < 2; sx++ ) {
					const quint32 p = src[size_t( y * 2 + sy ) * w + ( x * 2 + sx )];
					acc[0] += ( p >> 16 ) & 0xFF;
					acc[1] += ( p >> 8 ) & 0xFF;
					acc[2] += p & 0xFF;
					acc[3] += ( p >> 24 ) & 0xFF;
				}
			out[size_t( y ) * nw + x] =
				( ( keepAlpha ? ( ( acc[3] + 2 ) >> 2 ) : 0xFFU ) << 24 )
				| ( ( ( acc[0] + 2 ) >> 2 ) << 16 ) | ( ( ( acc[1] + 2 ) >> 2 ) << 8 )
				| ( ( acc[2] + 2 ) >> 2 );
		}
	}
	return out;
}

std::vector<quint16> lodgenVtHalve16( const std::vector<quint16> & src, int w, int h )
{
	const int nw = w / 2, nh = h / 2;
	std::vector<quint16> out( size_t( nw ) * nh );
	for ( int y = 0; y < nh; y++ )
		for ( int x = 0; x < nw; x++ ) {
			const quint32 acc = quint32( src[size_t( y * 2 ) * w + x * 2] )
				+ src[size_t( y * 2 ) * w + x * 2 + 1]
				+ src[size_t( y * 2 + 1 ) * w + x * 2]
				+ src[size_t( y * 2 + 1 ) * w + x * 2 + 1];
			out[size_t( y ) * nw + x] = quint16( ( acc + 2 ) >> 2 );
		}
	return out;
}

//! One BGRA image as BC1 or BC3 blocks, tightly packed, no header.
void lodgenVtEncodeBlocks( const std::vector<quint32> & img, int w, int h, bool bc3,
	QByteArray & out )
{
	const int bw = ( w + 3 ) / 4, bh = ( h + 3 ) / 4;
	const int blockBytes = bc3 ? 16 : 8;
	const qsizetype base = out.size();
	out.resize( base + qsizetype( bw ) * bh * blockBytes );
	quint8 * p = reinterpret_cast<quint8 *>( out.data() ) + base;
	// BLOCK ROWS IN PARALLEL: disjoint writes into `out`, `img` read-only.
	lodgenParallelFor( bh, [&]( int by ) {
		for ( int bx = 0; bx < bw; bx++ ) {
			quint8 * o = p + ( size_t( by ) * bw + bx ) * blockBytes;
			if ( bc3 ) {
				lodgenVtEncodeBC3Alpha( img.data(), w, h, bx, by, o );
				o += 8;
			}
			// allowPunch is false under BC3 (its alpha lives in its own block)
			// and harmless under BC1 here, whose alpha is forced opaque
			lodgenEncodeBC1Block( img.data(), w, h, bx, by, o, !bc3 );
		}
	} );
}

void lodgenVtEncodeR16( const std::vector<quint16> & img, QByteArray & out )
{
	const qsizetype base = out.size();
	out.resize( base + qsizetype( img.size() ) * 2 );
	quint8 * p = reinterpret_cast<quint8 *>( out.data() ) + base;
	for ( size_t i = 0; i < img.size(); i++ ) {
		p[i * 2] = quint8( img[i] );
		p[i * 2 + 1] = quint8( img[i] >> 8 );
	}
}

/*! One tile's payload: sheet 0 mip 0, sheet 0 mip 1, sheet 1 mip 0, ... in
 *  that exact order, rows tightly packed at blocksX * blockBytes.
 *
 *  The no-cover data sheet's alpha is forced to 0xFF HERE and only here.
 *  Writing the staging's cover-0 alpha into a BC1 sheet would set
 *  punch-through on every block and turn the sheet -- and its index-3 texels
 *  -- into something new for no reason. */
/*! One tile's payload, sheet-major and mip-minor, in the SAME order the header
 *  lists the sheets: colour, msn, mask, [height], [emissive].
 *
 *  `coverInColor` moves the ground-cover byte from the mask sheet's alpha to
 *  the colour sheet's -- the object family's `coverage` slot -- which is the
 *  way back to the other half of bungo's open question (the director rules; see
 *  the report's section 2). Exactly one of the two sheets carries it and
 *  exactly that one is the BC3/BC1 switch, which is what the container's
 *  "one cover carrier" rule pins. */
QByteArray lodgenVtEncodeTile( const LodgenVtStage & st, int stored, int mips, bool withHeight,
	bool withEmissive, bool coverInColor )
{
	QByteArray out;
	struct SheetSrc { const std::vector<quint32> * px; bool bc3; bool alpha; };
	// the carrier keeps its alpha; the other sheet ships opaque, exactly as the
	// v1 data sheet did on a cover-free tile, so no BC1 block gains punch-through
	std::vector<quint32> maskOpaque, colourOpaque;
	const std::vector<quint32> * maskPx = &st.mask;
	const std::vector<quint32> * colourPx = &st.colour;
	const bool maskCarries = st.cover && !coverInColor;
	const bool colourCarries = st.cover && coverInColor;
	if ( !maskCarries ) {
		maskOpaque = st.mask;
		for ( quint32 & v : maskOpaque )
			v |= 0xFF000000U;
		maskPx = &maskOpaque;
	}
	if ( !colourCarries ) {
		colourOpaque = st.colour;
		for ( quint32 & v : colourOpaque )
			v |= 0xFF000000U;
		colourPx = &colourOpaque;
	}
	const SheetSrc sheets[3] = {
		{ colourPx, colourCarries, colourCarries },
		{ &st.msn, false, false },
		{ maskPx, maskCarries, maskCarries }
	};
	for ( int s = 0; s < 3; s++ ) {
		std::vector<quint32> img = *sheets[s].px;
		int w = stored, h = stored;
		for ( int m = 0; m < mips; m++ ) {
			if ( m ) {
				img = lodgenVtHalve( img, w, h, sheets[s].alpha );
				w /= 2;
				h /= 2;
			}
			lodgenVtEncodeBlocks( img, w, h, sheets[s].bc3, out );
		}
	}
	if ( withHeight ) {
		std::vector<quint16> img = st.height;
		int w = stored, h = stored;
		for ( int m = 0; m < mips; m++ ) {
			if ( m ) {
				img = lodgenVtHalve16( img, w, h );
				w /= 2;
				h /= 2;
			}
			lodgenVtEncodeR16( img, out );
		}
	}
	if ( withEmissive ) {
		// BC1 and opaque: coverage lives on the colour sheet under the object
		// family, which is exactly why the emissive can be BC1 (.lodm 2.1)
		std::vector<quint32> img = st.emissive;
		if ( img.empty() )
			img.assign( size_t( stored ) * stored, 0xFF000000U );
		int w = stored, h = stored;
		for ( int m = 0; m < mips; m++ ) {
			if ( m ) {
				img = lodgenVtHalve( img, w, h, false );
				w /= 2;
				h /= 2;
			}
			lodgenVtEncodeBlocks( img, w, h, false, out );
		}
	}
	return out;
}

} // namespace

namespace
{

//! Per-LTEX cover constants for one quadrant, as the chunk baker resolves them.
struct LodgenVtLtexVals
{
	float d = 0.0f, s = 0.0f, tintD = 0.0f;
	float t[3] = { 0.0f, 0.0f, 0.0f };
};

struct LodgenVtQuadCover
{
	LodgenVtLtexVals base;
	QVector<LodgenVtLtexVals> layers;
};

/*! What one LTEX layer contributes to the MASK sheet, resolved ONCE per form.
 *
 *  The maps themselves are sampled per texel through lodgenCachedTexture, the
 *  same way the diffuse is, because that cache is an LRU and a pointer held
 *  across tiles would dangle. What is cached here is the LAW -- which rule
 *  served, which map and which channel, and the constants that stand in when a
 *  map will not load. */
struct LodgenVtLayerMask
{
	LodgenMaterialMask mat;
	bool resolved = false;
};

//! Per-LTEX mask answers for one pass, plus the census of which rule served.
struct LodgenVtMaskCache
{
	QHash<quint32, LodgenVtLayerMask> byForm;
	int ruleCounts[3] = { 0, 0, 0 };      //!< none-default, legacy-inverted, pbrm
	int withEmissive = 0;
	int withMetallicMap = 0;
	int withRoughnessMap = 0;

	const LodgenVtLayerMask & resolve( const EsmWorld & world, const QString & dataRoot,
		quint32 form )
	{
		auto it = byForm.find( form );
		if ( it != byForm.end() )
			return *it;
		LodgenVtLayerMask m;
		if ( form ) {
			const EsmLtexTextureSet & ts = world.ltexTextureSet( form );
			/* The layer's material is the TXST's MNAM; its `_s` map is TX07.
			 * A material-backed TXST names no TX00 either, which is why the
			 * diffuse loop already hands the material path to the texture
			 * loader -- the same material is what the mask law reads. */
			lodgenResolveMaterialMask( dataRoot, ts.material, ts.specular, 1.0f, &m.mat,
				ts.diffuse );
			m.resolved = true;
			ruleCounts[int( m.mat.rule )]++;
			if ( m.mat.haveEmissive )
				withEmissive++;
			if ( m.mat.haveMetallicMap )
				withMetallicMap++;
			if ( m.mat.haveRoughnessMap )
				withRoughnessMap++;
		}
		return *byForm.insert( form, m );
	}
};

/*! Bake ONE tile of the finest level, from the paint.
 *
 *  (cellX0, cellY0) is the tile's south-west cell; the tile spans dim x dim
 *  cells and stores content + 2*border texels a side. Everything is baked over
 *  a ONE-CELL RING, which is what gives the border, the msn's central
 *  differences and the 2,048-unit AO march real data instead of the per-chunk
 *  path's edge clamp.
 *
 *  `dominantBase` is computed over the ENCLOSING dim-4 chunk's cells, not over
 *  the tile's own: it is what NULL-LTEX layers and baseTex == 0 texels paint,
 *  so a tile scoped to its own two cells would paint them a different colour
 *  and the assembled chunk sheet would stop matching a direct bake. */
/* `static` since 2026-09-11: it now takes a `LodgenRoadSet`, which lives in this
 * translation unit's anonymous namespace, and nothing outside this file has ever
 * called it. */
static bool lodgenBakeVtTile( const EsmWorld & world, const QString & dataRoot,
	LodgenBakeCaches & bc, const LodgenCoverOptions & coverOpts,
	LodgenVtLandCache & landCache, LodgenVtMaskCache & maskCache, bool wantEmissive,
	int cellX0, int cellY0, int dim,
	int content, int border, LodgenVtStage & out,
	const LodgenRoadSet * roads = nullptr, LodgenRoadCensus * roadCensus = nullptr,
	const LodgenObjectHeightField * objField = nullptr,
	LodgenObjectAoCensus * objCensus = nullptr )
{
	const int S = content + 2 * border;
	const float upt = float( dim ) * 4096.0f / float( content );

	/* THE ROAD PLANE for this tile, scan-converted once. The tile's world
	 * rectangle includes its BORDER, so a road crossing a tile edge is painted
	 * identically in both tiles' content and in both tiles' borders. Empty when
	 * the feature is off, and then the per-texel branch below is never taken. */
	std::vector<quint32> roadPlane;
	if ( roads && coverOpts.roads ) {
		const float wx0 = float( cellX0 ) * 4096.0f - float( border ) * upt;
		const float wyTop = float( cellY0 + dim ) * 4096.0f + float( border ) * upt;
		LodgenRoadCensus local;
		roads->rasterise( wx0, wyTop, upt, S, roadPlane, bc, dataRoot, local,
			coverOpts.roadComposite, coverOpts.roadDetail,
			coverOpts.roadGroundPaint );
		if ( roadCensus )
			roadCensus->add( local );
	}
	const int rdim = dim + 2 * LODGEN_TERRAIN_RING_CELLS;
	const int rx0 = cellX0 - LODGEN_TERRAIN_RING_CELLS;
	const int ry0 = cellY0 - LODGEN_TERRAIN_RING_CELLS;
	const int hn = rdim * 32 + 1;
	/* World-space units per repeat of a landscape diffuse. 341.3333 = 128/0.375
	 * is the engine's own tiling, read out of Fallout4.exe 1.10.155 at
	 * 0x1403A74C6 / 0x1403A7620 (lane SPLAT1); `--land-tiling 2048` restores the
	 * pre-2026-09-11 bake byte for byte. Every TILE site below reads this. */
	const float TILE = lodgenLandTiling();

	std::vector<EsmLand> cells( size_t( rdim ) * rdim );
	std::vector<bool> haveLand( size_t( rdim ) * rdim, false );
	std::vector<float> hgt;
	/* THE INNER UNIT IS THE CHUNK, NOT THE TILE (lane VT1, 2026-09-16).
	 *
	 * A chunk at dim D is the 2x2 content blocks of the level at dim D/2
	 * (`assembleChunkRow` below), and every level is anchored to one origin
	 * aligned to the coarsest dim, so the chunk a tile belongs to is
	 * `lodgenVtFloorTo( cellX0, 2 * dim )` on both axes -- read off the tile's
	 * own world coordinates, with no option and no level index in it, so a
	 * tile's bytes do not depend on whether .btr sheets were asked for.
	 *
	 * Handing the chunk's box to the shared filler is what makes this grid
	 * agree with the chunk baker's over the whole overlap, and it is the whole
	 * fix: modelled offline first over the four dim-4 chunks of the probe
	 * region -- 108 disagreeing samples before, 0 after. The box edges fall
	 * outside this smaller grid on two sides of every tile and are clipped
	 * there; a clipped edge cannot bite, because no cell of this grid reaches
	 * past it. */
	LodgenRingInner inner;
	{
		const int pd = dim * 2;
		const int px0 = lodgenVtFloorTo( cellX0, pd );
		const int py0 = lodgenVtFloorTo( cellY0, pd );
		inner.given = true;
		inner.loX = ( px0 - rx0 ) * 32;
		inner.hiX = ( px0 + pd - rx0 ) * 32;
		inner.loY = ( py0 - ry0 ) * 32;
		inner.hiY = ( py0 + pd - ry0 ) * 32;
	}
	lodgenTerrainFillRing( hgt, hn, rdim, world.defaultLandHeight(),
		[&]( int cx, int cy ) -> const EsmLand * {
			const EsmLand * l = landCache.get( world, rx0 + cx, ry0 + cy );
			if ( !l )
				return nullptr;
			// copied out at once: the cache's pointer dies on the next get()
			const size_t ci = size_t( cy ) * rdim + cx;
			cells[ci] = *l;
			haveLand[ci] = true;
			return &cells[ci];
		}, inner );

	quint32 dominantBase = 0;
	{
		const int bx = lodgenVtFloorTo( cellX0, 4 ), by = lodgenVtFloorTo( cellY0, 4 );
		QMap<quint32, int> counts;
		for ( int y = 0; y < 4; y++ ) {
			for ( int x = 0; x < 4; x++ ) {
				const EsmLand * l = landCache.get( world, bx + x, by + y );
				if ( !l )
					continue;
				for ( int q = 0; q < 4; q++ )
					if ( l->baseTex[q] )
						counts[l->baseTex[q]]++;
			}
		}
		int best = 0;
		for ( auto it = counts.constBegin(); it != counts.constEnd(); ++it )
			if ( it.value() > best ) { best = it.value(); dominantBase = it.key(); }
	}

	const bool doCover = coverOpts.cover;
	const float coverFull = qMax( 1.0f, coverOpts.coverFull );
	std::vector<LodgenVtQuadCover> quadCover;
	std::vector<float> aCover;
	if ( doCover ) {
		// the resolver is installed once by the driver: installing it here
		// would drop the LTEX cover cache on every tile
		quadCover.resize( size_t( rdim ) * rdim * 4 );
		aCover.assign( 64, 0.0f );
		auto resolve = [&]( quint32 form, LodgenVtLtexVals & v ) {
			if ( !form )
				return;
			const EsmLtexCover c = world.ltexCover( form, dataRoot );
			if ( !c.exists )
				return;
			v.d = float( c.density );
			v.s = c.maxSlope;
			v.tintD = float( c.tintDensity );
			for ( int k = 0; k < 3; k++ )
				v.t[k] = c.tint[k];
		};
		for ( size_t ci = 0; ci < cells.size(); ci++ ) {
			if ( !haveLand[ci] )
				continue;
			const EsmLand & land = cells[ci];
			for ( int q = 0; q < 4; q++ ) {
				LodgenVtQuadCover & qc = quadCover[ci * 4 + q];
				resolve( land.baseTex[q] ? land.baseTex[q] : dominantBase, qc.base );
				for ( const EsmLandLayer & layer : land.layers[q] ) {
					LodgenVtLtexVals v;
					if ( layer.ltex )
						resolve( layer.ltex, v );
					else
						resolve( dominantBase, v );
					qc.layers.append( v );
				}
			}
		}
	}

	std::vector<quint8> tMat, tWet, tAo2, tMat2, tShore;
	std::vector<float> tSky;
	lodgenTerrainChannels( world, rx0, ry0, rdim, hgt,
		tMat, tWet, tAo2, tSky, tMat2, tShore, nullptr );

	// both taps are the shared one; the tile's coordinates are already
	// RING-local, so they carry no offset of their own
	auto heightAt = [&]( float lx, float ly ) {
		return lodgenTerrainGridSample( hgt, hn, lx, ly );
	};
	auto sampleU8 = [&]( const std::vector<quint8> & f, float lx, float ly ) {
		return lodgenTerrainGridSample( f, hn, lx, ly );
	};
	static const float dirs[8][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
		{ 0.7071f, 0.7071f }, { 0.7071f, -0.7071f },
		{ -0.7071f, 0.7071f }, { -0.7071f, -0.7071f } };

	out.colour.assign( size_t( S ) * S, 0xFF808080U );
	out.msn.assign( size_t( S ) * S, LODGEN_MSN_FLAT );
	out.data.assign( size_t( S ) * S, 0x00FFFFFFU );
	/* The mask's default is the honest unknown: FULLY ROUGH (R 255), metallic 0,
	 * AO open (B 255), cover 0. A texel with no land is not a mirror. */
	out.mask.assign( size_t( S ) * S, 0x00FF00FFU );
	if ( wantEmissive )
		out.emissive.assign( size_t( S ) * S, 0xFF000000U );
	out.height.assign( size_t( S ) * S, 32767 );
	out.cover = false;

	const float tileW = float( cellX0 ) * 4096.0f;
	const float tileN = float( cellY0 + dim ) * 4096.0f;
	const float ringW = float( rx0 ) * 4096.0f;
	const float ringS = float( ry0 ) * 4096.0f;

	/* THE EROSION LATTICE (lane GROUND1 Part B), the pyramid's own copy of the
	 * SAME class the chunk baker builds -- the lattice is world-aligned and
	 * its step is this tile's own texel, so two tiles agree cell for cell
	 * where they meet and a tile agrees with the chunk sheet at the same dim.
	 * The rect is the tile's CONTENT; the class grows its own border. */
	std::unique_ptr<LodgenErosionField> eroField;
	if ( lodgenErosion() > 0.0f ) {
		eroField.reset( new LodgenErosionField );
		eroField->build( hgt, hn, ringW, ringS, upt,
			tileW, tileN - float( content ) * upt, content, content,
			lodgenErosionIterations(), lodgenErosionSeed() );
		lodgenErosionCensusAdd( eroField->census() );
	}

	for ( int j = 0; j < S; j++ ) {
		const float wy = tileN - ( float( j ) - float( border ) + 0.5f ) * upt;
		const float ly = wy - ringS;
		for ( int i = 0; i < S; i++ ) {
			const float wx = tileW + ( float( i ) - float( border ) + 0.5f ) * upt;
			const float lx = wx - ringW;
			const int cx = qBound( 0, int( std::floor( lx / 4096.0f ) ), rdim - 1 );
			const int cy = qBound( 0, int( std::floor( ly / 4096.0f ) ), rdim - 1 );
			const size_t ci = size_t( cy ) * rdim + cx;

			/* The normal first: the cover gate reads its Z.
			 *
			 * THROUGH THE SHARED RECONSTRUCTION AND THE SHARED ENCODER, both at
			 * the top of this file. Until 2026-09-09 these lines were their own
			 * copy and carried both of the defects the chunk path lost on
			 * 2026-09-07: `int( ngx )` -- NEAREST, so all sixteen texels of a
			 * 4x4 block read one grid point and got one normal -- and north in
			 * green with up in blue, the conventional order, which on the chunk
			 * path cost 67.7% of the light and 92.0% of the shading variation
			 * when it was measured. That matters here and not only in a future
			 * consumer: with --vt on, the .btr chunk sheets are ASSEMBLED from
			 * these tiles (docs/LODGEN_TERRAIN_VT.md 2.4), so a --vt bake was
			 * shipping the pre-2026-09-07 _msn into the stock engine.
			 *
			 * The grid step is 128 world units on both paths, so the central
			 * difference divides by 2*128 exactly as the chunk path's
			 * 2*spacing does. */
			const float ngx = lx / 128.0f;
			const float ngy = ly / 128.0f;
			float dzdx = ( lodgenTerrainHeightAt( hgt, hn, ngx + 1.0f, ngy )
				- lodgenTerrainHeightAt( hgt, hn, ngx - 1.0f, ngy ) ) / 256.0f;
			float dzdy = ( lodgenTerrainHeightAt( hgt, hn, ngx, ngy + 1.0f )
				- lodgenTerrainHeightAt( hgt, hn, ngx, ngy - 1.0f ) ) / 256.0f;
			if ( eroField ) {
				float egx = 0.0f, egy = 0.0f;
				eroField->gradAt( wx, wy, upt, &egx, &egy );
				dzdx += egx * lodgenErosion();
				dzdy += egy * lodgenErosion();
			}
			Vector3 nrm( -dzdx, -dzdy, 1.0f );
			nrm.normalize();
			out.msn[size_t( j ) * S + i] = lodgenTerrainMsnPixel( nrm );

			/* THE MACRO GRADIENT (lane LAND1). THIS is the site that writes
			 * the shipped sheet with --vt on; the chunk site above carries the
			 * same five lines, which is why the patch that made them is a
			 * script. */
			LodgenLandGuideCtx lguide;
			lguide.hgt = &hgt;
			lguide.hn = hn;
			lguide.ngOffX = -ringW / 128.0f;
			lguide.ngOffY = -ringS / 128.0f;
			float mgx = 0.0f, mgy = 0.0f;
			if ( lodgenLandGuideRule() != LODGEN_LANDGUIDE_OFF ) {
				double gdx = 0.0, gdy = 0.0;
				lodgenLandMacroGradient( lguide, double( wx ), double( wy ),
					&gdx, &gdy );
				mgx = float( gdx );
				mgy = float( gdy );
			}

			FloatVector4 color( 0.5f, 0.5f, 0.5f, 1.0f );
			/* THE MASK, through the SAME blend as the colour (2.2). Roughness
			 * starts fully rough and metallic at zero, which is what a texel
			 * with no resolvable material keeps. */
			float rough = 1.0f, metal = 0.0f;
			float emisRgb[3] = { 0.0f, 0.0f, 0.0f };
			int coverByte = 0;
			float coverTintD = 0.0f;
			float coverTint[3] = { 0.0f, 0.0f, 0.0f };
			if ( haveLand[ci] ) {
				const EsmLand & land = cells[ci];
				const float clx = lx - float( cx ) * 4096.0f;
				const float cly = ly - float( cy ) * 4096.0f;
				const int q = ( cly >= 2048.0f ? 2 : 0 ) + ( clx >= 2048.0f ? 1 : 0 );
				const float qx = ( clx - ( q & 1 ? 2048.0f : 0.0f ) ) / 2048.0f;
				const float qy = ( cly - ( q & 2 ? 2048.0f : 0.0f ) ) / 2048.0f;
				/* The mask maps are sampled at the SAME world point, the same
				 * 2,048-unit tiling and the same footprint-chosen mip as the
				 * diffuse, or the roughness would describe a different patch of
				 * ground from the colour beside it. */
				auto sampleMaskChannel = [&]( const QString & path, int channel,
					bool * got ) -> float {
					if ( got )
						*got = false;
					if ( path.isEmpty() )
						return 0.0f;
					const DDSTexture16 * tex = lodgenCachedTexture( bc, dataRoot, path );
					if ( !tex )
						return 0.0f;
					float u = std::fmod( wx / TILE, 1.0f );
					float v = std::fmod( wy / TILE, 1.0f );
					if ( u < 0.0f ) u += 1.0f;
					if ( v < 0.0f ) v += 1.0f;
					const float texelWorld = TILE / float( tex->getWidth() );
					const float mip = qBound( 0.0f,
						std::log2( qMax( 1.0f, upt / texelWorld ) ),
						float( tex->getMaxMipLevel() ) );
					if ( got )
						*got = true;
					return tex->getPixelT( u, v, mip )[channel];
				};
				auto layerRough = [&]( const LodgenMaterialMask & m ) -> float {
					if ( !m.haveRoughnessMap )
						return m.roughnessConst;
					bool got = false;
					const float c = sampleMaskChannel( m.roughnessTex, m.roughnessChannel, &got );
					if ( !got )
						return m.roughnessConst;     // named but unreadable: the constant, counted in the census
					// THE INVERSION, through the one gloss law (lodgen.h)
					return m.invertRoughness
						? 1.0f - lodgenLegacyGloss( m.glossScale, c )
						: qBound( 0.0f, c, 1.0f );
				};
				auto layerMetal = [&]( const LodgenMaterialMask & m ) -> float {
					if ( !m.haveMetallicMap )
						return m.metallicConst;      // legacy: 0, never a guess
					bool got = false;
					const float c = sampleMaskChannel( m.metallicTex, m.metallicChannel, &got );
					return got ? qBound( 0.0f, c, 1.0f ) : m.metallicConst;
				};
				auto layerEmis = [&]( const LodgenMaterialMask & m, float * rgb ) {
					rgb[0] = rgb[1] = rgb[2] = 0.0f;
					if ( !m.haveEmissive || m.emissiveTex.isEmpty() )
						return;
					const DDSTexture16 * tex = lodgenCachedTexture( bc, dataRoot, m.emissiveTex );
					if ( !tex )
						return;
					float u = std::fmod( wx / TILE, 1.0f );
					float v = std::fmod( wy / TILE, 1.0f );
					if ( u < 0.0f ) u += 1.0f;
					if ( v < 0.0f ) v += 1.0f;
					const float texelWorld = TILE / float( tex->getWidth() );
					const float mip = qBound( 0.0f,
						std::log2( qMax( 1.0f, upt / texelWorld ) ),
						float( tex->getMaxMipLevel() ) );
					const FloatVector4 e = tex->getPixelT( u, v, mip );
					for ( int k = 0; k < 3; k++ )
						rgb[k] = e[k];
				};
				auto sampleLtex = [&]( quint32 ltex ) -> FloatVector4 {
					QString d, n;
					world.ltexTextures( ltex, d, n );
					const DDSTexture16 * tex = d.isEmpty() ? nullptr
						: lodgenCachedTexture( bc, dataRoot, d );
					if ( !tex )
						return FloatVector4( 0.5f, 0.5f, 0.5f, 1.0f );
					/* THE DOMAIN WARP (lane TILING3). THIS is the site that
					 * writes the shipped sheet -- the PYRAMID/VT path -- and it
					 * is the one lane TILING2 missed and lost a relink to.
					 * Land diffuse only: the emissive lambda above, `_msn` and
					 * `.lodl` are untouched. Off returns the coordinate as it
					 * came in. */
					float swx = wx, swy = wy;
					lodgenLandGuidedWarp( lguide, wx, wy, mgx, mgy, &swx, &swy );
					float u = std::fmod( swx / TILE, 1.0f );
					float v = std::fmod( swy / TILE, 1.0f );
					if ( u < 0.0f ) u += 1.0f;
					if ( v < 0.0f ) v += 1.0f;
					const float texelWorld = TILE / float( tex->getWidth() );
					const float maxMip = float( tex->getMaxMipLevel() );
					/* THE MIP BIAS (lane TILING3), as a branch so that the
					 * default compiles the rung's expression exactly. */
					float mipRaw = std::log2( qMax( 1.0f, upt / texelWorld ) );
					const float mipBias = lodgenLandMipBias();
					if ( mipBias != 0.0f )
						mipRaw += mipBias;
					const float mip = qBound( 0.0f, mipRaw, maxMip );
					/* THE HEX TILING (lane TILING4), on the land diffuse
					 * lookup and on nothing else. ONE call, at BOTH sites:
					 * off, it evaluates the identical expression this line
					 * used to hold, so the default is the rung's bytes. It
					 * takes the WARP-offset coordinate, so setting both
					 * gives warp-then-hex (measured, and refused as a
					 * default: it costs the swirl and buys no repeat). */
					const FloatVector4 fp =
						lodgenLandHexTap( tex, swx, swy, TILE, u, v, mip, maxMip,
							&lguide );
					if ( !lodgenLandSampleAverage() )
						return fp;
					/* THE REPEAT-AVERAGED SAMPLE (lane TILING2). A landscape
					 * diffuse ships a full mip chain down to 1x1, and one
					 * repeat IS the whole texture, so the 1x1 texel is the
					 * exact average over a repeat and carries no periodic term
					 * at all. --land-detail adds back a fraction of the
					 * footprint sample's departure from that average, which
					 * scales the repeat by exactly the same fraction. */
					const FloatVector4 avg = tex->getPixelT( 0.5f, 0.5f, maxMip );
					const float kDetail = lodgenLandDetail();
					if ( kDetail <= 0.0f )
						return avg;
					return avg + ( fp - avg ) * kDetail;
				};
				const quint32 baseTex = land.baseTex[q] ? land.baseTex[q] : dominantBase;
				if ( baseTex ) {
					color = sampleLtex( baseTex );
					const LodgenMaterialMask & bm =
						maskCache.resolve( world, dataRoot, baseTex ).mat;
					rough = layerRough( bm );
					metal = layerMetal( bm );
					if ( wantEmissive )
						layerEmis( bm, emisRgb );
				}
				int nLayers = 0;
				for ( const EsmLandLayer & layer : land.layers[q] ) {
					const float fx = qBound( 0.0f, qx * 16.0f, 15.999f );
					const float fy = qBound( 0.0f, qy * 16.0f, 15.999f );
					const int ix = int( fx ), iy = int( fy );
					const float tx = fx - ix, ty = fy - iy;
					const float a =
						( layer.opacity[iy][ix] * ( 1 - tx ) + layer.opacity[iy][ix + 1] * tx ) * ( 1 - ty )
						+ ( layer.opacity[iy + 1][ix] * ( 1 - tx ) + layer.opacity[iy + 1][ix + 1] * tx ) * ty;
					if ( doCover && nLayers < int( aCover.size() ) )
						aCover[nLayers] = qBound( 0.0f, a, 1.0f );
					nLayers++;
					if ( a <= 0.001f )
						continue;
					const quint32 lform = layer.ltex ? layer.ltex : dominantBase;
					const FloatVector4 lc = sampleLtex( lform );
					const float aw = qBound( 0.0f, a, 1.0f );
					color = color + ( lc - color ) * aw;
					/* THE SAME BLEND, on the same operands, in the same order --
					 * the layer opacities, over the base, un-renormalised. A
					 * different composite for the mask would put the roughness of
					 * one material on the colour of another. VCLR is NOT applied
					 * (it is the artist's shading of the ground's COLOUR) and
					 * neither is the grass tint. */
					const LodgenMaterialMask & lm = maskCache.resolve( world, dataRoot, lform ).mat;
					rough = rough + ( layerRough( lm ) - rough ) * aw;
					metal = metal + ( layerMetal( lm ) - metal ) * aw;
					if ( wantEmissive ) {
						float le[3];
						layerEmis( lm, le );
						for ( int k = 0; k < 3; k++ )
							emisRgb[k] = emisRgb[k] + ( le[k] - emisRgb[k] ) * aw;
					}
				}
				if ( lodgenBlendEdges() == 1 ) {
					/* THE QUADRANT CROSS-FADE, COLOUR ONLY (lane TILING2).
					 *
					 * This is the path that writes the sheets a player sees --
					 * the per-chunk colour DDS comes out of the virtual-texture
					 * tile bake, not the stock chunk bake -- so the fix has to
					 * be here as well, and it is the same fix: within `margin`
					 * units of a 2,048-unit quadrant line the neighbouring
					 * quadrant's composite is evaluated AT THIS SAME WORLD
					 * POINT, with its own layer set and its own opacities read
					 * past its edge (hence clamped to its edge row), and the
					 * two are cross-faded with a quintic ease that is exactly
					 * 0.5 AT the line, so both sides meet there.
					 *
					 * The colour composite is repeated here rather than shared
					 * with the loop above ON PURPOSE: that loop also blends the
					 * roughness, the metalness, the emissive and the cover
					 * opacities, and NONE of those may move -- `_data` and the
					 * cover output stay byte-identical at every setting of this
					 * switch, which is what the F2 gate compares. */
					auto quadColorAt = [&]( const EsmLand & pl, int pq,
							float pqx, float pqy ) -> FloatVector4 {
						FloatVector4 c( 0.5f, 0.5f, 0.5f, 1.0f );
						const quint32 bt = pl.baseTex[pq] ? pl.baseTex[pq] : dominantBase;
						if ( bt )
							c = sampleLtex( bt );
						for ( const EsmLandLayer & layer : pl.layers[pq] ) {
							const float fx = qBound( 0.0f, pqx * 16.0f, 15.999f );
							const float fy = qBound( 0.0f, pqy * 16.0f, 15.999f );
							const int ix = int( fx ), iy = int( fy );
							const float tx = fx - ix, ty = fy - iy;
							const float a =
								( layer.opacity[iy][ix] * ( 1 - tx ) + layer.opacity[iy][ix + 1] * tx ) * ( 1 - ty )
								+ ( layer.opacity[iy + 1][ix] * ( 1 - tx ) + layer.opacity[iy + 1][ix + 1] * tx ) * ty;
							if ( a <= 0.001f )
								continue;
							const FloatVector4 lc = sampleLtex(
								layer.ltex ? layer.ltex : dominantBase );
							c = c + ( lc - c ) * qBound( 0.0f, a, 1.0f );
						}
						return c;
					};
					const float margin = lodgenBlendMargin();
					const float lxq = clx - ( q & 1 ? 2048.0f : 0.0f );
					const float lyq = cly - ( q & 2 ? 2048.0f : 0.0f );
					int sx = 0, sy = 0;
					float wxN = 0.0f, wyN = 0.0f;
					if ( lxq < margin ) {
						sx = -1;
						wxN = 1.0f - lxq / margin;
					} else if ( lxq > 2048.0f - margin ) {
						sx = 1;
						wxN = 1.0f - ( 2048.0f - lxq ) / margin;
					}
					if ( lyq < margin ) {
						sy = -1;
						wyN = 1.0f - lyq / margin;
					} else if ( lyq > 2048.0f - margin ) {
						sy = 1;
						wyN = 1.0f - ( 2048.0f - lyq ) / margin;
					}
					auto ease = []( float t ) -> float {
						const float u2 = qBound( 0.0f, t, 1.0f );
						// Perlin's quintic, halved: 1 at the line -> 0.5
						return 0.5f * u2 * u2 * u2 * ( u2 * ( u2 * 6.0f - 15.0f ) + 10.0f );
					};
					wxN = sx ? ease( wxN ) : 0.0f;
					wyN = sy ? ease( wyN ) : 0.0f;
					if ( wxN > 0.0f || wyN > 0.0f ) {
						auto nbr = [&]( int sxx, int syy ) -> FloatVector4 {
							int bx = ( q & 1 ) + sxx;
							int by = ( ( q >> 1 ) & 1 ) + syy;
							int ncx = cx, ncy = cy;
							if ( bx < 0 ) { bx = 1; ncx--; }
							else if ( bx > 1 ) { bx = 0; ncx++; }
							if ( by < 0 ) { by = 1; ncy--; }
							else if ( by > 1 ) { by = 0; ncy++; }
							if ( ncx < 0 || ncx >= rdim || ncy < 0 || ncy >= rdim )
								return color;
							const size_t nci = size_t( ncy ) * rdim + ncx;
							if ( !haveLand[nci] )
								return color;
							const float nlx = lx - float( ncx ) * 4096.0f
								- ( bx ? 2048.0f : 0.0f );
							const float nly = ly - float( ncy ) * 4096.0f
								- ( by ? 2048.0f : 0.0f );
							return quadColorAt( cells[nci], ( by << 1 ) | bx,
								nlx / 2048.0f, nly / 2048.0f );
						};
						const FloatVector4 cX = wxN > 0.0f ? nbr( sx, 0 ) : color;
						const FloatVector4 cY = wyN > 0.0f ? nbr( 0, sy ) : color;
						const FloatVector4 cD = ( wxN > 0.0f && wyN > 0.0f )
							? nbr( sx, sy ) : color;
						color = color * ( ( 1.0f - wxN ) * ( 1.0f - wyN ) )
							+ cX * ( wxN * ( 1.0f - wyN ) )
							+ cY * ( ( 1.0f - wxN ) * wyN )
							+ cD * ( wxN * wyN );
					}
				}
				if ( doCover ) {
					const LodgenVtQuadCover & qc = quadCover[ci * 4 + q];
					const int nL = qMin( nLayers, qc.layers.size() );
					float A = 0.0f;
					for ( int k = 0; k < nL; k++ )
						A += aCover[k];
					if ( A > 1.0f ) {
						const float s = 1.0f / A;
						for ( int k = 0; k < nL; k++ )
							aCover[k] *= s;
						A = 1.0f;
					}
					const float wBase = 1.0f - A;
					float dTex = wBase * qc.base.d;
					float sNum = wBase * qc.base.d * qc.base.s;
					float dTint = wBase * qc.base.tintD;
					float tNum[3];
					for ( int k = 0; k < 3; k++ )
						tNum[k] = wBase * qc.base.tintD * qc.base.t[k];
					for ( int k = 0; k < nL; k++ ) {
						const LodgenVtLtexVals & lv = qc.layers[k];
						dTex += aCover[k] * lv.d;
						sNum += aCover[k] * lv.d * lv.s;
						dTint += aCover[k] * lv.tintD;
						for ( int c3 = 0; c3 < 3; c3++ )
							tNum[c3] += aCover[k] * lv.tintD * lv.t[c3];
					}
					const float sTex = dTex > 0.0f ? sNum / dTex : 0.0f;
					const float theta = std::acos( qBound( 0.0f, nrm[2], 1.0f ) ) * 57.29577951f;
					const float gate = qBound( 0.0f, ( sTex + 5.0f - theta ) / 10.0f, 1.0f );
					coverByte = int( qBound( 0.0f, 255.0f * gate * dTex / coverFull + 0.5f, 255.0f ) );
					coverTintD = dTint;
					if ( dTint > 0.0f )
						for ( int k = 0; k < 3; k++ )
							coverTint[k] = tNum[k] / dTint;
					if ( coverByte > 0 )
						out.cover = true;
				}
				if ( land.hasColors ) {
					const float gx = qBound( 0.0f, clx / 4096.0f * 32.0f, 31.999f );
					const float gy = qBound( 0.0f, cly / 4096.0f * 32.0f, 31.999f );
					const int ix = int( gx ), iy = int( gy );
					const float tx = gx - ix, ty = gy - iy;
					for ( int k = 0; k < 3; k++ ) {
						const float c =
							( land.colors[iy][ix][k] * ( 1 - tx )
								+ land.colors[iy][ix + 1][k] * tx ) * ( 1 - ty )
							+ ( land.colors[iy + 1][ix][k] * ( 1 - tx )
								+ land.colors[iy + 1][ix + 1][k] * tx ) * ty;
						color[k] *= c / 255.0f;
					}
				}
				/* THE ROAD, between the VCLR multiply and the grass tint -- the
				 * same place, for the same two reasons, as the chunk path's
				 * (see lodgenBakeTerrainTextures). */
				if ( !roadPlane.empty() ) {
					const quint32 rp = roadPlane[size_t( j ) * S + i];
					/* Coverage and paint strength, the same two things and
					 * the same rule as the chunk path's -- see there. */
					const float raGeom = float( rp >> 24 ) / 255.0f;
					const float ra = ( coverOpts.roadOpacity == 1.0f )
						? raGeom : raGeom * coverOpts.roadOpacity;
					if ( raGeom > 0.0f ) {
						const float rc[3] = {
							float( ( rp >> 16 ) & 0xFF ) / 255.0f,
							float( ( rp >> 8 ) & 0xFF ) / 255.0f,
							float( rp & 0xFF ) / 255.0f };
						if ( ra > 0.0f )
							for ( int k = 0; k < 3; k++ )
								color[k] = color[k] + ( rc[k] - color[k] ) * ra;
						if ( doCover ) {
							const float keep = qBound( 0.0f,
								1.0f - raGeom * coverOpts.roadCoverSuppress, 1.0f );
							coverByte = int( float( coverByte ) * keep + 0.5f );
						}
					}
				}
				const float tintW = ( float( coverByte ) / 255.0f ) * coverOpts.tintStrength;
				if ( tintW > 0.0f && coverTintD > 0.0f )
					for ( int k = 0; k < 3; k++ )
						color[k] = color[k] + ( coverTint[k] - color[k] ) * tintW;
			}
			// THE EROSION SHADING -- see the chunk writer above; same rule, same
			// place, same refusal of a material tint.
			if ( eroField && g_landShade != 0.0f ) {
				const float dL = g_landShade * eroField->creviceAt( wx, wy, upt )
					* lodgenErosion() / 255.0f;
				for ( int k = 0; k < 3; k++ )
					color[k] = qBound( 0.0f, color[k] + dL, 1.0f );
			}
			// THE GRADE -- see the chunk writer above; same rule, same place.
			if ( g_landGrade != 1.0f )
				for ( int k = 0; k < 3; k++ )
					color[k] *= g_landGrade;
			out.colour[size_t( j ) * S + i] = 0xFF000000U
				| ( quint32( qBound( 0, int( color[0] * 255.0f + 0.5f ), 255 ) ) << 16 )
				| ( quint32( qBound( 0, int( color[1] * 255.0f + 0.5f ), 255 ) ) << 8 )
				| quint32( qBound( 0, int( color[2] * 255.0f + 0.5f ), 255 ) );

			// R AO, G wetness, B shore proximity, A cover
			const float h0 = heightAt( lx, ly );
			float occl = 0.0f;
			for ( const auto & d : dirs ) {
				float maxSlope = 0.0f;
				for ( float dist = 128.0f; dist <= 2048.0f; dist *= 1.5f ) {
					const float dh = heightAt( lx + d[0] * dist, ly + d[1] * dist ) - h0;
					if ( dh > 0.0f )
						maxSlope = qMax( maxSlope, dh / dist );
				}
				occl += maxSlope / ( 1.0f + maxSlope );
			}
			const float vis = qBound( 0.0f, 1.0f - occl / 8.0f * 1.6f, 1.0f );
			quint32 ao8 = quint32( qBound( 0.0f, vis * 255.0f + 0.5f, 255.0f ) );
			/* THE OBJECT TERM (lane GROUND1). `wx`/`wy` on this path are already
			 * WORLD coordinates, so the field is read with them unchanged. The
			 * census counts CONTENT texels only: tiles overlap by `border` and a
			 * texel counted twice is not a texel. */
			if ( objField ) {
				const float vo = lodgenObjectSkyVis( *objField, wx, wy, h0, dirs,
					coverOpts.terrainObjectAoStrength,
					coverOpts.terrainObjectAoSlab );
				const quint32 a2 = quint32( qBound( 0.0f,
					vis * vo * 255.0f + 0.5f, 255.0f ) );
				if ( objCensus && a2 < ao8 && i >= border && i < S - border
					&& j >= border && j < S - border ) {
					objCensus->texels++;
					objCensus->darkSum += double( ao8 - a2 );
				}
				ao8 = a2;
			}
			const quint32 wet8 = quint32( qBound( 0.0f, sampleU8( tWet, lx, ly ) + 0.5f, 255.0f ) );
			const quint32 sho8 = quint32( qBound( 0.0f, sampleU8( tShore, lx, ly ) + 0.5f, 255.0f ) );
			out.data[size_t( j ) * S + i] =
				( quint32( coverByte ) << 24 ) | ( ao8 << 16 ) | ( wet8 << 8 ) | sho8;

			/* THE MASK SHEET: R roughness, G metallic, B the SAME sky AO the
			 * retired data sheet carried in its R, A ground cover. Wetness and
			 * shore proximity are gone -- shore is a runtime subtraction from
			 * the .lodl water planes and wetness is a close-up effect. */
			const quint32 r8 = quint32( qBound( 0.0f, rough * 255.0f + 0.5f, 255.0f ) );
			const quint32 m8 = quint32( qBound( 0.0f, metal * 255.0f + 0.5f, 255.0f ) );
			out.mask[size_t( j ) * S + i] =
				( quint32( coverByte ) << 24 ) | ( r8 << 16 ) | ( m8 << 8 ) | ao8;
			if ( wantEmissive ) {
				auto e8 = []( float f ) {
					return quint32( qBound( 0.0f, f * 255.0f + 0.5f, 255.0f ) );
				};
				out.emissive[size_t( j ) * S + i] = 0xFF000000U
					| ( e8( emisRgb[0] ) << 16 ) | ( e8( emisRgb[1] ) << 8 ) | e8( emisRgb[2] );
			}

			// the height sheet: the shadow heightmap's own encoding, so the two
			// agree without a consumer converting between them
			const double p = double( h0 ) / 8.0 + 32767.0;
			out.height[size_t( j ) * S + i] =
				quint16( qBound( 0.0, std::floor( p + 0.5 ), 65535.0 ) );
		}
	}
	return true;
}

/*! A parent tile, box-filtered from the finer level's content mosaic.
 *
 *  The mosaic is the DEFINITION and it is what makes a parent's border
 *  correct: a border texel falls outside its own four children's footprint and
 *  must come from a fifth, sixth or seventh child. Defining the filter over
 *  "four tiles" cannot express that. The mosaic is never materialised -- held
 *  whole, level 2's would be 6.75 GiB -- only four child rows are staged. */
void lodgenVtFilterTile( const QMap<int, std::vector<LodgenVtStage>> & childRows,
	int childTilesX, int childTilesY, int content, int border, int stored,
	int tx, int ty, bool wantEmissive, LodgenVtStage & out )
{
	const int mosW = childTilesX * content, mosH = childTilesY * content;
	/* FIVE planes now, not three: colour, msn, the retired-but-still-staged
	 * data plane (the .btr path reads it), the MASK and the EMISSIVE. Adding
	 * them to the same accumulator rather than to a second pass is what keeps
	 * the one rounding law -- (a+b+c+d+2)>>2, round-half-up -- over every
	 * sheet. */
	auto sample = [&]( int u, int v, quint32 * bgra, quint16 * h16 ) {
		u = qBound( 0, u, mosW - 1 );
		v = qBound( 0, v, mosH - 1 );
		const int ctx = u / content, cty = v / content;
		auto row = childRows.constFind( cty );
		if ( row == childRows.constEnd() || ctx >= int( row->size() ) ) {
			bgra[0] = 0xFF808080U;
			bgra[1] = LODGEN_MSN_FLAT;
			bgra[2] = 0x00FFFFFFU;
			bgra[3] = 0x00FF00FFU;
			bgra[4] = 0xFF000000U;
			*h16 = 32767;
			return;
		}
		const LodgenVtStage & st = ( *row )[size_t( ctx )];
		const size_t idx = size_t( border + v % content ) * size_t( stored )
			+ size_t( border + u % content );
		bgra[0] = st.colour[idx];
		bgra[1] = st.msn[idx];
		bgra[2] = st.data[idx];
		bgra[3] = idx < st.mask.size() ? st.mask[idx] : 0x00FF00FFU;
		bgra[4] = idx < st.emissive.size() ? st.emissive[idx] : 0xFF000000U;
		*h16 = st.height[idx];
	};

	out.colour.assign( size_t( stored ) * stored, 0xFF808080U );
	out.msn.assign( size_t( stored ) * stored, LODGEN_MSN_FLAT );
	out.data.assign( size_t( stored ) * stored, 0x00FFFFFFU );
	out.mask.assign( size_t( stored ) * stored, 0x00FF00FFU );
	if ( wantEmissive )
		out.emissive.assign( size_t( stored ) * stored, 0xFF000000U );
	out.height.assign( size_t( stored ) * stored, 32767 );
	out.cover = false;

	for ( int j = 0; j < stored; j++ ) {
		const int v0 = 2 * ( ty * content + j - border );
		for ( int i = 0; i < stored; i++ ) {
			const int u0 = 2 * ( tx * content + i - border );
			quint32 acc[5][4] = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 },
				{ 0, 0, 0, 0 }, { 0, 0, 0, 0 } };
			quint32 hAcc = 0;
			for ( int dv = 0; dv < 2; dv++ ) {
				for ( int du = 0; du < 2; du++ ) {
					quint32 px[5];
					quint16 hv = 0;
					sample( u0 + du, v0 + dv, px, &hv );
					for ( int s = 0; s < 5; s++ ) {
						acc[s][0] += ( px[s] >> 16 ) & 0xFF;
						acc[s][1] += ( px[s] >> 8 ) & 0xFF;
						acc[s][2] += px[s] & 0xFF;
						acc[s][3] += ( px[s] >> 24 ) & 0xFF;
					}
					hAcc += hv;
				}
			}
			const size_t o = size_t( j ) * stored + i;
			out.colour[o] = 0xFF000000U | ( ( ( acc[0][0] + 2 ) >> 2 ) << 16 )
				| ( ( ( acc[0][1] + 2 ) >> 2 ) << 8 ) | ( ( acc[0][2] + 2 ) >> 2 );
			/* The msn is the one sheet whose channels are NOT independent: a box
			 * average of two opposite slopes gives a short vector whose decoded
			 * tilt magnitude is wrong, so the average is renormalised. */
			{
				float n[3];
				for ( int k = 0; k < 3; k++ )
					n[k] = float( ( acc[1][k] + 2 ) >> 2 ) / 255.0f * 2.0f - 1.0f;
				const float len = std::sqrt( n[0] * n[0] + n[1] * n[1] + n[2] * n[2] );
				if ( len > 1e-6f )
					for ( int k = 0; k < 3; k++ )
						n[k] /= len;
				/* n[] is in CHANNEL order, R G B = east, UP, north; the pixel
				 * function takes east, north, up. Passed straight through, every
				 * parent level swapped up and north, so alternate levels of the
				 * ladder stored a sideways normal (measured 2026-09-18: VT.2 up in
				 * green, VT.4 up in blue) and the land lit in black patches. */
				out.msn[o] = lodgenTerrainMsnPixel( Vector3( n[0], n[2], n[1] ) );
			}
			// cover averages plainly: it is linear in composition, so the mean
			// of four cover bytes IS the cover of the union at half resolution
			const quint32 cov = ( acc[2][3] + 2 ) >> 2;
			out.data[o] = ( cov << 24 ) | ( ( ( acc[2][0] + 2 ) >> 2 ) << 16 )
				| ( ( ( acc[2][1] + 2 ) >> 2 ) << 8 ) | ( ( acc[2][2] + 2 ) >> 2 );
			/* The mask filters plainly on all four channels. Roughness and
			 * metallic are material constants resampled, so their mean is the
			 * mean material -- unlike AO, which is a fixed-radius horizon march
			 * and therefore scale-dependent, exactly as the contract already
			 * says of the retired data sheet. */
			out.mask[o] = ( ( ( acc[3][3] + 2 ) >> 2 ) << 24 )
				| ( ( ( acc[3][0] + 2 ) >> 2 ) << 16 )
				| ( ( ( acc[3][1] + 2 ) >> 2 ) << 8 ) | ( ( acc[3][2] + 2 ) >> 2 );
			if ( wantEmissive )
				out.emissive[o] = 0xFF000000U | ( ( ( acc[4][0] + 2 ) >> 2 ) << 16 )
					| ( ( ( acc[4][1] + 2 ) >> 2 ) << 8 ) | ( ( acc[4][2] + 2 ) >> 2 );
			if ( cov )
				out.cover = true;
			out.height[o] = quint16( ( hAcc + 2 ) >> 2 );
		}
	}
}

} // namespace

/*! Bytes one tile occupies raw, from the geometry alone. Kept beside the
 *  estimator rather than derived from a written file, so `--vt-estimate` never
 *  has to bake anything to say what a bake would cost. */
static qint64 lodgenVtTileBytes( int stored, int mips, bool coverTile, bool withHeight,
	bool withEmissive = false, bool coverInColor = false )
{
	qint64 n = 0;
	for ( int m = 0; m < mips; m++ ) {
		const qint64 s = stored >> m;
		const qint64 blocks = ( s / 4 ) * ( s / 4 );
		/* EXACTLY ONE sheet carries the ground-cover alpha and it is the only
		 * one that doubles. By default it is the mask (bungo's open question;
		 * `--vt-cover-in-color` is the other arm and makes the COLOUR sheet the
		 * BC3 one instead). */
		n += blocks * ( ( coverTile && coverInColor ) ? 16 : 8 );    // colour
		n += blocks * 8;                                            // msn, BC1
		n += blocks * ( ( coverTile && !coverInColor ) ? 16 : 8 );  // mask
		if ( withHeight )
			n += s * s * 2;                         // height, R16 uncompressed
		if ( withEmissive )
			n += blocks * 8;                        // emissive, BC1, no alpha
	}
	return n;
}

bool lodgenVtEstimateBounds( int wW, int wS, int wE, int wN,
	const LodgenVtOptions & opts, bool alsoBtr, LodgenVtEstimateOut * out )
{
	if ( !out )
		return false;
	*out = LodgenVtEstimateOut();
	LodgenVtLevel levels[8];
	int coarsest = 0;
	const int n = lodgenVtLevelsFromBounds( wW, wS, wE, wN, opts, levels, 8, &coarsest );
	if ( n < 1 )
		return false;
	const int stored = opts.content + 2 * opts.border;
	out->levels = n;
	out->coarsestDim = coarsest;
	out->shortened = ( coarsest < 32 );
	const qint64 rawNoCover = lodgenVtTileBytes( stored, opts.mips, false, opts.height,
		false, opts.coverInColor );
	const qint64 rawCover = lodgenVtTileBytes( stored, opts.mips, true, opts.height,
		false, opts.coverInColor );
	for ( int i = 0; i < n; i++ ) {
		const qint64 tiles = qint64( levels[i].tilesX ) * levels[i].tilesY;
		out->levelDims[i] = levels[i].dim;
		out->levelTiles[i] = tiles;
		out->tiles += tiles;
		/* The expected, not the worst, case: 13,850 of the Commonwealth's
		 * 147,456 quadrants carry grass (9.393%), and a tile holds 4*dim^2 of
		 * them, so P(this tile has any) = 1 - (1 - 0.09393)^(4*dim^2). At dim 2
		 * that is 79.4% and at dim 4 and coarser it is effectively 1 -- which
		 * is the honest headline: with cover on, nearly every tile is BC3. */
		double pCover = 0.0;
		if ( opts.cover.cover ) {
			const double q = std::pow( 1.0 - 0.09393, double( 4 * levels[i].dim * levels[i].dim ) );
			pCover = 1.0 - q;
		}
		const qint64 avgRaw = qint64( double( rawNoCover ) * ( 1.0 - pCover )
			+ double( rawCover ) * pCover + 0.5 );
		// payloads are 4,096-aligned and the pad is deterministic, not the
		// 2 KiB average a random payload size would give
		const qint64 padNo = ( 4096 - rawNoCover % 4096 ) % 4096;
		const qint64 padCo = ( 4096 - rawCover % 4096 ) % 4096;
		const qint64 avgPad = qint64( double( padNo ) * ( 1.0 - pCover )
			+ double( padCo ) * pCover + 0.5 );
		out->pyramidBytes += tiles * ( avgRaw + avgPad ) + tiles * 24 + 4096;
	}
	if ( alsoBtr ) {
		/* The chunk sheets do NOT go away: the pyramid supplies their bytes,
		 * it does not replace the files. Today's three sheets are 174,888 each
		 * at dim 4..32; under cover the data sheet doubles wherever the chunk
		 * has grass, which at 64 quadrants a chunk is 99.8% of them. */
		qint64 chunks = 0;
		for ( int d : { 4, 8, 16, 32 } ) {
			const int nx = ( wE - lodgenVtFloorTo( wW, d ) + d ) / d;
			const int ny = ( wN - lodgenVtFloorTo( wS, d ) + d ) / d;
			chunks += qint64( nx ) * ny;
		}
		const double pChunkCover = opts.cover.cover
			? ( 1.0 - std::pow( 1.0 - 0.09393, 64.0 ) ) : 0.0;
		out->btrBytes = chunks * ( 174888 * 2
			+ qint64( 174888.0 * ( 1.0 - pChunkCover ) + 349648.0 * pChunkCover + 0.5 ) );
	}
	out->deliveredBytes = out->pyramidBytes + out->btrBytes;
	return true;
}

bool lodgenVtEstimate( const EsmWorld & world, const LodgenVtOptions & opts,
	bool alsoBtr, LodgenVtEstimateOut * out )
{
	int wW = 0, wS = 0, wE = 0, wN = 0;
	world.cellBounds( wW, wS, wE, wN );
	if ( opts.haveRegion ) {
		wW = qMax( wW, qMin( opts.region[0], opts.region[2] ) );
		wE = qMin( wE, qMax( opts.region[0], opts.region[2] ) );
		wS = qMax( wS, qMin( opts.region[1], opts.region[3] ) );
		wN = qMin( wN, qMax( opts.region[1], opts.region[3] ) );
	}
	return lodgenVtEstimateBounds( wW, wS, wE, wN, opts, alsoBtr, out );
}

/*! The whole pass: bake the finest level from the paint, filter every coarser
 *  level from it, and stream both into one container per level.
 *
 *  The traversal is the normative one and it is what bounds the memory. Rows of
 *  the finest level are baked in increasing north-to-south order; a parent row
 *  is assembled the moment its four child rows exist (2p-1, 2p, 2p+1 and
 *  2p+2 -- FOUR, not three: a parent's own border reaches 16 child texels past
 *  its content and 16 > 0, so the row below the obvious three is needed too),
 *  and child rows no future parent can reach are released at once. Nothing
 *  holds a level: level 2's content mosaic alone would be 6.75 GiB and its
 *  staging 7.6 GiB.
 *
 *  Tiles are appended to their container in table-index order, which with the
 *  4,096-aligned offsets and the zero pad is what makes two runs of the same
 *  bake byte-identical. */
bool lodgenBakeTerrainVt( const EsmWorld & world, const QString & dataRoot,
	const QString & outDir, const LodgenVtOptions & opts, LodgenBakeCaches * caches,
	QString * report, QString * error )
{
	/* The slab law's known-answer control, on the PYRAMID path. It has to sit
	 * here as well as in `lodgenBakeTerrainTextures`: measured 2026-09-18, a
	 * `--vt` bake never enters that per-chunk entry at all -- with
	 * WW_TERRAIN_RING_TEST set it prints the ring test exactly once, from
	 * `lodgenWarmSharedIndices`, and not twice. One-shot, and on this entry it
	 * runs single-threaded before any tile worker starts. */
	lodgenObjectSlabSelfTestOnce();
	auto fail = [error]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};
	LodgenVtLevel levels[8];
	int coarsest = 0, wW = 0, wS = 0, wE = 0, wN = 0;
	const int nLevels = lodgenVtBuildLevels( world, opts, levels, 8, &coarsest, &wW, &wS, &wE, &wN );
	if ( nLevels < 1 )
		return fail( QStringLiteral( "the worldspace has no indexed cells" ) );
	const int content = opts.content, border = opts.border, mips = opts.mips;
	const int stored = content + 2 * border;
	if ( content < 128 || content > 512 || ( content & ( content - 1 ) ) )
		return fail( QStringLiteral( "--vt-content must be a power of two between 128 and 512" ) );
	if ( border % 4 )
		return fail( QStringLiteral( "--vt-border must be a multiple of 4; a BC block is 4x4 and a "
			"border that splits one makes a tile depend on its neighbour's bake" ) );
	if ( mips < 1 || ( border >> ( mips - 1 ) ) % 4
		|| ( ( border >> ( mips - 1 ) ) << ( mips - 1 ) ) != border )
		return fail( QString( "--vt-border %1 cannot carry %2 mips; the border halves at every mip "
			"and must stay a multiple of 4 (that needs at least %3)" )
			.arg( border ).arg( mips ).arg( 4 << ( mips - 1 ) ) );

	LodgenBakeCaches * ownCaches = caches ? nullptr : lodgenCreateBakeCaches();
	struct CacheGuard
	{
		LodgenBakeCaches * p;
		~CacheGuard() { if ( p ) lodgenDestroyBakeCaches( p ); }
	} cacheGuard{ ownCaches };
	LodgenBakeCaches & bc = caches ? *caches : *ownCaches;

	const QString ws = world.worldspaceEdid();
	if ( ws.isEmpty() || ws.size() > 31 )
		return fail( QString( "worldspace %1 has a %2-character EDID; the container stores 32 bytes "
			"with a terminator, so this worldspace cannot be named in a .lodt" )
			.arg( ws ).arg( ws.size() ) );
	/* ONE ROOT (lane LAYOUT1, 2026-09-16): the pyramid's levels and its index
	 * used to sit in `Terrain\` beside the landscape file; both now live under
	 * `FO4CSLOD\<ws>\`, and the game-relative `file` written into the index
	 * below says so as well. */
	const QString dir = lodgenFo4csWorldDir( outDir, ws );
	if ( !QDir().mkpath( dir ) )
		return fail( QString( "could not create %1" ).arg( dir ) );

	const quint64 vhgtHash = world.vhgtCorpusHash();
	const quint64 paintHash = world.paintCorpusHash();

	/* THE EMISSIVE SHEET IS DECIDED BEFORE ANY CONTAINER OPENS, because its
	 * presence is a HEADER field and a tile's payload size depends on it.
	 *
	 * bungo's ruling, 2026-09-11 09:5x: an EMISSIVE sheet "when any layer
	 * supplies one (absent = none, named in the index)". So every LTEX the
	 * bake's own rectangle paints is resolved once, here, through the same
	 * cache the tiles then reuse -- no layer is read twice and the pass costs
	 * one walk of the region's LAND records. A worldspace whose landscape
	 * names no emissive map writes NO emissive sheet at all, which is the
	 * fallback, and the index says so in words rather than shipping a black
	 * sheet nobody can tell from a missing one. */
	LodgenVtMaskCache maskCache;
	bool wantEmissive = false;
	int layerFormsSeen = 0;
	{
		EsmLand land;
		for ( int cy = levels[0].south; cy <= levels[0].north; cy++ ) {
			for ( int cx = levels[0].west; cx <= levels[0].east; cx++ ) {
				if ( !world.land( cx, cy, land ) )
					continue;
				for ( int q = 0; q < 4; q++ ) {
					if ( land.baseTex[q] ) {
						maskCache.resolve( world, dataRoot, land.baseTex[q] );
						layerFormsSeen++;
					}
					for ( const EsmLandLayer & layer : land.layers[q] ) {
						if ( !layer.ltex )
							continue;
						maskCache.resolve( world, dataRoot, layer.ltex );
						layerFormsSeen++;
					}
				}
			}
		}
		wantEmissive = maskCache.withEmissive > 0;
	}

	std::vector<std::unique_ptr<LodvWriter>> writers;
	std::vector<QString> paths;
	for ( int l = 0; l < nLevels; l++ ) {
		LodvHeaderFields h;
		h.flags = LODV_FLAG_ROW_ORDER_NORTH_UP
			| ( opts.finestDim == 1 ? LODV_FLAG_FULL_MODE : 0u );
		h.vhgtCorpusHash = vhgtHash;
		h.paintCorpusHash = paintHash;
		h.worldspaceEdid = ws;
		h.west = qint16( levels[l].west );
		h.east = qint16( levels[l].east );
		h.south = qint16( levels[l].south );
		h.north = qint16( levels[l].north );
		h.worldWest = qint16( wW );
		h.worldEast = qint16( wE );
		h.worldSouth = qint16( wS );
		h.worldNorth = qint16( wN );
		h.levelDim = quint16( levels[l].dim );
		h.levelIndex = quint16( l );
		h.levelCount = quint16( nLevels );
		h.tilesX = quint16( levels[l].tilesX );
		h.tilesY = quint16( levels[l].tilesY );
		h.contentTexels = quint16( content );
		h.borderTexels = quint16( border );
		h.storedTexels = quint16( stored );
		h.mipCount = quint8( mips );
		/* THE SHEET SET (2.2, version 2): colour, msn, mask, then HEIGHT if it
		 * was asked for, then EMISSIVE if any layer in this bake supplies one.
		 * The order here is the order the payload concatenates them in, so the
		 * two are written from one place and cannot drift. */
		h.sheetCount = quint8( 3 + ( opts.height ? 1 : 0 ) + ( wantEmissive ? 1 : 0 ) );
		// B >= ceil(A/2) at the coarsest stored mip: border 8 with 2 mips has 4
		// there, which carries 8x. The container DECLARES what it supports and
		// the consumer clamps its sampler; a border sized for a setting the
		// consumer may not use is disk given away.
		h.anisoSupported = quint8( qMin( 16, 2 * ( border >> ( mips - 1 ) ) ) );
		h.compression = quint8( opts.compression );
		h.coverNormalisation = opts.cover.coverFull;
		h.tintStrength = opts.cover.tintStrength;
		for ( int i = 0; i < nLevels && i < 8; i++ )
			h.levelDims[i] = quint16( levels[i].dim );
		const quint16 colorCoverFmt = opts.coverInColor ? LODV_DXGI_BC3_UNORM : LODV_DXGI_BC1_UNORM;
		const quint16 maskCoverFmt = opts.coverInColor ? LODV_DXGI_BC1_UNORM : LODV_DXGI_BC3_UNORM;
		h.sheets[0] = { LODV_DXGI_BC1_UNORM, colorCoverFmt, LODV_ROLE_COLOR, 1 };
		h.sheets[1] = { LODV_DXGI_BC1_UNORM, LODV_DXGI_BC1_UNORM, LODV_ROLE_MSN, 0 };
		h.sheets[2] = { LODV_DXGI_BC1_UNORM, maskCoverFmt, LODV_ROLE_MASK, 0 };
		int nextSheet = 3;
		if ( opts.height )
			h.sheets[nextSheet++] = { LODV_DXGI_R16_UNORM, LODV_DXGI_R16_UNORM, LODV_ROLE_HEIGHT, 0 };
		if ( wantEmissive )
			h.sheets[nextSheet++] = { LODV_DXGI_BC1_UNORM, LODV_DXGI_BC1_UNORM, LODV_ROLE_EMISSIVE, 0 };
		const QString path = QString( "%1/%2.VT.%3.lodt" ).arg( dir ).arg( ws ).arg( levels[l].dim );
		auto w = std::make_unique<LodvWriter>();
		QString werr;
		if ( !w->begin( path, h, &werr ) )
			return fail( werr );
		writers.push_back( std::move( w ) );
		paths.push_back( path );
		lodgenNoteLayoutFile( path );
	}

	LodgenVtLandCache landCache;

	/* ROADS. Gathered ONCE for the whole pyramid, over the finest level's cell
	 * rectangle, because every tile at every level stands on the same ground:
	 * the ESM walk and the model loads are paid once and each tile pays only
	 * its own scan conversion. Coarser levels inherit the roads through the box
	 * filter, exactly as they inherit the splat. */
	std::unique_ptr<LodgenRoadSet> roadSet;
	LodgenRoadCensus roadCensus;
	if ( opts.cover.roads ) {
		roadSet.reset( new LodgenRoadSet );
		roadSet->gather( world, dataRoot, levels[0].west, levels[0].south,
			levels[0].east, levels[0].north, opts.cover.roadRaised,
			opts.cover.roadSidewalks );
		roadCensus.add( roadSet->gatherCensus() );
	}

	/* THE OBJECT HEIGHT FIELD (lane GROUND1), gathered ONCE for the whole
	 * pyramid for the same reason the roads are: every tile at every level
	 * stands on the same ground, so the ESM walk and the LOD model loads are
	 * paid once. Coarser levels inherit the darkened AO byte through the
	 * existing box filter, exactly as they inherit the splat. */
	std::unique_ptr<LodgenObjectHeightField> objField;
	LodgenObjectAoCensus objCensus;
	if ( opts.cover.terrainObjectAo ) {
		objField.reset( new LodgenObjectHeightField );
		objField->gather( world, dataRoot, levels[0].west, levels[0].south,
			levels[0].east, levels[0].north );
		objCensus.add( objField->gatherCensus() );
		if ( !opts.cover.dumpObjectAoPath.isEmpty() )
			objField->dump( opts.cover.dumpObjectAoPath );
	}

	// static_cast, not size_t(...): the functional cast is a most vexing parse here
	// and would declare a function taking an unnamed size_t.
	std::vector<QMap<int, std::vector<LodgenVtStage>>> rings( static_cast<size_t>( nLevels ) );
	std::vector<int> nextParent( size_t( nLevels ), 0 );
	qint64 presentTotal = 0, coverTiles = 0;

	if ( opts.cover.cover )
		world.setGrassTintResolver( &lodgenGrassTintResolve, &bc );

	auto writeTile = [&]( int lv, const LodgenVtStage & st ) -> bool {
		const QByteArray raw = lodgenVtEncodeTile( st, stored, mips, opts.height,
			wantEmissive, opts.coverInColor );
		QString werr;
		if ( !writers[size_t( lv )]->addTile( raw, st.cover, &werr ) )
			return fail( werr );
		presentTotal++;
		if ( st.cover )
			coverTiles++;
		return true;
	};

	/* The .btr chunk sheets, when they are asked for, are assembled HERE --
	 * from the level's own staging, while its two rows are live -- and not
	 * from the written container. Reading them back would mean decoding BC
	 * blocks and re-encoding them, which is exactly what the filter law
	 * forbids and what would put the assembled sheet a quantisation step away
	 * from a direct bake. A chunk at dim D is the 2x2 content blocks of level
	 * D/2; a chunk row lines up with a tile-row PAIR because both are anchored
	 * to the same origin, which is why this can happen at parent-row time. */
	auto assembleChunkRow = [&]( int lv, int parentRow ) -> bool {
		if ( opts.btrTexDir.isEmpty() )
			return true;
		const int d = levels[lv].dim;
		const int D = d * 2;
		if ( !opts.btrDims.contains( D ) )
			return true;
		const QMap<int, std::vector<LodgenVtStage>> & rows = rings[size_t( lv )];
		auto rTop = rows.constFind( parentRow * 2 );
		auto rBot = rows.constFind( parentRow * 2 + 1 );
		if ( rTop == rows.constEnd() || rBot == rows.constEnd() )
			return true;
		const int RES = content * 2;
		for ( int px = 0; px + 1 < levels[lv].tilesX; px += 2 ) {
			const int chunkX = levels[lv].west + px * d;
			const int chunkY = levels[lv].north - ( parentRow * 2 + 2 ) * d + 1;
			// a chunk entirely outside the worldspace is padding, not a chunk
			if ( chunkX > wE || chunkY > wN || chunkX + D - 1 < wW || chunkY + D - 1 < wS )
				continue;
			std::vector<quint32> col( size_t( RES ) * RES, 0xFF808080U );
			std::vector<quint32> nrm( size_t( RES ) * RES, LODGEN_MSN_FLAT );
			std::vector<quint32> dat( size_t( RES ) * RES, 0x00FFFFFFU );
			int coverMax = 0;
			for ( int by = 0; by < 2; by++ ) {
				const std::vector<LodgenVtStage> & row = ( by == 0 ) ? *rTop : *rBot;
				for ( int bx = 0; bx < 2; bx++ ) {
					if ( px + bx >= int( row.size() ) )
						continue;
					const LodgenVtStage & st = row[size_t( px + bx )];
					for ( int j = 0; j < content; j++ ) {
						for ( int i = 0; i < content; i++ ) {
							const size_t src = size_t( border + j ) * stored + size_t( border + i );
							const size_t dst = size_t( by * content + j ) * RES
								+ size_t( bx * content + i );
							col[dst] = st.colour[src];
							nrm[dst] = st.msn[src];
							const quint32 cov = ( st.data[src] >> 24 ) & 0xFF;
							if ( int( cov ) > coverMax )
								coverMax = int( cov );
							dat[dst] = st.data[src];
						}
					}
				}
			}
			// the assembled sheet's coverMax is taken over the assembled plane,
			// exactly as a direct bake takes it over its own, so the BC1/BC3
			// switch has ONE definition on both paths
			if ( coverMax == 0 )
				for ( quint32 & v : dat )
					v |= 0xFF000000U;
			const QString base = QString( "%1/%2.%3.%4.%5" )
				.arg( opts.btrTexDir ).arg( ws ).arg( D ).arg( chunkX ).arg( chunkY );
			const quint32 stamp0 = coverMax > 0 ? 0x56435757U : 0U;
			const quint32 stamp1 = coverMax > 0
				? ( ( 1U << 24 ) | quint32( qBound( 0, int( opts.cover.coverFull + 0.5f ), 0xFFFFFF ) ) )
				: 0U;
			/* VANILLA REUSE (lane TILING3). THIS is the writer that ships --
			 * the chunk sheet a player sees comes out of this assembly, not
			 * the stock per-chunk bake -- so the ruling has to land here, and
			 * it lands at both writers through one function so the two cannot
			 * drift. `_data` is NOT touched at any setting. */
			QByteArray msnCopy, colCopy;
			lodgenVanillaChunkSheets( world, ws, D, chunkX, chunkY, RES,
				lodgenChunkHasLandPaint( world, chunkX, chunkY, D ),
				col, nrm, msnCopy, colCopy );
			if ( !lodgenWriteChunkSheets( base, RES, col, nrm, colCopy, msnCopy )
				|| !lodgenWriteDds( base + QStringLiteral( "_data.DDS" ), RES, RES, dat,
					coverMax > 0, 0, false, stamp0, stamp1 ) )
				return fail( QString( "could not write the assembled sheets for chunk (%1,%2) at dim %3" )
					.arg( chunkX ).arg( chunkY ).arg( D ) );
		}
		return true;
	};

	std::function<bool( int, int )> propagate = [&]( int lv, int r ) -> bool {
		if ( lv + 1 >= nLevels )
			return true;
		const int lastChild = levels[lv].tilesY - 1;
		while ( nextParent[size_t( lv )] < levels[lv + 1].tilesY ) {
			const int p = nextParent[size_t( lv )];
			const int need = qMin( 2 * p + 2, lastChild );
			if ( r < need )
				break;
			std::vector<LodgenVtStage> prow( size_t( levels[lv + 1].tilesX ) );
			for ( int tx = 0; tx < levels[lv + 1].tilesX; tx++ )
				lodgenVtFilterTile( rings[size_t( lv )], levels[lv].tilesX, levels[lv].tilesY,
					content, border, stored, tx, p, wantEmissive, prow[size_t( tx )] );
			for ( int tx = 0; tx < levels[lv + 1].tilesX; tx++ )
				if ( !writeTile( lv + 1, prow[size_t( tx )] ) )
					return false;
			if ( !assembleChunkRow( lv, p ) )
				return false;
			rings[size_t( lv + 1 )][p] = std::move( prow );
			nextParent[size_t( lv )] = p + 1;
			// parent p+1 reaches down to child row 2(p+1)-1; nothing below it
			// will be read again
			const int keepFrom = 2 * ( p + 1 ) - 1;
			for ( auto it = rings[size_t( lv )].begin(); it != rings[size_t( lv )].end(); ) {
				if ( it.key() < keepFrom )
					it = rings[size_t( lv )].erase( it );
				else
					++it;
			}
			if ( !propagate( lv + 1, p ) )
				return false;
		}
		return true;
	};

	for ( int ty = 0; ty < levels[0].tilesY; ty++ ) {
		std::vector<LodgenVtStage> row( size_t( levels[0].tilesX ) );
		for ( int tx = 0; tx < levels[0].tilesX; tx++ ) {
			const int cellX0 = levels[0].west + tx * levels[0].dim;
			const int cellY0 = levels[0].north - ( ty + 1 ) * levels[0].dim + 1;
			if ( !lodgenBakeVtTile( world, dataRoot, bc, opts.cover, landCache, maskCache,
				wantEmissive, cellX0, cellY0, levels[0].dim, content, border, row[size_t( tx )],
				roadSet.get(), &roadCensus, objField.get(), &objCensus ) )
				return fail( QString( "could not bake tile (%1,%2)" ).arg( tx ).arg( ty ) );
		}
		for ( int tx = 0; tx < levels[0].tilesX; tx++ )
			if ( !writeTile( 0, row[size_t( tx )] ) )
				return false;
		rings[0][ty] = std::move( row );
		// only the cells a later tile row can still reach stay cached
		landCache.dropBelow( levels[0].north - ( ty + 2 ) * levels[0].dim );
		if ( !propagate( 0, ty ) )
			return false;
		if ( opts.progress && !opts.progress( opts.progressUser, ty + 1, levels[0].tilesY ) )
			return fail( QStringLiteral( "cancelled" ) );
	}

	qint64 fileBytesTotal = 0;
	for ( int l = 0; l < nLevels; l++ ) {
		QString werr;
		if ( !writers[size_t( l )]->finish( &werr ) )
			return fail( QString( "%1: %2" ).arg( paths[size_t( l )] ).arg( werr ) );
		QFileInfo fi( paths[size_t( l )] );
		fileBytesTotal += fi.size();
	}

	/* The index. It lives in Data\Terrain\, deliberately NOT under materials\,
	 * so lodmSourceCandidate() can never produce its path and the two readers
	 * that ignore `kind` can never open it. `family` is vestigial here -- it
	 * carries the legacy/PBR MATERIAL split and a tile pyramid is neither --
	 * and `kind` is the discriminator. */
	{
		QJsonObject root;
		root.insert( QStringLiteral( "lodm" ), 1 );
		/* THE FAMILY WORD IS REAL NOW, and it MEANS it (bungo, 2026-09-11
		 * 09:5x: "you can mirror how it is set up for the .lodm").
		 *
		 * It said "legacy" until today and the contract called it vestigial,
		 * because the sheets were terrain's own invention and neither family
		 * described them. They are the OBJECT family's now: the mask sheet is
		 * the `rmaos` slot's channels in the `rmaos` slot's order, so a
		 * consumer that knows `.lodm` 2.1 knows this pyramid without a second
		 * table. Legacy materials are CONVERTED at bake -- gloss inverted into
		 * roughness, metallic 0 -- so what ships is PBR whatever the source
		 * was, and `terrain.maskRules` below says how many layers came by which
		 * road rather than leaving the word to be taken on trust. */
		root.insert( QStringLiteral( "family" ), QStringLiteral( "pbr" ) );
		root.insert( QStringLiteral( "kind" ), QStringLiteral( "terrainVT" ) );
		QJsonObject t;
		t.insert( QStringLiteral( "worldspace" ), ws );
		QJsonObject ext;
		ext.insert( QStringLiteral( "south" ), wS );
		ext.insert( QStringLiteral( "west" ), wW );
		ext.insert( QStringLiteral( "north" ), wN );
		ext.insert( QStringLiteral( "east" ), wE );
		t.insert( QStringLiteral( "extent" ), ext );
		t.insert( QStringLiteral( "cellUnits" ), 4096 );
		t.insert( QStringLiteral( "content" ), content );
		t.insert( QStringLiteral( "border" ), border );
		t.insert( QStringLiteral( "stored" ), stored );
		t.insert( QStringLiteral( "mips" ), mips );
		t.insert( QStringLiteral( "aniso" ), qMin( 16, 2 * ( border >> ( mips - 1 ) ) ) );
		t.insert( QStringLiteral( "rowOrder" ), QStringLiteral( "northUp" ) );
		t.insert( QStringLiteral( "compression" ),
			opts.compression ? QStringLiteral( "zlib" ) : QStringLiteral( "none" ) );
		auto hex = []( quint64 v ) {
			return QStringLiteral( "0x" )
				+ QString::number( v, 16 ).toUpper().rightJustified( 16, QChar( '0' ) );
		};
		t.insert( QStringLiteral( "vhgtCorpusHash" ), hex( vhgtHash ) );
		t.insert( QStringLiteral( "paintCorpusHash" ), hex( paintHash ) );
		QJsonArray sheets;
		auto sheet = []( const char * role, int dxgi, int dxgiCover, const char * space,
			const char * channels ) {
			QJsonObject s;
			s.insert( QStringLiteral( "role" ), QLatin1String( role ) );
			s.insert( QStringLiteral( "dxgi" ), dxgi );
			s.insert( QStringLiteral( "dxgiWithCover" ), dxgiCover );
			s.insert( QStringLiteral( "colorSpace" ), QLatin1String( space ) );
			s.insert( QStringLiteral( "channels" ), QLatin1String( channels ) );
			return s;
		};
		const int colorCover = opts.coverInColor ? 77 : 71;
		const int maskCover = opts.coverInColor ? 71 : 77;
		sheets.append( sheet( "color", 71, colorCover, "sRGB",
			opts.coverInColor
				? "RGB albedo, grass tint folded in, A ground cover (the object family's coverage slot)"
				: "RGB albedo, grass tint folded in" ) );
		sheets.append( sheet( "msn", 71, 71, "linear", "model-space normal, 0.5+0.5 encoded" ) );
		sheets.append( sheet( "mask", 71, maskCover, "linear",
			opts.coverInColor
				? "rmaos: R roughness, G metallic, B sky-free AO, A unused (0)"
				: "rmaos: R roughness, G metallic, B sky-free AO, A ground cover" ) );
		if ( opts.height )
			sheets.append( sheet( "height", 56, 56, "linear",
				"R16_UNORM, height/8 + 32767, the shadow heightmap's own encoding" ) );
		if ( wantEmissive )
			sheets.append( sheet( "emissive", 71, 71, "linear",
				"RGB emissive colour, no alpha" ) );
		t.insert( QStringLiteral( "sheets" ), sheets );
		/* ABSENCE, SAID IN WORDS. A consumer must be able to tell "this
		 * worldspace emits nothing" from "the writer forgot"; a black sheet
		 * says neither. */
		t.insert( QStringLiteral( "emissive" ), wantEmissive
			? QStringLiteral( "present" ) : QStringLiteral( "none" ) );
		/* CHANNELS THIS CONTAINER DOES NOT CARRY. Naming an absent channel
		 * here costs one key and saves a reader looking for a channel that was
		 * removed on purpose. */
		{
			QJsonObject dropped;
			dropped.insert( QStringLiteral( "shoreProximity" ),
				QStringLiteral( "runtime: subtract the .lodl water body plane from the height "
					"at the sample (docs/LODGEN_BTD_FORMAT.md, \"What is NOT in this file\")" ) );
			dropped.insert( QStringLiteral( "wetness" ),
				QStringLiteral( "not baked: a close-up effect; far wetness is a weather state "
					"the runtime owns" ) );
			t.insert( QStringLiteral( "dropped" ), dropped );
		}
		/* THE PER-LAYER RULE CENSUS. Every landscape texture the bake's own
		 * rectangle paints, counted by the rule that served its mask -- so the
		 * `family: pbr` above is auditable instead of asserted, and a
		 * worldspace served entirely by `none-default` cannot pass for a
		 * measurement. */
		{
			QJsonObject rules;
			rules.insert( QStringLiteral( "pbrm" ), maskCache.ruleCounts[LODGEN_MASK_PBRM] );
			rules.insert( QStringLiteral( "legacyInverted" ),
				maskCache.ruleCounts[LODGEN_MASK_LEGACY_INVERTED] );
			rules.insert( QStringLiteral( "noneDefault" ), maskCache.ruleCounts[LODGEN_MASK_NONE] );
			rules.insert( QStringLiteral( "withRoughnessMap" ), maskCache.withRoughnessMap );
			rules.insert( QStringLiteral( "withMetallicMap" ), maskCache.withMetallicMap );
			rules.insert( QStringLiteral( "withEmissiveMap" ), maskCache.withEmissive );
			rules.insert( QStringLiteral( "distinctLtex" ), maskCache.byForm.size() );
			rules.insert( QStringLiteral( "roughnessDefault" ), 1.0 );
			rules.insert( QStringLiteral( "metallicDefault" ), 0.0 );
			t.insert( QStringLiteral( "maskRules" ), rules );
		}
		QJsonObject cov;
		cov.insert( QStringLiteral( "present" ), coverTiles > 0 );
		cov.insert( QStringLiteral( "normalisation" ), double( opts.cover.coverFull ) );
		/* ordinal AND linear in composition: the value is not a coverage
		 * fraction -- nothing states what Density counts per unit area -- but
		 * it IS linear in Dtex, which is what makes averaging four cover bytes
		 * the right filter for a coarser tile. A bare "ordinal" would forbid
		 * the filter the pyramid needs. */
		cov.insert( QStringLiteral( "ordinal" ), true );
		cov.insert( QStringLiteral( "linearInComposition" ), true );
		cov.insert( QStringLiteral( "tintStrength" ), double( opts.cover.tintStrength ) );
		t.insert( QStringLiteral( "cover" ), cov );
		t.insert( QStringLiteral( "coarseLevelsAreDownsamples" ), true );
		/* Every level is anchored to ONE origin and each coarser tile covers
		 * exactly four finer ones, so a consumer assembling nested grids can
		 * take whole tiles at any level without resampling. Stated here, and
		 * checked by the harness rather than assumed. */
		t.insert( QStringLiteral( "alignedToWorldOrigin" ), true );
		if ( opts.haveRegion )
			t.insert( QStringLiteral( "partial" ), true );
		QJsonArray ls;
		for ( int l = 0; l < nLevels; l++ ) {
			QJsonObject o;
			o.insert( QStringLiteral( "index" ), l );
			o.insert( QStringLiteral( "dim" ), levels[l].dim );
			o.insert( QStringLiteral( "tilesX" ), levels[l].tilesX );
			o.insert( QStringLiteral( "tilesY" ), levels[l].tilesY );
			// stated, never implied: a consumer picking clipmap rings reads
			// these two numbers at load time
			o.insert( QStringLiteral( "worldUnitsPerTile" ), levels[l].dim * 4096 );
			o.insert( QStringLiteral( "contentTexels" ), content );
			o.insert( QStringLiteral( "unitsPerTexel" ), levels[l].dim * 4096 / content );
			/* The GAME-RELATIVE name of the level container, which moved with
			 * the folder (lane LAYOUT1, 2026-09-16): `Terrain\<ws>.VT.<d>.lodt`
			 * until today, `FO4CSLOD\<ws>\<ws>.VT.<d>.lodt` now. */
			o.insert( QStringLiteral( "container" ),
				lodgenFo4csGameWorldPath( ws ) + QChar( 92 )
					+ QString( "%1.VT.%2.lodt" ).arg( ws ).arg( levels[l].dim ) );
			const int tiles = levels[l].tilesX * levels[l].tilesY;
			o.insert( QStringLiteral( "tiles" ), tiles );
			o.insert( QStringLiteral( "present" ), tiles );
			ls.append( o );
		}
		t.insert( QStringLiteral( "levels" ), ls );
		root.insert( QStringLiteral( "terrain" ), t );
		const QString idx = QString( "%1/%2.VT.lodm" ).arg( dir ).arg( ws );
		if ( !lodmWriteFile( idx, root ) )
			return fail( QString( "could not write %1" ).arg( idx ) );
		lodgenNoteLayoutFile( idx );
	}

	if ( report ) {
		QStringList r;
		r << QStringLiteral( "vt:" );
		r << QString( "levels %1" ).arg( nLevels );
		r << QString( "tiles %1" ).arg( presentTotal );
		r << QString( "present %1" ).arg( presentTotal );
		r << QString( "coverTiles %1" ).arg( coverTiles );
		r << QString( "bytes %1" ).arg( fileBytesTotal );
		r << QString( "cover %1" ).arg( opts.cover.cover ? 1 : 0 );
		r << QString( "coverIn %1" ).arg( opts.coverInColor
			? QStringLiteral( "color" ) : QStringLiteral( "mask" ) );
		r << QString( "sheets %1" ).arg( 3 + ( opts.height ? 1 : 0 ) + ( wantEmissive ? 1 : 0 ) );
		r << QString( "emissive %1" ).arg( wantEmissive
			? QStringLiteral( "present" ) : QStringLiteral( "none" ) );
		/* THE RULE CENSUS, on one physical line of key=value tokens, because
		 * report lines in this tree are parsed by keyword and never by field
		 * position. Every word is WRITTEN and every one of them MOVES with the
		 * corpus (the three rules of 2026-09-04 21:33). */
		r << QString( "maskPbrm %1" ).arg( maskCache.ruleCounts[LODGEN_MASK_PBRM] );
		r << QString( "maskLegacyInverted %1" ).arg( maskCache.ruleCounts[LODGEN_MASK_LEGACY_INVERTED] );
		r << QString( "maskNoneDefault %1" ).arg( maskCache.ruleCounts[LODGEN_MASK_NONE] );
		r << QString( "maskRoughMaps %1" ).arg( maskCache.withRoughnessMap );
		r << QString( "maskMetalMaps %1" ).arg( maskCache.withMetallicMap );
		r << QString( "maskEmissiveMaps %1" ).arg( maskCache.withEmissive );
		r << QString( "maskDistinctLtex %1" ).arg( maskCache.byForm.size() );
		r << QString( "maskLayerRefs %1" ).arg( layerFormsSeen );
		r << QString( "finest %1" ).arg( levels[0].dim );
		r << QString( "coarsest %1" ).arg( levels[nLevels - 1].dim );
		r << QString( "content %1" ).arg( content );
		r << QString( "border %1" ).arg( border );
		r << QString( "mips %1" ).arg( mips );
		r << QString( "compression %1" ).arg( opts.compression );
		/* THE VANILLA-REUSE CENSUS (lane TILING3), written UNCONDITIONALLY so
		 * that a run with the switch off says so with zeros rather than going
		 * silent, and so that a run that DID copy vanilla's sheets cannot be
		 * believed without the counts beside it. `landDetail 0` means the
		 * switch was off; `landDetail 1 msnCopied 0` means it was on and no
		 * vanilla sheet was found under the named root. */
		{
			const LodgenVanillaReuse vr = lodgenVanillaReuseCensus();
			r << QString( "landDetail %1" ).arg( lodgenLandDetailSource() );
			r << QString( "vanillaRoot %1" ).arg( lodgenVanillaLodRoot().isEmpty()
				? QStringLiteral( "(none)" ) : lodgenVanillaLodRoot() );
			r << QString( "msnCopied %1" ).arg( vr.msnCopied );
			r << QString( "msnOurs %1" ).arg( vr.msnOurs );
			r << QString( "colCopied %1" ).arg( vr.colCopied );
			r << QString( "colOurs %1" ).arg( vr.colOurs );
			r << QString( "chunksLayered %1" ).arg( vr.layered );
			r << QString( "chunksLayerless %1" ).arg( vr.layerless );
			r << QString( "chunksLayerlessNoVanilla %1" ).arg( vr.layerlessNoVanilla );
			r << QString( "chunksShaded %1" ).arg( vr.shaded );
			r << QString( "landShade %1" ).arg( double( lodgenLandShade() ), 0, 'f', 3 );
			/* The sheet format and the cleaned-`_msn` cache, same discipline:
			 * written whether or not either is on, so `sheetFormat 0
			 * msnCacheHit 0 msnCacheMiss 0` is a run that changed nothing and
			 * says so. NOTE, because it would otherwise be a trap for a later
			 * reader: this census LINE is emitted only on the `--vt` path,
			 * while the counters increment on BOTH paths, because
			 * lodgenWriteChunkSheets is shared. A stock per-chunk bake moves
			 * these numbers and prints none of them. */
			r << QString( "sheetFormat %1" ).arg( vr.sheetFormat );
			r << QString( "msnCacheDir %1" ).arg( lodgenMsnCacheDir().isEmpty()
				? QStringLiteral( "(none)" ) : lodgenMsnCacheDir() );
			r << QString( "msnCacheHit %1" ).arg( vr.msnCacheHit );
			r << QString( "msnCacheMiss %1" ).arg( vr.msnCacheMiss );
			r << QString( "msnCacheRenorm %1" ).arg( vr.msnCacheRenorm );
		}
		auto hex16 = []( quint64 v ) {
			return QStringLiteral( "0x" )
				+ QString::number( v, 16 ).toUpper().rightJustified( 16, QChar( '0' ) );
		};
		r << QString( "vhgtCorpusHash %1" ).arg( hex16( vhgtHash ) );
		r << QString( "paintCorpusHash %1" ).arg( hex16( paintHash ) );
		r << QString( "vhgtCorpusHashSorted %1" ).arg( hex16( world.vhgtCorpusHashSorted() ) );
		/* THE ROAD CENSUS, same discipline: written whether or not the pass is
		 * on, and every field zero on a region with no roads. `roads 0` means
		 * the switch was off; `roads 1 roadTexels 0` means it was on and the
		 * ground carries none. */
		r << QString( "roads %1" ).arg( opts.cover.roads ? 1 : 0 );
		r << QString( "roadPlacements %1" ).arg( roadCensus.placements );
		r << QString( "roadMeshes %1" ).arg( roadCensus.meshes );
		r << QString( "roadShapeTiles %1" ).arg( roadCensus.shapes );
		r << QString( "roadDecalShapeTiles %1" ).arg( roadCensus.decalShapes );
		r << QString( "roadTriangles %1" ).arg( roadCensus.triangles );
		r << QString( "roadTexels %1" ).arg( roadCensus.texels );
		r << QString( "roadDecalTexels %1" ).arg( roadCensus.decalTexels );
		r << QString( "roadAlphaRejected %1" ).arg( roadCensus.alphaRejected );
		r << QString( "roadRefusedNoLoad %1" ).arg( roadCensus.refusedNoLoad );
		r << QString( "roadRefusedNoTexture %1" ).arg( roadCensus.refusedNoTexture );
		/* WHICH road rule this bake ran under, named in the same line as its
		 * counts, so a sheet can never be read against the wrong rule. */
		r << QString( "roadComposite %1" ).arg( opts.cover.roadComposite
			== LodgenCoverOptions::RoadMaxZ ? QStringLiteral( "max-z" )
			: QStringLiteral( "blend" ) );
		r << QString( "roadDetail %1" ).arg( double( opts.cover.roadDetail ), 0, 'f', 3 );
		r << QString( "roadGroundPaint %1" )
			.arg( double( opts.cover.roadGroundPaint ), 0, 'f', 3 );
		r << QString( "roadGroundShapes %1" ).arg( roadCensus.groundShapes );
		r << QString( "roadGroundTexels %1" ).arg( roadCensus.groundTexels );
		r << QString( "roadOpacity %1" ).arg( double( opts.cover.roadOpacity ), 0, 'f', 3 );
		r << QString( "landGrade %1" ).arg( double( lodgenLandGrade() ), 0, 'f', 4 );
		r << QString( "roadRaisedIncluded %1" ).arg( opts.cover.roadRaised ? 1 : 0 );
		r << QString( "roadRefusedRaised %1" ).arg( roadCensus.refusedRaised );
		r << QString( "roadRaisedBases %1" ).arg( roadCensus.raisedBases );
		r << QString( "roadBlendTexels %1" ).arg( roadCensus.blendTexels );
		r << QString( "roadSidewalksIncluded %1" ).arg( opts.cover.roadSidewalks ? 1 : 0 );
		r << QString( "roadRefusedSidewalk %1" ).arg( roadCensus.refusedSidewalk );
		r << QString( "roadSidewalkBases %1" ).arg( roadCensus.sidewalkBases );
		r << QString( "roadRefusals %1" ).arg( roadCensus.refusals.isEmpty()
			? QStringLiteral( "none" )
			: QString( QStringLiteral( "[%1]" ) )
				.arg( roadCensus.refusals.join( QStringLiteral( "; " ) ) ) );
		/* THE OBJECT-AO CENSUS, same discipline as the road one: written whether
		 * or not the pass is on. `terrainObjectAo 0` means the switch was off;
		 * `terrainObjectAo 1 objAoTexels 0` means it was on and nothing in the
		 * region stood high enough within 1,458 units of a texel to darken it. */
		r << QString( "terrainObjectAo %1" ).arg( opts.cover.terrainObjectAo ? 1 : 0 );
		r << QString( "objAoReach %1" ).arg( 1458 );
		r << QString( "objAoStrength %1" )
			.arg( double( opts.cover.terrainObjectAoStrength ), 0, 'f', 3 );
		r << QString( "objAoPlacements %1" ).arg( objCensus.placements );
		r << QString( "objAoMeshes %1" ).arg( objCensus.meshes );
		r << QString( "objAoTriangles %1" ).arg( objCensus.triangles );
		r << QString( "objAoSlab %1" ).arg( opts.cover.terrainObjectAoSlab ? 1 : 0 );
		r << QString( "objAoSquares %1" ).arg( objCensus.squares );
		/* `objAoSlabSquares` = of those squares, the ones whose LOWEST object
		 * surface stands more than one cell (128 units) above the ESM terrain
		 * under them: the squares the ceiling term can act on. 0 on a region
		 * with no elevated deck, and that zero is WRITTEN, not omitted. */
		r << QString( "objAoSlabSquares %1" ).arg( objCensus.slabSquares );
		r << QString( "objAoTexels %1" ).arg( objCensus.texels );
		r << QString( "objAoMeanDark %1" )
			.arg( objCensus.meanDarkening(), 0, 'f', 4 );
		r << QString( "objAoRefusedNoLod %1" ).arg( objCensus.refusedNoLod );
		r << QString( "objAoNoLodBases %1" ).arg( objCensus.noLodBases );
		r << QString( "objAoRefusedNoLoad %1" ).arg( objCensus.refusedNoLoad );
		r << QString( "objAoRefusals %1" ).arg( objCensus.refusals.isEmpty()
			? QStringLiteral( "none" )
			: QString( QStringLiteral( "[%1]" ) )
				.arg( objCensus.refusals.join( QStringLiteral( "; " ) ) ) );
		/* THE EROSION CENSUS, same discipline again. `erosion 0` means the pass
		 * never ran and the sheets are the bytes from before it existed. */
		const LodgenErosionCensus ec = lodgenErosionCensusTotal();
		r << QString( "erosion %1" ).arg( double( lodgenErosion() ), 0, 'f', 3 );
		r << QString( "erosionIterations %1" ).arg( lodgenErosionIterations() );
		r << QString( "erosionSeed %1" ).arg( lodgenErosionSeed() );
		r << QString( "erosionStep %1" ).arg( ec.step, 0, 'f', 1 );
		r << QString( "erosionCells %1" ).arg( ec.cells );
		r << QString( "erosionMoved %1" ).arg( ec.moved );
		r << QString( "erosionMeanAbs %1" ).arg( ec.meanAbs, 0, 'f', 4 );
		r << QString( "erosionMaxCut %1" ).arg( ec.maxCut, 0, 'f', 3 );
		r << QString( "erosionMaxFill %1" ).arg( ec.maxFill, 0, 'f', 3 );
		*report = r.join( QChar( ' ' ) );
	}
	if ( error )
		error->clear();
	return true;
}


bool lodgenBuildAtlas( const QStringList & btoPaths, const QString & dataRoot,
	const QString & atlasFileBase, const QString & atlasGameBase,
	const QString & looseRoot, bool bc1, QString * error )
{
	auto fail = [error]( const QString & message ) {
		if ( error )
			*error = message;
		return false;
	};
	constexpr int AW = 4096, AH = 2048, CELL = 256;
	constexpr int COLS = AW / CELL, ROWS = AH / CELL;   // 16 x 8 = 128 cells
	constexpr float INSET = 2.0f;                       // texels, against mip bleed

	const QString atlasDds = atlasGameBase + QStringLiteral( ".DDS" );
	const QString atlasNrm = atlasGameBase + QStringLiteral( "_n.DDS" );
	const QString atlasSpec = atlasGameBase + QStringLiteral( "_s.DDS" );

	/* Pass 1: find every atlasable shape and give each unique diffuse a
	 * cell. Atlasable = UVs inside [0,1] (tiling breaks under an atlas).
	 * The cell carries the source's _s too (vanilla's atlas has
	 * Commonwealth.Objects_s.DDS beside the diffuse and normal), with the
	 * shape's specular strength and smoothness folded into it so every
	 * atlased shape can carry constants of 1 and merge with its neighbours. */
	QHash<QString, int> cellOf;         // lowercased tex0 -> cell index
	struct CellSrc { QString tex0, tex1, tex7; float smooth = 1.0f, mult = 1.0f; };
	QHash<QString, CellSrc> cellTex;    // key -> the cell's sources
	QSet<QString> directTex;            // kept-direct references, to copy loose
	int overflow = 0, tilingShapes = 0;

	auto shapeInfo = [&]( NifModel & nif, const QModelIndex & iShape,
		QString & tex0, QString & tex1, QString & tex7, float & smooth, float & mult, bool & inRange ) -> bool {
		QModelIndex iShader = nif.getBlockIndex( nif.getLink( iShape, "Shader Property" ) );
		if ( !iShader.isValid() )
			return false;
		QModelIndex iTexSet = nif.getBlockIndex( nif.getLink( iShader, "Texture Set" ) );
		if ( !iTexSet.isValid() )
			return false;
		QModelIndex iArr = nif.getIndex( iTexSet, "Textures" );
		if ( !iArr.isValid() )
			return false;
		tex0 = nif.get<QString>( nif.getIndex( iArr, 0 ) );
		tex1 = nif.get<QString>( nif.getIndex( iArr, 1 ) );
		tex7 = nif.get<int>( iTexSet, "Num Textures" ) > 7 ? nif.get<QString>( nif.getIndex( iArr, 7 ) ) : QString();
		smooth = nif.get<float>( iShader, "Smoothness" );
		mult = nif.get<float>( iShader, "Specular Strength" );
		if ( tex0.isEmpty() || tex0.compare( atlasDds, Qt::CaseInsensitive ) == 0 )
			return false;
		inRange = true;
		QModelIndex iVerts = nif.getIndex( iShape, "Vertex Data" );
		const int numVerts = int( nif.get<quint32>( iShape, "Num Vertices" ) );
		if ( !iVerts.isValid() || numVerts <= 0 )
			return false;
		for ( int v = 0; v < numVerts && inRange; v++ ) {
			const Vector2 uv = nif.get<HalfVector2>(
				nif.index( v, 0, iVerts ), "UV" );
			if ( uv[0] < -0.002f || uv[0] > 1.002f
				|| uv[1] < -0.002f || uv[1] > 1.002f )
				inRange = false;
		}
		return true;
	};

	for ( const QString & path : btoPaths ) {
		NifModel nif;
		if ( !nif.loadFromFile( path ) )
			continue;
		for ( int b = 0; b < nif.getBlockCount(); b++ ) {
			QModelIndex iShape = nif.getBlockIndex( b );
			if ( !nif.isNiBlock( iShape, "BSSubIndexTriShape" )
				&& !nif.isNiBlock( iShape, "BSTriShape" ) )
				continue;
			QString tex0, tex1, tex7;
			float smooth = 1.0f, mult = 1.0f;
			bool inRange = false;
			if ( !shapeInfo( nif, iShape, tex0, tex1, tex7, smooth, mult, inRange ) )
				continue;
			if ( !inRange ) {
				tilingShapes++;
				directTex.insert( tex0 );
				if ( !tex1.isEmpty() )
					directTex.insert( tex1 );
				if ( !tex7.isEmpty() )
					directTex.insert( tex7 );
				continue;
			}
			const QString key = tex0.toLower();
			if ( cellOf.contains( key ) )
				continue;
			if ( cellOf.size() >= COLS * ROWS ) {
				overflow++;
				directTex.insert( tex0 );
				if ( !tex1.isEmpty() )
					directTex.insert( tex1 );
				if ( !tex7.isEmpty() )
					directTex.insert( tex7 );
				continue;
			}
			cellTex.insert( key, CellSrc{ tex0, tex1, tex7, smooth, mult } );
			cellOf.insert( key, cellOf.size() );
		}
	}
	if ( cellOf.isEmpty() )
		return fail( QStringLiteral( "no atlasable shapes in the input files" ) );

	/* Pass 2: compose the sheets. Each cell is the source texture sampled
	 * at the mip whose texel footprint matches a 256-wide cell. */
	std::vector<quint32> sheet( size_t( AW ) * AH, 0x00000000U );
	std::vector<quint32> nrmSheet( size_t( AW ) * AH, 0xFF8080FFU );
	// the _s sheet: R specular, G gloss, as a shape without a map reads them (1, 1)
	std::vector<quint8> specR( size_t( AW ) * AH, 255 ), specG( size_t( AW ) * AH, 255 );
	QHash<QString, DDSTexture16 *> texCache;
	for ( auto it = cellOf.constBegin(); it != cellOf.constEnd(); ++it ) {
		const int cx = ( it.value() % COLS ) * CELL;
		const int cy = ( it.value() / COLS ) * CELL;
		const CellSrc & texes = cellTex[it.key()];
		const DDSTexture16 * d = lodgenLoadTexture( dataRoot, texes.tex0, texCache );
		const DDSTexture16 * nm = texes.tex1.isEmpty() ? nullptr
			: lodgenLoadTexture( dataRoot, texes.tex1, texCache );
		const DDSTexture16 * sp = texes.tex7.isEmpty() ? nullptr
			: lodgenLoadTexture( dataRoot, texes.tex7, texCache );
		const float smooth = qBound( 0.0f, texes.smooth, 1.0f );
		const float mult = texes.mult > 0.0f ? texes.mult : 1.0f;
		for ( int y = 0; y < CELL; y++ ) {
			for ( int x = 0; x < CELL; x++ ) {
				const float u = ( float( x ) + 0.5f ) / CELL;
				const float v = ( float( y ) + 0.5f ) / CELL;
				quint32 dp = 0xFF808080U, np = 0xFF8080FFU;
				{
					// the constants folded in: the sheet is what the engine composes
					float sR = 1.0f, sG = 1.0f;
					if ( sp ) {
						const float mip = qBound( 0.0f,
							std::log2( qMax( 1.0f, float( sp->getWidth() ) / CELL ) ),
							float( sp->getMaxMipLevel() ) );
						const FloatVector4 c = sp->getPixelT( u, v, mip );
						sR = c[0];
						sG = c[1];
					}
					specR[size_t( cy + y ) * AW + cx + x] = quint8( qBound( 0, int( qMin( 1.0f, sR * mult ) * 255.0f + 0.5f ), 255 ) );
					specG[size_t( cy + y ) * AW + cx + x] = quint8( qBound( 0, int( sG * smooth * 255.0f + 0.5f ), 255 ) );
				}
				if ( d ) {
					const float mip = qBound( 0.0f,
						std::log2( qMax( 1.0f, float( d->getWidth() ) / CELL ) ),
						float( d->getMaxMipLevel() ) );
					const FloatVector4 c = d->getPixelT( u, v, mip );
					dp = ( quint32( qBound( 0, int( c[3] * 255.0f + 0.5f ), 255 ) ) << 24 )
						| ( quint32( qBound( 0, int( c[0] * 255.0f + 0.5f ), 255 ) ) << 16 )
						| ( quint32( qBound( 0, int( c[1] * 255.0f + 0.5f ), 255 ) ) << 8 )
						| quint32( qBound( 0, int( c[2] * 255.0f + 0.5f ), 255 ) );
				}
				if ( nm ) {
					const float mip = qBound( 0.0f,
						std::log2( qMax( 1.0f, float( nm->getWidth() ) / CELL ) ),
						float( nm->getMaxMipLevel() ) );
					const FloatVector4 c = nm->getPixelT( u, v, mip );
					// alpha carries the source's smoothness/specular channel
					np = ( quint32( qBound( 0, int( c[3] * 255.0f + 0.5f ), 255 ) ) << 24 )
						| ( quint32( qBound( 0, int( c[0] * 255.0f + 0.5f ), 255 ) ) << 16 )
						| ( quint32( qBound( 0, int( c[1] * 255.0f + 0.5f ), 255 ) ) << 8 )
						| quint32( qBound( 0, int( c[2] * 255.0f + 0.5f ), 255 ) );
				}
				sheet[size_t( cy + y ) * AW + cx + x] = dp;
				nrmSheet[size_t( cy + y ) * AW + cx + x] = np;
			}
		}
	}
	for ( DDSTexture16 * t : texCache )
		delete t;

	/* Colour dilation: filtering and mips blend RGB across the alpha edge,
	 * so transparent texels must hold plausible colour, not black — the
	 * black-fringed mud on tree cards came exactly from here (found by
	 * bungo's vanilla-vs-atlas colour comparison; vanilla's sheet keeps
	 * filled RGB under its transparency). Dilate opaque RGB outward, then
	 * flood what remains with the sheet's average opaque colour. */
	{
		quint64 accR = 0, accG = 0, accB = 0, accN = 0;
		for ( quint32 p : sheet ) {
			if ( ( p >> 24 ) >= 128 ) {
				accR += ( p >> 16 ) & 0xFF;
				accG += ( p >> 8 ) & 0xFF;
				accB += p & 0xFF;
				accN++;
			}
		}
		const quint32 avg = accN ? ( ( quint32( accR / accN ) << 16 )
			| ( quint32( accG / accN ) << 8 ) | quint32( accB / accN ) ) : 0x808080U;
		std::vector<quint8> filled( sheet.size() );
		for ( size_t i = 0; i < sheet.size(); i++ )
			filled[i] = ( sheet[i] >> 24 ) >= 128;
		for ( int pass = 0; pass < 20; pass++ ) {
			bool changed = false;
			std::vector<quint8> nextFilled = filled;
			for ( int y = 0; y < AH; y++ ) {
				for ( int x = 0; x < AW; x++ ) {
					const size_t i = size_t( y ) * AW + x;
					if ( filled[i] )
						continue;
					quint32 r = 0, gc = 0, b = 0, n = 0;
					for ( int dy = -1; dy <= 1; dy++ ) {
						for ( int dx = -1; dx <= 1; dx++ ) {
							const int sx = x + dx, sy = y + dy;
							if ( sx < 0 || sy < 0 || sx >= AW || sy >= AH )
								continue;
							const size_t j = size_t( sy ) * AW + sx;
							if ( !filled[j] )
								continue;
							const quint32 p = sheet[j];
							r += ( p >> 16 ) & 0xFF;
							gc += ( p >> 8 ) & 0xFF;
							b += p & 0xFF;
							n++;
						}
					}
					if ( n ) {
						sheet[i] = ( sheet[i] & 0xFF000000U )
							| ( ( r / n ) << 16 ) | ( ( gc / n ) << 8 ) | ( b / n );
						nextFilled[i] = 1;
						changed = true;
					}
				}
			}
			filled.swap( nextFilled );
			if ( !changed )
				break;
		}
		for ( size_t i = 0; i < sheet.size(); i++ )
			if ( !filled[i] )
				sheet[i] = ( sheet[i] & 0xFF000000U ) | avg;
	}

	/* Pass 3: repoint the shapes and remap their UVs into the cells. */
	int movedShapes = 0;
	for ( const QString & path : btoPaths ) {
		NifModel nif;
		if ( !nif.loadFromFile( path ) )
			continue;
		bool changed = false;
		for ( int b = 0; b < nif.getBlockCount(); b++ ) {
			QModelIndex iShape = nif.getBlockIndex( b );
			if ( !nif.isNiBlock( iShape, "BSSubIndexTriShape" )
				&& !nif.isNiBlock( iShape, "BSTriShape" ) )
				continue;
			QString tex0, tex1, tex7;
			float smooth = 1.0f, mult = 1.0f;
			bool inRange = false;
			if ( !shapeInfo( nif, iShape, tex0, tex1, tex7, smooth, mult, inRange ) || !inRange )
				continue;
			auto cellIt = cellOf.constFind( tex0.toLower() );
			if ( cellIt == cellOf.constEnd() )
				continue;
			const float cx = float( ( cellIt.value() % COLS ) * CELL );
			const float cy = float( ( cellIt.value() / COLS ) * CELL );
			nif.setState( BaseModel::Processing );
			QModelIndex iVerts = nif.getIndex( iShape, "Vertex Data" );
			const int numVerts = int( nif.get<quint32>( iShape, "Num Vertices" ) );
			for ( int v = 0; v < numVerts; v++ ) {
				QModelIndex row = nif.index( v, 0, iVerts );
				const Vector2 uv = nif.get<HalfVector2>( row, "UV" );
				const float su = qBound( 0.0f, uv[0], 1.0f );
				const float sv = qBound( 0.0f, uv[1], 1.0f );
				nif.set<HalfVector2>( row, "UV", HalfVector2( Vector2(
					( cx + INSET + su * ( CELL - 2.0f * INSET ) ) / AW,
					( cy + INSET + sv * ( CELL - 2.0f * INSET ) ) / AH ) ) );
			}
			nif.restoreState();
			QModelIndex iShader = nif.getBlockIndex( nif.getLink( iShape, "Shader Property" ) );
			QModelIndex iTexSet = nif.getBlockIndex( nif.getLink( iShader, "Texture Set" ) );
			QModelIndex iArr = nif.getIndex( iTexSet, "Textures" );
			nif.set<QString>( nif.getIndex( iArr, 0 ), atlasDds );
			nif.set<QString>( nif.getIndex( iArr, 1 ), atlasNrm );
			// the _s sheet carries the constants now; the shape reads it at 1/1, as vanilla's do
			if ( nif.get<int>( iTexSet, "Num Textures" ) > 7 )
				nif.set<QString>( nif.getIndex( iArr, 7 ), atlasSpec );
			nif.set<float>( iShader, "Smoothness", 1.0f );
			nif.set<float>( iShader, "Specular Strength", 1.0f );
			changed = true;
			movedShapes++;
		}
		if ( changed && !nif.saveToFile( path ) )
			return fail( QString( "could not rewrite %1" ).arg( path ) );
	}

	/* Vanilla's diffuse sheet is DXT1, not BC3 -- measured on the shipped
	 * `Commonwealth.Objects.DDS`: 4096x2048, 13 mips, fourCC DXT1,
	 * 5,592,552 bytes. (Its `_n` and `_s` are both BC5U.) The comment here
	 * claimed BC3 for a month and the claim was the argument for ours being
	 * BC3 too, which is twice the memory per sheet.
	 *
	 * bc1 writes DXT1 with BC1's one-bit punch-through for the cut-outs and
	 * carries that alpha down the mip chain; BC3 keeps eight-bit alpha, which
	 * only a consumer that soft-blends card edges can spend. So the stock
	 * target takes BC1 (vanilla parity, half the memory) and FO4CS keeps BC3.
	 * RGB survives under transparent texels either way -- the dilation above
	 * is what puts it there. */
	if ( !lodgenWriteDds( atlasFileBase + QStringLiteral( ".DDS" ), AW, AH, sheet, !bc1, 0, bc1 ) )
		return fail( QStringLiteral( "could not write the atlas sheet" ) );
	if ( !lodgenWriteDds( atlasFileBase + QStringLiteral( "_n.DDS" ), AW, AH, nrmSheet, true ) )
		return fail( QStringLiteral( "could not write the atlas normal sheet" ) );
	// BC5 like vanilla's Commonwealth.Objects_s.DDS: two independent ramps, R specular and G gloss
	if ( !lodgenWriteDdsBC5( atlasFileBase + QStringLiteral( "_s.DDS" ), AW, AH, specR, specG ) )
		return fail( QStringLiteral( "could not write the atlas specular sheet" ) );

	/* Textures still referenced directly (tiling shapes, cell overflow) are
	 * CK-only files a stock game does not have — copy them loose into the
	 * output Data tree so the result is self-contained. */
	int copied = 0, uncopyable = 0;
	if ( !looseRoot.isEmpty() ) {
		for ( const QString & t : directTex ) {
			QString rel = t;
			rel.replace( QChar( '\\' ), QChar( '/' ) );
			if ( !rel.startsWith( QStringLiteral( "textures/" ), Qt::CaseInsensitive ) )
				rel.prepend( QStringLiteral( "textures/" ) );
			const QString dst = looseRoot + "/" + rel;
			if ( QFile::exists( dst ) ) {
				copied++;
				continue;
			}
			QByteArray bytes;
			if ( !lodgenReadAsset( dataRoot, rel, "textures", ".dds", bytes ) ) {
				uncopyable++;
				continue;
			}
			QDir().mkpath( QFileInfo( dst ).absolutePath() );
			QFile out( dst );
			if ( out.open( QIODevice::WriteOnly ) && out.write( bytes ) == bytes.size() )
				copied++;
			else
				uncopyable++;
		}
	}
	fprintf( stderr, "atlas: %d textures in cells, %d shapes moved, "
		"%d tiling shapes kept direct (%d source textures copied loose, "
		"%d unavailable), %d textures past capacity\n",
		cellOf.size(), movedShapes, tilingShapes, copied, uncopyable, overflow );
	if ( error )
		error->clear();
	return true;
}

/*! Merge a chunk's shapes down to one per material, after the atlas and the
 *  arrays have had their say.
 *
 *  A chunk is built one shape per source material (texture set, alpha,
 *  constants, material name), because the arrays pass keys layers on the
 *  source and the atlas keys cells on the diffuse. Once the atlas has
 *  repointed every atlasable shape at the same three sheets and folded the
 *  constants into the `_s` sheet, most of those shapes are the same material
 *  again — and the stock engine draws each as its own call. Vanilla's
 *  Commonwealth chunks hold three shapes (opaque, alpha-tested, and the
 *  toggle-ref one); Sanctuary (-20,24) built ten before this pass.
 *
 *  Shapes merge when everything the engine reads agrees: name (obj/obj-at),
 *  the ten texture slots, the alpha property, the shader's flags, type and
 *  constants, the vertex descriptor, and — for FO4CS — the array set their
 *  `A` line names, so one merged shape never spans two texture arrays (the
 *  layer stays per vertex in UV2.y; the `A` line then says `-1`). Vertices
 *  and triangles concatenate, triangles regrouped per segment (the cell
 *  segments must match, and do: dim x dim each); the bound and the multi
 *  bound take the union; the merged-away branches go and the root's child
 *  list is rebuilt without holes. A merge never crosses 65535 vertices.
 *  Manifest `A` and `M` lines are rewritten for the surviving blocks. */
bool lodgenMergeChunkShapes( const QStringList & btoPaths, QString * report, QString * error )
{
	auto fail = [error]( const QString & message ) {
		if ( error )
			*error = message;
		return false;
	};
	struct Vtx { Vector3 pos, nrm, tan; Vector2 uv, uv2; float bx = 0, by = 0, bz = 0, eye = 0; Color4 col; };
	struct Rec {
		QPersistentModelIndex iShape, iNode;
		QString key, lodm;
		int layer = -2;                       // -2: no A line
		QStringList materials;
		int numVerts = 0, numTris = 0;
	};
	int chunks = 0, before = 0, after = 0;
	for ( const QString & path : btoPaths ) {
		NifModel nif;
		if ( !nif.loadFromFile( path ) )
			continue;
		// the manifest: A and M lines by block, everything else kept as it is
		const QString manPath = path + QStringLiteral( ".manifest.txt" );
		QStringList keep;
		QHash<int, QPair<int, QString>> aOf;
		QMultiHash<int, QString> mOf;
		bool haveManifest = false;
		{
			QFile mf( manPath );
			if ( mf.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
				haveManifest = true;
				while ( !mf.atEnd() ) {
					const QString line = QString::fromUtf8( mf.readLine() ).trimmed();
					if ( line.isEmpty() )
						continue;
					const QStringList t = line.split( QChar( ' ' ), Qt::SkipEmptyParts );
					if ( t.size() >= 4 && t[0] == QLatin1String( "A" ) )
						aOf.insert( t[1].toInt(), qMakePair( t[2].toInt(), t.mid( 3 ).join( QChar( ' ' ) ) ) );
					else if ( t.size() >= 3 && t[0] == QLatin1String( "M" ) )
						mOf.insert( t[1].toInt(), t.mid( 2 ).join( QChar( ' ' ) ) );
					else
						keep.append( line );
				}
			}
		}
		// every object shape under a multi-bound node, keyed by what the engine reads
		QVector<Rec> recs;
		QStringList order;
		QHash<QString, QVector<int>> groups;
		for ( int b = 0; b < nif.getBlockCount(); b++ ) {
			const QModelIndex iShape = nif.getBlockIndex( b );
			if ( !nif.isNiBlock( iShape, "BSSubIndexTriShape" ) )
				continue;
			const QModelIndex iShader = nif.getBlockIndex( nif.getLink( iShape, "Shader Property" ) );
			if ( !iShader.isValid() || !nif.isNiBlock( iShader, "BSLightingShaderProperty" ) )
				continue;
			const int parent = nif.getParent( b );
			const QModelIndex iNode = nif.getBlockIndex( parent );
			if ( !iNode.isValid() || !nif.isNiBlock( iNode, "BSMultiBoundNode" ) )
				continue;
			QString key = nif.get<QString>( iShape, "Name" ) + QChar( '|' );
			const QModelIndex iTexSet = nif.getBlockIndex( nif.getLink( iShader, "Texture Set" ) );
			if ( iTexSet.isValid() ) {
				const QModelIndex iArr = nif.getIndex( iTexSet, "Textures" );
				const int n = nif.get<int>( iTexSet, "Num Textures" );
				for ( int t = 0; t < n; t++ )
					key += nif.get<QString>( nif.getIndex( iArr, t ) ).toLower() + QChar( '|' );
			}
			const QModelIndex iAlpha = nif.getBlockIndex( nif.getLink( iShape, "Alpha Property" ) );
			key += iAlpha.isValid()
				? QString( "a%1/%2|" ).arg( nif.get<int>( iAlpha, "Flags" ) ).arg( nif.get<int>( iAlpha, "Threshold" ) )
				: QStringLiteral( "-|" );
			key += QString( "%1/%2/%3/%4/%5|" ).arg( nif.get<quint32>( iShader, "Shader Type" ) )
				.arg( nif.get<quint32>( iShader, "Shader Flags 1" ) ).arg( nif.get<quint32>( iShader, "Shader Flags 2" ) )
				.arg( double( nif.get<float>( iShader, "Smoothness" ) ) ).arg( double( nif.get<float>( iShader, "Specular Strength" ) ) );
			// the emissive constants are constants too: two shapes that emit
			// different colours are not one shape to the engine
			{
				const Color3 ec = nif.get<Color3>( iShader, "Emissive Color" );
				key += QString( "%1/%2/%3/%4|" ).arg( double( ec.red() ) ).arg( double( ec.green() ) )
					.arg( double( ec.blue() ) ).arg( double( nif.get<float>( iShader, "Emissive Multiple" ) ) );
			}
			key += QString::number( qulonglong( nif.get<BSVertexDesc>( iShape, "Vertex Desc" ).Value() ) ) + QChar( '|' );
			Rec r;
			r.iShape = iShape;
			r.iNode = iNode;
			r.numVerts = int( nif.get<quint32>( iShape, "Num Vertices" ) );
			r.numTris = int( nif.get<quint32>( iShape, "Num Triangles" ) );
			if ( aOf.contains( b ) ) {
				r.layer = aOf.value( b ).first;
				r.lodm = aOf.value( b ).second;
			}
			r.materials = mOf.values( b );
			key += r.lodm.toLower();
			r.key = key;
			if ( !groups.contains( key ) )
				order.append( key );
			groups[key].append( recs.size() );
			recs.append( r );
		}
		if ( recs.isEmpty() )
			continue;
		chunks++;
		before += recs.size();

		/* Full precision is asked of the SHAPE, never of the chunk: one
		 * object chunk carries one descriptor today, but the group key
		 * carries the descriptor exactly because that may stop being true,
		 * and reading a half as a float is silent. */
		auto isFullPrec = [&]( const QModelIndex & iShape ) {
			return ( ( nif.get<BSVertexDesc>( iShape, "Vertex Desc" ).Value() >> 44 ) & 0x400 ) != 0;
		};
		auto readShape = [&]( const QModelIndex & iShape, QVector<Vtx> & verts, QVector<QVector<Triangle>> & segTris,
			bool & hasCol, bool & hasUv2, bool & hasEye ) {
			const bool fullPrec = isFullPrec( iShape );
			const QModelIndex iVD = nif.getIndex( iShape, "Vertex Data" );
			const int nv = int( nif.get<quint32>( iShape, "Num Vertices" ) );
			verts.resize( nv );
			const QModelIndex row0 = nif.index( 0, 0, iVD );
			hasCol = nif.getIndex( row0, "Vertex Colors" ).isValid();
			hasUv2 = nif.getIndex( row0, "UV 2" ).isValid();
			hasEye = nif.getIndex( row0, "Eye Data" ).isValid();
			for ( int v = 0; v < nv; v++ ) {
				const QModelIndex row = nif.index( v, 0, iVD );
				Vtx & o = verts[v];
				o.pos = fullPrec ? nif.get<Vector3>( row, "Vertex" ) : Vector3( nif.get<HalfVector3>( row, "Vertex" ) );
				o.uv = Vector2( nif.get<HalfVector2>( row, "UV" ) );
				o.nrm = Vector3( nif.get<ByteVector3>( row, "Normal" ) );
				o.tan = Vector3( nif.get<ByteVector3>( row, "Tangent" ) );
				o.bx = nif.get<float>( row, "Bitangent X" );
				o.by = nif.get<float>( row, "Bitangent Y" );
				o.bz = nif.get<float>( row, "Bitangent Z" );
				if ( hasCol )
					o.col = Color4( nif.get<ByteColor4>( row, "Vertex Colors" ) );
				if ( hasUv2 )
					o.uv2 = Vector2( nif.get<HalfVector2>( row, "UV 2" ) );
				if ( hasEye )
					o.eye = nif.get<float>( row, "Eye Data" );
			}
			const QVector<Triangle> tris = nif.getArray<Triangle>( nif.getIndex( iShape, "Triangles" ) );
			const QModelIndex iSegs = nif.getIndex( iShape, "Segment" );
			const int ns = iSegs.isValid() ? nif.rowCount( iSegs ) : 0;
			segTris.clear();
			segTris.resize( qMax( 1, ns ) );
			if ( ns == 0 ) {
				segTris[0] = tris;
			} else {
				for ( int s = 0; s < ns; s++ ) {
					const QModelIndex seg = nif.index( s, 0, iSegs );
					const int start = int( nif.get<quint32>( seg, "Start Index" ) ) / 3;
					const int count = int( nif.get<quint32>( seg, "Num Primitives" ) );
					for ( int t = start; t < start + count && t < tris.size(); t++ )
						segTris[s].append( tris[t] );
				}
			}
		};
		auto aabbOf = [&]( const QModelIndex & iNode, Vector3 & lo, Vector3 & hi ) {
			const QModelIndex iMB = nif.getBlockIndex( nif.getLink( iNode, "Multi Bound" ) );
			const QModelIndex iBox = iMB.isValid() ? nif.getBlockIndex( nif.getLink( iMB, "Data" ) ) : QModelIndex();
			if ( !iBox.isValid() )
				return false;
			const Vector3 p = nif.get<Vector3>( iBox, "Position" ), e = nif.get<Vector3>( iBox, "Extent" );
			lo = p - e;
			hi = p + e;
			return true;
		};
		auto setAabb = [&]( const QModelIndex & iNode, const Vector3 & lo, const Vector3 & hi ) {
			const QModelIndex iMB = nif.getBlockIndex( nif.getLink( iNode, "Multi Bound" ) );
			const QModelIndex iBox = iMB.isValid() ? nif.getBlockIndex( nif.getLink( iMB, "Data" ) ) : QModelIndex();
			if ( !iBox.isValid() )
				return;
			nif.set<Vector3>( iBox, "Position", ( lo + hi ) * 0.5f );
			nif.set<Vector3>( iBox, "Extent", ( hi - lo ) * 0.5f );
		};

		QVector<QPersistentModelIndex> doomed;
		QSet<int> gone;
		for ( const QString & key : order ) {
			const QVector<int> & members = groups[key];
			if ( members.size() < 2 )
				continue;
			// greedy runs under the 16-bit vertex cap
			int i = 0;
			while ( i < members.size() ) {
				QVector<int> run;
				int total = 0;
				for ( ; i < members.size(); i++ ) {
					if ( !run.isEmpty() && total + recs[members[i]].numVerts > 65535 )
						break;
					run.append( members[i] );
					total += recs[members[i]].numVerts;
				}
				if ( run.size() < 2 )
					continue;
				Rec & target = recs[run[0]];
				QVector<Vtx> verts;
				QVector<QVector<Triangle>> segTris;
				bool hasCol = false, hasUv2 = false, hasEye = false;
				readShape( target.iShape, verts, segTris, hasCol, hasUv2, hasEye );
				Vector3 lo, hi;
				const bool haveBox = aabbOf( target.iNode, lo, hi );
				QSet<int> layers;
				if ( target.layer >= 0 )
					layers.insert( target.layer );
				for ( int k = 1; k < run.size(); k++ ) {
					Rec & m = recs[run[k]];
					QVector<Vtx> mv;
					QVector<QVector<Triangle>> ms;
					bool c2, u2, e2;
					readShape( m.iShape, mv, ms, c2, u2, e2 );
					if ( ms.size() != segTris.size() ) {
						fprintf( stderr, "lodgen: merge: %s: segment counts differ (%d vs %d), shape kept apart\n",
							QFileInfo( path ).fileName().toLocal8Bit().constData(), int( ms.size() ), int( segTris.size() ) );
						continue;
					}
					const quint16 base = quint16( verts.size() );
					verts += mv;
					for ( int s = 0; s < ms.size(); s++ )
						for ( const Triangle & t : ms[s] )
							segTris[s].append( Triangle( quint16( base + t.v1() ), quint16( base + t.v2() ), quint16( base + t.v3() ) ) );
					Vector3 mlo, mhi;
					if ( haveBox && aabbOf( m.iNode, mlo, mhi ) ) {
						for ( int c = 0; c < 3; c++ ) {
							lo[c] = qMin( lo[c], mlo[c] );
							hi[c] = qMax( hi[c], mhi[c] );
						}
					}
					if ( m.layer >= 0 )
						layers.insert( m.layer );
					for ( const QString & mat : m.materials )
						if ( !target.materials.contains( mat ) )
							target.materials.append( mat );
					doomed.append( m.iNode );
					gone.insert( run[k] );
				}
				// write the merged shape back
				QVector<Triangle> tris;
				QVector<QPair<int, int>> segRuns;
				for ( const QVector<Triangle> & st : segTris ) {
					segRuns.append( qMakePair( tris.size(), st.size() ) );
					tris += st;
				}
				const QModelIndex iShape = target.iShape;
				const BSVertexDesc desc = nif.get<BSVertexDesc>( iShape, "Vertex Desc" );
				const bool fullPrec = isFullPrec( iShape );
				const quint32 numVerts = quint32( verts.size() ), numTris = quint32( tris.size() );
				nif.set<quint32>( iShape, "Num Vertices", numVerts );
				nif.set<quint32>( iShape, "Num Triangles", numTris );
				nif.set<quint32>( iShape, "Data Size", numVerts * quint32( desc.GetVertexSize() ) + numTris * 6 );
				nif.setState( BaseModel::Processing );
				const QModelIndex iVD = nif.getIndex( iShape, "Vertex Data" );
				nif.updateArraySize( iVD );
				float mnx = 3.4e38f, mny = 3.4e38f, mnz = 3.4e38f, mxx = -3.4e38f, mxy = -3.4e38f, mxz = -3.4e38f;
				for ( int v = 0; v < verts.size(); v++ ) {
					const QModelIndex row = nif.index( v, 0, iVD );
					const Vtx & o = verts[v];
					mnx = qMin( mnx, o.pos[0] ); mny = qMin( mny, o.pos[1] ); mnz = qMin( mnz, o.pos[2] );
					mxx = qMax( mxx, o.pos[0] ); mxy = qMax( mxy, o.pos[1] ); mxz = qMax( mxz, o.pos[2] );
					if ( fullPrec )
						nif.set<Vector3>( row, "Vertex", o.pos );
					else
						nif.set<HalfVector3>( row, "Vertex", HalfVector3( o.pos ) );
					nif.set<HalfVector2>( row, "UV", HalfVector2( o.uv ) );
					nif.set<ByteVector3>( row, "Normal", ByteVector3( o.nrm ) );
					nif.set<ByteVector3>( row, "Tangent", ByteVector3( o.tan ) );
					nif.set<float>( row, "Bitangent X", o.bx );
					nif.set<float>( row, "Bitangent Y", o.by );
					nif.set<float>( row, "Bitangent Z", o.bz );
					if ( hasCol )
						nif.set<ByteColor4>( row, "Vertex Colors", ByteColor4( FloatVector4( o.col.red(), o.col.green(), o.col.blue(), o.col.alpha() ) ) );
					if ( hasUv2 )
						nif.set<HalfVector2>( row, "UV 2", HalfVector2( o.uv2 ) );
					if ( hasEye )
						nif.set<float>( row, "Eye Data", o.eye );
				}
				const QModelIndex iTris = nif.getIndex( iShape, "Triangles" );
				nif.updateArraySize( iTris );
				nif.setArray<Triangle>( iTris, tris );
				nif.set<quint32>( iShape, "Num Primitives", numTris );
				const QModelIndex iSegs = nif.getIndex( iShape, "Segment" );
				if ( iSegs.isValid() ) {
					nif.set<quint32>( iShape, "Num Segments", quint32( segRuns.size() ) );
					nif.set<quint32>( iShape, "Total Segments", quint32( segRuns.size() ) );
					nif.updateArraySize( iSegs );
					for ( int s = 0; s < segRuns.size(); s++ ) {
						const QModelIndex seg = nif.index( s, 0, iSegs );
						nif.set<quint32>( seg, "Start Index", quint32( segRuns[s].first * 3 ) );
						nif.set<quint32>( seg, "Num Primitives", quint32( segRuns[s].second ) );
						nif.set<quint32>( seg, "Parent Array Index", 0xFFFFFFFFU );
					}
				}
				setBound( &nif, iShape, mnx, mny, mnz, mxx, mxy, mxz );
				nif.restoreState();
				if ( haveBox )
					setAabb( target.iNode, lo, hi );
				target.numVerts = int( numVerts );
				target.numTris = int( numTris );
				target.layer = ( layers.size() == 1 ) ? *layers.constBegin() : ( layers.isEmpty() ? target.layer : -1 );
			}
		}
		// the merged-away branches, then the root's child list without holes
		std::function<void( const QPersistentModelIndex & )> removeBranch = [&]( const QPersistentModelIndex & iBlock ) {
			QVector<QPersistentModelIndex> kids;
			const int bn = nif.getBlockNumber( iBlock );
			for ( int link : nif.getChildLinks( bn ) )
				if ( nif.isValidBlockNumber( link ) && nif.getParent( link ) == bn )
					kids.append( QPersistentModelIndex( nif.getBlockIndex( link ) ) );
			for ( const QPersistentModelIndex & k : kids )
				if ( k.isValid() )
					removeBranch( k );
			if ( iBlock.isValid() )
				nif.removeNiBlock( nif.getBlockNumber( iBlock ) );
		};
		for ( const QPersistentModelIndex & d : doomed )
			if ( d.isValid() )
				removeBranch( d );
		if ( !doomed.isEmpty() ) {
			const QModelIndex iRoot = nif.getBlockIndex( 0 );
			const QModelIndex iKids = nif.getIndex( iRoot, "Children" );
			if ( iKids.isValid() ) {
				QVector<qint32> links;
				for ( qint32 l : nif.getLinkArray( iKids ) )
					if ( l >= 0 )
						links.append( l );
				nif.set<uint>( iRoot, "Num Children", uint( links.size() ) );
				nif.updateArraySize( iKids );
				for ( int i = 0; i < links.size(); i++ )
					nif.setLink( nif.getIndex( iKids, i ), links[i] );
			}
			if ( !nif.saveToFile( path ) )
				return fail( QString( "could not rewrite %1" ).arg( path ) );
		}
		// the manifest's A and M lines for the surviving blocks
		if ( haveManifest ) {
			QStringList lines = keep;
			for ( int i = 0; i < recs.size(); i++ ) {
				if ( gone.contains( i ) || !recs[i].iShape.isValid() )
					continue;
				const int b = nif.getBlockNumber( recs[i].iShape );
				if ( !recs[i].lodm.isEmpty() )
					lines.append( QString( "A %1 %2 %3" ).arg( b ).arg( recs[i].layer ).arg( recs[i].lodm ) );
				for ( const QString & mat : recs[i].materials )
					lines.append( QString( "M %1 %2" ).arg( b ).arg( mat ) );
			}
			QFile mf( manPath );
			if ( mf.open( QIODevice::WriteOnly | QIODevice::Text ) )
				mf.write( ( lines.join( QChar( '\n' ) ) + QChar( '\n' ) ).toUtf8() );
		}
		after += recs.size() - gone.size();
	}
	if ( report )
		*report = QString( "%1 shapes -> %2 across %3 chunks" ).arg( before ).arg( after ).arg( chunks );
	if ( error )
		error->clear();
	return true;
}

/*! Far-ring proxy meshes: the ratio for one ring. Ring 0 (dim 4) is what the
 *  player walks up to, and its chunks are the ones every byte-identity gate
 *  stands on, so it is never simplified whatever is set here. */
float lodgenSimplifyRatio( const LodgenSimplifyOptions & opts, int dim )
{
	if ( !opts.enabled )
		return 1.0f;
	switch ( dim ) {
	case 8:
		return opts.ratio8;
	case 16:
		return opts.ratio16;
	case 32:
		return opts.ratio32;
	default:
		return 1.0f;
	}
}

/*! Far-ring proxy meshes, run LAST after the merge: rings 2 and 3 ship at a
 *  fraction of their triangles with the same textures, which is what every
 *  engine since 2017 does with a far cluster. The merge has already made one
 *  shape per material, so "one proxy per cluster" is the same operation as
 *  simplifying that shape in place.
 *
 *  WHAT A SIMPLIFIER MUST NOT LOSE HERE. Every vertex of a chunk carries six
 *  channels the stock engine never reads and FO4CS does
 *  (docs/LODGEN_VERTEX_PACKING.md): the object identity index in vertex
 *  colours R+G, baked AO in B, the sway weight in A, sky visibility in UV2.x,
 *  the texture-array layer in UV2.y, and the ground-contact blend in Eye Data.
 *  Two of those are INDICES, not quantities: an interpolated identity is a
 *  different object and an interpolated layer is a different texture.
 *
 *  So the cut is made per GROUP, keyed by (identity index, array layer):
 *
 *   - meshoptimizer never creates a vertex -- the surviving set is a SUBSET of
 *     the original one -- so no channel is ever interpolated, whatever the
 *     metric does;
 *   - grouping by identity means no collapse can weld two objects together,
 *     and because a group is asked for at least two triangles and its
 *     originals are restored if the simplifier returns none, no object can
 *     vanish from the chunk;
 *   - grouping by layer means a merged shape that spans layers (the manifest's
 *     `A <block> -1`) still has one layer per group, so UV2.y is constant
 *     inside every collapse.
 *
 *  The four quantities ride along as weighted attributes, together with the
 *  normal and the UV, so the metric keeps them meaningful rather than merely
 *  intact. The weights are fractions of the error bound, so one knob scales
 *  the whole metric.
 *
 *  WHAT IS NOT CUT. A shape with an alpha property keeps every triangle: a
 *  cut-out card is four vertices that spell a silhouette and a collapse spends
 *  the silhouette to save nothing. That covers the impostor quads and the
 *  crossed quads inside vanilla's own tree LOD models alike; the impostor
 *  cards are excluded a second time, by object index off the manifest's `C`
 *  lines, so the rule is checkable rather than incidental. Groups at or under
 *  `minTris` keep every triangle too.
 *
 *  AFTERWARDS. Segments are regrouped by the cell of each triangle's CENTROID
 *  (the generator assigns them per placement; after a cut the placement is no
 *  longer the unit), the vertex array is compacted to the survivors, and the
 *  bounding sphere and the node's multi-bound AABB are recomputed from what is
 *  left. The manifest is not touched: no row's meaning changed. */
bool lodgenSimplifyFarRings( const QStringList & btoPaths, const LodgenSimplifyOptions & opts,
	QString * report, QString * error )
{
	auto fail = [error]( const QString & message ) {
		if ( error )
			*error = message;
		return false;
	};
	struct Vtx { Vector3 pos, nrm, tan; Vector2 uv, uv2; float bx = 0, by = 0, bz = 0, eye = 0; Color4 col; };
	int chunks = 0, shapesCut = 0, shapesKept = 0, groupsKept = 0, groupsRestored = 0;
	qint64 triIn = 0, triOut = 0, vtxIn = 0, vtxOut = 0;
	float worstErrorWorld = 0.0f;

	for ( const QString & path : btoPaths ) {
		/* The ring is the chunk's own dim, and its name carries it:
		 * <ws>.<dim>.<x>.<y>.BTO. The shape's Scale carries it too and is
		 * cross-checked below -- the error bound and the segment grid both
		 * depend on it, so a file whose two answers disagree is left alone
		 * rather than cut on a guess. */
		const QStringList nameParts = QFileInfo( path ).fileName().split( QChar( '.' ) );
		const int dim = nameParts.size() >= 5 ? nameParts[1].toInt() : 0;
		const float ratio = lodgenSimplifyRatio( opts, dim );
		if ( dim <= 4 || ratio >= 1.0f || ratio <= 0.0f )
			continue;
		NifModel nif;
		if ( !nif.loadFromFile( path ) )
			continue;

		// the impostor cards standing in this chunk, by object index
		QSet<quint32> cardIndices;
		{
			QFile mf( path + QStringLiteral( ".manifest.txt" ) );
			if ( mf.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
				while ( !mf.atEnd() ) {
					const QString line = QString::fromUtf8( mf.readLine() ).trimmed();
					if ( !line.startsWith( QLatin1String( "C " ) ) )
						continue;
					const QStringList t = line.split( QChar( ' ' ), Qt::SkipEmptyParts );
					if ( t.size() >= 2 )
						cardIndices.insert( t[1].toUInt() );
				}
			}
		}

		/* The bound: errorWorld world units at ring 0, times dim/4 for this
		 * ring, then divided by dim because a chunk shape's vertices are
		 * miniatures at Scale = dim. The two cancel, which is the point --
		 * the same on-screen error at every ring is a constant in the file's
		 * own units. */
		const float localBound = opts.errorWorld * ( float( dim ) / 4.0f ) / float( dim );
		const float cellSpan = 4096.0f / float( dim );   // one cell, in miniature units
		bool changed = false;
		bool sawShape = false;

		for ( int b = 0; b < nif.getBlockCount(); b++ ) {
			const QModelIndex iShape = nif.getBlockIndex( b );
			if ( !nif.isNiBlock( iShape, "BSSubIndexTriShape" ) )
				continue;
			const int parent = nif.getParent( b );
			const QModelIndex iNode = nif.getBlockIndex( parent );
			if ( !iNode.isValid() || !nif.isNiBlock( iNode, "BSMultiBoundNode" ) )
				continue;
			sawShape = true;
			if ( nif.getBlockIndex( nif.getLink( iShape, "Alpha Property" ) ).isValid() ) {
				shapesKept++;
				continue;
			}
			if ( qRound( nif.get<float>( iShape, "Scale" ) ) != dim ) {
				fprintf( stderr, "lodgen: simplify: %s block %d: scale %g is not the ring's dim %d, shape kept\n",
					QFileInfo( path ).fileName().toLocal8Bit().constData(), b,
					double( nif.get<float>( iShape, "Scale" ) ), dim );
				shapesKept++;
				continue;
			}

			const BSVertexDesc desc = nif.get<BSVertexDesc>( iShape, "Vertex Desc" );
			const bool fullPrec = ( ( desc.Value() >> 44 ) & 0x400 ) != 0;
			const QModelIndex iVD = nif.getIndex( iShape, "Vertex Data" );
			const int nv = int( nif.get<quint32>( iShape, "Num Vertices" ) );
			if ( nv < 3 || !iVD.isValid() ) {
				shapesKept++;
				continue;
			}
			const QModelIndex row0 = nif.index( 0, 0, iVD );
			const bool hasCol = nif.getIndex( row0, "Vertex Colors" ).isValid();
			const bool hasUv2 = nif.getIndex( row0, "UV 2" ).isValid();
			const bool hasEye = nif.getIndex( row0, "Eye Data" ).isValid();
			QVector<Vtx> verts( nv );
			for ( int v = 0; v < nv; v++ ) {
				const QModelIndex row = nif.index( v, 0, iVD );
				Vtx & o = verts[v];
				o.pos = fullPrec ? nif.get<Vector3>( row, "Vertex" ) : Vector3( nif.get<HalfVector3>( row, "Vertex" ) );
				o.uv = Vector2( nif.get<HalfVector2>( row, "UV" ) );
				o.nrm = Vector3( nif.get<ByteVector3>( row, "Normal" ) );
				o.tan = Vector3( nif.get<ByteVector3>( row, "Tangent" ) );
				o.bx = nif.get<float>( row, "Bitangent X" );
				o.by = nif.get<float>( row, "Bitangent Y" );
				o.bz = nif.get<float>( row, "Bitangent Z" );
				if ( hasCol )
					o.col = Color4( nif.get<ByteColor4>( row, "Vertex Colors" ) );
				if ( hasUv2 )
					o.uv2 = Vector2( nif.get<HalfVector2>( row, "UV 2" ) );
				if ( hasEye )
					o.eye = nif.get<float>( row, "Eye Data" );
			}
			const QVector<Triangle> tris = nif.getArray<Triangle>( nif.getIndex( iShape, "Triangles" ) );
			if ( tris.isEmpty() ) {
				shapesKept++;
				continue;
			}
			const QModelIndex iSegs = nif.getIndex( iShape, "Segment" );
			const int ns = iSegs.isValid() ? nif.rowCount( iSegs ) : 0;
			if ( ns > 1 && ns != dim * dim ) {
				fprintf( stderr, "lodgen: simplify: %s block %d: %d segments is neither 1 nor %dx%d, shape kept\n",
					QFileInfo( path ).fileName().toLocal8Bit().constData(), b, ns, dim, dim );
				shapesKept++;
				continue;
			}

			/* (identity index, array layer) per triangle. Both are constant
			 * over a triangle by construction: a triangle comes from one
			 * placement's one source shape, which carries one index and sits
			 * on one layer. */
			auto keyOf = [&]( quint16 v ) {
				const Vtx & o = verts[v];
				const quint32 id = hasCol
					? ( quint32( qBound( 0, qRound( o.col.red() * 255.0f ), 255 ) )
						| ( quint32( qBound( 0, qRound( o.col.green() * 255.0f ), 255 ) ) << 8 ) )
					: 0U;
				const quint32 layer = hasUv2 ? quint32( qBound( 0, qRound( o.uv2[1] ), 0xFFFF ) ) : 0U;
				return id | ( layer << 16 );
			};
			QHash<quint32, QVector<int>> groups;
			QVector<quint32> order;
			for ( int t = 0; t < tris.size(); t++ ) {
				const quint32 key = keyOf( tris[t].v1() );
				if ( !groups.contains( key ) )
					order.append( key );
				groups[key].append( t );
			}

			/* Weights as fractions of the error bound: a full unit swing of an
			 * attribute costs that share of the geometric budget, so one knob
			 * scales position and appearance together. UV first because a
			 * swimming texture is the artefact a distant chunk shows soonest;
			 * the baked channels are quantities, and a quarter-bound is enough
			 * to stop a collapse that would flatten one. UV2.y (the layer) is
			 * NOT an attribute: it is in the group key, so it never varies
			 * inside a collapse. */
			std::vector<float> weights;
			for ( int k = 0; k < 3; k++ )
				weights.push_back( localBound * 0.5f );          // normal
			for ( int k = 0; k < 2; k++ )
				weights.push_back( localBound * 1.0f );          // UV
			if ( hasUv2 )
				weights.push_back( localBound * 0.25f );         // sky visibility, UV2.x
			if ( hasCol ) {
				weights.push_back( localBound * 0.25f );         // baked AO, colour B
				weights.push_back( localBound * 0.25f );         // sway weight, colour A
			}
			if ( hasEye )
				weights.push_back( localBound * 0.25f );         // ground contact
			const size_t attrCount = weights.size();

			QVector<Triangle> outTris;
			outTris.reserve( tris.size() );
			std::vector<int> stamp( size_t( nv ), -1 ), localOf( size_t( nv ), 0 );
			int groupId = 0;
			bool cutSomething = false;
			for ( const quint32 key : order ) {
				const QVector<int> & gtris = groups[key];
				const bool isCard = cardIndices.contains( key & 0xFFFFU );
				if ( isCard || gtris.size() <= opts.minTris ) {
					for ( int t : gtris )
						outTris.append( tris[t] );
					groupsKept++;
					continue;
				}
				groupId++;
				std::vector<float> gpos, gattr;
				std::vector<unsigned int> gidx, gvert;
				gidx.reserve( size_t( gtris.size() ) * 3 );
				for ( int t : gtris ) {
					const quint16 corner[3] = { tris[t].v1(), tris[t].v2(), tris[t].v3() };
					for ( int k = 0; k < 3; k++ ) {
						const quint16 gv = corner[k];
						if ( stamp[gv] != groupId ) {
							stamp[gv] = groupId;
							localOf[gv] = int( gvert.size() );
							gvert.push_back( gv );
							const Vtx & o = verts[gv];
							gpos.push_back( o.pos[0] );
							gpos.push_back( o.pos[1] );
							gpos.push_back( o.pos[2] );
							gattr.push_back( o.nrm[0] );
							gattr.push_back( o.nrm[1] );
							gattr.push_back( o.nrm[2] );
							gattr.push_back( o.uv[0] );
							gattr.push_back( o.uv[1] );
							if ( hasUv2 )
								gattr.push_back( o.uv2[0] );
							if ( hasCol ) {
								gattr.push_back( o.col.blue() );
								gattr.push_back( o.col.alpha() );
							}
							if ( hasEye )
								gattr.push_back( o.eye );
						}
						gidx.push_back( (unsigned int) localOf[gv] );
					}
				}
				/* At least two triangles asked of every group, so nothing this
				 * pass touches can leave the chunk: the identity set before
				 * the cut is the identity set after it. */
				const size_t targetIdx = qMax<size_t>( 6,
					size_t( qMax( 1, qRound( double( gtris.size() ) * double( ratio ) ) ) ) * 3 );
				if ( targetIdx >= gidx.size() ) {
					for ( int t : gtris )
						outTris.append( tris[t] );
					groupsKept++;
					continue;
				}
				std::vector<unsigned int> simplified( gidx.size() );
				float resultError = 0.0f;
				const size_t count = meshopt_simplifyWithAttributes( simplified.data(),
					gidx.data(), gidx.size(), gpos.data(), gvert.size(), 12,
					gattr.data(), attrCount * sizeof( float ), weights.data(), attrCount,
					nullptr, targetIdx, localBound, meshopt_SimplifyErrorAbsolute, &resultError );
				if ( count < 3 || ( count % 3 ) != 0 || count > gidx.size() ) {
					/* The simplifier gave nothing usable back. Keep the group
					 * whole: an object that vanishes is a worse answer than an
					 * object that costs its triangles. */
					for ( int t : gtris )
						outTris.append( tris[t] );
					groupsRestored++;
					continue;
				}
				for ( size_t i = 0; i + 2 < count; i += 3 )
					outTris.append( Triangle( quint16( gvert[simplified[i]] ),
						quint16( gvert[simplified[i + 1]] ), quint16( gvert[simplified[i + 2]] ) ) );
				worstErrorWorld = qMax( worstErrorWorld, resultError * float( dim ) );
				cutSomething = cutSomething || count < gidx.size();
			}
			if ( !cutSomething ) {
				shapesKept++;
				continue;
			}

			// segments by the cell of each triangle's centroid
			QVector<QVector<Triangle>> segTris( qMax( 1, ns ) );
			for ( const Triangle & t : outTris ) {
				int seg = 0;
				if ( ns == dim * dim ) {
					const Vector3 c = ( verts[t.v1()].pos + verts[t.v2()].pos + verts[t.v3()].pos ) * ( 1.0f / 3.0f );
					const int lx = qBound( 0, int( c[0] / cellSpan ), dim - 1 );
					const int ly = qBound( 0, int( c[1] / cellSpan ), dim - 1 );
					seg = ly * dim + lx;
				}
				segTris[seg].append( t );
			}
			QVector<Triangle> finalTris;
			QVector<QPair<int, int>> segRuns;
			finalTris.reserve( outTris.size() );
			for ( const QVector<Triangle> & st : segTris ) {
				segRuns.append( qMakePair( finalTris.size(), st.size() ) );
				finalTris += st;
			}

			// compact the vertex array to the survivors, in draw order
			QVector<int> remap( nv, -1 );
			QVector<Vtx> newVerts;
			newVerts.reserve( nv );
			for ( Triangle & t : finalTris ) {
				quint16 corner[3] = { t.v1(), t.v2(), t.v3() };
				for ( int k = 0; k < 3; k++ ) {
					if ( remap[corner[k]] < 0 ) {
						remap[corner[k]] = int( newVerts.size() );
						newVerts.append( verts[corner[k]] );
					}
					corner[k] = quint16( remap[corner[k]] );
				}
				t = Triangle( corner[0], corner[1], corner[2] );
			}

			triIn += tris.size();
			triOut += finalTris.size();
			vtxIn += nv;
			vtxOut += newVerts.size();
			shapesCut++;
			changed = true;

			// write it back
			const quint32 numVerts = quint32( newVerts.size() ), numTris = quint32( finalTris.size() );
			nif.set<quint32>( iShape, "Num Vertices", numVerts );
			nif.set<quint32>( iShape, "Num Triangles", numTris );
			nif.set<quint32>( iShape, "Data Size", numVerts * quint32( desc.GetVertexSize() ) + numTris * 6 );
			nif.setState( BaseModel::Processing );
			/* Re-fetched after the state change and before the resize, as the
			 * merge does: an index held across updateArraySize is the kind of
			 * thing that works until the array shrinks under it. */
			const QModelIndex iVDw = nif.getIndex( iShape, "Vertex Data" );
			nif.updateArraySize( iVDw );
			float mnx = 3.4e38f, mny = 3.4e38f, mnz = 3.4e38f, mxx = -3.4e38f, mxy = -3.4e38f, mxz = -3.4e38f;
			for ( int v = 0; v < newVerts.size(); v++ ) {
				const QModelIndex row = nif.index( v, 0, iVDw );
				const Vtx & o = newVerts[v];
				mnx = qMin( mnx, o.pos[0] ); mny = qMin( mny, o.pos[1] ); mnz = qMin( mnz, o.pos[2] );
				mxx = qMax( mxx, o.pos[0] ); mxy = qMax( mxy, o.pos[1] ); mxz = qMax( mxz, o.pos[2] );
				if ( fullPrec )
					nif.set<Vector3>( row, "Vertex", o.pos );
				else
					nif.set<HalfVector3>( row, "Vertex", HalfVector3( o.pos ) );
				nif.set<HalfVector2>( row, "UV", HalfVector2( o.uv ) );
				nif.set<ByteVector3>( row, "Normal", ByteVector3( o.nrm ) );
				nif.set<ByteVector3>( row, "Tangent", ByteVector3( o.tan ) );
				nif.set<float>( row, "Bitangent X", o.bx );
				nif.set<float>( row, "Bitangent Y", o.by );
				nif.set<float>( row, "Bitangent Z", o.bz );
				if ( hasCol )
					nif.set<ByteColor4>( row, "Vertex Colors", ByteColor4( FloatVector4( o.col.red(), o.col.green(), o.col.blue(), o.col.alpha() ) ) );
				if ( hasUv2 )
					nif.set<HalfVector2>( row, "UV 2", HalfVector2( o.uv2 ) );
				if ( hasEye )
					nif.set<float>( row, "Eye Data", o.eye );
			}
			const QModelIndex iTris = nif.getIndex( iShape, "Triangles" );
			nif.updateArraySize( iTris );
			nif.setArray<Triangle>( iTris, finalTris );
			nif.set<quint32>( iShape, "Num Primitives", numTris );
			const QModelIndex iSegsW = nif.getIndex( iShape, "Segment" );
			if ( iSegsW.isValid() ) {
				nif.set<quint32>( iShape, "Num Segments", quint32( segRuns.size() ) );
				nif.set<quint32>( iShape, "Total Segments", quint32( segRuns.size() ) );
				nif.updateArraySize( iSegsW );
				for ( int s = 0; s < segRuns.size(); s++ ) {
					const QModelIndex seg = nif.index( s, 0, iSegsW );
					nif.set<quint32>( seg, "Start Index", quint32( segRuns[s].first * 3 ) );
					nif.set<quint32>( seg, "Num Primitives", quint32( segRuns[s].second ) );
					nif.set<quint32>( seg, "Parent Array Index", 0xFFFFFFFFU );
				}
			}
			setBound( &nif, iShape, mnx, mny, mnz, mxx, mxy, mxz );
			nif.restoreState();
			// the node's AABB is in the chunk's own frame: miniatures x dim
			const QModelIndex iMB = nif.getBlockIndex( nif.getLink( iNode, "Multi Bound" ) );
			const QModelIndex iBox = iMB.isValid() ? nif.getBlockIndex( nif.getLink( iMB, "Data" ) ) : QModelIndex();
			if ( iBox.isValid() ) {
				nif.set<Vector3>( iBox, "Position", Vector3( ( mnx + mxx ) * 0.5f * float( dim ),
					( mny + mxy ) * 0.5f * float( dim ), ( mnz + mxz ) * 0.5f * float( dim ) ) );
				nif.set<Vector3>( iBox, "Extent", Vector3( ( mxx - mnx ) * 0.5f * float( dim ),
					( mxy - mny ) * 0.5f * float( dim ), ( mxz - mnz ) * 0.5f * float( dim ) ) );
			}
		}
		if ( sawShape )
			chunks++;
		if ( changed && !nif.saveToFile( path ) )
			return fail( QString( "could not rewrite %1" ).arg( path ) );
	}

	if ( report )
		*report = QString( "%1 shapes cut in %2 chunks: %3 -> %4 triangles, %5 -> %6 vertices"
			" (%7 shapes and %8 groups kept whole, %9 restored; worst error %10 units)" )
			.arg( shapesCut ).arg( chunks ).arg( triIn ).arg( triOut ).arg( vtxIn ).arg( vtxOut )
			.arg( shapesKept ).arg( groupsKept ).arg( groupsRestored )
			.arg( double( worstErrorWorld ), 0, 'f', 1 );
	if ( error )
		error->clear();
	return true;
}

/*! Card sheet arrays: every octahedral card set a worldspace's chunks stand
 *  on, packed by family and sheet size into one DX10 array per texture (BC3,
 *  and BC1 for the emissive, which has no alpha), so a consumer draws a whole
 *  ring's trees as one instanced quad per chunk instead of one texture bind
 *  per tree type.
 *
 *  The layers are built the way `lodgenCard` builds a set's own sheets —
 *  from the bake's PNGs, dilated frame by frame, mips capped while a frame
 *  spans eight texels — so a layer is what the per-card DDS would hold, not
 *  a re-encoding of it. A `.lodm` of kind `cardArray` beside the arrays
 *  carries the family, the three textures, the grid and frame shared by the
 *  set, and per layer the card's own extents, centre, depth span and source
 *  (`docs/LODGEN_IMPOSTOR_SPEC.md`). Every `C` manifest line whose card sits
 *  in an array gains two tokens: the array's `.lodm` and the layer. The
 *  per-card sets stay beside the cards for a consumer without arrays. */
bool lodgenBuildCardArrays( const QStringList & btoPaths, const QString & cardDir,
	const QString & arrayFileBase, const QString & arrayGameBase, int auxDiv,
	QString * report, QString * error )
{
	auto fail = [error]( const QString & message ) {
		if ( error )
			*error = message;
		return false;
	};
	// pass 1: every card set the manifests stand on, by its .lodm game path
	QMap<QString, QString> idOf;         // lodm game path (lower) -> card id (QMap: stable order)
	for ( const QString & path : btoPaths ) {
		QFile mf( path + QStringLiteral( ".manifest.txt" ) );
		if ( !mf.open( QIODevice::ReadOnly | QIODevice::Text ) )
			continue;
		while ( !mf.atEnd() ) {
			const QString line = QString::fromUtf8( mf.readLine() ).trimmed();
			if ( !line.startsWith( QLatin1String( "C " ) ) )
				continue;
			const QStringList t = line.split( QChar( ' ' ), Qt::SkipEmptyParts );
			if ( t.size() < 10 )
				continue;
			const QString lodm = t[9];
			QString id = lodm;
			id.replace( QChar( '\\' ), QChar( '/' ) );
			id = id.mid( id.lastIndexOf( QChar( '/' ) ) + 1 );
			if ( !id.endsWith( QStringLiteral( "_oct.lodm" ), Qt::CaseInsensitive ) )
				continue;
			id.chop( 9 );
			idOf.insert( lodm.toLower(), id );
		}
	}
	if ( idOf.isEmpty() )
		return fail( QStringLiteral( "no placement in the chunks stands on an octahedral card" ) );

	// pass 2: the sets, grouped by family and sheet size, their layers built as lodgenCard builds them
	/* `conv` is the view-convention token (2026-09-19): per LAYER, not per
	 * array, because an array packs whatever sets share a size class and a
	 * library part-way through a re-bake legitimately holds both vintages. */
	struct Layer { QString id, lodmGame, source, projection, conv; float halfW = 0, halfH = 0, span = 0, emissiveScale = 1.0f; Vector3 center; QJsonArray frameOff; QJsonObject coverage; };
	struct Group { bool pbr = false; int w = 0, h = 0, aw = 0, ah = 0, oct = 0, fw = 0, fh = 0, padX = 0, padY = 0, gapX = 0, gapY = 0, mips = 1, auxMips = 1; QVector<Layer> layers; std::vector<std::vector<quint32>> color, n, mask, emis; };
	if ( auxDiv < 1 )
		auxDiv = 1;
	QMap<QString, Group> groups;
	int unreadable = 0;
	auto pixels = []( const QImage & img ) {
		std::vector<quint32> px( size_t( img.width() ) * img.height() );
		for ( int y = 0; y < img.height(); y++ )
			for ( int x = 0; x < img.width(); x++ )
				px[size_t( y ) * img.width() + x] = img.pixel( x, y );
		return px;
	};
	for ( auto it = idOf.constBegin(); it != idOf.constEnd(); ++it ) {
		const QString & id = it.value();
		QFile lf( cardDir + "/" + id + QStringLiteral( "_oct.lodm" ) );
		LodmMaterial lm;
		if ( lf.open( QIODevice::ReadOnly ) )
			lm = lodmParse( lf.readAll() );
		if ( !lm.ok || lm.kind != QLatin1String( "card" ) ) {
			unreadable++;
			continue;
		}
		const QJsonObject card = lm.root.value( QStringLiteral( "card" ) ).toObject();
		const QJsonArray frame = card.value( QStringLiteral( "frame" ) ).toArray();
		const QJsonArray half = card.value( QStringLiteral( "half" ) ).toArray();
		const QJsonArray center = card.value( QStringLiteral( "center" ) ).toArray();
		const int oct = card.value( QStringLiteral( "oct" ) ).toInt();
		const int fw = frame.size() == 2 ? frame[0].toInt() : 0, fh = frame.size() == 2 ? frame[1].toInt() : 0;
		/* The gutter and the GAP, per axis, as the set's own .lodm records them. An
		 * array is built from the same PNGs and the same dilation as the per-card set,
		 * so it inherits that set's spacing and therefore that set's clean mip depth.
		 * The MIP CAP is log2(min(gap)) (bungo, 2026-09-09 evening: one mip fewer, so
		 * the deepest shipped level still has a whole texel of margin on each side of
		 * a border). Three vintages, and the same expression serves all of them:
		 * `gap` present names the gap outright, while `pad` alone (lane CARDFIT3) and
		 * neither (older still, max(4, longSide/16) per side) wrote a PER-SIDE number
		 * whose gap is twice it -- and log2(2*pad) = 1 + log2(pad), exactly the chain
		 * those two were built for. */
		const QJsonArray padA = card.value( QStringLiteral( "pad" ) ).toArray();
		const QJsonArray gapA = card.value( QStringLiteral( "gap" ) ).toArray();
		const int padFallback = qMax( 4, qMax( fw, fh ) / 16 );
		const int padX = padA.size() == 2 ? padA[0].toInt() : padFallback;
		const int padY = padA.size() == 2 ? padA[1].toInt() : padFallback;
		const int gapX = gapA.size() == 2 ? gapA[0].toInt() : 2 * padX;
		const int gapY = gapA.size() == 2 ? gapA[1].toInt() : 2 * padY;
		const int mipUnit = qMin( gapX, gapY );
		if ( oct < 2 || fw <= 0 || fh <= 0 ) {
			unreadable++;
			continue;
		}
		QImage alb( cardDir + "/" + id + QStringLiteral( "_oct_albedo.png" ) );
		QImage nrm( cardDir + "/" + id + QStringLiteral( "_oct_normal.png" ) );
		QImage rm( cardDir + "/" + id + QStringLiteral( "_oct" ) + QLatin1String( lodmMaskSuffix( lm.pbr ) ) + QStringLiteral( ".png" ) );
		if ( alb.isNull() || nrm.size() != alb.size() || rm.size() != alb.size()
			|| alb.width() != oct * fw || alb.height() != oct * fh ) {
			unreadable++;
			continue;
		}
		alb = alb.convertToFormat( QImage::Format_ARGB32 );
		nrm = nrm.convertToFormat( QImage::Format_ARGB32 );
		rm = rm.convertToFormat( QImage::Format_ARGB32 );
		/* The emissive sheet. A set baked before it existed has none: that
		 * layer is BLACK, so the array keeps one layer per set and a layer
		 * index still means what the C lines say it means. */
		QImage emi( cardDir + "/" + id + QStringLiteral( "_oct" ) + QLatin1String( lodmEmissiveSuffix( lm.pbr ) ) + QStringLiteral( ".png" ) );
		if ( emi.isNull() || emi.size() != alb.size() ) {
			emi = QImage( alb.size(), QImage::Format_ARGB32 );
			emi.fill( qRgba( 0, 0, 0, 255 ) );
		} else {
			emi = emi.convertToFormat( QImage::Format_ARGB32 );
		}
		const int deep = qMax( 8, qMax( fw, fh ) / 8 );
		lodgenDilateFrames( nrm, alb, fw, fh, deep );
		lodgenDilateFrames( rm, alb, fw, fh, deep );
		lodgenDilateFrames( emi, alb, fw, fh, deep );
		lodgenDilateFrames( alb, alb, fw, fh, deep );
		{
			QString heightReport;
			lodgenRepairOctHeight( nrm, alb, fw, fh, &heightReport );
			if ( !heightReport.isEmpty() )
				fprintf( stderr, "lodgen: arrays: card %s: %s\n", id.toLocal8Bit().constData(),
					heightReport.toLocal8Bit().constData() );
		}
		const QString key = QString( "%1|%2x%3" ).arg( lm.pbr ? QStringLiteral( "pbr" ) : QStringLiteral( "legacy" ) ).arg( alb.width() ).arg( alb.height() );
		Group & g = groups[key];
		g.pbr = lm.pbr;
		g.w = alb.width();
		g.h = alb.height();
		g.oct = oct;
		g.fw = fw;
		g.fh = fh;
		g.padX = padX;
		g.padY = padY;
		g.gapX = gapX;
		g.gapY = gapY;
		// the clean depth: a whole texel of MARGIN inside each of the two frames that
		// meet on a border, at the deepest level sampled -- log2(gap)
		g.mips = 0;
		for ( int g2 = mipUnit; g2 >= 2; g2 /= 2 )
			g.mips++;
		g.mips = qMax( 1, g.mips );
		/* The three sheets that are not the base colour come down by auxDiv,
		 * AFTER the dilation just above - the gutter keeps a halving from
		 * mixing across a frame border. They become their own arrays at their
		 * own size; a layer index still means the same set in all four. */
		g.aw = qMax( 4, g.w / auxDiv );
		g.ah = qMax( 4, g.h / auxDiv );
		g.auxMips = 0;
		for ( int g2 = mipUnit / auxDiv; g2 >= 2; g2 /= 2 )
			g.auxMips++;
		g.auxMips = qMax( 1, g.auxMips );
		if ( auxDiv > 1 ) {
			nrm = nrm.scaled( g.aw, g.ah, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
			rm = rm.scaled( g.aw, g.ah, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
			emi = emi.scaled( g.aw, g.ah, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
		}
		Layer l;
		l.id = id;
		l.lodmGame = it.key();
		l.source = card.value( QStringLiteral( "source" ) ).toString();
		if ( half.size() == 2 ) {
			l.halfW = float( half[0].toDouble() );
			l.halfH = float( half[1].toDouble() );
		}
		if ( center.size() == 3 )
			l.center = Vector3( float( center[0].toDouble() ), float( center[1].toDouble() ), float( center[2].toDouble() ) );
		l.span = float( card.value( QStringLiteral( "depthSpan" ) ).toDouble() );
		/* PER-FRAME POSITIONING travels with the layer, like its geometry: the
		 * frames of two sets in one array sit at different offsets, so the array
		 * cannot hold one list for all of them. Absent on a set from before the
		 * law, and absent from that layer here too. */
		l.frameOff = card.value( QStringLiteral( "frameOffset" ) ).toArray();
		/* THE CAMERA the layer's own sheet was photographed through travels with
		 * the layer too, and for the same reason as its geometry: an array can
		 * hold a metric set and a foreshortened one side by side, and only the
		 * layer knows which it is. Absent on a set from before the line, which
		 * means perspective (lane CARDORTHO, 2026-09-10). */
		l.projection = card.value( QStringLiteral( "projection" ) ).toString();
		l.conv = card.value( QStringLiteral( "conv" ) ).toString();
		/* THE COVERAGE CONTRACT travels with the layer for the same reason: an
		 * array can hold a set whose alpha the consumer's 0.5 test reads correctly
		 * beside one from before the contract, whose alpha is the raw fraction and
		 * whose silhouette at 0.5 is smaller than its `half` declares. Absent on
		 * an older set, and absent from that layer here too (lane CARDWIDTH,
		 * 2026-09-10). */
		l.coverage = card.value( QStringLiteral( "coverage" ) ).toObject();
		// the set's emissive multiple travels with its layer, like its geometry
		l.emissiveScale = lm.emissiveScale;
		g.layers.append( l );
		g.color.push_back( pixels( alb ) );
		g.n.push_back( lodgenNLayout2( pixels( nrm ) ) );	// `_n` layout 2
		g.mask.push_back( pixels( rm ) );
		g.emis.push_back( pixels( emi ) );
	}
	if ( groups.isEmpty() )
		return fail( QStringLiteral( "none of the card sets could be read" ) );

	// pass 3: the arrays and their .lodm; the layer of every card
	QHash<QString, QPair<QString, int>> layerOf;   // card lodm (lower) -> (array lodm game path, layer)
	int arrays = 0, sets = 0;
	for ( auto it = groups.begin(); it != groups.end(); ++it ) {
		Group & g = it.value();
		const QString sizeKey = it.key().mid( it.key().indexOf( QChar( '|' ) ) + 1 );
		const QString stem = QString( ".%1.%2" ).arg( g.pbr ? QStringLiteral( "pbr" ) : QStringLiteral( "legacy" ) ).arg( sizeKey );
		const QString fileBase = arrayFileBase + stem, gameBase = arrayGameBase + stem;
		const QString colorSfx = QLatin1String( lodmColorSuffix( g.pbr ) ) + QStringLiteral( ".DDS" );
		const QString maskSfx = QLatin1String( lodmMaskSuffix( g.pbr ) ) + QStringLiteral( ".DDS" );
		const QString emSfx = QLatin1String( lodmEmissiveSuffix( g.pbr ) ) + QStringLiteral( ".DDS" );
		const struct { QString suffix; const std::vector<std::vector<quint32>> * px; bool bc3; bool aux; } sheets[4] = {
			{ colorSfx, &g.color, true, false }, { QStringLiteral( "_n.DDS" ), &g.n, true, true },
			{ maskSfx, &g.mask, true, true },
			// the emissive is BC1: three channels and no alpha to carry
			{ emSfx, &g.emis, false, true } };
		for ( const auto & s : sheets ) {
			const int sw = s.aux ? g.aw : g.w, sh = s.aux ? g.ah : g.h;
			const int sm = s.aux ? g.auxMips : g.mips;
			if ( !lodgenWriteDdsArray( fileBase + s.suffix, sw, sh, *s.px, s.bc3, sm ) )
				return fail( QString( "could not write %1" ).arg( fileBase + s.suffix ) );
			arrays++;
		}
		QJsonObject root, tex, arr;
		root.insert( QStringLiteral( "lodm" ), 1 );
		root.insert( QStringLiteral( "family" ), g.pbr ? QStringLiteral( "pbr" ) : QStringLiteral( "legacy" ) );
		root.insert( QStringLiteral( "kind" ), QStringLiteral( "cardArray" ) );
		root.insert( QStringLiteral( "nlayout" ), 2 );	// `_n`: height in A, sway in B
		tex.insert( QLatin1String( lodmColorKey( g.pbr ) ), gameBase + colorSfx );
		tex.insert( QStringLiteral( "normal" ), gameBase + QStringLiteral( "_n.DDS" ) );
		tex.insert( QLatin1String( lodmMaskKey( g.pbr ) ), gameBase + maskSfx );
		tex.insert( QStringLiteral( "emissive" ), gameBase + emSfx );
		root.insert( QStringLiteral( "textures" ), tex );
		arr.insert( QStringLiteral( "class" ), QJsonArray{ g.w, g.h } );
		arr.insert( QStringLiteral( "oct" ), g.oct );
		arr.insert( QStringLiteral( "frame" ), QJsonArray{ g.fw, g.fh } );
		arr.insert( QStringLiteral( "pad" ), QJsonArray{ g.padX, g.padY } );
		// the distance between two neighbouring silhouettes, shared: the mip cap's own input
		arr.insert( QStringLiteral( "gap" ), QJsonArray{ g.gapX, g.gapY } );
		arr.insert( QStringLiteral( "mips" ), g.mips );
		if ( auxDiv > 1 ) {
			// the other three arrays are this much smaller on each side
			arr.insert( QStringLiteral( "auxDiv" ), auxDiv );
			arr.insert( QStringLiteral( "auxClass" ), QJsonArray{ g.aw, g.ah } );
			arr.insert( QStringLiteral( "auxMips" ), g.auxMips );
		}
		QJsonArray layers, scales;
		const QString lodmGame = gameBase + QStringLiteral( ".lodm" );
		for ( int l = 0; l < g.layers.size(); l++ ) {
			const Layer & L = g.layers[l];
			QJsonObject o;
			o.insert( QStringLiteral( "id" ), L.id );
			o.insert( QStringLiteral( "half" ), QJsonArray{ double( L.halfW ), double( L.halfH ) } );
			o.insert( QStringLiteral( "center" ), QJsonArray{ double( L.center[0] ), double( L.center[1] ), double( L.center[2] ) } );
			o.insert( QStringLiteral( "depthSpan" ), double( L.span ) );
			if ( !L.frameOff.isEmpty() )
				o.insert( QStringLiteral( "frameOffset" ), L.frameOff );
			if ( !L.projection.isEmpty() )
				o.insert( QStringLiteral( "projection" ), L.projection );
			// the view convention this layer's frames were photographed under;
			// absent = the pre-2026-09-19 bake, azimuth turned by 180 degrees
			if ( !L.conv.isEmpty() )
				o.insert( QStringLiteral( "conv" ), L.conv );
			if ( !L.coverage.isEmpty() )
				o.insert( QStringLiteral( "coverage" ), L.coverage );
			if ( !L.source.isEmpty() )
				o.insert( QStringLiteral( "source" ), L.source );
			layers.append( o );
			scales.append( double( L.emissiveScale ) );
			layerOf.insert( L.lodmGame, qMakePair( lodmGame, l ) );
			sets++;
		}
		arr.insert( QStringLiteral( "layers" ), layers );
		// one multiple per layer, parallel to `layers`, as the mesh arrays have it
		arr.insert( QStringLiteral( "emissiveScale" ), scales );
		root.insert( QStringLiteral( "array" ), arr );
		if ( !lodmWriteFile( fileBase + QStringLiteral( ".lodm" ), root ) )
			return fail( QString( "could not write %1" ).arg( fileBase + QStringLiteral( ".lodm" ) ) );
	}

	// pass 4: the C lines gain the array's .lodm and the layer
	int cLines = 0;
	for ( const QString & path : btoPaths ) {
		const QString manPath = path + QStringLiteral( ".manifest.txt" );
		QFile mf( manPath );
		if ( !mf.open( QIODevice::ReadOnly | QIODevice::Text ) )
			continue;
		QStringList lines;
		bool changed = false;
		while ( !mf.atEnd() ) {
			QString line = QString::fromUtf8( mf.readLine() ).trimmed();
			if ( line.startsWith( QLatin1String( "C " ) ) ) {
				const QStringList t = line.split( QChar( ' ' ), Qt::SkipEmptyParts );
				if ( t.size() == 10 ) {
					auto lit = layerOf.constFind( t[9].toLower() );
					if ( lit != layerOf.constEnd() ) {
						line += QString( " %1 %2" ).arg( lit.value().first ).arg( lit.value().second );
						changed = true;
						cLines++;
					}
				}
			}
			lines.append( line );
		}
		mf.close();
		if ( changed && mf.open( QIODevice::WriteOnly | QIODevice::Text ) )
			mf.write( ( lines.join( QChar( '\n' ) ) + QChar( '\n' ) ).toUtf8() );
	}
	if ( report )
		*report = QString( "%1 card sets in %2 arrays (%3 groups), %4 C lines carry a layer, %5 sets unreadable" )
			.arg( sets ).arg( arrays ).arg( groups.size() ).arg( cLines ).arg( unreadable );
	if ( error )
		error->clear();
	return true;
}

/* ================= THE INPUT LEDGER (.lodb) -- lane INCR1, 2026-09-12 =======
 *
 * See docs/LODGEN_LEDGER_FORMAT.md. The dependency map came first; this is its
 * implementation and nothing here decides policy.
 */

#include <QCryptographicHash>
#include <QJsonDocument>

//! Asset bytes are hashed once per path per process: a dim-4 region asks for
//! the same tree mesh hundreds of times and the answer cannot change under us.
static QMutex                    g_ledgerAssetMutex;
static QHash<QString, QString>   g_ledgerAssetDigest;

static QString lodgenLedgerAssetDigest( const QString & dataRoot, const QString & relPath,
	const char * folder = "meshes", const char * ext = ".nif" )
{
	if ( relPath.isEmpty() )
		return QString();
	/* The folder is part of the key: the same relative name can name a mesh and
	 * a texture, and a cache that forgot which it read would hand a .nif digest
	 * to a .dds question. */
	const QString key = QString::fromLatin1( folder ) + QLatin1Char( '|' ) + relPath.toLower();
	{
		QMutexLocker lock( &g_ledgerAssetMutex );
		const auto it = g_ledgerAssetDigest.constFind( key );
		if ( it != g_ledgerAssetDigest.constEnd() )
			return it.value();
	}
	QByteArray bytes;
	QString hex;
	if ( lodgenReadAsset( dataRoot, relPath, folder, ext, bytes ) && !bytes.isEmpty() )
		hex = QString::fromLatin1( QCryptographicHash::hash( bytes, QCryptographicHash::Sha1 ).toHex() );
	else
		hex = QStringLiteral( "missing" );   /* recorded, not skipped: a model that
		                                      * APPEARS later must count as a change */
	QMutexLocker lock( &g_ledgerAssetMutex );
	g_ledgerAssetDigest.insert( key, hex );
	return hex;
}

QString lodgenFileDigest( const QString & path )
{
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) )
		return QString();
	QCryptographicHash h( QCryptographicHash::Sha1 );
	if ( !h.addData( &f ) )
		return QString();
	return QString::fromLatin1( h.result().toHex() );
}

QString lodgenChunkInputDigest( const EsmWorld & world, int dim, int cx, int cy,
	const QString & dataRoot )
{
	QCryptographicHash h( QCryptographicHash::Sha1 );
	auto feed = [&h]( const QByteArray & b ) { h.addData( b ); };
	auto feedStr = [&feed]( const QString & s ) { feed( s.toLower().toUtf8() ); feed( "\x1f" ); };
	auto feedI = [&feed]( qint64 v ) {
		char buf[24];
		const int n = qsnprintf( buf, sizeof( buf ), "%lld;", static_cast<long long>( v ) );
		feed( QByteArray( buf, n ) );
	};
	/* The float rule: a REFR's position is fed as its RAW BYTES below, never
	 * as a rounded decimal -- a 0.001-unit nudge moves a shadow, and a decimal
	 * with too few places would hide it. */

	feedI( dim ); feedI( cx ); feedI( cy );

	/* THE ONE-CELL RING. lodgen.h's LODGEN_TERRAIN_RING_CELLS and the AO skirt
	 * (LodgenObjectOptions::aoSkirtCells) each reach exactly one cell past the
	 * chunk; the land-guide macro gradient reaches --land-guide-scale/2, which
	 * is capped at 1024 units and so is well inside it. Widening here is the
	 * cheap half of the correctness: this loop is why a height edit one cell
	 * outside a chunk still dirties it. */
	for ( int y = cy - 1; y < cy + dim + 1; y++ ) {
		for ( int x = cx - 1; x < cx + dim + 1; x++ ) {
			feedI( x ); feedI( y );
			EsmLand land;
			if ( !world.land( x, y, land ) || !land.valid ) {
				feed( "noland;" );
			} else {
				feed( QByteArray( reinterpret_cast<const char *>( &land.heights[0][0] ),
					int( sizeof( land.heights ) ) ) );
				feedI( land.hasColors ? 1 : 0 );
				if ( land.hasColors )
					feed( QByteArray( reinterpret_cast<const char *>( &land.colors[0][0][0] ),
						int( sizeof( land.colors ) ) ) );
				QSet<quint32> ltexSeen;
				auto feedLtex = [&]( quint32 form ) {
					/* THE LANDSCAPE TEXTURES THEMSELVES (2026-09-12, second
					 * pass). Feeding only the LTEX form id was a hole: the
					 * colour sheet is built from the LTEX's DIFFUSE BYTES, so a
					 * loose override of that .dds changes the sheet while every
					 * form id in the cell stays put, and an incremental run
					 * would have kept a stale sheet and called it clean. The
					 * gate arm that found this is in the report; the digest
					 * below is the fix, and the arm now passes. */
					if ( !form || ltexSeen.contains( form ) )
						return;
					ltexSeen.insert( form );
					const EsmLtexTextureSet & ts = world.ltexTextureSet( form );
					feedI( ts.exists ? 1 : 0 );
					for ( const QString & tp : { ts.diffuse, ts.normal, ts.specular } ) {
						feedStr( tp );
						if ( !tp.isEmpty() )
							feedStr( lodgenLedgerAssetDigest( dataRoot, tp, "textures", ".dds" ) );
					}
					feedStr( ts.material );
					/* Vanilla's LTEX TXSTs spell their MNAM as the ABSOLUTE
					 * path of Bethesda's own build machine
					 * ("c:/projects/fallout4/build/pc/data/materials/..."), so
					 * asking the archives for it can only ever miss, once per
					 * LTEX, loudly. The path string is still fed -- a changed
					 * spelling is still a changed input -- but the lookup is
					 * skipped, and with it a screenful of "not found in
					 * archives" in every bake log. */
					if ( !ts.material.isEmpty() && !ts.material.contains( QLatin1Char( ':' ) ) )
						feedStr( lodgenLedgerAssetDigest( dataRoot, ts.material, "materials", ".bgsm" ) );
				};
				for ( int q = 0; q < 4; q++ ) {
					feedI( land.baseTex[q] );
					feedLtex( land.baseTex[q] );
					feedI( land.layers[q].size() );
					for ( const EsmLandLayer & L : land.layers[q] ) {
						feedI( L.ltex );
						feedLtex( L.ltex );
						feed( QByteArray( reinterpret_cast<const char *>( L.opacity ),
							int( sizeof( L.opacity ) ) ) );
					}
				}
			}
			/* The refs, in FORM-ID order so the digest cannot depend on the
			 * order the parser happened to hand them back. */
			QVector<EsmRefr> refs = world.refrs( x, y );
			std::sort( refs.begin(), refs.end(),
				[]( const EsmRefr & a, const EsmRefr & b ) { return a.formID < b.formID; } );
			feedI( refs.size() );
			for ( const EsmRefr & r : refs ) {
				feedI( r.formID ); feedI( r.base ); feedI( r.baseType );
				feed( QByteArray( reinterpret_cast<const char *>( r.pos ), 12 ) );
				feed( QByteArray( reinterpret_cast<const char *>( r.rot ), 12 ) );
				feed( QByteArray( reinterpret_cast<const char *>( &r.scale ), 4 ) );
				feedI( r.initiallyDisabled ? 1 : 0 );
				feedI( r.deleted ? 1 : 0 );
				if ( r.initiallyDisabled || r.deleted )
					continue;
				const EsmLodBase & lb = world.lodBase( r.base );
				feedI( lb.hasLod ? 1 : 0 );
				for ( int k = 0; k < 4; k++ ) {
					feedStr( lb.models[k] );
					if ( !lb.models[k].isEmpty() )
						feedStr( lodgenLedgerAssetDigest( dataRoot, lb.models[k] ) );
				}
				/* A static collection's PARTS are inputs too: editing the SCOL
				 * changes the chunk without touching the REFR. */
				const QVector<EsmScolPart> & parts = world.scolParts( r.base );
				feedI( parts.size() );
				for ( const EsmScolPart & pt : parts ) {
					feedI( pt.base );
					feedI( pt.placements.size() );
					for ( const EsmScolPlacement & pl : pt.placements ) {
						feed( QByteArray( reinterpret_cast<const char *>( pl.pos ), 12 ) );
						feed( QByteArray( reinterpret_cast<const char *>( pl.rot ), 12 ) );
						feed( QByteArray( reinterpret_cast<const char *>( &pl.scale ), 4 ) );
					}
					const EsmLodBase & pb = world.lodBase( pt.base );
					for ( int k = 0; k < 4; k++ ) {
						feedStr( pb.models[k] );
						if ( !pb.models[k].isEmpty() )
							feedStr( lodgenLedgerAssetDigest( dataRoot, pb.models[k] ) );
					}
				}
			}
		}
	}
	return QString::fromLatin1( h.result().toHex() );
}

/* ---- the container ------------------------------------------------------
 * MOVED to src/lodbfile.cpp (lane BAKEREC1, 2026-09-17). The chunk INPUT digest
 * and the file digest above stay here, beside the bake that feeds them; the
 * .lodb container is now a plain-text BAKE RECORD and lives in its own file,
 * with the plugin lines, the corpus hashes, the switch vector and the census.
 * docs/LODGEN_BAKE_RECORD.md is the format. */

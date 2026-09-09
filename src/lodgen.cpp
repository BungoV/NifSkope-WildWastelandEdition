/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "lodgen.h"

#include "esmdata.h"
#include "io/material.h"
#include "io/lodmfile.h"
#include "io/lodvfile.h"
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
#include <QFileInfo>
#include <QMap>
#include <QVector>

#include <algorithm>
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

bool lodgenIsTreeModel( const QString & model )
{
	const int slash = qMax( model.lastIndexOf( QChar( '\\' ) ), model.lastIndexOf( QChar( '/' ) ) );
	const QString modelFile = model.mid( slash + 1 ).toLower();
	return model.contains( QLatin1String( "\\trees\\" ), Qt::CaseInsensitive )
		|| model.contains( QLatin1String( "/trees/" ), Qt::CaseInsensitive )
		|| modelFile.startsWith( QLatin1String( "tree" ) );
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
				 * difference as exactly this cutoff. */
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
					if ( !mp.startsWith( QStringLiteral( "materials/" ), Qt::CaseInsensitive ) )
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
						}
					}
				}
			}
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


/* ================= rung 3: per-placement AO bake ======================= */

namespace
{

/* CPU ambient-occlusion over the assembled chunk: a uniform XY grid of
 * triangle bins plus the terrain heightfield. Per vertex, a fixed cosine
 * hemisphere (rotated to the vertex normal) is sampled; ray hits against
 * nearby chunk geometry or the ground darken the vertex. This is the
 * per-PLACEMENT data no shared texture can carry — the reason the B channel
 * exists (docs/TO_BE_IMPLEMENTED.md). */
struct LodgenAoScene
{
	static constexpr int BINS = 64;
	/* The binned area and the heightfield may reach BEYOND the chunk, so the
	 * origin is explicit rather than assumed to be zero. With a bake skirt the
	 * chunk's own geometry sits in the middle of a larger field and skirt
	 * coordinates are negative on two sides. */
	float ox = 0.0f, oy = 0.0f;         // miniature position of the field origin
	float span = 4096.0f;               // miniature span of the BINNED area
	std::vector<float> tri;             // 9 floats per triangle
	std::vector<std::vector<int>> bins; // BINS*BINS triangle lists
	// terrain heightfield in miniature units (n x n), optional
	int hn = 0;
	float hSpacing = 1.0f;
	std::vector<float> hgt;

	void addTriangle( const Vector3 & a, const Vector3 & b, const Vector3 & c )
	{
		const int t = int( tri.size() / 9 );
		for ( const Vector3 * p : { &a, &b, &c } ) {
			tri.push_back( (*p)[0] );
			tri.push_back( (*p)[1] );
			tri.push_back( (*p)[2] );
		}
		if ( bins.empty() )
			bins.resize( BINS * BINS );
		const float mnx = qMin( a[0], qMin( b[0], c[0] ) ), mxx = qMax( a[0], qMax( b[0], c[0] ) );
		const float mny = qMin( a[1], qMin( b[1], c[1] ) ), mxy = qMax( a[1], qMax( b[1], c[1] ) );
		const int bx0 = qBound( 0, int( ( mnx - ox ) / span * BINS ), BINS - 1 );
		const int bx1 = qBound( 0, int( ( mxx - ox ) / span * BINS ), BINS - 1 );
		const int by0 = qBound( 0, int( ( mny - oy ) / span * BINS ), BINS - 1 );
		const int by1 = qBound( 0, int( ( mxy - oy ) / span * BINS ), BINS - 1 );
		for ( int by = by0; by <= by1; by++ )
			for ( int bx = bx0; bx <= bx1; bx++ )
				bins[by * BINS + bx].push_back( t );
	}

	float groundHeight( float x, float y ) const
	{
		if ( !hn )
			return -3.4e38f;
		const float fx = qBound( 0.0f, ( x - ox ) / hSpacing, float( hn - 1 ) - 0.001f );
		const float fy = qBound( 0.0f, ( y - oy ) / hSpacing, float( hn - 1 ) - 0.001f );
		const int ix = int( fx ), iy = int( fy );
		const float tx = fx - ix, ty = fy - iy;
		const float h00 = hgt[size_t( iy ) * hn + ix], h10 = hgt[size_t( iy ) * hn + ix + 1];
		const float h01 = hgt[size_t( iy + 1 ) * hn + ix], h11 = hgt[size_t( iy + 1 ) * hn + ix + 1];
		return ( h00 * ( 1 - tx ) + h10 * tx ) * ( 1 - ty )
			+ ( h01 * ( 1 - tx ) + h11 * tx ) * ty;
	}

	bool rayHit( const Vector3 & o, const Vector3 & d, float maxT ) const
	{
		// terrain: march and compare against the heightfield
		if ( hn && d[2] < 0.9f ) {
			for ( float t = 8.0f; t < maxT; t += 24.0f ) {
				const float x = o[0] + d[0] * t, y = o[1] + d[1] * t;
				if ( x < ox || y < oy || x > ox + span || y > oy + span )
					break;
				if ( o[2] + d[2] * t < groundHeight( x, y ) )
					return true;
			}
		}
		if ( bins.empty() )
			return false;
		// DDA over the XY bins
		const float cell = span / BINS;
		float t = 0.0f;
		int guard = 0;
		while ( t < maxT && guard++ < 2 * BINS ) {
			const float x = o[0] + d[0] * t, y = o[1] + d[1] * t;
			const int bx = int( ( x - ox ) / cell ), by = int( ( y - oy ) / cell );
			if ( bx < 0 || by < 0 || bx >= BINS || by >= BINS )
				break;
			for ( int ti : bins[by * BINS + bx] ) {
				const float * p = tri.data() + size_t( ti ) * 9;
				// Moller-Trumbore
				const Vector3 v0( p[0], p[1], p[2] ), v1( p[3], p[4], p[5] ), v2( p[6], p[7], p[8] );
				const Vector3 e1 = v1 - v0, e2 = v2 - v0;
				const Vector3 pv = Vector3::crossproduct( d, e2 );
				const float det = Vector3::dotproduct( e1, pv );
				if ( std::fabs( det ) < 1e-8f )
					continue;
				const float inv = 1.0f / det;
				const Vector3 tv = o - v0;
				const float u = Vector3::dotproduct( tv, pv ) * inv;
				if ( u < 0.0f || u > 1.0f )
					continue;
				const Vector3 qv = Vector3::crossproduct( tv, e1 );
				const float vv = Vector3::dotproduct( d, qv ) * inv;
				if ( vv < 0.0f || u + vv > 1.0f )
					continue;
				const float hitT = Vector3::dotproduct( e2, qv ) * inv;
				if ( hitT > 1.0f && hitT < maxT )
					return true;
			}
			// advance to the next bin boundary along the dominant axis
			const float step = cell / qMax( 0.05f,
				qMax( std::fabs( d[0] ), std::fabs( d[1] ) ) );
			t += step;
		}
		return false;
	}

	/*! Fraction of the UPPER hemisphere that reaches open sky.
	 *
	 *  Not ambient occlusion with a different name: AO is cosine-weighted about
	 *  the surface normal and answers "how enclosed is this point", while this
	 *  is normal-independent and answers "can weather and skylight land here".
	 *  A vertical wall face has low AO and high sky visibility; the floor of a
	 *  narrow gully has the reverse.
	 */
	float skyVisibility( const Vector3 & p, float maxT ) const
	{
		static const float dirs[9][3] = {
			{ 0.0f, 0.0f, 1.0f },
			{ 0.5f, 0.0f, 0.87f }, { -0.5f, 0.0f, 0.87f },
			{ 0.0f, 0.5f, 0.87f }, { 0.0f, -0.5f, 0.87f },
			{ 0.7f, 0.0f, 0.71f }, { -0.7f, 0.0f, 0.71f },
			{ 0.0f, 0.7f, 0.71f }, { 0.0f, -0.7f, 0.71f } };
		const Vector3 o = p + Vector3( 0.0f, 0.0f, 2.0f );
		int open = 0;
		for ( const auto & dv : dirs ) {
			Vector3 d( dv[0], dv[1], dv[2] );
			d.normalize();
			if ( !rayHit( o, d, maxT ) )
				open++;
		}
		return float( open ) / 9.0f;
	}

	float ambientOcclusion( const Vector3 & p, const Vector3 & n, float maxT ) const
	{
		// 8 fixed hemisphere directions blended toward the normal
		static const float dirs[8][3] = {
			{ 0.7f, 0.0f, 0.7f }, { -0.7f, 0.0f, 0.7f },
			{ 0.0f, 0.7f, 0.7f }, { 0.0f, -0.7f, 0.7f },
			{ 0.5f, 0.5f, 0.7f }, { -0.5f, 0.5f, 0.7f },
			{ 0.5f, -0.5f, 0.7f }, { -0.5f, -0.5f, 0.7f } };
		const Vector3 o = p + n * 2.0f;
		int hits = 0;
		for ( const auto & dv : dirs ) {
			Vector3 d( dv[0], dv[1], dv[2] );
			d = d + n * 0.6f;
			d.normalize();
			if ( Vector3::dotproduct( d, n ) < 0.05f )
				continue;
			if ( rayHit( o, d, maxT ) )
				hits++;
		}
		return 1.0f - 0.85f * float( hits ) / 8.0f;
	}
};

} // namespace


namespace
{
// defined with the texture-bake section below (same anonymous namespace)
bool lodgenWriteDds( const QString & path, int w, int h,
	const std::vector<quint32> & bgra, bool bc3 = false, int maxMips = 0,
	bool bc1Alpha = false, quint32 stamp0 = 0, quint32 stamp1 = 0 );

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
	//! What the MIP CAP divides, min over the two axes. It is the GAP under the
	//! law of 2026-09-09 -- a tap on a frame's UV border reads half of that
	//! frame's last texel and half of the neighbour's first, so what separates
	//! the two silhouettes at level k is gap / 2^k and the chain stops at the
	//! last level where that is still a whole texel -- and it is the PER-SIDE
	//! padding on a sidecar written under the reading before it, so those sheets
	//! still convert to exactly the chain they were built for.
	//! mips = 1 + log2(octMipUnit).
	int octMipUnit = 0;
	float octHalfW = 0, octHalfH = 0, octSpan = 0;
	Vector3 octCenter;
	bool octPbr = false;        // the set's family: pbr (_bc/_n/_rmaos) or legacy (_d/_n/_gsaos)
	// `emissiveScale`: what a consumer multiplies the emissive sheet by (the
	// meta's `emissive` line). 1 when a bake from before it says nothing.
	float octEmissiveScale = 1.0f;
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
		for ( int by = 0; by < bh; by++ )
			for ( int bx = 0; bx < bw; bx++ ) {
				quint8 * o = data.data() + at + ( size_t( by ) * bw + bx ) * 16;
				lodgenEncodeBC4Block( mr[m], mw, mh, bx, by, o );
				lodgenEncodeBC4Block( mg[m], mw, mh, bx, by, o + 8 );
			}
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
				card.octMipUnit = qMin( card.octPadX, card.octPadY );
			} else if ( line[0] == QLatin1String( "emissive" ) && line.size() >= 2 ) {
				// the set's emissive multiple; the colour is already in the sheet
				card.octEmissiveScale = line[1].toFloat();
			} else if ( line[0] == QLatin1String( "oct" ) ) {
				// a bake from before the families: its third sheet means something else
				fprintf( stderr, "lodgen: card %s: oct line without a family, rebake it (docs/LODGEN_IMPOSTOR_SPEC.md)\n",
					id.toLocal8Bit().constData() );
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
			card.texPath = QStringLiteral( "Data\\Textures\\Lodgen\\Cards\\" )
				+ id + QStringLiteral( "_fs.DDS" );
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
				const int w = alb.width(), h = alb.height();
				const QString base = dir + "/" + id + QStringLiteral( "_oct" );
				/* THE MIP CAP IS THE GAP'S (bungo, 2026-09-09: "enough pixel padding so
				 * that there's no mip map bleeding into other rows and columns", with
				 * the number named the same day as "8 pixels of distance between two
				 * rendered objects"). A frame's mips never mix ACROSS a border -- the
				 * box filter halves an even frame into an even frame -- but a reader
				 * sampling ON a frame's UV border takes half its value from the next
				 * frame, so what has to survive is the SEPARATION between the two
				 * silhouettes: gap / 2^k >= 1. Hence 1 + log2(min(gapX, gapY)) levels,
				 * and never the one after -- 4 on a 128-texel frame, which is the count
				 * the per-side reading also gave, because the count was always the
				 * gap's.
				 *
				 * `octMipUnit` is the gap on a sidecar that names one and the per-side
				 * padding on the two older kinds, each read under its own law. The last
				 * fallback -- neither line -- is the pre-2026-09-09 sheets' own
				 * max(4, longSide/16) per side, capped as those sheets were capped. */
				const int padFallback = qMax( 4, qMax( card.octTileW, card.octTileH ) / 16 );
				const int padX = card.octPadX > 0 ? card.octPadX : padFallback;
				const int padY = card.octPadY > 0 ? card.octPadY : padFallback;
				const int gapX = card.octGapX > 0 ? card.octGapX : 2 * padFallback;
				const int gapY = card.octGapY > 0 ? card.octGapY : 2 * padFallback;
				const int mipUnit = card.octMipUnit > 0 ? card.octMipUnit : padFallback;
				int frameMips = 1;
				for ( int g = mipUnit; g >= 2; g /= 2 )
					frameMips++;
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
				 * clean depth does too. The bake rounds the gap UP TO EVEN, so a halved
				 * frame still splits it into two whole texels down to gap 2; below that
				 * -- the 16- and 32-texel frames at --card-half-aux -- the division
				 * reaches 1 and the aux sheets ship a single level, which is the
				 * fallback naming itself rather than a silent bleed. */
				int auxMips = 1;
				for ( int g = mipUnit / auxDiv; g >= 2; g /= 2 )
					auxMips++;
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
						ok = lodgenWriteDds( path, aw, ah, pixels( *s.img ), true, auxMips ) && ok;
				}
				// the emissive sheet is BC1: RGB only, no alpha to carry
				const QString emSfx = QLatin1String( lodmEmissiveSuffix( card.octPbr ) ) + QStringLiteral( ".DDS" );
				if ( !emi.isNull() && !QFile::exists( base + emSfx ) ) {
					const QImage emiA = down( emi );
					ok = lodgenWriteDds( base + emSfx, aw, ah, pixels( emiA ), false, auxMips ) && ok;
				}
				const QString game = QStringLiteral( "Data\\Textures\\Lodgen\\Cards\\" ) + id + QStringLiteral( "_oct" );
				if ( ok ) {
					// the set's .lodm: family, the sheets, the frame grid (compact by design)
					QJsonObject root, tex, oc;
					root.insert( QStringLiteral( "lodm" ), 1 );
					root.insert( QStringLiteral( "family" ), card.octPbr ? QStringLiteral( "pbr" ) : QStringLiteral( "legacy" ) );
					root.insert( QStringLiteral( "kind" ), QStringLiteral( "card" ) );
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
					 * 1 + log2(min(gap)). A set that carries `pad` and no `gap` is a
					 * CARDFIT3 set whose count was 1 + log2(min(pad)). */
					oc.insert( QStringLiteral( "pad" ), QJsonArray{ padX, padY } );
					oc.insert( QStringLiteral( "gap" ), QJsonArray{ gapX, gapY } );
					oc.insert( QStringLiteral( "half" ), QJsonArray{ double( card.octHalfW ), double( card.octHalfH ) } );
					oc.insert( QStringLiteral( "center" ), QJsonArray{ double( card.octCenter[0] ), double( card.octCenter[1] ), double( card.octCenter[2] ) } );
					oc.insert( QStringLiteral( "depthSpan" ), double( card.octSpan ) );
					oc.insert( QStringLiteral( "mips" ), frameMips );
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
	const int skirt = ( opts.identity && opts.bakeAO ) ? qMax( 0, opts.aoSkirtCells ) : 0;
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
	int placed = 0, skippedNoLod = 0, cardsForMeshes = 0;

	for ( const LodPlacement & r : placements ) {
		const EsmLodBase & base = world.lodBase( r.base );
		if ( !base.hasLod )
			continue;
		QString model = base.models[qMin( lodLevel, 3 )];
		QVector<LodSrcShape> cardShapes;
		LodgenCard usedCard;		// a copy: the cache may move on a later insert
		/* A card stands in where the ring's slot is missing (it beats falling
		 * back to a heavier near-slot mesh) and, from opts.impostorFromLevel
		 * on, in place of the slot's mesh too: one quad per tree at the near
		 * rings, for a consumer that draws the octahedral sheets. */
		const bool cardWanted = !opts.impostorDir.isEmpty()
			&& ( model.isEmpty() || ( opts.impostorFromLevel >= 0 && lodLevel >= opts.impostorFromLevel ) );
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
		 * trees folder, and tree-prefixed model names only. */
		const int slash = qMax( model.lastIndexOf( QChar( '\\' ) ),
			model.lastIndexOf( QChar( '/' ) ) );
		const QString modelFile = model.mid( slash + 1 ).toLower();
		const bool isTree = std::memcmp( &base.type, "TREE", 4 ) == 0
			|| model.contains( QLatin1String( "\\trees\\" ), Qt::CaseInsensitive )
			|| model.contains( QLatin1String( "/trees/" ), Qt::CaseInsensitive )
			|| modelFile.startsWith( QLatin1String( "tree" ) );
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
		for ( const LodSrcShape & s : shapes ) {
			for ( const Vector3 & lp : s.pos ) {
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

		// identity channel: 16-bit per-chunk index in R+G
		Color4 idColor( 1, 1, 1, 1 );
		if ( opts.identity ) {
			idColor = Color4( float( objectIndex & 0xFF ) / 255.0f,
				float( ( objectIndex >> 8 ) & 0xFF ) / 255.0f, 1.0f, 1.0f );
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
		if ( opts.identity ) {
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
	 * listed identity indices and draw the model instanced instead. */
	if ( opts.identity ) {
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
	 * against the whole assembled chunk plus the terrain heightfield. */
	if ( opts.identity && opts.bakeAO ) {
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
				if ( opts.aoGrey )
					bucket.col[v].setRGBA( ao, ao, ao, bucket.col[v].alpha() );
				else
					bucket.col[v].setRGBA( bucket.col[v].red(), bucket.col[v].green(),
						ao, bucket.col[v].alpha() );
			}
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
	quint32 stamp0, quint32 stamp1 )
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
					( ( ( bc3 || bc1Alpha ) ? ( ( acc[3] + 2 ) >> 2 ) : 0xFFU ) << 24 )
					| ( ( ( acc[0] + 2 ) >> 2 ) << 16 ) | ( ( ( acc[1] + 2 ) >> 2 ) << 8 )
					| ( ( acc[2] + 2 ) >> 2 );
			}
		mips.push_back( std::move( next ) );
		mw = nw;
		mh = nh;
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
	mw = w;
	mh = h;
	for ( const std::vector<quint32> & mip : mips ) {
		const int bw = ( mw + 3 ) / 4, bh = ( mh + 3 ) / 4;
		std::vector<quint8> block( size_t( bw ) * bh * blockBytes );
		for ( int by = 0; by < bh; by++ ) {
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
		}
		f.write( reinterpret_cast<const char *>( block.data() ), qint64( block.size() ) );
		mw = qMax( 4, mw / 2 );
		mh = qMax( 4, mh / 2 );
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
		if ( !path.startsWith( QStringLiteral( "materials/" ), Qt::CaseInsensitive ) )
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
		for ( int by = 0; by < bh; by++ ) {
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
		}
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
					r = b8( smooth * sG ); g = b8( qMin( 1.0f, sR * mult ) ); bl = 255U;
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

bool lodgenBakeTerrainTextures( const EsmWorld & world, int chunkX, int chunkY,
	int dim, const QString & dataRoot, const QString & outDir,
	const LodgenCoverOptions & coverOpts, LodgenBakeCaches * caches, QString * error )
{
	auto fail = [error]( const QString & message ) {
		if ( error )
			*error = message;
		return false;
	};
	constexpr int RES = 512;
	// world-space tiling of the source landscape textures; near-terrain
	// repeats roughly every half cell (calibration against vanilla bakes is
	// an open refinement — the constant only affects apparent texel density)
	constexpr float TILE = 2048.0f;
	const float span = float( dim ) * 4096.0f;
	const float cwX = float( chunkX ) * 4096.0f, cwY = float( chunkY ) * 4096.0f;

	// per-cell land data, loaded once
	std::vector<EsmLand> cells( size_t( dim ) * dim );
	std::vector<bool> haveLand( size_t( dim ) * dim, false );
	const int hn = dim * 32 + 1;
	std::vector<float> hgt( size_t( hn ) * hn, world.defaultLandHeight() );
	for ( int cy = 0; cy < dim; cy++ ) {
		for ( int cx = 0; cx < dim; cx++ ) {
			EsmLand & land = cells[size_t( cy ) * dim + cx];
			if ( world.land( chunkX + cx, chunkY + cy, land ) ) {
				haveLand[size_t( cy ) * dim + cx] = true;
				for ( int row = 0; row < 33; row++ )
					for ( int col = 0; col < 33; col++ )
						hgt[size_t( cy * 32 + row ) * hn + size_t( cx * 32 + col )] =
							land.heights[row][col];
			}
		}
	}

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
			const float ngx = ( wx - cwX ) / span * float( hn - 1 );
			const float ngy = ( wy - cwY ) / span * float( hn - 1 );
			const float spacing = span / float( hn - 1 );
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
			const float dzdx = ( lodgenTerrainHeightAt( hgt, hn, ngx + 1.0f, ngy )
				- lodgenTerrainHeightAt( hgt, hn, ngx - 1.0f, ngy ) ) / ( 2.0f * spacing );
			const float dzdy = ( lodgenTerrainHeightAt( hgt, hn, ngx, ngy + 1.0f )
				- lodgenTerrainHeightAt( hgt, hn, ngx, ngy - 1.0f ) ) / ( 2.0f * spacing );
			Vector3 nrm( -dzdx, -dzdy, 1.0f );
			nrm.normalize();
			msn[size_t( py ) * RES + px] = lodgenTerrainMsnPixel( nrm );

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
					// wrap by hand: getPixelB clamps, and the tiling is ours
					float u = std::fmod( wx / TILE, 1.0f );
					float v = std::fmod( wy / TILE, 1.0f );
					if ( u < 0.0f ) u += 1.0f;
					if ( v < 0.0f ) v += 1.0f;
					/* getPixelB/T take NORMALIZED 0..1 coordinates. Sample at
					 * the mip whose texel matches the bake texel's WORLD
					 * footprint (span/RES units), or the result is tiling
					 * noise instead of the material's local average. */
					const float texelWorld = TILE / float( tex->getWidth() );
					const float footprint = span / float( RES );
					const float mip = qBound( 0.0f,
						std::log2( qMax( 1.0f, footprint / texelWorld ) ),
						float( tex->getMaxMipLevel() ) );
					return tex->getPixelT( u, v, mip );
				};
				const quint32 baseTex = land.baseTex[q] ? land.baseTex[q] : dominantBase;
				if ( baseTex )
					color = sampleLtex( baseTex );
				else
					statNoBase++;
				int nLayers = 0;
				for ( const EsmLandLayer & layer : land.layers[q] ) {
					// bilinear over the 17x17 quadrant opacities
					const float fx = qBound( 0.0f, qx * 16.0f, 15.999f );
					const float fy = qBound( 0.0f, qy * 16.0f, 15.999f );
					const int ix = int( fx ), iy = int( fy );
					const float tx = fx - ix, ty = fy - iy;
					const float a =
						( layer.opacity[iy][ix] * ( 1 - tx ) + layer.opacity[iy][ix + 1] * tx ) * ( 1 - ty )
						+ ( layer.opacity[iy + 1][ix] * ( 1 - tx ) + layer.opacity[iy + 1][ix + 1] * tx ) * ty;
					// the cover side sees EVERY layer's opacity, including the
					// ones the colour composite skips as negligible
					if ( doCover && nLayers < int( aCover.size() ) )
						aCover[nLayers] = qBound( 0.0f, a, 1.0f );
					nLayers++;
					if ( a <= 0.001f )
						continue;
					// NULL-texture layers paint the engine's hardcoded default
					// ground; the chunk's dominant base is the local stand-in
					const FloatVector4 lc = sampleLtex(
						layer.ltex ? layer.ltex : dominantBase );
					color = color + ( lc - color ) * qBound( 0.0f, a, 1.0f );
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
		kvi( "pxNoLand", statNoLand );
		kvi( "pxNoBase", statNoBase );
		kvi( "pxUnresolvableLtex", statNoTex );
		kv( "unresolvableLtexIds", QChar( '[' ) + idList( failedLtex ) + QChar( ']' ) );
		fprintf( stderr, "%s\n", line.toLatin1().constData() );
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
	if ( !lodgenWriteDds( base + QStringLiteral( ".DDS" ), RES, RES, diffuse ) )
		return fail( QStringLiteral( "could not write the diffuse bake" ) );
	if ( !lodgenWriteDds( base + QStringLiteral( "_msn.DDS" ), RES, RES, msn ) )
		return fail( QStringLiteral( "could not write the normal bake" ) );

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
		// bilinear sample of the chunk heightfield, in chunk-local world units
		auto heightAt = [&]( float wx, float wy ) {
			const float fx = qBound( 0.0f, wx / 128.0f, float( hn - 1 ) );
			const float fy = qBound( 0.0f, wy / 128.0f, float( hn - 1 ) );
			const int x0 = int( fx ), y0 = int( fy );
			const int x1 = qMin( x0 + 1, hn - 1 ), y1 = qMin( y0 + 1, hn - 1 );
			const float tx = fx - float( x0 ), ty = fy - float( y0 );
			const float a = hgt[size_t( y0 ) * hn + x0], bb = hgt[size_t( y0 ) * hn + x1];
			const float c = hgt[size_t( y1 ) * hn + x0], d = hgt[size_t( y1 ) * hn + x1];
			return ( a + ( bb - a ) * tx ) + ( ( c + ( d - c ) * tx )
				- ( a + ( bb - a ) * tx ) ) * ty;
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
		lodgenTerrainChannels( world, chunkX, chunkY, dim, hgt,
			tMat, tWet, tAo2, tSky, tMat2, tShore, &tBlend );
		auto sampleF = [&]( const std::vector<float> & f, float wx, float wy ) {
			const float fx = qBound( 0.0f, wx / 128.0f, float( hn - 1 ) );
			const float fy = qBound( 0.0f, wy / 128.0f, float( hn - 1 ) );
			const int x0 = int( fx ), y0 = int( fy );
			const int x1 = qMin( x0 + 1, hn - 1 ), y1 = qMin( y0 + 1, hn - 1 );
			const float tx = fx - float( x0 ), ty = fy - float( y0 );
			const float a = f[size_t( y0 ) * hn + x0], bb = f[size_t( y0 ) * hn + x1];
			const float c = f[size_t( y1 ) * hn + x0], d = f[size_t( y1 ) * hn + x1];
			const float top = a + ( bb - a ) * tx, bot = c + ( d - c ) * tx;
			return top + ( bot - top ) * ty;
		};
		auto sampleU8 = [&]( const std::vector<quint8> & f, float wx, float wy ) {
			const float fx = qBound( 0.0f, wx / 128.0f, float( hn - 1 ) );
			const float fy = qBound( 0.0f, wy / 128.0f, float( hn - 1 ) );
			const int x0 = int( fx ), y0 = int( fy );
			const int x1 = qMin( x0 + 1, hn - 1 ), y1 = qMin( y0 + 1, hn - 1 );
			const float tx = fx - float( x0 ), ty = fy - float( y0 );
			const float a = f[size_t( y0 ) * hn + x0], bb = f[size_t( y0 ) * hn + x1];
			const float c = f[size_t( y1 ) * hn + x0], d = f[size_t( y1 ) * hn + x1];
			const float top = a + ( bb - a ) * tx, bot = c + ( d - c ) * tx;
			return top + ( bot - top ) * ty;
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
				const quint32 ao8 = quint32( qBound( 0.0f, vis * 255.0f + 0.5f, 255.0f ) );
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
 * than a shared function asserting it.
 *
 * Three things the tile path does that the chunk path cannot:
 *   - it bakes a one-cell RING around every tile, so the border, the msn's
 *     central differences and the 2,048-unit AO march all have real data
 *     instead of the chunk path's edge clamp;
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
	std::vector<quint32> data;
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
	for ( int by = 0; by < bh; by++ ) {
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
	}
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
QByteArray lodgenVtEncodeTile( const LodgenVtStage & st, int stored, int mips, bool withHeight )
{
	QByteArray out;
	struct SheetSrc { const std::vector<quint32> * px; bool bc3; bool alpha; };
	std::vector<quint32> dataOpaque;
	const std::vector<quint32> * dataPx = &st.data;
	if ( !st.cover ) {
		dataOpaque = st.data;
		for ( quint32 & v : dataOpaque )
			v |= 0xFF000000U;
		dataPx = &dataOpaque;
	}
	const SheetSrc sheets[3] = {
		{ &st.colour, false, false },
		{ &st.msn, false, false },
		{ dataPx, st.cover, st.cover }
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
bool lodgenBakeVtTile( const EsmWorld & world, const QString & dataRoot,
	LodgenBakeCaches & bc, const LodgenCoverOptions & coverOpts,
	LodgenVtLandCache & landCache, int cellX0, int cellY0, int dim,
	int content, int border, LodgenVtStage & out )
{
	const int S = content + 2 * border;
	const float upt = float( dim ) * 4096.0f / float( content );
	const int rdim = dim + 2;
	const int rx0 = cellX0 - 1, ry0 = cellY0 - 1;
	const int hn = rdim * 32 + 1;
	constexpr float TILE = 2048.0f;

	std::vector<EsmLand> cells( size_t( rdim ) * rdim );
	std::vector<bool> haveLand( size_t( rdim ) * rdim, false );
	std::vector<float> hgt( size_t( hn ) * hn, world.defaultLandHeight() );
	for ( int cy = 0; cy < rdim; cy++ ) {
		for ( int cx = 0; cx < rdim; cx++ ) {
			const EsmLand * l = landCache.get( world, rx0 + cx, ry0 + cy );
			if ( !l )
				continue;
			// copied out at once: the cache's pointer dies on the next get()
			cells[size_t( cy ) * rdim + cx] = *l;
			haveLand[size_t( cy ) * rdim + cx] = true;
			for ( int row = 0; row < 33; row++ )
				for ( int col = 0; col < 33; col++ )
					hgt[size_t( cy * 32 + row ) * hn + size_t( cx * 32 + col )] =
						cells[size_t( cy ) * rdim + cx].heights[row][col];
		}
	}

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

	auto heightAt = [&]( float lx, float ly ) {
		const float fx = qBound( 0.0f, lx / 128.0f, float( hn - 1 ) );
		const float fy = qBound( 0.0f, ly / 128.0f, float( hn - 1 ) );
		const int x0 = int( fx ), y0 = int( fy );
		const int x1 = qMin( x0 + 1, hn - 1 ), y1 = qMin( y0 + 1, hn - 1 );
		const float tx = fx - float( x0 ), ty = fy - float( y0 );
		const float a = hgt[size_t( y0 ) * hn + x0], bb = hgt[size_t( y0 ) * hn + x1];
		const float c = hgt[size_t( y1 ) * hn + x0], d = hgt[size_t( y1 ) * hn + x1];
		const float top = a + ( bb - a ) * tx, bot = c + ( d - c ) * tx;
		return top + ( bot - top ) * ty;
	};
	auto sampleU8 = [&]( const std::vector<quint8> & f, float lx, float ly ) {
		const float fx = qBound( 0.0f, lx / 128.0f, float( hn - 1 ) );
		const float fy = qBound( 0.0f, ly / 128.0f, float( hn - 1 ) );
		const int x0 = int( fx ), y0 = int( fy );
		const int x1 = qMin( x0 + 1, hn - 1 ), y1 = qMin( y0 + 1, hn - 1 );
		const float tx = fx - float( x0 ), ty = fy - float( y0 );
		const float a = f[size_t( y0 ) * hn + x0], bb = f[size_t( y0 ) * hn + x1];
		const float c = f[size_t( y1 ) * hn + x0], d = f[size_t( y1 ) * hn + x1];
		const float top = a + ( bb - a ) * tx, bot = c + ( d - c ) * tx;
		return top + ( bot - top ) * ty;
	};
	static const float dirs[8][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
		{ 0.7071f, 0.7071f }, { 0.7071f, -0.7071f },
		{ -0.7071f, 0.7071f }, { -0.7071f, -0.7071f } };

	out.colour.assign( size_t( S ) * S, 0xFF808080U );
	out.msn.assign( size_t( S ) * S, LODGEN_MSN_FLAT );
	out.data.assign( size_t( S ) * S, 0x00FFFFFFU );
	out.height.assign( size_t( S ) * S, 32767 );
	out.cover = false;

	const float tileW = float( cellX0 ) * 4096.0f;
	const float tileN = float( cellY0 + dim ) * 4096.0f;
	const float ringW = float( rx0 ) * 4096.0f;
	const float ringS = float( ry0 ) * 4096.0f;

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
			const float dzdx = ( lodgenTerrainHeightAt( hgt, hn, ngx + 1.0f, ngy )
				- lodgenTerrainHeightAt( hgt, hn, ngx - 1.0f, ngy ) ) / 256.0f;
			const float dzdy = ( lodgenTerrainHeightAt( hgt, hn, ngx, ngy + 1.0f )
				- lodgenTerrainHeightAt( hgt, hn, ngx, ngy - 1.0f ) ) / 256.0f;
			Vector3 nrm( -dzdx, -dzdy, 1.0f );
			nrm.normalize();
			out.msn[size_t( j ) * S + i] = lodgenTerrainMsnPixel( nrm );

			FloatVector4 color( 0.5f, 0.5f, 0.5f, 1.0f );
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
				auto sampleLtex = [&]( quint32 ltex ) -> FloatVector4 {
					QString d, n;
					world.ltexTextures( ltex, d, n );
					const DDSTexture16 * tex = d.isEmpty() ? nullptr
						: lodgenCachedTexture( bc, dataRoot, d );
					if ( !tex )
						return FloatVector4( 0.5f, 0.5f, 0.5f, 1.0f );
					float u = std::fmod( wx / TILE, 1.0f );
					float v = std::fmod( wy / TILE, 1.0f );
					if ( u < 0.0f ) u += 1.0f;
					if ( v < 0.0f ) v += 1.0f;
					const float texelWorld = TILE / float( tex->getWidth() );
					const float mip = qBound( 0.0f,
						std::log2( qMax( 1.0f, upt / texelWorld ) ),
						float( tex->getMaxMipLevel() ) );
					return tex->getPixelT( u, v, mip );
				};
				const quint32 baseTex = land.baseTex[q] ? land.baseTex[q] : dominantBase;
				if ( baseTex )
					color = sampleLtex( baseTex );
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
					const FloatVector4 lc = sampleLtex( layer.ltex ? layer.ltex : dominantBase );
					color = color + ( lc - color ) * qBound( 0.0f, a, 1.0f );
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
				const float tintW = ( float( coverByte ) / 255.0f ) * coverOpts.tintStrength;
				if ( tintW > 0.0f && coverTintD > 0.0f )
					for ( int k = 0; k < 3; k++ )
						color[k] = color[k] + ( coverTint[k] - color[k] ) * tintW;
			}
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
			const quint32 ao8 = quint32( qBound( 0.0f, vis * 255.0f + 0.5f, 255.0f ) );
			const quint32 wet8 = quint32( qBound( 0.0f, sampleU8( tWet, lx, ly ) + 0.5f, 255.0f ) );
			const quint32 sho8 = quint32( qBound( 0.0f, sampleU8( tShore, lx, ly ) + 0.5f, 255.0f ) );
			out.data[size_t( j ) * S + i] =
				( quint32( coverByte ) << 24 ) | ( ao8 << 16 ) | ( wet8 << 8 ) | sho8;

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
	int tx, int ty, LodgenVtStage & out )
{
	const int mosW = childTilesX * content, mosH = childTilesY * content;
	auto sample = [&]( int u, int v, quint32 * bgra3, quint16 * h16 ) {
		u = qBound( 0, u, mosW - 1 );
		v = qBound( 0, v, mosH - 1 );
		const int ctx = u / content, cty = v / content;
		auto row = childRows.constFind( cty );
		if ( row == childRows.constEnd() || ctx >= int( row->size() ) ) {
			bgra3[0] = 0xFF808080U;
			bgra3[1] = LODGEN_MSN_FLAT;
			bgra3[2] = 0x00FFFFFFU;
			*h16 = 32767;
			return;
		}
		const LodgenVtStage & st = ( *row )[size_t( ctx )];
		const size_t idx = size_t( border + v % content ) * size_t( stored )
			+ size_t( border + u % content );
		bgra3[0] = st.colour[idx];
		bgra3[1] = st.msn[idx];
		bgra3[2] = st.data[idx];
		*h16 = st.height[idx];
	};

	out.colour.assign( size_t( stored ) * stored, 0xFF808080U );
	out.msn.assign( size_t( stored ) * stored, LODGEN_MSN_FLAT );
	out.data.assign( size_t( stored ) * stored, 0x00FFFFFFU );
	out.height.assign( size_t( stored ) * stored, 32767 );
	out.cover = false;

	for ( int j = 0; j < stored; j++ ) {
		const int v0 = 2 * ( ty * content + j - border );
		for ( int i = 0; i < stored; i++ ) {
			const int u0 = 2 * ( tx * content + i - border );
			quint32 acc[3][4] = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };
			quint32 hAcc = 0;
			for ( int dv = 0; dv < 2; dv++ ) {
				for ( int du = 0; du < 2; du++ ) {
					quint32 px[3];
					quint16 hv = 0;
					sample( u0 + du, v0 + dv, px, &hv );
					for ( int s = 0; s < 3; s++ ) {
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
				out.msn[o] = lodgenTerrainMsnPixel( Vector3( n[0], n[1], n[2] ) );
			}
			// cover averages plainly: it is linear in composition, so the mean
			// of four cover bytes IS the cover of the union at half resolution
			const quint32 cov = ( acc[2][3] + 2 ) >> 2;
			out.data[o] = ( cov << 24 ) | ( ( ( acc[2][0] + 2 ) >> 2 ) << 16 )
				| ( ( ( acc[2][1] + 2 ) >> 2 ) << 8 ) | ( ( acc[2][2] + 2 ) >> 2 );
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
static qint64 lodgenVtTileBytes( int stored, int mips, bool coverTile, bool withHeight )
{
	qint64 n = 0;
	for ( int m = 0; m < mips; m++ ) {
		const qint64 s = stored >> m;
		const qint64 blocks = ( s / 4 ) * ( s / 4 );
		n += blocks * 8;                            // colour, BC1
		n += blocks * 8;                            // msn, BC1
		n += blocks * ( coverTile ? 16 : 8 );       // data, BC3 only with cover
		if ( withHeight )
			n += s * s * 2;                         // height, R16 uncompressed
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
	const qint64 rawNoCover = lodgenVtTileBytes( stored, opts.mips, false, opts.height );
	const qint64 rawCover = lodgenVtTileBytes( stored, opts.mips, true, opts.height );
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
	const QString dir = outDir + QStringLiteral( "/Terrain" );
	if ( !QDir().mkpath( dir ) )
		return fail( QString( "could not create %1" ).arg( dir ) );

	const quint64 vhgtHash = world.vhgtCorpusHash();
	const quint64 paintHash = world.paintCorpusHash();

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
		h.sheetCount = opts.height ? 4 : 3;
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
		h.sheets[0] = { LODV_DXGI_BC1_UNORM, LODV_DXGI_BC1_UNORM, LODV_ROLE_COLOR, 1 };
		h.sheets[1] = { LODV_DXGI_BC1_UNORM, LODV_DXGI_BC1_UNORM, LODV_ROLE_MSN, 0 };
		h.sheets[2] = { LODV_DXGI_BC1_UNORM, LODV_DXGI_BC3_UNORM, LODV_ROLE_DATA, 0 };
		if ( opts.height )
			h.sheets[3] = { LODV_DXGI_R16_UNORM, LODV_DXGI_R16_UNORM, LODV_ROLE_HEIGHT, 0 };
		const QString path = QString( "%1/%2.VT.%3.lodt" ).arg( dir ).arg( ws ).arg( levels[l].dim );
		auto w = std::make_unique<LodvWriter>();
		QString werr;
		if ( !w->begin( path, h, &werr ) )
			return fail( werr );
		writers.push_back( std::move( w ) );
		paths.push_back( path );
	}

	LodgenVtLandCache landCache;
	// static_cast, not size_t(...): the functional cast is a most vexing parse here
	// and would declare a function taking an unnamed size_t.
	std::vector<QMap<int, std::vector<LodgenVtStage>>> rings( static_cast<size_t>( nLevels ) );
	std::vector<int> nextParent( size_t( nLevels ), 0 );
	qint64 presentTotal = 0, coverTiles = 0;

	if ( opts.cover.cover )
		world.setGrassTintResolver( &lodgenGrassTintResolve, &bc );

	auto writeTile = [&]( int lv, const LodgenVtStage & st ) -> bool {
		const QByteArray raw = lodgenVtEncodeTile( st, stored, mips, opts.height );
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
			if ( !lodgenWriteDds( base + QStringLiteral( ".DDS" ), RES, RES, col )
				|| !lodgenWriteDds( base + QStringLiteral( "_msn.DDS" ), RES, RES, nrm )
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
					content, border, stored, tx, p, prow[size_t( tx )] );
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
			if ( !lodgenBakeVtTile( world, dataRoot, bc, opts.cover, landCache,
				cellX0, cellY0, levels[0].dim, content, border, row[size_t( tx )] ) )
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
		root.insert( QStringLiteral( "family" ), QStringLiteral( "legacy" ) );
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
		sheets.append( sheet( "color", 71, 71, "sRGB", "RGB albedo, grass tint folded in" ) );
		sheets.append( sheet( "msn", 71, 71, "linear", "model-space normal, 0.5+0.5 encoded" ) );
		sheets.append( sheet( "data", 71, 77, "linear",
			"R sky-free AO, G flow wetness, B shore proximity, A ground cover" ) );
		sheets.append( sheet( "height", 56, 56, "linear",
			"R16_UNORM, height/8 + 32767, the shadow heightmap's own encoding" ) );
		t.insert( QStringLiteral( "sheets" ), sheets );
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
			o.insert( QStringLiteral( "container" ),
				QString( "Terrain%1%2.VT.%3.lodt" ).arg( QChar( 92 ) ).arg( ws ).arg( levels[l].dim ) );
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
		r << QString( "finest %1" ).arg( levels[0].dim );
		r << QString( "coarsest %1" ).arg( levels[nLevels - 1].dim );
		r << QString( "content %1" ).arg( content );
		r << QString( "border %1" ).arg( border );
		r << QString( "mips %1" ).arg( mips );
		r << QString( "compression %1" ).arg( opts.compression );
		auto hex16 = []( quint64 v ) {
			return QStringLiteral( "0x" )
				+ QString::number( v, 16 ).toUpper().rightJustified( 16, QChar( '0' ) );
		};
		r << QString( "vhgtCorpusHash %1" ).arg( hex16( vhgtHash ) );
		r << QString( "paintCorpusHash %1" ).arg( hex16( paintHash ) );
		r << QString( "vhgtCorpusHashSorted %1" ).arg( hex16( world.vhgtCorpusHashSorted() ) );
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
	struct Layer { QString id, lodmGame, source; float halfW = 0, halfH = 0, span = 0, emissiveScale = 1.0f; Vector3 center; };
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
		 * Three vintages, each read under the law it was written under: `gap` present
		 * (2026-09-09, the gap law, mips = 1 + log2(min(gap))); `pad` alone (lane
		 * CARDFIT3, per-side, mips = 1 + log2(min(pad))); neither (older still,
		 * max(4, longSide/16) per side under that same per-side law). */
		const QJsonArray padA = card.value( QStringLiteral( "pad" ) ).toArray();
		const QJsonArray gapA = card.value( QStringLiteral( "gap" ) ).toArray();
		const int padFallback = qMax( 4, qMax( fw, fh ) / 16 );
		const int padX = padA.size() == 2 ? padA[0].toInt() : padFallback;
		const int padY = padA.size() == 2 ? padA[1].toInt() : padFallback;
		const int gapX = gapA.size() == 2 ? gapA[0].toInt() : 2 * padX;
		const int gapY = gapA.size() == 2 ? gapA[1].toInt() : 2 * padY;
		const int mipUnit = gapA.size() == 2 ? qMin( gapX, gapY ) : qMin( padX, padY );
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
		// the clean depth: a whole texel of GAP between the two silhouettes that meet
		// on a border, at the deepest level sampled
		g.mips = 1;
		for ( int g2 = mipUnit; g2 >= 2; g2 /= 2 )
			g.mips++;
		/* The three sheets that are not the base colour come down by auxDiv,
		 * AFTER the dilation just above - the gutter keeps a halving from
		 * mixing across a frame border. They become their own arrays at their
		 * own size; a layer index still means the same set in all four. */
		g.aw = qMax( 4, g.w / auxDiv );
		g.ah = qMax( 4, g.h / auxDiv );
		g.auxMips = 1;
		for ( int g2 = mipUnit / auxDiv; g2 >= 2; g2 /= 2 )
			g.auxMips++;
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
		// the set's emissive multiple travels with its layer, like its geometry
		l.emissiveScale = lm.emissiveScale;
		g.layers.append( l );
		g.color.push_back( pixels( alb ) );
		g.n.push_back( pixels( nrm ) );
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

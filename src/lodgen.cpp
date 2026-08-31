/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "lodgen.h"

#include "esmdata.h"
#include "io/material.h"
#include "model/nifmodel.h"
#include "spells/blocks.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "meshoptimizer/src/meshoptimizer.h"

/* Measured constants from Commonwealth.4.-20.24.BTR — see
 * docs/LODGEN_ESM_LAYOUTS.md and the 2026-08-31 dumps. The Land vertex
 * descriptor is vanilla's: flags VERTEX|UV, 12-byte stride (half3 position,
 * half bitangent X, half2 UV). Water's is position-only, 8 bytes. */
namespace
{

constexpr std::uint64_t LAND_VERTEX_DESC = 52776558133763ULL;   // 0x300000000303
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
	idx.clear();
	idx.reserve( size_t( n - 1 ) * ( n - 1 ) * 6 );
	for ( int row = 0; row < n - 1; row++ )
		for ( int col = 0; col < n - 1; col++ ) {
			const unsigned int a = (unsigned int) ( row * n + col );
			idx.push_back( a ); idx.push_back( a + 1 ); idx.push_back( a + n + 1 );
			idx.push_back( a ); idx.push_back( a + n + 1 ); idx.push_back( a + n );
		}
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
	std::vector<float> & skyVis, std::vector<quint8> & matClass2 )
{
	const int n = dim * 32 + 1;
	matClass.assign( size_t( n ) * n, 0 );
	wetness.assign( size_t( n ) * n, 0 );
	ao.assign( size_t( n ) * n, 255 );
	skyVis.assign( size_t( n ) * n, 1.0f );
	matClass2.assign( size_t( n ) * n, 0 );

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
				}
			}
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
	idx.reserve( size_t( n - 1 ) * ( n - 1 ) * 6 );
	for ( int row = 0; row < n - 1; row++ ) {
		for ( int col = 0; col < n - 1; col++ ) {
			const unsigned int a = (unsigned int) ( row * n + col );
			idx.push_back( a ); idx.push_back( a + 1 ); idx.push_back( a + n + 1 );
			idx.push_back( a ); idx.push_back( a + n + 1 ); idx.push_back( a + n );
		}
	}
	if ( opts.targetTrisPerCell > 0 ) {
		// per-CHUNK budget calibrated at dim 4 — see lodgenLandGeometry
		const size_t targetIdx = size_t( opts.targetTrisPerCell ) * 16 * 3;
		std::vector<unsigned int> simplified( idx.size() );
		float resultError = 0.0f;
		/* No border lock: vanilla decimates its border too — cracks between
		 * neighbouring chunks are exactly what the skirt exists to hide. */
		const size_t count = meshopt_simplify( simplified.data(), idx.data(), idx.size(),
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

	std::vector<quint8> tMat, tWet, tAo, tMat2;
	std::vector<float> tSky;
	if ( opts.terrainIdentity )
		lodgenTerrainChannels( world, chunkX, chunkY, dim, grid,
			tMat, tWet, tAo, tSky, tMat2 );

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
			nif->set<ByteColor4>( row, "Vertex Colors", ByteColor4( FloatVector4(
				float( tMat[s] ) / 255.0f, float( tWet[s] ) / 255.0f,
				float( tAo[s] ) / 255.0f, 1.0f ) ) );
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
				const int quads = it.value().size();
				nif->set<quint32>( iWater, "Num Vertices", quint32( quads * 4 ) );
				nif->set<quint32>( iWater, "Num Triangles", quint32( quads * 2 ) );
				nif->set<quint32>( iWater, "Data Size",
					quint32( quads * 4 * 8 + quads * 2 * 6 ) );

				nif->setState( BaseModel::Processing );
				QModelIndex iWV = nif->getIndex( iWater, "Vertex Data" );
				nif->updateArraySize( iWV );
				QVector<Triangle> wtris;
				const float cellSpan = 4096.0f * 32.0f * invDim / 32.0f; // 4096/dim... cell span in miniature
				float wMinX = 3.4e38f, wMinY = 3.4e38f, wMaxX = -3.4e38f, wMaxY = -3.4e38f;
				/* segment layout: dim*dim segments in cell row-major order;
				 * each wet cell's quad lands in its own segment, dry cells
				 * get empty segments — mirroring vanilla's per-cell hiding */
				QVector<QPair<int, int>> segPrims( dim * dim, qMakePair( 0, 0 ) );
				int vBase = 0, tBase = 0;
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
					wtris.append( Triangle( quint16( vBase ), quint16( vBase + 1 ), quint16( vBase + 2 ) ) );
					wtris.append( Triangle( quint16( vBase ), quint16( vBase + 2 ), quint16( vBase + 3 ) ) );
					segPrims[cell.second * dim + cell.first] = qMakePair( tBase, 2 );
					vBase += 4;
					tBase += 2;
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
constexpr std::uint64_t OBJ_VERTEX_DESC = 474989027590661ULL;       // 0x1B00000650405
constexpr std::uint64_t OBJ_VERTEX_DESC_COLORS = 1037939064898054ULL; // 0x3B00000650406

struct LodSrcShape
{
	QVector<Vector3> pos, nrm, tan;
	QVector<Vector2> uv;
	QVector<Color4> col;
	QVector<Triangle> tris;
	QString tex0, tex1;
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
	if ( src.loadFromFile( dataRoot + "/" + path ) ) {
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
				s.alphaThreshold = quint8( src.get<int>( iAlpha, "Threshold" ) );
			}
			QModelIndex iShader = src.getBlockIndex(
				src.getLink( iShape, "Shader Property" ) );
			if ( iShader.isValid() ) {
				QModelIndex iTexSet = src.getBlockIndex(
					src.getLink( iShader, "Texture Set" ) );
				if ( iTexSet.isValid() ) {
					QModelIndex iArr = src.getIndex( iTexSet, "Textures" );
					if ( iArr.isValid() ) {
						s.tex0 = src.get<QString>( src.getIndex( iArr, 0 ) );
						s.tex1 = src.get<QString>( src.getIndex( iArr, 1 ) );
					}
				}
			}
			if ( !s.tris.isEmpty() )
				shapes.append( s );
		}
	}
	return *cache.insert( key, shapes );
}

struct ObjBucket
{
	QVector<Vector3> pos, nrm, tan;
	QVector<Vector2> uv;
	QVector<Color4> col;
	// triangles grouped per cell for the dim4 segment split
	QVector<QVector<Triangle>> cellTris;
	QString tex0, tex1;
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
	float span = 4096.0f;               // miniature chunk span
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
		const int bx0 = qBound( 0, int( mnx / span * BINS ), BINS - 1 );
		const int bx1 = qBound( 0, int( mxx / span * BINS ), BINS - 1 );
		const int by0 = qBound( 0, int( mny / span * BINS ), BINS - 1 );
		const int by1 = qBound( 0, int( mxy / span * BINS ), BINS - 1 );
		for ( int by = by0; by <= by1; by++ )
			for ( int bx = bx0; bx <= bx1; bx++ )
				bins[by * BINS + bx].push_back( t );
	}

	float groundHeight( float x, float y ) const
	{
		if ( !hn )
			return -3.4e38f;
		const float fx = qBound( 0.0f, x / hSpacing, float( hn - 1 ) - 0.001f );
		const float fy = qBound( 0.0f, y / hSpacing, float( hn - 1 ) - 0.001f );
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
				if ( x < 0 || y < 0 || x > span || y > span )
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
			const int bx = int( x / cell ), by = int( y / cell );
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
	const std::vector<quint32> & bgra );

struct LodgenCard
{
	bool valid = false;
	QString texPath;            // game path for the texture set
	float halfW = 0, halfH = 0;
	Vector3 center;             // model-space centre of the photographed bound
};

//! Look up (and lazily DDS-convert) an impostor card for a base form.
const LodgenCard & lodgenCard( const QString & dir, quint32 formID,
	QHash<quint32, LodgenCard> & cache )
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
		const QStringList line = QString::fromLatin1( meta.readLine() )
			.split( QChar( ' ' ), Qt::SkipEmptyParts );
		if ( line.size() >= 6 ) {
			card.halfW = line[1].toFloat();
			card.halfH = line[2].toFloat();
			card.center = Vector3( line[3].toFloat(), line[4].toFloat(),
				line[5].toFloat() );
			const QString dds = dir + "/" + id + QStringLiteral( ".DDS" );
			if ( !QFile::exists( dds ) ) {
				QImage img( frontPng );
				img = img.convertToFormat( QImage::Format_ARGB32 );
				std::vector<quint32> px( size_t( img.width() ) * img.height() );
				for ( int y = 0; y < img.height(); y++ )
					for ( int x = 0; x < img.width(); x++ )
						px[size_t( y ) * img.width() + x] = img.pixel( x, y );
				lodgenWriteDds( dds, img.width(), img.height(), px );
			}
			card.texPath = QStringLiteral( "Data\\Textures\\Lodgen\\Cards\\" )
				+ id + QStringLiteral( ".DDS" );
			card.valid = true;
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
	auto quad = [&s]( const Vector3 & a, const Vector3 & b,
		const Vector3 & c, const Vector3 & d, const Vector3 & n ) {
		const quint16 base = quint16( s.pos.size() );
		const Vector3 pts[4] = { a, b, c, d };
		const float us[4] = { 0, 1, 1, 0 };
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
	// front card (facing -Y) and side card (facing +X), crossed at centre
	quad( Vector3( cx - card.halfW, cy, z0 ), Vector3( cx + card.halfW, cy, z0 ),
		Vector3( cx + card.halfW, cy, z1 ), Vector3( cx - card.halfW, cy, z1 ),
		Vector3( 0, -1, 0 ) );
	quad( Vector3( cx, cy - card.halfW, z0 ), Vector3( cx, cy + card.halfW, z0 ),
		Vector3( cx, cy + card.halfW, z1 ), Vector3( cx, cy - card.halfW, z1 ),
		Vector3( 1, 0, 0 ) );
	return s;
}

} // namespace

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

	// gather refs: every cell's own plus the persistent overlay
	QVector<EsmRefr> refs;
	for ( int cy = 0; cy < dim; cy++ )
		for ( int cx = 0; cx < dim; cx++ )
			refs += world.refrs( chunkX + cx, chunkY + cy );
	refs += world.persistentRefrsIn( cwX, cwY,
		cwX + float( dim ) * 4096.0f, cwY + float( dim ) * 4096.0f );

	/* SCOL expansion: a static collection has no LOD models of its own — the
	 * CK generates its LOD by unpacking the parts back into their source
	 * bases (xLODGen does the same). Each part placement composes under the
	 * placing REFR: world = T_ref * T_placement. Everything downstream sees
	 * one flat placement list, so identity/AO/manifests treat expanded
	 * copies exactly like first-class refs. */
	struct LodPlacement
	{
		quint32 base;
		Vector3 pos;    // world units
		Matrix rot;
		float scale;
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
					placements.append( out );
				}
			}
			continue;
		}
		placements.append( LodPlacement{ r.base, rp, rm, r.scale } );
	}

	QHash<QString, QVector<LodSrcShape>> modelCache;
	QHash<quint32, LodgenCard> cardCache;
	QMap<QString, ObjBucket> buckets;   // key = tex0|tex1
	QStringList manifest;
	// instance grouping: base form -> (model, member object indices)
	QMap<quint32, QPair<QString, QVector<int>>> instanceGroups;
	int objectIndex = 0;
	int placed = 0, skippedNoLod = 0;

	for ( const LodPlacement & r : placements ) {
		const EsmLodBase & base = world.lodBase( r.base );
		if ( !base.hasLod )
			continue;
		QString model = base.models[qMin( lodLevel, 3 )];
		QVector<LodSrcShape> cardShapes;
		if ( model.isEmpty() && !opts.impostorDir.isEmpty() ) {
			// the requested far slot is missing: an impostor card beats
			// falling back to a heavier near-slot mesh
			const LodgenCard & card = lodgenCard( opts.impostorDir, r.base, cardCache );
			if ( card.valid )
				cardShapes.append( lodgenCardShape( card ) );
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
		// identity channel: 16-bit per-chunk index in R+G
		Color4 idColor( 1, 1, 1, 1 );
		if ( opts.identity ) {
			idColor = Color4( float( objectIndex & 0xFF ) / 255.0f,
				float( ( objectIndex >> 8 ) & 0xFF ) / 255.0f, 1.0f, 1.0f );
			manifest.append( QString( "%1 %2 %3 %4 %5 %6 %7" )
				.arg( objectIndex )
				.arg( r.base, 8, 16, QChar( '0' ) )
				.arg( QString::fromLatin1(
					reinterpret_cast<const char *>( &base.type ), 4 ) )
				.arg( double( r.pos[0] ) ).arg( double( r.pos[1] ) )
				.arg( double( r.pos[2] ) ).arg( double( r.scale ) ) );
		}

		for ( const LodSrcShape & s : shapes ) {
			ObjBucket & bucket = buckets[s.tex0.toLower() + QChar( '|' )
				+ s.tex1.toLower() + ( s.hasAlpha ? QStringLiteral( "|at" ) : QString() )];
			if ( bucket.cellTris.isEmpty() ) {
				bucket.cellTris.resize( segs );
				bucket.tex0 = s.tex0;
				bucket.tex1 = s.tex1;
				bucket.hasAlpha = s.hasAlpha;
				bucket.alphaFlags = s.alphaFlags;
				bucket.alphaThreshold = s.alphaThreshold;
			}
			const quint32 vBase = quint32( bucket.pos.size() );
			if ( vBase + quint32( s.pos.size() ) > 65535 )
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
			for ( int v = 0; v < s.pos.size(); v++ ) {
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
				if ( opts.identity )
					c.setAlpha( s.col[v].alpha() );   // authored sway weight rides along
				bucket.col.append( c );
			}
			for ( const Triangle & t : s.tris )
				bucket.cellTris[cellIdx].append( Triangle(
					quint16( vBase + t.v1() ), quint16( vBase + t.v2() ),
					quint16( vBase + t.v3() ) ) );
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
		return fail( QString( "no LOD-bearing refs in chunk (%1,%2)x%3" )
			.arg( chunkX ).arg( chunkY ).arg( dim ) );

	/* Rung 3 bake: per-placement AO into the identity B channel, ray-cast
	 * against the whole assembled chunk plus the terrain heightfield. */
	if ( opts.identity && opts.bakeAO ) {
		LodgenAoScene scene;
		const int hn = dim * 32 + 1;
		scene.hn = hn;
		scene.hSpacing = 4096.0f / float( hn - 1 );
		scene.hgt.assign( size_t( hn ) * hn, world.defaultLandHeight() * invDim );
		EsmLand land;
		for ( int cy = 0; cy < dim; cy++ )
			for ( int cx = 0; cx < dim; cx++ )
				if ( world.land( chunkX + cx, chunkY + cy, land ) )
					for ( int row = 0; row < 33; row++ )
						for ( int col = 0; col < 33; col++ )
							scene.hgt[size_t( cy * 32 + row ) * hn + size_t( cx * 32 + col )] =
								land.heights[row][col] * invDim;
		for ( auto it = buckets.constBegin(); it != buckets.constEnd(); ++it )
			for ( const QVector<Triangle> & ct : it.value().cellTris )
				for ( const Triangle & t : ct )
					scene.addTriangle( it.value().pos[t.v1()],
						it.value().pos[t.v2()], it.value().pos[t.v3()] );
		for ( auto it = buckets.begin(); it != buckets.end(); ++it ) {
			ObjBucket & bucket = it.value();
			for ( int v = 0; v < bucket.pos.size(); v++ ) {
				const float ao = scene.ambientOcclusion( bucket.pos[v],
					bucket.nrm[v], 300.0f );
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

	const std::uint64_t desc = opts.identity ? OBJ_VERTEX_DESC_COLORS : OBJ_VERTEX_DESC;
	const int stride = opts.identity ? 24 : 20;

	for ( auto it = buckets.begin(); it != buckets.end(); ++it ) {
		ObjBucket & bucket = it.value();
		QModelIndex iBoundNode = insertAvObject( nif,
			QStringLiteral( "BSMultiBoundNode" ), QString(), 1.0f );
		nif->set<quint32>( iBoundNode, "Culling Mode", 1 );
		QModelIndex iShape = insertAvObject( nif,
			QStringLiteral( "BSSubIndexTriShape" ),
			bucket.hasAlpha ? QStringLiteral( "obj-at" ) : QStringLiteral( "obj" ),
			float( dim ) );

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
		nif->set<quint32>( iShader, "Shader Flags 1", 2151677953U );
		nif->set<quint32>( iShader, "Shader Flags 2",
			opts.identity ? ( 5U | 0x20U ) : 5U );   // vertex-colours bit with identity
		QModelIndex iTexSet = nif->insertNiBlock( QStringLiteral( "BSShaderTextureSet" ) );
		nif->setLink( iShader, "Texture Set", nif->getBlockNumber( iTexSet ) );
		nif->set<uint>( iTexSet, "Num Textures", 10 );
		nif->updateArraySize( iTexSet, "Textures" );
		QModelIndex iArr = nif->getIndex( iTexSet, "Textures" );
		nif->set<QString>( nif->getIndex( iArr, 0 ), bucket.tex0 );
		nif->set<QString>( nif->getIndex( iArr, 1 ), bucket.tex1 );
		nif->setLink( iShape, "Shader Property", nif->getBlockNumber( iShader ) );
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
	if ( manifestOut )
		*manifestOut = manifest.join( QChar( '\n' ) );
	if ( error )
		*error = QString( "placed %1 objects, %2 without usable LOD, %3 material buckets" )
			.arg( placed ).arg( skippedNoLod ).arg( buckets.size() );
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
	quint8 * out )
{
	// endpoints: the block's min/max-luminance colours; blocks holding
	// transparent pixels (alpha < 128) use BC1's punch-through mode.
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
			if ( ( ( p >> 24 ) & 0xFF ) < 128 ) {
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
	const std::vector<quint32> & bgra )
{
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly ) )
		return false;
	// mip chain by box filter, down to 4x4 (BC1 block floor)
	std::vector<std::vector<quint32>> mips;
	mips.push_back( bgra );
	int mw = w, mh = h;
	while ( mw > 4 && mh > 4 ) {
		const std::vector<quint32> & prev = mips.back();
		const int nw = mw / 2, nh = mh / 2;
		std::vector<quint32> next( size_t( nw ) * nh );
		for ( int y = 0; y < nh; y++ )
			for ( int x = 0; x < nw; x++ ) {
				quint32 acc[3] = { 0, 0, 0 };
				for ( int sy = 0; sy < 2; sy++ )
					for ( int sx = 0; sx < 2; sx++ ) {
						const quint32 p = prev[size_t( y * 2 + sy ) * mw + ( x * 2 + sx )];
						acc[0] += ( p >> 16 ) & 0xFF;
						acc[1] += ( p >> 8 ) & 0xFF;
						acc[2] += p & 0xFF;
					}
				next[size_t( y ) * nw + x] = 0xFF000000U
					| ( ( acc[0] / 4 ) << 16 ) | ( ( acc[1] / 4 ) << 8 ) | ( acc[2] / 4 );
			}
		mips.push_back( std::move( next ) );
		mw = nw;
		mh = nh;
	}

	quint32 hdr[32] = { 0 };
	hdr[0] = 0x20534444;            // 'DDS '
	hdr[1] = 124;
	hdr[2] = 0x000A1007;            // caps|height|width|linearsize|pf|mipcount
	hdr[3] = quint32( h );
	hdr[4] = quint32( w );
	hdr[5] = quint32( ( ( w + 3 ) / 4 ) * ( ( h + 3 ) / 4 ) * 8 );
	hdr[7] = quint32( mips.size() );
	hdr[19] = 32;
	hdr[20] = 0x4;                  // fourCC
	hdr[21] = 0x31545844;           // 'DXT1'
	hdr[27] = 0x401008;             // caps: complex|texture|mipmap
	f.write( reinterpret_cast<const char *>( hdr ), 128 );
	mw = w;
	mh = h;
	for ( const std::vector<quint32> & mip : mips ) {
		const int bw = ( mw + 3 ) / 4, bh = ( mh + 3 ) / 4;
		std::vector<quint8> block( size_t( bw ) * bh * 8 );
		for ( int by = 0; by < bh; by++ )
			for ( int bx = 0; bx < bw; bx++ )
				lodgenEncodeBC1Block( mip.data(), mw, mh, bx, by,
					block.data() + ( size_t( by ) * bw + bx ) * 8 );
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
		QFile mf( dataRoot + "/" + path );
		path.clear();
		if ( mf.open( QIODevice::ReadOnly ) ) {
			const ShaderMaterial sm( mf.readAll() );
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
	try {
		tex = new DDSTexture16(
			( dataRoot + "/" + path ).toLocal8Bit().constData() );
	} catch ( std::exception & ) {
		tex = nullptr;
	}
	cache.insert( key, tex );
	return tex;
}

} // namespace

bool lodgenBakeTerrainTextures( const EsmWorld & world, int chunkX, int chunkY,
	int dim, const QString & dataRoot, const QString & outDir, QString * error )
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

	QHash<QString, DDSTexture16 *> texCache;
	std::vector<quint32> diffuse( size_t( RES ) * RES, 0xFF808080U );
	std::vector<quint32> msn( size_t( RES ) * RES, 0xFFFF8080U );
	// diagnostics: where the bake falls back to the flat default
	int statNoLand = 0, statNoBase = 0, statNoTex = 0;
	QSet<quint32> failedLtex;

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

	for ( int py = 0; py < RES; py++ ) {
		// image row 0 = V 0 = NORTH edge (v = 1 - y/span in the Land UVs)
		const float wy = cwY + ( 1.0f - ( float( py ) + 0.5f ) / RES ) * span;
		for ( int px = 0; px < RES; px++ ) {
			const float wx = cwX + ( ( float( px ) + 0.5f ) / RES ) * span;
			const int cx = qBound( 0, int( ( wx - cwX ) / 4096.0f ), dim - 1 );
			const int cy = qBound( 0, int( ( wy - cwY ) / 4096.0f ), dim - 1 );
			const size_t ci = size_t( cy ) * dim + cx;
			FloatVector4 color( 0.5f, 0.5f, 0.5f, 1.0f );
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
						: lodgenLoadTexture( dataRoot, d, texCache );
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
				for ( const EsmLandLayer & layer : land.layers[q] ) {
					// bilinear over the 17x17 quadrant opacities
					const float fx = qBound( 0.0f, qx * 16.0f, 15.999f );
					const float fy = qBound( 0.0f, qy * 16.0f, 15.999f );
					const int ix = int( fx ), iy = int( fy );
					const float tx = fx - ix, ty = fy - iy;
					const float a =
						( layer.opacity[iy][ix] * ( 1 - tx ) + layer.opacity[iy][ix + 1] * tx ) * ( 1 - ty )
						+ ( layer.opacity[iy + 1][ix] * ( 1 - tx ) + layer.opacity[iy + 1][ix + 1] * tx ) * ty;
					if ( a <= 0.001f )
						continue;
					// NULL-texture layers paint the engine's hardcoded default
					// ground; the chunk's dominant base is the local stand-in
					const FloatVector4 lc = sampleLtex(
						layer.ltex ? layer.ltex : dominantBase );
					color = color + ( lc - color ) * qBound( 0.0f, a, 1.0f );
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
			}
			const int r = qBound( 0, int( color[0] * 255.0f + 0.5f ), 255 );
			const int g = qBound( 0, int( color[1] * 255.0f + 0.5f ), 255 );
			const int b = qBound( 0, int( color[2] * 255.0f + 0.5f ), 255 );
			diffuse[size_t( py ) * RES + px] =
				0xFF000000U | quint32( r << 16 ) | quint32( g << 8 ) | quint32( b );

			// model-space normal from the heightfield (central differences)
			const float gx = ( wx - cwX ) / span * float( hn - 1 );
			const float gy = ( wy - cwY ) / span * float( hn - 1 );
			const int hx = qBound( 1, int( gx ), hn - 2 );
			const int hy = qBound( 1, int( gy ), hn - 2 );
			const float spacing = span / float( hn - 1 );
			const float dzdx = ( hgt[size_t( hy ) * hn + hx + 1]
				- hgt[size_t( hy ) * hn + hx - 1] ) / ( 2.0f * spacing );
			const float dzdy = ( hgt[size_t( hy + 1 ) * hn + hx]
				- hgt[size_t( hy - 1 ) * hn + hx] ) / ( 2.0f * spacing );
			Vector3 nrm( -dzdx, -dzdy, 1.0f );
			nrm.normalize();
			const int nr = qBound( 0, int( ( nrm[0] * 0.5f + 0.5f ) * 255.0f + 0.5f ), 255 );
			const int ng = qBound( 0, int( ( nrm[1] * 0.5f + 0.5f ) * 255.0f + 0.5f ), 255 );
			const int nb = qBound( 0, int( ( nrm[2] * 0.5f + 0.5f ) * 255.0f + 0.5f ), 255 );
			msn[size_t( py ) * RES + px] =
				0xFF000000U | quint32( nr << 16 ) | quint32( ng << 8 ) | quint32( nb );
		}
	}
	for ( DDSTexture16 * t : texCache )
		delete t;
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
	if ( error )
		error->clear();
	return true;
}

bool lodgenBuildAtlas( const QStringList & btoPaths, const QString & dataRoot,
	const QString & atlasFileBase, const QString & atlasGameBase,
	const QString & looseRoot, QString * error )
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

	/* Pass 1: find every atlasable shape and give each unique diffuse a
	 * cell. Atlasable = UVs inside [0,1] (tiling breaks under an atlas). */
	QHash<QString, int> cellOf;         // lowercased tex0 -> cell index
	QHash<QString, QPair<QString, QString>> cellTex;   // key -> (tex0, tex1)
	QSet<QString> directTex;            // kept-direct references, to copy loose
	int overflow = 0, tilingShapes = 0;

	auto shapeInfo = [&]( NifModel & nif, const QModelIndex & iShape,
		QString & tex0, QString & tex1, bool & inRange ) -> bool {
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
			QString tex0, tex1;
			bool inRange = false;
			if ( !shapeInfo( nif, iShape, tex0, tex1, inRange ) )
				continue;
			if ( !inRange ) {
				tilingShapes++;
				directTex.insert( tex0 );
				if ( !tex1.isEmpty() )
					directTex.insert( tex1 );
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
				continue;
			}
			cellTex.insert( key, qMakePair( tex0, tex1 ) );
			cellOf.insert( key, cellOf.size() );
		}
	}
	if ( cellOf.isEmpty() )
		return fail( QStringLiteral( "no atlasable shapes in the input files" ) );

	/* Pass 2: compose the sheets. Each cell is the source texture sampled
	 * at the mip whose texel footprint matches a 256-wide cell. */
	std::vector<quint32> sheet( size_t( AW ) * AH, 0x00000000U );
	std::vector<quint32> nrmSheet( size_t( AW ) * AH, 0xFF8080FFU );
	QHash<QString, DDSTexture16 *> texCache;
	for ( auto it = cellOf.constBegin(); it != cellOf.constEnd(); ++it ) {
		const int cx = ( it.value() % COLS ) * CELL;
		const int cy = ( it.value() / COLS ) * CELL;
		const QPair<QString, QString> & texes = cellTex[it.key()];
		const DDSTexture16 * d = lodgenLoadTexture( dataRoot, texes.first, texCache );
		const DDSTexture16 * nm = texes.second.isEmpty() ? nullptr
			: lodgenLoadTexture( dataRoot, texes.second, texCache );
		for ( int y = 0; y < CELL; y++ ) {
			for ( int x = 0; x < CELL; x++ ) {
				const float u = ( float( x ) + 0.5f ) / CELL;
				const float v = ( float( y ) + 0.5f ) / CELL;
				quint32 dp = 0xFF808080U, np = 0xFF8080FFU;
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
					np = 0xFF000000U
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
			QString tex0, tex1;
			bool inRange = false;
			if ( !shapeInfo( nif, iShape, tex0, tex1, inRange ) || !inRange )
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
			changed = true;
			movedShapes++;
		}
		if ( changed && !nif.saveToFile( path ) )
			return fail( QString( "could not rewrite %1" ).arg( path ) );
	}

	if ( !lodgenWriteDds( atlasFileBase + QStringLiteral( ".DDS" ), AW, AH, sheet ) )
		return fail( QStringLiteral( "could not write the atlas sheet" ) );
	if ( !lodgenWriteDds( atlasFileBase + QStringLiteral( "_n.DDS" ), AW, AH, nrmSheet ) )
		return fail( QStringLiteral( "could not write the atlas normal sheet" ) );

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
			const QString src = dataRoot + "/" + rel;
			const QString dst = looseRoot + "/" + rel;
			if ( QFile::exists( dst ) ) {
				copied++;
				continue;
			}
			if ( !QFile::exists( src ) ) {
				uncopyable++;
				continue;
			}
			QDir().mkpath( QFileInfo( dst ).absolutePath() );
			if ( QFile::copy( src, dst ) )
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

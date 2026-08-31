/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "lodgen.h"

#include "esmdata.h"
#include "model/nifmodel.h"
#include "spells/blocks.h"

#include <QMap>
#include <QVector>

#include <cmath>
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
		const size_t targetIdx = size_t( opts.targetTrisPerCell ) * dim * dim * 3;
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
	float zMin = 3.4e38f, zMax = -3.4e38f;
	for ( size_t v = 0; v < cpos.size(); v += 3 ) {
		zMin = qMin( zMin, cpos[v + 2] );
		zMax = qMax( zMax, cpos[v + 2] );
	}

	nif->set<BSVertexDesc>( iLand, "Vertex Desc", LAND_VERTEX_DESC );
	nif->set<quint32>( iLand, "Num Vertices", numVerts );
	nif->set<quint32>( iLand, "Num Triangles", numTris );
	nif->set<quint32>( iLand, "Data Size", numVerts * 12 + numTris * 6 );

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
		// group wet cells by height; one 16(=dim*dim)-segment shape per height
		QMap<float, QVector<QPair<int, int>>> byHeight;
		for ( int cy = 0; cy < dim; cy++ ) {
			for ( int cx = 0; cx < dim; cx++ ) {
				float h = 0.0f;
				if ( world.cellWater( chunkX + cx, chunkY + cy, h ) )
					byHeight[h].append( qMakePair( cx, cy ) );
			}
		}
		if ( !byHeight.isEmpty() ) {
			iWaterNode = insertAvObject( nif, QStringLiteral( "BSMultiBoundNode" ),
				QStringLiteral( "WATER" ), 1.0f );
			nif->set<quint32>( iWaterNode, "Culling Mode", 1 );
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

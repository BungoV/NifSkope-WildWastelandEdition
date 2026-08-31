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
	float zMin = 3.4e38f, zMax = -3.4e38f;

	/* Vanilla adds a SKIRT: the border ring is duplicated 1000 world units
	 * lower and joined to the true border by vertical flaps, hiding cracks
	 * against neighbouring chunks and rings. Measured on
	 * Commonwealth.4.-20.24.BTR: border positions appear TWICE, once at the
	 * LAND height and once exactly 1000 below. */
	constexpr float SKIRT_DROP = 1000.0f;
	const int borderCount = 4 * ( n - 1 );
	nif->set<BSVertexDesc>( iLand, "Vertex Desc", LAND_VERTEX_DESC );
	const quint32 numVerts = quint32( n ) * quint32( n ) + quint32( borderCount );
	const quint32 numTris = quint32( n - 1 ) * quint32( n - 1 ) * 2 + quint32( borderCount ) * 2;
	if ( numVerts > 65535 )
		return fail( QStringLiteral( "vertex grid exceeds the u16 limit (decimation rung not built yet)" ) );
	nif->set<quint32>( iLand, "Num Vertices", numVerts );
	nif->set<quint32>( iLand, "Num Triangles", numTris );
	nif->set<quint32>( iLand, "Data Size", numVerts * 12 + numTris * 6 );

	nif->setState( BaseModel::Processing );
	QModelIndex iVertexData = nif->getIndex( iLand, "Vertex Data" );
	nif->updateArraySize( iVertexData );
	for ( int row = 0; row < n; row++ ) {
		for ( int col = 0; col < n; col++ ) {
			const float z = grid[size_t( row ) * n + col] * invDim;
			zMin = qMin( zMin, z );
			zMax = qMax( zMax, z );
			QModelIndex v = nif->index( row * n + col, 0, iVertexData );
			nif->set<HalfVector3>( v, "Vertex",
				HalfVector3( Vector3( float( col ) * spacing, float( row ) * spacing, z ) ) );
			nif->set<float>( v, "Bitangent X", 1.0f );
			nif->set<HalfVector2>( v, "UV",
				HalfVector2( Vector2( float( col ) / float( n - 1 ),
					1.0f - float( row ) / float( n - 1 ) ) ) );
		}
	}
	/* The skirt ring: perimeter positions in walk order (south row, east
	 * column, north row reversed, west column reversed), duplicated at
	 * height-1000, then a flap quad per perimeter edge, wound to face
	 * outward. */
	{
		QVector<QPair<int, int>> ring;
		ring.reserve( borderCount );
		for ( int col = 0; col < n - 1; col++ )
			ring.append( qMakePair( 0, col ) );
		for ( int row = 0; row < n - 1; row++ )
			ring.append( qMakePair( row, n - 1 ) );
		for ( int col = n - 1; col > 0; col-- )
			ring.append( qMakePair( n - 1, col ) );
		for ( int row = n - 1; row > 0; row-- )
			ring.append( qMakePair( row, 0 ) );
		const int gridVerts = n * n;
		for ( int k = 0; k < ring.size(); k++ ) {
			const int row = ring[k].first, col = ring[k].second;
			const float z = ( grid[size_t( row ) * n + col] - SKIRT_DROP ) * invDim;
			zMin = qMin( zMin, z );
			QModelIndex v = nif->index( gridVerts + k, 0, iVertexData );
			nif->set<HalfVector3>( v, "Vertex",
				HalfVector3( Vector3( float( col ) * spacing, float( row ) * spacing, z ) ) );
			nif->set<float>( v, "Bitangent X", 1.0f );
			nif->set<HalfVector2>( v, "UV",
				HalfVector2( Vector2( float( col ) / float( n - 1 ),
					1.0f - float( row ) / float( n - 1 ) ) ) );
		}

		QVector<Triangle> tris;
		tris.reserve( int( numTris ) );
		for ( int row = 0; row < n - 1; row++ ) {
			for ( int col = 0; col < n - 1; col++ ) {
				const int a = row * n + col;
				tris.append( Triangle( quint16( a ), quint16( a + 1 ), quint16( a + n + 1 ) ) );
				tris.append( Triangle( quint16( a ), quint16( a + n + 1 ), quint16( a + n ) ) );
			}
		}
		for ( int k = 0; k < ring.size(); k++ ) {
			const int k2 = ( k + 1 ) % ring.size();
			const quint16 t0 = quint16( ring[k].first * n + ring[k].second );
			const quint16 t1 = quint16( ring[k2].first * n + ring[k2].second );
			const quint16 b0 = quint16( gridVerts + k );
			const quint16 b1 = quint16( gridVerts + k2 );
			// ring runs counter-clockwise seen from above, so (top, bottom,
			// next-top) faces outward on every side
			tris.append( Triangle( t0, b0, t1 ) );
			tris.append( Triangle( t1, b0, b1 ) );
		}
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

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

	QHash<QString, QVector<LodSrcShape>> modelCache;
	QMap<QString, ObjBucket> buckets;   // key = tex0|tex1
	QStringList manifest;
	int objectIndex = 0;
	int placed = 0, skippedNoLod = 0;

	for ( const EsmRefr & r : refs ) {
		if ( r.initiallyDisabled || r.deleted || !r.base )
			continue;
		const EsmLodBase & base = world.lodBase( r.base );
		if ( !base.hasLod )
			continue;
		QString model = base.models[qMin( lodLevel, 3 )];
		for ( int l = lodLevel; l >= 0 && model.isEmpty(); l-- )
			model = base.models[l];
		for ( int l = lodLevel; l < 4 && model.isEmpty(); l++ )
			model = base.models[l];
		if ( model.isEmpty() ) {
			skippedNoLod++;
			continue;
		}
		const QVector<LodSrcShape> & shapes =
			lodgenLoadModel( opts.dataRoot, model, modelCache );
		if ( shapes.isEmpty() ) {
			skippedNoLod++;
			continue;
		}

		Transform xf;
		xf.translation = Vector3( ( r.pos[0] - cwX ) * invDim,
			( r.pos[1] - cwY ) * invDim, r.pos[2] * invDim );
		xf.rotation = Matrix();
		xf.rotation.fromEuler( r.rot[0], r.rot[1], r.rot[2] );
		xf.scale = r.scale * invDim;

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
			for ( int v = 0; v < s.pos.size(); v++ ) {
				bucket.pos.append( xf * s.pos[v] );
				Vector3 wn = xf.rotation * s.nrm[v];
				wn.normalize();
				bucket.nrm.append( wn );
				Vector3 wt = xf.rotation * s.tan[v];
				wt.normalize();
				bucket.tan.append( wt );
				bucket.uv.append( s.uv[v] );
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
		objectIndex++;
		placed++;
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

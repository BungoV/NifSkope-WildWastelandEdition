/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "btdterrain.h"

#include "model/nifmodel.h"
#include "spells/blocks.h"

#include "btdfile.hpp"
#include "lodtfile.h"

#include <QCoreApplication>
#include <QDebug>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

/* Two whole-worldspace landscape databases open through this file, and neither
 * stores a triangle.
 *
 *  - a Fallout 76 `.btd` is the game's own terrain database: per-cell
 *    heightmaps at five detail levels, land-texture blend layers, ground cover
 *    and terrain colour, which the FO76 engine meshes at runtime the way FO4
 *    shipped pre-baked `.btr` meshes. The parser is fo76utils' own BTDFile
 *    (lib/libfo76utils/src/btdfile.cpp), vendored verbatim from the author
 *    whose library this codebase already carries; the format spec is the
 *    comment block at the top of that file.
 *
 *  - a `.lodl` is OUR equivalent, written by the LOD generator and specified in
 *    docs/LODGEN_BTD_FORMAT.md. It takes FO76's idea and not its layout, and it
 *    carries planes a `.btd` has no room for: baked ambient occlusion, water
 *    height and type, per-cell height ranges. Its reader is src/lodtfile.cpp
 *    and this file does not contain a second one.
 *
 * So "opening" either means choosing a region and a detail level and BUILDING
 * the mesh here. The tile mesher below is ONE piece of code serving both: the
 * rules it encodes -- the 254-vertex tile side that keeps a BSTriShape's u16
 * vertex count legal, the +1 rim row that closes the seam between tiles, the
 * non-zero bitangent that stops the shader basis going NaN -- are properties of
 * the OUTPUT format, not of either input.
 *
 * The one thing a `.lodl` has that a `.btd` does not is PLANES. Heights are the
 * geometry either way; a `.lodl` view can paint the same surface with any one
 * of its other stored planes as vertex colours, which is how each is made
 * viewable on its own.
 */

namespace
{

//! The format's own ceiling: BSTriShape counts vertices in a u16.
constexpr int MAX_TILE_VERTS_PER_SIDE = 254;
//! Guard rails for one document, not the format's.
constexpr qint64 MAX_TOTAL_VERTS = 9500000;
constexpr qint64 MAX_SHAPES = 4096;

int samplesPerCell( int lod )
{
	return 128 >> lod;
}

//! Cells per BSTriShape side: as many as keep (n*k + 1) under 255 vertices.
int cellsPerTileForRate( int n )
{
	return qMax( 1, MAX_TILE_VERTS_PER_SIDE / qMax( 1, n ) );
}

int cellsPerTile( int lod )
{
	return cellsPerTileForRate( samplesPerCell( lod ) );
}

//! Shapes and vertices a cellsX x cellsY region meshes to at n samples a cell.
//! Shared by both formats' estimators so a dialog and its build cannot disagree.
void tileCounts( int cellsX, int cellsY, int n, qint64 & shapeCount, qint64 & vertCount )
{
	const int k = cellsPerTileForRate( n );
	const int tilesX = ( cellsX + k - 1 ) / k;
	const int tilesY = ( cellsY + k - 1 ) / k;
	vertCount = 0;
	for ( int ty = 0; ty < tilesY; ty++ ) {
		const qint64 hCells = qMin( k, cellsY - ty * k );
		for ( int tx = 0; tx < tilesX; tx++ ) {
			const qint64 wCells = qMin( k, cellsX - tx * k );
			vertCount += ( wCells * n + 1 ) * ( hCells * n + 1 );
		}
	}
	shapeCount = qint64( tilesX ) * tilesY;
}

struct TerrainVert
{
	Vector3 pos, nrm;
	Vector2 uv;
};

/*! Everything the tile mesher needs, and nothing about where it came from.
 *
 *  `z` is a region-local grid of (cellsX*n + 1) by (cellsY*n + 1) world-unit
 *  heights -- the +1 is the rim that closes the seam between tiles, and both
 *  callers fill it from the NEIGHBOURING cell rather than duplicating an edge,
 *  except where the region reaches the worldspace's own rim and there is no
 *  neighbour to read.
 *
 *  `rgba` is either empty (no vertex colours at all, and the descriptor then
 *  has no colour channel) or the same grid packed R | G<<8 | B<<16 | A<<24.
 */
struct TerrainSurface
{
	QString rootName;
	QString shapePrefix = QStringLiteral( "Terrain" );
	int cellX0 = 0, cellY0 = 0;
	int cellsX = 1, cellsY = 1;
	int n = 8;
	float spacing = 512.0f;
	std::vector<float> z;
	std::vector<quint32> rgba;
	//! Slot 0 of the texture set, in the renderer's inline-colour syntax.
	QString diffuse = QStringLiteral( "#FF808080" );
};

bool buildTerrainSurface( NifModel * nif, TerrainSurface & s, QString * error )
{
	auto fail = [error]( const QString & message ) {
		if ( error )
			*error = message;
		return false;
	};
	if ( !nif )
		return fail( QStringLiteral( "no model" ) );

	const int n = s.n;
	const int k = cellsPerTileForRate( n );
	const int gridW = s.cellsX * n + 1;
	const int gridH = s.cellsY * n + 1;
	if ( s.z.size() != size_t( gridW ) * size_t( gridH ) )
		return fail( QStringLiteral( "height grid is %1 samples, expected %2" )
			.arg( qulonglong( s.z.size() ) ).arg( qulonglong( size_t( gridW ) * gridH ) ) );
	const bool haveColour = !s.rgba.empty();
	if ( haveColour && s.rgba.size() != s.z.size() )
		return fail( QStringLiteral( "colour grid does not match the height grid" ) );

	const float spacing = s.spacing;
	auto height = [&]( int gx, int gy ) -> float {
		return s.z[size_t( gy ) * gridW + gx];
	};
	auto normalAt = [&]( int gx, int gy ) -> Vector3 {
		const int xm = qMax( 0, gx - 1 ), xp = qMin( gridW - 1, gx + 1 );
		const int ym = qMax( 0, gy - 1 ), yp = qMin( gridH - 1, gy + 1 );
		const float dzdx = ( height( xp, gy ) - height( xm, gy ) ) / ( float( xp - xm ) * spacing );
		const float dzdy = ( height( gx, yp ) - height( gx, ym ) ) / ( float( yp - ym ) * spacing );
		Vector3 nrm( -dzdx, -dzdy, 1.0f );
		nrm.normalize();
		return nrm;
	};

	/* Fallout 4 document, the same header the starter scene and every other
	 * generated document here uses; BS version 130 conditions the BSTriShape
	 * vertex layout below. */
	if ( !nif->createNew( 0x14020007, 12, 130 ) )
		return fail( QStringLiteral( "could not create a Fallout 4 document" ) );

	nif->holdUpdates( true );
	QModelIndex iRoot = nif->insertNiBlock( QStringLiteral( "NiNode" ) );
	nif->set<QString>( iRoot, "Name", s.rootName );
	nif->set<quint32>( iRoot, "Flags", 14 );
	nif->set<float>( iRoot, "Scale", 1.0f );

	/* Full-precision layout, 28 bytes a vertex -- the same descriptor the
	 * starter cube and the collision proxy use, known to load and render. A
	 * plane view adds the colour channel, which ResetAttributeOffsets puts at
	 * offset 28 and takes the vertex to 32 bytes; without it the value is
	 * exactly the 0x0041B00000650407 this route has always written, which is
	 * why a Height view is the same bytes a .btd builds. */
	BSVertexDesc desc( 0x0041B00000650407ULL );
	if ( haveColour ) {
		desc.SetFlag( VertexFlags::VF_COLORS );
		desc.ResetAttributeOffsets( 130 );
	}
	const std::uint64_t vertexDesc = desc.Value();
	const int stride = int( desc.GetVertexSize() );

	const int tilesX = ( s.cellsX + k - 1 ) / k;
	const int tilesY = ( s.cellsY + k - 1 ) / k;
	QVector<TerrainVert> verts;
	QVector<Triangle> tris;

	for ( int ty = 0; ty < tilesY; ty++ ) {
		const int hCells = qMin( k, s.cellsY - ty * k );
		const int hV = hCells * n + 1;
		for ( int tx = 0; tx < tilesX; tx++ ) {
			const int wCells = qMin( k, s.cellsX - tx * k );
			const int wV = wCells * n + 1;
			const int gx0 = tx * k * n;
			const int gy0 = ty * k * n;

			verts.clear();
			verts.reserve( wV * hV );
			float zMin = 3.4e38f, zMax = -3.4e38f;
			for ( int j = 0; j < hV; j++ ) {
				for ( int i = 0; i < wV; i++ ) {
					TerrainVert v;
					const float z = height( gx0 + i, gy0 + j );
					v.pos = Vector3( float( i ) * spacing, float( j ) * spacing, z );
					v.nrm = normalAt( gx0 + i, gy0 + j );
					v.uv = Vector2( float( i ) / float( wV - 1 ), 1.0f - float( j ) / float( hV - 1 ) );
					verts.append( v );
					zMin = qMin( zMin, z );
					zMax = qMax( zMax, z );
				}
			}

			tris.clear();
			tris.reserve( ( wV - 1 ) * ( hV - 1 ) * 2 );
			for ( int j = 0; j < hV - 1; j++ ) {
				for ( int i = 0; i < wV - 1; i++ ) {
					const int a = j * wV + i;
					// CCW seen from above, the outward (+Z) winding
					tris.append( Triangle( quint16( a ), quint16( a + 1 ), quint16( a + wV + 1 ) ) );
					tris.append( Triangle( quint16( a ), quint16( a + wV + 1 ), quint16( a + wV ) ) );
				}
			}

			QModelIndex iShape = nif->insertNiBlock( QStringLiteral( "BSTriShape" ) );
			nif->set<QString>( iShape, "Name",
				QString( "%1 %2,%3" ).arg( s.shapePrefix )
					.arg( s.cellX0 + tx * k ).arg( s.cellY0 + ty * k ) );
			nif->set<quint32>( iShape, "Flags", 14 );
			nif->set<float>( iShape, "Scale", 1.0f );
			nif->set<Vector3>( iShape, "Translation",
				Vector3( float( s.cellX0 + tx * k ) * 4096.0f,
					float( s.cellY0 + ty * k ) * 4096.0f, 0.0f ) );

			nif->set<BSVertexDesc>( iShape, "Vertex Desc", vertexDesc );
			nif->set<quint32>( iShape, "Num Vertices", quint32( verts.size() ) );
			nif->set<quint32>( iShape, "Num Triangles", quint32( tris.size() ) );
			nif->set<quint32>( iShape, "Data Size",
				quint32( verts.size() * stride + tris.size() * 6 ) );

			nif->setState( BaseModel::Processing );
			QModelIndex iVertexData = nif->getIndex( iShape, "Vertex Data" );
			nif->updateArraySize( iVertexData );
			for ( int i = 0; i < verts.size(); i++ ) {
				QModelIndex row = nif->index( i, 0, iVertexData );
				const TerrainVert & v = verts.at( i );
				nif->set<Vector3>( row, "Vertex", v.pos );
				nif->set<HalfVector2>( row, "UV", HalfVector2( v.uv ) );
				nif->set<ByteVector3>( row, "Normal", ByteVector3( v.nrm ) );
				Vector3 t = Vector3::crossproduct( v.nrm, Vector3( 0, 0, 1 ) );
				if ( t.squaredLength() < 1.0e-6f )
					t = Vector3( 1, 0, 0 );
				t.normalize();
				nif->set<ByteVector3>( row, "Tangent", t );
				// the zero bitangent is a NaN in the shader's basis and renders
				// BLACK — the starter cube's own documented landmine
				const Vector3 b = Vector3::crossproduct( v.nrm, t );
				nif->set<float>( row, "Bitangent X", b[0] );
				nif->set<float>( row, "Bitangent Y", b[1] );
				nif->set<float>( row, "Bitangent Z", b[2] );
				if ( haveColour ) {
					const int gi = ( gy0 + i / wV ) * gridW + ( gx0 + i % wV );
					const quint32 c = s.rgba[size_t( gi )];
					nif->set<ByteColor4>( row, "Vertex Colors", ByteColor4( FloatVector4(
						float( c & 0xFF ) / 255.0f, float( ( c >> 8 ) & 0xFF ) / 255.0f,
						float( ( c >> 16 ) & 0xFF ) / 255.0f,
						float( ( c >> 24 ) & 0xFF ) / 255.0f ) ) );
				}
			}
			QModelIndex iTriangles = nif->getIndex( iShape, "Triangles" );
			nif->updateArraySize( iTriangles );
			nif->setArray<Triangle>( iTriangles, tris );

			QModelIndex iBound = nif->getIndex( iShape, "Bounding Sphere" );
			if ( iBound.isValid() ) {
				const float wSpan = float( wV - 1 ) * spacing;
				const float hSpan = float( hV - 1 ) * spacing;
				const Vector3 center( wSpan * 0.5f, hSpan * 0.5f, ( zMin + zMax ) * 0.5f );
				const Vector3 halfDiag( wSpan * 0.5f, hSpan * 0.5f, ( zMax - zMin ) * 0.5f );
				nif->set<Vector3>( iBound, "Center", center );
				nif->set<float>( iBound, "Radius", halfDiag.length() );
			}
			nif->restoreState();

			// the renderer's inline-colour texture syntax, as the starter scene
			// does; land-texture blending from real sheets is a later round
			QModelIndex iShader = nif->insertNiBlock( QStringLiteral( "BSLightingShaderProperty" ) );
			QModelIndex iTextures = nif->insertNiBlock( QStringLiteral( "BSShaderTextureSet" ) );
			nif->setLink( iShader, "Texture Set", nif->getBlockNumber( iTextures ) );
			nif->set<uint>( iTextures, "Num Textures", 10 );
			nif->updateArraySize( iTextures, "Textures" );
			QModelIndex iTexArray = nif->getIndex( iTextures, "Textures" );
			nif->set<QString>( nif->getIndex( iTexArray, 0 ), s.diffuse );
			nif->set<QString>( nif->getIndex( iTexArray, 1 ), QStringLiteral( "#FFFF8080" ) );
			if ( haveColour ) {
				/* FO4's own path always applies vertex colours when the vertex
				 * has them, but the legacy program path still reads the flag,
				 * so set it rather than leave the picture depending on which
				 * program the scene picked. */
				const quint32 sf2 = nif->get<quint32>( iShader, "Shader Flags 2" );
				nif->set<quint32>( iShader, "Shader Flags 2", sf2 | 0x20u );
			}
			nif->setLink( iShape, "Shader Property", nif->getBlockNumber( iShader ) );

			addLink( nif, iRoot, QStringLiteral( "Children" ), nif->getBlockNumber( iShape ) );
		}
	}

	nif->holdUpdates( false );
	nif->updateModel();

	if ( error )
		error->clear();
	return true;
}

//! A stable, well-separated colour for an arbitrary id (a form ID, a bit mask).
//! Deterministic so the same texture is the same colour in every picture.
void hashColour( quint32 id, float & r, float & g, float & b )
{
	quint32 h = id * 2654435761u;
	h ^= h >> 15;
	h *= 2246822519u;
	h ^= h >> 13;
	const float hue = float( h & 0xFFFFFFu ) / float( 0x1000000u ) * 6.0f;
	const float sat = 0.55f + float( ( h >> 24 ) & 0x3F ) / 255.0f;   // 0.55..0.80
	const int sector = int( hue ) % 6;
	const float frac = hue - std::floor( hue );
	const float p = 1.0f - sat, q = 1.0f - sat * frac, t = 1.0f - sat * ( 1.0f - frac );
	switch ( sector ) {
	case 0: r = 1.0f; g = t; b = p; break;
	case 1: r = q; g = 1.0f; b = p; break;
	case 2: r = p; g = 1.0f; b = t; break;
	case 3: r = p; g = q; b = 1.0f; break;
	case 4: r = t; g = p; b = 1.0f; break;
	default: r = 1.0f; g = p; b = q; break;
	}
}

quint32 packRgba( float r, float g, float b, float a = 1.0f )
{
	auto q = []( float v ) -> quint32 {
		return quint32( qBound( 0, int( v * 255.0f + 0.5f ), 255 ) );
	};
	return q( r ) | ( q( g ) << 8 ) | ( q( b ) << 16 ) | ( q( a ) << 24 );
}

} // namespace

bool btdReadWorldInfo( const QString & path, BtdWorldInfo & info, QString * error )
{
	try {
		BTDFile btd( QFile::encodeName( path ).constData() );
		info.cellMinX = btd.getCellMinX();
		info.cellMinY = btd.getCellMinY();
		info.cellMaxX = btd.getCellMaxX();
		info.cellMaxY = btd.getCellMaxY();
		info.heightMin = btd.getMinHeight();
		info.heightMax = btd.getMaxHeight();
		info.landTextureCount = int( btd.getLandTextureCount() );
		info.groundCoverCount = int( btd.getGroundCoverCount() );
		if ( error )
			error->clear();
		return true;
	} catch ( std::exception & e ) {
		if ( error )
			*error = QString::fromLatin1( e.what() );
		return false;
	}
}

bool btdEstimateRegion( const BtdWorldInfo & info, const BtdRegionSpec & spec,
	qint64 * shapes, qint64 * verts, QString * error )
{
	auto fail = [error]( const QString & message ) {
		if ( error )
			*error = message;
		return false;
	};
	if ( spec.lod < 0 || spec.lod > 4 )
		return fail( QStringLiteral( "detail level must be 0 (finest) to 4 (coarsest)" ) );
	if ( spec.x0 > spec.x1 || spec.y0 > spec.y1 )
		return fail( QStringLiteral( "the region rectangle is empty" ) );
	if ( spec.x0 < info.cellMinX || spec.x1 > info.cellMaxX
		|| spec.y0 < info.cellMinY || spec.y1 > info.cellMaxY )
		return fail( QString( "the region leaves the worldspace (cells %1..%2 x %3..%4)" )
			.arg( info.cellMinX ).arg( info.cellMaxX )
			.arg( info.cellMinY ).arg( info.cellMaxY ) );

	qint64 shapeCount = 0, vertCount = 0;
	tileCounts( spec.x1 - spec.x0 + 1, spec.y1 - spec.y0 + 1,
		samplesPerCell( spec.lod ), shapeCount, vertCount );
	if ( shapes )
		*shapes = shapeCount;
	if ( verts )
		*verts = vertCount;

	if ( shapeCount > MAX_SHAPES )
		return fail( QString( "%1 shapes is over the %2 limit — shrink the region or raise the detail level number" )
			.arg( shapeCount ).arg( MAX_SHAPES ) );
	if ( vertCount > MAX_TOTAL_VERTS )
		return fail( QString( "%L1 vertices is over the %L2 limit — shrink the region or raise the detail level number" )
			.arg( vertCount ).arg( MAX_TOTAL_VERTS ) );
	if ( error )
		error->clear();
	return true;
}

BtdRegionSpec btdDefaultRegion( const BtdWorldInfo & info )
{
	BtdRegionSpec spec;
	spec.x0 = info.cellMinX;
	spec.y0 = info.cellMinY;
	spec.x1 = info.cellMaxX;
	spec.y1 = info.cellMaxY;
	spec.lod = 4;
	spec.valid = btdEstimateRegion( info, spec, nullptr, nullptr, nullptr );
	if ( !spec.valid ) {
		// A worldspace so large even LOD4 overflows the budget: take a centred
		// window instead of refusing to open at all.
		const int cx = ( info.cellMinX + info.cellMaxX ) / 2;
		const int cy = ( info.cellMinY + info.cellMaxY ) / 2;
		spec.x0 = qMax( info.cellMinX, cx - 50 );
		spec.x1 = qMin( info.cellMaxX, cx + 49 );
		spec.y0 = qMax( info.cellMinY, cy - 50 );
		spec.y1 = qMin( info.cellMaxY, cy + 49 );
		spec.valid = btdEstimateRegion( info, spec, nullptr, nullptr, nullptr );
	}
	return spec;
}

bool nifCreateBtdTerrainScene( NifModel * nif, const QString & btdPath,
	const BtdRegionSpec & spec, QString * error )
{
	auto fail = [error]( const QString & message ) {
		if ( error )
			*error = message;
		return false;
	};
	if ( !nif )
		return fail( QStringLiteral( "no model" ) );

	BtdWorldInfo info;
	if ( !btdReadWorldInfo( btdPath, info, error ) )
		return false;
	if ( !btdEstimateRegion( info, spec, nullptr, nullptr, error ) )
		return false;

	const int lod = spec.lod;
	const int n = samplesPerCell( lod );
	const int cellsX = spec.x1 - spec.x0 + 1;
	const int cellsY = spec.y1 - spec.y0 + 1;
	const int gridW = cellsX * n + 1;
	const int gridH = cellsY * n + 1;

	/* One height grid for the whole region, in the file's raw u16 scale. The
	 * +1 row and column come from the neighbouring cells' first samples, which
	 * is what closes the seam between cells — and between the tiles built from
	 * this grid, which share their edge samples through it. */
	std::vector<std::uint16_t> grid( size_t( gridW ) * size_t( gridH ), 0 );

	try {
		BTDFile btd( QFile::encodeName( btdPath ).constData() );
		// row-major cell visits re-enter each 8x8-cell tile once per cell row,
		// so the cache has to hold a full row of tiles to decompress each once
		btd.setTileCacheSize( size_t( qMin( 64, ( cellsX + 1 ) / 8 + 3 ) ) );
		std::vector<std::uint16_t> cellBuf( size_t( n ) * size_t( n ) );
		for ( int cy = spec.y0; cy <= spec.y1 + 1; cy++ ) {
			if ( cy > info.cellMaxY )
				continue;
			for ( int cx = spec.x0; cx <= spec.x1 + 1; cx++ ) {
				if ( cx > info.cellMaxX )
					continue;
				btd.getCellHeightMap( cellBuf.data(), cx, cy, (unsigned char) lod );
				const int baseX = ( cx - spec.x0 ) * n;
				const int baseY = ( cy - spec.y0 ) * n;
				const int maxI = qMin( n, gridW - baseX );
				const int maxJ = qMin( n, gridH - baseY );
				for ( int j = 0; j < maxJ; j++ ) {
					std::memcpy( grid.data() + size_t( baseY + j ) * gridW + baseX,
						cellBuf.data() + size_t( j ) * n,
						size_t( maxI ) * sizeof( std::uint16_t ) );
				}
			}
		}
	} catch ( std::exception & e ) {
		return fail( QString::fromLatin1( e.what() ) );
	}

	// no neighbour beyond the worldspace edge: extend the last real samples
	if ( spec.x1 + 1 > info.cellMaxX ) {
		for ( int gy = 0; gy < gridH; gy++ )
			grid[size_t( gy ) * gridW + gridW - 1] = grid[size_t( gy ) * gridW + gridW - 2];
	}
	if ( spec.y1 + 1 > info.cellMaxY ) {
		std::memcpy( grid.data() + size_t( gridH - 1 ) * gridW,
			grid.data() + size_t( gridH - 2 ) * gridW,
			size_t( gridW ) * sizeof( std::uint16_t ) );
	}

	TerrainSurface s;
	s.rootName = QString( "%1 [%2,%3]..[%4,%5] LOD%6" )
		.arg( QFileInfo( btdPath ).completeBaseName() )
		.arg( spec.x0 ).arg( spec.y0 ).arg( spec.x1 ).arg( spec.y1 ).arg( lod );
	s.cellX0 = spec.x0;
	s.cellY0 = spec.y0;
	s.cellsX = cellsX;
	s.cellsY = cellsY;
	s.n = n;
	s.spacing = 4096.0f / float( n );

	const float zScale = ( info.heightMax - info.heightMin ) / 65535.0f;
	s.z.resize( grid.size() );
	for ( size_t i = 0; i < grid.size(); i++ )
		s.z[i] = info.heightMin + float( grid[i] ) * zScale;
	std::vector<std::uint16_t>().swap( grid );

	return buildTerrainSurface( nif, s, error );
}

bool btdRegionFromEnv( BtdRegionSpec & spec )
{
	const QByteArray env = qgetenv( "WW_BTD_REGION" );
	if ( env.isEmpty() )
		return false;
	const QStringList parts = QString::fromLatin1( env ).split( QLatin1Char( ',' ) );
	if ( parts.size() != 5 )
		return false;
	spec.x0 = parts[0].toInt();
	spec.y0 = parts[1].toInt();
	spec.x1 = parts[2].toInt();
	spec.y1 = parts[3].toInt();
	spec.lod = parts[4].toInt();
	spec.valid = true;
	return true;
}

bool btdQueryRegion( QWidget * parent, const QString & path,
	const BtdWorldInfo & info, BtdRegionSpec & spec )
{
	// the harness route: no dialog, the region comes from the environment
	if ( btdRegionFromEnv( spec ) )
		return true;

	QDialog dlg( parent );
	dlg.setWindowTitle( QCoreApplication::translate( "btdterrain", "Open Terrain — %1" )
		.arg( QFileInfo( path ).fileName() ) );

	auto layout = new QVBoxLayout( &dlg );
	auto infoLabel = new QLabel(
		QCoreApplication::translate( "btdterrain",
			"Worldspace cells [%1,%2]..[%3,%4], heights %5 to %6.\n"
			"This file stores heightmaps, not meshes; choose how much to build." )
			.arg( info.cellMinX ).arg( info.cellMinY )
			.arg( info.cellMaxX ).arg( info.cellMaxY )
			.arg( double( info.heightMin ), 0, 'f', 0 )
			.arg( double( info.heightMax ), 0, 'f', 0 ), &dlg );
	layout->addWidget( infoLabel );

	auto grid = new QGridLayout();
	layout->addLayout( grid );
	auto makeSpin = [&]( int row, int col, const QString & label, int value ) {
		grid->addWidget( new QLabel( label, &dlg ), row, col );
		auto spin = new QSpinBox( &dlg );
		spin->setRange( qMin( info.cellMinX, info.cellMinY ), qMax( info.cellMaxX, info.cellMaxY ) );
		spin->setValue( value );
		grid->addWidget( spin, row, col + 1 );
		return spin;
	};
	auto spinX0 = makeSpin( 0, 0, QStringLiteral( "West cell X" ), info.cellMinX );
	auto spinX1 = makeSpin( 0, 2, QStringLiteral( "East cell X" ), info.cellMaxX );
	auto spinY0 = makeSpin( 1, 0, QStringLiteral( "South cell Y" ), info.cellMinY );
	auto spinY1 = makeSpin( 1, 2, QStringLiteral( "North cell Y" ), info.cellMaxY );

	grid->addWidget( new QLabel( QStringLiteral( "Detail" ), &dlg ), 2, 0 );
	auto lodBox = new QComboBox( &dlg );
	for ( int l = 0; l <= 4; l++ )
		lodBox->addItem( QCoreApplication::translate( "btdterrain", "LOD %1 — one sample every %2 units" )
			.arg( l ).arg( 32 << l ) );
	lodBox->setCurrentIndex( 4 );
	grid->addWidget( lodBox, 2, 1, 1, 3 );

	auto estimate = new QLabel( &dlg );
	layout->addWidget( estimate );

	auto buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg );
	layout->addWidget( buttons );
	QObject::connect( buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept );
	QObject::connect( buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject );

	auto refresh = [&]() {
		BtdRegionSpec s;
		s.x0 = spinX0->value();
		s.x1 = spinX1->value();
		s.y0 = spinY0->value();
		s.y1 = spinY1->value();
		s.lod = lodBox->currentIndex();
		qint64 shapes = 0, vertCount = 0;
		QString why;
		const bool ok = btdEstimateRegion( info, s, &shapes, &vertCount, &why );
		estimate->setText( ok
			? QCoreApplication::translate( "btdterrain", "%L1 terrain shapes, %L2 vertices." )
				.arg( shapes ).arg( vertCount )
			: why );
		buttons->button( QDialogButtonBox::Ok )->setEnabled( ok );
	};
	for ( auto spin : { spinX0, spinX1, spinY0, spinY1 } )
		QObject::connect( spin, qOverload<int>( &QSpinBox::valueChanged ), &dlg, refresh );
	QObject::connect( lodBox, qOverload<int>( &QComboBox::currentIndexChanged ), &dlg, refresh );
	refresh();

	if ( dlg.exec() != QDialog::Accepted )
		return false;

	spec.x0 = spinX0->value();
	spec.x1 = spinX1->value();
	spec.y0 = spinY0->value();
	spec.y1 = spinY1->value();
	spec.lod = lodBox->currentIndex();
	spec.valid = true;
	return true;
}


/* =========================================================================
 * .lodl  (written and opened as .lodt until bungo's 2026-09-09 ruling; the
 *         C++ names here are internal and did not move with the extension)
 * ========================================================================= */

const char * lodtPlaneKey( LodtPlane plane )
{
	switch ( plane ) {
	case LodtPlane::Height:            return "height";
	case LodtPlane::AmbientOcclusion:  return "ao";
	case LodtPlane::LandTextureBlend:  return "blend";
	case LodtPlane::TerrainColour:     return "colour";
	case LodtPlane::GroundCover:       return "groundcover";
	case LodtPlane::WaterHeight:       return "waterheight";
	case LodtPlane::WaterType:         return "watertype";
	case LodtPlane::CellFlags:         return "cellflags";
	case LodtPlane::CellHeightRange:   return "cellrange";
	case LodtPlane::CoarseOverview:    return "overview";
	default:                           return "height";
	}
}

QString lodtPlaneLabel( LodtPlane plane )
{
	switch ( plane ) {
	case LodtPlane::Height:            return QStringLiteral( "Heights — the geometry itself" );
	case LodtPlane::AmbientOcclusion:  return QStringLiteral( "Ambient occlusion — baked sky visibility" );
	case LodtPlane::LandTextureBlend:  return QStringLiteral( "Land texture blend — five layers over a base" );
	case LodtPlane::TerrainColour:     return QStringLiteral( "Terrain colour — the authored tint" );
	case LodtPlane::GroundCover:       return QStringLiteral( "Ground cover — which covers reach a sample" );
	case LodtPlane::WaterHeight:       return QStringLiteral( "Water height — the per-cell water plane" );
	case LodtPlane::WaterType:         return QStringLiteral( "Water type — which WATR record a cell uses" );
	case LodtPlane::CellFlags:         return QStringLiteral( "Cell flags — has land, has water" );
	case LodtPlane::CellHeightRange:   return QStringLiteral( "Cell height range — per-cell relief" );
	case LodtPlane::CoarseOverview:    return QStringLiteral( "Coarse overview — the always-resident grid" );
	default:                           return QStringLiteral( "Heights" );
	}
}

bool lodtPlaneFromKey( const QString & key, LodtPlane & plane )
{
	const QString k = key.trimmed().toLower();
	for ( int i = 0; i < int( LodtPlane::Count ); i++ ) {
		if ( k == QLatin1String( lodtPlaneKey( LodtPlane( i ) ) ) ) {
			plane = LodtPlane( i );
			return true;
		}
	}
	return false;
}

//! The header a caller wants, out of a reader that is ALREADY open -- so a
//! scene build reads the file's prefix once rather than opening it twice (that
//! prefix is 2.5 MB on the Commonwealth and ~20 MB on Appalachia).
static void lodtInfoFrom( const LodtFile & f, LodtWorldInfo & info )
{
	info.cellMinX = f.cellMinX();
	info.cellMinY = f.cellMinY();
	info.cellMaxX = f.cellMaxX();
	info.cellMaxY = f.cellMaxY();
	info.heightMin = f.minHeight();
	info.heightMax = f.maxHeight();
	info.heightQuantum = f.heightQuantum();
	info.samplesPerCell = f.samplesPerCell();
	info.blockEdge = f.blockEdge();
	info.levelCount = f.levelCount();
	info.aoSamples = f.aoSamples();
	info.overviewSamples = f.overviewSamples();
	info.ltexCount = f.ltexCount();
	info.watrCount = f.watrCount();
	info.gcvrCount = f.gcvrCount();
	info.blockCount = f.blockCount();
	info.sectionFlags = f.sectionFlags();
}

bool lodtReadWorldInfo( const QString & path, LodtWorldInfo & info, QString * error )
{
	LodtFile f;
	if ( !f.open( path, error ) )
		return false;
	lodtInfoFrom( f, info );
	if ( error )
		error->clear();
	return true;
}

QList<LodtPlane> lodtAvailablePlanes( const LodtWorldInfo & info )
{
	QList<LodtPlane> out;
	out << LodtPlane::Height;
	if ( ( info.sectionFlags & 4u ) && info.aoSamples > 0 )
		out << LodtPlane::AmbientOcclusion;
	if ( info.ltexCount > 0 )
		out << LodtPlane::LandTextureBlend;
	if ( info.sectionFlags & 1u )
		out << LodtPlane::TerrainColour;
	if ( info.sectionFlags & 2u )
		out << LodtPlane::GroundCover;
	if ( info.sectionFlags & 8u ) {
		out << LodtPlane::WaterHeight;
		out << LodtPlane::WaterType;
	}
	// the per-cell table is never optional
	out << LodtPlane::CellFlags;
	out << LodtPlane::CellHeightRange;
	if ( info.overviewSamples > 0 )
		out << LodtPlane::CoarseOverview;
	return out;
}

int lodtMaxLod( const LodtWorldInfo & info )
{
	int lod = 0;
	while ( ( info.samplesPerCell >> ( lod + 1 ) ) >= 1 && lod < 8 )
		lod++;
	return lod;
}

//! Samples a cell at this detail level; never zero.
static int lodtRate( const LodtWorldInfo & info, int lod )
{
	return qMax( 1, info.samplesPerCell >> qMax( 0, lod ) );
}

bool lodtEstimateRegion( const LodtWorldInfo & info, const LodtRegionSpec & spec,
	qint64 * shapes, qint64 * verts, QString * error )
{
	auto fail = [error]( const QString & message ) {
		if ( error )
			*error = message;
		return false;
	};
	if ( spec.lod < 0 || spec.lod > lodtMaxLod( info ) )
		return fail( QString( "detail level must be 0 (the file's own %1 samples a cell) to %2 (one a cell)" )
			.arg( info.samplesPerCell ).arg( lodtMaxLod( info ) ) );
	if ( spec.x0 > spec.x1 || spec.y0 > spec.y1 )
		return fail( QStringLiteral( "the region rectangle is empty" ) );
	if ( spec.x0 < info.cellMinX || spec.x1 > info.cellMaxX
		|| spec.y0 < info.cellMinY || spec.y1 > info.cellMaxY )
		return fail( QString( "the region leaves the worldspace (cells %1..%2 x %3..%4)" )
			.arg( info.cellMinX ).arg( info.cellMaxX )
			.arg( info.cellMinY ).arg( info.cellMaxY ) );
	/* A missing section REFUSES IN WORDS rather than drawing a black world:
	 * an all-zero plane and an absent one look the same in a picture. */
	if ( !lodtAvailablePlanes( info ).contains( spec.plane ) )
		return fail( QString( "this file carries no \"%1\" plane "
				"(section flags 0x%2, AO %3 a cell, %4 LTEX, %5 WATR, %6 GCVR, "
				"overview %7 a cell)" )
			.arg( QLatin1String( lodtPlaneKey( spec.plane ) ) )
			.arg( info.sectionFlags, 0, 16 )
			.arg( info.aoSamples ).arg( info.ltexCount ).arg( info.watrCount )
			.arg( info.gcvrCount ).arg( info.overviewSamples ) );

	qint64 shapeCount = 0, vertCount = 0;
	tileCounts( spec.x1 - spec.x0 + 1, spec.y1 - spec.y0 + 1,
		lodtRate( info, spec.lod ), shapeCount, vertCount );
	if ( shapes )
		*shapes = shapeCount;
	if ( verts )
		*verts = vertCount;

	if ( shapeCount > MAX_SHAPES )
		return fail( QString( "%1 shapes is over the %2 limit — shrink the region or raise the detail level number" )
			.arg( shapeCount ).arg( MAX_SHAPES ) );
	if ( vertCount > MAX_TOTAL_VERTS )
		return fail( QString( "%L1 vertices is over the %L2 limit — shrink the region or raise the detail level number" )
			.arg( vertCount ).arg( MAX_TOTAL_VERTS ) );
	if ( error )
		error->clear();
	return true;
}

LodtRegionSpec lodtDefaultRegion( const LodtWorldInfo & info )
{
	LodtRegionSpec spec;
	spec.x0 = info.cellMinX;
	spec.y0 = info.cellMinY;
	spec.x1 = info.cellMaxX;
	spec.y1 = info.cellMaxY;
	spec.plane = LodtPlane::Height;
	/* Start at 8 samples a cell -- the density the .btd route's own default
	 * lands on -- and coarsen until the whole worldspace fits the budget. */
	const int maxLod = lodtMaxLod( info );
	int lod = 0;
	while ( lod < maxLod && lodtRate( info, lod ) > 8 )
		lod++;
	for ( ; lod <= maxLod; lod++ ) {
		spec.lod = lod;
		if ( lodtEstimateRegion( info, spec, nullptr, nullptr, nullptr ) ) {
			spec.valid = true;
			return spec;
		}
	}
	// too large even at one sample a cell: a centred window, as the .btd does
	spec.lod = maxLod;
	const int cx = ( info.cellMinX + info.cellMaxX ) / 2;
	const int cy = ( info.cellMinY + info.cellMaxY ) / 2;
	spec.x0 = qMax( info.cellMinX, cx - 50 );
	spec.x1 = qMin( info.cellMaxX, cx + 49 );
	spec.y0 = qMax( info.cellMinY, cy - 50 );
	spec.y1 = qMin( info.cellMaxY, cy + 49 );
	spec.valid = lodtEstimateRegion( info, spec, nullptr, nullptr, nullptr );
	return spec;
}

bool lodtRegionFromEnv( LodtRegionSpec & spec )
{
	/* The retired spellings are named, not ignored: a harness or a render
	 * still setting WW_LODT_* was written for the format this extension used
	 * to mean, and honouring it silently is exactly what the rename was meant
	 * to stop. Refusing here would leave the caller with no region at all, so
	 * this SAYS SO and then reads nothing from them. */
	for ( const char * old : { "WW_LODT_REGION", "WW_LODT_PLANE" } ) {
		if ( !qgetenv( old ).isEmpty() )
			qCritical().noquote() << QString( "REFUSED: %1 is retired -- the landscape "
				"file is .lodl now (.lodt names the terrain texture sheets). Set %2 "
				"instead; this run is ignoring it." )
				.arg( QLatin1String( old ) )
				.arg( QLatin1String( old ) == QLatin1String( "WW_LODT_REGION" )
					? "WW_LODL_REGION" : "WW_LODL_PLANE" );
	}
	const QByteArray env = qgetenv( "WW_LODL_REGION" );
	bool got = false;
	if ( !env.isEmpty() ) {
		const QStringList parts = QString::fromLatin1( env ).split( QLatin1Char( ',' ) );
		if ( parts.size() == 5 || parts.size() == 6 ) {
			spec.x0 = parts[0].toInt();
			spec.y0 = parts[1].toInt();
			spec.x1 = parts[2].toInt();
			spec.y1 = parts[3].toInt();
			spec.lod = parts[4].toInt();
			spec.valid = true;
			got = true;
			if ( parts.size() == 6 )
				lodtPlaneFromKey( parts[5], spec.plane );
		}
	}
	// the plane on its own, so one region can be photographed plane by plane
	const QByteArray planeEnv = qgetenv( "WW_LODL_PLANE" );
	if ( !planeEnv.isEmpty() && lodtPlaneFromKey( QString::fromLatin1( planeEnv ), spec.plane ) )
		got = got || spec.valid;
	return got;
}

bool nifCreateLodtTerrainScene( NifModel * nif, const QString & lodtPath,
	const LodtRegionSpec & spec, QString * error, QString * notes )
{
	auto fail = [error]( const QString & message ) {
		if ( error )
			*error = message;
		return false;
	};
	if ( !nif )
		return fail( QStringLiteral( "no model" ) );

	QElapsedTimer timer;
	timer.start();

	LodtFile f;
	if ( !f.open( lodtPath, error ) )
		return false;

	LodtWorldInfo info;
	lodtInfoFrom( f, info );
	if ( !lodtEstimateRegion( info, spec, nullptr, nullptr, error ) )
		return false;

	const int spc = info.samplesPerCell;
	const int n = lodtRate( info, spec.lod );
	const int step = qMax( 1, spc / n );
	const int cellsXr = spec.x1 - spec.x0 + 1;
	const int cellsYr = spec.y1 - spec.y0 + 1;
	const int gridW = cellsXr * n + 1;
	const int gridH = cellsYr * n + 1;
	const int lastX = ( ( f.cellsX() * spc - 1 ) / step ) * step;
	const int lastY = ( ( f.cellsY() * spc - 1 ) / step ) * step;
	const int baseX = ( spec.x0 - info.cellMinX ) * spc;
	const int baseY = ( spec.y0 - info.cellMinY ) * spc;

	/* The pyramid is PROGRESSIVE, so a subsampled walk alternates between the
	 * level that stores every step-th sample and the one that stores every
	 * 2*step-th; a one-block cache then misses on every single sample. Hold a
	 * couple of full block rows at both levels instead. Commonwealth at 8 a
	 * cell: 48 + 24 blocks a row, ~6 KB each. */
	f.setBlockCacheSize( 256 );

	auto gxOf = [&]( int i ) { return qMin( baseX + i * step, lastX ); };
	auto gyOf = [&]( int j ) { return qMin( baseY + j * step, lastY ); };

	std::vector<float> z( size_t( gridW ) * size_t( gridH ), 0.0f );
	for ( int j = 0; j < gridH; j++ ) {
		const int gy = gyOf( j );
		for ( int i = 0; i < gridW; i++ )
			z[size_t( j ) * gridW + i] = f.height( gxOf( i ), gy );
	}
	const qint64 msHeights = timer.elapsed();

	QStringList note;
	note << QString( "lodl %1: cells [%2,%3]..[%4,%5], %6 samples a cell "
			"(file %7), spacing %8 units, grid %9x%10" )
		.arg( QFileInfo( lodtPath ).fileName() )
		.arg( spec.x0 ).arg( spec.y0 ).arg( spec.x1 ).arg( spec.y1 )
		.arg( n ).arg( spc ).arg( double( 4096.0f / float( n ) ), 0, 'f', 1 )
		.arg( gridW ).arg( gridH );
	note << QString( "heights read in %1 ms" ).arg( msHeights );

	TerrainSurface s;
	s.rootName = QString( "%1 [%2,%3]..[%4,%5] LOD%6 %7" )
		.arg( QFileInfo( lodtPath ).completeBaseName() )
		.arg( spec.x0 ).arg( spec.y0 ).arg( spec.x1 ).arg( spec.y1 )
		.arg( spec.lod ).arg( QLatin1String( lodtPlaneKey( spec.plane ) ) );
	s.cellX0 = spec.x0;
	s.cellY0 = spec.y0;
	s.cellsX = cellsXr;
	s.cellsY = cellsYr;
	s.n = n;
	s.spacing = 4096.0f / float( n );
	s.z.swap( z );

	if ( spec.plane != LodtPlane::Height ) {
		/* Every plane view forces the diffuse WHITE, so the picture is the
		 * plane and not the plane times the Height view's grey. */
		s.diffuse = QStringLiteral( "#FFFFFFFF" );
		s.rgba.assign( s.z.size(), packRgba( 1.0f, 1.0f, 1.0f ) );

		const int cellsXall = f.cellsX();
		auto cellOf = [&]( int gx, int gy, int & cx, int & cy ) {
			cx = qBound( info.cellMinX, info.cellMinX + gx / spc, info.cellMaxX );
			cy = qBound( info.cellMinY, info.cellMinY + gy / spc, info.cellMaxY );
		};

		switch ( spec.plane ) {

		case LodtPlane::AmbientOcclusion: {
			const int aoS = f.aoSamples();
			int lo = 255, hi = 0;
			double sum = 0.0;
			for ( int j = 0; j < gridH; j++ ) {
				const int ay = gyOf( j ) * aoS / spc;
				for ( int i = 0; i < gridW; i++ ) {
					const int v = f.aoSample( gxOf( i ) * aoS / spc, ay );
					lo = qMin( lo, v );
					hi = qMax( hi, v );
					sum += v;
					const float g = float( v ) / 255.0f;
					s.rgba[size_t( j ) * gridW + i] = packRgba( g, g, g );
				}
			}
			note << QString( "AO plane %1x%2 texels at %3 a cell; values %4..%5, mean %6 "
					"(255 = open sky)" )
				.arg( cellsXall * aoS ).arg( f.cellsY() * aoS ).arg( aoS )
				.arg( lo ).arg( hi )
				.arg( sum / double( gridW ) / double( gridH ), 0, 'f', 1 );
			break;
		}

		case LodtPlane::LandTextureBlend: {
			qint64 painted = 0;
			int layersSeen = 0;
			for ( int j = 0; j < gridH; j++ ) {
				const int gy = gyOf( j );
				for ( int i = 0; i < gridW; i++ ) {
					const int gx = gxOf( i );
					int cx = 0, cy = 0;
					cellOf( gx, gy, cx, cy );
					const int quad = ( ( ( gy % spc ) >= spc / 2 ) ? 2 : 0 )
						| ( ( ( gx % spc ) >= spc / 2 ) ? 1 : 0 );
					quint16 qslots[6];
					f.quadrantSlots( cx, cy, quad, qslots );
					float r = 0.28f, g = 0.26f, b = 0.24f;
					if ( qslots[5] != 0xFFFFU && qslots[5] < quint16( f.ltexCount() ) )
						hashColour( f.ltexForm( qslots[5] ), r, g, b );
					const quint16 w = f.alphaWord( gx, gy );
					bool any = false;
					// slot 0 is the TOP layer, drawn last, so composite upward
					for ( int layer = 4; layer >= 0; layer-- ) {
						const int a = ( w >> ( 3 * layer ) ) & 7;
						if ( !a || qslots[layer] == 0xFFFFU
							|| qslots[layer] >= quint16( f.ltexCount() ) )
							continue;
						any = true;
						layersSeen = qMax( layersSeen, layer + 1 );
						float lr, lg, lb;
						hashColour( f.ltexForm( qslots[layer] ), lr, lg, lb );
						const float t = float( a ) / 7.0f;
						r += ( lr - r ) * t;
						g += ( lg - g ) * t;
						b += ( lb - b ) * t;
					}
					if ( any )
						painted++;
					s.rgba[size_t( j ) * gridW + i] = packRgba( r, g, b );
				}
			}
			note << QString( "land texture blend: %1 LTEX records; %2 of %3 samples "
					"(%4%) carry a layer over their quadrant's base, deepest stack %5" )
				.arg( f.ltexCount() ).arg( painted )
				.arg( qint64( gridW ) * gridH )
				.arg( 100.0 * double( painted ) / double( qint64( gridW ) * gridH ), 0, 'f', 1 )
				.arg( layersSeen );
			break;
		}

		case LodtPlane::TerrainColour: {
			/* TWO different questions, and counting the first as the second is
			 * how this note lied once (2026-09-09): a sample can CARRY a
			 * colour record and still be white. The writer packs
			 * R<<11 | G<<6 | B and never touches bit 5, so pure white is
			 * 0xFFDF while 0xFFFF is the no-record word -- `w != 0xFFFF`
			 * therefore counts RECORDS, not tints. A picture shows the TINT,
			 * so that is what the headline number has to be. */
			qint64 tinted = 0, recorded = 0;
			for ( int j = 0; j < gridH; j++ ) {
				const int gy = gyOf( j );
				for ( int i = 0; i < gridW; i++ ) {
					const quint16 w = f.colourWord( gxOf( i ), gy );
					// R at bits 11-15, G at 6-10, B at 0-4; bit 5 unused
					const int cr = ( w >> 11 ) & 31, cg = ( w >> 6 ) & 31, cb = w & 31;
					if ( w != 0xFFFFU )
						recorded++;
					if ( cr != 31 || cg != 31 || cb != 31 )
						tinted++;
					s.rgba[size_t( j ) * gridW + i] = packRgba(
						float( cr ) / 31.0f, float( cg ) / 31.0f, float( cb ) / 31.0f );
				}
			}
			note << QString( "terrain colour: %1 of %2 samples (%3%) carry a tint, "
					"%4 carry a colour record. A region with no tint paints white -- "
					"the file's own answer, not a plane that went unread" )
				.arg( tinted ).arg( qint64( gridW ) * gridH )
				.arg( 100.0 * double( tinted ) / double( qint64( gridW ) * gridH ), 0, 'f', 1 )
				.arg( recorded );
			break;
		}

		case LodtPlane::GroundCover: {
			qint64 covered = 0;
			quint16 seen = 0;
			for ( int j = 0; j < gridH; j++ ) {
				const int gy = gyOf( j );
				for ( int i = 0; i < gridW; i++ ) {
					const quint16 m = f.groundCover( gxOf( i ), gy );
					seen |= m;
					if ( !m ) {
						s.rgba[size_t( j ) * gridW + i] = packRgba( 0.10f, 0.10f, 0.12f );
						continue;
					}
					covered++;
					int lowest = 0;
					while ( lowest < 15 && !( m & ( 1u << lowest ) ) )
						lowest++;
					int bits = 0;
					for ( int b = 0; b < 16; b++ )
						if ( m & ( 1u << b ) )
							bits++;
					float r, g, b2;
					hashColour( f.gcvrCount() > lowest ? f.gcvrForm( lowest ) : quint32( lowest ),
						r, g, b2 );
					const float k = 0.45f + 0.55f * float( bits ) / 8.0f;
					s.rgba[size_t( j ) * gridW + i] = packRgba( r * k, g * k, b2 * k );
				}
			}
			note << QString( "ground cover: %1 GCVR records, mask bits used 0x%2, "
					"%3 of %4 samples carry a cover" )
				.arg( f.gcvrCount() ).arg( seen, 0, 16 ).arg( covered )
				.arg( qint64( gridW ) * gridH );
			break;
		}

		case LodtPlane::WaterHeight:
		case LodtPlane::WaterType:
		case LodtPlane::CellFlags:
		case LodtPlane::CellHeightRange: {
			/* Per-cell planes. One pass over the region's cells for the range,
			 * then paint; a sample takes its own cell's value, so the picture
			 * is honestly cell-shaped rather than interpolated. */
			float wLo = 3.4e38f, wHi = -3.4e38f, reliefMax = 0.0f;
			qint64 water = 0, land = 0;
			for ( int cy = spec.y0; cy <= spec.y1; cy++ ) {
				for ( int cx = spec.x0; cx <= spec.x1; cx++ ) {
					float lo, hi, wh;
					quint16 wt, fl;
					if ( !f.cell( cx, cy, lo, hi, wh, wt, fl ) )
						continue;
					if ( fl & 1u ) {
						water++;
						wLo = qMin( wLo, wh );
						wHi = qMax( wHi, wh );
					}
					if ( fl & 2u ) {
						land++;
						reliefMax = qMax( reliefMax, hi - lo );
					}
				}
			}
			if ( wLo > wHi ) {
				wLo = 0.0f;
				wHi = 0.0f;
			}
			for ( int j = 0; j < gridH; j++ ) {
				const int gy = gyOf( j );
				for ( int i = 0; i < gridW; i++ ) {
					int cx = 0, cy = 0;
					cellOf( gxOf( i ), gy, cx, cy );
					float lo = 0, hi = 0, wh = 0;
					quint16 wt = 0xFFFFU, fl = 0;
					f.cell( cx, cy, lo, hi, wh, wt, fl );
					quint32 c = packRgba( 0.08f, 0.08f, 0.10f );
					if ( spec.plane == LodtPlane::WaterHeight ) {
						if ( fl & 1u ) {
							const float t = ( wHi > wLo ) ? ( wh - wLo ) / ( wHi - wLo ) : 0.5f;
							c = packRgba( 0.10f + 0.25f * t, 0.30f + 0.45f * t, 0.60f + 0.40f * t );
						}
					} else if ( spec.plane == LodtPlane::WaterType ) {
						if ( fl & 1u ) {
							if ( wt == 0xFFFFU ) {
								c = packRgba( 0.20f, 0.85f, 0.95f );   // worldspace default
							} else {
								float r, g, b;
								hashColour( wt < quint16( f.watrCount() )
									? f.watrForm( wt ) : quint32( wt ), r, g, b );
								c = packRgba( r, g, b );
							}
						}
					} else if ( spec.plane == LodtPlane::CellFlags ) {
						c = packRgba( ( fl & 1u ) ? 0.90f : 0.10f,
							( fl & 2u ) ? 0.90f : 0.10f, 0.15f );
					} else {
						const float t = ( reliefMax > 0.0f )
							? qBound( 0.0f, ( hi - lo ) / reliefMax, 1.0f ) : 0.0f;
						c = ( fl & 2u ) ? packRgba( t, t * 0.92f, t * 0.80f )
							: packRgba( 0.05f, 0.05f, 0.08f );
					}
					s.rgba[size_t( j ) * gridW + i] = c;
				}
			}
			const qint64 cellsInRegion = qint64( cellsXr ) * cellsYr;
			if ( spec.plane == LodtPlane::WaterHeight )
				note << QString( "water height: %1 of %2 cells have water, plane %3..%4 units" )
					.arg( water ).arg( cellsInRegion )
					.arg( double( wLo ), 0, 'f', 1 ).arg( double( wHi ), 0, 'f', 1 );
			else if ( spec.plane == LodtPlane::WaterType )
				note << QString( "water type: %1 WATR records; %2 of %3 cells have water "
						"(cyan = the worldspace default, 0xFFFF)" )
					.arg( f.watrCount() ).arg( water ).arg( cellsInRegion );
			else if ( spec.plane == LodtPlane::CellFlags )
				note << QString( "cell flags: %1 of %3 cells have land (green), "
						"%2 have water (red); both = yellow" )
					.arg( land ).arg( water ).arg( cellsInRegion );
			else
				note << QString( "cell height range: %1 land cells, deepest relief %2 units" )
					.arg( land ).arg( double( reliefMax ), 0, 'f', 1 );
			break;
		}

		case LodtPlane::CoarseOverview: {
			const int ovS = f.overviewSamples();
			float lo = 3.4e38f, hi = -3.4e38f, maxDelta = 0.0f;
			for ( int j = 0; j < gridH; j++ ) {
				const int gy = gyOf( j );
				for ( int i = 0; i < gridW; i++ ) {
					const float h = f.overviewHeight( gxOf( i ) * ovS / spc, gy * ovS / spc );
					lo = qMin( lo, h );
					hi = qMax( hi, h );
				}
			}
			/* The overview is a SUBSAMPLE of the same terrain, so on the exact
			 * overview lattice it must equal the block height; anything else
			 * means the two sections disagree about the world. */
			for ( int cy = spec.y0; cy <= spec.y1; cy++ ) {
				for ( int cx = spec.x0; cx <= spec.x1; cx++ ) {
					for ( int q = 0; q < ovS; q++ ) {
						const int ox = ( cx - info.cellMinX ) * ovS + q;
						const int oy = ( cy - info.cellMinY ) * ovS + q;
						const float a = f.overviewHeight( ox, oy );
						const float b = f.height( ox * ( spc / ovS ), oy * ( spc / ovS ) );
						maxDelta = qMax( maxDelta, std::fabs( a - b ) );
					}
				}
			}
			for ( int j = 0; j < gridH; j++ ) {
				const int gy = gyOf( j );
				for ( int i = 0; i < gridW; i++ ) {
					const float h = f.overviewHeight( gxOf( i ) * ovS / spc, gy * ovS / spc );
					const float t = ( hi > lo ) ? ( h - lo ) / ( hi - lo ) : 0.5f;
					s.rgba[size_t( j ) * gridW + i] = packRgba( t, t, t * 0.9f + 0.1f );
				}
			}
			note << QString( "coarse overview: %1x%2 at %3 a cell, heights %4..%5; "
					"greatest disagreement with the block pyramid on the overview "
					"lattice %6 units (quantum %7)" )
				.arg( cellsXall * ovS ).arg( f.cellsY() * ovS ).arg( ovS )
				.arg( double( lo ), 0, 'f', 1 ).arg( double( hi ), 0, 'f', 1 )
				.arg( double( maxDelta ), 0, 'f', 3 )
				.arg( double( info.heightQuantum ), 0, 'f', 2 );
			break;
		}

		default:
			break;
		}
	}

	const qint64 msPlane = timer.elapsed();
	qint64 shapeCount = 0, vertCount = 0;
	tileCounts( cellsXr, cellsYr, n, shapeCount, vertCount );
	const qint64 gridBytes = qint64( gridW ) * gridH
		* ( spec.plane == LodtPlane::Height ? 4 : 8 );
	note << QString( "%1 shapes, %L2 vertices; sample grids %L3 bytes; "
			"plane read at %4 ms" )
		.arg( shapeCount ).arg( vertCount ).arg( gridBytes ).arg( msPlane );

	if ( !buildTerrainSurface( nif, s, error ) )
		return false;

	note << QString( "meshed and built at %1 ms" ).arg( timer.elapsed() );
	if ( notes )
		*notes = note.join( QStringLiteral( "\n" ) );
	if ( error )
		error->clear();
	return true;
}

bool lodtQueryRegion( QWidget * parent, const QString & path,
	const LodtWorldInfo & info, LodtRegionSpec & spec )
{
	// the harness route: no dialog, the view comes from the environment
	if ( lodtRegionFromEnv( spec ) )
		return true;

	const LodtRegionSpec fallback = lodtDefaultRegion( info );
	const QList<LodtPlane> planes = lodtAvailablePlanes( info );

	QDialog dlg( parent );
	dlg.setWindowTitle( QCoreApplication::translate( "btdterrain", "Open Terrain — %1" )
		.arg( QFileInfo( path ).fileName() ) );

	auto layout = new QVBoxLayout( &dlg );
	auto infoLabel = new QLabel(
		QCoreApplication::translate( "btdterrain",
			"Worldspace cells [%1,%2]..[%3,%4], heights %5 to %6 at a quantum of %7.\n"
			"%8 samples a cell, %9 LOD levels, %10 land textures, %11 water types.\n"
			"This file stores planes, not meshes; choose how much to build and which "
			"plane to see." )
			.arg( info.cellMinX ).arg( info.cellMinY )
			.arg( info.cellMaxX ).arg( info.cellMaxY )
			.arg( double( info.heightMin ), 0, 'f', 0 )
			.arg( double( info.heightMax ), 0, 'f', 0 )
			.arg( double( info.heightQuantum ), 0, 'f', 2 )
			.arg( info.samplesPerCell ).arg( info.levelCount )
			.arg( info.ltexCount ).arg( info.watrCount ), &dlg );
	layout->addWidget( infoLabel );

	auto grid = new QGridLayout();
	layout->addLayout( grid );
	auto makeSpin = [&]( int row, int col, const QString & label, int value ) {
		grid->addWidget( new QLabel( label, &dlg ), row, col );
		auto spin = new QSpinBox( &dlg );
		spin->setRange( qMin( info.cellMinX, info.cellMinY ), qMax( info.cellMaxX, info.cellMaxY ) );
		spin->setValue( value );
		grid->addWidget( spin, row, col + 1 );
		return spin;
	};
	auto spinX0 = makeSpin( 0, 0, QStringLiteral( "West cell X" ), fallback.x0 );
	auto spinX1 = makeSpin( 0, 2, QStringLiteral( "East cell X" ), fallback.x1 );
	auto spinY0 = makeSpin( 1, 0, QStringLiteral( "South cell Y" ), fallback.y0 );
	auto spinY1 = makeSpin( 1, 2, QStringLiteral( "North cell Y" ), fallback.y1 );

	grid->addWidget( new QLabel( QStringLiteral( "Detail" ), &dlg ), 2, 0 );
	auto lodBox = new QComboBox( &dlg );
	const int maxLod = lodtMaxLod( info );
	for ( int l = 0; l <= maxLod; l++ )
		lodBox->addItem( QCoreApplication::translate( "btdterrain",
			"LOD %1 — %2 samples a cell, one every %3 units" )
			.arg( l ).arg( lodtRate( info, l ) )
			.arg( 4096 / lodtRate( info, l ) ) );
	lodBox->setCurrentIndex( qBound( 0, fallback.lod, maxLod ) );
	grid->addWidget( lodBox, 2, 1, 1, 3 );

	grid->addWidget( new QLabel( QStringLiteral( "Plane" ), &dlg ), 3, 0 );
	auto planeBox = new QComboBox( &dlg );
	for ( LodtPlane p : planes )
		planeBox->addItem( lodtPlaneLabel( p ), int( p ) );
	planeBox->setCurrentIndex( qMax( 0, int( planes.indexOf( spec.plane ) ) ) );
	grid->addWidget( planeBox, 3, 1, 1, 3 );

	auto estimate = new QLabel( &dlg );
	layout->addWidget( estimate );

	auto buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg );
	layout->addWidget( buttons );
	QObject::connect( buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept );
	QObject::connect( buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject );

	auto currentSpec = [&]() {
		LodtRegionSpec s;
		s.x0 = spinX0->value();
		s.x1 = spinX1->value();
		s.y0 = spinY0->value();
		s.y1 = spinY1->value();
		s.lod = lodBox->currentIndex();
		s.plane = LodtPlane( planeBox->currentData().toInt() );
		return s;
	};
	auto refresh = [&]() {
		qint64 shapes = 0, vertCount = 0;
		QString why;
		const bool ok = lodtEstimateRegion( info, currentSpec(), &shapes, &vertCount, &why );
		estimate->setText( ok
			? QCoreApplication::translate( "btdterrain", "%L1 terrain shapes, %L2 vertices." )
				.arg( shapes ).arg( vertCount )
			: why );
		buttons->button( QDialogButtonBox::Ok )->setEnabled( ok );
	};
	for ( auto spin : { spinX0, spinX1, spinY0, spinY1 } )
		QObject::connect( spin, qOverload<int>( &QSpinBox::valueChanged ), &dlg, refresh );
	QObject::connect( lodBox, qOverload<int>( &QComboBox::currentIndexChanged ), &dlg, refresh );
	QObject::connect( planeBox, qOverload<int>( &QComboBox::currentIndexChanged ), &dlg, refresh );
	refresh();

	if ( dlg.exec() != QDialog::Accepted )
		return false;

	spec = currentSpec();
	spec.valid = true;
	return true;
}

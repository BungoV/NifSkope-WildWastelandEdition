/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "btdterrain.h"

#include "model/nifmodel.h"
#include "spells/blocks.h"

#include "btdfile.hpp"
#include "io/lodvfile.h"
#include "lodinative.h"
#include "lodtfile.h"
#include "lodtsheets.h"

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
#include <QSet>
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

/* `cellsPerTile( lod )` was the terrain horizon channel's only caller and went
 * with it (lane HORIZONOUT, 2026-09-19); `cellsPerTileForRate` above is the one
 * every other caller uses, with the sample rate it already has in hand. */

//! Shapes and vertices a cellsX x cellsY region meshes to at n samples a cell.
//! Shared by both formats' estimators so a dialog and its build cannot disagree.
void tileCounts( int cellsX, int cellsY, int n, qint64 & shapeCount, qint64 & vertCount,
	int kOverride = 0 )
{
	const int k = kOverride > 0 ? kOverride : cellsPerTileForRate( n );
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

	/* ---- the `.lodt` sheets, when a pyramid was found (NATIVEVIEW1) ----
	 *
	 *  All of this is inert at its defaults, and a surface built without
	 *  sheets goes through exactly the arithmetic it always did: `uvBias +
	 *  u * uvScale` with bias 0 and scale 1 returns u's own bits, and an empty
	 *  `tileDiffuse` leaves the inline-colour slots alone. That is what makes
	 *  the data view byte-identical before and after this round.
	 *
	 *  `sheetDim` is how many CELLS one sheet tile covers. A mesh tile covers
	 *  `k` cells with `k` a divisor of `sheetDim`, so every mesh tile lies
	 *  inside exactly one sheet tile and takes a sub-rectangle of its UV. The
	 *  sheet tile grid is NORTH-UP (row 0 is the north row) while cells count
	 *  northward, which is why the row index is flipped where it is read.
	 */
	int sheetDim = 0;
	int sheetTilesX = 0, sheetTilesY = 0;
	float uvBias = 0.0f;
	float uvScale = 1.0f;
	//! Indexed `sty * sheetTilesX + stx`, `sty` = 0 the NORTH row. Empty = no sheets.
	std::vector<QString> tileDiffuse;
	std::vector<QString> tileNormal;
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
	/* With sheets the mesh tile has to nest inside a sheet tile, so `k` drops
	 * to the largest DIVISOR of `sheetDim` that still fits under 255 vertices
	 * a side. At LOD2 (32 samples a cell) the cap is 7 cells and `sheetDim` is
	 * 4, so k = 4 and one mesh tile is one sheet tile; at LOD0 (128 a cell)
	 * the cap is 1 and k = 1, so four mesh tiles share a sheet tile and each
	 * takes a quarter of its UV. */
	int k = cellsPerTileForRate( n );
	if ( s.sheetDim > 0 ) {
		int best = 1;
		for ( int d = 1; d <= s.sheetDim; d++ )
			if ( s.sheetDim % d == 0 && d <= k )
				best = d;
		k = best;
	}
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

			/* Which sheet tile this mesh tile sits in, and where inside it.
			 * `sty` is flipped because the sheet grid's row 0 is the NORTH
			 * row while cells count northward. sheetIndex < 0 means no sheets,
			 * and then every line below takes the path it always took. */
			int sheetIndex = -1;
			int sxCells = 0, syCellsFromSouth = 0;
			if ( s.sheetDim > 0 && !s.tileDiffuse.empty() ) {
				const int stx = ( tx * k ) / s.sheetDim;
				const int styFromSouth = ( ty * k ) / s.sheetDim;
				const int sty = s.sheetTilesY - 1 - styFromSouth;
				if ( stx >= 0 && stx < s.sheetTilesX && sty >= 0 && sty < s.sheetTilesY ) {
					sheetIndex = sty * s.sheetTilesX + stx;
					sxCells = ( tx * k ) % s.sheetDim;
					syCellsFromSouth = ( ty * k ) % s.sheetDim;
				}
			}

			verts.clear();
			verts.reserve( wV * hV );
			float zMin = 3.4e38f, zMax = -3.4e38f;
			for ( int j = 0; j < hV; j++ ) {
				for ( int i = 0; i < wV; i++ ) {
					TerrainVert v;
					const float z = height( gx0 + i, gy0 + j );
					v.pos = Vector3( float( i ) * spacing, float( j ) * spacing, z );
					v.nrm = normalAt( gx0 + i, gy0 + j );
					if ( sheetIndex >= 0 ) {
						/* Into the sheet tile's CONTENT square, then inset past
						 * the border: a stored sheet is content + 2*border
						 * texels wide, so 0..1 over the content is
						 * uvBias..uvBias+uvScale over the stored image. */
						const float fu = ( float( sxCells )
							+ float( wCells ) * ( float( i ) / float( wV - 1 ) ) )
							/ float( s.sheetDim );
						const float fv = ( float( s.sheetDim - syCellsFromSouth )
							- float( hCells ) * ( float( j ) / float( hV - 1 ) ) )
							/ float( s.sheetDim );
						v.uv = Vector2( s.uvBias + fu * s.uvScale, s.uvBias + fv * s.uvScale );
					} else {
						v.uv = Vector2( float( i ) / float( wV - 1 ), 1.0f - float( j ) / float( hV - 1 ) );
					}
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
			QString diffuse = s.diffuse;
			QString normal = QStringLiteral( "#FFFF8080" );
			bool sheetNormal = false;
			if ( sheetIndex >= 0 && sheetIndex < int( s.tileDiffuse.size() )
				&& !s.tileDiffuse[size_t( sheetIndex )].isEmpty() ) {
				// the bake's own colour sheet and its _msn, by name, through
				// the resource stack -- not a second copy of the colours
				diffuse = s.tileDiffuse[size_t( sheetIndex )];
				if ( sheetIndex < int( s.tileNormal.size() )
					&& !s.tileNormal[size_t( sheetIndex )].isEmpty() ) {
					normal = s.tileNormal[size_t( sheetIndex )];
					sheetNormal = true;
				}
			}
			nif->set<QString>( nif->getIndex( iTexArray, 0 ), diffuse );
			nif->set<QString>( nif->getIndex( iTexArray, 1 ), normal );
			if ( sheetNormal ) {
				/* The sheet's normal map is an `_msn`, a MODEL-SPACE map. Read
				 * as a tangent-space one it lights the land far too dark (mean
				 * luma 70.6 against the .BTR's 121.1). The .BTR of the same
				 * bake says so with bit 12 of Shader Flags 1, with the specular
				 * bit clear (the two are documented as incompatible), and marks
				 * itself LOD landscape in Shader Flags 2. A sheet-lit tile
				 * takes those three bits and leaves the rest alone. */
				quint32 sf1 = nif->get<quint32>( iShader, "Shader Flags 1" );
				sf1 = ( sf1 | 0x1000u ) & ~0x1u;
				nif->set<quint32>( iShader, "Shader Flags 1", sf1 );
				const quint32 sf2 = nif->get<quint32>( iShader, "Shader Flags 2" );
				nif->set<quint32>( iShader, "Shader Flags 2", sf2 | 0x2u );
			}
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
	case LodtPlane::WaterBodyId:       return "bodyid";
	case LodtPlane::WaterFlow:         return "flow";
	case LodtPlane::WaterShore:        return "shore";
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
	case LodtPlane::WaterBodyId:       return QStringLiteral( "Water body — which sheet of water a texel belongs to" );
	case LodtPlane::WaterFlow:         return QStringLiteral( "Water flow — direction, speed and confidence" );
	case LodtPlane::WaterShore:        return QStringLiteral( "Shore distance — how far a texel is from dry land" );
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
	info.headerVersion = f.headerVersion();
	info.bodyCount = f.bodyCount();
	info.bodySamples = f.bodyIdSamples();
	info.flowSamples = f.flowPlaneSamples();
	info.shoreSamples = f.shorePlaneSamples();
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
	/* The BIT, then the rate: a section bit set over a zero rate is a refusal
	 * in the reader, so by the time a plane is offered here both agree. */
	if ( ( info.sectionFlags & LODL_SECT_BODIES ) && info.bodySamples > 0 && info.bodyCount > 0 )
		out << LodtPlane::WaterBodyId;
	if ( ( info.sectionFlags & LODL_SECT_FLOW ) && info.flowSamples > 0 )
		out << LodtPlane::WaterFlow;
	if ( ( info.sectionFlags & LODL_SECT_SHORE ) && info.shoreSamples > 0 )
		out << LodtPlane::WaterShore;
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
	const LodtRegionSpec & specIn, QString * error, QString * notes )
{
	/* The region can widen here, and only here: a lit view snaps outward to
	 * whole sheet tiles so that no mesh tile straddles two sheets. Every other
	 * route sees the caller's own rectangle. */
	LodtRegionSpec spec = specIn;
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

	/* ---- the `.lodt` sheet pyramid, when one is beside the file ----------
	 *
	 *  A HEIGHT view of a worldspace whose sheets were baked is drawn LIT:
	 *  the bake's own colour sheet in slot 0 and its `_msn` in slot 1, per
	 *  sheet tile. Any other plane, or no pyramid, and this whole block does
	 *  nothing and the inline-colour data view is exactly what it was.
	 */
	LodtSheets sheets;
	QStringList sheetNote;
	/* WW_LODL_CHANNEL=<name>, the terrain half (lane CHANVIEW1). Read once, here,
	 * so the tile binding below and the vertex-colour block further down cannot
	 * disagree about which channel this run is. */
	QString sheetChanGiven;
	int sheetChanBin = 0;
	const LodlChannel sheetChan = lodlChannelFromEnv( &sheetChanGiven, &sheetChanBin );
	bool haveSheets = false;
	int sheetTilesX = 0, sheetTilesY = 0, sheetDim = 0, sheetTilesFound = 0;
	if ( spec.plane == LodtPlane::Height ) {
		QString why;
		if ( sheets.open( lodtPath, &why ) ) {
			sheetDim = qMax( 1, sheets.levelDim() );
			auto floorDiv = []( int a, int b ) {
				return ( a >= 0 ) ? a / b : -( ( -a + b - 1 ) / b );
			};
			const int w = sheets.west(), sth = sheets.south();
			const int sx0 = w + floorDiv( spec.x0 - w, sheetDim ) * sheetDim;
			const int sx1 = w + ( floorDiv( spec.x1 - w, sheetDim ) + 1 ) * sheetDim - 1;
			const int sy0 = sth + floorDiv( spec.y0 - sth, sheetDim ) * sheetDim;
			const int sy1 = sth + ( floorDiv( spec.y1 - sth, sheetDim ) + 1 ) * sheetDim - 1;
			// never past what the sheets hold, nor past what the .lodl holds
			if ( sx0 >= sheets.west() && sx1 <= sheets.east()
				&& sy0 >= sheets.south() && sy1 <= sheets.north()
				&& sx0 >= info.cellMinX && sx1 <= info.cellMaxX
				&& sy0 >= info.cellMinY && sy1 <= info.cellMaxY ) {
				if ( sx0 != spec.x0 || sy0 != spec.y0 || sx1 != spec.x1 || sy1 != spec.y1 )
					sheetNote << QString( "region widened from [%1,%2]..[%3,%4] to whole "
							"sheet tiles of %5 cells" )
						.arg( spec.x0 ).arg( spec.y0 ).arg( spec.x1 ).arg( spec.y1 )
						.arg( sheetDim );
				spec.x0 = sx0;
				spec.y0 = sy0;
				spec.x1 = sx1;
				spec.y1 = sy1;
				sheetTilesX = ( sx1 - sx0 + 1 ) / sheetDim;
				sheetTilesY = ( sy1 - sy0 + 1 ) / sheetDim;
				haveSheets = true;
			} else {
				sheetNote << QString( "sheets cover cells [%1,%2]..[%3,%4] and this region "
						"snaps outside that, so the data view is drawn instead" )
					.arg( sheets.west() ).arg( sheets.south() )
					.arg( sheets.east() ).arg( sheets.north() );
			}
		} else {
			sheetNote << QString( "no .lodt sheets (%1); drawing the inline-colour data view" )
				.arg( why );
		}
	}

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

	if ( haveSheets ) {
		s.sheetDim = sheetDim;
		s.sheetTilesX = sheetTilesX;
		s.sheetTilesY = sheetTilesY;
		s.uvBias = sheets.uvBias();
		s.uvScale = sheets.uvScale();
		s.tileDiffuse.assign( size_t( sheetTilesX ) * size_t( sheetTilesY ), QString() );
		s.tileNormal.assign( s.tileDiffuse.size(), QString() );
		QStringList missing;
		for ( int sty = 0; sty < sheetTilesY; sty++ ) {
			// sty 0 is the NORTH row of the region, the sheets' own order
			const int cellY = spec.y1 - sty * sheetDim;
			for ( int stx = 0; stx < sheetTilesX; stx++ ) {
				const int cellX = spec.x0 + stx * sheetDim;
				int gx = 0, gy = 0;
				if ( !sheets.tileOfCell( cellX, cellY, &gx, &gy ) )
					continue;
				LodtSheetTile t;
				QString why;
				if ( !sheets.tile( gx, gy, t, &why ) ) {
					if ( missing.size() < 4 )
						missing << QString( "(%1,%2) %3" ).arg( gx ).arg( gy ).arg( why );
					continue;
				}
				const size_t at = size_t( sty ) * size_t( sheetTilesX ) + size_t( stx );
				/* WW_LODL_CHANNEL=normal / =emissive: the sheet BEING ASKED ABOUT is
				 * bound where the colour sheet goes, so what is photographed is that
				 * sheet's own texels and not a re-encoding of them. Unlit is the
				 * caller's WW_LOD_CHANNEL=12 (raw base colour), the same way the
				 * stock `.bto` channel views do it. */
				s.tileDiffuse[at] = ( sheetChan == LodlChannel::Normal && !t.msn.isEmpty() )
					? t.msn
					: ( sheetChan == LodlChannel::Emissive && !t.emissive.isEmpty() )
						? t.emissive : t.colour;
				s.tileNormal[at] = t.msn;
				sheetTilesFound++;
			}
		}
		if ( sheetTilesFound == 0 ) {
			// nothing to draw with: fall all the way back rather than render black
			s.sheetDim = 0;
			s.tileDiffuse.clear();
			s.tileNormal.clear();
			s.uvBias = 0.0f;
			s.uvScale = 1.0f;
			haveSheets = false;
			sheetNote << QStringLiteral( "no sheet tile of this region unpacked; "
				"drawing the inline-colour data view" );
		}
		if ( !missing.isEmpty() )
			sheetNote << QString( "%1 sheet tiles could not be unpacked: %2" )
				.arg( missing.size() ).arg( missing.join( QStringLiteral( "; " ) ) );
		if ( haveSheets ) {
			sheetNote << QString( "lit from %1: level dim %2, %3x%4 tiles over this region, "
					"%5 unpacked; UV inset %6 + t x %7 (border %8 of %9 texels)" )
				.arg( QFileInfo( sheets.containerPath() ).fileName() )
				.arg( sheetDim ).arg( sheetTilesX ).arg( sheetTilesY ).arg( sheetTilesFound )
				.arg( double( s.uvBias ), 0, 'f', 5 ).arg( double( s.uvScale ), 0, 'f', 5 )
				.arg( sheets.borderTexels() ).arg( sheets.storedTexels() );
			sheetNote << sheets.notes();
		}
	}
	note << sheetNote;

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

		case LodtPlane::WaterBodyId:
		case LodtPlane::WaterFlow:
		case LodtPlane::WaterShore: {
			/* The three version-3 water planes. Each has its OWN sample rate in
			 * the header, which need not be the file's, so the grid position is
			 * mapped into the plane's grid rather than assumed equal to it.
			 *
			 * Body ID is drawn as a categorical hue -- the picture that answers
			 * "or at least an ID for them" on sight -- and NEAREST, never
			 * blended: an id is a name, not a quantity, and an average of two
			 * names is a third body that does not exist. */
			const int rate = qMax( 1, spec.plane == LodtPlane::WaterFlow
				? info.flowSamples
				: ( spec.plane == LodtPlane::WaterShore ? info.shoreSamples : info.bodySamples ) );
			const int idRate = qMax( 1, info.bodySamples );
			qint64 wetSamples = 0, flowing = 0;
			quint16 maxId = 0;
			QSet<quint16> distinct;
			int shoreMin = 255, shoreMax = 0;
			for ( int j = 0; j < gridH; j++ ) {
				const int gy = gyOf( j );
				for ( int i = 0; i < gridW; i++ ) {
					const int gx = gxOf( i );
					const int px = int( qint64( gx ) * rate / spc );
					const int py = int( qint64( gy ) * rate / spc );
					const quint16 id = f.bodyIdAt( int( qint64( gx ) * idRate / spc ),
						int( qint64( gy ) * idRate / spc ) );
					quint32 c = packRgba( 0.06f, 0.06f, 0.09f );
					if ( id ) {
						wetSamples++;
						maxId = qMax( maxId, id );
						if ( distinct.size() < 4096 )
							distinct.insert( id );
					}
					if ( spec.plane == LodtPlane::WaterBodyId ) {
						if ( id ) {
							float r, g, b;
							hashColour( quint32( id ) * 2654435761u, r, g, b );
							c = packRgba( r, g, b );
						}
					} else if ( spec.plane == LodtPlane::WaterFlow ) {
						const quint16 w = f.flowWordAt( px, py );
						if ( w ) {
							flowing++;
							const float ang = float( w & 0xFF ) / 256.0f;
							const float spd = float( ( w >> 8 ) & 0xF ) / 15.0f;
							/* direction as a hue round the wheel, speed as its
							 * brightness -- so still water inside a body reads
							 * as dark and a fast reach as bright. */
							const float h6 = ang * 6.0f;
							const int sector = int( h6 ) % 6;
							const float frac = h6 - float( int( h6 ) );
							float rr = 0, gg = 0, bb = 0;
							switch ( sector ) {
							case 0: rr = 1; gg = frac; break;
							case 1: rr = 1 - frac; gg = 1; break;
							case 2: gg = 1; bb = frac; break;
							case 3: gg = 1 - frac; bb = 1; break;
							case 4: rr = frac; bb = 1; break;
							default: rr = 1; bb = 1 - frac; break;
							}
							const float k = 0.35f + 0.65f * spd;
							c = packRgba( rr * k, gg * k, bb * k );
						} else if ( id ) {
							c = packRgba( 0.12f, 0.16f, 0.22f );   // in a body, still
						}
					} else {
						const quint8 sv = f.shoreAt( px, py );
						if ( id ) {
							shoreMin = qMin( shoreMin, int( sv ) );
							shoreMax = qMax( shoreMax, int( sv ) );
							const float t = float( sv ) / 255.0f;
							c = packRgba( 0.05f + 0.20f * t, 0.25f + 0.55f * t, 0.45f + 0.50f * t );
						}
					}
					s.rgba[size_t( j ) * gridW + i] = c;
				}
			}
			const qint64 total = qint64( gridW ) * gridH;
			if ( spec.plane == LodtPlane::WaterBodyId )
				note << QString( "water bodies: %1 in the table, %2 of %3 samples name one, "
						"%4 distinct here, highest id %5 (nearest sampling: an id is a name, "
						"never an average)" )
					.arg( info.bodyCount ).arg( wetSamples ).arg( total )
					.arg( distinct.size() ).arg( maxId );
			else if ( spec.plane == LodtPlane::WaterFlow )
				note << QString( "water flow: %1 of %2 wet samples carry a direction "
						"(%3 of the region); the rest are still water, which is the same "
						"word as dry on purpose" )
					.arg( flowing ).arg( wetSamples ).arg( total );
			else
				note << QString( "shore distance: %1 wet samples, stored steps %2..%3 "
						"(x %4 world units, 255 saturates)" )
					.arg( wetSamples ).arg( wetSamples ? shoreMin : 0 ).arg( shoreMax )
					.arg( double( f.shoreQuantum() ), 0, 'f', 0 );
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

	/* WW_LODL_AO=1: the LIT view with the file's AO plane multiplied in as a
	 * per-vertex grey (bungo 2026-09-18, "show me the same area on the map,
	 * with AO overlaid on the terrain and objects / trees"). The lit view has
	 * never shown the plane -- the sheets carry colour and normal, the AO
	 * rides in the mask sheet's B the viewer does not bind -- so a picture
	 * of "the bake" was a picture without its occlusion. The same sampling
	 * as the AmbientOcclusion plane view, into the same vertex colour the
	 * plane views use; the object half does its own in src/lodinative.cpp.
	 * Unset, not a byte of the document changes. */
	/* WW_LODL_CHANNEL widens exactly this block (lane CHANVIEW1): `ao` IS
	 * WW_LODL_AO, `mask-b` is the same texel read under its own name, and
	 * `mask-r` / `mask-g` / `mask-a` are the other three channels of the SAME
	 * sampler -- one decoder, `LodtSheets::sheetChannel`. `ground` draws the
	 * terrain at the ramp's value AT THE SURFACE so the placements' per-placement
	 * blend can be read against the thing they blend into. */
	if ( !sheetChanGiven.isEmpty() && sheetChan == LodlChannel::None )
		note << QString( "WW_LODL_CHANNEL: REFUSED \"%1\" -- no such channel; the terrain is "
				"the default one. Known names: %2" )
			.arg( sheetChanGiven ).arg( lodlChannelNames() );
	/* `normal` and `emissive` are PICTURES OF A SHEET, bound above in place of
	 * the colour sheet. Their note line still has to carry numbers, and they come
	 * out of the SAME sampler the mask channels use -- the same container bytes
	 * the bound DDS was unpacked from, decoded once here for the census. */
	if ( sheetChan == LodlChannel::Normal || sheetChan == LodlChannel::Emissive ) {
		const int role = ( sheetChan == LodlChannel::Normal )
			? LODV_ROLE_MSN : LODV_ROLE_EMISSIVE;
		if ( !sheets.isOpen() ) {
			note << QString( "WW_LODL_CHANNEL=%1: no sheet container is open for this .lodl; "
					"nothing drawn differently" ).arg( lodlChannelName( sheetChan ) );
		} else if ( !sheets.hasRole( role ) ) {
			note << QString( "WW_LODL_CHANNEL=%1: %2 sheet ABSENT -- %3 carries no sheet with "
					"role %4; nothing drawn differently" )
				.arg( lodlChannelName( sheetChan ) )
				.arg( lodlChannelName( sheetChan ) )
				.arg( QFileInfo( sheets.containerPath() ).fileName() ).arg( role );
		} else {
			static const char * const CH = "RGB";
			QStringList per;
			qint64 texels = 0;
			int tiles = 0;
			for ( int ch = 0; ch < 3; ch++ ) {
				int lo = 255, hi = 0;
				double sum = 0.0;
				qint64 n = 0;
				int seen = 0;
				for ( int sty = 0; sty < sheetTilesY; sty++ ) {
					const int cellY = spec.y1 - sty * sheetDim;
					for ( int stx = 0; stx < sheetTilesX; stx++ ) {
						const int cellX = spec.x0 + stx * sheetDim;
						int gx = 0, gy = 0;
						if ( !sheets.tileOfCell( cellX, cellY, &gx, &gy ) )
							continue;
						std::vector<quint8> bytes;
						QString why;
						if ( !sheets.sheetChannel( role, gx, gy, ch, bytes, &why ) )
							continue;
						seen++;
						const int dim = sheets.storedTexels(), b = sheets.borderTexels();
						for ( int y = b; y < dim - b; y++ )
							for ( int x = b; x < dim - b; x++ ) {
								const int v = bytes[size_t( y ) * size_t( dim ) + size_t( x )];
								lo = qMin( lo, v );
								hi = qMax( hi, v );
								sum += v;
								n++;
							}
					}
				}
				tiles = qMax( tiles, seen );
				texels = qMax( texels, n );
				per << ( n == 0 ? QString( "%1 none" ).arg( QChar( QLatin1Char( CH[ch] ) ) )
					: lo == hi ? QString( "%1 constant %2" ).arg( QChar( QLatin1Char( CH[ch] ) ) ).arg( lo )
					: QString( "%1 %2..%3 mean %4" ).arg( QChar( QLatin1Char( CH[ch] ) ) )
						.arg( lo ).arg( hi ).arg( sum / double( n ), 0, 'f', 3 ) );
			}
			note << QString( "WW_LODL_CHANNEL=%1: the role-%2 sheet of %3 bound as the terrain's "
					"base colour, %4 tiles, %L5 content texels a channel; %6" )
				.arg( lodlChannelName( sheetChan ) ).arg( role )
				.arg( QFileInfo( sheets.containerPath() ).fileName() )
				.arg( tiles ).arg( texels ).arg( per.join( QStringLiteral( ", " ) ) );
		}
	}
	const bool wantAoT = qEnvironmentVariableIntValue( "WW_LODL_AO" ) != 0
		|| sheetChan == LodlChannel::Ao;
	int maskCh = -1;
	switch ( sheetChan ) {
	case LodlChannel::MaskR: maskCh = 0; break;
	case LodlChannel::MaskG: maskCh = 1; break;
	case LodlChannel::MaskB: maskCh = 2; break;
	case LodlChannel::MaskA: maskCh = 3; break;
	default: break;
	}
	const bool maskNamed = maskCh >= 0;
	if ( !maskNamed && wantAoT )
		maskCh = 2;                          // the AO read: the mask sheet's B
	const QString chanLabel = maskNamed
		? QString( "WW_LODL_CHANNEL=%1" ).arg( lodlChannelName( sheetChan ) )
		: QStringLiteral( "WW_LODL_AO" );
	if ( spec.plane == LodtPlane::Height && s.rgba.empty()
		&& sheetChan == LodlChannel::Ground ) {
		/* The ramp is 1 at or below the surface falling to 0 over 256 world units
		 * above it (src/lodgen.cpp ~4008), so the surface's own value is 255. The
		 * terrain is drawn at that value -- the top of the same grey ramp the
		 * placements are drawn in -- and a placement reads as its distance off
		 * the ground against a white floor. */
		s.rgba.assign( s.z.size(), packRgba( 1.0f, 1.0f, 1.0f ) );
		note << QString( "WW_LODL_CHANNEL=ground: the terrain drawn at the contact ramp's value "
				"AT THE SURFACE (constant 255) in the same grey ramp as the placements, "
				"%L1 vertices; the per-placement bytes are in the objects' note line" )
			.arg( qint64( s.z.size() ) );
	}
	if ( spec.plane == LodtPlane::Height && s.rgba.empty()
		&& ( wantAoT || maskNamed ) ) {
		const int aoS = f.aoSamples();
		/* The TEXTURE is the terrain's AO: the mask sheet's B, per texel, with
		 * the placed objects' occlusion folded in by the bake. The 8-a-cell
		 * .lodl plane below is the coarse ring-0 source and read as vertex
		 * colour it lands as 512-unit squares (bungo 2026-09-18, "why is the
		 * terrain AO so low res? You can see the pixels there"). With sheets
		 * open, every terrain vertex samples the mask B bilinearly instead;
		 * the plane is the fallback when there are no sheets or no mask. */
		bool fromMask = false;
		if ( sheets.isOpen() && sheetDim > 0 ) {
			QHash<int, std::vector<quint8>> maskTiles;
			const int dim = sheets.storedTexels();
			const float bias = sheets.uvBias() * float( dim ), scale = sheets.uvScale() * float( dim );
			int lo = 255, hi = 0, missing = 0;
			QString maskWhy;
			double sum = 0.0;
			std::vector<quint32> rgba( s.z.size(), packRgba( 1.0f, 1.0f, 1.0f ) );
			for ( int j = 0; j < gridH; j++ ) {
				const int gy = gyOf( j );
				const int cellY = info.cellMinY + gy / spc;
				const float fy = float( gy % spc ) / float( spc );
				for ( int i = 0; i < gridW; i++ ) {
					const int gx = gxOf( i );
					const int cellX = info.cellMinX + gx / spc;
					const float fx = float( gx % spc ) / float( spc );
					int tx, ty;
					if ( !sheets.tileOfCell( cellX, cellY, &tx, &ty ) ) {
						missing++;
						continue;
					}
					const int key = ty * 65536 + tx;
					auto it = maskTiles.find( key );
					if ( it == maskTiles.end() ) {
						std::vector<quint8> bytes;
						QString why;
						if ( !sheets.sheetChannel( LODV_ROLE_MASK, tx, ty, maskCh, bytes, &why )
							&& maskWhy.isEmpty() )
							maskWhy = why;
						it = maskTiles.insert( key, bytes );
					}
					if ( it.value().empty() ) {
						missing++;
						continue;
					}
					// the same content-square mapping the mesh builder gives the colour sheet
					const int westCell = sheets.west() + tx * sheetDim;
					const int southCell = sheets.north() - ( ty + 1 ) * sheetDim + 1;
					const float u = ( float( cellX - westCell ) + fx ) / float( sheetDim );
					const float v = ( float( sheetDim ) - float( cellY - southCell ) - fy ) / float( sheetDim );
					const float px = bias + u * scale - 0.5f, py = bias + v * scale - 0.5f;
					const int x0 = qBound( 0, int( std::floor( px ) ), dim - 1 ), y0 = qBound( 0, int( std::floor( py ) ), dim - 1 );
					const int x1 = qMin( x0 + 1, dim - 1 ), y1 = qMin( y0 + 1, dim - 1 );
					const float ax = qBound( 0.0f, px - float( x0 ), 1.0f ), ay = qBound( 0.0f, py - float( y0 ), 1.0f );
					const std::vector<quint8> & m = it.value();
					const float top = float( m[size_t( y0 ) * dim + x0] ) * ( 1.0f - ax ) + float( m[size_t( y0 ) * dim + x1] ) * ax;
					const float bot = float( m[size_t( y1 ) * dim + x0] ) * ( 1.0f - ax ) + float( m[size_t( y1 ) * dim + x1] ) * ax;
					const float val = top * ( 1.0f - ay ) + bot * ay;
					const int vi = int( val + 0.5f );
					lo = qMin( lo, vi );
					hi = qMax( hi, vi );
					sum += val;
					const float g = val / 255.0f;
					rgba[size_t( j ) * gridW + i] = packRgba( g, g, g );
				}
			}
			if ( !maskTiles.isEmpty() && missing < int( s.z.size() ) ) {
				s.rgba.swap( rgba );
				fromMask = true;
				static const char * const CH = "RGBA";
				const qint64 got = qint64( s.z.size() ) - missing;
				note << QString( "%1: terrain %2 from the MASK SHEET'S %3 (the texture, %4), %5 texels a cell, "
						"bilinear a vertex; %6 tiles read, %7 vertices without a tile (drawn open); values %8..%9, mean %10" )
					.arg( chanLabel )
					.arg( maskNamed ? lodlChannelName( sheetChan ) : QStringLiteral( "AO" ) )
					.arg( QChar( QLatin1Char( CH[maskCh] ) ) )
					.arg( QFileInfo( sheets.containerPath() ).fileName() )
					.arg( sheets.contentTexels() / qMax( 1, sheetDim ) ).arg( maskTiles.size() ).arg( missing )
					.arg( lo ).arg( hi ).arg( sum / double( qMax( qint64( 1 ), got ) ), 0, 'f', 1 );
				if ( lo == hi )
					note << QString( "%1: constant %2 over this chunk" ).arg( chanLabel ).arg( lo );
				/* The second number: the same decoded tiles censused over their
				 * CONTENT texels (borders out), which is what an independent
				 * reader of the container counts. The line above is the vertex
				 * resample and is deliberately not the same number -- 129x129
				 * grid vertices are not 4,194,304 texels -- and printing both is
				 * what lets the two decoders be compared instead of argued about. */
				const int sdim = sheets.storedTexels(), sb = sheets.borderTexels();
				int tlo = 255, thi = 0;
				double tsum = 0.0;
				qint64 tn = 0;
				for ( auto tit = maskTiles.constBegin(); tit != maskTiles.constEnd(); ++tit ) {
					if ( int( tit.value().size() ) < sdim * sdim )
						continue;
					for ( int y = sb; y < sdim - sb; y++ )
						for ( int x = sb; x < sdim - sb; x++ ) {
							const int v = tit.value()[size_t( y ) * size_t( sdim ) + size_t( x )];
							tlo = qMin( tlo, v );
							thi = qMax( thi, v );
							tsum += v;
							tn++;
						}
				}
				if ( tn )
					note << QString( "%1: over the sheet's own CONTENT texels, %L2 texels of %3 tiles, "
							"values %4..%5, mean %6" )
						.arg( chanLabel ).arg( tn ).arg( maskTiles.size() )
						.arg( tlo ).arg( thi ).arg( tsum / double( tn ), 0, 'f', 3 );
			} else if ( maskNamed ) {
				note << QString( "%1: ABSENT on this bake -- %2; the lit view is unchanged" )
					.arg( chanLabel )
					.arg( maskWhy.isEmpty() ? QStringLiteral( "no tile of this region could be read" )
						: maskWhy );
			}
		}
		if ( fromMask || maskNamed ) {
			/* A NAMED mask channel never falls back to the 8-a-cell `.lodl` AO
			 * plane: that plane is the AO and nothing else, and drawing it under
			 * the name `mask-r` would be a proxy shown as the channel
			 * (root MISTAKES 05:0x). Absent is said in words instead. */
		} else if ( aoS > 0 ) {
			s.rgba.assign( s.z.size(), packRgba( 1.0f, 1.0f, 1.0f ) );
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
			note << QString( "WW_LODL_AO: terrain AO plane multiplied into the lit view "
					"as vertex colour, %1 a cell, values %2..%3, mean %4" )
				.arg( aoS ).arg( lo ).arg( hi )
				.arg( sum / double( qint64( gridW ) * gridH ), 0, 'f', 1 );
		} else {
			note << QStringLiteral( "WW_LODL_AO: this file carries no AO plane; lit view unchanged" );
		}
	}

	const qint64 msPlane = timer.elapsed();
	qint64 shapeCount = 0, vertCount = 0;
	/* With sheets the mesh tile shrinks to a divisor of the sheet tile, so the
	 * counts have to be taken at the size the mesher will actually use -- a
	 * reported number that does not match what was built is worse than none. */
	int kBuilt = 0;
	if ( haveSheets ) {
		const int cap = cellsPerTileForRate( n );
		for ( int d = 1; d <= sheetDim; d++ )
			if ( sheetDim % d == 0 && d <= cap )
				kBuilt = d;
	}
	tileCounts( cellsXr, cellsYr, n, shapeCount, vertCount, kBuilt );
	if ( kBuilt > 0 )
		note << QString( "mesh tile forced to %1 cells (the sheet tile is %2, the rate's own "
				"cap is %3)" ).arg( kBuilt ).arg( sheetDim ).arg( cellsPerTileForRate( n ) );
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

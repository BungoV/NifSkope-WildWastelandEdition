/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "btdterrain.h"

#include "model/nifmodel.h"
#include "spells/blocks.h"

#include "btdfile.hpp"

#include <QCoreApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
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

/* A .btd stores no triangles. It is the raw terrain database of a whole
 * worldspace — per-cell heightmaps at five detail levels, land-texture blend
 * layers, ground cover, terrain color — which the FO76 engine meshes at
 * runtime the way FO4 shipped pre-baked .btr meshes. So "opening" one means
 * choosing a region and a detail level and BUILDING the mesh here, from the
 * heightmap, at 128>>lod samples per 4096-unit cell.
 *
 * The parser is fo76utils' own BTDFile (lib/libfo76utils/src/btdfile.cpp),
 * vendored verbatim from the author whose library this codebase already
 * carries; the format spec is the comment block at the top of that file.
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
int cellsPerTile( int lod )
{
	return qMax( 1, MAX_TILE_VERTS_PER_SIDE / samplesPerCell( lod ) );
}

struct TerrainVert
{
	Vector3 pos, nrm;
	Vector2 uv;
};

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

	const int n = samplesPerCell( spec.lod );
	const int k = cellsPerTile( spec.lod );
	const int cellsX = spec.x1 - spec.x0 + 1;
	const int cellsY = spec.y1 - spec.y0 + 1;
	const int tilesX = ( cellsX + k - 1 ) / k;
	const int tilesY = ( cellsY + k - 1 ) / k;

	qint64 vertCount = 0;
	for ( int ty = 0; ty < tilesY; ty++ ) {
		const qint64 hCells = qMin( k, cellsY - ty * k );
		for ( int tx = 0; tx < tilesX; tx++ ) {
			const qint64 wCells = qMin( k, cellsX - tx * k );
			vertCount += ( wCells * n + 1 ) * ( hCells * n + 1 );
		}
	}
	const qint64 shapeCount = qint64( tilesX ) * tilesY;
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
	const int k = cellsPerTile( lod );
	const float spacing = 4096.0f / float( n );
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

	const float zScale = ( info.heightMax - info.heightMin ) / 65535.0f;
	auto height = [&]( int gx, int gy ) -> float {
		return info.heightMin + float( grid[size_t( gy ) * gridW + gx] ) * zScale;
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
	nif->set<QString>( iRoot, "Name",
		QString( "%1 [%2,%3]..[%4,%5] LOD%6" )
			.arg( QFileInfo( btdPath ).completeBaseName() )
			.arg( spec.x0 ).arg( spec.y0 ).arg( spec.x1 ).arg( spec.y1 ).arg( lod ) );
	nif->set<quint32>( iRoot, "Flags", 14 );
	nif->set<float>( iRoot, "Scale", 1.0f );

	// full-precision layout, 28 bytes a vertex — the same descriptor the
	// starter cube and the collision proxy use, known to load and render
	const std::uint64_t vertexDesc = 0x0041B00000650407ULL;
	const int stride = 28;

	const int tilesX = ( cellsX + k - 1 ) / k;
	const int tilesY = ( cellsY + k - 1 ) / k;
	QVector<TerrainVert> verts;
	QVector<Triangle> tris;

	for ( int ty = 0; ty < tilesY; ty++ ) {
		const int hCells = qMin( k, cellsY - ty * k );
		const int hV = hCells * n + 1;
		for ( int tx = 0; tx < tilesX; tx++ ) {
			const int wCells = qMin( k, cellsX - tx * k );
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
				QString( "Terrain %1,%2" ).arg( spec.x0 + tx * k ).arg( spec.y0 + ty * k ) );
			nif->set<quint32>( iShape, "Flags", 14 );
			nif->set<float>( iShape, "Scale", 1.0f );
			nif->set<Vector3>( iShape, "Translation",
				Vector3( float( spec.x0 + tx * k ) * 4096.0f,
					float( spec.y0 + ty * k ) * 4096.0f, 0.0f ) );

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

			// neutral grey through the renderer's inline-colour texture syntax,
			// as the starter scene does; land-texture blending is a later round
			QModelIndex iShader = nif->insertNiBlock( QStringLiteral( "BSLightingShaderProperty" ) );
			QModelIndex iTextures = nif->insertNiBlock( QStringLiteral( "BSShaderTextureSet" ) );
			nif->setLink( iShader, "Texture Set", nif->getBlockNumber( iTextures ) );
			nif->set<uint>( iTextures, "Num Textures", 10 );
			nif->updateArraySize( iTextures, "Textures" );
			QModelIndex iTexArray = nif->getIndex( iTextures, "Textures" );
			nif->set<QString>( nif->getIndex( iTexArray, 0 ), QStringLiteral( "#FF808080" ) );
			nif->set<QString>( nif->getIndex( iTexArray, 1 ), QStringLiteral( "#FFFF8080" ) );
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

#include "probefar.h"

#include "lodifile.h"
#include "lodofile.h"
#include "lodtfile.h"
#include "lodtsheets.h"
#include "probebake.h"
#include "io/lodvfile.h"

#include "ddstxt16.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QTextStream>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>

/* THE FAR MAP (lane PRTPFAR; docs/PRTP_PLAN.md 2h). See probefar.h. */

namespace {

constexpr float kCell = 4096.0f;

float srgbToLinear( float c )
{
	return c <= 0.04045f ? c / 12.92f : std::pow( ( c + 0.055f ) / 1.055f, 2.4f );
}

quint8 toByte( float lin )
{
	return quint8( std::clamp( int( std::lround( lin * 255.0f ) ), 0, 255 ) );
}

// sRGB 110, 100, 85: the bake's own stated dirt tone when no colour is known (probealbedo.cpp)
const quint8 kDirt[3] = { toByte( srgbToLinear( 110 / 255.0f ) ), toByte( srgbToLinear( 100 / 255.0f ) ),
	toByte( srgbToLinear( 85 / 255.0f ) ) };
// open water seen from above: dark, slightly blue-green (sRGB 40, 60, 66)
const quint8 kWater[3] = { toByte( srgbToLinear( 40 / 255.0f ) ), toByte( srgbToLinear( 60 / 255.0f ) ),
	toByte( srgbToLinear( 66 / 255.0f ) ) };
// a building box: weathered concrete and brick, sRGB 120, 112, 102
const quint8 kBuilding[3] = { toByte( srgbToLinear( 120 / 255.0f ) ), toByte( srgbToLinear( 112 / 255.0f ) ),
	toByte( srgbToLinear( 102 / 255.0f ) ) };

struct CellRec
{
	float lo = 0, hi = 0, waterH = 0, roof = -1.0e30f;
	bool ok = false, water = false;
};

/* lane TREE1: THE TREES. The bake knows solid triangles of one color, and a leaf card is mostly holes
 * (its texture keeps 0.08 .. 0.43 of it). So a tree goes in as its coarsest authored LOD model (what the
 * game shows far away), every alpha-tested triangle shrunk about its centroid to the share of it its
 * texture keeps, in the mean color of the texels that pass: to a ray the same area, color and place.
 * Measured against the first-slot models cut out texel by texel (Concord block, 48 probes, irradiance
 * median / 95th percentile): this 0.017 / 0.038, the first slot the same way 0.016 / 0.029, leaf cards
 * left solid 0.038 / 0.092, a solid box a tree 0.289 / 0.741, no trees 0.117 / 0.229. */
// a tree material the LOD folder holds no texture layer for is solid bark, sRGB 74, 62, 48 (counted)
const float kBark[3] = { srgbToLinear( 74 / 255.0f ), srgbToLinear( 62 / 255.0f ), srgbToLinear( 48 / 255.0f ) };

struct TreeTri
{
	float p[9];
	quint8 rgb[3];
};

struct TreeModel
{
	std::vector<TreeTri> tris;   //!< model space, as they go into the soup
	std::vector<float> top;      //!< every authored vertex of every slot, 3 each: the canopy
	double area = 0, areaFull = 0;
};

struct TreeMat
{
	const DDSTexture16 * tex = nullptr;
	int threshold = 0;
};

// a material path as the LOD folder's lists key it: lower case, from "materials\"
QString treeSource( QString s )
{
	s = s.trimmed().toLower().replace( QChar( '/' ), QChar( '\\' ) );
	const int k = s.lastIndexOf( QLatin1String( "materials\\" ) );
	return k > 0 ? s.mid( k ) : s;
}

class FarTrees
{
public:
	FarTrees( const LodoLibrary & l, const QString & lodiPath, bool boxes );
	const TreeModel & model( quint16 baseId, bool mirrored );
	int materials() const { return int( mats.size() ); }
	int noTexture = 0;

private:
	const TreeMat & material( quint16 id );
	void readManifests();
	void texels( const TreeMat & m, const float uv[3][2], float & cover, float lin[3] ) const;

	const LodoLibrary & lib;
	bool solidBoxes;
	QDir dir;
	QString world;
	QHash<QString, QPair<QString, int>> layers;   // material -> the texture array's size class, its layer
	bool manifestsRead = false;
	QHash<QString, QByteArray> arrays;            // size class -> the array file
	std::vector<std::unique_ptr<DDSTexture16>> textures;
	std::map<quint16, TreeMat> mats;
	std::map<quint32, TreeModel> models;
};

FarTrees::FarTrees( const LodoLibrary & l, const QString & lodiPath, bool boxes ) : lib( l ), solidBoxes( boxes )
{
	const QFileInfo fi( lodiPath );
	dir = fi.absoluteDir();
	world = fi.completeBaseName();
	// Objects/<world>.LodgenArrays.txt: family class layer lodm color normal mask emissive source emissiveScale
	QFile list( dir.filePath( QStringLiteral( "Objects/%1.LodgenArrays.txt" ).arg( world ) ) );
	if ( list.open( QIODevice::ReadOnly ) )
		while ( !list.atEnd() ) {
			const QStringList c = QString::fromUtf8( list.readLine() ).trimmed().split( QChar( ' ' ) );
			if ( c.size() < 10 || c[0].startsWith( QChar( '#' ) ) )
				continue;
			const QString key = treeSource( c[c.size() - 2] );
			if ( !layers.contains( key ) )
				layers.insert( key, { c[1], c[2].toInt() } );
		}
}

/* A material the list does not name shares its layer with one it does (the list is keyed on the
 * textures). The chunk manifests say which: `M <shape> <material>` and `A <shape> <layer> <array>`. */
void FarTrees::readManifests()
{
	manifestsRead = true;
	const QStringList names = dir.entryList( { world + QStringLiteral( ".*.BTO.manifest.txt" ) }, QDir::Files, QDir::Name );
	for ( const QString & name : names ) {
		QFile mf( dir.filePath( name ) );
		if ( !mf.open( QIODevice::ReadOnly ) )
			continue;
		std::map<QByteArray, QString> shape;
		QHash<QByteArray, QPair<QString, int>> layer;
		while ( !mf.atEnd() ) {
			const QByteArray ln = mf.readLine().trimmed();
			if ( ln.startsWith( "M " ) ) {
				const int s = ln.indexOf( ' ', 2 );
				if ( s > 0 )
					shape[ln.mid( 2, s - 2 )] = treeSource( QString::fromUtf8( ln.mid( s + 1 ) ) );
			} else if ( ln.startsWith( "A " ) ) {
				const QList<QByteArray> c = ln.split( ' ' );
				// ...\<world>.LodgenArrays.<size class>.lodm
				const QList<QByteArray> dot = c.size() >= 4 ? c.last().split( '.' ) : QList<QByteArray>();
				if ( dot.size() >= 3 && c[2].toInt() >= 0 )
					layer.insert( c[1], { QString::fromUtf8( dot[dot.size() - 2] ), c[2].toInt() } );
			}
		}
		for ( const auto & s : shape )
			if ( !layers.contains( s.second ) && layer.contains( s.first ) )
				layers.insert( s.second, layer.value( s.first ) );
	}
}

const TreeMat & FarTrees::material( quint16 id )
{
	const auto it = mats.find( id );
	if ( it != mats.end() )
		return it->second;
	TreeMat m;
	if ( id < lib.materials.size() ) {
		const LodoMaterial & row = lib.materials[id];
		m.threshold = row.alphaThreshold;
		const QString key = treeSource( lib.stringAt( row.lodmStringOffset ) );
		if ( !layers.contains( key ) && !manifestsRead )
			readManifests();
		const auto l = layers.constFind( key );
		if ( l != layers.constEnd() ) {
			if ( !arrays.contains( l->first ) ) {
				QFile df( dir.filePath( QStringLiteral( "Objects/%1.LodgenArrays.%2_d.DDS" ).arg( world, l->first ) ) );
				arrays.insert( l->first, df.open( QIODevice::ReadOnly ) ? df.readAll() : QByteArray() );
			}
			/* One layer as a texture of its own: the file's header, then that layer's bytes (the
			 * decoder reads at most 256 layers of a file, and all of them). */
			const QByteArray & dds = arrays[l->first];
			const qint64 count = dds.size() > 148 && dds.mid( 84, 4 ) == "DX10"
				? qint64( qFromLittleEndian<quint32>( reinterpret_cast<const uchar *>( dds.constData() ) + 140 ) ) : 0;
			const qint64 per = count > 0 ? ( qint64( dds.size() ) - 148 ) / count : 0;
			if ( per > 0 && l->second >= 0 && l->second < count ) {
				const QByteArray one = dds.left( 148 ) + dds.mid( int( 148 + qint64( l->second ) * per ), int( per ) );
				try {
					textures.emplace_back( new DDSTexture16( reinterpret_cast<const unsigned char *>( one.constData() ),
						size_t( one.size() ) ) );
					m.tex = textures.back().get();
				} catch ( std::exception & ) {
					m.tex = nullptr;
				}
			}
		}
	}
	if ( !m.tex )
		noTexture++;
	return mats.emplace( id, m ).first->second;
}

// what the texture keeps of a triangle, and the mean color of that: one texel at the middle of each
// piece of the triangle, the pieces about two texels wide
void FarTrees::texels( const TreeMat & m, const float uv[3][2], float & cover, float lin[3] ) const
{
	cover = 1.0f;
	for ( int k = 0; k < 3; k++ )
		lin[k] = kBark[k];
	if ( !m.tex )
		return;
	const int w = m.tex->getWidth(), h = m.tex->getHeight();
	float e = 0;
	for ( int i = 0; i < 3; i++ ) {
		const int j = ( i + 1 ) % 3;
		e = std::max( { e, std::fabs( uv[i][0] - uv[j][0] ) * float( w ), std::fabs( uv[i][1] - uv[j][1] ) * float( h ) } );
	}
	const int n = std::clamp( int( std::ceil( e / 2.0f ) ), 2, 48 );
	const bool linear = m.tex->isSRGBTexture();   // an sRGB format is decoded to linear already
	int all = 0, pass = 0;
	double sum[3] = { 0, 0, 0 };
	auto take = [&]( float a, float b ) {
		const float u = uv[0][0] + a * ( uv[1][0] - uv[0][0] ) + b * ( uv[2][0] - uv[0][0] );
		const float v = uv[0][1] + a * ( uv[1][1] - uv[0][1] ) + b * ( uv[2][1] - uv[0][1] );
		const FloatVector4 c = FloatVector4::convertFloat16( m.tex->getPixelN( int( std::floor( ( u - std::floor( u ) ) * float( w ) ) ),
			int( std::floor( ( v - std::floor( v ) ) * float( h ) ) ), 0 ) );
		all++;
		if ( m.threshold && int( std::lround( std::clamp( c[3], 0.0f, 1.0f ) * 255.0f ) ) < m.threshold )
			return;
		pass++;
		for ( int k = 0; k < 3; k++ ) {
			const float g = std::clamp( c[size_t( k )], 0.0f, 1.0f );
			sum[k] += linear ? g : srgbToLinear( g );
		}
	};
	for ( int i = 0; i < n; i++ )
		for ( int j = 0; j < n - i; j++ ) {
			take( ( float( i ) + 1.0f / 3.0f ) / float( n ), ( float( j ) + 1.0f / 3.0f ) / float( n ) );
			if ( j < n - i - 1 )
				take( ( float( i ) + 2.0f / 3.0f ) / float( n ), ( float( j ) + 2.0f / 3.0f ) / float( n ) );
		}
	cover = float( pass ) / float( all );
	for ( int k = 0; k < 3 && pass; k++ )
		lin[k] = float( sum[k] / pass );
}

const TreeModel & FarTrees::model( quint16 baseId, bool mirrored )
{
	const quint32 key = ( quint32( baseId ) << 1 ) | quint32( mirrored );
	const auto it = models.find( key );
	if ( it != models.end() )
		return it->second;
	TreeModel & M = models[key];
	const LodoBase & base = lib.bases[baseId];
	struct V
	{
		float p[3], uv[2];
	};
	// the full-detail clusters of one authored slot, each with its vertices decoded
	auto walk = [&]( quint16 meshId, const std::function<void( const LodoCluster &, quint32, const std::vector<V> & )> & fn ) {
		const LodoMesh & mesh = lib.meshes[meshId];
		std::vector<V> v;
		for ( quint32 c = mesh.clusterFirst; c < mesh.clusterFirst + mesh.clusterCount && c < lib.clusters.size(); c++ ) {
			if ( c < lib.clusterLods.size() && lib.clusterLods[c].level != 0 )
				continue;
			const LodoCluster & cl = lib.clusters[c];
			v.clear();
			for ( size_t vi = cl.vertexBase; vi < size_t( cl.vertexBase ) + cl.vertexCount && vi < lib.vertices.size(); vi++ ) {
				const LodoVertex & lv = lib.vertices[vi];
				V o;
				for ( int k = 0; k < 3; k++ )
					o.p[k] = lodoDequantU16( lv.pos[k], mesh.aabbMin[k], mesh.aabbExtent[k] );
				for ( int k = 0; k < 2; k++ )
					o.uv[k] = lodoDequantU16( lv.uv[k], mesh.uvMin[k], mesh.uvExtent[k] );
				v.push_back( o );
			}
			fn( cl, c, v );
		}
	};
	int far = -1;   // the coarsest authored slot
	for ( int r = 0; r < 4; r++ ) {
		if ( base.rep[r] == LODO_NO_MESH || base.rep[r] >= lib.meshes.size() )
			continue;
		far = r;
		if ( std::find( base.rep, base.rep + r, base.rep[r] ) == base.rep + r )
			walk( base.rep[r], [&]( const LodoCluster &, quint32, const std::vector<V> & v ) {
				for ( const V & o : v )
					M.top.insert( M.top.end(), o.p, o.p + 3 );
			} );
	}
	if ( far < 0 )
		return M;
	// the repetition breaker's mirror is about each material's OWN U range (the viewer's rule, src/lodinative.cpp)
	std::map<quint16, std::pair<float, float>> uRange;
	if ( mirrored )
		walk( base.rep[far], [&]( const LodoCluster & cl, quint32, const std::vector<V> & v ) {
			for ( const V & o : v ) {
				auto r = uRange.emplace( cl.materialId, std::make_pair( o.uv[0], o.uv[0] ) ).first;
				r->second.first = std::min( r->second.first, o.uv[0] );
				r->second.second = std::max( r->second.second, o.uv[0] );
			}
		} );
	double tone[3] = { 0, 0, 0 };
	walk( base.rep[far], [&]( const LodoCluster & cl, quint32 c, const std::vector<V> & v ) {
		const TreeMat & mat = material( cl.materialId );
		const float uMid = mirrored ? uRange[cl.materialId].first + uRange[cl.materialId].second : 0.0f;
		const size_t li = size_t( c ) * LODO_LOCAL_INDEX_BYTES;
		for ( int t = 0; t < int( cl.triangleCount ) && li + size_t( t ) * 3 + 2 < lib.localIndices.size(); t++ ) {
			const quint8 * ix = &lib.localIndices[li + size_t( t ) * 3];
			if ( ix[0] >= v.size() || ix[1] >= v.size() || ix[2] >= v.size() )
				continue;
			const V * s[3] = { &v[ix[0]], &v[ix[1]], &v[ix[2]] };
			float uv[3][2], mid[3] = { 0, 0, 0 }, e1[3], e2[3];
			for ( int i = 0; i < 3; i++ ) {
				uv[i][0] = mirrored ? uMid - s[i]->uv[0] : s[i]->uv[0];
				uv[i][1] = s[i]->uv[1];
				for ( int k = 0; k < 3; k++ )
					mid[k] += s[i]->p[k] / 3.0f;
			}
			for ( int k = 0; k < 3; k++ ) {
				e1[k] = s[1]->p[k] - s[0]->p[k];
				e2[k] = s[2]->p[k] - s[0]->p[k];
			}
			const double full = 0.5 * std::sqrt( std::pow( double( e1[1] ) * e2[2] - double( e1[2] ) * e2[1], 2.0 )
				+ std::pow( double( e1[2] ) * e2[0] - double( e1[0] ) * e2[2], 2.0 )
				+ std::pow( double( e1[0] ) * e2[1] - double( e1[1] ) * e2[0], 2.0 ) );
			float cover = 1.0f, lin[3];
			texels( mat, uv, cover, lin );
			M.areaFull += full;
			if ( cover <= 0.0f )
				continue;
			M.area += full * cover;
			TreeTri tt;
			const float shrink = std::sqrt( cover );
			for ( int i = 0; i < 3; i++ )
				for ( int k = 0; k < 3; k++ )
					tt.p[i * 3 + k] = mid[k] + ( s[i]->p[k] - mid[k] ) * shrink;
			for ( int k = 0; k < 3; k++ ) {
				tt.rgb[k] = toByte( lin[k] );
				tone[k] += double( lin[k] ) * full * cover;
			}
			M.tris.push_back( tt );
		}
	} );
	if ( solidBoxes && !M.tris.empty() ) {
		// the gate's refuter: the model's whole bounding box, solid, in the tree's mean color
		const LodoMesh & mesh = lib.meshes[base.rep[far]];
		TreeTri tt;
		for ( int k = 0; k < 3; k++ )
			tt.rgb[k] = toByte( float( tone[k] / std::max( M.area, 1.0e-9 ) ) );
		float v[8][3];
		for ( int k = 0; k < 8; k++ )
			for ( int r = 0; r < 3; r++ )
				v[k][r] = mesh.aabbMin[r] + ( ( k >> r ) & 1 ? mesh.aabbExtent[r] : 0.0f );
		static const int F[6][4] = { { 0, 2, 3, 1 }, { 4, 5, 7, 6 }, { 0, 1, 5, 4 }, { 2, 6, 7, 3 }, { 0, 4, 6, 2 }, { 1, 3, 7, 5 } };
		M.tris.clear();
		const float * x = mesh.aabbExtent;
		M.area = M.areaFull = 2.0 * ( double( x[0] ) * x[1] + double( x[1] ) * x[2] + double( x[2] ) * x[0] );
		for ( const auto & fc : F )
			for ( int half = 0; half < 2; half++ ) {
				const int q[3] = { fc[0], fc[1 + half], fc[2 + half] };
				for ( int i = 0; i < 3; i++ )
					for ( int k = 0; k < 3; k++ )
						tt.p[i * 3 + k] = v[q[i]][k];
				M.tris.push_back( tt );
			}
	}
	return M;
}

}   // namespace

bool probeFarBuild( const ProbeFarSpec & spec, ProbeSoup & soup, std::vector<ProbePoint> & probes,
	ProbeFarResult * out )
{
	ProbeFarResult R;
	auto fail = [&]( const QString & m ) {
		R.error = m;
		if ( out )
			*out = R;
		return false;
	};
	LodtFile f;
	QString err;
	if ( !f.open( spec.lodl, &err ) )
		return fail( QStringLiteral( "%1: %2" ).arg( spec.lodl, err ) );
	const int spc = f.samplesPerCell();
	const int step = std::clamp( spec.step, 1, spc );
	if ( spc % step )
		return fail( QStringLiteral( "--step %1 does not divide %2 samples a cell" ).arg( step ).arg( spc ) );
	int x0 = f.cellMinX(), y0 = f.cellMinY(), x1 = f.cellMaxX(), y1 = f.cellMaxY();
	if ( spec.region ) {
		x0 = std::max( x0, spec.x0 );
		y0 = std::max( y0, spec.y0 );
		x1 = std::min( x1, spec.x1 );
		y1 = std::min( y1, spec.y1 );
	}
	if ( x0 > x1 || y0 > y1 )
		return fail( QStringLiteral( "the region holds no cell of %1" ).arg( spec.lodl ) );
	// the ground reaches past the probes' cells, so an edge probe still sees ground to its side
	const int ring = spec.region ? int( std::ceil( spec.ringCells ) ) : 0;
	const int gx0 = std::max( f.cellMinX(), x0 - ring ), gy0 = std::max( f.cellMinY(), y0 - ring );
	const int gx1 = std::min( f.cellMaxX(), x1 + ring ), gy1 = std::min( f.cellMaxY(), y1 + ring );
	const int W = gx1 - gx0 + 1, H = gy1 - gy0 + 1;
	// lane BAKEBLOCK1: the caller's full-detail block
	auto inHole = [&]( int cx, int cy ) {
		return spec.hole && cx >= spec.hx0 && cx <= spec.hx1 && cy >= spec.hy0 && cy <= spec.hy1;
	};

	std::vector<CellRec> cells( size_t( W ) * size_t( H ) );
	auto at = [&]( int cx, int cy ) -> CellRec & { return cells[size_t( cy - gy0 ) * size_t( W ) + size_t( cx - gx0 )]; };
	for ( int cy = gy0; cy <= gy1; cy++ )
		for ( int cx = gx0; cx <= gx1; cx++ ) {
			CellRec & c = at( cx, cy );
			quint16 wt = 0, fl = 0;
			c.ok = f.cell( cx, cy, c.lo, c.hi, c.waterH, wt, fl );
			c.water = c.ok && ( fl & 1u ) && c.waterH > c.lo;
			c.roof = c.hi;
			if ( c.water )
				c.roof = std::max( c.roof, c.waterH );
		}

	// --- the ground: one quad every `step` samples; heights read in .lodl sample space
	const int shift = spec.red == QLatin1String( "shift" ) ? spc : 0;
	const int maxGx = f.cellsX() * spc - 1, maxGy = f.cellsY() * spc - 1;
	auto h = [&]( int cx, int cy, int i, int j ) {
		const int gx = std::clamp( ( cx - f.cellMinX() ) * spc + i + shift, 0, maxGx );
		const int gy = std::clamp( ( cy - f.cellMinY() ) * spc + j, 0, maxGy );
		return f.height( gx, gy );
	};
	const float q = f.heightQuantum();
	const int n = spc / step;
	const float qs = kCell / float( n );

	// the colour sheet: decoded one tile at a time, quads laid tile by tile
	LodtSheets sheets;
	bool haveSheet = false;
	if ( spec.sheetDim > 0 ) {
		const QByteArray old = qgetenv( "WW_LODL_SHEET_DIM" );
		qputenv( "WW_LODL_SHEET_DIM", QByteArray::number( spec.sheetDim ) );
		QString why;
		haveSheet = sheets.open( spec.lodl, &why ) && sheets.hasRole( LODV_ROLE_COLOR );
		if ( old.isEmpty() )
			qunsetenv( "WW_LODL_SHEET_DIM" );
		else
			qputenv( "WW_LODL_SHEET_DIM", old );
		R.sheet = haveSheet ? sheets.containerPath() : QStringLiteral( "none (%1)" ).arg( why.isEmpty()
			? QStringLiteral( "no colour sheet" ) : why );
	} else {
		R.sheet = QStringLiteral( "none (--sheet-dim 0)" );
	}

	// cells grouped by the sheet tile that holds them (one group when there is no sheet)
	std::map<std::pair<int, int>, std::vector<std::pair<int, int>>> byTile;
	for ( int cy = gy0; cy <= gy1; cy++ )
		for ( int cx = gx0; cx <= gx1; cx++ ) {
			if ( !at( cx, cy ).ok || inHole( cx, cy ) )
				continue;
			int tx = -1, ty = -1;
			if ( haveSheet && !sheets.tileOfCell( cx, cy, &tx, &ty ) )
				tx = ty = -1;
			byTile[{ tx, ty }].push_back( { cx, cy } );
		}
	for ( const auto & grp : byTile ) {
		const int tx = grp.first.first, ty = grp.first.second;
		std::vector<quint8> ch[3];
		bool colour = tx >= 0;
		if ( colour ) {
			QString why;
			for ( int k = 0; k < 3 && colour; k++ )
				colour = sheets.sheetChannel( LODV_ROLE_COLOR, tx, ty, k, ch[k], &why );
		}
		const int st = colour ? sheets.storedTexels() : 0;
		const int dim = colour ? sheets.levelDim() : 1;
		for ( const auto & cc : grp.second ) {
			const int cx = cc.first, cy = cc.second;
			const CellRec & c = at( cx, cy );
			// the gate: every sample this cell reads lies in its own stored lo..hi
			for ( int j = 0; j <= spc; j += step )
				for ( int i = 0; i <= spc; i += step ) {
					if ( ( i == spc && cx == f.cellMaxX() ) || ( j == spc && cy == f.cellMaxY() ) )
						continue;
					if ( i == spc || j == spc )
						continue;   // the next cell's first sample: its range, not this one's
					const float z = h( cx, cy, i, j );
					if ( z < c.lo - q || z > c.hi + q )
						R.heightOutside++;
				}
			for ( int b = 0; b < n; b++ )
				for ( int a = 0; a < n; a++ ) {
					const float wx0 = cx * kCell + a * qs, wy0 = cy * kCell + b * qs;
					const float z00 = h( cx, cy, a * step, b * step ), z10 = h( cx, cy, ( a + 1 ) * step, b * step );
					const float z01 = h( cx, cy, a * step, ( b + 1 ) * step );
					const float z11 = h( cx, cy, ( a + 1 ) * step, ( b + 1 ) * step );
					quint8 rgb[3] = { kDirt[0], kDirt[1], kDirt[2] };
					if ( colour ) {
						// this quad's footprint on the tile, v from the NORTH edge (docs/LODGEN_TERRAIN_VT.md 2.2)
						const float u0 = ( float( cx - sheets.west() - tx * dim ) + float( a ) / n ) / dim;
						const float u1 = u0 + 1.0f / ( float( n ) * dim );
						const float v0 = ( float( sheets.north() - cy - ty * dim ) + 1.0f - float( b + 1 ) / n ) / dim;
						const float v1 = v0 + 1.0f / ( float( n ) * dim );
						auto tex = [&]( float t ) { return ( sheets.uvBias() + t * sheets.uvScale() ) * st; };
						const int sx0 = std::clamp( int( tex( u0 ) ), 0, st - 1 );
						const int sx1 = std::clamp( int( std::ceil( tex( u1 ) ) ), sx0 + 1, st );
						const int sy0 = std::clamp( int( tex( v0 ) ), 0, st - 1 );
						const int sy1 = std::clamp( int( std::ceil( tex( v1 ) ) ), sy0 + 1, st );
						double sum[3] = { 0, 0, 0 };
						int cnt = 0;
						for ( int y = sy0; y < sy1; y++ )
							for ( int x = sx0; x < sx1; x++ ) {
								for ( int k = 0; k < 3; k++ )
									sum[k] += srgbToLinear( ch[k][size_t( y ) * size_t( st ) + size_t( x )] / 255.0f );
								cnt++;
							}
						if ( cnt ) {
							for ( int k = 0; k < 3; k++ )
								rgb[k] = toByte( float( sum[k] / cnt ) );
							R.albedoQuads++;
						}
					}
					const float p00[3] = { wx0, wy0, z00 }, p10[3] = { wx0 + qs, wy0, z10 };
					const float p01[3] = { wx0, wy0 + qs, z01 }, p11[3] = { wx0 + qs, wy0 + qs, z11 };
					soup.addTri( p00, p10, p11, rgb );
					soup.addTri( p00, p11, p01, rgb );
					R.quads++;
					if ( c.water && std::min( { z00, z10, z01, z11 } ) < c.waterH ) {
						const float w00[3] = { wx0, wy0, c.waterH }, w10[3] = { wx0 + qs, wy0, c.waterH };
						const float w01[3] = { wx0, wy0 + qs, c.waterH }, w11[3] = { wx0 + qs, wy0 + qs, c.waterH };
						soup.addTri( w00, w10, w11, kWater );
						soup.addTri( w00, w11, w01, kWater );
						R.waterQuads++;
					}
				}
		}
	}

	// --- the buildings: the .lodi occluder boxes (inside each object's own LOD mesh)
	if ( !spec.lodi.isEmpty() ) {
		LodiHeader lh;
		LodiTable lt;
		if ( !lodiRead( spec.lodi, &lh, &lt, false, &err ) )
			return fail( QStringLiteral( "%1: %2" ).arg( spec.lodi, err ) );
		std::vector<char> seen( cells.size(), 0 );
		for ( const LodiOccluder & o : lt.occluders ) {
			float quat[4], m[9];
			lodiUnpackRotation( o.rot, quat, m );
			float v[8][3];
			for ( int k = 0; k < 8; k++ ) {
				const float s[3] = { ( k & 1 ) ? o.halfExtent[0] : -o.halfExtent[0],
					( k & 2 ) ? o.halfExtent[1] : -o.halfExtent[1], ( k & 4 ) ? o.halfExtent[2] : -o.halfExtent[2] };
				for ( int r = 0; r < 3; r++ )
					v[k][r] = o.centre[r] + m[r * 3 + 0] * s[0] + m[r * 3 + 1] * s[1] + m[r * 3 + 2] * s[2];
			}
			static const int F[6][4] = { { 0, 2, 3, 1 }, { 4, 5, 7, 6 }, { 0, 1, 5, 4 }, { 2, 6, 7, 3 },
				{ 0, 4, 6, 2 }, { 1, 3, 7, 5 } };
			/* The roof of EVERY cell the box's footprint crosses, not only the cell of its centre: a
			 * long building over a cell border stood 1583 units over its neighbour's probe (the
			 * gate's roofline check, 2026-10-01). */
			float lo[3] = { v[0][0], v[0][1], v[0][2] }, hi[3] = { v[0][0], v[0][1], v[0][2] };
			for ( const auto & p : v )
				for ( int r = 0; r < 3; r++ ) {
					lo[r] = std::min( lo[r], p[r] );
					hi[r] = std::max( hi[r], p[r] );
				}
			const int cx0 = std::max( gx0, int( std::floor( lo[0] / kCell ) ) ), cx1 = std::min( gx1, int( std::floor( hi[0] / kCell ) ) );
			const int cy0 = std::max( gy0, int( std::floor( lo[1] / kCell ) ) ), cy1 = std::min( gy1, int( std::floor( hi[1] / kCell ) ) );
			if ( cx0 > cx1 || cy0 > cy1 )
				continue;
			if ( inHole( int( std::floor( 0.5f * ( lo[0] + hi[0] ) / kCell ) ), int( std::floor( 0.5f * ( lo[1] + hi[1] ) / kCell ) ) ) )
				continue;
			for ( const auto & fc : F ) {
				soup.addTri( v[fc[0]], v[fc[1]], v[fc[2]], kBuilding );
				soup.addTri( v[fc[0]], v[fc[2]], v[fc[3]], kBuilding );
			}
			R.boxes++;
			for ( int cy = cy0; cy <= cy1; cy++ )
				for ( int cx = cx0; cx <= cx1; cx++ ) {
					at( cx, cy ).roof = std::max( at( cx, cy ).roof, hi[2] );
					const size_t si = size_t( cy - gy0 ) * size_t( W ) + size_t( cx - gx0 );
					if ( !seen[si] ) {
						seen[si] = 1;
						R.boxCells++;
					}
				}
		}

		// --- lane TREE1: the trees, every placement whose library base is a tree (see FarTrees)
		const QFileInfo li( spec.lodi );
		const QString lodoPath = li.absoluteDir().filePath( li.completeBaseName() + QStringLiteral( ".lodo" ) );
		LodoHeader oh;
		LodoLibrary lib;
		if ( spec.red == QLatin1String( "notrees" ) )
			R.treeNote = QStringLiteral( "--red notrees" );
		else if ( !QFileInfo::exists( lodoPath ) )
			R.treeNote = QStringLiteral( "no %1 beside the .lodi" ).arg( QFileInfo( lodoPath ).fileName() );
		else if ( !lodoRead( lodoPath, &oh, &lib, false, &err ) )
			return fail( QStringLiteral( "%1: %2" ).arg( lodoPath, err ) );
		else {
			FarTrees trees( lib, spec.lodi, spec.red == QLatin1String( "treebox" ) );
			const bool canopy = spec.red != QLatin1String( "canopy" );
			const float east = spec.red == QLatin1String( "treeshift" ) ? kCell : 0.0f;
			std::vector<char> raised( cells.size(), 0 );
			R.treeFirst = soup.triCount();
			const int cw = std::max( 1, int( lh.chunkCells ) );
			for ( quint32 ci = 0; ci < quint32( lt.chunks.size() ); ci++ ) {
				const LodiChunk & ch = lt.chunks[ci];
				int chx = 0, chy = 0;
				lodiChunkAt( lh, ci, &chx, &chy );
				if ( !ch.instanceCount || chx * cw > gx1 || chx * cw + cw - 1 < gx0 || chy * cw > gy1 || chy * cw + cw - 1 < gy0 )
					continue;
				for ( quint32 ii = ch.instanceFirst; ii < ch.instanceFirst + ch.instanceCount && ii < lt.instances.size(); ii++ ) {
					const LodiInstance & inst = lt.instances[ii];
					if ( inst.baseId >= lib.bases.size() || !( lib.bases[inst.baseId].flags & LODO_BASE_TREE ) )
						continue;
					float pos[3];
					lodiDecodePosition( lh, ci, ch, inst, pos );
					const int tx = int( std::floor( pos[0] / kCell ) ), ty = int( std::floor( pos[1] / kCell ) );
					if ( tx < gx0 || tx > gx1 || ty < gy0 || ty > gy1 || inHole( tx, ty ) )
						continue;
					R.treesPlaced++;
					const TreeModel & tm = trees.model( inst.baseId, ( inst.flags & LODI_INST_MIRRORED ) != 0 );
					if ( tm.tris.empty() )
						continue;
					float quat[4], m[9];
					lodiUnpackRotation( inst.rot, quat, m );
					const float sc = lodiScaleValue( inst.scale, inst.flags );
					pos[0] += east;
					// as the viewer stands it: world = position + rotation x ( model x scale )
					auto world = [&]( const float * l, float * w ) {
						for ( int r = 0; r < 3; r++ )
							w[r] = pos[r] + ( m[r * 3 + 0] * l[0] + m[r * 3 + 1] * l[1] + m[r * 3 + 2] * l[2] ) * sc;
					};
					/* The canopy is roofline, as a box is: every authored vertex of every slot lifts the
					 * cell it stands in. Left alone, 28 of 48 Concord probes saw under 0.40 of a 0.5 sky
					 * (9 under 0.25); the coarsest slot is in places 568 units taller than the first. */
					auto lift = [&]( const float * w ) {
						const int cx = int( std::floor( w[0] / kCell ) ), cy = int( std::floor( w[1] / kCell ) );
						if ( !canopy || cx < gx0 || cx > gx1 || cy < gy0 || cy > gy1 || w[2] <= at( cx, cy ).roof )
							return;
						at( cx, cy ).roof = w[2];
						const size_t si = size_t( cy - gy0 ) * size_t( W ) + size_t( cx - gx0 );
						if ( !raised[si] ) {
							raised[si] = 1;
							R.treeCells++;
						}
					};
					for ( const TreeTri & t : tm.tris ) {
						float a[3], b[3], c[3];
						world( t.p, a );
						world( t.p + 3, b );
						world( t.p + 6, c );
						soup.addTri( a, b, c, t.rgb );
						// a shrunk leaf can cross into the next cell still high (the gate's Concord block, 117 units)
						lift( a );
						lift( b );
						lift( c );
					}
					R.trees++;
					R.treeTris += qint64( tm.tris.size() );
					R.treeArea += tm.area * double( sc ) * double( sc );
					R.treeAreaFull += tm.areaFull * double( sc ) * double( sc );
					for ( size_t k = 0; k + 2 < tm.top.size(); k += 3 ) {
						float w[3];
						world( &tm.top[k], w );
						lift( w );
					}
				}
			}
			R.treeMaterials = trees.materials();
			R.treeNoTexture = trees.noTexture;
			if ( !R.trees )
				R.treeNote = QStringLiteral( "the LOD data places none here" );
		}
	} else
		R.treeNote = QStringLiteral( "no --lodi" );

	// --- one probe per cell, hoisted over the roofline at the cell's middle
	double hoistSum = 0;
	for ( int cy = y0; cy <= y1; cy++ )
		for ( int cx = x0; cx <= x1; cx++ ) {
			const CellRec & c = at( cx, cy );
			if ( !c.ok )
				continue;
			R.cells++;
			ProbePoint p;
			p.pos[0] = ( float( cx ) + 0.5f ) * kCell;
			p.pos[1] = ( float( cy ) + 0.5f ) * kCell;
			p.pos[2] = c.roof + spec.hoist;
			p.cellX = cx;
			p.cellY = cy;
			probes.push_back( p );
			hoistSum += double( p.pos[2] ) - double( h( cx, cy, spc / 2, spc / 2 ) );
			R.roofMax = R.probes ? std::max( R.roofMax, c.roof ) : c.roof;
			R.probes++;
		}
	R.hoistMean = R.probes ? hoistSum / R.probes : 0.0;
	if ( out )
		*out = R;
	return true;
}

bool probeFarAppendRing( const QString & lodl, const QString & lodi, int cx, int cy, int half, int radius,
	ProbeSoup & soup, ProbeFarResult * out )
{
	ProbeFarSpec fs;
	fs.lodl = lodl;
	fs.lodi = lodi;
	fs.region = true;
	fs.ringCells = 0.0f;
	fs.x0 = cx - radius;
	fs.y0 = cy - radius;
	fs.x1 = cx + radius;
	fs.y1 = cy + radius;
	fs.hole = true;
	fs.hx0 = cx - half;
	fs.hy0 = cy - half;
	fs.hx1 = cx + half;
	fs.hy1 = cy + half;
	std::vector<ProbePoint> unused;
	return probeFarBuild( fs, soup, unused, out );
}

QString probeFarCensusText( const ProbeFarResult & r )
{
	QString s;
	QTextStream t( &s );
	t << "far: " << r.probes << " probes over " << r.cells << " cells, hoisted " << QString::number( r.hoistMean, 'f', 0 )
	  << " over the ground on average, highest roof " << QString::number( r.roofMax, 'f', 0 ) << "\n";
	t << "far: ground quads " << r.quads << " (colour from the sheet " << r.albedoQuads << "), water quads "
	  << r.waterQuads << ", building boxes " << r.boxes << " in " << r.boxCells << " cells\n";
	t << "far: height samples outside their cell's stored range " << r.heightOutside << "; colour sheet " << r.sheet << "\n";
	if ( r.trees )
		t << "far: trees " << r.trees << " of " << r.treesPlaced << " in the LOD data (" << r.treeTris
		  << " triangles from soup triangle " << r.treeFirst << ", keeping " << QString::number( r.treeAreaFull > 0 ? r.treeArea / r.treeAreaFull : 0.0, 'f', 3 )
		  << " of their area), tree materials " << r.treeMaterials << " (without a texture " << r.treeNoTexture
		  << "), cells a canopy raised " << r.treeCells << "\n";
	else
		t << "far: trees 0 (" << r.treeNote << ")\n";
	return s;
}

int probeFarCli( const QStringList & args )
{
	ProbeFarSpec fs;
	ProbeBakeSpec bs;
	/* Measured on the whole Commonwealth (2026-10-01): surfel cell 512 / one file a cell = 731 MB;
	 * 1024 / 16 x 16 cells a file = 265 MB at irradiance 0.027 / 0.046 against the brute-force
	 * reference; 2048 = 161 MB at 0.053 / 0.094. */
	bs.surfelCell = 1024.0f;
	bs.sector = 16384.0f;
	bs.tbkVersion = 3;   // lane BAKE4: the far map stays FO4CS's v3 (no rooms or glass at this scale)
	QString outDir, soupOut, probesOut;
	for ( int i = 0; i < args.size(); i++ ) {
		const QString & a = args[i];
		const QString nx = i + 1 < args.size() ? args[i + 1] : QString();
		if ( a == QLatin1String( "--lodl" ) ) { fs.lodl = nx; i++; }
		else if ( a == QLatin1String( "--lodi" ) ) { fs.lodi = nx; i++; }
		else if ( a == QLatin1String( "--out" ) ) { outDir = nx; i++; }
		else if ( a == QLatin1String( "--soup-out" ) ) { soupOut = nx; i++; }
		else if ( a == QLatin1String( "--probes-out" ) ) { probesOut = nx; i++; }
		else if ( a == QLatin1String( "--step" ) ) { fs.step = nx.toInt(); i++; }
		else if ( a == QLatin1String( "--hoist" ) ) { fs.hoist = nx.toFloat(); i++; }
		else if ( a == QLatin1String( "--sheet-dim" ) ) { fs.sheetDim = nx.toInt(); i++; }
		else if ( a == QLatin1String( "--red" ) ) { fs.red = nx; i++; }
		else if ( a == QLatin1String( "--cells" ) ) {
			const QStringList c = nx.split( ',' );
			if ( c.size() == 4 ) {
				fs.region = true;
				fs.x0 = c[0].toInt(); fs.y0 = c[1].toInt(); fs.x1 = c[2].toInt(); fs.y1 = c[3].toInt();
			}
			i++;
		}
		else if ( a == QLatin1String( "--rays" ) ) { bs.rays = nx.toInt(); i++; }
		// lane SMOOTHN1: the noise-driven extra batches, at most n x the base set (1 = the base set alone, the old bake)
		else if ( a == QLatin1String( "--adapt" ) ) { bs.adaptMax = nx.toInt(); i++; }
		else if ( a == QLatin1String( "--threads" ) ) { bs.threads = nx.toInt(); i++; }
		else if ( a == QLatin1String( "--sector" ) ) { bs.sector = std::max( 4096.0f, nx.toFloat() ); i++; }
		else if ( a == QLatin1String( "--surfel-cell" ) ) { bs.surfelCell = nx.toFloat(); i++; }
		else if ( a == QLatin1String( "--max-links" ) ) { bs.maxLinks = quint32( qBound( 8, nx.toInt(), 4096 ) ); i++; }
	}
	if ( fs.lodl.isEmpty() || ( outDir.isEmpty() && soupOut.isEmpty() ) ) {
		std::fprintf( stderr, "usage: probefar --lodl <world.lodl> [--lodi <world.lodi>] --out <dir> [--cells x0,y0,x1,y1] "
			"[--step n] [--hoist u] [--sheet-dim n] [--sector u] [--rays n] [--adapt n] [--threads n] [--surfel-cell u] [--max-links n] "
			"[--soup-out f.psp] [--probes-out f.txt] [--red shift|notrees|treebox|treeshift|canopy]\n" );
		return 2;
	}
	ProbeSoup soup;
	std::vector<ProbePoint> probes;
	ProbeFarResult fr;
	if ( !probeFarBuild( fs, soup, probes, &fr ) ) {
		std::fprintf( stderr, "probefar: %s\n", qPrintable( fr.error ) );
		return 1;
	}
	std::fputs( qPrintable( probeFarCensusText( fr ) ), stdout );
	std::fprintf( stdout, "far: soup %lld triangles\n", soup.triCount() );
	QString err;
	if ( !soupOut.isEmpty() && !probeSoupWrite( soupOut, soup, &err ) ) {
		std::fprintf( stderr, "probefar: %s\n", qPrintable( err ) );
		return 1;
	}
	if ( !probesOut.isEmpty() ) {
		if ( FILE * pf = std::fopen( qPrintable( QDir::toNativeSeparators( probesOut ) ), "w" ) ) {
			for ( const ProbePoint & p : probes )
				std::fprintf( pf, "%.4f %.4f %.4f\n", p.pos[0], p.pos[1], p.pos[2] );
			std::fclose( pf );
		}
	}
	if ( outDir.isEmpty() )
		return 0;
	ProbeBakeResult br;
	if ( !probeBake( soup, probes, bs, outDir, &br ) ) {
		std::fprintf( stderr, "probefar: %s\n", qPrintable( br.error ) );
		return 1;
	}
	std::fputs( qPrintable( probeBakeCensusText( br ) ), stdout );
	std::fprintf( stdout, "far: rays %.0f ms, write %.0f ms\n", br.msRays, br.msWrite );
	/* The file's square is not in the `.tbk` header (FO4CS's near bake is always one cell), so the
	 * far folder states it: sector_X_Y.tbk holds the probes with floor(x / sector) = X. */
	QFile man( QDir( outDir ).filePath( QStringLiteral( "far.txt" ) ) );
	if ( man.open( QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text ) ) {
		QTextStream m( &man );
		m << "PRTP far map (NifSkope probefar)\n"
		  << "sector " << bs.sector << "\n"
		  << "surfel_cell " << bs.surfelCell << "\n"
		  << "hoist " << fs.hoist << "\n"
		  << "step " << fs.step << "\n"
		  << "probes " << br.probes << "\n"
		  << "world " << QFileInfo( fs.lodl ).completeBaseName() << "\n";
	}
	return 0;
}

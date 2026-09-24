/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellview.h"

#include "cellpick.h"
#include "cellrefs.h"
#include "cellclick.h"		// lane CELLVIEW2
#include "cellground.h"		// lane CELLVIEW2
#include "cellsplat.h"		// lane CELLVIEW4
#include "cellidentity.h"	// lane CELLVIEW2
#include "esmdata.h"
#include "lodgen.h"
#include "nativeemit.h"

#include "model/nifmodel.h"
#include "spells/blocks.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace
{

/* The guard rails of ONE document. They are set from the measurement in
 * cellview.h 1 rather than from a feeling: the biggest thing anyone asked for
 * is a 5x5 downtown block, which welds to 10.1M vertices on Fallout4.esm alone,
 * so the cap sits just above that and a load order that pushes past it is
 * REFUSED IN WORDS instead of spending four minutes on a scene nobody can
 * move. Anything smaller is comfortable: a 3x3 downtown is 4.2M and Sanctuary
 * is 0.23M. */
constexpr qint64 CELL_MAX_TOTAL_VERTS = 12000000;
constexpr qint64 CELL_MAX_SHAPES = 16384;
//! BSTriShape counts vertices in a u16, and its triangles index them the same way.
constexpr int MAX_SHAPE_VERTS = 65000;

//! Full precision, 28 bytes a vertex -- the descriptor the terrain and native
//! LOD routes already write. These vertices are in BLOCK space, tens of
//! thousands of units out, and a half float cannot hold that.
constexpr std::uint64_t CELL_VERTEX_DESC = 0x0041B00000650407ULL;

constexpr float CELL_UNITS = 4096.0f;       //!< one exterior cell, game units
constexpr int LAND_GRID = 33;               //!< LAND heights per side

struct OutVert
{
	Vector3 pos, nrm, tan, bit;
	Vector2 uv;
	/* chan[3] is the SPLAT WEIGHT (lane CELLVIEW4): the layer's own VTXT
	 * opacity at this corner, interpolated across the quad by the
	 * rasteriser, which IS the engine's bilinear blend. 1.0 everywhere
	 * else, so every existing caller is unchanged. */
	float chan[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
};

struct Bucket
{
	QString name;
	QString matString;          //!< a `.bgsm`/`.bgem`, or a diffuse texture path
	QString normalTex, specTex; //!< only used when matString is a texture
	/*! The shape NAMED a material and nothing resolved from it -- no BGSM, no
	 *  BGEM, no texture set. Drawn neutral grey and counted in the census;
	 *  magenta stays reserved for a genuinely missing file (lane CELLVIEW3). */
	bool matUnreadable = false;
	bool hasAlpha = false;
	quint8 alphaThreshold = 128;
	bool emits = false;
	float emissiveScale = 1.0f;
	bool withColour = false;
	std::vector<OutVert> verts;
	std::vector<Triangle> tris;
};

/*! IS THIS STRING SOMETHING THE SHADER PROPERTY'S **Name** CAN RESOLVE?
 *
 *  ONLY a `.bgsm` (lane CELLVIEW3, 2026-09-19). The bucket writer builds a
 *  BSLightingShaderProperty and, for a "material file", puts the string in its
 *  Name, which the renderer resolves through ShaderMaterial. A `.bgem` is an
 *  EFFECT material: it does not read as a ShaderMaterial, so all ten texture
 *  slots stayed empty, and an empty diffuse binds the missing-texture MAGENTA
 *  (Scene::DoErrorColor, src/gl/renderer.cpp ~951). That is exactly what the
 *  downtown cars' glass has been -- measured on
 *  `Vehicles\Automotive\Sedan02_Postwar.nif`, one BSEffectShaderProperty
 *  naming `Materials\Vehicles\Automotive\Car_Glass01.BGEM`.
 *
 *  A `.bgem` shape now arrives with its base map already resolved into
 *  `NativeSrcShape::effectTex0` by lodgenLoadModel, so it takes the ordinary
 *  texture path below. The refuter if this is wrong: the cars go magenta
 *  again, which is the picture of item 3. */
bool isMaterialFile( const QString & s )
{
	return s.endsWith( QStringLiteral( ".bgsm" ), Qt::CaseInsensitive );
}

//! Separators only -- see the note in src/lodinative.cpp: prepending
//! `materials\` is what BREAKS an absolute build-machine path.
QString materialNameFor( const QString & s )
{
	QString p = s;
	p.replace( QChar( '/' ), QChar( '\\' ) );
	return p;
}

/*! A DETERMINISTIC colour for an overlay key. FNV-1a over the key's bytes, then
 *  the low bits spread over a fixed 24-entry wheel, so the same layer is the
 *  same colour in every run, on every machine, in every screenshot -- which is
 *  the whole point of an overlay somebody is going to compare two pictures of.
 *  The wheel avoids near-black and near-white so nothing reads as "unlit". */
void overlayColour( quint64 key, float rgb[3] )
{
	quint64 h = 1469598103934665603ULL;
	for ( int i = 0; i < 8; i++ ) {
		h ^= ( key >> ( i * 8 ) ) & 0xFF;
		h *= 1099511628211ULL;
	}
	const float hue = float( h % 360ULL );
	const float sat = 0.55f + float( ( h >> 16 ) % 30ULL ) / 100.0f;   // 0.55..0.84
	const float val = 0.60f + float( ( h >> 32 ) % 30ULL ) / 100.0f;   // 0.60..0.89
	const float c = val * sat;
	const float hp = hue / 60.0f;
	const float x = c * ( 1.0f - std::fabs( std::fmod( hp, 2.0f ) - 1.0f ) );
	float r = 0, g = 0, b = 0;
	switch ( int( hp ) % 6 ) {
	case 0: r = c; g = x; break;
	case 1: r = x; g = c; break;
	case 2: g = c; b = x; break;
	case 3: g = x; b = c; break;
	case 4: r = x; b = c; break;
	default: r = c; b = x; break;
	}
	const float m = val - c;
	rgb[0] = r + m;
	rgb[1] = g + m;
	rgb[2] = b + m;
}

/* THE SENTINEL KEYS, AND WHY THE LEGEND USED TO LIE (lane CELLVIEW3).
 *
 * Everything that is NOT a real overlay key goes through a reserved key, and
 * every reserved key has a fixed colour that is NOT on the wheel. Until today
 * the draw site wrote flat grey 0.35 for the one "unknown" sentinel while the
 * legend called overlayColour() on that same key and printed mauve
 * (0.60,0.21,0.37) -- two code paths, two answers, and the picture disagreed
 * with its own legend. overlayKeyColour() below is now the ONLY way either
 * side gets a colour, so printed rgb == drawn rgb BY CONSTRUCTION; the refuter
 * is the gate row that samples the picture at a known placement.
 *
 * The old single grey also merged two different facts. Measured on Sanctuary
 * -20,7 (join_probe.py, an independent .lodi + plugin join): of 142 REFRs, 26
 * are in the bake and 116 are not, and the count of "drawn, base HAS a LOD
 * model, yet NOT in the .lodi" is ZERO. So the grey was CORRECT -- there is no
 * join defect and no chunk-coverage defect -- but it could not SAY so. It is
 * split: NOLOD is the expected, correct case; ORPHAN is a genuine defect (a
 * base with a LOD model whose reference the bake never gave a group) and is
 * drawn a loud red so one pixel of it is visible. */
static const quint64 OVERLAY_KEY_NOLOD  = 0xFFFFFFFFFFULL;   //!< no LOD model on the base: correct, expected
static const quint64 OVERLAY_KEY_ORPHAN = 0xFFFFFFFFFEULL;   //!< HAS a LOD model, no group: a defect
static const quint64 OVERLAY_KEY_NONE   = 0xFFFFFFFFFDULL;   //!< the overlay has nothing to say about this placement

//! The ONE colour source for the overlay: sentinels fixed, everything else the wheel.
void overlayKeyColour( quint64 key, float rgb[3] )
{
	switch ( key ) {
	case OVERLAY_KEY_NOLOD:
		rgb[0] = rgb[1] = rgb[2] = 0.35f;                       // neutral grey
		return;
	case OVERLAY_KEY_ORPHAN:
		rgb[0] = 0.95f; rgb[1] = 0.10f; rgb[2] = 0.10f;         // red: look at me
		return;
	case OVERLAY_KEY_NONE:
		rgb[0] = rgb[1] = rgb[2] = 0.35f;
		return;
	default:
		overlayColour( key, rgb );
		return;
	}
}

//! The legend's word for a sentinel; empty for a real key (the caller names those).
QString overlayKeyLabel( quint64 key )
{
	switch ( key ) {
	case OVERLAY_KEY_NOLOD:  return QStringLiteral( "no LOD model on the base (correct)" );
	case OVERLAY_KEY_ORPHAN: return QStringLiteral( "has a LOD model but no group (a defect)" );
	case OVERLAY_KEY_NONE:   return QStringLiteral( "unknown" );
	}
	return QString();
}

//! The markers the CK hides behind its own marker toggle. Matched on the model
//! path, because that is the only thing every marker base has in common.
bool isMarkerModel( const QString & model )
{
	const QString m = model.toLower();
	return m.contains( QLatin1String( "\\marker" ) )
		|| m.contains( QLatin1String( "marker_" ) )
		|| m.endsWith( QLatin1String( "markerx.nif" ) )
		/* AT THE MESHES ROOT THERE IS NO BACKSLASH (lane CELLVIEW4). Every
		 * test above needs one, or the exact suffix `markerx.nif`, so a
		 * marker sitting at meshes\ itself -- `markerxheading.nif`,
		 * `markercocheading.nif`, the whole `markers\...` subtree -- was
		 * drawn as an ordinary static. `m` is already lowercased above, so
		 * one startsWith covers all of them; a path with a leading folder
		 * was already caught by the `\marker` test. Measured in cell
		 * 5,-11: exactly three models take the untextured branch and all
		 * three are markers, one of which is the black arrow. */
		|| m.startsWith( QLatin1String( "marker" ) )
		|| m.contains( QLatin1String( "\\editor\\" ) );
}

//! Write one bucket as one or more BSTriShapes under `iRoot`. Splits at
//! MAX_SHAPE_VERTS on TRIANGLE boundaries, so no triangle points across a cut.
bool emitBucket( NifModel * nif, const QModelIndex & iRoot, const Bucket & b,
	const Vector3 & origin, qint64 & shapesOut, qint64 & vertsOut, qint64 & trisOut,
	QString * error )
{
	auto fail = [error]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};
	if ( b.verts.empty() || b.tris.empty() )
		return true;

	BSVertexDesc desc( CELL_VERTEX_DESC );
	if ( b.withColour ) {
		desc.SetFlag( VertexFlags::VF_COLORS );
		desc.ResetAttributeOffsets( 130 );
	}
	const std::uint64_t vertexDesc = desc.Value();
	const int stride = int( desc.GetVertexSize() );

	size_t first = 0;
	int part = 0;
	while ( first < b.tris.size() ) {
		QHash<int, quint16> remap;
		std::vector<OutVert> pv;
		std::vector<Triangle> pt;
		size_t i = first;
		for ( ; i < b.tris.size(); i++ ) {
			if ( pv.size() + 3 > size_t( MAX_SHAPE_VERTS ) )
				break;
			const Triangle & t = b.tris[i];
			quint16 idx[3];
			for ( int k = 0; k < 3; k++ ) {
				const int src = int( t[k] );
				auto it = remap.constFind( src );
				if ( it != remap.constEnd() ) {
					idx[k] = it.value();
				} else {
					idx[k] = quint16( pv.size() );
					remap.insert( src, idx[k] );
					pv.push_back( b.verts[size_t( src )] );
				}
			}
			pt.push_back( Triangle( idx[0], idx[1], idx[2] ) );
		}
		first = i;
		part++;
		if ( pv.empty() || pt.empty() )
			break;

		shapesOut++;
		vertsOut += qint64( pv.size() );
		trisOut += qint64( pt.size() );
		if ( shapesOut > CELL_MAX_SHAPES )
			return fail( QString( "this block needs more than %1 shapes; ask for a "
				"smaller block size" ).arg( CELL_MAX_SHAPES ) );
		if ( vertsOut > CELL_MAX_TOTAL_VERTS )
			return fail( QString( "this block needs more than %L1 vertices welded; ask "
				"for a smaller block size (a 3x3 downtown block is about 4.2M)" )
				.arg( CELL_MAX_TOTAL_VERTS ) );

		QModelIndex iShape = nif->insertNiBlock( QStringLiteral( "BSTriShape" ) );
		nif->set<QString>( iShape, "Name", part > 1
			? QString( "%1 #%2" ).arg( b.name ).arg( part ) : b.name );
		nif->set<quint32>( iShape, "Flags", 14 );
		nif->set<float>( iShape, "Scale", 1.0f );
		nif->set<Vector3>( iShape, "Translation", origin );
		nif->set<BSVertexDesc>( iShape, "Vertex Desc", vertexDesc );
		nif->set<quint32>( iShape, "Num Vertices", quint32( pv.size() ) );
		nif->set<quint32>( iShape, "Num Triangles", quint32( pt.size() ) );
		nif->set<quint32>( iShape, "Data Size",
			quint32( qint64( pv.size() ) * stride + qint64( pt.size() ) * 6 ) );

		nif->setState( BaseModel::Processing );
		QModelIndex iVertexData = nif->getIndex( iShape, "Vertex Data" );
		nif->updateArraySize( iVertexData );
		Vector3 lo( 3.4e38f, 3.4e38f, 3.4e38f ), hi( -3.4e38f, -3.4e38f, -3.4e38f );
		for ( size_t v = 0; v < pv.size(); v++ ) {
			QModelIndex row = nif->index( int( v ), 0, iVertexData );
			const OutVert & o = pv[v];
			nif->set<Vector3>( row, "Vertex", o.pos );
			nif->set<HalfVector2>( row, "UV", HalfVector2( o.uv ) );
			nif->set<ByteVector3>( row, "Normal", ByteVector3( o.nrm ) );
			nif->set<ByteVector3>( row, "Tangent", ByteVector3( o.tan ) );
			// a zero bitangent is a NaN in the shader's basis and renders BLACK
			nif->set<float>( row, "Bitangent X", o.bit[0] );
			nif->set<float>( row, "Bitangent Y", o.bit[1] );
			nif->set<float>( row, "Bitangent Z", o.bit[2] );
			if ( b.withColour )
				nif->set<ByteColor4>( row, "Vertex Colors",
					ByteColor4( FloatVector4( o.chan[0], o.chan[1], o.chan[2], o.chan[3] ) ) );
			for ( int k = 0; k < 3; k++ ) {
				lo[k] = qMin( lo[k], o.pos[k] );
				hi[k] = qMax( hi[k], o.pos[k] );
			}
		}
		QModelIndex iTriangles = nif->getIndex( iShape, "Triangles" );
		nif->updateArraySize( iTriangles );
		{
			QVector<Triangle> qt;
			qt.reserve( int( pt.size() ) );
			for ( const Triangle & t : pt )
				qt.append( t );
			nif->setArray<Triangle>( iTriangles, qt );
		}
		QModelIndex iBound = nif->getIndex( iShape, "Bounding Sphere" );
		if ( iBound.isValid() ) {
			const Vector3 c = ( lo + hi ) / 2.0f;
			const Vector3 h = ( hi - lo ) / 2.0f;
			nif->set<Vector3>( iBound, "Center", c );
			nif->set<float>( iBound, "Radius", h.length() );
		}
		nif->restoreState();

		/* The same material plumbing the chunk builder and the native LOD scene
		 * write (src/lodgen.cpp ~4189, src/lodinative.cpp ~316): the same block
		 * pair, the same ten slots, the BGSM in the shader's Name so the
		 * renderer resolves it exactly as it does for a `.BTO` shape. */
		QModelIndex iShader = nif->insertNiBlock( QStringLiteral( "BSLightingShaderProperty" ) );
		nif->set<quint32>( iShader, "Shader Type", 0 );
		nif->set<quint32>( iShader, "Shader Flags 1",
			b.emits ? 2151677953U : ( 2151677953U & ~0x400000U ) );
		nif->set<quint32>( iShader, "Shader Flags 2", b.withColour ? 0x25U : 5U );
		QModelIndex iTexSet = nif->insertNiBlock( QStringLiteral( "BSShaderTextureSet" ) );
		nif->setLink( iShader, "Texture Set", nif->getBlockNumber( iTexSet ) );
		nif->set<uint>( iTexSet, "Num Textures", 10 );
		nif->updateArraySize( iTexSet, "Textures" );
		QModelIndex iArr = nif->getIndex( iTexSet, "Textures" );
		if ( isMaterialFile( b.matString ) ) {
			nif->set<QString>( iShader, "Name", materialNameFor( b.matString ) );
		} else if ( !b.matString.isEmpty() ) {
			nif->set<QString>( nif->getIndex( iArr, 0 ), b.matString );
			if ( !b.normalTex.isEmpty() )
				nif->set<QString>( nif->getIndex( iArr, 1 ), b.normalTex );
			if ( !b.specTex.isEmpty() )
				nif->set<QString>( nif->getIndex( iArr, 7 ), b.specTex );
		} else {
			/* No material at all: the ground, the water and the cell grid,
			 * which carry their whole appearance in the vertex colours.
			 *
			 * An EMPTY diffuse slot is NOT neutral. src/gl/renderer.cpp:951
			 * binds the missing-texture magenta for one whenever
			 * Scene::DoErrorColor is on, which it is by default, and the first
			 * wilderness shot of this lane came back with the whole terrain a
			 * flat magenta sheet under correctly placed rocks -- the geometry
			 * was right and the picture was unusable.
			 *
			 * src/gl/gltex.cpp:172 reads a name of the form #AARRGGBB as a
			 * one-texel texture, which is how the renderer states its own
			 * fallbacks. White here is therefore a texture that always
			 * resolves, with no file behind it that can be missing, and it
			 * multiplies to exactly the vertex colour.
			 *
			 * A shape whose material would not read at all gets the SAME
			 * one-texel trick at 69% grey instead of white, so it is visibly
			 * "we could not read this" without being magenta -- magenta means
			 * a file the renderer went looking for and did not find, and that
			 * distinction is the whole point of the census line that counts
			 * these (lane CELLVIEW3). */
			nif->set<QString>( nif->getIndex( iArr, 0 ),
				b.matUnreadable ? QStringLiteral( "#FFB0B0B0" ) : QStringLiteral( "#FFFFFFFF" ) );
			nif->set<QString>( nif->getIndex( iArr, 1 ), QStringLiteral( "#FFFF8080n" ) );
		}
		if ( b.emits )
			nif->set<float>( iShader, "Emissive Multiple", b.emissiveScale );
		nif->setLink( iShape, "Shader Property", nif->getBlockNumber( iShader ) );
		if ( b.hasAlpha ) {
			QModelIndex iAlpha = nif->insertNiBlock( QStringLiteral( "NiAlphaProperty" ) );
			/* 4844 = alpha TEST only; 4333 = alpha BLEND, SRC_ALPHA /
			 * ONE_MINUS_SRC_ALPHA, which is what a splat layer needs
			 * (lane CELLVIEW4). A threshold of 0 is asked for by the splat
			 * layer buckets and by nothing else. */
			nif->set<int>( iAlpha, "Flags", b.alphaThreshold ? 4844 : 4333 );
			nif->set<int>( iAlpha, "Threshold", int( b.alphaThreshold ) );
			nif->setLink( iShape, "Alpha Property", nif->getBlockNumber( iAlpha ) );
		}
		addLink( nif, iRoot, QStringLiteral( "Children" ), nif->getBlockNumber( iShape ) );
	}
	return true;
}

//! A flat quad, four corners counter-clockwise seen from +Z.
void appendQuad( Bucket & b, const Vector3 & a, const Vector3 & c,
	const Vector3 & d, const Vector3 & e, const Vector3 & nrm, const float rgb[3] )
{
	const int base = int( b.verts.size() );
	const Vector3 corners[4] = { a, c, d, e };
	const Vector2 uvs[4] = { Vector2( 0, 0 ), Vector2( 1, 0 ), Vector2( 1, 1 ), Vector2( 0, 1 ) };
	for ( int i = 0; i < 4; i++ ) {
		OutVert v;
		v.pos = corners[i];
		v.nrm = nrm;
		v.tan = Vector3( 1.0f, 0.0f, 0.0f );
		v.bit = Vector3( 0.0f, 1.0f, 0.0f );
		v.uv = uvs[i];
		v.chan[0] = rgb[0];
		v.chan[1] = rgb[1];
		v.chan[2] = rgb[2];
		b.verts.push_back( v );
	}
	b.tris.push_back( Triangle( quint16( base ), quint16( base + 1 ), quint16( base + 2 ) ) );
	b.tris.push_back( Triangle( quint16( base ), quint16( base + 2 ), quint16( base + 3 ) ) );
}

} // namespace


// ---------------------------------------------------------------- the spec

QString cellOverlayName( CellOverlay o )
{
	switch ( o ) {
	case CellOverlay::Layer: return QStringLiteral( "layer" );
	case CellOverlay::Identity: return QStringLiteral( "identity" );
	case CellOverlay::HasLod: return QStringLiteral( "has-lod" );
	case CellOverlay::RecordType: return QStringLiteral( "type" );
	case CellOverlay::Precombined: return QStringLiteral( "precombined" );
	default: return QString();
	}
}

QString cellOverlayNames()
{
	return QStringLiteral( "layer, identity, has-lod, type, precombined" );
}

CellOverlay cellOverlayFromName( const QString & name, bool * known )
{
	if ( known )
		*known = true;
	const QString n = name.trimmed().toLower();
	if ( n.isEmpty() || n == QLatin1String( "none" ) )
		return CellOverlay::None;
	if ( n == QLatin1String( "layer" ) )
		return CellOverlay::Layer;
	if ( n == QLatin1String( "identity" ) || n == QLatin1String( "group" ) )
		return CellOverlay::Identity;
	if ( n == QLatin1String( "has-lod" ) || n == QLatin1String( "haslod" ) )
		return CellOverlay::HasLod;
	if ( n == QLatin1String( "type" ) || n == QLatin1String( "record" ) )
		return CellOverlay::RecordType;
	if ( n == QLatin1String( "precombined" ) )
		return CellOverlay::Precombined;
	if ( known )
		*known = false;
	return CellOverlay::None;
}

void CellSceneSpec::rect( int & x0, int & y0, int & x1, int & y1 ) const
{
	const int half = ( qMax( 1, n ) - 1 ) / 2;
	x0 = cx - half;
	x1 = cx + half;
	y0 = cy - half;
	y1 = cy + half;
}

bool cellSpecFromLine( const QString & line, CellSceneSpec & spec, QString * error )
{
	auto fail = [error]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};
	const QStringList parts = line.trimmed().split( QLatin1Char( '|' ) );
	if ( parts.size() < 4 )
		return fail( QStringLiteral( "expected plugins|worldspace|x,y|n, got \"%1\"" )
			.arg( line.trimmed() ) );
	spec.plugins = parts[0].trimmed();
	spec.world = parts[1].trimmed();
	const QStringList xy = parts[2].split( QLatin1Char( ',' ) );
	if ( xy.size() != 2 )
		return fail( QStringLiteral( "expected x,y, got \"%1\"" ).arg( parts[2] ) );
	bool okx = false, oky = false, okn = false;
	spec.cx = xy[0].trimmed().toInt( &okx );
	spec.cy = xy[1].trimmed().toInt( &oky );
	spec.n = parts[3].trimmed().toInt( &okn );
	if ( !okx || !oky || !okn )
		return fail( QStringLiteral( "x, y and n must be whole numbers" ) );
	if ( spec.n < 1 || ( spec.n % 2 ) == 0 )
		return fail( QStringLiteral( "the block size must be odd (1, 3, 5); got %1" )
			.arg( spec.n ) );
	if ( parts.size() >= 5 ) {
		bool known = false;
		spec.overlay = cellOverlayFromName( parts[4], &known );
		if ( !known )
			return fail( QStringLiteral( "\"%1\" is not an overlay; try one of: %2" )
				.arg( parts[4].trimmed(), cellOverlayNames() ) );
	}
	if ( spec.plugins.isEmpty() )
		return fail( QStringLiteral( "no plugin named" ) );
	if ( spec.world.isEmpty() )
		return fail( QStringLiteral( "no worldspace named" ) );
	spec.valid = true;
	return true;
}

bool cellSpecFromFile( const QString & path, CellSceneSpec & spec, QString * error )
{
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
		if ( error )
			*error = QStringLiteral( "could not read %1" ).arg( path );
		return false;
	}
	QTextStream in( &f );
	while ( !in.atEnd() ) {
		const QString line = in.readLine().trimmed();
		if ( line.isEmpty() || line.startsWith( QLatin1Char( '#' ) ) )
			continue;
		return cellSpecFromLine( line, spec, error );
	}
	if ( error )
		*error = QStringLiteral( "%1 has no spec line" ).arg( path );
	return false;
}

void cellApplyEnvModifiers( CellSceneSpec & spec )
{
	const QByteArray ov = qgetenv( "WW_CELL_OVERLAY" );
	if ( !ov.isEmpty() ) {
		bool known = false;
		const CellOverlay o = cellOverlayFromName( QString::fromLatin1( ov ), &known );
		if ( known )
			spec.overlay = o;
		else
			qWarning() << "WW_CELL_OVERLAY:" << ov << "is not an overlay; try one of:"
				<< cellOverlayNames();
	}
	if ( !qgetenv( "WW_CELL_DISABLED" ).isEmpty() )
		spec.showDisabled = true;
	if ( !qgetenv( "WW_CELL_MARKERS" ).isEmpty() )
		spec.showMarkers = true;
	if ( !qgetenv( "WW_CELL_NOTERRAIN" ).isEmpty() )
		spec.terrain = false;
	if ( !qgetenv( "WW_CELL_NOWATER" ).isEmpty() )
		spec.water = false;
	if ( !qgetenv( "WW_CELL_NOGRID" ).isEmpty() )
		spec.grid = false;
	const QByteArray dr = qgetenv( "WW_CELL_DATAROOT" );
	if ( !dr.isEmpty() )
		spec.dataRoot = QString::fromLocal8Bit( dr );
	const QByteArray li = qgetenv( "WW_CELL_LODI" );
	if ( !li.isEmpty() )
		spec.lodiPath = QString::fromLocal8Bit( li );
}

bool cellSpecFromEnv( CellSceneSpec & spec, QString * error )
{
	const QByteArray env = qgetenv( "WW_CELL_OPEN" );
	if ( env.isEmpty() )
		return false;
	if ( !cellSpecFromLine( QString::fromLocal8Bit( env ), spec, error ) )
		return false;
	cellApplyEnvModifiers( spec );
	return true;
}


// --------------------------------------------------------------- the build

bool nifCreateCellScene( NifModel * nif, const CellSceneSpec & spec,
	QString * error, QString * notes )
{
	auto fail = [error]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};
	if ( !nif )
		return fail( QStringLiteral( "no model" ) );
	if ( !spec.valid )
		return fail( QStringLiteral( "no cell spec" ) );

	QElapsedTimer clock;
	clock.start();

	// ---- the worldspace
	const QString firstPlugin = spec.plugins.split( QLatin1Char( ',' ) ).first().trimmed();
	QString wsError;
	const QVector<QPair<quint32, QString>> worlds =
		EsmWorld::listWorldspaces( spec.plugins, &wsError );
	if ( worlds.isEmpty() )
		return fail( wsError.isEmpty()
			? QStringLiteral( "%1 names no worldspace" ).arg( firstPlugin ) : wsError );
	quint32 wsForm = 0;
	for ( const QPair<quint32, QString> & w : worlds ) {
		if ( w.second.compare( spec.world, Qt::CaseInsensitive ) == 0 ) {
			wsForm = w.first;
			break;
		}
	}
	if ( !wsForm ) {
		QStringList names;
		for ( int i = 0; i < worlds.size() && i < 12; i++ )
			names.append( worlds[i].second );
		return fail( QStringLiteral( "no worldspace called \"%1\"; this load order has %2 "
			"(first: %3)" ).arg( spec.world ).arg( worlds.size() )
			.arg( names.join( QLatin1String( ", " ) ) ) );
	}

	EsmWorld world;
	QString loadError;
	if ( !world.load( spec.plugins, wsForm, &loadError ) )
		return fail( loadError.isEmpty()
			? QStringLiteral( "could not index %1" ).arg( spec.world ) : loadError );

	/* WHERE MODELS COME FROM. The spec's data root when it names one, else the
	 * FIRST entry of the resource stack the LOD panel already set -- one
	 * resolver, the generator's, never a second one. The notes line says which
	 * of the two answered, because "the models are all missing" and "the data
	 * root was empty" look identical in a picture. */
	QString dataRoot = spec.dataRoot;
	QString dataRootSource = QStringLiteral( "the spec" );
	if ( dataRoot.isEmpty() ) {
		const QStringList stack = lodgenResourceSearchPaths();
		if ( !stack.isEmpty() ) {
			dataRoot = stack.first();
			dataRootSource = QStringLiteral( "the resource stack" );
		} else {
			dataRootSource = QStringLiteral( "nothing -- no data root and an empty resource stack" );
		}
	}

	int x0, y0, x1, y1;
	spec.rect( x0, y0, x1, y1 );

	// ---- the placements
	struct Placement
	{
		quint32 base = 0;
		Vector3 pos;
		Matrix rot;
		float scale = 1.0f;
		quint32 ref = 0;
		int part = -1;
		int cellX = 0, cellY = 0;
		bool persistent = false;
		bool disabled = false;
		/* Carried whether or not the reader supplies them: without the hook-up
		 * they stay 0/false and the census says the fields are absent, which is
		 * what keeps this file compiling on both sides of it. */
		float storedRot[3] = { 0, 0, 0 };   //!< the euler as the plugin stores it, for the dump
		quint32 layerForm = 0;
		quint32 enableParentForm = 0;
		bool enableParentOppositeFlag = false;
		/*! SHOULD THIS REFERENCE BE IN THE OBJECT-LOD BAKE AT ALL?
		 *
		 *  The rule, measured over Sanctuary -20,7 with zero exceptions
		 *  (scratchpad/cellview3_20260919/join_probe.py): a reference is in the
		 *  `.lodi` if and only if its base -- or, for a SCOL, at least ONE of
		 *  its parts' bases -- carries a LOD model. Without this field the
		 *  Identity overlay could only say "no group" and had to paint the 154
		 *  expected references and any genuinely orphaned one the same grey.
		 *  With it the two are different buckets and different colours, so a
		 *  real defect cannot hide inside the expected one. */
		bool expectLod = false;
	};
	QVector<Placement> placements;
	QHash<QString, int> skippedByType;
	int refsRead = 0, refsDeleted = 0, refsHidden = 0, refsMarker = 0, refsNoBase = 0;
	QSet<quint32> seenRefs;

	/* lane CELLWORK1: the reference model is emptied HERE, immediately before
	 * the reads that fill it, and not beside the pick table's clear further
	 * down -- the REFR loop runs before that point, so clearing there would
	 * throw away every row this build had just recorded. */
	cellRefTableMutable().clear();

	auto pushRefr = [&]( const EsmRefr & r, int cellX, int cellY, bool persistent ) {
		refsRead++;

		/* THE REFERENCE MODEL (lane CELLWORK1, bungo: "view all the technical
		 * placed objects ... do everything creation kit does with cell
		 * editing"). Recorded HERE, at the top of the one funnel every REFR
		 * passes through, and therefore BEFORE any decision about drawing: a
		 * light, a sound marker, a trigger box, a deleted ref and a disabled
		 * ref all get a row, because the Creation Kit's cell list is a list of
		 * what EXISTS and not of what was welded into the scene.
		 *
		 * Each early return below sets the FATE on its way out, so the reason a
		 * reference is not on screen is the same reason the census counts, in
		 * the same place, and the two cannot drift. */
		CellRefEntry ref;
		ref.refForm = r.formID;
		ref.baseForm = r.base;
		ref.baseType = r.baseType;
		for ( int k = 0; k < 3; k++ ) {
			ref.pos[k] = r.pos[k];
			ref.rot[k] = r.rot[k];
		}
		ref.scale = r.scale;
		ref.cellX = cellX;
		ref.cellY = cellY;
		ref.persistent = persistent;
		ref.initiallyDisabled = r.initiallyDisabled;
#ifdef ESM_HAS_CELL_FIELDS
		ref.layer = r.layer;
		ref.enableParent = r.enableParent;
		ref.enableParentOpposite = r.enableParentOpposite;
#endif
		if ( r.base ) {
			const EsmLodBase & rb = world.lodBase( r.base );
			ref.baseEdid = rb.edid;
			ref.model = rb.model;
		}
		const int refRow = cellRefTableMutable().append( ref );

		if ( r.deleted ) {
			refsDeleted++;
			cellRefTableMutable().setFate( refRow, CellRefFate::Deleted );
			return;
		}
		if ( !r.base ) {
			refsNoBase++;
			cellRefTableMutable().setFate( refRow, CellRefFate::NoBase );
			return;
		}
		bool disabled = r.initiallyDisabled;
#ifdef ESM_HAS_CELL_FIELDS
		/* An enable parent in the OPPOSITE state means the ref's visible state
		 * is the parent's inverted. We do not simulate the parent's state -- it
		 * is a runtime fact -- so the rule here is the CK's own default view:
		 * a ref with an opposite-state parent starts the other way round from
		 * its own flag. The panel shows both facts so the guess is inspectable. */
		if ( r.enableParent && r.enableParentOpposite )
			disabled = !disabled;
#endif
		if ( disabled && !spec.showDisabled ) {
			refsHidden++;
			cellRefTableMutable().setFate( refRow, CellRefFate::Disabled );
			return;
		}
		const EsmLodBase & lb = world.lodBase( r.base );
		const bool isScol = std::memcmp( &r.baseType, "SCOL", 4 ) == 0;
		if ( !isScol && lb.model.isEmpty() ) {
			skippedByType[CellPickTable::typeName( r.baseType )]++;
			cellRefTableMutable().setFate( refRow, CellRefFate::NoModel );
			return;
		}
		if ( !isScol && !spec.showMarkers && isMarkerModel( lb.model ) ) {
			refsMarker++;
			cellRefTableMutable().setFate( refRow, CellRefFate::Marker );
			return;
		}
		Matrix rm;
		rm.fromEuler( -r.rot[0], -r.rot[1], -r.rot[2] );
		const Vector3 rp( r.pos[0], r.pos[1], r.pos[2] );
		if ( isScol ) {
			int scolPart = 0;
			/* A COLLECTION IS IN THE BAKE IF ANY ONE PART IS (lane CELLVIEW3).
			 * Measured on Sanctuary -20,7: all 15 SCOLs the `.lodi` holds have
			 * at least one part whose base carries a LOD model, and all 5 it
			 * does not hold (MetalShelf03Debris02, MetalShelf04Debris01,
			 * HWSingleRampDown01SCVine01, BranchPile03 twice) have none of N.
			 * The whole collection shares the reference's form id, so this
			 * flag is a property of the reference, not of the part. */
			bool anyPartLod = false;
			for ( const EsmScolPart & part : world.scolParts( r.base ) ) {
				if ( world.lodBase( part.base ).hasLod ) {
					anyPartLod = true;
					break;
				}
			}
			for ( const EsmScolPart & part : world.scolParts( r.base ) ) {
				for ( const EsmScolPlacement & pl : part.placements ) {
					Matrix pm;
					pm.fromEuler( -pl.rot[0], -pl.rot[1], -pl.rot[2] );
					Placement out;
					out.base = part.base;
					out.pos = rp + rm * ( Vector3( pl.pos[0], pl.pos[1], pl.pos[2] ) * r.scale );
					out.rot = rm * pm;
					out.scale = r.scale * pl.scale;
					out.ref = r.formID;
					out.part = scolPart++;
					out.cellX = cellX;
					out.cellY = cellY;
					out.persistent = persistent;
					out.disabled = disabled;
					out.expectLod = anyPartLod;
					for ( int k = 0; k < 3; k++ )
						out.storedRot[k] = r.rot[k];
#ifdef ESM_HAS_CELL_FIELDS
					out.layerForm = r.layer;
					out.enableParentForm = r.enableParent;
					out.enableParentOppositeFlag = r.enableParentOpposite;
#endif
					placements.append( out );
				}
			}
			return;
		}
		Placement out;
		out.base = r.base;
		out.pos = rp;
		out.rot = rm;
		out.scale = r.scale;
		out.ref = r.formID;
		out.cellX = cellX;
		out.cellY = cellY;
		out.persistent = persistent;
		out.disabled = disabled;
		out.expectLod = lb.hasLod;
		for ( int k = 0; k < 3; k++ )
			out.storedRot[k] = r.rot[k];
#ifdef ESM_HAS_CELL_FIELDS
		out.layerForm = r.layer;
		out.enableParentForm = r.enableParent;
		out.enableParentOppositeFlag = r.enableParentOpposite;
#endif
		placements.append( out );
	};

	int cellsWithData = 0;
	for ( int y = y0; y <= y1; y++ ) {
		for ( int x = x0; x <= x1; x++ ) {
			if ( !world.hasCell( x, y ) )
				continue;
			cellsWithData++;

			/* lane CELLWORK1: the loaded-cells list. The view has always loaded
			 * a BLOCK (spec.n is 1, 3 or 5), so "more than one cell" is not new
			 * -- what was missing is that nothing recorded WHICH cells those
			 * were or what each carried. One row per grid position that has a
			 * CELL record, named by its EDID. Today's single cell is a list of
			 * one, which is the shape a streaming lane needs. */
			CellBlockEntry block;
			block.cx = x;
			block.cy = y;
			block.cellForm = world.cellForm( x, y );
			block.edid = world.cellEditorId( x, y );
			cellRefTableMutable().addCell( block );

			for ( const EsmRefr & r : world.refrs( x, y ) ) {
				seenRefs.insert( r.formID );
				pushRefr( r, x, y, false );
			}
		}
	}
	// the worldspace's persistent cell, grid-filtered to the block
	{
		const float minX = float( x0 ) * CELL_UNITS;
		const float minY = float( y0 ) * CELL_UNITS;
		const float maxX = float( x1 + 1 ) * CELL_UNITS;
		const float maxY = float( y1 + 1 ) * CELL_UNITS;
		for ( const EsmRefr & r : world.persistentRefrsIn( minX, minY, maxX, maxY ) ) {
			if ( seenRefs.contains( r.formID ) )
				continue;
			const int cx = int( std::floor( r.pos[0] / CELL_UNITS ) );
			const int cy = int( std::floor( r.pos[1] / CELL_UNITS ) );
			pushRefr( r, cx, cy, true );
		}
	}

	/* Per-cell counts are a WALK of the reference table, taken once here, after
	 * every read and every fate is settled. They are not a second set of
	 * counters incremented alongside the first: a walk of the rows the list
	 * shows cannot disagree with the rows the list shows. */
	cellRefTableMutable().tallyCells();

	// ---- the document
	if ( !nif->createNew( 0x14020007, 12, 130 ) )
		return fail( QStringLiteral( "could not create a Fallout 4 document" ) );
	nif->holdUpdates( true );
	/* AN ORDINARY NiNode ROOT (lane CELLVIEW4B, and this is CELLVIEW4's own
	 * refuter being acted on rather than argued with).
	 *
	 * CELLVIEW4 made the ROOT a BSOrderedNode so the splat passes composite in
	 * paint order. That works, and it also does something nobody asked for:
	 * src/gl/glnode.cpp NodeList::orderedNodeSort() sets `presorted = true` on
	 * every child, and Node::drawShapes() recurses, so a BSOrderedNode root
	 * marks EVERY node in the scene presorted -- after which compareNodesAlpha
	 * sorts the ENTIRE transparent pass by block number instead of by depth.
	 * Every tree card, every pane of car glass, every alpha-tested railing in
	 * a downtown cell changes its draw order, to pay for a guarantee only the
	 * ground needed.
	 *
	 * compareNodesAlpha takes the block-order branch only when BOTH nodes are
	 * presorted; any pair with one ordinary node falls through to the same
	 * alpha-then-depth rule it used before. So a BSOrderedNode that holds ONLY
	 * the land shapes buys the ground its paint order and costs the rest of
	 * the scene nothing -- see `iGround` below. */
	QModelIndex iRoot = nif->insertNiBlock( QStringLiteral( "NiNode" ) );
	nif->set<QString>( iRoot, "Name", QStringLiteral( "%1 %2,%3 %4x%4" )
		.arg( spec.world ).arg( spec.cx ).arg( spec.cy ).arg( spec.n ) );
	nif->set<quint32>( iRoot, "Flags", 14 );
	nif->set<float>( iRoot, "Scale", 1.0f );

	const Vector3 origin( float( spec.cx ) * CELL_UNITS + CELL_UNITS * 0.5f,
		float( spec.cy ) * CELL_UNITS + CELL_UNITS * 0.5f, 0.0f );

	CellPickTable & picks = cellPickTableMutable();
	picks.clear();

	/* THE .lodi IDENTITY GROUPS (lane CELLVIEW2). One read per build, before a
	 * single placement is drawn, so CellOverlay::Identity can colour a reference
	 * by the LOD group the native lodgen put it in -- the view for judging the
	 * ruled 64-unit proximity join. An absent, old or unreadable bake is NOT an
	 * error here: the index stays empty, every reference draws grey, and the
	 * notes line says which of those it was. */
	QHash<quint32, CellIdentity> identity;
	CellIdentityCensus identityCensus;
	QString identityError;
	if ( !spec.lodiPath.isEmpty() )
		cellIdentityLoad( spec.lodiPath, identity, &identityCensus, &identityError );

	QHash<QString, Bucket> buckets;
	QHash<QString, std::vector<NativeSrcShape>> modelCache;
	QSet<QString> modelsFailed;
	QHash<quint64, int> overlayLegend;      // key -> placements
	int drawn = 0, modelLoads = 0;
	// lane CELLVIEW3: the two material facts the census now states outright
	int shapesFromEffectMat = 0;            //!< drawn from a `.bgem`'s base map
	int shapesUnreadableMat = 0;            //!< named a material, nothing resolved: neutral grey
	QStringList unreadableMatNames;
	qint64 srcTris = 0;

	auto bucketFor = [&]( const NativeSrcShape & s, bool withColour ) -> Bucket & {
		/* WHICH STRING DESCRIBES THIS SHAPE'S SURFACE (lane CELLVIEW3).
		 *
		 * Order: a `.bgsm` (the shader Name resolves it), else the BGEM's own
		 * base map, else the texture set's diffuse. A `.bgem` is deliberately
		 * NOT used as the material string -- see isMaterialFile() above; it
		 * cannot resolve in a BSLightingShaderProperty's Name and the shape
		 * came out magenta. The bucket key is this same string, so two shapes
		 * that draw the same still share one BSTriShape. */
		QString mat;
		if ( isMaterialFile( s.matName ) )
			mat = s.matName;
		else if ( !s.effectTex0.isEmpty() )
			mat = s.effectTex0;
		else
			mat = s.tex0;
		const QString key = QStringLiteral( "%1|%2|%3|%4|%5|%6" ).arg( mat )
			.arg( s.hasAlpha ? 1 : 0 ).arg( int( s.alphaThreshold ) )
			.arg( s.ownEmit ? 1 : 0 ).arg( withColour ? 1 : 0 )
			.arg( ( mat.isEmpty() && s.matUnreadable ) ? 1 : 0 );
		auto it = buckets.find( key );
		if ( it != buckets.end() )
			return it.value();
		Bucket b;
		b.name = mat.isEmpty()
			? ( s.matUnreadable ? QStringLiteral( "material unreadable" )
			                    : QStringLiteral( "untextured" ) )
			: QFileInfo( mat ).fileName();
		b.matString = mat;
		b.matUnreadable = mat.isEmpty() && s.matUnreadable;
		b.normalTex = s.tex1;
		b.specTex = s.tex7;
		b.hasAlpha = s.hasAlpha;
		b.alphaThreshold = s.alphaThreshold;
		b.emits = s.ownEmit;
		b.emissiveScale = s.emitMult;
		b.withColour = withColour;
		return buckets.insert( key, b ).value();
	};

	const bool colouring = spec.overlay != CellOverlay::None;

	for ( const Placement & p : placements ) {
		const EsmLodBase & lb = world.lodBase( p.base );
		const QString model = lb.model;
		if ( model.isEmpty() ) {
			skippedByType[CellPickTable::typeName( lb.type )]++;
			continue;
		}
		auto mit = modelCache.find( model );
		if ( mit == modelCache.end() ) {
			if ( modelsFailed.contains( model ) )
				continue;
			std::vector<NativeSrcShape> shapes;
			// ONE LOAD PER DISTINCT MODEL -- the whole block shares this cache.
			if ( !lodgenNativeLoadModel( const_cast<QString *>( &dataRoot ), model, &shapes )
				|| shapes.empty() ) {
				modelsFailed.insert( model );
				continue;
			}
			modelLoads++;
			mit = modelCache.insert( model, shapes );
		}

		// the overlay key for THIS placement
		float rgb[3] = { 1.0f, 1.0f, 1.0f };
		quint64 okey = 0;
		bool haveKey = false;
		switch ( spec.overlay ) {
		case CellOverlay::RecordType:
			okey = quint64( lb.type );
			haveKey = true;
			break;
		case CellOverlay::HasLod: {
			quint32 mask = 0;
			for ( int k = 0; k < 4; k++ ) {
				if ( !lb.models[k].isEmpty() )
					mask |= 1U << k;
			}
			okey = quint64( mask ) | 0x100ULL;
			haveKey = true;
			break;
		}
		case CellOverlay::Layer:
#ifdef ESM_HAS_CELL_FIELDS
			okey = quint64( p.layerForm ) | 0x200000000ULL;
			haveKey = true;
#endif
			break;
		case CellOverlay::Precombined:
			// XCRI is not read by this build; see the notes line
			break;
		case CellOverlay::Identity: {
			/* THE .lodi GROUP (lane CELLVIEW2). The key is the group the bake put
			 * this reference in, made file-wide in cellidentity.cpp because a
			 * `.lodi` group id is dense PER CHUNK.
			 *
			 * A reference the bake never saw does NOT simply go grey any more
			 * (lane CELLVIEW3). Whether that is correct depends on a fact the
			 * `.lodi` cannot supply: does the base carry a LOD model at all?
			 * With no model there is nothing to bake and grey is the right
			 * answer; WITH a model and no group, something lost the reference,
			 * and that is a defect that must not share a colour with 154
			 * perfectly ordinary bushes and shelves. */
			const auto idIt = identity.constFind( p.ref );
			if ( idIt != identity.constEnd() ) {
				okey = quint64( idIt->group ) | 0x400000000000ULL;
				haveKey = true;
			} else {
				okey = p.expectLod ? OVERLAY_KEY_ORPHAN : OVERLAY_KEY_NOLOD;
				haveKey = true;
			}
			break;
		}
		default:
			break;
		}
		if ( haveKey ) {
			overlayKeyColour( okey, rgb );
			overlayLegend[okey]++;
		} else if ( colouring ) {
			// the overlay has nothing to say about this placement: flat mid
			// grey, counted, so a picture that is all grey says "no data",
			// not "no objects"
			overlayKeyColour( OVERLAY_KEY_NONE, rgb );
			overlayLegend[OVERLAY_KEY_NONE]++;
		}
		if ( p.disabled ) {
			// tinted, so a shown-disabled ref is never mistaken for a live one
			rgb[0] = qMin( 1.0f, rgb[0] * 0.4f + 0.6f );
			rgb[1] *= 0.35f;
			rgb[2] *= 0.35f;
		}

		CellPickEntry pick;
		pick.refForm = p.ref;
		pick.baseForm = p.base;
		pick.baseType = lb.type;
		pick.scolPart = p.part;
		pick.model = model;
		pick.pos[0] = p.pos[0];
		pick.pos[1] = p.pos[1];
		pick.pos[2] = p.pos[2];
		pick.rot[0] = p.storedRot[0];
		pick.rot[1] = p.storedRot[1];
		pick.rot[2] = p.storedRot[2];
		pick.drawPos[0] = p.pos[0];
		pick.drawPos[1] = p.pos[1];
		pick.drawPos[2] = p.pos[2];
		pick.scale = p.scale;
		pick.cellX = p.cellX;
		pick.cellY = p.cellY;
		pick.persistent = p.persistent;
		pick.disabled = p.disabled;
		pick.marker = isMarkerModel( model );
		pick.hasLod = lb.hasLod;
		{   // lane CELLVIEW2: the `.lodi` group, for the pick panel's rows
			const auto idIt = identity.constFind( p.ref );
			if ( idIt != identity.constEnd() ) {
				pick.group = idIt->group;
				pick.groupSize = idIt->size;
				pick.haveGroup = true;
			}
		}
		for ( int k = 0; k < 4; k++ )
			pick.lodModels[k] = lb.models[k];
#ifdef ESM_HAS_CELL_FIELDS
		pick.baseEdid = lb.edid;
		pick.layer = p.layerForm;
		pick.enableParent = p.enableParentForm;
		pick.enableParentOpposite = p.enableParentOppositeFlag;
#endif
		Vector3 lo( 3.4e38f, 3.4e38f, 3.4e38f ), hi( -3.4e38f, -3.4e38f, -3.4e38f );

		for ( const NativeSrcShape & s : mit.value() ) {
			const size_t nv = s.geom.pos.size() / 3;
			if ( !nv || s.geom.tris.empty() )
				continue;
			Bucket & b = bucketFor( s, colouring );
			/* COUNTED, NOT GUESSED (lane CELLVIEW3). A shape drawn neutral
			 * because its material would not read is a fact the census has to
			 * state, or "no magenta in the picture" would just mean the failure
			 * got quieter. Per drawn SHAPE, not per bucket: the buckets weld. */
			if ( b.matUnreadable ) {
				shapesUnreadableMat++;
				if ( unreadableMatNames.size() < 8 && !s.matName.isEmpty()
					&& !unreadableMatNames.contains( s.matName ) )
					unreadableMatNames.append( s.matName );
			} else if ( !s.effectTex0.isEmpty() ) {
				shapesFromEffectMat++;
			}
			const int base = int( b.verts.size() );
			for ( size_t v = 0; v < nv; v++ ) {
				const Vector3 lp( s.geom.pos[v * 3 + 0], s.geom.pos[v * 3 + 1],
					s.geom.pos[v * 3 + 2] );
				const Vector3 ln( s.geom.nrm.size() >= ( v + 1 ) * 3
					? Vector3( s.geom.nrm[v * 3 + 0], s.geom.nrm[v * 3 + 1], s.geom.nrm[v * 3 + 2] )
					: Vector3( 0.0f, 0.0f, 1.0f ) );
				const Vector3 lt( s.geom.tan.size() >= ( v + 1 ) * 3
					? Vector3( s.geom.tan[v * 3 + 0], s.geom.tan[v * 3 + 1], s.geom.tan[v * 3 + 2] )
					: Vector3( 1.0f, 0.0f, 0.0f ) );
				OutVert o;
				const Vector3 wp = p.pos + p.rot * ( lp * p.scale );
				o.pos = wp - origin;
				o.nrm = p.rot * ln;
				o.tan = p.rot * lt;
				o.bit = Vector3::crossproduct( o.nrm, o.tan );
				if ( o.bit.length() < 1.0e-6f )
					o.bit = Vector3( 0.0f, 0.0f, 1.0f );
				o.uv = s.geom.uv.size() >= ( v + 1 ) * 2
					? Vector2( s.geom.uv[v * 2 + 0], s.geom.uv[v * 2 + 1] )
					: Vector2( 0.0f, 0.0f );
				o.chan[0] = rgb[0];
				o.chan[1] = rgb[1];
				o.chan[2] = rgb[2];
				b.verts.push_back( o );
				for ( int k = 0; k < 3; k++ ) {
					lo[k] = qMin( lo[k], wp[k] );
					hi[k] = qMax( hi[k], wp[k] );
				}
			}
			for ( size_t t = 0; t + 2 < s.geom.tris.size(); t += 3 ) {
				b.tris.push_back( Triangle( quint16( base + int( s.geom.tris[t + 0] ) ),
					quint16( base + int( s.geom.tris[t + 1] ) ),
					quint16( base + int( s.geom.tris[t + 2] ) ) ) );
			}
			pick.triangles += quint32( s.geom.tris.size() / 3 );
			srcTris += qint64( s.geom.tris.size() / 3 );
		}
		if ( pick.triangles ) {
			for ( int k = 0; k < 3; k++ ) {
				pick.bmin[k] = lo[k];
				pick.bmax[k] = hi[k];
			}
			picks.append( pick );
			drawn++;
		}
	}

	// ---- the ground, the water and the grid
	int landsDrawn = 0, waterCells = 0;
	QString groundNote;   // lane CELLVIEW2: the painted ground's own census
	if ( spec.terrain || spec.water || spec.grid ) {
		Bucket ground;
		ground.name = QStringLiteral( "landscape" );
		ground.withColour = true;
		Bucket waterB;
		waterB.name = QStringLiteral( "water" );
		waterB.withColour = true;
		Bucket gridB;
		gridB.name = QStringLiteral( "cell grid" );
		gridB.withColour = true;

		/* THE PAINTED GROUND (lane CELLVIEW2). Built for the WHOLE rectangle in
		 * one call, so every landscape texture is resolved once, then welded
		 * into this file's own buckets with this file's own vertex writer --
		 * there is no second geometry path. The HEIGHTS were never missing
		 * (VHGT is decoded in EsmWorld::land and the first build already drew
		 * the relief); what was missing is the PAINT, and that is what this
		 * adds. It does not blend -- it is a hard-edged mosaic of the strongest
		 * layer per quad, and the census line says so (see cellground.h). */
		QVector<Bucket> groundBuckets;
		/* THE BLENDED GROUND (lane CELLVIEW4). The mosaic below takes ONE
		 * texture per 128-unit quad, so the ground is a hard-edged grid of
		 * tiles. This draws the quadrant's BTXT and then every ATXT layer
		 * over it in paint order, each with its own VTXT opacity in the
		 * vertex colour's alpha. It owns the rectangle when it builds
		 * anything; the mosaic stays as the fallback, exactly as the
		 * mosaic is itself the fallback for the vertex-colour sheet. */
		QString splatNote;   // carried past the mosaic fallback (lane CELLVIEW4B)
		/* THE CAP, AND THE HARNESS HOOK THAT LETS IT BE PROVEN (lane CELLVIEW4B).
		 * Unset, this is CELL_MAX_TOTAL_VERTS and nothing has changed. Set, the
		 * blend refuses at a rectangle a gate can actually afford to open:
		 * Sanctuary -20,7 costs 8,936 land vertices, so proving the 12,000,000
		 * cap honestly would mean a 38x38-cell block, and a refusal path that is
		 * too expensive to run is a refusal path nobody knows works. Same family
		 * as WW_CELL_NOTERRAIN / NOWATER / NOGRID above: it forces the state the
		 * measurement needs. It is not a feature switch and not a way back. */
		qint64 splatCap = qint64( CELL_MAX_TOTAL_VERTS );
		{
			bool capOk = false;
			const qint64 v = qEnvironmentVariableIntValue( "WW_CELL_SPLAT_CAP", &capOk );
			if ( capOk && v > 0 )
				splatCap = v;
		}
		if ( spec.terrain ) {
			const qint64 splatVerts = cellSplatCountVerts( world, x0, y0, x1, y1 );
			/* THE BUDGET IS PRINTED WHETHER OR NOT IT REFUSES (lane CELLVIEW4B).
			 * The count was computed and then discarded on the success path, so
			 * the one number that decides the refusal was unreadable in the only
			 * case anybody ever looks at. */
			splatNote = QStringLiteral( "; %L1 land vertices counted before "
				"allocating, against the %L2 cap" )
				.arg( splatVerts ).arg( splatCap );
			/* THE CAP IS CHECKED BEFORE THE ALLOCATION, not after: a splat
			 * emits one quad per contributing layer, so a heavily painted
			 * block costs a multiple of the mosaic's fixed 4096 verts per
			 * cell. Refusing here leaves the mosaic to draw the rectangle. */
			if ( splatVerts > 0 && splatVerts <= splatCap ) {
				CellSplatBuild sb;
				QString serr;
				if ( cellBuildSplat( world, x0, y0, x1, y1, origin[0], origin[1],
					CELL_GROUND_TILING, sb, &serr ) && !sb.quads.empty() ) {
					landsDrawn = sb.cells;
					groundNote = cellSplatLegend( sb ) + splatNote;
					splatNote.clear();
					groundBuckets.resize( sb.buckets.size() );
					for ( int bi = 0; bi < sb.buckets.size(); bi++ ) {
						Bucket & gbk = groundBuckets[bi];
						const CellSplatBucket & src = sb.buckets.at( bi );
						gbk.name = src.diffuse.isEmpty()
							? QStringLiteral( "landscape" )
							: QFileInfo( src.diffuse ).fileName();
						gbk.matString = src.diffuse;
						gbk.normalTex = src.normal;
						gbk.withColour = true;
						/* A layer pass blends; a base pass is opaque. Threshold 0 is
						 * how emitBucket above tells the two apart. */
						gbk.hasAlpha = src.blend;
						gbk.alphaThreshold = 0;
					}
					for ( const CellSplatQuad & q : sb.quads ) {
						if ( q.bucket < 0 || q.bucket >= groundBuckets.size() )
							continue;
						Bucket & gbk = groundBuckets[q.bucket];
						const int base = int( gbk.verts.size() );
						for ( int k = 0; k < 4; k++ ) {
							OutVert o;
							o.pos = Vector3( q.v[k].p[0], q.v[k].p[1], q.v[k].p[2] );
							o.nrm = Vector3( q.nrm[0], q.nrm[1], q.nrm[2] );
							o.tan = Vector3( 1.0f, 0.0f, 0.0f );
							o.bit = Vector3::crossproduct( o.nrm, o.tan );
							if ( o.bit.length() < 1.0e-6f )
								o.bit = Vector3( 0.0f, 1.0f, 0.0f );
							o.uv = Vector2( q.v[k].uv[0], q.v[k].uv[1] );
							for ( int c = 0; c < 3; c++ )
								o.chan[c] = q.v[k].rgb[c];
							o.chan[3] = q.v[k].w;
							gbk.verts.push_back( o );
						}
						gbk.tris.push_back( Triangle( quint16( base ), quint16( base + 1 ),
							quint16( base + 2 ) ) );
						gbk.tris.push_back( Triangle( quint16( base ), quint16( base + 2 ),
							quint16( base + 3 ) ) );
					}
				}
			} else if ( splatVerts > splatCap ) {
				/* THE REFUSAL HAS TO SURVIVE THE FALLBACK (lane CELLVIEW4B). It
				 * was written into groundNote here and the mosaic below then
				 * overwrote groundNote with its own legend, so past the cap the
				 * census said nothing about a refusal at all -- a silent
				 * downgrade, which CONSTITUTION 10 forbids by name. It is
				 * carried in splatNote instead and prefixed onto whatever the
				 * fallback says. */
				splatNote = QStringLiteral( "ground: the BLEND REFUSED -- %L1 vertices "
					"is past the %L2 cap; the hard-edged mosaic was drawn instead. " )
					.arg( splatVerts ).arg( splatCap );
			}
		}
		// the hard-edged mosaic, now the FALLBACK for the blend above (lane CELLVIEW4)
		if ( spec.terrain && groundBuckets.isEmpty() ) {
			CellGroundBuild gb;
			QString gerr;
			if ( cellBuildGround( world, x0, y0, x1, y1, origin[0], origin[1],
					CELL_GROUND_TILING, gb, &gerr ) && !gb.quads.empty() ) {
				landsDrawn = gb.cells;
				groundNote = splatNote + cellGroundLegend( gb );
				groundBuckets.resize( gb.buckets.size() );
				for ( int bi = 0; bi < gb.buckets.size(); bi++ ) {
					Bucket & gbk = groundBuckets[bi];
					const CellGroundBucket & src = gb.buckets.at( bi );
					/* matString takes EITHER a `.bgsm` or a diffuse texture -- the
					 * same slot the model buckets use, so a material-backed TXST
					 * and a plain TX00 travel the one path. An EMPTY diffuse is
					 * not neutral: it binds the missing-texture magenta under
					 * Scene::DoErrorColor, which is why the bare bucket keeps the
					 * `#AARRGGBB` pseudo-texture emitBucket already gives it. */
					gbk.name = src.diffuse.isEmpty()
						? QStringLiteral( "landscape" )
						: QFileInfo( src.diffuse ).fileName();
					gbk.matString = src.diffuse;
					gbk.normalTex = src.normal;
					gbk.withColour = true;
				}
				for ( const CellGroundQuad & q : gb.quads ) {
					if ( q.bucket < 0 || q.bucket >= groundBuckets.size() )
						continue;
					Bucket & gbk = groundBuckets[q.bucket];
					const int base = int( gbk.verts.size() );
					for ( int k = 0; k < 4; k++ ) {
						OutVert o;
						o.pos = Vector3( q.v[k].p[0], q.v[k].p[1], q.v[k].p[2] );
						o.nrm = Vector3( q.nrm[0], q.nrm[1], q.nrm[2] );
						o.tan = Vector3( 1.0f, 0.0f, 0.0f );
						o.bit = Vector3::crossproduct( o.nrm, o.tan );
						if ( o.bit.length() < 1.0e-6f )
							o.bit = Vector3( 0.0f, 1.0f, 0.0f );
						o.uv = Vector2( q.v[k].uv[0], q.v[k].uv[1] );
						for ( int c = 0; c < 3; c++ )
							o.chan[c] = q.v[k].rgb[c];
						gbk.verts.push_back( o );
					}
					// the same quint16 index pair appendQuad writes; emitBucket does
					// the splitting, on triangle boundaries
					gbk.tris.push_back( Triangle( quint16( base ), quint16( base + 1 ),
						quint16( base + 2 ) ) );
					gbk.tris.push_back( Triangle( quint16( base ), quint16( base + 2 ),
						quint16( base + 3 ) ) );
				}
			} else {
				groundNote = QStringLiteral( "ground: the painted mosaic REFUSED (%1) -- the vertex-colour sheet was drawn instead" )
					.arg( gerr.isEmpty() ? QStringLiteral( "no LAND in the rectangle" ) : gerr );
			}
		}

		for ( int y = y0; y <= y1; y++ ) {
			for ( int x = x0; x <= x1; x++ ) {
				EsmLand land;
				/* lane CELLVIEW2: the PAINTED ground above owns this rectangle when
				 * it built anything. The vertex-colour-only sheet stays as the
				 * fallback for a refusal, so the viewer is never left with no ground
				 * at all -- and the two are never drawn on top of each other. */
				const bool haveLand = spec.terrain && groundBuckets.isEmpty()
					&& world.land( x, y, land );
				if ( haveLand ) {
					landsDrawn++;
					const float ox = float( x ) * CELL_UNITS;
					const float oy = float( y ) * CELL_UNITS;
					const float step = CELL_UNITS / float( LAND_GRID - 1 );
					for ( int row = 0; row + 1 < LAND_GRID; row++ ) {
						for ( int col = 0; col + 1 < LAND_GRID; col++ ) {
							const float xs[2] = { ox + float( col ) * step,
								ox + float( col + 1 ) * step };
							const float ys[2] = { oy + float( row ) * step,
								oy + float( row + 1 ) * step };
							const Vector3 a( xs[0] - origin[0], ys[0] - origin[1],
								land.heights[row][col] );
							const Vector3 b2( xs[1] - origin[0], ys[0] - origin[1],
								land.heights[row][col + 1] );
							const Vector3 c( xs[1] - origin[0], ys[1] - origin[1],
								land.heights[row + 1][col + 1] );
							const Vector3 d( xs[0] - origin[0], ys[1] - origin[1],
								land.heights[row + 1][col] );
							float rgb[3] = { 0.5f, 0.5f, 0.5f };
							if ( land.hasColors ) {
								for ( int k = 0; k < 3; k++ )
									rgb[k] = float( land.colors[row][col][k] ) / 255.0f;
							}
							Vector3 n = Vector3::crossproduct( b2 - a, d - a );
							if ( n.length() > 1.0e-6f )
								n.normalize();
							else
								n = Vector3( 0.0f, 0.0f, 1.0f );
							appendQuad( ground, a, b2, c, d, n, rgb );
						}
					}
				}
				float wh = 0.0f;
				if ( spec.water && world.cellWater( x, y, wh ) ) {
					waterCells++;
					const float ox = float( x ) * CELL_UNITS - origin[0];
					const float oy = float( y ) * CELL_UNITS - origin[1];
					const float rgb[3] = { 0.18f, 0.32f, 0.42f };
					appendQuad( waterB,
						Vector3( ox, oy, wh ),
						Vector3( ox + CELL_UNITS, oy, wh ),
						Vector3( ox + CELL_UNITS, oy + CELL_UNITS, wh ),
						Vector3( ox, oy + CELL_UNITS, wh ),
						Vector3( 0.0f, 0.0f, 1.0f ), rgb );
				}
				if ( spec.grid ) {
					/* The cell border as four thin vertical curtains, so the
					 * grid reads from above AND from inside the block. The CK
					 * draws a flat line on the ground; a flat line is invisible
					 * the moment the camera is level with it, which is most of
					 * the time in a viewer whose whole point is looking at
					 * buildings. The coordinates themselves are NOT drawn --
					 * that needs text geometry this lane does not have -- they
					 * are in the notes and in the pick panel. */
					const float ox = float( x ) * CELL_UNITS - origin[0];
					const float oy = float( y ) * CELL_UNITS - origin[1];
					float base = 0.0f;
					EsmLand l2;
					if ( world.land( x, y, l2 ) )
						base = l2.heights[0][0];
					const float h = 512.0f;
					const float rgb[3] = { 0.85f, 0.75f, 0.25f };
					const float c[4][2] = { { ox, oy }, { ox + CELL_UNITS, oy },
						{ ox + CELL_UNITS, oy + CELL_UNITS }, { ox, oy + CELL_UNITS } };
					for ( int e = 0; e < 4; e++ ) {
						const int f = ( e + 1 ) % 4;
						appendQuad( gridB,
							Vector3( c[e][0], c[e][1], base ),
							Vector3( c[f][0], c[f][1], base ),
							Vector3( c[f][0], c[f][1], base + h ),
							Vector3( c[e][0], c[e][1], base + h ),
							Vector3( 0.0f, 0.0f, 1.0f ), rgb );
					}
				}
			}
		}
		if ( !ground.verts.empty() )
			buckets.insert( QStringLiteral( "\x01ground" ), ground );
		for ( int bi = 0; bi < groundBuckets.size(); bi++ ) {   // lane CELLVIEW2
			if ( groundBuckets[bi].verts.empty() )
				continue;
			// sorted after the sheet and before the water, one shape per texture
			buckets.insert( QStringLiteral( "\x01land%1" )
				.arg( bi, 3, 10, QLatin1Char( '0' ) ), groundBuckets[bi] );
		}
		if ( !waterB.verts.empty() )
			buckets.insert( QStringLiteral( "\x01water" ), waterB );
		if ( !gridB.verts.empty() )
			buckets.insert( QStringLiteral( "\x01grid" ), gridB );
	}

	// ---- emit
	qint64 shapes = 0, verts = 0, tris = 0;
	bool ok = true;
	QStringList keys = buckets.keys();
	std::sort( keys.begin(), keys.end() );
	/* THE GROUND'S OWN ORDERED NODE (lane CELLVIEW4B). Created lazily, and
	 * only when there is a land bucket to put in it, so a cell with the
	 * terrain off produces the identical document it produced before.
	 * It must be inserted BEFORE the first land shape, because the sort it
	 * enables is by BLOCK NUMBER: the land buckets are keyed "\x01land%03d"
	 * in bucket order, the keys are sorted here, so the shapes are created in
	 * paint order and their block numbers ascend in paint order with them. */
	QPersistentModelIndex iGround;
	for ( const QString & k : keys ) {
		QModelIndex iParent = iRoot;
		if ( k.startsWith( QLatin1String( "\x01land" ) ) ) {
			if ( !iGround.isValid() ) {
				QModelIndex iG = nif->insertNiBlock( QStringLiteral( "BSOrderedNode" ) );
				nif->set<QString>( iG, "Name", QStringLiteral( "ground" ) );
				nif->set<quint32>( iG, "Flags", 14 );
				nif->set<float>( iG, "Scale", 1.0f );
				addLink( nif, iRoot, QStringLiteral( "Children" ),
					nif->getBlockNumber( iG ) );
				iGround = iG;
			}
			iParent = iGround;
		}
		if ( !emitBucket( nif, iParent, buckets.value( k ), origin, shapes, verts, tris, error ) ) {
			ok = false;
			break;
		}
	}

	/* THE PICK HIGHLIGHT (lane CELLVIEW2) -- created ONCE, here, and MOVED on
	 * every click (src/cellclick.h). The scene WELDS, so the picked reference
	 * is a few hundred vertices inside a shape holding a hundred thousand and
	 * there is nothing for the application's own selection highlight to light
	 * up. This is twelve edge bars of the picked placement's world box, and it
	 * is document geometry, so a screenshot of a pick is a screenshot of the
	 * document and no block appears or disappears when one is made. Forget
	 * first: the previous scene's shape went with its document, and the dock
	 * has to be told the rows it is showing are gone. */
	cellHighlightForget();
	{
		const float hlOrigin[3] = { origin[0], origin[1], origin[2] };
		cellHighlightCreate( nif, iRoot, hlOrigin );
	}

	nif->holdUpdates( false );
	nif->updateModel();

	// ---- the census
	if ( notes ) {
		QString n;
		QTextStream s( &n );
		s << "cell view " << spec.world << " " << spec.cx << "," << spec.cy
		  << " block " << spec.n << "x" << spec.n
		  << " (cells " << x0 << "," << y0 << " .. " << x1 << "," << y1 << ")\n";
		s << "  plugins: " << spec.plugins << "\n";
		s << "  data root: " << ( dataRoot.isEmpty() ? QStringLiteral( "(none)" ) : dataRoot )
		  << " -- from " << dataRootSource << "\n";
		s << "  cells with data: " << cellsWithData << " of " << ( spec.n * spec.n ) << "\n";

		/* THE LOADED CELLS, BY NAME (lane CELLWORK1). One line per cell, in the
		 * order the block was walked, so an independent reader can check the
		 * names and the per-cell counts against the plugin without a window.
		 * A cell with no EDID prints its grid coords alone -- that is a cell
		 * with no editor id, not a cell we failed to look at. */
		{
			const CellRefTable & cellsModel = cellRefTable();
			for ( int ci = 0; ci < cellsModel.cellCount(); ci++ ) {
				const CellBlockEntry & c = cellsModel.cellAt( ci );
				s << "  cell " << cellBlockLabel( c )
				  << " form 0x" << QString::number( c.cellForm, 16 ).rightJustified( 8, '0' )
				  << " references " << c.references << ", drawn " << c.drawn << "\n";
			}
		}
		s << "  refrs read " << refsRead << ", placements " << placements.size()
		  << ", drawn " << drawn << "\n";
		s << "  hidden: disabled " << refsHidden << ", markers " << refsMarker
		  << ", deleted " << refsDeleted << ", no base " << refsNoBase << "\n";
		if ( !skippedByType.isEmpty() ) {
			QStringList sk;
			QStringList st = skippedByType.keys();
			std::sort( st.begin(), st.end() );
			for ( const QString & t : st )
				sk.append( QStringLiteral( "%1 %2" ).arg( t ).arg( skippedByType.value( t ) ) );
			s << "  skipped, no model on the base: " << sk.join( QLatin1String( ", " ) ) << "\n";
		}
		s << "  distinct models loaded " << modelLoads << ", failed to load "
		  << modelsFailed.size() << "\n";
		s << "  source triangles " << srcTris << ", welded shapes " << shapes
		  << ", vertices " << verts << ", triangles " << tris << "\n";
		/* THE MATERIAL LINE (lane CELLVIEW3). Magenta in this viewer means one
		 * thing and one thing only: a texture path the renderer went looking
		 * for and did not find. Every other way a surface can fail is stated
		 * here as a number instead of being painted pink. */
		s << "  materials: " << shapesFromEffectMat
		  << " shapes textured from a `.bgem` effect material, "
		  << shapesUnreadableMat << " drawn neutral grey because a named material "
		     "resolved to nothing";
		if ( !unreadableMatNames.isEmpty() )
			s << " (" << unreadableMatNames.join( QLatin1String( ", " ) ) << ")";
		s << "\n";
		if ( groundNote.isEmpty() ) {   // lane CELLVIEW2
			s << "  ground: " << landsDrawn << " LAND cells, vertex colour only"
			  << " (the splat layers are NOT sampled -- that is the terrain bake's compositor)\n";
		} else {
			s << "  " << groundNote << "\n";
		}
		s << "  water: " << waterCells << " cells\n";
		if ( !identityCensus.path.isEmpty() || !identityError.isEmpty() ) {   // lane CELLVIEW2
			s << "  " << cellIdentityLegend( identityCensus );
			if ( !identityError.isEmpty() )
				s << " -- REFUSED: " << identityError;
			s << "\n";
		}
		s << "  overlay: " << ( spec.overlay == CellOverlay::None
			? QStringLiteral( "none" ) : cellOverlayName( spec.overlay ) );
		if ( spec.overlay == CellOverlay::Identity && identity.isEmpty() )   // lane CELLVIEW2
			s << " -- REFUSED: " << ( identityError.isEmpty()
				? QStringLiteral( "no `.lodi` bake is loaded (WW_CELL_LODI), so every placement is grey" )
				: identityError );
		else if ( spec.overlay == CellOverlay::Precombined )
			s << " -- REFUSED: this build does not read XCRI, so every placement is grey";
#ifndef ESM_HAS_CELL_FIELDS
		else if ( spec.overlay == CellOverlay::Layer )
			s << " -- REFUSED: this build does not read XLYR, so every placement is grey";
#endif
		s << "\n";
		if ( !overlayLegend.isEmpty() ) {
			s << "  legend: " << overlayLegend.size() << " buckets\n";
			QList<quint64> lk = overlayLegend.keys();
			std::sort( lk.begin(), lk.end() );
			for ( quint64 k : lk ) {
				float rgb[3];
				/* THE SAME FUNCTION THE DRAW SITE CALLED (lane CELLVIEW3).
				 * This line used to call overlayColour() unconditionally and so
				 * printed a wheel colour -- mauve 0.60,0.21,0.37 -- for the grey
				 * sentinel: the legend contradicted the picture it described.
				 * One function now, so there is no second answer to disagree
				 * with; the gate row that samples the rendered image at a known
				 * placement is the refuter. */
				overlayKeyColour( k, rgb );
				const QString label = overlayKeyLabel( k );
				s << "    key 0x" << QString::number( k, 16 ) << "  "
				  << overlayLegend.value( k ) << " placements  rgb "
				  << QString::number( rgb[0], 'f', 2 ) << ","
				  << QString::number( rgb[1], 'f', 2 ) << ","
				  << QString::number( rgb[2], 'f', 2 );
				if ( !label.isEmpty() )
					s << "  " << label;
				s << "\n";
			}
		}
		s << "  pick table " << picks.size() << " entries\n";
		/* THE REFERENCE MODEL, READ BACK (lane CELLWORK1). Not a second count of
		 * the same thing: `refrs read` above is a counter the loop increments,
		 * this is the SIZE OF THE TABLE the workspace's list actually shows, and
		 * the fates are counted by walking that table rather than by reading the
		 * counters beside it. If the two disagree, the list is showing something
		 * the census does not describe -- which is the failure this line exists
		 * to make visible instead of invisible. */
		const CellRefTable & refModel = cellRefTable();
		s << "  reference model " << refModel.size() << " entries (drawn "
		  << refModel.countOfFate( CellRefFate::Drawn ) << ", no model "
		  << refModel.countOfFate( CellRefFate::NoModel ) << ", marker "
		  << refModel.countOfFate( CellRefFate::Marker ) << ", disabled "
		  << refModel.countOfFate( CellRefFate::Disabled ) << ", deleted "
		  << refModel.countOfFate( CellRefFate::Deleted ) << ", no base "
		  << refModel.countOfFate( CellRefFate::NoBase ) << ")\n";
		s << "  built in " << clock.elapsed() << " ms\n";
#ifndef ESM_HAS_CELL_FIELDS
		s << "  NOTE: built without the esmdata cell fields (XLYR, XESP, editor ids and "
		     "the MODL of MSTT/FURN/CONT/DOOR/ACTI/FLOR/LIGH). Those records are counted "
		     "as skipped above rather than drawn.\n";
#endif
		*notes = n;
	}

	// ---- the placement dump: a gate cannot count placements in welded geometry
	const QByteArray dump = qgetenv( "WW_CELL_DUMP" );
	if ( !dump.isEmpty() ) {
		QFile f( QString::fromLocal8Bit( dump ) );
		if ( f.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
			QTextStream s( &f );
			/* The AABB is in the dump ON PURPOSE. The position alone only proves
			 * the reader read; the box depends on the ROTATION CONVENTION and
			 * the scale as well, so an independent walk that recomputes it from
			 * the model's own bounds is a control on the transform and not just
			 * on the parse. It is what the gate's red run flips. */
			s << "# ref base type part cellX cellY x y z rx ry rz scale tris "
			     "bminx bminy bminz bmaxx bmaxy bmaxz model\n";
			for ( int i = 0; i < picks.size(); i++ ) {
				const CellPickEntry & e = picks.at( i );
				s << CellPickTable::formName( e.refForm ) << " "
				  << CellPickTable::formName( e.baseForm ) << " "
				  << CellPickTable::typeName( e.baseType ) << " " << e.scolPart << " "
				  << e.cellX << " " << e.cellY << " "
				  << QString::number( e.pos[0], 'f', 3 ) << " "
				  << QString::number( e.pos[1], 'f', 3 ) << " "
				  << QString::number( e.pos[2], 'f', 3 ) << " "
				  << QString::number( e.rot[0], 'f', 6 ) << " "
				  << QString::number( e.rot[1], 'f', 6 ) << " "
				  << QString::number( e.rot[2], 'f', 6 ) << " "
				  << QString::number( e.scale, 'f', 4 ) << " "
				  << e.triangles << " "
				  << QString::number( e.bmin[0], 'f', 3 ) << " "
				  << QString::number( e.bmin[1], 'f', 3 ) << " "
				  << QString::number( e.bmin[2], 'f', 3 ) << " "
				  << QString::number( e.bmax[0], 'f', 3 ) << " "
				  << QString::number( e.bmax[1], 'f', 3 ) << " "
				  << QString::number( e.bmax[2], 'f', 3 ) << " "
				  << e.model << "\n";
			}
		} else {
			qWarning() << "WW_CELL_DUMP: could not write" << dump;
		}
	}

	/* WW_CELL_REFDUMP -- THE REFERENCE MODEL, not the draw data (lane CELLWORK1).
	 *
	 * WW_CELL_DUMP above is one line per PICKABLE thing, which is neither more
	 * nor less than what was welded into the scene: an SCOL is several lines, a
	 * light is none. This dump is one line per REFR the plugin returned, with
	 * the builder's own reason when it is not on screen -- so a gate can compare
	 * its line count against an independent walk of the same plugin
	 * (tests/spells/cell_census.py) and a disagreement is a reading defect
	 * rather than a drawing one. The two dumps together are how "the list shows
	 * everything, and the viewport draws some of it" is checked as arithmetic. */
	const QByteArray refdump = qgetenv( "WW_CELL_REFDUMP" );
	if ( !refdump.isEmpty() ) {
		QFile f( QString::fromLocal8Bit( refdump ) );
		if ( f.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
			QTextStream s( &f );
			const CellRefTable & rt = cellRefTable();
			for ( int i = 0; i < rt.cellCount(); i++ ) {
				const CellBlockEntry & c = rt.cellAt( i );
				s << "# cell " << cellBlockLabel( c ) << " form 0x"
				  << QString::number( c.cellForm, 16 ).rightJustified( 8, '0' )
				  << " references " << c.references << " drawn " << c.drawn << "\n";
			}
			s << "# ref base type cellX cellY state edid model\n";
			for ( int i = 0; i < rt.size(); i++ ) {
				const CellRefEntry & e = rt.at( i );
				s << CellPickTable::formName( e.refForm ) << " "
				  << CellPickTable::formName( e.baseForm ) << " "
				  << CellPickTable::typeName( e.baseType ) << " "
				  << e.cellX << " " << e.cellY << " "
				  << CellRefTable::fateName( e.fate ).replace( QLatin1Char( ' ' ),
					QLatin1Char( '-' ) ) << " "
				  << ( e.baseEdid.isEmpty() ? QStringLiteral( "-" ) : e.baseEdid ) << " "
				  << ( e.model.isEmpty() ? QStringLiteral( "-" ) : e.model ) << "\n";
			}
		} else {
			qWarning() << "WW_CELL_REFDUMP: could not write" << refdump;
		}
	}

	return ok;
}

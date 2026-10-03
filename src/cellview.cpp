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
#include "cellactor.h"		// lane PLACED1
#include "celldecal.h"		// lane PLACED1
#include "esmdata.h"
#include "esmweather.h"		// lane FOG2: the fog packing
#include "lodgen.h"
#include "nativeemit.h"
#include "probeplace.h"		// lane PRTPPLACE
#include "probebake.h"		// lane PRTPBAKE
#include "probegi.h"		// lane PRTPGI
#include "probealbedo.h"		// lane PRTPBAKE
#include "probefar.h"		// lane BAKEBLOCK1: the far soup beyond the loaded block
#include "cellmodelahead.h"	// lane SPEED1: models parsed on worker threads
#include "cellmesh.h"		// lane SPEED1: the welded geometry beside the document
#include "cellspeed.h"		// lane SPEED1: stage timers (WW_CELL_SPEED_DUMP)

#include <limits>

#include "model/nifmodel.h"
#include "spells/blocks.h"
#include "gl/celllights.h"
#include "gl/cellprobeview.h"	// lane PROBEVIEW1
#include "gl/cellfxlit.h"
#include "gamemanager.h"	// lane IMGS1: the imagespace LUT

#include <QBuffer>
#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QCoreApplication>
#include <QDir>
#include <QRegularExpression>
#include <QSettings>
#include <QTextStream>

#include <algorithm>
#include <array>
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

/* A BUCKET TRIANGLE IS 32-BIT (lane PRTPPLACE, 2026-09-30). Triangle holds quint16, and a
 * bucket welds every shape of one material across the whole block: Concord 5x5 passes 65,536
 * vertices on the shared trim and siding materials, the offset wrapped, and later pieces drew
 * with earlier pieces' vertices -- missing porch rails and roof trim, grey wedges over the
 * town (bungo 2026-09-30). The writer cuts each bucket into <= MAX_SHAPE_VERTS shapes and
 * remaps to 16 bits there, per shape, where it fits. */
struct BucketTri
{
	quint32 v[3];
	quint32 operator[]( int k ) const { return v[k]; }
};

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
	/*! 2026-10-01: GLASS. A blended source (NiAlphaProperty bit 0, a BGSM's or a
	 *  BGEM's bAlphaBlend) blends here too, at the material's fAlpha -- before
	 *  this every car window was drawn as an opaque sheet. */
	bool blend = false;
	float alpha = 1.0f;
	/* A BGEM shape (shattered car glass, 2026-10-01): drawn through a
	 * BSEffectShaderProperty that names the BGEM, with the source property's flags,
	 * so the renderer applies the material's palette alpha -- the holes -- and the
	 * vertex alpha the way the game does. Empty for every other bucket. */
	QString effectMat;
	quint32 effSF1 = 0, effSF2 = 0;
	/* Lane EFX1: an effect property with no BGEM (the Vault's ground steam), the source
	 * block serialized and written back whole, with the source's NiAlphaProperty flags. */
	QByteArray effectBlock;
	quint16 effAlphaFlags = 0;
	/* Lane EFX1: a Refraction-flagged lighting shape (the walkway's splash rings): written back
	 * with Shader Flags 1 bit 15 and its strength, so the screen-space refraction preview bends
	 * what lies behind it. */
	bool refract = false;
	float refractStrength = 0.0f;
	bool emits = false;
	float emissiveScale = 1.0f;
	bool withColour = false;
	/* Lane GLOW1: one placement's billboard shapes, emitted under their own NiBillboardNode at
	 * bbPos (world) and bbScale, vertices in the node's frame, so the renderer turns them to the
	 * camera as the game does. Welded flat they lie edge-on to the eye: the pods' missing glow. */
	bool billboard = false;
	Vector3 bbPos;
	float bbScale = 1.0f;
	int bbMode = 0;
	//! Lane FXLIT1: one placement's lit effect shapes; the placed model's serial in src/gl/cellfxlit.h, -1 = none.
	int fxLit = -1;
	bool decal = false;   // lane PLACED1: a placed decal's pieces (src/celldecal.h): Decal flag, no depth write
	std::vector<OutVert> verts;
	std::vector<BucketTri> tris;   // 32-bit: a bucket welds far more than 65,536 vertices
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
	// lane MISS1: the black plane lives in the markers folder and is no marker -- the game draws it, in every
	// cell's own combined meshes (its editor-only copy is a shape named EditorMarker, dropped by the loader)
	if ( m.endsWith( QLatin1String( "\\blackplane01.nif" ) ) )
		return false;
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

//! Lane SPEED1: one welded shape's arrays, handed to src/cellmesh.h instead of being written into the document
//! row by row. The values are the ones the rows would hold (the document keeps floats), so the scene reads the
//! same numbers either way; `lo`/`hi` come back for the bounding sphere.
void cellMeshEmit( NifModel * nif, const QModelIndex & iShape, const std::vector<OutVert> & pv,
	const std::vector<Triangle> & pt, bool withColour, Vector3 & lo, Vector3 & hi )
{
	QSharedPointer<CellMesh> m( new CellMesh );
	const int nv = int( pv.size() );
	m->withColour = withColour;
	m->verts.resize( nv );
	m->norms.resize( nv );
	m->tangents.resize( nv );
	m->bitangents.resize( nv );
	m->coords.resize( nv );
	m->colors.fill( Color4( 0.0f, 0.0f, 0.0f, 1.0f ), nv );
	for ( int v = 0; v < nv; v++ ) {
		const OutVert & o = pv[size_t( v )];
		m->verts[v] = o.pos;
		m->norms[v] = o.nrm;
		m->tangents[v] = o.tan;
		m->bitangents[v] = o.bit;
		m->coords[v] = o.uv;
		if ( withColour )
			m->colors[v] = ByteColor4( FloatVector4( o.chan[0], o.chan[1], o.chan[2], o.chan[3] ) );
		for ( int k = 0; k < 3; k++ ) {
			lo[k] = qMin( lo[k], o.pos[k] );
			hi[k] = qMax( hi[k], o.pos[k] );
		}
	}
	m->triangles.reserve( int( pt.size() ) );
	for ( const Triangle & t : pt )
		m->triangles.append( t );
	cellMeshPut( nif, iShape, m );
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
			const BucketTri & t = b.tris[i];
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
		if ( b.fxLit >= 0 )   // lane FXLIT1: the renderer asks for this shape's four lights by block
			wwCellFxLitShape( nif, nif->getBlockNumber( iShape ), b.fxLit );
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
		const bool sideMesh = cellMeshOn();   // lane SPEED1: the rows stay unwritten, the arrays go beside the document
		if ( !sideMesh )
			nif->updateArraySize( iVertexData );
		Vector3 lo( 3.4e38f, 3.4e38f, 3.4e38f ), hi( -3.4e38f, -3.4e38f, -3.4e38f );
		if ( sideMesh )
			cellMeshEmit( nif, iShape, pv, pt, b.withColour, lo, hi );
		for ( size_t v = 0; !sideMesh && v < pv.size(); v++ ) {
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
		if ( !sideMesh ) {
			QModelIndex iTriangles = nif->getIndex( iShape, "Triangles" );
			nif->updateArraySize( iTriangles );
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
		if ( !b.effectMat.isEmpty() || !b.effectBlock.isEmpty() ) {
			QModelIndex iShader = nif->insertNiBlock( QStringLiteral( "BSEffectShaderProperty" ) );
			if ( !b.effectBlock.isEmpty() ) {
				// lane EFX1: the source property whole; its controller (UV scroll) is not welded
				QByteArray eb = b.effectBlock;
				QBuffer buf( &eb );
				if ( buf.open( QIODevice::ReadOnly ) )
					nif->loadIndex( buf, iShader );
				nif->setLink( iShader, "Controller", -1 );
			} else {
				nif->set<QString>( iShader, "Name", materialNameFor( b.effectMat ) );
				nif->set<QString>( iShader, "Source Texture", b.matString );
			}
			nif->set<quint32>( iShader, "Shader Flags 1", b.effSF1 );
			// Vertex_Colors only when this shape really carries them
			nif->set<quint32>( iShader, "Shader Flags 2", b.withColour ? b.effSF2 : ( b.effSF2 & ~0x20U ) );
			nif->setLink( iShape, "Shader Property", nif->getBlockNumber( iShader ) );
			if ( b.hasAlpha ) {
				QModelIndex iAlpha = nif->insertNiBlock( QStringLiteral( "NiAlphaProperty" ) );
				nif->set<int>( iAlpha, "Flags", b.effAlphaFlags ? int( b.effAlphaFlags ) : b.alphaThreshold ? 4844 : 4333 );
				nif->set<int>( iAlpha, "Threshold", int( b.alphaThreshold ) );
				nif->setLink( iShape, "Alpha Property", nif->getBlockNumber( iAlpha ) );
			}
			addLink( nif, iRoot, QStringLiteral( "Children" ), nif->getBlockNumber( iShape ) );
			continue;
		}
		QModelIndex iShader = nif->insertNiBlock( QStringLiteral( "BSLightingShaderProperty" ) );
		nif->set<quint32>( iShader, "Shader Type", 0 );
		/* Lane EFX1: a Refraction bucket keeps Shader Flags 1 bit 15 and its strength, so the
		 * screen-space refraction preview (src/gl/renderer.cpp) bends the scene behind it the
		 * way the game does, instead of showing its normal-map diffuse as a solid swirled disk. */
		nif->set<quint32>( iShader, "Shader Flags 1",
			( b.emits ? 2151677953U : ( 2151677953U & ~0x400000U ) ) | ( b.refract ? 0x8000U : 0U )
			| ( b.decal ? 0x04000000U : 0U ) );   // lane PLACED1: Decal
		if ( b.refract )
			nif->set<float>( iShader, "Refraction Strength", b.refractStrength );
		nif->set<quint32>( iShader, "Shader Flags 2", ( b.withColour ? 0x25U : 5U ) & ( b.decal ? ~1U : ~0U ) );
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
		if ( b.blend )
			nif->set<float>( iShader, "Alpha", b.alpha );
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
	b.tris.push_back( BucketTri{ { quint32( base ), quint32( base + 1 ), quint32( base + 2 ) } } );
	b.tris.push_back( BucketTri{ { quint32( base ), quint32( base + 2 ), quint32( base + 3 ) } } );
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
	/* lane PRTP1: `plugins|interior|<EDID or hex form>[|overlay]` */
	if ( parts.size() >= 3 && parts[1].trimmed().compare( QLatin1String( "interior" ), Qt::CaseInsensitive ) == 0 ) {
		spec.plugins = parts[0].trimmed();
		spec.world = QStringLiteral( "interior" );
		spec.interior = true;
		spec.interiorCell = parts[2].trimmed();
		spec.cx = spec.cy = 0;
		spec.n = 1;
		spec.terrain = spec.water = spec.grid = false;
		if ( parts.size() >= 4 ) {
			bool known = false;
			spec.overlay = cellOverlayFromName( parts[3], &known );
			if ( !known )
				return fail( QStringLiteral( "\"%1\" is not an overlay; try one of: %2" )
					.arg( parts[3].trimmed(), cellOverlayNames() ) );
		}
		if ( spec.plugins.isEmpty() || spec.interiorCell.isEmpty() )
			return fail( QStringLiteral( "expected plugins|interior|<cell>, got \"%1\"" ).arg( line.trimmed() ) );
		spec.valid = true;
		return true;
	}
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
	if ( !qgetenv( "WW_CELL_PROBES_HIDE" ).isEmpty() )
		spec.probesShow = false;    // lane PRTPGI: placed and baked, the markers not drawn (clean pictures)
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

/* Lane PRTPPLACE (2026-09-30): the probe marker kinds. ONE table: the markers draw
 * with it and the Cell workspace's PRTP band paints its swatches from it. */
static const CellProbeKind g_probeKinds[] = {
	{ "First hit", { 0.2f, 0.85f, 0.25f } },
	{ "Interior",  { 1.0f, 0.55f, 0.1f } },
	{ "Wall",      { 0.2f, 0.45f, 1.0f } },
	{ "Doorway",   { 1.0f, 0.2f, 0.9f } },
	{ "Window",    { 0.1f, 0.9f, 0.95f } },
	{ "Breach",    { 1.0f, 0.1f, 0.1f } },
	{ "Room",      { 1.0f, 0.95f, 0.1f } },
	{ "Cover",     { 0.95f, 0.95f, 0.95f } }
};

const CellProbeKind * cellProbeKinds( int * count )
{
	if ( count )
		*count = int( sizeof( g_probeKinds ) / sizeof( g_probeKinds[0] ) );
	return g_probeKinds;
}

/* lane SKYINT1: an interior's CELL DATA flags, bit 7 Show Sky, 8 Use Sky Lighting, 11 Sunlight Shadows. The
 * gate's refuters (WW_CELL_SKYINT_RED): "noflag" every interior is read as closed, "all" every one as Show Sky. */
static quint16 cellInteriorFlags( const EsmInteriorCell & ic )
{
	const QByteArray red = qgetenv( "WW_CELL_SKYINT_RED" ).trimmed();
	if ( red == "noflag" )
		return quint16( ic.flags & ~0x0980u );
	if ( red == "all" )
		return quint16( ic.flags | 0x0080u );
	return ic.flags;
}

static int cellProbeKindOf( const ProbePoint & q )
{
	switch ( q.cls ) {
	case ProbeClass::FirstHit: return 0;
	case ProbeClass::Interior: return 1;
	case ProbeClass::Wall:     return 2;
	case ProbeClass::Room:     return 6;
	case ProbeClass::Cover:    return 7;
	default:
		return q.kind == ApertureKind::Doorway ? 3 : q.kind == ApertureKind::Window ? 4 : 5;
	}
}

/* LANE PRTP3 -- THE CELL'S LIGHTING, for the renderer (src/gl/celllights.h).
 *
 * Every placed light that is on at load: not initially disabled, not "Off By Default" (0x20).
 *   radius   = the base's DATA radius + the ref's XRDS. XRDS is a DELTA, measured: 2,080 of the
 *              3,853 XRDS in the PRTP1 gate's ten interiors are negative, and as base + XRDS only 9
 *              reach zero or below (those 9 draw nothing); as an absolute radius over half the
 *              lights in the game would have none.
 *   color    = pow(byte / 255, 2.2) x (FNAM fade + XLIG fade delta)          PRTP2 sections 0, 1
 *   curve    = DATA Constant / Scalar / Exponent, (0, 1, 2) when DATA is short
 *   spot     = flags 0x400 / 0x4000: FOV (+ XLIG FOV delta), Falloff Exponent the edge, aimed along
 *              the ref's local +X (measured, celllights.h)
 * An interior adds its ambient and directional light: each field from XCLL, or from the lighting
 * template (LTMP -> LGTM) when the cell has no XCLL or its Inherits flag names that field.
 *   DALC     = XCLL Ambient Colors (offset 40) / the LGTM's DALC; the flat Ambient Color on all six
 *              axes when neither is there
 *   direct.  = Directional Color x Directional Fade; its direction from Rotation XY (elevation) and
 *              Z (azimuth from +Y, clockwise), degrees -- ASSUMED, the PRTP4 capture is the refuter */
static void cellPublishLighting( const NifModel * nif, const EsmWorld & world, const CellSceneSpec & spec,
	const QVector<EsmRefr> & lightRefs, const QHash<quint32, EsmRefr> & primRefs, const Vector3 & center )
{
	WwCellLighting L;
	L.interior = spec.interior;
	L.showSky = spec.interior && ( cellInteriorFlags( world.interior() ) & 0x0080u );	// lane SKYFULL1
	for ( int k = 0; k < 3; k++ )
		L.center[k] = center[k];
	const bool axisRed = ( wwCellLightsRed() & 2 ) != 0;
	const bool shapeRed = ( wwCellLightsRed() & 256 ) != 0;	// lane HEMI1: WW_CELL_LIT_RED=hemiomni
	int omni = 0, spot = 0, off = 0, noRadius = 0, dark = 0, ambientOnly = 0, hemi = 0, box = 0, boxLost = 0;
	int ambientBox = 0;
	/* lane HEMI1: the box of primitive ref p for a light of scale `scale` (celllights.h): rotation
	 * Rz(-z) Rx(-x) Ry(-y) of the primitive's own angles, axis k = column k, half extent k = |XPRM bound k| x scale. */
	auto boxRows = []( const EsmRefr & p, float scale, float out[3][4] ) {
		auto rot = []( int ax, float a, double m[3][3] ) {
			const double c = std::cos( a ), s = std::sin( a );
			const int i = ( ax + 1 ) % 3, j = ( ax + 2 ) % 3;
			for ( int u = 0; u < 3; u++ )
				for ( int v = 0; v < 3; v++ )
					m[u][v] = u == v ? 1.0 : 0.0;
			m[i][i] = c; m[i][j] = -s; m[j][i] = s; m[j][j] = c;
		};
		auto mul = []( const double a[3][3], const double b2[3][3], double o[3][3] ) {
			for ( int u = 0; u < 3; u++ )
				for ( int v = 0; v < 3; v++ )
					o[u][v] = a[u][0] * b2[0][v] + a[u][1] * b2[1][v] + a[u][2] * b2[2][v];
		};
		double rz[3][3], rx[3][3], ry[3][3], t[3][3], m[3][3];
		rot( 2, -p.rot[2], rz );
		rot( 0, -p.rot[0], rx );
		rot( 1, -p.rot[1], ry );
		mul( rz, rx, t );
		mul( t, ry, m );
		for ( int k = 0; k < 3; k++ ) {
			const double h = std::max( double( std::abs( p.primHalf[k] ) ) * double( scale ), 1e-3 );
			double w = 0.0;
			for ( int c = 0; c < 3; c++ ) {
				out[k][c] = float( m[c][k] / h );
				w -= m[c][k] / h * double( p.pos[c] );
			}
			out[k][3] = float( w );
		}
	};
	for ( const EsmRefr & r : lightRefs ) {
		const EsmLight & b = world.light( r.base );
		if ( !b.exists )
			continue;
		if ( r.initiallyDisabled || ( b.flags & 0x20 ) ) {
			off++;
			continue;
		}
		// lane AMBO1: an Ambient Only light (0x100000) adds no light of its own in game; it scales the cell's
		// ambient where it applies. Lane AMBO2: inside a sphere of 1.22077 x its radius, each channel of the
		// ambient's affine sum x pow(byte / 255, 2.2) x fade (celllights.h). A black one still darkens.
		if ( ( b.flags & 0x100000 ) && !( wwCellLightsRed() & 128 ) ) {
			ambientOnly++;
			const float radius = float( b.radius ) + ( r.hasRadius ? r.radius : 0.0f );
			if ( radius > 0.0f ) {
				WwCellAmbientLight a;
				const float fade = b.fade + ( r.xligCount >= 2 ? r.xlig[1] : 0.0f );
				for ( int k = 0; k < 3; k++ ) {
					a.pos[k] = r.pos[k];
					a.k[k] = std::pow( float( b.color[k] ) / 255.0f, 2.2f ) * fade;
				}
				a.volume = 1.22077f * radius;
				// lane HEMI1: linked to a box, it fills the box instead (WW_CELL_LIT_RED=hemiomni: the sphere again)
				if ( r.lightBox && !( b.flags & ( 0x800 | 0x400 | 0x4000 ) ) ) {
					const auto it = primRefs.constFind( r.lightBox );
					if ( it == primRefs.cend() ) {
						boxLost++;
					} else {
						ambientBox++;
						a.hasBox = !shapeRed;
						boxRows( it.value(), r.scale, a.box );
					}
				}
				L.ambientLights.append( a );
			}
			continue;
		}
		WwCellLight l;
		l.radius = float( b.radius ) + ( r.hasRadius ? r.radius : 0.0f );
		if ( !( l.radius > 0.0f ) ) {
			noRadius++;
			continue;
		}
		const float fade = b.fade + ( r.xligCount >= 2 ? r.xlig[1] : 0.0f );
		for ( int c = 0; c < 3; c++ )
			l.color[c] = std::pow( float( b.color[c] ) / 255.0f, 2.2f ) * fade;
		if ( l.color[0] <= 0.0f && l.color[1] <= 0.0f && l.color[2] <= 0.0f ) {
			dark++;
			continue;
		}
		for ( int k = 0; k < 3; k++ )
			l.pos[k] = r.pos[k];
		if ( b.hasAttenuation ) {
			l.bias = b.constant;
			l.scale = b.scalar;
			l.exponent = b.exponent;
		}
		l.noSpecular = ( b.flags & 0x8000 ) != 0;
		l.noRim = ( b.flags & 0x80000 ) != 0;
		l.ignoreRoughness = ( b.flags & 0x40000 ) != 0;
		l.spot = !( b.flags & 0x800 ) && ( b.flags & ( 0x400 | 0x4000 ) ) != 0;	// lane HEMI1: the hemisphere flag wins
		// lane SHADOW1: the shadow kind, near clip (DATA + XLIG delta) and XLIG Shadow Depth Bias
		l.shadow = ( b.flags & 0x400 ) ? 1 : ( b.flags & 0x800 ) ? 2 : ( b.flags & 0x1000 ) ? 3 : 0;
		l.nearClip = std::max( b.nearClip + ( r.xligCount >= 5 ? r.xlig[4] : 0.0f ), 0.0f );
		l.shadowBias = r.xligCount >= 4 ? r.xlig[3] : 0.0f;
		if ( l.spot || ( b.flags & 0x800 ) ) {
			Matrix rm;
			rm.fromEuler( -r.rot[0], -r.rot[1], -r.rot[2] );
			const Vector3 d = rm * ( axisRed ? Vector3( 0, 0, -1 ) : Vector3( 1, 0, 0 ) );
			for ( int k = 0; k < 3; k++ )
				l.dir[k] = d[k];   // a hemisphere faces its local +X too: 14 of the 17 placed aim it down (measured)
		}
		if ( l.spot ) {
			const float fov = b.fov + ( r.xligCount >= 1 ? r.xlig[0] : 0.0f );
			l.cosOuter = std::cos( fov * 0.5f * 3.14159265f / 180.0f );
			l.cone = b.falloff;
			Matrix rm;
			rm.fromEuler( -r.rot[0], -r.rot[1], -r.rot[2] );
			const Vector3 d = rm * ( axisRed ? Vector3( 0, 0, -1 ) : Vector3( 1, 0, 0 ) );
			for ( int k = 0; k < 3; k++ )
				l.dir[k] = d[k];
			spot++;
		} else {
			omni++;
		}
		/* lane HEMI1: the shape (celllights.h). A hemisphere is clipped to its +X half space (dir above); a
		 * light linked by LightBoxLink to a primitive ref to that ref's box: rotation Rz(-z) Rx(-x) Ry(-y) of
		 * the primitive's own angles, axis k = column k, half extent k = |XPRM bound k| x the light's scale. */
		if ( b.flags & 0x800 ) {
			hemi++;
			l.shape = shapeRed ? 0 : 1;
		} else if ( !l.spot && r.lightBox ) {
			const auto it = primRefs.constFind( r.lightBox );
			if ( it == primRefs.cend() ) {
				boxLost++;
			} else {
				box++;
				boxRows( it.value(), r.scale, l.box );
				l.shape = shapeRed ? 0 : 2;
			}
		}
		L.lights.append( l );
		wwCellFxLitNoOffset( nif, L.lights.size() - 1, fade > 0.0f ? b.fade / fade : 1.0f );   // lane FXLIT1: a red control's data
	}
	QString amb = QStringLiteral( "none (exterior: the viewport light)" ), dir = QStringLiteral( "none" );
	if ( spec.interior ) {
		const EsmInteriorCell & ic = world.interior();
		QByteArray tData, tDalc;
		const bool haveT = ic.lightingTemplate && world.lightingTemplate( ic.lightingTemplate, tData, tDalc );
		const QByteArray & x = ic.xcll;
		// a field comes from the template when the cell has no XCLL or inherits it
		auto fromT = [&]( quint32 flag ) { return haveT && ( x.isEmpty() || ( ic.inherits & flag ) ); };
		auto u8 = [&]( const QByteArray & a, int o ) { return quint8( a.at( o ) ); };
		auto f32 = [&]( const QByteArray & a, int o ) { float v; std::memcpy( &v, a.constData() + o, 4 ); return v; };
		auto s32 = [&]( const QByteArray & a, int o ) { qint32 v; std::memcpy( &v, a.constData() + o, 4 ); return v; };
		// ambient (Inherits 0x1)
		const QByteArray & aSrc = fromT( 0x1 ) ? tData : x;
		const bool aT = fromT( 0x1 );
		if ( aT && tDalc.size() >= 24 ) {
			for ( int a = 0; a < 6; a++ )
				for ( int c = 0; c < 3; c++ )
					L.dalc[a][c] = u8( tDalc, a * 4 + c ) / 255.0f;
			L.hasDalc = true;
			amb = QStringLiteral( "DALC from the template" );
		} else if ( !aT && x.size() >= 64 ) {
			for ( int a = 0; a < 6; a++ )
				for ( int c = 0; c < 3; c++ )
					L.dalc[a][c] = u8( x, 40 + a * 4 + c ) / 255.0f;
			L.hasDalc = true;
			amb = QStringLiteral( "DALC from XCLL" );
		} else if ( aSrc.size() >= 4 ) {
			for ( int a = 0; a < 6; a++ )
				for ( int c = 0; c < 3; c++ )
					L.dalc[a][c] = u8( aSrc, c ) / 255.0f;
			L.hasDalc = true;
			amb = QStringLiteral( "flat Ambient Color from %1" ).arg( aT ? "the template" : "XCLL" );
		} else {
			amb = QStringLiteral( "none (no XCLL, no template)" );
		}
		// directional (Inherits 0x2 colour, 0x20 rotation, 0x40 fade)
		const QByteArray & cSrc = fromT( 0x2 ) ? tData : x;
		const QByteArray & rSrc = fromT( 0x20 ) ? tData : x;
		const QByteArray & fSrc = fromT( 0x40 ) ? tData : x;
		if ( cSrc.size() >= 8 && rSrc.size() >= 28 ) {
			const float dfade = fSrc.size() >= 32 ? f32( fSrc, 28 ) : 1.0f;
			for ( int c = 0; c < 3; c++ )
				L.dirColor[c] = std::pow( u8( cSrc, 4 + c ) / 255.0f, 2.2f ) * dfade;
			const float el = float( s32( rSrc, 20 ) ) * 3.14159265f / 180.0f;
			const float az = float( s32( rSrc, 24 ) ) * 3.14159265f / 180.0f;
			L.dirTo[0] = std::cos( el ) * std::sin( az );
			L.dirTo[1] = std::cos( el ) * std::cos( az );
			L.dirTo[2] = std::sin( el );
			L.hasDirectional = L.dirColor[0] > 0.0f || L.dirColor[1] > 0.0f || L.dirColor[2] > 0.0f;
			dir = L.hasDirectional
				? QStringLiteral( "%1,%2,%3 x%4 rot %5,%6" ).arg( u8( cSrc, 4 ) ).arg( u8( cSrc, 5 ) ).arg( u8( cSrc, 6 ) )
					.arg( dfade, 0, 'f', 2 ).arg( s32( rSrc, 20 ) ).arg( s32( rSrc, 24 ) )
				: QStringLiteral( "black" );
		}
		/* lane FOG2: the fog (celllights.h WwCellLighting::hasFog). Pin WW_CELL_FOG=0 publishes none;
		 * WW_CELL_FOG_RED=noclamp|nogamma|noinherit are the gate's refuters. */
		{	// lane SSR1: the clip distance (the reflections' far plane, src/gl/cellssr.h)
			const QByteArray & clS = fromT( 0x80 ) ? tData : x;
			L.clipDist = clS.size() >= 36 ? f32( clS, 32 ) : 0.0f;
		}
		const QByteArray fogPin = qgetenv( "WW_CELL_FOG" ).trimmed();
		const QByteArray fogRed = qgetenv( "WW_CELL_FOG_RED" ).trimmed();
		auto fT = [&]( quint32 flag ) { return fogRed == "noinherit" ? ( haveT && x.isEmpty() ) : fromT( flag ); };
		const QByteArray & cS = fT( 0x4 ) ? tData : x;
		const QByteArray & nS = fT( 0x8 ) ? tData : x;
		const QByteArray & farS = fT( 0x10 ) ? tData : x;
		const QByteArray & pS = fT( 0x100 ) ? tData : x;
		const QByteArray & mS = fT( 0x200 ) ? tData : x;
		if ( fogPin == "0" ) {
			L.fogNote = QStringLiteral( "off (WW_CELL_FOG=0)" );
		} else if ( nS.size() < 20 || farS.size() < 20 ) {
			L.fogNote = QStringLiteral( "none (no XCLL, no template)" );
		} else {
			auto fOr = [&]( const QByteArray & a, int o, float d ) { return a.size() >= o + 4 ? f32( a, o ) : d; };
			WwFog F;
			F.fogFar = f32( farS, 16 );
			F.fogNear = f32( nS, 12 );
			if ( fogRed != "noclamp" ) {
				if ( !( F.fogFar > 0.0f ) || F.fogFar > 163840.0f )
					F.fogFar = 163840.0f;
				if ( !( F.fogNear > 0.0f ) || F.fogNear > F.fogFar )
					F.fogNear = F.fogFar * 0.17f;
			}
			F.power = fOr( pS, 36, 1.0f );
			F.maxv = fOr( mS, 76, 1.0f );
			F.nMid = fOr( cS, 92, 0.0f );
			F.nRange = fOr( cS, 96, 10000.0f );
			F.hds = fOr( cS, 108, 1.0f );
			F.fMid = fOr( cS, 128, 0.0f );
			F.fRange = fOr( cS, 132, 10000.0f );
			// near 8 x 112, far 72 x 116, high near 100 x 120, high far 104 x 124
			const int at[4] = { 8, 72, 100, 104 };
			float * dst[4] = { F.nearLow, F.farLow, F.nearHigh, F.farHigh };
			for ( int k = 0; k < 4; k++ ) {
				F.scale[k] = fOr( cS, 112 + 4 * k, 1.0f );
				for ( int c = 0; c < 3; c++ ) {
					const float v = std::max( 0.0f, ( cS.size() >= at[k] + 3 ? u8( cS, at[k] + c ) : 0 ) / 255.0f * F.scale[k] );
					dst[k][c] = fogRed == "nogamma" ? v : std::pow( v, 2.2f );
				}
			}
			wwFogPackK( F );
			std::memcpy( L.fogK, F.K, sizeof( F.K ) );
			L.hasFog = true;
			auto src = [&]( const QByteArray & a ) { return QChar( &a == &tData ? 'T' : 'X' ); };
			L.fogNote = QStringLiteral( "near=%1 far=%2 power=%3 max=%4 hds=%5 nmid=%6 nrange=%7 fmid=%8 frange=%9" )
				.arg( double( F.fogNear ), 0, 'f', 1 ).arg( double( F.fogFar ), 0, 'f', 1 ).arg( double( F.power ), 0, 'f', 4 )
				.arg( double( F.maxv ), 0, 'f', 4 ).arg( double( F.hds ), 0, 'f', 4 ).arg( double( F.nMid ), 0, 'f', 1 )
				.arg( double( F.nRange ), 0, 'f', 1 ).arg( double( F.fMid ), 0, 'f', 1 ).arg( double( F.fRange ), 0, 'f', 1 )
				+ QStringLiteral( " scale=%1,%2,%3,%4 src=%5%6%7%8%9 xcll=%10%11" )
				.arg( double( F.scale[0] ) ).arg( double( F.scale[1] ) ).arg( double( F.scale[2] ) ).arg( double( F.scale[3] ) )
				.arg( src( cS ) ).arg( src( nS ) ).arg( src( farS ) ).arg( src( pS ) ).arg( src( mS ) ).arg( x.size() )
				.arg( fogRed.isEmpty() ? QString() : QStringLiteral( " red=" ) + QString::fromLatin1( fogRed ) );
		}
	}
	/* lane IMGS1: the cell's imagespace (XCIM -> IMGS) and its LUT strip. The strip is a 256x16 B8G8R8
	 * DDS: x = r + 16 b, y = g (MEASURED: ColorLUT_BaseInteriorAdjusted sits 19/255 off the identity in
	 * that order, 63 with g flipped, 71 with r and b swapped); the shaders sample it as a 16^3 3D LUT. */
	QString isNote = QStringLiteral( "none" );
	if ( spec.interior && world.interior().imageSpace ) {
		QByteArray h, c, t;
		QString edid, lut;
		if ( world.imageSpace( world.interior().imageSpace, edid, h, c, t, lut ) && h.size() >= 36 ) {
			L.hasImageSpace = true;
			L.isName = edid;
			std::memcpy( L.isHdr, h.constData(), 36 );
			if ( c.size() >= 12 )
				std::memcpy( L.isCine, c.constData(), 12 );
			if ( t.size() >= 16 )
				std::memcpy( L.isTint, t.constData(), 16 );
			L.isLutPath = lut;
			QString lutNote = QStringLiteral( "no LUT" );
			if ( !lut.isEmpty() ) {
				QByteArray dds;
				bool got = Game::GameManager::get_file( dds, Game::FALLOUT_4, lut, "textures", ".dds" );
				if ( !got && !spec.dataRoot.isEmpty() ) {
					QFile lf( spec.dataRoot + QStringLiteral( "/Textures/" ) + QString( lut ).replace( QLatin1Char( '\\' ), QLatin1Char( '/' ) ) );
					got = lf.open( QIODevice::ReadOnly ) && !( dds = lf.readAll() ).isEmpty();
				}
				auto u32 = [&]( int o ) { quint32 v = 0; std::memcpy( &v, dds.constData() + o, 4 ); return v; };
				// DDS: height 12, width 16, pixel format flags 80 (0x40 RGB), bit count 88, R mask 92
				if ( got && dds.size() >= 128 + 256 * 16 * 3 && dds.startsWith( "DDS " ) && u32( 12 ) == 16 && u32( 16 ) == 256
					&& ( u32( 80 ) & 0x40 ) && u32( 88 ) == 24 && u32( 92 ) == 0xff0000 ) {
					const unsigned char * px = reinterpret_cast<const unsigned char *>( dds.constData() ) + 128;
					L.isLut.resize( 16 * 16 * 16 * 3 );
					for ( int b = 0; b < 16; b++ )
						for ( int g = 0; g < 16; g++ )
							for ( int r = 0; r < 16; r++ ) {
								const unsigned char * q = px + ( g * 256 + b * 16 + r ) * 3;
								unsigned char * o = &L.isLut[size_t( ( ( b * 16 + g ) * 16 + r ) * 3 )];
								o[0] = q[2];
								o[1] = q[1];
								o[2] = q[0];
							}
					lutNote = QStringLiteral( "LUT %1" ).arg( lut );
				} else {
					lutNote = got ? QStringLiteral( "LUT %1 UNREAD (not a 256x16 B8G8R8 strip)" ).arg( lut )
						: QStringLiteral( "LUT %1 NOT FOUND" ).arg( lut );
				}
			}
			isNote = QStringLiteral( "%1 hdr %2 cine %3,%4,%5 tint %6 %7" ).arg( edid )
				.arg( [&] { QStringList v; for ( float f : L.isHdr ) v << QString::number( double( f ), 'g', 4 ); return v.join( ',' ); }() )
				.arg( double( L.isCine[0] ) ).arg( double( L.isCine[1] ) ).arg( double( L.isCine[2] ) )
				.arg( double( L.isTint[0] ) ).arg( lutNote );
		} else {
			isNote = QStringLiteral( "XCIM %1 is not a readable IMGS" ).arg( world.interior().imageSpace, 8, 16, QLatin1Char( '0' ) );
		}
	}
	L.summary = QStringLiteral( "lights=%1 (omni %2, spot %3; skipped: off %4, no radius %5, black %6) ambient=%7 directional=%8 center=%9" )
		.arg( L.lights.size() ).arg( omni ).arg( spot ).arg( off ).arg( noRadius ).arg( dark ).arg( amb, dir )
		.arg( QStringLiteral( "%1,%2,%3" ).arg( center[0], 0, 'f', 1 ).arg( center[1], 0, 'f', 1 ).arg( center[2], 0, 'f', 1 ) )
		+ QStringLiteral( " norim=%1 ignorerough=%2" )	// lane RIM1: the lights whose shader drops the back-light
			.arg( std::count_if( L.lights.cbegin(), L.lights.cend(), []( const WwCellLight & l ) { return l.noRim; } ) )
			.arg( std::count_if( L.lights.cbegin(), L.lights.cend(), []( const WwCellLight & l ) { return l.ignoreRoughness; } ) )
		+ QStringLiteral( " ambientonly=%1" ).arg( ambientOnly )	// lane AMBO1: skipped, no direct light in game
		+ QStringLiteral( " ambientvolumes=%1" ).arg( L.ambientLights.size() )	// lane AMBO2: they scale the ambient
		+ QStringLiteral( " shapes=hemisphere %1 box %2 (box link unresolved %3) ambientboxes=%4" ).arg( hemi ).arg( box )
			.arg( boxLost ).arg( ambientBox )	// lane HEMI1
		+ QStringLiteral( " imagespace=%1" ).arg( isNote )
		+ QStringLiteral( " fog=%1" ).arg( L.fogNote.isEmpty() ? QStringLiteral( "none (exterior: the Lookdev weather fog)" ) : L.fogNote );
	wwCellLightsPublish( nif, L );
}

/* lane BAKEBLOCK1: the worldspace's far LOD (`FO4CSLOD/<ws>/<ws>.lodl`, its `.lodi` beside it), looked for in
 * WW_CELL_FAR_LOD (a folder or the .lodl), the data root, the LOD panel's resources and output folder, then
 * every mod of the Mod Organizer mods folder the panel uses. `searched` names where it looked. */
static QString cellFarLodl( const QString & ws, const QString & dataRoot, QStringList & searched )
{
	const QString rel = QStringLiteral( "FO4CSLOD/%1/%1.lodl" ).arg( ws );
	auto hit = [&]( const QString & root, const QString & r ) {
		searched << root;
		const QString p = QDir( root ).filePath( r );
		return QFileInfo( p ).isFile() ? QDir::cleanPath( p ) : QString();
	};
	const QString env = qEnvironmentVariable( "WW_CELL_FAR_LOD" );
	if ( !env.isEmpty() ) {
		if ( QFileInfo( env ).isFile() )
			return QDir::cleanPath( env );
		for ( const QString & r : { QStringLiteral( "%1.lodl" ).arg( ws ), rel } ) {
			const QString p = hit( env, r );
			if ( !p.isEmpty() )
				return p;
		}
	}
	QSettings cfg;
	QStringList roots { dataRoot };
	roots << lodgenResources() << cfg.value( QStringLiteral( "LodGeneration/output" ) ).toString();
	for ( const QString & r : roots ) {
		const QString p = r.isEmpty() ? QString() : hit( r, rel );
		if ( !p.isEmpty() )
			return p;
	}
	QString mods = cfg.value( QStringLiteral( "LodGeneration/mo2Mods" ) ).toString();
	if ( mods.isEmpty() )   // the LOD panel's own default
		mods = QStringLiteral( "E:/Projects/Fallout 4 Mods/mods" );
	QStringList names = QDir( mods ).entryList( QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name );
	if ( names.removeAll( QStringLiteral( "FO4CSLOD" ) ) )
		names.prepend( QStringLiteral( "FO4CSLOD" ) );
	for ( const QString & m : names ) {
		const QString p = QFileInfo( QDir( mods ).filePath( m + QLatin1Char( '/' ) + rel ) ).isFile()
			? QDir::cleanPath( QDir( mods ).filePath( m + QLatin1Char( '/' ) + rel ) ) : QString();
		if ( !p.isEmpty() )
			return p;
	}
	searched << mods + QStringLiteral( "/*" );
	return QString();
}

/* lane BAKEBLOCK1: append the far soup around the bake's block; the census line it returns says what went in */
static QString cellBakeFarSoup( const CellSceneSpec & spec, const QString & dataRoot, const QByteArray & red,
	ProbeSoup & soup )
{
	if ( !red.isEmpty() )
		return QStringLiteral( "far soup: none (RED %1)" ).arg( QString::fromLatin1( red ) );
	QElapsedTimer t;
	t.start();
	QStringList searched;
	const QString lodl = cellFarLodl( spec.world, dataRoot, searched );
	if ( lodl.isEmpty() ) {
		const QString why = QStringLiteral( "far soup: NONE -- no FO4CSLOD/%1/%1.lodl in %2" )
			.arg( spec.world, searched.join( QLatin1String( "; " ) ) );
		qWarning() << qPrintable( why );
		return why;
	}
	const QFileInfo li( lodl );
	const QString lodi = li.absoluteDir().filePath( li.completeBaseName() + QStringLiteral( ".lodi" ) );
	const int radius = int( std::ceil( ProbeBakeSpec().rayMax / CELL_UNITS ) );
	const qint64 before = soup.triCount();
	ProbeFarResult r;
	if ( !probeFarAppendRing( lodl, QFileInfo( lodi ).isFile() ? lodi : QString(), spec.cx, spec.cy, ( spec.n - 1 ) / 2,
			radius, soup, &r ) ) {
		qWarning() << "far soup:" << r.error;
		return QStringLiteral( "far soup: REFUSED -- %1" ).arg( r.error );
	}
	return QStringLiteral( "far soup %1 out to %2 cells: ground quads %3 (colour %4), water quads %5, boxes %6, "
		"trees %7, triangles +%8, %9 ms" ).arg( QDir::toNativeSeparators( lodl ) ).arg( radius ).arg( r.quads )
		.arg( r.albedoQuads ).arg( r.waterQuads ).arg( r.boxes ).arg( r.trees ).arg( soup.triCount() - before )
		.arg( t.elapsed() );
}

bool nifCreateCellScene( NifModel * nif, const CellSceneSpec & specAsked,
	QString * error, QString * notes )
{
	/* lane BAKEBLOCK1 (bungo, PRTP_PLAN item 5): an exterior BAKE loads the game's own 5x5 block around the
	 * cell at full detail (uGridsToLoad), and the far soup from the LOD files beyond it (below); the probes
	 * stay the asked block's. WW_CELL_BAKEBLOCK_RED=n1 (the asked block, no far) | nolod (5x5, no far):
	 * the gate's red controls only. A relight of a bake on disk (WW_CELL_GI_FROM) traces its shadow, sun and
	 * feed rays against the soup too, so it loads the same block + far the bake did. */
	CellSceneSpec spec = specAsked;
	const QByteArray blockRed = qgetenv( "WW_CELL_BAKEBLOCK_RED" );
	const bool bakeExterior = !spec.interior && ( !qEnvironmentVariableIsEmpty( "WW_CELL_PROBES" ) || spec.probes )
		&& ( spec.probesBake || !qEnvironmentVariableIsEmpty( "WW_CELL_PROBE_BAKE" )
			|| !qEnvironmentVariableIsEmpty( "WW_CELL_GI_FROM" ) );
	if ( bakeExterior && blockRed != "n1" )
		spec.n = qMax( spec.n, 5 );
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
	CellSpeed::begin();   // lane SPEED1
	cellMeshClear( nif );

	// ---- the worldspace, or (lane PRTP1) one interior cell
	EsmWorld world;
	QString loadError;
	if ( spec.interior ) {
		if ( !world.loadInterior( spec.plugins, spec.interiorCell, &loadError ) )
			return fail( loadError.isEmpty()
				? QStringLiteral( "could not open interior %1" ).arg( spec.interiorCell ) : loadError );
	} else {
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

		if ( !world.load( spec.plugins, wsForm, &loadError ) )
			return fail( loadError.isEmpty()
				? QStringLiteral( "could not index %1" ).arg( spec.world ) : loadError );
	}

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
	CellSpeed::mark( "plugin index" );

	// ---- the placements
	struct Placement
	{
		quint32 base = 0;
		Vector3 pos;
		Matrix rot;
		float scale = 1.0f;
		quint32 ref = 0;
		int part = -1;
		quint32 swap = 0;   //!< lane PRTPPLACE: the MSWP this placement draws with (ref XMSP, else the base's MODS)
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
		QString actorKey;   //!< lane PLACED1: a placed actor (src/cellactor.h); its shapes come from there
		bool loadDoor = false;   //!< lane ROOMCLAMP1: a door with a teleport (XTEL)
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

	/* lane PRTP1: the cell's LIGHTS. Every placed LIGH that is not deleted, whether
	 * or not it has a model and whether or not it starts disabled -- the light
	 * list is what the cell CONTAINS, as the ref table is. WW_CELL_LIGHTS dumps it
	 * and the gate compares it with an independent walk of the plugin. */
	QVector<EsmRefr> lightRefs;
	std::vector<CellDecalRef> decalRefs;   // lane PLACED1: placed decals, projected after the weld
	CellActors actors( world, dataRoot );  // lane PLACED1: placed actors, built at rest
	int actorPlacements = 0;
	QHash<quint32, EsmRefr> primRefs;	// lane HEMI1: the refs carrying a primitive (a light's LightBoxLink target)
	/* lane MISS1: a reference with an enable parent starts in the PARENT's state, inverted when it is
	 * "opposite"; the parent's own starting state is a plugin fact (its initially-disabled flag, or its own
	 * parent in turn). A parent outside the loaded block counts as enabled. WW_CELL_REFS_RED=parent is the
	 * gate's red control: the old rule, which never looked at the parent. */
	struct EnableFact { bool off; quint32 parent; bool opposite; };
	QHash<quint32, EnableFact> enableFacts;
	const QByteArray refsRed = qgetenv( "WW_CELL_REFS_RED" );
	auto startsDisabled = [&]( const EsmRefr & r ) {
		bool off = r.initiallyDisabled;
#ifdef ESM_HAS_CELL_FIELDS
		if ( refsRed == "parent" )
			return r.enableParent && r.enableParentOpposite ? !off : off;
		bool flip = false, opposite = r.enableParentOpposite;
		quint32 up = r.enableParent;
		for ( int hop = 0; up && hop < 16; hop++ ) {
			flip ^= opposite;
			const auto it = enableFacts.constFind( up );
			if ( it == enableFacts.constEnd() ) {
				off = false;
				break;
			}
			off = it->off;
			up = it->parent;
			opposite = it->opposite;
		}
		if ( r.enableParent )
			off = off != flip;
#endif
		return off;
	};
	auto pushRefr = [&]( const EsmRefr & r, int cellX, int cellY, bool persistent ) {
		refsRead++;
		if ( !r.deleted && std::memcmp( &r.baseType, "LIGH", 4 ) == 0 )
			lightRefs.append( r );
		if ( !r.deleted && r.hasPrim )
			primRefs.insert( r.formID, r );

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
		// lane MISS1: the state the plugin starts the reference in (startsDisabled above); what a quest does
		// to an enable marker later is a runtime fact and is not simulated. The panel shows both facts.
		const bool disabled = startsDisabled( r );
		if ( disabled && !spec.showDisabled ) {
			refsHidden++;
			cellRefTableMutable().setFate( refRow, CellRefFate::Disabled );
			return;
		}
		const EsmLodBase & lb = world.lodBase( r.base );
		const bool isScol = std::memcmp( &r.baseType, "SCOL", 4 ) == 0;
		if ( std::memcmp( &r.baseType, "TXST", 4 ) == 0 ) {   // lane PLACED1: a decal; its fate is set after the weld
			CellDecalRef d;
			d.form = r.formID;
			d.base = r.base;
			for ( int k = 0; k < 3; k++ ) {
				d.pos[k] = r.pos[k];
				d.rot[k] = r.rot[k];
			}
			d.refRow = refRow;
			decalRefs.push_back( d );
			return;
		}
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
					out.swap = r.materialSwap ? r.materialSwap : world.lodBase( part.base ).materialSwap;
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
		out.swap = r.materialSwap ? r.materialSwap : lb.materialSwap;
		out.loadDoor = r.teleport != 0;
		placements.append( out );
	};

	const QVector<EsmRefr> actorRefs = spec.interior ? actors.references() : QVector<EsmRefr>();   // lane PLACED1
	// lane MISS1: every reference's enable facts, before the first one is placed (a parent may come later)
	{
		auto note = [&]( const QVector<EsmRefr> & refs ) {
#ifdef ESM_HAS_CELL_FIELDS
			for ( const EsmRefr & r : refs )
				enableFacts.insert( r.formID, EnableFact{ r.initiallyDisabled, r.enableParent, r.enableParentOpposite } );
#else
			Q_UNUSED( refs );
#endif
		};
		if ( spec.interior ) {
			note( world.interiorRefrs() );
			note( actorRefs );   // lane PLACED1: a placed actor has a start state too, and may be a parent
		} else {
			for ( int y = y0; y <= y1; y++ )
				for ( int x = x0; x <= x1; x++ )
					if ( world.hasCell( x, y ) )
						note( world.refrs( x, y ) );
			note( world.persistentRefrsIn( float( x0 ) * CELL_UNITS, float( y0 ) * CELL_UNITS,
				float( x1 + 1 ) * CELL_UNITS, float( y1 + 1 ) * CELL_UNITS ) );
		}
	}

	int cellsWithData = 0;
	if ( spec.interior ) {
		// lane PRTP1: one interior cell, persistent and temporary refs together
		cellsWithData = 1;
		CellBlockEntry block;
		block.cx = 0;
		block.cy = 0;
		block.cellForm = world.interior().cellForm;
		block.edid = world.interior().edid;
		cellRefTableMutable().addCell( block );
		for ( const EsmRefr & r : world.interiorRefrs() )
			pushRefr( r, 0, 0, false );
		/* lane PLACED1: the placed actors. They are not REFRs: they stay out of the reference
		 * list, the REFR counts and the placement dump, and have their own census line. */
		for ( const EsmRefr & r : actorRefs ) {
			Placement out;
			if ( !actors.place( r, startsDisabled( r ) && !spec.showDisabled, out.actorKey, out.scale ) )
				continue;
			out.base = r.base;
			out.pos = Vector3( r.pos[0], r.pos[1], r.pos[2] );
			out.rot.fromEuler( -r.rot[0], -r.rot[1], -r.rot[2] );
			out.ref = r.formID;
			for ( int k = 0; k < 3; k++ )
				out.storedRot[k] = r.rot[k];
			placements.append( out );
			actorPlacements++;
		}
	} else {
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

	Vector3 origin( float( spec.cx ) * CELL_UNITS + CELL_UNITS * 0.5f,
		float( spec.cy ) * CELL_UNITS + CELL_UNITS * 0.5f, 0.0f );
	if ( spec.interior && placements.size() > actorPlacements ) {
		// lane PRTP1: an interior has no grid; centre the welded scene on its placements
		double sx = 0, sy = 0;
		for ( const Placement & p : placements ) {
			if ( !p.actorKey.isEmpty() )
				continue;   // lane PLACED1: the centre stays where the REFRs put it
			sx += p.pos[0];
			sy += p.pos[1];
		}
		const int n = int( placements.size() ) - actorPlacements;
		origin = Vector3( float( sx / n ), float( sy / n ), 0.0f );
	}

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
	int blendBuckets = 0;                   //!< 2026-10-01: blended (glass) buckets
	int effectBuckets = 0;                  //!< 2026-10-01: BGEM buckets drawn by the effect shader
	int inlineFxBuckets = 0;                //!< lane EFX1: BGEM-less effect buckets, the source block written back
	int refractBuckets = 0;                 //!< lane EFX1: Refraction-flagged buckets, drawn by the refraction preview
	/* Lane GLOW1: billboard shapes turned to the camera, and those welded flat (the red
	 * WW_CELL_GLOW_RED=1 welds all of them, as before; past the cap the rest weld flat too). */
	const bool glowRed = qEnvironmentVariableIntValue( "WW_CELL_GLOW_RED" ) != 0;
	const int glowCap = 8192;
	int billboardShapes = 0, billboardFlat = 0;
	/* Lane FXLIT1: in an interior a lit effect shape (the effect-lighting flag) takes a bucket per placement,
	 * because the game lights it with its MODEL's four placed lights (src/gl/cellfxlit.h). */
	const bool fxLitOn = spec.interior && spec.overlay == CellOverlay::None;
	const int fxLitCap = 4096;
	int fxLitModels = 0, fxLitShapes = 0;
	QHash<QString, std::array<float, 4>> fxLitBound;   // a loaded model's bounding sphere, model space
	wwCellFxLitBegin( nif );
	int shapesVertexColor = 0;              //!< lane PRTPPLACE: drawn with the mesh's own vertex colors
	int placementsSwapped = 0;              //!< lane PRTPPLACE: drawn with a material swap
	int skyCardsHidden = 0;                 //!< lane PRTPPLACE: sky cards left to the sky layer
	/*! Lane PRTPPLACE: one resolved swap per MSWP form -- the material substitution, and the
	 *  CNAM color remapping index per folded ORIGINAL material (self rows included). */
	struct SwapUse
	{
		LodgenMaterialSubst sub;
		QHash<QString, float> cnam;
	};
	QHash<quint32, SwapUse> swapCache;
	int shapesRepainted = 0;                //!< lane PRTPPLACE: palette row replaced by a CNAM
	int cnamNoPalette = 0;                  //!< lane PRTPPLACE: a CNAM on a material with no palette (the game ignores it)
	int modcShapes = 0;                     //!< 2026-10-01: palette row taken from the base's MODC
	QStringList unreadableMatNames;
	qint64 srcTris = 0;

	auto bucketFor = [&]( const NativeSrcShape & s, bool withColour, const QString & own = QString() ) -> Bucket & {
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
		const bool blend = s.nearFacts.alphaBlend || s.effectBlend;
		const float alpha = blend ? qBound( 0.0f, s.matAlpha, 1.0f ) : 1.0f;
		const bool effect = s.nearFacts.effectShader && s.effectMatRead && spec.overlay == CellOverlay::None;
		// lane EFX1: an effect property with no BGEM, keyed by its own bytes
		const bool inlineFx = s.nearFacts.effectShader && !s.effectMatRead && !s.effectBlock.isEmpty()
			&& spec.overlay == CellOverlay::None;
		// lane EFX1: a Refraction-flagged lighting shape, drawn as the bend it is
		const bool refract = !s.nearFacts.effectShader && ( s.shaderSF1 & ( 1U << 15 ) )
			&& spec.overlay == CellOverlay::None;
		const QString key = QStringLiteral( "%1%2|%3|%4|%5|%6|%7|%8" )
			.arg( effect ? QStringLiteral( "E|%1|%2|%3|" ).arg( s.matName ).arg( s.shaderSF1 ).arg( s.shaderSF2 )
			    : inlineFx ? QStringLiteral( "I|%1|%2|%3|%4|" ).arg( QString::fromLatin1( s.effectBlock.toHex() ) )
			                     .arg( s.shaderSF1 ).arg( s.shaderSF2 ).arg( s.alphaFlags )
			    : refract ? QStringLiteral( "R|%1|" ).arg( double( s.refractStrength ) )
			             : QString() )
			.arg( mat )
			.arg( ( s.hasAlpha || blend ) ? 1 : 0 ).arg( blend ? 0 : int( s.alphaThreshold ) )
			.arg( s.ownEmit ? 1 : 0 ).arg( withColour ? 1 : 0 )
			.arg( ( mat.isEmpty() && s.matUnreadable ) ? 1 : 0 ).arg( double( alpha ) )
			+ own;   // lane GLOW1: a billboard placement's own bucket
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
		/* A blended bucket takes threshold 0, which the writer turns into the
		 * blend flags (4333); the tested buckets keep their cutoff. */
		b.hasAlpha = s.hasAlpha || blend;
		b.alphaThreshold = blend ? 0 : s.alphaThreshold;
		b.blend = blend;
		b.alpha = alpha;
		if ( blend )
			blendBuckets++;
		b.emits = s.ownEmit;
		b.emissiveScale = s.emitMult;
		b.withColour = withColour;
		if ( effect ) {
			b.effectMat = s.matName;
			b.effSF1 = s.shaderSF1;
			b.effSF2 = s.shaderSF2;
			effectBuckets++;
		} else if ( inlineFx ) {
			b.effectBlock = s.effectBlock;
			b.effSF1 = s.shaderSF1;
			b.effSF2 = s.shaderSF2;
			b.effAlphaFlags = s.alphaFlags;
			b.name = QStringLiteral( "effect (in the NIF)" );
			inlineFxBuckets++;
		} else if ( refract ) {
			b.refract = true;
			b.refractStrength = s.refractStrength;
			refractBuckets++;
		}
		return buckets.insert( key, b ).value();
	};

	const bool colouring = spec.overlay != CellOverlay::None;

	/* ---- lane PRTPPLACE: THE PROBE SOUP. What the transfer probes see is the
	 * STATIC world only (docs/PRTP_PLAN.md 2b): actors, havok clutter, FX and
	 * workshop builds are receivers, never occluders. Doors are left out too --
	 * baked open -- and only their boxes are kept, to tag the openings they
	 * stand in. Per shape, effect shaders, alpha-blended glass and decals are
	 * dropped: light passes through all three. */
	const QString probeOut = QString::fromLocal8Bit( qgetenv( "WW_CELL_PROBES" ) );
	const bool probing = !probeOut.isEmpty() || spec.probes;   // the env, or the PRTP band's Place
	ProbeSoup probeSoup;
	int soupRefs = 0, soupShapesDropped = 0;
	/* lane PRTPBAKE: the bake's albedo, one per soup triangle -- the band's Bake, or
	 * WW_CELL_PROBE_BAKE=<folder> (every probing build bakes there, for a headless run).
	 * WW_CELL_PROBE_BAKE_DIR=<folder> only moves the Bake button's folder. */
	const QString bakeEnv = QString::fromLocal8Bit( qgetenv( "WW_CELL_PROBE_BAKE" ) );
	const QString bakeDirEnv = QString::fromLocal8Bit( qgetenv( "WW_CELL_PROBE_BAKE_DIR" ) );
	const bool baking = probing && ( spec.probesBake || !bakeEnv.isEmpty() );
	ProbeAlbedo probeAlb( dataRoot );
	/* lane CAPTURE1: WW_CELL_BAKE_ALBEDO = tri (default, today) | hit | cube picks where the surfels'
	 * albedo and normal come from (ProbeBakeSpec::albedoWay); hit and cube need every object
	 * triangle's UVs, vertex colors, vertex normals and maps in the soup. WW_CELL_BAKE_ALBEDO_RED =
	 * centroid | nofilter, WW_CELL_BAKE_CUBE_FACE = pixels a face (128), WW_CELL_BAKE_CUBE_DUMP=<file>
	 * + WW_CELL_BAKE_CUBE_PROBES=x,y,z;... (the nearest probes' faces): gate and sheet only. */
	const QString albWay = qEnvironmentVariable( "WW_CELL_BAKE_ALBEDO", QStringLiteral( "tri" ) );
	const QString albRed = qEnvironmentVariable( "WW_CELL_BAKE_ALBEDO_RED" );
	const bool wantMat = baking && ( albWay == QLatin1String( "hit" ) || albWay == QLatin1String( "cube" ) );
	const bool wantExtra = wantMat && albWay == QLatin1String( "cube" ) && albRed == QLatin1String( "nofilter" );
	/* lane CAPTURE1 (docs/PRTP_PLAN.md 2o, EFX1): a Refraction-flagged lighting shape (Shader Flags 1 bit
	 * 15: the walkway's drip-splash rings) only bends what is behind it in game; its diffuse slot holds a
	 * normal map. It is no surface: it leaves the bake's soup (every albedo way, the cube included) and
	 * is counted. WW_CELL_BAKE_REFRACT_RED=keep keeps it as a solid surface (the gate's red only). */
	const bool refractKeepRed = qEnvironmentVariable( "WW_CELL_BAKE_REFRACT_RED" ) == QLatin1String( "keep" );
	/* lane GICAL1: the decals and alpha-blended surfaces the soup leaves out still color the surface under them in
	 * game (Vault111Cryo view w1: they change 36% of the surface pixels and darken the drawn albedo by 12%).
	 * They, and the placed decals (TXST, projected below), are folded into the surfel albedo (ProbeSoup::decal).
 * WW_CELL_BAKE_DECALS=0 leaves them out, as before. */
	const bool decalFold = qEnvironmentVariable( "WW_CELL_BAKE_DECALS" ) != QLatin1String( "0" );
	int soupDecalShapes = 0, soupPlacedDecals = 0, soupPlacedDecalTris = 0;
	int soupRefractShapes = 0, soupRefractTris = 0;
	QMap<QString, int> soupRefractModels;
	QHash<QString, int> matTexIdx;
	auto matTexOf = [&]( const QString & tex ) -> qint32 {
		if ( tex.isEmpty() )
			return -1;
		const QString k = tex.toLower();
		auto it = matTexIdx.constFind( k );
		if ( it != matTexIdx.constEnd() )
			return *it;
		const int i = int( probeSoup.matTex.size() );
		probeSoup.matTex.push_back( tex );
		matTexIdx.insert( k, i );
		return i;
	};
	// lane BAKE4: glass panes into the bake's soup; WW_CELL_PROBE_GLASS=<tsv> dumps the census
	QString glassCensus;
	const QByteArray glassDump = qgetenv( "WW_CELL_PROBE_GLASS" );
	int soupGlassShapes = 0;
	int soupLoadDoors = 0;   // lane ROOMCLAMP1
	/* lane ROOMCLAMP1: WW_CELL_ROOMCLAMP_PIN=off (gates only, never a user toggle) turns every rule of the lane off
	 * (load doors open, the placer's old floors, no probe moved off a back face, no rooms, no eye fallbacks): the
	 * output is byte for byte the exe from before the lane, which the before/after gates compare against. */
	const bool roomclampPin = qgetenv( "WW_CELL_ROOMCLAMP_PIN" ).trimmed() == "off";
	const bool loadDoorRed = roomclampPin || qEnvironmentVariable( "WW_CELL_PROBE_LOADDOOR_RED" ) == QLatin1String( "open" );
	// lane BAKE4: WW_CELL_PROBE_SOUP_REFS=<tsv> lists every reference the soup took (form, role, base type)
	QString soupRefList;
	const QByteArray soupRefDump = qgetenv( "WW_CELL_PROBE_SOUP_REFS" );
	int albTextured = 0, albUntextured = 0, albLandSplat = 0, albLandFlat = 0, albPalette = 0;
	QHash<qint64, std::array<float, 3>> landAlb;   // 128-unit ground quad -> gamma color, from the splat
	QHash<QString, int> soupSkippedTypes;
	auto soupRole = []( const QString & t ) -> int {   // 0 left out, 1 in the soup, 2 a door
		static const QSet<QString> in { QStringLiteral( "STAT" ), QStringLiteral( "MSTT" ),
			QStringLiteral( "TREE" ), QStringLiteral( "FURN" ), QStringLiteral( "CONT" ),
			QStringLiteral( "ACTI" ), QStringLiteral( "TERM" ), QStringLiteral( "FLOR" ),
			QStringLiteral( "LIGH" ) };
		if ( t == QLatin1String( "DOOR" ) )
			return 2;
		// lane BAKE4: the bake sees the fixed world only. WW_CELL_PROBE_SOUP_RED=items is the gate's red
		// control: pick-up items let into the soup.
		static const bool itemsRed = qgetenv( "WW_CELL_PROBE_SOUP_RED" ) == "items";
		static const QSet<QString> items { QStringLiteral( "ARMO" ), QStringLiteral( "WEAP" ),
			QStringLiteral( "MISC" ), QStringLiteral( "ALCH" ), QStringLiteral( "AMMO" ),
			QStringLiteral( "BOOK" ), QStringLiteral( "KEYM" ), QStringLiteral( "NOTE" ) };
		if ( itemsRed && items.contains( t ) )
			return 1;
		return in.contains( t ) ? 1 : 0;
	};
	/* lane SPEED1: A HEADLESS BAKE READS THE SOUP ONLY. With WW_CELL_PROBE_BAKE set and no lit picture asked
	 * (WW_CELL_LIT, WW_CELL_GI_DUMP), a placement the soup leaves out is neither loaded nor drawn: the same
	 * rule as the role below (disabled, marker, actor, a type soupRole leaves out, sky, water), counted the same.
	 * WW_CELL_SPEED_RED=nolean loads them all again -- the bake's files must be the same bytes either way. */
	const bool bakeLean = probing && !bakeEnv.isEmpty() && qEnvironmentVariableIsEmpty( "WW_CELL_LIT" )
		&& qEnvironmentVariableIsEmpty( "WW_CELL_GI_DUMP" ) && qgetenv( "WW_CELL_SPEED_RED" ) != "nolean";
	int leanSkipped = 0;
	auto soupLeavesOut = [&]( const Placement & p, bool counted ) -> bool {
		const EsmLodBase & lb = world.lodBase( p.base );
		if ( !p.actorKey.isEmpty() ) {   // a placed actor (lane PLACED1) never enters the soup
			if ( !p.disabled && counted )
				soupSkippedTypes[CellPickTable::typeName( lb.type )]++;
			return true;
		}
		if ( lb.model.isEmpty() )
			return false;   // the loop's own row counts it
		if ( p.disabled || isMarkerModel( lb.model ) )
			return true;
		QString tn = CellPickTable::typeName( lb.type );
		int role = soupRole( tn );
		const QString ml = QString( lb.model ).replace( '/', '\\' ).toLower();
		if ( role == 1 && ml.startsWith( QLatin1String( "sky\\" ) ) ) {
			role = 0;
			tn = QStringLiteral( "sky" );
		} else if ( role == 1 && ml.startsWith( QLatin1String( "water\\" ) ) ) {
			role = 0;
			tn = QStringLiteral( "water" );
		}
		// the gate's red control (WW_CELL_SPEED_RED=leanred): every second reference the soup DOES take is skipped too
		static const bool leanRed = qgetenv( "WW_CELL_SPEED_RED" ) == "leanred";
		if ( leanRed && role == 1 && ( p.ref & 1 ) )
			return true;
		if ( role == 0 && counted )
			soupSkippedTypes[tn]++;
		return role == 0;
	};

	CellSpeed::mark( "references gathered" );
	/* Lane SPEED1, the gate's second red control (WW_CELL_SPEED_RED=transform): the model placed most often
	 * loses its per-placement transform -- every copy stands where the first one does. The picture check of
	 * tests/spells/cell_speed.sh must see it. */
	if ( qgetenv( "WW_CELL_SPEED_RED" ) == "transform" ) {
		QHash<QString, int> uses;
		for ( const Placement & p : placements )
			uses[world.lodBase( p.base ).model]++;
		QString worst;
		int most = 0;
		for ( auto it = uses.constBegin(); it != uses.constEnd(); ++it ) {
			if ( !it.key().isEmpty() && ( it.value() > most || ( it.value() == most && it.key() < worst ) ) ) {
				worst = it.key();
				most = it.value();
			}
		}
		const Placement * first = nullptr;
		for ( Placement & p : placements ) {
			if ( world.lodBase( p.base ).model != worst )
				continue;
			if ( first ) {
				p.pos = first->pos;
				p.rot = first->rot;
				p.scale = first->scale;
			} else {
				first = &p;
			}
		}
		fprintf( stderr, "cell speed red: %d placements of %s stand on the first one\n", most, qPrintable( worst ) );
	}
	// lane SPEED1: every model the loop below will ask for, read and parsed on worker threads first
	CellModelAhead modelsAhead( dataRoot, refsRed != "root" );
	for ( const Placement & p : placements )
		if ( p.actorKey.isEmpty() && ( !bakeLean || !soupLeavesOut( p, false ) ) )   // an actor is built, not read
			modelsAhead.want( world, p.base, p.swap );
	modelsAhead.load();
	CellSpeed::mark( "models read ahead" );
	for ( const Placement & p : placements ) {
		const EsmLodBase & lb = world.lodBase( p.base );
		const bool isActor = !p.actorKey.isEmpty();   // lane PLACED1
		const QString model = isActor ? p.actorKey : lb.model;
		if ( model.isEmpty() ) {
			skippedByType[CellPickTable::typeName( lb.type )]++;
			continue;
		}
		/* SKY CARDS ARE NOT WORLD GEOMETRY (lane PRTPPLACE, 2026-09-30). The distant cloud
		 * cards (Sky\CloudDistant*) are placed STATs the game draws in its sky layer, behind
		 * everything. Drawn as world geometry, one card hid the whole of Concord behind a grey
		 * sheet (bungo's "wedges"; measured: hiding it alone uncovers the town). Hidden and
		 * counted; WW_CELL_SKY=1 draws them again. */
		static const bool showSky = !qgetenv( "WW_CELL_SKY" ).isEmpty();
		if ( !showSky && QString( model ).replace( '/', '\\' ).startsWith( QLatin1String( "sky\\" ), Qt::CaseInsensitive ) ) {
			skyCardsHidden++;
			continue;
		}
		if ( bakeLean && soupLeavesOut( p, true ) ) {   // lane SPEED1
			leanSkipped++;
			continue;
		}
		/* THE MATERIAL SWAP (lane PRTPPLACE, 2026-09-30): the ref's XMSP, else the base's
		 * MODS, applied at load exactly as the LOD bake does (lodgen.cpp swapFor: first row
		 * per key wins, a self-swap is dropped). Without it every car drew its default
		 * rust paint (bungo, the Concord pickup). The cache key carries the swap. */
		const LodgenMaterialSubst * subst = nullptr;
		const QHash<QString, float> * cnamOf = nullptr;
		if ( p.swap ) {
			auto sit = swapCache.constFind( p.swap );
			if ( sit == swapCache.constEnd() ) {
				SwapUse use;
				const EsmMaterialSwap & mw = world.materialSwap( p.swap );
				if ( mw.exists ) {
					QSet<QString> seen;
					for ( const EsmMaterialSubst & row : mw.rows ) {
						const QString k = lodgenMaterialSwapKey( row.original );
						if ( k.isEmpty() || row.replacement.isEmpty() || seen.contains( k ) )
							continue;
						seen.insert( k );
						// the engine takes the index only below FLT_MAX (unset = FLT_MAX)
						if ( row.hasColorRemap && row.colorRemap < std::numeric_limits<float>::max() )
							use.cnam.insert( k, row.colorRemap );
						if ( lodgenMaterialSwapKey( row.replacement ) != k )
							use.sub.append( qMakePair( k, row.replacement ) );
					}
				}
				sit = swapCache.insert( p.swap, use );
			}
			if ( !sit->sub.isEmpty() )
				subst = &sit->sub;
			if ( !sit->cnam.isEmpty() )
				cnamOf = &sit->cnam;
		}
		const QString mkey = subst
			? QStringLiteral( "%1|%2" ).arg( model ).arg( p.swap, 8, 16, QChar( '0' ) ) : model;
		auto mit = modelCache.find( mkey );
		if ( mit == modelCache.end() ) {
			if ( modelsFailed.contains( mkey ) )
				continue;
			std::vector<NativeSrcShape> shapes;
			CellSpeed::Acc speedLoad( "model loads" );
			// ONE LOAD PER DISTINCT MODEL AND SWAP -- the whole block shares this cache.
			// lane MISS1: a placed model's root node takes the reference's transform; the file's own is left out.
			// WW_CELL_REFS_RED=root is the gate's red control: the old load, root transform composed.
			// lane PLACED1: a placed actor's shapes come posed from src/cellactor.cpp, not from a model file.
			bool okAhead = false;   // lane SPEED1: the answer read ahead, else the load as before
			const bool okLoad = isActor ? actors.shapes( model, &shapes )
				: modelsAhead.take( model, subst, &shapes, &okAhead ) ? okAhead : refsRed != "root"
				? lodgenNativeLoadModelPlaced( const_cast<QString *>( &dataRoot ), model, subst, &shapes )
				: subst
				? lodgenNativeLoadModelSwapped( const_cast<QString *>( &dataRoot ), model, *subst, &shapes )
				: lodgenNativeLoadModel( const_cast<QString *>( &dataRoot ), model, &shapes );
			if ( !okLoad || shapes.empty() ) {
				modelsFailed.insert( mkey );
				continue;
			}
			modelLoads += isActor ? 0 : 1;
			mit = modelCache.insert( mkey, shapes );
		}
		placementsSwapped += subst ? 1 : 0;
		/* THE PAINT (lane PRTPPLACE, 2026-09-30). An MSWP row's CNAM color remapping index
		 * replaces the material's palette row, and only on a Greyscale_To_PaletteColor
		 * material (engine 1.10.155, lane FIX1's reading, nativeemit.cpp). The row is found by
		 * the shape's ORIGINAL material, so the plain load gives each shape's original name
		 * (a swap changes materials only: the shape lists line up). The renderer samples the
		 * palette at paletteScale * vertex R, so the index goes in as R = CNAM / scale. */
		const std::vector<NativeSrcShape> * plainShapes = nullptr;
		if ( cnamOf ) {
			auto pit = subst ? modelCache.find( model ) : mit;
			if ( pit == modelCache.end() && !modelsFailed.contains( model ) ) {
				std::vector<NativeSrcShape> shapes;
				if ( ( refsRed != "root"
						? lodgenNativeLoadModelPlaced( const_cast<QString *>( &dataRoot ), model, nullptr, &shapes )
						: lodgenNativeLoadModel( const_cast<QString *>( &dataRoot ), model, &shapes ) ) && !shapes.empty() ) {
					modelLoads++;
					pit = modelCache.insert( model, shapes );
					mit = modelCache.find( mkey );   // an insert may rehash
				} else {
					modelsFailed.insert( model );
				}
			}
			if ( pit != modelCache.end() && pit.value().size() == mit.value().size() )
				plainShapes = &pit.value();
		}
		/* WW_CELL_SWAPLOG=<model substring>: every shape of a matching placement, its material
		 * before and after the swap and what the paint rule decided -- to stderr. */
		static const QString swapLog = QString::fromLocal8Bit( qgetenv( "WW_CELL_SWAPLOG" ) );
		if ( !swapLog.isEmpty() && model.contains( swapLog, Qt::CaseInsensitive ) ) {
			fprintf( stderr, "SWAPLOG ref %08x model %s swap %08x subst %d cnam %d modc %g\n", p.ref,
				qPrintable( model ), p.swap, subst ? int( subst->size() ) : 0, cnamOf ? int( cnamOf->size() ) : 0,
				lb.hasColorRemap ? double( lb.colorRemap ) : -1.0 );
			if ( cnamOf )
				for ( auto c = cnamOf->cbegin(); c != cnamOf->cend(); ++c )
					fprintf( stderr, "SWAPLOG   cnam %s = %g\n", qPrintable( c.key() ), double( c.value() ) );
			for ( size_t si = 0; si < mit.value().size(); si++ ) {
				const NativeSrcShape & s = mit.value()[si];
				fprintf( stderr, "SWAPLOG   shape %d mat %s orig %s g2p %d scale %g\n", int( si ),
					qPrintable( s.matName ), plainShapes ? qPrintable( ( *plainShapes )[si].matName ) : "-",
					int( s.g2p ), double( s.g2pScale ) );
			}
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
		int fxLitSerial = -1;   // lane FXLIT1: this placement's model in the lit-effect table, once a shape needs it
		int role = 0;   // lane PRTPPLACE
		bool soupFoliage = false;   // alpha-tested leaves and grass cards never roof or wall a probe
		if ( probing && !p.disabled && !pick.marker ) {
			QString tn = CellPickTable::typeName( lb.type );
			role = isActor ? 0 : soupRole( tn );   // lane PLACED1: an actor never enters the probe soup
			// Sky meshes (distant clouds) are kilometer sheets over the town: a false roof everywhere.
			// Water planes have no collision, so FO4CS's rays pass them too.
			const QString ml = QString( model ).replace( '/', '\\' ).toLower();
			if ( role == 1 && ml.startsWith( QLatin1String( "sky\\" ) ) ) {
				role = 0;
				tn = QStringLiteral( "sky" );
			} else if ( role == 1 && ml.startsWith( QLatin1String( "water\\" ) ) ) {
				role = 0;
				tn = QStringLiteral( "water" );
			}
			soupFoliage = ml.startsWith( QLatin1String( "landscape\\" ) );
			/* lane ROOMCLAMP1: a load door (XTEL) never opens in game: solid in the soup, always shut, no
			 * opening for rooms or the placer. WW_CELL_PROBE_LOADDOOR_RED=open keeps today's rule (gate red). */
			if ( role == 2 && p.loadDoor ) {
				soupLoadDoors++;
				if ( !loadDoorRed )
					role = 1;
			}
			if ( role == 0 )
				soupSkippedTypes[tn]++;
			else if ( role == 1 )
				soupRefs++;
			if ( role != 0 && !soupRefDump.isEmpty() )   // lane BAKE4
				soupRefList += QStringLiteral( "%1\t%2\t%3\n" ).arg( p.ref, 8, 16, QLatin1Char( '0' ) ).arg( role ).arg( tn );
		}

		for ( size_t si = 0; si < mit.value().size(); si++ ) {
			const NativeSrcShape & s = mit.value()[si];
			const size_t nv = s.geom.pos.size() / 3;
			if ( !nv || s.geom.tris.empty() )
				continue;
			/* THE PAINT ROW this shape takes: the swap row's CNAM for its original material, else
			 * the base's own MODC (2026-10-01: the museum pickup's paint lives there, bungo's
			 * "vertex paint"). Only a Greyscale_To_PaletteColor material takes one. */
			bool cnamHere = false, havePaint = false;
			float paintIndex = 0.0f;
			if ( plainShapes ) {
				auto cit = cnamOf->constFind( lodgenMaterialSwapKey( ( *plainShapes )[si].matName ) );
				if ( cit != cnamOf->constEnd() ) {
					cnamHere = havePaint = true;
					paintIndex = cit.value();
				}
			}
			if ( !havePaint && lb.hasColorRemap ) {
				havePaint = true;
				paintIndex = lb.colorRemap;
			}
			const bool palette = s.g2p && s.g2pScale > 1.0e-6f;
			const bool painted = palette && havePaint;
			// lane CAPTURE1: a triangle's material for the hit and cube albedo ways
			auto triMat = [&]( size_t t, const QString & tex, bool withPalette ) {
				ProbeSoup::TriMat m;
				m.tex = matTexOf( tex );
				if ( withPalette && s.g2p && !s.g2pTex.isEmpty() ) {
					m.pal = matTexOf( s.g2pTex );
					m.row = painted ? paintIndex : -1.0f;
					m.rowScale = s.g2pScale;
				}
				for ( int i = 0; i < 3; i++ ) {
					const size_t vi = size_t( s.geom.tris[t + size_t( i )] );
					if ( s.geom.uv.size() >= nv * 2 ) {
						m.uv[i * 2] = s.geom.uv[vi * 2];
						m.uv[i * 2 + 1] = s.geom.uv[vi * 2 + 1];
					}
					if ( s.geom.rgba.size() == nv * 4 )
						for ( int k = 0; k < 3; k++ )
							m.vc[i * 3 + k] = s.geom.rgba[vi * 4 + size_t( k )];
					if ( s.geom.nrm.size() >= nv * 3 ) {
						const Vector3 wn = p.rot * Vector3( s.geom.nrm[vi * 3], s.geom.nrm[vi * 3 + 1], s.geom.nrm[vi * 3 + 2] );
						const float l = wn.length();
						for ( int k = 0; k < 3; k++ )
							m.n[i * 3 + k] = l > 1e-6f ? qint16( std::lround( std::clamp( wn[k] / l, -1.0f, 1.0f ) * 32767.0f ) ) : 0;
					}
				}
				return m;
			};
			if ( role == 1 ) {
				if ( baking && probeGlassFeed( probeSoup, probeAlb, s, p.pos, p.rot, p.scale, p.ref, model,
						glassDump.isEmpty() ? nullptr : &glassCensus ) )
					soupGlassShapes++;   // lane BAKE4
				const bool refractOnly = !s.nearFacts.effectShader && ( s.shaderSF1 & ( 1U << 15 ) ) && !refractKeepRed;
				if ( refractOnly ) {   // lane CAPTURE1: refraction-only, no surface
					soupRefractShapes++;
					soupRefractTris += int( s.geom.tris.size() / 3 );
					soupRefractModels[model]++;
				}
				if ( refractOnly || s.nearFacts.effectShader || !s.effectTex0.isEmpty() || s.nearFacts.alphaBlend
					|| s.nearFacts.decal || ( soupFoliage && s.nearFacts.alphaTest ) ) {
					soupShapesDropped++;
					if ( baking && decalFold && !refractOnly && !s.nearFacts.effectShader && s.effectTex0.isEmpty()
						&& ( s.nearFacts.alphaBlend || s.nearFacts.decal ) && !( soupFoliage && s.nearFacts.alphaTest ) ) {
						soupDecalShapes++;   // lane GICAL1: its albedo and coverage, the mean of 10 points a triangle
						for ( size_t t = 0; t + 2 < s.geom.tris.size(); t += 3 ) {
							size_t vi[3];
							bool okTri = true;
							float w[9];
							for ( int k = 0; k < 3; k++ ) {
								vi[k] = size_t( s.geom.tris[t + size_t( k )] );
								okTri = okTri && vi[k] < nv;
								if ( !okTri )
									break;
								const Vector3 wp = p.pos + p.rot * ( Vector3( s.geom.pos[vi[k] * 3], s.geom.pos[vi[k] * 3 + 1],
									s.geom.pos[vi[k] * 3 + 2] ) * p.scale );
								for ( int c = 0; c < 3; c++ )
									w[k * 3 + c] = wp[c];
							}
							if ( !okTri )
								continue;
							double sum[3] = { 0, 0, 0 }, sa = 0;
							int got = 0;
							for ( int i = 0; i < 4; i++ )
								for ( int j = 0; i + j < 4; j++ ) {
									const float b1 = ( float( i ) + 1.0f / 3.0f ) / 4.0f, b2 = ( float( j ) + 1.0f / 3.0f ) / 4.0f;
									const float bw[3] = { 1.0f - b1 - b2, b1, b2 };
									float uv[2] = { 0.5f, 0.5f }, vc[3] = { 1, 1, 1 }, va = 1.0f, lin[3], ta = 1.0f;
									if ( s.geom.uv.size() >= nv * 2 )
										for ( int k = 0; k < 2; k++ )
											uv[k] = bw[0] * s.geom.uv[vi[0] * 2 + size_t( k )] + bw[1] * s.geom.uv[vi[1] * 2 + size_t( k )]
												+ bw[2] * s.geom.uv[vi[2] * 2 + size_t( k )];
									if ( s.geom.rgba.size() == nv * 4 ) {
										for ( int k = 0; k < 3; k++ )
											vc[k] = ( bw[0] * s.geom.rgba[vi[0] * 4 + size_t( k )] + bw[1] * s.geom.rgba[vi[1] * 4 + size_t( k )]
												+ bw[2] * s.geom.rgba[vi[2] * 4 + size_t( k )] ) / 255.0f;
										va = ( bw[0] * s.geom.rgba[vi[0] * 4 + 3] + bw[1] * s.geom.rgba[vi[1] * 4 + 3]
											+ bw[2] * s.geom.rgba[vi[2] * 4 + 3] ) / 255.0f;
									}
									if ( !probeAlb.sample( s.tex0, uv[0], uv[1], vc, lin, &ta ) )
										continue;
									const double a = double( std::clamp( ta * va, 0.0f, 1.0f ) );
									for ( int k = 0; k < 3; k++ )
										sum[k] += lin[k] * a;
									sa += a;
									got++;
								}
							if ( !got || !( sa > 0 ) )
								continue;   // no map read, or nothing covered
							quint8 q[4];
							for ( int k = 0; k < 3; k++ )
								q[k] = quint8( std::lround( std::clamp( sum[k] / sa, 0.0, 1.0 ) * 255.0 ) );
							q[3] = quint8( std::lround( std::clamp( sa / got, 0.0, 1.0 ) * 255.0 ) );
							probeSoup.decal.insert( probeSoup.decal.end(), w, w + 9 );
							probeSoup.decalA.insert( probeSoup.decalA.end(), q, q + 4 );
						}
					}
					if ( wantExtra )   // lane CAPTURE1 red nofilter: the cube sees what the soup leaves out
						for ( size_t t = 0; t + 2 < s.geom.tris.size(); t += 3 ) {
							bool okTri = true;
							float w[9];
							for ( int k = 0; k < 3 && okTri; k++ ) {
								const size_t vi = size_t( s.geom.tris[t + size_t( k )] );
								okTri = vi < nv;
								if ( !okTri )
									break;
								const Vector3 wp = p.pos + p.rot * ( Vector3( s.geom.pos[vi * 3], s.geom.pos[vi * 3 + 1],
									s.geom.pos[vi * 3 + 2] ) * p.scale );
								for ( int c = 0; c < 3; c++ )
									w[k * 3 + c] = wp[c];
							}
							if ( !okTri )
								continue;
							probeSoup.cubeExtra.insert( probeSoup.cubeExtra.end(), w, w + 9 );
							probeSoup.cubeExtraMat.push_back( triMat( t,
								s.nearFacts.effectShader || !s.effectTex0.isEmpty() ? s.effectTex0 : s.tex0, false ) );
						}
				} else {
					for ( size_t t = 0; t + 2 < s.geom.tris.size(); t += 3 ) {
						float w[3][3];
						bool okTri = true;
						for ( int k = 0; k < 3; k++ ) {
							const size_t vi = size_t( s.geom.tris[t + size_t( k )] );
							if ( vi >= nv ) {
								okTri = false;
								break;
							}
							const Vector3 lp( s.geom.pos[vi * 3 + 0], s.geom.pos[vi * 3 + 1],
								s.geom.pos[vi * 3 + 2] );
							const Vector3 wp = p.pos + p.rot * ( lp * p.scale );
							w[k][0] = wp[0];
							w[k][1] = wp[1];
							w[k][2] = wp[2];
						}
						if ( okTri && baking ) {
							// the map at the triangle's UV centroid, times its vertex color
							float uv[2] = { 0.5f, 0.5f }, vc[3] = { 1, 1, 1 }, lin[3];
							const size_t i0 = size_t( s.geom.tris[t] ), i1 = size_t( s.geom.tris[t + 1] ),
								i2 = size_t( s.geom.tris[t + 2] );
							if ( s.geom.uv.size() >= nv * 2 )
								for ( int k = 0; k < 2; k++ )
									uv[k] = ( s.geom.uv[i0 * 2 + size_t( k )] + s.geom.uv[i1 * 2 + size_t( k )]
										+ s.geom.uv[i2 * 2 + size_t( k )] ) / 3.0f;
							if ( s.geom.rgba.size() == nv * 4 )
								for ( int k = 0; k < 3; k++ )
									vc[k] = ( s.geom.rgba[i0 * 4 + size_t( k )] + s.geom.rgba[i1 * 4 + size_t( k )]
										+ s.geom.rgba[i2 * 4 + size_t( k )] ) / ( 3.0f * 255.0f );
							quint8 rgb[3] = { 128, 128, 128 };
							/* a palette material is painted as the game paints it: the map's green
							 * picks the column, the paint index (or scale x vertex red) the row */
							const bool viaPalette = s.g2p && !s.g2pTex.isEmpty()
								&& probeAlb.samplePalette( s.tex0, s.g2pTex, uv[0], uv[1],
									painted ? paintIndex : s.g2pScale * vc[0], lin );
							albPalette += viaPalette ? 1 : 0;
							if ( viaPalette || probeAlb.sample( s.tex0, uv[0], uv[1], vc, lin ) ) {
								albTextured++;
								for ( int k = 0; k < 3; k++ )
									rgb[k] = quint8( std::lround( std::clamp( lin[k], 0.0f, 1.0f ) * 255.0f ) );
							} else {
								albUntextured++;
							}
							probeSoup.addTri( w[0], w[1], w[2], rgb );
							if ( wantMat )
								probeSoup.setLastMat( triMat( t, s.tex0, true ) );
						} else if ( okTri ) {
							probeSoup.addTri( w[0], w[1], w[2] );
						}
						if ( okTri && s.nearFacts.twoSided )
							probeSoup.markLastTwoSided();   // lane ROOMCLAMP1: no back face
					}
				}
			}
			/* THE MESH'S OWN VERTEX COLORS (lane PRTPPLACE, 2026-09-30): the loader kept them
			 * (geom.rgba, only where the game applies them) and this view never drew them -- the
			 * Concord pickup came out bare rust orange (bungo). An overlay still wins. RGB only:
			 * the alpha channel's meaning varies by material and this view has no use for it. */
			const bool ownColor = !colouring && s.geom.rgba.size() == nv * 4;
			shapesVertexColor += ownColor ? 1 : 0;
			float paletteR = 1.0f;
			bool repaint = false;
			if ( !colouring ) {
				if ( painted ) {
					repaint = true;
					paletteR = paintIndex / s.g2pScale;
					shapesRepainted++;
					modcShapes += cnamHere ? 0 : 1;
				} else if ( cnamHere ) {
					cnamNoPalette++;   // a swap row's CNAM the game ignores; a MODC on an unpainted part is normal
				}
			}
			/* Lane GLOW1: a billboard shape takes a bucket of its own, its vertices in the
			 * billboard node's frame; the game drops the node's (and the reference's) rotation
			 * and faces the camera, which is what the viewer's NiBillboardNode does. */
			const bool bb = s.billboard && !glowRed && billboardShapes < glowCap;
			billboardFlat += ( s.billboard && !bb ) ? 1 : 0;
			Matrix bbInv;
			Vector3 bbPivot( s.bbPos[0], s.bbPos[1], s.bbPos[2] );
			float bbInvScale = 1.0f;
			QString bbOwn;
			if ( bb ) {
				Matrix bbRot;
				for ( int r = 0; r < 3; r++ )
					for ( int c = 0; c < 3; c++ )
						bbRot( r, c ) = s.bbRot[r * 3 + c];
				bbInv = bbRot.inverted();
				bbInvScale = s.bbScale > 1.0e-6f ? 1.0f / s.bbScale : 1.0f;
				bbOwn = QStringLiteral( "|BB|%1" ).arg( billboardShapes++ );
			}
			/* Lane FXLIT1: a lit effect shape joins its placement's own bucket. The model's bound is the
			 * merge of its shapes' spheres, as a scene node merges its children's, placed by the reference. */
			if ( fxLitOn && s.effectLit && s.nearFacts.effectShader && !p.disabled
				&& ( fxLitSerial >= 0 || fxLitModels < fxLitCap ) ) {
				if ( fxLitSerial < 0 ) {
					auto bit = fxLitBound.constFind( mkey );
					if ( bit == fxLitBound.constEnd() ) {
						std::array<float, 4> all = { 0.0f, 0.0f, 0.0f, -1.0f };
						for ( const NativeSrcShape & o : mit.value() ) {
							float one[4];
							wwCellFxLitSphere( o.geom.pos.data(), o.geom.pos.size() / 3, one );
							wwCellFxLitMerge( all.data(), one );
						}
						bit = fxLitBound.insert( mkey, all );
					}
					const Vector3 wc = p.pos + p.rot * ( Vector3( bit.value()[0], bit.value()[1], bit.value()[2] ) * p.scale );
					WwFxLitModel m;
					for ( int k = 0; k < 3; k++ )
						m.center[k] = wc[k];
					m.radius = std::max( bit.value()[3], 0.0f ) * p.scale;
					m.ref = p.ref;
					m.model = model;
					fxLitSerial = wwCellFxLitModel( nif, m );
					fxLitModels++;
				}
				bbOwn += QStringLiteral( "|FL|%1" ).arg( fxLitSerial );
				fxLitShapes++;
			}
			if ( isActor )
				bbOwn += QLatin1String( "|ACTOR" );   // lane PLACED1: own buckets; no decal lands on an actor
			Bucket & b = bucketFor( s, colouring || ownColor || repaint, bbOwn );
			if ( fxLitSerial >= 0 && bbOwn.contains( QLatin1String( "|FL|" ) ) )
				b.fxLit = fxLitSerial;
			if ( bb ) {
				b.billboard = true;
				b.bbPos = p.pos + p.rot * ( bbPivot * p.scale );
				b.bbScale = p.scale * s.bbScale;
				b.bbMode = s.bbMode;
			}
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
				if ( bb ) {   // lane GLOW1: the billboard node's own frame
					o.pos = bbInv * ( lp - bbPivot ) * bbInvScale;
					o.nrm = bbInv * ln;
					o.tan = bbInv * lt;
				} else {
					o.pos = wp - origin;
					o.nrm = p.rot * ln;
					o.tan = p.rot * lt;
				}
				o.bit = Vector3::crossproduct( o.nrm, o.tan );
				if ( o.bit.length() < 1.0e-6f )
					o.bit = Vector3( 0.0f, 0.0f, 1.0f );
				o.uv = s.geom.uv.size() >= ( v + 1 ) * 2
					? Vector2( s.geom.uv[v * 2 + 0], s.geom.uv[v * 2 + 1] )
					: Vector2( 0.0f, 0.0f );
				for ( int c = 0; c < 3; c++ )
					o.chan[c] = ownColor ? float( s.geom.rgba[v * 4 + size_t( c )] ) / 255.0f : rgb[c];
				if ( repaint )
					o.chan[0] *= paletteR;
				if ( ownColor && ( !b.effectMat.isEmpty() || !b.effectBlock.isEmpty() ) )
					o.chan[3] = float( s.geom.rgba[v * 4 + 3] ) / 255.0f;   // the effect shader's vertex alpha
				b.verts.push_back( o );
				for ( int k = 0; k < 3; k++ ) {
					lo[k] = qMin( lo[k], wp[k] );
					hi[k] = qMax( hi[k], wp[k] );
				}
			}
			for ( size_t t = 0; t + 2 < s.geom.tris.size(); t += 3 ) {
				b.tris.push_back( BucketTri{ { quint32( base + int( s.geom.tris[t + 0] ) ),
					quint32( base + int( s.geom.tris[t + 1] ) ),
					quint32( base + int( s.geom.tris[t + 2] ) ) } } );
			}
			pick.triangles += quint32( s.geom.tris.size() / 3 );
			srcTris += qint64( s.geom.tris.size() / 3 );
		}
		if ( pick.triangles ) {
			for ( int k = 0; k < 3; k++ ) {
				pick.bmin[k] = lo[k];
				pick.bmax[k] = hi[k];
			}
			if ( role == 2 ) {   // lane PRTPPLACE: a door only tags an opening
				ProbeSoup::Door d;
				d.ref = p.ref;
				for ( int k = 0; k < 3; k++ ) {
					d.lo[k] = lo[k];
					d.hi[k] = hi[k];
				}
				probeSoup.doors.push_back( d );
			}
			picks.append( pick );
			drawn += isActor ? 0 : 1;
		}
	}
	CellSpeed::mark( "models loaded and placements welded" );

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
					if ( baking ) {
						/* lane PRTPBAKE: the ground's albedo per 128-unit quad -- each pass's map
						 * mean times its VCLR, laid over the one below by the pass's mean opacity,
						 * in the splat's draw order (buckets are emitted in it) */
						std::vector<size_t> order( sb.quads.size() );
						for ( size_t qi = 0; qi < order.size(); qi++ )
							order[qi] = qi;
						std::stable_sort( order.begin(), order.end(), [&sb]( size_t a, size_t b ) {
							return sb.quads[a].bucket < sb.quads[b].bucket;
						} );
						for ( size_t qi : order ) {
							const CellSplatQuad & q = sb.quads[qi];
							if ( q.bucket < 0 || q.bucket >= sb.buckets.size() )
								continue;
							float m[3];
							if ( !probeAlb.meanGamma( sb.buckets.at( q.bucket ).diffuse, m ) )
								continue;
							float wv = 0, vc[3] = { 0, 0, 0 };
							for ( int k = 0; k < 4; k++ ) {
								wv += q.v[k].w * 0.25f;
								for ( int c = 0; c < 3; c++ )
									vc[c] += q.v[k].rgb[c] * 0.25f;
							}
							const qint64 gx = qint64( std::lround( ( q.v[0].p[0] + origin[0] ) / 128.0f ) );
							const qint64 gy = qint64( std::lround( ( q.v[0].p[1] + origin[1] ) / 128.0f ) );
							const qint64 key = ( gx << 32 ) ^ ( gy & 0xffffffffll );
							auto it = landAlb.find( key );
							const bool base = !sb.buckets.at( q.bucket ).blend || it == landAlb.end();
							std::array<float, 3> & col = landAlb[key];
							for ( int c = 0; c < 3; c++ ) {
								const float layer = m[c] * vc[c];
								col[c] = base ? layer : col[c] + ( layer - col[c] ) * std::clamp( wv, 0.0f, 1.0f );
							}
						}
					}
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
						gbk.tris.push_back( BucketTri{ { quint32( base ), quint32( base + 1 ),
							quint32( base + 2 ) } } );
						gbk.tris.push_back( BucketTri{ { quint32( base ), quint32( base + 2 ),
							quint32( base + 3 ) } } );
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
					gbk.tris.push_back( BucketTri{ { quint32( base ), quint32( base + 1 ),
						quint32( base + 2 ) } } );
					gbk.tris.push_back( BucketTri{ { quint32( base ), quint32( base + 2 ),
						quint32( base + 3 ) } } );
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

	/* ---- lane PLACED1: THE PLACED DECALS. Each one is clipped out of the opaque surfaces welded
	 * above (src/celldecal.h has the rule) and drawn as a blended surface with the Decal flag, so
	 * the lit program lights it like the surface it lies on. The pieces never enter the probe soup:
	 * the game has no bake that sees its decals. */
	CellDecalResult decalResult;
	if ( !decalRefs.empty() ) {
		std::vector<CellDecalReceiver> receivers;
		// lane SPEED1: in key order. A hash's own order changes from run to run (its seed), and with it
		// which of two equal crossings a decal's ray keeps: measured, the same cell welded 2975079 to
		// 2975085 vertices in five runs of one program.
		QStringList receiverKeys = buckets.keys();
		std::sort( receiverKeys.begin(), receiverKeys.end() );
		for ( const QString & key : receiverKeys ) {
			const auto it = buckets.constFind( key );
			const Bucket & b = it.value();
			if ( b.blend || b.hasAlpha || b.billboard || b.refract || !b.effectMat.isEmpty()
				|| !b.effectBlock.isEmpty() || b.verts.empty() || b.tris.empty()
				|| it.key() == QLatin1String( "\x01water" ) || it.key() == QLatin1String( "\x01grid" )
				|| it.key().contains( QLatin1String( "|ACTOR" ) ) )
				continue;
			CellDecalReceiver rc;
			rc.pos = &b.verts[0].pos[0];
			rc.nrm = &b.verts[0].nrm[0];
			rc.stride = sizeof( OutVert );
			rc.numVerts = b.verts.size();
			rc.tris = &b.tris[0].v[0];
			rc.numTris = b.tris.size();
			receivers.push_back( rc );
		}
		const float org[3] = { origin[0], origin[1], origin[2] };
		cellProjectDecals( world, dataRoot, decalRefs, receivers, org, decalResult,
			QString::fromLocal8Bit( qgetenv( "WW_CELL_DECAL_DUMP" ) ) );
		for ( const CellDecalMesh & m : decalResult.meshes ) {
			if ( m.tris.empty() )
				continue;
			Bucket b;
			b.name = m.name;
			b.matString = m.diffuse;
			b.normalTex = m.normal;
			b.specTex = m.spec;
			b.decal = true;
			b.hasAlpha = true;       // blended; the vertex alpha carries the angle fade
			b.alphaThreshold = 0;
			b.withColour = true;
			b.verts.reserve( m.verts.size() );
			for ( const CellDecalVert & v : m.verts ) {
				OutVert o;
				o.pos = Vector3( v.pos[0], v.pos[1], v.pos[2] );
				o.nrm = Vector3( v.nrm[0], v.nrm[1], v.nrm[2] );
				o.tan = Vector3( v.tan[0], v.tan[1], v.tan[2] );
				o.bit = Vector3( v.bit[0], v.bit[1], v.bit[2] );
				o.uv = Vector2( v.uv[0], v.uv[1] );
				o.chan[3] = v.alpha;
				b.verts.push_back( o );
			}
			for ( size_t t = 0; t + 2 < m.tris.size(); t += 3 )
				b.tris.push_back( BucketTri{ { m.tris[t], m.tris[t + 1], m.tris[t + 2] } } );
			buckets.insert( QStringLiteral( "\x02" ) + m.name, b );
			/* lane GICAL1: the placed decals cover the surfaces they were clipped from, so they fold into the
			 * bake's albedo too (same rule as the dropped decal shapes: mean color and coverage, 10 points a
			 * triangle; coverage = the texture's alpha x the angle fade, as the visible draw blends it) */
			if ( baking && decalFold ) {
				soupPlacedDecals += m.decals;
				for ( size_t t = 0; t + 2 < m.tris.size(); t += 3 ) {
					const CellDecalVert * dv[3];
					bool okTri = true;
					float w[9];
					for ( int k = 0; k < 3; k++ ) {
						const size_t vi = size_t( m.tris[t + size_t( k )] );
						okTri = okTri && vi < m.verts.size();
						if ( !okTri )
							break;
						dv[k] = &m.verts[vi];
						for ( int c = 0; c < 3; c++ )
							w[k * 3 + c] = dv[k]->pos[c] + origin[c];
					}
					if ( !okTri )
						continue;
					double sum[3] = { 0, 0, 0 }, sa = 0;
					int got = 0;
					for ( int i = 0; i < 4; i++ )
						for ( int j = 0; i + j < 4; j++ ) {
							const float b1 = ( float( i ) + 1.0f / 3.0f ) / 4.0f, b2 = ( float( j ) + 1.0f / 3.0f ) / 4.0f;
							const float bw[3] = { 1.0f - b1 - b2, b1, b2 };
							float uv[2], vc[3] = { 1, 1, 1 }, lin[3], ta = 1.0f;
							for ( int k = 0; k < 2; k++ )
								uv[k] = bw[0] * dv[0]->uv[k] + bw[1] * dv[1]->uv[k] + bw[2] * dv[2]->uv[k];
							const float va = bw[0] * dv[0]->alpha + bw[1] * dv[1]->alpha + bw[2] * dv[2]->alpha;
							if ( !probeAlb.sample( m.diffuse, uv[0], uv[1], vc, lin, &ta ) )
								continue;
							const double a = double( std::clamp( ta * va, 0.0f, 1.0f ) );
							for ( int k = 0; k < 3; k++ )
								sum[k] += lin[k] * a;
							sa += a;
							got++;
						}
					if ( !got || !( sa > 0 ) )
						continue;
					quint8 q[4];
					for ( int k = 0; k < 3; k++ )
						q[k] = quint8( std::lround( std::clamp( sum[k] / sa, 0.0, 1.0 ) * 255.0 ) );
					q[3] = quint8( std::lround( std::clamp( sa / got, 0.0, 1.0 ) * 255.0 ) );
					probeSoup.decal.insert( probeSoup.decal.end(), w, w + 9 );
					probeSoup.decalA.insert( probeSoup.decalA.end(), q, q + 4 );
					soupPlacedDecalTris++;
				}
			}
		}
		for ( size_t i = 0; i < decalRefs.size(); i++ ) {
			const CellDecalFate f = decalResult.fates[i];
			if ( f == CellDecalFate::NoDecalData )
				skippedByType[QStringLiteral( "TXST" )]++;   // a texture set that is not a decal: as before
			cellRefTableMutable().setFate( decalRefs[i].refRow, f == CellDecalFate::Drawn ? CellRefFate::Projected
				: ( f == CellDecalFate::NoDecalData ? CellRefFate::NoModel : CellRefFate::Refused ) );
		}
	}

	// ---- lane PRTPPLACE: place the probes, write them, draw them
	QString probeNotes;
	QString giBakeDir;      // lane PRTPGI: the folder the bake just wrote, relit once the lights are read
	if ( probing ) {
		if ( !spec.interior ) {
			// the ground is part of what a column ray meets, painted or not
			for ( int y = y0; y <= y1; y++ )
				for ( int x = x0; x <= x1; x++ ) {
					EsmLand l;
					if ( !world.land( x, y, l ) )
						continue;
					const float ox = float( x ) * CELL_UNITS, oy = float( y ) * CELL_UNITS;
					const float st = CELL_UNITS / float( LAND_GRID - 1 );
					for ( int r = 0; r + 1 < LAND_GRID; r++ )
						for ( int c = 0; c + 1 < LAND_GRID; c++ ) {
							const float a[3] = { ox + c * st, oy + r * st, l.heights[r][c] };
							const float b[3] = { ox + ( c + 1 ) * st, oy + r * st, l.heights[r][c + 1] };
							const float cc[3] = { ox + ( c + 1 ) * st, oy + ( r + 1 ) * st, l.heights[r + 1][c + 1] };
							const float d[3] = { ox + c * st, oy + ( r + 1 ) * st, l.heights[r + 1][c] };
							if ( baking ) {
								/* the splat's composite; without one, a stated dirt tone (sRGB 110, 100, 85)
								 * times the cell's VCLR */
								const qint64 gx = qint64( std::lround( a[0] / 128.0f ) ), gy = qint64( std::lround( a[1] / 128.0f ) );
								const auto it = landAlb.constFind( ( gx << 32 ) ^ ( gy & 0xffffffffll ) );
								float g[3] = { 110.0f / 255.0f, 100.0f / 255.0f, 85.0f / 255.0f };
								if ( it != landAlb.constEnd() ) {
									for ( int k = 0; k < 3; k++ )
										g[k] = ( *it )[size_t( k )];
									albLandSplat++;
								} else {
									if ( l.hasColors )
										for ( int k = 0; k < 3; k++ )
											g[k] *= ( l.colors[r][c][k] + l.colors[r][c + 1][k] + l.colors[r + 1][c + 1][k]
												+ l.colors[r + 1][c][k] ) / ( 4.0f * 255.0f );
									albLandFlat++;
								}
								quint8 rgb[3];
								for ( int k = 0; k < 3; k++ )
									rgb[k] = quint8( std::lround( ProbeAlbedo::srgbToLinear( g[k] ) * 255.0f ) );
								probeSoup.addTri( a, b, cc, rgb );
								probeSoup.addTri( a, cc, d, rgb );
							} else {
								probeSoup.addTri( a, b, cc );
								probeSoup.addTri( a, cc, d );
							}
						}
				}
		}
		ProbePlaceSpec ps;
		if ( spec.interior ) {
			float mn[2] = { 3.4e38f, 3.4e38f }, mx[2] = { -3.4e38f, -3.4e38f };
			for ( size_t i = 0; i < probeSoup.tris.size(); i += 3 )
				for ( int k = 0; k < 2; k++ ) {
					mn[k] = qMin( mn[k], probeSoup.tris[i + size_t( k )] );
					mx[k] = qMax( mx[k], probeSoup.tris[i + size_t( k )] );
				}
			ps.minX = mn[0];
			ps.minY = mn[1];
			ps.maxX = mx[0];
			ps.maxY = mx[1];
		} else {
			// the probed block: the middle N x N of what is loaded (default the center cell)
			// (the PRTP band's Place, with no variable set: the whole loaded block)
			int pn = qEnvironmentVariableIntValue( "WW_CELL_PROBES_N" );
			pn = qBound( 1, pn > 0 ? pn : ( spec.probes ? specAsked.n : 1 ), qMax( 1, spec.n ) );
			const int ph = ( pn - 1 ) / 2;
			ps.minX = float( spec.cx - ph ) * CELL_UNITS;
			ps.maxX = float( spec.cx + ph + 1 ) * CELL_UNITS;
			ps.minY = float( spec.cy - ph ) * CELL_UNITS;
			ps.maxY = float( spec.cy + ph + 1 ) * CELL_UNITS;
		}
		const QByteArray sp = qgetenv( "WW_CELL_PROBES_SPACING" );
		if ( !sp.isEmpty() && sp.toFloat() > 1.0f )
			ps.spacing = sp.toFloat();
		ps.red = QString::fromLatin1( qgetenv( "WW_PROBE_RED" ) );
		if ( roomclampPin && ps.red.isEmpty() )
			ps.red = QStringLiteral( "floor" );   // lane ROOMCLAMP1 pin: the old floors
		ProbePlaceResult pr;
		const bool placed = probePlace( probeSoup, ps, &pr );
		// lane BAKEBLOCK1: past the loaded block, out to the bake's ray reach, the LOD files' far soup
		// (after the placing, which the loaded block decides; before the dump, so the gates see it)
		const QString farLine = bakeExterior && placed ? cellBakeFarSoup( spec, dataRoot, blockRed, probeSoup ) : QString();
		const QByteArray soupDump = qgetenv( "WW_CELL_PROBE_SOUP" );
		QString perr;
		if ( !soupDump.isEmpty() && !probeSoupWrite( QString::fromLocal8Bit( soupDump ), probeSoup, &perr ) )
			qWarning() << "WW_CELL_PROBE_SOUP:" << perr;
		if ( placed && !probeOut.isEmpty() && !probeWriteTsv( probeOut, ps, pr, &perr ) )
			qWarning() << "WW_CELL_PROBES:" << perr;
		{
			QTextStream t( &probeNotes );
			t << "  probe rect " << ps.minX << "," << ps.minY << " .. " << ps.maxX << "," << ps.maxY
			  << ", spacing " << ps.spacing << ( ps.red.isEmpty() ? QString() : QStringLiteral( ", RED " ) + ps.red )
			  << "\n";
			if ( bakeExterior )   // lane BAKEBLOCK1
				t << "  bake block " << spec.n << "x" << spec.n << " (asked " << specAsked.n << "x" << specAsked.n
				  << ( blockRed.isEmpty() ? QString() : QStringLiteral( ", RED " ) + QString::fromLatin1( blockRed ) )
				  << "); " << farLine << "\n";
			t << "  probe soup refs " << soupRefs << ", doors " << int( probeSoup.doors.size() )
			  << ", load doors shut " << ( loadDoorRed ? 0 : soupLoadDoors ) << " of " << soupLoadDoors
			  << ( loadDoorRed ? " RED open" : "" )
			  << ", shapes left out (effect, glass, decal, leaves) " << soupShapesDropped << ", refs left out by type";
			QStringList sk = soupSkippedTypes.keys();
			std::sort( sk.begin(), sk.end() );
			for ( const QString & k : sk )
				t << " " << k << " " << soupSkippedTypes.value( k );
			t << "\n";
			// lane CAPTURE1: refraction-only shapes left out of the soup, per model
			t << "  probe soup refraction-only shapes left out " << soupRefractShapes << " (" << soupRefractTris
			  << " triangles)" << ( refractKeepRed ? " RED keep: kept as surfaces" : "" );
			for ( auto it = soupRefractModels.cbegin(); it != soupRefractModels.cend(); ++it )
				t << "; " << it.key() << " x" << it.value();
			t << "\n";
			if ( bakeLean )   // lane SPEED1
				t << "  headless bake: " << leanSkipped << " placements the soup leaves out were not loaded\n";
			if ( baking ) {   // lane BAKE4
				t << "  bake glass: " << soupGlassShapes << " panes, " << int( probeSoup.glassT.size() / 3 )
				  << " triangles\n";
				if ( decalFold )   // lane GICAL1 (the pin WW_CELL_BAKE_DECALS=0 prints nothing new)
					t << "  bake decals: folded into the albedo, "
					  << soupDecalShapes << " shapes and " << soupPlacedDecals << " placed decals (" << soupPlacedDecalTris << " of their triangles), " << int( probeSoup.decalA.size() / 4 ) << " triangles\n";	// lane GICAL1
				QFile gf( QString::fromLocal8Bit( glassDump ) );
				if ( !glassDump.isEmpty() && gf.open( QIODevice::WriteOnly ) )
					gf.write( glassCensus.toUtf8() );
				QFile rf( QString::fromLocal8Bit( soupRefDump ) );
				if ( !soupRefDump.isEmpty() && rf.open( QIODevice::WriteOnly ) )
					rf.write( soupRefList.toUtf8() );
			}
			if ( !placed )
				t << "  probes REFUSED: " << pr.error << "\n";
			else
				for ( const QString & line : probeCensusText( pr ).split( '\n', Qt::SkipEmptyParts ) )
					t << "  " << line << "\n";
			/* lane PRTPBAKE: the bake, into FO4CS's .tbk sector files. The folder is never the
			 * game's own (Documents/My Games/Fallout4/F4SE/TransportBake): copying there is a
			 * choice the user makes. */
			// lane PRTPGI: WW_CELL_GI_FROM=<bake folder> relights a bake already on disk (no new bake)
			if ( !baking && placed && !qgetenv( "WW_CELL_GI_FROM" ).isEmpty() )
				giBakeDir = QDir::cleanPath( QString::fromLocal8Bit( qgetenv( "WW_CELL_GI_FROM" ) ) );
			if ( baking && placed ) {
				QString dir =!bakeEnv.isEmpty() ? bakeEnv : !bakeDirEnv.isEmpty() ? bakeDirEnv : spec.bakeDir;
				if ( dir.isEmpty() ) {
					QString name = spec.interior ? spec.interiorCell : spec.world;
					name.replace( QRegularExpression( QStringLiteral( "[^A-Za-z0-9_+-]" ) ), QStringLiteral( "_" ) );
					dir = QCoreApplication::applicationDirPath() + QStringLiteral( "/prtp_bake/" )
						+ ( name.isEmpty() ? QStringLiteral( "cell" ) : name );
				}
				ProbeBakeSpec bs;
				const QByteArray br = qgetenv( "WW_CELL_PROBE_BAKE_RAYS" );
				if ( br.toInt() > 0 )
					bs.rays = br.toInt();
				bs.red = QString::fromLatin1( qgetenv( "WW_PROBE_BAKE_RED" ) );
				if ( roomclampPin && bs.red.isEmpty() )
					bs.red = QStringLiteral( "backface" );   // lane ROOMCLAMP1 pin: no probe moved
				// lane ROOMCLAMP1: the outside-the-shell threshold (gate red: 1 = off) and the gate's list
				if ( qEnvironmentVariableIsSet( "WW_CELL_PROBE_BACKMAX" ) )
					bs.backMax = qEnvironmentVariable( "WW_CELL_PROBE_BACKMAX" ).toFloat();
				bs.backDump = qEnvironmentVariable( "WW_CELL_PROBE_BACKDUMP" );
				// an interior's misses are void, never sky -- unless its cell shows the sky (lane SKYINT1, the deck's
				// rule: a ray that meets nothing sees the sky)
				bs.noSky = spec.interior && !( cellInteriorFlags( world.interior() ) & 0x0080u );
				if ( wantMat ) {   // lane CAPTURE1: the albedo way, its maps loaded once (the bake reads them from threads)
					bs.albedoWay = albWay;
					bs.albedoRed = albRed;
					const int cf = qEnvironmentVariableIntValue( "WW_CELL_BAKE_CUBE_FACE" );
					if ( cf > 0 )
						bs.cubeFace = cf;
					probeSoup.mat.resize( size_t( probeSoup.triCount() ) );   // the ground: its tri value
					probeSoup.matTexPtr.clear();
					for ( const QString & tx : probeSoup.matTex )
						probeSoup.matTexPtr.push_back( probeAlb.loadFine( tx ) );
					bs.cubeDump = qEnvironmentVariable( "WW_CELL_BAKE_CUBE_DUMP" );
					for ( const QString & q : qEnvironmentVariable( "WW_CELL_BAKE_CUBE_PROBES" ).split( ';', Qt::SkipEmptyParts ) ) {
						const QStringList c = q.split( ',' );
						if ( c.size() != 3 )
							continue;
						int best = -1;
						double bd = 1e300;
						for ( size_t i = 0; i < pr.probes.size(); i++ ) {
							double dd = 0;
							for ( int k = 0; k < 3; k++ )
								dd += std::pow( double( pr.probes[i].pos[k] ) - c[k].toDouble(), 2 );
							if ( dd < bd ) {
								bd = dd;
								best = int( i );
							}
						}
						if ( best >= 0 )
							bs.cubeDumpProbes.push_back( best );
					}
					t << "  bake albedo way " << albWay << ( albRed.isEmpty() ? QString() : QStringLiteral( " RED " ) + albRed )
					  << ": " << probeSoup.matTex.size() << " maps, " << probeAlb.finesRead << " read at up to 512 texels; "
					  << probeSoup.cubeExtra.size() / 9 << " left-out triangles for the cube\n";
				}
				ProbeBakeResult bres;
				t << "  bake albedo: object triangles from their map " << albTextured << " (of them through a paint palette "
				  << albPalette << "), grey (no map read) "
				  << albUntextured << ", ground quads from the splat " << albLandSplat << ", flat tone x VCLR "
				  << albLandFlat << ", maps read " << probeAlb.texturesRead << ", missing " << probeAlb.texturesMissing
				  << "\n";
				// lane BAKE4: the placer's room boxes go into the `.tbk` v4 files
				if ( !probeBake( probeSoup, pr.probes, bs, QDir::cleanPath( dir ), &bres, &pr.roomBoxes ) ) {
					t << "  bake REFUSED: " << bres.error << "\n";
				} else {
					for ( const QString & line : probeBakeCensusText( bres ).split( '\n', Qt::SkipEmptyParts ) )
						t << "  " << line << "\n";
					t << "  bake folder " << QDir::toNativeSeparators( QDir::cleanPath( dir ) ) << "\n";
					giBakeDir = QDir::cleanPath( dir );
				}
			}
		}
		// the markers: one small box each, colored by class, split under the 16-bit index limit
		int bucketNo = 0;
		Bucket pb;
		pb.name = QStringLiteral( "probes" );
		pb.withColour = true;
		auto flush = [&]() {
			if ( pb.verts.empty() )
				return;
			buckets.insert( QStringLiteral( "\x01probes%1" ).arg( bucketNo++, 2, 10, QLatin1Char( '0' ) ), pb );
			pb.verts.clear();
			pb.tris.clear();
		};
		auto box = [&]( const Vector3 & c, float h, const float rgb[3] ) {
			if ( pb.verts.size() + 24 > 60000 )
				flush();
			const Vector3 X( h, 0, 0 ), Y( 0, h, 0 ), Z( 0, 0, h );
			appendQuad( pb, c - X - Y + Z, c + X - Y + Z, c + X + Y + Z, c - X + Y + Z, Vector3( 0, 0, 1 ), rgb );
			appendQuad( pb, c - X + Y - Z, c + X + Y - Z, c + X - Y - Z, c - X - Y - Z, Vector3( 0, 0, -1 ), rgb );
			appendQuad( pb, c + X - Y - Z, c + X + Y - Z, c + X + Y + Z, c + X - Y + Z, Vector3( 1, 0, 0 ), rgb );
			appendQuad( pb, c - X + Y - Z, c - X - Y - Z, c - X - Y + Z, c - X + Y + Z, Vector3( -1, 0, 0 ), rgb );
			appendQuad( pb, c + X + Y - Z, c - X + Y - Z, c - X + Y + Z, c + X + Y + Z, Vector3( 0, 1, 0 ), rgb );
			appendQuad( pb, c - X - Y - Z, c + X - Y - Z, c + X - Y + Z, c - X - Y + Z, Vector3( 0, -1, 0 ), rgb );
		};
		for ( const ProbePoint & q : pr.probes ) {
			if ( !spec.probesShow )
				break;      // placed and counted, not drawn (the PRTP band's Show probes off)
			const float * rgb = g_probeKinds[cellProbeKindOf( q )].rgb;
			const Vector3 c( q.pos[0] - origin[0], q.pos[1] - origin[1], q.pos[2] - origin[2] );
			if ( q.cls == ProbeClass::Aperture ) {
				box( c, 16.0f, rgb );
				// a smaller box on the open side shows which way the opening faces
				box( c + Vector3( q.nrm[0], q.nrm[1], q.nrm[2] ) * 36.0f, 7.0f, rgb );
			} else {
				box( c, q.cls == ProbeClass::Room ? 14.0f : 10.0f, rgb );
			}
		}
		flush();
	}

	// ---- emit
	CellSpeed::mark( "ground, water, lights, probes" );
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
	// lane PRTPPLACE: how many buckets needed the 32-bit BucketTri (more than 16-bit indices hold)
	int bucketsWide = 0;
	size_t bucketMaxVerts = 0;
	for ( const QString & k : keys ) {
		const size_t nvb = buckets.value( k ).verts.size();
		bucketsWide += nvb > 65536 ? 1 : 0;
		bucketMaxVerts = qMax( bucketMaxVerts, nvb );
	}
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
		const Bucket & bk = buckets[k];
		if ( bk.billboard ) {   // lane GLOW1: its own node at the card's pivot, no rotation
			QModelIndex iB = nif->insertNiBlock( QStringLiteral( "NiBillboardNode" ) );
			nif->set<QString>( iB, "Name", QStringLiteral( "billboard " ) + bk.name );
			nif->set<quint32>( iB, "Flags", 14 );
			nif->set<Vector3>( iB, "Translation", bk.bbPos );
			nif->set<float>( iB, "Scale", bk.bbScale );
			nif->set<int>( iB, "Billboard Mode", bk.bbMode );
			addLink( nif, iRoot, QStringLiteral( "Children" ), nif->getBlockNumber( iB ) );
			if ( !emitBucket( nif, iB, bk, Vector3( 0.0f, 0.0f, 0.0f ), shapes, verts, tris, error ) ) {
				ok = false;
				break;
			}
			continue;
		}
		if ( !emitBucket( nif, iParent, bk, origin, shapes, verts, tris, error ) ) {
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
	CellSpeed::mark( "welded shapes written to the document" );

	nif->holdUpdates( false );
	nif->updateModel();
	CellSpeed::mark( "document updateModel" );

	// lane PRTP3: the renderer lights this document with the cell's own lights (the Cell lights row)
	cellPublishLighting( nif, world, spec, lightRefs, primRefs, origin );
	wwCellFxLitPick( nif );   // lane FXLIT1: each lit effect model's four placed lights

	/* lane PRTPGI: the bake just written, relit by those lights (src/probegi.h), for the GI row.
	 * WW_CELL_GI_DUMP=<folder> writes the gate's copies; WW_CELL_GI_RED=<red> its refuters. */
	if ( !giBakeDir.isEmpty() ) {
		if ( const WwCellLighting * L = wwCellLightsFor( nif ) ) {
			ProbeGiSpec gs;
			gs.red = QString::fromLatin1( qgetenv( "WW_CELL_GI_RED" ) ).trimmed();
			if ( gs.red.isEmpty() && qgetenv( "WW_CELL_ROOMCLAMP_PIN" ).trimmed() == "off" )
				gs.red = QStringLiteral( "prelane" );   // lane ROOMCLAMP1 pin: no rooms, no eye fallbacks
			gs.passes = qEnvironmentVariableIntValue( "WW_CELL_GI_PASSES" );   // lane BOUNCE2: a gate's pin (1 = one bounce)
			gs.rooms.red = QString::fromLatin1( qgetenv( "WW_CELL_ROOMS_RED" ) ).trimmed();   // lane ROOMCLAMP1: the rooms' refuters
			// lane SKY1: outdoors, the weather's sky and sun (src/probesky.h); WW_CELL_SKY_RED its refuters
			// lane SKYINT1: and an interior whose cell shows the sky (the sun where Use Sky Lighting + Sunlight Shadows)
			const quint16 cf = spec.interior ? cellInteriorFlags( world.interior() ) : quint16( 0 );
			gs.interiorSky = ( cf & 0x0080u ) != 0;
			gs.interiorSun = gs.interiorSky && ( cf & 0x0900u ) == 0x0900u;
			if ( !spec.interior || gs.interiorSky ) {
				gs.sky = probeSkyLightNow();
				gs.skyRed = QString::fromLatin1( qgetenv( "WW_CELL_SKY_RED" ) ).trimmed();
				if ( !gs.sky.on )
					probeNotes += QStringLiteral( "  gi sky: none (the view is not weather-lit; Scene mode Lookdev lights it with the weather)\n" );
			}
			ProbeGiResult gr;
			const bool ok = probeGiRelight( probeSoup, giBakeDir, *L, gs, &gr );
			probeNotes += QStringLiteral( "  %1\n" ).arg( probeGiCensusText( gr ) );
			if ( ok ) {
				WwCellGi gi;
				for ( int k = 0; k < 3; k++ ) {
					gi.origin[k] = gr.origin[k];
					gi.dims[k] = gr.dims[k];
				}
				gi.voxel = gr.voxel;
				gi.summary = QStringLiteral( "grid %1x%2x%3 voxel %4" ).arg( gr.dims[0] ).arg( gr.dims[1] ).arg( gr.dims[2] )
					.arg( double( gr.voxel ), 0, 'f', 1 );
				const QString dump = QString::fromLocal8Bit( qgetenv( "WW_CELL_GI_DUMP" ) );
				QString derr;
				if ( !dump.isEmpty() && !probeGiDump( gr, gs, dump, &derr ) )
					probeNotes += QStringLiteral( "  gi dump FAILED: %1\n" ).arg( derr );
				gi.rgba = std::move( gr.grid );
				// lane SKY1; lane SKYINT1: an interior keeps its own ambient, so its grid never stands in for the weather's
				gi.skyLit = gr.skyLit && !spec.interior && gs.skyRed != QLatin1String( "keepamb" );
				gi.sky = std::move( gr.gridSky );	// lane PROBEVIEW1: the Pass drop-down's Sky visibility
				probeGiRoomsInto( gr, gi );   // lane ROOMCLAMP1: the second slots and the rooms
				wwCellGiPublish( nif, gi );
				if ( !spec.interior || gs.interiorSky )
					probeSkyKeep( nif, probeSoup, giBakeDir, gs );   // a later change of weather relights it
				WwCellProbeView pv;	// lane PROBEVIEW1: the surfel and probe previews
				pv.surfelCell = gr.surfelCell;
				pv.probesShown = spec.probesShow;
				pv.surfels = std::move( gr.surfelOut );
				pv.probes = std::move( gr.probeCube );
				pv.probeSky = std::move( gr.probeSky );
				pv.linkStart = std::move( gr.probeLinkStart );
				pv.links = std::move( gr.probeLinks );
				wwCellProbeViewPublish( nif, pv );
			}
		}
	}

	// ---- the census
	if ( notes ) {
		QString n;
		QTextStream s( &n );
		if ( spec.interior ) {
			const EsmInteriorCell & ic = world.interior();
			s << "cell view interior " << ic.edid << " form 0x"
			  << QString::number( ic.cellForm, 16 ).rightJustified( 8, '0' )
			  << " (no LAND, no grid; XCLL " << ( ic.xcll.isEmpty() ? QStringLiteral( "absent" )
			     : QStringLiteral( "%1 bytes" ).arg( ic.xcll.size() ) )
			  << ", lighting template 0x" << QString::number( ic.lightingTemplate, 16 ).rightJustified( 8, '0' )
			  << ", inherits 0x" << QString::number( ic.inherits, 16 ) << ")\n";
			const quint16 cf = cellInteriorFlags( ic );   // lane SKYINT1
			s << "  cell flags 0x" << QString::number( ic.flags, 16 ).rightJustified( 4, '0' ) << ": Show Sky "
			  << ( ( cf & 0x0080u ) ? "yes" : "no" ) << ", Use Sky Lighting " << ( ( cf & 0x0100u ) ? "yes" : "no" )
			  << ", Sunlight Shadows " << ( ( cf & 0x0800u ) ? "yes" : "no" )
			  << ( cf != ic.flags ? QStringLiteral( " (RED %1)" ).arg( QString::fromLatin1( qgetenv( "WW_CELL_SKYINT_RED" ) ) ) : QString() )
			  << "\n";
		} else {
			s << "cell view " << spec.world << " " << spec.cx << "," << spec.cy
			  << " block " << spec.n << "x" << spec.n
			  << " (cells " << x0 << "," << y0 << " .. " << x1 << "," << y1 << ")\n";
		}
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
		s << "  refrs read " << refsRead << ", placements " << placements.size() - actorPlacements
		  << ", drawn " << drawn << "\n";
		s << "  hidden: disabled " << refsHidden << ", markers " << refsMarker
		  << ", deleted " << refsDeleted << ", no base " << refsNoBase << "\n";
		{
			// lane PRTP1: the lights census -- read, not yet lit (PRTP3 lights them)
			QMap<QString, int> byType;
			int withXrds = 0, withXlig = 0, off = 0;
			for ( const EsmRefr & r : lightRefs ) {
				byType[world.light( r.base ).typeName()]++;
				withXrds += r.hasRadius ? 1 : 0;
				withXlig += r.xligCount > 0 ? 1 : 0;
				off += r.initiallyDisabled ? 1 : 0;
			}
			QStringList bt;
			for ( auto it = byType.constBegin(); it != byType.constEnd(); ++it )
				bt.append( QStringLiteral( "%1 %2" ).arg( it.key() ).arg( it.value() ) );
			s << "  lights: " << lightRefs.size() << " placed (" << bt.join( QLatin1String( ", " ) )
			  << "); XRDS " << withXrds << ", XLIG " << withXlig << ", initially disabled " << off << "\n";
			if ( const WwCellLighting * cl = wwCellLightsFor( nif ) )
				s << "  cell lighting: " << cl->summary << "\n";
		}
		if ( !skippedByType.isEmpty() ) {
			QStringList sk;
			QStringList st = skippedByType.keys();
			std::sort( st.begin(), st.end() );
			for ( const QString & t : st )
				sk.append( QStringLiteral( "%1 %2" ).arg( t ).arg( skippedByType.value( t ) ) );
			s << "  skipped, no model on the base: " << sk.join( QLatin1String( ", " ) ) << "\n";
		}
		s << cellDecalCensusLine( decalResult );   // lane PLACED1
		s << actors.censusLine();
		actors.dump( QString::fromLocal8Bit( qgetenv( "WW_CELL_ACTOR_DUMP" ) ) );
		s << "  buckets over 65,536 vertices " << bucketsWide << " (largest " << qulonglong( bucketMaxVerts )
		  << "), shapes drawn with their own vertex colors " << shapesVertexColor
		  << ", placements drawn with a material swap " << placementsSwapped
		  << ", shapes repainted by a CNAM " << shapesRepainted
		  << ", CNAMs on a material with no palette " << cnamNoPalette
		  << ", shapes painted by their base's MODC " << modcShapes
		  << ", sky cards hidden " << skyCardsHidden << "\n";
		s << "  distinct models loaded " << modelLoads << ", failed to load "
		  << modelsFailed.size() << "\n";
		if ( !modelsFailed.isEmpty() ) {   // lane PRTP5: their names, for the cell census
			QStringList mf( modelsFailed.cbegin(), modelsFailed.cend() );
			std::sort( mf.begin(), mf.end() );
			s << "  models failed: " << mf.mid( 0, 12 ).join( QLatin1String( ", " ) ) << "\n";
		}
		s << "  source triangles " << srcTris << ", welded shapes " << shapes
		  << ", vertices " << verts << ", triangles " << tris << "\n";
		/* THE MATERIAL LINE (lane CELLVIEW3). Magenta in this viewer means one
		 * thing and one thing only: a texture path the renderer went looking
		 * for and did not find. Every other way a surface can fail is stated
		 * here as a number instead of being painted pink. */
		s << "  materials: " << shapesFromEffectMat
		  << " shapes textured from a `.bgem` effect material, "
		  << shapesUnreadableMat << " drawn neutral grey because a named material "
		     "resolved to nothing, " << blendBuckets << " blended (glass) buckets, "
		  << effectBuckets << " drawn by the effect shader (BGEM), "
		  << inlineFxBuckets << " by the effect shader as the NIF sets it (lane EFX1), "
		  << refractBuckets << " refraction buckets (lane EFX1)";
		if ( !unreadableMatNames.isEmpty() )
			s << " (" << unreadableMatNames.join( QLatin1String( ", " ) ) << ")";
		s << "\n";
		// lane FXREST1: editor-only shapes the loader left out (per distinct model loaded)
		s << "  editor markers left out: " << lodgenEditorMarkerCount( -1 ) << " shapes named EditorMarker*, "
		  << lodgenEditorMarkerCount( -2 ) << " with the word inside the name, in "
		  << lodgenEditorMarkerInsideModels().size() << " models ("
		  << lodgenEditorMarkerInsideModels().join( QLatin1String( "; " ) ) << ")\n";
		// lane GLOW1
		s << "  billboards: " << billboardShapes << " shapes turned to the camera, "
		  << billboardFlat << " welded flat" << ( glowRed ? " (WW_CELL_GLOW_RED)" : "" ) << "\n";
		// lane FXLIT1
		s << "  lit effects: " << fxLitShapes << " shapes of " << fxLitModels
		  << " placed models take their four placed lights\n";
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
		s << probeNotes;   // lane PRTPPLACE, empty unless WW_CELL_PROBES
		s << "  built in " << clock.elapsed() << " ms\n";
#ifndef ESM_HAS_CELL_FIELDS
		s << "  NOTE: built without the esmdata cell fields (XLYR, XESP, editor ids and "
		     "the MODL of MSTT/FURN/CONT/DOOR/ACTI/FLOR/LIGH). Those records are counted "
		     "as skipped above rather than drawn.\n";
#endif
		*notes = n;
	}
	if ( CellSpeed::on() ) {   // lane SPEED1: what was shared, in numbers
		CellSpeed::count( "placements", placements.size() );
		CellSpeed::count( "placements drawn", drawn );
		CellSpeed::count( "model loads", modelLoads );
		CellSpeed::count( "models failed", modelsFailed.size() );
		CellSpeed::count( "shapes written", shapes );
		CellSpeed::count( "vertices written", verts );
		CellSpeed::count( "triangles written", tris );
		CellSpeed::count( "document blocks", nif->getBlockCount() );
		CellSpeed::mark( "lighting, GI, census" );
	}
	cellMeshSaveIfAsked( nif );   // lane SPEED1: WW_CELL_SPEED_SAVE, the gate's saved-file check

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
				if ( std::memcmp( &e.baseType, "NPC_", 4 ) == 0 || std::memcmp( &e.baseType, "LVLN", 4 ) == 0 )
					continue;   // lane PLACED1: a placed actor has its own dump (WW_CELL_ACTOR_DUMP)
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

	/* WW_CELL_LIGHTS (lane PRTP1): one row per placed light, the columns the
	 * independent walk (scratchpad/prtp1_20260930/light_census.py) writes, so the
	 * gate is a diff. Sorted by ref form. */
	const QByteArray lightDump = qgetenv( "WW_CELL_LIGHTS" );
	if ( !lightDump.isEmpty() ) {
		QFile f( QString::fromLocal8Bit( lightDump ) );
		if ( f.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
			QTextStream s( &f );
			s << "ref\tbase\tbaseEdid\tx\ty\tz\tradius\txrds\txlig\tinitDisabled\ttype\tcolor\n";
			QVector<EsmRefr> sorted = lightRefs;
			std::sort( sorted.begin(), sorted.end(),
				[]( const EsmRefr & a, const EsmRefr & b ) { return a.formID < b.formID; } );
			auto hex8 = []( quint32 v ) { return QString::number( v, 16 ).toUpper().rightJustified( 8, '0' ); };
			for ( const EsmRefr & r : sorted ) {
				const EsmLight & L = world.light( r.base );
				s << hex8( r.formID ) << "\t" << hex8( r.base ) << "\t" << L.edid << "\t"
				  << QString::number( r.pos[0], 'f', 1 ) << "\t" << QString::number( r.pos[1], 'f', 1 ) << "\t"
				  << QString::number( r.pos[2], 'f', 1 ) << "\t" << L.radius << "\t"
				  << ( r.hasRadius ? QString::number( r.radius, 'f', 1 ) : QString() ) << "\t"
				  << ( r.xligCount > 0 ? 1 : 0 ) << "\t" << ( r.initiallyDisabled ? 1 : 0 ) << "\t"
				  << L.typeName() << "\t" << L.color[0] << "," << L.color[1] << "," << L.color[2] << "\n";
			}
		} else {
			qWarning() << "WW_CELL_LIGHTS: could not write" << lightDump;
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

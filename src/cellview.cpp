/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellview.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store
#include "probeincr.h"
#include <QCryptographicHash>

#include "cellpick.h"
#include "cellrefs.h"
#include "cellclick.h"		// lane CELLVIEW2
#include "cellground.h"		// lane CELLVIEW2
#include "cellsplat.h"		// lane CELLVIEW4
#include "landlodspec.h"	// lane LANDSPEC6: fixedWorld
#include "cellidentity.h"	// lane CELLVIEW2
#include "cellactor.h"		// lane PLACED1
#include "gl/celllooks.h"	// lane GAPLOOK2: Actors outdoors, Weapon mod parts
#include "cellworldanim.h"		// lane FXPARITY2: Animated objects
#include "cellweapon.h"		// lane MISS1 (GAPLOOK2)
#include "cellvertexalpha.h"	// lane TERRAINBLOCK1
#include "cellcable.h"		// lane CABLES1
#include "celldecal.h"		// lane PLACED1
#include "celldecalangle.h"	// lane DECALANGLE2
#include "esmdata.h"
#include "esmfile.hpp"		// lane LEFTOVERS1: light-plugin form IDs back to their file
#include "esmweather.h"		// lane FOG2: the fog packing
#include "lodgen.h"
#include "nativeemit.h"
#include "probeplace.h"		// lane PRTPPLACE
#include "probebake.h"		// lane PRTPBAKE
#include "pgiwrite.h"		// lane PGI0
#include "probegi.h"		// lane PRTPGI
#include "probealbedo.h"		// lane PRTPBAKE
#include "probefar.h"		// lane BAKEBLOCK1: the far soup beyond the loaded block
#include "gl/cellpasstime.h"	// lane PERF1: private bytes at each stage
#include "proberelight.h"	// lane GPURELIGHT1: the relight from recorded operators (CPU + GPU)
#include "celldoorlive.h"	// lane DOORLIVE1: one live door
#include "farlight.h"		// lane FARVIEW1: distant light from the surfels
#include "cellmodelahead.h"	// lane SPEED1: models parsed on worker threads
#include "cellmesh.h"		// lane SPEED1: the welded geometry beside the document
#include "cellfarlod.h"		// lane FARLOD1: the far field around the block
#include "cellmodpick.h"	// lane MODPICK1
#include "cellspeed.h"		// lane SPEED1: stage timers (WW_CELL_SPEED_DUMP)
#include "cellaodecal.h"		// lane AODECAL1: baked AO decals under big movable statics
#include "cellstream.h"		// lane CELLSTREAM1: the cells round the camera
#include "cellspawn.h"		// lane CELLSPAWN1: where the camera starts

#include <limits>

#include "model/nifmodel.h"
#include "spells/blocks.h"
#include "gl/celllights.h"
#include "io/nifxfile.h"	// lane NIFXLIGHT1: a model's .nifx path
#include "gl/cellnifxlights.h"	// lane NIFXLIGHT3: the .nifx path rule, kept live by src/gl/celllights.cpp
#include "gl/sunshadow.h"	// lane LIVESET1: wwSunShadowForget
#include "gl/cellprobeview.h"	// lane PROBEVIEW1
#include "gl/lookdevstage.h"	// lane SKYTOP1: the cell's load order feeds the weather
#include "gl/cellvolumeview.h"	// lane VOLFIT1
#include "probevolumes.h"	// lane VOLFIT1: the Space stage
#include "probevolumesgi.h"	// lane VOLFIT1: its rooms as the GI's volumes
#include "gl/cellfxlit.h"
#include "gl/celllensflare.h"	// lane LIGHTS3 (LENSFLARE1)
#include "gl/cellaodecalgl.h"	// lane AODECAL1
#include "gl/cellwater.h"	// lane WATER1
#include "gl/cellwaterinfo.h"	// lane WATERDATA1
#include "gl/cellcull.h"	// lane SUNCELL1: the per-placement culling runs
#include "gl/glproperty.h"	// lane FIXPACK1: the renderer's .pbrm resolve (pbrmResolveForMaterial)
#include "gl/cellgrass.h"
#include "gl/cellpost.h"	// lane GRASSMB1
#include "esmwater.h"
#include "gamemanager.h"	// lane IMGS1: the imagespace LUT

#include <QBuffer>
#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QStringList>
#include <QCoreApplication>
#include <QDir>
#include <QRegularExpression>
#include <QSettings>
#include <QTextStream>

#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <cmath>
#include <cstring>
#include <vector>

/* lane VOLFIT1: feeds the GI result's volume set (lane PERF1's ProbeGiResult::volumes) when this build's GI has one;
 * before that merge the second overload is chosen and nothing is fed. */
template <class R>
static auto wwVolumesIntoGi( R & r, const ProbeVolumesGi & g, int ) -> decltype( (void) r.volumes, bool() )
{
	probeVolumesGiFill( g, r.volumes );
	// the room-grid rooms the Space stage did not keep (ids after Space's) keep the stand-in's boxes and openings
	const int nSpace = int( g.rooms.size() );
	const GiVolumeSet st = giVolumesFromRooms( r.rooms );
	for ( const GiVolumeShape & s : st.shapes )
		if ( s.room > nSpace )
			r.volumes.shapes.push_back( s );
	for ( const GiThreshold & t : st.thresholds )
		if ( t.rooms[0] > nSpace || t.rooms[1] > nSpace )
			r.volumes.thresholds.push_back( t );
	return true;
}
template <class R>
static bool wwVolumesIntoGi( R &, const ProbeVolumesGi &, long )
{
	return false;
}

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
	QString groundSpec;         //!< lane DUNESAND2: a ground shape's TX07 even under the nospec red (live set)
	/*! The shape NAMED a material and nothing resolved from it -- no BGSM, no
	 *  BGEM, no texture set. Drawn neutral grey and counted in the census;
	 *  magenta stays reserved for a genuinely missing file (lane CELLVIEW3). */
	bool matUnreadable = false;
	bool hasAlpha = false;
	quint8 alphaThreshold = 128;
	/*! lane TERRBLEND1: a blended-ground pass. Its vertex alpha is the pass's
	 *  SHARE of the engine's weighted sum: the base opaque and scaled by it, a
	 *  layer ADDED at it (SRC_ALPHA, ONE), and the renderer told so by block. */
	bool landSplat = false;
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
	quint32 xemi = 0;   // lane LIGHTS2 (XEMI1): the placement's emittance colour, 0x01RRGGBB (bit 24 = set), 0 = none
	bool decal = false;   // lane PLACED1: a placed decal's pieces (src/celldecal.h): Decal flag, no depth write
	bool decalAngle = false;   // lane DECALANGLE: its vertex alpha is dot(N, D); the shader runs the angle rule
	bool decalMapped = false;  // lane DECALANGLE2: its vertex rgb is D; the rule reads the drawn normal
	quint32 decalSF1 = 0;   // lane GAPFIX1: a lighting source's own Decal / Dynamic_Decal bits (the model's flags), kept
	QString heightTex;      // lane DECALS1: a placed decal's TX04 height map (texture slot 2; no glow on decals)
	float pomScale = 0.0f;  // lane DECALS1: its DODT parallax scale and passes
	int pomPasses = 0;
	/* Lane WATER1: a water surface of one WATR record (the cell's plane or a placed water mesh); its shapes are
	 * registered with src/gl/cellwater.h, which draws them the game's way while the Cell lights row is on. */
	bool water = false;
	WwWaterRecord waterRec;
	bool grass = false;   // lane GRASSMB1: blades of the game's grass (src/gl/cellgrass.h): the fade applies
	float grassWave = 0.0f;   // lane GRASSMB1: its GRAS wave period (one bucket per grass type), the wind's phase
	bool grassNoFade = false;   // lane CABLES1: a swaying cable: the grass wind, never the grass fade
	std::vector<OutVert> verts;
	std::vector<BucketTri> tris;   // 32-bit: a bucket welds far more than 65,536 vertices
	//! Lane SUNCELL1: where each placed shape's triangles start in `tris` (the culling runs, src/gl/cellcull.h)
	std::vector<quint32> runStart;
	//! Lane UMBRA1: the placed reference of each run (parallel to `runStart`; 0 = none)
	std::vector<quint32> runRef;
	//! lane CELLSTREAM1: the shapes' own vertex lists, cut on the worker so the writer only copies them out
	struct Part { std::vector<OutVert> pv; std::vector<Triangle> pt; size_t p0 = 0, p1 = 0; };
	std::vector<Part> parts;
	size_t nVerts = 0, nTris = 0;   //!< the welded counts once `parts` holds them
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

// lane CELLSTREAM1: while a streamed cell is written, its blocks come from the streamer (freed ones first)
static CellBlockSource * g_cellBlockSource = nullptr;
static CellChunk * g_cellChunkEmit = nullptr;
// lane CELLSTREAM1: where one bucket's main-thread time goes (remap, take, runs, shape, mesh, material)
double g_emitSecMs[6] = {};
static QElapsedTimer g_emitLap;
static void emitLap( int k ) { g_emitSecMs[k] += double( g_emitLap.nsecsElapsed() ) * 1.0e-6; g_emitLap.restart(); }
static QModelIndex cellNewBlock( NifModel * nif, const QString & type )
{
	const qint64 t0 = g_emitLap.nsecsElapsed();
	QModelIndex r = g_cellBlockSource ? g_cellBlockSource->take( nif, type ) : nif->insertNiBlock( type );
	g_emitSecMs[1] += double( g_emitLap.nsecsElapsed() - t0 ) * 1.0e-6;
	return r;
}

//! A bucket cut into shapes of at most MAX_SHAPE_VERTS, on TRIANGLE boundaries, so no triangle points across a cut.
static void splitBucket( const Bucket & b, std::vector<Bucket::Part> & out )
{
	size_t first = 0;
	while ( first < b.tris.size() ) {
		Bucket::Part P;
		P.p0 = first;
		QHash<int, quint16> remap;
		size_t i = first;
		for ( ; i < b.tris.size(); i++ ) {
			if ( P.pv.size() + 3 > size_t( MAX_SHAPE_VERTS ) )
				break;
			const BucketTri & t = b.tris[i];
			quint16 idx[3];
			for ( int k = 0; k < 3; k++ ) {
				const int src = int( t[k] );
				auto it = remap.constFind( src );
				if ( it != remap.constEnd() ) {
					idx[k] = it.value();
				} else {
					idx[k] = quint16( P.pv.size() );
					remap.insert( src, idx[k] );
					P.pv.push_back( b.verts[size_t( src )] );
				}
			}
			P.pt.push_back( Triangle( idx[0], idx[1], idx[2] ) );
		}
		first = i;
		P.p1 = i;
		const bool empty = P.pv.empty() || P.pt.empty();
		out.push_back( std::move( P ) );
		if ( empty )
			break;
	}
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
	if ( b.parts.empty() && ( b.verts.empty() || b.tris.empty() ) )
		return true;

	BSVertexDesc desc( CELL_VERTEX_DESC );
	if ( b.withColour ) {
		desc.SetFlag( VertexFlags::VF_COLORS );
		desc.ResetAttributeOffsets( 130 );
	}
	const std::uint64_t vertexDesc = desc.Value();
	const int stride = int( desc.GetVertexSize() );
	for ( double & d : g_emitSecMs )
		d = 0.0;
	g_emitLap.start();

	std::vector<Bucket::Part> local;
	const std::vector<Bucket::Part> * parts = &b.parts;
	if ( b.parts.empty() ) {
		splitBucket( b, local );
		parts = &local;
	}
	emitLap( 0 );
	int part = 0;
	for ( const Bucket::Part & P : *parts ) {
		const std::vector<OutVert> & pv = P.pv;
		const std::vector<Triangle> & pt = P.pt;
		const size_t p0 = P.p0;   // lane SUNCELL1: this part's first bucket triangle
		const size_t i = P.p1;
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

		QModelIndex iShape = cellNewBlock( nif, QStringLiteral( "BSTriShape" ) );
		if ( b.fxLit >= 0 && g_cellChunkEmit )   // lane CELLSTREAM1: a local serial, registered by the streamer
			g_cellChunkEmit->fxShapes.emplace_back( nif->getBlockNumber( iShape ), b.fxLit );
		else if ( b.fxLit >= 0 )   // lane FXLIT1: the renderer asks for this shape's four lights by block
			wwCellFxLitShape( nif, nif->getBlockNumber( iShape ), b.fxLit );
		if ( b.xemi && g_cellChunkEmit )   // lane LIGHTS2 (XEMI1): the renderer asks for the colour by block
			g_cellChunkEmit->xemiShapes.emplace_back( nif->getBlockNumber( iShape ), b.xemi );
		else if ( b.xemi )
			wwCellXemiShape( nif, nif->getBlockNumber( iShape ), b.xemi );
		emitLap( 5 );   // the shape's take (counted in 1) plus the fx registration
		if ( !b.billboard && !b.runStart.empty() ) {   // lane SUNCELL1: the part's culling runs, spheres in its own space
			std::vector<WwCullRun> runs;
			auto rb = std::upper_bound( b.runStart.begin(), b.runStart.end(), quint32( p0 ) );
			size_t s0 = p0;
			while ( s0 < i ) {
				size_t s1 = i;
				if ( rb != b.runStart.end() && size_t( *rb ) < i )
					s1 = size_t( *rb++ );
				if ( s1 <= s0 )
					continue;
				Vector3 rlo( 3.4e38f, 3.4e38f, 3.4e38f ), rhi( -3.4e38f, -3.4e38f, -3.4e38f );
				for ( size_t t = s0 - p0; t < s1 - p0; t++ )
					for ( unsigned int k = 0; k < 3; k++ ) {
						const Vector3 & q = pv[pt[t][k]].pos;
						for ( int a = 0; a < 3; a++ ) {
							rlo[a] = qMin( rlo[a], q[a] );
							rhi[a] = qMax( rhi[a], q[a] );
						}
					}
				WwCullRun r;
				r.first = std::uint32_t( s0 - p0 );
				r.count = std::uint32_t( s1 - s0 );
				{	// lane UMBRA1: the run's placed reference, for the previs tome's visible set
					const auto rr = std::upper_bound( b.runStart.begin(), b.runStart.end(), quint32( s0 ) );
					const size_t ri = size_t( rr - b.runStart.begin() );
					if ( ri > 0 && ri - 1 < b.runRef.size() )
						r.ref = b.runRef[ri - 1];
				}
				r.center = ( rlo + rhi ) / 2.0f;
				for ( size_t t = s0 - p0; t < s1 - p0; t++ )
					for ( unsigned int k = 0; k < 3; k++ )
						r.radius = qMax( r.radius, ( pv[pt[t][k]].pos - r.center ).length() );
				r.radius *= 1.001f;
				runs.push_back( r );
				s0 = s1;
			}
			if ( b.decalSF1 && !b.decal )	// lane DECALS1: the game's decal LOD fade, per placement (gl/cellcull.h)
				wwCellDecalFadeShape( nif, nif->getBlockNumber( iShape ), runs );
			wwCellCullShape( nif, nif->getBlockNumber( iShape ), std::move( runs ) );
		}
		emitLap( 2 );
		if ( b.decalAngle )   // lane DECALANGLE: the renderer runs the game's angle rule per pixel on this shape
			wwCellDecalAngleShape( nif, nif->getBlockNumber( iShape ), b.decalMapped );
		if ( b.decal && !b.heightTex.isEmpty() )   // lane DECALS1: the renderer runs the decal parallax on this shape
			wwCellDecalPomShape( nif, nif->getBlockNumber( iShape ), b.pomScale, b.pomPasses );
		if ( b.water )        // lane WATER1: the renderer draws this shape with the game's water terms
			wwCellWaterShape( nif, nif->getBlockNumber( iShape ), b.waterRec );
		if ( b.landSplat )    // lane TERRBLEND1: the renderer tells the shader the alpha is a share
			wwCellLandShape( nif, nif->getBlockNumber( iShape ) );
		if ( !b.groundSpec.isEmpty() )
			wwCellGroundSpecShape( nif, nif->getBlockNumber( iShape ), b.groundSpec );
		if ( b.grass )        // lane GRASSMB1: the renderer fades this shape the game's grass way
			wwCellGrassShape( nif, nif->getBlockNumber( iShape ), b.grassWave, !b.grassNoFade );
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

		emitLap( 3 );
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
		emitLap( 4 );

		/* The same material plumbing the chunk builder and the native LOD scene
		 * write (src/lodgen.cpp ~4189, src/lodinative.cpp ~316): the same block
		 * pair, the same ten slots, the BGSM in the shader's Name so the
		 * renderer resolves it exactly as it does for a `.BTO` shape. */
		if ( !b.effectMat.isEmpty() || !b.effectBlock.isEmpty() ) {
			QModelIndex iShader = cellNewBlock( nif, QStringLiteral( "BSEffectShaderProperty" ) );
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
				QModelIndex iAlpha = cellNewBlock( nif, QStringLiteral( "NiAlphaProperty" ) );
				nif->set<int>( iAlpha, "Flags", b.effAlphaFlags ? int( b.effAlphaFlags ) : b.alphaThreshold ? 4844 : 4333 );
				nif->set<int>( iAlpha, "Threshold", int( b.alphaThreshold ) );
				nif->setLink( iShape, "Alpha Property", nif->getBlockNumber( iAlpha ) );
			}
			addLink( nif, iRoot, QStringLiteral( "Children" ), nif->getBlockNumber( iShape ) );
			emitLap( 5 );
			continue;
		}
		QModelIndex iShader = cellNewBlock( nif, QStringLiteral( "BSLightingShaderProperty" ) );
		nif->set<quint32>( iShader, "Shader Type", 0 );
		/* Lane EFX1: a Refraction bucket keeps Shader Flags 1 bit 15 and its strength, so the
		 * screen-space refraction preview (src/gl/renderer.cpp) bends the scene behind it the
		 * way the game does, instead of showing its normal-map diffuse as a solid swirled disk. */
		nif->set<quint32>( iShader, "Shader Flags 1",
			( b.emits ? 2151677953U : ( 2151677953U & ~0x400000U ) ) | ( b.refract ? 0x8000U : 0U )
			| ( b.decal ? 0x04000000U : 0U ) | b.decalSF1 );   // lane PLACED1: Decal; lane GAPFIX1: the model's own
		if ( b.refract )
			nif->set<float>( iShader, "Refraction Strength", b.refractStrength );
		nif->set<quint32>( iShader, "Shader Flags 2", ( b.withColour ? 0x25U : 5U ) & ( b.decal ? ~1U : ~0U ) );
		QModelIndex iTexSet = cellNewBlock( nif, QStringLiteral( "BSShaderTextureSet" ) );
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
			if ( !b.heightTex.isEmpty() )	// lane DECALS1: read through GlowMap (no new sampler; shader type 0 = no glow)
				nif->set<QString>( nif->getIndex( iArr, 2 ), b.heightTex );
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
			QModelIndex iAlpha = cellNewBlock( nif, QStringLiteral( "NiAlphaProperty" ) );
			/* 4844 = alpha TEST only; 4333 = alpha BLEND, SRC_ALPHA /
			 * ONE_MINUS_SRC_ALPHA. 4109 = SRC_ALPHA / ONE, ADDED: a blended-ground
			 * layer is one share of the engine's weighted sum, not an opacity
			 * over what is below it (lane TERRBLEND1). */
			nif->set<int>( iAlpha, "Flags", b.landSplat ? 4109 : b.alphaThreshold ? 4844 : 4333 );
			nif->set<int>( iAlpha, "Threshold", int( b.alphaThreshold ) );
			nif->setLink( iShape, "Alpha Property", nif->getBlockNumber( iAlpha ) );
		}
		addLink( nif, iRoot, QStringLiteral( "Children" ), nif->getBlockNumber( iShape ) );
		emitLap( 5 );
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
	bool read = false;
	while ( !in.atEnd() ) {
		const QString line = in.readLine().trimmed();
		if ( line.isEmpty() || line.startsWith( QLatin1Char( '#' ) ) )
			continue;
		if ( !read ) {
			if ( !cellSpecFromLine( line, spec, error ) )
				return false;
			read = true;
		} else if ( line.startsWith( QLatin1String( "resources|" ), Qt::CaseInsensitive ) ) {
			// lane OPENCELLFIX1: the Mod Organizer profile's folders, lowest first, so a mod's models load
			spec.resources = line.mid( 10 ).split( QLatin1Char( ';' ), Qt::SkipEmptyParts );
		}
	}
	if ( read )
		return true;
	if ( error )
		*error = QStringLiteral( "%1 has no spec line" ).arg( path );
	return false;
}

void cellApplyEnvModifiers( CellSceneSpec & spec )
{
	const QByteArray ov = wwEnv( "WW_CELL_OVERLAY" );
	if ( !ov.isEmpty() ) {
		bool known = false;
		const CellOverlay o = cellOverlayFromName( QString::fromLatin1( ov ), &known );
		if ( known )
			spec.overlay = o;
		else
			qWarning() << "WW_CELL_OVERLAY:" << ov << "is not an overlay; try one of:"
				<< cellOverlayNames();
	}
	if ( !wwEnv( "WW_CELL_DISABLED" ).isEmpty() )
		spec.showDisabled = true;
	if ( !wwEnv( "WW_CELL_MARKERS" ).isEmpty() )
		spec.showMarkers = true;
	if ( !wwEnv( "WW_CELL_PROBES_HIDE" ).isEmpty() ) {
		spec.probesHiddenByEnv = spec.probesHiddenByEnv || spec.probesShow;	// lane LIVESET1: a live unset shows them
		spec.probesShow = false;    // lane PRTPGI: placed and baked, the markers not drawn (clean pictures)
	}
	if ( !wwEnv( "WW_CELL_NOTERRAIN" ).isEmpty() )
		spec.terrain = false;
	if ( !wwEnv( "WW_CELL_NOWATER" ).isEmpty() )
		spec.water = false;
	if ( !wwEnv( "WW_CELL_NOGRID" ).isEmpty() )
		spec.grid = false;
	if ( wwEnvInt( "WW_CELL_GRID" ) != 0 )
		spec.grid = true;     // lane FIXPACK1 (YELLOWSLAB1): the borders forced on, the gate's red
	const QByteArray dr = wwEnv( "WW_CELL_DATAROOT" );
	if ( !dr.isEmpty() )
		spec.dataRoot = QString::fromLocal8Bit( dr );
	const QByteArray li = wwEnv( "WW_CELL_LODI" );
	if ( !li.isEmpty() )
		spec.lodiPath = QString::fromLocal8Bit( li );
}

bool cellSpecFromEnv( CellSceneSpec & spec, QString * error )
{
	const QByteArray env = wwEnv( "WW_CELL_OPEN" );
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
	const QByteArray red = wwEnv( "WW_CELL_SKYINT_RED" ).trimmed();
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

/* lane NIFXLIGHT1: a placed model's .nifx (WW_NIFX_ROOT first, then the data root; Meshes\ added when the path lacks it)
 * and, when there is one, every NiAVObject's name and frame in the model (the root's own transform is the placement's) */
static std::shared_ptr<const WwNifxModel> cellNifxModel( const QString & dataRoot, const QString & model )
{
	// lane NIFXLIGHT3: the same rule as nifxPathForNif, shared with the live scan (src/gl/cellnifxlights.cpp)
	const QString found = wwNifxSidecarFind( { wwEnvStr( "WW_NIFX_ROOT" ), dataRoot }, model );
	if ( found.isEmpty() )
		return nullptr;
	return wwCellNifxModelLoad( dataRoot, model, found );
}

/* lane NIFXLIGHT3: a dimmer key group (NiFloatData "Data") as a track, timed by its controller's flags; null under 2 keys */
static std::shared_ptr<WwNifxDimTrack> cellNifxDimKeys( NifModel & src, const QModelIndex & ip, int flags )
{
	if ( !ip.isValid() || !src.blockInherits( ip, "NiFloatInterpolator" ) )
		return nullptr;
	const QModelIndex fd = src.getBlockIndex( src.getLink( ip, "Data" ) );
	const QModelIndex grp = fd.isValid() ? src.getIndex( fd, "Data" ) : QModelIndex();
	const QModelIndex keys = grp.isValid() ? src.getIndex( grp, "Keys" ) : QModelIndex();
	if ( !keys.isValid() || src.rowCount( keys ) < 2 )
		return nullptr;
	auto k = std::make_shared<WwNifxDimTrack>();
	k->interp = src.get<int>( grp, "Interpolation" );
	for ( int r = 0; r < src.rowCount( keys ); r++ ) {
		const QModelIndex ik = src.getIndex( keys, r );
		k->t.push_back( src.get<float>( ik, "Time" ) );
		k->v.push_back( src.get<float>( ik, "Value" ) );
		k->fwd.push_back( src.get<float>( ik, "Forward" ) );
		k->bwd.push_back( src.get<float>( ik, "Backward" ) );
	}
	k->cycle = ( flags & 6 ) >> 1;
	k->backwards = ( flags & 0x10 ) != 0;
	k->scaled = ( flags & 0x40 ) != 0;
	return k;
}

std::shared_ptr<const WwNifxModel> wwCellNifxModelLoad( const QString & dataRoot, const QString & model, const QString & found )
{
	auto m = std::make_shared<WwNifxModel>();
	m->model = model;
	m->sidecar = found;
	NifModel src;
	QByteArray bytes;
	QBuffer dev( &bytes );
	if ( !lodgenReadModelBytes( dataRoot, model, bytes ) || !dev.open( QIODevice::ReadOnly )
		|| !src.load( dev, model.toLocal8Bit().constData() ) )
		return m;
	src.resetState();
	const QList<int> roots = src.getRootLinks();
	QSet<qint32> seen;
	QHash<int, qint32> lightBlock;	// lane NIFXLIGHT3: node row -> its NiLight block (its own dimmer controller)
	std::function<void( qint32, const Transform & )> walk = [&]( qint32 b, const Transform & up ) {
		const QModelIndex ib = b < 0 || seen.contains( b ) ? QModelIndex() : src.getBlockIndex( b );
		if ( !ib.isValid() || !src.blockInherits( ib, "NiAVObject" ) )
			return;
		seen.insert( b );
		const Transform here = b != roots.first() ? up * Transform( &src, ib ) : up;
		WwNifxNode n;
		n.name = src.get<QString>( ib, "Name" );
		n.niLight = src.blockInherits( ib, "NiLight" );
		if ( n.niLight ) {
			n.dimmer = src.get<float>( ib, "Dimmer" );	// lane NIFXLIGHT2
			lightBlock.insert( int( m->nodes.size() ), b );
		}
		for ( int r = 0; r < 3; r++ ) {
			for ( int c = 0; c < 3; c++ )
				n.m[r][c] = here.rotation( r, c ) * here.scale;
			n.m[r][3] = here.translation[r];
		}
		m->nodes.append( n );
		for ( const int c : src.getChildLinks( b ) )
			walk( c, here );
	};
	if ( !roots.isEmpty() )
		walk( roots.first(), Transform() );
	/* lane NIFXLIGHT2: the sequences whose dimmer blocks name a NiLight (they still bind: an edit keeps the name), and the
	 * dimmer the cell view draws: the On sequence's last key (the lamp as placed is on), else the NiLight's Dimmer */
	for ( int b = 0; b < src.getBlockCount(); b++ ) {
		const QModelIndex is = src.getBlockIndex( b );
		if ( !is.isValid() || !src.blockInherits( is, "NiControllerSequence" ) )
			continue;
		const bool onSeq = src.get<QString>( is, "Name" ).compare( QLatin1String( "On" ), Qt::CaseInsensitive ) == 0;
		const QModelIndex arr = src.getIndex( is, "Controlled Blocks" );
		for ( int r = 0; arr.isValid() && r < src.rowCount( arr ); r++ ) {
			const QModelIndex cb = src.getIndex( arr, r );
			if ( src.resolveString( cb, "Controller Type" ) != QLatin1String( "NiLightDimmerController" ) )
				continue;
			const QString node = src.resolveString( cb, "Node Name" );
			for ( WwNifxNode & n : m->nodes ) {
				if ( !n.niLight || n.name.compare( node, Qt::CaseInsensitive ) != 0 )
					continue;
				n.sequences++;
				const QModelIndex ip = src.getBlockIndex( src.getLink( cb, "Interpolator" ) );
				if ( !onSeq || !ip.isValid() || !src.blockInherits( ip, "NiFloatInterpolator" ) )
					continue;
				float v = src.get<float>( ip, "Value" );
				const QModelIndex fd = src.getBlockIndex( src.getLink( ip, "Data" ) );
				const QModelIndex keys = fd.isValid() ? src.getIndex( src.getIndex( fd, "Data" ), "Keys" ) : QModelIndex();
				if ( keys.isValid() && src.rowCount( keys ) > 0 )
					v = src.get<float>( src.getIndex( keys, src.rowCount( keys ) - 1 ), "Value" );
				if ( std::isfinite( v ) && std::fabs( v ) < 1e30f )
					n.dimmer = v;
			}
		}
	}
	/* lane NIFXLIGHT3: the dimmer over time, as NifSkope plays it (src/gl/controllers.cpp ControllerManager::setSequence):
	 * a light a sequence binds follows the model's first looping sequence (start, stop, frequency and phase from the
	 * sequence, the flags from its controller); an unbound light its own active NiLightDimmerController */
	QModelIndex loopSeq;
	for ( int b = 0; b < src.getBlockCount() && !loopSeq.isValid(); b++ ) {
		const QModelIndex im = src.getBlockIndex( b );
		if ( !im.isValid() || !src.blockInherits( im, "NiControllerManager" ) )
			continue;
		for ( const qint32 l : src.getLinkArray( im, "Controller Sequences" ) ) {
			const QModelIndex iSeq = src.getBlockIndex( l, "NiControllerSequence" );
			if ( iSeq.isValid() && src.get<int>( iSeq, "Cycle Type" ) == 0 ) {
				loopSeq = iSeq;
				break;
			}
		}
	}
	auto ownDimmer = [&]( qint32 lb ) {
		QSet<qint32> seenC;
		for ( qint32 c = src.getLink( src.getBlockIndex( lb ), "Controller" ); c >= 0 && !seenC.contains( c ); ) {
			seenC.insert( c );
			const QModelIndex ic = src.getBlockIndex( c );
			if ( !ic.isValid() )
				break;
			if ( src.blockInherits( ic, "NiLightDimmerController" ) )
				return ic;
			c = src.getLink( ic, "Next Controller" );
		}
		return QModelIndex();
	};
	for ( auto it = lightBlock.cbegin(); it != lightBlock.cend(); ++it ) {
		WwNifxNode & n = m->nodes[it.key()];
		const QModelIndex own = ownDimmer( it.value() );
		std::shared_ptr<WwNifxDimTrack> k;
		if ( n.sequences > 0 && loopSeq.isValid() ) {
			const QModelIndex arr = src.getIndex( loopSeq, "Controlled Blocks" );
			for ( int r = 0; arr.isValid() && r < src.rowCount( arr ) && !k; r++ ) {
				const QModelIndex cb = src.getIndex( arr, r );
				if ( src.resolveString( cb, "Controller Type" ) != QLatin1String( "NiLightDimmerController" )
					|| src.resolveString( cb, "Node Name" ).compare( n.name, Qt::CaseInsensitive ) != 0 )
					continue;
				const QModelIndex ic = src.getBlockIndex( src.getLink( cb, "Controller" ) );
				const int flags = ic.isValid() ? src.get<int>( ic, "Flags" ) : own.isValid() ? src.get<int>( own, "Flags" ) : 76;
				k = cellNifxDimKeys( src, src.getBlockIndex( src.getLink( cb, "Interpolator" ) ), flags );
				if ( k ) {
					k->start = src.get<float>( loopSeq, "Start Time" );
					k->stop = src.get<float>( loopSeq, "Stop Time" );
					k->frequency = src.get<float>( loopSeq, "Frequency" );
					k->phase = src.get<float>( loopSeq, "Phase" );	// absent in Fallout 4 files: 0, as NifSkope reads it
					k->sequence = src.get<QString>( loopSeq, "Name" );
				}
			}
		} else if ( n.sequences == 0 && own.isValid() && ( src.get<int>( own, "Flags" ) & 0x08 ) ) {
			k = cellNifxDimKeys( src, src.getBlockIndex( src.getLink( own, "Interpolator" ) ), src.get<int>( own, "Flags" ) );
			if ( k ) {
				k->start = src.get<float>( own, "Start Time" );
				k->stop = src.get<float>( own, "Stop Time" );
				k->frequency = src.get<float>( own, "Frequency" );
				k->phase = src.get<float>( own, "Phase" );
			}
		}
		n.track = k;
	}
	return m;
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
	const QVector<EsmRefr> & lightRefs, const QHash<quint32, EsmRefr> & primRefs, const Vector3 & center,
	WwCellLighting * chunkOut = nullptr, const QHash<quint32, bool> * startOffOf = nullptr,
	const QVector<WwNifxFixture> * nifx = nullptr )
{
	WwCellLighting L;
	L.interior = spec.interior;
	L.dataRoot = spec.dataRoot;	// lane SUNCELL1: an exterior's weather LUT is looked up there too
	L.showSky = spec.interior && ( cellInteriorFlags( world.interior() ) & 0x0080u );	// lane SKYFULL1
	if ( !spec.interior ) {	// lane WEATHERBLEND4: the centre cell's regions, for a teleport's region weather
		L.regions = world.cellRegions( spec.cx, spec.cy, &L.regionSeed );
		L.regionAt[0] = ( float( spec.cx ) + 0.5f ) * 4096.0f;
		L.regionAt[1] = ( float( spec.cy ) + 0.5f ) * 4096.0f;
	}
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
	// lane GPURELIGHT1: WW_CELL_GI_GPU keeps the lights that start off in L.offLights (the relight's record)
	const bool giKeepOff = wwCellGiGpuOn();	// lane CELLALL1: the GPU relight row (the pin wins)
	/* lane SKYPIC1 (the brick review's light gate; a measurement pin, never a user toggle): WW_CELL_LIGHT_OFF =
	 * all | <ref form id hex>[,<id>...] starts those placed lights off, as if initially disabled, so the frame and the
	 * probe relight both lose them. Unset: no change. */
	const QByteArray lightOffPin = wwEnv( "WW_CELL_LIGHT_OFF" ).trimmed().toLower();
	QSet<quint32> lightOffRefs;
	for ( const QByteArray & t : lightOffPin.split( ',' ) ) {
		bool ok = false;
		const quint32 id = t.trimmed().toUInt( &ok, 16 );
		if ( ok )
			lightOffRefs.insert( id );
	}
	int lightOffHit = 0;
	/* lane VOLAIR1 (bungo's barrel look test; a measurement pin, never a user toggle, never a shipped default):
	 * WW_CELL_LIGHT_BOOST = <ref form id hex>:<colour x>:<radius x>[,...] scales those placed lights' colour and
	 * radius where the cell reads them, so the surfaces, the probe relight and the air all see the same light.
	 * Unset: no change. */
	const QByteArray lightBoostPin = wwEnv( "WW_CELL_LIGHT_BOOST" ).trimmed().toLower();
	QHash<quint32, QPair<float, float>> lightBoost;
	for ( const QByteArray & t : lightBoostPin.split( ',' ) ) {
		const QList<QByteArray> f = t.trimmed().split( ':' );
		bool ok = false;
		const quint32 id = f.value( 0 ).toUInt( &ok, 16 );
		if ( ok && f.size() >= 2 )
			lightBoost.insert( id, qMakePair( f.value( 1 ).toFloat(), f.size() >= 3 ? f.value( 2 ).toFloat() : 1.0f ) );
	}
	int lightBoostHit = 0;
	int parentMoved = 0;
	// lane LIGHTS5 (FOGSCALE1): the load order, for each light's .recx keys (celllights.cpp reads fog.scale live)
	for ( const QString & pl : world.pluginList().split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) )
		L.recxPlugins << pl.trimmed();
	L.recxData = EsmWeather::gameDataDir();
	QHash<quint32, QString> pluginFile;	// lane NIFXLIGHT3: every placed model is a fixture now, so name each plugin once
	auto recxKey = [&]( quint32 f, QString & plugin, quint32 & local ) {
		const quint32 idx = world.plugin() ? world.plugin()->getFormIDFile( f ) : ( f >> 24 );
		auto pf = pluginFile.constFind( idx );
		if ( pf == pluginFile.cend() )
			pf = pluginFile.insert( idx, idx < quint32( L.recxPlugins.size() ) ? QFileInfo( L.recxPlugins.at( int( idx ) ) ).fileName() : QString() );
		plugin = *pf;
		local = ESMFile::getFormIDLocal( f );
	};
	// what a .nifx light takes over from a placed light (lane NIFXLIGHT1): its keys, its flicker or pulse
	auto carry = [&]( const EsmRefr & r, const EsmLight & b, WwCellLight & l ) {
		l.ref = r.formID;
		recxKey( r.base, l.recxBasePlugin, l.recxBaseLocal );	// lane LIGHTS5 (FOGSCALE1)
		recxKey( r.formID, l.recxRefPlugin, l.recxRefLocal );
		l.recxBaseEdid = b.edid;
		// lane LIGHTFLICKER1: the game tests 0x8 first, then 0x180; the offset is in the light's local frame
		l.flicker = ( b.flags & 0x8 ) ? 1 : ( b.flags & 0x180 ) ? 2 : 0;
		if ( l.flicker ) {
			l.flickerPeriod = b.flickerPeriod;
			l.flickerIntensity = b.flickerIntensity;
			l.flickerMovement = b.flickerMovement;
			Matrix rm;
			rm.fromEuler( -r.rot[0], -r.rot[1], -r.rot[2] );
			const float sc = r.scale > 0.0f ? r.scale : 1.0f;
			for ( int a = 0; a < 3; a++ ) {
				const Vector3 d = rm * Vector3( a == 0 ? 1.0f : 0.0f, a == 1 ? 1.0f : 0.0f, a == 2 ? 1.0f : 0.0f );
				for ( int k = 0; k < 3; k++ )
					l.flickerAxes[a][k] = d[k] * sc;
			}
		}
	};
	for ( const EsmRefr & r : lightRefs ) {
		const EsmLight & b = world.light( r.base );
		if ( !b.exists )
			continue;
		const bool pinOff = lightOffPin == "all" || lightOffRefs.contains( r.formID );
		lightOffHit += pinOff ? 1 : 0;
		// lane GAPFIX1: a placed light starts in its enable parent's state, as every other ref does (MISS1)
		const bool refOff = startOffOf ? startOffOf->value( r.formID, r.initiallyDisabled ) : r.initiallyDisabled;
		parentMoved += refOff != r.initiallyDisabled ? 1 : 0;
		const bool startOff = refOff || ( b.flags & 0x20 ) || pinOff;
		if ( startOff ) {
			off++;
			if ( !giKeepOff || ( b.flags & 0x100000 ) ) {
				// lane NIFXLIGHT1: its state carries over (NIFXLIGHT3: kept for a .nifx made later too)
				if ( nifx && wwNifxKeepStartOff( *nifx, wwNifxLightsRed() ) && !( b.flags & 0x100000 ) ) {
					WwCellLight l;
					for ( int k = 0; k < 3; k++ )
						l.pos[k] = r.pos[k];
					carry( r, b, l );
					L.nifxOffPlaced.append( l );
				}
				continue;
			}
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
		const auto bi = lightBoost.constFind( r.formID );
		if ( bi != lightBoost.cend() ) {	// WW_CELL_LIGHT_BOOST
			lightBoostHit++;
			for ( int c = 0; c < 3; c++ )
				l.color[c] *= bi.value().first;
			if ( bi.value().second > 0.0f )
				l.radius *= bi.value().second;
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
		// lane VOLFOG1: a shaft emitter is a shadowed light with a GDRY (GenDynamic 0x320a50 + AddLight, MEASURED); its
		// volume intensity is that GDRY's
		if ( l.shadow && b.godRays && world.plugin() ) {
			WwGodRays gr;
			if ( wwGodRaysRead( *world.plugin(), b.godRays, gr ) )
				l.godRay = gr.intensity;
		}
		// lane LIGHTS3 (LENSFLARE1): the base's LENS, read once into the flare pass's table
		if ( b.lens && world.plugin() && wwCellLensKnown( b.lens, *world.plugin() ) )
			l.lens = b.lens;
		for ( int c = 0; c < 3; c++ )
			l.byteColor[c] = float( b.color[c] ) / 255.0f;
		l.nearClip =std::max( b.nearClip + ( r.xligCount >= 5 ? r.xlig[4] : 0.0f ), 0.0f );
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
			if ( !b.gobo.isEmpty() ) {	// lane LIGHTS5 (GOBO1): the pattern's frame, the ref's local y and z
				l.gobo = b.gobo;
				const Vector3 y = rm * Vector3( 0, 1, 0 ), z = rm * Vector3( 0, 0, 1 );
				for ( int k = 0; k < 3; k++ ) {
					l.goboY[k] = y[k];
					l.goboZ[k] = z[k];
				}
				l.goboTan = std::tan( fov * 0.5f * 3.14159265f / 180.0f );
			}
			spot++;
		} else {
			omni++;
			if ( !b.gobo.isEmpty() ) {	// lane GOBO2: an omni/box pattern's frame, the ref's local x, y, z
				l.gobo = b.gobo;
				Matrix rm;
				rm.fromEuler( -r.rot[0], -r.rot[1], -r.rot[2] );
				const Vector3 x = rm * Vector3( 1, 0, 0 ), y = rm * Vector3( 0, 1, 0 ), z = rm * Vector3( 0, 0, 1 );
				for ( int k = 0; k < 3; k++ ) {
					l.goboX[k] = x[k];
					l.goboY[k] = y[k];
					l.goboZ[k] = z[k];
				}
			}
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
		carry( r, b, l );
		/* lane LIGHTS4 (XEMI-LIGHT): the game's cell update multiplies a placed light's colour by its XEMI source's colour
		 * (a LIGH's bytes, a region's weather at the hour; notes/lights4); celllights.cpp applies it each frame */
		if ( r.emittance ) {
			const EsmLight & xl = world.light( r.emittance );
			const ESMFile::ESMRecord * xr = xl.exists || !world.plugin() ? nullptr : world.plugin()->findRecord( r.emittance );
			l.xemi = xl.exists ? 0x01000000U | ( quint32( xl.color[0] ) << 16 ) | ( quint32( xl.color[1] ) << 8 ) | quint32( xl.color[2] )
				: xr && *xr == "REGN" ? wwCellXemiRegionCode( r.emittance ) : 0U;
		}
		if ( startOff ) {   // lane GPURELIGHT1: recorded, lights nothing at the start
			L.offLights.append( l );
			continue;
		}
		L.lights.append( l );
		if ( !chunkOut )
			wwCellFxLitNoOffset( nif, L.lights.size() - 1, fade > 0.0f ? b.fade / fade : 1.0f );   // lane FXLIT1: a red control's data
	}
	if ( nifx && !nifx->isEmpty() ) {	// lane NIFXLIGHT1: the fixtures, with their reference's .recx key
		L.nifxFixtures = *nifx;
		for ( WwNifxFixture & f : L.nifxFixtures )
			recxKey( f.ref, f.refPlugin, f.refLocal );
	}
	if ( chunkOut ) {	// lane CELLSTREAM1: one streamed cell's lights, merged and published by src/cellstream.cpp
		*chunkOut = L;
		return;
	}
	QString amb = QStringLiteral( "none (exterior: the viewport light)" ), dir = QStringLiteral( "none (exterior: the Lookdev weather sun or moon)" );
	if ( spec.interior ) {
		const EsmInteriorCell & ic = world.interior();
		QByteArray tData, tDalc;
		const bool haveT = ic.lightingTemplate && world.lightingTemplate( ic.lightingTemplate, tData, tDalc );
		// lane VOLFOG1: the god-ray medium: XGDR, else the template's WGDR, else the engine's fallback
		{
			const quint32 tg = ic.lightingTemplate ? world.lightingTemplateGodRays( ic.lightingTemplate ) : 0;
			const quint32 gform = ic.godRays ? ic.godRays : tg;
			WwGodRays gr;
			if ( gform && world.plugin() && wwGodRaysRead( *world.plugin(), gform, gr ) ) {
				const WwGodRayMedium m = wwGodRayMediumOf( gr );
				const float v[12] = { m.air[0], m.air[1], m.air[2], m.fwd[0], m.fwd[1], m.fwd[2], m.back[0], m.back[1], m.back[2],
					m.gFwd, m.gBack, m.post };
				std::copy( v, v + 12, L.godRay );
				L.godRayNote = QStringLiteral( "%1 %2 (%3)" ).arg( ic.godRays ? QStringLiteral( "XGDR" ) : QStringLiteral( "LGTM WGDR" ) )
					.arg( gform, 8, 16, QLatin1Char( '0' ) ).arg( gr.edid );
			} else {
				L.godRayNote = QStringLiteral( "fallback (no GDRY)" );
			}
		}
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
		const QByteArray fogPin = wwEnv( "WW_CELL_FOG" ).trimmed();
		const QByteArray fogRed = wwEnv( "WW_CELL_FOG_RED" ).trimmed();
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
	wwCellPostSetCell( world, spec.interior );	// lane GRASSMB1: the depth of field's record (XCIM indoors)
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
		+ ( lightOffPin.isEmpty() ? QString() : QStringLiteral( " lightoff=%1 (%2 placed lights pinned off)" )	// lane SKYPIC1
			.arg( QString::fromLatin1( lightOffPin ) ).arg( lightOffHit ) )
		+ ( lightBoostPin.isEmpty() ? QString() : QStringLiteral( " lightboost=%1 (%2 placed lights boosted)" )	// lane VOLAIR1
			.arg( QString::fromLatin1( lightBoostPin ) ).arg( lightBoostHit ) )
		+ QStringLiteral( " norim=%1 ignorerough=%2" )	// lane RIM1: the lights whose shader drops the back-light
			.arg( std::count_if( L.lights.cbegin(), L.lights.cend(), []( const WwCellLight & l ) { return l.noRim; } ) )
			.arg( std::count_if( L.lights.cbegin(), L.lights.cend(), []( const WwCellLight & l ) { return l.ignoreRoughness; } ) )
		+ QStringLiteral( " parentstate=%1" ).arg( parentMoved )	// lane GAPFIX1: lights an enable parent turned on/off
		+ QStringLiteral( " xemilights=%1" )	// lane LIGHTS4: lights whose placement names an emittance source
			.arg( std::count_if( L.lights.cbegin(), L.lights.cend(), []( const WwCellLight & l ) { return l.xemi != 0; } ) )
		+ QStringLiteral( " ambientonly=%1" ).arg( ambientOnly )	// lane AMBO1: skipped, no direct light in game
		+ QStringLiteral( " ambientvolumes=%1" ).arg( L.ambientLights.size() )	// lane AMBO2: they scale the ambient
		+ QStringLiteral( " shapes=hemisphere %1 box %2 (box link unresolved %3) ambientboxes=%4" ).arg( hemi ).arg( box )
			.arg( boxLost ).arg( ambientBox )	// lane HEMI1
		+ QStringLiteral( " imagespace=%1" ).arg( isNote )
		+ QStringLiteral( " fog=%1" ).arg( L.fogNote.isEmpty() ? QStringLiteral( "none (exterior: the Lookdev weather fog)" ) : L.fogNote )
		+ QStringLiteral( " godrays=%1 emitters=%2" ).arg( L.godRayNote )	// lane VOLFOG1
			.arg( std::count_if( L.lights.cbegin(), L.lights.cend(), []( const WwCellLight & l ) { return l.godRay > 0.0f; } ) );
	wwCellLightsPublish( nif, L );
}

/* lane LANDSPEC4: the worldspace whose far LOD rings a worldspace draws: its parent's with WRLD "Use LOD Data"
 * (Goodneighbor -> Commonwealth), else its own; red WW_LANDSPEC_RED=ownlod keeps the child's own */
static QString cellLodWs( const QString & ws, const EsmWorld * w )
{
	if ( !w || ws.isEmpty() || wwEnv( "WW_LANDSPEC_RED" ) == "ownlod" )
		return ws;
	const QString p = w->lodWorldEdid();
	return p.isEmpty() || p.compare( w->worldspaceEdid(), Qt::CaseInsensitive ) == 0 ? ws : p;
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
	const QString env = wwEnvStr( "WW_CELL_FAR_LOD" );
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
/* lane GAPBAKE1: WW_CELL_BAKE_FARNORMALS=1 gives the far ring's ground quads smooth vertex normals (the .lodl
 * heights' central differences) instead of their faceted 1024-unit face normals; off = the old soup, byte for byte */
static bool cellBakeFarNormals()
{
	return wwEnvStr( "WW_CELL_BAKE_FARNORMALS" ) == QLatin1String( "1" ) && wwEnvStr( "WW_CELL_BAKE_SMOOTH" ) != QLatin1String( "0" );
}

static QString cellBakeFarSoup( const CellSceneSpec & spec, const QString & dataRoot, const QByteArray & red,
	ProbeSoup & soup, const EsmWorld * world )
{
	if ( !red.isEmpty() )
		return QStringLiteral( "far soup: none (RED %1)" ).arg( QString::fromLatin1( red ) );
	QElapsedTimer t;
	t.start();
	QStringList searched;
	const QString lodWs = cellLodWs( spec.world, world );	// lane LANDSPEC5: a "Use LOD Data" child bakes its parent's
	const QString lodl = cellFarLodl( lodWs, dataRoot, searched );
	if ( lodl.isEmpty() ) {
		const QString why = QStringLiteral( "far soup: NONE -- no FO4CSLOD/%1/%1.lodl in %2" )
			.arg( lodWs, searched.join( QLatin1String( "; " ) ) );
		qWarning() << qPrintable( why );
		return why;
	}
	const QFileInfo li( lodl );
	const QString lodi = li.absoluteDir().filePath( li.completeBaseName() + QStringLiteral( ".lodi" ) );
	const int radius = int( std::ceil( ProbeBakeSpec().rayMax / CELL_UNITS ) );
	const qint64 before = soup.triCount();
	ProbeFarResult r;
	if ( !probeFarAppendRing( lodl, QFileInfo( lodi ).isFile() ? lodi : QString(), spec.cx, spec.cy, ( spec.n - 1 ) / 2,
			radius, soup, &r, cellBakeFarNormals() ) ) {
		qWarning() << "far soup:" << r.error;
		return QStringLiteral( "far soup: REFUSED -- %1" ).arg( r.error );
	}
	return QStringLiteral( "far soup %1 out to %2 cells: ground quads %3 (colour %4), water quads %5, boxes %6, "
		"trees %7, triangles +%8, %9 ms%10" ).arg( QDir::toNativeSeparators( lodl ) ).arg( radius ).arg( r.quads )
		.arg( r.albedoQuads ).arg( r.waterQuads ).arg( r.boxes ).arg( r.trees ).arg( soup.triCount() - before )
		.arg( t.elapsed() ).arg( r.normalQuads ? QStringLiteral( ", ground vertex normals %1 quads" ).arg( r.normalQuads ) : QString() );
}

/* lane SUNCELL1: the Particles row (WW/CellParticles, env pin WW_CELL_PARTICLES, ships OFF). Read when a cell
 * opens: the row reopens the cell. */
bool wwCellParticlesOn()
{
	const QByteArray pin = wwEnv( "WW_CELL_PARTICLES" ).trimmed();
	if ( !pin.isEmpty() )
		return pin != "0";
	return QSettings().value( QStringLiteral( "WW/CellParticles" ), false ).toBool();
}

void wwCellParticlesSetOn( bool on )
{
	QSettings().setValue( QStringLiteral( "WW/CellParticles" ), on );
}

// Copy / Paste Branch's per-block strings (src/spells/blocks.cpp)
QStringList serializeStrings( NifModel * nif, const QModelIndex & iBlock, const QString & type );
void deserializeStrings( NifModel * nif, const QModelIndex & iBlock, const QString & type, QStringList & strings );

/*! lane SUNCELL1: one placed model's particle systems, copied into the cell's document the way Paste Branch
 *  copies a branch: the root's branch, without its triangle shapes (the weld has them already), its collision
 *  and its lights, every link out of the copy mapped to none, the root's own transform replaced by the
 *  reference's (lane MISS1's rule). Returns the particle systems copied (0: none in the branch, -1: a block
 *  would not copy; `why` says which). */
/*! lane SUNCELL1: a primitive ref's axes in the world, the rotation lane HEMI1 measured for light boxes:
 *  Rz(-z) Rx(-x) Ry(-y) of the ref's stored angles, axis k = column k. */
static void cellPrimAxes( const float r[3], Vector3 ax[3] )
{
	auto rot = []( int a, double ang, double m[3][3] ) {
		const double c = std::cos( ang ), s = std::sin( ang );
		const int i = ( a + 1 ) % 3, j = ( a + 2 ) % 3;
		for ( int u = 0; u < 3; u++ )
			for ( int v = 0; v < 3; v++ )
				m[u][v] = u == v ? 1.0 : 0.0;
		m[i][i] = c; m[i][j] = -s; m[j][i] = s; m[j][j] = c;
	};
	auto mul = []( const double a[3][3], const double b[3][3], double o[3][3] ) {
		for ( int u = 0; u < 3; u++ )
			for ( int v = 0; v < 3; v++ )
				o[u][v] = a[u][0] * b[0][v] + a[u][1] * b[1][v] + a[u][2] * b[2][v];
	};
	double rz[3][3], rx[3][3], ry[3][3], t[3][3], m[3][3];
	rot( 2, -r[2], rz );
	rot( 0, -r[0], rx );
	rot( 1, -r[1], ry );
	mul( rz, rx, t );
	mul( t, ry, m );
	for ( int k = 0; k < 3; k++ )
		ax[k] = Vector3( float( m[0][k] ), float( m[1][k] ), float( m[2][k] ) );
}

static quint32 cellLe32( const QByteArray & b, int at )
{
	quint32 v = 0;
	if ( at >= 0 && at + 4 <= b.size() )
		std::memcpy( &v, b.constData() + at, 4 );
	return v;
}

/* lane SUNCELL1 (last round): one copied particle system -- its block in the document, its bounding sphere in the
 * placed model's space (the root's own transform is the placement's), and its shader property in the source. */
struct CellPfxSystem
{
	qint32 block = -1;
	Vector3 center;
	float radius = 0.0f;
	qint32 srcShader = -1;
};

static int cellCopyParticleBranch( NifModel * nif, const QModelIndex & iParent, NifModel & src,
	const Vector3 & at, const Matrix & rot, float scale, QString * why, QVector<CellPfxSystem> * copied = nullptr,
	CellBlockSource * pool = nullptr, QVector<qint32> * written = nullptr )
{
	const QList<int> roots = src.getRootLinks();
	if ( roots.isEmpty() || !src.blockInherits( src.getBlockIndex( roots.first() ), "NiNode" ) ) {
		*why = QStringLiteral( "no NiNode root" );
		return -1;
	}
	QList<qint32> order;
	QSet<qint32> seen;
	int systems = 0;
	QVector<CellPfxSystem> found;	// block = the index in `order` until the copy's base is known
	std::function<void( qint32, const Transform & )> walk = [&]( qint32 b, const Transform & up ) {
		if ( b < 0 || seen.contains( b ) )
			return;
		seen.insert( b );
		const QModelIndex ib = src.getBlockIndex( b );
		if ( !ib.isValid() || src.blockInherits( ib, "BSTriShape" ) || src.blockInherits( ib, "NiCollisionObject" )
			|| src.blockInherits( ib, "NiLight" ) )
			return;
		order.append( b );
		// the model's frame: the root's own transform is replaced by the placement's, its children keep theirs
		const bool av = src.blockInherits( ib, "NiAVObject" );
		const Transform here = ( av && b != roots.first() ) ? up * Transform( &src, ib ) : up;
		if ( src.blockInherits( ib, "NiParticleSystem" ) ) {
			systems++;
			CellPfxSystem f;
			f.block = order.size() - 1;
			const QModelIndex iBound = src.getIndex( ib, "Bounding Sphere" );
			const Vector3 c = iBound.isValid() ? src.get<Vector3>( iBound, "Center" ) : Vector3();
			const float r = iBound.isValid() ? src.get<float>( iBound, "Radius" ) : 0.0f;
			f.center = here * c;
			f.radius = std::max( r, 0.0f ) * here.scale;
			f.srcShader = src.getLink( ib, "Shader Property" );
			found.append( f );
		}
		if ( av )
			for ( const int c : src.getChildLinks( b ) )
				walk( c, here );
		else
			for ( const int c : src.getChildLinks( b ) )
				walk( c, up );
	};
	walk( roots.first(), Transform() );
	if ( !systems )
		return 0;
	const qint32 base = nif->getBlockCount();
	QMap<qint32, qint32> map;	// every source block named: a link to one left out becomes none
	for ( qint32 b = 0; b < src.getBlockCount(); b++ )
		map.insert( b, -1 );
	// lane LIVESET4: a streamed cell takes freed blocks of each type first (the pool), so roaming does not grow the document
	QVector<qint32> dst;
	for ( int i = 0; i < order.size(); i++ ) {
		qint32 to = base + i;
		if ( pool ) {
			const QModelIndex ti = pool->take( nif, src.createRTTIName( src.getBlockIndex( order.at( i ) ) ) );
			to = ti.isValid() ? nif->getBlockNumber( ti ) : -1;
			if ( to < 0 ) {
				*why = QStringLiteral( "no block for source %1" ).arg( order.at( i ) );
				return -1;
			}
			if ( written )
				written->append( to );
		}
		map.insert( order.at( i ), to );
		dst.append( to );
	}
	for ( int oi = 0; oi < order.size(); oi++ ) {
		const qint32 b = order.at( oi );
		const QModelIndex ib = src.getBlockIndex( b );
		const QString type = src.createRTTIName( ib );
		QStringList strings = serializeStrings( &src, ib, type );
		QByteArray data;
		QBuffer buf( &data );
		if ( !buf.open( QIODevice::WriteOnly ) || !src.saveIndex( buf, ib ) ) {
			*why = QStringLiteral( "%1 %2 would not save" ).arg( type ).arg( b );
			return -1;
		}
		buf.close();
		const QModelIndex nb = !buf.open( QIODevice::ReadOnly ) ? QModelIndex()
			: pool ? nif->getBlockIndex( dst.at( oi ) ) : nif->insertNiBlock( type );
		if ( !nb.isValid() || !nif->loadAndMapLinks( buf, nb, map ) ) {
			*why = QStringLiteral( "%1 %2 would not load" ).arg( type ).arg( b );
			return -1;
		}
		deserializeStrings( nif, nb, type, strings );
	}
	Transform t;
	t.rotation = rot;
	t.translation = at;
	t.scale = scale;
	t.writeBack( nif, nif->getBlockIndex( dst.first() ) );
	addLink( nif, iParent, QStringLiteral( "Children" ), dst.first() );
	if ( copied )
		for ( CellPfxSystem f : found ) {
			f.block = dst.at( f.block );
			copied->append( f );
		}
	return systems;
}

/* lane DOORLIVE1: the live door's whole model (its shapes too, unlike the particles' copy) under a new node that
 * carries the placement; the copied root's own transform is cleared as the weld ignores it. Collision and lights are
 * left out. Returns the blocks copied (the root first), 0 or less on a refusal. */
static int cellCopyDoorBranch( NifModel * nif, const QModelIndex & iParent, NifModel & src, const Vector3 & at,
	const Matrix & rot, float scale, quint32 ref, qint32 * wrapper, qint32 * rootOut, QString * why,
	QString * animLoop = nullptr )
{
	/* lane FXPARITY2: with animLoop (an animated object) the particle systems stay out (the Particles row copies them)
	 * and so does a manager without a looping sequence (a door's Open / Close: the copy rests); the first looping
	 * sequence's name goes to *animLoop */
	auto managerLoops = [&]( const QModelIndex & ib ) -> QString {
		for ( const qint32 l : src.getLinkArray( ib, "Controller Sequences" ) ) {
			const QModelIndex iSeq = src.getBlockIndex( l, "NiControllerSequence" );
			if ( iSeq.isValid() && src.get<int>( iSeq, "Cycle Type" ) == 0 )
				return src.get<QString>( iSeq, "Name" );
		}
		return QString();
	};
	const QList<int> roots = src.getRootLinks();
	if ( roots.isEmpty() || !src.blockInherits( src.getBlockIndex( roots.first() ), "NiNode" ) ) {
		*why = QStringLiteral( "no NiNode root" );
		return -1;
	}
	QList<qint32> order;
	QSet<qint32> seen;
	std::function<void( qint32 )> walk = [&]( qint32 b ) {
		if ( b < 0 || seen.contains( b ) )
			return;
		seen.insert( b );
		const QModelIndex ib = src.getBlockIndex( b );
		if ( !ib.isValid() || src.blockInherits( ib, "NiCollisionObject" ) || src.blockInherits( ib, "NiLight" )
			|| src.blockInherits( ib, "bhkSerializable" ) )
			return;
		if ( animLoop && src.blockInherits( ib, "NiParticleSystem" ) )
			return;
		if ( animLoop && src.blockInherits( ib, "NiControllerManager" ) ) {
			const QString loop = managerLoops( ib );
			if ( loop.isEmpty() )
				return;
			if ( animLoop->isEmpty() )
				*animLoop = loop;
		}
		order.append( b );
		for ( const int c : src.getChildLinks( b ) )
			walk( c );
	};
	walk( roots.first() );
	const qint32 base = nif->getBlockCount();
	QMap<qint32, qint32> map;
	for ( qint32 b = 0; b < src.getBlockCount(); b++ )
		map.insert( b, -1 );
	for ( int i = 0; i < order.size(); i++ )
		map.insert( order.at( i ), base + i );
	for ( const qint32 b : order ) {
		const QModelIndex ib = src.getBlockIndex( b );
		const QString type = src.createRTTIName( ib );
		QStringList strings = serializeStrings( &src, ib, type );
		QByteArray data;
		QBuffer buf( &data );
		if ( !buf.open( QIODevice::WriteOnly ) || !src.saveIndex( buf, ib ) ) {
			*why = QStringLiteral( "%1 %2 would not save" ).arg( type ).arg( b );
			return -1;
		}
		buf.close();
		const QModelIndex nb = buf.open( QIODevice::ReadOnly ) ? nif->insertNiBlock( type ) : QModelIndex();
		if ( !nb.isValid() || !nif->loadAndMapLinks( buf, nb, map ) ) {
			*why = QStringLiteral( "%1 %2 would not load" ).arg( type ).arg( b );
			return -1;
		}
		deserializeStrings( nif, nb, type, strings );
	}
	Transform().writeBack( nif, nif->getBlockIndex( base ) );
	const QModelIndex iW = nif->insertNiBlock( QStringLiteral( "NiNode" ) );
	nif->set<QString>( iW, "Name", QStringLiteral( "%1 %2" ).arg( animLoop ? "animobj" : "doorlive" ).arg( ref, 8, 16, QLatin1Char( '0' ) ) );
	nif->set<quint32>( iW, "Flags", 14 );
	nif->set<float>( iW, "Scale", 1.0f );
	Transform t;
	t.rotation = rot;
	t.translation = at;
	t.scale = scale;
	t.writeBack( nif, iW );
	const qint32 w = nif->getBlockNumber( iW );
	addLink( nif, iW, QStringLiteral( "Children" ), base );
	addLink( nif, iParent, QStringLiteral( "Children" ), w );
	*wrapper = w;
	*rootOut = base;
	return int( order.size() );
}

/* lane BAKEINCR1: the far soup a bake of (cx, cy) with this block would append, as one sha1 -- the
 * triangles, their albedo and glass, the very ring probeFarAppendRing reads. "none" when the world has
 * no FO4CSLOD .lodl (the bake then appends none either). Only cells whose ring content moved go dirty. */
QString cellBakeFarDigest( const QString & world, const QString & dataRoot, int cx, int cy, int half,
	const EsmWorld * w )
{
	QStringList searched;
	const QString lodl = cellFarLodl( cellLodWs( world, w ), dataRoot, searched );	// lane LANDSPEC5: the parent's ring
	if ( lodl.isEmpty() )
		return QStringLiteral( "none" );
	const QFileInfo li( lodl );
	const QString lodi = li.absoluteDir().filePath( li.completeBaseName() + QStringLiteral( ".lodi" ) );
	const int radius = int( std::ceil( ProbeBakeSpec().rayMax / CELL_UNITS ) );
	ProbeSoup soup;
	ProbeFarResult r;
	if ( !probeFarAppendRing( lodl, QFileInfo( lodi ).isFile() ? lodi : QString(), cx, cy, half, radius, soup, &r,
			cellBakeFarNormals() ) )
		return QStringLiteral( "refused" );
	QCryptographicHash h( QCryptographicHash::Sha1 );
	auto feed = [&h]( const void * p, size_t n ) { h.addData( QByteArrayView( static_cast<const char *>( p ), qsizetype( n ) ) ); };
	const qint64 n[4] = { qint64( soup.tris.size() ), qint64( soup.alb.size() ), qint64( soup.glass.size() ),
		qint64( soup.glassT.size() ) };
	feed( n, sizeof( n ) );
	feed( soup.tris.data(), soup.tris.size() * sizeof( float ) );
	feed( soup.alb.data(), soup.alb.size() );
	feed( soup.glass.data(), soup.glass.size() * sizeof( float ) );
	feed( soup.glassT.data(), soup.glassT.size() );
	if ( !soup.vn.empty() )   // lane GAPBAKE1: only with WW_CELL_BAKE_FARNORMALS=1, so the old digests stand
		feed( soup.vn.data(), soup.vn.size() * sizeof( qint16 ) );
	return QString::fromLatin1( h.result().toHex() );
}

// lane CELLSTREAM1: a streamed cell's welded buckets, handed from the worker to the writer
struct CellChunkBuckets
{
	QHash<QString, Bucket> buckets;
	QStringList keys;
};

//! lane PRTPPLACE: the probe markers, one small box each, colored by class, split under the 16-bit index limit
static void cellProbeMarkerBuckets( const std::vector<ProbePoint> & probes, const Vector3 & origin, QHash<QString, Bucket> & buckets )
{
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
	for ( const ProbePoint & q : probes ) {
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

/* lane LIVESET1 (bungo 10-23: "In games, if you change a setting, that setting updates live"). The far LOD is the
 * document's last section: a live set cuts the document from the section on and appends it again, so the rest of the
 * cell stays as built. lane LIVESET3: the probe markers sit inside the document; they are cut from the middle or put
 * back where a fresh open builds them (liveRemove/liveMoveTo renumber every block-keyed side table), and the
 * streamer's cells after the far LOD are dropped with it and streamed again. A switch that moves the near cells' load
 * order still asks for the reopen. */
namespace
{
struct LiveDoc
{
	int rootBlock = -1;
	int tailEnd = -1;              //!< the block count our last step left: anything else appended = not our tail
	bool interior = false;
	bool farWanted = false;        //!< the far LOD was built (exterior, Far LOD on)
	bool streamed = false;         //!< the cells come from the streamer, after the far LOD
	QByteArray farType, farRed;    //!< the inputs that also move the near cells' load order (the bake's stack)
	int farFirst = -1;             //!< the block count before the far section
	std::shared_ptr<EsmWorld> world;
	QString ws, lodl;
	int cx = 0, cy = 0, n = 1;
	Vector3 origin;
	std::vector<ProbePoint> probes;
	bool probesBand = true;        //!< the PRTP band's Show probes (before WW_CELL_PROBES_HIDE)
	int markersAt = -1;            //!< where a fresh open puts the probe markers (shown or not)
	int markersCount = 0;          //!< their blocks there (0 = hidden)
	quint64 farSeen = 0, probesSeen = 0;
	QByteArray farKey;             //!< liveFarKey() the far LOD was built with
	// lane LIVESET4: Far LOD on/off and type stay live while the near cells' stack is the one they were loaded with
	QString askedPlugins;          //!< the plugins the cell asked, before the far stack step and the mod pick
	QString specRoot;              //!< the spec's data root (empty: the resource stack's first)
	QByteArray stackSig;           //!< liveSigOf() of the stack the near cells were loaded with
	QByteArray cellsKey;           //!< liveCellsKey(): actors outdoors and weapon parts as built
	QByteArray terrRed;            //!< lane DUNESAND2: WW_TERRBLEND_RED the ground was built with
	quint64 terrSeen = 0;
};

std::map<const NifModel *, LiveDoc> & liveDocs()
{
	static std::map<const NifModel *, LiveDoc> m;
	return m;
}

QByteArray liveFarType()
{
	return wwEnv( "WW_CELL_FARLOD_TYPE" ).toLower() + '|' + QByteArray::number( cellFarLodTypePinned() );
}

//! The values cellFarLodAppend reads while it builds; the per-draw ones (NEAR, SWAYTIME, CARDSWAY, SKY*) stay out
QByteArray liveFarKey()
{
	QByteArray k;
	for ( const char * n : { "WW_CELL_FARLOD_RED", "WW_CELL_FARLOD_NEARONLY", "WW_CELL_FARLOD_MAX", "WW_CELL_FARLOD_L0",
			"WW_CELL_FARLOD_L1", "WW_CELL_FARLOD_L2", "WW_CELL_FARLOD_TREE_DIST", "WW_CELL_FARLOD_RINGS",
			"WW_CELL_FARLOD_BUDGET_MB", "WW_CELL_FARLOD_OURS", "WW_CELL_FARLOD_SWAY", "WW_CELL_FARLOD_SHORE" } )
		k += wwEnv( n ) + '\x1f';
	// lane FAR6: Game LOD trees sway is written into the .bto shapes at build; its menu tick lives in the settings
	k += QByteArray( wwLookOn( WwLook::BtoSway ) ? "1" : "0" ) + wwEnv( "WW_CELL_FARLOD_BTOSWAY_RED" ) + '\x1f';
	return k;
}

//! The near cells' load order as one key: the plugins, the resource stack, the far field's lent roots
QByteArray liveSigOf( const QString & plugins, const QStringList & stack, const QByteArray & lend )
{
	return plugins.toUtf8() + '\x1f' + stack.join( QLatin1Char( '|' ) ).toUtf8() + '\x1f' + lend;
}

/*! lane LIVESET5: what moved between two liveSigOf() keys, for the reopen reason. A bake made from another load
 *  order (MERGE1025 c3: the 09-29 Commonwealth bake, 44 plugins, against today's 45) moves the plugins on every far
 *  on/off set; the reason names them so a judge can tell a data drift from a code one */
QString liveSigDiff( const QByteArray & was, const QByteArray & now )
{
	const QList<QByteArray> a = was.split( '\x1f' ), b = now.split( '\x1f' );
	if ( a.size() != 3 || b.size() != 3 )
		return QString();
	auto names = []( const QByteArray & s ) {
		QStringList out;
		for ( const QByteArray & p : s.split( ',' ) )
			if ( !p.isEmpty() )
				out << QFileInfo( QString::fromUtf8( p ) ).fileName().toLower();
		return out;
	};
	auto count = []( const QByteArray & s ) { return s.isEmpty() ? 0 : int( s.count( '|' ) ) + 1; };
	QStringList parts;
	if ( a[0] != b[0] ) {
		const QStringList pa = names( a[0] ), pb = names( b[0] );
		QStringList d;
		for ( const QString & n : pb )
			if ( !pa.contains( n ) )
				d << QLatin1Char( '+' ) + n;
		for ( const QString & n : pa )
			if ( !pb.contains( n ) )
				d << QLatin1Char( '-' ) + n;
		parts << QStringLiteral( "plugins %1 -> %2%3" ).arg( pa.size() ).arg( pb.size() )
			.arg( d.isEmpty() ? QStringLiteral( " (same files, another order)" ) : QStringLiteral( ": " ) + d.join( QLatin1Char( ' ' ) ) );
	}
	if ( a[1] != b[1] )
		parts << QStringLiteral( "resource roots %1 -> %2" ).arg( count( a[1] ) ).arg( count( b[1] ) );
	if ( a[2] != b[2] )
		parts << QStringLiteral( "bake roots lent %1 -> %2" ).arg( a[2].isEmpty() ? QStringLiteral( "none" ) : QStringLiteral( "yes" ) )
			.arg( b[2].isEmpty() ? QStringLiteral( "none" ) : QStringLiteral( "yes" ) );
	return parts.join( QLatin1String( "; " ) );
}

QByteArray liveLendKey( const QStringList & bakeRoots )
{
	return bakeRoots.join( QLatin1Char( '|' ) ).toUtf8() + ( wwEnv( "WW_CELL_FARLOD_RED" ) == "nobakeroots" ? "|0" : "|1" );
}

//! The values the near cells read per build that a live set re-streams: weapon parts, actors outdoors (exterior)
QByteArray liveCellsKey( bool interior )
{
	QByteArray k = wwLookOn( WwLook::WeaponParts ) ? "w1|" : "w0|";
	k += wwEnv( "WW_CELL_WEAP_PARTS_RED" ) + '|';
	if ( !interior )
		k += ( wwLookOn( WwLook::ActorsOutdoors ) ? "a1|" : "a0|" ) + wwEnv( "WW_CELL_ACTORS_OUT_RED" );
	return k;
}

//! Every side table keyed by block number follows the document's renumbering (a removal or move in the middle)
void liveRemapTables( NifModel * nif, const WwBlockMap & m )
{
	wwCellCullRemap( nif, m );
	wwCellWaterRemap( nif, m );
	wwCellLandRemap( nif, m );
	wwCellGrassRemap( nif, m );
	wwCellFxLitRemap( nif, m );
	wwDoorLiveRemap( nif, m );
	cellHighlightRemap( m );
}

//! Drops blocks first..first+count-1 from anywhere in the document, the root's links to them and their side tables
void liveRemove( NifModel * nif, LiveDoc & d, int first, int count )
{
	const int n = nif->getBlockCount();
	count = std::min( count, n - first );
	if ( first < 0 || count <= 0 )
		return;
	for ( int b = first; b < first + count; b++ ) {
		cellMeshDrop( nif, b );
		wwCellCullForget( nif, b );
		wwCellWaterForget( nif, b );
		wwCellLandForget( nif, b );
		wwCellGrassForget( nif, b );
		wwCellFxLitForget( nif, b );
		wwCellXemiForget( nif, b );   // lane LIGHTS2 (XEMI1)
	}
	const QModelIndex iRoot = nif->getBlockIndex( d.rootBlock );
	for ( const qint32 l : nif->getLinkArray( nif->getIndex( iRoot, "Children" ) ) )
		if ( l >= first && l < first + count )
			delLink( nif, iRoot, QStringLiteral( "Children" ), l );
	nif->removeBlockRange( first, count );
	if ( first + count < n )
		liveRemapTables( nif, wwBlockMapRemove( first, count ) );
}

//! Drops blocks `first`.. (our tail)
void liveCut( NifModel * nif, LiveDoc & d, int first )
{
	liveRemove( nif, d, first, nif->getBlockCount() - first );
}

//! Blocks first..first+count-1 (just appended) moved to start at `dst`, where a fresh open writes them; the root's
//! Children keep block order, as a fresh open adds them
void liveMoveTo( NifModel * nif, LiveDoc & d, int first, int count, int dst )
{
	if ( count <= 0 || dst < 0 || dst >= first )
		return;
	nif->moveBlockRange( first, count, dst );
	liveRemapTables( nif, wwBlockMapMove( first, count, dst ) );
	const QModelIndex iRoot = nif->getBlockIndex( d.rootBlock );
	const QVector<qint32> links = nif->getLinkArray( iRoot, "Children" );
	QVector<qint32> moved, rest;
	for ( const qint32 l : links )
		( l >= dst && l < dst + count ? moved : rest ).append( l );
	int at = 0;
	while ( at < rest.size() && rest.at( at ) < dst )
		at++;
	for ( int i = 0; i < moved.size(); i++ )
		rest.insert( at + i, moved.at( i ) );
	if ( rest != links ) {
		nif->setLinkArray( nif->getBlockItem( d.rootBlock ), QStringLiteral( "Children" ), rest );
		nif->updateBlockLinks( d.rootBlock );
	}
}

QString liveAppendFar( NifModel * nif, LiveDoc & d, CellFarStream * stream )
{
	d.farFirst = nif->getBlockCount();
	return cellFarLodAppend( nif, nif->getBlockIndex( d.rootBlock ), *d.world, cellLodWs( d.ws, d.world.get() ), d.cx, d.cy,
		d.n, d.lodl, d.origin, stream );
}

void liveAppendMarkers( NifModel * nif, LiveDoc & d )
{
	QHash<QString, Bucket> buckets;
	cellProbeMarkerBuckets( d.probes, d.origin, buckets );
	QStringList keys = buckets.keys();
	std::sort( keys.begin(), keys.end() );
	qint64 shapes = 0, verts = 0, tris = 0;
	const QModelIndex iRoot = nif->getBlockIndex( d.rootBlock );
	for ( const QString & k : keys )
		emitBucket( nif, iRoot, buckets[k], d.origin, shapes, verts, tris, nullptr );
}

//! The markers to `want`: 0 nothing to do, 1 done, 2 a reopen is needed
int liveMarkers( NifModel * nif, LiveDoc & d, QString * why )
{
	const bool want = d.probesBand && wwEnvIsEmpty( "WW_CELL_PROBES_HIDE" ) && !d.probes.empty();
	wwCellProbeViewSetShown( nif, d.probesBand && wwEnvIsEmpty( "WW_CELL_PROBES_HIDE" ) );
	if ( want == ( d.markersCount > 0 ) )
		return 0;
	if ( d.markersAt < 0 || d.streamed || nif->getBlockCount() != d.tailEnd ) {
		*why = d.markersAt < 0 ? QStringLiteral( "no place for the probe markers in this document" )
			: d.streamed ? QStringLiteral( "probe markers under streamed cells" )
			: QStringLiteral( "blocks were added after the live sections" );
		return 2;
	}
	nif->holdUpdates( true );
	if ( want ) {
		const int tail = nif->getBlockCount();
		liveAppendMarkers( nif, d );
		const int count = nif->getBlockCount() - tail;
		liveMoveTo( nif, d, tail, count, d.markersAt );
		d.markersCount = count;
		if ( d.farFirst >= d.markersAt )
			d.farFirst += count;
	} else {
		liveRemove( nif, d, d.markersAt, d.markersCount );
		if ( d.farFirst >= d.markersAt + d.markersCount )
			d.farFirst -= d.markersCount;
		d.markersCount = 0;
	}
	nif->holdUpdates( false );
	nif->updateModel();
	d.tailEnd = nif->getBlockCount();
	*why = QStringLiteral( "probe markers %1 in place" ).arg( want ? QStringLiteral( "shown" ) : QStringLiteral( "hidden" ) );
	return 1;
}

//! The data root a build would resolve now (the spec's, else the resource stack's first), as cellBuild does
QString liveRootNow( const LiveDoc & d )
{
	const QStringList stack = lodgenResourceSearchPaths();
	return d.specRoot.isEmpty() && !stack.isEmpty() ? stack.first() : d.specRoot;
}

/*! lane LIVESET4: the near cells' stack an open with the switches now would load (cellBuild's far stack step, then
 *  the mod pick), as liveSigOf(); empty with `refusal` set when it cannot be told without loading */
QByteArray liveStackNow( const LiveDoc & d, bool farOn, QString * viewNote, QString * lines, QString * refusal )
{
	QString plugins = d.askedPlugins, lodb;
	QStringList present;
	QByteArray lend;
	const QStringList stack = lodgenResources();
	if ( farOn ) {
		QStringList searched;
		const QString lodWs = cellLodWs( d.ws, d.world.get() );	// lane LANDSPEC4
		const CellFarStack fs = cellFarLodStackFor( lodWs, cellFarLodl( lodWs, liveRootNow( d ), searched ), plugins );
		lodb = fs.lodb;
		present = fs.bakePresent;
		*lines += QStringLiteral( "  " ) + fs.line + QLatin1Char( '\n' );
		if ( fs.adopt ) {
			plugins = fs.plugins;
			if ( !fs.resources.isEmpty() && fs.resources != stack ) {
				*refusal = QStringLiteral( "Far LOD: the bake's resource roots are not the loaded ones (the near cells load again)" );
				return QByteArray();
			}
		} else if ( !fs.bakeRoots.isEmpty() ) {
			lend = liveLendKey( fs.bakeRoots );
		}
	}
	const CellModApplied mp = cellModPickApply( d.ws, plugins, lodb, present );
	*lines += QStringLiteral( "  " ) + mp.line + QLatin1Char( '\n' );
	*viewNote = mp.viewNote;
	return liveSigOf( mp.plugins, mp.resources.isEmpty() ? stack : mp.resources, lend );
}

/*! The far LOD, and the streamed cells after it, again from the switches: 0 nothing to do, 1 rebuilt, 2 a reopen is
 *  needed. lane LIVESET4: on/off, type and the stack reds too while the near stack holds; actors and weapon parts by
 *  streaming the cells again (a fixed block welds them with its statics) */
int liveFar( NifModel * nif, LiveDoc & d, QString * why )
{
	const QByteArray red = wwEnv( "WW_CELL_FARLOD_RED" );
	const bool wanted = !d.interior && cellFarLodWanted();
	const QByteArray type = liveFarType(), key = liveFarKey(), cells = liveCellsKey( d.interior );
	const bool farMoved = wanted != d.farWanted || ( wanted && ( type != d.farType || red != d.farRed || key != d.farKey ) );
	const bool cellsMoved = cells != d.cellsKey;
	if ( !farMoved && !cellsMoved )
		return 0;	// no far LOD either way, or no build-time value moved
	if ( cellsMoved && !d.streamed ) {
		*why = d.interior ? QStringLiteral( "weapon parts: an interior is one fixed block, its weapons welded with the statics" )
			: QStringLiteral( "actors outdoors / weapon parts: this fixed block (a harness or bake open) welds them with the "
				"statics; a streamed view rebuilds them live" );
		return 2;
	}
	QString viewNote, lines;
	if ( farMoved ) {	// the stack the near cells load: live only while it is the one they were loaded with
		QString refusal;
		const QByteArray sig = liveStackNow( d, wanted, &viewNote, &lines, &refusal );
		if ( sig.isEmpty() || sig != d.stackSig ) {
			*why = !refusal.isEmpty() ? refusal : QStringLiteral( "Far LOD on/off, type or stack red: the near cells' load "
				"order (the bake's stack) is not the one they were loaded with" );
			const QString moved = sig.isEmpty() ? QString() : liveSigDiff( d.stackSig, sig );	// lane LIVESET5
			if ( !moved.isEmpty() )
				*why += QStringLiteral( " (" ) + moved + QLatin1Char( ')' );
			return 2;
		}
	}
	if ( !d.streamed && nif->getBlockCount() != d.tailEnd ) {
		*why = QStringLiteral( "blocks were added after the live sections" );
		return 2;
	}
	if ( wanted && !d.world ) {
		*why = QStringLiteral( "no worldspace index kept for this document" );
		return 2;
	}
	QElapsedTimer t;
	t.start();
	// lane LIVESET3 (FARSTREAM): the streamed cells sit after the far LOD; they go with it and stream in again
	const std::shared_ptr<CellStreamSeed> seed = d.streamed ? cellStreamSuspend( nif ) : nullptr;
	CellFarStream fs;
	if ( seed )
		fs.roam = seed->roam;
	nif->holdUpdates( true );
	liveCut( nif, d, d.farFirst );
	cellFarLodForget( nif );
	QString notes;
	if ( wanted ) {
		QStringList searched;
		d.lodl = cellFarLodl( cellLodWs( d.ws, d.world.get() ), liveRootNow( d ), searched );
		notes = liveAppendFar( nif, d, seed ? &fs : nullptr );
	}
	nif->holdUpdates( false );
	nif->updateModel();
	if ( seed ) {
		auto next = std::make_shared<CellStreamSeed>( *seed );
		next->farHand = fs.handBlocks;
		next->farLand = fs.landBlocks;
		cellStreamStart( nif, next );
	}
	if ( farMoved )
		cellModPickSetViewNote( nif, viewNote );
	const bool onOff = wanted != d.farWanted;
	d.farWanted = wanted;
	d.farType = type;
	d.farRed = red;
	d.farKey = key;
	d.cellsKey = cells;
	d.tailEnd = nif->getBlockCount();
	if ( !lines.isEmpty() )
		qInfo().noquote() << lines.trimmed();
	if ( !notes.isEmpty() )
		qInfo().noquote() << notes.trimmed();
	*why = QStringLiteral( "%1 in place in %2 ms%3" )
		.arg( onOff ? ( wanted ? QStringLiteral( "far LOD added" ) : QStringLiteral( "far LOD removed" ) )
			: farMoved ? QStringLiteral( "far LOD rebuilt" ) : QStringLiteral( "cells rebuilt (actors outdoors / weapon parts)" ) )
		.arg( t.elapsed() )
		.arg( seed ? QStringLiteral( " (streamed cells dropped and streamed again)" ) : QString() );
	return 1;
}

void liveForget( const NifModel * nif )
{
	liveDocs().erase( nif );
}

void liveKeep( NifModel * nif, const CellSceneSpec & spec, const std::shared_ptr<EsmWorld> & world, const QString & lodl,
	int farFirst, const Vector3 & origin, int rootBlock, bool streamed, std::vector<ProbePoint> probes, int markersAt,
	int markersCount, const QString & askedPlugins, const QByteArray & stackSig )
{
	LiveDoc d;
	d.rootBlock = rootBlock;
	d.interior = spec.interior;
	d.farWanted = !spec.interior && cellFarLodWanted();
	d.streamed = streamed;
	d.farType = liveFarType();
	d.farRed = wwEnv( "WW_CELL_FARLOD_RED" );
	d.farKey = liveFarKey();
	d.farFirst = farFirst;
	if ( !spec.interior )
		d.world = world;	// read-only to a build (a stream holds it the same way); Far LOD on needs it later
	d.askedPlugins = askedPlugins;
	d.specRoot = spec.dataRoot;
	d.stackSig = stackSig;
	d.cellsKey = liveCellsKey( spec.interior );
	d.terrRed = wwEnv( "WW_TERRBLEND_RED" );
	d.ws = spec.world;
	d.lodl = lodl;
	d.cx = spec.cx;
	d.cy = spec.cy;
	d.n = spec.n;
	d.origin = origin;
	d.probesBand = spec.probesShow || spec.probesHiddenByEnv;
	d.markersAt = markersAt;
	d.markersCount = markersCount;
	d.probes = std::move( probes );
	d.tailEnd = nif->getBlockCount();
	d.farSeen = d.probesSeen = d.terrSeen = wwSwitchGen();
	liveDocs()[nif] = std::move( d );
	static QSet<const void *> hooked;
	if ( hooked.contains( nif ) )
		return;
	hooked.insert( nif );
	const NifModel * key = nif;
	QObject::connect( nif, &QAbstractItemModel::modelReset, nif, [nif]() {
		if ( nif->getBlockCount() == 0 )
			liveForget( nif );
	} );
	QObject::connect( nif, &QObject::destroyed, [key]() {
		liveForget( key );
		hooked.remove( key );
	} );
}

/*! lane DUNESAND2: WW_TERRBLEND_RED. nospec only drops the ground's TX07, swapped here in place; nearest and
 *  facenormal are written into the ground's vertices at build (the reopen); edgepass and matalpha are per draw. */
int liveTerrRed( NifModel * nif, LiveDoc & d, QString * why )
{
	const QByteArray now = wwEnv( "WW_TERRBLEND_RED" );
	if ( now == d.terrRed )
		return 0;
	auto inVerts = []( const QByteArray & v ) { return v == "nearest" || v == "facenormal"; };
	if ( inVerts( now ) || inVerts( d.terrRed ) ) {
		*why = QStringLiteral( "WW_TERRBLEND_RED=%1 is written into the ground's vertices at build" )
			.arg( QString::fromLatin1( inVerts( now ) ? now : d.terrRed ) );
		return 2;
	}
	const bool was = d.terrRed == "nospec", is = now == "nospec";
	d.terrRed = now;
	if ( was == is )
		return 0;
	const QHash<int, QString> specs = wwCellGroundSpecs( nif );
	nif->holdUpdates( true );
	for ( auto it = specs.cbegin(); it != specs.cend(); ++it ) {
		const QModelIndex iShape = nif->getBlockIndex( it.key() );
		const QModelIndex iShader = nif->getBlockIndex( nif->getLink( iShape, "Shader Property" ) );
		const QModelIndex iSet = nif->getBlockIndex( nif->getLink( iShader, "Texture Set" ) );
		if ( iSet.isValid() )
			nif->set<QString>( nif->getIndex( nif->getIndex( iSet, "Textures" ), 7 ), is ? QString() : it.value() );
	}
	nif->holdUpdates( false );
	nif->updateModel();
	*why = QStringLiteral( "ground TX07 %1 on %2 shapes" ).arg( is ? "dropped" : "bound" ).arg( specs.size() );
	return 1;
}
} // namespace

int cellLiveSections( NifModel * nif, QString * why )
{
	QString w;
	auto it = liveDocs().find( nif );
	if ( !nif || it == liveDocs().end() )
		return 0;
	LiveDoc & d = it->second;
	const bool farMoved = wwSwitchMovedFor( d.farSeen, { "WW_CELL_FARLOD", "WW_CELL_ACTORS_OUT", "WW_CELL_WEAP_PARTS" } );
	const bool probesMoved = wwSwitchMovedFor( d.probesSeen, { "WW_CELL_PROBES_HIDE" } );
	const bool terrMoved = wwSwitchMovedFor( d.terrSeen, { "WW_TERRBLEND_RED" } );
	int r = farMoved ? liveFar( nif, d, &w ) : 0;
	if ( r != 2 && terrMoved ) {
		QString tw;
		const int t = liveTerrRed( nif, d, &tw );
		if ( t )
			w = w.isEmpty() || t == 2 ? tw : w + QStringLiteral( "; " ) + tw;
		r = std::max( r, t );
	}
	if ( r != 2 && probesMoved ) {
		QString pw;
		const int p = liveMarkers( nif, d, &pw );
		if ( p )
			w = w.isEmpty() || p == 2 ? pw : w + QStringLiteral( "; " ) + pw;
		r = std::max( r, p );
	}
	if ( r == 1 ) {	// held shadow maps still hold the old casters: drop them as a fresh open has none
		wwCellShadowsForget( nif );
		wwSunShadowForget();
	}
	if ( why )
		*why = w;
	return r;
}

int cellLiveRebuild( NifModel * nif, QString * why )
{
	auto it = liveDocs().find( nif );
	if ( !nif || it == liveDocs().end() ) {
		if ( why )
			*why = QStringLiteral( "no live cell document" );
		return 2;
	}
	QString w;
	const int r = liveFar( nif, it->second, &w );
	if ( r == 1 ) {
		wwCellShadowsForget( nif );
		wwSunShadowForget();
	}
	if ( why )
		*why = w;
	return r;
}

int cellLiveProbesShow( NifModel * nif, bool show, QString * why )
{
	auto it = liveDocs().find( nif );
	if ( !nif || it == liveDocs().end() ) {
		if ( why )
			*why = QStringLiteral( "no live cell document" );
		return 2;
	}
	LiveDoc & d = it->second;
	const bool was = d.probesBand;
	d.probesBand = show;
	QString w;
	const int r = liveMarkers( nif, d, &w );
	if ( r == 2 )
		d.probesBand = was;
	if ( r == 1 ) {
		wwCellShadowsForget( nif );
		wwSunShadowForget();
	}
	if ( why )
		*why = w;
	return r == 2 ? 2 : 1;
}

namespace
{
//! lane LIGHTPHYS4: the far light's inputs from the cell's bake, kept so a change of the light rows rebakes it live
struct CellFarLive
{
	const void * nif = nullptr;
	std::unique_ptr<ProbeRelightOps> ops;
	QString dir;
	FarLightBakeSpec fs;
	ProbeGiSpec gp;
	WwCellLighting lp;
};

CellFarLive & cellFarLive()
{
	static CellFarLive f;
	return f;
}
}

//! lane FARVIEW1 (a function since LIGHTPHYS4): the far tables for the group state, and the GI's placed share; the notes
static QString cellFarLightPublish( const void * nif, const QString & farDir, const ProbeSoup & probeSoup,
	const QString & giBakeDir, const WwCellLighting & lp, const ProbeGiSpec & gp )
{
	FarLightSet set;
	QString ferr;
	if ( !farLightLoad( farDir, &set, &ferr ) )
		return QStringLiteral( "  far light load FAILED: %1\n" ).arg( ferr );
	// WW_CELL_FARLIGHT_STATE: start (each group as the game starts it; the default) | all | none
	const QByteArray st = wwEnv( "WW_CELL_FARLIGHT_STATE" ).trimmed();
	auto on = [&]( quint64, bool onAtStart ) { return st == "all" ? true : st == "none" ? false : onAtStart; };
	std::vector<float> E;
	farLightSum( set, on, E );
	WwCellFar fr;
	farLightTables( set, E, fr.slotTab, fr.recs, &fr.bits, &fr.maxProbe );
	fr.cell = set.cell;
	fr.records = int( set.recs.size() );
	const QStringList band = QString::fromLatin1( wwEnv( "WW_CELL_FAR_BAND" ) ).split( QLatin1Char( ',' ) );
	if ( band.size() == 2 && band[0].toFloat() > 0.0f && band[1].toFloat() > band[0].toFloat() ) {
		fr.band[0] = band[0].toFloat();
		fr.band[1] = band[1].toFloat();
	}
	for ( const FarLightSet::Dot & d : set.dots )
		if ( on( d.group, d.onAtStart ) )
			fr.dots.insert( fr.dots.end(), { d.pos[0], d.pos[1], d.pos[2], 0.0f, d.I[0], d.I[1], d.I[2], 0.0f } );
	ProbeGiResult gq;
	if ( probeGiRelight( probeSoup, giBakeDir, lp, gp, &gq ) && !gq.grid.empty() ) {
		fr.placed = std::move( gq.grid );
		for ( int k = 0; k < 3; k++ ) {
			fr.placedOrigin[k] = gq.origin[k];
			fr.placedDims[k] = gq.dims[k];
		}
		fr.placedVoxel = gq.voxel;
	}
	fr.summary = QStringLiteral( "%1 records, table 2^%2 (walk %3), %4 dots, band %5-%6, placed grid %7 (%8 passes), state %9" )
		.arg( fr.records ).arg( fr.bits ).arg( fr.maxProbe ).arg( fr.dots.size() / 8 ).arg( double( fr.band[0] ) )
		.arg( double( fr.band[1] ) ).arg( fr.placed.empty() ? QStringLiteral( "none" ) : QStringLiteral( "%1x%2x%3" )
		.arg( fr.placedDims[0] ).arg( fr.placedDims[1] ).arg( fr.placedDims[2] ) ).arg( gq.passes )
		.arg( st.isEmpty() ? QStringLiteral( "start" ) : QString::fromLatin1( st ) );
	const QString note = QStringLiteral( "  far light published: %1; %2\n" ).arg( fr.summary, set.census );
	wwCellFarPublish( nif, fr );
	return note;
}

bool cellFarLightRelit( const void * nif, const ProbeSoup & soup, const QString & giBakeDir )
{
	CellFarLive & f = cellFarLive();
	if ( !f.ops || f.nif != nif )
		return false;
	const wwlp::Params now = wwLightPhysParams();
	if ( wwlp::sameBounce( now, f.fs.lightPhys ) )
		return false;
	QElapsedTimer t;
	t.start();
	f.fs.lightPhys = now;
	f.gp.lightPhys = now;
	FarLightBakeOut fo;
	QString err, note;
	if ( farLightBake( *f.ops, f.dir, f.fs, &fo, &err ) )
		note = fo.census + QLatin1Char( ';' ) + cellFarLightPublish( nif, f.dir, soup, giBakeDir, f.lp, f.gp ).trimmed();
	else
		note = QStringLiteral( "far light bake FAILED: %1" ).arg( err );
	fprintf( stderr, "far light relit for the light rows (%lld ms): %s\n", qint64( t.elapsed() ), qPrintable( note ) );
	fflush( stderr );
	return true;
}

// lane CELLSTREAM1: the builder; `chunk` set = one streamed cell (n = 1), no document touched, stops before emit
static bool cellBuild( NifModel * nif, const CellSceneSpec & specAsked, CellChunk * chunk,
	QString * error, QString * notes )
{
	/* lane BAKEBLOCK1 (bungo, PRTP_PLAN item 5): an exterior BAKE loads the game's own 5x5 block around the
	 * cell at full detail (uGridsToLoad), and the far soup from the LOD files beyond it (below); the probes
	 * stay the asked block's. WW_CELL_BAKEBLOCK_RED=n1 (the asked block, no far) | nolod (5x5, no far):
	 * the gate's red controls only. A relight of a bake on disk (WW_CELL_GI_FROM) traces its shadow, sun and
	 * feed rays against the soup too, so it loads the same block + far the bake did. */
	CellSceneSpec spec = specAsked;
	const QByteArray blockRed = wwEnv( "WW_CELL_BAKEBLOCK_RED" );
	const bool bakeExterior = !spec.interior && ( !wwEnvIsEmpty( "WW_CELL_PROBES" ) || spec.probes )
		&& ( spec.probesBake || !wwEnvIsEmpty( "WW_CELL_PROBE_BAKE" )
			|| !wwEnvIsEmpty( "WW_CELL_GI_FROM" ) );
	if ( bakeExterior && blockRed != "n1" )
		spec.n = qMax( spec.n, 5 );
	auto fail = [error]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};
	if ( !nif && !chunk )
		return fail( QStringLiteral( "no model" ) );
	if ( !spec.valid )
		return fail( QStringLiteral( "no cell spec" ) );
	if ( !chunk )
		liveForget( nif );	// lane LIVESET1: the old document's live context (and the world it holds) goes first
	// lane CELLSTREAM1: an exterior view streams its cells round the camera (src/cellstream.h); this open builds
	// the document, the far field and the lighting, and hands the world to the streamer instead of welding a block
	const bool streamInit = !chunk && !spec.interior && spec.overlay == CellOverlay::None && !bakeExterior
		&& !spec.probes && wwEnvIsEmpty( "WW_CELL_PROBES" )
		&& wwEnvIsEmpty( "WW_CELL_GI_FROM" ) && cellStreamWanted();

	QElapsedTimer clock;
	clock.start();
	if ( !chunk ) {
		CellSpeed::begin();   // lane SPEED1
		cellMeshClear( nif );
		cellStreamStop( nif );   // lane CELLSTREAM1: the last open's streamer lets go of this document
	}

	/* lane FARLOD2: ONE STACK FOR NEAR AND FAR (cellfarlod.h). With the FO4CS far LOD the near cells load the
	 * load order and resources the far LOD was baked from (its .lodb record), so a mod's trees are the mod's
	 * trees on both sides of the block edge. Decided before the worldspace loads; the line goes in the notes. */
	QString farStackNote;
	const QString liveAsked = spec.plugins;	// lane LIVESET4: the live stack key's inputs
	QByteArray liveLend;
	QString modLodb;               // lane MODPICK1: the shown bake's record, empty when none
	QStringList modBakePresent;    // lane MODPICK1: its plugins still on disk
	if ( !chunk && !spec.resources.isEmpty() )
		cellFarLodAdoptResources( spec.resources );	// lane OPENCELLFIX1: the picked load order's folders
	if ( !chunk && !spec.interior && cellFarLodWanted() ) {   // a streamed cell: the open's plugins (the seed)
		QString preRoot = spec.dataRoot;
		if ( preRoot.isEmpty() && !lodgenResourceSearchPaths().isEmpty() )
			preRoot = lodgenResourceSearchPaths().first();
		QStringList preSearched;
		/* lane LANDSPEC5: a "Use LOD Data" child's stack is its parent's bake (no EsmWorld yet: the WRLD read alone);
		 * red WW_LANDSPEC_RED=ownlod keeps the child's own */
		const QString preParent = wwEnv( "WW_LANDSPEC_RED" ) == "ownlod" ? QString()
			: EsmWorld::lodWorldOf( spec.plugins, spec.world );
		const QString preWs = preParent.isEmpty() ? spec.world : preParent;
		const CellFarStack fs = cellFarLodStackFor( preWs, cellFarLodl( preWs, preRoot, preSearched ), spec.plugins );
		farStackNote = QStringLiteral( "  " ) + fs.line + QLatin1Char( '\n' );
		if ( !preParent.isEmpty() ) {
			const QString pl = QStringLiteral( "far stack: %1 takes the far LOD stack of %2" ).arg( spec.world, preParent );
			farStackNote += QStringLiteral( "  " ) + pl + QLatin1Char( '\n' );
			qInfo().noquote() << pl;
		}
		modLodb = fs.lodb;
		modBakePresent = fs.bakePresent;
		qInfo().noquote() << fs.line;
		if ( fs.adopt ) {
			spec.plugins = fs.plugins;
			cellFarLodAdoptResources( fs.resources );
		} else if ( !fs.bakeRoots.isEmpty() ) {
			// lane FARLOD3: not adopted, but the far field's own models still need the bake's roots
			const QString lend = cellFarLodLendRoots( fs.bakeRoots );
			liveLend = liveLendKey( fs.bakeRoots );
			farStackNote += QStringLiteral( "  " ) + lend + QLatin1Char( '\n' );
			qInfo().noquote() << lend;
		}
	}

	/* lane MODPICK1: the mod picker's saved choice for this worldspace (cellmodpick.h). Untouched, the plugin
	 * list stays exactly what the cell asked (after the FARLOD stack step); the line goes in the notes, and
	 * the view's note is set once the document is built. */
	QString modPickNote, modPickViewNote;
	if ( !chunk )
		cellModPickSetViewNote( nif, QString() );
	if ( !chunk && !spec.interior ) {
		const CellModApplied mp = cellModPickApply( spec.world, spec.plugins, modLodb, modBakePresent );
		spec.plugins = mp.plugins;
		cellFarLodAdoptResources( mp.resources );	// the MO2 profile's stack when the choice came from it
		modPickNote = QStringLiteral( "  " ) + mp.line + QLatin1Char( '\n' );
		modPickViewNote = mp.viewNote;
		qInfo().noquote() << mp.line;
	}
	const QByteArray liveStack = chunk ? QByteArray() : liveSigOf( spec.plugins, lodgenResources(), liveLend );
	if ( !chunk )	// lane SKYTOP1: the lookdev weather reads this cell's load order (the last plugin's record wins)
		wwLookdevSetCellPlugins( spec.plugins.split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) );
	// lane BAKEINCR1: an exterior bake records every asset it reads, for its bakeincr.txt
	const bool incrRecord = !chunk && bakeExterior && !spec.interior && wwEnvIsEmpty( "WW_CELL_GI_FROM" );
	if ( incrRecord )
		ProbeIncr::startRecording();

	// ---- the worldspace, or (lane PRTP1) one interior cell
	// lane CELLSTREAM1: shared, so a streaming open can hand it on; a streamed cell reads the open's
	/* lane FARLIGHTMAP1: WW_CELL_KEEP_WORLD=1 (a whole-map walk) keeps the last worldspace index and hands it to the next
	 * open of the same plugins and world: the index is read-only to a build (its memo caches are pure), as a stream's is */
	static std::shared_ptr<EsmWorld> keptWorld;
	static QString keptKey;
	const bool keepWorld = !chunk && !spec.interior && qEnvironmentVariable( "WW_CELL_KEEP_WORLD" ) == QLatin1String( "1" );
	const QString worldKey = spec.plugins + QLatin1Char( '|' ) + spec.world.toLower();
	const bool worldKept = keepWorld && keptWorld && keptKey == worldKey;
	if ( !keepWorld )
		keptWorld.reset();
	std::shared_ptr<EsmWorld> worldOwn = chunk ? chunk->seed->world : worldKept ? keptWorld : std::make_shared<EsmWorld>();
	EsmWorld & world = *worldOwn;
	QString loadError;
	if ( chunk ) {
		// indexed once by the open
	} else if ( worldKept ) {
		// indexed by an earlier cell of the walk
	} else if ( spec.interior ) {
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
		if ( keepWorld ) {
			keptWorld = worldOwn;
			keptKey = worldKey;
		}
	}
	/* lane LANDSPEC7: the game loads a fixed world's cell only within uGridsToLoad / 2 of WCTR, so the open's block is
	 * fitted inside WCTR +- 2 (src/landlodspec.h fixedBlock) before the camera, the far rings, the land-LOD block and
	 * the streamer read it. Red WW_LANDSPEC_RED=fixedopen: the asked block. */
	QString fixedOpenNote;
	if ( !chunk && !spec.interior && LandLodSpec::fixedWorld( world.worldFlags() ) ) {
		const int ax = spec.cx, ay = spec.cy, an = spec.n;
		int fx = ax, fy = ay, fn = an;
		const bool moved = LandLodSpec::fixedBlock( world.fixedCentreX(), world.fixedCentreY(),
			LandLodSpec::kFixedGridHalf, fx, fy, fn );
		const bool red = wwEnv( "WW_LANDSPEC_RED" ) == "fixedopen";
		if ( moved && !red ) {
			spec.cx = fx;
			spec.cy = fy;
			spec.n = fn;
		}
		fixedOpenNote = QStringLiteral( "  landspec: fixed world, WCTR %1,%2 +-%3: asked %4,%5 n%6 -> %7\n" )
			.arg( world.fixedCentreX() ).arg( world.fixedCentreY() ).arg( LandLodSpec::kFixedGridHalf )
			.arg( ax ).arg( ay ).arg( an )
			.arg( !moved ? QStringLiteral( "inside, kept" )
				: red ? QStringLiteral( "RED fixedopen: kept, %1,%2 n%3 is what the game loads" ).arg( fx ).arg( fy ).arg( fn )
				: QStringLiteral( "%1,%2 n%3" ).arg( fx ).arg( fy ).arg( fn ) );
		std::fprintf( stderr, "%s", qPrintable( fixedOpenNote.trimmed() + QLatin1Char( '\n' ) ) );
	}
	// lane CELLSPAWN1: where the window stands the camera when this open is a tab's first document (src/cellspawn.h)
	QString spawnNote = fixedOpenNote;
	if ( !chunk ) {
		const CellSpawn spawn = cellSpawnCompute( world, spec );
		cellSpawnPublish( nif, spawn );
		spawnNote += QStringLiteral( "  %1\n" ).arg( spawn.line() );
	}

	/* WHERE MODELS COME FROM. The spec's data root when it names one, else the
	 * FIRST entry of the resource stack the LOD panel already set -- one
	 * resolver, the generator's, never a second one. The notes line says which
	 * of the two answered, because "the models are all missing" and "the data
	 * root was empty" look identical in a picture. */
	QString dataRoot = chunk ? chunk->seed->dataRoot : spec.dataRoot;
	QString dataRootSource = QStringLiteral( "the spec" );
	if ( dataRoot.isEmpty() && !chunk ) {
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
	// lane CELLSTREAM1: what the streamer is handed: the resolved spec (plugins after the far stack and the
	// mod pick), the data root, the world. The open itself welds no cell: an empty rectangle, no ground.
	auto seedPtr = std::make_shared<CellStreamSeed>();
	CellStreamSeed & seed = *seedPtr;
	if ( streamInit ) {
		seed.world = worldOwn;
		seed.spec = spec;
		seed.dataRoot = dataRoot;
		x1 = x0 - 1;
		y1 = y0 - 1;
		spec.terrain = spec.water = spec.grid = false;
	}
	CellSpeed::mark( "plugin index" );
	// lane CELLSTREAM1: a streamed cell fills its own tables; the view's are merged from the resident cells
	CellRefTable & refTable = chunk ? chunk->refs : cellRefTableMutable();

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
		quint32 xemi = 0;   //!< lane LIGHTS2 (XEMI1): XEMI's LIGH colour, 0x01RRGGBB (bit 24 = set); 0 none or a region
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
		bool cable = false;   //!< lane CABLES1: a placed cable (BNDS base); its tube is built (src/cellcable.h)
		WwCableSpec cableSpec;
		bool loadDoor = false;   //!< lane ROOMCLAMP1: a door with a teleport (XTEL)
	};
	QVector<Placement> placements;
	QHash<QString, int> skippedByType;
	int refsRead = 0, refsDeleted = 0, refsHidden = 0, refsMarker = 0, refsNoBase = 0;
	// lane CABLES1: references carrying XBSD, on a BNDS base, placed as cables, detached, XBSD on another base
	int cableXbsd = 0, cableBnds = 0, cablePlaced = 0, cableDetached = 0, cableOtherBase = 0;
	QSet<quint32> seenRefs;

	/* lane CELLWORK1: the reference model is emptied HERE, immediately before
	 * the reads that fill it, and not beside the pick table's clear further
	 * down -- the REFR loop runs before that point, so clearing there would
	 * throw away every row this build had just recorded. */
	refTable.clear();

	/* lane PRTP1: the cell's LIGHTS. Every placed LIGH that is not deleted, whether
	 * or not it has a model and whether or not it starts disabled -- the light
	 * list is what the cell CONTAINS, as the ref table is. WW_CELL_LIGHTS dumps it
	 * and the gate compares it with an independent walk of the plugin. */
	QVector<EsmRefr> lightRefs;
	QHash<quint32, bool> lightStartOff;	// lane GAPFIX1: each placed light's start state, its enable parent's included
	std::vector<CellDecalRef> decalRefs;   // lane PLACED1: placed decals, projected after the weld
	CellActors actors( world, dataRoot );  // lane PLACED1: placed actors, built at rest
	CellWeapons weapons( world, dataRoot );  // lane MISS1 (GAPLOOK2): placed weapons with their default parts
	int actorPlacements = 0;
	QHash<quint32, EsmRefr> primRefs;	// lane HEMI1: the refs carrying a primitive (a light's LightBoxLink target)
	/* lane MISS1: a reference with an enable parent starts in the PARENT's state, inverted when it is
	 * "opposite"; the parent's own starting state is a plugin fact (its initially-disabled flag, or its own
	 * parent in turn). A parent outside the loaded block counts as enabled. WW_CELL_REFS_RED=parent is the
	 * gate's red control: the old rule, which never looked at the parent. */
	struct EnableFact { bool off; quint32 parent; bool opposite; };
	QHash<quint32, EnableFact> enableFacts;
	const QByteArray refsRed = wwEnv( "WW_CELL_REFS_RED" );
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
	/* lane LIGHTBOOTH1 (the light test booth; a harness pin, never a user toggle): WW_CELL_BOOTH = <ref form id
	 * hex>[,...] keeps ONLY those references of the opened cell -- meshes and lights alike -- so one fixture and its
	 * own lights stand alone (the booth adds the flat ground and the even air: WW_LOOKDEV_GROUND, WW_CELL_BOOTH_AIR).
	 * Every other reference is dropped before anything reads it: no row, no light, no shadow caster, no GI soup.
	 * Unset: no change. */
	QSet<quint32> boothRefs;
	for ( const QByteArray & t : wwEnv( "WW_CELL_BOOTH" ).split( ',' ) ) {
		bool ok = false;
		const quint32 id = t.trimmed().toUInt( &ok, 16 );
		if ( ok && id )
			boothRefs.insert( id );
	}
	int boothDropped = 0;
	QSet<quint32> boothSeen;
	/* lane LIGHTS2 (XEMI1): a reference's XEMI names a LIGH (its colour; the game returns the light's colour member, read
	 * here as DATA bytes / 255) or a REGN (a region colour the plugin does not state for FO4: counted, not drawn). */
	int xemiRefs = 0, xemiLigh = 0, xemiOther = 0, xemiRegn = 0;
	auto xemiOf = [&]( quint32 form ) -> quint32 {
		if ( !form )
			return 0;
		xemiRefs++;
		const EsmLight & el = world.light( form );
		if ( !el.exists ) {
			// lane LIGHTS3 (XEMI-REGN): a region gives its weather's Effect Lighting at the hour (look row of its own)
			const ESMFile::ESMRecord * rr = world.plugin() ? world.plugin()->findRecord( form ) : nullptr;
			if ( rr && *rr == "REGN" ) {
				xemiRegn++;
				return wwCellXemiRegionCode( form );
			}
			xemiOther++;
			return 0;
		}
		xemiLigh++;
		return 0x01000000U | ( quint32( el.color[0] ) << 16 ) | ( quint32( el.color[1] ) << 8 ) | quint32( el.color[2] );
	};
	auto pushRefr = [&]( const EsmRefr & r, int cellX, int cellY, bool persistent ) {
		if ( !boothRefs.isEmpty() ) {
			if ( !boothRefs.contains( r.formID ) ) {
				boothDropped++;
				return;
			}
			boothSeen.insert( r.formID );
		}
		refsRead++;
		if ( !r.deleted && std::memcmp( &r.baseType, "LIGH", 4 ) == 0 ) {
			lightRefs.append( r );
			lightStartOff.insert( r.formID, refsRed == "lightparent" ? r.initiallyDisabled : startsDisabled( r ) );	// red: the old rule
		}
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
		const int refRow = refTable.append( ref );

		if ( r.deleted ) {
			refsDeleted++;
			refTable.setFate( refRow, CellRefFate::Deleted );
			return;
		}
		if ( !r.base ) {
			refsNoBase++;
			refTable.setFate( refRow, CellRefFate::NoBase );
			return;
		}
		// lane MISS1: the state the plugin starts the reference in (startsDisabled above); what a quest does
		// to an enable marker later is a runtime fact and is not simulated. The panel shows both facts.
		const bool disabled = startsDisabled( r );
		if ( disabled && !spec.showDisabled ) {
			refsHidden++;
			refTable.setFate( refRow, CellRefFate::Disabled );
			return;
		}
		const EsmLodBase & lb = world.lodBase( r.base );
		const bool isScol = std::memcmp( &r.baseType, "SCOL", 4 ) == 0;
		/* lane CABLES1: A CABLE. Its base (BNDS) has no model: the tube is built from the reference's XBSD and
		 * the base's look, in the reference's frame, and then placed like any static. WW_CELL_CABLES_RED=off
		 * leaves it to the "no model" row below, as before the lane. */
		if ( r.hasSpline )
			cableXbsd++;
		if ( std::memcmp( &r.baseType, "BNDS", 4 ) == 0 ) {
			cableBnds++;
			if ( wwCellCablesOn() && r.hasSpline && world.bendable( r.base ).exists ) {
				Placement out;
				out.base = r.base;
				out.pos = Vector3( r.pos[0], r.pos[1], r.pos[2] );
				out.rot.fromEuler( -r.rot[0], -r.rot[1], -r.rot[2] );
				out.scale = r.scale;
				out.ref = r.formID;
				out.cellX = cellX;
				out.cellY = cellY;
				out.persistent = persistent;
				out.disabled = disabled;
				for ( int k = 0; k < 3; k++ ) {
					out.storedRot[k] = r.rot[k];
					out.cableSpec.half[k] = r.splineHalf[k];
				}
#ifdef ESM_HAS_CELL_FIELDS
				out.layerForm = r.layer;
				out.enableParentForm = r.enableParent;
				out.enableParentOppositeFlag = r.enableParentOpposite;
#endif
				out.cable = true;
				out.cableSpec.slack = r.splineSlack;
				out.cableSpec.thickness = r.splineThickness;
				out.cableSpec.detached = r.splineDetached;
				cablePlaced++;
				cableDetached += r.splineDetached ? 1 : 0;
				placements.append( out );
				return;
			}
		} else if ( r.hasSpline ) {
			cableOtherBase++;   // an XBSD on a base that is not a BNDS (vanilla: STATs with zero extents): its own model
		}
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
			refTable.setFate( refRow, CellRefFate::NoModel );
			return;
		}
		if ( !isScol && !spec.showMarkers && isMarkerModel( lb.model ) ) {
			refsMarker++;
			refTable.setFate( refRow, CellRefFate::Marker );
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
					out.xemi = xemiOf( r.emittance );
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
		out.xemi = xemiOf( r.emittance );
		out.loadDoor = r.teleport != 0;
		placements.append( out );
	};

	QVector<EsmRefr> actorRefs;   // lane PLACED1
	if ( spec.interior ) {
		actorRefs = actors.references();
	} else if ( wwLookOn( WwLook::ActorsOutdoors ) ) {
		// lane GAPLOOK2: the block's own cells, then the persistent cell's actors standing in the block
		for ( int y = y0; y <= y1; y++ )
			for ( int x = x0; x <= x1; x++ )
				actorRefs += actors.referencesIn( world.cellChildGroups( x, y ) );
		const float box[4] = { float( x0 ) * CELL_UNITS, float( y0 ) * CELL_UNITS, float( x1 + 1 ) * CELL_UNITS,
			float( y1 + 1 ) * CELL_UNITS };
		actorRefs += actors.referencesIn( world.persistentChildGroups(), box );
	}
	// red WW_CELL_ACTORS_OUT_RED=lift (read per build): outdoor actors stand 256 units up
	const float actorLift = !spec.interior && wwEnvStr( "WW_CELL_ACTORS_OUT_RED" ) == QLatin1String( "lift" ) ? 256.0f : 0.0f;
	auto pushActor = [&]( const EsmRefr & r, int cellX, int cellY ) {
		Placement out;
		if ( !actors.place( r, startsDisabled( r ) && !spec.showDisabled, out.actorKey, out.scale ) )
			return;
		out.base = r.base;
		out.pos = Vector3( r.pos[0], r.pos[1], r.pos[2] + actorLift );
		out.rot.fromEuler( -r.rot[0], -r.rot[1], -r.rot[2] );
		out.ref = r.formID;
		out.cellX = cellX;
		out.cellY = cellY;
		for ( int k = 0; k < 3; k++ )
			out.storedRot[k] = r.rot[k];
		placements.append( out );
		actorPlacements++;
	};
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
			// lane CELLSTREAM1: a streamed cell takes the facts of the 5x5 round it, as the fixed block does
			const int fx0 = chunk ? chunk->facts[0] : x0, fy0 = chunk ? chunk->facts[1] : y0;
			const int fx1 = chunk ? chunk->facts[2] : x1, fy1 = chunk ? chunk->facts[3] : y1;
			for ( int y = fy0; y <= fy1; y++ )
				for ( int x = fx0; x <= fx1; x++ )
					if ( world.hasCell( x, y ) )
						note( world.refrs( x, y ) );
			/* lane GLOWHAZE1: the world's persistent refs are always loaded in the game, wherever they stand, so a
			 * parent outside the block still gives its state (Sanctuary: five MistLargeRoundDustyBrightish02 under the
			 * initially-disabled marker VideoTempClouds one cell north drew as a white glow). Red
			 * WW_CELL_REFS_RED=persistbox: only the block's persistent refs, as before. */
			if ( refsRed == "persistbox" )
				note( world.persistentRefrsIn( float( fx0 ) * CELL_UNITS, float( fy0 ) * CELL_UNITS,
					float( fx1 + 1 ) * CELL_UNITS, float( fy1 + 1 ) * CELL_UNITS ) );
			else
				note( world.persistentRefrsIn( -1e30f, -1e30f, 1e30f, 1e30f ) );
			note( actorRefs );   // lane GAPLOOK2: outdoor actors (empty with the row off)
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
		refTable.addCell( block );
		for ( const EsmRefr & r : world.interiorRefrs() )
			pushRefr( r, 0, 0, false );
		/* lane PLACED1: the placed actors. They are not REFRs: they stay out of the reference
		 * list, the REFR counts and the placement dump, and have their own census line. */
		for ( const EsmRefr & r : actorRefs )
			pushActor( r, 0, 0 );
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
				refTable.addCell( block );

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
		// lane GAPLOOK2: outdoor actors, as the interior's (not REFRs: their own census line)
		for ( const EsmRefr & r : actorRefs )
			pushActor( r, int( std::floor( r.pos[0] / CELL_UNITS ) ), int( std::floor( r.pos[1] / CELL_UNITS ) ) );
	}

	/* Per-cell counts are a WALK of the reference table, taken once here, after
	 * every read and every fate is settled. They are not a second set of
	 * counters incremented alongside the first: a walk of the rows the list
	 * shows cannot disagree with the rows the list shows. */
	refTable.tallyCells();

	// ---- the document
	if ( !chunk && !nif->createNew( 0x14020007, 12, 130 ) )
		return fail( QStringLiteral( "could not create a Fallout 4 document" ) );
	if ( !chunk )
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
	QModelIndex iRoot;
	if ( !chunk ) {
		iRoot = nif->insertNiBlock( QStringLiteral( "NiNode" ) );
		nif->set<QString>( iRoot, "Name", QStringLiteral( "%1 %2,%3 %4x%4" )
			.arg( spec.world ).arg( spec.cx ).arg( spec.cy ).arg( spec.n ) );
		nif->set<quint32>( iRoot, "Flags", 14 );
		nif->set<float>( iRoot, "Scale", 1.0f );
	}

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

	if ( chunk )
		origin = chunk->seed->origin;   // lane CELLSTREAM1: every streamed cell shares the open's origin
	CellPickTable & picks = chunk ? chunk->picks : cellPickTableMutable();
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
	std::vector<ProbePoint> liveProbes;	// lane LIVESET1: the placed probes, for a live Show probes
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
	int decalFlagBuckets = 0, decalFlagShapes = 0;   //!< lane GAPFIX1: lighting buckets keeping the model's decal flags
	// lane GAPFIX1 red (gates only): the welded lighting shapes drop the model's decal flags again
	const bool fxdNoFlag = wwEnv( "WW_CELL_FXD_RED" ).trimmed() == "noflag";
	/* Lane GLOW1: billboard shapes turned to the camera, and those welded flat (the red
	 * WW_CELL_GLOW_RED=1 welds all of them, as before; past the cap the rest weld flat too). */
	const bool glowRed = wwEnvInt( "WW_CELL_GLOW_RED" ) != 0;
	const int glowCap = 8192;
	int billboardShapes = 0, billboardFlat = 0;
	/* Lane FXLIT1: in an interior a lit effect shape (the effect-lighting flag) takes a bucket per placement,
	 * because the game lights it with its MODEL's four placed lights (src/gl/cellfxlit.h). */
	/* lane SUNCELL1 final: exteriors too. The game's lit effect constants (its lit-effect setup, lit branch)
	 * take the scene's directional (the sun outdoors) and the placed lights near the model, inside or out. */
	const bool fxLitOn = spec.overlay == CellOverlay::None;
	const int fxLitCap = 4096;
	int fxLitModels = 0, fxLitShapes = 0;
	QHash<QString, std::array<float, 4>> fxLitBound;   // a loaded model's bounding sphere, model space
	/* lane LIGHTS2 (XEMI1): a placement's emittance colour on the shapes that ask for it (External Emittance): a lighting
	 * shape's emissive colour is replaced, an UNLIT effect is tinted (lit effect variants never read it). Read at build. */
	const bool xemiOn = wwLookOn( WwLook::PlacementEmittance ) && spec.overlay == CellOverlay::None;
	const bool xemiRegnOn = wwLookOn( WwLook::RegionEmittance ) && spec.overlay == CellOverlay::None;   // lane LIGHTS3
	const bool xemiGiOn = wwLookOn( WwLook::BounceEmittance );   // lane LIGHTS5: read once; the other state rides the soup
	const bool xemiBuild = spec.overlay == CellOverlay::None;   // lane LIGHTS4: registered always; the rows are read at draw time
	int xemiShapesLit = 0, xemiShapesFx = 0, xemiNoFlag = 0;
	int xemiLitFx = 0;   // lane REDFIX1: lit effect shapes on XEMI placements (skipped by rule, now counted)
	if ( !chunk ) {
		wwCellFxLitBegin( nif );
		wwCellXemiBegin( nif );   // lane LIGHTS2 (XEMI1)
		wwCellCullBegin( nif );   // lane SUNCELL1
	}

	/* lane SUNCELL1: PREVIS AND OCCLUSION, READ. The plugin's own visibility data for the loaded cells:
	 *  - each CELL's RVIS (the cell whose previs file covers it: an exterior 3x3 block shares one), its VISI / PCMB
	 *    build stamps, XCRI (the precombined meshes and the refs they replace) and XPRI (the refs previs took in);
	 *  - each block's previs file `vis/<plugin>/<RVIS>.uvd`: its header (magic, size, bounds) and the build string
	 *    (an Umbra tome; lane UMBRA1 decodes it and queries it in gl/cellumbra, see gl/cellcull.h);
	 *  - the occlusion primitives the game culls with (PlaneMarker planes and boxes) and, indoors, the rooms
	 *    (RoomMarker boxes, XLRM linked rooms) and portals (PortalMarker planes, XPOD rooms): handed to gl/cellcull,
	 *    which the Previs row applies to the camera pass only (casters always cast). */
	std::vector<ProbeVolumeMarker> volMarkers;   // lane VOLFIT1: rooms and portals, evidence for the Space stage
	if ( !chunk ) {   // lane CELLSTREAM1: previs for streamed cells is an open item
		WwPrevisScene pv;
		int planes = 0, boxes = 0, linked = 0, toOutside = 0, prevDisabled = 0;
		QList<quint32> primKeys = primRefs.keys();
		std::sort( primKeys.begin(), primKeys.end() );
		for ( const quint32 key : primKeys ) {
			const EsmRefr & r = primRefs[key];
			const bool occ = r.base == 0x17U && ( r.primType == 1 || r.primType == 3 );	// PlaneMarker
			const bool room = r.base == 0x1FU && r.primType == 1;					// RoomMarker
			const bool portal = r.base == 0x20U && r.primType == 3;				// PortalMarker
			if ( !occ && !room && !portal )
				continue;
			if ( startsDisabled( r ) ) {
				prevDisabled++;	// a disabled marker culls nothing in game
				continue;
			}
			WwPrevisBox x;
			x.c = Vector3( r.pos[0], r.pos[1], r.pos[2] );	// finalfix: world units -- scene->view takes world points (cell shapes carry the origin as their translation)
			cellPrimAxes( r.rot, x.ax );
			for ( int k = 0; k < 3; k++ )
				x.half[k] = std::max( std::abs( r.primHalf[k] ) * r.scale, 1.0f );
			x.type = int( r.primType );
			x.ref = r.formID;
			x.from = r.portalFrom;
			x.to = r.portalTo;
			for ( const quint32 l : r.linkedRooms )
				x.linked.push_back( l );
			if ( room || portal ) {
				ProbeVolumeMarker vm;
				vm.type = room ? 1 : 2;
				for ( int k = 0; k < 3; k++ ) {
					vm.c[k] = x.c[k];
					vm.half[k] = x.half[k];
					for ( int q = 0; q < 3; q++ )
						vm.ax[k][q] = x.ax[k][q];
				}
				vm.ref = x.ref;
				vm.from = x.from;
				vm.to = x.to;
				volMarkers.push_back( vm );
			}
			if ( occ ) {
				( r.primType == 3 ? planes : boxes )++;
				pv.occluders.push_back( std::move( x ) );
			} else if ( room ) {
				linked += int( x.linked.size() );
				pv.rooms.push_back( std::move( x ) );
			} else {
				toOutside += ( !x.from || !x.to ) ? 1 : 0;
				pv.portals.push_back( std::move( x ) );
			}
		}
		fprintf( stderr, "cell previs scene: occluders %d (planes %d, boxes %d), rooms %d (linked %d), portals %d "
			"(to outside %d), %d disabled markers left out; Previs row %s%s\n", int( pv.occluders.size() ), planes,
			boxes, int( pv.rooms.size() ), linked, int( pv.portals.size() ), toOutside, prevDisabled,
			wwCellPrevisOn() ? "on" : "off", wwCellPrevisRed() == 1 ? " RED=casters" : "" );
		wwCellPrevisSet( nif, std::move( pv ) );

		// the cells' previs fields, grouped by the block that holds their visibility
		std::map<quint32, std::vector<std::pair<int, int>>> blocks;
		// lane UMBRA1: each block's XCRI refs with their cell's code (bits 14..23 of a combined id; interiors 0)
		std::map<quint32, std::vector<std::pair<std::uint32_t, std::uint32_t>>> blockComb;
		std::vector<WwUmbraBlockIn> umbraIn;
		int cellsRead = 0, cellsRvis = 0, cellsVisi = 0, cellsPcmb = 0;
		quint64 combRefs = 0, combMeshes = 0, previsRefs = 0;
		auto readCell = [&]( quint32 form, int cx, int cy ) {
			const EsmCellPrevis c = world.cellPrevis( form );
			if ( !c.exists )
				return;
			cellsRead++;
			cellsVisi += c.hasVisi ? 1 : 0;
			cellsPcmb += c.hasPcmb ? 1 : 0;
			combRefs += c.combinedRefs;
			combMeshes += c.combinedMeshes;
			previsRefs += c.previsRefs;
			if ( c.rvis ) {
				cellsRvis++;
				blocks[c.rvis].push_back( { cx, cy } );
			} else if ( spec.interior ) {
				blocks[form].push_back( { cx, cy } );	// an interior with no RVIS: its own file, if any
			}
			const quint32 owner = c.rvis ? c.rvis : ( spec.interior ? form : 0U );
			if ( owner ) {	// lane UMBRA1
				const std::uint32_t code = spec.interior ? 0U
					: ( ( std::uint32_t( cx ) & 31U ) << 19 ) | ( ( std::uint32_t( cy ) & 31U ) << 14 );
				for ( const quint32 cr : c.combinedRefIds )
					blockComb[owner].push_back( { std::uint32_t( cr ), code } );
			}
		};
		if ( spec.interior ) {
			readCell( world.interior().cellForm, 0, 0 );
		} else {
			for ( int y = y0; y <= y1; y++ )
				for ( int x = x0; x <= x1; x++ )
					if ( world.hasCell( x, y ) )
						readCell( world.cellForm( x, y ), x, y );
		}
		fprintf( stderr, "cell previs: %d cells read, %d with RVIS in %d previs blocks, VISI %d, PCMB %d; precombined %llu refs "
			"in %llu meshes; previs refs %llu\n", cellsRead, cellsRvis, int( blocks.size() ), cellsVisi, cellsPcmb,
			(unsigned long long)combRefs, (unsigned long long)combMeshes, (unsigned long long)previsRefs );
		const QStringList plugs = spec.plugins.split( QLatin1Char( ',' ), Qt::SkipEmptyParts );
		static const QRegularExpression verRx( QStringLiteral( "(\\d+\\.\\d+\\.\\d+)" ) );
		for ( const auto & b : blocks ) {
			const quint32 f = b.first;
			const quint32 idx = world.plugin() ? world.plugin()->getFormIDFile( f ) : ( f >> 24 );
			const QString plugin = idx < quint32( plugs.size() ) ? QFileInfo( plugs.at( int( idx ) ).trimmed() ).fileName()
				: QStringLiteral( "Fallout4.esm" );
			QByteArray u;
			QString path;
			for ( const quint32 name : { f, ESMFile::getFormIDLocal( f ) } ) {
				path = QStringLiteral( "vis/%1/%2.uvd" ).arg( plugin ).arg( name, 8, 16, QLatin1Char( '0' ) );
				if ( lodgenReadVisFile( dataRoot, path, u ) && u.size() >= 44 )
					break;
				u.clear();
			}
			if ( u.isEmpty() ) {
				fprintf( stderr, "cell previs block %08X: %d loaded cells; file %s not found\n", f, int( b.second.size() ),
					qPrintable( path ) );
				continue;
			}
			float bb[6];
			for ( int k = 0; k < 6; k++ ) {
				const quint32 w = cellLe32( u, 20 + 4 * k );
				std::memcpy( &bb[k], &w, 4 );
			}
			// the build string: the first printable run of 16+ bytes in the header
			QString build;
			const int lim = std::min<int>( int( u.size() ), 2048 );
			for ( int i = 44, run = 0; i < lim; i++ ) {
				const unsigned char ch = uchar( u.at( i ) );
				if ( ch >= 32 && ch < 127 ) {
					run++;
					continue;
				}
				if ( run >= 16 ) {
					build = QString::fromLatin1( u.constData() + i - run, run );
					break;
				}
				run = 0;
			}
			const QRegularExpressionMatch vm = verRx.match( build );
			bool covers = !spec.interior;
			for ( const auto & c : b.second )
				covers = covers && bb[0] <= float( c.first ) * CELL_UNITS + 1.0f && bb[3] >= float( c.first + 1 ) * CELL_UNITS - 1.0f
					&& bb[1] <= float( c.second ) * CELL_UNITS + 1.0f && bb[4] >= float( c.second + 1 ) * CELL_UNITS - 1.0f;
			fprintf( stderr, "cell previs block %08X: %d loaded cells; file %s %d bytes, magic %08X, size field %u, "
				"tome %s, bounds %.0f,%.0f,%.0f..%.0f,%.0f,%.0f = %.2fx%.2f cells, covers its cells %s; visibility: the "
				"cell umbra lines\n", f, int( b.second.size() ), qPrintable( path ), int( u.size() ), cellLe32( u, 0 ),
				cellLe32( u, 8 ), vm.hasMatch() ? qPrintable( vm.captured( 1 ) ) : "unknown", bb[0], bb[1], bb[2], bb[3], bb[4],
				bb[5], ( bb[3] - bb[0] ) / CELL_UNITS, ( bb[4] - bb[1] ) / CELL_UNITS,
				spec.interior ? "n/a (interior)" : covers ? "yes" : "NO" );
			/* lane SUNCELL1 (last round): THE TOME'S HEADER AND OBJECT TABLE, as the game's Umbra 3.3.17 runtime reads
			 * them (Todd's treat: the Umbra::ImpTome / Umbra::Tome getters; every offset is a getter's displacement,
			 * self-relative offsets from the tome's start, 0 = absent). The object user IDs are form IDs: placed
			 * references, or 0xFD...... ids (the combined meshes'). The body the game's query walks (tiles, their
			 * KD trees, cells, portals, the occlusion raster of Umbra::Query::queryPortalVisibility) is NOT decoded,
			 * so nothing is culled by it; the occluder / room path stays the cull. */
			{
				const auto u32At = [&u]( qint64 o ) -> quint32 { return o >= 0 && o + 4 <= u.size() ? cellLe32( u, int( o ) ) : 0U; };
				const bool magicOk = ( u32At( 0x00 ) & 0xFFFF0000U ) == 0xD6000000U && u32At( 0x08 ) == quint32( u.size() );
				const quint32 nObj = u32At( 0x40 ), oUid = u32At( 0x50 ), oStarts = u32At( 0x4c );
				const quint32 nTiles = u32At( 0x90 ), oCellStarts = u32At( 0x88 );
				quint32 refs = 0, combined = 0;
				bool uidsOk = magicOk && oStarts == 0 && oUid != 0 && qint64( oUid ) + 4 * qint64( nObj ) <= u.size();
				if ( uidsOk )
					for ( quint32 i = 0; i < nObj; i++ )
						( ( u32At( qint64( oUid ) + 4 * i ) >> 24 ) == 0xFDU ? combined : refs )++;
				const quint32 nCells = oCellStarts ? u32At( qint64( oCellStarts ) + 4 * qint64( nTiles ) ) : 0U;
				fprintf( stderr, "cell previs tome %08X: %s objects %u (%u reference ids, %u combined ids), clusters %u, tiles %u "
					"(%u leaf), cells %u, gates %u; body not decoded (tiles, KD trees, portals)\n", f,	// the SUNCELL1 g19 text; lane UMBRA1 decodes the body (cell umbra lines)
					magicOk ? "header read," : "header NOT a 3.x tome,", nObj, refs, combined, u32At( 0x7c ), nTiles,
					u32At( 0x8c ), nCells, u32At( 0x68 ) );
			}
			{	// lane UMBRA1: the whole tome, decoded and queried in gl/cellumbra
				WwUmbraBlockIn ui;
				ui.block = f;
				ui.tome = u;
				auto bc = blockComb.find( f );
				if ( bc != blockComb.end() )
					ui.combined = std::move( bc->second );
				umbraIn.push_back( std::move( ui ) );
			}
		}
		if ( !umbraIn.empty() )
			wwCellUmbraSet( nif, std::move( umbraIn ) );	// lane UMBRA1
	}
	if ( !chunk ) {
		wwCellWaterBegin( nif );   // lane WATER1: forget the last cell's water shapes
		wwCellLandBegin( nif );    // lane TERRBLEND1: forget the last cell's blended-ground shapes
		// lane LANDSPEC2; LANDSPEC3: the band reads the worldspace whose LOD it draws (red ownlod: the child's own)
		wwCellLandOrigin( nif, spec.interior ? QString()
			: wwEnv( "WW_LANDSPEC_RED" ) == "ownlod" ? spec.world : world.lodWorldEdid(), origin[0], origin[1] );
		if ( !spec.interior )	// lane LANDSPEC4: the band window covers the whole near block
			wwCellLandBlock( nif, spec.cx, spec.cy, spec.n / 2 );
		{	// lane LANDSPEC6: a "Fixed Dimensions" world keeps its blend centre on WCTR (red fixedfollow: the camera's)
			const bool fixedWs = !spec.interior && LandLodSpec::fixedWorld( world.worldFlags() );
			wwCellLandFixed( nif, fixedWs, world.fixedCentreX(), world.fixedCentreY() );
			if ( fixedWs )
				std::fprintf( stderr, "landspec: %s is a fixed world: blend centre and streamed block stay on cell %d,%d%s\n",
					qPrintable( spec.world ), world.fixedCentreX(), world.fixedCentreY(),
					wwEnv( "WW_LANDSPEC_RED" ) == "fixedfollow" ? " (RED fixedfollow: they follow the camera)" : "" );
		}
		wwCellGrassBegin( nif );   // lane GRASSMB1: and its grass shapes
	}
	wwCellWaterSetDefault( nif, world.defaultWaterType() );	// lane WATERUNDER5: its placed water reads as sea under it
	QHash<quint32, WwWaterRecord> placedWaterRecs;   // lane WATER1: WNAM form -> its record (form 0 = unreadable)
	int placedWaterShapes = 0;
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
		// lane GAPFIX1: the game honours a model's Decal flag without a material file; keyed only when set
		const quint32 decalBits = ( effect || inlineFx || refract || fxdNoFlag ) ? 0U : ( s.shaderSF1 & 0x0C000000U );
		const QString keyD = decalBits ? key + QStringLiteral( "|D%1" ).arg( decalBits ) : key;
		decalFlagShapes += decalBits ? 1 : 0;
		auto it = buckets.find( keyD );
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
		b.decalSF1 = decalBits;
		decalFlagBuckets += decalBits ? 1 : 0;
		return buckets.insert( keyD, b ).value();
	};

	const bool colouring = spec.overlay != CellOverlay::None;

	/* ---- lane PRTPPLACE: THE PROBE SOUP. What the transfer probes see is the
	 * STATIC world only (docs/PRTP_PLAN.md 2b): actors, havok clutter, FX and
	 * workshop builds are receivers, never occluders. Doors are left out too --
	 * baked open -- and only their boxes are kept, to tag the openings they
	 * stand in. Per shape, effect shaders, alpha-blended glass and decals are
	 * dropped: light passes through all three. */
	const QString probeOut = QString::fromLocal8Bit( wwEnv( "WW_CELL_PROBES" ) );
	const bool probing = !probeOut.isEmpty() || spec.probes;   // the env, or the PRTP band's Place
	ProbeSoup probeSoup;
	int soupRefs = 0, soupShapesDropped = 0;
	/* lane PRTPBAKE: the bake's albedo, one per soup triangle -- the band's Bake, or
	 * WW_CELL_PROBE_BAKE=<folder> (every probing build bakes there, for a headless run).
	 * WW_CELL_PROBE_BAKE_DIR=<folder> only moves the Bake button's folder. */
	const QString bakeEnv = QString::fromLocal8Bit( wwEnv( "WW_CELL_PROBE_BAKE" ) );
	const QString bakeDirEnv = QString::fromLocal8Bit( wwEnv( "WW_CELL_PROBE_BAKE_DIR" ) );
	const bool baking = probing && ( spec.probesBake || !bakeEnv.isEmpty() );
	ProbeAlbedo probeAlb( dataRoot );
	/* lane CAPTURE1: WW_CELL_BAKE_ALBEDO = tri (default, today) | hit | cube picks where the surfels'
	 * albedo and normal come from (ProbeBakeSpec::albedoWay); hit and cube need every object
	 * triangle's UVs, vertex colors, vertex normals and maps in the soup. WW_CELL_BAKE_ALBEDO_RED =
	 * centroid | nofilter, WW_CELL_BAKE_CUBE_FACE = pixels a face (128), WW_CELL_BAKE_CUBE_DUMP=<file>
	 * + WW_CELL_BAKE_CUBE_PROBES=x,y,z;... (the nearest probes' faces): gate and sheet only. */
	const QString albWay = wwEnvStr( "WW_CELL_BAKE_ALBEDO", QStringLiteral( "tri" ) );
	const QString albRed = wwEnvStr( "WW_CELL_BAKE_ALBEDO_RED" );
	const bool wantMat = baking && ( albWay == QLatin1String( "hit" ) || albWay == QLatin1String( "cube" ) );
	const bool wantExtra = wantMat && albWay == QLatin1String( "cube" ) && albRed == QLatin1String( "nofilter" );
	/* lane FIXPACK1 (PBRALBEDO1): a surface a .pbrm serves in the drawn view gives the bake its PBR diffuse,
	 * base x (1 - metallic) (ProbeAlbedo::samplePbrm), not the legacy diffuse slot. Found through the
	 * renderer's own resolve (pbrmResolveForMaterial) with the Name the shape's bucket property carries:
	 * a .bgsm (bucketFor's first choice), materialNameFor'd; any other bucket names no material and draws
	 * legacy. One resolve per material, through the bake's resource stack. The tri way (the default) only;
	 * the hit/cube ways, glass panes and folded decals still read the legacy diffuse.
	 * WW_CELL_BAKE_PBRALBEDO_RED=diffuse reads the legacy diffuse slot for every surface (the gate's red). */
	const bool pbrAlbRed = qEnvironmentVariable( "WW_CELL_BAKE_PBRALBEDO_RED" ) == QLatin1String( "diffuse" );
	QHash<QString, std::shared_ptr<PbrmMaterial>> bakePbrm;   // lower-case Name -> the material, null = legacy
	int albPbrm = 0;
	const QString bakePbrmTracePath = qEnvironmentVariable( "WW_CELL_BAKE_PBRM_TRACE" );   // lane PBRALBEDO2
	QStringList bakePbrmTrace;
	auto bakePbrmFor = [&]( const NativeSrcShape & s ) -> const PbrmMaterial * {
		if ( !isMaterialFile( s.matName ) )
			return nullptr;
		const QString name = materialNameFor( s.matName );
		const QString key = name.toLower();
		auto it = bakePbrm.constFind( key );
		if ( it == bakePbrm.constEnd() ) {
			const PbrmReader reader = [&]( const QString & path, QByteArray & out ) -> bool {
				out.clear();
				QString p = path;
				p.replace( QChar( '\\' ), QChar( '/' ) );
				return lodgenProbeAsset( dataRoot, p, nullptr, nullptr, nullptr, &out ) && !out.isEmpty();
			};
			const PbrmResolveResult r = pbrmResolveForMaterial( name, QString(), QString(), true, reader );
			if ( !bakePbrmTracePath.isEmpty() )   // lane PBRALBEDO2: why each material did or did not resolve
				bakePbrmTrace << QStringLiteral( "%1\t%2\t%3\t%4\t%5" ).arg( s.matName,
					QString::fromLatin1( pbrmRouteName( r.route ) ), r.path, r.refusal, r.tried.join( QStringLiteral( " | " ) ) );
			it = bakePbrm.insert( key, r.route != PbrmRoute::Legacy && r.material.ok
				? std::make_shared<PbrmMaterial>( r.material ) : std::shared_ptr<PbrmMaterial>() );
		}
		// the Legacy menu mode draws every surface the legacy way: the bake reads what is drawn
		return ( pbrAlbRed || pbrmMode() == PbrmModeLegacy ) ? nullptr : it->get();
	};
	/* lane CAPTURE1 (docs/PRTP_PLAN.md 2o, EFX1): a Refraction-flagged lighting shape (Shader Flags 1 bit
	 * 15: the walkway's drip-splash rings) only bends what is behind it in game; its diffuse slot holds a
	 * normal map. It is no surface: it leaves the bake's soup (every albedo way, the cube included) and
	 * is counted. WW_CELL_BAKE_REFRACT_RED=keep keeps it as a solid surface (the gate's red only). */
	const bool refractKeepRed = wwEnvStr( "WW_CELL_BAKE_REFRACT_RED" ) == QLatin1String( "keep" );
	// lane PRTP6: a water-shader shape is dynamic, so it is no surfel surface; WW_CELL_BAKE_WATER_SOUP_RED=1 keeps it (red)
	const bool waterSoupRed = wwEnvStr( "WW_CELL_BAKE_WATER_SOUP_RED" ) == QLatin1String( "1" );
	/* lane GICAL1: the decals and alpha-blended surfaces the soup leaves out still color the surface under them in
	 * game (Vault111Cryo view w1: they change 36% of the surface pixels and darken the drawn albedo by 12%).
	 * They, and the placed decals (TXST, projected below), are folded into the surfel albedo (ProbeSoup::decal).
 * WW_CELL_BAKE_DECALS=0 leaves them out, as before. */
	const bool decalFold = wwEnvStr( "WW_CELL_BAKE_DECALS" ) != QLatin1String( "0" );
	/* lane SMOOTHN1: the bake's hit normal is the surface's smooth one -- the NIF's vertex normals, the ground's
	 * LAND VNML -- blended at the hit (every albedo way); the hit and cube ways also bend it by the shape's
	 * normal map at the hit's UV. WW_CELL_BAKE_SMOOTH=0 (the face normal, as before, byte for byte) and
	 * WW_CELL_BAKE_NMAP=0 (no normal maps) are the gates' reds only. */
	const bool smoothOn = wwEnvStr( "WW_CELL_BAKE_SMOOTH" ) != QLatin1String( "0" );
	const bool nmapOn = smoothOn && wwEnvStr( "WW_CELL_BAKE_NMAP" ) != QLatin1String( "0" );
	int soupSmoothTris = 0, soupLandVnml = 0, soupLandDerived = 0, soupNmapModelSpace = 0;
	const QString landDump = wwEnvStr( "WW_CELL_BAKE_LAND_DUMP" );
	/* lane WATER1: the water surfaces in the placer and the bake (src/probebake.cpp, the water split). The pin
	 * WW_CELL_BAKE_WATER=0 leaves them out, as before (and prints nothing new); WW_CELL_WATER_BAKEDUMP=<file> +
	 * WW_CELL_WATER_BAKEDUMP_PROBES=x,y,z;... (the nearest probes) dumps every ray's split for the gate. */
	const bool bakeWater = wwEnvStr( "WW_CELL_BAKE_WATER" ) != QLatin1String( "0" );
	int soupWaterCells = 0;
	int soupDecalShapes = 0, soupPlacedDecals = 0, soupPlacedDecalTris = 0;
	int soupRefractShapes = 0, soupRefractTris = 0;
	int soupWaterShapes = 0, soupWaterTris = 0;	// lane PRTP6
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
	const QByteArray glassDump = wwEnv( "WW_CELL_PROBE_GLASS" );
	int soupGlassShapes = 0;
	int soupLoadDoors = 0;   // lane ROOMCLAMP1
	/* lane ROOMCLAMP1: WW_CELL_ROOMCLAMP_PIN=off (gates only, never a user toggle) turns every rule of the lane off
	 * (load doors open, the placer's old floors, no probe moved off a back face, no rooms, no eye fallbacks): the
	 * output is byte for byte the exe from before the lane, which the before/after gates compare against. */
	const bool roomclampPin = wwEnv( "WW_CELL_ROOMCLAMP_PIN" ).trimmed() == "off";
	const bool loadDoorRed = roomclampPin || wwEnvStr( "WW_CELL_PROBE_LOADDOOR_RED" ) == QLatin1String( "open" );
	/* lane ALPHATEST1: an alpha-tested shape's triangles carry their UVs and its map's alpha (probemask.h): a
	 * ray through a texel under the material's threshold passes on (the Concord storefront glass is 66-72%
	 * holes). A map whose every texel passes needs no mask. WW_CELL_ALPHATEST_PIN=off (gates only, never a
	 * user toggle) keeps every alpha-tested face solid: byte for byte the exe from before the lane. */
	const bool alphaPin = wwEnv( "WW_CELL_ALPHATEST_PIN" ).trimmed() == "off";
	/* lane GPURELIGHT1: WW_CELL_GI_GPU or WW_CELL_GI_DOORS: the doors' own triangles go into the soup's doorGeom (a
	 * closed door stops light by its faces, lets it through its alpha-test holes, tints it through its panes) */
	const bool giDoorGeom = baking && ( wwCellGiGpuOn() || !wwEnv( "WW_CELL_GI_DOORS" ).isEmpty() || wwDoorLiveWantsGi() );
	// lane DOORLIVE1: the live door is copied in whole after the weld (its own nodes move); the weld leaves it out
	const quint32 doorLiveRef = wwDoorLiveRef();
	int doorLiveShapes = 0;
	int dgShapes = 0, dgMasked = 0, dgGlass = 0, dgGlassUnread = 0, dgDropped = 0, dgScaled = 0, dgTrisScaled = 0;
	// Measurement pin only: WW_CELL_ALPHATEST_FOLIAGE=keep puts landscape\ alpha-tested cards in the soup with their mask.
	const bool foliageKeep = wwEnv( "WW_CELL_ALPHATEST_FOLIAGE" ).trimmed() == "keep";
	/* lane EMISSIVEGI1: glowing surfaces light the bake (probeemit.h). WW_CELL_EMISSIVE_PIN=off (gates only, never
	 * a user toggle) lists no glowing triangle: byte for byte the exe from before the lane. WW_CELL_EMISSIVE_RED =
	 * nomask (the glow map ignored: the whole surface glows) | noflag (Own-Emit ignored): the gate's reds. */
	const bool emitPin = wwEnv( "WW_CELL_EMISSIVE_PIN" ).trimmed() == "off";
	const QByteArray emitRed = wwEnv( "WW_CELL_EMISSIVE_RED" ).trimmed();
	QHash<QString, int> glowMapOf;   // lower-case path -> into probeSoup.glow.maps; -1 = unread
	QHash<QString, int> emitterOf;
	int emShapes = 0, emShapesBlack = 0, emShapesUnread = 0, emShapesNoUv = 0, emOffShapes = 0;
	int emXemiShapes = 0;   // lane LIGHTS3 (XEMI-GI): shapes whose glow is their placement's emittance
	QSet<quint32> emRefs, emRefsNearLight, emNearLights;
	QString emModels;   // census: model, material, map, color x mult, lights within 256 units
	/* lane NEONGI1: the effect surfaces that glow (the soup's veils). WW_CELL_EMISSIVE_RED=noeffects (gates only) lists
	 * none: the soup as before the lane. Census as EMISSIVEGI1's, plus the effect shapes left out and why. */
	int veilShapes = 0;
	QSet<quint32> veilRefs, veilRefsNearLight, veilNearLights;
	QMap<QString, int> veilLeftOut;   // why -> shapes
	QString veilModels;
	auto glowMapIdx = [&]( const QString & tex ) -> int {
		const QString k = tex.toLower();
		auto it = glowMapOf.constFind( k );
		if ( it != glowMapOf.constEnd() )
			return *it;
		ProbeEmit::Map mp;
		int idx = -1;
		if ( probeAlb.rgbBytes( tex, &mp.w, &mp.h, &mp.rgb ) ) {
			idx = int( probeSoup.glow.maps.size() );
			probeSoup.glow.maps.push_back( std::move( mp ) );
			probeSoup.glow.mapNames.push_back( tex.toStdString() );
		}
		glowMapOf.insert( k, idx );
		return idx;
	};
	struct MaskInfo { int map = -1; int minA = 255; };
	QHash<QString, MaskInfo> maskOfTex;
	/* lane ALPHATEST2: the tested alpha is vertex alpha x map alpha x material alpha (probemask.h), so a map with no
	 * texel under 255 still has holes under a scale < 1: such a map is kept on demand (its own entry, read again).
	 * WW_CELL_ALPHATEST_RED=noscale (gates only, never a user toggle): every scale 1, the ALPHATEST1 mask. */
	const bool alphaNoScale = wwEnv( "WW_CELL_ALPHATEST_RED" ).trimmed() == "noscale";
	QHash<QString, int> fullMaskOfTex;   // lower-case path -> into probeSoup.amask.maps (an all-255 map kept for a scale)
	int amShapesScaled = 0, amShapesScaleOnly = 0, amTrisScaled = 0;
	auto fullMaskOf = [&]( const QString & tex ) -> int {
		const QString k = tex.toLower();
		auto it = fullMaskOfTex.constFind( k );
		if ( it != fullMaskOfTex.constEnd() )
			return *it;
		int idx = -1, minA = 255;
		probebvh::AlphaMask::Map mp;
		if ( probeAlb.alphaBytes( tex, &mp.w, &mp.h, &mp.a, &minA ) ) {
			idx = int( probeSoup.amask.maps.size() );
			probeSoup.amask.maps.push_back( std::move( mp ) );
			probeSoup.amask.mapNames.push_back( tex.toStdString() );
		}
		fullMaskOfTex.insert( k, idx );
		return idx;
	};
	QHash<QString, int> maskModelIdx;
	int amShapes = 0, amShapesNoHole = 0, amShapesUnread = 0, amAlbMoved = 0;
	// gate only (tests/spells/alphatest_check.py): one row per kept shape: first soup triangle, count, model, material, map
	const QByteArray soupShapesPath = wwEnv( "WW_CELL_PROBE_SOUP_SHAPES" );
	QString soupShapes;
	auto maskOf = [&]( const QString & tex ) -> MaskInfo {
		const QString k = tex.toLower();
		auto it = maskOfTex.constFind( k );
		if ( it != maskOfTex.constEnd() )
			return *it;
		MaskInfo mi;
		probebvh::AlphaMask::Map mp;
		if ( probeAlb.alphaBytes( tex, &mp.w, &mp.h, &mp.a, &mi.minA ) ) {
			mi.map = int( probeSoup.amask.maps.size() );
			if ( mi.minA >= 255 ) {
				mi.map = -2;   // read, nothing under any threshold: no mask kept
			} else {
				probeSoup.amask.maps.push_back( std::move( mp ) );
				probeSoup.amask.mapNames.push_back( tex.toStdString() );
			}
		}
		maskOfTex.insert( k, mi );
		return mi;
	};
	// lane BAKE4: WW_CELL_PROBE_SOUP_REFS=<tsv> lists every reference the soup took (form, role, base type)
	QString soupRefList;
	const QByteArray soupRefDump = wwEnv( "WW_CELL_PROBE_SOUP_REFS" );
	int albTextured = 0, albUntextured = 0, albLandSplat = 0, albLandFlat = 0, albPalette = 0;
	QHash<qint64, std::array<float, 3>> landAlb;   // 128-unit ground quad -> gamma color, from the splat
	QHash<QString, int> soupSkippedTypes;
	auto soupRole = []( const QString & t ) -> int {   // 0 left out, 1 in the soup, 2 a door
		static const QSet<QString> in { QStringLiteral( "STAT" ), QStringLiteral( "MSTT" ),
			QStringLiteral( "TREE" ), QStringLiteral( "FURN" ), QStringLiteral( "CONT" ),
			QStringLiteral( "ACTI" ), QStringLiteral( "TERM" ), QStringLiteral( "FLOR" ),
			QStringLiteral( "LIGH" ), QStringLiteral( "BNDS" ) };   // lane CABLES1: a cable is a static to the bake
		if ( t == QLatin1String( "DOOR" ) )
			return 2;
		// lane BAKE4: the bake sees the fixed world only. WW_CELL_PROBE_SOUP_RED=items is the gate's red
		// control: pick-up items let into the soup.
		WW_SWITCH_STATIC( bool, itemsRed, wwEnv( "WW_CELL_PROBE_SOUP_RED" ) == "items" );
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
	const bool bakeLean = probing && !bakeEnv.isEmpty() && wwEnvIsEmpty( "WW_CELL_LIT" )
		&& wwEnvIsEmpty( "WW_CELL_GI_DUMP" ) && wwEnv( "WW_CELL_SPEED_RED" ) != "nolean";
	int leanSkipped = 0;
	/* lane FXPARITY2: THE ANIMATED OBJECTS row (src/cellworldanim.h). A placed model that moves alone (lodgenModelMoves)
	 * is left out of the weld and copied whole after it, as the live door; WW_CELL_ANIMOBJ_MAX (256) caps the copies.
	 * The probe soup above still takes its triangles, so the GI is the same. Not in streamed chunks. */
	const bool animObjOn = !chunk && !bakeLean && wwLookOn( WwLook::AnimObjects );
	WW_SWITCH_STATIC( int, animObjCap, wwEnvIsSet( "WW_CELL_ANIMOBJ_MAX" ) ? wwEnvInt( "WW_CELL_ANIMOBJ_MAX" ) : 256 );
	QSet<quint32> animObjRefs, animObjCapped;
	int animObjShapes = 0;
	auto soupLeavesOut = [&]( const Placement & p, bool counted ) -> bool {
		const EsmLodBase & lb = world.lodBase( p.base );
		if ( !p.actorKey.isEmpty() ) {   // a placed actor (lane PLACED1) never enters the soup
			if ( !p.disabled && counted )
				soupSkippedTypes[CellPickTable::typeName( lb.type )]++;
			return true;
		}
		if ( p.cable )
			return p.disabled;   // lane CABLES1: a cable is in the soup as a static is
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
		WW_SWITCH_STATIC( bool, leanRed, wwEnv( "WW_CELL_SPEED_RED" ) == "leanred" );
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
	if ( wwEnv( "WW_CELL_SPEED_RED" ) == "transform" ) {
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
		if ( p.actorKey.isEmpty() && !p.cable && ( !bakeLean || !soupLeavesOut( p, false ) ) )   // an actor or cable is built, not read
			modelsAhead.want( world, p.base, p.swap );
	modelsAhead.load();
	CellSpeed::mark( "models read ahead" );
	AoDecalBuilder aoBuild;	// lane AODECAL1: the copies that get a decal (off: the row's switch)
	/* lane CABLES1: a cable's tube, built once per reference, and its geometry gate: the ends against the
	 * reference's position -/+ its rotated half extents, the sag against 0.7 * L * slack. Every cable is
	 * checked; WW_CELL_CABLES_DUMP=<tsv> writes one row per cable (world ends, the curve's z under the chord's
	 * middle, sag) for the
	 * independent reader to compare against the plugin's own bytes. */
	int cableBuilt = 0, cableWind = 0, cableEndBad = 0, cableSagBad = 0, cableVerts = 0, cableTris = 0;
	float cableEndWorst = 0.0f, cableSagWorst = 0.0f;
	QString cableDump;
	const QString cableDumpPath = wwEnvStr( "WW_CELL_CABLES_DUMP" );
	auto cableBuild = [&]( const Placement & p, std::vector<NativeSrcShape> * shapes ) -> bool {
		const EsmBendable & look = world.bendable( p.base );
		NativeSrcShape s;
		WwCableBuilt cb;
		if ( !wwCellCableShape( look, p.cableSpec, &s, &cb ) || cb.rings < 2 )
			return false;
		cableBuilt++;
		cableWind += cb.wind && wwCellCablesWind() ? 1 : 0;
		cableVerts += int( s.geom.pos.size() / 3 );
		cableTris += int( s.geom.tris.size() / 3 );
		auto world3 = [&]( size_t i ) {
			return p.pos + p.rot * ( Vector3( cb.centres[i * 3], cb.centres[i * 3 + 1], cb.centres[i * 3 + 2] ) * p.scale );
		};
		const Vector3 he( p.cableSpec.half[0], p.cableSpec.half[1], p.cableSpec.half[2] );
		const Vector3 wantA = p.pos - p.rot * ( he * p.scale ), wantB = p.pos + p.rot * ( he * p.scale );
		const Vector3 gotA = world3( 0 ), gotB = world3( size_t( cb.rings - 1 ) );
		const float endErr = qMax( ( gotA - wantA ).length(), ( gotB - wantB ).length() );
		const float sagErr = std::fabs( cb.sagMeasured - cb.sagExpected );
		const float sagTol = qMax( 0.5f, 0.01f * cb.sagExpected );
		cableEndWorst = qMax( cableEndWorst, endErr );
		cableSagWorst = qMax( cableSagWorst, sagErr );
		cableEndBad += endErr > 0.01f ? 1 : 0;
		cableSagBad += sagErr > sagTol ? 1 : 0;
		if ( !cableDumpPath.isEmpty() ) {
			std::vector<float> wc;
			wc.reserve( size_t( cb.rings ) * 3 );
			for ( int i = 0; i < cb.rings; i++ ) {
				const Vector3 q = world3( size_t( i ) );
				wc.insert( wc.end(), { q[0], q[1], q[2] } );
			}
			const float lowZ = ( gotA[2] + gotB[2] ) * 0.5f - wwCableDropAtMiddle( wc );   // the curve under the chord's middle
			cableDump += QStringLiteral( "%1\t%2\t%3\t%4 %5 %6\t%7 %8 %9\t%10\t%11\t%12\t%13\t%14\t%15\n" )
				.arg( p.ref, 8, 16, QLatin1Char( '0' ) ).arg( look.edid ).arg( cb.wind ? 1 : 0 )
				.arg( double( gotA[0] ), 0, 'f', 3 ).arg( double( gotA[1] ), 0, 'f', 3 ).arg( double( gotA[2] ), 0, 'f', 3 )
				.arg( double( gotB[0] ), 0, 'f', 3 ).arg( double( gotB[1] ), 0, 'f', 3 ).arg( double( gotB[2] ), 0, 'f', 3 )
				.arg( double( lowZ ), 0, 'f', 3 ).arg( double( cb.sagMeasured ), 0, 'f', 3 )
				.arg( double( cb.sagExpected ), 0, 'f', 3 ).arg( cb.rings ).arg( cb.slices )
				.arg( double( cb.tiles ), 0, 'f', 4 );
		}
		shapes->push_back( std::move( s ) );
		return true;
	};
	for ( const Placement & p : placements ) {
		const EsmLodBase & lb = world.lodBase( p.base );
		const bool isActor = !p.actorKey.isEmpty();   // lane PLACED1
		const bool isWeap = !isActor && !p.cable && !lb.model.isEmpty() && weapons.wants( p.base );   // lane MISS1 (GAPLOOK2)
		// lane CABLES1: each cable's tube is its own (the key names the reference)
		const QString model = isActor ? p.actorKey
			: p.cable ? QStringLiteral( "cable|%1" ).arg( p.ref, 8, 16, QLatin1Char( '0' ) )
			: isWeap ? QStringLiteral( "weap|%1" ).arg( p.base, 8, 16, QLatin1Char( '0' ) ) : lb.model;
		if ( model.isEmpty() ) {
			skippedByType[CellPickTable::typeName( lb.type )]++;
			continue;
		}
		/* SKY CARDS ARE NOT WORLD GEOMETRY (lane PRTPPLACE, 2026-09-30). The distant cloud
		 * cards (Sky\CloudDistant*) are placed STATs the game draws in its sky layer, behind
		 * everything. Drawn as world geometry, one card hid the whole of Concord behind a grey
		 * sheet (bungo's "wedges"; measured: hiding it alone uncovers the town). Hidden and
		 * counted; WW_CELL_SKY=1 draws them again. */
		WW_SWITCH_STATIC( bool, showSky, !wwEnv( "WW_CELL_SKY" ).isEmpty() );
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
			const bool okLoad = p.cable ? cableBuild( p, &shapes )
				: isActor ? actors.shapes( model, &shapes )
				: isWeap ? weapons.shapes( p.base, &shapes )
				: modelsAhead.take( model, subst, &shapes, &okAhead ) ? okAhead : refsRed != "root"
				? lodgenNativeLoadModelPlaced( const_cast<QString *>( &dataRoot ), model, subst, &shapes )
				: subst
				? lodgenNativeLoadModelSwapped( const_cast<QString *>( &dataRoot ), model, *subst, &shapes )
				: lodgenNativeLoadModel( const_cast<QString *>( &dataRoot ), model, &shapes );
			if ( !okLoad || shapes.empty() ) {
				modelsFailed.insert( mkey );
				continue;
			}
			modelLoads += ( isActor || p.cable ) ? 0 : 1;
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
		WW_SWITCH_STATIC( QString, swapLog, QString::fromLocal8Bit( wwEnv( "WW_CELL_SWAPLOG" ) ) );
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

		const size_t aoTri0 = probeSoup.tris.size() / 9;	// lane AODECAL1: this placement's share of the soup
		for ( size_t si = 0; si < mit.value().size(); si++ ) {
			const NativeSrcShape & s = mit.value()[si];
			const size_t nv = s.geom.pos.size() / 3;
			if ( !nv || s.geom.tris.empty() )
				continue;
			/* lane ALPHATEST2: each vertex's alpha-test scale = its vertex alpha x the material's fAlpha (empty = all 1). The
			 * vertex alpha counts as the renderer counts it (src/gl/renderer.cpp: vertexColorOverride[3] = 1 unless SLSF1
			 * Vertex_Alpha): a colour stream drawn (the .lodo W4 rule), the Vertex_Alpha bit, and not a tree's wind weight.
			 * Returns the smallest scale. Lane GAPFIX1: the soup and the doors' masks both take it. */
			auto alphaScaleOf = [&]( std::vector<float> & sc ) -> float {
				sc.clear();
				if ( alphaPin || alphaNoScale || !s.nearFacts.alphaTest )
					return 1.0f;
				const bool va = s.geom.rgba.size() == nv * 4 && s.geom.vertexAlpha && !s.nearFacts.treeAnim;
				const float ma = std::clamp( s.matAlpha, 0.0f, 1.0f );
				if ( !va && ma == 1.0f )
					return 1.0f;
				sc.assign( nv, ma );
				if ( va )
					for ( size_t v = 0; v < nv; v++ )
						sc[v] = float( s.geom.rgba[v * 4 + 3] ) / 255.0f * ma;
				float lo = 1.0f;
				for ( float x : sc )
					lo = std::min( lo, x );
				if ( lo >= 1.0f )
					sc.clear();
				return lo;
			};
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
					if ( nmapOn && s.geom.tan.size() >= nv * 3 ) {   // lane SMOOTHN1: the tangent the cell view draws with
						const Vector3 wt = p.rot * Vector3( s.geom.tan[vi * 3], s.geom.tan[vi * 3 + 1], s.geom.tan[vi * 3 + 2] );
						const float l = wt.length();
						for ( int k = 0; k < 3; k++ )
							m.t[i * 3 + k] = l > 1e-6f ? qint16( std::lround( std::clamp( wt[k] / l, -1.0f, 1.0f ) * 32767.0f ) ) : 0;
					}
				}
				/* lane SMOOTHN1: the shape's tangent-space normal map (a model-space one, Shader Flags 1 bit 12, is
				 * left out and counted: its channels are model axes, not a tangent frame) */
				if ( nmapOn && !s.tex1.isEmpty() && s.geom.tan.size() >= nv * 3 ) {
					if ( s.shaderSF1 & ( 1U << 12 ) )
						soupNmapModelSpace++;
					else
						m.ntex = matTexOf( s.tex1 );
				}
				return m;
			};
			if ( role == 2 && giDoorGeom && !s.nearFacts.effectShader && s.effectTex0.isEmpty() && !s.nearFacts.decal ) {
				/* lane GPURELIGHT1: this door's triangles, as placed (closed). The bake never sees them (a door only
				 * tags its opening); the relight re-traces whatever crosses the door's box against them. */
				ProbeSoup::DoorGeom & dg = probeSoup.doorGeom;
				const int doorIdx = int( probeSoup.doors.size() );   // this placement's door, tagged after its shapes
				const bool glassy = s.nearFacts.alphaBlend;
				int dMap = -1;
				const quint8 dThr = s.nearFacts.alphaRef;
				std::vector<float> dScale;   // lane GAPFIX1: the door's mask takes the vertex alpha too (as the soup's)
				const float dScaleMin = glassy ? 1.0f : alphaScaleOf( dScale );
				if ( !glassy && !alphaPin && s.nearFacts.alphaTest && s.geom.uv.size() >= nv * 2 ) {
					const MaskInfo mi = maskOf( s.tex0 );
					const bool scaleHole = mi.map != -1 && double( mi.minA ) * double( dScaleMin ) < double( dThr );
					if ( mi.map >= 0 && ( mi.minA < int( dThr ) || scaleHole ) )
						dMap = mi.map;
					else if ( mi.map < -1 && scaleHole )
						dMap = fullMaskOf( s.tex0 );
				}
				if ( dMap < 0 )
					dScale.clear();
				dgScaled += dScale.empty() ? 0 : 1;
				dgShapes++;
				dgMasked += dMap >= 0 ? 1 : 0;
				dgGlass += glassy ? 1 : 0;
				for ( size_t t = 0; t + 2 < s.geom.tris.size(); t += 3 ) {
					float w[9];
					size_t vi[3] = { 0, 0, 0 };
					bool okTri = true;
					for ( int k = 0; k < 3 && okTri; k++ ) {
						vi[k] = size_t( s.geom.tris[t + size_t( k )] );
						okTri = vi[k] < nv;
						if ( !okTri )
							break;
						const Vector3 wp = p.pos + p.rot * ( Vector3( s.geom.pos[vi[k] * 3], s.geom.pos[vi[k] * 3 + 1],
							s.geom.pos[vi[k] * 3 + 2] ) * p.scale );
						for ( int c = 0; c < 3; c++ )
							w[k * 3 + c] = wp[c];
					}
					if ( !okTri )
						continue;
					if ( glassy ) {   // the soup's glass rule: T = 1 - a (1 - c), c and a at the UV centroid
						float uv[2] = { 0.5f, 0.5f }, vc[3] = { 1, 1, 1 }, lin[3] = { 1, 1, 1 }, ta = 1.0f, va = 1.0f;
						if ( s.geom.uv.size() >= nv * 2 )
							for ( int k = 0; k < 2; k++ )
								uv[k] = ( s.geom.uv[vi[0] * 2 + size_t( k )] + s.geom.uv[vi[1] * 2 + size_t( k )]
									+ s.geom.uv[vi[2] * 2 + size_t( k )] ) / 3.0f;
						if ( s.geom.rgba.size() == nv * 4 ) {
							for ( int k = 0; k < 3; k++ )
								vc[k] = ( s.geom.rgba[vi[0] * 4 + size_t( k )] + s.geom.rgba[vi[1] * 4 + size_t( k )]
									+ s.geom.rgba[vi[2] * 4 + size_t( k )] ) / ( 3.0f * 255.0f );
							va = ( s.geom.rgba[vi[0] * 4 + 3] + s.geom.rgba[vi[1] * 4 + 3] + s.geom.rgba[vi[2] * 4 + 3] ) / ( 3.0f * 255.0f );
						}
						if ( !probeAlb.sample( s.tex0, uv[0], uv[1], vc, lin, &ta ) ) {
							dgGlassUnread++;   // no map read: a neutral half-clear pane
							lin[0] = lin[1] = lin[2] = 1.0f;
							ta = 0.5f;
						}
						const float a = std::clamp( ta * va, 0.0f, 1.0f );
						dg.glass.insert( dg.glass.end(), w, w + 9 );
						for ( int c = 0; c < 3; c++ )
							dg.glassT.push_back( quint8( std::lround( std::clamp( 1.0f - a * ( 1.0f - std::clamp( lin[c], 0.0f, 1.0f ) ),
								0.0f, 1.0f ) * 255.0f ) ) );
						dg.glassDoor.push_back( doorIdx );
						continue;
					}
					dg.tris.insert( dg.tris.end(), w, w + 9 );
					dg.door.push_back( doorIdx );
					if ( dMap >= 0 ) {
						dg.amask.triOf.resize( dg.door.size(), -1 );
						probebvh::AlphaMask::Tri mt;
						mt.map = dMap;
						mt.thr = dThr;
						for ( int k = 0; k < 3; k++ ) {
							mt.uv[k * 2] = s.geom.uv[vi[k] * 2];
							mt.uv[k * 2 + 1] = s.geom.uv[vi[k] * 2 + 1];
							if ( !dScale.empty() )
								mt.as[k] = dScale[vi[k]];
						}
						dgTrisScaled += mt.scaled() ? 1 : 0;
						dg.amask.triOf.back() = int( dg.amask.tris.size() );
						dg.amask.tris.push_back( mt );
					}
				}
			}
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
				const bool waterSurf = s.nearFacts.waterShader && !waterSoupRed;
				if ( waterSurf ) {
					soupWaterShapes++;
					soupWaterTris += int( s.geom.tris.size() / 3 );
				}
				if ( refractOnly || waterSurf || s.nearFacts.effectShader || !s.effectTex0.isEmpty() || s.nearFacts.alphaBlend
					|| s.nearFacts.decal || ( soupFoliage && s.nearFacts.alphaTest && !foliageKeep ) ) {
					soupShapesDropped++;
					/* lane NEONGI1: an ADDITIVE effect surface (neon tubes: NeonRed01.BGEM, blend source alpha / one) can only
					 * add light, so it is light; with no face in the soup it lit nothing. Its triangles go into the soup's veils
					 * with the colour the renderer draws (fo4_effectshader.frag in the cell view: map x vertex colour x base
					 * colour x scale, powered 2.2, times its alpha when the blend reads it), the mean of 10 points a triangle. */
					if ( baking && !emitPin && emitRed != "noeffects" && s.nearFacts.effectShader && !refractOnly ) {
						const char * why = nullptr;
						if ( !s.effectMatRead )
							why = "no material read";
						else if ( !s.bakeBlend || s.bakeBlendDst != 0 || !( s.bakeBlendSrc == 6 || s.bakeBlendSrc == 0 ) )
							why = "not additive";
						else if ( s.bakeSoft )
							why = "soft (glow card, mist, beam)";
						else if ( s.effectLit )
							why = "lit effect";
						else if ( s.billboard || s.g2p )
							why = "billboard or palette";
						else if ( s.nearFacts.decal )
							why = "decal";
						const bool srcA = s.bakeBlendSrc == 6;
						const size_t vFirst = probeSoup.veil.count();
						double vArea = 0, vPow[3] = { 0, 0, 0 };
						bool unread = false;
						for ( size_t t = 0; !why && t + 2 < s.geom.tris.size(); t += 3 ) {
							size_t vi[3];
							bool okTri = true;
							float w[9];
							for ( int k = 0; k < 3 && okTri; k++ ) {
								vi[k] = size_t( s.geom.tris[t + size_t( k )] );
								okTri = vi[k] < nv;
								if ( !okTri )
									break;
								const Vector3 wp = p.pos + p.rot * ( Vector3( s.geom.pos[vi[k] * 3], s.geom.pos[vi[k] * 3 + 1],
									s.geom.pos[vi[k] * 3 + 2] ) * p.scale );
								for ( int c = 0; c < 3; c++ )
									w[k * 3 + c] = wp[c];
							}
							if ( !okTri )
								continue;
							double sum[3] = { 0, 0, 0 };
							int got = 0;
							for ( int i = 0; i < 4 && !unread; i++ )
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
									if ( s.effectTex0.isEmpty() ) {   // no map: the shader's white
										for ( int k = 0; k < 3; k++ )
											lin[k] = std::pow( std::max( vc[k] * s.bakeColor[k], 0.0f ), 2.2f );
									} else if ( probeAlb.sample( s.effectTex0, uv[0], uv[1], vc, lin, &ta ) ) {
										for ( int k = 0; k < 3; k++ )
											lin[k] *= std::pow( std::max( s.bakeColor[k], 0.0f ), 2.2f );
									} else {
										unread = true;
										break;
									}
									const double a = srcA ? double( std::clamp( ta * va * s.matAlpha * s.matAlpha, 0.0f, 1.0f ) ) : 1.0;
									for ( int k = 0; k < 3; k++ )
										sum[k] += double( lin[k] ) * a;
									got++;
								}
							if ( unread ) {
								why = "map unread";
								break;
							}
							if ( !got || !( sum[0] + sum[1] + sum[2] > 0 ) )
								continue;
							const double e1[3] = { double( w[3] ) - w[0], double( w[4] ) - w[1], double( w[5] ) - w[2] };
							const double e2[3] = { double( w[6] ) - w[0], double( w[7] ) - w[1], double( w[8] ) - w[2] };
							const double cx = e1[1] * e2[2] - e1[2] * e2[1], cy = e1[2] * e2[0] - e1[0] * e2[2], cz = e1[0] * e2[1] - e1[1] * e2[0];
							const double ar = 0.5 * std::sqrt( cx * cx + cy * cy + cz * cz );
							if ( !( ar > 0 ) )
								continue;
							probeSoup.veil.tris.insert( probeSoup.veil.tris.end(), w, w + 9 );
							for ( int k = 0; k < 3; k++ ) {
								probeSoup.veil.le.push_back( float( sum[k] / got ) );
								vPow[k] += sum[k] / got * ar;
							}
							probeSoup.veil.two.push_back( s.nearFacts.twoSided ? 1 : 0 );
							vArea += ar;
						}
						if ( why ) {   // nothing of this shape stays
							probeSoup.veil.tris.resize( vFirst * 9 );
							probeSoup.veil.le.resize( vFirst * 3 );
							probeSoup.veil.two.resize( vFirst );
							veilLeftOut[QString::fromLatin1( why )]++;
						} else if ( probeSoup.veil.count() == vFirst ) {
							veilLeftOut[QStringLiteral( "black" )]++;
						} else {
							veilShapes++;
							veilRefs.insert( p.ref );
							// the double-count census: placed lights within 256 units of this placement (EMISSIVEGI1's)
							int near = 0;
							for ( const EsmRefr & lr : lightRefs ) {
								const float dx = lr.pos[0] - p.pos[0], dy = lr.pos[1] - p.pos[1], dz = lr.pos[2] - p.pos[2];
								if ( dx * dx + dy * dy + dz * dz <= 256.0f * 256.0f && !startsDisabled( lr ) ) {
									near++;
									veilNearLights.insert( lr.formID );
								}
							}
							if ( near )
								veilRefsNearLight.insert( p.ref );
							if ( veilShapes <= 200 )
								veilModels += QStringLiteral( "    %1 | %2 | ref %3 | triangles %4, area %5 | Le mean %6,%7,%8 | lights within 256: %9\n" )
								.arg( model, s.matName ).arg( p.ref, 8, 16, QLatin1Char( '0' ) ).arg( int( probeSoup.veil.count() - vFirst ) )
								.arg( vArea, 0, 'f', 0 ).arg( vPow[0] / vArea, 0, 'f', 3 ).arg( vPow[1] / vArea, 0, 'f', 3 )
								.arg( vPow[2] / vArea, 0, 'f', 3 ).arg( near );
						}
					}
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
					// lane ALPHATEST1: this shape's alpha-test mask (-1: none, solid as before)
					int amMap = -1, amModel = -1;
					const quint8 amThr = s.nearFacts.alphaRef;
					/* lane ALPHATEST2: each vertex's scale = its vertex alpha x the material's fAlpha. The vertex alpha
					 * counts as the renderer counts it (src/gl/renderer.cpp: vertexColorOverride[3] = 1 unless SLSF1
					 * Vertex_Alpha): a colour stream drawn (the .lodo W4 rule), the Vertex_Alpha bit, and not a tree's
					 * wind weight */
					std::vector<float> amScale;
					const float amScaleMin = alphaScaleOf( amScale );
					if ( !alphaPin && s.nearFacts.alphaTest && s.geom.uv.size() >= nv * 2 ) {
						const MaskInfo mi = maskOf( s.tex0 );
						// lane ALPHATEST2: a hole exists where map alpha x scale < threshold (the smallest scale is a corner's)
						const bool scaleHole = mi.map != -1 && double( mi.minA ) * double( amScaleMin ) < double( amThr );
						if ( mi.map == -1 ) {
							amShapesUnread++;
						} else if ( ( mi.map < 0 || mi.minA >= int( amThr ) ) && !scaleHole ) {
							amShapesNoHole++;
						} else if ( mi.map < 0 && ( amMap = fullMaskOf( s.tex0 ) ) < 0 ) {
							amShapesUnread++;   // read once, unreadable the second time (never seen)
						} else {
							if ( mi.map >= 0 )
								amMap = mi.map;
							amShapesScaleOnly += ( mi.map < 0 || mi.minA >= int( amThr ) ) ? 1 : 0;
							amShapesScaled += amScale.empty() ? 0 : 1;
							amShapes++;
							auto mit2 = maskModelIdx.constFind( model );
							if ( mit2 == maskModelIdx.constEnd() ) {
								mit2 = maskModelIdx.insert( model, int( probeSoup.amask.models.size() ) );
								probeSoup.amask.models.push_back( QString( model ).toStdString() );
							}
							amModel = *mit2;
						}
					}
					/* lane EMISSIVEGI1: does this shape glow, as the renderer draws it (fo4_default.frag: emissive =
					 * hasEmit ? glowColor x glowMult x (hasGlowMap ? glowMap.rgb : 1) : 0)? -1 = no */
					int emIdx = -1;
					float emC[3] = { s.emitColor[0] * s.emitMult, s.emitColor[1] * s.emitMult, s.emitColor[2] * s.emitMult };
					/* lane LIGHTS3 (XEMI-GI): a placement's emittance replaces the emissive colour on the drawn shape, so
					 * the bounce takes it too (a region's colour at the bake's hour: the bake does not follow the clock) */
					float xeC[3];
					// lane LIGHTS5: emA = the colour under the other row state, kept so the row can flip live
					float emA[3] = { emC[0], emC[1], emC[2] };
					if ( p.xemi && s.extEmit && s.ownEmit
						&& !s.nearFacts.effectShader && ( ( p.xemi & 0x02000000U ) ? xemiRegnOn : xemiOn )
						&& wwCellXemiCode( p.xemi, xeC ) ) {
						for ( int k = 0; k < 3; k++ )
							( xemiGiOn ? emC : emA )[k] = xeC[k] * s.emitMult;
						if ( xemiGiOn )
							emXemiShapes++;
					}
					const bool emColor = emC[0] > 0.0f || emC[1] > 0.0f || emC[2] > 0.0f;
					const bool emAlt = emA[0] != emC[0] || emA[1] != emC[1] || emA[2] != emC[2];
					const bool emAltColor = emAlt && ( emA[0] > 0.0f || emA[1] > 0.0f || emA[2] > 0.0f );
					if ( !emitPin && emColor && !s.ownEmit )
						emOffShapes++;
					if ( !emitPin && ( emColor || emAltColor ) && ( s.ownEmit || emitRed == "noflag" ) ) {
						const bool useMap = s.glowFlag && emitRed != "nomask";
						int gm = -1;
						if ( useMap && s.glowTex.isEmpty() ) {
							emShapesBlack++;   // the flag with no map: the shader's sampler reads black
						} else if ( useMap && s.geom.uv.size() < nv * 2 ) {
							emShapesNoUv++;
						} else if ( useMap && ( gm = glowMapIdx( s.glowTex ) ) < 0 ) {
							emShapesUnread++;
						} else {
							QString ek = QStringLiteral( "%1|%2|%3|%4" ).arg( double( emC[0] ) ).arg( double( emC[1] ) )
								.arg( double( emC[2] ) ).arg( gm );
							if ( emAlt )
								ek += QStringLiteral( "|a%1|%2|%3" ).arg( double( emA[0] ) ).arg( double( emA[1] ) ).arg( double( emA[2] ) );
							auto eit = emitterOf.constFind( ek );
							if ( eit == emitterOf.constEnd() ) {
								ProbeEmit::Emitter em;
								for ( int k = 0; k < 3; k++ ) {
									em.e[k] = emC[k];
									em.alt[k] = emA[k];
								}
								em.hasAlt = emAlt;
								em.map = gm;
								eit = emitterOf.insert( ek, int( probeSoup.glow.emitters.size() ) );
								probeSoup.glow.emitters.push_back( em );
							}
							emIdx = *eit;
							emShapes += emColor ? 1 : 0;
							if ( emColor )
								emRefs.insert( p.ref );
							// the double-count census: placed lights within 256 units of this placement
							int near = 0;
							for ( const EsmRefr & lr : lightRefs ) {
								if ( !emColor )
									break;
								const float dx = lr.pos[0] - p.pos[0], dy = lr.pos[1] - p.pos[1], dz = lr.pos[2] - p.pos[2];
								if ( dx * dx + dy * dy + dz * dz <= 256.0f * 256.0f && !startsDisabled( lr ) ) {
									near++;
									emNearLights.insert( lr.formID );
								}
							}
							if ( emColor && emShapes <= 200 )
								emModels += QStringLiteral( "    %1 | %2 | %3 | e %4,%5,%6 | lights within 256: %7\n" )
								.arg( model, s.matName, useMap ? s.glowTex : QStringLiteral( "(whole surface)" ) )
								.arg( double( emC[0] ), 0, 'f', 3 ).arg( double( emC[1] ), 0, 'f', 3 ).arg( double( emC[2] ), 0, 'f', 3 )
								.arg( near );
							if ( near )
								emRefsNearLight.insert( p.ref );
						}
					}
					const int amFirst = probeSoup.triCount();
					const PbrmMaterial * shapePbrm = baking ? bakePbrmFor( s ) : nullptr;   // lane FIXPACK1
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
							/* lane ALPHATEST1: a masked triangle whose UV centroid is a hole reads its color at
							 * the solid point nearest the centroid (a 4x4 barycentric grid): no ray stops on a hole */
							// lane ALPHATEST2: the scale interpolates like the vertex colour (1 = the ALPHATEST1 test)
							auto amS = [&]( float b0, float b1, float b2 ) -> double {
								return amScale.empty() ? 1.0
									: double( b0 * amScale[i0] + b1 * amScale[i1] + b2 * amScale[i2] );
							};
							if ( amMap >= 0 && probeSoup.amask.holeAt( amMap, uv[0], uv[1], amThr, amS( 1.0f / 3, 1.0f / 3, 1.0f / 3 ) ) ) {
								double bestD = 1e300;
								float buv[2] = { uv[0], uv[1] };
								for ( int a = 0; a < 4; a++ )
									for ( int c = 0; a + c < 4; c++ ) {
										const float b1 = ( a + 0.5f ) / 4.5f, b2 = ( c + 0.5f ) / 4.5f, b0 = 1.0f - b1 - b2;
										const float q[2] = {
											b0 * s.geom.uv[i0 * 2] + b1 * s.geom.uv[i1 * 2] + b2 * s.geom.uv[i2 * 2],
											b0 * s.geom.uv[i0 * 2 + 1] + b1 * s.geom.uv[i1 * 2 + 1] + b2 * s.geom.uv[i2 * 2 + 1] };
										const double dd = std::pow( b1 - 1.0 / 3, 2 ) + std::pow( b2 - 1.0 / 3, 2 );
										if ( dd < bestD && !probeSoup.amask.holeAt( amMap, q[0], q[1], amThr, amS( b0, b1, b2 ) ) ) {
											bestD = dd;
											buv[0] = q[0];
											buv[1] = q[1];
										}
									}
								amAlbMoved += bestD < 1e300 ? 1 : 0;
								uv[0] = buv[0];
								uv[1] = buv[1];
							}
							quint8 rgb[3] = { 128, 128, 128 };
							/* a palette material is painted as the game paints it: the map's green
							 * picks the column, the paint index (or scale x vertex red) the row */
							// lane FIXPACK1: a .pbrm-served surface first (the PBR program draws it: no palette)
							const bool viaPbrm = shapePbrm && probeAlb.samplePbrm( *shapePbrm, uv[0], uv[1], vc, lin );
							albPbrm += viaPbrm ? 1 : 0;
							const bool viaPalette = !viaPbrm && s.g2p && !s.g2pTex.isEmpty()
								&& probeAlb.samplePalette( s.tex0, s.g2pTex, uv[0], uv[1],
									painted ? paintIndex : s.g2pScale * vc[0], lin );
							albPalette += viaPalette ? 1 : 0;
							if ( viaPbrm || viaPalette || probeAlb.sample( s.tex0, uv[0], uv[1], vc, lin ) ) {
								albTextured++;
								for ( int k = 0; k < 3; k++ )
									rgb[k] = quint8( std::lround( std::clamp( lin[k], 0.0f, 1.0f ) * 255.0f ) );
							} else {
								albUntextured++;
							}
							probeSoup.addTri( w[0], w[1], w[2], rgb );
							if ( wantMat )
								probeSoup.setLastMat( triMat( t, s.tex0, true ) );
							if ( smoothOn && s.geom.nrm.size() >= nv * 3 ) {   // lane SMOOTHN1: the NIF's vertex normals, world
								float vn9[9];
								for ( int i = 0; i < 3; i++ ) {
									const size_t vi = size_t( s.geom.tris[t + size_t( i )] );
									const Vector3 wn = p.rot * Vector3( s.geom.nrm[vi * 3], s.geom.nrm[vi * 3 + 1], s.geom.nrm[vi * 3 + 2] );
									for ( int k = 0; k < 3; k++ )
										vn9[i * 3 + k] = wn[k];
								}
								probeSoup.setLastNormals( vn9 );
								soupSmoothTris++;
							}
						} else if ( okTri ) {
							probeSoup.addTri( w[0], w[1], w[2] );
						}
						if ( okTri && s.nearFacts.twoSided )
							probeSoup.markLastTwoSided();   // lane ROOMCLAMP1: no back face
						if ( okTri && amMap >= 0 ) {   // lane ALPHATEST1
							float tuv[6];
							for ( int k = 0; k < 3; k++ ) {
								const size_t vi = size_t( s.geom.tris[t + size_t( k )] );
								tuv[k * 2] = s.geom.uv[vi * 2];
								tuv[k * 2 + 1] = s.geom.uv[vi * 2 + 1];
							}
							float tas[3] = { 1.0f, 1.0f, 1.0f };   // lane ALPHATEST2
							if ( !amScale.empty() )
								for ( int k = 0; k < 3; k++ )
									tas[k] = amScale[size_t( s.geom.tris[t + size_t( k )] )];
							amTrisScaled += ( tas[0] != 1.0f || tas[1] != 1.0f || tas[2] != 1.0f ) ? 1 : 0;
							probeSoup.markLastMasked( amMap, amThr, tuv, amModel, tas );
						}
						if ( okTri && emIdx >= 0 ) {   // lane EMISSIVEGI1
							float tuv[6] = { 0, 0, 0, 0, 0, 0 };
							if ( s.geom.uv.size() >= nv * 2 )
								for ( int k = 0; k < 3; k++ ) {
									const size_t vi = size_t( s.geom.tris[t + size_t( k )] );
									tuv[k * 2] = s.geom.uv[vi * 2];
									tuv[k * 2 + 1] = s.geom.uv[vi * 2 + 1];
								}
							probeSoup.glow.markLast( probeSoup.triCount() - 1, emIdx, tuv );
						}
					}
					if ( !soupShapesPath.isEmpty() && probeSoup.triCount() > amFirst )
						soupShapes += QStringLiteral( "%1\t%2\t%3\t%4\t%5\n" ).arg( amFirst ).arg( probeSoup.triCount() - amFirst )
							.arg( model, s.matName, s.tex0 );
				}
			}
			/* THE MESH'S OWN VERTEX COLORS (lane PRTPPLACE, 2026-09-30): the loader kept them
			 * (geom.rgba, only where the game applies them) and this view never drew them -- the
			 * Concord pickup came out bare rust orange (bungo). An overlay still wins. Alpha: see
			 * src/cellvertexalpha.h (lane TERRAINBLOCK1). */
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
					if ( chunk ) {   // lane CELLSTREAM1: a local serial; registered when the cell is written
						chunk->fxModels.append( m );
						fxLitSerial = chunk->fxModels.size() - 1;
					} else {
						fxLitSerial = wwCellFxLitModel( nif, m );
					}
					fxLitModels++;
				}
				bbOwn += QStringLiteral( "|FL|%1" ).arg( fxLitSerial );
				fxLitShapes++;
			}
			if ( isActor )
				bbOwn += QLatin1String( "|ACTOR" );   // lane PLACED1: own buckets; no decal lands on an actor
			quint32 xemiHere = 0;   // lane LIGHTS2 (XEMI1): a bucket per colour
			if ( xemiBuild && p.xemi && !isActor && !( s.nearFacts.effectShader && s.effectLit ) ) {
				if ( s.extEmit ) {
					xemiHere = p.xemi;
					bbOwn += QStringLiteral( "|XE|%1" ).arg( p.xemi, 8, 16, QLatin1Char( '0' ) );
					( s.nearFacts.effectShader ? xemiShapesFx : xemiShapesLit )++;
				} else {
					xemiNoFlag++;
				}
			} else if ( xemiBuild && p.xemi && !isActor ) {
				xemiLitFx++;   // lane REDFIX1: Memory Den's 34 XEMI cards are all lit effects; the line said 0 of everything
			}
			/* lane CABLES1: a cable whose base has wind sensibility sways: the game draws it with its grass shader
			 * (the wind, the colour's 2.2 power) and its wave period is the base's wind flexibility. One bucket per
			 * base; the vertex alpha is the weight. The grass FADE is not applied (not measured for cables). */
			const EsmBendable * cableLook = p.cable ? &world.bendable( p.base ) : nullptr;
			const bool cableSway = cableLook && cableLook->windSensibility > 0.0f && wwCellCablesWind();
			if ( cableSway )
				bbOwn += QStringLiteral( "|CABLEWIND|%1" ).arg( p.base, 8, 16, QLatin1Char( '0' ) );
			/* Lane WATER1: a placed water mesh (an ACTI whose WNAM names a WATR; its shape carries the water
			 * shader) gets a bucket per record, drawn by src/gl/cellwater.h with that record's terms. */
			const WwWaterRecord * placedWater = nullptr;
			if ( !isActor && lb.waterType != 0 && s.nearFacts.waterShader && !s.nearFacts.effectShader ) {
				auto pw = placedWaterRecs.find( lb.waterType );
				if ( pw == placedWaterRecs.end() ) {
					WwWaterRecord rec;
					if ( !world.waterRecord( lb.waterType, rec ) )
						rec.form = 0;
					pw = placedWaterRecs.insert( lb.waterType, rec );
				}
				if ( pw.value().form != 0 ) {
					placedWater = &pw.value();
					bbOwn += QStringLiteral( "|W%1" ).arg( lb.waterType, 8, 16, QLatin1Char( '0' ) );
					placedWaterShapes++;
				}
			}
			if ( doorLiveRef && p.ref == doorLiveRef && !isActor ) {   // lane DOORLIVE1: drawn by its copied branch
				for ( size_t v = 0; v < nv; v++ ) {
					const Vector3 wp = p.pos + p.rot * ( Vector3( s.geom.pos[v * 3 + 0], s.geom.pos[v * 3 + 1], s.geom.pos[v * 3 + 2] ) * p.scale );
					for ( int k = 0; k < 3; k++ ) {
						lo[k] = qMin( lo[k], wp[k] );
						hi[k] = qMax( hi[k], wp[k] );
					}
				}
				pick.triangles += quint32( s.geom.tris.size() / 3 );
				doorLiveShapes++;
				continue;
			}
			if ( animObjOn && !isActor && !animObjRefs.contains( p.ref ) && !animObjCapped.contains( p.ref )
				&& lodgenModelMoves( model ) )
				( animObjRefs.size() < animObjCap ? animObjRefs : animObjCapped ).insert( p.ref );
			if ( animObjOn && !isActor && animObjRefs.contains( p.ref ) ) {   // lane FXPARITY2: drawn by its copy
				for ( size_t v = 0; v < nv; v++ ) {
					const Vector3 wp = p.pos + p.rot * ( Vector3( s.geom.pos[v * 3 + 0], s.geom.pos[v * 3 + 1], s.geom.pos[v * 3 + 2] ) * p.scale );
					for ( int k = 0; k < 3; k++ ) {
						lo[k] = qMin( lo[k], wp[k] );
						hi[k] = qMax( hi[k], wp[k] );
					}
				}
				pick.triangles += quint32( s.geom.tris.size() / 3 );
				animObjShapes++;
				continue;
			}
			Bucket & b = bucketFor( s, colouring || ownColor || repaint, bbOwn );
			if ( cableSway ) {
				b.grass = true;
				b.grassWave = cableLook->windFlexibility;
				b.grassNoFade = true;
			}
			if ( placedWater ) {
				b.water = true;
				b.waterRec = *placedWater;
			}
			if ( fxLitSerial >= 0 && bbOwn.contains( QLatin1String( "|FL|" ) ) )
				b.fxLit = fxLitSerial;
			if ( xemiHere )
				b.xemi = xemiHere;
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
			static const bool vaRed = qgetenv( "WW_CELL_VERTEXALPHA_RED" ) == "1";
			const bool keepVa = wwCellKeepsVertexAlpha( ownColor, !b.effectMat.isEmpty() || !b.effectBlock.isEmpty(),
				cableSway, s.geom.vertexAlpha, s.nearFacts.treeAnim, vaRed );
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
				if ( keepVa )
					o.chan[3] = float( s.geom.rgba[v * 4 + 3] ) / 255.0f;   // the shape's vertex alpha; a cable's wind weight
				b.verts.push_back( o );
				for ( int k = 0; k < 3; k++ ) {
					lo[k] = qMin( lo[k], wp[k] );
					hi[k] = qMax( hi[k], wp[k] );
				}
			}
			b.runStart.push_back( quint32( b.tris.size() ) );   // lane SUNCELL1: one culling run per placed shape
			b.runRef.push_back( p.ref );   // lane UMBRA1
			for ( size_t t = 0; t + 2 < s.geom.tris.size(); t += 3 ) {
				b.tris.push_back( BucketTri{ { quint32( base + int( s.geom.tris[t + 0] ) ),
					quint32( base + int( s.geom.tris[t + 1] ) ),
					quint32( base + int( s.geom.tris[t + 2] ) ) } } );
			}
			pick.triangles += quint32( s.geom.tris.size() / 3 );
			srcTris += qint64( s.geom.tris.size() / 3 );
		}
		// lane AODECAL1: a copy in the probe soup (its triangles are the sky the probes saw it take)
		if ( aoBuild.on() && role == 1 && !isActor )
			aoBuild.consider( CellPickTable::typeName( lb.type ), model, mit.value(), p.pos, p.rot, p.scale, p.ref,
				aoTri0, probeSoup.tris.size() / 9 );
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
	/* lane CABLES1: the cables' census. "XBSD refs" counts every reference read that carries one and is not
	 * deleted or hidden; "placed" those on a BNDS base that reached the placements; "built" those the
	 * generator made a tube for. */
	const QString cableLine = wwCellCablesOn()
		? QStringLiteral( "cables: on, XBSD refs %1 (on a BNDS base %2, on another base %3, drawn as its model), placed %4"
		                  " (detached %5), built %6, swaying %7, verts %8, tris %9; geometry gate: ends off %10 (worst %11),"
		                  " sag off %12 (worst %13)" )
			.arg( cableXbsd ).arg( cableBnds ).arg( cableOtherBase ).arg( cablePlaced ).arg( cableDetached )
			.arg( cableBuilt ).arg( cableWind ).arg( cableVerts ).arg( cableTris ).arg( cableEndBad )
			.arg( double( cableEndWorst ), 0, 'f', 4 ).arg( cableSagBad ).arg( double( cableSagWorst ), 0, 'f', 4 )
			+ ( wwCellCablesWind() ? QString() : QStringLiteral( "; wind off (WW_CELL_CABLES_RED=nowind)" ) )
		: QStringLiteral( "cables: off (WW_CELL_CABLES_RED=off), XBSD refs %1 (on a BNDS base %2), placed 0" )
			.arg( cableXbsd ).arg( cableBnds );
	qInfo().noquote() << cableLine;
	if ( !cableDumpPath.isEmpty() ) {
		QFile df( cableDumpPath );
		if ( df.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
			df.write( ( QStringLiteral( "ref\tbase\twind\tendA\tendB\tmidZ\tsag\tsagWant\trings\tslices\ttiles\n" )
				+ cableDump ).toUtf8() );
	}

	/* ---- lane GRASSMB1: THE GAME'S GRASS (src/gl/cellgrass.h). Every exterior cell's blades, placed the engine's
	 * way, each a copy of its GRAS model welded into the cell's own "|GRASS" buckets and drawn by the cell
	 * programs (one lighting and fog path) with the grass fade. Row off: nothing here runs, nothing is added. */
	if ( wwCellGrassOn() ) {
		QString gWhy;
		qint64 gBlades = 0, gDrawn = 0, gVerts = 0, gTris = 0, gCells = 0, gNoModel = 0, gNeed = 0, gRoom = 0;
		QSet<quint32> gTypes;
		QElapsedTimer gClock;
		gClock.start();
		if ( spec.interior )
			gWhy = QStringLiteral( "interior" );
		else if ( !spec.terrain )
			gWhy = QStringLiteral( "no terrain" );
		else if ( spec.overlay != CellOverlay::None )
			gWhy = QStringLiteral( "overlay view" );
		if ( gWhy.isEmpty() ) {
			std::vector<WwGrassBlade> blades;
			for ( int y = y0; y <= y1; y++ )
				for ( int x = x0; x <= x1; x++ )
					if ( wwCellGrassPlace( world, x, y, blades ) )
						gCells++;
			gBlades = qint64( blades.size() );
			// each grass's model, loaded once (copied out: a later cache insert may rehash the cache)
			QHash<quint32, std::vector<NativeSrcShape>> gShapes;
			QHash<quint32, bool> gUniform;
			QHash<quint32, float> gWave;   // the GRAS wave period: the wind's phase is per grass type
			for ( const WwGrassBlade & bl : blades ) {
				if ( gUniform.contains( bl.form ) )
					continue;
				gUniform.insert( bl.form, false );
				EsmGrass g;
				if ( !world.grass( bl.form, g ) || g.model.isEmpty() )
					continue;
				gUniform[bl.form] = ( g.flags & 2 ) != 0;
				gWave[bl.form] = g.wavePeriod;
				auto mit = modelCache.find( g.model );
				if ( mit == modelCache.end() && !modelsFailed.contains( g.model ) ) {
					std::vector<NativeSrcShape> shapes;
					if ( lodgenNativeLoadModel( const_cast<QString *>( &dataRoot ), g.model, &shapes ) && !shapes.empty() )
						mit = modelCache.insert( g.model, shapes );
					else
						modelsFailed.insert( g.model );
				}
				if ( mit != modelCache.end() )
					gShapes.insert( bl.form, mit.value() );
			}
			// the vertex bill first: a field that would break the block's cap is refused whole, not cut in half
			qint64 have = 0;
			for ( auto bit = buckets.cbegin(); bit != buckets.cend(); ++bit )
				have += qint64( bit.value().verts.size() );
			gRoom = qint64( CELL_MAX_TOTAL_VERTS ) - have - 1000000;   // the ground and water still to come
			for ( const WwGrassBlade & bl : blades ) {
				const auto sit = gShapes.constFind( bl.form );
				if ( sit == gShapes.constEnd() )
					continue;
				for ( const NativeSrcShape & s : sit.value() )
					gNeed += qint64( s.geom.pos.size() / 3 );
			}
			if ( gNeed > gRoom ) {
				gWhy = QStringLiteral( "REFUSED: %1 vertices, room for %2 under the block cap" ).arg( gNeed ).arg( gRoom );
			} else {
				for ( const WwGrassBlade & bl : blades ) {
					const auto sit = gShapes.constFind( bl.form );
					if ( sit == gShapes.constEnd() ) {
						gNoModel++;
						continue;
					}
					float t[3], R[9], sc[3], shade = 1.0f;
					wwCellGrassTransform( bl, gUniform.value( bl.form ), t, R, sc, &shade );
					// the wind's per-blade phase offset rides in the bitangent's length (fo4_default.vert reads it back);
					// only the side mesh keeps float bitangents (document rows store Y and Z as bytes: offset 0 there)
					const float bitLen = cellMeshOn() ? 1024.0f + wwCellGrassPhaseOffset( bl ) : 1.0f;
					gTypes.insert( bl.form );
					gDrawn++;
					for ( const NativeSrcShape & s : sit.value() ) {
						const size_t nv = s.geom.pos.size() / 3;
						if ( !nv || s.geom.tris.size() < 3 )
							continue;
						Bucket & b = bucketFor( s, true, QStringLiteral( "|GRASS|%1" ).arg( bl.form, 8, 16, QLatin1Char( '0' ) ) );
						b.grass = true;
						b.grassWave = gWave.value( bl.form, 0.0f );
						const bool haveC = s.geom.rgba.size() >= nv * 4;
						const int base = int( b.verts.size() );
						const bool haveN = s.geom.nrm.size() >= nv * 3, haveT = s.geom.tan.size() >= nv * 3;
						for ( size_t v = 0; v < nv; v++ ) {
							const float p[3] = { s.geom.pos[v * 3 + 0] * sc[0], s.geom.pos[v * 3 + 1] * sc[1],
								s.geom.pos[v * 3 + 2] * sc[2] };
							const float ln[3] = { haveN ? s.geom.nrm[v * 3 + 0] : 0.0f, haveN ? s.geom.nrm[v * 3 + 1] : 0.0f,
								haveN ? s.geom.nrm[v * 3 + 2] : 1.0f };
							const float lt[3] = { haveT ? s.geom.tan[v * 3 + 0] : 1.0f, haveT ? s.geom.tan[v * 3 + 1] : 0.0f,
								haveT ? s.geom.tan[v * 3 + 2] : 0.0f };
							OutVert o;
							const Vector3 wp( t[0] + R[0] * p[0] + R[1] * p[1] + R[2] * p[2],
								t[1] + R[3] * p[0] + R[4] * p[1] + R[5] * p[2],
								t[2] + R[6] * p[0] + R[7] * p[1] + R[8] * p[2] );
							o.pos = wp - origin;
							o.nrm = Vector3( R[0] * ln[0] + R[1] * ln[1] + R[2] * ln[2],
								R[3] * ln[0] + R[4] * ln[1] + R[5] * ln[2], R[6] * ln[0] + R[7] * ln[1] + R[8] * ln[2] );
							if ( o.nrm.length() > 1.0e-6f )
								o.nrm.normalize();
							else
								o.nrm = Vector3( 0.0f, 0.0f, 1.0f );
							o.tan = Vector3( R[0] * lt[0] + R[1] * lt[1] + R[2] * lt[2],
								R[3] * lt[0] + R[4] * lt[1] + R[5] * lt[2], R[6] * lt[0] + R[7] * lt[1] + R[8] * lt[2] );
							if ( o.tan.length() > 1.0e-6f )
								o.tan.normalize();
							else
								o.tan = Vector3( 1.0f, 0.0f, 0.0f );
							o.bit = Vector3::crossproduct( o.nrm, o.tan );
							if ( o.bit.length() < 1.0e-6f )
								o.bit = Vector3( 0.0f, 0.0f, 1.0f );
							o.bit.normalize();
							o.bit *= bitLen;
							o.uv = s.geom.uv.size() >= ( v + 1 ) * 2
								? Vector2( s.geom.uv[v * 2 + 0], s.geom.uv[v * 2 + 1] ) : Vector2( 0.0f, 0.0f );
							// the game's v4.rgb * v5.w: the model's vertex colour times the blade's shade; A = the model's
							// vertex alpha, the wind's weight (no colours: (1,1,1,1))
							for ( int c = 0; c < 3; c++ )
								o.chan[c] = ( haveC ? float( s.geom.rgba[v * 4 + size_t( c )] ) / 255.0f : 1.0f ) * shade;
							o.chan[3] = haveC ? float( s.geom.rgba[v * 4 + 3] ) / 255.0f : 1.0f;
							b.verts.push_back( o );
						}
						for ( size_t tt = 0; tt + 2 < s.geom.tris.size(); tt += 3 )
							b.tris.push_back( BucketTri{ { quint32( base + int( s.geom.tris[tt + 0] ) ),
								quint32( base + int( s.geom.tris[tt + 1] ) ),
								quint32( base + int( s.geom.tris[tt + 2] ) ) } } );
						gVerts += qint64( nv );
						gTris += qint64( s.geom.tris.size() / 3 );
					}
				}
			}
		}
		const double gMb = double( gVerts * qint64( sizeof( OutVert ) ) + gTris * qint64( sizeof( BucketTri ) ) )
			/ ( 1024.0 * 1024.0 );
		if ( !chunk )
		wwCellGrassNote( nif, gWhy.isEmpty()
			? QStringLiteral( "cell grass: on, %1 blades placed in %2 cells, %3 drawn of %4 grass types (%5 without a "
			                  "model), verts %6, tris %7, weld %8 MB, place+weld %9 ms" )
				.arg( gBlades ).arg( gCells ).arg( gDrawn ).arg( gTypes.size() ).arg( gNoModel ).arg( gVerts )
				.arg( gTris ).arg( gMb, 0, 'f', 1 ).arg( gClock.elapsed() )
			: QStringLiteral( "cell grass: on, nothing drawn (%1); %2 blades placed in %3 cells" )
				.arg( gWhy ).arg( gBlades ).arg( gCells ) );
		CellSpeed::mark( "grass placed and welded" );
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
		/* Lane WATER1: one water bucket per WATR record, so each surface draws with its own record's
		 * terms. The worldspace default (and a record that does not read) stays in waterB under the
		 * old key; an override (XCWT: the rivers, the marshes, the Glowing Sea) gets its own. */
		QMap<quint32, Bucket> waterByForm;
		QHash<quint32, bool> waterReadable;
		WwWaterRecord waterDefaultRec;
		if ( world.defaultWaterType() != 0 && world.waterRecord( world.defaultWaterType(), waterDefaultRec ) ) {
			waterB.water = true;
			waterB.waterRec = waterDefaultRec;
		}
		Bucket waterAll;                       // every cell's water quad, in the old bucket's order
		std::vector<quint32> waterQuadOwner;   // per quad: 0 = waterB, else the WATR form of its bucket
		std::vector<Vector3> waterCellAt;      // lane WATERLOOK1: per quad its cell's corner x, y and the water height
		std::vector<quint32> waterVert0, waterTri0;   // per quad: its first vertex and triangle in waterAll (+ end)
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
			const qint64 v = wwEnvInt( "WW_CELL_SPLAT_CAP", &capOk );
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
						 * mean times its VCLR, times the pass's mean SHARE, summed: the engine's
						 * weighted sum (lane TERRBLEND1; it was laid alpha-over until then) */
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
							if ( it == landAlb.end() )
								it = landAlb.insert( key, std::array<float, 3>{ { 0.0f, 0.0f, 0.0f } } );
							std::array<float, 3> & col = it.value();
							for ( int c = 0; c < 3; c++ )
								col[c] += m[c] * vc[c] * std::clamp( wv, 0.0f, 1.0f );
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
						gbk.specTex = src.spec;	// lane DUNESAND1: slot 7, the LTEX's _s (cellsplat.cpp)
						gbk.groundSpec = src.specWant;
						gbk.withColour = true;
						/* A layer pass is ADDED; a base pass is opaque and scaled by its
						 * share in the shader (lane TERRBLEND1). */
						gbk.hasAlpha = src.blend;
						gbk.alphaThreshold = 0;
						gbk.landSplat = true;
					}
					for ( const CellSplatQuad & q : sb.quads ) {
						if ( q.bucket < 0 || q.bucket >= groundBuckets.size() )
							continue;
						Bucket & gbk = groundBuckets[q.bucket];
						const int base = int( gbk.verts.size() );
						for ( int k = 0; k < 4; k++ ) {
							OutVert o;
							o.pos = Vector3( q.v[k].p[0], q.v[k].p[1], q.v[k].p[2] );
							o.nrm = Vector3( q.v[k].n[0], q.v[k].n[1], q.v[k].n[2] );	// lane ALLON2: the vertex's VNML
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
					gbk.specTex = src.spec;	// lane DUNESAND2: the mosaic binds the _s too
					gbk.groundSpec = src.specWant;
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
				quint32 waterForm = 0;
				if ( spec.water && world.cellWater( x, y, wh, &waterForm ) ) {
					waterCells++;
					const float ox = float( x ) * CELL_UNITS - origin[0];
					const float oy = float( y ) * CELL_UNITS - origin[1];
					Bucket * wb = &waterB;   // lane WATER1: the bucket of the cell's own WATR record
					if ( waterForm != 0 && waterForm != world.defaultWaterType() ) {
						auto rd = waterReadable.find( waterForm );
						if ( rd == waterReadable.end() ) {
							WwWaterRecord rec;
							const bool ok = world.waterRecord( waterForm, rec );
							rd = waterReadable.insert( waterForm, ok );
							if ( ok ) {
								Bucket & nb = waterByForm[waterForm];
								nb.name = QStringLiteral( "water %1" )
									.arg( waterForm, 8, 16, QLatin1Char( '0' ) ).toUpper();
								nb.withColour = true;
								nb.water = true;
								nb.waterRec = rec;
							}
						}
						if ( rd.value() )
							wb = &waterByForm[waterForm];
					}
					waterQuadOwner.push_back( wb == &waterB ? 0u : waterForm );
					waterCellAt.push_back( Vector3( ox, oy, wh ) );
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
		/* lane WATERLOOK1: each cell's water a 4 x 4 grid of 1024 u quads on shared vertices, so the displaced
		 * water's tessellator (at most 64 splits an edge) reaches 16 u steps near instead of 64 u. The same plane,
		 * corners (bit for bit) and so the same bounds; neighbouring cells meet vertex to vertex. Past one shape's
		 * vertices the grid would cut the water into more shapes (their bounds would move): one quad a cell then. */
		const int wn = waterCellAt.size() * 25 <= size_t( MAX_SHAPE_VERTS ) ? 4 : 1;
		const float rgbW[3] = { 0.18f, 0.32f, 0.42f };
		for ( const Vector3 & c : waterCellAt ) {
			waterVert0.push_back( quint32( waterAll.verts.size() ) );
			waterTri0.push_back( quint32( waterAll.tris.size() ) );
			if ( wn == 1 ) {
				appendQuad( waterAll,
					Vector3( c[0], c[1], c[2] ),
					Vector3( c[0] + CELL_UNITS, c[1], c[2] ),
					Vector3( c[0] + CELL_UNITS, c[1] + CELL_UNITS, c[2] ),
					Vector3( c[0], c[1] + CELL_UNITS, c[2] ),
					Vector3( 0.0f, 0.0f, 1.0f ), rgbW );
				continue;
			}
			const quint32 base = quint32( waterAll.verts.size() );
			for ( int j = 0; j <= wn; j++ )
				for ( int i = 0; i <= wn; i++ ) {
					OutVert v;
					v.pos = Vector3( i == wn ? c[0] + CELL_UNITS : c[0] + float( i ) * ( CELL_UNITS / float( wn ) ),
						j == wn ? c[1] + CELL_UNITS : c[1] + float( j ) * ( CELL_UNITS / float( wn ) ), c[2] );
					v.nrm = Vector3( 0.0f, 0.0f, 1.0f );
					v.tan = Vector3( 1.0f, 0.0f, 0.0f );
					v.bit = Vector3( 0.0f, 1.0f, 0.0f );
					v.uv = Vector2( float( i ) / float( wn ), float( j ) / float( wn ) );
					v.chan[0] = rgbW[0];
					v.chan[1] = rgbW[1];
					v.chan[2] = rgbW[2];
					waterAll.verts.push_back( v );
				}
			for ( int j = 0; j < wn; j++ )
				for ( int i = 0; i < wn; i++ ) {	// counter-clockwise from +Z, appendQuad's diagonal
					const quint32 a = base + quint32( j * ( wn + 1 ) + i ), b = a + 1;
					const quint32 d = a + quint32( wn + 1 ), e = d + 1;
					waterAll.tris.push_back( BucketTri{ { a, b, e } } );
					waterAll.tris.push_back( BucketTri{ { a, e, d } } );
				}
		}
		waterVert0.push_back( quint32( waterAll.verts.size() ) );
		waterTri0.push_back( quint32( waterAll.tris.size() ) );
		/* lane WATER1: every water bucket carries ALL the water quads' vertices, in the one order the single
		 * water bucket had before the lane; a quad of another record keeps its vertices through zero-area
		 * triangles (no fragment). Each shape's bounding sphere is then the old water shape's, bit
		 * for bit, so the scene's bounds -- and with them the view's near/far planes and the sun cascades --
		 * do not move when the water splits by record (the Cell lights row off renders as before the lane). */
		auto waterFill = [&]( Bucket & wb, quint32 owner ) {
			wb.verts = waterAll.verts;
			wb.tris.clear();
			bool owns = false;
			for ( size_t q = 0; q < waterQuadOwner.size(); q++ ) {
				if ( waterQuadOwner[q] == owner ) {
					for ( quint32 t = waterTri0[q]; t < waterTri0[q + 1]; t++ )
						wb.tris.push_back( waterAll.tris[t] );
					owns = true;
				} else {
					const quint32 b = waterVert0[q], n = waterVert0[q + 1] - b;
					for ( quint32 v = 0; v < n; v += 2 ) {
						const quint32 w = b + qMin( v + 1, n - 1 );
						wb.tris.push_back( BucketTri{ { b + v, w, w } } );
					}
				}
			}
			return owns;
		};
		if ( waterFill( waterB, 0u ) )
			buckets.insert( QStringLiteral( "\x01water" ), waterB );
		for ( auto wf = waterByForm.begin(); wf != waterByForm.end(); ++wf )   // lane WATER1
			if ( waterFill( wf.value(), wf.key() ) )
				buckets.insert( QStringLiteral( "\x01water|%1" )
					.arg( wf.key(), 8, 16, QLatin1Char( '0' ) ), wf.value() );
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
				|| it.key().startsWith( QLatin1String( "\x01water" ) ) || it.key() == QLatin1String( "\x01grid" )
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
			QString::fromLocal8Bit( wwEnv( "WW_CELL_DECAL_DUMP" ) ) );
		for ( const CellDecalMesh & m : decalResult.meshes ) {
			if ( m.tris.empty() )
				continue;
			Bucket b;
			b.name = m.name;
			b.matString = m.diffuse;
			b.normalTex = m.normal;
			b.specTex = m.spec;
			b.heightTex = m.height;	// lane DECALS1
			b.pomScale = m.parallaxScale;
			b.pomPasses = m.parallaxPasses;
			b.decal = true;
			b.decalAngle = !decalResult.vertexAngle;   // lane DECALANGLE
			b.decalMapped = b.decalAngle && decalResult.mapped;   // lane DECALANGLE2
			b.hasAlpha = true;       // blended; the vertex alpha carries the angle fade (or its dot, decalAngle)
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
				o.chan[3] = b.decalAngle ? v.angle : v.alpha;
				for ( int c = 0; b.decalMapped && c < 3; c++ )   // lane DECALANGLE2: D as a signed byte
					o.chan[c] = celldecalangle::dirToColour( v.dir[c] );
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
			refTable.setFate( decalRefs[i].refRow, f == CellDecalFate::Drawn ? CellRefFate::Projected
				: ( f == CellDecalFate::NoDecalData ? CellRefFate::NoModel : CellRefFate::Refused ) );
		}
	}

	// ---- lane PRTPPLACE: place the probes, write them, draw them
	QString probeNotes;
	QString giBakeDir;      // lane PRTPGI: the folder the bake just wrote, relit once the lights are read
	std::shared_ptr<const ProbeVolumes> volGi;   // lane VOLFIT1: the Space rooms for the GI (WW_CELL_VOLUMES_GI=1); shared with the kept sky spec
	bool volGiOn = false;
	if ( probing ) {
		if ( !spec.interior ) {
			// the ground is part of what a column ray meets, painted or not
			QFile landDumpFile;   // lane SMOOTHN1: the gate's land (heights + raw VNML), WW_CELL_BAKE_LAND_DUMP
			if ( !landDump.isEmpty() && baking ) {
				landDumpFile.setFileName( landDump );
				if ( landDumpFile.open( QIODevice::WriteOnly ) )
					landDumpFile.write( "LND1", 4 );
			}
			for ( int y = y0; y <= y1; y++ )
				for ( int x = x0; x <= x1; x++ ) {
					EsmLand l;
					if ( !world.land( x, y, l ) )
						continue;
					const float ox = float( x ) * CELL_UNITS, oy = float( y ) * CELL_UNITS;
					const float st = CELL_UNITS / float( LAND_GRID - 1 );
					if ( landDumpFile.isOpen() ) {
						const qint32 xy[2] = { x, y };
						const quint8 hn = l.hasNormals ? 1 : 0;
						landDumpFile.write( reinterpret_cast<const char *>( xy ), sizeof xy );
						landDumpFile.write( reinterpret_cast<const char *>( &hn ), 1 );
						landDumpFile.write( reinterpret_cast<const char *>( &l.heights[0][0] ), sizeof l.heights );
						landDumpFile.write( reinterpret_cast<const char *>( l.normalsRaw ), sizeof l.normalsRaw );
					}
					/* lane SMOOTHN1: the ground's vertex normals -- its VNML, or (a LAND without one) the heights'
					 * central differences -- so a terrain hit gets the game's smooth normal, not its quad's face */
					auto landN = [&]( int r, int c, float * o ) {
						if ( l.hasNormals ) {
							for ( int k = 0; k < 3; k++ )
								o[k] = l.normals[r][c][k];
							return;
						}
						const int c0 = qMax( c - 1, 0 ), c1 = qMin( c + 1, LAND_GRID - 1 );
						const int r0 = qMax( r - 1, 0 ), r1 = qMin( r + 1, LAND_GRID - 1 );
						const float gx = ( l.heights[r][c1] - l.heights[r][c0] ) / ( float( c1 - c0 ) * st );
						const float gy = ( l.heights[r1][c] - l.heights[r0][c] ) / ( float( r1 - r0 ) * st );
						const float n = std::sqrt( gx * gx + gy * gy + 1.0f );
						o[0] = -gx / n;
						o[1] = -gy / n;
						o[2] = 1.0f / n;
					};
					if ( baking && smoothOn ) {
						if ( l.hasNormals )
							soupLandVnml++;
						else
							soupLandDerived++;
					}
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
								if ( smoothOn ) {
									float vn9[9];
									landN( r, c, vn9 );
									landN( r, c + 1, vn9 + 3 );
									landN( r + 1, c + 1, vn9 + 6 );
									probeSoup.setLastNormals( vn9 );
								}
								probeSoup.addTri( a, cc, d, rgb );
								if ( smoothOn ) {
									float vn9[9];
									landN( r, c, vn9 );
									landN( r + 1, c + 1, vn9 + 3 );
									landN( r + 1, c, vn9 + 6 );
									probeSoup.setLastNormals( vn9 );
								}
							} else {
								probeSoup.addTri( a, b, cc );
								probeSoup.addTri( a, cc, d );
							}
						}
				}
			/* lane WATER1: the cells' water planes, apart from the triangles (they stop no ray): the placer splits a
			 * column at the line, the bake splits a ray at the surface. Kind 0 = the worldspace default record (the
			 * far ring's water takes it); one kind per other WATR the cells name. Pin WW_CELL_BAKE_WATER=0: none. */
			if ( bakeWater ) {
				auto typeOf = [&]( quint32 form, const WwWaterRecord & r ) {
					ProbeSoup::WaterType w;
					w.form = form;
					w.fresnel = r.fresnel;
					w.reflectivity = r.reflectivity;
					for ( int k = 0; k < 3; k++ )
						w.uw[k] = wwWaterLin( r.underwater[k] );
					w.fogAmount = r.uwFogAmount;
					w.fogNear = r.uwFogNear;
					w.fogFar = r.uwFogFar;
					return w;
				};
				WwWaterRecord def;
				const quint32 defForm = world.defaultWaterType();
				if ( defForm && world.waterRecord( defForm, def ) )
					probeSoup.waterTypes.push_back( typeOf( defForm, def ) );
				else
					probeSoup.waterTypes.push_back( ProbeSoup::WaterType() );
				QHash<quint32, quint16> kindOf;
				kindOf.insert( defForm, 0 );
				for ( int y = y0; y <= y1; y++ )
					for ( int x = x0; x <= x1; x++ ) {
						float wh = 0.0f;
						quint32 form = 0;
						if ( !world.cellWater( x, y, wh, &form ) )
							continue;
						auto kt = kindOf.find( form );
						if ( kt == kindOf.end() ) {
							WwWaterRecord r;
							quint16 k = 0;   // an unreadable record: the default's terms
							if ( world.waterRecord( form, r ) ) {
								k = quint16( probeSoup.waterTypes.size() );
								probeSoup.waterTypes.push_back( typeOf( form, r ) );
							}
							kt = kindOf.insert( form, k );
						}
						const float ox = float( x ) * CELL_UNITS, oy = float( y ) * CELL_UNITS;
						const float a[3] = { ox, oy, wh }, b[3] = { ox + CELL_UNITS, oy, wh };
						const float c[3] = { ox + CELL_UNITS, oy + CELL_UNITS, wh }, d[3] = { ox, oy + CELL_UNITS, wh };
						probeSoup.addWater( a, b, c, kt.value() );
						probeSoup.addWater( a, c, d, kt.value() );
						soupWaterCells++;
					}
				probeSoup.waterApart = baking;   // the far ring's water, kind 0
			}
		}
		ProbePlaceSpec ps;
		int ownCells[4] = { 0, 0, -1, -1 };   // lane BAKENEIGH1: the probed cells x0, y0, x1, y1 (exterior only)
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
			int pn = wwEnvInt( "WW_CELL_PROBES_N" );
			pn = qBound( 1, pn > 0 ? pn : ( spec.probes ? specAsked.n : 1 ), qMax( 1, spec.n ) );
			const int ph = ( pn - 1 ) / 2;
			ps.minX = float( spec.cx - ph ) * CELL_UNITS;
			ps.maxX = float( spec.cx + ph + 1 ) * CELL_UNITS;
			ps.minY = float( spec.cy - ph ) * CELL_UNITS;
			ps.maxY = float( spec.cy + ph + 1 ) * CELL_UNITS;
			ownCells[0] = spec.cx - ph;
			ownCells[1] = spec.cy - ph;
			ownCells[2] = spec.cx + ph;
			ownCells[3] = spec.cy + ph;
		}
		const QByteArray sp = wwEnv( "WW_CELL_PROBES_SPACING" );
		if ( !sp.isEmpty() && sp.toFloat() > 1.0f )
			ps.spacing = sp.toFloat();
		ps.red = QString::fromLatin1( wwEnv( "WW_PROBE_RED" ) );
		if ( roomclampPin && ps.red.isEmpty() )
			ps.red = QStringLiteral( "floor" );   // lane ROOMCLAMP1 pin: the old floors
		if ( !probeSoup.doorGeom.empty() ) {
			/* lane GPURELIGHT1: drop the triangles of a door that was never tagged (no drawn triangles), and keep
			 * only the masks the doors use (renumbered; the soup's own list stays as it is) */
			const ProbeSoup::DoorGeom g = std::move( probeSoup.doorGeom );
			ProbeSoup::DoorGeom & dg = probeSoup.doorGeom;
			dg = ProbeSoup::DoorGeom();
			const int nd = int( probeSoup.doors.size() );
			std::map<int, int> mapNew;
			for ( size_t i = 0; i < g.door.size(); i++ ) {
				if ( g.door[i] < 0 || g.door[i] >= nd ) {
					dgDropped++;
					continue;
				}
				dg.tris.insert( dg.tris.end(), g.tris.begin() + long( i * 9 ), g.tris.begin() + long( i * 9 + 9 ) );
				dg.door.push_back( g.door[i] );
				if ( i < g.amask.triOf.size() && g.amask.triOf[i] >= 0 ) {
					probebvh::AlphaMask::Tri mt = g.amask.tris[size_t( g.amask.triOf[i] )];
					auto it = mapNew.find( mt.map );
					if ( it == mapNew.end() ) {
						it = mapNew.emplace( mt.map, int( dg.amask.maps.size() ) ).first;
						dg.amask.maps.push_back( probeSoup.amask.maps[size_t( mt.map )] );
						dg.amask.mapNames.push_back( probeSoup.amask.mapNames[size_t( mt.map )] );
					}
					mt.map = it->second;
					dg.amask.triOf.resize( dg.door.size(), -1 );
					dg.amask.triOf.back() = int( dg.amask.tris.size() );
					dg.amask.tris.push_back( mt );
				}
			}
			for ( size_t i = 0; i < g.glassDoor.size(); i++ ) {
				if ( g.glassDoor[i] < 0 || g.glassDoor[i] >= nd ) {
					dgDropped++;
					continue;
				}
				dg.glass.insert( dg.glass.end(), g.glass.begin() + long( i * 9 ), g.glass.begin() + long( i * 9 + 9 ) );
				dg.glassT.insert( dg.glassT.end(), g.glassT.begin() + long( i * 3 ), g.glassT.begin() + long( i * 3 + 3 ) );
				dg.glassDoor.push_back( g.glassDoor[i] );
			}
		}
		ProbePlaceResult pr;
		const bool placed = probePlace( probeSoup, ps, &pr );
		// lane BAKEBLOCK1: past the loaded block, out to the bake's ray reach, the LOD files' far soup
		// (after the placing, which the loaded block decides; before the dump, so the gates see it)
		const QString farLine = bakeExterior && placed ? cellBakeFarSoup( spec, dataRoot, blockRed, probeSoup, &world ) : QString();
		PassTime::memLog( "probe soup built" );	// lane PERF1
		const QByteArray soupDump = wwEnv( "WW_CELL_PROBE_SOUP" );
		QString perr;
		if ( !soupDump.isEmpty() && !probeSoupWrite( QString::fromLocal8Bit( soupDump ), probeSoup, &perr ) )
			qWarning() << "WW_CELL_PROBE_SOUP:" << perr;
		// lane VOLFIT1: the markers beside the soup, so the headless `volumes` command sees what the view saw
		if ( !soupDump.isEmpty() && !probeVolumesMarkersWrite( QString::fromLocal8Bit( soupDump ) + QStringLiteral( ".markers" ), volMarkers ) )
			qWarning() << "WW_CELL_PROBE_SOUP: cannot write the markers";
		// lane VOLFIT1: WW_CELL_VOLUMES=<bake root> runs the Space stage (rooms, thresholds) into <root>/<World>/<X>_<Y>
		const QByteArray volRoot = wwEnv( "WW_CELL_VOLUMES" );
		if ( placed && !volRoot.isEmpty() ) {
			QElapsedTimer vt;
			vt.start();
			std::vector<ProbeVolumeAperture> volAps;
			for ( const ProbePoint & p : pr.probes )
				if ( p.cls == ProbeClass::Aperture ) {
					ProbeVolumeAperture a;
					a.type = int( p.kind );
					for ( int c = 0; c < 3; c++ )
						a.pos[c] = p.pos[c];
					a.width = p.width;
					a.height = p.height;
					a.doorRef = p.doorRef;
					volAps.push_back( a );
				}
			ProbeVolumesStageSpec vs;
			vs.root = QDir::cleanPath( QString::fromLocal8Bit( volRoot ) );
			vs.interior = spec.interior;
			vs.byAnchor = !spec.interior;
			vs.spec.red = QString::fromLatin1( wwEnv( "WW_CELL_VOLUMES_RED" ) );
			QString recxName;
			if ( spec.interior ) {
				QString k = world.interior().edid.isEmpty() ? spec.interiorCell : world.interior().edid;
				k.replace( QRegularExpression( QStringLiteral( "[^A-Za-z0-9_+-]" ) ), QStringLiteral( "_" ) );
				vs.keys << ( k.isEmpty() ? QStringLiteral( "cell" ) : k );
				recxName = vs.keys.front();
			} else {
				vs.world = spec.world;
				vs.spec.clipLo[0] = ps.minX - 512.0f;   // the block plus a margin: an edge room is whole, kept by its anchor's cell
				vs.spec.clipLo[1] = ps.minY - 512.0f;
				vs.spec.clipHi[0] = ps.maxX + 512.0f;
				vs.spec.clipHi[1] = ps.maxY + 512.0f;
				for ( int y = ownCells[1]; y <= ownCells[3]; y++ )
					for ( int x = ownCells[0]; x <= ownCells[2]; x++ )
						vs.keys << QStringLiteral( "%1_%2" ).arg( x ).arg( y );
				recxName = spec.world;
			}
			// hand fixes: WW_CELL_VOLUMES_RECX=<dir>, else Data/Plugins/FO4CS/Volumes/<World or EDID>.recx when present
			const QByteArray rxDir = wwEnv( "WW_CELL_VOLUMES_RECX" );
			const QString rx = QDir( rxDir.isEmpty() ? dataRoot + QStringLiteral( "/Plugins/FO4CS/Volumes" ) : QString::fromLocal8Bit( rxDir ) )
				.filePath( recxName + QStringLiteral( ".recx" ) );
			if ( QFileInfo::exists( rx ) ) {
				vs.recxFile = rx;
				vs.recxName = recxName;
			}
			// lane ROOMFIT9: the stage's other inputs beside the soup dump (`volumes --like-cell` rebuilds these files exactly)
			if ( !soupDump.isEmpty() && !probeVolumesStageInWrite( QString::fromLocal8Bit( soupDump ) + QStringLiteral( ".spacein" ), volAps, vs ) )
				qWarning() << "WW_CELL_PROBE_SOUP: cannot write the Space stage inputs";
			ProbeVolumes vol;
			bool vok = false;
			const QString vlog = probeVolumesStage( probeSoup, volMarkers, volAps, vs, &vol, &vok );
			for ( const QString & l : vlog.split( '\n', Qt::SkipEmptyParts ) )
				probeNotes += QStringLiteral( "  %1\n" ).arg( l );
			probeNotes += QStringLiteral( "  space stage %1 ms (%2)\n" ).arg( vt.elapsed() ).arg( vok ? QStringLiteral( "ok" ) : QStringLiteral( "FAILED" ) );
			wwCellVolumeViewPublish( nif, vol );
			if ( vok && wwEnv( "WW_CELL_VOLUMES_GI" ) == "1" ) {
				volGi = std::make_shared<const ProbeVolumes>( std::move( vol ) );
				volGiOn = true;
			}
		}
		if ( !soupShapesPath.isEmpty() ) {   // lane ALPHATEST1 gate
			QFile f( QString::fromLocal8Bit( soupShapesPath ) );
			if ( f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
				f.write( ( QStringLiteral( "first\tcount\tmodel\tmaterial\tmap\n" ) + soupShapes ).toUtf8() );
		}
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
			// lane ALPHATEST1: the alpha-test masks
			t << "  probe soup alpha-test masks" << ( alphaPin ? " PIN off (every face solid)" : "" ) << ": shapes masked "
			  << amShapes << ", triangles " << int( probeSoup.amask.tris.size() ) << ", maps " << int( probeSoup.amask.maps.size() )
			  << "; alpha-tested shapes left solid: map with no texel under the threshold " << amShapesNoHole
			  << ", map unread " << amShapesUnread << "; triangle colors read off a centroid hole " << amAlbMoved << "\n";
			// lane ALPHATEST2: masks scaled by vertex alpha x material alpha
			t << "  probe soup alpha-test scale" << ( alphaNoScale ? " RED noscale" : "" ) << ": shapes scaled " << amShapesScaled
			  << ", shapes masked by the scale alone " << amShapesScaleOnly << ", triangles scaled " << amTrisScaled << "\n";
			if ( giDoorGeom )   // lane GPURELIGHT1
				t << "  probe soup door geometry: shapes " << dgShapes << " (alpha-tested with a hole " << dgMasked << ", blended = glass "
				  << dgGlass << ", glass map unread " << dgGlassUnread << "), triangles " << int( probeSoup.doorGeom.door.size() )
				  << " (masked " << int( probeSoup.doorGeom.amask.tris.size() ) << ", maps " << int( probeSoup.doorGeom.amask.maps.size() )
				  << "), panes " << int( probeSoup.doorGeom.glassDoor.size() ) << ", dropped (door never tagged) " << dgDropped
				  << "; vertex-alpha scaled shapes " << dgScaled << ", triangles " << dgTrisScaled << "\n";   // lane GAPFIX1
			// lane EMISSIVEGI1: the glowing surfaces
			t << "  probe soup emissive" << ( emitPin ? " PIN off (nothing glows)" : "" )
			  << ( emitRed.isEmpty() ? "" : " RED " ) << emitRed.constData() << ": shapes glowing " << emShapes
			  << " (placements " << emRefs.size() << ", with a placed light lit within 256 units " << emRefsNearLight.size()
			  << ", those lights " << emNearLights.size() << "), triangles " << int( probeSoup.glow.tris.size() )
			  << ", emitters " << int( probeSoup.glow.emitters.size() ) << ", glow maps " << int( probeSoup.glow.maps.size() )
			  << "; left dark: glow flag with no map " << emShapesBlack << ", map unread " << emShapesUnread << ", no UVs "
			  << emShapesNoUv << "; a color without Own-Emit " << emOffShapes << "; emittance colour (row "
			  << ( xemiGiOn ? "on" : "off" ) << ") " << emXemiShapes << "\n" << emModels;
			// lane NEONGI1: the effect surfaces that glow
			t << "  probe soup emissive effects" << ( emitPin || emitRed == "noeffects" ? " OFF" : "" ) << ": shapes " << veilShapes
			  << " (placements " << veilRefs.size() << ", with a placed light lit within 256 units " << veilRefsNearLight.size()
			  << ", those lights " << veilNearLights.size() << "), triangles " << int( probeSoup.veil.count() ) << "; effect shapes left out:";
			for ( auto it = veilLeftOut.constBegin(); it != veilLeftOut.constEnd(); ++it )
				t << " " << it.key() << " " << it.value() << ";";
			t << "\n" << veilModels;
			/* lane GIBAKE2: placed weapons and placed particles, counted. A weapon is a pick-up item (BAKE4: left
			 * out); let in by WW_CELL_PROBE_SOUP_RED=items it carries its mod parts when the Weapon mod parts row is on.
			 * Particles stay out: a placed light within 256 units carries the fire's light (NEONGI1's rule). */
			{
				int wTotal = 0, wSoup = 0, wParts = 0, pfxRefs = 0, pfxNear = 0;
				QSet<QString> pfxModelSet;
				for ( const Placement & p : placements ) {
					if ( !p.actorKey.isEmpty() || p.disabled || p.cable )
						continue;
					const EsmLodBase & lb = world.lodBase( p.base );
					if ( CellPickTable::typeName( lb.type ) == QLatin1String( "WEAP" ) ) {
						wTotal++;
						wSoup += soupRole( QStringLiteral( "WEAP" ) ) == 1 ? 1 : 0;
						wParts += weapons.wants( p.base ) ? 1 : 0;
					}
					if ( lb.model.isEmpty() || !lodgenModelHasParticles( lb.model ) )
						continue;
					pfxRefs++;
					pfxModelSet.insert( lb.model.toLower() );
					for ( const EsmRefr & lr : lightRefs ) {
						const float dx = lr.pos[0] - p.pos[0], dy = lr.pos[1] - p.pos[1], dz = lr.pos[2] - p.pos[2];
						if ( dx * dx + dy * dy + dz * dz <= 256.0f * 256.0f && !startsDisabled( lr ) ) {
							pfxNear++;
							break;
						}
					}
				}
				t << "  probe soup placed weapons: " << wTotal << " (in the soup " << wSoup << ", with mod parts " << wParts
				  << ( wwLookOn( WwLook::WeaponParts ) ? "" : "; Weapon mod parts row off" ) << ")\n";
				t << "  probe soup placed particles left out: placements " << pfxRefs << " of " << pfxModelSet.size()
				  << " models (models loaded this build only), with a placed light lit within 256 units " << pfxNear << "\n";
			}
			// lane CAPTURE1: refraction-only shapes left out of the soup, per model
			t << "  probe soup water-shader shapes left out " << soupWaterShapes << " (" << soupWaterTris << " triangles)"
			  << ( waterSoupRed ? " RED: kept as surfaces" : "" ) << "\n";	// lane PRTP6
			t << "  probe soup refraction-only shapes left out " << soupRefractShapes << " (" << soupRefractTris
			  << " triangles)" << ( refractKeepRed ? " RED keep: kept as surfaces" : "" );
			for ( auto it = soupRefractModels.cbegin(); it != soupRefractModels.cend(); ++it )
				t << "; " << it.key() << " x" << it.value();
			t << "\n";
			if ( bakeLean )   // lane SPEED1
				t << "  headless bake: " << leanSkipped << " placements the soup leaves out were not loaded\n";
			if ( baking ) {   // lane BAKE4
				if ( soupWaterCells > 0 )   // lane WATER1 (the pin WW_CELL_BAKE_WATER=0 prints nothing new)
					t << "  bake water: " << soupWaterCells << " cell planes, " << int( probeSoup.waterTypes.size() )
					  << " records, " << int( probeSoup.water.size() / 9 ) << " triangles before the far ring\n";
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
			if ( !baking && placed && !wwEnv( "WW_CELL_GI_FROM" ).isEmpty() )
				giBakeDir = QDir::cleanPath( QString::fromLocal8Bit( wwEnv( "WW_CELL_GI_FROM" ) ) );
			if ( baking && placed ) {
				QString dir =!bakeEnv.isEmpty() ? bakeEnv : !bakeDirEnv.isEmpty() ? bakeDirEnv : spec.bakeDir;
				if ( dir.isEmpty() ) {
					QString name = spec.interior ? spec.interiorCell : spec.world;
					name.replace( QRegularExpression( QStringLiteral( "[^A-Za-z0-9_+-]" ) ), QStringLiteral( "_" ) );
					dir = QCoreApplication::applicationDirPath() + QStringLiteral( "/prtp_bake/" )
						+ ( name.isEmpty() ? QStringLiteral( "cell" ) : name );
				}
				ProbeBakeSpec bs;
				// lane BAKENEIGH1: write only the probed cells' files (a sector is one cell); never a neighbour's
				bs.ownX0 = ownCells[0];
				bs.ownY0 = ownCells[1];
				bs.ownX1 = ownCells[2];
				bs.ownY1 = ownCells[3];
				const QByteArray br = wwEnv( "WW_CELL_PROBE_BAKE_RAYS" );
				if ( br.toInt() > 0 )
					bs.rays = br.toInt();
				bs.red = QString::fromLatin1( wwEnv( "WW_PROBE_BAKE_RED" ) );
				// lane SIDES6: the file version a gate asks for (the census's, and the two-sides red: 4)
				if ( wwEnv( "WW_CELL_PROBE_BAKE_TBK" ).toInt() >= 3 && wwEnv( "WW_CELL_PROBE_BAKE_TBK" ).toInt() <= 5 )
					bs.tbkVersion = wwEnv( "WW_CELL_PROBE_BAKE_TBK" ).toInt();
				if ( roomclampPin && bs.red.isEmpty() )
					bs.red = QStringLiteral( "backface" );   // lane ROOMCLAMP1 pin: no probe moved
				// lane ROOMCLAMP1: the outside-the-shell threshold (gate red: 1 = off) and the gate's list
				if ( wwEnvIsSet( "WW_CELL_PROBE_BACKMAX" ) )
					bs.backMax = wwEnvStr( "WW_CELL_PROBE_BACKMAX" ).toFloat();
				bs.backDump = wwEnvStr( "WW_CELL_PROBE_BACKDUMP" );
				// lane GIPROBE2: Probes out of walls (Show menu; the row reopens the cell, so this bake sees it)
				bs.probeClear = wwLookOn( WwLook::ProbeClear ) ? 32.0f : 0.0f;
				bs.clearDump = wwEnvStr( "WW_CELL_PROBE_CLEAR_DUMP" );
				bs.waterDump = wwEnvStr( "WW_CELL_WATER_BAKEDUMP" );   // lane WATER1
				for ( const QString & q : wwEnvStr( "WW_CELL_WATER_BAKEDUMP_PROBES" ).split( ';', Qt::SkipEmptyParts ) ) {
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
						bs.waterDumpProbes.push_back( best );
				}
				// lane SMOOTHN1: the most batches a noisy probe takes (1 = the base set alone: the gate's pin)
				if ( wwEnvInt( "WW_CELL_PROBE_BAKE_ADAPT" ) > 0 )
					bs.adaptMax = wwEnvInt( "WW_CELL_PROBE_BAKE_ADAPT" );
				/* lane GAPBAKE1: WW_CELL_BAKE_ROOMSPLIT=1 keeps a surfel cell's rooms apart (a 70-unit cell straddling a
				 * thin wall no longer hands the lit room's floor to the dark room's probes); off = the old .tbk */
				bs.roomSplit = wwEnvStr( "WW_CELL_BAKE_ROOMSPLIT" ) == QLatin1String( "1" );
				// an interior's misses are void, never sky -- unless its cell shows the sky (lane SKYINT1, the deck's
				// rule: a ray that meets nothing sees the sky)
				bs.noSky = spec.interior && !( cellInteriorFlags( world.interior() ) & 0x0080u );
				if ( wantMat ) {   // lane CAPTURE1: the albedo way, its maps loaded once (the bake reads them from threads)
					bs.albedoWay = albWay;
					bs.albedoRed = albRed;
					const int cf = wwEnvInt( "WW_CELL_BAKE_CUBE_FACE" );
					if ( cf > 0 )
						bs.cubeFace = cf;
					probeSoup.mat.resize( size_t( probeSoup.triCount() ) );   // the ground: its tri value
					probeSoup.matTexPtr.clear();
					for ( const QString & tx : probeSoup.matTex )
						probeSoup.matTexPtr.push_back( probeAlb.loadFine( tx ) );
					bs.cubeDump = wwEnvStr( "WW_CELL_BAKE_CUBE_DUMP" );
					for ( const QString & q : wwEnvStr( "WW_CELL_BAKE_CUBE_PROBES" ).split( ';', Qt::SkipEmptyParts ) ) {
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
				{	// lane FIXPACK1 (PBRALBEDO1): written only when a .pbrm served (a vanilla bake's notes are unchanged)
					int served = 0;
					for ( auto it = bakePbrm.constBegin(); it != bakePbrm.constEnd(); ++it )
						served += it.value() ? 1 : 0;
					if ( served || pbrAlbRed )
						t << "  bake albedo pbrm: " << served << " of " << bakePbrm.size() << " materials served by a .pbrm, "
						  << albPbrm << " triangles read base x (1 - metallic)"
						  << ( pbrAlbRed ? " RED diffuse (every surface read its legacy diffuse slot)" : "" ) << "\n";
				}
				if ( !bakePbrmTracePath.isEmpty() ) {	// lane PBRALBEDO2: diagnosis only
					QFile tf( bakePbrmTracePath );
					if ( tf.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
						tf.write( ( QStringLiteral( "matName\troute\tpath\trefusal\ttried\n" )
							+ bakePbrmTrace.join( QChar( '\n' ) ) + QChar( '\n' ) ).toUtf8() );
				}
				// lane SMOOTHN1: where the soup's vertex normals came from
				t << "  bake smooth normals " << ( smoothOn ? "on" : "OFF (WW_CELL_BAKE_SMOOTH=0)" ) << ", normal maps "
				  << ( nmapOn ? "on" : "OFF" ) << ": object triangles with NIF normals " << soupSmoothTris << ", ground cells from VNML "
				  << soupLandVnml << ", from the heights (no VNML) " << soupLandDerived << ", model-space normal maps left out "
				  << soupNmapModelSpace << "\n";
				// lane GAPBAKE1: the room split's rooms, the soup's grid (the GI's rule) before the bake
				ProbeRooms splitRooms;
				if ( bs.roomSplit ) {
					ProbeRoomSpec rsp;
					rsp.red = QString::fromLatin1( wwEnv( "WW_CELL_ROOMS_RED" ) ).trimmed();
					if ( probeBakeRoomGrid( probeSoup, pr.probes, rsp, &splitRooms ) )
						bs.roomGrid = &splitRooms;
				}
				// lane BAKE4: the placer's room boxes go into the `.tbk` v4 files
				if ( !probeBake( probeSoup, pr.probes, bs, QDir::cleanPath( dir ), &bres, &pr.roomBoxes ) ) {
					t << "  bake REFUSED: " << bres.error << "\n";
				} else {
					for ( const QString & line : probeBakeCensusText( bres ).split( '\n', Qt::SkipEmptyParts ) )
						t << "  " << line << "\n";
					t << "  bake folder " << QDir::toNativeSeparators( QDir::cleanPath( dir ) ) << "\n";
					giBakeDir = QDir::cleanPath( dir );
					/* lane PGI0: the .pgi ("Precomputed GI", src/pgi_format.h) beside the .tbk, from the files this bake
					 * just wrote (no new tracing): exterior <X>_<Y>.pgi a cell, interior <Cell EditorID>.pgi */
					if ( bs.tbkVersion == 5 && bs.red.isEmpty() ) {
						PgiWriteSpec ps;
						ps.interior = spec.interior;
						ps.world = spec.world;
						ps.interiorName = spec.interiorCell;
						ps.interiorName.replace( QRegularExpression( QStringLiteral( "[^A-Za-z0-9_+-]" ) ), QStringLiteral( "_" ) );
						ps.options = QStringLiteral( "rays %1 maxLinks %2 cell %3 tbk %4 albedo %5 noSky %6 adapt %7" )
							.arg( bs.rays ).arg( bs.maxLinks ).arg( double( bs.surfelCell ) ).arg( bs.tbkVersion )
							.arg( bs.albedoWay ).arg( int( bs.noSky ) ).arg( bs.adaptMax ).toUtf8();
						PgiWriteResult pres;
						pgiWriteFromTbk( bres.files, ps, QDir::cleanPath( dir ), &pres );
						t << "  " << pgiWriteCensusText( pres ) << "\n";
					}
					/* lane BAKEINCR1: what this bake read, beside what it wrote (src/probeincr.h). The
					 * far line is its own ring read again into an empty soup -- warm, about a second. */
					if ( incrRecord ) {
						ProbeIncr::stopRecording();
						QElapsedTimer rt;
						rt.start();
						const int half = ( spec.n - 1 ) / 2;
						const QString recs = ProbeIncr::recordsDigest( world, spec.cx, spec.cy, half );
						const QString recs1 = ProbeIncr::recordsDigest( world, spec.cx, spec.cy, 0 );
						const QString far = cellBakeFarDigest( spec.world, dataRoot, spec.cx, spec.cy, half, &world );
						QString rerr;
						// lane FARLIGHTMAP1: a far-light bake's record holds its lights too (light moves no .tbk byte)
						const QString lights = qEnvironmentVariableIsEmpty( "WW_CELL_FARLIGHT" ) ? QString()
							: ProbeIncr::lightsDigest( world, spec.cx, spec.cy, half );
						if ( ProbeIncr::writeRecord( QDir::cleanPath( dir ), spec.world, spec.cx, spec.cy, half, recs, recs1, far, &rerr, lights ) )
							t << "  bake record " << ProbeIncr::recordName() << ": " << ProbeIncr::recorded().size()
							  << " assets, records " << recs.left( 12 ) << ", far " << far.left( 12 ) << ", "
							  << rt.elapsed() << " ms\n";
						else
							t << "  bake record REFUSED: " << rerr << "\n";
					}
				}
			}
		}
		// the markers (placed and counted always; drawn with the PRTP band's Show probes on)
		if ( spec.probesShow )
			cellProbeMarkerBuckets( pr.probes, origin, buckets );
		if ( !chunk )
			liveProbes = pr.probes;	// lane LIVESET1
	}

	/* lane NIFXLIGHT1: the placed fixtures whose model has a .nifx (src/gl/celllights.cpp reads and applies it live).
	 * NIFXLIGHT3: every placed model is kept (model null without one), so a .nifx made later is found live. */
	QVector<WwNifxFixture> nifxFix;
	{
		QHash<QString, std::shared_ptr<const WwNifxModel>> models;
		for ( const Placement & p : placements ) {
			const QString model = p.disabled || !p.actorKey.isEmpty() ? QString() : world.lodBase( p.base ).model;
			if ( model.isEmpty() )
				continue;
			auto it = models.constFind( model.toLower() );
			if ( it == models.cend() )
				it = models.insert( model.toLower(), cellNifxModel( dataRoot, model ) );
			WwNifxFixture f;
			f.model = *it;
			f.modelPath = model;
			f.dataRoot = dataRoot;
			for ( int r = 0; r < 3; r++ ) {
				f.pos[r] = p.pos[r];
				for ( int c = 0; c < 3; c++ )
					f.rot[r][c] = p.rot( r, c );
			}
			f.scale = p.scale;
			f.ref = p.ref;
			nifxFix.append( f );
		}
	}
	// lane NIFXCAL1: the pick panel reads out each light under its own ref and under the smallest box holding it
	// (+ 32 units: a bulb's light sits just off the fixture's mesh)
	for ( const EsmRefr & r : lightRefs ) {
		const EsmLight & b = world.light( r.base );
		if ( !b.exists )
			continue;
		CellPickLight pl;
		pl.ref = r.formID;
		pl.base = r.base;
		pl.edid = b.edid;
		for ( int k = 0; k < 3; k++ )
			pl.color[k] = b.color[k];
		pl.fade = b.fade + ( r.xligCount >= 2 ? r.xlig[1] : 0.0f );
		pl.fov = b.fov + ( r.xligCount >= 1 ? r.xlig[0] : 0.0f );
		pl.radius = float( b.radius ) + ( r.hasRadius ? r.radius : 0.0f );
		pl.flags = b.flags;
		pl.box = r.lightBox != 0;
		int best = -1;
		float bestVol = 0.0f;
		for ( int i = 0; i < picks.size(); i++ ) {
			const CellPickEntry & e = picks.at( i );
			if ( e.refForm == r.formID ) {
				picks.addLight( i, pl );
				continue;
			}
			bool in = true;
			for ( int k = 0; k < 3 && in; k++ )
				in = r.pos[k] >= e.bmin[k] - 32.0f && r.pos[k] <= e.bmax[k] + 32.0f;
			const float vol = ( e.bmax[0] - e.bmin[0] + 64.0f ) * ( e.bmax[1] - e.bmin[1] + 64.0f ) * ( e.bmax[2] - e.bmin[2] + 64.0f );
			if ( in && ( best < 0 || vol < bestVol ) ) {
				best = i;
				bestVol = vol;
			}
		}
		if ( best >= 0 && picks.at( best ).lights.size() < 8 )
			picks.addLight( best, pl );
	}

	// ---- emit
	CellSpeed::mark( "ground, water, lights, probes" );
	if ( chunk ) {	// lane CELLSTREAM1: a streamed cell stops here; src/cellstream.cpp writes it in slices
		cellPublishLighting( nullptr, world, spec, lightRefs, primRefs, origin, &chunk->lights, &lightStartOff, &nifxFix );
		chunk->buckets = std::make_shared<CellChunkBuckets>();
		chunk->buckets->keys = buckets.keys();
		std::sort( chunk->buckets->keys.begin(), chunk->buckets->keys.end() );
		chunk->buckets->buckets = std::move( buckets );
		for ( const Placement & p : placements ) {	// lane LIVESET3 (STREAMPSYS1): the particle step's filter below
			if ( !p.actorKey.isEmpty() || p.disabled )
				continue;
			const QString model = world.lodBase( p.base ).model;
			if ( !model.isEmpty() && lodgenModelHasParticles( model ) )
				chunk->pfx.push_back( { model, p.pos - origin, p.rot, p.scale } );
		}
		chunk->ok = true;
		return true;
	}
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
	// lane LIVESET3: where the probe markers' buckets go (shown or not), so a live Show probes puts them there
	int liveMarkersAt = -1, liveMarkersEnd = -1;
	for ( const QString & k : keys ) {
		const bool markerKey = k.startsWith( QLatin1String( "\x01probes" ) );
		if ( liveMarkersAt < 0 && ( markerKey || k.compare( QLatin1String( "\x01probes" ) ) > 0 ) )
			liveMarkersAt = nif->getBlockCount();
		if ( liveMarkersAt >= 0 && liveMarkersEnd < 0 && !markerKey )
			liveMarkersEnd = nif->getBlockCount();
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
	if ( liveMarkersAt < 0 )
		liveMarkersAt = nif->getBlockCount();
	if ( liveMarkersEnd < 0 )
		liveMarkersEnd = nif->getBlockCount();

	/* lane SUNCELL1: THE PARTICLES. The weld takes triangle shapes only (lodgen.cpp), so a placed model's
	 * particle systems (steam, smoke, fire, sparks, dust) never reached the cell, and a model of particles alone
	 * was a failed model. With the Particles row on, each placed model whose parse found one has its systems
	 * copied in under the reference's transform (cellCopyParticleBranch); the viewer's simulation runs them and
	 * Particles::drawShapes draws them with the cell's effect law (linear light, the fog, the HDR frame or the
	 * imagespace), as the game draws its particles with the effect shader. A disabled reference and an actor
	 * carry none. WW_CELL_PARTICLES_MAX (512) caps the copies. */
	if ( ok && !bakeLean ) {
		if ( wwCellParticlesOn() ) {
			WW_SWITCH_STATIC( int, cap, wwEnvIsSet( "WW_CELL_PARTICLES_MAX" )
				? wwEnvInt( "WW_CELL_PARTICLES_MAX" ) : 512 );
			QHash<QString, std::shared_ptr<NifModel>> pfxSrc;	// one parse a model; null = unusable
			QPersistentModelIndex iPfx;
			int pfxCopies = 0, pfxSystems = 0, pfxModels = 0, pfxRefused = 0, pfxCapped = 0;
			int pfxLitSystems = 0, pfxLitModels = 0, pfxLitCapped = 0;
			int pfxXemiSystems = 0, pfxXemiNoFlag = 0, pfxXemiLit = 0;	// lane LIGHTS3 (XEMI-PARTICLES)
			QHash<QString, bool> pfxBgemLit;	// a BGEM path (lower case) -> lit
			QString pfxWhy;
			for ( const Placement & p : placements ) {
				if ( !p.actorKey.isEmpty() || p.disabled )
					continue;
				const QString model = world.lodBase( p.base ).model;
				if ( model.isEmpty() || !lodgenModelHasParticles( model ) )
					continue;
				if ( pfxCopies >= cap ) {
					pfxCapped++;
					continue;
				}
				const QString key = model.toLower();
				auto it = pfxSrc.find( key );
				if ( it == pfxSrc.end() ) {
					auto m = std::make_shared<NifModel>();
					QByteArray bytes;
					QBuffer dev( &bytes );
					bool good = lodgenReadModelBytes( dataRoot, model, bytes ) && dev.open( QIODevice::ReadOnly )
						&& m->load( dev, model.toLocal8Bit().constData() );
					if ( good ) {
						m->resetState();
						good = m->getVersionNumber() == nif->getVersionNumber() && m->getBSVersion() == nif->getBSVersion();
					}
					if ( !good ) {
						m.reset();
						pfxRefused++;
						pfxWhy = QStringLiteral( "%1 unreadable or not this version" ).arg( model );
					} else {
						pfxModels++;
					}
					it = pfxSrc.insert( key, m );
				}
				if ( !it.value() )
					continue;
				if ( !iPfx.isValid() ) {
					QModelIndex iP = nif->insertNiBlock( QStringLiteral( "NiNode" ) );
					nif->set<QString>( iP, "Name", QStringLiteral( "particles" ) );
					nif->set<quint32>( iP, "Flags", 14 );
					nif->set<float>( iP, "Scale", 1.0f );
					addLink( nif, iRoot, QStringLiteral( "Children" ), nif->getBlockNumber( iP ) );
					iPfx = iP;
				}
				QVector<CellPfxSystem> copied;
				const int n = cellCopyParticleBranch( nif, QModelIndex( iPfx ), *it.value(), p.pos - origin, p.rot, p.scale,
					&pfxWhy, &copied );
				/* lane SUNCELL1 (last round): A LIT PARTICLE. In the game the effect technique's Lit bit comes from the
				 * property's lighting flag alone, beside Ptcl (checked in Todd's treat),
				 * and the lit particle pixel shader sums the same four placed lights as a lit effect card. So a copied
				 * system whose effect property is lit (the BGEM's Effect Lighting and influence, or Shader Flags 2 bit 30
				 * and Lighting Influence, the welded shapes' rule) joins FXLIT1's table as one more placed model: its
				 * bound is the merge of its lit systems' spheres, placed by the reference. Interiors only, as FXLIT1. */
				if ( n > 0 && fxLitOn ) {
					NifModel & sm = *it.value();
					int serial = -1;
					float all[4] = { 0.0f, 0.0f, 0.0f, -1.0f };
					QVector<qint32> litBlocks;
					for ( const CellPfxSystem & f : copied ) {
						const QModelIndex iSh = sm.getBlockIndex( f.srcShader );
						if ( !iSh.isValid() || !sm.blockInherits( iSh, "BSEffectShaderProperty" ) )
							continue;
						const QString mat = sm.get<QString>( iSh, "Name" );
						bool lit;
						if ( mat.endsWith( QStringLiteral( ".bgem" ), Qt::CaseInsensitive ) ) {
							const QString mk = mat.toLower();
							auto bl = pfxBgemLit.constFind( mk );
							if ( bl == pfxBgemLit.constEnd() )
								bl = pfxBgemLit.insert( mk, lodgenEffectMaterialLit( dataRoot, mat ) );
							lit = bl.value();
						} else {
							lit = ( sm.get<quint32>( iSh, "Shader Flags 2" ) & 0x40000000U )
								&& sm.get<quint8>( iSh, "Lighting Influence" ) > 0;
						}
						if ( !lit )
							continue;
						const float one[4] = { f.center[0], f.center[1], f.center[2], f.radius };
						wwCellFxLitMerge( all, one );
						litBlocks.append( f.block );
					}
					if ( !litBlocks.isEmpty() && fxLitModels >= fxLitCap ) {
						pfxLitCapped++;
					} else if ( !litBlocks.isEmpty() ) {
						const Vector3 wc = p.pos + p.rot * ( Vector3( all[0], all[1], all[2] ) * p.scale );
						WwFxLitModel m;
						for ( int k = 0; k < 3; k++ )
							m.center[k] = wc[k];
						m.radius = std::max( all[3], 0.0f ) * p.scale;
						m.ref = p.ref;
						m.model = model;
						serial = wwCellFxLitModel( nif, m );
						fxLitModels++;
						pfxLitModels++;
						for ( const qint32 b : litBlocks )
							wwCellFxLitShape( nif, b, serial );
						pfxLitSystems += litBlocks.size();
					}
				}
				/* lane LIGHTS3 (XEMI-PARTICLES): an UNLIT system with External Emittance (SF1 bit 29) takes the placement's
				 * emittance as an unlit effect card does; the game's lit effect variants never read it. */
				if ( n > 0 && p.xemi && xemiBuild ) {	// lane LIGHTS4: the particle row is read at draw time (bit 0x04000000)
					NifModel & sm = *it.value();
					for ( const CellPfxSystem & f : copied ) {
						const QModelIndex iSh = sm.getBlockIndex( f.srcShader );
						if ( !iSh.isValid() || !sm.blockInherits( iSh, "BSEffectShaderProperty" ) )
							continue;
						if ( !( sm.get<quint32>( iSh, "Shader Flags 1" ) & ( 1U << 29 ) ) ) {
							pfxXemiNoFlag++;
							continue;
						}
						const QString mat = sm.get<QString>( iSh, "Name" );
						const bool lit = mat.endsWith( QStringLiteral( ".bgem" ), Qt::CaseInsensitive )
							? lodgenEffectMaterialLit( dataRoot, mat )
							: ( ( sm.get<quint32>( iSh, "Shader Flags 2" ) & 0x40000000U )
								&& sm.get<quint8>( iSh, "Lighting Influence" ) > 0 );
						if ( lit ) {
							pfxXemiLit++;
							continue;
						}
						wwCellXemiShape( nif, f.block, p.xemi | 0x04000000U );
						pfxXemiSystems++;
					}
				}
				if ( n > 0 ) {
					pfxCopies++;
					pfxSystems += n;
				} else if ( n < 0 ) {
					pfxRefused++;
				}
			}
			fprintf( stderr, "cell particles: %d systems in %d copies of %d models (%d refused%s%s, %d past the cap of %d)\n",
				pfxSystems, pfxCopies, pfxModels, pfxRefused, pfxWhy.isEmpty() ? "" : ": last ",
				qPrintable( pfxWhy ), pfxCapped, cap );
			fprintf( stderr, "cell particles lit: %d systems of %d placed models registered with their placed lights%s (%d past the lit cap)\n",
				pfxLitSystems, pfxLitModels, fxLitOn ? ( spec.interior ? " (interior)" : " (exterior: the sun is the base)" )
					: " (off: overlay)", pfxLitCapped );
			fprintf( stderr, "cell particles emittance: %s, %d unlit systems take their placement's colour (%d lack External "
				"Emittance, %d lit: the game's lit variants never read it)\n",
				wwLookOn( WwLook::ParticleEmittance ) ? "on" : "off", pfxXemiSystems, pfxXemiNoFlag, pfxXemiLit );
		} else {
			fprintf( stderr, "cell particles: off (the Particles row)\n" );
		}
	}

	/* lane DOORLIVE1: THE LIVE DOOR. The placement WW_CELL_DOOR_ANIM names is left out of the weld above and its model
	 * copied in whole (shapes, nodes, controllers, sequences; no collision, no lights) under a node carrying the
	 * placement, so its own Open / Close sequence moves it (src/celldoorlive.h). */
	if ( ok && doorLiveRef ) {
		QString why;
		qint32 wrapper = -1, root = -1, count = 0;
		QString model;
		for ( const Placement & p : placements ) {
			if ( p.ref != doorLiveRef || !p.actorKey.isEmpty() )
				continue;
			model = world.lodBase( p.base ).model;
			auto m = std::make_shared<NifModel>();
			QByteArray bytes;
			QBuffer dev( &bytes );
			bool good = !model.isEmpty() && lodgenReadModelBytes( dataRoot, model, bytes ) && dev.open( QIODevice::ReadOnly )
				&& m->load( dev, model.toLocal8Bit().constData() );
			if ( good ) {
				m->resetState();
				good = m->getVersionNumber() == nif->getVersionNumber() && m->getBSVersion() == nif->getBSVersion();
			}
			if ( !good )
				why = QStringLiteral( "%1 unreadable or not this version" ).arg( model );
			else
				count = cellCopyDoorBranch( nif, iRoot, *m, p.pos - origin, p.rot, p.scale, p.ref, &wrapper, &root, &why );
			break;
		}
		if ( model.isEmpty() && why.isEmpty() )
			why = QStringLiteral( "no placement with that ref in the cell" );
		const QString note = count > 0 ? QStringLiteral( "%1 blocks copied, %2 welded shapes left out" ).arg( count ).arg( doorLiveShapes )
			: QStringLiteral( "not copied: %1" ).arg( why );
		wwDoorLiveSetBranch( nif, doorLiveRef, model, count > 0 ? wrapper : -1, count > 0 ? root : -1, count, note );
		fprintf( stderr, "door live: ref %08x model %s: %s\n", doorLiveRef, qPrintable( model ), qPrintable( note ) );
	}

	/* lane FXPARITY2: THE ANIMATED OBJECTS, copied whole under nodes carrying their placements (cellCopyDoorBranch with
	 * animLoop); the world clock binds each copy's looping sequence and runs its free controllers (cellworldanim.cpp). */
	if ( ok && nif && !chunk ) {
		QVector<WwAnimObjRoot> roots;
		if ( animObjOn ) {
			QHash<QString, std::shared_ptr<NifModel>> srcs;	// one parse a model; null = unusable
			int blocks = 0, refused = 0, looping = 0;
			QString why;
			for ( const Placement & p : placements ) {
				if ( !animObjRefs.contains( p.ref ) || !p.actorKey.isEmpty() )
					continue;
				const QString model = world.lodBase( p.base ).model;
				const QString key = model.toLower();
				auto it = srcs.find( key );
				if ( it == srcs.end() ) {
					auto m = std::make_shared<NifModel>();
					QByteArray bytes;
					QBuffer dev( &bytes );
					bool good = !model.isEmpty() && lodgenReadModelBytes( dataRoot, model, bytes ) && dev.open( QIODevice::ReadOnly )
						&& m->load( dev, model.toLocal8Bit().constData() );
					if ( good ) {
						m->resetState();
						good = m->getVersionNumber() == nif->getVersionNumber() && m->getBSVersion() == nif->getBSVersion();
					}
					if ( !good ) {
						m.reset();
						why = QStringLiteral( "%1 unreadable or not this version" ).arg( model );
					}
					it = srcs.insert( key, m );
				}
				if ( !it.value() ) {
					refused++;
					continue;
				}
				qint32 wrapper = -1, root = -1;
				QString loop;
				const int n = cellCopyDoorBranch( nif, iRoot, *it.value(), p.pos - origin, p.rot, p.scale, p.ref, &wrapper,
					&root, &why, &loop );
				if ( n <= 0 ) {
					refused++;
					continue;
				}
				WwAnimObjRoot r;
				r.root = root;
				r.loopSeq = loop;
				roots.append( r );
				blocks += n;
				looping += loop.isEmpty() ? 0 : 1;
			}
			fprintf( stderr, "cell animated objects: %d copies of %d models (%d blocks, %d with a looping sequence), %d welded shapes"
				" left out, %d refused%s%s, %d past the cap of %d\n", int( roots.size() ), int( srcs.size() ), blocks, looping,
				animObjShapes, refused, why.isEmpty() ? "" : ": last ", qPrintable( why ), int( animObjCapped.size() ), animObjCap );
		}
		wwCellAnimObjSet( nif, roots );
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

	/* lane FARLOD1: the far field around the block (cellfarlod.h) -- the Far LOD row, ships off */
	QString farLodNotes;
	CellFarStream farStream;   // lane CELLSTREAM1: ring 0 uncut over the roam area, a shape a cell
	if ( wwEnvIsSet( "WW_CELL_STREAM_ROAM" ) )
		farStream.roam = qBound( 2, wwEnvInt( "WW_CELL_STREAM_ROAM" ), 16 );
	const int liveFarFirst = chunk ? -1 : nif->getBlockCount();	// lane LIVESET1: where the far section starts
	QString liveFarLodl;
	if ( !spec.interior && cellFarLodWanted() ) {
		QStringList farSearched;
		const QString lodWs = cellLodWs( spec.world, &world );	// lane LANDSPEC4: a child draws its parent's rings
		if ( lodWs != spec.world )
			std::fprintf( stderr, "far lod: %s draws the far LOD of %s\n", qPrintable( spec.world ), qPrintable( lodWs ) );
		const QString farLodl = cellFarLodl( lodWs, dataRoot, farSearched );
		liveFarLodl = farLodl;
		farLodNotes = farStackNote + cellFarLodAppend( nif, iRoot, world, lodWs, spec.cx, spec.cy, spec.n, farLodl, origin,
			streamInit ? &farStream : nullptr );
		CellSpeed::mark( "far LOD appended" );
	}
	if ( !chunk ) {	// lane WATERDATA1: the worldspace's baked water planes, far LOD on or off ("" = interior)
		QStringList infoSearched;
		wwCellWaterInfoSet( nif, spec.interior ? QString() : !liveFarLodl.isEmpty() ? liveFarLodl
			: cellFarLodl( cellLodWs( spec.world, &world ), dataRoot, infoSearched ) );
	}

	nif->holdUpdates( false );
	nif->updateModel();
	CellSpeed::mark( "document updateModel" );
	// lane LIVESET1: what a live set of the far LOD or the probe markers rebuilds from
	liveKeep( nif, spec, worldOwn, liveFarLodl, liveFarFirst, origin, nif->getBlockNumber( iRoot ), streamInit, std::move( liveProbes ),
		liveMarkersAt, liveMarkersEnd - liveMarkersAt, liveAsked, liveStack );
	// lane MODPICK1: "Mods differ from the far LOD bake", over the view, for as long as this document is the cell
	cellModPickSetViewNote( nif, modPickViewNote );
	{
		static QSet<const void *> hooked;
		if ( !hooked.contains( nif ) ) {
			hooked.insert( nif );
			const void * key = nif;
			QObject::connect( nif, &QAbstractItemModel::modelReset, nif, [nif, key]() {
				if ( nif->getBlockCount() == 0 )
					cellModPickSetViewNote( key, QString() );
			} );
			QObject::connect( nif, &QObject::destroyed, [key]() {
				cellModPickSetViewNote( key, QString() );
				hooked.remove( key );
			} );
		}
	}

	// lane PRTP3: the renderer lights this document with the cell's own lights (the Cell lights row)
	cellPublishLighting( nif, world, spec, lightRefs, primRefs, origin, nullptr, &lightStartOff, &nifxFix );
	wwCellFxLitPick( nif );   // lane FXLIT1: each lit effect model's four placed lights
	if ( streamInit ) {	// lane CELLSTREAM1: the cells themselves come from the streamer, round the camera
		seed.origin = origin;
		seed.rootBlock = nif->getBlockNumber( iRoot );
		seed.cx = spec.cx;
		seed.cy = spec.cy;
		seed.roam = farStream.roam;
		seed.fixedWorld = LandLodSpec::fixedWorld( world.worldFlags() );	// lane LANDSPEC6: the block stays on WCTR
		seed.fixedX = world.fixedCentreX();
		seed.fixedY = world.fixedCentreY();
		if ( const WwCellLighting * L = wwCellLightsFor( nif ) )
			seed.base = *L;
		seed.farHand = farStream.handBlocks;
		seed.farLand = farStream.landBlocks;
		cellStreamStart( nif, seedPtr );
	}

	/* lane PRTPGI: the bake just written, relit by those lights (src/probegi.h), for the GI row.
	 * WW_CELL_GI_DUMP=<folder> writes the gate's copies; WW_CELL_GI_RED=<red> its refuters. */
	if ( !giBakeDir.isEmpty() ) {
		if ( const WwCellLighting * L = wwCellLightsFor( nif ) ) {
			ProbeGiSpec gs;
			gs.red = QString::fromLatin1( wwEnv( "WW_CELL_GI_RED" ) ).trimmed();
			if ( gs.red.isEmpty() && wwEnv( "WW_CELL_ROOMCLAMP_PIN" ).trimmed() == "off" )
				gs.red = QStringLiteral( "prelane" );   // lane ROOMCLAMP1 pin: no rooms, no eye fallbacks
			// lane BRICK1: the probes gather the deck's 4 m bricks (off: their links, as before); WW_CELL_GI_BRICK_RED its refuters
			gs.bricks = wwEnvInt( "WW_CELL_GI_BRICKS" ) == 1;
			gs.brickRed = QString::fromLatin1( wwEnv( "WW_CELL_GI_BRICK_RED" ) ).trimmed();
			gs.emissiveGrid = wwCellEmissiveGridOn();	// lane EMISSIVEGRID1: the fine boxes round bright glow
			gs.emissiveGridRed = QString::fromLatin1( qgetenv( "WW_CELL_EMISSIVEGRID_RED" ) ).trimmed();
			// lane SKYPIC1 (part B, the review sheet): WW_CELL_GI_BRICK_EDGE=<game units> sizes the bricks (unset: 4 m)
			if ( wwEnvIsSet( "WW_CELL_GI_BRICK_EDGE" ) )
				gs.brickEdge = wwEnvStr( "WW_CELL_GI_BRICK_EDGE" ).toFloat();
			gs.passes = wwEnvInt( "WW_CELL_GI_PASSES" );   // lane BOUNCE2: a gate's pin (1 = one bounce)
			gs.lightPhys = wwLightPhysParams();   // lane LIGHTPHYS2: the bounce takes the surfaces' falloff and intensity curve
			gs.rooms.red = QString::fromLatin1( wwEnv( "WW_CELL_ROOMS_RED" ) ).trimmed();   // lane ROOMCLAMP1: the rooms' refuters
			gs.space = volGiOn ? volGi : nullptr;   // lane VOLFIT1: WW_CELL_VOLUMES_GI=1, the Space rooms are the GI's rooms
			// lane GPURELIGHT1 (10-23): Probe sharing (Show menu); a later tick relights through probesky.cpp
			gs.share = wwLookOn( WwLook::ProbeShare ) || wwLookOn( WwLook::ProbeShareWide );   // GIPROBE2: wide implies sharing
			gs.shareRed = wwEnvStr( "WW_CELL_GI_SHARE_RED" ).trimmed();
			// lane GIPROBE2: Wide probe sharing (3 steps) and Probe depth visibility, both relit live by probesky.cpp
			gs.sharePasses = wwLookOn( WwLook::ProbeShareWide ) ? 3 : 1;
			gs.depthVis = wwLookOn( WwLook::ProbeDepthVis );
			gs.depthVisRed = wwEnvStr( "WW_CELL_GI_DEPTHVIS_RED" ).trimmed();
			// lane SKY1: outdoors, the weather's sky and sun (src/probesky.h); WW_CELL_SKY_RED its refuters
			// lane SKYINT1: and an interior whose cell shows the sky (the sun where Use Sky Lighting + Sunlight Shadows)
			const quint16 cf = spec.interior ? cellInteriorFlags( world.interior() ) : quint16( 0 );
			gs.interiorSky = ( cf & 0x0080u ) != 0;
			if ( probeSoup.glow.hasAlt() )   // lane LIGHTS5: the bake keeps the other emittance state for a live flip
				gs.emitBaked = gs.emitWant = xemiGiOn ? 1 : 0;
			gs.interiorSun = gs.interiorSky && ( cf & 0x0900u ) == 0x0900u;
			if ( !spec.interior || gs.interiorSky ) {
				gs.sky = probeSkyLightNow();
				gs.skyRed = QString::fromLatin1( wwEnv( "WW_CELL_SKY_RED" ) ).trimmed();
				if ( !gs.sky.on )
					probeNotes += QStringLiteral( "  gi sky: none (the view is not weather-lit; Scene mode Lookdev lights it with the weather)\n" );
			}
			/* lane GPURELIGHT1: WW_CELL_GI_GPU=1 records the relight's operators (the lights that start off too, each
			 * with its FARVIEW1b group key), then relights from them on the CPU and on the GPU, doors open and every
			 * door closed, and states the timings; WW_CELL_GI_RECORDS=<folder> writes the shared light record */
			const bool giGpu = wwCellGiGpuOn();
			/* lane FARVIEW1: WW_CELL_FARLIGHT=<folder> records the operators too, writes the light record and the far light's
			 * layers there (src/farlight.h), and publishes the far tables for the Far light row */
			const QString farDir = QString::fromLocal8Bit( wwEnv( "WW_CELL_FARLIGHT" ) );
			const bool giRec = giGpu || !farDir.isEmpty() || wwDoorLiveWantsGi();
			bool farBaked = false;
			ProbeRelightOps relOps;
			QStringList relPlugins;
			if ( giRec ) {
				for ( const QString & pl : world.pluginList().split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) )
					relPlugins << QFileInfo( pl.trimmed() ).fileName();
				QHash<quint32, const EsmRefr *> refOf;
				for ( const EsmRefr & r : lightRefs )
					refOf.insert( r.formID, &r );
				// FARVIEW1b 3.2: ALWAYS 0 | PARENT 1 (root, parity) | SELF 2 (a light switched itself)
				auto keyOf = [&]( quint32 ref ) -> quint64 {
					const auto it = refOf.constFind( ref );
					if ( !ref || it == refOf.constEnd() )
						return 0;
					const EsmRefr & r = **it;
					auto pack = []( quint64 kind, quint32 form, bool parity ) {
						return ( kind << 62 ) | ( quint64( parity ? 1 : 0 ) << 40 ) | ( quint64( ( form >> 24 ) & 0xFFFFu ) << 24 )
							| quint64( form & 0xFFFFFFu );
					};
#ifdef ESM_HAS_CELL_FIELDS
					if ( r.enableParent ) {
						bool parity = r.enableParentOpposite;
						quint32 root = r.enableParent;
						for ( int hop = 0; hop < 16; hop++ ) {
							const auto f = enableFacts.constFind( root );
							if ( f == enableFacts.constEnd() || !f->parent )
								break;
							parity ^= f->opposite;
							root = f->parent;
						}
						return pack( 1, root, parity );
					}
#endif
					const EsmLight & b = world.light( r.base );
					if ( r.initiallyDisabled || ( b.exists && ( b.flags & 0x20 ) ) )
						return pack( 2, ref, false );
					return 0;
				};
				gs.record = &relOps;
				gs.recordExtra = L->offLights;
				for ( const QVector<WwCellLight> * v : { &L->lights, &L->offLights } )
					for ( const WwCellLight & l : *v ) {
						gs.recordRef.push_back( l.ref );
						gs.recordGroup.push_back( keyOf( l.ref ) );
					}
			}
			// lane AODECAL1: the copies' volumes (read or baked), so the relight also builds the copy-free grid
			std::shared_ptr<const AoDecalSet> aoSet;
			if ( aoBuild.on() ) {
				aoSet = aoBuild.finish( dataRoot );
				probeNotes += aoBuild.census();
				if ( aoSet && !aoSet->copies.empty() )
					gs.aoDecals = aoSet;
			}
			if ( wwEnvInt( "WW_CELL_GI_STATICCACHE_WARM" ) == 1 ) {
				/* lane STATICCACHE1's gate: one relight first under another sun (turned 90 degrees, half as bright), so
				 * the relight below is a second hour's: it takes the placed lights from the cache */
				ProbeGiSpec gw = gs;
				const float x = gw.sky.sunTo[0], y = gw.sky.sunTo[1];
				gw.sky.sunTo[0] = -y;
				gw.sky.sunTo[1] = x;
				for ( float & c : gw.sky.sun )
					c *= 0.5f;
				ProbeGiResult grw;
				probeGiRelight( probeSoup, giBakeDir, *L, gw, &grw );
			}
			ProbeGiResult gr;
			const bool ok = probeGiRelight( probeSoup, giBakeDir, *L, gs, &gr );
			probeNotes += QStringLiteral( "  %1\n" ).arg( probeGiCensusText( gr ) );
			if ( giGpu && ok && relOps.built ) {
				probeNotes += QStringLiteral( "  %1\n" ).arg( probeRelightCensusText( relOps ) );
				ProbeRelightState open, shut;
				shut.doorClosed.assign( relOps.doorRefs.size(), 1 );
				open.share = shut.share = gs.share;   // lane GPURELIGHT1: the timings with Probe sharing as drawn
				ProbeRelightOut co, cs, go, gsh;
				QString why;
				const bool cok = probeRelightCpu( relOps, open, &co, &why ) && probeRelightCpu( relOps, shut, &cs, &why );
				ProbeRelightGpu gpu;
				const bool gok = cok && gpu.init( &why ) && gpu.upload( relOps, &why ) && gpu.run( open, &go, &why )
					&& gpu.run( shut, &gsh, &why ) && gpu.run( open, &go, &why ) && gpu.run( shut, &gsh, &why );   // warm, then timed
				auto rel = []( const std::vector<float> & a, const std::vector<float> & b ) {
					double m = 0, d = 0;
					for ( size_t i = 0; i < a.size() && i < b.size(); i++ ) {
						m = std::max( m, std::fabs( double( b[i] ) ) );
						d = std::max( d, std::fabs( double( a[i] ) - double( b[i] ) ) );
					}
					return m > 0 ? d / m : d;
				};
				auto sum = []( const std::vector<float> & a ) {
					double s = 0;
					for ( float v : a )
						s += v;
					return s;
				};
				if ( !cok || !gok )
					probeNotes += QStringLiteral( "  gi relight FAILED: %1\n" ).arg( why );
				else
					probeNotes += QStringLiteral( "  gi relight (GPURELIGHT1, %1): doors open CPU %2 ms %3 passes, GPU %4 ms %5 passes "
						"(gather %6 ms; upload once %7 ms), GPU vs CPU B %8; all %9 doors closed CPU %10 ms, GPU %11 ms, GPU vs CPU B %12, "
						"slots emptied %13, sum B closed / open %14\n" )
						.arg( gpu.renderer() ).arg( co.ms, 0, 'f', 1 ).arg( co.passes ).arg( go.ms, 0, 'f', 1 ).arg( go.passes )
						.arg( go.msKernel[1], 0, 'f', 1 ).arg( gpu.msUpload(), 0, 'f', 1 ).arg( rel( go.B, co.B ), 0, 'g', 3 )
						.arg( relOps.doorRefs.size() ).arg( cs.ms, 0, 'f', 1 ).arg( gsh.ms, 0, 'f', 1 ).arg( rel( gsh.B, cs.B ), 0, 'g', 3 )
						.arg( cs.slotsEmptied ).arg( sum( cs.B ) / std::max( sum( co.B ), 1e-30 ), 0, 'f', 4 );
				if ( cok && gok && gs.share )
					probeNotes += QStringLiteral( "  gi relight share (GPURELIGHT1): GPU %1 ms, CPU %2 ms, mean nu %3, E moved %4 %, GPU vs CPU E %5\n" )
						.arg( go.msShare, 0, 'f', 2 ).arg( co.msShare, 0, 'f', 1 ).arg( co.shareNuMean, 0, 'f', 4 )
						.arg( 100.0 * co.shareMoved, 0, 'f', 3 ).arg( rel( go.E, co.E ), 0, 'g', 3 );
				const QString recDir = QString::fromLocal8Bit( wwEnv( "WW_CELL_GI_RECORDS" ) );
				if ( !recDir.isEmpty() ) {
					QString rerr, rcensus;
					probeNotes += probeRelightWriteRecords( relOps, recDir, relPlugins, &rerr, &rcensus )
						? QStringLiteral( "  %1\n" ).arg( rcensus ) : QStringLiteral( "  gi records FAILED: %1\n" ).arg( rerr );
				}
			}
			cellFarLive() = CellFarLive();	// lane LIGHTPHYS4: a new cell's far light replaces the kept one
			if ( !farDir.isEmpty() && ok && relOps.built ) {	// lane FARVIEW1: the dots, the light record, the layers
				relOps.interior = spec.interior;
				QString fc, ferr;
				farLightDots( probeSoup, relOps, 48.0f, &fc );
				probeNotes += QStringLiteral( "  %1\n" ).arg( fc );
				QDir().mkpath( farDir );
				QString rc;
				if ( !probeRelightWriteRecords( relOps, farDir, relPlugins, &ferr, &rc ) ) {
					probeNotes += QStringLiteral( "  far light record FAILED: %1\n" ).arg( ferr );
				} else {
					probeNotes += QStringLiteral( "  %1\n" ).arg( rc );
					FarLightBakeSpec fs;
					fs.red = QString::fromLatin1( wwEnv( "WW_CELL_FARLIGHT_RED" ) ).trimmed();
					fs.dump = !wwEnv( "WW_CELL_FARLIGHT_DUMP" ).isEmpty();
					fs.lightPhys = gs.lightPhys;	// lane LIGHTPHYS4: the far light bounces on the near bounce's curve
					FarLightBakeOut fo;
					farBaked = farLightBake( relOps, farDir, fs, &fo, &ferr );
					if ( farBaked ) {
						CellFarLive & fl = cellFarLive();
						fl.nif = nif;
						fl.ops.reset( new ProbeRelightOps( relOps ) );
						fl.dir = farDir;
						fl.fs = fs;
					}
					probeNotes += farBaked ? QStringLiteral( "  %1\n" ).arg( fo.census ) : QStringLiteral( "  far light bake FAILED: %1\n" ).arg( ferr );
				}
			}
			if ( ok ) {
				WwCellGi gi;
				for ( int k = 0; k < 3; k++ ) {
					gi.origin[k] = gr.origin[k];
					gi.dims[k] = gr.dims[k];
				}
				gi.voxel = gr.voxel;
				gi.pad = gr.pad;
				gi.summary = QStringLiteral( "grid %1x%2x%3 voxel %4" ).arg( gr.dims[0] ).arg( gr.dims[1] ).arg( gr.dims[2] )
					.arg( double( gr.voxel ), 0, 'f', 1 );
				const QString dump = QString::fromLocal8Bit( wwEnv( "WW_CELL_GI_DUMP" ) );
				QString derr;
				if ( !dump.isEmpty() && !probeGiDump( gr, gs, dump, &derr ) )
					probeNotes += QStringLiteral( "  gi dump FAILED: %1\n" ).arg( derr );
				if ( !dump.isEmpty() && !probeGiDumpLights( *L, gs, dump, &derr ) )	// lane DECK1
					probeNotes += QStringLiteral( "  gi lights dump FAILED: %1\n" ).arg( derr );
				if ( !gr.aoGate.isEmpty() )
					probeNotes += QStringLiteral( "  %1\n" ).arg( gr.aoGate );
				probeGiAoFreeSwap( gr );	// lane AODECAL1: the decals darken; the grid no longer does it twice
				gi.rgba = std::move( gr.grid );
				// lane SKY1; lane SKYINT1: an interior keeps its own ambient, so its grid never stands in for the weather's
				gi.skyLit = gr.skyLit && !spec.interior && gs.skyRed != QLatin1String( "keepamb" );
				gi.sky = std::move( gr.gridSky );	// lane PROBEVIEW1: the Pass drop-down's Sky visibility
				if ( volGiOn ) {   // lane VOLFIT1: the Space rooms as the GI's volumes (the room grid now carries their ids)
					const ProbeVolumesGi vg = probeVolumesToGi( *volGi, gr.rooms, 0.2f, QString::fromLatin1( wwEnv( "WW_CELL_VOLUMES_GI_RED" ) ) );
					probeNotes += QStringLiteral( "  %1\n" ).arg( vg.census );
					for ( const QString & l : vg.log )
						probeNotes += QStringLiteral( "  space to GI: %1\n" ).arg( l );
					if ( !wwVolumesIntoGi( gr, vg, 0 ) )
						probeNotes += QStringLiteral( "  space to GI: this build's GI takes no volume set (lane PERF1 not merged); not fed\n" );
					else if ( vg.shapes.empty() )
						probeNotes += QStringLiteral( "  space to GI: no room mapped; the GI keeps its stand-in\n" );
				}
				probeGiRoomsInto( gr, gi );   // lane ROOMCLAMP1: the second slots and the rooms
				wwCellGiPublish( nif, gi );
				if ( wwDoorLiveWantsGi() ) {   // lane DOORLIVE1: the live door's GI follows from these operators
					if ( relOps.built ) {
						std::vector<WwDoorLiveBox> boxes;
						for ( const ProbeSoup::Door & d : probeSoup.doors ) {
							WwDoorLiveBox bx;
							bx.ref = d.ref;
							for ( int k = 0; k < 3; k++ ) {
								bx.lo[k] = d.lo[k];
								bx.hi[k] = d.hi[k];
							}
							boxes.push_back( bx );
						}
						probeNotes += wwDoorLiveKeepGi( nif, relOps, gi, boxes );
					} else {
						probeNotes += QStringLiteral( "  door live: no relight operators recorded; the GI does not follow\n" );
					}
				}
				wwCellAoDecalPublish( nif, gs.aoDecals );	// lane AODECAL1 (null: no decals)
				if ( farBaked ) {	// lane FARVIEW1: the far tables for the group state, and the GI's placed share
					// the GI grid relit by the placed lights alone, at the grid's own pass count: no sky, sun, directional or glow
					ProbeGiSpec gp = gs;
					gp.record = nullptr;
					gp.recordExtra.clear();
					gp.recordRef.clear();
					gp.recordGroup.clear();
					gp.sky = ProbeSkyLight();
					gp.skyRed.clear();
					gp.interiorSky = gp.interiorSun = false;
					gp.noGlow = true;
					gp.passes = gr.passes;
					WwCellLighting lp = *L;
					lp.hasDirectional = false;
					probeNotes += cellFarLightPublish( nif, farDir, probeSoup, giBakeDir, lp, gp );
					cellFarLive().gp = gp;	// lane LIGHTPHYS4
					cellFarLive().lp = lp;
				}
				// a later change of weather (sky-lit cells) or of Probe sharing (every cell, lane GPURELIGHT1) relights it; lane LIGHTS5: so does the emittance row (gs.emitBaked); lane LIGHTPHYS2: and the light rows
				probeSkyKeep( nif, probeSoup, giBakeDir, gs, !spec.interior || gs.interiorSky );
				PassTime::memLog( "GI relit" );	// lane PERF1
				WwCellProbeView pv;	// lane PROBEVIEW1: the surfel and probe previews
				pv.surfelCell = gr.surfelCell;
				pv.probesShown = spec.probesShow;
				pv.surfels = std::move( gr.surfelOut );
				pv.surfelBrick = std::move( gr.surfelBrick );	// lane BRICK1
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
			  << ( cf != ic.flags ? QStringLiteral( " (RED %1)" ).arg( QString::fromLatin1( wwEnv( "WW_CELL_SKYINT_RED" ) ) ) : QString() )
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
		if ( !boothRefs.isEmpty() ) {	// lane LIGHTBOOTH1: what the booth kept, and every asked id it never met
			QStringList kept, missing;
			for ( quint32 id : std::as_const( boothRefs ) )
				( boothSeen.contains( id ) ? kept : missing ).append( QStringLiteral( "%1" ).arg( id, 8, 16, QLatin1Char( '0' ) ) );
			kept.sort();
			missing.sort();
			s << "  booth: kept " << kept.size() << " of " << boothRefs.size() << " asked (" << kept.join( QLatin1Char( ',' ) )
			  << "), missing " << missing.size() << ( missing.isEmpty() ? QString() : QStringLiteral( " (%1)" ).arg( missing.join( QLatin1Char( ',' ) ) ) )
			  << ", dropped " << boothDropped << " other references\n";
		}
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
		s << weapons.censusLine();   // lane MISS1 (GAPLOOK2)
		actors.dump( QString::fromLocal8Bit( wwEnv( "WW_CELL_ACTOR_DUMP" ) ) );
		s << "  buckets over 65,536 vertices " << bucketsWide << " (largest " << qulonglong( bucketMaxVerts )
		  << "), shapes drawn with their own vertex colors " << shapesVertexColor
		  << ", placements drawn with a material swap " << placementsSwapped
		  << ", shapes repainted by a CNAM " << shapesRepainted
		  << ", CNAMs on a material with no palette " << cnamNoPalette
		  << ", shapes painted by their base's MODC " << modcShapes
		  << ", sky cards hidden " << skyCardsHidden << "\n";
		s << "  " << cableLine << "\n";   // lane CABLES1
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
		  << refractBuckets << " refraction buckets (lane EFX1), "
		  << decalFlagBuckets << " lighting buckets with the model's decal flag from " << decalFlagShapes << " shapes"
		  << ( fxdNoFlag ? " RED noflag" : "" ) << " (lane GAPFIX1)";
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
		  << " placed models take their four placed lights"
		  << ( spec.interior ? "" : fxLitOn ? " (exterior: the sun is the base)" : " (exterior off: RED=indoors)" ) << "\n";
		// lane LIGHTS2 (XEMI1)
		s << "  placement emittance: " << ( xemiOn ? "on" : "off" ) << ", " << xemiRefs << " placements carry XEMI ("
		  << xemiLigh << " a light, " << xemiRegn << " a region (region emittance " << ( xemiRegnOn ? "on" : "off" )
		  << "), " << xemiOther << " other: not drawn); " << xemiShapesLit << " lighting + "
		  << xemiShapesFx << " unlit effect shapes take it, " << xemiNoFlag << " shapes lack External Emittance, "
		  << xemiLitFx << " lit effect shapes skip it (the game's lit effect never reads it)\n";
		if ( groundNote.isEmpty() ) {   // lane CELLVIEW2
			s << "  ground: " << landsDrawn << " LAND cells, vertex colour only"
			  << " (the splat layers are NOT sampled -- that is the terrain bake's compositor)\n";
		} else {
			s << "  " << groundNote << "\n";
		}
		s << "  water: " << waterCells << " cells\n";
		// lane WATER1: the surfaces the game's water terms draw, and the placed water meshes among them
		s << "  " << wwCellWaterEcho( nif ) << "; placed water shapes " << placedWaterShapes << "\n";
		s << "  " << wwCellWaterInfoEcho( nif ) << "\n";   // lane WATERDATA1
		s << "  " << wwCellGrassEcho( nif ) << "\n";   // lane GRASSMB1
		/* lane WATER1: the reader gate's records -- WW_CELL_WATER_DUMP_FORMS=<hex,hex,...> describes each into
		 * WW_CELL_WATER_DUMP + ".forms" (a form that is no WATR: "WATR <form> NONE") */
		const QString dumpForms = wwEnvStr( "WW_CELL_WATER_DUMP_FORMS" );
		if ( !dumpForms.isEmpty() && wwEnvIsSet( "WW_CELL_WATER_DUMP" ) ) {
			QFile ff( wwEnvStr( "WW_CELL_WATER_DUMP" ) + QStringLiteral( ".forms" ) );
			if ( ff.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
				QTextStream fo( &ff );
				for ( const QString & h : dumpForms.split( ',', Qt::SkipEmptyParts ) ) {
					const quint32 form = h.trimmed().toUInt( nullptr, 16 );
					WwWaterRecord r;
					if ( world.waterRecord( form, r ) )
						fo << wwWaterDescribe( r ) << "\n";
					else
						fo << "WATR " << QString::number( form, 16 ).rightJustified( 8, QLatin1Char( '0' ) ) << " NONE\n";
				}
			}
		}
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
		s << farLodNotes;  // lane FARLOD1, empty unless the Far LOD row
		s << modPickNote;  // lane MODPICK1, exteriors
		s << spawnNote;    // lane CELLSPAWN1
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
	const QByteArray dump = wwEnv( "WW_CELL_DUMP" );
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
	const QByteArray lightDump = wwEnv( "WW_CELL_LIGHTS" );
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
	const QByteArray refdump = wwEnv( "WW_CELL_REFDUMP" );
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

bool nifCreateCellScene( NifModel * nif, const CellSceneSpec & spec, QString * error, QString * notes )
{
	return cellBuild( nif, spec, nullptr, error, notes );
}

// lane CELLSTREAM1: one streamed cell, on the streamer's worker thread
void cellChunkBuild( CellChunk & c )
{
	CellSceneSpec spec = c.seed->spec;
	spec.cx = c.cx;
	spec.cy = c.cy;
	spec.n = 1;
	QString notes;
	QElapsedTimer t;
	t.start();
	c.ok = false;
	if ( !cellBuild( nullptr, spec, &c, &c.error, &notes ) )
		c.ok = false;
	if ( c.ok && c.buckets )   // the shapes' vertex split here, off the drawing thread
		for ( Bucket & b : c.buckets->buckets ) {
			b.nVerts = b.verts.size();
			b.nTris = b.tris.size();
			if ( b.verts.empty() || b.tris.empty() )
				continue;
			splitBucket( b, b.parts );
			std::vector<OutVert>().swap( b.verts );
			std::vector<BucketTri>().swap( b.tris );
		}
	c.buildMs = t.elapsed();
}

/* lane LIVESET3 (STREAMPSYS1): a streamed cell's particle systems, copied as the open copies its own (the particle
 * step in cellBuild): one "particles" node under the cell and WW_CELL_PARTICLES_MAX over the whole stream. The
 * effect-light table (FXLIT1) stays as an exterior open leaves it: off outdoors. */
static void cellChunkParticles( NifModel * nif, CellChunk & c, CellBlockSource & src, const QModelIndex & iCell )
{
	c.pfxDone = true;
	QHash<QString, std::shared_ptr<NifModel>> * cache = src.pfxModels();
	if ( c.pfx.empty() || !cache || !wwCellParticlesOn() )
		return;
	QVector<qint32> written;	// lane LIVESET4: taken from the stream's pool (it records them for the cell)
	QPersistentModelIndex iPfx;
	QString why;
	for ( const CellChunk::Pfx & p : c.pfx ) {
		if ( c.pfxCopies >= src.pfxRoom() )
			break;
		const QString key = p.model.toLower();
		auto it = cache->find( key );
		if ( it == cache->end() ) {
			auto m = std::make_shared<NifModel>();
			QByteArray bytes;
			QBuffer dev( &bytes );
			bool good = lodgenReadModelBytes( c.seed->dataRoot, p.model, bytes ) && dev.open( QIODevice::ReadOnly )
				&& m->load( dev, p.model.toLocal8Bit().constData() );
			if ( good ) {
				m->resetState();
				good = m->getVersionNumber() == nif->getVersionNumber() && m->getBSVersion() == nif->getBSVersion();
			}
			if ( !good )
				m.reset();
			it = cache->insert( key, m );
		}
		if ( !it.value() )
			continue;
		if ( !iPfx.isValid() ) {
			QModelIndex iP = src.take( nif, QStringLiteral( "NiNode" ) );
			if ( !iP.isValid() )
				break;
			written.append( nif->getBlockNumber( iP ) );
			nif->set<QString>( iP, "Name", QStringLiteral( "particles" ) );
			nif->set<quint32>( iP, "Flags", 14 );
			nif->set<float>( iP, "Scale", 1.0f );
			addLink( nif, iCell, QStringLiteral( "Children" ), nif->getBlockNumber( iP ) );
			iPfx = iP;
		}
		if ( cellCopyParticleBranch( nif, QModelIndex( iPfx ), *it.value(), p.at, p.rot, p.scale, &why, nullptr, &src, &written ) > 0 )
			c.pfxCopies++;
	}
	for ( const qint32 b : written )
		nif->updateBlockLinks( b );
}

// lane CELLSTREAM1: the cell's buckets written under its node, as the open writes them, a slice at a time
bool cellChunkEmit( NifModel * nif, CellChunk & c, CellBlockSource & src, const QModelIndex & iCell,
	double budgetMs, QString * error )
{
	if ( !c.buckets )
		return true;
	QElapsedTimer t;
	t.start();
	g_cellBlockSource = &src;
	g_cellChunkEmit = &c;
	CellChunkBuckets & B = *c.buckets;
	const Vector3 origin = c.seed->origin;
	bool ok = true;
	while ( ok && c.next < B.keys.size() ) {
		const QString k = B.keys.at( c.next++ );
		auto it = B.buckets.find( k );
		if ( it == B.buckets.end() )
			continue;
		const Bucket & bk = it.value();
		QModelIndex iParent = iCell;
		if ( k.startsWith( QLatin1String( "\x01land" ) ) ) {
			if ( !c.iGround.isValid() ) {
				QModelIndex iG = src.take( nif, QStringLiteral( "BSOrderedNode" ) );
				nif->set<QString>( iG, "Name", QStringLiteral( "ground" ) );
				nif->set<quint32>( iG, "Flags", 14 );
				nif->set<float>( iG, "Scale", 1.0f );
				addLink( nif, iCell, QStringLiteral( "Children" ), nif->getBlockNumber( iG ) );
				c.iGround = iG;
			}
			iParent = c.iGround;
		}
		if ( bk.billboard ) {
			QModelIndex iB = src.take( nif, QStringLiteral( "NiBillboardNode" ) );
			nif->set<QString>( iB, "Name", QStringLiteral( "billboard " ) + bk.name );
			nif->set<quint32>( iB, "Flags", 14 );
			nif->set<Vector3>( iB, "Translation", bk.bbPos );
			nif->set<float>( iB, "Scale", bk.bbScale );
			nif->set<int>( iB, "Billboard Mode", bk.bbMode );
			addLink( nif, iCell, QStringLiteral( "Children" ), nif->getBlockNumber( iB ) );
			ok = emitBucket( nif, iB, bk, Vector3( 0.0f, 0.0f, 0.0f ), c.shapes, c.verts, c.tris, error );
		} else {
			ok = emitBucket( nif, iParent, bk, origin, c.shapes, c.verts, c.tris, error );
		}
		if ( double( t.nsecsElapsed() ) > ( budgetMs + 3.0 ) * 1.0e6 )   // WW_CELL_STREAM_TRACE: what overran
			cellStreamTrace( QStringLiteral( "slow bucket %1: %2 ms, %3 tris %4 verts%5%6 (remap %7 take %8 runs %9 shape %10 mesh %11 mat %12)" ).arg( bk.name )
				.arg( double( t.nsecsElapsed() ) * 1.0e-6, 0, 'f', 1 ).arg( bk.nTris ).arg( bk.nVerts )
				.arg( bk.billboard ? " billboard" : "" ).arg( bk.effectBlock.isEmpty() ? "" : " effect" )
				.arg( g_emitSecMs[0], 0, 'f', 1 ).arg( g_emitSecMs[1], 0, 'f', 1 ).arg( g_emitSecMs[2], 0, 'f', 1 )
				.arg( g_emitSecMs[3], 0, 'f', 1 ).arg( g_emitSecMs[4], 0, 'f', 1 ).arg( g_emitSecMs[5], 0, 'f', 1 ) );
		B.buckets.erase( it );   // its vertices are in the side arrays now
		if ( double( t.nsecsElapsed() ) * 1.0e-6 + src.wireOwedMs() > budgetMs )
			break;
	}
	g_cellBlockSource = nullptr;
	g_cellChunkEmit = nullptr;
	if ( !ok ) {
		c.ok = false;
		return true;
	}
	if ( c.next >= B.keys.size() ) {
		if ( !c.pfxDone )
			cellChunkParticles( nif, c, src, iCell );
		c.buckets.reset();
		return true;
	}
	return false;
}

// lane CELLSPAWN1: the marker test for src/cellspawn.cpp (isMarkerModel lives in this file's anonymous namespace)
bool cellIsMarkerModel( const QString & model )
{
	return isMarkerModel( model );
}

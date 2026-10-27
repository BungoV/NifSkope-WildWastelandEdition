/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "gl/impostordraw.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store

#include "gamemanager.h"
#include "gl/celllights.h"
#include "gl/cellhdr.h"
#include "gl/cellpasstime.h"
#include "gl/cellunits.h"
#include "gl/lookdevstage.h"
#include "gl/glscene.h"
#include "gl/gltex.h"
#include "gl/renderer.h"
#include "impostorcard.h"
#include "model/nifmodel.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QOpenGLContext>
#include <QTextStream>

#include <gli.hpp>

#include <cmath>

/* WW_IMPOSTOR_TRACE -- a bracket trace for THIS draw path, written with the
 * file closed again at every step so a segfault cannot lose the last line.
 *
 * It exists because the shipped exe is stripped (`nm` reports "no symbols"), so
 * a gdb backtrace of a crash in here is a column of `?? ()` and says nothing.
 * Off by default and costing one already-read environment flag per call; it
 * writes beside the application, never into the scene. Delete it only once the
 * path it brackets has a gate that would fail without it. */
namespace {
void wwImpostorTrace( const char * where )
{
	WW_SWITCH_STATIC( bool, on, wwEnvIsSet( "WW_IMPOSTOR_TRACE" ) );
	if ( !on )
		return;
	QFile f( QCoreApplication::applicationDirPath() + QStringLiteral( "/ww_impostor_trace.log" ) );
	if ( f.open( QIODevice::Append | QIODevice::Text ) ) {
		QTextStream( &f ) << where << "\n";
		f.close();
	}
}
}

/* ---------------------------------------------------------------------------
 * SPACES, once, so nothing below has to guess.
 *
 * The card's numbers (`center`, `half`) come out of the `.lodm` in the base
 * object's own model space. This preview draws the card in the scene's WORLD
 * space with `worldOffset` added, and hands the shader `scene->view` as
 * `modelViewMatrix`, exactly the way every other shape in this renderer is
 * drawn (`glshape.cpp:659`). So "model space" in the two shaders means the
 * scene's world space, and a card opened on its own sits at the origin.
 *
 * The camera's position in that space is what the frame choice needs, and
 * `scene->view` is world -> view. A Transform is R*s*p + t with R orthonormal,
 * so the eye (view-space origin) is at `R^T * (-t) / s` in world space.
 * ------------------------------------------------------------------------- */

namespace {

//! The unit quad the card is drawn as: -1..1 in x and y, z unused. The vertex
//! stage turns it into the card's rectangle; it is never scaled here.
const float kQuadPos[12] = {
	-1.0f, -1.0f, 0.0f,
	 1.0f, -1.0f, 0.0f,
	-1.0f,  1.0f, 0.0f,
	 1.0f,  1.0f, 0.0f
};
const float kQuadUv[8] = {
	0.0f, 1.0f,
	1.0f, 1.0f,
	0.0f, 0.0f,
	1.0f, 0.0f
};
const std::uint16_t kQuadIdx[6] = { 0, 1, 2, 2, 1, 3 };

/* attrMask: one nibble per attribute LOCATION, the component count.
 * location 0 = vertexPosition, 3 floats  -> nibble 0 = 3
 * location 7 = multiTexCoord0, 2 floats  -> nibble 7 = 2 -> 2 << 28 */
const std::uint64_t kQuadAttrMask = 3ULL | ( 2ULL << 28 );

//! The camera's position in the scene's world space.
Vector3 eyeWorld( const Scene * scene )
{
	const Transform & v = scene->view;
	Vector3 t = v.translation;
	Vector3 e = v.rotation.inverted() * Vector3( -t[0], -t[1], -t[2] );
	if ( v.scale != 0.0f )
		e = e / v.scale;
	return e;
}

/*! ONE LAYER OF A `cardArray`'s SHEETS AS A 2D TEXTURE (lane FARLOD2).
 *
 *  A cardArray's sheets are DX10 texture arrays (`legacy.256x1024` holds 8
 *  layers), and the texture cache cannot serve one: `GLI_create_texture`
 *  handles 2D and cube targets only, and the sheet's game path
 *  ("data\FO4CSLOD\...") is not a `textures/` name the stack can answer. So the
 *  far LOD's atlas-served cards loaded and then drew 0 -- every card refused at
 *  "the colour sheet did not bind". The shaders sample `sampler2D`, so the
 *  layer the set names is uploaded on its own as a 2D texture with its whole
 *  stored mip chain (the shader's `cardMipCap` still caps it): the same texels
 *  the game samples from that layer, nothing resampled.
 *
 *  Cached per GL context, file and layer -- a refusal too, so 19,705 cards a
 *  frame do not re-read a file that failed once. */
GLuint arrayLayerTexture( Scene * scene, const QString & file, int layer, QString * err )
{
	struct Entry { GLuint id = 0; QString err; };
	static QHash< QString, Entry > cache;
	const QString key = QStringLiteral( "%1|%2|%3" )
			.arg( quintptr( QOpenGLContext::currentContext() ) ).arg( file.toLower() ).arg( layer );
	const auto it = cache.constFind( key );
	if ( it != cache.constEnd() ) {
		if ( err )
			*err = it->err;
		return it->id;
	}
	Entry e;
	do {
		QFile f( file );
		if ( !f.open( QIODevice::ReadOnly ) ) {
			e.err = QStringLiteral( "cannot open %1" ).arg( file );
			break;
		}
		const QByteArray bytes = f.readAll();
		gli::texture t = gli::load_dds( bytes.constData(), std::size_t( bytes.size() ) );
		if ( t.empty() ) {
			e.err = QStringLiteral( "%1 is not a DDS this build reads" ).arg( file );
			break;
		}
		if ( layer < 0 || std::size_t( layer ) >= t.layers() || t.faces() != 1 ) {
			e.err = QStringLiteral( "%1 has %2 layers and %3 faces, layer %4 asked for" )
					.arg( file ).arg( t.layers() ).arg( t.faces() ).arg( layer );
			break;
		}
		gli::gl glp( gli::gl::PROFILE_GL33 );
		const gli::gl::format fmt = glp.translate( t.format(), t.swizzles() );
		auto * fn = scene->renderer->fn;
		fn->glGenTextures( 1, &e.id );
		fn->glBindTexture( GL_TEXTURE_2D, e.id );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0 );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, GLint( t.levels() ) - 1 );
		const bool one = gli::component_count( t.format() ) == 1;
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, fmt.Swizzles[0] );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, one ? fmt.Swizzles[0] : fmt.Swizzles[1] );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, one ? fmt.Swizzles[0] : fmt.Swizzles[2] );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, one ? GLint( GL_ONE ) : fmt.Swizzles[3] );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		for ( std::size_t level = 0; level < t.levels(); level++ ) {
			const auto ext = t.extent( level );
			if ( gli::is_compressed( t.format() ) )
				fn->glCompressedTexImage2D( GL_TEXTURE_2D, GLint( level ), GLenum( fmt.Internal ), ext.x, ext.y, 0,
						GLsizei( t.size( level ) ), t.data( std::size_t( layer ), 0, level ) );
			else
				fn->glTexImage2D( GL_TEXTURE_2D, GLint( level ), GLint( fmt.Internal ), ext.x, ext.y, 0,
						GLenum( fmt.External ), GLenum( fmt.Type ), t.data( std::size_t( layer ), 0, level ) );
		}
	} while ( false );
	cache.insert( key, e );
	if ( err )
		*err = e.err;
	return e.id;
}

//! An array-layer sheet's texture: the file beside the .lodm, else an absolute resolved path. 0 = none.
GLuint layerSheetId( Scene * scene, const ImpostorSheet & sheet, int layer )
{
	QString file = sheet.localPath;
	if ( file.isEmpty() || !QFileInfo::exists( file ) )
		file = ( !sheet.resolved.isEmpty() && QFileInfo( sheet.resolved ).isAbsolute() ) ? sheet.resolved : QString();
	return file.isEmpty() ? 0 : arrayLayerTexture( scene, file, layer, nullptr );
}

//! Bind one sheet to `unit` and tell the program. Returns false when the
//! texture cache could not produce it, which the caller REPORTS.
//! `layer` >= 0: the sheet is a cardArray's texture array (lane FARLOD2).
bool bindSheet( Scene * scene, NifSkopeOpenGLContext::Program * prog,
				const char * uniform, const ImpostorSheet & sheet, int unit, int layer = -1 );

/* lane PERF1: the card pass's state, bound once (ImpostorDraw::batchBegin). A street frame drew ~5,000
 * cards at ~40 us each, mostly re-binding the same program, uniforms and sheets (four file checks and a
 * key string per card for an array-layer sheet). */
struct CardBatch
{
	bool on = false;
	NifSkopeOpenGLContext::Program * prog = nullptr;	// bound, with the pass-wide uniforms and state set
	const char * progName = nullptr;
	const ImpostorCardSet * set = nullptr;				// whose sheets are bound now
	bool haveColour = false, haveNormal = false;
	Vector3 eye;
	QHash< const ImpostorSheet *, GLuint > layerIds;	// an array-layer sheet's texture, resolved once
};

CardBatch & cardBatch()
{
	static CardBatch b;
	return b;
}

bool batchRedPerDraw()
{
	WW_SWITCH_STATIC( bool, v, wwEnv( "WW_IMPOSTOR_RED" ) == "perdraw" );
	return v;
}

bool bindSheet( Scene * scene, NifSkopeOpenGLContext::Program * prog,
				const char * uniform, const ImpostorSheet & sheet, int unit, int layer )
{
	const GLint loc = prog->uniLocation( uniform );
	if ( loc < 0 )
		return false;
	scene->renderer->fn->glActiveTexture( GL_TEXTURE0 + GLenum( unit ) );

	if ( layer >= 0 ) {
		CardBatch & b = cardBatch();
		if ( b.on ) {
			const auto it = b.layerIds.constFind( &sheet );
			if ( it != b.layerIds.constEnd() ) {
				if ( !*it )
					return false;
				scene->renderer->fn->glBindTexture( GL_TEXTURE_2D, *it );
				scene->renderer->fn->glUniform1i( loc, unit );
				return true;
			}
		}
		const GLuint id = layerSheetId( scene, sheet, layer );
		if ( b.on )
			b.layerIds.insert( &sheet, id );
		if ( !id )
			return false;
		scene->renderer->fn->glBindTexture( GL_TEXTURE_2D, id );
		scene->renderer->fn->glUniform1i( loc, unit );
		return true;
	}

	/* THE TEXTURE CACHE HAS NO ABSOLUTE-PATH ROUTE, so an absolute `resolved`
	 * is not merely second choice -- it CANNOT succeed and is not asked first.
	 * `TexCache::find` hands the name to `GameManager::get_full_path`
	 * (src/gamemanager.cpp:608), which lowercases it, forces a `textures/`
	 * prefix and then asks the resource stack, so `E:\...\x_oct_d.DDS` becomes
	 * `textures/e:/.../x_oct_d.dds`: measured 2026-09-19, four of those in the
	 * warning stream of a run whose sheets all bound. The set's own GAME PATH is
	 * the name the stack can answer once `registerLooseSheets` has put the
	 * bake's tree on the resource list.
	 *
	 * Order, not exclusion: a `resolved` that is RELATIVE is a game-relative
	 * name the stack can answer, and it is still tried first because it is the
	 * more specific of the two. Both are always tried; only the order moves. */
	QStringList order;
	const bool resolvedIsAbsolute = !sheet.resolved.isEmpty()
			&& QFileInfo( sheet.resolved ).isAbsolute();
	if ( !sheet.resolved.isEmpty() && !resolvedIsAbsolute )
		order << sheet.resolved;
	if ( !sheet.gamePath.isEmpty() )
		order << sheet.gamePath;
	if ( !sheet.resolved.isEmpty() && resolvedIsAbsolute )
		order << sheet.resolved;

	int mips = 0;
	for ( const QString & name : order ) {
		if ( mips > 0 )
			break;
		mips = scene->bindTexture( QStringView( name ), true );
	}
	scene->renderer->fn->glUniform1i( loc, unit );
	return mips > 0;
}

/*! WHICH CONVENTION THIS CARD IS READ UNDER, and the only place that decides.
 *
 *  The set's own token decides, because the token is a fact about the bytes on
 *  disk; `opt.convention` only wins when `opt.forceConvention` says so, which
 *  is the gate's red control and a person inspecting a set by hand. A drawer
 *  that took the option unconditionally would draw a repaired set backwards for
 *  anyone who had once set the environment variable. */
ImpostorOct::Convention effectiveConvention( const ImpostorCardSet & set,
												const ImpostorDraw::Options & opt )
{
	if ( opt.forceConvention )
		return opt.convention;
	return set.legacyBake() ? ImpostorOct::Convention::AsBaked
							: ImpostorOct::Convention::SpecLiteral;
}

//! `pickFramesForCamera` for the card as this scene currently sees it.
void selectFrames( const Scene * scene, const ImpostorCardSet & set,
					const Vector3 & worldOffset, const ImpostorDraw::Options & opt,
					float camDir[3], int idx[3], float w[3], const Vector3 * eyeIn = nullptr )
{
	const Vector3 centre = worldOffset + Vector3( set.center[0], set.center[1], set.center[2] )
			* ( ( opt.worldScale > 0.0f ) ? opt.worldScale : 1.0f );
	Vector3 d = ( eyeIn ? *eyeIn : cardBatch().prog ? cardBatch().eye : eyeWorld( scene ) ) - centre;
	const float len = d.length();
	if ( len > 1e-6f )
		d = d / len;
	else
		d = Vector3( 0.0f, 0.0f, 1.0f );

	camDir[0] = d[0]; camDir[1] = d[1]; camDir[2] = d[2];

	if ( set.ring() ) {
		/* THE HORIZON RING (lane CARDFIX1 step 5, IMPOSTORRING1). Frame v was
		 * photographed from azimuth 360*v/V at elevation 0, so the two frames that
		 * bracket the camera's azimuth are the neighbours, and each weighs by how
		 * near it is: t = the fraction of a step past the lower one. Elevation
		 * picks nothing -- a ring has no top frames, and a view from above reads
		 * the horizon frames as they are (the error that costs is measured, not
		 * hidden). The stronger frame goes first, which is the one the crisp end
		 * draws alone. Slot 2 repeats it at weight 0: a ring blends TWO. */
		const int V = set.views;
		const float twoPi = 6.28318530718f;
		float phi = std::atan2( d[1], d[0] );
		if ( phi < 0.0f )
			phi += twoPi;
		const float f = phi / twoPi * float( V );
		const float fl = std::floor( f );
		const float t = f - fl;
		const int i0 = ( ( int( fl ) % V ) + V ) % V, i1 = ( i0 + 1 ) % V;
		if ( t <= 0.5f ) {
			idx[0] = i0; w[0] = 1.0f - t; idx[1] = i1; w[1] = t;
		} else {
			idx[0] = i1; w[0] = t; idx[1] = i0; w[1] = 1.0f - t;
		}
		idx[2] = idx[0]; w[2] = 0.0f;
		// THE RED CONTROL, the ring's form of the grid's quarter turn: every frame
		// becomes the view from 90 degrees round.
		if ( opt.shuffleFrames )
			for ( int k = 0; k < 3; k++ )
				idx[k] = ( idx[k] + V / 4 ) % V;
		return;
	}

	int gi[3] = { 0, 0, 0 }, gj[3] = { 0, 0, 0 };
	const ImpostorOct::Convention saved = ImpostorOct::g_convention;
	ImpostorOct::g_convention = effectiveConvention( set, opt );
	ImpostorOct::pickFramesForCamera( camDir, set.oct, idx, gi, gj, w );
	ImpostorOct::g_convention = saved;

	if ( opt.shuffleFrames ) {
		/* THE RED CONTROL. Not a random shuffle: a FIXED permutation of the
		 * grid, so a shuffled run is reproducible and two shuffled runs of the
		 * same build give the same number.
		 *
		 * A QUARTER TURN, and the quarter is measured rather than assumed.
		 * `(i,j) -> (j, N-1-i)` sends `u' = v, v' = -u`, so `(x,y) -> (-y,x)`:
		 * every frame becomes the view from 90 degrees round. The mirror this
		 * replaced -- `(N-1-i, N-1-j)`, the view from the OPPOSITE side -- reads
		 * as the larger error and is not: a Commonwealth tree seen from behind
		 * has very nearly the silhouette it has from the front, and on
		 * TreeMapleblasted05 the mirrored control scored 0.1770 against the
		 * honest run's 0.1763 (2026-09-19). A control that cannot lose is not a
		 * control. At 90 degrees the silhouette is a different shape, which is
		 * the largest wrong answer a SILHOUETTE can actually see.
		 *
		 * The 180-degree question is not abandoned, it is asked by the
		 * instrument that can hear the answer: the azimuth row photographs the
		 * card against the mesh from the spec direction AND from the opposite
		 * one, and step 8 runs the old convention on purpose to show that row
		 * failing. */
		for ( int k = 0; k < 3; k++ ) {
			const int i = idx[k] % set.oct;
			const int j = idx[k] / set.oct;
			idx[k] = j + ( set.oct - 1 - i ) * set.oct;
		}
	}
}


//! One baked frame of a set: its rect on the sheet, its camera basis, its unscaled quad offset.
struct CardFrame
{
	float rect[4];
	float right[3], up[3], fwd[3];
	float offR = 0.0f, offU = 0.0f;
};

/*! Frame (i, j) of `set`. The rect comes from the ONE function that knows it (`ImpostorOct::frameRect`; row j
 *  runs downward in sheet space, the way the bake writes it). The basis: `frameDir` gives the SPEC direction of
 *  the cell, `frameBasis` the camera the bake actually used for it -- a legacy set still carries the old 180
 *  degrees. A ring's frame v was photographed from azimuth 2 pi v / V at elevation 0 under the spec law
 *  (docs/LODGEN_LODM_FORMAT.md 3a). The offset is the set's own (0, 0 for a set baked before the law). */
void frameData( const ImpostorCardSet & set, const ImpostorDraw::Options & opt, int i, int j, CardFrame & f )
{
	const float du = 1.0f / float( set.cols() );
	float u0 = 0.0f, v0 = 0.0f, dU = du, dV = du;
	if ( set.ring() ) {
		// the ring's one row: frame v at [v/V, (v+1)/V) x [0, 1)
		u0 = float( i ) * du; v0 = 0.0f; dU = du; dV = 1.0f;
	} else {
		ImpostorOct::frameRect( i, j, set.oct, &u0, &v0, &dU, &dV );
	}
	f.rect[0] = u0; f.rect[1] = v0; f.rect[2] = dU; f.rect[3] = dV;

	float dir[3];
	const ImpostorOct::Convention saved = ImpostorOct::g_convention;
	ImpostorOct::g_convention = effectiveConvention( set, opt );
	if ( set.ring() ) {
		const float p = 6.28318530718f * float( i ) / float( set.views );
		dir[0] = std::cos( p ); dir[1] = std::sin( p ); dir[2] = 0.0f;
		ImpostorOct::g_convention = ImpostorOct::Convention::SpecLiteral;
	} else {
		ImpostorOct::frameDir( i, j, set.oct, dir );
	}
	ImpostorOct::frameBasis( dir, f.right, f.up, f.fwd );
	ImpostorOct::g_convention = saved;

	set.frameOffsetOf( i, j, &f.offR, &f.offU );
}

//! What one card draws: its frames and weights, its scale and its billboard.
struct CardPick
{
	int frameCount = 1;
	int idx[3] = { 0, 0, 0 };
	float w[3] = { 0.0f, 0.0f, 0.0f };
	float sc = 1.0f;
	Vector3 centre, rightV, upV, fwdV;
};

/*! The CPU half of one card, shared by the per-card and the instanced pass so both hand the GPU the same floats.
 *  `eye` null = the batch's eye or the scene's (selectFrames). */
void pickCard( const Scene * scene, const ImpostorCardSet & set, const Vector3 & worldOffset,
				const ImpostorDraw::Options & opt, const ImpostorDraw::Resolved & rs, const Vector3 * eye, CardPick & c )
{
	float camDir[3];
	int * idx = c.idx;
	float * w = c.w;
	selectFrames( scene, set, worldOffset, opt, camDir, idx, w, eye );

	/* THE SLIDER (lane IMPOSTORDEPTH2): crisp end = FLAT SNAP, the nearest frame alone; smooth end = three
	 * frames, stipple, search 16; between = three frames with the weights sharpened. A RING set blends its two
	 * neighbouring azimuths, never three (CARDFIX1 step 5). */
	const int frameCount = set.ring() ? qMin( rs.frameCount, 2 ) : rs.frameCount;
	if ( frameCount > 1 && rs.sharpen != 1.0f ) {
		float sum = 0.0f;
		for ( int k = 0; k < 3; k++ ) {
			w[k] = std::pow( qMax( 0.0f, w[k] ), rs.sharpen );
			sum += w[k];
		}
		if ( sum > 0.0f )
			for ( int k = 0; k < 3; k++ )
				w[k] /= sum;
	}
	if ( frameCount == 1 ) {
		// No blend: the nearest frame alone, at full weight.
		int best = 0;
		for ( int k = 1; k < 3; k++ ) {
			if ( w[k] > w[best] )
				best = k;
		}
		idx[0] = idx[best];
		w[0] = 1.0f;
	}
	c.frameCount = frameCount;

	// the placement's scale multiplies the centre offset, the quad, the depth span and the frame offsets together
	const float sc = ( opt.worldScale > 0.0f ) ? opt.worldScale : 1.0f;
	c.sc = sc;

	/* The quad faces the camera and stands UP: world +Z projected into the plane perpendicular to the view
	 * direction. A card that rolls with the camera is the classic impostor tell. */
	Vector3 fwdV( camDir[0], camDir[1], camDir[2] );
	Vector3 worldUp( 0.0f, 0.0f, 1.0f );
	Vector3 rightV = Vector3::crossproduct( worldUp, fwdV );
	if ( rightV.length() < 1e-4f ) {
		// Looking straight down the pole: +X, the one the bake's own top frame uses.
		rightV = Vector3( 1.0f, 0.0f, 0.0f );
	}
	rightV.normalize();
	Vector3 upV = Vector3::crossproduct( fwdV, rightV );
	upV.normalize();

	c.fwdV = fwdV;
	c.rightV = rightV;
	c.upV = upV;
	c.centre = worldOffset
			+ Vector3( set.center[0], set.center[1], set.center[2] ) * sc;
}

/* The cut's coverage (lane IMPOSTORTEAR1): WW_IMPOSTOR_CUT=mean|strong forces rule 1 or 2 for every draw, read
 * by the drawer and not the caller so every path that draws a card has the way back. -1 = not forced. */
int envCutRule()
{
	WW_SWITCH_STATIC( int, v, []() {
		const QString s = wwEnvStr( "WW_IMPOSTOR_CUT" );
		if ( s.compare( QStringLiteral( "mean" ), Qt::CaseInsensitive ) == 0 )
			return 1;
		if ( s.compare( QStringLiteral( "strong" ), Qt::CaseInsensitive ) == 0 )
			return 2;
		return -1;
	}() );
	return v;
}


//! Why a set's cards are refused when its colour (or else its normal) sheet did not bind, by name.
QString sheetRefusal( Scene * scene, const ImpostorCardSet & set, bool haveColour )
{
	if ( !haveColour ) {
		QString arrayWhy;
		if ( set.layer >= 0 )
			arrayLayerTexture( scene, set.colour.localPath.isEmpty() ? set.colour.resolved : set.colour.localPath,
					set.layer, &arrayWhy );
		return set.layer >= 0
				? QStringLiteral( "the colour sheet's array layer %1 did not bind: %2" ).arg( set.layer ).arg( arrayWhy )
				: QStringLiteral( "the colour sheet did not bind: %1"
				"  (a loose set outside a data folder is unreachable by the texture cache --"
				" see ImpostorDraw::registerLooseSheets)" ).arg( set.colour.resolved );
	}
	return QStringLiteral( "the normal sheet did not bind: %1"
			"  (without it there is no height channel, so no depth and no blend)" ).arg( set.normal.resolved );
}

} // namespace

void ImpostorDraw::batchBegin()
{
	CardBatch & b = cardBatch();
	b = CardBatch();
	b.on = !batchRedPerDraw();
}

void ImpostorDraw::batchEnd( Scene * scene )
{
	CardBatch & b = cardBatch();
	const bool bound = b.on && b.prog;
	b = CardBatch();
	if ( bound && scene && scene->renderer ) {
		scene->renderer->stopProgram();
		scene->renderer->fn->glActiveTexture( GL_TEXTURE0 );
	}
}

QString ImpostorDraw::registerLooseSheets( Scene * scene, const ImpostorCardSet & set )
{
	if ( !scene || !scene->nifModel || !set.ok )
		return QString();
	/* Registered whether or not the sheets are beside the `.lodm`. The earlier
	 * `fromLocal` early-out read as "only loose sets need this", but it is the
	 * OTHER case that needs it most: a set baked into a data-shaped tree has
	 * nothing beside its `.lodm` at all, so every sheet is a game path and the
	 * tree holding them is exactly what has to reach the resource list.
	 * Registering a root that is already installed costs a duplicate entry. */
	QDir dir = QFileInfo( set.lodmPath ).absoluteDir();
	for ( int up = 0; up < 6; up++ ) {
		const QStringList hits = dir.entryList( { QStringLiteral( "textures" ) },
												QDir::Dirs | QDir::NoDotAndDotDot );
		if ( !hits.isEmpty() ) {
			/* THE `textures` FOLDER ITSELF IS THE ROOT, not the tree above it.
			 *
			 * `BA2File` names a loose file by cutting the path at the first
			 * component that is one of ITS OWN data-folder names -- "textures",
			 * "meshes", "materials" and eleven more (`ba2file.cpp:371..422`) --
			 * and when the registered path contains none of them the prefix
			 * length is ZERO and every file is indexed under its whole absolute
			 * path. Registering the parent therefore indexed the sheets as
			 * `e:/.../fixture/textures/data/...dds` while every lookup asks for
			 * `textures/data/...dds`, and nothing was ever found although the
			 * files were plainly there. This is also why `find_paths` registers
			 * the SUBFOLDERS of a data directory and not the directory
			 * (`gamemanager.cpp:808`). The folder's own spelling is taken from
			 * the listing rather than assumed, because the cut is by name. */
			const QString root = QDir::toNativeSeparators( dir.absoluteFilePath( hits.first() ) );
			/* THROUGH THE MODEL, NEVER THROUGH `GameManager` DIRECTLY.
			 * `addNIFResourcePath` replaces the model's GameResources object and
			 * deletes the old one; calling it here and dropping the result left
			 * `NifModel::gameResources` pointing at freed memory, and the next
			 * texture lookup segfaulted inside the paint event.
			 *
			 * The cast is because `Scene` holds the model as `const NifModel *`
			 * for reading; the object behind it is the window's own document and
			 * is not const. Registering a resource root is exactly as const as
			 * `findResourceFile`, which builds the archive index on demand
			 * through the same pointer. */
			const_cast< NifModel * >( scene->nifModel )->addResourceRoot( root );
			return root;
		}
		if ( !dir.cdUp() )
			break;
	}
	return QString();
}

QStringList ImpostorDraw::describeSelection( Scene * scene, const ImpostorCardSet & set,
												const Vector3 & worldOffset, const Options & opt )
{
	QStringList out;
	if ( !scene || !set.ok ) {
		out << QStringLiteral( "selection: no scene or the set was refused" );
		return out;
	}

	float camDir[3], w[3];
	int idx[3];
	selectFrames( scene, set, worldOffset, opt, camDir, idx, w );

	float look[3];
	{
		const ImpostorOct::Convention saved = ImpostorOct::g_convention;
		ImpostorOct::g_convention = effectiveConvention( set, opt );
		ImpostorOct::gridLookupDir( camDir, look );
		ImpostorOct::g_convention = saved;
	}
	float fi = 0.0f, fj = 0.0f;
	if ( !set.ring() )
		ImpostorOct::dirToGrid( look, set.oct, &fi, &fj );

	out << QStringLiteral( "convention %1 (%2, set token \"%3\")" ).arg(
			effectiveConvention( set, opt ) == ImpostorOct::Convention::AsBaked
				? QStringLiteral( "AsBaked" ) : QStringLiteral( "SpecLiteral" ),
			opt.forceConvention ? QStringLiteral( "FORCED by the caller" )
								: QStringLiteral( "from the set" ),
			set.conv.isEmpty() ? QStringLiteral( "none" ) : set.conv );
	out << QStringLiteral( "camdir %1 %2 %3" )
			.arg( double( camDir[0] ) ).arg( double( camDir[1] ) ).arg( double( camDir[2] ) );
	out << QStringLiteral( "lookdir %1 %2 %3" )
			.arg( double( look[0] ) ).arg( double( look[1] ) ).arg( double( look[2] ) );
	if ( set.ring() )
		out << QStringLiteral( "ring of %1 views, camera azimuth %2 deg" ).arg( set.views )
				.arg( double( std::atan2( camDir[1], camDir[0] ) * 57.2957795f ) );
	else
		out << QStringLiteral( "cell %1 %2 of %3" ).arg( double( fi ) ).arg( double( fj ) ).arg( set.oct );
	for ( int k = 0; k < 3; k++ ) {
		out << QStringLiteral( "frame %1 index %2 (i %3 j %4) weight %5" )
				.arg( k ).arg( idx[k] )
				.arg( idx[k] % set.cols() ).arg( idx[k] / set.cols() )
				.arg( double( w[k] ) );
	}
	if ( opt.shuffleFrames )
		out << QStringLiteral( "SHUFFLED: the indices above are the red control's, not the honest ones" );
	return out;
}

ImpostorDraw::Resolved ImpostorDraw::resolve( const Options & opt )
{
	WW_SWITCH_STATIC( float, envSlider, wwEnvIsSet( "WW_IMPOSTOR_SLIDER" )
			? qBound( 0.0f, float( wwEnvStr( "WW_IMPOSTOR_SLIDER" ).toDouble() ), 1.0f ) : -1.0f );
	WW_SWITCH_STATIC( bool, envSnap, wwEnvInt( "WW_IMPOSTOR_SNAP" ) != 0 );
	WW_SWITCH_STATIC( int, envSearch, wwEnvIsSet( "WW_IMPOSTOR_SEARCH" )
			? qBound( 0, wwEnvInt( "WW_IMPOSTOR_SEARCH" ), 64 ) : -1 );
	Resolved r;
	r.sliderForced = envSlider >= 0.0f;
	r.slider = r.sliderForced ? envSlider : qBound( 0.0f, opt.slider, 1.0f );
	r.snapForced = envSnap;
	r.snap = opt.snap || envSnap || r.slider <= 0.0f;
	r.frameCount = ( opt.heightBlend && !r.snap ) ? 3 : 1;
	/* The crisp end is the FLAT snap (bungo, 2026-09-23 13:1x); only the
	 * harness override (WW_IMPOSTOR_SNAP=1 / Options::snap) moves the lone
	 * frame by its depth. */
	r.parallax = opt.heightBlend && ( !r.snap || opt.snap || envSnap );
	r.searchForced = envSearch >= 0;
	r.searchSteps = r.searchForced ? envSearch
			: opt.depthSearchSteps >= 0 ? qBound( 0, opt.depthSearchSteps, 64 )
			: r.snap ? kCrispSearchSteps : kSmoothSearchSteps;
	r.sharpen = ( r.snap || r.slider >= 1.0f ) ? 1.0f : 1.0f / r.slider;
	/* THE CRISP CUT (R5, bungo 2026-09-24 21:1x). One frame at weight 1 under
	 * the stipple rule already cut exactly where that frame's coverage does;
	 * naming rule 2 here makes it so by construction, not by arithmetic. */
	r.cutRule = ( r.frameCount == 1 ) ? 2 : opt.cutRule;
	return r;
}

bool ImpostorDraw::drawCard( Scene * scene, const ImpostorCardSet & set,
								const Vector3 & worldOffset, const Options & opt, QString * why )
{
	auto refuse = [why]( const QString & msg ) -> bool {
		if ( why )
			*why = msg;
		return false;
	};

	if ( !scene || !scene->renderer )
		return refuse( QStringLiteral( "no scene or no renderer" ) );
	if ( !set.ok )
		return refuse( QStringLiteral( "the set was refused at load: %1" ).arg( set.error ) );

	wwImpostorTrace( "A enter" );
	// lane FARLOD1: the cell view's far cards take the cell variant (the cell's lights, fog, imagespace)
	const char * progName = opt.cellLit ? "impostor_cell.prog" : "impostor_oct.prog";
	CardBatch & batch = cardBatch();
	// lane PERF1: in a batch the program, its pass-wide uniforms and the state stay from the card before
	const bool fresh = !batch.on || !batch.prog || batch.progName != progName;
	NifSkopeOpenGLContext::Program * prog = fresh ? scene->renderer->useProgram( progName ) : batch.prog;
	wwImpostorTrace( prog ? "B useProgram ok" : "B useProgram null" );
	if ( !prog )
		return refuse( QStringLiteral( "res/shaders/%1 did not compile or was not found" ).arg( progName ) );
	if ( fresh && opt.cellLit ) {
		// both act on the bound program and do nothing where it lacks their uniforms
		wwLookdevFogUniforms( scene );
		wwCellLightsUniforms( scene );
	}
	if ( batch.on && fresh ) {
		batch.prog = prog;
		batch.progName = progName;
		batch.set = nullptr;
		batch.eye = eyeWorld( scene );
	}

	// ---- the sheets ------------------------------------------------------
	const bool sameSet = batch.on && !fresh && batch.set == &set;
	bool haveColour = sameSet && batch.haveColour, haveNormal = sameSet && batch.haveNormal;
	if ( !sameSet ) {
		haveColour = bindSheet( scene, prog, "ColourSheet", set.colour, 0, set.layer );
		wwImpostorTrace( "C colour bound" );
		haveNormal = bindSheet( scene, prog, "NormalSheet", set.normal, 1, set.nLayer() );
		wwImpostorTrace( "D normal bound" );
		const bool haveMask   = bindSheet( scene, prog, "MaskSheet",   set.mask,   2, set.layer );
		const bool haveEmiss  = bindSheet( scene, prog, "EmissiveSheet", set.emissive, 3, set.layer );
		wwImpostorTrace( "E aux bound" );
		if ( haveColour && haveNormal ) {
			prog->uni1b( "hasMaskSheet", haveMask );
			prog->uni1b( "hasEmissiveSheet", haveEmiss );
			prog->uni1b( "familyPbr", set.pbr );
			prog->uni1f( "emissiveScale", set.emissiveScale );
		}
		if ( batch.on ) {
			batch.set = &set;
			batch.haveColour = haveColour;
			batch.haveNormal = haveNormal;
		}
	}

	if ( !haveColour || !haveNormal )
		batch.prog = nullptr;	// a refusal unbinds the program: the next card binds it again
	if ( !haveColour || !haveNormal ) {
		scene->renderer->stopProgram();
		return refuse( sheetRefusal( scene, set, haveColour ) );
	}

	// ---- which frames, and the billboard (pickCard: the one function the instanced pass shares) ----
	const Resolved rs = resolve( opt );
	CardPick pick;
	pickCard( scene, set, worldOffset, opt, rs, nullptr, pick );
	wwImpostorTrace( "F frames selected" );
	const int frameCount = pick.frameCount;
	const float sc = pick.sc;

	const GLint locRect   = prog->uniLocation( "frameRect" );
	const GLint locWeight = prog->uniLocation( "frameWeight" );
	const GLint locRight  = prog->uniLocation( "frameRight" );
	const GLint locUp     = prog->uniLocation( "frameUp" );
	const GLint locFwd    = prog->uniLocation( "frameFwd" );
	const GLint locOffset = prog->uniLocation( "frameOffset" );

	for ( int k = 0; k < frameCount; k++ ) {
		const int i = pick.idx[k] % set.cols();
		const int j = pick.idx[k] / set.cols();
		CardFrame f;
		frameData( set, opt, i, j, f );
		if ( locRect >= 0 )
			scene->renderer->fn->glUniform4fv( locRect + k, 1, f.rect );
		if ( locWeight >= 0 )
			scene->renderer->fn->glUniform1f( locWeight + k, pick.w[k] );
		if ( locRight >= 0 )
			scene->renderer->fn->glUniform3fv( locRight + k, 1, f.right );
		if ( locUp >= 0 )
			scene->renderer->fn->glUniform3fv( locUp + k, 1, f.up );
		if ( locFwd >= 0 )
			scene->renderer->fn->glUniform3fv( locFwd + k, 1, f.fwd );
		if ( locOffset >= 0 ) {
			const float off[2] = { f.offR * sc, f.offU * sc };
			scene->renderer->fn->glUniform2fv( locOffset + k, 1, off );
		}
	}
	prog->uni1i( "frameCount", frameCount );
	wwImpostorTrace( "G per-frame uniforms" );

	const Vector3 & fwdV = pick.fwdV, & rightV = pick.rightV, & upV = pick.upV, & centre = pick.centre;
	const Vector3 eye = batch.on ? batch.eye : eyeWorld( scene );

	prog->uni3f( "cardCenter", centre[0], centre[1], centre[2] );
	prog->uni2f( "cardHalf", set.halfW * sc, set.halfH * sc );
	prog->uni3f( "cardRight", rightV[0], rightV[1], rightV[2] );
	prog->uni3f( "cardUp", upV[0], upV[1], upV[2] );
	prog->uni3f( "cardFwd", fwdV[0], fwdV[1], fwdV[2] );
	prog->uni3f( "cardEyeModel", eye[0], eye[1], eye[2] );
	prog->uni1b( "cardOrtho", false );
	prog->uni1f( "cardDepthSpan", set.depthSpan * sc );
	prog->uni1f( "cardMipCap", float( set.mips ) );

	/* THE SET'S OWN COVERAGE CONTRACT, not a constant of this viewer. A set
	 * that declares none sends base 0, which is how the shader's `coverageOf`
	 * is told to treat the alpha as the raw fraction. */
	prog->uni1f( "cardCovFloor", set.covOk() ? float( set.covFloor ) / 255.0f : 0.0f );
	prog->uni1f( "cardCovBase",  set.covOk() ? float( set.covBase )  / 255.0f : 0.0f );

	/* The cut, in the DECODED fraction domain the shader now works in. The
	 * default is VANILLA'S LOD alpha test, 128/255, for every set -- see
	 * `ImpostorDrawOptions::alphaThreshold` (lane IMPOSTORFIN1). The set's
	 * own `floor` is no longer the default; `WW_IMPOSTOR_ALPHA=0.0627` draws
	 * the old picture. */
	const float thr = ( opt.alphaThreshold >= 0.0f ) ? opt.alphaThreshold
			: kImpostorVanillaCut;
	prog->uni1f( "alphaThreshold", thr );
	prog->uni1b( "useHeightBlend", rs.parallax );
	/* The cut's coverage (lane IMPOSTORTEAR1): 0 the stippled cut, 1 the old
	 * 3-frame mean, 2 the strongest frame alone (see ImpostorDrawOptions::cutRule).
	 * WW_IMPOSTOR_CUT=mean|strong is read here and not by the caller so every
	 * path that draws a card has the way back. */
	prog->uni1i( "cutRule", envCutRule() >= 0 ? envCutRule() : rs.cutRule );
	/* The depth search (lane IMPOSTORDEPTH1): the slider's number unless
	 * Options or WW_IMPOSTOR_SEARCH=N force one (clamped to 64 so a typo cannot
	 * hang the GPU). The coverage is always decoded per texel, then filtered:
	 * that is the shader's only path now (lane IMPOSTORDEPTH2). */
	prog->uni1i( "depthSearchSteps", rs.searchSteps );
	prog->uni1b( "useDepthOffset", opt.depthOffset );
	prog->uni1b( "useBakedAo", opt.bakedAo );
	prog->uni1i( "debugChannel", opt.debugChannel );
	prog->uni1f( "swayAmplitude", opt.swayAmplitude );
	prog->uni1f( "swayPhase", opt.swayPhase );

	wwImpostorTrace( "H card uniforms" );
	if ( fresh ) {
		prog->uni4m( "modelViewMatrix", scene->view.toMatrix4() );
		wwImpostorTrace( "I modelview" );

		// ---- state -------------------------------------------------------
		/* Alpha TEST, not alpha blend: the card's coverage is a cut-out (spec
		 * 311..316) and it writes depth. Blending it would put it in the
		 * transparent pass, where it could not write the depth the pixel offset
		 * exists to write, and two cards would fight over order. */
		glDisable( GL_BLEND );
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		glDepthMask( GL_TRUE );
		glDisable( GL_CULL_FACE );
		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		wwImpostorTrace( "J state set" );
	}

	const float * attrs[8] = { kQuadPos, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, kQuadUv };
	scene->renderer->drawShape( 4, kQuadAttrMask, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, attrs, kQuadIdx );
	PassTime::countDraw( 1 );	// lane FARCARD1: the far cards were missing from the census draws
	wwImpostorTrace( "K drawShape returned" );

	if ( !batch.on ) {
		scene->renderer->stopProgram();
		scene->renderer->fn->glActiveTexture( GL_TEXTURE0 );
	}
	wwImpostorTrace( "L done" );

	if ( why )
		why->clear();
	return true;
}

/* ---- lane FARCARD1: the instanced card pass (impostordraw.h) ----
 * WHOLEFRAME (perfresearch1): the far-cards pass cost 17 ms on Concord, one draw per card with ~30 by-name
 * uniforms and a geometry hash each. Shipped games draw impostors instanced, the per-instance data in a buffer:
 * here one RGBA32F texture buffer of 6 texels a card, a per-set frame table built once per arming, one quad VAO
 * and one glDrawElementsInstanced per set. */
namespace {

struct InstGl
{
	QOpenGLContext * cx = nullptr;
	GLuint vao = 0, vbo = 0, ibo = 0, dataBuf = 0, dataTex = 0, tabBuf = 0, tabTex = 0;
	quint64 tabKey = ~quint64( 0 );
	QHash< const ImpostorCardSet *, int > tabBase;		// a set's first texel in the frame table
	std::vector< float > tab;
	bool tabDirty = false;
	QHash< const ImpostorSheet *, GLuint > layerIds;	// an array-layer sheet's texture, resolved once per arming
	std::vector< float > data;
};

InstGl & instGl()
{
	static InstGl g;
	return g;
}

constexpr int kInstTexels = 6;		// impostor_inst.vert's cardData layout

//! bindSheet, with an array layer's file resolved once per arming instead of once per pass.
bool bindInstSheet( Scene * scene, NifSkopeOpenGLContext::Program * prog, const char * uniform,
					const ImpostorSheet & sheet, int unit, int layer, InstGl & g )
{
	if ( layer < 0 )
		return bindSheet( scene, prog, uniform, sheet, unit, -1 );
	const GLint loc = prog->uniLocation( uniform );
	if ( loc < 0 )
		return false;
	auto it = g.layerIds.constFind( &sheet );
	if ( it == g.layerIds.constEnd() )
		it = g.layerIds.insert( &sheet, layerSheetId( scene, sheet, layer ) );
	if ( !*it )
		return false;
	auto * fn = scene->renderer->fn;
	fn->glActiveTexture( GL_TEXTURE0 + GLenum( unit ) );
	fn->glBindTexture( GL_TEXTURE_2D, *it );
	fn->glUniform1i( loc, unit );
	return true;
}

//! The set's frame table, appended on its first card: 4 texels a frame, frame index = i + j * cols.
int frameTableBase( const ImpostorCardSet & set, const ImpostorDraw::Options & opt, InstGl & g )
{
	const auto it = g.tabBase.constFind( &set );
	if ( it != g.tabBase.constEnd() )
		return *it;
	const int base = int( g.tab.size() / 4 );
	const int cols = set.cols(), n = cols * set.rows();
	for ( int f = 0; f < n; f++ ) {
		CardFrame fr;
		frameData( set, opt, f % cols, f / cols, fr );
		const float t[16] = { fr.rect[0], fr.rect[1], fr.rect[2], fr.rect[3],
			fr.right[0], fr.right[1], fr.right[2], fr.offR,
			fr.up[0], fr.up[1], fr.up[2], fr.offU,
			fr.fwd[0], fr.fwd[1], fr.fwd[2], 0.0f };
		g.tab.insert( g.tab.end(), t, t + 16 );
	}
	g.tabBase.insert( &set, base );
	g.tabDirty = true;
	return base;
}

void instGlCreate( Scene * scene, InstGl & g )
{
	auto * fn = scene->renderer->fn;
	fn->glGenVertexArrays( 1, &g.vao );
	fn->glBindVertexArray( g.vao );
	fn->glGenBuffers( 1, &g.vbo );
	fn->glBindBuffer( GL_ARRAY_BUFFER, g.vbo );
	fn->glBufferData( GL_ARRAY_BUFFER, GLsizeiptr( sizeof( kQuadPos ) + sizeof( kQuadUv ) ), nullptr, GL_STATIC_DRAW );
	fn->glBufferSubData( GL_ARRAY_BUFFER, 0, GLsizeiptr( sizeof( kQuadPos ) ), kQuadPos );
	fn->glBufferSubData( GL_ARRAY_BUFFER, GLintptr( sizeof( kQuadPos ) ), GLsizeiptr( sizeof( kQuadUv ) ), kQuadUv );
	fn->glEnableVertexAttribArray( 0 );
	fn->glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, nullptr );
	fn->glEnableVertexAttribArray( 7 );
	fn->glVertexAttribPointer( 7, 2, GL_FLOAT, GL_FALSE, 0, reinterpret_cast< const void * >( sizeof( kQuadPos ) ) );
	fn->glGenBuffers( 1, &g.ibo );
	fn->glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, g.ibo );
	fn->glBufferData( GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr( sizeof( kQuadIdx ) ), kQuadIdx, GL_STATIC_DRAW );
	fn->glBindVertexArray( 0 );
	fn->glBindBuffer( GL_ARRAY_BUFFER, 0 );
	fn->glGenBuffers( 1, &g.dataBuf );
	fn->glGenBuffers( 1, &g.tabBuf );
	fn->glGenTextures( 1, &g.dataTex );
	fn->glGenTextures( 1, &g.tabTex );
}

} // namespace

bool ImpostorDraw::instancedOn( Scene * scene )
{
	WW_SWITCH_STATIC( bool, off, !wwSwitchEcho( "far cards instanced", !( qgetenv( "WW_FARCARD" ) == "0" || qgetenv( "WW_FARCARD_RED" ) == "percard" ) ) );	// lane LIVESET1: per frame
	if ( off || !scene || !scene->renderer )
		return false;
	static GLint units = -1;
	if ( units < 0 )
		scene->renderer->fn->glGetIntegerv( GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &units );
	return units > WwCellUnit::CardFrames;
}

bool ImpostorDraw::drawCardsInstanced( Scene * scene, const QVector< InstCard > & cards, const Options & opt,
										quint64 tableKey, InstStats * stats )
{
	InstStats st;
	if ( !scene || !scene->renderer )
		return false;
	Renderer * r = scene->renderer;
	auto * fn = r->fn;
	/* lane PERF4: THE DEPTH REJECT. The card writes gl_FragDepth, so the hardware's early depth test is off and every
	 * fragment, hidden or not, marched its sheets. The scene's farthest depth per pixel (taken here, before the first
	 * card) lets impostor_oct.frag stop a fragment whose nearest possible depth is behind it: under GL_LEQUAL every
	 * sample would refuse that fragment anyway (stencil KEEP on a depth fail), so the picture is the same.
	 * WW_FARCARD_ZCULL=0: every fragment marches, as before. WW_FARCARD_ZCULL_RED=1 (gates only): the bound takes the
	 * card's FAR end, so fragments that would pass are thrown away and the picture must change. */
	WW_SWITCH_STATIC( bool, zcullOn, wwEnv( "WW_FARCARD_ZCULL" ) != "0" );
	WW_SWITCH_STATIC( bool, zcullRed, wwEnv( "WW_FARCARD_ZCULL_RED" ) == "1" );
	const unsigned int zTex = zcullOn ? wwCellHdrDepthMax( scene ) : 0u;
	{
		static QString said;
		const QString line = !zcullOn ? QStringLiteral( "far cards: depth reject off (WW_FARCARD_ZCULL=0)" )
			: zTex ? QStringLiteral( "far cards: depth reject on (the scene's farthest depth per pixel)%1" )
				.arg( zcullRed ? QStringLiteral( ", RED far end" ) : QString() )
			: QStringLiteral( "far cards: depth reject idle (no linear frame with a depth texture)" );
		if ( line != said ) {
			said = line;
			qInfo().noquote() << line;
		}
	}
	NifSkopeOpenGLContext::Program * prog = r->useProgram( "impostor_cellinst.prog" );
	if ( !prog )
		return false;

	InstGl & g = instGl();
	if ( g.cx != QOpenGLContext::currentContext() ) {
		g = InstGl();	// another context: the old names are not ours
		g.cx = QOpenGLContext::currentContext();
	}
	if ( !g.vao )
		instGlCreate( scene, g );
	// a forced convention reads the frames another way: its own tables
	const quint64 key = tableKey * 4 + ( opt.forceConvention ? 1 + quint64( opt.convention ) : 0 );
	if ( g.tabKey != key ) {
		g.tabKey = key;
		g.tabBase.clear();
		g.tab.clear();
		g.layerIds.clear();
		g.tabDirty = true;
	}

	// ---- the sets, in the order their first card comes; a set's cards keep their placement order ----
	QHash< const ImpostorCardSet *, int > groupOf;
	QVector< const ImpostorCardSet * > sets;
	QVector< int > count;
	QVector< int > groupOfCard( cards.size() );
	for ( int i = 0; i < cards.size(); i++ ) {
		auto it = groupOf.constFind( cards.at( i ).set );
		if ( it == groupOf.constEnd() ) {
			it = groupOf.insert( cards.at( i ).set, sets.size() );
			sets << cards.at( i ).set;
			count << 0;
		}
		count[*it]++;
		groupOfCard[i] = *it;
	}
	QVector< int > first( sets.size() ), fill( sets.size() );
	for ( int s = 0, run = 0; s < sets.size(); s++ ) {
		first[s] = fill[s] = run;
		run += count.at( s );
	}

	// ---- every card's values: drawCard's own arithmetic (pickCard), into the buffer ----
	const Resolved rs = resolve( opt );
	const Vector3 eye = eyeWorld( scene );
	g.data.resize( size_t( cards.size() ) * kInstTexels * 4 );
	for ( int i = 0; i < cards.size(); i++ ) {
		const InstCard & c = cards.at( i );
		const ImpostorCardSet & set = *c.set;
		Options o = opt;
		o.worldScale = c.scale;
		o.swayPhase = c.swayPhase;
		CardPick pk;
		pickCard( scene, set, Vector3( c.offset[0], c.offset[1], c.offset[2] ), o, rs, &eye, pk );
		frameTableBase( set, opt, g );
		const float sc = pk.sc;
		float * d = g.data.data() + size_t( fill[groupOfCard.at( i )]++ ) * kInstTexels * 4;
		d[0] = pk.centre[0]; d[1] = pk.centre[1]; d[2] = pk.centre[2]; d[3] = set.depthSpan * sc;
		d[4] = set.halfW * sc; d[5] = set.halfH * sc; d[6] = o.swayPhase; d[7] = sc;
		d[8] = pk.rightV[0]; d[9] = pk.rightV[1]; d[10] = pk.rightV[2]; d[11] = float( pk.frameCount );
		d[12] = pk.upV[0]; d[13] = pk.upV[1]; d[14] = pk.upV[2]; d[15] = float( pk.idx[0] );
		d[16] = pk.fwdV[0]; d[17] = pk.fwdV[1]; d[18] = pk.fwdV[2]; d[19] = float( pk.idx[1] );
		d[20] = pk.w[0]; d[21] = pk.w[1]; d[22] = pk.w[2]; d[23] = float( pk.idx[2] );
	}

	// ---- one upload a pass; the frame table only when a set was added ----
	st.bytes = qint64( g.data.size() * sizeof( float ) );
	fn->glBindBuffer( GL_TEXTURE_BUFFER, g.dataBuf );
	fn->glBufferData( GL_TEXTURE_BUFFER, GLsizeiptr( st.bytes ), g.data.data(), GL_STREAM_DRAW );
	if ( g.tabDirty ) {
		fn->glBindBuffer( GL_TEXTURE_BUFFER, g.tabBuf );
		fn->glBufferData( GL_TEXTURE_BUFFER, GLsizeiptr( g.tab.size() * sizeof( float ) ), g.tab.data(), GL_STATIC_DRAW );
		g.tabDirty = false;
	}
	fn->glBindBuffer( GL_TEXTURE_BUFFER, 0 );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::CardData ) );
	fn->glBindTexture( GL_TEXTURE_BUFFER, g.dataTex );
	fn->glTexBuffer( GL_TEXTURE_BUFFER, GL_RGBA32F, g.dataBuf );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::CardFrames ) );
	fn->glBindTexture( GL_TEXTURE_BUFFER, g.tabTex );
	fn->glTexBuffer( GL_TEXTURE_BUFFER, GL_RGBA32F, g.tabBuf );

	// ---- the pass: drawCard's pass-wide uniforms and state, once ----
	if ( opt.cellLit ) {
		wwLookdevFogUniforms( scene );
		wwCellLightsUniforms( scene );
	}
	prog->uni1i( "cardData", WwCellUnit::CardData );
	prog->uni1i( "cardFrames", WwCellUnit::CardFrames );
	if ( zTex ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::CardDepth ) );
		fn->glBindTexture( GL_TEXTURE_2D, GLuint( zTex ) );
	}
	prog->uni1i( "cardSceneDepth", WwCellUnit::CardDepth );
	prog->uni1b( "cardZcull", zTex != 0 );
	prog->uni1b( "cardZcullRed", zcullRed );
	prog->uni4m( "modelViewMatrix", scene->view.toMatrix4() );
	prog->uni3f( "cardEyeModel", eye[0], eye[1], eye[2] );
	prog->uni1b( "cardOrtho", false );
	prog->uni1f( "alphaThreshold", ( opt.alphaThreshold >= 0.0f ) ? opt.alphaThreshold : kImpostorVanillaCut );
	prog->uni1b( "useHeightBlend", rs.parallax );
	prog->uni1i( "cutRule", envCutRule() >= 0 ? envCutRule() : rs.cutRule );
	prog->uni1i( "depthSearchSteps", rs.searchSteps );
	prog->uni1b( "useDepthOffset", opt.depthOffset );
	prog->uni1b( "useBakedAo", opt.bakedAo );
	prog->uni1i( "debugChannel", opt.debugChannel );
	prog->uni1f( "swayAmplitude", opt.swayAmplitude );
	glDisable( GL_BLEND );
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LEQUAL );
	glDepthMask( GL_TRUE );
	glDisable( GL_CULL_FACE );
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	fn->glBindVertexArray( g.vao );

	// ---- one instanced draw per set ----
	for ( int s = 0; s < sets.size(); s++ ) {
		const ImpostorCardSet & set = *sets.at( s );
		const bool haveColour = bindInstSheet( scene, prog, "ColourSheet", set.colour, 0, set.layer, g );
		const bool haveNormal = bindInstSheet( scene, prog, "NormalSheet", set.normal, 1, set.nLayer(), g );
		if ( !haveColour || !haveNormal ) {
			if ( !st.refused )
				st.firstWhy = QFileInfo( set.lodmPath ).fileName() + QStringLiteral( ": " ) + sheetRefusal( scene, set, haveColour );
			st.refused += count.at( s );
			continue;
		}
		const bool haveMask = bindInstSheet( scene, prog, "MaskSheet", set.mask, 2, set.layer, g );
		const bool haveEmiss = bindInstSheet( scene, prog, "EmissiveSheet", set.emissive, 3, set.layer, g );
		prog->uni1b( "hasMaskSheet", haveMask );
		prog->uni1b( "hasEmissiveSheet", haveEmiss );
		prog->uni1b( "familyPbr", set.pbr );
		prog->uni1f( "emissiveScale", set.emissiveScale );
		prog->uni1f( "cardMipCap", float( set.mips ) );
		prog->uni1f( "cardCovFloor", set.covOk() ? float( set.covFloor ) / 255.0f : 0.0f );
		prog->uni1f( "cardCovBase",  set.covOk() ? float( set.covBase )  / 255.0f : 0.0f );
		prog->uni1i( "cardBase", first.at( s ) );
		prog->uni1i( "frameTableBase", g.tabBase.value( &set ) );
		fn->glDrawElementsInstanced( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr, GLsizei( count.at( s ) ) );
		PassTime::countDraw( count.at( s ) );
		st.draws++;
		st.drawn += count.at( s );
	}
	st.sets = sets.size();

	fn->glBindVertexArray( 0 );
	r->stopProgram();
	if ( zTex ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::CardDepth ) );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
	}
	fn->glActiveTexture( GL_TEXTURE0 );
	if ( stats )
		*stats = st;
	return true;
}

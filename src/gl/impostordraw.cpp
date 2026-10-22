/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "gl/impostordraw.h"

#include "gamemanager.h"
#include "gl/glscene.h"
#include "gl/gltex.h"
#include "gl/renderer.h"
#include "impostorcard.h"
#include "model/nifmodel.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

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
	static const bool on = qEnvironmentVariableIsSet( "WW_IMPOSTOR_TRACE" );
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

//! Bind one sheet to `unit` and tell the program. Returns false when the
//! texture cache could not produce it, which the caller REPORTS.
bool bindSheet( Scene * scene, NifSkopeOpenGLContext::Program * prog,
				const char * uniform, const ImpostorSheet & sheet, int unit )
{
	const GLint loc = prog->uniLocation( uniform );
	if ( loc < 0 )
		return false;
	scene->renderer->fn->glActiveTexture( GL_TEXTURE0 + GLenum( unit ) );

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
					float camDir[3], int idx[3], float w[3] )
{
	const Vector3 centre = worldOffset + Vector3( set.center[0], set.center[1], set.center[2] )
			* ( ( opt.worldScale > 0.0f ) ? opt.worldScale : 1.0f );
	Vector3 d = eyeWorld( scene ) - centre;
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

} // namespace

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
	static const float envSlider = qEnvironmentVariableIsSet( "WW_IMPOSTOR_SLIDER" )
			? qBound( 0.0f, float( qEnvironmentVariable( "WW_IMPOSTOR_SLIDER" ).toDouble() ), 1.0f ) : -1.0f;
	static const bool envSnap = qEnvironmentVariableIntValue( "WW_IMPOSTOR_SNAP" ) != 0;
	static const int envSearch = qEnvironmentVariableIsSet( "WW_IMPOSTOR_SEARCH" )
			? qBound( 0, qEnvironmentVariableIntValue( "WW_IMPOSTOR_SEARCH" ), 64 ) : -1;
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
	NifSkopeOpenGLContext::Program * prog = scene->renderer->useProgram( "impostor_oct.prog" );
	wwImpostorTrace( prog ? "B useProgram ok" : "B useProgram null" );
	if ( !prog )
		return refuse( QStringLiteral( "res/shaders/impostor_oct.prog did not compile or was not found" ) );

	// ---- the sheets ------------------------------------------------------
	const bool haveColour = bindSheet( scene, prog, "ColourSheet", set.colour, 0 );
	wwImpostorTrace( "C colour bound" );
	const bool haveNormal = bindSheet( scene, prog, "NormalSheet", set.normal, 1 );
	wwImpostorTrace( "D normal bound" );
	const bool haveMask   = bindSheet( scene, prog, "MaskSheet",   set.mask,   2 );
	const bool haveEmiss  = bindSheet( scene, prog, "EmissiveSheet", set.emissive, 3 );
	wwImpostorTrace( "E aux bound" );

	if ( !haveColour ) {
		scene->renderer->stopProgram();
		return refuse( QStringLiteral( "the colour sheet did not bind: %1"
				"  (a loose set outside a data folder is unreachable by the texture cache --"
				" see ImpostorDraw::registerLooseSheets)" ).arg( set.colour.resolved ) );
	}
	if ( !haveNormal ) {
		scene->renderer->stopProgram();
		return refuse( QStringLiteral( "the normal sheet did not bind: %1"
				"  (without it there is no height channel, so no depth and no blend)" )
				.arg( set.normal.resolved ) );
	}

	prog->uni1b( "hasMaskSheet", haveMask );
	prog->uni1b( "hasEmissiveSheet", haveEmiss );
	prog->uni1b( "familyPbr", set.pbr );
	prog->uni1f( "emissiveScale", set.emissiveScale );

	// ---- which frames ----------------------------------------------------
	float camDir[3], w[3];
	int idx[3];
	selectFrames( scene, set, worldOffset, opt, camDir, idx, w );
	wwImpostorTrace( "F frames selected" );

	/* THE SLIDER (lane IMPOSTORDEPTH2): crisp end = FLAT SNAP, the nearest
	 * frame alone, the no-blend picture below (WW_IMPOSTOR_SNAP=1 adds the
	 * height parallax, a harness override);
	 * smooth end = three frames, stipple, search 16; between = three frames
	 * with the weights sharpened. `resolve` reads the environment's overrides,
	 * here and not by the caller, like WW_IMPOSTOR_CUT, so every path that
	 * draws a card has them. */
	const Resolved rs = resolve( opt );
	// a RING set blends its two neighbouring azimuths, never three (CARDFIX1 step 5)
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
		// No blend: the nearest frame alone, at full weight. This is the
		// before-picture the gate's blend row compares ghosting against.
		int best = 0;
		for ( int k = 1; k < 3; k++ ) {
			if ( w[k] > w[best] )
				best = k;
		}
		idx[0] = idx[best];
		w[0] = 1.0f;
	}

	/* The placement's scale multiplies the centre offset, the quad, the depth
	 * span AND the per-frame offsets together -- they are all the same kind of
	 * model-unit measurement. See `ImpostorDrawOptions::worldScale`. Hoisted
	 * above the frame loop because the loop needs it too. */
	const float sc = ( opt.worldScale > 0.0f ) ? opt.worldScale : 1.0f;

	const float du = 1.0f / float( set.cols() );
	const GLint locRect   = prog->uniLocation( "frameRect" );
	const GLint locWeight = prog->uniLocation( "frameWeight" );
	const GLint locRight  = prog->uniLocation( "frameRight" );
	const GLint locUp     = prog->uniLocation( "frameUp" );
	const GLint locFwd    = prog->uniLocation( "frameFwd" );
	const GLint locOffset = prog->uniLocation( "frameOffset" );

	for ( int k = 0; k < frameCount; k++ ) {
		const int i = idx[k] % set.cols();
		const int j = idx[k] / set.cols();

		/* The frame's rectangle in the sheet, from the ONE function that knows
		 * it (`ImpostorOct::frameRect`, checked offline against the Python
		 * reference). Row j runs DOWNWARD in sheet space, the way the bake
		 * writes it; the fragment stage flips within the frame when it turns
		 * the card's own y into a v. A sign error here draws a perfectly
		 * convincing card of the WRONG frame, which is why the frame index is
		 * checked against the reference AND the picture against the mesh's
		 * silhouette -- neither alone would catch it. */
		if ( locRect >= 0 ) {
			float u0 = 0.0f, v0 = 0.0f, dU = du, dV = du;
			if ( set.ring() ) {
				// the ring's one row: frame v at [v/V, (v+1)/V) x [0, 1)
				u0 = float( i ) * du; v0 = 0.0f; dU = du; dV = 1.0f;
			} else {
				ImpostorOct::frameRect( i, j, set.oct, &u0, &v0, &dU, &dV );
			}
			const float rect[4] = { u0, v0, dU, dV };
			scene->renderer->fn->glUniform4fv( locRect + k, 1, rect );
		}
		if ( locWeight >= 0 )
			scene->renderer->fn->glUniform1f( locWeight + k, w[k] );

		/* The frame's own axes. `frameDir` gives the SPEC direction of cell
		 * (i,j); `frameBasis` turns a direction into the camera basis the bake
		 * actually used for it -- which for a legacy set still carries the old
		 * 180 degrees. The two are kept apart on purpose: the first is what the
		 * grid means, the second is what the picture in the sheet was taken
		 * with, and this lane found them disagreeing before the repair. */
		float right[3], up[3], fwd[3], dir[3];
		{
			const ImpostorOct::Convention saved = ImpostorOct::g_convention;
			ImpostorOct::g_convention = effectiveConvention( set, opt );
			if ( set.ring() ) {
				/* The ring's own eye (docs/LODGEN_LODM_FORMAT.md 3a): ( cos p, sin p, 0 ),
				 * p = 2 pi v / V; frameBasis then gives right ( -sin p, cos p, 0 ) and up
				 * ( 0, 0, 1 ) -- the bake's camera for that frame, under the spec law
				 * (a ring set is only ever baked under it). */
				const float p = 6.28318530718f * float( i ) / float( set.views );
				dir[0] = std::cos( p ); dir[1] = std::sin( p ); dir[2] = 0.0f;
				ImpostorOct::g_convention = ImpostorOct::Convention::SpecLiteral;
			} else {
				ImpostorOct::frameDir( i, j, set.oct, dir );
			}
			ImpostorOct::frameBasis( dir, right, up, fwd );
			ImpostorOct::g_convention = saved;
		}
		if ( locRight >= 0 )
			scene->renderer->fn->glUniform3fv( locRight + k, 1, right );
		if ( locUp >= 0 )
			scene->renderer->fn->glUniform3fv( locUp + k, 1, up );
		if ( locFwd >= 0 )
			scene->renderer->fn->glUniform3fv( locFwd + k, 1, fwd );

		/* WHERE THIS FRAME'S QUAD SITS. The set's own per-frame offset, in the
		 * frame's right/up, scaled with everything else. A set that declares
		 * none answers (0, 0), which is the law it was baked under. */
		if ( locOffset >= 0 ) {
			float offR = 0.0f, offU = 0.0f;
			set.frameOffsetOf( i, j, &offR, &offU );
			const float off[2] = { offR * sc, offU * sc };
			scene->renderer->fn->glUniform2fv( locOffset + k, 1, off );
		}
	}
	prog->uni1i( "frameCount", frameCount );
	wwImpostorTrace( "G per-frame uniforms" );

	// ---- the card's own billboard ----------------------------------------
	/* The quad faces the camera and stands UP: world +Z projected into the
	 * plane perpendicular to the view direction. A card that rolls with the
	 * camera is the classic impostor tell, and a tree that leans when you tilt
	 * your head is worse than no impostor at all. */
	Vector3 fwdV( camDir[0], camDir[1], camDir[2] );
	Vector3 worldUp( 0.0f, 0.0f, 1.0f );
	Vector3 rightV = Vector3::crossproduct( worldUp, fwdV );
	if ( rightV.length() < 1e-4f ) {
		// Looking straight down the pole: any right will do, and +X is the one
		// the bake's own top frame uses.
		rightV = Vector3( 1.0f, 0.0f, 0.0f );
	}
	rightV.normalize();
	Vector3 upV = Vector3::crossproduct( fwdV, rightV );
	upV.normalize();

	const Vector3 centre = worldOffset
			+ Vector3( set.center[0], set.center[1], set.center[2] ) * sc;
	const Vector3 eye = eyeWorld( scene );

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
	static const int envCutRule = []() {
		const QString v = qEnvironmentVariable( "WW_IMPOSTOR_CUT" );
		if ( v.compare( QStringLiteral( "mean" ), Qt::CaseInsensitive ) == 0 )
			return 1;
		if ( v.compare( QStringLiteral( "strong" ), Qt::CaseInsensitive ) == 0 )
			return 2;
		return -1;
	}();
	prog->uni1i( "cutRule", envCutRule >= 0 ? envCutRule : rs.cutRule );
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
	prog->uni4m( "modelViewMatrix", scene->view.toMatrix4() );
	wwImpostorTrace( "I modelview" );

	// ---- state and draw --------------------------------------------------
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
	const float * attrs[8] = { kQuadPos, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, kQuadUv };
	scene->renderer->drawShape( 4, kQuadAttrMask, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, attrs, kQuadIdx );
	wwImpostorTrace( "K drawShape returned" );

	scene->renderer->stopProgram();
	scene->renderer->fn->glActiveTexture( GL_TEXTURE0 );
	wwImpostorTrace( "L done" );

	if ( why )
		why->clear();
	return true;
}

#include "cellunderwater.h"
#include "wwswitch.h"

#include "cellunits.h"
#include "gl/cellhdr.h"
#include "gl/celllights.h"
#include "gl/celllooks.h"
#include "gl/cellwater.h"
#include "gl/cellwaterinfo.h"
#include "gl/glscene.h"
#include "gl/lookdevstage.h"
#include "gl/renderer.h"
#include "esmwater.h"

#include <QDebug>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{

struct UnderState
{
	QString echo = QStringLiteral( "underwater: off" );
	QString lastLogged;
	// lane WATERUNDER3: decided before the scene draws (wwCellUnderwaterFrame), so the imagespace is not a frame late
	const void * frameDoc = nullptr;
	WwWaterRecord frameRec;
	bool cloudsHidden = false;	// this frame's pass fogged a camera under water (the volume clouds draw after it)
};

UnderState & ust()
{
	static UnderState s;
	return s;
}

void note( const QString & line )
{
	UnderState & s = ust();
	s.echo = line;
	if ( line != s.lastLogged ) {	// telemetry on change only: a still camera prints once
		s.lastLogged = line;
		qInfo().noquote() << "cell" << line;
	}
}

//! Gauss-Jordan inverse of a 4x4 (row-major), double; false when singular.
bool invert4( const double a[4][4], double o[4][4] )
{
	double m[4][8];
	for ( int i = 0; i < 4; i++ )
		for ( int j = 0; j < 8; j++ )
			m[i][j] = j < 4 ? a[i][j] : ( j - 4 == i ? 1.0 : 0.0 );
	for ( int c = 0; c < 4; c++ ) {
		int p = c;
		for ( int r = c + 1; r < 4; r++ )
			if ( std::fabs( m[r][c] ) > std::fabs( m[p][c] ) )
				p = r;
		if ( std::fabs( m[p][c] ) < 1e-30 )
			return false;
		for ( int j = 0; j < 8; j++ )
			std::swap( m[c][j], m[p][j] );
		const double d = m[c][c];
		for ( int j = 0; j < 8; j++ )
			m[c][j] /= d;
		for ( int r = 0; r < 4; r++ )
			if ( r != c ) {
				const double f = m[r][c];
				for ( int j = 0; j < 8; j++ )
					m[r][j] -= f * m[c][j];
			}
	}
	for ( int i = 0; i < 4; i++ )
		for ( int j = 0; j < 4; j++ )
			o[i][j] = m[i][j + 4];
	return true;
}

//! the camera, world: world = R^T (v - t) / sc; row = view -> world
void eyeOf( const Scene * scene, double row[3][4], double eye[3] )
{
	const Transform & vt = scene->view;
	const double sc = vt.scale != 0.0f ? double( vt.scale ) : 1.0;
	for ( int i = 0; i < 3; i++ ) {
		double c = 0.0;
		for ( int j = 0; j < 3; j++ ) {
			row[i][j] = double( vt.rotation( j, i ) ) / sc;
			c -= double( vt.rotation( j, i ) ) * double( vt.translation[j] ) / sc;
		}
		row[i][3] = c;
		eye[i] = c;
	}
}

/* WW_CELL_UNDERWATER_RED, re-read per call: notint 1, nofog 2, radial 4 (WATERUNDER3), maxdepth 8 (WATERUNDER4),
 * oldplane 16 (WATERUNDER5: the water shape's box and top vertex, no room test), oldcolour 32 (WATERUNDER5: half
 * horizon + half underwater colour, no WaterColorMult), roofonly 64 (WATERUNDER6: any roof over the camera is a room) */
int uwRed( QString * name = nullptr )
{
	const QString r = wwEnvStr( "WW_CELL_UNDERWATER_RED" ).trimmed();
	if ( name )
		*name = r;
	static const char * const k[7] = { "notint", "nofog", "radial", "maxdepth", "oldplane", "oldcolour", "roofonly" };
	for ( int i = 0; i < 7; i++ )
		if ( r == QLatin1String( k[i] ) )
			return 1 << i;
	return 0;
}

//! which water: the baked body at the camera, else the lowest registered water shape above it (`margin` = near plane)
bool findWater( Scene * scene, float ex, float ey, float ez, float margin, WwWaterRecord & rec, float & H, int & cls,
	QString & via )
{
	WwWaterInfoHit hit;
	cls = 2;	// a placed water shape reads as a lake (the clear colour's rule), the worldspace default as sea
	if ( wwCellWaterInfoAt( scene->nifModel, ex, ey, hit ) && hit.body && ez < hit.height + margin
		&& ez > hit.height - hit.depth - 256.0f && wwCellWaterRecordOf( scene->nifModel, hit.form, rec ) ) {
		H = hit.height;
		cls = hit.cls >= 0 && hit.cls <= 2 ? hit.cls : 2;
		via = QStringLiteral( "baked body %1" ).arg( hit.body );
		return true;
	}
	const bool oldPlane = uwRed() == 16;
	bool sea = false, roofed = false;
	if ( wwCellWaterPlaneOver( scene, ex, ey, ez - margin, 8192.0f, H, rec, oldPlane, &sea, &roofed, uwRed() == 64 ) ) {
		if ( sea )
			cls = 0;
		via = oldPlane ? QStringLiteral( "water shape (red oldplane: box, top vertex)" )
			: sea ? QStringLiteral( "water shape (worldspace default: sea)" ) : QStringLiteral( "water shape" );
		return true;
	}
	if ( roofed )
		via = uwRed() == 64 ? QStringLiteral( "a roof over the camera (red roofonly)" ) : QStringLiteral( "a room below the water" );
	return false;
}

QByteArray lightRed()
{
	WW_SWITCH_STATIC( QByteArray, lr, wwEnv( "WW_CELL_WATER_UNDERLIGHT_RED" ).trimmed() );
	return lr;
}

} // namespace

void wwCellUnderwaterFrame( Scene * scene )
{
	UnderState & s = ust();
	s.frameDoc = nullptr;
	s.cloudsHidden = false;
	if ( !scene || !scene->nifModel || scene->selecting || !wwCellWaterOn()
		|| ( !wwLookOn( WwLook::Underwater ) && !wwLookOn( WwLook::WaterUnderLight ) ) )
		return;
	double row[3][4], eye[3];
	eyeOf( scene, row, eye );
	float H = 0.0f;
	int cls = 2;
	QString via;
	if ( findWater( scene, float( eye[0] ), float( eye[1] ), float( eye[2] ), 0.0f, s.frameRec, H, cls, via )
		&& float( eye[2] ) < H )
		s.frameDoc = scene->nifModel;
}

const WwWaterRecord * wwCellUnderwaterImageSpace( const void * nif )
{
	const UnderState & s = ust();
	if ( !nif || s.frameDoc != nif || !s.frameRec.hasUwIs || !wwLookOn( WwLook::WaterUnderLight )
		|| lightRed().contains( "noimagespace" ) )
		return nullptr;
	return &s.frameRec;
}

bool wwCellUnderwaterHidesClouds()
{
	return ust().cloudsHidden && !lightRed().contains( "clouds" );
}

bool wwCellUnderwaterSun( Scene * scene, float to[3], float col[3] )
{
	for ( int c = 0; c < 3; c++ ) {
		to[c] = c == 2 ? 1.0f : 0.0f;
		col[c] = 0.0f;
	}
	const WwCellLighting * cl = scene ? wwCellLightsFor( scene->nifModel ) : nullptr;
	if ( !cl )
		return false;
	float dif[4] = {}, amb[4] = {};
	if ( !cl->interior && wwLookdevActive() ) {
		wwLookdevLight( to, dif, amb );
		for ( int c = 0; c < 3; c++ )
			col[c] = dif[c] * wwLookdevSunlightScale();
	} else if ( cl->hasDirectional ) {
		for ( int c = 0; c < 3; c++ ) {
			to[c] = cl->dirTo[c];
			col[c] = cl->dirColor[c];
		}
	} else {
		return false;
	}
	const float len = std::sqrt( to[0] * to[0] + to[1] * to[1] + to[2] * to[2] );
	for ( int c = 0; c < 3; c++ )
		to[c] = len > 0.0f ? to[c] / len : ( c == 2 ? 1.0f : 0.0f );
	if ( to[2] <= 0.0f )	// below the horizon: no sun under the water
		for ( int c = 0; c < 3; c++ )
			col[c] = 0.0f;
	return true;
}

void wwCellUnderwaterPass( Scene * scene )
{
	const bool light = wwLookOn( WwLook::WaterUnderLight );	// lane WATERLOOK1 step 16: the Underwater light row
	if ( !wwLookOn( WwLook::Underwater ) && !light ) {
		ust().echo = QStringLiteral( "underwater: off" );
		return;
	}
	if ( !scene || !scene->renderer || scene->selecting || !wwCellHdrActive() || !wwCellLightsWanted( scene )
		|| wwCellProbePass( scene ) || !wwCellWaterOn() )
		return;
	Renderer * r = scene->renderer;

	const double sc = scene->view.scale != 0.0f ? double( scene->view.scale ) : 1.0;
	double row[3][4], eye[3];
	eyeOf( scene, row, eye );
	// clip -> view (projectionMatrix is column-major: pm[col][row])
	const auto & pm = r->globalUniforms->projectionMatrix;
	double P[4][4], Pi[4][4];
	for ( int i = 0; i < 4; i++ )
		for ( int j = 0; j < 4; j++ )
			P[i][j] = double( pm[j][i] );
	if ( !invert4( P, Pi ) )
		return;
	// the near plane's farthest corner from the eye: a camera this close above a surface still has pixels under it
	double margin = 0.0;
	for ( int k = 0; k < 4; k++ ) {
		const double nx = ( k & 1 ) ? 1.0 : -1.0, ny = ( k & 2 ) ? 1.0 : -1.0, cl[4] = { nx, ny, -1.0, 1.0 };
		double v[4] = { 0, 0, 0, 0 };
		for ( int i = 0; i < 4; i++ )
			for ( int j = 0; j < 4; j++ )
				v[i] += Pi[i][j] * cl[j];
		if ( std::fabs( v[3] ) > 1e-30 )
			margin = std::max( margin, std::sqrt( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] ) / std::fabs( v[3] ) / sc );
	}
	const float ex = float( eye[0] ), ey = float( eye[1] ), ez = float( eye[2] );

	// which water: the baked body at the camera, else the lowest registered water shape above it
	WwWaterRecord rec;
	float H = 0.0f;
	int cls = 2;
	QString via;
	if ( !findWater( scene, ex, ey, ez, float( margin ), rec, H, cls, via ) ) {
		note( via.isEmpty() ? QStringLiteral( "underwater: above water" ) : QStringLiteral( "underwater: above water (%1)" ).arg( via ) );
		return;
	}

	const unsigned int depthTex = wwCellHdrDepthMax( scene );
	auto prog = depthTex ? r->useProgram( "cell_underwater.prog" ) : nullptr;
	if ( !prog ) {
		note( QStringLiteral( "underwater: refused(%1)" ).arg( QLatin1String( depthTex ? "no program" : "no depth" ) ) );
		return;
	}
	auto fn = r->fn;

	float uw[3], hor[3], lo[3], up[3];
	for ( int c = 0; c < 3; c++ )
		uw[c] = wwWaterLin( rec.underwater[c] );
	if ( !wwLookdevWaterSky( hor, lo, up ) )
		for ( int c = 0; c < 3; c++ )
			hor[c] = wwWaterLin( rec.reflection[c] );	// the water draw's own fallback (cellwater.cpp)
	QString redS;
	const int red = uwRed( &redS );
	/* lane WATERUNDER5: the game's fog colour = the WATR underwater colour (linear) x WaterColorMult: white in an
	 * interior without Show Sky, else the weather's Water Multiplier colour (NAM0 row 14, CIELab over the day, byte /
	 * 255); white with no weather loaded. Red oldcolour: WATERUNDER1's half horizon + half underwater colour. */
	float fogCol[3], mult[3] = { 1.0f, 1.0f, 1.0f };
	QString multWhy;
	if ( red == 32 ) {
		for ( int c = 0; c < 3; c++ )
			fogCol[c] = 0.5f * ( hor[c] + uw[c] );
	} else {
		const WwCellLighting * cl = wwCellLightsFor( scene->nifModel );
		const bool sky = !cl || !cl->interior || cl->showSky;
		multWhy = !sky ? QStringLiteral( "white (interior)" )
			: wwLookdevWaterMult( mult ) ? QStringLiteral( "weather" ) : QStringLiteral( "white (no weather)" );
		for ( int c = 0; c < 3; c++ )
			fogCol[c] = uw[c] * mult[c];
	}
	// lane WATERUNDER3: radial = WATERUNDER1's distance; WATERUNDER4: maxdepth = the farthest depth per pixel

	GLint vp[4] = { 0, 0, 1, 1 }, prevActive = 0, prevTex = 0, bSrcRgb = 0, bDstRgb = 0, bSrcA = 0, bDstA = 0,
		bEqRgb = 0, bEqA = 0;
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glGetIntegerv( GL_BLEND_SRC_RGB, &bSrcRgb );
	fn->glGetIntegerv( GL_BLEND_DST_RGB, &bDstRgb );
	fn->glGetIntegerv( GL_BLEND_SRC_ALPHA, &bSrcA );
	fn->glGetIntegerv( GL_BLEND_DST_ALPHA, &bDstA );
	fn->glGetIntegerv( GL_BLEND_EQUATION_RGB, &bEqRgb );
	fn->glGetIntegerv( GL_BLEND_EQUATION_ALPHA, &bEqA );
	GLboolean colorMask[4], depthMask = GL_TRUE;
	fn->glGetBooleanv( GL_COLOR_WRITEMASK, colorMask );
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	const bool wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasStencil = fn->glIsEnabled( GL_STENCIL_TEST ),
		wasBlend = fn->glIsEnabled( GL_BLEND ), wasCull = fn->glIsEnabled( GL_CULL_FACE ),
		wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );

	/* lane WATERUNDER4: with MSAA the per-pixel draw fogs the samples near the farthest depth and the per-sample twin
	 * the rest, each from its own depth (no twin: one draw from the farthest, as before) */
	int samples = ( red & 8 ) ? 0 : wwCellHdrDepthMsBegin( scene, WwCellUnit::UnderDepthMs );
	QString msWhy = ( red & 8 ) ? QStringLiteral( "red maxdepth" ) : samples ? QString() : QStringLiteral( "one sample" );
	if ( samples > 0 && !r->useProgram( "cell_underwater_ms.prog" ) ) {
		wwCellHdrDepthMsEnd( scene, WwCellUnit::UnderDepthMs );
		samples = 0;
		msWhy = QStringLiteral( "no per-sample program" );
	}
	prog = r->useProgram( "cell_underwater.prog" );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::CardDepth ) );
	fn->glGetIntegerv( GL_TEXTURE_BINDING_2D, &prevTex );
	fn->glBindTexture( GL_TEXTURE_2D, depthTex );
	prog->uni1i( "uwDepth", WwCellUnit::CardDepth );
	prog->uni1i( "uwDepthMs", WwCellUnit::UnderDepthMs );
	prog->uni1i( "uwSamples", samples );
	float inv[16];
	for ( int i = 0; i < 4; i++ )
		for ( int j = 0; j < 4; j++ )
			inv[j * 4 + i] = float( Pi[i][j] );	// column-major
	fn->glUniformMatrix4fv( prog->uniLocation( "uwInvProj" ), 1, GL_FALSE, inv );
	FloatVector4 rows[3];
	for ( int i = 0; i < 3; i++ )
		rows[i] = FloatVector4( float( row[i][0] ), float( row[i][1] ), float( row[i][2] ), float( row[i][3] ) );
	prog->uni4fv( "uwRow", rows, 3 );
	prog->uni4f( "uwVp", FloatVector4( float( vp[0] ), float( vp[1] ), float( vp[2] ), float( vp[3] ) ) );
	prog->uni3f( "uwEye", ex, ey, ez );
	prog->uni4f( "uwFog", FloatVector4( rec.uwFogAmount, rec.uwFogNear, rec.uwFogFar, H ) );
	prog->uni3f( "uwColor", fogCol[0], fogCol[1], fogCol[2] );
	prog->uni3f( "uwTint", uw[0], uw[1], uw[2] );
	prog->uni1i( "uwRed", red );
	/* lane WATERLOOK1 step 16: the Underwater light row: the class's coefficients, lit by the sun the cell surfaces
	 * take (an exterior's weather sun x Sunlight Scale, else the cell's directional) and the water's sky; the meniscus
	 * 1.5 px each side, 0.6 dark at its middle */
	const QByteArray lr = lightRed();
	const int lred = ( lr.contains( "nomeniscus" ) ? 2 : 0 ) | ( lr.contains( "nocolor" ) ? 4 : 0 );
	float skyH[3], skyL[3], skyU[3];	// the water program's two sky rows (the horizon and the upper row)
	if ( !wwLookdevWaterSky( skyH, skyL, skyU ) )
		for ( int c = 0; c < 3; c++ )
			skyH[c] = skyU[c] = wwWaterLin( rec.reflection[c] );
	float sunTo[3], sunCol[3];	// lane WATERUNDER3: one source with the water program's surface from below
	wwCellUnderwaterSun( scene, sunTo, sunCol );
	const float sunCos = std::max( sunTo[2], 0.0f );
	prog->uni1i( "uwLight", light ? 1 : 0 );
	prog->uni1i( "uwLightRed", lred );
	prog->uni4f( "uwSun", FloatVector4( sunCol[0], sunCol[1], sunCol[2], sunCos ) );
	prog->uni3f( "uwSky", 0.5f * ( skyH[0] + skyU[0] ), 0.5f * ( skyH[1] + skyU[1] ), 0.5f * ( skyH[2] + skyU[2] ) );
	prog->uni1i( "uwClass", cls );
	prog->uni2f( "uwMenK", 1.5f, 0.6f );

	fn->glDisable( GL_DEPTH_TEST );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glDepthMask( GL_FALSE );
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE );
	fn->glEnable( GL_BLEND );
	fn->glBlendEquationSeparate( GL_FUNC_ADD, GL_FUNC_ADD );
	fn->glBlendFuncSeparate( GL_ONE, GL_SRC1_COLOR, GL_ZERO, GL_ONE );	// frame = frame * mul + add
	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = quad;
	r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
	if ( samples > 0 ) {
		auto prog2 = r->useProgram( "cell_underwater_ms.prog" );
		prog2->uni1i( "uwDepth", WwCellUnit::CardDepth );
		prog2->uni1i( "uwDepthMs", WwCellUnit::UnderDepthMs );
		prog2->uni1i( "uwSamples", samples );
		fn->glUniformMatrix4fv( prog2->uniLocation( "uwInvProj" ), 1, GL_FALSE, inv );
		prog2->uni4fv( "uwRow", rows, 3 );
		prog2->uni4f( "uwVp", FloatVector4( float( vp[0] ), float( vp[1] ), float( vp[2] ), float( vp[3] ) ) );
		prog2->uni3f( "uwEye", ex, ey, ez );
		prog2->uni4f( "uwFog", FloatVector4( rec.uwFogAmount, rec.uwFogNear, rec.uwFogFar, H ) );
		prog2->uni3f( "uwColor", fogCol[0], fogCol[1], fogCol[2] );
		prog2->uni3f( "uwTint", uw[0], uw[1], uw[2] );
		prog2->uni1i( "uwRed", red );
		prog2->uni1i( "uwLight", light ? 1 : 0 );
		prog2->uni1i( "uwLightRed", lred );
		prog2->uni4f( "uwSun", FloatVector4( sunCol[0], sunCol[1], sunCol[2], sunCos ) );
		prog2->uni3f( "uwSky", 0.5f * ( skyH[0] + skyU[0] ), 0.5f * ( skyH[1] + skyU[1] ), 0.5f * ( skyH[2] + skyU[2] ) );
		prog2->uni1i( "uwClass", cls );
		prog2->uni2f( "uwMenK", 1.5f, 0.6f );
		r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
	}
	r->stopProgram();
	if ( samples > 0 )
		wwCellHdrDepthMsEnd( scene, WwCellUnit::UnderDepthMs );

	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::CardDepth ) );
	fn->glBindTexture( GL_TEXTURE_2D, GLuint( prevTex ) );
	fn->glActiveTexture( GLenum( prevActive ) );
	fn->glBlendFuncSeparate( GLenum( bSrcRgb ), GLenum( bDstRgb ), GLenum( bSrcA ), GLenum( bDstA ) );
	fn->glBlendEquationSeparate( GLenum( bEqRgb ), GLenum( bEqA ) );
	fn->glColorMask( colorMask[0], colorMask[1], colorMask[2], colorMask[3] );
	fn->glDepthMask( depthMask );
	if ( !wasBlend ) fn->glDisable( GL_BLEND );
	if ( wasDepth ) fn->glEnable( GL_DEPTH_TEST );
	if ( wasStencil ) fn->glEnable( GL_STENCIL_TEST );
	if ( wasCull ) fn->glEnable( GL_CULL_FACE );
	if ( wasScissor ) fn->glEnable( GL_SCISSOR_TEST );
	ust().cloudsHidden = ez < H;	// lane WATERUNDER3: the eye itself is under water (not only the near plane's margin)
	const WwWaterRecord * uwIs = wwCellUnderwaterImageSpace( scene->nifModel );

	note( QStringLiteral( "underwater: %1, surface z %2 (camera %3 below), WATR %4 fog %5 near %6 far %7, colour %8 %9 %10%11" )
		.arg( via ).arg( double( H ), 0, 'f', 1 ).arg( double( H - ez ), 0, 'f', 1 )
		.arg( rec.form, 8, 16, QLatin1Char( '0' ) ).arg( double( rec.uwFogAmount ), 0, 'g', 4 )
		.arg( double( rec.uwFogNear ), 0, 'g', 6 ).arg( double( rec.uwFogFar ), 0, 'g', 6 )
		.arg( double( fogCol[0] ), 0, 'f', 4 ).arg( double( fogCol[1] ), 0, 'f', 4 ).arg( double( fogCol[2] ), 0, 'f', 4 )
		.arg( ( red ? QStringLiteral( " red %1" ).arg( redS ) : QString() )
			+ ( multWhy.isEmpty() ? QString() : QStringLiteral( " (water colour mult %1 %2 %3, %4)" )
				.arg( double( mult[0] ), 0, 'f', 4 ).arg( double( mult[1] ), 0, 'f', 4 ).arg( double( mult[2] ), 0, 'f', 4 )
				.arg( multWhy ) )
			+ ( samples > 0
			? QStringLiteral( "; fog per sample at edges (%1 samples)" ).arg( samples )
			: QStringLiteral( "; fog per pixel (%1)" ).arg( msWhy ) ) )
		+ ( light ? QStringLiteral( "; light class %1 sun %2 %3 %4 at cos %5%6" ).arg( cls )
			.arg( double( sunCol[0] ), 0, 'f', 3 ).arg( double( sunCol[1] ), 0, 'f', 3 ).arg( double( sunCol[2] ), 0, 'f', 3 )
			.arg( double( sunCos ), 0, 'f', 3 ).arg( lred ? QStringLiteral( " red %1" ).arg( QString::fromLatin1( lr ) ) : QString() )
			: QString() )
		+ ( uwIs ? QStringLiteral( "; imagespace %1 (INAM %2)" ).arg( uwIs->uwIsName )
			.arg( uwIs->imageSpace, 8, 16, QLatin1Char( '0' ) ) : QString() )
		+ ( ust().cloudsHidden ? ( wwCellUnderwaterHidesClouds() ? QStringLiteral( "; clouds fogged" )
			: QStringLiteral( "; clouds kept (red)" ) ) : QString() ) );
}

QString wwCellUnderwaterEcho()
{
	return ust().echo;
}

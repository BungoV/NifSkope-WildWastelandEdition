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

} // namespace

void wwCellUnderwaterPass( Scene * scene )
{
	if ( !wwLookOn( WwLook::Underwater ) ) {
		ust().echo = QStringLiteral( "underwater: off" );
		return;
	}
	if ( !scene || !scene->renderer || scene->selecting || !wwCellHdrActive() || !wwCellLightsWanted( scene )
		|| wwCellProbePass( scene ) || !wwCellWaterOn() )
		return;
	Renderer * r = scene->renderer;

	// the camera, world: world = R^T (v - t) / sc
	const Transform & vt = scene->view;
	const double sc = vt.scale != 0.0f ? double( vt.scale ) : 1.0;
	double row[3][4], eye[3];
	for ( int i = 0; i < 3; i++ ) {
		double c = 0.0;
		for ( int j = 0; j < 3; j++ ) {
			row[i][j] = double( vt.rotation( j, i ) ) / sc;
			c -= double( vt.rotation( j, i ) ) * double( vt.translation[j] ) / sc;
		}
		row[i][3] = c;
		eye[i] = c;
	}
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
	bool found = false;
	QString via;
	WwWaterInfoHit hit;
	if ( wwCellWaterInfoAt( scene->nifModel, ex, ey, hit ) && hit.body && ez < hit.height + float( margin )
		&& ez > hit.height - hit.depth - 256.0f && wwCellWaterRecordOf( scene->nifModel, hit.form, rec ) ) {
		H = hit.height;
		found = true;
		via = QStringLiteral( "baked body %1" ).arg( hit.body );
	} else if ( wwCellWaterPlaneOver( scene, ex, ey, ez - float( margin ), 8192.0f, H, rec ) ) {
		found = true;
		via = QStringLiteral( "water shape" );
	}
	if ( !found ) {
		note( QStringLiteral( "underwater: above water" ) );
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
	float fogCol[3];
	for ( int c = 0; c < 3; c++ )
		fogCol[c] = 0.5f * ( hor[c] + uw[c] );
	const QString redS = wwEnvStr( "WW_CELL_UNDERWATER_RED" ).trimmed();
	const int red = redS == QLatin1String( "notint" ) ? 1 : redS == QLatin1String( "nofog" ) ? 2 : 0;

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

	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::CardDepth ) );
	fn->glGetIntegerv( GL_TEXTURE_BINDING_2D, &prevTex );
	fn->glBindTexture( GL_TEXTURE_2D, depthTex );
	prog->uni1i( "uwDepth", WwCellUnit::CardDepth );
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
	r->stopProgram();

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

	note( QStringLiteral( "underwater: %1, surface z %2 (camera %3 below), WATR %4 fog %5 near %6 far %7, colour %8 %9 %10%11" )
		.arg( via ).arg( double( H ), 0, 'f', 1 ).arg( double( H - ez ), 0, 'f', 1 )
		.arg( rec.form, 8, 16, QLatin1Char( '0' ) ).arg( double( rec.uwFogAmount ), 0, 'g', 4 )
		.arg( double( rec.uwFogNear ), 0, 'g', 6 ).arg( double( rec.uwFogFar ), 0, 'g', 6 )
		.arg( double( fogCol[0] ), 0, 'f', 4 ).arg( double( fogCol[1] ), 0, 'f', 4 ).arg( double( fogCol[2] ), 0, 'f', 4 )
		.arg( red ? QStringLiteral( " red %1" ).arg( redS ) : QString() ) );
}

QString wwCellUnderwaterEcho()
{
	return ust().echo;
}

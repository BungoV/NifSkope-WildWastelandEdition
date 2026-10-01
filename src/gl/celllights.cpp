/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "celllights.h"

#include "gl/glscene.h"
#include "gl/renderer.h"

#include <QHash>
#include <QSettings>

#include <cmath>
#include <vector>

namespace
{

constexpr int kTextureUnit = 14;		// TexCache allocates from unit 0 upward; 15 is the CSM map
constexpr int kTexelsPerLight = 4;

struct ClState
{
	bool loaded = false;
	bool on = false;
	bool pinned = false;
	int probe = 0;
	int red = 0;            // bit 1 linear, 2 axis, 4 nodalc
	QHash<const void *, WwCellLighting> docs;
	QHash<const void *, int> version;
	int nextVersion = 1;
};

ClState & st()
{
	static ClState s;
	if ( !s.loaded ) {
		s.loaded = true;
		const QByteArray pin = qgetenv( "WW_CELL_LIT" );
		if ( !pin.isEmpty() ) {
			s.pinned = true;
			s.on = pin.trimmed() != "0";
		} else {
			s.on = QSettings().value( QStringLiteral( "WW/CellLights" ), false ).toBool();
		}
		s.probe = qEnvironmentVariableIntValue( "WW_CELL_LIT_PROBE" );
		const QByteArray red = qgetenv( "WW_CELL_LIT_RED" ).trimmed();
		if ( red == "linear" )
			s.red = 1;
		else if ( red == "axis" )
			s.red = 2;
		else if ( red == "nodalc" )
			s.red = 4;
	}
	return s;
}

//! one texture buffer per GL context (the renderer), re-uploaded when the published version moves
struct Gpu
{
	GLuint buf = 0, tex = 0;
	const void * doc = nullptr;
	int version = 0;
	int count = 0;
};
QHash<const void *, Gpu> & gpus()
{
	static QHash<const void *, Gpu> g;
	return g;
}

}	// namespace

void wwCellLightsPublish( const void * nif, const WwCellLighting & lighting )
{
	ClState & s = st();
	s.docs.insert( nif, lighting );
	s.version.insert( nif, s.nextVersion++ );
}

const WwCellLighting * wwCellLightsFor( const void * nif )
{
	ClState & s = st();
	auto it = s.docs.constFind( nif );
	return it == s.docs.constEnd() ? nullptr : &*it;
}

bool wwCellLightsOn()
{
	return st().on;
}

void wwCellLightsSetOn( bool on )
{
	ClState & s = st();
	if ( s.pinned )
		return;
	s.on = on;
	QSettings().setValue( QStringLiteral( "WW/CellLights" ), on );
}

bool wwCellLightsWanted( Scene * scene )
{
	if ( !scene || !scene->renderer || !scene->nifModel || scene->selecting )
		return false;
	if ( !st().on || !wwCellLightsFor( scene->nifModel ) )
		return false;
	// an orthographic camera has no eye position to measure light distances from
	return scene->hasOption( Scene::DoLighting ) && scene->renderer->globalUniforms->projectionMatrix[3][3] != 1.0f;
}

void wwCellLightsUniforms( Scene * scene )
{
	if ( !scene || !scene->renderer )
		return;
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->getCurrentProgram();
	if ( !prog || prog->uniLocation( "cellOn" ) < 0 )
		return;
	const bool on = wwCellLightsWanted( scene );
	prog->uni1b( "cellOn", on );
	auto fn = r->fn;
	ClState & s = st();
	Gpu & g = gpus()[r];
	const WwCellLighting * L = on ? wwCellLightsFor( scene->nifModel ) : nullptr;
	if ( L && ( g.doc != scene->nifModel || g.version != s.version.value( scene->nifModel ) ) ) {
		std::vector<float> t;
		t.reserve( size_t( L->lights.size() ) * kTexelsPerLight * 4 + 16 );
		for ( const WwCellLight & l : L->lights ) {
			float dir[3] = { l.dir[0], l.dir[1], l.dir[2] };
			const float tex[16] = {
				l.pos[0], l.pos[1], l.pos[2], l.radius,
				l.color[0], l.color[1], l.color[2], l.spot ? l.cosOuter : -2.0f,
				dir[0], dir[1], dir[2], l.cone,
				l.bias, l.scale, l.exponent, l.noSpecular ? 1.0f : 0.0f };
			t.insert( t.end(), tex, tex + 16 );
		}
		if ( t.empty() )
			t.assign( 16, 0.0f );	// a buffer texture must have a store
		if ( !g.buf ) {
			fn->glGenBuffers( 1, &g.buf );
			fn->glGenTextures( 1, &g.tex );
		}
		fn->glBindBuffer( GL_TEXTURE_BUFFER, g.buf );
		fn->glBufferData( GL_TEXTURE_BUFFER, GLsizeiptr( t.size() * sizeof( float ) ), t.data(), GL_STATIC_DRAW );
		fn->glBindBuffer( GL_TEXTURE_BUFFER, 0 );
		g.doc = scene->nifModel;
		g.version = s.version.value( scene->nifModel );
		g.count = int( L->lights.size() );
	}
	/* The sampler is bound whether or not this draw lights: a samplerBuffer left on unit 0
	 * beside BaseMap's sampler2D is a draw-time INVALID_OPERATION. */
	GLint prevActive = 0;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kTextureUnit ) );
	if ( g.tex && g.buf ) {
		fn->glBindTexture( GL_TEXTURE_BUFFER, g.tex );
		fn->glTexBuffer( GL_TEXTURE_BUFFER, GL_RGBA32F, g.buf );
	} else {
		fn->glBindTexture( GL_TEXTURE_BUFFER, 0 );
	}
	fn->glActiveTexture( GLenum( prevActive ) );
	prog->uni1i( "cellLights", kTextureUnit );
	if ( !L )
		return;
	prog->uni1i( "cellLightCount", g.count );
	// world = R^T (posView - t) / sc  (lookdevstage.cpp's fog uses the same inverse)
	const Transform & vt = scene->view;
	const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
	for ( int k = 0; k < 3; k++ ) {
		float w = 0.0f;
		for ( int j = 0; j < 3; j++ )
			w -= vt.rotation( j, k ) * vt.translation[j];
		prog->uni4f_l( prog->uniLocation( "cellRow[%d]", k ), FloatVector4( vt.rotation( 0, k ) / sc,
			vt.rotation( 1, k ) / sc, vt.rotation( 2, k ) / sc, w / sc ) );
	}
	// PRTP2 section 4: per channel, ((p - n) / 2 per axis, mean of the six), gamma; the shader powers it
	const bool dalc = L->hasDalc && !( s.red & 4 );
	prog->uni1b( "cellHasDalc", dalc );
	for ( int c = 0; c < 3 && dalc; c++ ) {
		float mean = 0.0f;
		for ( int a = 0; a < 6; a++ )
			mean += L->dalc[a][c] / 6.0f;
		prog->uni4f_l( prog->uniLocation( "cellDalc[%d]", c ), FloatVector4(
			( L->dalc[0][c] - L->dalc[1][c] ) * 0.5f, ( L->dalc[2][c] - L->dalc[3][c] ) * 0.5f,
			( L->dalc[4][c] - L->dalc[5][c] ) * 0.5f, mean ) );
	}
	prog->uni1b( "cellHasDir", L->hasDirectional );
	prog->uni3f( "cellDirColor", L->dirColor[0], L->dirColor[1], L->dirColor[2] );
	prog->uni3f( "cellDirTo", L->dirTo[0], L->dirTo[1], L->dirTo[2] );
	prog->uni1b( "cellInterior", L->interior );
	prog->uni3f( "cellCenter", L->center[0], L->center[1], L->center[2] );
	prog->uni1i( "cellProbe", s.probe );
	prog->uni1i( "cellRed", s.red );
}

int wwCellLightsRed()
{
	return st().red;
}

QString wwCellLightsEcho( Scene * scene )
{
	ClState & s = st();
	const WwCellLighting * L = scene && scene->nifModel ? wwCellLightsFor( scene->nifModel ) : nullptr;
	QString o = QStringLiteral( "celllit=%1(asked=%2)" ).arg( wwCellLightsWanted( scene ) ? "on" : "off" )
		.arg( s.on ? 1 : 0 );
	if ( L )
		o += QStringLiteral( " %1" ).arg( L->summary );
	else
		o += QStringLiteral( " (no cell lighting published for this document)" );
	if ( s.probe )
		o += QStringLiteral( " probe=%1" ).arg( s.probe );
	if ( s.red )
		o += QStringLiteral( " red=%1" ).arg( s.red );
	return o;
}

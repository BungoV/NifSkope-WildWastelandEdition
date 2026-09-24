/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "sunshadow.h"

#include "gl/glnode.h"
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "gl/lookdevstage.h"
#include "gl/renderer.h"

#include <QDir>
#include <QFile>
#include <QOpenGLContext>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

/* See sunshadow.h for the law and its sources. Everything the fit computes is in
 * doubles and echoed, so tests/spells/pbr_csm1_gates.py can re-derive it from the
 * camera and the hour alone. */

namespace
{

constexpr int kCascades = 3;
constexpr double kSplit0 = 800.0, kSplit1 = 3000.0;	// spec 2.1: vanilla fShadowCascadeSplit 800 / 3000
constexpr double kDefaultDistance = 3000.0;		// the shadow distance D, code default (spec 3)
constexpr int kDefaultMap = 2048;			// the settings' code default map size (spec 3)
constexpr double kBlend = 100.0;			// B: the slice overlap and the seam blend band (spec 2.1/2.4)
constexpr double kLightBack = 15000.0;			// the light origin sits this far behind the camera along L (spec 2.3)
constexpr double kNear = 150.0;				// the light's near plane, and far = max z + 150 (spec 2.3)
constexpr float kSlopeBias = 6.0f, kUnitBias = 12.0f;	// glPolygonOffset (spec 2.6)
constexpr float kOffsetA = 0.275f, kOffsetB = 1.0f;	// receiver depth offsets, cascade A / B of the pair (spec 2.4, INFERRED pair law)
constexpr int kTextureUnit = 15;			// TexCache allocates from unit 0 upward

struct Cascade
{
	double zn = 0, zf = 0;		// the slice, planar view depth
	double minX = 0, maxX = 0, minY = 0, maxY = 0, maxZ = 0;	// light-space box relative to the camera (z + 15000)
	double w = 0, h = 0;		// floored extents
	double texel = 1;
	int n = 0;			// the sub-unit branch: texel = 1/n (0 = the integer branch)
	int vw = 0, vh = 0;		// viewport, texels
	double pb[3] = { 0, 0, 0 };	// the snapped light origin (dot(C,right), dot(C,up), dot(C,L) - 15000)
	double l = 0, b = 0;		// the snapped window corner
	double far = 0;
	float clipFromView[16];		// the caster pass: view space -> the cascade's clip space
	float uvdFromView[16];		// the receiver: view space -> (u, v, depth)
};

struct CsmState
{
	bool init = false;
	double D = kDefaultDistance;
	int map = kDefaultMap;
	int probe = 0;
	QStringList reds;

	// GL objects, per context
	const void * ctx = nullptr;
	unsigned int tex = 0, fbo = 0;
	int texMap = 0;

	// the last pass
	bool valid = false;
	QString last = QStringLiteral( "pending" );
	Cascade c[kCascades];
	double L[3] = { 0, 0, -1 }, right[3] = { 1, 0, 0 }, up[3] = { 0, 1, 0 };
	double C[3] = { 0, 0, 0 }, fwd[3] = { 0, 0, -1 }, tanX = 1, tanY = 1, camNear = 1, sc = 1;
	double rc[3] = { 1, 0, 0 }, uc[3] = { 0, 1, 0 };	// the camera's right and up rows (world)
	double split[2] = { kSplit0, kSplit1 };
	int casters = 0, tris = 0;
	bool groundCast = false;
};

CsmState & cs()
{
	static CsmState s;
	if ( !s.init ) {
		s.init = true;
		bool ok = false;
		const double d = qEnvironmentVariable( "WW_CSM_DISTANCE" ).toDouble( &ok );
		if ( ok && d > 2.0 * kBlend )
			s.D = d;
		const int m = qEnvironmentVariable( "WW_CSM_MAP" ).toInt( &ok );
		if ( ok && m >= 64 && m <= 8192 )
			s.map = m;
		s.probe = qEnvironmentVariable( "WW_CSM_PROBE" ).toInt();
		s.reds = qEnvironmentVariable( "WW_CSM_RED" ).split( QChar( ',' ), Qt::SkipEmptyParts );
		for ( QString & r : s.reds )
			r = r.trimmed().toLower();
	}
	return s;
}

bool red( const char * name )
{
	return cs().reds.contains( QString::fromLatin1( name ) );
}

int redBits()
{
	return ( red( "noblend" ) ? 1 : 0 ) | ( red( "nofade" ) ? 2 : 0 ) | ( red( "diffonly" ) ? 4 : 0 )
		| ( red( "factorhalf" ) && !wwLookdevShadows() ? 8 : 0 );
}

double dot3( const double * a, const double * b )
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void cross3( const double * a, const double * b, double * o )
{
	o[0] = a[1] * b[2] - a[2] * b[1];
	o[1] = a[2] * b[0] - a[0] * b[2];
	o[2] = a[0] * b[1] - a[1] * b[0];
}

bool norm3( double * v )
{
	const double l = std::sqrt( dot3( v, v ) );
	if ( l < 1e-12 )
		return false;
	for ( int i = 0; i < 3; i++ )
		v[i] /= l;
	return true;
}

//! the spec's snap: onto the texel (integer branch) or onto 1/n (the sub-unit branch)
double snap( double v, const Cascade & c )
{
	if ( red( "nosnap" ) )
		return v;
	if ( c.n > 0 )
		return std::floor( v * c.n ) / c.n;
	return std::floor( v / c.texel ) * c.texel;
}

/*! One affine row out = a . Q + k over WORLD Q, re-expressed over VIEW space p
 *  (posView = R (sc Q) + t, so Q = R^T (p - t) / sc), written into row `row` of
 *  the column-major matrix m. */
void viewRow( float * m, int row, const double a[3], double k, const Transform & vt, double sc )
{
	double ap[3];
	for ( int i = 0; i < 3; i++ )
		ap[i] = ( vt.rotation( i, 0 ) * a[0] + vt.rotation( i, 1 ) * a[1] + vt.rotation( i, 2 ) * a[2] ) / sc;
	double kk = k;
	for ( int i = 0; i < 3; i++ )
		kk -= ap[i] * vt.translation[i];
	m[0 * 4 + row] = float( ap[0] );
	m[1 * 4 + row] = float( ap[1] );
	m[2 * 4 + row] = float( ap[2] );
	m[3 * 4 + row] = float( kk );
}

void lastRow( float * m )
{
	m[3] = 0.0f;
	m[7] = 0.0f;
	m[11] = 0.0f;
	m[15] = 1.0f;
}

//! the fit, spec 2.1-2.3, for the camera and light already in s
void fitCascades( CsmState & s, const Transform & vt )
{
	const bool one = red( "onecascade" );
	s.split[0] = red( "wrongsplit" ) ? 400.0 : kSplit0;
	s.split[1] = red( "wrongsplit" ) ? 1500.0 : kSplit1;
	const double far[kCascades] = { s.split[0], s.split[1], s.D };	// the last is overridden by D
	double * rc = s.rc, * uc = s.uc;
	for ( int j = 0; j < 3; j++ ) {
		rc[j] = vt.rotation( 0, j );
		uc[j] = vt.rotation( 1, j );
	}
	for ( int i = 0; i < kCascades; i++ ) {
		Cascade & c = s.c[i];
		c = Cascade();
		c.zn = ( i == 0 || one ) ? s.camNear : far[i - 1] - kBlend;
		c.zf = ( one ? s.D : far[i] ) + kBlend;
		bool first = true;
		for ( int zi = 0; zi < 2; zi++ ) {
			const double z = zi ? c.zf : c.zn;
			for ( int sx = -1; sx <= 1; sx += 2 ) {
				for ( int sy = -1; sy <= 1; sy += 2 ) {
					double q[3];	// Q - C
					for ( int j = 0; j < 3; j++ )
						q[j] = z * ( s.fwd[j] + sx * s.tanX * rc[j] + sy * s.tanY * uc[j] );
					const double x = dot3( q, s.right ), y = dot3( q, s.up ), zz = dot3( q, s.L ) + kLightBack;
					if ( first ) {
						c.minX = c.maxX = x;
						c.minY = c.maxY = y;
						c.maxZ = zz;
						first = false;
					} else {
						c.minX = std::min( c.minX, x );
						c.maxX = std::max( c.maxX, x );
						c.minY = std::min( c.minY, y );
						c.maxY = std::max( c.maxY, y );
						c.maxZ = std::max( c.maxZ, zz );
					}
				}
			}
		}
		c.w = std::floor( c.maxX - c.minX );
		c.h = std::floor( c.maxY - c.minY );
		const double ext = std::max( std::max( c.w, c.h ), 1.0 );
		const double ratio = ext / s.map;
		if ( red( "nosnap" ) ) {
			c.texel = ratio;
			c.n = 0;
		} else if ( ratio > 0.5 ) {
			c.texel = std::ceil( ratio );
			c.n = 0;
		} else {
			c.n = std::max( 1, int( std::floor( 1.0 / ratio ) ) );
			c.texel = 1.0 / c.n;
		}
		c.pb[0] = snap( dot3( s.C, s.right ), c );
		c.pb[1] = snap( dot3( s.C, s.up ), c );
		c.pb[2] = snap( dot3( s.C, s.L ) - kLightBack, c );
		c.l = snap( c.minX, c );
		c.b = snap( c.minY, c );
		c.vw = std::clamp( int( std::floor( c.w / c.texel + 1e-9 ) ), 1, s.map );
		c.vh = std::clamp( int( std::floor( c.h / c.texel + 1e-9 ) ), 1, s.map );
		c.far = c.maxZ + kNear;

		// world rows: x_l = right.Q - pb.x, window [l, l + vw texel] (INFERRED right edge)
		const double wx = c.vw * c.texel, wy = c.vh * c.texel, wz = c.far - kNear;
		double a[3];
		for ( int j = 0; j < 3; j++ )
			a[j] = s.right[j] * 2.0 / wx;
		viewRow( c.clipFromView, 0, a, -( c.pb[0] + c.l ) * 2.0 / wx - 1.0, vt, s.sc );
		for ( int j = 0; j < 3; j++ )
			a[j] = s.up[j] * 2.0 / wy;
		viewRow( c.clipFromView, 1, a, -( c.pb[1] + c.b ) * 2.0 / wy - 1.0, vt, s.sc );
		for ( int j = 0; j < 3; j++ )
			a[j] = s.L[j] * 2.0 / wz;
		viewRow( c.clipFromView, 2, a, -( c.pb[2] + kNear ) * 2.0 / wz - 1.0, vt, s.sc );
		lastRow( c.clipFromView );

		const double tm = c.texel * s.map;
		for ( int j = 0; j < 3; j++ )
			a[j] = s.right[j] / tm;
		viewRow( c.uvdFromView, 0, a, -( c.pb[0] + c.l ) / tm, vt, s.sc );
		for ( int j = 0; j < 3; j++ )
			a[j] = s.up[j] / tm;
		viewRow( c.uvdFromView, 1, a, -( c.pb[1] + c.b ) / tm, vt, s.sc );
		for ( int j = 0; j < 3; j++ )
			a[j] = s.L[j] / wz;
		viewRow( c.uvdFromView, 2, a, -( c.pb[2] + kNear ) / wz, vt, s.sc );
		lastRow( c.uvdFromView );
	}
}

//! a shape that goes into the map, with CPU-skinned vertices when the GPU would skin it
struct Caster
{
	const Shape * sh;
	bool twoSided;
	std::vector<Vector3> skinned;
};

bool ensureTargets( CsmState & s, Renderer * r, QString & why )
{
	auto fn = r->fn;
	const void * ctx = QOpenGLContext::currentContext();
	if ( ctx != s.ctx ) {
		// a new context: the old names belong to the old one
		s.ctx = ctx;
		s.tex = s.fbo = 0;
		s.texMap = 0;
	}
	if ( !s.tex || s.texMap != s.map ) {
		if ( !s.tex )
			fn->glGenTextures( 1, &s.tex );
		GLint prevTex = 0;
		fn->glGetIntegerv( GL_TEXTURE_BINDING_2D_ARRAY, &prevTex );
		fn->glBindTexture( GL_TEXTURE_2D_ARRAY, s.tex );
		fn->glTexImage3D( GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT16, s.map, s.map, kCascades, 0,
			GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, nullptr );
		fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER );
		fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER );
		const GLfloat border[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		fn->glTexParameterfv( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, border );
		fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE );
		fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL );
		fn->glBindTexture( GL_TEXTURE_2D_ARRAY, GLuint( prevTex ) );
		s.texMap = s.map;
	}
	if ( !s.fbo )
		fn->glGenFramebuffers( 1, &s.fbo );
	if ( !s.tex || !s.fbo ) {
		why = QStringLiteral( "refused(no GL texture/framebuffer)" );
		return false;
	}
	return true;
}

} // namespace

bool wwSunShadowWanted( Scene * scene )
{
	const CsmState & s = cs();
	return s.valid && scene && !scene->selecting && wwLookdevActive();
}

static void sunShadowPassImpl( Scene * scene );

/* WW_CSM_ECHO=<ABSOLUTE path>: the summary of the pass that just ran, rewritten
 * whenever it changes. The PBRM census row is written at a shape's FIRST draw,
 * which is before the render hook's camera pin takes; this file holds the fit
 * of the frame that was grabbed. */
static void csmEchoFrame()
{
	static int armed = -1;
	static QString path, lastWritten;
	if ( armed < 0 ) {
		path = qEnvironmentVariable( "WW_CSM_ECHO" ).trimmed();
		armed = ( !path.isEmpty() && QDir::isAbsolutePath( path ) ) ? 1 : 0;
	}
	if ( armed != 1 )
		return;
	const QString line = QStringLiteral( " " ) + wwSunShadowSummary() + QStringLiteral( "\n" );
	if ( line == lastWritten )
		return;
	QFile f( path );
	if ( f.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
		f.write( line.toUtf8() );
		f.close();
		lastWritten = line;
	}
}

void wwSunShadowPass( Scene * scene )
{
	if ( scene && scene->selecting )
		return;
	sunShadowPassImpl( scene );
	csmEchoFrame();
}

static void sunShadowPassImpl( Scene * scene )
{
	CsmState & s = cs();
	if ( !scene || !scene->renderer )
		return;
	if ( scene->selecting )
		return;	// the pick render: nothing receives, keep the last fit for the echo
	s.valid = false;
	const bool leak = !wwLookdevShadows() && red( "factorhalf" );
	if ( !wwLookdevActive() || ( !wwLookdevShadows() && !leak ) ) {
		s.last = QStringLiteral( "off" );
		return;
	}
	Renderer * r = scene->renderer;
	const auto & P = r->globalUniforms->projectionMatrix;
	if ( P[3][3] == 1.0f ) {
		s.last = QStringLiteral( "refused(orthographic view)" );
		return;
	}
	if ( !scene->hasOption( Scene::DoLighting ) || scene->hasVisMode( Scene::VisSilhouette ) ) {
		s.last = QStringLiteral( "refused(lighting off)" );
		return;
	}

	// the camera: posView = R (sc Q) + t; frustum from the projection (symmetric)
	const Transform & vt = scene->view;
	s.sc = vt.scale != 0.0f ? double( vt.scale ) : 1.0;
	for ( int j = 0; j < 3; j++ ) {
		double c = 0;
		for ( int k = 0; k < 3; k++ )
			c -= vt.rotation( k, j ) * vt.translation[k];
		s.C[j] = c / s.sc;
		s.fwd[j] = -vt.rotation( 2, j );
	}
	s.tanX = 1.0 / double( P[0][0] );
	s.tanY = 1.0 / double( P[1][1] );
	s.camNear = double( P[3][2] ) / ( double( P[2][2] ) - 1.0 ) / s.sc;

	// the light, as it is shaded (spec 2.2 / 2.7)
	float sunDir[3], disc[3];
	wwLookdevShadowLight( sunDir, disc );
	double to[3] = { sunDir[0], sunDir[1], sunDir[2] };
	if ( red( "nofloor" ) ) {
		to[0] = disc[0];
		to[1] = disc[1];
		to[2] = disc[2];
		norm3( to );
	}
	const double sign = red( "flipsun" ) ? 1.0 : -1.0;
	for ( int j = 0; j < 3; j++ )
		s.L[j] = sign * to[j];
	norm3( s.L );
	const double Y[3] = { 0, 1, 0 };
	cross3( s.L, Y, s.right );
	if ( !norm3( s.right ) ) {
		s.last = QStringLiteral( "refused(light along Y)" );
		return;
	}
	cross3( s.right, s.L, s.up );
	norm3( s.up );

	fitCascades( s, vt );

	// the casters
	std::vector<Caster> casters;
	s.tris = 0;
	const bool cpuSkin = scene->hasOption( Scene::DoSkinning );
	for ( Node * node : scene->nodes.list() ) {
		const Shape * sh = dynamic_cast<const Shape *>( node );
		if ( !sh || !sh->isVisible() || !sh->wwCastsSunShadow() )
			continue;
		if ( sh->verts.isEmpty() || sh->triangles.isEmpty() )
			continue;
		Caster c { sh, sh->wwDoubleSided(), {} };
		if ( cpuSkin && sh->wwGpuSkinned() ) {
			c.skinned.resize( size_t( sh->verts.size() ) );
			for ( qsizetype v = 0; v < sh->verts.size(); v++ )
				c.skinned[size_t( v )] = sh->skinVertex( int( v ), sh->verts.at( v ) );
		}
		s.tris += int( sh->triangles.size() );
		casters.push_back( std::move( c ) );
	}
	s.casters = int( casters.size() );
	float gz = 0, gxy[2] = { 0, 0 }, ghalf = 0;
	s.groundCast = wwLookdevGroundFrame( scene, gz, gxy, ghalf );

	QString why;
	if ( !ensureTargets( s, r, why ) ) {
		s.last = why;
		return;
	}
	NifSkopeOpenGLContext::Program * prog = r->useProgram( "lookdev_csmdepth.prog" );
	if ( !prog ) {
		s.last = QStringLiteral( "refused(no lookdev_csmdepth.prog)" );
		return;
	}
	auto fn = r->fn;

	// save what the pass touches
	GLint prevRead = 0, prevDraw = 0, vp[4], polyMode[2], cullMode = 0, frontFace = 0, depthFunc = 0;
	GLboolean depthMask = GL_TRUE;
	GLfloat pf = 0, pu = 0;
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	fn->glGetIntegerv( GL_POLYGON_MODE, polyMode );
	fn->glGetIntegerv( GL_CULL_FACE_MODE, &cullMode );
	fn->glGetIntegerv( GL_FRONT_FACE, &frontFace );
	fn->glGetIntegerv( GL_DEPTH_FUNC, &depthFunc );
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	fn->glGetFloatv( GL_POLYGON_OFFSET_FACTOR, &pf );
	fn->glGetFloatv( GL_POLYGON_OFFSET_UNITS, &pu );
	const bool wasCull = fn->glIsEnabled( GL_CULL_FACE ), wasOffset = fn->glIsEnabled( GL_POLYGON_OFFSET_FILL );
	const bool wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );
	const bool wasBlend = fn->glIsEnabled( GL_BLEND );

	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, s.fbo );
	fn->glDrawBuffer( GL_NONE );
	fn->glReadBuffer( GL_NONE );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glDisable( GL_BLEND );
	fn->glEnable( GL_DEPTH_TEST );
	fn->glDepthFunc( GL_LEQUAL );
	fn->glDepthMask( GL_TRUE );
	fn->glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	fn->glFrontFace( GL_CCW );
	fn->glCullFace( GL_BACK );
	if ( red( "nobias" ) ) {
		fn->glDisable( GL_POLYGON_OFFSET_FILL );
	} else {
		fn->glEnable( GL_POLYGON_OFFSET_FILL );
		fn->glPolygonOffset( kSlopeBias, kUnitBias );
	}

	const int lMvp = prog->uniLocation( "csmClipFromView" );
	bool complete = true;
	for ( int i = 0; i < kCascades && complete; i++ ) {
		const Cascade & c = s.c[i];
		fn->glFramebufferTextureLayer( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, s.tex, 0, i );
		if ( fn->glCheckFramebufferStatus( GL_DRAW_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
			complete = false;
			break;
		}
		fn->glViewport( 0, 0, s.map, s.map );
		fn->glClearDepth( 1.0 );
		fn->glClear( GL_DEPTH_BUFFER_BIT );
		fn->glViewport( 0, 0, c.vw, c.vh );
		if ( lMvp >= 0 )
			fn->glUniformMatrix4fv( lMvp, 1, GL_FALSE, c.clipFromView );
		for ( const Caster & k : casters ) {
			if ( k.twoSided )
				fn->glDisable( GL_CULL_FACE );
			else
				fn->glEnable( GL_CULL_FACE );
			prog->uni4m( "modelViewMatrix", k.sh->viewTrans().toMatrix4() );
			const float * attrs = k.skinned.empty() ? &( k.sh->verts.constFirst()[0] ) : &( k.skinned.front()[0] );
			r->drawShape( (unsigned int) ( k.sh->verts.size() ), 3, (unsigned int) ( k.sh->triangles.size() * 3 ),
				GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, k.sh->triangles.constData() );
		}
		if ( s.groundCast ) {
			// the ground quad, exactly as wwLookdevDrawGround places it (world space, Z up)
			fn->glEnable( GL_CULL_FACE );
			prog->uni4m( "modelViewMatrix", vt.toMatrix4() );
			const float x0 = gxy[0] - ghalf, x1 = gxy[0] + ghalf, y0 = gxy[1] - ghalf, y1 = gxy[1] + ghalf;
			const float quad[12] = { x0, y0, gz, x1, y0, gz, x0, y1, gz, x1, y1, gz };
			static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
			const float * attrs = quad;
			r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
		}
	}
	r->stopProgram();

	// restore
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
	fn->glPolygonMode( GL_FRONT_AND_BACK, GLenum( polyMode[0] ) );
	fn->glCullFace( GLenum( cullMode ) );
	fn->glFrontFace( GLenum( frontFace ) );
	fn->glDepthFunc( GLenum( depthFunc ) );
	fn->glDepthMask( depthMask );
	fn->glPolygonOffset( pf, pu );
	wasCull ? fn->glEnable( GL_CULL_FACE ) : fn->glDisable( GL_CULL_FACE );
	wasOffset ? fn->glEnable( GL_POLYGON_OFFSET_FILL ) : fn->glDisable( GL_POLYGON_OFFSET_FILL );
	wasDepth ? fn->glEnable( GL_DEPTH_TEST ) : fn->glDisable( GL_DEPTH_TEST );
	wasScissor ? fn->glEnable( GL_SCISSOR_TEST ) : fn->glDisable( GL_SCISSOR_TEST );
	wasBlend ? fn->glEnable( GL_BLEND ) : fn->glDisable( GL_BLEND );

	if ( !complete ) {
		s.last = QStringLiteral( "refused(shadow framebuffer incomplete)" );
		return;
	}
	s.valid = true;
	s.last = leak ? QStringLiteral( "on(leak)" ) : QStringLiteral( "on" );
}

void wwSunShadowUniforms( Scene * scene )
{
	if ( !scene || !scene->renderer )
		return;
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->getCurrentProgram();
	if ( !prog || prog->uniLocation( "csmMap" ) < 0 )
		return;
	const CsmState & s = cs();
	auto fn = r->fn;
	GLint prevActive = 0;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kTextureUnit ) );
	fn->glBindTexture( GL_TEXTURE_2D_ARRAY, s.valid ? s.tex : 0 );
	fn->glActiveTexture( GLenum( prevActive ) );
	prog->uni1i( "csmMap", kTextureUnit );
	for ( int i = 0; i < kCascades; i++ ) {
		const int l = prog->uniLocation( "csmMat[%d]", i );
		if ( l >= 0 )
			fn->glUniformMatrix4fv( l, 1, GL_FALSE, s.c[i].uvdFromView );
	}
	const bool nobias = red( "nobias" );
	// red "bigbias": a 600-unit receiver offset, the peter-panning control (the shadow must detach)
	const float offA = nobias ? 0.0f : red( "bigbias" ) ? 600.0f : kOffsetA;
	const float offB = nobias ? 0.0f : red( "bigbias" ) ? 600.0f : kOffsetB;
	prog->uni3f_l( prog->uniLocation( "csmInvRange" ), float( 1.0 / ( s.c[0].far - kNear ) ),
		float( 1.0 / ( s.c[1].far - kNear ) ), float( 1.0 / ( s.c[2].far - kNear ) ) );
	prog->uni4f( "csmParams", FloatVector4( float( 1.0 / s.sc ), float( s.D ), float( s.map ), float( kBlend ) ) );
	prog->uni4f( "csmSplitOffset", FloatVector4( float( s.split[0] ), float( s.split[1] ), offA, offB ) );
	prog->uni1i( "csmProbe", s.probe );
	prog->uni1i( "csmRed", redBits() );
}

QString wwSunShadowSummary()
{
	const CsmState & s = cs();
	if ( !s.valid )
		return QStringLiteral( "csm=%1" ).arg( s.last );
	auto g = []( double v ) { return QString::number( v, 'g', 12 ); };
	auto v3 = [&g]( const double * v ) { return g( v[0] ) + "," + g( v[1] ) + "," + g( v[2] ); };
	QString o = QString( "csm=%1 map=%2 D=%3 split=%4,%5 L=%6 right=%7 up=%8 cam=%9" )
		.arg( s.last ).arg( s.map ).arg( g( s.D ), g( s.split[0] ), g( s.split[1] ), v3( s.L ), v3( s.right ), v3( s.up ) )
		.arg( v3( s.C ) + "|" + v3( s.fwd ) + "|" + g( s.tanX ) + "," + g( s.tanY ) + "|" + g( s.camNear ) + "|" + g( s.sc ) );
	for ( int i = 0; i < kCascades; i++ ) {
		const Cascade & c = s.c[i];
		o += QString( " c%1=%2,%3,%4,%5,%6,%7,%8,%9" ).arg( i ).arg( g( c.zn ), g( c.zf ), g( c.texel ) ).arg( c.n ).arg( c.vw ).arg( c.vh )
			.arg( v3( c.pb ), g( c.l ) + "," + g( c.b ) + "," + g( c.far ) );
	}
	// the camera's right/up rows, and the exact floats handed to glUniformMatrix4fv (column-major),
	// so the judge can apply the UPLOADED receiver matrix to its own world points
	o += QString( " camrows=%1|%2" ).arg( v3( s.rc ), v3( s.uc ) );
	for ( int i = 0; i < kCascades; i++ ) {
		QStringList m;
		for ( int k = 0; k < 16; k++ )
			m << QString::number( double( s.c[i].uvdFromView[k] ), 'g', 9 );
		o += QString( " m%1=%2" ).arg( i ).arg( m.join( QChar( ',' ) ) );
	}
	o += QString( " casters=%1 tris=%2 ground=%3 probe=%4 red=%5" ).arg( s.casters ).arg( s.tris ).arg( s.groundCast ? 1 : 0 )
		.arg( s.probe ).arg( s.reds.isEmpty() ? QStringLiteral( "none" ) : s.reds.join( QChar( '+' ) ) );
	return o;
}

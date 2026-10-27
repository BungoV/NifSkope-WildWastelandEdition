#include "cloudvol.h"
#include "cloudmap.h"
#include "cloudnoise.h"
#include "cloudvolmath.h"
#include "cloudfly.h"
#include "atmosphere.h"
#include "atmomath.h"
#include "cellhdr.h"
#include "wwswitch.h"
#include "gl/glscene.h"
#include "gl/renderer.h"
#include "harnesswindow.h"

#include <QHash>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

// lane CLOUD2: the volume clouds (cloudvol.h). The march, resolve and composite are res/shaders/cloud_*.frag.

namespace
{

using WwAtmoMath::kUnitKm;
namespace V = WwCloudVolMath;

constexpr const char * kKey = "Settings/Render/Scene/Volume Clouds";
constexpr GLenum kTimestamp = 0x8E28;	// GL_TIMESTAMP

struct Preset
{
	int div;	// history = viewport / div per axis
	int steps, taps;
	float lightKm, maxKm;
};

// Med 1/4 res 48 steps 3 taps; High 64 / 6; Ultra 1/2 res 96 / 6 (BRIEF_CLOUD2)
const Preset & preset( int tier )
{
	static const Preset p[5] = { { 4, 48, 3, 1.0f, 60.0f }, { 4, 48, 3, 1.0f, 60.0f }, { 4, 48, 3, 1.0f, 60.0f },
		{ 4, 64, 6, 1.2f, 80.0f }, { 2, 96, 6, 1.2f, 100.0f } };
	return p[std::clamp( tier, 0, 4 )];
}

const char * tierName( int t )
{
	static const char * n[5] = { "off", "low", "medium", "high", "ultra" };
	return n[std::clamp( t, 0, 4 )];
}

struct CvState
{
	bool init = false;
	quint64 liveGen = 0;
	int tier = 0;
	bool pinned = false;
	QString last;
	// Low: the colour the 2D layers take this frame (valid while lowFrame == the sky's frame)
	float low[3] = { 1, 1, 1 };
	bool lowOk = false;
	unsigned long long lowFrame = ~0ull;
	// the sky irradiance memo (atmomath skyIrradiance costs ~1 ms)
	int irrKey[4] = { -999, 0, 0, 0 };
	float irr[3] = { 0, 0, 0 };
	double drawnSecs = 0.0;	// lane CLOUD2C: the clock of the last draw (the bind hands readers its wind)
};

CvState & cs()
{
	static CvState s;
	if ( !s.init || wwSwitchMovedFor( s.liveGen, { "WW_CLOUDVOL" } ) ) {
		s.init = true;
		s.liveGen = wwSwitchGen();
		s.pinned = false;
		const QString e = wwEnvStr( "WW_CLOUDVOL" ).trimmed().toLower();
		if ( !e.isEmpty() ) {
			bool ok = false;
			const int v = e.toInt( &ok );
			s.tier = ok ? std::clamp( v, 0, 4 )
				: e == QLatin1StringView( "low" ) ? 1 : e == QLatin1StringView( "medium" ) ? 2
				: e == QLatin1StringView( "high" ) ? 3 : e == QLatin1StringView( "ultra" ) ? 4 : 0;
			s.pinned = true;
		} else if ( wwHarnessRun() ) {
			s.tier = 0;	// a harness measures what it pins
			s.pinned = true;
		} else {
			s.tier = std::clamp( QSettings().value( QLatin1StringView( kKey ), 0 ).toInt(), 0, 4 );
		}
	}
	return s;
}

bool red( const char * word )
{
	const QString r = wwEnvStr( "WW_CLOUDVOL_RED" ).trimmed().toLower();
	for ( const QString & w : r.split( QChar( ',' ), Qt::SkipEmptyParts ) )
		if ( w.trimmed() == QLatin1StringView( word ) )
			return true;
	return false;
}

struct Tgt	// a colour (RGBA16F) + depth (R32F) pair in one FBO
{
	GLuint fbo = 0, col = 0, dep = 0;
	int w = 0, h = 0;
};

struct CvGpu
{
	GLuint shape = 0, detail = 0;
	Tgt march, hist[2];
	int cur = 0;
	bool histOk = false;
	unsigned long long frame = 0;
	float prevCam[9] = {}, prevProj[4] = {}, prevKm[3] = {};
	GLuint q[4] = { 0, 0, 0, 0 };
	bool pending[2] = { false, false };
	int qcur = 0;
	double ms = -1.0;
	int frames = 0;
	// the calibration memo
	int calKey[2][4] = { { -1, 0, 0, 0 }, { -1, 0, 0, 0 } };
	bool calRed[2] = { false, false };
	float cal[2][V::kCalN] = {};
	// lane CLOUD2B: the scene's depth (wwCloudVolSceneDepth, before the tone map) and what the pass over the scene needs
	bool drewNow = false;	// the volume drew in this frame's backdrop
	GLuint sceneTex = 0;	// the linear frame's farthest window depth (cellhdr.cpp owns it)
	int sceneW = 0, sceneH = 0;
	unsigned long long sceneFrame = ~0ull;	// g.frame at the grab
	float sceneZ[3] = {};	// projection [2][2], [3][2]; km per view unit
	int frontHist = 0;
	float frontProj[4] = {}, frontExposure = 1.0f;
	int frontVt = 0;
	bool redo = false;	// lane CLOUD2C: this frame's march had no depth of this frame: run it again after the scene
};

QHash<Renderer *, CvGpu> & gpus()
{
	static QHash<Renderer *, CvGpu> g;
	return g;
}

//! the two noise volumes, built once a process (all cores, ~0.3 s), shared by every context's upload
const std::vector<std::uint8_t> & noise( bool detail )
{
	static const std::vector<std::uint8_t> s = WwCloudNoise::shapeVolume();
	static const std::vector<std::uint8_t> d = WwCloudNoise::detailVolume();
	return detail ? d : s;
}

GLuint upload3d( NifSkopeOpenGLContext::GLFunctions * fn, const std::vector<std::uint8_t> & v, int n )
{
	GLuint t = 0;
	fn->glGenTextures( 1, &t );
	fn->glBindTexture( GL_TEXTURE_3D, t );
	fn->glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	fn->glTexImage3D( GL_TEXTURE_3D, 0, GL_R8, n, n, n, 0, GL_RED, GL_UNSIGNED_BYTE, v.data() );
	fn->glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
	fn->glGenerateMipmap( GL_TEXTURE_3D );
	fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	for ( GLenum w : { GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T, GL_TEXTURE_WRAP_R } )
		fn->glTexParameteri( GL_TEXTURE_3D, w, GL_REPEAT );
	fn->glBindTexture( GL_TEXTURE_3D, 0 );
	return t;
}

bool ensure( NifSkopeOpenGLContext::GLFunctions * fn, Tgt & t, int w, int h )
{
	if ( t.fbo && t.w == w && t.h == h )
		return true;
	if ( !t.fbo )
		fn->glGenFramebuffers( 1, &t.fbo );
	auto make = [&]( GLuint & tex, GLenum fmt, GLenum chans ) {
		if ( tex )
			fn->glDeleteTextures( 1, &tex );
		fn->glGenTextures( 1, &tex );
		fn->glBindTexture( GL_TEXTURE_2D, tex );
		fn->glTexImage2D( GL_TEXTURE_2D, 0, GLint( fmt ), w, h, 0, chans, GL_FLOAT, nullptr );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	};
	make( t.col, GL_RGBA16F, GL_RGBA );
	make( t.dep, GL_R32F, GL_RED );
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	fn->glBindFramebuffer( GL_FRAMEBUFFER, t.fbo );
	fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.col, 0 );
	fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, t.dep, 0 );
	const GLenum two[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	fn->glDrawBuffers( 2, two );
	t.w = w;
	t.h = h;
	return fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE;
}

void quad( Renderer * r )
{
	static const float q[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = q;
	r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
}

void timeRead( NifSkopeOpenGLContext::GLFunctions * fn, CvGpu & g, int slot, bool wait )
{
	if ( !g.pending[slot] )
		return;
	if ( !wait ) {
		GLuint avail = 0;
		fn->glGetQueryObjectuiv( g.q[slot * 2 + 1], GL_QUERY_RESULT_AVAILABLE, &avail );
		if ( !avail )
			return;
	}
	GLuint64 a = 0, b = 0;
	fn->glGetQueryObjectui64v( g.q[slot * 2], GL_QUERY_RESULT, &a );
	fn->glGetQueryObjectui64v( g.q[slot * 2 + 1], GL_QUERY_RESULT, &b );
	g.ms = b > a ? double( b - a ) * 1e-6 : 0.0;
	g.pending[slot] = false;
	if ( ++g.frames % 120 == 1 )
		fprintf( stderr, "cloudvol: gpu %.4f ms\n", g.ms );
}

//! the map's bands in km (z above world z = 0), shared by the march and wwCloudVolBindDensity
struct Bands
{
	float baseKm[3], topKm[3], density[3], coverage[3], sheet;
	float rect[4];
	bool mapBound;
	float tint[3][3];	// lane CLOUD2B: CLOUD1's band tints (the march only; not in cloud_common's struct)
};

Bands bandsOf( const WwCloudMap & m )
{
	Bands b{};
	for ( int i = 0; i < 3; i++ ) {
		b.baseKm[i] = m.band[i].baseZ * kUnitKm;
		b.topKm[i] = std::max( m.band[i].topZ * kUnitKm, b.baseKm[i] + 1e-3f );
		b.density[i] = m.band[i].layers > 0 ? m.band[i].density : 0.0f;
		b.coverage[i] = m.band[i].coverage;
		for ( int c = 0; c < 3; c++ )
			b.tint[i][c] = red( "notint" ) ? 1.0f : m.band[i].tint[c];
	}
	b.sheet = m.band[0].sheet;
	const float he = std::max( m.halfExtent * kUnitKm, 1e-3f );
	b.rect[0] = m.centerX * kUnitKm - he;
	b.rect[1] = m.centerY * kUnitKm - he;
	b.rect[2] = 1.0f / ( 2.0f * he );
	b.mapBound = m.tex != 0 && !m.stub;
	b.rect[3] = b.mapBound ? 1.0f : 0.0f;
	return b;
}

//! the coverage tables of the Low and Mid bands for these heights, memoised (~10 ms a band on a change)
void calibrated( CvGpu & g, const Bands & b, float out[2 * V::kCalN] )
{
	const bool noCal = red( "nocal" );
	for ( int band = 0; band < 2; band++ ) {
		const int key[4] = { int( std::lround( b.baseKm[band] * 50.0f ) ), int( std::lround( b.topKm[band] * 50.0f ) ),
			int( std::lround( b.density[band] * 100.0f ) ), int( std::lround( ( band == 0 ? b.sheet : 0.0f ) * 32.0f ) ) };
		if ( std::memcmp( key, g.calKey[band], sizeof( key ) ) || g.calRed[band] != noCal ) {
			std::memcpy( g.calKey[band], key, sizeof( key ) );
			g.calRed[band] = noCal;
			V::calibrate( noise( false ).data(), WwCloudNoise::kShapeN, noise( true ).data(), WwCloudNoise::kDetailN, band,
				b.baseKm[band], b.topKm[band], b.density[band], band == 0 ? b.sheet : 0.0f, noCal, g.cal[band] );
		}
		std::copy( g.cal[band], g.cal[band] + V::kCalN, out + band * V::kCalN );
	}
}

//! cloud_common.glsl's uniforms and samplers on the current program
void densityUniforms( Renderer * r, NifSkopeOpenGLContext::Program * p, CvGpu & g, const Bands & b, const WwCloudMap & m,
	float windKm, int unit )
{
	auto fn = r->fn;
	p->uni3f( "cvBands.baseKm", b.baseKm[0], b.baseKm[1], b.baseKm[2] );
	p->uni3f( "cvBands.topKm", b.topKm[0], b.topKm[1], b.topKm[2] );
	p->uni3f( "cvBands.density", b.density[0], b.density[1], b.density[2] );
	p->uni3f( "cvBands.coverage", b.coverage[0], b.coverage[1], b.coverage[2] );
	p->uni1f( "cvBands.sheet", b.sheet );
	p->uni4f( "cvBands.mapRect", FloatVector4( b.rect[0], b.rect[1], b.rect[2], b.rect[3] ) );
	p->uni3f( "cvBands.windKm", windKm, 0.0f, 0.0f );
	float cal[2 * V::kCalN];
	calibrated( g, b, cal );
	p->uni1fv( "cvCal", cal, 2 * V::kCalN );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
	fn->glBindTexture( GL_TEXTURE_3D, g.shape );
	p->uni1i( "cvShape", unit );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit + 1 ) );
	fn->glBindTexture( GL_TEXTURE_3D, g.detail );
	p->uni1i( "cvDetail", unit + 1 );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit + 2 ) );
	fn->glBindTexture( GL_TEXTURE_2D, b.mapBound ? m.tex : 0 );
	p->uni1i( "cvMap", unit + 2 );
}

bool ensureNoise( Renderer * r, CvGpu & g )
{
	if ( !g.shape )
		g.shape = upload3d( r->fn, noise( false ), WwCloudNoise::kShapeN );
	if ( !g.detail )
		g.detail = upload3d( r->fn, noise( true ), WwCloudNoise::kDetailN );
	return g.shape && g.detail;
}

//! the camera this frame: world columns (right, up, back), the projection's terms, the eye in km
struct Cam
{
	float m[9];	// column-major mat3: d_world = M x d_view
	float proj[4];
	float km[3];
};

Cam cameraOf( Scene * scene )
{
	Cam c{};
	const auto & vm = scene->renderer->globalUniforms->viewMatrix;	// vm[column][row], world to view
	for ( int col = 0; col < 3; col++ )
		for ( int row = 0; row < 3; row++ )
			c.m[col * 3 + row] = vm[row][col];	// the transpose: view to world
	const auto & pm = scene->renderer->globalUniforms->projectionMatrix;	// pm[column][row]
	c.proj[0] = pm[0][0];
	c.proj[1] = pm[1][1];
	c.proj[2] = pm[2][0];
	c.proj[3] = pm[2][1];
	const Transform & vt = scene->view;
	const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
	float eye[3] = { 0, 0, 0 };
	for ( int i = 0; i < 3; i++ )
		for ( int j = 0; j < 3; j++ )
			eye[i] -= vt.rotation( j, i ) * vt.translation[j] / sc;
	for ( int i = 0; i < 3; i++ )
		c.km[i] = eye[i] * kUnitKm;
	return c;
}

void setMat3( Renderer * r, NifSkopeOpenGLContext::Program * p, const char * name, const float m[9] )
{
	const int l = p->uniLocation( name );
	if ( l >= 0 )
		r->fn->glUniformMatrix3fv( l, 1, GL_FALSE, m );
}

//! the sky's mean radiance over the upper hemisphere per unit key light (E / pi), memoised by the key's elevation
void skyAmbient( CvState & s, float mus, const float haze[3], float out[3] )
{
	const int key[4] = { int( std::lround( mus * 256.0f ) ), int( std::lround( haze[0] * 1e4f ) ),
		int( std::lround( haze[1] * 1e3f ) ), int( std::lround( haze[2] * 1e3f ) ) };
	if ( std::memcmp( key, s.irrKey, sizeof( key ) ) ) {
		std::memcpy( s.irrKey, key, sizeof( key ) );
		WwAtmoMath::skyIrradiance( std::clamp( mus, -0.3f, 1.0f ), haze, s.irr );
	}
	for ( int c = 0; c < 3; c++ )
		out[c] = s.irr[c] / WwAtmoMath::kPi;
}

//! the key light: the sun while it is above -0.1 (twilight), else the moon when it is up
bool keyLight( const WwAtmoFrame & f, float w[3], float & k )
{
	if ( f.sunW[2] > -0.1f || f.moonK <= 0.0f ) {
		std::copy( f.sunW, f.sunW + 3, w );
		k = f.sunK;
	} else {
		std::copy( f.moonW, f.moonW + 3, w );
		k = f.moonK;
	}
	return k > 0.0f;
}

//! lane CLOUD2C: one frame's march + resolve inputs, kept so the pair can run again once this frame's scene depth exists
struct CvRedo
{
	bool ok = false;
	Bands b{};
	Cam cam{};
	float keyW[3] = {}, keyK = 0.0f, amb[3] = {};
	int tier = 0, hw = 1, hh = 1, mw = 1, mh = 1, stride = 1, cur = 0, prev = 1;
	float phase[2] = {}, frameNo = 0.0f;
	double secs = 0.0;
	bool histOn = false;
	float prevCam[9] = {}, prevProj[4] = {}, prevKm[3] = {};
};

QHash<Renderer *, CvRedo> & redos()
{
	static QHash<Renderer *, CvRedo> d;
	return d;
}

//! the march: one fragment a history pixel of this frame's phase, ended at the scene when sceneTex is set (g.sceneZ)
bool marchPass( Renderer * r, CvGpu & g, const CvRedo & d, const WwAtmoFrame & f, const WwCloudMap & m, GLuint sceneTex )
{
	auto fn = r->fn;
	const bool fly = wwCloudFlyActive( r );	// lane CLOUD4: the march with the fly-through box
	NifSkopeOpenGLContext::Program * p = r->useProgram( fly ? "cloud_fly.prog" : "cloud_march.prog" );
	if ( !p )
		return false;
	const Preset & pr = preset( d.tier );
	const bool noOct = red( "nooctaves" ), noMs = noOct || red( "noms" );
	const V::Droplet drop = V::dropletParams( V::kDropletUm );
	fn->glBindFramebuffer( GL_FRAMEBUFFER, g.march.fbo );
	fn->glViewport( 0, 0, d.mw, d.mh );
	densityUniforms( r, p, g, d.b, m, V::windKm( d.secs ), 0 );
	fn->glActiveTexture( GL_TEXTURE3 );
	fn->glBindTexture( GL_TEXTURE_2D, f.trans );
	p->uni1i( "cvTrans", 3 );
	fn->glActiveTexture( GL_TEXTURE4 );
	fn->glBindTexture( GL_TEXTURE_2D, f.sunView );
	p->uni1i( "cvSunView", 4 );
	fn->glActiveTexture( GL_TEXTURE5 );
	fn->glBindTexture( GL_TEXTURE_2D, f.moonView );
	p->uni1i( "cvMoonView", 5 );
	p->uni2f( "cvViewSize", float( f.viewW ), float( f.viewH ) );
	p->uni1f( "cvCamR", f.camR );
	p->uni3f( "cvSunW", f.sunW[0], f.sunW[1], f.sunW[2] );
	p->uni3f( "cvMoonW", f.moonW[0], f.moonW[1], f.moonW[2] );
	p->uni1f( "cvSunK", f.sunK );
	p->uni1f( "cvMoonK", f.moonK );
	p->uni3f( "cvKeyW", d.keyW[0], d.keyW[1], d.keyW[2] );
	p->uni1f( "cvKeyK", d.keyK );
	setMat3( r, p, "cvCam", d.cam.m );
	p->uni4f( "cvProj", FloatVector4( d.cam.proj[0], d.cam.proj[1], d.cam.proj[2], d.cam.proj[3] ) );
	p->uni3f( "cvCamKm", d.cam.km[0], d.cam.km[1], d.cam.km[2] );
	p->uni2f( "cvHistSize", float( d.hw ), float( d.hh ) );
	p->uni1f( "cvStride", float( d.stride ) );
	p->uni2f( "cvPhase", d.phase[0], d.phase[1] );
	p->uni1i( "cvSteps", pr.steps );
	p->uni1i( "cvTaps", pr.taps );
	p->uni1i( "cvOctaves", noOct ? 1 : 4 );
	p->uni1f( "cvLightKm", pr.lightKm );
	p->uni4f( "cvPhasePrm", FloatVector4( drop.gHg, drop.gD, drop.alpha, drop.wD ) );
	p->uni3f( "cvAmb", d.amb[0], d.amb[1], d.amb[2] );
	p->uni1f( "cvAmbOn", 1.0f );
	p->uni1f( "cvApOn", 1.0f );
	p->uni1f( "cvFrame", d.frameNo );
	p->uni1f( "cvMaxKm", pr.maxKm );
	p->uni1f( "cvSlab", 0.0f );
	p->uni1i( "cvProbe", 0 );
	p->uni1f( "cvMsOn", noMs ? 0.0f : 1.0f );
	p->uni1f( "cvMsG", V::msMeanCosine( drop ) );
	p->uni1f( "cvTopOn", red( "notop" ) ? 0.0f : 1.0f );	// lane CLOUD2C: the light the cloud below reflects up
	// lane CLOUDLOOK1: literal names (the location cache keys on the name's address; a heap name per loop set all three
	// through cvTint[0]). Red tintname: the old heap names.
	static const char * const kTint[3] = { "cvTint[0]", "cvTint[1]", "cvTint[2]" };
	for ( int i = 0; i < 3; i++ ) {
		if ( red( "tintname" ) ) {
			const QByteArray n = QByteArrayLiteral( "cvTint[" ) + QByteArray::number( i ) + ']';
			p->uni3f( n.constData(), d.b.tint[i][0], d.b.tint[i][1], d.b.tint[i][2] );	// uniname-red
		} else {
			p->uni3f( kTint[i], d.b.tint[i][0], d.b.tint[i][1], d.b.tint[i][2] );
		}
	}
	fn->glActiveTexture( GL_TEXTURE6 );
	fn->glBindTexture( GL_TEXTURE_2D, sceneTex );
	p->uni1i( "cvScene", 6 );
	p->uni4f( "cvSceneZ", FloatVector4( g.sceneZ[0], g.sceneZ[1], g.sceneZ[2], sceneTex ? 1.0f : 0.0f ) );
	if ( fly )
		wwCloudFlyUniforms( r, p, 7 );
	quad( r );
	r->stopProgram();
	if ( fly )
		wwCloudFlyRelease( r, 7 );
	return true;
}

//! the resolve into this frame's history (it reads only the other one, so running it twice gives the same history)
bool resolvePass( Renderer * r, CvGpu & g, const CvRedo & d )
{
	auto fn = r->fn;
	NifSkopeOpenGLContext::Program * p = r->useProgram( "cloud_resolve.prog" );
	if ( !p )
		return false;
	fn->glBindFramebuffer( GL_FRAMEBUFFER, g.hist[d.cur].fbo );
	fn->glViewport( 0, 0, d.hw, d.hh );
	const GLuint tex[4] = { g.march.col, g.march.dep, g.hist[d.prev].col, g.hist[d.prev].dep };
	for ( int u = 0; u < 4; u++ ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		fn->glBindTexture( GL_TEXTURE_2D, tex[u] );
	}
	p->uni1i( "cvMarch", 0 );
	p->uni1i( "cvMarchDepth", 1 );
	p->uni1i( "cvPrev", 2 );
	p->uni1i( "cvPrevDepth", 3 );
	p->uni2f( "cvHistSize", float( d.hw ), float( d.hh ) );
	p->uni1f( "cvStride", float( d.stride ) );
	p->uni2f( "cvPhase", d.phase[0], d.phase[1] );
	setMat3( r, p, "cvCam", d.cam.m );
	p->uni4f( "cvProj", FloatVector4( d.cam.proj[0], d.cam.proj[1], d.cam.proj[2], d.cam.proj[3] ) );
	p->uni3f( "cvCamKm", d.cam.km[0], d.cam.km[1], d.cam.km[2] );
	setMat3( r, p, "cvPrevCam", d.prevCam );
	p->uni4f( "cvPrevProj", FloatVector4( d.prevProj[0], d.prevProj[1], d.prevProj[2], d.prevProj[3] ) );
	p->uni3f( "cvPrevCamKm", d.prevKm[0], d.prevKm[1], d.prevKm[2] );
	p->uni1f( "cvHistOn", d.histOn ? 1.0f : 0.0f );
	p->uni1f( "cvReproj", red( "noreproj" ) ? 0.0f : 1.0f );
	quad( r );
	r->stopProgram();
	return true;
}

//! lane CLOUD2C: the GL state the passes after the scene change, put back as found when it goes out of scope
struct GlKeep
{
	NifSkopeOpenGLContext::GLFunctions * fn;
	GLint draw = 0, read = 0, active = 0, vp[4] = { 0, 0, 1, 1 };
	GLint bf[4] = { GL_ONE, GL_ZERO, GL_ONE, GL_ZERO };
	GLint sf = GL_ALWAYS, sref = 0, smask = ~0, swrite = ~0, sop[3] = { GL_KEEP, GL_KEEP, GL_KEEP };
	bool depth, blend, cull, scissor, stencil;
	GLboolean depthMask = GL_TRUE;

	explicit GlKeep( NifSkopeOpenGLContext::GLFunctions * f ) : fn( f )
	{
		fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &draw );
		fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &read );
		fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &active );
		fn->glGetIntegerv( GL_VIEWPORT, vp );
		fn->glGetIntegerv( GL_BLEND_SRC_RGB, &bf[0] );
		fn->glGetIntegerv( GL_BLEND_DST_RGB, &bf[1] );
		fn->glGetIntegerv( GL_BLEND_SRC_ALPHA, &bf[2] );
		fn->glGetIntegerv( GL_BLEND_DST_ALPHA, &bf[3] );
		fn->glGetIntegerv( GL_STENCIL_FUNC, &sf );
		fn->glGetIntegerv( GL_STENCIL_REF, &sref );
		fn->glGetIntegerv( GL_STENCIL_VALUE_MASK, &smask );
		fn->glGetIntegerv( GL_STENCIL_WRITEMASK, &swrite );
		fn->glGetIntegerv( GL_STENCIL_FAIL, &sop[0] );
		fn->glGetIntegerv( GL_STENCIL_PASS_DEPTH_FAIL, &sop[1] );
		fn->glGetIntegerv( GL_STENCIL_PASS_DEPTH_PASS, &sop[2] );
		depth = fn->glIsEnabled( GL_DEPTH_TEST );
		blend = fn->glIsEnabled( GL_BLEND );
		cull = fn->glIsEnabled( GL_CULL_FACE );
		scissor = fn->glIsEnabled( GL_SCISSOR_TEST );
		stencil = fn->glIsEnabled( GL_STENCIL_TEST );
		fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
		fn->glDisable( GL_DEPTH_TEST );
		fn->glDisable( GL_BLEND );
		fn->glDisable( GL_CULL_FACE );
		fn->glDisable( GL_SCISSOR_TEST );
		fn->glDisable( GL_STENCIL_TEST );
		fn->glDepthMask( GL_FALSE );
	}
	~GlKeep()
	{
		for ( int u = 6; u >= 0; u-- ) {
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
			fn->glBindTexture( GL_TEXTURE_2D, 0 );
			fn->glBindTexture( GL_TEXTURE_3D, 0 );
		}
		fn->glActiveTexture( GLenum( active ) );
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( draw ) );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( read ) );
		fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
		fn->glBlendFuncSeparate( GLenum( bf[0] ), GLenum( bf[1] ), GLenum( bf[2] ), GLenum( bf[3] ) );
		fn->glStencilFunc( GLenum( sf ), sref, GLuint( smask ) );
		fn->glStencilMask( GLuint( swrite ) );
		fn->glStencilOp( GLenum( sop[0] ), GLenum( sop[1] ), GLenum( sop[2] ) );
		auto set = [this]( GLenum cap, bool on ) { if ( on ) fn->glEnable( cap ); else fn->glDisable( cap ); };
		set( GL_DEPTH_TEST, depth );
		set( GL_BLEND, blend );
		set( GL_CULL_FACE, cull );
		set( GL_SCISSOR_TEST, scissor );
		set( GL_STENCIL_TEST, stencil );
		fn->glDepthMask( depthMask );
	}
};

//! lane CLOUD2C: the march + resolve of this frame again, ended at this frame's scene depth (g.sceneTex)
void redoWithDepth( Renderer * r, CvGpu & g )
{
	if ( !g.redo || !g.sceneTex )
		return;
	g.redo = false;
	const auto it = redos().constFind( r );
	const WwAtmoFrame & f = wwAtmoFrame();
	const WwCloudMap & m = wwCloudMap();
	if ( it == redos().constEnd() || !it->ok || !f.valid || !m.valid )
		return;
	GlKeep keep( r->fn );
	if ( marchPass( r, g, *it, f, m, g.sceneTex ) )
		resolvePass( r, g, *it );
}

//! the history over the scene where the clouds' mean depth is in front of it (cloud_composite.frag cvFront): mode 1 in
//! display space (exposure, view transform; blend SRC_ALPHA), mode 2 linear light into the cell frame's stencil-1 pixels
void frontPass( Renderer * r, CvGpu & g, int mode )
{
	NifSkopeOpenGLContext::Program * p = r->useProgram( "cloud_composite.prog" );
	if ( !p )
		return;
	auto fn = r->fn;
	GlKeep keep( fn );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( keep.draw ) );
	const GLuint tex[3] = { g.hist[g.frontHist].col, g.hist[g.frontHist].dep, g.sceneTex };
	for ( int u = 0; u < 3; u++ ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		fn->glBindTexture( GL_TEXTURE_2D, tex[u] );
	}
	p->uni1i( "cvHist", 0 );
	p->uni1i( "cvHistDepth", 1 );
	p->uni1i( "cvScene", 2 );
	p->uni1i( "cvFront", mode );
	p->uni4f( "cvSceneZ", FloatVector4( g.sceneZ[0], g.sceneZ[1], g.sceneZ[2], 1.0f ) );
	p->uni4f( "cvProj", FloatVector4( g.frontProj[0], g.frontProj[1], g.frontProj[2], g.frontProj[3] ) );
	p->uni4f( "cvRect", FloatVector4( float( keep.vp[0] ), float( keep.vp[1] ), 1.0f / float( keep.vp[2] ), 1.0f / float( keep.vp[3] ) ) );
	p->uni1f( "sceneExposure", g.frontExposure );
	p->uni1i( "viewTransform", g.frontVt );
	fn->glEnable( GL_BLEND );
	if ( mode == 2 ) {
		// the cell's linear light only (stencil 1, cellhdr.cpp): the tone map takes those pixels; the others pass as written
		fn->glBlendFuncSeparate( GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE );
		fn->glEnable( GL_STENCIL_TEST );
		fn->glStencilMask( 0 );
		fn->glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
		fn->glStencilFunc( GL_EQUAL, 1, 0x03 );
	} else {
		fn->glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE );
	}
	quad( r );
	r->stopProgram();
}

//! the projection terms the depth decode needs (cloud_march.frag cvSceneZ)
void sceneZOf( Scene * scene, CvGpu & g )
{
	const auto & pm = scene->renderer->globalUniforms->projectionMatrix;	// pm[column][row]
	const float sc = scene->view.scale != 0.0f ? scene->view.scale : 1.0f;
	g.sceneZ[0] = pm[2][2];
	g.sceneZ[1] = pm[3][2];
	g.sceneZ[2] = kUnitKm / sc;
}

QString refusal( CvState & s, const QString & why )
{
	s.last = QStringLiteral( "volclouds:refused(%1)" ).arg( why );
	return s.last;
}

} // namespace

int wwCloudVolTier()
{
	return cs().tier;
}

void wwCloudVolSetTier( int tier )
{
	CvState & s = cs();
	if ( s.pinned )
		return;
	s.tier = std::clamp( tier, 0, 4 );
	QSettings().setValue( QLatin1StringView( kKey ), s.tier );
}

bool wwCloudVolPinned()
{
	return cs().pinned;
}

bool wwCloudVolLowColour( float out[3] )
{
	CvState & s = cs();
	if ( s.tier != 1 )
		return false;
	const WwAtmoFrame & f = wwAtmoFrame();
	if ( !f.valid )
		return false;
	if ( s.lowFrame != f.frame ) {
		s.lowFrame = f.frame;
		float w[3], k = 0.0f;
		s.lowOk = keyLight( f, w, k );
		if ( s.lowOk ) {
			const WwCloudMap & m = wwCloudMap();
			const float baseKm = m.valid ? std::max( m.band[0].baseZ * kUnitKm, 0.0f ) : 1.5f;
			float T[3], E[3];
			WwAtmoMath::cpuTrans( baseKm, w[2], f.haze, false, T );
			skyAmbient( s, w[2], f.haze, E );
			for ( float & e : E )
				e *= WwAtmoMath::kPi;	// lowColour takes the irradiance
			V::lowColour( k, T, w[2], k, E, s.low );
			if ( m.valid )
				for ( int c = 0; c < 3; c++ )
					s.low[c] *= red( "notint" ) ? 1.0f : m.band[0].tint[c];	// lane CLOUD2B: CLOUD1's Low band tint
		}
	}
	if ( s.lowOk )
		std::copy( s.low, s.low + 3, out );
	return s.lowOk;
}

bool wwCloudVolBindDensity( Scene * scene, int firstUnit )
{
	if ( !scene || !scene->renderer || cs().tier < 2 )
		return false;
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * p = r->getCurrentProgram();
	const WwCloudMap & m = wwCloudMap();
	if ( !p || !m.valid )
		return false;
	CvGpu & g = gpus()[r];
	if ( !ensureNoise( r, g ) )
		return false;
	// lane CLOUD2C: the wind the last draw used, so a reader's clouds (CLOUD3's shadows) move with the drawn ones
	densityUniforms( r, p, g, bandsOf( m ), m, V::readerWindKm( cs().drawnSecs, red( "wind0" ) ), firstUnit );
	r->fn->glActiveTexture( GL_TEXTURE0 );
	return true;
}

bool wwCloudVolBindDensityAt( Scene * scene, int firstUnit, double secs )
{
	if ( !scene || !scene->renderer || cs().tier < 2 )
		return false;
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * p = r->getCurrentProgram();
	const WwCloudMap & m = wwCloudMap();
	if ( !p || !m.valid )
		return false;
	CvGpu & g = gpus()[r];
	if ( !ensureNoise( r, g ) )
		return false;
	densityUniforms( r, p, g, bandsOf( m ), m, V::windKm( secs ), firstUnit );
	r->fn->glActiveTexture( GL_TEXTURE0 );
	return true;
}

bool wwCloudVolDraw( Scene * scene, const WwCloudVolView & view, QStringList & drew )
{
	CvState & s = cs();
	if ( s.tier <= 0 )
		return false;
	if ( s.tier == 1 ) {
		float c[3];
		drew << ( wwCloudVolLowColour( c )
			? QStringLiteral( "volclouds:low(2d layers lit by the model, %1 %2 %3)" ).arg( double( c[0] ), 0, 'g', 4 )
				.arg( double( c[1] ), 0, 'g', 4 ).arg( double( c[2] ), 0, 'g', 4 )
			: QStringLiteral( "volclouds:low(no physical sky: the weather's colour)" ) );
		return false;
	}
	if ( view.capture )
		return false;	// the probes' capture keeps the 2D layers (the history is the view's)
	if ( !scene || !scene->renderer ) {
		drew << refusal( s, QStringLiteral( "no renderer" ) );
		return false;
	}
	const WwAtmoFrame & f = wwAtmoFrame();
	if ( !f.valid ) {
		drew << refusal( s, QStringLiteral( "Physical Sky off" ) );
		return false;
	}
	const WwCloudMap & m = wwCloudMapUpdate( scene );
	if ( !m.valid ) {
		drew << refusal( s, QStringLiteral( "Cloud Map off or no weather" ) );
		return false;
	}
	float keyW[3], keyK = 0.0f;
	keyLight( f, keyW, keyK );

	Renderer * r = scene->renderer;
	auto fn = r->fn;
	CvGpu & g = gpus()[r];
	if ( !ensureNoise( r, g ) ) {
		drew << refusal( s, QStringLiteral( "noise upload" ) );
		return false;
	}
	GLint vp[4] = { 0, 0, 1, 1 };
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	const Preset & pr = preset( s.tier );
	// a harness run marches every pixel (stride 1, phase 0, no jitter): the same inputs give the same picture
	const bool everyPixel = wwHarnessRun() || red( "everypixel" );
	const int stride = everyPixel ? 1 : 4;
	const int hw = std::max( 1, ( vp[2] + pr.div - 1 ) / pr.div ), hh = std::max( 1, ( vp[3] + pr.div - 1 ) / pr.div );
	const int mw = ( hw + stride - 1 ) / stride, mh = ( hh + stride - 1 ) / stride;

	GLint prevDraw = 0, prevRead = 0, prevActive = 0;
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	const bool wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasBlend = fn->glIsEnabled( GL_BLEND );
	const bool wasCull = fn->glIsEnabled( GL_CULL_FACE ), wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );
	const bool wasStencil = fn->glIsEnabled( GL_STENCIL_TEST );
	GLboolean depthMask = GL_TRUE;
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	fn->glDisable( GL_DEPTH_TEST );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glDepthMask( GL_FALSE );

	if ( !g.q[0] )
		fn->glGenQueries( 4, g.q );
	timeRead( fn, g, 1 - g.qcur, false );
	timeRead( fn, g, g.qcur, true );
	fn->glQueryCounter( g.q[g.qcur * 2], kTimestamp );

	const bool sized = g.hist[0].w == hw && g.hist[0].h == hh;
	bool ok = ensure( fn, g.march, mw, mh ) && ensure( fn, g.hist[0], hw, hh ) && ensure( fn, g.hist[1], hw, hh );
	QString why = ok ? QString() : QStringLiteral( "framebuffer incomplete" );
	const Cam cam = cameraOf( scene );
	const float jump = std::hypot( cam.km[0] - g.prevKm[0], cam.km[1] - g.prevKm[1], cam.km[2] - g.prevKm[2] );
	const bool histOn = ok && sized && g.histOk && jump < 2.0f;
	const int frameNo = everyPixel ? 0 : int( g.frame % 16 );
	// 4x4 Bayer order: consecutive frames land far apart in the block
	static const int bayer[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };
	const int slot = everyPixel ? 0 : bayer[frameNo];
	const float phase[2] = { float( slot % 4 ), float( slot / 4 ) };
	const Bands b = bandsOf( m );
	/* lane CLOUD2B: the last frame's scene depth ends the march (grabbed right after the last draw, same viewport).
	 * lane CLOUD2C: a harness paint never takes the last paint's depth (the first paint has none, so a fresh open and a
	 * live set differed): the pair runs again after the scene with this frame's depth (wwCloudVolSceneDepth,
	 * wwCloudVolLookdevFront); a live view does that only while it has no depth yet. Red firstpaint: CLOUD2B's order. */
	const bool noDepth = red( "nodepth" ), firstPaint = red( "firstpaint" );
	const bool sceneOn = !noDepth && ( firstPaint || !everyPixel ) && g.sceneTex && g.sceneW == vp[2] && g.sceneH == vp[3]
		&& g.sceneFrame == g.frame;
	g.redo = !sceneOn && !noDepth && !firstPaint;
	g.drewNow = false;
	float amb[3];
	skyAmbient( s, keyW[2], f.haze, amb );
	for ( float & a : amb )
		a *= keyK;
	CvRedo & d = redos()[r];
	d.ok = false;
	d.b = b;
	d.cam = cam;
	std::copy( keyW, keyW + 3, d.keyW );
	d.keyK = keyK;
	std::copy( amb, amb + 3, d.amb );
	d.tier = s.tier;
	d.hw = hw;
	d.hh = hh;
	d.mw = mw;
	d.mh = mh;
	d.stride = stride;
	d.cur = g.cur;
	d.prev = 1 - g.cur;
	std::copy( phase, phase + 2, d.phase );
	d.frameNo = everyPixel ? 0.0f : float( g.frame % 64 );
	d.secs = view.cloudSeconds;
	d.histOn = histOn;
	std::copy( g.prevCam, g.prevCam + 9, d.prevCam );
	std::copy( g.prevProj, g.prevProj + 4, d.prevProj );
	std::copy( g.prevKm, g.prevKm + 3, d.prevKm );

	if ( ok )	// lane CLOUD4: the fly-through box over the camera (its own FBO; the march binds its own after)
		wwCloudFlyPrepare( scene, cam.km, view.cloudSeconds );
	// 1. the march: one fragment a history pixel of this frame's phase; 2. the resolve into this frame's history
	if ( ok && !marchPass( r, g, d, f, m, sceneOn ? g.sceneTex : 0 ) ) {
		ok = false;
		why = QStringLiteral( "cloud_march.prog did not link" );
	}
	if ( ok && !resolvePass( r, g, d ) ) {
		ok = false;
		why = QStringLiteral( "cloud_resolve.prog did not link" );
	}
	d.ok = ok;
	if ( !ok )
		g.redo = false;
	const int cur = d.cur, prev = d.prev;
	// 3. the composite over the sky, in the caller's framebuffer
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
	if ( ok ) {
		NifSkopeOpenGLContext::Program * p = r->useProgram( "cloud_composite.prog" );
		if ( p ) {
			fn->glActiveTexture( GL_TEXTURE0 );
			fn->glBindTexture( GL_TEXTURE_2D, g.hist[cur].col );
			p->uni1i( "cvHist", 0 );
			p->uni4f( "cvRect", FloatVector4( float( vp[0] ), float( vp[1] ), 1.0f / float( vp[2] ), 1.0f / float( vp[3] ) ) );
			p->uni1f( "sceneExposure", view.exposure );
			p->uni1i( "viewTransform", view.viewTransform );
			p->uni1i( "cvFront", 0 );
			fn->glEnable( GL_BLEND );
			fn->glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE );	// as the 2D layers
			quad( r );
			fn->glDisable( GL_BLEND );
			r->stopProgram();
		} else {
			ok = false;
			why = QStringLiteral( "cloud_composite.prog did not link" );
		}
	}
	for ( int u = 6; u >= 0; u-- ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
		fn->glBindTexture( GL_TEXTURE_3D, 0 );
	}
	fn->glActiveTexture( GLenum( prevActive ) );
	if ( wasDepth )
		fn->glEnable( GL_DEPTH_TEST );
	if ( wasBlend )
		fn->glEnable( GL_BLEND );
	if ( wasCull )
		fn->glEnable( GL_CULL_FACE );
	if ( wasScissor )
		fn->glEnable( GL_SCISSOR_TEST );
	if ( wasStencil )
		fn->glEnable( GL_STENCIL_TEST );
	fn->glDepthMask( depthMask );
	fn->glQueryCounter( g.q[g.qcur * 2 + 1], kTimestamp );
	g.pending[g.qcur] = true;
	g.qcur = 1 - g.qcur;

	if ( ok ) {
		// what wwCloudVolFront needs after the scene drew
		g.drewNow = true;
		g.frontHist = cur;
		std::copy( cam.proj, cam.proj + 4, g.frontProj );
		g.frontExposure = view.exposure;
		g.frontVt = view.viewTransform;
		s.drawnSecs = view.cloudSeconds;	// lane CLOUD2C: the bind hands readers this clock's wind
		g.cur = prev;
		g.histOk = true;
		g.frame++;
		std::copy( cam.m, cam.m + 9, g.prevCam );
		std::copy( cam.proj, cam.proj + 4, g.prevProj );
		std::copy( cam.km, cam.km + 3, g.prevKm );
	} else {
		g.histOk = false;
	}
	// memory: noise 128^3 + mips, 32^3 + mips; two histories and the march, 12 bytes a pixel each
	const double mb = ( 128.0 * 128 * 128 * 8 / 7 + 32.0 * 32 * 32 * 8 / 7 + ( 2.0 * hw * hh + double( mw ) * mh ) * 12.0 ) / 1048576.0;
	QString reds;
	for ( const char * w : { "noreproj", "nooctaves", "nocal", "everypixel", "noms", "nodepth", "notint", "notop", "wind0",
			  "display", "firstpaint", "lookdevback", "tintname" } )
		if ( red( w ) )
			reds += QStringLiteral( ",RED=%1" ).arg( QLatin1StringView( w ) );
	// lane CLOUD2C: clip = which scene depth ends the march (last = the last paint's, this = this paint's, after the scene)
	const QString clip = sceneOn ? QStringLiteral( "last" ) : g.redo ? QStringLiteral( "this" ) : QStringLiteral( "none" );
	s.last = ok ? QStringLiteral( "volclouds:%1(hist=%2x%3,march=%4x%5,steps=%6,taps=%7,key=%8,map=%9,mem=%10MB,clip=%11,"
								  "wind=%12km/s%13)" )
			.arg( QLatin1StringView( tierName( s.tier ) ) ).arg( hw ).arg( hh ).arg( mw ).arg( mh ).arg( pr.steps ).arg( pr.taps )
			.arg( keyK == f.sunK ? QStringLiteral( "sun" ) : QStringLiteral( "moon" ) )
			.arg( m.stub ? QStringLiteral( "stub" ) : QString::number( m.size ) ).arg( mb, 0, 'f', 1 ).arg( clip )
			.arg( double( V::kWindKmS ), 0, 'g', 3 ).arg( reds )
		: refusal( s, why );
	if ( ok && wwCloudFlyTier() > 0 )	// lane CLOUD4
		s.last += QStringLiteral( " fly=" ) + wwCloudFlySummary();
	drew << s.last;
	return ok;
}

void wwCloudVolSceneDepth( Scene * scene )
{
	if ( !scene || !scene->renderer || !gpus().contains( scene->renderer ) )
		return;
	Renderer * r = scene->renderer;
	CvGpu & g = gpus()[r];
	g.sceneTex = 0;
	if ( cs().tier < 2 || !g.drewNow || red( "nodepth" ) ) {
		g.redo = false;
		return;
	}
	g.sceneTex = wwCellHdrDepthMax( scene );
	if ( !g.sceneTex ) {
		g.redo = false;
		return;
	}
	wwCellHdrFrameSize( g.sceneW, g.sceneH );
	g.sceneFrame = g.frame;	// the next draw's frame number: a live march reads it one frame late
	sceneZOf( scene, g );
	redoWithDepth( r, g );	// lane CLOUD2C: a harness paint, or the first: this frame's depth ends the march
	/* lane CLOUD2C: the clouds in front of the scene go into the linear frame before the tone map, so the cell's
	 * imagespace treats them as it treats the terrain behind them. Red display: CLOUD2B's pass after the tone map. */
	if ( !red( "display" ) ) {
		frontPass( r, g, 2 );
		g.drewNow = false;
	}
}

void wwCloudVolFront( Scene * scene )
{
	if ( !scene || !scene->renderer || !gpus().contains( scene->renderer ) )
		return;
	CvGpu & g = gpus()[scene->renderer];
	const bool go = g.drewNow && g.sceneTex && !red( "nodepth" );
	g.drewNow = false;
	if ( go )
		frontPass( scene->renderer, g, 1 );	// the red display (CLOUD2B); red nodepth: behind all geometry, as CLOUD2
}

void wwCloudVolLookdevFront( Scene * scene )
{
	if ( !scene || !scene->renderer || !gpus().contains( scene->renderer ) )
		return;
	Renderer * r = scene->renderer;
	CvGpu & g = gpus()[r];
	const bool drew = g.drewNow;
	g.drewNow = false;
	g.sceneTex = 0;
	if ( !drew || cs().tier < 2 || red( "nodepth" ) || red( "lookdevback" ) || !scene->grabEffectDepth()
		|| !scene->fxDepthTexId ) {
		g.redo = false;
		return;	// red lookdevback: CLOUD2B's single-NIF order (the volume behind all geometry)
	}
	g.sceneTex = scene->fxDepthTexId;	// the opaque depth the soft effects read (Scene::grabEffectDepth)
	g.sceneW = scene->fxDepthTexW;
	g.sceneH = scene->fxDepthTexH;
	g.sceneFrame = g.frame;
	sceneZOf( scene, g );
	redoWithDepth( r, g );
	frontPass( r, g, 1 );	// display space, as every other lookdev draw over the backdrop
}

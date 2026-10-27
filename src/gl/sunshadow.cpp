/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "sunshadow.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store
#include "cellunits.h"
#include "celluniform.h"	// lane CELLUNI2
#include "gametaa.h"
#include "gl/cellwater.h"	// lane SUNCELL1: water never casts
#include "cellmesh.h"	// lane FARDRAW1: CellMeshInst
#include "gl/cellfarinst.h"	// lane FARDRAW1: the instanced far rings cast
#include "cellfarhide.h"	// lane FARHOLE2: the rows a loaded near cell leaves out
#include "gl/cellcull.h"	// lane SUNCELL1: each cascade culls its casters against its own window
#include "gl/cellgrass.h"	// lane FAR5: a wind cable's shadow sways with it
#include "gl/celllooks.h"
#include "gl/cellpasstime.h"	// lane PERF1: the casters' draw count
#include "gl/cellqueue.h"	// lane GPUDRIVEN1: the casters' render queues

#include "gl/glnode.h"
#include "gl/glproperty.h"	// lane VOLAIR1: alpha-tested casters read the base map
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "gl/lookdevstage.h"
#include "gl/renderer.h"
#include "gl/shadowset.h"	// lane TERRSHADOW1: the unified set (presets, terrain, contact, composition)
#include "gl/cloudshadow.h"	// lane CLOUD3
#include "gl/cellfarground.h"	// lane FARDRAW1

#include <QDir>
#include <QFile>
#include <QOpenGLContext>
#include <QStringList>

#include <QHash>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
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
constexpr int kTextureUnit = WwCellUnit::Csm;			// TexCache allocates from unit 0 upward

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
	quint64 liveGen = 0;	// lane TESTDRIVER1: the switch generation last read
	double D = kDefaultDistance;
	int map = kDefaultMap;
	int probe = 0;
	float force = -1.0f;
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
	double C[3] = { 0, 0, 0 }, fwd[3] = { 0, 0, -1 }, tanX = 1, tanY = 1, camNear = 1, camFar = 1, sc = 1;
	double cL[kCascades][3] = {}, cR[kCascades][3] = {}, cU[kCascades][3] = {};	// lane SHADOWCACHE1: per cascade
	double rc[3] = { 1, 0, 0 }, uc[3] = { 0, 1, 0 };	// the camera's right and up rows (world)
	double split[2] = { kSplit0, kSplit1 };
	int casters = 0, tris = 0, atCasters = 0;
	int swayCables = -1;	// lane FAR5: wind cables cast where they sway (-1 = Cable shadows sway off)
	int slopeClamp = 0;	// lane SHADOWPAR1: 1 the caster bias clamped (FO4CS's slope 4), -1 no glPolygonOffsetClamp
	// lane TERRSHADOW1: the fit's numbers, CSM1's constants unless Cascade Quality picks a preset
	double baseD = kDefaultDistance, split0 = kSplit0, split1 = kSplit1, blend = kBlend, lightBack = kLightBack;
	int baseMap = kDefaultMap;
	float slopeBias = kSlopeBias, unitBias = kUnitBias;
	bool groundCast = false;
};

CsmState & cs()
{
	static CsmState s;
	// lane TESTDRIVER1: a live set of these switches re-reads them (pins and or-ed reds back to their defaults first)
	if ( !s.init || wwSwitchMovedFor( s.liveGen, { "WW_CSM_" } ) ) {
		s.init = true;
		s.liveGen = wwSwitchGen();
		{
			const CsmState d {};
			s.D = d.D;
			s.map = d.map;
			s.force = d.force;
		}
		bool ok = false;
		const double d = wwEnvStr( "WW_CSM_DISTANCE" ).toDouble( &ok );
		if ( ok && d > 2.0 * kBlend )
			s.D = d;
		const int m = wwEnvStr( "WW_CSM_MAP" ).toInt( &ok );
		if ( ok && m >= 64 && m <= 8192 )
			s.map = m;
		s.baseD = s.D;	// lane TERRSHADOW1
		s.baseMap = s.map;
		s.probe = wwEnvStr( "WW_CSM_PROBE" ).toInt();
		// WW_CSM_FORCE=<0..1>: the receiver returns this factor instead of the map's (the
		// wiring test of the factor sites: both sun terms must take it); unset = -1, off
		const float f = wwEnvStr( "WW_CSM_FORCE" ).toFloat( &ok );
		if ( ok && f >= 0.0f && f <= 1.0f )
			s.force = f;
		s.reds = wwEnvStr( "WW_CSM_RED" ).split( QChar( ',' ), Qt::SkipEmptyParts );
		for ( QString & r : s.reds )
			r = r.trimmed().toLower();
	}
	return s;
}

bool red( const char * name )
{
	const QStringList & reds = cs().reds;	// lane PERF1: asked per caster and per shape; no reds = no string
	if ( reds.isEmpty() )
		return false;
	// lane TERRSHADOW1: its solidcast red is an alias of VOLAIR1's atsolid (VOLAIR1's alpha casters won)
	if ( qstrcmp( name, "atsolid" ) == 0 && reds.contains( QStringLiteral( "solidcast" ) ) )
		return true;
	return reds.contains( QString::fromLatin1( name ) );
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
	s.split[0] = red( "wrongsplit" ) ? 400.0 : s.split0;
	s.split[1] = red( "wrongsplit" ) ? 1500.0 : s.split1;
	const double far[kCascades] = { s.split[0], s.split[1], s.D };	// the last is overridden by D
	double * rc = s.rc, * uc = s.uc;
	for ( int j = 0; j < 3; j++ ) {
		rc[j] = vt.rotation( 0, j );
		uc[j] = vt.rotation( 1, j );
	}
	for ( int i = 0; i < kCascades; i++ ) {
		Cascade & c = s.c[i];
		// lane SHADOWCACHE1: each cascade's own light direction (the far ones may hold theirs a few frames)
		const double * Lr = s.cL[i], * Rr = s.cR[i], * Ur = s.cU[i];
		c = Cascade();
		c.zn = ( i == 0 || one ) ? s.camNear : far[i - 1] - s.blend;
		c.zf = ( one ? s.D : far[i] ) + s.blend;
		bool first = true;
		for ( int zi = 0; zi < 2; zi++ ) {
			const double z = zi ? c.zf : c.zn;
			for ( int sx = -1; sx <= 1; sx += 2 ) {
				for ( int sy = -1; sy <= 1; sy += 2 ) {
					double q[3];	// Q - C
					for ( int j = 0; j < 3; j++ )
						q[j] = z * ( s.fwd[j] + sx * s.tanX * rc[j] + sy * s.tanY * uc[j] );
					const double x = dot3( q, Rr ), y = dot3( q, Ur ), zz = dot3( q, Lr ) + s.lightBack;
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
		c.pb[0] = snap( dot3( s.C, Rr ), c );
		c.pb[1] = snap( dot3( s.C, Ur ), c );
		c.pb[2] = snap( dot3( s.C, Lr ) - s.lightBack, c );
		c.l = snap( c.minX, c );
		c.b = snap( c.minY, c );
		c.vw = std::clamp( int( std::floor( c.w / c.texel + 1e-9 ) ), 1, s.map );
		c.vh = std::clamp( int( std::floor( c.h / c.texel + 1e-9 ) ), 1, s.map );
		c.far = c.maxZ + kNear;

		// world rows: x_l = right.Q - pb.x, window [l, l + vw texel] (INFERRED right edge)
		const double wx = c.vw * c.texel, wy = c.vh * c.texel, wz = c.far - kNear;
		double a[3];
		for ( int j = 0; j < 3; j++ )
			a[j] = Rr[j] * 2.0 / wx;
		viewRow( c.clipFromView, 0, a, -( c.pb[0] + c.l ) * 2.0 / wx - 1.0, vt, s.sc );
		for ( int j = 0; j < 3; j++ )
			a[j] = Ur[j] * 2.0 / wy;
		viewRow( c.clipFromView, 1, a, -( c.pb[1] + c.b ) * 2.0 / wy - 1.0, vt, s.sc );
		for ( int j = 0; j < 3; j++ )
			a[j] = Lr[j] * 2.0 / wz;
		viewRow( c.clipFromView, 2, a, -( c.pb[2] + kNear ) * 2.0 / wz - 1.0, vt, s.sc );
		lastRow( c.clipFromView );

		const double tm = c.texel * s.map;
		for ( int j = 0; j < 3; j++ )
			a[j] = Rr[j] / tm;
		viewRow( c.uvdFromView, 0, a, -( c.pb[0] + c.l ) / tm, vt, s.sc );
		for ( int j = 0; j < 3; j++ )
			a[j] = Ur[j] / tm;
		viewRow( c.uvdFromView, 1, a, -( c.pb[1] + c.b ) / tm, vt, s.sc );
		for ( int j = 0; j < 3; j++ )
			a[j] = Lr[j] / wz;
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
	bool alphaTested = false;	// lane VOLAIR1: drawn by lookdev_csmdepth_at.prog (the base map's alpha test)
	bool dynamic = false;		// lane SHADOWCACHE1: drawn over the cached static layer every pass
	std::uint64_t sig = 0;
};

// lane FAR5: fo4_default.vert's grass wind on the CPU (model space) for a wind cable; false for any other shape.
// Red WW_CELL_CABLES_SHADOWSWAY_RED=double casts at twice the offset.
bool cableSway( const Scene * scene, const Shape * sh, std::vector<Vector3> & out )
{
	float wind[4], wind2[4];
	if ( sh->colors.size() != sh->verts.size() || !wwCellGrassCableWind( scene->nifModel, sh->id(), wind, wind2 ) )
		return false;
	const float k = wwEnvStr( "WW_CELL_CABLES_SHADOWSWAY_RED" ) == QLatin1String( "double" ) ? 2.0f : 1.0f;
	const float half = ( wind2[1] - wind2[0] ) * 0.5f;
	const float dx = std::cos( wind[0] ), dy = std::sin( wind[0] );
	const bool hasB = sh->bitangents.size() == sh->verts.size();
	out.resize( size_t( sh->verts.size() ) );
	for ( qsizetype v = 0; v < sh->verts.size(); v++ ) {
		const float L = hasB ? sh->bitangents.at( v ).length() : 1.0f;
		const float off = L > 2.0f ? L - 1024.0f : 0.0f;
		const float p = ( wind[3] - off ) * wind2[2];
		const float sp = std::sin( p ), cp = std::cos( p );
		float w = std::sin( sp * 3.14159274f ) + std::sin( sp * 6.28318548f );
		w = w * 0.3f + std::cos( cp * 3.14159274f ) * 0.2f + 1.0f;
		w = w * half + wind2[0];
		const float a = sh->colors.at( v ).alpha();
		w *= a * a * 0.5f * k;
		Vector3 q = sh->verts.at( v );
		q[0] += dx * w;
		q[1] += dy * w;
		out[size_t( v )] = q;
	}
	return true;
}

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


/* lane SHADOWCACHE1 (2026-10-22): THE STATIC CASCADE CACHE (the way CoD, Snowdrop and UE5's virtual shadow maps do it).
 * Each cascade keeps a second depth layer holding only its static casters. A pass whose fit, camera, static set and
 * render state equal the cached ones copies that layer into the live map and draws the dynamic casters over it (or
 * does nothing when there are none): depth-only LEQUAL keeps the nearest depth in any order, so this is exact.
 * Far cascades (WW_SHADOWCACHE_SCROLL mask) also scroll: when the window only moved on the same texel lattice, the old
 * static depths are copied across with their depth range remapped, and only the newly exposed strips are drawn.
 * Switches: WW_SHADOWCACHE=0 (GPUDRIVEN1's pass exactly); WW_SHADOWCACHE_RED=full (static redrawn every pass, its
 * cost back) or =stale (never redrawn: the moving gate must fail); WW_SHADOWCACHE_SCROLL=<cascade mask, 6>;
 * WW_SHADOWCACHE_SUNHOLD=<degrees, 0.05> (the scrolling cascades keep their sun direction until it turns further);
 * WW_SHADOWCACHE_DYNFRAMES=<120> (a caster whose transform or geometry changed that recently draws as dynamic);
 * WW_SHADOWCACHE_CENSUS=1 (or WW_GPUDRIVEN_CENSUS=1): the modes per cascade on stderr. */
namespace ShadowCache
{

enum Mode { Off, Skip, Hit, Full, Scroll };

struct Hash
{
	std::uint64_t h = 0x9e3779b97f4a7c15ULL;
	void w( std::uint64_t x )
	{
		h ^= x;
		h *= 0xff51afd7ed558ccdULL;
		h ^= h >> 33;
	}
	void f( float x )
	{
		std::uint32_t u;
		std::memcpy( &u, &x, 4 );
		w( u );
	}
	void d( double x )
	{
		std::uint64_t u;
		std::memcpy( &u, &x, 8 );
		w( u );
	}
	void p( const void * x ) { w( std::uint64_t( reinterpret_cast<std::uintptr_t>( x ) ) ); }
	void t( const Transform & x )
	{
		for ( int i = 0; i < 3; i++ )
			for ( int j = 0; j < 3; j++ )
				f( x.rotation( i, j ) );
		for ( int i = 0; i < 3; i++ )
			f( x.translation[i] );
		f( x.scale );
	}
};

struct Track
{
	std::uint64_t h = 0;
	qint64 changed = -( qint64( 1 ) << 40 );
	qint64 seen = 0;
};

struct Layer
{
	bool valid = false;
	bool liveIsStatic = false;	// the live layer holds exactly the static layer (no dynamic caster drawn over it)
	std::uint64_t key = 0, setSig = 0, stateSig = 0;
	double corner[2] = { 0, 0 }, nearZ = 0, wz = 0, texel = 0;
	double L[3] = { 0, 0, 0 };
	int n = 0, vw = 0, vh = 0, map = 0;
};

struct State
{
	bool init = false, on = true, census = false;
	int red = 0;		// 1 full, 2 stale, 3 scrollgrow
	int mask = 6;		// the cascades that scroll and hold the sun
	double holdDeg = 0.05, holdCos = 1.0;
	qint64 dynFrames = 120;
	const void * ctx = nullptr;
	unsigned int tex = 0, scratch = 0, fbo = 0, vao = 0;
	unsigned int liveTex = 0;
	int texMap = 0;
	qint64 frame = 0;
	Layer c[kCascades];
	QHash<const Shape *, Track> track;
	bool haveL[kCascades] = {};
	quint64 liveGen = 0;	// lane LIVESET1
};

State & st()
{
	static State s;
	// lane LIVESET1: a live set reads the switches again (defaults first) and empties the cache, so the next frame
	// draws every layer as a fresh launch's first frame does; the GL objects are kept
	const bool moved = s.init && wwSwitchMovedFor( s.liveGen, { "WW_SHADOWCACHE", "WW_GPUDRIVEN_CENSUS" } );
	if ( moved ) {
		for ( int i = 0; i < kCascades; i++ ) {
			s.c[i] = Layer();
			s.haveL[i] = false;
		}
		s.track.clear();
		s.mask = 6;
		s.holdDeg = 0.05;
		s.dynFrames = 120;
	}
	if ( !s.init || moved ) {
		s.init = true;
		s.on = qEnvironmentVariable( "WW_SHADOWCACHE" ).trimmed() != QLatin1StringView( "0" );
		const QString r = qEnvironmentVariable( "WW_SHADOWCACHE_RED" ).trimmed().toLower();
		s.red = ( r == QLatin1StringView( "full" ) || r == QLatin1StringView( "1" ) ) ? 1 : r == QLatin1StringView( "stale" ) ? 2
			: r == QLatin1StringView( "scrollgrow" ) ? 3 : 0;
		bool ok = false;
		const int m = qEnvironmentVariable( "WW_SHADOWCACHE_SCROLL" ).toInt( &ok );
		if ( ok )
			s.mask = m & 7;
		const double deg = qEnvironmentVariable( "WW_SHADOWCACHE_SUNHOLD" ).toDouble( &ok );
		if ( ok )
			s.holdDeg = std::clamp( deg, 0.0, 5.0 );
		s.holdCos = s.holdDeg > 0.0 ? std::cos( s.holdDeg * 3.14159265358979323846 / 180.0 ) : 2.0;	// 2: never holds
		const int df = qEnvironmentVariable( "WW_SHADOWCACHE_DYNFRAMES" ).toInt( &ok );
		if ( ok && df >= 0 )
			s.dynFrames = df;
		s.census = qEnvironmentVariable( "WW_SHADOWCACHE_CENSUS" ).trimmed() == QLatin1StringView( "1" ) || GpuDriven::census();
		std::fprintf( stderr, "shadowcache: %s red=%d scroll=%d sunhold=%g dynframes=%lld\n", s.on ? "on" : "off", s.red, s.mask,
			s.holdDeg, (long long)s.dynFrames );
	}
	return s;
}

//! the cascades' light directions: the current sun, except a masked cascade keeps its last one within the hold angle
void chooseLights( CsmState & cs, bool presetChanged )
{
	State & s = st();
	for ( int i = 0; i < kCascades; i++ ) {
		bool keep = s.on && s.red != 1 && ( s.mask >> i & 1 ) && s.haveL[i] && !presetChanged;
		if ( keep )
			keep = dot3( cs.cL[i], cs.L ) >= s.holdCos;
		if ( !keep ) {
			std::memcpy( cs.cL[i], cs.L, sizeof( cs.L ) );
			std::memcpy( cs.cR[i], cs.right, sizeof( cs.right ) );
			std::memcpy( cs.cU[i], cs.up, sizeof( cs.up ) );
		}
		s.haveL[i] = true;
	}
}

//! a caster's static signature; false when it draws as dynamic this pass (skinned, or changed within dynFrames)
bool classify( const Shape * sh, bool skinned, bool twoSided, bool alphaTested, std::uint64_t & sig )
{
	State & s = st();
	Hash h;
	h.p( sh );
	h.w( std::uint64_t( std::uint32_t( sh->id() ) ) );
	h.t( sh->worldTrans() );
	h.p( sh->verts.constData() );
	h.w( std::uint64_t( sh->verts.size() ) );
	h.p( sh->triangles.constData() );
	h.w( std::uint64_t( sh->triangles.size() ) );
	h.w( ( twoSided ? 1u : 0u ) | ( alphaTested ? 2u : 0u ) );
	if ( const NifSkopeOpenGLContext::ShapeDataHash * dh = sh->wwDataHash() ) {
		h.w( dh->h[0] );
		h.w( dh->h[1] );
	}
	// lane FARHOLE2: an instanced far ring casts only the rows drawn now; the cached cascades redraw when they change
	if ( sh->wwFarInstData() && CellFarHide::shadowKeyed() )
		h.w( CellFarHide::view( *sh->wwFarInstData() ).key );
	if ( alphaTested ) {
		const BSLightingShaderProperty * lsp = sh->wwLightingShader();
		BSShaderLightingProperty * bsp = sh->wwShaderLighting();
		h.p( bsp );
		if ( bsp ) {
			h.f( bsp->uvScale.x );
			h.f( bsp->uvScale.y );
			h.f( bsp->uvOffset.x );
			h.f( bsp->uvOffset.y );
			h.w( std::uint64_t( int( bsp->clampMode ) ) );
			h.w( qHash( bsp->fileName( 0 ) ) );
		}
		h.f( lsp ? lsp->alpha : 1.0f );
		h.f( sh->wwAlphaThreshold() );
		h.p( sh->coords.isEmpty() ? nullptr : sh->coords.constFirst().constData() );
	}
	sig = h.h;
	Track & t = s.track[sh];
	if ( t.seen && t.h != sig )
		t.changed = s.frame;
	t.h = sig;
	t.seen = s.frame;
	return !skinned && s.frame - t.changed >= s.dynFrames;
}

//! once a pass, after the casters: forget shapes not seen for 600 passes (their pointers may be reused)
void sweep()
{
	State & s = st();
	if ( s.frame % 600 )
		return;
	for ( auto it = s.track.begin(); it != s.track.end(); ) {
		if ( s.frame - it->seen > 600 )
			it = s.track.erase( it );
		else
			++it;
	}
}

void invalidate()
{
	State & s = st();
	for ( Layer & l : s.c )
		l = Layer();
}

//! the static layers, the one-layer scratch and their framebuffer, per context and map size
bool ensure( Renderer * r, int map, unsigned int liveTex )
{
	State & s = st();
	auto fn = r->fn;
	const void * ctx = QOpenGLContext::currentContext();
	if ( ctx != s.ctx ) {
		s.ctx = ctx;
		s.tex = s.scratch = s.fbo = s.vao = 0;
		s.texMap = 0;
		invalidate();
	}
	if ( liveTex != s.liveTex ) {
		s.liveTex = liveTex;	// the live map was made again: its content is gone
		for ( Layer & l : s.c )
			l.liveIsStatic = false;
	}
	if ( !s.tex || s.texMap != map ) {
		if ( !s.tex )
			fn->glGenTextures( 1, &s.tex );
		if ( !s.scratch )
			fn->glGenTextures( 1, &s.scratch );
		GLint prevTex = 0;
		fn->glGetIntegerv( GL_TEXTURE_BINDING_2D_ARRAY, &prevTex );
		for ( unsigned int t : { s.tex, s.scratch } ) {
			fn->glBindTexture( GL_TEXTURE_2D_ARRAY, t );
			fn->glTexImage3D( GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT16, map, map, t == s.tex ? kCascades : 1, 0,
				GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, nullptr );
			fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
			fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
			fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_NONE );
		}
		fn->glBindTexture( GL_TEXTURE_2D_ARRAY, GLuint( prevTex ) );
		s.texMap = map;
		invalidate();
	}
	if ( !s.fbo )
		fn->glGenFramebuffers( 1, &s.fbo );
	if ( !s.vao )
		fn->glGenVertexArrays( 1, &s.vao );
	return s.tex && s.scratch && s.fbo && s.vao;
}

//! the cascade's clip matrix narrowed to the texel rect rc = (x0, y0, x1, y1) of its (vw, vh) viewport, for culling
void stripMatrix( const float * m, int vw, int vh, const int * rc, float * out )
{
	std::memcpy( out, m, sizeof( float ) * 16 );
	const double ax = double( vw ) / double( rc[2] - rc[0] ), bx = ( double( vw ) - double( rc[0] + rc[2] ) ) / double( rc[2] - rc[0] );
	const double ay = double( vh ) / double( rc[3] - rc[1] ), by = ( double( vh ) - double( rc[1] + rc[3] ) ) / double( rc[3] - rc[1] );
	for ( int col = 0; col < 4; col++ ) {
		out[col * 4 + 0] = float( m[col * 4 + 0] * ax );
		out[col * 4 + 1] = float( m[col * 4 + 1] * ay );
	}
	out[12] += float( bx );
	out[13] += float( by );
}

//! a caster's world bounds against a strip's matrix (x and y only, as the cell cull does)
bool boundsInside( const Scene * scene, const Shape * sh, const float * m )
{
	const BoundSphere bs = sh->bounds();
	const Vector3 v = scene->view * bs.center;
	const float rv = bs.radius * std::fabs( scene->view.scale );
	float lx = 0.0f, ly = 0.0f;
	for ( int c = 0; c < 3; c++ ) {
		lx += m[c * 4 + 0] * m[c * 4 + 0];
		ly += m[c * 4 + 1] * m[c * 4 + 1];
	}
	const float x = m[0] * v[0] + m[4] * v[1] + m[8] * v[2] + m[12];
	const float y = m[1] * v[0] + m[5] * v[1] + m[9] * v[2] + m[13];
	return std::fabs( x ) <= 1.0f + rv * std::sqrt( lx ) && std::fabs( y ) <= 1.0f + rv * std::sqrt( ly );
}

} // namespace ShadowCache

} // namespace

bool wwSunShadowWanted( Scene * scene )
{
	const CsmState & s = cs();
	// lane TERRSHADOW1: the receivers also run while the terrain or contact producer is valid
	return ( s.valid || wwShadowSetActive( scene ) ) && scene && !scene->selecting && wwLookdevActive();
}

static void sunShadowPassImpl( Scene * scene );

/* WW_CSM_DUMP=<ABSOLUTE path> (lane VOLAIR1, gates only): the cascade map the receivers read, rewritten whenever the
 * pass summary changes (passes with casters only): int32 map, cascades; then each layer's map x map uint16 depths, bottom row first. With
 * its own echo (<path>.txt) a judge predicts any point's sun visibility from the map itself. */
static void csmDumpMap( const CsmState & s, Renderer * r )
{
	static int armed = -1;
	static QString path, lastSummary;
	static quint64 liveGen = 0;	// fix TDFIX2: a live set of WW_CSM_DUMP re-arms it
	if ( armed < 0 || wwSwitchMovedFor( liveGen, { "WW_CSM_DUMP" } ) ) {
		liveGen = wwSwitchGen();
		lastSummary.clear();
		path = wwEnvStr( "WW_CSM_DUMP" ).trimmed();
		armed = ( !path.isEmpty() && QDir::isAbsolutePath( path ) ) ? 1 : 0;
	}
	// a pass with no casters (a frame before the cell is placed, or after it is torn down) is never the grabbed one
	if ( armed != 1 || !s.valid || !s.fbo || !s.tex || s.casters <= 0 )
		return;
	const QString line = wwSunShadowSummary();
	if ( line == lastSummary )
		return;
	lastSummary = line;
	auto fn = r->fn;
	GLint prevRead = 0;
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, s.fbo );
	fn->glPixelStorei( GL_PACK_ALIGNMENT, 2 );
	std::vector<std::uint16_t> layer( size_t( s.map ) * size_t( s.map ) );
	QFile f( path );
	if ( f.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
		const qint32 hd[2] = { s.map, kCascades };
		f.write( reinterpret_cast<const char *>( hd ), sizeof( hd ) );
		for ( int i = 0; i < kCascades; i++ ) {
			fn->glFramebufferTextureLayer( GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, s.tex, 0, i );
			fn->glReadPixels( 0, 0, s.map, s.map, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, layer.data() );
			f.write( reinterpret_cast<const char *>( layer.data() ), qint64( layer.size() * sizeof( std::uint16_t ) ) );
		}
	}
	// its own echo beside it (<path>.txt), written under the same rule, so map and matrices are one pass's
	QFile t( path + QStringLiteral( ".txt" ) );
	if ( t.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
		t.write( ( QStringLiteral( " " ) + line + QStringLiteral( "\n" ) ).toUtf8() );
	fn->glPixelStorei( GL_PACK_ALIGNMENT, 4 );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
}

/* WW_CSM_ECHO=<ABSOLUTE path>: the summary of the pass that just ran, rewritten
 * whenever it changes. The PBRM census row is written at a shape's FIRST draw,
 * which is before the render hook's camera pin takes; this file holds the fit
 * of the frame that was grabbed. */
static void csmEchoFrame()
{
	static int armed = -1;
	static QString path, lastWritten;
	static quint64 liveGen = 0;	// lane LIVESET1: a live set arms it (or moves the file) on the next frame
	if ( armed < 0 || wwSwitchMovedFor( liveGen, { "WW_CSM_ECHO" } ) ) {
		lastWritten.clear();
		path = wwEnvStr( "WW_CSM_ECHO" ).trimmed();
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
	wwShadowSetFrame( scene );	// lane TERRSHADOW1: the terrain sweep's step, whatever the Shadows row says
	wwCloudShadowPass( scene );	// lane CLOUD3: the clouds' shadow map rows, whatever the Shadows row says
	CellFarGround::pass( scene );	// lane FARDRAW1: the far ring 0's live ground (uploads, page stamps, frees)
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
	// lane MOTION1: the fit reads the UNJITTERED projection, so the temporal AA's sub-pixel jitter never moves a cascade
	const auto & Pg = r->globalUniforms->projectionMatrix;
	FloatVector4 Pfit[4] = { Pg[0], Pg[1], Pg[2], Pg[3] };
	Matrix4 Pu;
	if ( wwGameTaaUnjittered( Pu ) ) {
		const float * u = Pu.data();	// column-major, as uploaded
		for ( int c = 0; c < 4; c++ )
			Pfit[c] = FloatVector4( u[c * 4 + 0], u[c * 4 + 1], u[c * 4 + 2], u[c * 4 + 3] );
	}
	const auto & P = Pfit;
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
	s.camFar = double( P[3][2] ) / ( double( P[2][2] ) + 1.0 ) / s.sc;	// echo only: the judge's ground ends there

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

	// lane TERRSHADOW1: Cascade Quality (Off = CSM1's own numbers, the WW_CSM_DISTANCE / WW_CSM_MAP pins kept)
	WwCascadePreset cp;
	const bool presetOn = wwShadowCascadePreset( cp );
	if ( presetOn ) {
		s.D = cp.D;
		s.map = cp.map;
		s.split0 = cp.split0;
		s.split1 = cp.split1;
		s.blend = cp.blend;
		s.lightBack = cp.lightBack;
		s.slopeBias = cp.slopeBias;
		s.unitBias = cp.unitBias;
	} else {
		s.D = s.baseD;
		s.map = s.baseMap;
		s.split0 = kSplit0;
		s.split1 = kSplit1;
		s.blend = kBlend;
		s.lightBack = kLightBack;
		s.slopeBias = kSlopeBias;
		s.unitBias = kUnitBias;
	}

	// lane SHADOWCACHE1: the cascades' own light directions (a far cascade may hold its last one a little)
	ShadowCache::State & cst = ShadowCache::st();
	if ( cst.on )
		cst.frame++;
	ShadowCache::chooseLights( s, false );
	fitCascades( s, vt );

	// the casters
	std::vector<Caster> casters;
	s.tris = 0;
	const bool cpuSkin = scene->hasOption( Scene::DoSkinning );
	const bool cableSwayOn = wwLookOn( WwLook::CableShadowSway );
	s.swayCables = cableSwayOn ? 0 : -1;
	for ( Node * node : scene->nodes.list() ) {
		const Shape * sh = dynamic_cast<const Shape *>( node );
		if ( !sh || !sh->isVisible() || !sh->wwCastsSunShadow() )
			continue;
		// lane SUNCELL1: a cell's water surface is no caster (the game's water casts no sun shadow; a creek's plane
		// shadowed its whole bed). Red WW_CSM_RED=watercasts keeps it.
		if ( !red( "watercasts" ) && wwCellWaterIsShape( scene->nifModel, sh->id() ) )
			continue;
		if ( !sh->wwFarInstData() && ( sh->verts.isEmpty() || sh->triangles.isEmpty() ) )
			continue;   // lane FARDRAW1: an instanced far ring has no rows and casts through gl/cellfarinst
		// lane VOLAIR1: red WW_CSM_RED=atnone drops the alpha-tested casters (the window gate's ceiling: glass gone)
		if ( red( "atnone" ) && sh->wwAlphaTested() )
			continue;
		Caster c { sh, sh->wwDoubleSided(), {} };
		// lane VOLAIR1 (10-15): an alpha-tested shape (a window's GlassWindows01 glass, a leaf card) casts what its
		// alpha test keeps, as the colour pass draws it; it cast a solid card before, and the sun never reached the
		// air behind a window. Red WW_CSM_RED=atsolid casts them solid again.
		c.alphaTested = !red( "atsolid" ) && sh->wwAlphaTested() && sh->wwShaderLighting()
			&& ( sh->wwFarInstData() || ( !sh->coords.isEmpty() && sh->coords.constFirst().size() == sh->verts.size() ) );
		if ( cpuSkin && sh->wwGpuSkinned() ) {
			c.skinned.resize( size_t( sh->verts.size() ) );
			for ( qsizetype v = 0; v < sh->verts.size(); v++ )
				c.skinned[size_t( v )] = sh->skinVertex( int( v ), sh->verts.at( v ) );
		}
		if ( cableSwayOn && c.skinned.empty() && !sh->wwFarInstData() && cableSway( scene, sh, c.skinned ) )
			s.swayCables++;
		if ( cst.on )
			c.dynamic = !ShadowCache::classify( sh, sh->wwGpuSkinned() || !c.skinned.empty(), c.twoSided, c.alphaTested,
				c.sig );
		s.tris += sh->wwFarInstData() ? int( sh->wwFarInstData()->drawnTris ) : int( sh->triangles.size() );
		casters.push_back( std::move( c ) );
	}
	s.casters = int( casters.size() );
	s.atCasters = int( std::count_if( casters.begin(), casters.end(), []( const Caster & k ) { return k.alphaTested; } ) );
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
	// lane MSAABLACK1: bind READ too, so glReadBuffer below sets OUR fbo; on the view's it left TAA's colour blit
	// a silent no-op (MSAA 1 + TAA saved black). WW_MSAABLACK1_RED=1: the old binding
	WW_SWITCH_STATIC( bool, msaaBlackRed, wwEnvInt( "WW_MSAABLACK1_RED" ) == 1 );
	if ( !msaaBlackRed )
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, s.fbo );
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
		fn->glPolygonOffset( s.slopeBias, s.unitBias );
	}

	const int lMvp = prog->uniLocation( "csmClipFromView" );
	// lane VOLAIR1: the alpha-tested casters' program; without it they cast solid as before (said in the line)
	NifSkopeOpenGLContext::Program * progAt = nullptr;
	if ( s.atCasters ) {
		progAt = r->useProgram( "lookdev_csmdepth_at.prog" );
		if ( !progAt )
			s.atCasters = -1;
		r->useProgram( "lookdev_csmdepth.prog" );
	}
	// lane SHADOWPAR1: FO4CS clamps the caster's slope bias at a slope of 4 texels per texel (D3D DepthBiasClamp)
	typedef void ( *PolyOffsetClamp )( float, float, float );
	PolyOffsetClamp offsetClamp = nullptr;
	s.slopeClamp = 0;
	if ( wwShadowPar( WwParCascSlope ) && !red( "nobias" ) && QOpenGLContext::currentContext() ) {
		// looked up once per context, not every pass (the lookup is a driver call)
		static const QOpenGLContext * clampCtx = nullptr;
		static PolyOffsetClamp clampFn = nullptr;
		auto * gc = QOpenGLContext::currentContext();
		if ( gc != clampCtx ) {
			clampCtx = gc;
			clampFn = reinterpret_cast<PolyOffsetClamp>( gc->getProcAddress( "glPolygonOffsetClamp" ) );
			if ( !clampFn )
				clampFn = reinterpret_cast<PolyOffsetClamp>( gc->getProcAddress( "glPolygonOffsetClampEXT" ) );
		}
		offsetClamp = clampFn;
		s.slopeClamp = offsetClamp ? 1 : -1;
	}
	const bool cslopeZero = offsetClamp && wwShadowRed( "cslopezero" );
	bool complete = true;
	std::vector<std::uint32_t> cullRanges;	// lane SUNCELL1
	std::vector<std::uint16_t> cullIdx;

	/* lane SHADOWCACHE1: each cascade's mode (ShadowCache above), then its jobs. A job is (cascade, caster set, cull
	 * matrix, scissor): set 0 = every caster into the live layer (the old pass), 1 = the static casters into the
	 * static layer, 2 = the dynamic casters over the live layer. */
	using ShadowCache::Mode;
	Mode mode[kCascades] = { ShadowCache::Off, ShadowCache::Off, ShadowCache::Off };
	const bool cacheOn = cst.on && ShadowCache::ensure( r, s.map, s.tex );
	std::uint64_t setSig = 0, stateSig = 0, key[kCascades] = {};
	bool anyDyn = false;
	int nStatic = 0, srcOff[kCascades][2] = {}, oldSize[kCascades][2] = {};
	float zMap[kCascades][2] = {};
	int strip[kCascades][4][4] = {}, nStrip[kCascades] = {};
	if ( cacheOn ) {
		ShadowCache::Hash hs, ht;
		for ( const Caster & k : casters ) {
			anyDyn |= k.dynamic;
			if ( !k.dynamic ) {
				hs.w( k.sig );
				nStatic++;
			}
		}
		hs.w( s.groundCast ? 1u : 0u );
		if ( s.groundCast ) {
			hs.f( gz );
			hs.f( gxy[0] );
			hs.f( gxy[1] );
			hs.f( ghalf );
		}
		setSig = hs.h;
		ht.w( std::uint64_t( s.map ) );
		ht.f( s.slopeBias );
		ht.f( s.unitBias );
		ht.w( ( red( "nobias" ) ? 1u : 0u ) | ( GpuDriven::csm() ? 2u : 0u ) | ( offsetClamp ? 4u : 0u ) | ( cslopeZero ? 8u : 0u ) );	// SHADOWPAR1's clamp, OBJLODSHADOW1's probe
		ht.p( progAt );
		for ( const QString & rr : cs().reds )
			ht.w( qHash( rr ) );
		stateSig = ht.h;
	}
	bool anyScroll = false;
	int grew = 0;
	for ( int i = 0; i < kCascades && cacheOn; i++ ) {
		const Cascade & c = s.c[i];
		const ShadowCache::Layer & L = cst.c[i];
		ShadowCache::Hash hk;
		for ( float v : c.clipFromView )
			hk.f( v );
		hk.w( std::uint64_t( c.vw ) << 32 | std::uint64_t( c.vh ) );
		hk.t( vt );
		hk.w( setSig );
		hk.w( stateSig );
		key[i] = hk.h;
		mode[i] = ShadowCache::Full;
		if ( cst.red == 1 || !L.valid || L.map != s.map )
			continue;
		if ( cst.red == 2 || L.key == key[i] ) {
			mode[i] = ( L.liveIsStatic && !anyDyn ) ? ShadowCache::Skip : ShadowCache::Hit;
			continue;
		}
		// the scroll: same casters, state, light and lattice; the window moved by whole texels
		if ( !( cst.mask >> i & 1 ) || L.setSig != setSig || L.stateSig != stateSig || L.n != c.n || L.texel != c.texel
			|| std::memcmp( L.L, s.cL[i], sizeof( L.L ) ) != 0 )
			continue;
		const double fx = ( c.pb[0] + c.l - L.corner[0] ) / c.texel, fy = ( c.pb[1] + c.b - L.corner[1] ) / c.texel;
		const double rx = std::round( fx ), ry = std::round( fy );
		if ( std::fabs( fx - rx ) > 1e-3 || std::fabs( fy - ry ) > 1e-3 || std::fabs( rx ) > s.map || std::fabs( ry ) > s.map )
			continue;
		const int dx = int( rx ), dy = int( ry );
		// the old window in the new texels is [-dx, L.vw - dx) x [-dy, L.vh - dy)
		const int ix0 = std::max( 0, -dx ), ix1 = std::min( c.vw, L.vw - dx ), iy0 = std::max( 0, -dy ), iy1 = std::min( c.vh, L.vh - dy );
		if ( ix0 >= ix1 || iy0 >= iy1 )
			continue;
		const double wzNew = c.far - kNear;
		if ( wzNew <= 0.0 )
			continue;
		// lane FAR4 TERRBLACK1: the old layer holds only casters inside ITS light-depth range (no depth clamp), so a
		// range that grew (camera moved along the light, or turned) would lose the new slab's casters: draw it whole
		const double newNear = c.pb[2] + kNear;
		if ( cst.red != 3 && ( newNear < L.nearZ || newNear + wzNew > L.nearZ + L.wz ) ) {
			grew++;
			continue;
		}
		mode[i] = ShadowCache::Scroll;
		anyScroll = true;
		srcOff[i][0] = dx;
		srcOff[i][1] = dy;
		oldSize[i][0] = L.vw;
		oldSize[i][1] = L.vh;
		zMap[i][0] = float( L.wz / wzNew );
		zMap[i][1] = float( ( L.nearZ - ( c.pb[2] + kNear ) ) / wzNew );
		auto addStrip = [&]( int x0, int y0, int x1, int y1 ) {
			if ( x0 < x1 && y0 < y1 ) {
				int * o = strip[i][nStrip[i]++];
				o[0] = x0;
				o[1] = y0;
				o[2] = x1;
				o[3] = y1;
			}
		};
		addStrip( 0, 0, ix0, c.vh );
		addStrip( ix1, 0, c.vw, c.vh );
		addStrip( ix0, 0, ix1, iy0 );
		addStrip( ix0, iy1, ix1, c.vh );
	}
	NifSkopeOpenGLContext::Program * progRemap = nullptr;
	if ( anyScroll ) {
		progRemap = r->useProgram( "lookdev_csmremap.prog" );
		r->useProgram( "lookdev_csmdepth.prog" );
		for ( int i = 0; i < kCascades && !progRemap; i++ ) {
			if ( mode[i] == ShadowCache::Scroll )
				mode[i] = ShadowCache::Full;	// no remap program: draw it whole
		}
	}
	struct Job
	{
		int i = 0, set = 0;
		float m[16];
		int rc[4] = { 0, 0, 0, 0 };
		bool scissor = false;
	};
	std::vector<Job> jobs;
	std::vector<int> jobsOf[kCascades][3];
	for ( int i = 0; i < kCascades; i++ ) {
		const Cascade & c = s.c[i];
		auto add = [&]( int set, const int * rc ) {
			Job j;
			j.i = i;
			j.set = set;
			if ( rc ) {
				ShadowCache::stripMatrix( c.clipFromView, c.vw, c.vh, rc, j.m );
				std::memcpy( j.rc, rc, sizeof( j.rc ) );
				j.scissor = true;
			} else {
				std::memcpy( j.m, c.clipFromView, sizeof( j.m ) );
			}
			jobsOf[i][set].push_back( int( jobs.size() ) );
			jobs.push_back( j );
		};
		switch ( mode[i] ) {
		case ShadowCache::Off:
			add( 0, nullptr );
			break;
		case ShadowCache::Full:
			add( 1, nullptr );
			break;
		case ShadowCache::Scroll:
			for ( int k = 0; k < nStrip[i]; k++ )
				add( 1, strip[i][k] );
			break;
		default:
			break;
		}
		if ( mode[i] != ShadowCache::Off && mode[i] != ShadowCache::Skip && anyDyn )
			add( 2, nullptr );
	}
	// set 0 is every caster; 1 the static ones; 2 the dynamic ones
	auto inSet = []( const Caster & k, int set ) { return set == 0 || ( set == 1 ) != k.dynamic; };

	/* lane GPUDRIVEN1: the casters' render queues (gl/cellqueue.h). Solid one-sided, solid two-sided and alpha-tested
	 * casters draw from the shared pool, each solid queue as one indirect multi-draw per cascade; skinned casters
	 * keep the per-draw loop below. Depth only with LEQUAL keeps the nearest depth in any order. */
	enum { QSolid, QTwoSided, QAlpha, QCount };
	std::vector<int> queue[QCount];
	std::vector<GpuDriven::PoolEntry> entry( casters.size() );
	std::vector<char> queued( casters.size(), 0 );
	NifSkopeOpenGLContext::Program * progQ = nullptr, * progAtQ = nullptr;
	struct Span { std::size_t first = 0, n = 0; };
	std::vector<std::array<Span, QCount>> span( jobs.size() );
	std::vector<Span> atSpan;
	qint64 qCalls = 0, qCommands = 0, perDraw = 0;
	if ( GpuDriven::csm() && !casters.empty() && !jobs.empty() && GpuDriven::poolBegin( r ) ) {
		progQ = r->useProgram( "lookdev_csmdepth_q.prog" );
		if ( progAt )
			progAtQ = r->useProgram( "lookdev_csmdepth_atq.prog" );
		r->useProgram( "lookdev_csmdepth.prog" );
	}
	if ( progQ ) {
		for ( std::size_t n = 0; n < casters.size(); n++ ) {
			const Caster & k = casters[n];
			if ( !k.skinned.empty() || k.sh->wwGpuSkinned() || k.sh->wwFarInstData() )
				continue;
			const bool at = progAt && k.alphaTested;
			if ( ( at && !progAtQ ) || !GpuDriven::poolEnsure( r, k.sh, entry[n] ) || ( at && !entry[n].uv ) )
				continue;
			queued[n] = 1;
			queue[at ? QAlpha : ( k.twoSided ? QTwoSided : QSolid )].push_back( int( n ) );
		}
		if ( GpuDriven::redOrder() ) {
			std::mt19937 g( 0x6d1u );
			for ( auto & q : queue )
				std::shuffle( q.begin(), q.end(), g );
		}
		// the frame's matrices, one per queued caster, and every job's commands, uploaded once
		std::vector<float> mats;
		std::vector<std::uint32_t> matOf( casters.size(), 0 );
		for ( const auto & q : queue ) {
			for ( int n : q ) {
				matOf[std::size_t( n )] = std::uint32_t( mats.size() / 16 );
				const Matrix4 m = casters[std::size_t( n )].sh->viewTrans().toMatrix4();
				mats.insert( mats.end(), m.data(), m.data() + 16 );
			}
		}
		std::vector<GpuDriven::DrawCmd> cmds;
		atSpan.resize( jobs.size() * queue[QAlpha].size() );
		for ( std::size_t jb = 0; jb < jobs.size(); jb++ ) {
			const Job & J = jobs[jb];
			for ( int q = 0; q < QCount; q++ ) {
				span[jb][q].first = cmds.size();
				for ( std::size_t j = 0; j < queue[q].size(); j++ ) {
					const std::size_t n = std::size_t( queue[q][j] ), f = cmds.size();
					const GpuDriven::PoolEntry & e = entry[n];
					const Shape * sh = casters[n].sh;
					if ( !inSet( casters[n], J.set ) ) {
					} else if ( wwCellCullCaster( scene, sh, sh->id(), J.i, J.m, cullRanges ) ) {
						for ( std::size_t r2 = 0; r2 + 1 < cullRanges.size(); r2 += 2 ) {
							if ( cullRanges[r2 + 1] )
								cmds.push_back( { cullRanges[r2 + 1] * 3, 1, e.firstIndex + cullRanges[r2] * 3, e.baseVertex,
									matOf[n] } );
						}
					} else if ( !J.scissor || ShadowCache::boundsInside( scene, sh, J.m ) ) {
						cmds.push_back( { e.numIndices, 1, e.firstIndex, e.baseVertex, matOf[n] } );
					}
					if ( q == QAlpha )
						atSpan[jb * queue[QAlpha].size() + j] = { f, cmds.size() - f };
				}
				span[jb][q].n = cmds.size() - span[jb][q].first;
			}
		}
		GpuDriven::poolBind( r, mats, cmds );
	}

	// one job into whatever layer is attached
	auto drawJob = [&]( std::size_t jb ) {
		const Job & J = jobs[jb];
		const Cascade & c = s.c[J.i];
		fn->glViewport( 0, 0, c.vw, c.vh );
		if ( J.scissor ) {
			fn->glEnable( GL_SCISSOR_TEST );
			fn->glScissor( J.rc[0], J.rc[1], J.rc[2] - J.rc[0], J.rc[3] - J.rc[1] );
		}
		// lane OBJLODSHADOW1: probe WW_SHADOW_RED=cslopezero -- the same entry point with clamp 0 (no clamping), to split
		// cslope's +1.8 ms between the clamp call and the clamped bias (timing only)
		if ( offsetClamp )
			offsetClamp( s.slopeBias, s.unitBias, cslopeZero ? 0.0f
				: float( s.unitBias / 65536.0 + s.slopeBias * c.texel * 4.0 / std::max( c.far - kNear, 1e-6 ) ) );
		if ( lMvp >= 0 )
			fn->glUniformMatrix4fv( lMvp, 1, GL_FALSE, c.clipFromView );
		if ( progQ && ( span[jb][QSolid].n || span[jb][QTwoSided].n || !queue[QAlpha].empty() ) ) {
			// lane GPUDRIVEN1: the queues first, render state set once per queue
			GpuDriven::poolRebind( r );
			r->useProgram( "lookdev_csmdepth_q.prog" );
			if ( const int l = progQ->uniLocation( "csmClipFromView" ); l >= 0 )
				fn->glUniformMatrix4fv( l, 1, GL_FALSE, c.clipFromView );
			for ( int q : { int( QSolid ), int( QTwoSided ) } ) {
				if ( !span[jb][q].n )
					continue;
				q == QTwoSided ? fn->glDisable( GL_CULL_FACE ) : fn->glEnable( GL_CULL_FACE );
				GpuDriven::poolDraw( r, span[jb][q].first, span[jb][q].n );
				PassTime::countDraw( qint64( span[jb][q].n ) );
				qCalls++;
				qCommands += qint64( span[jb][q].n );
			}
			if ( !queue[QAlpha].empty() ) {
				// the base map differs per caster: one multi-draw each, geometry and matrices still from the pool
				r->useProgram( "lookdev_csmdepth_atq.prog" );
				if ( const int l = progAtQ->uniLocation( "csmClipFromView" ); l >= 0 )
					fn->glUniformMatrix4fv( l, 1, GL_FALSE, c.clipFromView );
				for ( std::size_t j = 0; j < queue[QAlpha].size(); j++ ) {
					const Span & sp = atSpan[jb * queue[QAlpha].size() + j];
					if ( !sp.n )
						continue;
					const Caster & k = casters[std::size_t( queue[QAlpha][j] )];
					k.twoSided ? fn->glDisable( GL_CULL_FACE ) : fn->glEnable( GL_CULL_FACE );
					int texunit = 0;
					const BSLightingShaderProperty * lsp = k.sh->wwLightingShader();
					BSShaderLightingProperty * bsp = k.sh->wwShaderLighting();
					progAtQ->uniSampler( bsp, "BaseMap", 0, texunit, QString(), bsp->clampMode, QString() );
					progAtQ->uni2f( "uvScale", bsp->uvScale.x, bsp->uvScale.y );
					progAtQ->uni2f( "uvOffset", bsp->uvOffset.x, bsp->uvOffset.y );
					progAtQ->uni1f( "alpha", lsp ? lsp->alpha : 1.0f );
					progAtQ->uni1f( "alphaThreshold", k.sh->wwAlphaThreshold() );
					GpuDriven::poolDraw( r, sp.first, sp.n );
					PassTime::countDraw( qint64( sp.n ) );
					qCalls++;
					qCommands += qint64( sp.n );
				}
			}
			r->useProgram( "lookdev_csmdepth.prog" );
		}
		for ( int pass = 0; pass < ( progAt ? 2 : 1 ); pass++ ) {
		// pass 0: the solid casters (and every caster when there is no alpha-tested program); pass 1: alpha-tested
		NifSkopeOpenGLContext::Program * p = pass ? progAt : prog;
		if ( pass ) {
			r->useProgram( "lookdev_csmdepth_at.prog" );
			const int l = p->uniLocation( "csmClipFromView" );
			if ( l >= 0 )
				fn->glUniformMatrix4fv( l, 1, GL_FALSE, c.clipFromView );
		}
		for ( std::size_t kn = 0; kn < casters.size(); kn++ ) {
			const Caster & k = casters[kn];
			if ( ( progAt && k.alphaTested != bool( pass ) ) || queued[kn] || !inSet( k, J.set ) )
				continue;
			// lane SUNCELL1: a welded cell shape gives this cascade only the placements inside its own window
			const bool runs = wwCellCullCaster( scene, k.sh, k.sh->id(), J.i, J.m, cullRanges );
			if ( !runs && J.scissor && !ShadowCache::boundsInside( scene, k.sh, J.m ) )
				continue;
			perDraw++;
			if ( k.twoSided )
				fn->glDisable( GL_CULL_FACE );
			else
				fn->glEnable( GL_CULL_FACE );
			p->uni4m( "modelViewMatrix", k.sh->viewTrans().toMatrix4() );
			const float * pos = k.skinned.empty() ? &( k.sh->verts.constFirst()[0] ) : &( k.skinned.front()[0] );
			const float * attrv[8] = { pos, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
			std::uint64_t mask = 3;
			if ( pass ) {
				int texunit = 0;
				const BSLightingShaderProperty * lsp = k.sh->wwLightingShader();
				BSShaderLightingProperty * bsp = k.sh->wwShaderLighting();
				p->uniSampler( bsp, "BaseMap", 0, texunit, QString(), bsp->clampMode, QString() );
				p->uni2f( "uvScale", bsp->uvScale.x, bsp->uvScale.y );
				p->uni2f( "uvOffset", bsp->uvOffset.x, bsp->uvOffset.y );
				p->uni1f( "alpha", lsp ? lsp->alpha : 1.0f );
				p->uni1f( "alphaThreshold", k.sh->wwAlphaThreshold() );
				if ( !k.sh->wwFarInstData() )
					attrv[7] = &( k.sh->coords.constFirst().constFirst()[0] );
				mask = 0x20000003ULL;
			}
			if ( k.sh->wwFarInstData() ) {
				// lane FARDRAW1: the ring's tiles inside this cascade, one indirect call (gl/cellfarinst.h)
				const Matrix4 mv = k.sh->viewTrans().toMatrix4();
				const float * a = c.clipFromView, * b = mv.data();
				float m[16];
				for ( int col = 0; col < 4; col++ )
					for ( int row = 0; row < 4; row++ )
						m[col * 4 + row] = a[row] * b[col * 4] + a[4 + row] * b[col * 4 + 1] + a[8 + row] * b[col * 4 + 2]
							+ a[12 + row] * b[col * 4 + 3];
				CellFarInst::drawCascade( r, *k.sh->wwFarInstData(), p, m );
				continue;
			}
			const float * const * attrs = attrv;
			if ( runs ) {
				cullIdx.clear();
				const std::uint16_t * src = reinterpret_cast<const std::uint16_t *>( k.sh->triangles.constData() );
				for ( size_t q = 0; q + 1 < cullRanges.size(); q += 2 )
					cullIdx.insert( cullIdx.end(), src + size_t( cullRanges[q] ) * 3,
						src + ( size_t( cullRanges[q] ) + size_t( cullRanges[q + 1] ) ) * 3 );
				if ( !cullIdx.empty() ) {
					r->drawShape( (unsigned int) ( k.sh->verts.size() ), mask, (unsigned int) cullIdx.size(),
						GL_TRIANGLES, GL_UNSIGNED_SHORT, attrs, cullIdx.data() );
					PassTime::countDraw( 1 );
				}
				continue;
			}
			r->drawShape( (unsigned int) ( k.sh->verts.size() ), mask, (unsigned int) ( k.sh->triangles.size() * 3 ),
				GL_TRIANGLES, GL_UNSIGNED_SHORT, attrs, k.sh->triangles.constData() );
			PassTime::countDraw( 1 );
		}
		}
		if ( progAt )
			r->useProgram( "lookdev_csmdepth.prog" );
		if ( s.groundCast && J.set != 2 ) {
			// the ground quad, exactly as wwLookdevDrawGround places it (world space, Z up); static
			fn->glEnable( GL_CULL_FACE );
			prog->uni4m( "modelViewMatrix", vt.toMatrix4() );
			const float x0 = gxy[0] - ghalf, x1 = gxy[0] + ghalf, y0 = gxy[1] - ghalf, y1 = gxy[1] + ghalf;
			const float quad[12] = { x0, y0, gz, x1, y0, gz, x0, y1, gz, x1, y1, gz };
			static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
			const float * attrs = quad;
			r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
		}
		if ( J.scissor )
			fn->glDisable( GL_SCISSOR_TEST );
	};
	auto attach = [&]( unsigned int tex, int layer ) {
		fn->glFramebufferTextureLayer( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, tex, 0, layer );
		if ( fn->glCheckFramebufferStatus( GL_DRAW_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
			complete = false;
		return complete;
	};
	auto clearAll = [&]() {
		fn->glViewport( 0, 0, s.map, s.map );
		fn->glClearDepth( 1.0 );
		fn->glClear( GL_DEPTH_BUFFER_BIT );
	};
	// the whole layer (the cleared margin too) from (tex, layer) into the attached one: depth bits, exact
	auto copyFrom = [&]( unsigned int tex, int layer ) {
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, cst.fbo );
		fn->glFramebufferTextureLayer( GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, tex, 0, layer );
		fn->glReadBuffer( GL_NONE );
		fn->glBlitFramebuffer( 0, 0, s.map, s.map, 0, 0, s.map, s.map, GL_DEPTH_BUFFER_BIT, GL_NEAREST );
	};
	auto remap = [&]( int i ) {
		const Cascade & c = s.c[i];
		r->useProgram( "lookdev_csmremap.prog" );
		progRemap->uni1i( "srcMap", kTextureUnit );
		progRemap->uni1i( "srcX", srcOff[i][0] );
		progRemap->uni1i( "srcY", srcOff[i][1] );
		progRemap->uni1i( "srcW", oldSize[i][0] );
		progRemap->uni1i( "srcH", oldSize[i][1] );
		progRemap->uni1f( "zScale", zMap[i][0] );
		progRemap->uni1f( "zBias", zMap[i][1] );
		GLint prevActive = 0, prevTex = 0, prevVao = 0;
		fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kTextureUnit ) );
		fn->glGetIntegerv( GL_TEXTURE_BINDING_2D_ARRAY, &prevTex );
		fn->glBindTexture( GL_TEXTURE_2D_ARRAY, cst.scratch );
		fn->glGetIntegerv( GL_VERTEX_ARRAY_BINDING, &prevVao );
		fn->glBindVertexArray( cst.vao );
		fn->glDisable( GL_CULL_FACE );
		fn->glDisable( GL_POLYGON_OFFSET_FILL );
		fn->glDepthFunc( GL_ALWAYS );
		fn->glViewport( 0, 0, c.vw, c.vh );
		fn->glDrawArrays( GL_TRIANGLES, 0, 3 );
		fn->glDepthFunc( GL_LEQUAL );
		if ( !red( "nobias" ) )
			fn->glEnable( GL_POLYGON_OFFSET_FILL );
		fn->glBindVertexArray( GLuint( prevVao ) );
		fn->glBindTexture( GL_TEXTURE_2D_ARRAY, GLuint( prevTex ) );
		fn->glActiveTexture( GLenum( prevActive ) );
		r->useProgram( "lookdev_csmdepth.prog" );
	};
	auto runSet = [&]( int i, int set ) {
		for ( int jb : jobsOf[i][set] )
			drawJob( std::size_t( jb ) );
	};

	for ( int i = 0; i < kCascades && complete; i++ ) {
		const Cascade & c = s.c[i];
		ShadowCache::Layer & L = cst.c[i];
		switch ( mode[i] ) {
		case ShadowCache::Off:
			if ( attach( s.tex, i ) ) {
				clearAll();
				runSet( i, 0 );
			}
			break;
		case ShadowCache::Skip:
			break;
		case ShadowCache::Full:
			if ( attach( cst.tex, i ) ) {
				clearAll();
				runSet( i, 1 );
			}
			break;
		case ShadowCache::Scroll:
			// the old static layer to the scratch, then back remapped onto the moved window, then the new strips
			if ( attach( cst.scratch, 0 ) )
				copyFrom( cst.tex, i );
			if ( attach( cst.tex, i ) ) {
				clearAll();
				remap( i );
				runSet( i, 1 );
			}
			break;
		case ShadowCache::Hit:
			break;
		}
		if ( !complete || mode[i] == ShadowCache::Off || mode[i] == ShadowCache::Skip )
			continue;
		if ( mode[i] == ShadowCache::Full || mode[i] == ShadowCache::Scroll ) {
			L.valid = true;
			L.key = key[i];
			L.setSig = setSig;
			L.stateSig = stateSig;
			L.corner[0] = c.pb[0] + c.l;
			L.corner[1] = c.pb[1] + c.b;
			L.nearZ = c.pb[2] + kNear;
			L.wz = c.far - kNear;
			L.texel = c.texel;
			L.n = c.n;
			std::memcpy( L.L, s.cL[i], sizeof( L.L ) );
			L.vw = c.vw;
			L.vh = c.vh;
			L.map = s.map;
		}
		// the live layer: the static one, then the dynamic casters over it
		if ( attach( s.tex, i ) ) {
			copyFrom( cst.tex, i );
			runSet( i, 2 );
			L.liveIsStatic = !anyDyn;
		}
	}
	if ( cacheOn && !complete )
		ShadowCache::invalidate();
	if ( progQ )
		GpuDriven::poolUnbind( r );
	r->stopProgram();
	if ( GpuDriven::census() ) {
		GpuDriven::censusLine( "csm", QStringLiteral( "casters %1 solid %2 two-sided %3 alpha-test %4 per-draw %5 (draws); "
			"queue calls %6 commands %7; %8" ).arg( casters.size() ).arg( queue[QSolid].size() ).arg( queue[QTwoSided].size() )
			.arg( queue[QAlpha].size() ).arg( perDraw ).arg( qCalls ).arg( qCommands ).arg( GpuDriven::poolCensus( r ) ) );
	}
	if ( cst.census ) {
		static const char * const names[] = { "off", "skip", "hit", "full", "scroll" };
		GpuDriven::censusLine( "shadowcache", QStringLiteral( "c0 %1 c1 %2 c2 %3 static %4 dynamic %5 jobs %6 strips %7,%8 grew %9" )
			.arg( names[mode[0]] ).arg( names[mode[1]] ).arg( names[mode[2]] ).arg( nStatic )
			.arg( qint64( casters.size() ) - nStatic ).arg( qint64( jobs.size() ) ).arg( nStrip[1] ).arg( nStrip[2] ).arg( grew ) );
	}
	if ( cst.on )
		ShadowCache::sweep();

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
	csmDumpMap( s, r );
}

/* lane PERF3: res/shaders/ww_sunshadow.glsl's wwShadowFrame block, std140 (mat4[3] at 0, vec3 at 192 with the float
 * packed after it at 204, vec4s at 208 and 224, ints at 240 and 244) */
struct CsmFrameBlock
{
	float mat[48];
	float invRange[3];
	float force;
	float params[4];
	float split[4];
	qint32 probe;
	qint32 red;
	qint32 pad[2];
};
static_assert( sizeof( CsmFrameBlock ) == 256, "std140 wwShadowFrame" );

struct CsmFrameBuffer
{
	GLuint buf = 0;
	CsmFrameBlock last{};
	bool have = false;
	qint64 uploads = 0, kept = 0;
};

static void csmFrameUpload( Renderer * r, const CsmFrameBlock & b )
{
	static QHash<Renderer *, CsmFrameBuffer> bufs;
	CsmFrameBuffer & u = bufs[r];
	auto fn = r->fn;
	if ( !u.buf ) {
		fn->glGenBuffers( 1, &u.buf );
		fn->glBindBuffer( GL_UNIFORM_BUFFER, u.buf );
		fn->glBufferData( GL_UNIFORM_BUFFER, GLsizeiptr( sizeof( CsmFrameBlock ) ), nullptr, GL_DYNAMIC_DRAW );
		fn->glBindBuffer( GL_UNIFORM_BUFFER, 0 );
	}
	if ( !u.have || std::memcmp( &u.last, &b, sizeof( b ) ) != 0 ) {
		fn->glBindBuffer( GL_UNIFORM_BUFFER, u.buf );
		fn->glBufferSubData( GL_UNIFORM_BUFFER, 0, GLsizeiptr( sizeof( b ) ), &b );
		fn->glBindBuffer( GL_UNIFORM_BUFFER, 0 );
		u.last = b;
		u.have = true;
		if ( ++u.uploads % 600 == 1 )
			std::fprintf( stderr, "ww frame ubo: uploads=%lld kept=%lld\n", (long long)u.uploads, (long long)u.kept );
	} else {
		u.kept++;
	}
	fn->glBindBufferBase( GL_UNIFORM_BUFFER, 2, u.buf );
}

void wwSunShadowUniforms( Scene * scene )
{
	PassTime::DrawCpuScope cpuT( PassTime::CpuSunUni );	// lane PERF2
	if ( !scene || !scene->renderer )
		return;
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->getCurrentProgram();
	if ( !prog || prog->uniLocation( "csmMap" ) < 0 )
		return;
	const CsmState & s = cs();
	auto fn = r->fn;
	WwCellUni::Units<std::remove_pointer_t<decltype( fn )>> u( fn, wwCellUniDsa() );	// lane CELLUNI2: no query with DSA
	u.bind( kTextureUnit, GL_TEXTURE_2D_ARRAY, s.valid ? s.tex : 0 );
	u.done();
	prog->uni1i( "csmMap", kTextureUnit );
	const bool nobias = red( "nobias" );
	// red "bigbias": a 600-unit receiver offset, the peter-panning control (the shadow must detach)
	const float offA = nobias ? 0.0f : red( "bigbias" ) ? 600.0f : kOffsetA;
	const float offB = nobias ? 0.0f : red( "bigbias" ) ? 600.0f : kOffsetB;
	if ( prog->wwShadowFrame ) {
		// lane PERF3: the same values, once into the frame block (uploaded only when its bytes change)
		CsmFrameBlock b{};
		for ( int i = 0; i < kCascades; i++ )
			std::memcpy( b.mat + 16 * i, s.c[i].uvdFromView, sizeof( float ) * 16 );
		for ( int i = 0; i < kCascades; i++ )
			b.invRange[i] = float( 1.0 / ( s.c[i].far - kNear ) );
		b.force = s.force;
		const float params[4] = { float( 1.0 / s.sc ), float( s.D ), float( s.map ), float( s.blend ) };
		const float split[4] = { float( s.split[0] ), float( s.split[1] ), offA, offB };
		std::memcpy( b.params, params, sizeof( params ) );
		std::memcpy( b.split, split, sizeof( split ) );
		b.probe = s.probe;
		b.red = redBits();
		csmFrameUpload( r, b );
	} else {
		for ( int i = 0; i < kCascades; i++ ) {
			const int l = prog->uniLocation( "csmMat[%d]", i );
			if ( l >= 0 ) {
				fn->glUniformMatrix4fv( l, 1, GL_FALSE, s.c[i].uvdFromView );
				WwCellUni::wrote( prog->wwUni, l, 1 );	// lane CELLUNI2: a raw write
			}
		}
		prog->uni3f_l( prog->uniLocation( "csmInvRange" ), float( 1.0 / ( s.c[0].far - kNear ) ),
			float( 1.0 / ( s.c[1].far - kNear ) ), float( 1.0 / ( s.c[2].far - kNear ) ) );
		prog->uni4f( "csmParams", FloatVector4( float( 1.0 / s.sc ), float( s.D ), float( s.map ), float( s.blend ) ) );
		prog->uni4f( "csmSplitOffset", FloatVector4( float( s.split[0] ), float( s.split[1] ), offA, offB ) );
		prog->uni1i( "csmProbe", s.probe );
		prog->uni1f( "csmForce", s.force );
		prog->uni1i( "csmRed", redBits() );
	}
	// lane TERRSHADOW1: the composition (ussOn = 0 keeps the code above exactly CSM1's)
	const float texel[3] = { float( s.c[0].texel ), float( s.c[1].texel ), float( s.c[2].texel ) };
	wwShadowSetUniforms( scene, prog, s.valid, texel );
}

void wwSunShadowForget()
{
	ShadowCache::State & s = ShadowCache::st();
	for ( int i = 0; i < kCascades; i++ ) {
		s.c[i] = ShadowCache::Layer();
		s.haveL[i] = false;
	}
	s.track.clear();
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
		.arg( v3( s.C ) + "|" + v3( s.fwd ) + "|" + g( s.tanX ) + "," + g( s.tanY ) + "|" + g( s.camNear ) + "|" + g( s.sc ) + "|" + g( s.camFar ) );
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
	o += QString( " csmat=%1 slopeclamp=%2" ).arg( s.atCasters ).arg( s.slopeClamp );	// lane VOLAIR1: -1 = no program (they cast solid)
	if ( s.swayCables >= 0 )
		o += QString( " cablesway=%1" ).arg( s.swayCables );	// lane FAR5: the wind cables cast where they sway
	// lane VOLAIR1: the receiver constants a judge needs to predict the factor from WW_CSM_DUMP's map
	o += QString( " recv=%1,%2,%3,%4" ).arg( g( kOffsetA ), g( kOffsetB ), g( kBlend ), g( kNear ) );
	o += QString( " casters=%1 tris=%2 ground=%3 probe=%4 force=%5 red=%6" ).arg( s.casters ).arg( s.tris ).arg( s.groundCast ? 1 : 0 )
		.arg( s.probe ).arg( s.force < 0.0f ? QStringLiteral( "off" ) : QString::number( double( s.force ) ) )
		.arg( s.reds.isEmpty() ? QStringLiteral( "none" ) : s.reds.join( QChar( '+' ) ) );
	return o;
}

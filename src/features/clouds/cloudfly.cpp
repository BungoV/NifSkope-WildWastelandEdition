#include "cloudfly.h"
#include "cloudflybakeroute.h"	// lane VKCLOUD6: the bake's slices on Vulkan, GL only on a refusal
#include "cloudflymath.h"
#include "cloudmap.h"
#include "cloudvol.h"
#include "cloudvolmath.h"
#include "features/sky/atmomath.h"
#include "features/vkframe/vkpresent.h"	// lane VKCLOUD6: the box volumes on the present device
#include "cloudinshare.h"	// lane VKCLOUD10: the "vk cloudflybake:" line's in-place names
#include "recx.h"
#include "wwswitch.h"
#include "wwsyncred.h"	// lane PERFFIX6: ^WW_SYNC_RED
#include "gl/glscene.h"
#include "gl/lookdevstage.h"
#include "gl/renderer.h"
#include "harnesswindow.h"

#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QSettings>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// lane CLOUD4: fly-through clouds (cloudfly.h). The box: res/shaders/features/clouds/cloud_flybake.frag (occupancy), cloud_flysdf.frag
// (the distance field, three separable passes), cloud_fly.frag (the march with the box).

namespace
{

namespace F = WwCloudFlyMath;
using WwAtmoMath::kUnitKm;
/* SKYLIGHT1 (bungo: the clouds "don't have a defined height"): the row ships Low, so the bands sit at world heights
 * (wwCloudFlyWorldHeights) and the camera can fly up to and through them; a new key, so a saved Off from before does not
 * keep the eye-relative bands. Red: WW_CLOUDFLY=0 (the row Off), WW_CLOUDFLY_RED=eyeheights (the bands above the eye). */
constexpr const char * kKey = "Settings/Render/Scene/Fly-through Clouds 2";
constexpr int kTierDef = 1;
constexpr GLenum kTimestamp = 0x8E28;	// GL_TIMESTAMP
constexpr double kMaxAgeS = 2.0;	// an older grid is rebuilt at once (the map's scroll is bounded by kSlackKm, not by time)

const char * tierName( int t )
{
	static const char * n[5] = { "off", "low", "medium", "high", "ultra" };
	return n[std::clamp( t, 0, 4 )];
}

struct FlState
{
	bool init = false;
	quint64 liveGen = 0;
	int tier = 0;
	bool pinned = false;
	QString last = QStringLiteral( "off" );
	// the .recx heights memo
	QString hKey;
	float h[32][2] = {};
	int hCount = 0;
	QString hSource = QStringLiteral( "none" );
	QElapsedTimer hAge;
};

bool red( const char * word );

FlState & cs()
{
	static FlState s;
	if ( !s.init || wwSwitchMovedFor( s.liveGen, { "WW_CLOUDFLY" } ) ) {
		s.init = true;
		s.liveGen = wwSwitchGen();
		s.pinned = false;
		const QString e = wwEnvStr( "WW_CLOUDFLY" ).trimmed().toLower();
		if ( !e.isEmpty() ) {
			bool ok = false;
			const int v = e.toInt( &ok );
			s.tier = ok ? std::clamp( v, 0, 4 )
				: e == QLatin1StringView( "low" ) ? 1 : e == QLatin1StringView( "medium" ) ? 2
				: e == QLatin1StringView( "high" ) ? 3 : e == QLatin1StringView( "ultra" ) ? 4 : 0;
			s.pinned = true;
		} else if ( wwHarnessRun() ) {
			/* SKYLIGHT1 round 2: any WW_ variable in the environment makes a "harness" run (nifskope.cpp wwHeadlessRun),
			 * bungo's flights included, so the row's default never reached them (cloudmap heights=eye: the bands rode with
			 * the camera and ran away from it). A harness now takes the shipped default; it pins WW_CLOUDFLY to measure
			 * another. Red WW_CLOUDFLY_RED=harnessoff: a harness pinned Off (before). */
			s.tier = red( "harnessoff" ) ? 0 : kTierDef;
			s.pinned = true;
		} else {
			s.tier = std::clamp( QSettings().value( QLatin1StringView( kKey ), kTierDef ).toInt(), 0, 4 );
		}
		s.hKey.clear();
		fprintf( stderr, "cloudfly: tier=%s (%s) heights=%s (red WW_CLOUDFLY_RED=harnessoff,eyeheights)\n", tierName( s.tier ),
			!e.isEmpty() ? "WW_CLOUDFLY" : wwHarnessRun() ? ( red( "harnessoff" ) ? "harness: Off, the red" : "harness: the shipped default" )
				: "the row's saved setting", s.tier >= 1 && !red( "eyeheights" ) ? "world" : "eye" );
	}
	return s;
}

bool red( const char * word )
{
	const QString r = wwEnvStr( "WW_CLOUDFLY_RED" ).trimmed().toLower();
	for ( const QString & w : r.split( QChar( ',' ), Qt::SkipEmptyParts ) )
		if ( w.trimmed() == QLatin1StringView( word ) )
			return true;
	return false;
}

struct Grid	// what one build was made from
{
	float org[3] = { 0, 0, 0 };
	double secs = 0.0;
	float cam[2] = { 0, 0 };
};

struct FlGpu
{
	GLuint fbo = 0;
	GLuint vol[4] = { 0, 0, 0, 0 };	// occupancy, ping-pong, two distance fields (one published, one building)
	int nxy = 0, nz = 0;
	int pub = -1;	// the published field (2 or 3), -1 none
	Grid pubG, buildG;
	int phase = -1, layer = 0;	// the rolling build: -1 idle, 0 bake, 1..3 the x, y, z passes
	QString key;
	bool active = false, noBox = false;
	float safe = 0.0f;
	int ageCells = 0;
	float camKm[3] = { 0, 0, 0 };
	int builds = 0, full = 0;
	GLuint q[4] = { 0, 0, 0, 0 };
	bool pending[2] = { false, false };
	int qcur = 0;
	double ms = -1.0;
	int frames = 0;
	/* lane VKCLOUD6: vol[] are shared targets on the present device (the bake's slices drawn on Vulkan); keys are heap bytes
	 * (QHash moves this struct): 0..3 vol[], 4..6 the bake inputs' twins (cvShape, cvDetail, cvMap) */
	bool shared = false, shareRefused = false;
	std::shared_ptr<char> keys { new char[7](), std::default_delete<char[]>() };
};

std::uintptr_t volKey( const FlGpu & g, int i )
{
	return reinterpret_cast<std::uintptr_t>( g.keys.get() + i );
}

/* lane VKCLOUD6: the "vk cloudflybake:" line, one a 5 s window while a bake was asked of Vulkan (red
 * WW_VKVIEW_CLOUDFLYBAKE_RED=1 or the view on GL: GL's own volumes, no line) */
void flyBakeSay()
{
	static WwVkPresent::FxStat sum;
	static auto at = std::chrono::steady_clock::now();
	const WwVkPresent::FxStat st = WwVkPresent::takeFlyBakeStat();
	sum.runs += st.runs;
	sum.crossings += st.crossings;
	sum.gl += st.gl;
	sum.fails += st.fails;
	sum.cpuMs += st.cpuMs;
	sum.maxMs = std::max( sum.maxMs, st.maxMs );
	sum.copies += st.copies;
	sum.inPlace |= st.inPlace;
	sum.pipelines = st.pipelines;
	sum.buildMs = st.buildMs;
	if ( !st.glWhy.empty() )
		sum.glWhy = st.glWhy;
	if ( !st.failWhy.empty() )
		sum.failWhy = st.failWhy;
	const auto now = std::chrono::steady_clock::now();
	if ( std::chrono::duration<double>( now - at ).count() < 5.0 )
		return;
	if ( sum.runs + sum.gl + sum.fails > 0 ) {
		char line[512];
		// lane VKCLOUD9: crossings and runs (passes) apart, copies per crossing and per run
		std::snprintf( line, sizeof( line ), "vk cloudflybake: on Vulkan %d runs in %d crossings (one render pass a slice; %d pipelines "
			"built in %.1f ms; %d input copies, %.2f a crossing, %.2f a run, read in place: %s; drawing thread %.3f ms a crossing, "
			"max %.3f)", sum.runs, sum.crossings, sum.pipelines, sum.buildMs, sum.copies,
			sum.crossings > 0 ? double( sum.copies ) / sum.crossings : 0.0, sum.runs > 0 ? double( sum.copies ) / sum.runs : 0.0,
			WwCloudInShare::inPlaceNames( sum.inPlace ).c_str(), sum.crossings > 0 ? sum.cpuMs / sum.crossings : 0.0, sum.maxMs );
		std::string s = line;
		if ( sum.gl > 0 )
			s += ", on GL " + std::to_string( sum.gl ) + " (last: " + sum.glWhy + ")";
		if ( sum.fails > 0 )
			s += ", refused " + std::to_string( sum.fails ) + " (last: " + sum.failWhy + ")";
		std::fprintf( stderr, "%s\n", s.c_str() );
	}
	at = now;
	sum = WwVkPresent::FxStat();
}

QHash<Renderer *, FlGpu> & gpus()
{
	static QHash<Renderer *, FlGpu> g;
	return g;
}

void quad( Renderer * r )
{
	static const float q[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = q;
	r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
}

void timeRead( NifSkopeOpenGLContext::GLFunctions * fn, FlGpu & g, int slot, bool wait )
{
	if ( !g.pending[slot] )
		return;
	if ( !wait || !wwSyncRed() ) {	// lane PERFFIX6: an unfinished query is read next frame
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
		fprintf( stderr, "cloudfly: gpu %.4f ms\n", g.ms );
}

//! the four R8 volumes at the preset's size (a new size drops the published field)
bool ensureVolumes( NifSkopeOpenGLContext::GLFunctions * fn, FlGpu & g, const F::Preset & p )
{
	if ( !g.fbo )
		fn->glGenFramebuffers( 1, &g.fbo );
	/* lane VKCLOUD6: while the view is on Vulkan the four volumes are shared targets (3D R8 with slice views), so the bake's
	 * slices draw on Vulkan (runPass); GL draws and reads the same names. Red WW_VKVIEW_CLOUDFLYBAKE_RED=1 (or a refusal):
	 * GL's own textures, as before. */
	const bool asked = WwVkPresent::shareWanted( "WW_VKVIEW_CLOUDFLYBAKE_RED" );
	if ( !asked )
		g.shareRefused = false;	// asked again later: one more try
	const bool want = asked && !g.shareRefused;
	bool live = g.shared;
	for ( int i = 0; i < 4 && live; i++ )
		live = WwVkPresent::sharedTargetIs( volKey( g, i ), g.vol[i] );
	if ( g.shared && !live )
		std::fill( g.vol, g.vol + 4, 0u );	// a shutdown dropped them (their GL names with them)
	if ( g.vol[0] && g.nxy == p.nxy && g.nz == p.nz && g.shared == want && ( !g.shared || live ) )
		return true;
	const bool wasShared = g.shared;
	if ( g.shared ) {
		for ( int i = 0; i < 4; i++ )
			WwVkPresent::sharedTargetDrop( volKey( g, i ) );
		std::fill( g.vol, g.vol + 4, 0u );
		g.shared = false;
	}
	if ( want ) {
		unsigned made[4] = {};
		std::string why;
		bool ok = true;
		for ( int i = 0; i < 4 && ok; i++ )
			ok = WwVkPresent::sharedTarget( volKey( g, i ), 1, p.nxy, p.nxy, p.nz, GL_R8, made[i], why );
		if ( ok ) {
			for ( int i = 0; i < 4; i++ ) {
				if ( g.vol[i] )
					fn->glDeleteTextures( 1, &g.vol[i] );
				g.vol[i] = made[i];
			}
			g.shared = true;
		} else {
			for ( int i = 0; i < 4; i++ )
				WwVkPresent::sharedTargetDrop( volKey( g, i ) );
			g.shareRefused = true;
			std::fprintf( stderr, "cloudfly: %dx%dx%d box volumes stay GL's own (shared target refused: %s)\n", p.nxy, p.nxy, p.nz, why.c_str() );
		}
	}
	for ( GLuint & t : g.vol ) {
		if ( !g.shared ) {
			if ( t )
				fn->glDeleteTextures( 1, &t );
			fn->glGenTextures( 1, &t );
		}
		fn->glBindTexture( GL_TEXTURE_3D, t );
		if ( !g.shared )
			fn->glTexImage3D( GL_TEXTURE_3D, 0, GL_R8, p.nxy, p.nxy, p.nz, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 0 );
		for ( GLenum w : { GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T, GL_TEXTURE_WRAP_R } )
			fn->glTexParameteri( GL_TEXTURE_3D, w, GL_CLAMP_TO_EDGE );
	}
	fn->glBindTexture( GL_TEXTURE_3D, 0 );
	if ( g.shared != wasShared || ( g.shared && ( g.nxy != p.nxy || g.nz != p.nz ) ) )
		std::fprintf( stderr, "cloudfly: %dx%dx%d box volumes %s\n", p.nxy, p.nxy, p.nz,
			g.shared ? "on the present device (shared with GL)" : "back on GL's own textures" );
	g.nxy = p.nxy;
	g.nz = p.nz;
	g.pub = -1;
	g.phase = -1;
	return g.vol[3] != 0;
}

/* one pass over layers [from, to) of the build: 0 = the occupancy of the density (bound at the build's clock), 1..3 =
 * the distance field along x, y, z (occupancy -> ping-pong -> occupancy -> the building field) */
/* lane VKCLOUD9: the full build's passes queued (runPass with a queue) and drawn in ONE crossing; a refusal there sends the
 * whole build to GL (runPass with glOnly) */
bool flyQueueRun( std::vector<WwVkPresent::FlyBakeRun> & q, std::string & why )
{
	if ( q.empty() )
		return true;
	const bool ok = WwVkPresent::flyBakeRunN( q.data(), int( q.size() ), why );
	q.clear();
	if ( ok )
		why.clear();
	return ok;
}

bool runPass( Scene * scene, FlGpu & g, const F::Preset & p, int pass, int from, int to,
	std::vector<WwVkPresent::FlyBakeRun> * queue = nullptr, const char * glOnly = nullptr, std::string * queueWhy = nullptr )
{
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	const int back = g.pub == 2 ? 3 : 2;
	// lane CLOUD5: Cloud Variety on, the occupancy bound of the variety density (cloud_flybake.frag WW_CLOUD_VARIETY)
	const bool variety = wwCloudVolVarietyOn() && wwCloudMap().variety;
	NifSkopeOpenGLContext::Program * prog = r->useProgram( pass == 0 ? ( variety ? "cloud_flybake_v.prog" : "cloud_flybake.prog" )
																	: "cloud_flysdf.prog" );
	if ( !prog )
		return false;
	GLuint dst = 0;
	int dstAt = 0, srcAt = 0;	// vol[] indices (lane VKCLOUD6: the shared targets' keys)
	if ( pass == 0 ) {
		if ( !wwCloudVolBindDensityAt( scene, 0, g.buildG.secs ) ) {
			r->stopProgram();
			return false;
		}
		prog->uni3f( "cfOrgKm", g.buildG.org[0], g.buildG.org[1], g.buildG.org[2] );
		prog->uni1f( "cfCellKm", p.cellKm );
		prog->uni1f( "cfMargin", F::kMarginS );
		prog->uni1f( "cfSlackKm", F::kSlackKm );
		prog->uni1i( "cfProbe", 0 );
	} else {
		srcAt = pass == 2 ? 1 : 0;
		dstAt = pass == 1 ? 1 : pass == 2 ? 0 : back;
		fn->glActiveTexture( GL_TEXTURE0 );
		fn->glBindTexture( GL_TEXTURE_3D, g.vol[srcAt] );
		prog->uni1i( "cfSrc", 0 );
		prog->uni1i( "cfAxis", pass - 1 );
		prog->uni3f( "cfDim", float( p.nxy ), float( p.nxy ), float( p.nz ) );
	}
	dst = g.vol[dstAt];
	/* lane VKCLOUD6: the slices on Vulkan (flyBakeRun, one render pass a slice into the shared volume) from GL's program state
	 * read back (fxTexOf / fxPack, cfLayer set per slice there); GL draws them only when the route hands them back (the
	 * volumes GL's own, Cloud Variety's program, a refusal). Gates: tests/vkframe/flybakeview.sh (the run vs GL),
	 * tests/vkframe/flybakesite.py (this site). */
	WwVkPresent::FlyBakeRun t;
	std::string glWhy;
	bool wantVk = g.shared;
	if ( wantVk && glOnly ) {
		wantVk = false;
		glWhy = std::string( "the build's one crossing was refused: " ) + glOnly;
	} else if ( wantVk && pass == 0 && variety ) {
		wantVk = false;
		glWhy = "Cloud Variety (no Vulkan twin of cloud_flybake_v)";
	} else if ( wantVk ) {
		t.pass = pass;
		t.dst = volKey( g, dstAt );
		if ( pass == 0 ) {
			static const char * const kName[WwVkPresent::kFlyBakeTex] = { "cvShape", "cvDetail", "cvMap" };
			static const int kType[WwVkPresent::kFlyBakeTex] = { 1, 1, 0 };
			for ( int k = 0; k < WwVkPresent::kFlyBakeTex && wantVk; k++ ) {
				WwVkPresent::FxTex & x = t.tex[k];
				wantVk = WwVkPresent::fxTexOf( fn, prog->id, kName[k], kType[k], x, glWhy );
				x.twin = volKey( g, 4 + k );
				x.still = k <= 1;	// the noise volumes: made once; the map moves
			}
			if ( wantVk )
				WwVkPresent::fxPack( fn, prog->id, WwVkPresent::kT_flybake, int( std::size( WwVkPresent::kT_flybake ) ), t.loose );
		} else {
			t.src = volKey( g, srcAt );
			WwVkPresent::fxPack( fn, prog->id, WwVkPresent::kT_flysdf, int( std::size( WwVkPresent::kT_flysdf ) ), t.loose );
		}
	}
	bool bound = false;
	auto glSlice = [&]( int k ) {
		if ( !bound ) {
			bound = true;
			fn->glBindFramebuffer( GL_FRAMEBUFFER, g.fbo );
			fn->glViewport( 0, 0, p.nxy, p.nxy );
		}
		fn->glFramebufferTextureLayer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, dst, 0, k );
		if ( k == from ) {
			fn->glDrawBuffer( GL_COLOR_ATTACHMENT0 );
			if ( fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
				return false;
		}
		prog->uni1i( "cfLayer", k );
		quad( r );
		return true;
	};
	std::string vkWhy;
	auto vkRun = [&]( int a, int b ) {
		t.from = a;
		t.to = b;
		if ( queue ) {
			queue->push_back( t );
			return true;
		}
		return WwVkPresent::flyBakeRun( t, vkWhy );
	};
	bool onVk = false;
	// a pass GL draws flushes the queued passes first (they come before it); a refused flush skips the pass (the build redoes on GL)
	const bool held = queue && !wantVk && !flyQueueRun( *queue, *queueWhy );
	bool ok = true;
	if ( !held )
		ok = wwFlyBakeRoute( wantVk, false, vkRun, glSlice, from, to, onVk );
	if ( g.shared && !onVk && !held )
		WwVkPresent::flyBakeOnGl( ( wantVk ? vkWhy : glWhy ).c_str() );
	r->stopProgram();
	for ( int u = 2; u >= 0; u-- ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
		fn->glBindTexture( GL_TEXTURE_3D, 0 );
	}
	return ok;
}

//! the bands the march clips to (cloud_march.frag lo, hi), km above z = 0
bool bandSpan( const WwCloudMap & m, float & lo, float & hi )
{
	float base[2], top[2], den[2];
	for ( int i = 0; i < 2; i++ ) {
		base[i] = m.band[i].baseZ * kUnitKm;
		top[i] = std::max( m.band[i].topZ * kUnitKm, base[i] + 1e-3f );
		den[i] = m.band[i].layers > 0 ? m.band[i].density : 0.0f;
	}
	lo = std::min( base[0], base[1] );
	hi = std::max( top[0], top[1] );
	if ( den[0] <= 0.0f )
		lo = base[1];
	if ( den[1] <= 0.0f )
		hi = top[0];
	return hi > lo && ( den[0] > 0.0f || den[1] > 0.0f );
}

} // namespace

int wwCloudFlyTier()
{
	return cs().tier;
}

void wwCloudFlySetTier( int tier )
{
	FlState & s = cs();
	if ( s.pinned )
		return;
	s.tier = std::clamp( tier, 0, 4 );
	QSettings().setValue( QLatin1StringView( kKey ), s.tier );
}

bool wwCloudFlyPinned()
{
	return cs().pinned;
}

bool wwCloudFlyWorldHeights()
{
	return cs().tier >= 1 && !red( "eyeheights" );
}

int wwCloudFlyLayerHeights( const QString & wkey, float out[32][2] )
{
	FlState & s = cs();
	if ( wkey != s.hKey || !s.hAge.isValid() || s.hAge.elapsed() > 1000 ) {
		s.hKey = wkey;
		s.hAge.start();
		std::memset( s.h, 0, sizeof( s.h ) );
		s.hCount = 0;
		s.hSource = QStringLiteral( "none" );
		quint32 formID = 0;
		QString edid, owner, dataDir;
		QStringList files;
		if ( wwLookdevWeatherId( formID, edid, owner, files, dataDir ) ) {
			const quint32 localId = ( formID >> 24 ) == 0xFEu ? ( formID & 0xFFFu ) : ( formID & 0xFFFFFFu );
			const WwRecxHit hit = wwRecxFind( wwRecxRoots( files, dataDir ), files, QStringLiteral( "Weathers" ),
				QStringLiteral( "weather" ), owner, localId, edid, QStringLiteral( "clouds" ) );
			if ( hit.ok ) {
				// WX1's keys (FO4-Extension-Formats recx/SPEC.md): layers.<n>.base / top, metres above sea level
				const QJsonObject L = hit.block.value( QStringLiteral( "layers" ) ).toObject();
				for ( auto it = L.constBegin(); it != L.constEnd(); ++it ) {
					bool ok = false;
					const int n = it.key().toInt( &ok );
					const QJsonObject o = it.value().toObject();
					if ( !ok || n < 0 || n > 31 || !o.contains( QStringLiteral( "base" ) ) || !o.contains( QStringLiteral( "top" ) ) )
						continue;
					const float b = float( std::clamp( o.value( QStringLiteral( "base" ) ).toDouble(), 600.0, 20000.0 ) );
					const float t = float( std::clamp( o.value( QStringLiteral( "top" ) ).toDouble(), 600.0, 20000.0 ) );
					if ( !( t > b ) )
						continue;
					s.h[n][0] = b;
					s.h[n][1] = t;
					s.hCount++;
				}
				s.hSource = QStringLiteral( "recx:%1" ).arg( s.hCount );
			}
		}
	}
	std::memcpy( out, s.h, sizeof( s.h ) );
	return s.hCount;
}

bool wwCloudFlyBandPin( float & baseM, float & topM )
{
	const QStringList v = wwEnvStr( "WW_CLOUDFLY_BAND" ).trimmed().split( QChar( ',' ), Qt::SkipEmptyParts );
	if ( v.size() != 2 )
		return false;
	baseM = std::clamp( v[0].trimmed().toFloat(), 600.0f, 20000.0f );
	topM = std::clamp( v[1].trimmed().toFloat(), 600.0f, 20000.0f );
	return topM > baseM;
}

void wwCloudFlyPrepare( Scene * scene, const float camKm[3], double secs )
{
	FlState & s = cs();
	if ( !scene || !scene->renderer )
		return;
	Renderer * r = scene->renderer;
	FlGpu & g = gpus()[r];
	g.active = false;
	g.noBox = false;
	std::copy( camKm, camKm + 3, g.camKm );
	if ( s.tier <= 0 ) {
		s.last = QStringLiteral( "off" );
		return;
	}
	if ( wwCloudVolTier() < 2 ) {
		s.last = QStringLiteral( "idle(Volume Clouds below Medium)" );
		return;
	}
	const WwCloudMap & m = wwCloudMap();
	float lo = 0.0f, hi = 0.0f;
	if ( !m.valid || !bandSpan( m, lo, hi ) ) {
		s.last = QStringLiteral( "idle(no band)" );
		return;
	}
	const QString hs = s.hSource;
	if ( !F::nearBand( camKm[2], lo, hi ) ) {
		s.last = QStringLiteral( "idle(camera %1 km, bands %2..%3 km, heights %4)" ).arg( double( camKm[2] ), 0, 'f', 3 )
			.arg( double( lo ), 0, 'f', 3 ).arg( double( hi ), 0, 'f', 3 ).arg( hs );
		return;
	}
	if ( red( "nobox" ) ) {
		g.active = true;
		g.noBox = true;
		s.last = QStringLiteral( "%1(RED=nobox, heights %2)" ).arg( QLatin1StringView( tierName( s.tier ) ) ).arg( hs );
		return;
	}
	const F::Preset & p = F::preset( s.tier );
	auto fn = r->fn;
	if ( !ensureVolumes( fn, g, p ) ) {
		s.last = QStringLiteral( "refused(volumes)" );
		return;
	}
	// a rebuild at once when the density's inputs move (the map's scroll and the wind are what the age covers)
	QString key = QStringLiteral( "%1" ).arg( s.tier );
	for ( int b = 0; b < 2; b++ )
		key += QStringLiteral( "|%1,%2,%3,%4,%5,%6" ).arg( std::lround( m.band[b].baseZ * kUnitKm * 100.0f ) )
				   .arg( std::lround( m.band[b].topZ * kUnitKm * 100.0f ) ).arg( std::lround( m.band[b].density * 100.0f ) )
				   .arg( std::lround( m.band[b].coverage * 256.0f ) ).arg( std::lround( m.band[b].sheet * 32.0f ) ).arg( m.band[b].layers );
	if ( wwCloudVolVarietyOn() && m.variety )	// lane CLOUD5: another density, another grid (absent when Off: the key is as before)
		key += QStringLiteral( "|var%1,%2" ).arg( std::lround( m.band[0].type * 1000.0f ) ).arg( std::lround( m.band[0].typeSpread * 1000.0f ) );
	const bool stale = red( "stale" ), harness = wwHarnessRun();
	float org[3];
	F::place( p, camKm, lo, hi, org );
	auto camMove = [&]( const Grid & G ) {
		return std::max( std::fabs( camKm[0] - G.cam[0] ), std::fabs( camKm[1] - G.cam[1] ) );
	};
	int age = 0;
	double ageS = 0.0;
	if ( g.pub >= 0 ) {
		ageS = secs - g.pubG.secs;
		age = F::ageCells( p, ageS, WwCloudVolMath::kWindKmS, camMove( g.pubG ) );
	}
	const bool samePlace = g.pub >= 0 && std::equal( org, org + 3, g.pubG.org );
	bool full = g.pub < 0 || key != g.key;
	if ( !stale && !full )
		full = age > F::kStaleCells || std::fabs( ageS ) > kMaxAgeS || ( harness && ( !samePlace || secs != g.pubG.secs ) );
	if ( stale && g.pub >= 0 && key == g.key )
		full = false;
	g.key = key;

	if ( !g.q[0] )
		fn->glGenQueries( 4, g.q );
	timeRead( fn, g, 1 - g.qcur, false );
	timeRead( fn, g, g.qcur, true );
	fn->glQueryCounter( g.q[g.qcur * 2], kTimestamp );
	bool ok = true;
	auto start = [&]() {
		std::copy( org, org + 3, g.buildG.org );
		g.buildG.secs = secs;
		g.buildG.cam[0] = camKm[0];
		g.buildG.cam[1] = camKm[1];
		g.phase = 0;
		g.layer = 0;
	};
	auto publish = [&]() {
		g.pub = g.pub == 2 ? 3 : 2;
		g.pubG = g.buildG;
		g.phase = -1;
		g.builds++;
	};
	if ( full ) {
		start();
		// lane VKCLOUD9: the four passes in one crossing; a refused crossing redoes the whole build on GL
		std::vector<WwVkPresent::FlyBakeRun> q;
		std::string qWhy;
		for ( int pass = 0; pass < 4 && ok && qWhy.empty(); pass++ )
			ok = runPass( scene, g, p, pass, 0, p.nz, &q, nullptr, &qWhy );
		if ( ok && qWhy.empty() )
			flyQueueRun( q, qWhy );
		for ( int pass = 0; pass < 4 && ok && !qWhy.empty(); pass++ )
			ok = runPass( scene, g, p, pass, 0, p.nz, nullptr, qWhy.c_str() );
		if ( ok ) {
			publish();
			g.full++;
		}
	} else if ( !stale ) {
		// the rolling refresh: bake, x, y, z over kFrames frames, then the new field replaces the published one
		if ( g.phase < 0 )
			start();
		const int n = F::layersPerFrame( p.nz );
		const int to = std::min( p.nz, g.layer + n );
		ok = runPass( scene, g, p, g.phase, g.layer, to );
		g.layer = to;
		if ( ok && g.layer >= p.nz ) {
			g.layer = 0;
			if ( ++g.phase > 3 )
				publish();
		}
	}
	flyBakeSay();	// lane VKCLOUD6
	fn->glQueryCounter( g.q[g.qcur * 2 + 1], kTimestamp );
	g.pending[g.qcur] = true;
	g.qcur = 1 - g.qcur;
	fn->glActiveTexture( GL_TEXTURE0 );
	if ( !ok ) {
		g.pub = -1;
		g.phase = -1;
		g.key.clear();
		s.last = QStringLiteral( "refused(cloud_flybake/cloud_flysdf did not draw)" );
		return;
	}
	if ( g.pub >= 0 ) {
		ageS = secs - g.pubG.secs;
		g.ageCells = stale ? 0 : F::ageCells( p, ageS, WwCloudVolMath::kWindKmS, camMove( g.pubG ) );
		g.safe = red( "sdfshrink" ) ? -1.0f : float( F::kSafe + g.ageCells );
		g.active = true;
	}
	QString reds;
	for ( const char * w : { "skip", "sdfshrink", "stale" } )
		if ( red( w ) )
			reds += QStringLiteral( ",RED=%1" ).arg( QLatin1StringView( w ) );
	s.last = QStringLiteral( "%1(box=%2x%2x%3,cell=%4km,step=%5km,org=%6,%7,%8km,builds=%9,full=%10,age=%11cells,safe=%12,"
							 "mem=%13MB,gpu=%14ms,heights=%15%16)" )
				 .arg( QLatin1StringView( tierName( s.tier ) ) ).arg( p.nxy ).arg( p.nz ).arg( double( p.cellKm ), 0, 'g', 3 )
				 .arg( double( p.stepKm ), 0, 'g', 3 ).arg( double( g.pubG.org[0] ), 0, 'f', 3 ).arg( double( g.pubG.org[1] ), 0, 'f', 3 )
				 .arg( double( g.pubG.org[2] ), 0, 'f', 3 ).arg( g.builds ).arg( g.full ).arg( g.ageCells ).arg( double( g.safe ), 0, 'g', 3 )
				 .arg( F::memoryBytes( p ) / 1048576.0, 0, 'f', 2 ).arg( g.ms, 0, 'f', 3 ).arg( hs ).arg( reds );
}

bool wwCloudFlyActive( Renderer * r )
{
	if ( cs().tier <= 0 || !r )
		return false;
	const auto it = gpus().constFind( r );
	return it != gpus().constEnd() && it->active;
}

void wwCloudFlyUniforms( Renderer * r, NifSkopeOpenGLContext::Program * prog, int unit )
{
	FlGpu & g = gpus()[r];
	const F::Preset & p = F::preset( cs().tier );
	auto fn = r->fn;
	const bool box = !g.noBox && g.pub >= 0;
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
	fn->glBindTexture( GL_TEXTURE_3D, box ? g.vol[g.pub] : 0 );
	prog->uni1i( "cfSdf", unit );
	prog->uni3f( "cfOrgKm", g.pubG.org[0], g.pubG.org[1], g.pubG.org[2] );
	prog->uni3f( "cfDim", float( g.nxy ), float( g.nxy ), float( g.nz ) );
	prog->uni1f( "cfCellKm", p.cellKm );
	prog->uni1f( "cfStepKm", p.stepKm );
	prog->uni1i( "cfMaxSteps", p.maxSteps );
	prog->uni1f( "cfSafe", g.safe );
	prog->uni1f( "cfOn", box ? 1.0f : 0.0f );
	prog->uni1f( "cfSkip", red( "skip" ) ? 0.0f : 1.0f );
	fn->glActiveTexture( GL_TEXTURE0 );
}

void wwCloudFlyRelease( Renderer * r, int unit )
{
	auto fn = r->fn;
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
	fn->glBindTexture( GL_TEXTURE_3D, 0 );
	fn->glActiveTexture( GL_TEXTURE0 );
}

QString wwCloudFlySummary()
{
	return cs().last;
}

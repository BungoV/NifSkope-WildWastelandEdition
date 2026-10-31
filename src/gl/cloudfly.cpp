#include "cloudfly.h"
#include "cloudflymath.h"
#include "cloudmap.h"
#include "cloudvol.h"
#include "cloudvolmath.h"
#include "atmomath.h"
#include "recx.h"
#include "wwswitch.h"
#include "gl/glscene.h"
#include "gl/lookdevstage.h"
#include "gl/renderer.h"
#include "harnesswindow.h"

#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

// lane CLOUD4: fly-through clouds (cloudfly.h). The box: res/shaders/cloud_flybake.frag (occupancy), cloud_flysdf.frag
// (the distance field, three separable passes), cloud_fly.frag (the march with the box).

namespace
{

namespace F = WwCloudFlyMath;
using WwAtmoMath::kUnitKm;
constexpr const char * kKey = "Settings/Render/Scene/Fly-through Clouds";
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
			s.tier = 0;	// a harness measures what it pins
			s.pinned = true;
		} else {
			s.tier = std::clamp( QSettings().value( QLatin1StringView( kKey ), 0 ).toInt(), 0, 4 );
		}
		s.hKey.clear();
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
};

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
		fprintf( stderr, "cloudfly: gpu %.4f ms\n", g.ms );
}

//! the four R8 volumes at the preset's size (a new size drops the published field)
bool ensureVolumes( NifSkopeOpenGLContext::GLFunctions * fn, FlGpu & g, const F::Preset & p )
{
	if ( !g.fbo )
		fn->glGenFramebuffers( 1, &g.fbo );
	if ( g.vol[0] && g.nxy == p.nxy && g.nz == p.nz )
		return true;
	for ( GLuint & t : g.vol ) {
		if ( t )
			fn->glDeleteTextures( 1, &t );
		fn->glGenTextures( 1, &t );
		fn->glBindTexture( GL_TEXTURE_3D, t );
		fn->glTexImage3D( GL_TEXTURE_3D, 0, GL_R8, p.nxy, p.nxy, p.nz, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 0 );
		for ( GLenum w : { GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T, GL_TEXTURE_WRAP_R } )
			fn->glTexParameteri( GL_TEXTURE_3D, w, GL_CLAMP_TO_EDGE );
	}
	fn->glBindTexture( GL_TEXTURE_3D, 0 );
	g.nxy = p.nxy;
	g.nz = p.nz;
	g.pub = -1;
	g.phase = -1;
	return g.vol[3] != 0;
}

/* one pass over layers [from, to) of the build: 0 = the occupancy of the density (bound at the build's clock), 1..3 =
 * the distance field along x, y, z (occupancy -> ping-pong -> occupancy -> the building field) */
bool runPass( Scene * scene, FlGpu & g, const F::Preset & p, int pass, int from, int to )
{
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	const int back = g.pub == 2 ? 3 : 2;
	// lane CLOUD5: Cloud Variety on, the occupancy bound of the variety density (cloud_flybake.frag WW_CLOUD_VARIETY)
	// lane CLOUDSHAPE1: Cloud Shapes on, the bound of its density (cloud_flybake.frag WW_CLOUD_SHAPE; supersedes Variety)
	const bool shape = wwCloudVolShapeOn();
	const bool variety = !shape && wwCloudVolVarietyOn() && wwCloudMap().variety;
	NifSkopeOpenGLContext::Program * prog = r->useProgram( pass == 0 ? ( shape ? "cloud_flybake_s.prog"
																			  : variety ? "cloud_flybake_v.prog" : "cloud_flybake.prog" )
																	: "cloud_flysdf.prog" );
	if ( !prog )
		return false;
	GLuint dst = 0;
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
		dst = g.vol[0];
	} else {
		const GLuint src = pass == 1 ? g.vol[0] : pass == 2 ? g.vol[1] : g.vol[0];
		dst = pass == 1 ? g.vol[1] : pass == 2 ? g.vol[0] : g.vol[back];
		fn->glActiveTexture( GL_TEXTURE0 );
		fn->glBindTexture( GL_TEXTURE_3D, src );
		prog->uni1i( "cfSrc", 0 );
		prog->uni1i( "cfAxis", pass - 1 );
		prog->uni3f( "cfDim", float( p.nxy ), float( p.nxy ), float( p.nz ) );
	}
	fn->glBindFramebuffer( GL_FRAMEBUFFER, g.fbo );
	fn->glViewport( 0, 0, p.nxy, p.nxy );
	bool ok = true;
	for ( int k = from; k < to && ok; k++ ) {
		fn->glFramebufferTextureLayer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, dst, 0, k );
		if ( k == from ) {
			fn->glDrawBuffer( GL_COLOR_ATTACHMENT0 );
			ok = fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE;
		}
		if ( ok ) {
			prog->uni1i( "cfLayer", k );
			quad( r );
		}
	}
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
	if ( wwCloudVolShapeOn() )	// lane CLOUDSHAPE1: another density, another grid (its type moves the domes)
		key += QStringLiteral( "|shp%1" ).arg( std::lround( m.band[0].type * 1000.0f ) );
	else if ( wwCloudVolVarietyOn() && m.variety )	// lane CLOUD5: another density, another grid (absent when Off: the key is as before)
		key +=QStringLiteral( "|var%1,%2" ).arg( std::lround( m.band[0].type * 1000.0f ) ).arg( std::lround( m.band[0].typeSpread * 1000.0f ) );
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
		for ( int pass = 0; pass < 4 && ok; pass++ )
			ok = runPass( scene, g, p, pass, 0, p.nz );
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

/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

/* lane MODULES1 (lights): the lamp half of the cell light law, split out of gl/celllights.cpp: flicker, XEMI
 * lights, gobo patterns, fog scale, .nifx fixture lights, the lamp cube shadows and the light spheres. */
#include "gl/celllights.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store

#include "cellclustergl.h"
#include "gl/alphabgsm.h"	// the side passes' alpha-test census
#include "gl/cellssr.h"
#include "features/air/cellvolair.h"
#include "features/shadows/shadowset.h"
#include "gl/glnode.h"
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "gl/glproperty.h"	// lane GAPFIX1: alpha-tested cube casters read the base map
#include "gl/renderer.h"
#include "gl/cellhdr.h"
#include "gl/cellbloom.h"	// lane BLOOM1b
#include "features/ao/cellaodecalgl.h"	// lane AODECAL1
#include "features/ao/cellaomethod.h"
#include "features/near/cellcull.h"	// wwCellDecalMappedHas
#include "features/shared/cellpasstime.h"	// sub-stage timers
#include "features/near/celldrawstate.h"
#include "gl/lookdevstage.h"
#include "gl/cellwater.h"	// water casts no cube shadow
#include "gl/cellunderwater.h"	// the WATR INAM imagespace under water
#include "esmwater.h"
#include "features/gi/cellwatergi.h"	// water reads no probe GI
#include "celllampshadow.h"	// lane LAMPSHADOW1
#include "cubefacecull.h"	// lane PERFFIX2 b: a caster draws only into the cube faces it can reach
#include "features/air/airlampatlas.h"	// lane MODULES1 route (a): the air's lamp atlas draws these casters
#include "lampslice.h"	// lane PERFFIX2 b: the lamp store's first stores spread over frames
#include "gl/celllooks.h"	// lane GAPLOOK1
#include "gl/celllensflare.h"	// lane LIGHTS3 (LENSFLARE1): its echo
#include "cellfxlit.h"	// lane LIGHTS4: a placement's emittance colour
#include "gamemanager.h"	// lane SUNCELL1: the weather imagespace's LUT
#include "esmweather.h"	// lane WEATHERBLEND2: wwLutBlendN
#include "features/gi/probealbedo.h"	// lane LIGHTS5 (GOBO1): a spot's pattern
#include "recx.h"	// lane LIGHTS5 (FOGSCALE1): fog.scale
#include "cellnifxlights.h"	// lane NIFXLIGHT1: a fixture mesh's .nifx lights
#include "features/gi/rtddgicell.h"	// lane RTXGI1 M2: the ray-traced GI volume's read (rtddgiCellUniforms)
#include "gl/celluniprof.h"	// lane PERFFIX1: WW_CELL_UNI_PROF
#include "gl/cellunirec.h"	// lane PERFFIX1: the cell-uniform class record
#include "gl/cloudvol.h"	// lane LIGHTNING2: a storm's flash on the exterior ambient (wwLightningGround)
#include "cellworldanim.h"	// lane NIFXLIGHT3: the world clock a .nifx light's dimmer follows
#include "features/sky/atmosphere.h"	// lane FIX1101A (atmo): wwAtmoWbGain, the test white balance
#include "features/sky/cellsunlit.h"	// lane INTSKY1: the sunlit interior rule
#include "cellflickclock.h"	// lane PERFFIX2 a: the flicker's phase on the world clock

#include <QDateTime>
#include <QDir>	// lane INTSKY1: the Cells file in the census
#include <QJsonValue>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSettings>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include "gl/cellunits.h"
#include "gl/celluniform.h"	// lane CELLUNI1
#include <QOpenGLContext>
#include <cstring>
#include <map>
#include <vector>
#include <unordered_map>	// lane PERFFIX1: the cell-uniform class records
#include "celllights_p.h"

using namespace WwCellLightsP;

/* ---- lane LIGHTFLICKER1: the light flicker (celllights.h) ----
 * Matches the game's flicker / pulse behavior, measured. s = frame seconds x Period; 512-entry sin / cos tables
 * indexed int(x 512 / 2 pi) & 511. Phases p (per ref; start 1, 0, 0) wrap at 2 pi.
 *  flicker (0x8): p += random(1.1, 13.1), (1.2, 13.2), (1.3, 19.3) x s; q = p + (1.7, 0.5, 0.3);
 *    o = Movement x sin q0 x sin q1 / 2 (0 unless o + Movement > 0); local offset o (sin q0, sin q1, sin q2);
 *    v = clamp((sin 1.3 q1 + 1) / 2 x (sin 1.1 q0 + 1) / 2 / 3 + 0.2 sin 3 q2, -1, 1)
 *  pulse (0x80 | 0x100): p0 += s; c, n = cos, sin p0; v = c; local offset Movement (c, n, n c)
 *  dimmer = 1 - A + A v with A = Intensity / 2 (x the fade, which the colour already holds). */
namespace WwCellLightsP
{
QHash<const void *, FlickDoc> & flickDocs()
{
	static QHash<const void *, FlickDoc> d;
	return d;
}
bool flickLive( Scene * scene )
{
	const ClState & s = st();
	return s.flickerOn && !s.flickerRed && wwCellLightsWanted( scene );
}
//! lane NIFXLIGHT3: what a frame changes: 1 flicker, 2 the .nifx dimmer tracks (World animation row), 4 bulbs flicker
int frameMode( Scene * scene )
{
	const int red = wwNifxLightsRed();
	const bool fl = flickLive( scene );
	const bool dl = wwLookOn( WwLook::NifxLights ) && wwCellWorldAnimOn() && !( red & WwNifxRedStaticDim ) && wwCellLightsWanted( scene );
	return ( fl ? 1 : 0 ) | ( dl ? 2 : 0 ) | ( fl && !( red & WwNifxRedNoBulbFlicker ) ? 4 : 0 );
}
}	// namespace

bool wwCellFlickerOn()
{
	return st().flickerOn;
}

void wwCellFlickerSetOn( bool on )
{
	ClState & s = st();
	if ( s.flickerPinned )
		return;
	s.flickerOn = on;
	QSettings().setValue( QStringLiteral( "WW/CellFlicker2" ), on );
}

void wwCellFlickerFrame( Scene * scene )
{
	using Clock = std::chrono::steady_clock;
	static Clock::time_point last = Clock::now();
	const Clock::time_point now = Clock::now();
	float dt = std::clamp( std::chrono::duration<float>( now - last ).count(), 0.0f, 0.25f );
	last = now;
	const int mode = frameMode( scene );
	if ( !mode ) {
		const auto was = flickDocs().find( scene->nifModel );
		if ( was != flickDocs().end() ) {
			was->mode = 0;	// lane NIFXLIGHT3: nothing live; the next live frame lists the lights again
			was->version = -1;
		}
		return;
	}
	const ClState & s = st();
	if ( s.flickerStep > 0.0f )
		dt = s.flickerStep;
	const WwCellLighting * L = wwCellLightsFor( scene->nifModel );
	FlickDoc & d = flickDocs()[scene->nifModel];
	const int ver = s.version.value( scene->nifModel );
	const qsizetype nLit = L->lights.size();
	if ( d.version != ver || d.mode != mode ) {
		d.version = ver;
		d.mode = mode;
		d.idx = wwNifxFrameList( *L, mode & 1, mode & 4, mode & 2, &d.flickN, &d.dimN );
		d.moving = 0;
		for ( const int i : d.idx )
			if ( i < nLit && ( mode & 1 ) && L->lights.at( i ).flicker )
				d.moving += L->lights.at( i ).flickerMovement != 0.0f && !L->lights.at( i ).shadow ? 1 : 0;
		d.val.assign( size_t( nLit + L->nifxBulbs.size() ) * 4, 0.0f );
		for ( size_t i = 0; i < d.val.size(); i += 4 )
			d.val[i] = 1.0f;
	}
	if ( d.idx.empty() )
		return;
	const double worldT = ( mode & 2 ) ? wwCellWorldSeconds() : 0.0;
	/* lane PERFFIX2 a: the flicker's phase is a function of the world clock (the wall clock in a live window, the
	 * pinned WW_CELL_WORLDANIM_TIME under a harness), so two rows at one time draw the same lights. The red and the
	 * STEP pin keep LIGHTFLICKER1's one step of this painted frame's dt. */
	const bool perFrame = s.flickerClockRed || s.flickerStep > 0.0f;
	const double clockT = perFrame ? 0.0 : wwCellWorldSeconds();
	const long long catchUp = wwCellWorldClockMoves() ? WwFlick::kLiveCatchUp : -1;
	const std::vector<float> was = d.val;	// a frame that changes nothing patches nothing (the stamp stays)
	QFile dump( s.flickerDump );
	const bool dumping = !s.flickerDump.isEmpty() && dump.open( QIODevice::Append | QIODevice::Text );
	for ( int i : d.idx ) {
		const WwCellLight & l = i < nLit ? L->lights.at( i ) : L->nifxBulbs.at( i - nLit );
		float * out = d.val.data() + size_t( i ) * 4;
		// lane NIFXLIGHT3: a bulb flickers like the placed light it took over (its fields came with the carrier)
		if ( !( ( mode & 1 ) && l.flicker && ( i < nLit || ( mode & 4 ) ) ) ) {
			out[0] = ( mode & 2 ) ? wwNifxDimFactor( l, worldT ) : 1.0f;
			out[1] = out[2] = out[3] = 0.0f;
			if ( dumping )
				dump.write( QStringLiteral( "%1 dim %2 %3 %4 %5\n" ).arg( d.frames ).arg( worldT, 0, 'f', 4 ).arg( i )
					.arg( l.ref, 8, 16, QLatin1Char( '0' ) ).arg( double( out[0] ), 0, 'f', 6 ).toLatin1() );
			continue;
		}
		FlickPhase & ph = d.phase[l.ref];
		const quint32 seed = WwFlick::seed( l.ref, i );
		if ( perFrame )
			WwFlick::stepFrame( ph, seed, l.flicker, l.flickerPeriod, dt );
		else
			WwFlick::toTime( ph, seed, l.flicker, l.flickerPeriod, clockT, catchUp );
		const float m = l.flickerMovement, A = l.flickerIntensity * 0.5f;
		float local[3];
		const float v = WwFlick::eval( ph, l.flicker, m, local );
		out[0] = ( 1.0f - A + A * v ) * ( ( mode & 2 ) ? wwNifxDimFactor( l, worldT ) : 1.0f );
		// a shadow-casting light keeps its place: its depth cube is drawn from there, not redrawn each frame
		for ( int k = 0; k < 3; k++ )
			out[1 + k] = l.shadow ? 0.0f
				: l.flickerAxes[0][k] * local[0] + l.flickerAxes[1][k] * local[1] + l.flickerAxes[2][k] * local[2];
		if ( dumping )
			dump.write( QStringLiteral( "%1 %2 %3 %4 %5 %6 %7 %8 %9\n" ).arg( d.frames ).arg( perFrame ? double( dt ) : clockT, 0, 'g', 6 ).arg( i )
				.arg( l.ref, 8, 16, QLatin1Char( '0' ) ).arg( l.flicker ).arg( double( out[0] ), 0, 'f', 6 )
				.arg( double( out[1] ), 0, 'f', 4 ).arg( double( out[2] ), 0, 'f', 4 ).arg( double( out[3] ), 0, 'f', 4 ).toLatin1() );
	}
	d.frames++;
	if ( perFrame || d.val != was )
		d.stamp++;
}

bool wwCellFlickerWantsFrames( Scene * scene )
{
	const int mode = frameMode( scene );
	if ( !mode )
		return false;
	const auto it = flickDocs().constFind( scene->nifModel );
	// before the first frame has listed them: ask once, the frame lists them
	if ( it == flickDocs().constEnd() || it->version != st().version.value( scene->nifModel ) || it->mode != mode )
		return true;
	// lane NIFXLIGHT3: a dimmer track moves only while the world clock does
	// lane PERFFIX2 a: so does the flicker now (its red and the STEP pin step every painted frame)
	const ClState & s = st();
	const bool flickMoves = s.flickerClockRed || s.flickerStep > 0.0f || wwCellWorldClockMoves();
	return ( ( mode & 1 ) && it->flickN && flickMoves ) || ( ( mode & 2 ) && it->dimN && wwCellWorldAnimWantsFrames( scene ) );
}

bool wwCellFlickerOf( const void * nif, qsizetype i, float * dimmer, float offset[3] )
{
	*dimmer = 1.0f;
	offset[0] = offset[1] = offset[2] = 0.0f;
	const ClState & s = st();
	const auto it = flickDocs().constFind( nif );
	// lane NIFXLIGHT3: the frame's values whenever a frame wrote them (the flicker, or a .nifx light's dimmer)
	if ( it == flickDocs().constEnd() || !it->mode || it->version != s.version.value( nif ) || i < 0 || size_t( i ) * 4 + 4 > it->val.size() )
		return false;
	const float * v = it->val.data() + size_t( i ) * 4;
	*dimmer = v[0];
	for ( int k = 0; k < 3; k++ )
		offset[k] = v[1 + k];
	return v[0] != 1.0f || v[1] != 0.0f || v[2] != 0.0f || v[3] != 0.0f;
}

namespace WwCellLightsP
{
QHash<const void *, XemiDoc> & xemiDocs()
{
	static QHash<const void *, XemiDoc> d;
	return d;
}
std::uint64_t xemiStampOf( const void * doc )
{
	const auto it = xemiDocs().constFind( doc );
	return it == xemiDocs().constEnd() ? 0 : it->stamp;
}
}	// namespace

void wwCellXemiLightsFrame( Scene * scene )
{
	if ( !scene || !wwCellLightsWanted( scene ) )
		return;
	const WwCellLighting * L = wwCellLightsFor( scene->nifModel );
	if ( !L )
		return;
	XemiDoc & d = xemiDocs()[scene->nifModel];
	const int ver = st().version.value( scene->nifModel );
	if ( d.version != ver ) {
		d.version = ver;
		d.idx.clear();
		for ( qsizetype i = 0; i < L->lights.size(); i++ )
			if ( L->lights.at( i ).xemi )
				d.idx.push_back( int( i ) );
		d.mul.assign( size_t( L->lights.size() ) * 3, 1.0f );
		d.stamp++;
	}
	if ( d.idx.empty() )
		return;
	QHash<quint32, std::array<float, 3>> byCode;	// a few sources for many lights: each colour read once a frame
	bool moved = false;
	int byLight = 0, byRegion = 0;
	for ( const int i : d.idx ) {
		const quint32 code = L->lights.at( i ).xemi;
		std::array<float, 3> f = { 1.0f, 1.0f, 1.0f };
		const auto c = byCode.constFind( code );
		if ( c != byCode.constEnd() ) {
			f = c.value();
		} else {
			float rgb[3];
			if ( wwLookOn( ( code & 0x02000000U ) ? WwLook::RegionEmittance : WwLook::PlacementEmittance )
				&& wwCellXemiCode( code, rgb ) )
				for ( int k = 0; k < 3; k++ )
					f[size_t( k )] = std::pow( std::max( rgb[k], 0.0f ), 2.2f );
			byCode.insert( code, f );
		}
		if ( f[0] != 1.0f || f[1] != 1.0f || f[2] != 1.0f )
			( ( code & 0x02000000U ) ? byRegion : byLight )++;
		for ( int k = 0; k < 3; k++ ) {
			float & m = d.mul[size_t( i ) * 3 + size_t( k )];
			if ( std::abs( m - f[size_t( k )] ) > 1.0e-4f ) {
				m = f[size_t( k )];
				moved = true;
			}
		}
	}
	if ( moved )
		d.stamp++;
	const QByteArray echo = QByteArray( "cell xemi lights: " ) + QByteArray::number( qulonglong( d.idx.size() ) )
		+ " with XEMI, recoloured " + QByteArray::number( byLight ) + " by a light, " + QByteArray::number( byRegion )
		+ " by a region (" + QByteArray::number( byCode.size() ) + " sources)";
	if ( echo != d.echo ) {
		d.echo = echo;
		std::fprintf( stderr, "%s\n", echo.constData() );
	}
}

const GoboPat & WwCellLightsP::goboPattern( const QString & dataRoot, const QString & path )
{
	static std::map<QString, GoboPat> cache;	// a map: references stay valid across inserts
	const QString key = dataRoot + QLatin1Char( '|' ) + path.toLower();
	const auto it = cache.find( key );
	if ( it != cache.end() )
		return it->second;
	GoboPat p;
	int w = 0, h = 0;
	std::vector<unsigned char> b;
	if ( ProbeAlbedo( dataRoot ).rgbBytes( path, &w, &h, &b, 128 ) && w > 0 && h > 0 ) {
		bool one = true;
		for ( size_t k = 0; k < b.size() && one; k += 3 )
			one = !b[k + 1] && !b[k + 2];
		p.w = w;
		p.h = h;
		p.rgb.resize( b.size() );
		double s[3] = { 0, 0, 0 };
		for ( size_t k = 0; k < b.size(); k++ ) {
			p.rgb[k] = float( one ? b[k - k % 3] : b[k] ) / 255.0f;
			s[k % 3] += p.rgb[k];
		}
		for ( int c = 0; c < 3; c++ )
			p.mean[c] = float( s[c] / ( double( w ) * double( h ) ) );
	}
	return cache.emplace( key, std::move( p ) ).first->second;
}

/* lane LIGHTS5 (FOGSCALE1): each light's fog.scale (.recx category Lights, block fog): a placed lamp's `reference` claim,
 * then (NIFXLIGHT1) a .nifx light's own fog.scale, then its LIGH's `light` claim, else 1. Pin WW_CELL_FOGSCALE:
 * off = the files ignored, a number = that scale on every light. Re-read when the files or the document move. */
void WwCellLightsP::fogScaleTick( Gpu & g, const void * doc, int version, const WwCellLighting & L )
{
	WW_SWITCH_STATIC( QByteArray, pin, wwEnv( "WW_CELL_FOGSCALE" ).trimmed() );
	const bool same = g.fogDoc == doc && g.fogVersion == version && g.fogSwitchGen == wwSwitchGen();
	if ( same && g.fogClock.isValid() && g.fogClock.elapsed() < 500 )
		return;
	g.fogClock.start();
	const QStringList roots = wwRecxRoots( L.recxPlugins, L.recxData );
	int files = 0;
	const quint64 gen = pin.isEmpty() ? wwRecxScan( roots, QStringLiteral( "Lights" ), &files ) : 0;
	if ( same && gen == g.fogRecxGen )
		return;
	g.fogDoc = doc;
	g.fogVersion = version;
	g.fogSwitchGen = wwSwitchGen();
	g.fogRecxGen = gen;
	const qsizetype nLit = L.lights.size(), n = nLit + L.nifxBulbs.size();	// lane NIFXLIGHT2: then the bulbs
	std::vector<float> v;
	int placed = 0, record = 0, mesh = 0;
	bool ok = false;
	const float forced = pin == "off" ? 1.0f : pin.toFloat( &ok );
	const bool nifx = !L.nifxBulbs.isEmpty()
		|| std::any_of( L.lights.cbegin(), L.lights.cend(), []( const WwCellLight & l ) { return l.tier >= 0; } );
	WW_SWITCH_STATIC( bool, redOrder, ( wwNifxRedOf( wwEnv( "WW_NIFX_LIGHTS_RED" ) ) & WwNifxRedPrecedence ) != 0 );
	if ( ok ) {
		v.assign( size_t( n ), std::clamp( forced, 0.0f, 8.0f ) );
	} else if ( pin.isEmpty() && ( files > 0 || nifx ) ) {
		v.assign( size_t( n ), 1.0f );
		auto scaleOf = []( const WwRecxHit & h, float & s ) {
			if ( !h.ok || !h.block.value( QStringLiteral( "scale" ) ).isDouble() )
				return false;
			s = std::clamp( float( h.block.value( QStringLiteral( "scale" ) ).toDouble() ), 0.0f, 8.0f );
			return true;
		};
		for ( qsizetype i = 0; i < n; i++ ) {
			const WwCellLight & l = i < nLit ? L.lights.at( i ) : L.nifxBulbs.at( i - nLit );
			float s = 1.0f;
			const bool own = l.tier >= 0 && l.nifxFog >= 0.0f;
			auto recordClaim = [&] {
				return files > 0 && ( l.tier < 0 || !l.recxBasePlugin.isEmpty() || !l.recxBaseEdid.isEmpty() ) && scaleOf( wwRecxFind( roots,
					L.recxPlugins, QStringLiteral( "Lights" ), QStringLiteral( "light" ), l.recxBasePlugin, l.recxBaseLocal,
					l.recxBaseEdid, QStringLiteral( "fog" ) ), s );
			};
			const int rung = wwNifxFogRung( [&] {
				return files > 0 && !l.recxRefPlugin.isEmpty() && scaleOf( wwRecxFind( roots, L.recxPlugins, QStringLiteral( "Lights" ),
					QStringLiteral( "reference" ), l.recxRefPlugin, l.recxRefLocal, QString(), QStringLiteral( "fog" ) ), s );
			}, own, recordClaim, redOrder );
			if ( rung == WwNifxFogRef ) {
				v[size_t( i )] = s;
				placed++;
			} else if ( rung == WwNifxFogMesh ) {
				v[size_t( i )] = std::clamp( l.nifxFog, 0.0f, 8.0f );
				mesh++;
			} else if ( rung == WwNifxFogRecord ) {
				v[size_t( i )] = s;
				record++;
			}
		}
		if ( placed + record + mesh == 0 )
			v.clear();
	}
	const QByteArray echo = QStringLiteral( "cell fogscale: %1 lights, %2 (placed lamp %3, light record %4, recx Lights files %5)" )
		.arg( n ).arg( ok ? QStringLiteral( "pinned %1" ).arg( double( v.empty() ? 1.0f : v[0] ) )
			: pin == "off" ? QStringLiteral( "files ignored (pin off)" ) : v.empty() ? QStringLiteral( "every light 1" )
			: QStringLiteral( "%1 scaled" ).arg( placed + record + mesh ) )
		.arg( placed ).arg( record ).arg( files ).toUtf8()
		+ ( nifx ? QStringLiteral( " .nifx light %1%2" ).arg( mesh ).arg( redOrder ? QStringLiteral( " (red precedence)" )
			: QString() ).toUtf8() : QByteArray() );
	static QByteArray said;
	if ( echo != said ) {
		said = echo;
		std::fprintf( stderr, "%s\n", echo.constData() );
	}
	if ( v != g.fogVals ) {
		g.fogVals = std::move( v );
		g.fogStamp++;
	}
}

int wwNifxLightsRed()
{
	WW_SWITCH_STATIC( int, red, wwNifxRedOf( wwEnv( "WW_NIFX_LIGHTS_RED" ) ) );
	return red;
}

/* lane NIFXLIGHT1: Mesh lights (.nifx), pin WW_CELL_NIFX_LIGHTS. Off: the document shows its lights as built. On: each
 * fixture's .nifx is read again when its size or time moves (every 500 ms at most) and applied to a copy of them. */
void WwCellLightsP::nifxTick( const void * nif )
{
	ClState & s = st();
	auto it = s.nifx.find( nif );
	if ( it == s.nifx.end() )
		return;
	ClNifx & x = *it;
	if ( !wwLookOn( WwLook::NifxLights ) ) {
		if ( x.applied ) {
			x.applied = false;
			x.key.clear();
			s.docs.insert( nif, x.base );
			s.version.insert( nif, s.nextVersion++ );
			s.nifxStamp.remove( nif );	// lane NIFXLIGHT2: the GI relights to the lights as built
			std::fprintf( stderr, "nifx lights: off, %lld lights as built\n", qint64( x.base.lights.size() ) );
		}
		x.ticked = false;
		return;
	}
	if ( x.ticked && x.clock.isValid() && x.clock.elapsed() < 500 )
		return;
	x.clock.start();
	x.ticked = true;
	const int red = wwNifxLightsRed();
	/* lane NIFXLIGHT3: a .nifx made, moved or removed after the build is seen here, no reopen: the folders the placed
	 * models' .nifx would sit in are looked at in turn (about 1.5 ms a tick; every folder when WW_NIFX_ROOT moves) */
	if ( !x.liveInit ) {
		wwNifxLiveInit( x.live, x.base.nifxFixtures );
		x.liveInit = true;
	}
	if ( !( red & WwNifxRedNoLiveNew ) ) {
		const qint64 found = x.live.found, lost = x.live.lost;
		const int moved = wwNifxLiveScan( x.live, x.base.nifxFixtures, { wwEnvStr( "WW_NIFX_ROOT" ), x.live.dataRoot }, 1.5,
			wwCellNifxModelLoad );
		if ( moved )
			std::fprintf( stderr, "nifx lights: live: %d models' .nifx changed (%lld found, %lld gone), %lld folders, %lld looks\n",
				moved, x.live.found - found, x.live.lost - lost, qint64( x.live.dirs.size() ), x.live.looks );
	}
	QStringList files;
	for ( const WwNifxFixture & f : x.base.nifxFixtures )
		if ( f.model && !files.contains( f.model->sidecar ) )
			files << f.model->sidecar;
	if ( files.isEmpty() ) {	// lane NIFXLIGHT3: no .nifx (yet): the lights as built, and no GI relight for nothing
		if ( x.applied ) {
			x.applied = false;
			s.docs.insert( nif, x.base );
			s.version.insert( nif, s.nextVersion++ );
			s.nifxStamp.insert( nif, s.nextNifxStamp++ );
			std::fprintf( stderr, "nifx lights: no .nifx left, %lld lights as built\n", qint64( x.base.lights.size() ) );
		}
		x.key.clear();
		return;
	}
	QByteArray key = QByteArray::number( red );
	for ( const QString & p : files ) {
		const QFileInfo fi( p );
		key += '|' + p.toUtf8() + ':' + QByteArray::number( fi.exists() ? fi.size() : -1 ) + ':'
			+ QByteArray::number( fi.exists() ? fi.lastModified().toMSecsSinceEpoch() : 0 );
	}
	if ( x.applied && key == x.key )
		return;
	QHash<QString, nifxl::Lights> docs;
	for ( const QString & p : files ) {
		QFile f( p );
		if ( !f.open( QIODevice::ReadOnly ) )
			continue;
		const QByteArray b = f.read( qint64( nifxl::kMaxBytes ) + 1 );
		docs.insert( p, nifxl::read( std::string( b.constData(), size_t( b.size() ) ) ) );
	}
	WwCellLighting L = x.base;
	QStringList census;
	wwNifxLightsApply( L, docs, red, &census );
	for ( const QString & c : census )
		std::fprintf( stderr, "%s\n", c.toUtf8().constData() );
	const QString dump = wwEnvStr( "WW_NIFX_LIGHTS_DUMP" );
	if ( !dump.isEmpty() ) {
		QFile o( dump );
		if ( o.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
			o.write( ( census.join( QLatin1Char( '\n' ) ) + QLatin1Char( '\n' ) + wwNifxLightsDump( L ) ).toUtf8() );
	}
	x.applied = true;
	x.key = key;
	s.docs.insert( nif, L );
	s.version.insert( nif, s.nextVersion++ );
	s.nifxStamp.insert( nif, s.nextNifxStamp++ );	// lane NIFXLIGHT2: src/features/gi/probesky.cpp relights the GI
}

//! WW_CELL_SHADOW_DUMP=<file>: the echo, rewritten whenever the slots change
static void shadowDump( Scene * scene )
{
	WW_SWITCH_STATIC( QString, path, QString::fromLocal8Bit( wwEnv( "WW_CELL_SHADOW_DUMP" ) ) );
	if ( path.isEmpty() )
		return;
	QFile f( path );
	if ( f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
		f.write( ( wwCellShadowEcho( scene ) + QLatin1Char( '\n' ) ).toUtf8() );
}

//! the depth cube array (kShadowSlots cubes of face x face) and its framebuffer, once
static void shadowAlloc( Renderer * r, Gpu & g, int face )
{
	if ( g.shTex )
		return;
	auto fn = r->fn;
	GLint prevActive = 0;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glGenTextures( 1, &g.shTex );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kShadowUnit ) );
	fn->glBindTexture( GL_TEXTURE_CUBE_MAP_ARRAY, g.shTex );
	fn->glTexImage3D( GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_DEPTH_COMPONENT16, face, face, 6 * kShadowSlots, 0,
		GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, nullptr );
	fn->glTexParameteri( GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	fn->glTexParameteri( GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	fn->glTexParameteri( GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE );
	fn->glTexParameteri( GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL );
	fn->glBindTexture( GL_TEXTURE_CUBE_MAP_ARRAY, 0 );
	fn->glActiveTexture( GLenum( prevActive ) );
	fn->glGenFramebuffers( 1, &g.shFbo );
	g.shFace = face;
}

/* lane LAMPSHADOW1: THE STORED LAMP SHADOWS (gl/celllampshadow.h). A held slot is only drawn when it is new, or a
 * mover in its light's volume moved, or the set of movers in it changed: the light's stored static map is copied in
 * (depth blits, exact), then the movers drawn over it (LEQUAL keeps the nearest: the same depths as one pass). */
namespace {

constexpr float kLsM[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
constexpr float kLsU[6][3] = { { 0, 0, -1 }, { 0, 0, 1 }, { 1, 0, 0 }, { 1, 0, 0 }, { 1, 0, 0 }, { -1, 0, 0 } };
constexpr float kLsV[6][3] = { { 0, -1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }, { 0, -1, 0 }, { 0, -1, 0 } };

struct LsCaster
{
	const Shape * sh;
	int id;
	Vector3 c;
	float rad;
};

struct LsRect
{
	int x = 0, y = 0, w = 0, h = 0;
};

//! one face draw: the layer of tex, the viewport's origin (a packed half shifts it), the scissor, clear first or not
struct LsTarget
{
	GLuint tex = 0;
	int layer = 0;
	int vx = 0, vy = 0;
	LsRect cut;
	bool clear = true;
};

bool lsTouch( const WwCellLight & l, const Vector3 & c, float rad )
{
	const float dx = c[0] - l.pos[0], dy = c[1] - l.pos[1], dz = c[2] - l.pos[2], reach = rad + l.radius;
	return dx * dx + dy * dy + dz * dz < reach * reach;
}

Gpu::LsSig lsSigOf( const Shape * sh, const Vector3 & c, float rad )
{
	Gpu::LsSig s;
	const Transform & w = sh->worldTrans();
	int n = 0;
	for ( int i = 0; i < 3; i++ )
		for ( int j = 0; j < 3; j++ )
			s.v[n++] = w.rotation( i, j );
	for ( int i = 0; i < 3; i++ )
		s.v[n++] = w.translation[i];
	s.v[n++] = w.scale;
	// two vertices: a CPU-skinned shape moves its vertices, not its transform
	const Vector3 & a = sh->verts.constFirst();
	const Vector3 & b = sh->verts.at( sh->verts.size() / 2 );
	for ( int i = 0; i < 3; i++ ) {
		s.v[n++] = a[i];
		s.v[n++] = b[i];
	}
	s.v[n++] = float( sh->verts.size() );
	s.c = c;
	s.rad = rad;
	return s;
}

//! side face f of a lamp whose back looks into a wall: the room half and the wall half of it
void lsSideHalves( int f, int back, int S, LsRect & room, LsRect & wall )
{
	const float * fr = kLsM[back ^ 1];
	const float cu = kLsU[f][0] * fr[0] + kLsU[f][1] * fr[1] + kLsU[f][2] * fr[2];
	const float cv = kLsV[f][0] * fr[0] + kLsV[f][1] * fr[1] + kLsV[f][2] * fr[2];
	const int h = S / 2;
	if ( cu != 0.0f ) {
		room = { cu > 0.0f ? h : 0, 0, h, S };
		wall = { cu > 0.0f ? 0 : h, 0, h, S };
	} else {
		room = { 0, cv > 0.0f ? h : 0, S, h };
		wall = { 0, cv > 0.0f ? 0 : h, S, h };
	}
}

//! a wall lamp's side faces (the four off its back axis) with their packed place: layer 1 + pair, member 0/1
void lsSides( int back, int sides[4], int layer[4], int member[4] )
{
	int n = 0, pair = 0;
	for ( int ax = 0; ax < 3; ax++ ) {
		if ( ax == back / 2 )
			continue;
		for ( int j = 0; j < 2; j++ ) {
			sides[n] = 2 * ax + j;
			layer[n] = 1 + pair;
			member[n] = j;
			n++;
		}
		pair++;
	}
}

LsRect lsPacked( const LsRect & room, int member, int S )
{
	const int h = S / 2;
	return room.w == h ? LsRect { member * h, 0, h, S } : LsRect { 0, member * h, S, h };
}

}	// namespace

/* lane GAPFIX1: an alpha-tested shape casts what its alpha test keeps into the light cubes, as the sun's map
 * does (VOLAIR1); the cube passes skipped it, and a barrel's own lamp drew an empty cube (lightbooth1).
 * Red WW_CELL_SHADOW_RED=atskip skips them again. */
/* lane ALPHABGSM1: the test is the colour draw's (Shape::wwDrawAlpha: a BGSM's wins over the NIF's); a BGSM-only
 * test cast solid into the cubes before. Red WW_ALPHABGSM_RED=cube. */
static bool cubeCasterAt( const Shape * sh )
{
	return sh->wwDrawAlpha( "cube" ).test && sh->wwShaderLighting() && !sh->coords.isEmpty()
		&& sh->coords.constFirst().size() == sh->verts.size();
}

static bool cubeSkipsCaster( const Shape * sh )
{
	WW_SWITCH_STATIC( bool, atRed, wwEnv( "WW_CELL_SHADOW_RED" ).trimmed() == "atskip" );	// lane LIVESET3: live
	return sh->wwDrawAlpha( "cube" ).test && ( atRed || !cubeCasterAt( sh ) );
}

static qint64 g_cubeAtCasters = 0, g_cubeAtDraws = 0;

/* lane PERFFIX2 b (item 5b): a caster goes only into the cube faces its bounding sphere can reach (gl/cubefacecull.h):
 * the skipped draws put no fragment into that face, so every depth texel is the old one. Red WW_CELL_SHADOW_RED=allfaces
 * draws every caster into all six faces again (a live set redraws every held cube and stores every lamp map again).
 * g_cubeFaceSkip counts the draws saved (the census's faceSkip=). */
static qint64 g_cubeFaceSkip = 0;

static bool cubeAllFacesRed()
{
	WW_SWITCH_STATIC( bool, allFacesRed, wwEnv( "WW_CELL_SHADOW_RED" ).trimmed() == "allfaces" );
	return allFacesRed;
}

/* lane PERFFIX2 b (item 5c, gl/cubefacecull.h coneMask): a spot's faces that its cone (widened by the lookup's reach)
 * never meets are read by no lit pixel, so they get no casters (they are still cleared). 959 of 1445 vanilla shadow
 * lights are spots. Red WW_CELL_SHADOW_RED=nocone (or allfaces). g_cubeConeSkip: the draws it saved (coneSkip=). */
static qint64 g_cubeConeSkip = 0;

static int cubeConeFaces( const WwCellLight & l, int faceTexels )
{
	WW_SWITCH_STATIC( bool, noConeRed, wwEnv( "WW_CELL_SHADOW_RED" ).trimmed() == "nocone" );
	if ( !l.spot || noConeRed || cubeAllFacesRed() )
		return 63;
	return WwCubeFace::coneMask( l.dir, l.cosOuter, WwCubeFace::coneMargin( faceTexels ) );
}

static bool cubeConeRed()
{
	WW_SWITCH_STATIC( bool, noConeRed, wwEnv( "WW_CELL_SHADOW_RED" ).trimmed() == "nocone" );
	return noConeRed;
}

static bool cubeFaceKeeps( int face, const float lpos[3], const Vector3 & c, float rad )
{
	if ( cubeAllFacesRed() )
		return true;
	const float rel[3] = { c[0] - lpos[0], c[1] - lpos[1], c[2] - lpos[2] };
	if ( WwCubeFace::touches( face, rel, rad ) )
		return true;
	g_cubeFaceSkip++;
	return false;
}

/* lane PERFFIX2 b: WW_CELL_SHADOW_HASH=<token> (gates only): every 8th pass, a 64-bit FNV-1a of every held slot's six
 * faces (the cube array read back) and the light it holds, on stderr: "shadow cube hash <token> pass N: <hex> slots K".
 * Two rows' hashes equal = the same depth texels (the per-face cull's exactness gate, beside its red). */
static void cubeHash( Renderer * r, const Gpu & g )
{
	WW_SWITCH_STATIC( QByteArray, token, wwEnv( "WW_CELL_SHADOW_HASH" ).trimmed() );
	static qint64 passes = 0;
	if ( token.isEmpty() || !g.shTex || g.shFace <= 0 || ( passes++ % 8 ) != 0 )
		return;
	auto fn = r->fn;
	const size_t faceTexels = size_t( g.shFace ) * size_t( g.shFace );
	std::vector<quint16> buf( faceTexels * 6u * size_t( kShadowSlots ) );
	GLint prevActive = 0, prevTex = 0;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kShadowUnit ) );
	fn->glGetIntegerv( GL_TEXTURE_BINDING_CUBE_MAP_ARRAY, &prevTex );
	fn->glBindTexture( GL_TEXTURE_CUBE_MAP_ARRAY, g.shTex );
	fn->glPixelStorei( GL_PACK_ALIGNMENT, 2 );
	fn->glGetTexImage( GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, buf.data() );
	fn->glPixelStorei( GL_PACK_ALIGNMENT, 4 );
	fn->glBindTexture( GL_TEXTURE_CUBE_MAP_ARRAY, GLuint( prevTex ) );
	fn->glActiveTexture( GLenum( prevActive ) );
	std::uint64_t h = 1469598103934665603ULL;
	int held = 0;
	for ( int k = 0; k < kShadowSlots; k++ ) {
		if ( g.shSlot[k] < 0 )
			continue;
		held++;
		h = ( h ^ std::uint64_t( std::uint32_t( g.shSlot[k] ) ) ) * 1099511628211ULL;
		const quint16 * p = buf.data() + faceTexels * 6u * size_t( k );
		for ( size_t t = 0; t < faceTexels * 6u; t++ )
			h = ( h ^ p[t] ) * 1099511628211ULL;
	}
	std::fprintf( stderr, "shadow cube hash %s pass %lld: %016llx slots %d face %d\n", token.constData(), (long long)passes,
		(unsigned long long)h, held, g.shFace );
}

static void cubeDrawCaster( Renderer * r, NifSkopeOpenGLContext::Program * prog, const Shape * sh, int instances = 1 )
{
	const Shape::WwDrawAlpha da = sh->wwDrawAlpha( "cube" );	// lane ALPHABGSM1
	const bool at = da.test;
	const float * attrv[8] = { &( sh->verts.constFirst()[0] ), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
	std::uint64_t mask = 3;
	prog->uni1i( "shAlphaTest", at ? 1 : 0 );
	if ( at ) {
		int texunit = 0;
		BSShaderLightingProperty * bsp = sh->wwShaderLighting();
		const BSLightingShaderProperty * lsp = sh->wwLightingShader();
		sh->wwLeafCovArm( "cube" );	// lane TREELEAF2: the colour pass's coverage copy (src/gl/wwleafcov.h)
		prog->uniSampler( bsp, "BaseMap", 0, texunit, QString(), bsp->clampMode, QString() );
		sh->wwLeafCovDisarm();
		prog->uni2f( "uvScale", bsp->uvScale.x, bsp->uvScale.y );
		prog->uni2f( "uvOffset", bsp->uvOffset.x, bsp->uvOffset.y );
		prog->uni1f( "alpha", lsp ? lsp->alpha : 1.0f );
		prog->uni1f( "alphaThreshold", sh->wwLeafSideThreshold( da, "cube" ) );	// lane TREELEAF2: the colour test's texels
		attrv[7] = &( sh->coords.constFirst().constFirst()[0] );
		mask = 0x20000003ULL;
		g_cubeAtDraws++;
	}
	if ( instances <= 1 ) {
		r->drawShape( (unsigned int) ( sh->verts.size() ), mask, (unsigned int) ( sh->triangles.size() * 3 ),
			GL_TRIANGLES, GL_UNSIGNED_SHORT, attrv, sh->triangles.constData() );
		return;
	}
	// lane MODULES1: one draw, one instance a cube face (the air atlas's layered program picks the face and viewport)
	const unsigned int n = (unsigned int) ( sh->triangles.size() * 3 );
	r->bindShape( (unsigned int) ( sh->verts.size() ), mask, size_t( n ) * 2u, attrv, sh->triangles.constData() );
	r->fn->glDrawElementsInstanced( GL_TRIANGLES, GLsizei( n ), GL_UNSIGNED_SHORT, nullptr, GLsizei( instances ) );
}

// lane MODULES1 route (a): the light cubes' casters (wwCellShadowPass's filter) and draw, for the air's lamp atlas
void wwCellCubeCasters( Scene * scene, std::vector<WwCubeCaster> & out )
{
	out.clear();
	WW_SWITCH_STATIC( bool, waterCastsRed, wwEnv( "WW_CELL_SHADOW_RED" ).trimmed() == "watercasts" );
	for ( Node * node : scene->nodes.list() ) {
		const Shape * sh = dynamic_cast<const Shape *>( node );
		if ( !sh || !sh->isVisible() || !sh->wwCastsSunShadow() || cubeSkipsCaster( sh ) )
			continue;
		if ( sh->verts.isEmpty() || sh->triangles.isEmpty() )
			continue;
		if ( !waterCastsRed && wwCellWaterIsShape( scene->nifModel, sh->id() ) )
			continue;
		const BoundSphere b = sh->bounds();
		out.push_back( { sh, { b.center[0], b.center[1], b.center[2] }, b.radius } );
	}
}

void wwCellCubeDrawCaster( Renderer * r, NifSkopeOpenGLContext::Program * prog, const Shape * sh, int instances )
{
	cubeDrawCaster( r, prog, sh, instances );
}

static void lampShadowPass( Scene * scene, Gpu & g, const WwCellLighting & L, std::vector<int> & dirty, const float cam[3] )
{
	ClState & s = st();
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	const int S = wwCellLampShadowFace();
	const int red = wwCellLampShadowRed();
	// lane ALPHABGSM1: a moved alpha-rule red (Shape::wwDrawAlphaGen) stores every lamp map again
	// lane PERFFIX2 b: so does the per-face cull's red
	const int ver = ( s.version.value( scene->nifModel ) + ( Shape::wwDrawAlphaGen() << 24 ) ) ^ ( cubeAllFacesRed() ? 0x40000000 : 0 ) ^ ( cubeConeRed() ? 0x20000000 : 0 );
	const std::uint64_t blockBytes = std::uint64_t( 3 ) * std::uint64_t( S ) * std::uint64_t( S ) * 2u;
	QElapsedTimer timer;
	timer.start();
	g.lsFrame++;
	if ( g.lsDoc != scene->nifModel || g.lsVersion != ver || g.lsFace != S ) {
		g.lsStore.clear();
		g.lsWall.clear();
		g.lsHist.clear();
		g.lsMovers.clear();
		for ( std::vector<int> & v : g.lsSlotMovers )
			v.clear();
		std::fill( g.lsBlockUsed.begin(), g.lsBlockUsed.end(), char( 0 ) );
		// the store holds the cell's shadow lamps (2 blocks each at most), up to the tier's budget
		int lamps = 0;
		for ( const WwCellLight & l : L.lights )
			lamps += l.shadow ? 1 : 0;
		const int budget = int( std::uint64_t( wwCellLampShadowBudgetMB() ) * 1048576u / blockBytes );
		const int want = std::max( 2, std::min( budget, 2 * lamps ) );
		if ( g.lsTex && ( g.lsFace != S || g.lsBlocks != want ) ) {
			fn->glDeleteTextures( 1, &g.lsTex );
			g.lsTex = 0;
		}
		g.lsBlocks = want;
		g.lsBlockUsed.assign( size_t( want ), char( 0 ) );
		g.lsDoc = scene->nifModel;
		g.lsVersion = ver;
		g.lsFace = S;
	}

	// the casters, as the cube pass takes them (the water too: SUNCELL1's red keeps it)
	WW_SWITCH_STATIC( bool, waterCastsRed, qgetenv( "WW_CELL_SHADOW_RED" ).trimmed() == "watercasts" );	// fix TDFIX2: follows a live set
	std::vector<LsCaster> casters;
	AlphaBgsmTally atTally;	// lane ALPHABGSM1: the census (gl/alphabgsm.h)
	qint64 atShapes = 0;
	for ( Node * node : scene->nodes.list() ) {
		const Shape * sh = dynamic_cast<const Shape *>( node );
		if ( !sh || !sh->isVisible() || !sh->wwCastsSunShadow() )
			continue;
		atShapes++;
		AlphaBgsmTally::note( atTally, sh->wwAlphaTested(), sh->wwDrawAlpha( "cube" ).test );
		if ( cubeSkipsCaster( sh ) )
			continue;
		if ( sh->verts.isEmpty() || sh->triangles.isEmpty() )
			continue;
		if ( !waterCastsRed && wwCellWaterIsShape( scene->nifModel, sh->id() ) )
			continue;
		const BoundSphere b = sh->bounds();
		casters.push_back( { sh, sh->id(), b.center, b.radius } );
	}
	AlphaBgsmTally::report( atTally, "lamp maps", atShapes );
	// what moved since the last frame; a first move makes a caster a mover and drops the stored maps it was in
	auto slotOfLight = [&g]( int li ) {
		for ( int k = 0; k < kShadowSlots; k++ )
			if ( g.shSlot[k] == li )
				return k;
		return -1;
	};
	auto markDirty = [&dirty]( int k ) {
		if ( k >= 0 && std::find( dirty.begin(), dirty.end(), k ) == dirty.end() )
			dirty.push_back( k );
	};
	auto freeEntry = [&g]( const Gpu::LsEntry & e ) {
		for ( int b : e.blocks )
			if ( b >= 0 && b < int( g.lsBlockUsed.size() ) )
				g.lsBlockUsed[size_t( b )] = 0;
	};
	QHash<int, char> moved;
	int newMovers = 0;
	for ( const LsCaster & c : casters ) {
		const Gpu::LsSig sig = lsSigOf( c.sh, c.c, c.rad );
		auto it = g.lsHist.find( c.id );
		if ( it != g.lsHist.end() && std::memcmp( it->v, sig.v, sizeof sig.v ) != 0 ) {
			moved.insert( c.id, 1 );
			if ( !g.lsMovers.contains( c.id ) ) {
				g.lsMovers.insert( c.id, 1 );
				newMovers++;
				for ( auto e = g.lsStore.begin(); e != g.lsStore.end(); ) {
					const WwCellLight & l = L.lights.at( e.key() );
					if ( lsTouch( l, it->c, it->rad ) || lsTouch( l, c.c, c.rad ) ) {
						freeEntry( e.value() );
						g.lsWall.remove( e.key() );
						markDirty( slotOfLight( e.key() ) );
						e = g.lsStore.erase( e );
					} else {
						++e;
					}
				}
			}
		}
		g.lsHist.insert( c.id, sig );
	}
	std::vector<const LsCaster *> movers;
	for ( const LsCaster & c : casters )
		if ( g.lsMovers.contains( c.id ) )
			movers.push_back( &c );

	// the slots to draw this frame
	struct Job
	{
		int k;
		std::vector<int> in;
	};
	std::vector<Job> jobs;
	for ( int k = 0; k < kShadowSlots; k++ ) {
		const int li = g.shSlot[k];
		if ( li < 0 || li >= L.lights.size() )
			continue;
		const WwCellLight & l = L.lights.at( li );
		Job j { k, {} };
		bool anyMoved = false;
		for ( const LsCaster * m : movers ) {
			if ( lsTouch( l, m->c, m->rad ) ) {
				j.in.push_back( m->id );
				anyMoved = anyMoved || moved.contains( m->id );
			}
		}
		const bool isDirty = std::find( dirty.begin(), dirty.end(), k ) != dirty.end();
		if ( red == 1 || isDirty || anyMoved || j.in != g.lsSlotMovers[k] || g.shPend[k] == li )	// lane PERFFIX2 b: a waiting store
			jobs.push_back( std::move( j ) );
	}
	auto census = [&]( int stored, int restored, int direct, int moverDraws ) {
		int used = 0, walls = 0;
		for ( char u : g.lsBlockUsed )
			used += u ? 1 : 0;
		for ( const Gpu::LsEntry & e : g.lsStore )
			walls += e.back >= 0 ? 1 : 0;
		const double mb = 1.0 / 1048576.0;
		s.lampLast = QStringLiteral( "%1 face=%2 store=%3(wall %4) storeMB=%5/%6 slotsMB=%7 slotsDrawn=%8 stored=%9 restored=%10"
			" direct=%11 movers=%12(new %13) moverDraws=%14 ms=%15%16 faceSkip=%17 slice=%18 waiting=%19 coneSkip=%20%21" )
			.arg( wwCellLampShadowTierName( wwCellLampShadowTier() ) ).arg( S )
			.arg( g.lsStore.size() ).arg( walls )
			.arg( double( used ) * double( blockBytes ) * mb, 0, 'f', 1 ).arg( double( g.lsBlocks ) * double( blockBytes ) * mb, 0, 'f', 1 )
			.arg( double( blockBytes ) * 2.0 * kShadowSlots * mb, 0, 'f', 1 )
			.arg( jobs.size() ).arg( stored ).arg( restored ).arg( direct ).arg( movers.size() ).arg( newMovers ).arg( moverDraws )
			.arg( timer.elapsed() )
			.arg( red == 1 ? QStringLiteral( " red=live" ) : red == 2 ? QStringLiteral( " red=cube" ) : QString() )
			.arg( g_cubeFaceSkip )	// lane PERFFIX2 b: draws the per-face cull saved, since the start
			.arg( wwCellLampShadowSlice() ).arg( s.lampSliced )	// lane PERFFIX2 b: the first-store budget, slots waiting
			.arg( g_cubeConeSkip ).arg( cubeConeRed() ? QStringLiteral( " red=nocone" ) : QString() );	// lane PERFFIX2 b: off-cone draws saved
	};
	if ( jobs.empty() ) {
		s.lampSliced = 0;	// lane PERFFIX2 b
		census( 0, 0, 0, 0 );
		return;
	}

	shadowAlloc( r, g, S );
	if ( !g.lsTex ) {
		fn->glGenTextures( 1, &g.lsTex );
		GLint prevActive = 0;
		fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kShadowUnit ) );
		GLint prevArray = 0;
		fn->glGetIntegerv( GL_TEXTURE_BINDING_2D_ARRAY, &prevArray );
		fn->glBindTexture( GL_TEXTURE_2D_ARRAY, g.lsTex );
		fn->glTexImage3D( GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT16, S, S, 3 * g.lsBlocks, 0, GL_DEPTH_COMPONENT,
			GL_UNSIGNED_SHORT, nullptr );
		fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		fn->glBindTexture( GL_TEXTURE_2D_ARRAY, GLuint( prevArray ) );
		fn->glActiveTexture( GLenum( prevActive ) );
	}
	if ( !g.lsReadFbo )
		fn->glGenFramebuffers( 1, &g.lsReadFbo );

	NifSkopeOpenGLContext::Program * prog = r->useProgram( "cell_shadowdepth.prog" );
	if ( !prog ) {
		s.lampLast = QStringLiteral( "refused(no cell_shadowdepth.prog)" );
		return;
	}
	// save what the pass touches
	GLint prevRead = 0, prevDraw = 0, vp[4], sc4[4], polyMode[2], depthFunc = 0;
	GLboolean depthMask = GL_TRUE;
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	fn->glGetIntegerv( GL_SCISSOR_BOX, sc4 );
	fn->glGetIntegerv( GL_POLYGON_MODE, polyMode );
	fn->glGetIntegerv( GL_DEPTH_FUNC, &depthFunc );
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	const bool wasCull = fn->glIsEnabled( GL_CULL_FACE ), wasOffset = fn->glIsEnabled( GL_POLYGON_OFFSET_FILL );
	const bool wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );
	const bool wasBlend = fn->glIsEnabled( GL_BLEND );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.lsReadFbo );
	fn->glReadBuffer( GL_NONE );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.shFbo );
	fn->glDrawBuffer( GL_NONE );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_POLYGON_OFFSET_FILL );
	fn->glEnable( GL_DEPTH_TEST );
	fn->glDepthFunc( GL_LEQUAL );
	fn->glDepthMask( GL_TRUE );
	fn->glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	fn->glClearDepth( 1.0 );

	const Transform & vt = scene->view;
	const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
	const Matrix4 viewM = vt.toMatrix4();
	const int lRel = prog->uniLocation( "shRelFromView" ), lClip = prog->uniLocation( "shClipFromRel" );
	const int lRn = prog->uniLocation( "shRadiusNear" );
	bool complete = true;
	auto setLight = [&]( const WwCellLight & l ) {
		float rel[16];
		for ( int row = 0; row < 3; row++ ) {
			for ( int j = 0; j < 3; j++ )
				rel[j * 4 + row] = vt.rotation( j, row ) / sc;
			rel[12 + row] = cam[row] - l.pos[row];
		}
		rel[3] = rel[7] = rel[11] = 0.0f;
		rel[15] = 1.0f;
		if ( lRel >= 0 )
			fn->glUniformMatrix4fv( lRel, 1, GL_FALSE, rel );
		if ( lRn >= 0 )
			fn->glUniform2f( lRn, l.radius, l.nearClip );
	};
	// one face: target, the face's projection, the casters (and the wall's plane when given)
	auto drawFace = [&]( const WwCellLight & l, int face, const LsTarget & t, const std::vector<const LsCaster *> & list,
		const float * quad ) -> qint64 {
		fn->glFramebufferTextureLayer( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, t.tex, 0, t.layer );
		if ( fn->glCheckFramebufferStatus( GL_DRAW_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
			complete = false;
			return 0;
		}
		if ( t.tex == g.shTex )
			g.shWrites++;	// lane PERFFIX2 a: wwCellLightsStateKey
		fn->glViewport( t.vx, t.vy, S, S );
		fn->glEnable( GL_SCISSOR_TEST );
		fn->glScissor( t.cut.x, t.cut.y, t.cut.w, t.cut.h );
		if ( t.clear )
			fn->glClear( GL_DEPTH_BUFFER_BIT );
		const float n = 1.0f, f = l.radius * 1.01f + 2.0f;
		const float A = ( f + n ) / ( f - n ), B = -2.0f * f * n / ( f - n );
		float clip[16];
		for ( int c = 0; c < 3; c++ ) {
			clip[c * 4 + 0] = kLsU[face][c];
			clip[c * 4 + 1] = kLsV[face][c];
			clip[c * 4 + 2] = A * kLsM[face][c];
			clip[c * 4 + 3] = kLsM[face][c];
		}
		clip[12] = clip[13] = clip[15] = 0.0f;
		clip[14] = B;
		if ( lClip >= 0 )
			fn->glUniformMatrix4fv( lClip, 1, GL_FALSE, clip );
		qint64 drawn = 0;
		const bool coneOff = !( cubeConeFaces( l, S ) >> face & 1 );	// lane PERFFIX2 b: off the spot's cone
		if ( coneOff )
			g_cubeConeSkip += qint64( list.size() );
		for ( const LsCaster * c : list ) {
			if ( coneOff )
				break;	// lane PERFFIX2 b: no lit pixel reads this face
			if ( !cubeFaceKeeps( face, l.pos, c->c, c->rad ) )
				continue;	// lane PERFFIX2 b: no fragment of it lands in this face
			prog->uni4m( "modelViewMatrix", c->sh->viewTrans().toMatrix4() );
			cubeDrawCaster( r, prog, c->sh );
			drawn++;
		}
		if ( quad ) {
			static const quint16 idx[6] = { 0, 1, 2, 0, 2, 3 };
			prog->uni1i( "shAlphaTest", 0 );
			prog->uni4m( "modelViewMatrix", viewM );
			r->drawShape( 4u, 3, 6u, GL_TRIANGLES, GL_UNSIGNED_SHORT, &quad, idx );
			drawn++;
		}
		return drawn;
	};
	auto blit = [&]( int srcLayer, const LsRect & a, int slotLayer, const LsRect & b ) {
		fn->glFramebufferTextureLayer( GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, g.lsTex, 0, srcLayer );
		fn->glFramebufferTextureLayer( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, g.shTex, 0, slotLayer );
		g.shWrites++;	// lane PERFFIX2 a: wwCellLightsStateKey
		if ( fn->glCheckFramebufferStatus( GL_READ_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE
			|| fn->glCheckFramebufferStatus( GL_DRAW_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
			complete = false;
			return;
		}
		fn->glDisable( GL_SCISSOR_TEST );
		fn->glBlitFramebuffer( a.x, a.y, a.x + a.w, a.y + a.h, b.x, b.y, b.x + b.w, b.y + b.h, GL_DEPTH_BUFFER_BIT, GL_NEAREST );
	};
	const LsRect full { 0, 0, S, S };

	int nStored = 0, nRestored = 0, nDirect = 0;
	qint64 moverDraws = 0;
	/* lane PERFFIX2 b (item 5d, gl/lampslice.h): the first stores of this frame spend at most `slice` cube faces, nearest
	 * slots first (the dirty list's order); one store always goes. A slot that waits reads unshadowed (its light as past
	 * the slot budget) until its map is drawn: the light buffer skips it (shPend) */
	WwLampSlice::Budget budgetF;
	budgetF.slice = red == 1 ? 0 : wwCellLampShadowSlice();
	int nWait = 0;
	bool pendMoved = false;
	for ( Job & j : jobs ) {
		if ( !complete )
			break;
		const int k = j.k, li = g.shSlot[k];
		const WwCellLight & l = L.lights.at( li );
		setLight( l );
		std::vector<const LsCaster *> still, mov;
		for ( const LsCaster & c : casters ) {
			if ( !lsTouch( l, c.c, c.rad ) )
				continue;
			if ( red != 1 && g.lsMovers.contains( c.id ) ) {
				if ( std::find( j.in.begin(), j.in.end(), c.id ) != j.in.end() )
					mov.push_back( &c );
			} else {
				still.push_back( &c );
			}
		}
		auto direct = [&]() {
			for ( int f = 0; f < 6 && complete; f++ )
				drawFace( l, f, LsTarget { g.shTex, 6 * k + f, 0, 0, full, true }, still, nullptr );
			nDirect++;
		};
		if ( red == 1 ) {
			direct();
			g.lsSlotMovers[k] = j.in;
			continue;
		}
		// stored already, or store it now (the wall test once per lamp)
		auto e = g.lsStore.find( li );
		if ( e == g.lsStore.end() ) {
			if ( !g.lsWall.contains( li ) ) {
				WwLampWall w;
				if ( red != 2 ) {
					std::vector<const Shape *> shapes;
					for ( const LsCaster & c : casters )
						if ( !g.lsMovers.contains( c.id ) )
							shapes.push_back( c.sh );
					w = wwLampFindWall( l.pos, shapes );
				}
				g.lsWall.insert( li, w );
			}
			const WwLampWall w = g.lsWall.value( li );
			if ( !budgetF.spend( w.back >= 0 ? 5 : 6 ) ) {
				if ( g.shPend[k] != li ) {	// lane PERFFIX2 b: waits for a later frame's budget
					g.shPend[k] = li;
					pendMoved = true;
				}
				nWait++;
				continue;
			}
			const int need = w.back >= 0 ? 1 : 2;
			std::vector<int> blocks;
			for ( int pass = 0; pass < 2 && int( blocks.size() ) < need; pass++ ) {
				blocks.clear();
				for ( int b = 0; b < g.lsBlocks && int( blocks.size() ) < need; b++ )
					if ( !g.lsBlockUsed[size_t( b )] )
						blocks.push_back( b );
				if ( int( blocks.size() ) >= need || pass == 1 )
					break;
				// full: evict the least recently used maps no slot holds until this one fits
				std::vector<std::pair<std::uint64_t, int>> lru;
				for ( auto it = g.lsStore.cbegin(); it != g.lsStore.cend(); ++it )
					if ( slotOfLight( it.key() ) < 0 )
						lru.emplace_back( it->used, it.key() );
				std::sort( lru.begin(), lru.end() );
				int freed = int( blocks.size() );
				for ( const auto & [used, key] : lru ) {
					if ( freed >= need )
						break;
					freed += int( g.lsStore.value( key ).blocks.size() );
					freeEntry( g.lsStore.value( key ) );
					g.lsStore.remove( key );
				}
			}
			if ( int( blocks.size() ) >= need ) {
				Gpu::LsEntry ne;
				ne.blocks = blocks;
				ne.back = w.back;
				ne.dist = w.dist;
				for ( int b : blocks )
					g.lsBlockUsed[size_t( b )] = 1;
				if ( w.back < 0 ) {
					for ( int f = 0; f < 6 && complete; f++ )
						drawFace( l, f, LsTarget { g.lsTex, 3 * blocks[size_t( f / 3 )] + f % 3, 0, 0, full, true }, still, nullptr );
				} else {
					drawFace( l, w.back ^ 1, LsTarget { g.lsTex, 3 * blocks[0], 0, 0, full, true }, still, nullptr );
					int sides[4], layer[4], member[4];
					lsSides( w.back, sides, layer, member );
					for ( int i = 0; i < 4 && complete; i++ ) {
						LsRect room, wall;
						lsSideHalves( sides[i], w.back, S, room, wall );
						const LsRect dst = lsPacked( room, member[i], S );
						drawFace( l, sides[i], LsTarget { g.lsTex, 3 * blocks[0] + layer[i], dst.x - room.x, dst.y - room.y, dst, true },
							still, nullptr );
					}
				}
				e = g.lsStore.insert( li, ne );
				nStored++;
			}
		}
		if ( e == g.lsStore.end() ) {
			direct();	// over the budget: drawn as before
		} else {
			e->used = g.lsFrame;
			const Gpu::LsEntry & en = e.value();
			if ( en.back < 0 ) {
				for ( int f = 0; f < 6; f++ )
					blit( 3 * en.blocks[size_t( f / 3 )] + f % 3, full, 6 * k + f, full );
			} else {
				blit( 3 * en.blocks[0], full, 6 * k + ( en.back ^ 1 ), full );
				int sides[4], layer[4], member[4];
				lsSides( en.back, sides, layer, member );
				for ( int i = 0; i < 4; i++ ) {
					LsRect room, wall;
					lsSideHalves( sides[i], en.back, S, room, wall );
					blit( 3 * en.blocks[0] + layer[i], lsPacked( room, member[i], S ), 6 * k + sides[i], room );
				}
				// the wall's half: the plane itself, out past the light's reach
				const int ax = en.back / 2, a1 = ( ax + 1 ) % 3, a2 = ( ax + 2 ) % 3;
				const float sg = ( en.back & 1 ) ? -1.0f : 1.0f, ext = l.radius * 2.0f + 64.0f;
				float quad[12];
				for ( int v = 0; v < 4; v++ ) {
					float * q = quad + 3 * v;
					q[ax] = l.pos[ax] + sg * en.dist;
					q[a1] = l.pos[a1] + ( v == 0 || v == 3 ? -ext : ext );
					q[a2] = l.pos[a2] + ( v < 2 ? -ext : ext );
				}
				const std::vector<const LsCaster *> none;
				drawFace( l, en.back, LsTarget { g.shTex, 6 * k + en.back, 0, 0, full, true }, none, quad );
				for ( int i = 0; i < 4 && complete; i++ ) {
					LsRect room, wall;
					lsSideHalves( sides[i], en.back, S, room, wall );
					drawFace( l, sides[i], LsTarget { g.shTex, 6 * k + sides[i], 0, 0, wall, true }, none, quad );
				}
			}
			nRestored++;
		}
		if ( g.shPend[k] >= 0 ) {	// lane PERFFIX2 b: drawn now
			g.shPend[k] = -1;
			pendMoved = true;
		}
		// the movers in this light's volume, over the static map
		if ( !mov.empty() )
			for ( int f = 0; f < 6 && complete; f++ )
				moverDraws += drawFace( l, f, LsTarget { g.shTex, 6 * k + f, 0, 0, full, false }, mov, nullptr );
		g.lsSlotMovers[k] = j.in;
	}
	r->stopProgram();

	// restore
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
	fn->glScissor( sc4[0], sc4[1], sc4[2], sc4[3] );
	fn->glPolygonMode( GL_FRONT_AND_BACK, GLenum( polyMode[0] ) );
	fn->glDepthFunc( GLenum( depthFunc ) );
	fn->glDepthMask( depthMask );
	wasCull ? fn->glEnable( GL_CULL_FACE ) : fn->glDisable( GL_CULL_FACE );
	wasOffset ? fn->glEnable( GL_POLYGON_OFFSET_FILL ) : fn->glDisable( GL_POLYGON_OFFSET_FILL );
	wasDepth ? fn->glEnable( GL_DEPTH_TEST ) : fn->glDisable( GL_DEPTH_TEST );
	wasScissor ? fn->glEnable( GL_SCISSOR_TEST ) : fn->glDisable( GL_SCISSOR_TEST );
	wasBlend ? fn->glEnable( GL_BLEND ) : fn->glDisable( GL_BLEND );

	if ( pendMoved )
		g.shStamp++;	// lane PERFFIX2 b: the light buffer reads the slots that wait / are drawn now
	s.lampSliced = nWait;
	if ( !complete ) {
		for ( int k = 0; k < kShadowSlots; k++ )
			g.shSlot[k] = -1;
		g.shStamp++;
		g.lsStore.clear();
		std::fill( g.lsBlockUsed.begin(), g.lsBlockUsed.end(), char( 0 ) );
		s.lampLast = QStringLiteral( "refused(framebuffer incomplete)" );
		return;
	}
	census( nStored, nRestored, nDirect, int( moverDraws ) );
	shadowDump( scene );
}

/* The depth cubes (celllights.h). Each face: world relative to the light, GL's cube face
 * axes (major m, s = u.r, t = v.r over |m.r|), 90 degrees; depth = distance / radius (the fragment stage). */
namespace {
QHash<const void *, int> & shadowKicks()
{
	static QHash<const void *, int> k;
	return k;
}
}	// namespace

void wwCellShadowKick( const void * nif )
{
	shadowKicks()[nif]++;
}

void wwCellShadowsForget( const void * nif )
{
	for ( Gpu & g : gpus() ) {	// the next pass empties the slots and the lamp store, as a fresh open's first pass
		if ( g.shDoc == nif )
			g.shDoc = nullptr;
		if ( g.lsDoc == nif )
			g.lsDoc = nullptr;
	}
}

void wwCellShadowPass( Scene * scene )
{
	if ( !wwCellLightsWanted( scene ) )
		return;
	ClState & s = st();
	const WwCellLighting * L = wwCellLightsFor( scene->nifModel );
	if ( !L )
		return;
	Renderer * r = scene->renderer;
	Gpu & g = gpus()[r];
	const int ver = s.version.value( scene->nifModel );
	if ( g.shDoc != scene->nifModel || g.shVersion != ver ) {
		std::fill( g.shSlot, g.shSlot + kShadowSlots, -1 );
		g.shDoc = scene->nifModel;
		g.shVersion = ver;
		g.shStamp++;
	}
	if ( !s.shadowOn ) {
		s.shadowLast = s.shadowPinned ? QStringLiteral( "off(WW_CELL_SHADOW=0)" ) : QStringLiteral( "off(row)" );
		return;
	}
	/* lane RESTIRDI1: the RT quality preset Medium+ traces the shadow-casting lights (ReSTIR DI, rtddgiRestirPass): no cube
	 * is drawn and every slot is let go (a frame the trace refuses draws those lights unshadowed, as past the budget) */
	if ( rtddgiRestirShaderDefine() ) {
		if ( std::any_of( g.shSlot, g.shSlot + kShadowSlots, []( int v ) { return v >= 0; } ) ) {
			std::fill( g.shSlot, g.shSlot + kShadowSlots, -1 );
			g.shStamp++;
		}
		s.shadowLast = QStringLiteral( "off(ray-traced: the RT quality preset's ReSTIR DI)" );
		return;
	}
	// lane LAMPSHADOW1: the tier's face edge; a change remakes the cube array and empties the slots
	const int face = wwCellLampShadowFace();
	if ( g.shTex && g.shFace != face ) {
		r->fn->glDeleteTextures( 1, &g.shTex );
		r->fn->glDeleteFramebuffers( 1, &g.shFbo );
		g.shTex = g.shFbo = 0;
		std::fill( g.shSlot, g.shSlot + kShadowSlots, -1 );
		g.shStamp++;
	}
	// the camera in the world (the view's inverse, as cellRow)
	const Transform & vt = scene->view;
	const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
	float cam[3];
	for ( int k = 0; k < 3; k++ ) {
		float w = 0.0f;
		for ( int j = 0; j < 3; j++ )
			w -= vt.rotation( j, k ) * vt.translation[j];
		cam[k] = w / sc;
	}
	// the wanted set: the shadow lights whose reach comes nearest the camera
	std::vector<std::pair<float, int>> cand;
	const int budget = wwCellShadowBudget();	// lane GAPLOOK1 (LIGHTSYS-3): 0 = the nearest, as before
	std::vector<float> fadeOf, rawOf;
	if ( budget > 0 ) {
		const bool worst = wwCellShadowBudgetRedWorst();
		std::vector<char> wasHeld( size_t( L->lights.size() ), 0 );
		for ( int k = 0; k < kShadowSlots; k++ )
			if ( g.shSlot[k] >= 0 && g.shSlot[k] < L->lights.size() )
				wasHeld[size_t( g.shSlot[k] )] = 1;
		fadeOf.assign( size_t( L->lights.size() ), 0.0f );
		rawOf.assign( size_t( L->lights.size() ), 0.0f );
		for ( qsizetype i = 0; i < L->lights.size(); i++ ) {
			const WwCellLight & l = L->lights.at( i );
			if ( !l.shadow )
				continue;
			const float dx = l.pos[0] - cam[0], dy = l.pos[1] - cam[1], dz = l.pos[2] - cam[2];
			const float d = std::sqrt( dx * dx + dy * dy + dz * dz );
			const float t = std::clamp( ( d - kShadowFadeBegin ) / ( kShadowFadeEnd - kShadowFadeBegin ), 0.0f, 1.0f );
			const float dfade = 1.0f - t * t * ( 3.0f - 2.0f * t );
			if ( dfade <= 0.0f )
				continue;	// faded out: takes no slot
			const float lum = 0.2126f * l.color[0] + 0.7152f * l.color[1] + 0.0722f * l.color[2];
			const float raw = std::max( lum, 1e-6f ) * l.radius / std::max( d, 1.0f ) * dfade;
			fadeOf[size_t( i )] = dfade;
			rawOf[size_t( i )] = raw;
			const float eff = raw * ( wasHeld[size_t( i )] ? 1.2f : 1.0f );	// hysteresis: a holder needs beating by 20 %
			cand.emplace_back( worst ? eff : -eff, int( i ) );
		}
	} else {
		for ( qsizetype i = 0; i < L->lights.size(); i++ ) {
			const WwCellLight & l = L->lights.at( i );
			if ( !l.shadow )
				continue;
			const float dx = l.pos[0] - cam[0], dy = l.pos[1] - cam[1], dz = l.pos[2] - cam[2];
			cand.emplace_back( std::max( 0.0f, std::sqrt( dx * dx + dy * dy + dz * dz ) - l.radius ), int( i ) );
		}
	}
	const int want = std::min( int( cand.size() ), budget > 0 ? budget : kShadowSlots );
	std::partial_sort( cand.begin(), cand.begin() + want, cand.end() );
	g.shBudget = budget;
	g.shCut = 0.0f;
	if ( budget > 0 ) {
		// the cut: the best raw score left out; a kept light fades in over 1.2..1.5 x it (past the hysteresis, so
		// neither the light leaving nor the one taking its slot pops)
		for ( size_t k = size_t( want ); k < cand.size(); k++ )
			g.shCut = std::max( g.shCut, rawOf[size_t( cand[k].second )] );
		if ( g.shCut > 0.0f && !wwCellShadowBudgetRedWorst() )
			for ( int k = 0; k < want; k++ ) {
				const size_t li = size_t( cand[size_t( k )].second );
				const float t = std::clamp( ( rawOf[li] / g.shCut - 1.2f ) / 0.3f, 0.0f, 1.0f );
				fadeOf[li] *= t * t * ( 3.0f - 2.0f * t );
			}
	}
	std::vector<char> wanted( size_t( L->lights.size() ), 0 ), held( size_t( L->lights.size() ), 0 );
	for ( int i = 0; i < want; i++ )
		wanted[size_t( cand[size_t( i )].second )] = 1;
	bool changed = false;
	for ( int k = 0; k < kShadowSlots; k++ ) {
		const int i = g.shSlot[k];
		if ( i < 0 )
			continue;
		if ( i >= L->lights.size() || !wanted[size_t( i )] ) {
			g.shSlot[k] = -1;
			changed = true;
		} else {
			held[size_t( i )] = 1;
		}
	}
	std::vector<int> dirty;
	for ( int i = 0; i < want; i++ ) {
		const int li = cand[size_t( i )].second;
		if ( held[size_t( li )] )
			continue;
		for ( int k = 0; k < kShadowSlots; k++ ) {
			if ( g.shSlot[k] < 0 ) {
				g.shSlot[k] = li;
				dirty.push_back( k );
				break;
			}
		}
	}
	{
		static int atGen = 0;	// lane ALPHABGSM1: a moved alpha-rule red draws every held cube again
		if ( atGen != Shape::wwDrawAlphaGen() ) {
			atGen = Shape::wwDrawAlphaGen();
			wwCellShadowKick( scene->nifModel );
		}
		static int allGen = -1;	// lane PERFFIX2 b: a moved per-face cull red draws every held cube again
		if ( allGen != int( cubeAllFacesRed() ) + 2 * int( cubeConeRed() ) ) {	// (and the cone cull's)
			allGen = int( cubeAllFacesRed() ) + 2 * int( cubeConeRed() );
			wwCellShadowKick( scene->nifModel );
		}
		const int kick = shadowKicks().value( scene->nifModel );
		if ( g.shKick != kick ) {
			g.shKick = kick;
			for ( int k = 0; k < kShadowSlots; k++ )
				if ( g.shSlot[k] >= 0 && std::find( dirty.begin(), dirty.end(), k ) == dirty.end() )
					dirty.push_back( k );
		}
	}
	if ( changed || !dirty.empty() )
		g.shStamp++;
	for ( int k = 0; k < kShadowSlots; k++ )	// lane GAPLOOK1: the strengths follow the camera every pass
		g.shFade[k] = budget > 0 && g.shSlot[k] >= 0 && size_t( g.shSlot[k] ) < fadeOf.size() ? fadeOf[size_t( g.shSlot[k] )] : 1.0f;
	cubeHash( r, g );	// lane PERFFIX2 b: the gate's hash of the cubes as the last pass left them (WW_CELL_SHADOW_HASH)
	if ( wwCellLampShadowTier() >= 0 ) {
		s.shadowLast = QStringLiteral( "on shadowLights=%1 face=%2 (lamp)" ).arg( cand.size() ).arg( face );
		lampShadowPass( scene, g, *L, dirty, cam );	// lane LAMPSHADOW1: stored maps, movers redrawn on top
		return;
	}
	for ( int k = 0; k < kShadowSlots; k++ ) {	// lane PERFFIX2 b: the plain cubes draw a slot that waited at once
		if ( g.shPend[k] >= 0 ) {
			if ( g.shSlot[k] == g.shPend[k] && std::find( dirty.begin(), dirty.end(), k ) == dirty.end() )
				dirty.push_back( k );
			g.shPend[k] = -1;
			g.shStamp++;
		}
	}
	if ( g.lsTex ) {	// the row turned Off: the store's memory goes back
		r->fn->glDeleteTextures( 1, &g.lsTex );
		g.lsTex = 0;
		g.lsDoc = nullptr;
		g.lsStore.clear();
	}
	if ( dirty.empty() ) {
		if ( changed )
			shadowDump( scene );
		return;
	}

	QElapsedTimer timer;
	timer.start();
	auto fn = r->fn;
	GLint prevActive = 0;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	shadowAlloc( r, g, face );

	// the casters: what the sun's map takes (opaque, depth-writing, no effect), less the alpha-tested
	struct ShCaster
	{
		const Shape * sh;
		Vector3 c;
		float rad;
		Matrix4 mv;
	};
	std::vector<ShCaster> casters;
	// a cell's water surface casts no light shadow (the game's water is no caster); WW_CELL_SHADOW_RED=watercasts keeps it
	WW_SWITCH_STATIC( bool, waterCastsRed, wwEnv( "WW_CELL_SHADOW_RED" ).trimmed() == "watercasts" );
	int waterSkipped = 0;
	AlphaBgsmTally atTally;	// lane ALPHABGSM1: the census (gl/alphabgsm.h)
	qint64 atShapes = 0;
	for ( Node * node : scene->nodes.list() ) {
		const Shape * sh = dynamic_cast<const Shape *>( node );
		if ( !sh || !sh->isVisible() || !sh->wwCastsSunShadow() )
			continue;
		atShapes++;
		AlphaBgsmTally::note( atTally, sh->wwAlphaTested(), sh->wwDrawAlpha( "cube" ).test );
		if ( cubeSkipsCaster( sh ) )
			continue;
		if ( sh->verts.isEmpty() || sh->triangles.isEmpty() )
			continue;
		if ( !waterCastsRed && wwCellWaterIsShape( scene->nifModel, sh->id() ) ) {
			waterSkipped++;
			continue;
		}
		g_cubeAtCasters += sh->wwDrawAlpha( "cube" ).test ? 1 : 0;	// lane ALPHABGSM1
		const BoundSphere b = sh->bounds();
		casters.push_back( { sh, b.center, b.radius, sh->viewTrans().toMatrix4() } );
	}
	AlphaBgsmTally::report( atTally, "light cubes", atShapes );

	NifSkopeOpenGLContext::Program * prog = r->useProgram( "cell_shadowdepth.prog" );
	if ( !prog ) {
		for ( int k : dirty )
			g.shSlot[k] = -1;
		g.shStamp++;
		s.shadowLast = QStringLiteral( "refused(no cell_shadowdepth.prog)" );
		return;
	}
	// save what the pass touches
	GLint prevRead = 0, prevDraw = 0, vp[4], polyMode[2], depthFunc = 0;
	GLboolean depthMask = GL_TRUE;
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	fn->glGetIntegerv( GL_POLYGON_MODE, polyMode );
	fn->glGetIntegerv( GL_DEPTH_FUNC, &depthFunc );
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	const bool wasCull = fn->glIsEnabled( GL_CULL_FACE ), wasOffset = fn->glIsEnabled( GL_POLYGON_OFFSET_FILL );
	const bool wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );
	const bool wasBlend = fn->glIsEnabled( GL_BLEND );

	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.shFbo );
	// lane MSAABLACK1: bind READ too, so glReadBuffer below sets OUR fbo; on the view's it left TAA's colour blit
	// a silent no-op (MSAA 1 + TAA saved black). WW_MSAABLACK1_RED=1: the old binding
	WW_SWITCH_STATIC( bool, msaaBlackRed, wwEnvInt( "WW_MSAABLACK1_RED" ) == 1 );
	if ( !msaaBlackRed )
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.shFbo );
	fn->glDrawBuffer( GL_NONE );
	fn->glReadBuffer( GL_NONE );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_CULL_FACE );			// both sides cast (a cube face mirrors the winding anyway)
	fn->glDisable( GL_POLYGON_OFFSET_FILL );	// the depth is written by hand: the receiver biases
	fn->glEnable( GL_DEPTH_TEST );
	fn->glDepthFunc( GL_LEQUAL );
	fn->glDepthMask( GL_TRUE );
	fn->glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	fn->glViewport( 0, 0, face, face );

	// GL's cube faces: major axis m, s along u, t along v (the spec's sc / tc table)
	static const float M[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
	static const float U[6][3] = { { 0, 0, -1 }, { 0, 0, 1 }, { 1, 0, 0 }, { 1, 0, 0 }, { 1, 0, 0 }, { -1, 0, 0 } };
	static const float V[6][3] = { { 0, -1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }, { 0, -1, 0 }, { 0, -1, 0 } };
	const int lRel = prog->uniLocation( "shRelFromView" ), lClip = prog->uniLocation( "shClipFromRel" );
	const int lRn = prog->uniLocation( "shRadiusNear" );
	bool complete = true;
	qint64 drawn = 0;
	for ( int k : dirty ) {
		if ( !complete )
			break;
		const WwCellLight & l = L->lights.at( g.shSlot[k] );
		float rel[16];
		for ( int row = 0; row < 3; row++ ) {
			for ( int j = 0; j < 3; j++ )
				rel[j * 4 + row] = vt.rotation( j, row ) / sc;
			rel[12 + row] = cam[row] - l.pos[row];
		}
		rel[3] = rel[7] = rel[11] = 0.0f;
		rel[15] = 1.0f;
		if ( lRel >= 0 )
			fn->glUniformMatrix4fv( lRel, 1, GL_FALSE, rel );
		if ( lRn >= 0 )
			fn->glUniform2f( lRn, l.radius, l.nearClip );
		const float n = 1.0f, f = l.radius * 1.01f + 2.0f;
		const float A = ( f + n ) / ( f - n ), B = -2.0f * f * n / ( f - n );
		std::vector<const ShCaster *> mine;
		for ( const ShCaster & c : casters ) {
			const float dx = c.c[0] - l.pos[0], dy = c.c[1] - l.pos[1], dz = c.c[2] - l.pos[2];
			const float reach = c.rad + l.radius;
			if ( dx * dx + dy * dy + dz * dz < reach * reach )
				mine.push_back( &c );
		}
		const int coneF = cubeConeFaces( l, face );	// lane PERFFIX2 b: the faces the spot's cone meets (63 for the rest)
		for ( int face = 0; face < 6; face++ ) {
			fn->glFramebufferTextureLayer( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, g.shTex, 0, 6 * k + face );
			if ( fn->glCheckFramebufferStatus( GL_DRAW_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
				complete = false;
				break;
			}
			g.shWrites++;	// lane PERFFIX2 a: wwCellLightsStateKey
			fn->glClearDepth( 1.0 );
			fn->glClear( GL_DEPTH_BUFFER_BIT );
			float clip[16];
			for ( int c = 0; c < 3; c++ ) {
				clip[c * 4 + 0] = U[face][c];
				clip[c * 4 + 1] = V[face][c];
				clip[c * 4 + 2] = A * M[face][c];
				clip[c * 4 + 3] = M[face][c];
			}
			clip[12] = clip[13] = clip[15] = 0.0f;
			clip[14] = B;
			if ( lClip >= 0 )
				fn->glUniformMatrix4fv( lClip, 1, GL_FALSE, clip );
			if ( !( coneF >> face & 1 ) ) {
				g_cubeConeSkip += qint64( mine.size() );	// lane PERFFIX2 b: cleared, no caster (no lit pixel reads it)
				continue;
			}
			for ( const ShCaster * c : mine ) {
				if ( !cubeFaceKeeps( face, l.pos, c->c, c->rad ) )
					continue;	// lane PERFFIX2 b: no fragment of it lands in this face
				prog->uni4m( "modelViewMatrix", c->mv );
				cubeDrawCaster( r, prog, c->sh );
				drawn++;
			}
		}
	}
	r->stopProgram();

	// restore
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
	fn->glPolygonMode( GL_FRONT_AND_BACK, GLenum( polyMode[0] ) );
	fn->glDepthFunc( GLenum( depthFunc ) );
	fn->glDepthMask( depthMask );
	wasCull ? fn->glEnable( GL_CULL_FACE ) : fn->glDisable( GL_CULL_FACE );
	wasOffset ? fn->glEnable( GL_POLYGON_OFFSET_FILL ) : fn->glDisable( GL_POLYGON_OFFSET_FILL );
	wasDepth ? fn->glEnable( GL_DEPTH_TEST ) : fn->glDisable( GL_DEPTH_TEST );
	wasScissor ? fn->glEnable( GL_SCISSOR_TEST ) : fn->glDisable( GL_SCISSOR_TEST );
	wasBlend ? fn->glEnable( GL_BLEND ) : fn->glDisable( GL_BLEND );

	if ( !complete ) {
		for ( int k : dirty )
			g.shSlot[k] = -1;
		g.shStamp++;
		s.shadowLast = QStringLiteral( "refused(shadow framebuffer incomplete)" );
		return;
	}
	s.shadowLast = QStringLiteral( "on shadowLights=%1 rendered=%2 casters=%3 draws=%4 ms=%5 face=%6 water=%7 faceSkip=%8 coneSkip=%9" )
		.arg( cand.size() ).arg( dirty.size() ).arg( casters.size() ).arg( drawn ).arg( timer.elapsed() ).arg( face )
		.arg( waterCastsRed ? QStringLiteral( "casts(RED)" ) : QStringLiteral( "skipped %1" ).arg( waterSkipped ) )
		.arg( g_cubeFaceSkip ).arg( g_cubeConeSkip );	// lane PERFFIX2 b: draws the per-face and the cone cull saved, since the start
	shadowDump( scene );
}

QString wwCellShadowEcho( Scene * scene )
{
	ClState & s = st();
	QString o = QStringLiteral( "shadow=%1" ).arg( s.shadowLast );
	o += QStringLiteral( " atCasters=%1 atDraws=%2" ).arg( g_cubeAtCasters ).arg( g_cubeAtDraws );	// lane GAPFIX1
	if ( wwCellLampShadowTier() >= 0 )
		o += QStringLiteral( " lamp=" ) + s.lampLast;	// lane LAMPSHADOW1
	if ( s.shadowRed )
		o += QStringLiteral( " red=noshadow" );
	if ( !scene || !scene->renderer || !scene->nifModel )
		return o;
	const WwCellLighting * L = wwCellLightsFor( scene->nifModel );
	const Gpu & g = gpus()[scene->renderer];
	if ( !L || g.shDoc != scene->nifModel )
		return o;
	int used = 0;
	for ( int k = 0; k < kShadowSlots; k++ )
		used += g.shSlot[k] >= 0 ? 1 : 0;
	o += QStringLiteral( " slots=%1/%2" ).arg( used ).arg( kShadowSlots );
	if ( g.shBudget > 0 ) {	// lane GAPLOOK1: the budget, its cut score and the strengths it gives the held slots
		QString f;
		for ( int k = 0; k < kShadowSlots; k++ )
			if ( g.shSlot[k] >= 0 )
				f += QStringLiteral( "%1%2" ).arg( f.isEmpty() ? QString() : QStringLiteral( "," ) ).arg( double( g.shFade[k] ), 0, 'f', 3 );
		o += QStringLiteral( " budget=%1 cut=%2 fade=%3%4" ).arg( g.shBudget ).arg( double( g.shCut ), 0, 'g', 4 )
			.arg( f.isEmpty() ? QStringLiteral( "-" ) : f ).arg( wwCellShadowBudgetRedWorst() ? QStringLiteral( " RED=worst" ) : QString() );
	}
	// the gate's three: position, radius, kind, near clip, direction (a hemisphere's plane)
	for ( int k = 0; k < 3; k++ ) {
		const int i = g.shSlot[k];
		if ( i < 0 || i >= L->lights.size() )
			continue;
		const WwCellLight & l = L->lights.at( i );
		o += QStringLiteral( " slot%1=%2,%3,%4,%5,%6,%7,%8,%9,%10" ).arg( k )
			.arg( double( l.pos[0] ), 0, 'f', 3 ).arg( double( l.pos[1] ), 0, 'f', 3 ).arg( double( l.pos[2] ), 0, 'f', 3 )
			.arg( double( l.radius ), 0, 'f', 3 ).arg( l.shadow ).arg( double( l.nearClip ), 0, 'f', 3 )
			.arg( double( l.dir[0] ), 0, 'f', 5 ).arg( double( l.dir[1] ), 0, 'f', 5 ).arg( double( l.dir[2] ), 0, 'f', 5 );
	}
	return o;
}

// lane VOLAIR1: the light buffer's spheres (src/gl/celllights.h)
const std::vector<float> * wwCellLightSpheres( Scene * scene )
{
	if ( !scene || !scene->renderer || !gpus().contains( scene->renderer ) )
		return nullptr;
	const Gpu & g = gpus()[scene->renderer];
	if ( g.doc != scene->nifModel || int( g.spheres.size() / 4 ) != g.count )
		return nullptr;
	return &g.spheres;
}

bool wwCellShadowOn()
{
	return st().shadowOn && ( st().shadowPinned || wwGroupLets( WwGroup::ShadowsVanilla ) );	// lane OPENCELLFIX2
}

void wwCellShadowSetOn( bool on )
{
	ClState & s = st();
	if ( s.shadowPinned )
		return;
	s.shadowOn = on;
	QSettings().setValue( QStringLiteral( "WW/CellShadow" ), on );
}

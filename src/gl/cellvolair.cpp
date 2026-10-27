/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellvolair.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store
#include "cellunits.h"

#include "gl/cellgrass.h"	// lane FOGWIND1: the game's wind angle, shared with the grass
#include "gl/cellhdr.h"
#include "gl/celllights.h"
#include "gl/celluniform.h"	// lane CELLUNI3: the units bound without a unit ask
#include "gl/celllooks.h"	// lane GAPLOOK1
#include "gl/cellpasstime.h"
#include "gl/cellpost.h"
#include "gl/cellssr.h"
#include "gl/fogterrain.h"	// lane FOGTERRAIN1: the ground layer on the low-pass terrain
#include "gl/glscene.h"
#include "gl/glshape.h"	// lane CLOUDLOOK1: the shapes' spheres (vaShapesTop)
#include "gl/lookdevstage.h"
#include "gl/renderer.h"
#include "gl/shadowset.h"	// lane FARGRID1: the terrain map
#include "gl/sunshadow.h"
#include "esmweather.h"
#include "harnesswindow.h"
#include "recx.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <vector>

/* ---- lane VOLAIR1: the air (cellvolair.h, res/shaders/cell_volair.frag) ---- */
namespace
{

constexpr int kVolTexUnit = WwCellUnit::AirVol;	// the integrated volume the surfaces read (sampler3D); the table: cellunits.h
constexpr int kInjectUnit = WwCellUnit::AirInject;		// stage 2 reads stage 1
constexpr int kClusterUnit = WwCellUnit::AirCluster;	// the optimisation (coordinator 10-14): the clusters' light lists (isamplerBuffer),
constexpr int kGbufUnit = WwCellUnit::AirGbuf;		// lane AO1's g-buffer (stage 0 reads its depth)
constexpr int kTileUnit = WwCellUnit::AirTile;		// and stage 0's per-column ray distances (stage 1 reads them); off below AirTile + 1 units
constexpr int kClusterCols = 8;		// a light cluster: 8 x 8 froxel columns x 8 slices
constexpr int kClusterSlices = 8;
constexpr int kColumnPx = 12;		// a froxel column: 12 x 12 pixels
constexpr int kSlices = 64;
constexpr float kNearMin = 16.0f;	// game units

// the reds (gates only): 1 off (computed, not applied), 2 gioff, 4 flat, 8 noshadow, 16 nolights, 32 wrongmed,
// 64 blendnear, 128 farfit, 256 physcol, 512 nosunt, 1024 froxonly, 2048 oldsun (celllights.cpp reads it: the air off the exterior sun and sky),
// 4096 dropcluster (every cluster list drops its nearest light), 8192 nearskip (the depth skip behind the column's NEAREST
// surface, undilated), 1048576 zcolour (the depth skip's columns from the obscurance pass's colour, as before 10-16),
// 2097152 nofarclamp (the surfaces' lookup not clamped to their column's farthest opaque distance, as before 10-16)
struct VaState
{
	bool loaded = false;
	quint64 liveGen = 0;	// lane TESTDRIVER1: the switch generation last read
	bool on = false, pinned = false;
	int farTier = 0;	// lane FARGRID1: the Far Air row, 0 Off .. 4 Ultra (the pin WW_CELL_AIR_FAR wins)
	bool farPinned = false;
	int red = 0;
	bool shellStep = false;	// SKY2 red WW_CELL_AIR_RED=shellstep: the old law (no air volume, the GI shell's edge one voxel wide)
	bool noAirVol = false;	// SKY2 red WW_CELL_AIR_RED=noairvol: no air volume, the shell read over the probes' reach (the stopgap)
	bool sphereTop = false;	// lane CLOUDLOOK1 red WW_CELL_AIR_RED=spheretop: the air's top at the scene sphere's (the veil)
	int terms = 15;
	int probe = 0;
	int fogBlend = 0;	// WW_CELL_VOLAIR_FOGBLEND=1 (option B, test only): the game's fog drawn over the lit air
	int tricubic = 0;	// WW_CELL_VOLAIR_TRICUBIC=1 (FOGTRICUBIC1, bungo 10-21): the surfaces' B-spline froxel read; off until he rules
	int opt = 3;		// WW_CELL_AIR_OPT (gates): 1 clustered lights, 2 the conservative depth skip; 0 the full loop
	QString dump;
	bool temporal = false;
	int shiftFrame = -1;
	float shift[3] = { 0, 0, 0 };
	int settleLeft = 0;			// temporal: paints still owed after a change (interactive windows)
	bool settlePending = false;
	bool inPass = false;
	QString last = QStringLiteral( "off" );
};

VaState & vs()
{
	static VaState s;
	// lane TESTDRIVER1: a live set of these switches re-reads them (pins and or-ed reds back to their defaults first)
	if ( !s.loaded || wwSwitchMovedFor( s.liveGen, { "WW_CELL_AIR", "WW_CELL_VOLAIR" } ) ) {
		s.loaded = true;
		s.liveGen = wwSwitchGen();
		{
			const VaState d {};
			s.pinned = d.pinned;
			s.farPinned = d.farPinned;
			s.red = d.red;
			s.shellStep = d.shellStep;
			s.noAirVol = d.noAirVol;
			s.sphereTop = d.sphereTop;
			s.terms = d.terms;
			s.opt = d.opt;
		}
		const QByteArray pin = wwEnv( "WW_CELL_AIR" );
		if ( !pin.isEmpty() ) {
			s.pinned = true;
			s.on = pin.trimmed() != "0";
		} else {
			s.on = QSettings().value( QStringLiteral( "WW/VolAir" ), false ).toBool();
		}
		for ( const QByteArray & t : wwEnv( "WW_CELL_AIR_RED" ).split( ',' ) ) {
			const QByteArray r = t.trimmed();
			s.shellStep = s.shellStep || r == "shellstep";
			s.noAirVol = s.noAirVol || r == "noairvol";
			s.sphereTop = s.sphereTop || r == "spheretop";
			s.red |= r == "off" ? 1 : r == "gioff" ? 2 : r == "flat" ? 4 : r == "noshadow" ? 8 : r == "nolights" ? 16
				: r == "wrongmed" ? 32 : r == "blendnear" ? 64 : r == "farfit" ? 128 : r == "physcol" ? 256 : r == "nosunt" ? 512 : r == "froxonly" ? 1024 : r == "oldsun" ? 2048
				: r == "dropcluster" ? 4096 : r == "nearskip" ? 8192 : r == "noground" ? 16384 : r == "nosidecar" ? 32768 : r == "nosky" ? 65536 : r == "fxfog" ? 131072 : r == "lightmid" ? 262144 : r == "bluescatter" ? 524288 : r == "zcolour" ? 1048576 : r == "nofarclamp" ? 2097152 : r == "roomstep" ? 4194304
				: r == "echoall" ? 8388608
				: r == "farnoterr" ? 16777216 : r == "farseam" ? 33554432 : r == "farjitter" ? 67108864	// lane FARGRID1: the far reds
				: r == "histstale" ? 134217728 : r == "oldint" ? 268435456 : r == "noearly" ? 536870912 : r == "noskip" ? 1073741824 : 0;
			// BATCH1: PERF2's oldint/noearly/noskip moved off FARGRID1's far reds (2^24..2^26) to 2^28..2^30
			// lane PERF2: echoall moved off 4194304 (VOLAIR1's roomstep); oldint the O(S^2) integrate, noearly no empty-air
			// early out, noskip recomputes unchanged frames, histstale (temporal gates) never refreshes the history
		}
		// lane PERF2: temporal reuse (one fresh stratum a frame + the history); off until bungo sees it. The red brings
		// the 4 sub-steps back
		s.temporal = qEnvironmentVariableIntValue( "WW_CELL_VOLAIR_TEMPORAL" ) == 1
			&& qEnvironmentVariableIntValue( "WW_CELL_VOLAIR_TEMPORAL_RED" ) != 1;
		// gates only: from path frame N the air sees every placed light moved by (dx, dy, dz) world units: "N,dx,dy,dz"
		const QList<QByteArray> sh = qgetenv( "WW_CELL_AIR_LIGHTSHIFT" ).split( ',' );
		if ( sh.size() == 4 ) {
			s.shiftFrame = sh[0].toInt();
			for ( int k = 0; k < 3; k++ )
				s.shift[k] = sh[k + 1].toFloat();
		}
		const QByteArray opt = wwEnv( "WW_CELL_AIR_OPT" ).trimmed();
		if ( !opt.isEmpty() )
			s.opt = opt.toInt() & 3;
		const QByteArray terms = wwEnv( "WW_CELL_AIR_TERMS" ).trimmed();
		if ( !terms.isEmpty() )
			s.terms = terms.toInt() & 15;
		s.probe = wwEnvInt( "WW_CELL_AIR_PROBE" );
		s.fogBlend = wwEnvInt( "WW_CELL_VOLAIR_FOGBLEND" ) == 1 ? 1 : 0;
		s.tricubic = wwEnvInt( "WW_CELL_VOLAIR_TRICUBIC" ) == 1 ? 1 : 0;
		if ( s.fogBlend && ( s.red & 64 ) )
			s.fogBlend = 2;	// red blendnear: the fog colour by the air's own 1 - T, no start distance (must fail B)
		s.farTier = std::clamp( QSettings().value( QStringLiteral( "WW/VolAirFar" ), 0 ).toInt(), 0, 4 );	// lane FARGRID1
		const QByteArray farPin = wwEnv( "WW_CELL_AIR_FAR" ).trimmed();	// lane FARGRID1: the pin wins
		if ( !farPin.isEmpty() ) {
			s.farPinned = true;
			s.farTier = std::clamp( farPin.toInt(), 0, 4 );
		}
		s.dump = QString::fromLocal8Bit( wwEnv( "WW_CELL_AIR_DUMP" ) );
	}
	return s;
}

/* lane FOGDRIFT1: the near air's drifting lumps (cellvolair.h). One tileable 64^3 R8 noise built once; its exact texel
 * mean and largest deviation go to the shader, so the factor 1 + a (n - mean) / span keeps the mean density. */
constexpr int kNoiseUnit = WwCellUnit::AirNoise;
constexpr int kNoiseN = 64;
constexpr float kDriftTile = 4096.0f;	// the base octave's tile, game units (~58 m); the detail octave's is 0.45 of it
constexpr float kDriftDetail = 0.45f;
constexpr float kDriftAmp = 0.95f;	// the density factor spans 0.05 .. 1.95 at the texture's extremes (std ~0.2)
constexpr float kDriftFade0 = 2000.0f, kDriftFade1 = 8000.0f;	// full strength to fade0, smooth air past fade1 (no shimmer)
constexpr float kDriftSpeed = 400.0f;	// units per second at the weather's full wind speed (DATA byte 255)

struct VaDrift
{
	bool loaded = false, on = false, pinned = false;
	quint64 liveGen = 0;		// fix TDFIX2: a live set of WW_CELL_FOGDRIFT* re-reads them
	int red = 0;				// 1 still (no motion), 2 smooth (no noise), 3 wobble (the pre-FOGWIND1 wind law)
	bool timePinned = false;
	double tPin = 0.0;
	QElapsedTimer clock;
	double tDone = 0.0;			// the offsets are integrated up to this time
	float ofs[2][3] = {};		// the two octaves' offsets, world units
	float vel[2] = { 0, 0 };	// the eased wind velocity (units / s)
	bool moving = false;		// the last pass drew moving lumps (the view keeps repainting)
	std::vector<quint8> tex;
	float mean = 0.5f, span = 0.5f;
	QString note = QStringLiteral( "off" );
};

VaDrift & vd()
{
	static VaDrift d;
	if ( !d.loaded || wwSwitchMovedFor( d.liveGen, { "WW_CELL_FOGDRIFT" } ) ) {
		d.loaded = true;
		d.liveGen = wwSwitchGen();
		d.pinned = false;	// back to the defaults before the re-read
		d.timePinned = false;
		d.tPin = 0.0;
		const QByteArray pin = qgetenv( "WW_CELL_FOGDRIFT" ).trimmed();
		if ( !pin.isEmpty() ) {
			d.pinned = true;
			d.on = pin != "0";
		} else {
			d.on = QSettings().value( QStringLiteral( "WW/VolAirDrift" ), false ).toBool();
		}
		const QByteArray red = qgetenv( "WW_CELL_FOGDRIFT_RED" ).trimmed();
		d.red = red == "still" ? 1 : red == "smooth" ? 2 : red == "wobble" ? 3 : 0;
		bool ok = false;
		const double t = qEnvironmentVariable( "WW_CELL_FOGDRIFT_T" ).toDouble( &ok );
		if ( ok ) {	// a fixed clock (the shots' frames): the drift integrated from 0 to t in fixed steps
			d.timePinned = true;
			d.tPin = std::max( t, 0.0 );
		} else if ( !wwWorldClockLive() ) {	// a harness never reads the wall clock
			d.timePinned = true;
		}
	}
	return d;
}

// tileable gradient noise at (x, y, z) in lattice cells, period P cells
float vaGrad( float x, float y, float z, int P )
{
	auto hash = []( int i, int j, int k ) {
		std::uint32_t h = std::uint32_t( i ) * 73856093u ^ std::uint32_t( j ) * 19349663u ^ std::uint32_t( k ) * 83492791u;
		h ^= h >> 13;
		h *= 0x5bd1e995u;
		return h ^ ( h >> 15 );
	};
	static const float g[12][3] = { { 1, 1, 0 }, { -1, 1, 0 }, { 1, -1, 0 }, { -1, -1, 0 }, { 1, 0, 1 }, { -1, 0, 1 },
		{ 1, 0, -1 }, { -1, 0, -1 }, { 0, 1, 1 }, { 0, -1, 1 }, { 0, 1, -1 }, { 0, -1, -1 } };
	const int x0 = int( std::floor( x ) ), y0 = int( std::floor( y ) ), z0 = int( std::floor( z ) );
	const float fx = x - float( x0 ), fy = y - float( y0 ), fz = z - float( z0 );
	auto fade = []( float t ) { return t * t * t * ( t * ( t * 6.0f - 15.0f ) + 10.0f ); };
	float r = 0.0f;
	for ( int c = 0; c < 8; c++ ) {
		const int dx = c & 1, dy = ( c >> 1 ) & 1, dz = ( c >> 2 ) & 1;
		const float * v = g[hash( ( x0 + dx ) % P, ( y0 + dy ) % P, ( z0 + dz ) % P ) % 12];
		const float d = v[0] * ( fx - float( dx ) ) + v[1] * ( fy - float( dy ) ) + v[2] * ( fz - float( dz ) );
		r += d * ( dx ? fade( fx ) : 1.0f - fade( fx ) ) * ( dy ? fade( fy ) : 1.0f - fade( fy ) ) * ( dz ? fade( fz ) : 1.0f - fade( fz ) );
	}
	return r;
}

// the 64^3 texture: two octaves (4 and 8 lattice cells across), bytes over their own range; mean and span measured
void vaNoiseBuild( VaDrift & d )
{
	const int n = kNoiseN;
	std::vector<float> v( size_t( n ) * n * n );
	float lo = 1e30f, hi = -1e30f;
	for ( int k = 0; k < n; k++ )
		for ( int j = 0; j < n; j++ )
			for ( int i = 0; i < n; i++ ) {
				const float x = ( float( i ) + 0.5f ) / float( n ), y = ( float( j ) + 0.5f ) / float( n ), z = ( float( k ) + 0.5f ) / float( n );
				const float f = vaGrad( x * 4.0f, y * 4.0f, z * 4.0f, 4 ) + 0.5f * vaGrad( x * 8.0f + 0.31f, y * 8.0f + 0.17f, z * 8.0f + 0.53f, 8 );
				v[( size_t( k ) * n + j ) * n + i] = f;
				lo = std::min( lo, f );
				hi = std::max( hi, f );
			}
	d.tex.resize( v.size() );
	double sum = 0.0;
	int bmin = 255, bmax = 0;
	for ( size_t i = 0; i < v.size(); i++ ) {
		const int b = int( std::lround( ( v[i] - lo ) / std::max( hi - lo, 1e-6f ) * 255.0f ) );
		d.tex[i] = quint8( b );
		sum += b;
		bmin = std::min( bmin, b );
		bmax = std::max( bmax, b );
	}
	const double mean = sum / double( v.size() );
	d.mean = float( mean / 255.0 );
	d.span = float( std::max( mean - bmin, bmax - mean ) / 255.0 );
}

/* the wind's offsets at time t (fixed steps of at most 1/30 s; a pinned clock restarts from 0, run to run identical).
 * FOGWIND1: the game's wind, a steady bearing (the grass's angle, 0 = +X counter-clockwise) at speed x magnitude; the
 * game's turbulence only sways plants. Red wobble: the old law (range/turbulence wobble and gust, 0 = +Y clockwise).
 * The velocity eases toward its target (2 s), so a weather change never jumps. The detail octave moves 1.35x faster. */
void vaDriftAdvance( VaDrift & d, double t, const quint8 w[4] )
{
	if ( t < d.tDone ) {
		d.tDone = 0.0;
		d.vel[0] = d.vel[1] = 0.0f;
		std::fill( &d.ofs[0][0], &d.ofs[0][0] + 6, 0.0f );
	}
	if ( !d.timePinned && t - d.tDone > 1.0 )
		d.tDone = t - 1.0;	// an idle window: carry on from a second ago, no catch-up loop
	const double twoPi = 6.283185307179586;
	const double M = w[0] / 255.0, dir = w[1] / 255.0 * twoPi, range = w[2] / 255.0 * 0.5 * twoPi, T = w[3] / 255.0;
	const float gameA = wwGameWindAngle( w[1] );
	const int steps = int( std::ceil( ( t - d.tDone ) * 30.0 ) );
	const double dt = steps > 0 ? ( t - d.tDone ) / steps : 0.0;
	for ( int s = 0; s < steps; s++ ) {
		const double e = 1.0 - std::exp( -dt / 2.0 );
		double vx, vy;
		if ( d.red == 3 ) {
			const double u = d.tDone + dt * ( s + 0.5 );
			const double a = dir + 0.5 * range * ( 0.6 * std::sin( twoPi * u / 23.0 ) + 0.4 * std::sin( twoPi * u / 9.1 ) );
			const double gust = std::max( 0.0, 1.0 + T * ( 0.35 * std::sin( twoPi * u / 7.3 ) + 0.25 * std::sin( twoPi * u / 3.1 ) ) );
			const double v = kDriftSpeed * M * gust;
			vx = v * std::sin( a );
			vy = v * std::cos( a );
		} else {
			const double v = kDriftSpeed * M;
			vx = v * std::cos( double( gameA ) );
			vy = v * std::sin( double( gameA ) );
		}
		d.vel[0] += float( ( vx - d.vel[0] ) * e );
		d.vel[1] += float( ( vy - d.vel[1] ) * e );
		for ( int o = 0; o < 2; o++ ) {
			// wrapped at the octave's own tile: the texture repeats there, so the wrap is seamless
			const float k = float( dt ) * ( o ? 1.35f : 1.0f ), tile = kDriftTile * ( o ? kDriftDetail : 1.0f );
			d.ofs[o][0] = std::fmod( d.ofs[o][0] + d.vel[0] * k, tile );
			d.ofs[o][1] = std::fmod( d.ofs[o][1] + d.vel[1] * k, tile );
			if ( o )
				d.ofs[o][2] = std::fmod( d.ofs[o][2] + 0.15f * std::hypot( d.vel[0], d.vel[1] ) * k, tile );
		}
	}
	d.tDone = t;
}

/* lane FARGRID1: the far grid's presets (cellvolair.h). Fixed sizes, not tied to the screen; 1 game unit = 1.43 cm, so
 * 1 km = 69,930 units. The end is also never past where the near grid would have ended (the medium's farD). */
struct VaFarPreset
{
	int cols, rows, slices;
	float endKm;
	int refresh;	// frames between rebuilds of an unchanged view (the terrain map's amortized sweep lands)
};
constexpr VaFarPreset kFarPresets[5] = {
	{ 0, 0, 0, 0.0f, 0 },
	{ 64, 36, 16, 4.0f, 32 },		// Low
	{ 96, 54, 24, 8.0f, 16 },		// Medium
	{ 160, 88, 32, 12.0f, 8 },		// High
	{ 240, 132, 48, 16.0f, 4 },		// Ultra
};
constexpr float kUnitsPerKm = 69930.07f;
constexpr float kFarHandoff = 17578.0f;	// the near grid's end with the far grid on: 250 m (RDR2's near range, < 160 m)

struct VaGpu
{
	GLuint inject = 0, integ = 0, fbo = 0;
	int cols = 0, rows = 0, slices = 0;
	int texW = 0, texH = 0, texD = 0;	// lane FARGRID1: the packed volume (the near grid, then the far grid's layers)
	int farCols = 0, farRows = 0, farSlices = 0;
	bool farOn = false;
	float farStart = 0.0f, farLog = 1.0f;
	std::vector<float> farKey;	// what the far grid was built from (the view, the sun, the medium)
	int farAge = 0;
	int farBuilds = 0;
	GLuint farQ[2] = { 0, 0 };
	bool farQPending = false;
	double farGpuMs = -1.0;
	const void * doc = nullptr;
	bool ready = false;
	float near = kNearMin, logRatio = 1.0f;
	float med[4] = { 0, 0, 0, 0 };	// the medium the pass drew (airMed), for the surfaces' sun (FOGSUN1)
	float layer[4] = { 0, 0, 0, 0 }, ground[4] = { 0, 0, 0, 0 };	// its near band and ground layer (airLayer, airGround)
	int unitsOk = -1;
	bool optUnitsOk = false;
	GLuint clBuf = 0, clTex = 0, tile = 0;	// the clusters' lists; stage 0's per-column distances (RG32F)
	int tileCols = 0, tileRows = 0;
	bool tileValid = false;	// stage 0 wrote this frame's column distances (the surfaces clamp to them)
	GLuint zTex = 0, zFbo = 0, zSrc = 0;	// stage 0's depth: a copy of the obscurance pass's depth buffer (10-16)
	int zW = 0, zH = 0;
	GLuint carry[2] = { 0, 0 };	// lane PERF2: the O(S) integrate's running (L, T), ping-pong between its draws (RGBA32F)
	int mrtOk = -1;				// 8 draw buffers and colour attachments (else the O(S^2) integrate)
	// lane PERF2, temporal: the strata (cols x rows x 4 slices, stratum p at layers p x slices ..), ping-pong; the
	// last pass's view (posView = sc R world + t: rows sc R | t) and grid
	GLuint hist[2] = { 0, 0 };
	int histCur = 0, histCols = 0, histRows = 0;
	bool histOk = false;
	double prevV[3][4] = {};
	float prevGrid[4] = { 0, 0, 0, 0 }, prevScale = 0.0f;
	unsigned frame = 0;
	int tmode = 0;
	// lane VOLPART1: the frame's phase and cascades, for the particles lit by the same law (wwVolPartUniforms; the
	// medium is med / layer / ground above)
	float phase[3][4] = {};
	float gi[4] = {};
	bool csm = false;
	GLuint noise = 0;	// lane FOGDRIFT1: the drift's noise (R8, repeat)
	GLuint terr = 0;	// lane FOGTERRAIN1: the low-pass terrain (R32F, clamp)
	std::uint64_t terrGen = 0;
	QString terrNote = QStringLiteral( "off" );
	bool terrOn = false;	// lane FOGTERRAIN2: the pass stood on the grid; the surfaces and particles read the same
	float terrXf[4] = { 0, 0, 0, 0 }, terrRef = 0.0f;
	float terrFineXf[4] = { 0, -1, 0, -1 };	// lane FOGTERRAIN3: layer 1's rows (uv -1: no fine level)
};

/* THE CLUSTERED LIGHTS (coordinator 10-14). The inject evaluates the placed lights once per froxel, at the middle of
 * its slice on its column's centre ray (since LAMPAIR2: over the slice's stretch [t0, t1]), and adds nothing at or past its radius
 * (cell_volair.frag airRest: d >= radius). A cluster (kClusterCols x kClusterCols columns x kClusterSlices slices)
 * lists the lights whose sphere can reach one of its points: the points' view-space box (exact: a point is dir x t
 * with t > 0, so each coordinate is extreme at the cluster's near or far slice EDGE) against the light's
 * centre in view space and its radius over the view's smallest stretch (world = R view + T: |R v| >= s_min |v|), padded
 * for the GPU's float error. Lists keep the ascending buffer order: the sum adds the same terms in the same order.
 * The buffer: per cluster (offset, count), then the indices. */
double vaSigmaMin( const double R[3][3] )
{
	// the smallest eigenvalue of R^T R (symmetric 3 x 3, closed form), its square root
	double A[3][3];
	for ( int i = 0; i < 3; i++ )
		for ( int j = 0; j < 3; j++ )
			A[i][j] = R[0][i] * R[0][j] + R[1][i] * R[1][j] + R[2][i] * R[2][j];
	const double p1 = A[0][1] * A[0][1] + A[0][2] * A[0][2] + A[1][2] * A[1][2];
	const double q = ( A[0][0] + A[1][1] + A[2][2] ) / 3.0;
	double emin;
	if ( p1 <= 1e-30 * std::max( q * q, 1e-300 ) ) {
		emin = std::min( { A[0][0], A[1][1], A[2][2] } );
	} else {
		const double p2 = ( A[0][0] - q ) * ( A[0][0] - q ) + ( A[1][1] - q ) * ( A[1][1] - q ) + ( A[2][2] - q ) * ( A[2][2] - q ) + 2.0 * p1;
		const double p = std::sqrt( p2 / 6.0 );
		double B[3][3];
		for ( int i = 0; i < 3; i++ )
			for ( int j = 0; j < 3; j++ )
				B[i][j] = ( A[i][j] - ( i == j ? q : 0.0 ) ) / p;
		const double detB = B[0][0] * ( B[1][1] * B[2][2] - B[1][2] * B[2][1] ) - B[0][1] * ( B[1][0] * B[2][2] - B[1][2] * B[2][0] )
			+ B[0][2] * ( B[1][0] * B[2][1] - B[1][1] * B[2][0] );
		const double rr = std::clamp( detB / 2.0, -1.0, 1.0 );
		const double phi = std::acos( rr ) / 3.0;
		emin = q + 2.0 * p * std::cos( phi + 2.0 * 3.14159265358979323846 / 3.0 );
	}
	return std::sqrt( std::max( emin, 0.0 ) ) * ( 1.0 - 1e-6 );
}

struct VaClusters
{
	std::vector<GLint> buf;
	int ncx = 0, ncy = 0, ncz = 0, refs = 0, nonEmpty = 0;
};

bool vaBuildClusters( VaClusters & out, const std::vector<float> & spheres, const float rowV[3][4], float distScale,
	int cols, int rows, float nearD, float logRatio, float p00, float p11, bool dropRed )
{
	const int n = int( spheres.size() / 4 );
	double R[3][3], T[3];
	for ( int i = 0; i < 3; i++ ) {
		for ( int j = 0; j < 3; j++ )
			R[i][j] = double( rowV[i][j] );
		T[i] = double( rowV[i][3] );
	}
	const double det = R[0][0] * ( R[1][1] * R[2][2] - R[1][2] * R[2][1] ) - R[0][1] * ( R[1][0] * R[2][2] - R[1][2] * R[2][0] )
		+ R[0][2] * ( R[1][0] * R[2][1] - R[1][1] * R[2][0] );
	const double sMin = vaSigmaMin( R );
	if ( !( std::abs( det ) > 1e-30 ) || !( sMin > 0.0 ) || !( distScale > 0.0f ) || n <= 0 )
		return false;
	double I[3][3];
	I[0][0] = ( R[1][1] * R[2][2] - R[1][2] * R[2][1] ) / det;
	I[0][1] = ( R[0][2] * R[2][1] - R[0][1] * R[2][2] ) / det;
	I[0][2] = ( R[0][1] * R[1][2] - R[0][2] * R[1][1] ) / det;
	I[1][0] = ( R[1][2] * R[2][0] - R[1][0] * R[2][2] ) / det;
	I[1][1] = ( R[0][0] * R[2][2] - R[0][2] * R[2][0] ) / det;
	I[1][2] = ( R[0][2] * R[1][0] - R[0][0] * R[1][2] ) / det;
	I[2][0] = ( R[1][0] * R[2][1] - R[1][1] * R[2][0] ) / det;
	I[2][1] = ( R[0][1] * R[2][0] - R[0][0] * R[2][1] ) / det;
	I[2][2] = ( R[0][0] * R[1][1] - R[0][1] * R[1][0] ) / det;
	// the lights in view space, the radius over the smallest stretch, padded (the GPU's P and d are float)
	std::vector<double> lv( size_t( n ) * 4 );
	for ( int i = 0; i < n; i++ ) {
		const float * s = &spheres[size_t( i ) * 4];
		const double d[3] = { double( s[0] ) - T[0], double( s[1] ) - T[1], double( s[2] ) - T[2] };
		double * o = &lv[size_t( i ) * 4];
		for ( int a = 0; a < 3; a++ )
			o[a] = I[a][0] * d[0] + I[a][1] * d[1] + I[a][2] * d[2];
		const double wl = std::sqrt( double( s[0] ) * s[0] + double( s[1] ) * s[1] + double( s[2] ) * s[2] );
		o[3] = ( std::max( double( s[3] ), 0.0 ) * 1.001 + 2.0 + 1e-5 * wl ) / sMin;
	}
	// the slices' EDGES (view units along the ray), the columns' centre directions (view space): the shader's.
	// lane VOLAIR1 10-15: since LAMPAIR2 a light is sampled over the slice's whole stretch [t0, t1], not at its middle, so
	// a cluster's box runs from its first slice's near edge to its last slice's far edge (the middles culled a lamp that
	// reached only the outer half of a boundary slice: the opt gate's Q on slices 16, 23-24, 31-32 at Memory Den).
	std::vector<double> te( size_t( kSlices ) + 1 );
	for ( int k = 0; k <= kSlices; k++ )
		te[size_t( k )] = double( nearD ) * std::exp( double( logRatio ) * double( k ) / double( kSlices ) ) / double( distScale );
	std::vector<double> dir( size_t( cols ) * size_t( rows ) * 3 );
	for ( int r = 0; r < rows; r++ )
		for ( int c = 0; c < cols; c++ ) {
			const double nx = ( double( c ) + 0.5 ) / double( cols ) * 2.0 - 1.0, ny = ( double( r ) + 0.5 ) / double( rows ) * 2.0 - 1.0;
			const double v[3] = { nx / double( p00 ), ny / double( p11 ), -1.0 };
			const double l = std::sqrt( v[0] * v[0] + v[1] * v[1] + 1.0 );
			double * o = &dir[( size_t( r ) * size_t( cols ) + size_t( c ) ) * 3];
			for ( int a = 0; a < 3; a++ )
				o[a] = v[a] / l;
		}
	out.ncx = ( cols + kClusterCols - 1 ) / kClusterCols;
	out.ncy = ( rows + kClusterCols - 1 ) / kClusterCols;
	out.ncz = ( kSlices + kClusterSlices - 1 ) / kClusterSlices;
	const int ncl = out.ncx * out.ncy * out.ncz;
	out.buf.assign( size_t( ncl ) * 2, 0 );
	out.refs = out.nonEmpty = 0;
	std::vector<GLint> list;
	for ( int cz = 0; cz < out.ncz; cz++ ) {
		const double ta = te[size_t( cz * kClusterSlices )];
		const double tb = te[size_t( std::min( kSlices, ( cz + 1 ) * kClusterSlices ) )];
		for ( int cy = 0; cy < out.ncy; cy++ )
			for ( int cx = 0; cx < out.ncx; cx++ ) {
				double lo[3] = { 1e300, 1e300, 1e300 }, hi[3] = { -1e300, -1e300, -1e300 };
				for ( int r = cy * kClusterCols; r < std::min( rows, ( cy + 1 ) * kClusterCols ); r++ )
					for ( int c = cx * kClusterCols; c < std::min( cols, ( cx + 1 ) * kClusterCols ); c++ ) {
						const double * d = &dir[( size_t( r ) * size_t( cols ) + size_t( c ) ) * 3];
						for ( int a = 0; a < 3; a++ ) {
							lo[a] = std::min( { lo[a], d[a] * ta, d[a] * tb } );
							hi[a] = std::max( { hi[a], d[a] * ta, d[a] * tb } );
						}
					}
				const double padP = 1e-5 * tb + 1e-3;	// the GPU's slice middles and directions are float
				for ( int a = 0; a < 3; a++ ) {
					lo[a] -= padP;
					hi[a] += padP;
				}
				list.clear();
				int nearest = -1;
				double nearestD = 1e300;
				for ( int i = 0; i < n; i++ ) {
					const double * o = &lv[size_t( i ) * 4];
					double d2 = 0.0;
					for ( int a = 0; a < 3; a++ ) {
						const double e = o[a] < lo[a] ? lo[a] - o[a] : o[a] > hi[a] ? o[a] - hi[a] : 0.0;
						d2 += e * e;
					}
					if ( d2 < o[3] * o[3] ) {
						if ( d2 < nearestD ) {
							nearestD = d2;
							nearest = int( list.size() );
						}
						list.push_back( GLint( i ) );
					}
				}
				if ( dropRed && nearest >= 0 )
					list.erase( list.begin() + nearest );	// red dropcluster: a reaching light left out
				const int ci = ( cz * out.ncy + cy ) * out.ncx + cx;
				out.buf[size_t( ci ) * 2] = GLint( out.buf.size() );
				out.buf[size_t( ci ) * 2 + 1] = GLint( list.size() );
				out.buf.insert( out.buf.end(), list.begin(), list.end() );
				out.refs += int( list.size() );
				out.nonEmpty += list.empty() ? 0 : 1;
			}
	}
	return true;
}

QHash<const void *, VaGpu> & vaGpus()
{
	static QHash<const void *, VaGpu> g;
	return g;
}

void vaAlloc3D( NifSkopeOpenGLContext::GLFunctions * fn, GLuint & tex, int w, int h, int d, bool linear )
{
	if ( !tex )
		fn->glGenTextures( 1, &tex );
	fn->glBindTexture( GL_TEXTURE_3D, tex );
	fn->glTexImage3D( GL_TEXTURE_3D, 0, GL_RGBA32F, w, h, d, 0, GL_RGBA, GL_FLOAT, nullptr );
	fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST );
	fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST );
	fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 0 );
	fn->glBindTexture( GL_TEXTURE_3D, 0 );
}

/* the phase uniforms from a packed god-ray record (air rgb, fwd rgb, back rgb, fwd g, back g, intensity). Lane VOLAIR1
 * 10-15 (bungo: "our fog is what vanilla has as the color"): the air adds no hue of its own. Each lobe's weight is
 * its luminance on all three channels, so the haze's colour comes from the fog records alone (airEnvColour) and a
 * placed light keeps its own colour in the air. The records' coloured weights (the air lobe is blue) took a warm
 * lamp's hue out of its halo. Red bluescatter (coloured = true): the records' coloured weights, as before. */
struct VaPhase
{
	float w[3][4] = {};	// rgb weight, w = g
	float gi[4] = {};	// rgb tint (the three weights summed), w = the luminance-weighted g
};

VaPhase vaPhase( const float m[12], bool coloured )
{
	VaPhase p;
	const float lumK[3] = { 0.2126f, 0.7152f, 0.0722f };
	float tot[3];
	for ( int c = 0; c < 3; c++ )
		tot[c] = m[c] + m[3 + c] + m[6 + c];
	const float lum = tot[0] * lumK[0] + tot[1] * lumK[1] + tot[2] * lumK[2];
	const float inv = lum > 1e-6f ? 1.0f / lum : 0.0f;
	for ( int k = 0; k < 3; k++ )
		for ( int c = 0; c < 3; c++ )
			p.w[k][c] = m[k * 3 + c] * inv;
	if ( !coloured )
		for ( int k = 0; k < 3; k++ ) {
			const float l = p.w[k][0] * lumK[0] + p.w[k][1] * lumK[1] + p.w[k][2] * lumK[2];
			p.w[k][0] = p.w[k][1] = p.w[k][2] = l;
		}
	p.w[0][3] = 0.0f;
	p.w[1][3] = m[9];
	p.w[2][3] = m[10];
	auto lumOf = [&]( int k ) { return p.w[k][0] * lumK[0] + p.w[k][1] * lumK[1] + p.w[k][2] * lumK[2]; };
	for ( int c = 0; c < 3; c++ )
		p.gi[c] = p.w[0][c] + p.w[1][c] + p.w[2][c];
	p.gi[3] = lumOf( 1 ) * m[9] + lumOf( 2 ) * m[10];	// the air's g is 0; the lobes' luminances sum to 1
	return p;
}

/* the medium fitted to the fog records in force (cellvolair.h): sigma0 (per game unit), zb, 1 / H (0: uniform), the
 * full distance, the far edge of the slices. The red wrongmed: twice the density and no height falloff (the "one
 * exponential" law); the red farfit: the density fitted at the full distance alone (the street-range air too thin). */
struct VaMedium
{
	float sigma0 = 0.0f, zb = 0.0f, invH = 0.0f, dFull = 0.0f, farD = 0.0f;
	bool band = false;
};

// the game's fog alpha at distance d and fog-frame height z (lookdev_fog.glsl wwFogEval, the same formula)
double vaGameAlpha( const float K[6][4], double d, double z )
{
	const double ramp = d * K[0][0] - K[0][2];
	const double f = std::min( std::max( ramp, 0.0 ), 1.0 );
	const double p0 = std::min( std::max( z * K[5][0] - K[5][2], 0.0 ), 1.0 );
	const double p1 = std::min( std::max( z * K[5][1] - K[5][3], 0.0 ), 1.0 );
	const double hb = p0 + ( p1 - p0 ) * f;
	const double mx = K[2][3];
	const double clampT = ramp > 0.75 ? std::min( ( f - 0.75 ) * 4.0 * ( 1.0 - mx ) + mx, 1.0 ) : mx;
	const double escape = ramp < 0.015 ? f * 66.666672 : 1.0;
	const double I = f > 0.0 ? std::min( clampT, std::pow( f, double( K[1][3] ) ) ) : 0.0;
	return ( hb * K[3][3] + ( 1.0 - hb ) ) * I * escape;
}

/* the density: least squares in optical depth against the game's fog over the street range (64 log-spaced distances
 * from 500 to 5000 units, below the height band, none past the curve's end ramp (ramp 0.75), alpha at most the
 * records' max, which is all the air ever reaches), sigma0 = sum(d tau) / sum(d^2) with tau = -ln(1 - alpha), never
 * thinner than the far-end fit (1 - exp(-sigma0 dFull) = min(max, 0.99)) unless the record's curve ends inside the
 * street range (an interior's short steep curve; there the street fit alone). The slices end where the air reaches that
 * opacity (tau_max / sigma0, at most dFull), so what lies beyond keeps the game's 1 - max showing through. */
VaMedium vaFit( const float K[6][4], bool wrong, bool farFit )
{
	VaMedium m;
	m.dFull = ( 1.0f + K[0][2] ) / K[0][0];
	const float mx = std::min( std::max( K[2][3], 0.0f ), 0.99f );
	const float tauMax = mx > 0.0f ? -std::log( 1.0f - mx ) : 0.0f;
	const float sFar = m.dFull > 0.0f ? tauMax / m.dFull : 0.0f;
	const float k5y = K[5][1], k5w = K[5][3], top = K[3][3];
	if ( k5y > 1e-9f && top > 0.0f && top < 1.0f ) {
		const float z0 = k5w / k5y, z1 = ( 1.0f + k5w ) / k5y;
		m.zb = z0;
		m.invH = -std::log( std::max( top, 1e-3f ) ) / std::max( z1 - z0, 1e-3f );
		m.band = true;
	}
	double num = 0.0, den = 0.0;
	const double zLow = m.band ? double( m.zb ) - 10.0 : 0.0;
	const double knee = ( 0.75 + K[0][2] ) / K[0][0];	// past it the game's own end ramp to 1 (clampT), not the air
	for ( int i = 0; i < 64; i++ ) {
		const double d = 500.0 * std::pow( 10.0, i / 63.0 );
		if ( d > knee && i > 0 )
			break;
		const double a = std::min( std::max( vaGameAlpha( K, d, zLow ), 0.0 ), std::max( double( mx ), 1e-4 ) );
		num += d * -std::log( 1.0 - a );
		den += d * d;
	}
	const float sStreet = den > 0.0 ? float( num / den ) : 0.0f;
	m.sigma0 = farFit ? sFar : ( knee <= 5000.0 ? sStreet : std::max( sFar, sStreet ) );	// curve ends in the street range: street alone
	m.farD = m.dFull;
	if ( m.sigma0 > sFar && m.sigma0 > 0.0f )
		m.farD = std::min( m.dFull, tauMax / m.sigma0 );
	if ( wrong ) {
		m.sigma0 *= 2.0f;
		m.invH = 0.0f;
		m.band = false;
	}
	// measurement pin (bungo 10-16): WW_CELL_AIR_DENSITY=<x> scales the fitted density, to show a lamp's haze alone
	WW_SWITCH_STATIC( float, densPin, wwEnvIsSet( "WW_CELL_AIR_DENSITY" ) ? wwEnv( "WW_CELL_AIR_DENSITY" ).toFloat() : 1.0f );
	if ( densPin > 0.0f )
		m.sigma0 *= densPin;
	return m;
}

/* THE GROUND LAYER (bungo 10-14). The density is one height law everywhere, so mountains and the far LOD kilometres
 * away get the same air as the street, with no seam and a closed-form column (cell_volair.frag airCol):
 *   rho(z) = (1 - w) B(z; zb, Hb) + w B(z; zn, Hn) + s B(z; zg, Hg),   B(z; z0, H) = min(1, exp(-(z - z0) / H))
 * zb, Hb the records' far band (vaFit); zn, Hn their near band, w its share; s, Hg the ground layer a .recx file
 * gives (groundFog: strength, thickness), anchored at the near band's middle (else the frame's ground, 0). With no
 * file the layer is the records' alone (s = 0); w = s = 0 draws exactly the far-band medium. */
struct VaLayer
{
	float w = 0.0f, zn = 0.0f, invHn = 0.0f;	// the near band's share, its base, 1 / its scale height
	float s = 0.0f, zg = 0.0f, invHg = 0.0f;	// the ground layer: strength (x sigma0), anchor, 1 / thickness
	QString recx = QStringLiteral( "none" );	// the census: rung:file of the entry applied, or why none
	QString conflict;	// "conflict: <losers>" (one provider claimed it twice) or "overrides: <losers>", "" when alone
	bool derived = false;	// FOGHEIGHT1: the layer came from the record (vaDerived): the caller keeps the street density
};

double vaB( double z, double z0, double invH )
{
	return invH > 0.0 ? std::min( 1.0, std::exp( -( z - z0 ) * invH ) ) : 1.0;
}

/* the near band's share w: the records' fog is the near band close to the eye and the far band far off (wwFogEval's
 * hb); the air has one density for every distance, so w is the share that fits the game's alpha best over the whole
 * curve (48 log-spaced distances from 500 units to its end ramp) at 24 heights across both bands and a quarter of
 * their span past them, sigma0 held; least squares on alpha, w on a grid of 1/50. 0 when the bands are the same. */
void vaNearShare( const float K[6][4], const VaMedium & M, VaLayer & L )
{
	const float k5x = K[5][0], k5z = K[5][2], top = K[3][3];
	if ( !M.band || !( k5x > 1e-9f ) || !( M.sigma0 > 0.0f ) || !( top > 0.0f && top < 1.0f ) )
		return;
	const float n0 = k5z / k5x, n1 = ( 1.0f + k5z ) / k5x;
	L.zn = n0;
	L.invHn = -std::log( std::max( top, 1e-3f ) ) / std::max( n1 - n0, 1e-3f );
	if ( std::fabs( L.zn - M.zb ) < 0.5f && std::fabs( L.invHn - M.invH ) <= 1e-4f * M.invH )
		return;
	static float lastK[24] = {};
	static float lastW = 0.0f;
	static bool have = false;
	if ( have && std::equal( &K[0][0], &K[0][0] + 24, lastK ) ) {
		L.w = lastW;
		return;
	}
	const double b1 = M.zb - std::log( std::max( double( top ), 1e-3 ) ) / M.invH;
	const double lo = std::min( double( n0 ), double( M.zb ) ), hi = std::max( double( n1 ), b1 );
	const double span = std::max( hi - lo, 1.0 );
	const double mx = std::min( std::max( double( K[2][3] ), 1e-4 ), 0.99 );
	const double knee = ( 0.75 + K[0][2] ) / K[0][0];
	const double dEnd = std::max( std::min( knee, double( M.dFull ) ), 600.0 );
	double game[24][48];
	double zs[24], ds[48];
	for ( int j = 0; j < 24; j++ )
		zs[j] = lo - 0.25 * span + 1.5 * span * j / 23.0;
	for ( int i = 0; i < 48; i++ )
		ds[i] = 500.0 * std::pow( dEnd / 500.0, i / 47.0 );
	for ( int j = 0; j < 24; j++ )
		for ( int i = 0; i < 48; i++ )
			game[j][i] = std::min( std::max( vaGameAlpha( K, ds[i], zs[j] ), 0.0 ), mx );
	double bestE = -1.0;
	for ( int k = 0; k <= 50; k++ ) {
		const double w = k / 50.0;
		double e = 0.0;
		for ( int j = 0; j < 24; j++ ) {
			const double rho = ( 1.0 - w ) * vaB( zs[j], M.zb, M.invH ) + w * vaB( zs[j], n0, L.invHn );
			for ( int i = 0; i < 48; i++ ) {
				const double a = std::min( 1.0 - std::exp( -double( M.sigma0 ) * rho * ds[i] ), mx ) - game[j][i];
				e += a * a;
			}
		}
		if ( bestE < 0.0 || e < bestE - 1e-12 ) {
			bestE = e;
			L.w = float( w );
		}
	}
	std::copy( &K[0][0], &K[0][0] + 24, lastK );
	lastW = L.w;
	have = true;
}

/* one groundFog value at the hour: the block's own, or, when its "hours" object keys the value ("6": {"strength":
 * 5}), linear between the keyed hours around the clock (one key: that value all day) */
double vaRecxValue( const QJsonObject & g, const QString & key, double hour, double def )
{
	const double base = g.value( key ).toDouble( def );
	const QJsonObject hrs = g.value( QStringLiteral( "hours" ) ).toObject();
	std::vector<std::pair<double, double>> keys;
	for ( auto it = hrs.begin(); it != hrs.end(); ++it ) {
		bool ok = false;
		const double h = it.key().toDouble( &ok );
		const QJsonValue v = it.value().toObject().value( key );
		if ( ok && v.isDouble() )
			keys.emplace_back( std::fmod( std::fmod( h, 24.0 ) + 24.0, 24.0 ), v.toDouble() );
	}
	if ( keys.empty() )
		return base;
	std::sort( keys.begin(), keys.end() );
	if ( keys.size() == 1 )
		return keys[0].second;
	hour = std::fmod( std::fmod( hour, 24.0 ) + 24.0, 24.0 );
	for ( size_t i = 0; i < keys.size(); i++ ) {
		const auto & a = keys[i];
		const auto & b = keys[( i + 1 ) % keys.size()];
		double span = b.first - a.first, t = hour - a.first;
		if ( span <= 0.0 )
			span += 24.0;
		if ( t < 0.0 )
			t += 24.0;
		if ( t <= span )
			return a.second + ( b.second - a.second ) * ( t / span );
	}
	return base;
}

/* the census, once per load order (stderr): every loaded weather, the file and rung its groundFog comes from, or
 * none; then the count matched */
void vaRecxCensus( const QStringList & files, const QString & dataDir )
{
	static QString said;
	const QString key = files.join( QLatin1Char( ';' ) ) + QLatin1Char( '|' ) + dataDir;
	if ( key == said )
		return;
	said = key;
	const QStringList roots = wwRecxRoots( files, dataDir );
	const QVector<WwWeatherEntry> all = wwLookdevWeathers();
	int matched = 0;
	for ( const WwWeatherEntry & e : all ) {
		const quint32 lid = ( e.formID >> 24 ) == 0xFEu ? ( e.formID & 0xFFFu ) : ( e.formID & 0xFFFFFFu );
		const WwRecxHit h = wwRecxFind( roots, files, QStringLiteral( "Weathers" ), QStringLiteral( "weather" ), e.ownerFile, lid,
			e.edid, QStringLiteral( "groundFog" ) );
		matched += h.ok ? 1 : 0;
		std::fprintf( stderr, "recx census: %s %s|0x%08X -> %s\n", qPrintable( e.edid ), qPrintable( e.ownerFile ), lid,
			h.ok ? qPrintable( h.rung + QLatin1Char( ':' ) + h.file ) : "none" );
	}
	std::fprintf( stderr, "recx census: %d weathers, %d matched, roots %s\n", int( all.size() ), matched,
		qPrintable( roots.join( QLatin1Char( ';' ) ) ) );
}

// how far the ground layer has burnt off at this hour, 0..1 (GAPLOOK1 FOGEXTRAS; red noon: burnt at every hour)
double vaBurnt( double hour )
{
	unsigned char tn[4];
	wwLookdevClimateTnam( tn );
	const bool red = wwEnvStr( "WW_CELL_AIR_BURNOFF_RED" ) == QLatin1String( "noon" );
	const double rise = ( tn[0] + tn[1] ) / 12.0, set = tn[3] / 6.0 + 1.0;
	auto smooth = []( double x ) { x = std::clamp( x, 0.0, 1.0 ); return x * x * ( 3.0 - 2.0 * x ); };
	return red ? 1.0 : smooth( ( hour - rise ) / 3.5 ) * ( 1.0 - smooth( ( hour - set ) / 2.0 ) );
}

// lane FOGHEIGHT1: with no groundFog entry, a layer from the weather's own street fog (formula + sources:
// notes/fogheight1/RESULTS.md). V = 3 / sigma0 (Koschmieder); fog (V < 1 km) puts 85 % of the street air in a
// 60 m layer, haze (V > 5 km) 50 % in 1.2 km; vaKeepStreet then keeps the game's density at the anchor.
void vaDerived( const VaMedium & M, const QString & why, VaLayer & L )
{
	L.recx = why;
	if ( !wwLookOn( WwLook::GroundFogDerived ) || !( M.sigma0 > 0.0f ) )
		return;
	if ( wwEnvStr( "WW_CELL_AIR_GROUND_DERIVED_RED" ) == QLatin1String( "drop" ) ) {	// red: the layer dropped
		L.recx += QStringLiteral( " derived(red drop)" );
		return;
	}
	const double upm = double( kUnitsPerKm ) / 1000.0;
	const double vis = 3.0 / ( double( M.sigma0 ) * upm );	// metres
	double t = std::clamp( ( std::log( vis ) - std::log( 1000.0 ) ) / ( std::log( 5000.0 ) - std::log( 1000.0 ) ), 0.0, 1.0 );
	t = t * t * ( 3.0 - 2.0 * t );
	const double share = 0.85 + ( 0.5 - 0.85 ) * t;
	const double thickM = std::exp( std::log( 60.0 ) + ( std::log( 1200.0 ) - std::log( 60.0 ) ) * t );
	double str = share / ( 1.0 - share ), thick = thickM * upm;
	const double burnt = wwLookOn( WwLook::GroundBurnOff ) ? vaBurnt( wwLookdevHour() ) : 0.0;
	str *= 1.0 - burnt;
	thick *= 1.0 - 0.75 * burnt;
	L.derived = true;
	L.s = float( str );
	L.invHg = float( 1.0 / thick );
	L.recx = QStringLiteral( "derived(V %1 m share %2 thickness %3 m strength %4 burn-off %5) %6" )
		.arg( vis, 0, 'f', 0 ).arg( share, 0, 'f', 3 ).arg( thick / upm, 0, 'f', 0 ).arg( str, 0, 'f', 3 )
		.arg( burnt, 0, 'f', 2 ).arg( why );
}

// a derived layer keeps the game's density at its anchor: sigma0 x rho(zg) is what it was without the layer
void vaKeepStreet( VaMedium & M, const VaLayer & G )
{
	auto b = []( float z, float z0, float ih ) { return ih > 0.0f ? std::min( 1.0f, std::exp( -( z - z0 ) * ih ) ) : 1.0f; };
	const float rb = ( 1.0f - G.w ) * b( G.zg, M.zb, M.invH ) + G.w * b( G.zg, G.zn, G.invHn );
	if ( G.derived && rb > 0.0f )
		M.sigma0 *= rb / ( rb + G.s );
}

// the ground layer of the Lookdev weather from its .recx entry (recx.h); red nosidecar: the files ignored
void vaGround( const float K[6][4], const VaMedium & M, bool interior, bool ignoreFiles, VaLayer & L )
{
	const float k5x = K[5][0];
	L.zg = k5x > 1e-9f ? ( 0.5f + K[5][2] ) / k5x : 0.0f;
	if ( interior ) {
		L.recx = QStringLiteral( "none(interior)" );
		return;
	}
	if ( ignoreFiles ) {
		vaDerived( M, QStringLiteral( "none(red nosidecar)" ), L );
		return;
	}
	quint32 formID = 0;
	QString edid, owner, dataDir;
	QStringList files;
	if ( !wwLookdevWeatherId( formID, edid, owner, files, dataDir ) ) {
		L.recx = QStringLiteral( "none(no weather)" );
		return;
	}
	const quint32 localId = ( formID >> 24 ) == 0xFEu ? ( formID & 0xFFFu ) : ( formID & 0xFFFFFFu );
	vaRecxCensus( files, dataDir );
	const WwRecxHit hit = wwRecxFind( wwRecxRoots( files, dataDir ), files, QStringLiteral( "Weathers" ), QStringLiteral( "weather" ),
		owner, localId, edid, QStringLiteral( "groundFog" ) );
	if ( !hit.ok ) {
		vaDerived( M, QStringLiteral( "none(no entry for %1|0x%2 %3)" ).arg( owner ).arg( localId, 8, 16, QLatin1Char( '0' ) ).arg( edid ), L );
		return;
	}
	L.recx = hit.rung + QLatin1Char( ':' ) + hit.file.section( QLatin1Char( '/' ), -3 );
	if ( !hit.overridden.isEmpty() )
		L.conflict = QStringLiteral( "%1 %2" ).arg( hit.conflict ? QStringLiteral( "conflict:" ) : QStringLiteral( "overrides:" ),
			hit.overridden.join( QStringLiteral( ", " ) ) );
	const QJsonObject g = hit.block;
	const double hour = wwLookdevHour();
	const double thick = vaRecxValue( g, QStringLiteral( "thickness" ), hour, 0.0 );
	const double str = vaRecxValue( g, QStringLiteral( "strength" ), hour, 0.0 );
	if ( !( thick > 0.0 ) || !( str > 0.0 ) || !( M.sigma0 > 0.0f ) ) {
		L.recx += QStringLiteral( "(thickness %1 strength %2: no layer)" ).arg( thick ).arg( str );
		return;
	}
	L.s = float( str );
	L.invHg = float( 1.0 / thick );
	// lane GAPLOOK1 (FOGEXTRAS): a file without hour keys burns off after sunrise (notes/fog-layer-and-fogmap-research.md
	// section 0: gone 2-5 h after sunrise, the top coming down; pooling again from about 1 h after sunset)
	if ( wwLookOn( WwLook::GroundBurnOff ) && g.value( QStringLiteral( "hours" ) ).toObject().isEmpty() ) {
		const double burnt = vaBurnt( hour );
		L.s = float( str * ( 1.0 - burnt ) );
		L.invHg = float( 1.0 / ( thick * ( 1.0 - 0.75 * burnt ) ) );
		L.recx += QStringLiteral( "(burn-off %1)" ).arg( burnt, 0, 'f', 2 );
	}
}

}	// namespace

bool wwVolAirWantsSettle()
{
	VaState & s = vs();
	if ( !s.temporal || wwHarnessRun() || s.settleLeft <= 0 )
		return false;
	s.settleLeft--;
	s.settlePending = true;
	return true;
}

bool wwVolAirOn()
{
	return vs().on;
}

void wwVolAirSetOn( bool on )
{
	VaState & s = vs();
	if ( s.pinned )
		return;
	s.on = on;
	QSettings().setValue( QStringLiteral( "WW/VolAir" ), on );
}

int wwVolAirFarTier()
{
	return vs().farTier;
}

void wwVolAirSetFarTier( int tier )
{
	VaState & s = vs();
	if ( s.farPinned )
		return;
	s.farTier = std::clamp( tier, 0, 4 );
	QSettings().setValue( QStringLiteral( "WW/VolAirFar" ), s.farTier );
}

bool wwVolAirDriftOn()
{
	return vd().on;
}

void wwVolAirSetDriftOn( bool on )
{
	VaDrift & d = vd();
	if ( d.pinned )
		return;
	d.on = on;
	QSettings().setValue( QStringLiteral( "WW/VolAirDrift" ), on );
}

bool wwVolAirWantsRepaint()
{
	VaDrift & d = vd();
	const bool want = d.moving && vs().on && d.on && !d.timePinned && wwWorldClockLive();
	d.moving = false;
	return want;
}

QString wwVolAirSummary()
{
	return QStringLiteral( "volair=" ) + vs().last;
}

// the air's state on stderr whenever it changes (a shot's notes say whether the air was drawn, and why not)
static void vaEcho()
{
	static QString said;
	const VaState & s = vs();
	const QString now = QStringLiteral( "volair=%1 fogblend=%2 tricubic=%3" ).arg( s.last ).arg( s.fogBlend ).arg( s.tricubic );
	/* lane PERF1: said again only when more than the frame's own timings changed (buildms, ms): a line to a redirected
	 * stderr every frame stalled the frame ~280 ms (measured). Red echoall: every frame, as before. */
	static const QRegularExpression timings( QStringLiteral( " (buildms|ms)=[-0-9.]+" ) );
	QString key = now;
	key.remove( timings );
	if ( key != said || ( s.red & 8388608 ) ) {
		said = key;
		std::fprintf( stderr, "cell air: %s\n", qPrintable( now ) );
	}
}

static void vaPassInner( Scene * scene, bool run );

static int vaFrameSerial = 0;

// lane FOGPERF1: the pass's CPU stage by stage; a mark adds the time since the previous mark to its stage
struct VaCpuMarks
{
	bool on = false;
	qint64 t = 0;
	void start()
	{
		on = PassTime::on();
		if ( on )
			t = PassTime::nowNs();
	}
	void mark( int sub )
	{
		if ( !on )
			return;
		const qint64 n = PassTime::nowNs();
		PassTime::cpuSubAdd( sub, n - t );
		t = n;
	}
};
static VaCpuMarks vaCpu;

int wwVolAirFrameSerial()
{
	return vaFrameSerial;
}

void wwVolAirPass( Scene * scene, bool run )
{
	WW_SWITCH_STATIC( bool, prof, wwEnvInt( "WW_CELL_AIR_PROF" ) == 1 );	// lane PERF1: where the pass's CPU goes
	QElapsedTimer pt;
	pt.start();
	vaFrameSerial++;
	vaCpu.start();
	vaPassInner( scene, run );
	vaCpu.mark( PassTime::CpuAirTail );
	for ( int k : { PassTime::AirPre, PassTime::AirFar, PassTime::AirPost } )
		PassTime::subEnd( k );	// no-ops unless an early return left one open
	const qint64 tInner = pt.nsecsElapsed();
	if ( vs().on && run )
		vaEcho();
	vaCpu.mark( PassTime::CpuAirEcho );
	if ( prof )
		std::fprintf( stderr, "air prof: inner %.3f ms echo %.3f ms\n", double( tInner ) * 1e-6, double( pt.nsecsElapsed() - tInner ) * 1e-6 );
}

/* lane CLOUDLOOK1: the medium's top = the scene's highest point, taken as the highest top of a visible shape's own
 * sphere (the far LOD left out, as Scene::bounds does). The scene's sphere is a union: its top sat a radius over its
 * centre, ~1 km over the ground on a cell set, and the sky's air (cell_volsky, optical depth ~3 up that column) laid a
 * uniform veil over the clouds. Cached per document, shape count and scene sphere. Red spheretop: the union's top. */
static float vaShapesTop( Scene * scene, const BoundSphere & bs )
{
	static const void * doc = nullptr;
	static qsizetype count = -1;
	static float key[4] = {}, top = 0.0f;
	const float k[4] = { bs.center[0], bs.center[1], bs.center[2], bs.radius };
	if ( doc == scene->nifModel && count == scene->shapes.size() && std::equal( k, k + 4, key ) )
		return top;
	doc = scene->nifModel;
	count = scene->shapes.size();
	std::copy( k, k + 4, key );
	float t = -std::numeric_limits<float>::infinity();
	int used = 0;
	for ( Shape * sh : scene->shapes ) {
		if ( !sh || !sh->isVisible() )
			continue;
		bool far = false;
		for ( const Node * n = sh; n && !far; n = n->parentNode() )
			far = n->getName().startsWith( QLatin1String( "FarLOD" ) );
		if ( far )
			continue;
		const BoundSphere b = sh->bounds();
		if ( !( b.radius > 0.0f ) )
			continue;
		t = std::max( t, b.center[2] + b.radius );
		used++;
	}
	const float u = bs.center[2] + bs.radius;
	top = used ? std::min( t, u ) : u;	// never above the union's: it bounds every shape
	qInfo().noquote() << QStringLiteral( "cell air top: %1 (the highest of %2 shapes' spheres; the scene sphere's %3)" )
		.arg( double( top ), 0, 'f', 1 ).arg( used ).arg( double( u ), 0, 'f', 1 );
	return top;
}

static void vaPassInner( Scene * scene, bool run )
{
	if ( !scene || !scene->renderer )
		return;
	VaState & s = vs();
	Renderer * r = scene->renderer;
	VaGpu & g = vaGpus()[r];
	g.ready = false;
	if ( !s.on ) {
		s.last = QStringLiteral( "off" );
		return;
	}
	if ( !run )
		return;
	if ( r->globalUniforms->projectionMatrix[3][3] == 1.0f ) {
		s.last = QStringLiteral( "refused(orthographic)" );
		return;
	}
	auto fn = r->fn;
	if ( g.unitsOk < 0 ) {
		GLint units = 0;
		fn->glGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS, &units );
		g.unitsOk = units > kInjectUnit ? 1 : 0;
		g.optUnitsOk = units > kTileUnit;
	}
	if ( !g.unitsOk ) {
		s.last = QStringLiteral( "refused(texture units)" );
		return;
	}
	QElapsedTimer timer;
	// lane FOGPERF1: its glFinish pair serializes the frame into the air zone's GPU ms; =0 turns it off in a row
	const bool timed = !s.dump.isEmpty()
		|| ( wwEnvIsSet( "WW_CELL_AIR_TIME" ) && wwEnvStr( "WW_CELL_AIR_TIME" ) != QLatin1String( "0" ) );
	if ( timed ) {
		fn->glFinish();
		timer.start();
	}
	PassTime::subBegin( PassTime::AirPre );

	GLint prevFbo = 0, prevRead = 0, vp[4] = { 0, 0, 1, 1 }, prevActive = 0;
	GLboolean depthMask = GL_TRUE, colorMask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevFbo );
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	fn->glGetBooleanv( GL_COLOR_WRITEMASK, colorMask );
	const bool wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasCull = fn->glIsEnabled( GL_CULL_FACE );
	const bool wasBlend = fn->glIsEnabled( GL_BLEND ), wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );
	const bool wasStencil = fn->glIsEnabled( GL_STENCIL_TEST );
	auto restore = [&]() {
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevFbo ) );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
		fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
		fn->glColorMask( colorMask[0], colorMask[1], colorMask[2], colorMask[3] );
		fn->glDepthMask( depthMask );
		if ( wasDepth ) fn->glEnable( GL_DEPTH_TEST ); else fn->glDisable( GL_DEPTH_TEST );
		if ( wasCull ) fn->glEnable( GL_CULL_FACE ); else fn->glDisable( GL_CULL_FACE );
		if ( wasBlend ) fn->glEnable( GL_BLEND ); else fn->glDisable( GL_BLEND );
		if ( wasScissor ) fn->glEnable( GL_SCISSOR_TEST ); else fn->glDisable( GL_SCISSOR_TEST );
		if ( wasStencil ) fn->glEnable( GL_STENCIL_TEST ); else fn->glDisable( GL_STENCIL_TEST );
		fn->glActiveTexture( GLenum( prevActive ) );
	};

	NifSkopeOpenGLContext::Program * prog = r->useProgram( "cell_volair.prog" );
	if ( !prog ) {
		s.last = QStringLiteral( "refused(cell_volair.prog did not link)" );
		restore();
		return;
	}
	// the setters the surfaces get, on this program: the fog records (weather or, indoors, the cell's), the cascades,
	// the cell's lights, its GI and its ambient
	s.inPass = true;
	wwLookdevFogUniforms( scene );
	wwSunShadowUniforms( scene );
	wwCellLightsUniforms( scene );
	s.inPass = false;
	auto uniI = [&]( const char * n ) { GLint v = 0; const int l = prog->uniLocation( n ); if ( l >= 0 ) fn->glGetUniformiv( prog->id, l, &v ); return v; };
	auto uniF = [&]( const char * n, int i, float * o, int cnt ) {
		std::fill( o, o + cnt, 0.0f );
		const int l = i < 0 ? prog->uniLocation( n ) : prog->uniLocation( n, i );
		if ( l >= 0 )
			fn->glGetUniformfv( prog->id, l, o );
	};
	if ( !uniI( "fogOn" ) ) {
		r->stopProgram();
		s.last = QStringLiteral( "refused(no fog records: the Fog row, Lookdev's weather or the interior's fog)" );
		restore();
		return;
	}
	float K[6][4], fogView[4], fogSun[4], fogSunCol[4], distScale = 1.0f;
	for ( int k = 0; k < 6; k++ )
		uniF( "fogK[%d]", k, K[k], 4 );
	uniF( "fogView", -1, fogView, 4 );
	uniF( "fogSun", -1, fogSun, 4 );
	uniF( "fogSunColour", -1, fogSunCol, 4 );
	uniF( "fogDistScale", -1, &distScale, 1 );
	if ( !( K[0][0] > 0.0f ) ) {
		r->stopProgram();
		s.last = QStringLiteral( "refused(fog scale %1)" ).arg( double( K[0][0] ) );
		restore();
		return;
	}
	vaCpu.mark( PassTime::CpuAirState );
	VaMedium M = vaFit( K, ( s.red & 32 ) != 0, ( s.red & 128 ) != 0 );
	/* lane LIGHTBOOTH1 (the light test booth; a harness pin, never a user toggle): WW_CELL_BOOTH_AIR = <sigma0 per
	 * unit> replaces the fitted medium with EVEN air of that density at every height (no band, no ground layer), so a
	 * fixture's glow is measured in the same air wherever it stands. The slices end where the air reaches 99%
	 * opacity (4.6 / sigma0, held between 2000 and 20000 units), or at <far> when given as "<sigma0>,<far>": a short
	 * far end makes the 64 slices thin near the fixture (the booth's measuring runs). Unset: the fitted medium. */
	const QStringList boothArg = wwEnvStr( "WW_CELL_BOOTH_AIR" ).split( QLatin1Char( ',' ) );
	const float boothAir = boothArg.value( 0 ).toFloat();
	if ( boothAir > 0.0f ) {
		M.sigma0 = boothAir;
		M.invH = 0.0f;
		M.band = false;
		const float boothFar = boothArg.value( 1 ).toFloat();
		M.farD = boothFar > 0.0f ? std::min( std::max( boothFar, 256.0f ), 20000.0f )
								 : std::min( std::max( 4.6f / boothAir, 2000.0f ), 20000.0f );
		static bool said = false;
		if ( !said ) {
			said = true;
			std::fprintf( stderr, "cell air booth: even air sigma0 %g, slices to %g\n", double( boothAir ), double( M.farD ) );
		}
	}
	const WwCellLighting * L = wwCellLightsWanted( scene ) ? wwCellLightsFor( scene->nifModel ) : nullptr;
	/* lane FARGRID1: with the far grid on (outdoors, and only where the air reaches past the handoff), the near grid
	 * ends at the handoff and the far grid runs from there to min(the preset's end, the medium's farD); the tail as
	 * before past it. Red farseam: the far grid starts 30 % past the near grid's end (a gap of air: the seam gate's red). */
	const VaFarPreset & fp = kFarPresets[std::clamp( s.farTier, 0, 4 )];
	const float farEnd = std::min( fp.endKm * kUnitsPerKm, M.farD );
	const bool farOn = s.farTier > 0 && !( L && L->interior ) && farEnd > kFarHandoff * 2.0f;
	const float farStart = kFarHandoff * ( ( s.red & 33554432 ) ? 1.3f : 1.0f );
	// the slices: exponential from 16 units to where the air reaches the records' max opacity (vaFit)
	const float farD = farOn ? kFarHandoff : std::max( M.farD, kNearMin * 4.0f );
	const float nearD = kNearMin;
	const float logRatio = std::log( farD / nearD );
	// the medium's top: the scene's highest point, in the fog's height frame (world z - the frame's ground)
	const Transform & vt = scene->view;
	const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
	float camZw = 0.0f;
	for ( int j = 0; j < 3; j++ )
		camZw -= vt.rotation( j, 2 ) * vt.translation[j] / sc;
	const BoundSphere bs = scene->bounds();
	const float topW = s.sphereTop ? bs.center[2] + bs.radius : vaShapesTop( scene, bs );
	// the booth's even air has no top: a lone fixture would otherwise end the air at its own crown (lane LIGHTBOOTH1)
	const float zTop = boothAir > 0.0f ? 1.0e6f : topW + fogView[3] - camZw;

	// the phase: indoors the cell's god-ray record, outdoors the weather's, else the fallback
	float med[12] = { 0.60f, 1.52f, 3.31f, 2, 2, 2, 1, 1, 1, 0.75f, 0, 1 };
	QString medNote = QStringLiteral( "fallback" );
	if ( L && L->interior ) {
		std::copy( L->godRay, L->godRay + 12, med );
		medNote = L->godRayNote;
	} else if ( !wwLookdevGodRays( med, &medNote ) ) {
		medNote = QStringLiteral( "fallback (no weather)" );
	}
	const VaPhase ph = vaPhase( med, ( s.red & 524288 ) != 0 );
	// the ground layer (vaNearShare, vaGround); red noground: the far band alone, as before it
	VaLayer G;
	if ( boothAir > 0.0f ) {
		G.recx = QStringLiteral( "none(booth even air)" );
	} else if ( !( s.red & 16384 ) ) {
		vaNearShare( K, M, G );
		vaGround( K, M, L && L->interior, ( s.red & 32768 ) != 0, G );
		vaKeepStreet( M, G );	// FOGHEIGHT1: the street keeps the game's density, the air above thins
	} else {
		G.recx = QStringLiteral( "none(red noground)" );
	}
	{
		static QString saidRecx;
		const QString now = G.recx + ( G.conflict.isEmpty() ? QString() : QStringLiteral( " " ) + G.conflict );
		if ( now != saidRecx ) {
			saidRecx = now;
			std::fprintf( stderr, "cell air recx: %s\n", qPrintable( now ) );
		}
	}

	const int W = std::max( vp[2], 1 ), H = std::max( vp[3], 1 );
	const int cols = ( W + kColumnPx - 1 ) / kColumnPx, rows = ( H + kColumnPx - 1 ) / kColumnPx;
	// lane FARGRID1: one volume, the far grid's layers after the near grid's (no new sampler on the surfaces)
	const int texW = farOn ? std::max( cols, fp.cols ) : cols, texH = farOn ? std::max( rows, fp.rows ) : rows;
	const int texD = kSlices + ( farOn ? fp.slices : 0 );
	if ( g.cols != cols || g.rows != rows || g.slices != kSlices || !g.inject || g.texW != texW || g.texH != texH || g.texD != texD ) {
		vaAlloc3D( fn, g.inject, texW, texH, texD, false );
		vaAlloc3D( fn, g.integ, texW, texH, texD, true );
		if ( !g.fbo )
			fn->glGenFramebuffers( 1, &g.fbo );
		for ( GLuint & c : g.carry ) {
			if ( !c )
				fn->glGenTextures( 1, &c );
			fn->glBindTexture( GL_TEXTURE_2D, c );
			fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, cols, rows, 0, GL_RGBA, GL_FLOAT, nullptr );
			fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
			fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
			fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
		}
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
		g.cols = cols;
		g.rows = rows;
		g.slices = kSlices;
		g.texW = texW;
		g.texH = texH;
		g.texD = texD;
		g.farKey.clear();	// a new volume holds no far grid
	}
	if ( g.mrtOk < 0 ) {
		GLint bufs = 0, atts = 0;
		fn->glGetIntegerv( GL_MAX_DRAW_BUFFERS, &bufs );
		fn->glGetIntegerv( GL_MAX_COLOR_ATTACHMENTS, &atts );
		g.mrtOk = bufs >= 8 && atts >= 8 ? 1 : 0;
	}
	// lane PERF2, temporal: not with red lightmid (its single light sample has no strata)
	const bool temporal = s.temporal && g.mrtOk == 1 && !( s.red & 262144 );
	if ( temporal && ( !g.hist[0] || g.histCols != cols || g.histRows != rows ) ) {
		for ( GLuint & h : g.hist )
			vaAlloc3D( fn, h, cols, rows, 4 * kSlices, true );
		g.histCols = cols;
		g.histRows = rows;
		g.histOk = false;
	}
	if ( !temporal || g.doc != scene->nifModel )
		g.histOk = false;	// another document: its history is not this one's

	// red flat: one cube for every froxel, the GI grid's mean per axis
	float flat[6][3] = {};
	const WwCellGi * gi = L ? wwCellGiFor( scene->nifModel ) : nullptr;
	if ( gi && ( s.red & 4 ) ) {
		const size_t per = size_t( gi->dims[0] ) * size_t( gi->dims[1] ) * size_t( gi->dims[2] );
		for ( int a = 0; a < 6; a++ ) {
			double sum[3] = { 0, 0, 0 };
			size_t n = 0;
			for ( size_t v = 0; v < per && ( size_t( a ) * per + v ) * 4 + 3 < gi->rgba.size(); v++ ) {
				const float * p = &gi->rgba[( size_t( a ) * per + v ) * 4];
				if ( p[3] > 0.01f ) {
					for ( int c = 0; c < 3; c++ )
						sum[c] += double( p[c] / p[3] );
					n++;
				}
			}
			for ( int c = 0; c < 3 && n; c++ )
				flat[a][c] = float( sum[c] / double( n ) );
		}
	}

	const auto & pm = r->globalUniforms->projectionMatrix;
	const float p00 = pm[0][0], p11 = pm[1][1];
	const bool csm = wwSunShadowWanted( scene );
	fn->glDisable( GL_DEPTH_TEST );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glDepthMask( GL_FALSE );
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	fn->glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kVolTexUnit ) );
	fn->glBindTexture( GL_TEXTURE_3D, 0 );
	prog->uni1i( "volInject", kInjectUnit );
	prog->uni1i( "volTex", kVolTexUnit );
	prog->uni1b( "volOn", false );
	fn->glUniform3i( prog->uniLocation( "volDims" ), cols, rows, kSlices );
	prog->uni2f( "volProj", p00, p11 );
	prog->uni4f( "volSlices", FloatVector4( nearD, logRatio, float( kSlices ), 1.0f ) );
	prog->uni4f( "airMed", FloatVector4( M.sigma0, M.zb, M.invH, zTop ) );
	prog->uni4f( "airLayer", FloatVector4( G.w, G.zn, G.invHn, G.s ) );
	prog->uni4f( "airGround", FloatVector4( G.zg, G.invHg, 0.0f, 0.0f ) );
	{
		/* lane FOGTERRAIN1: the ground layer stands on the low-pass terrain (cell_airmed.glsl airGroundAt): its anchor
		 * rises by the local ground over the fog frame's ground. Exteriors with a layer and a published grid only;
		 * red WW_FOG_TERRAIN_RED=absolute: the absolute anchor */
		const WwFogTerrGrid & tg = wwFogTerrainGrid();
		float gz = 0.0f, gxy[2] = { 0.0f, 0.0f }, gh = 0.0f;
		const bool exterior = !( L && L->interior );
		const bool want = wwFogTerrainWanted();
		if ( want && exterior )
			wwLookdevGroundFrame( scene, gz, gxy, gh );
		if ( want && exterior && G.s > 0.0f )
			wwFogTerrainRequest();	// lane FOGTERRAIN2: the grid's own job runs while asked (shadowset.cpp fogTerrainFrame)
		if ( g.terr && tg.nx == 0 && g.terrGen != tg.gen ) {	// the job retired or the set emptied: free the 4 MB
			fn->glDeleteTextures( 1, &g.terr );
			g.terr = 0;
			g.terrGen = tg.gen;
		}
		const bool flat = tg.gen != 0 && wwFogTerrainFlatAt( tg, gz );
		const bool on = want && exterior && G.s > 0.0f && tg.gen != 0 && tg.nx > 1 && tg.ny > 1 && !flat;
		g.terrOn = on;
		if ( on ) {
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::AirTerrain ) );
			if ( !g.terr )
				fn->glGenTextures( 1, &g.terr );
			// lane FOGTERRAIN3: a 2-layer array, layer 0 the coarse level, layer 1 the fine one (a copy of layer 0 and
			// rows to uv -1 when there is none); 8 MB
			const bool fine = tg.fnx == tg.nx && tg.F.size() == tg.H.size();
			fn->glBindTexture( GL_TEXTURE_2D_ARRAY, g.terr );
			if ( g.terrGen != tg.gen ) {
				fn->glTexImage3D( GL_TEXTURE_2D_ARRAY, 0, GL_R32F, tg.nx, tg.ny, 2, 0, GL_RED, GL_FLOAT, nullptr );
				fn->glTexSubImage3D( GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, tg.nx, tg.ny, 1, GL_RED, GL_FLOAT, tg.H.data() );
				fn->glTexSubImage3D( GL_TEXTURE_2D_ARRAY, 0, 0, 0, 1, tg.nx, tg.ny, 1, GL_RED, GL_FLOAT,
					fine ? tg.F.data() : tg.H.data() );
				fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
				fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
				fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
				fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
				fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0 );
				g.terrGen = tg.gen;
			}
			prog->uni1i( "airTerr", WwCellUnit::AirTerrain );
			wwFogTerrainXf( tg, false, g.terrXf );
			wwFogTerrainXf( tg, fine, g.terrFineXf );
			if ( !fine )
				wwFogTerrainXf( WwFogTerrGrid(), false, g.terrFineXf );
			g.terrRef = gz;
			prog->uni4f( "airTerrXf", FloatVector4( g.terrXf[0], g.terrXf[1], g.terrXf[2], g.terrXf[3] ) );
			prog->uni4f( "airTerrFineXf", FloatVector4( g.terrFineXf[0], g.terrFineXf[1], g.terrFineXf[2], g.terrFineXf[3] ) );
			prog->uni1f( "airTerrRef", gz );
		}
		prog->uni1i( "airTerrOn", on ? 1 : 0 );
		g.terrNote = on ? QStringLiteral( "on(%1x%2 cell=%3 sigma=%4 ref=%5 job shapes=%6 ms=%7)" ).arg( tg.nx ).arg( tg.ny )
				.arg( tg.cell, 0, 'f', 0 ).arg( kFogTerrSigma, 0, 'f', 0 ).arg( double( gz ), 0, 'f', 1 ).arg( tg.shapes )
				.arg( tg.ms, 0, 'f', 0 )
				// lane FOGTERRAIN3: the fine level's texels (0: none) and the water shapes / raster texels it raised
				+ QStringLiteral( " fine=%1 water=%2 wet=%3" ).arg( tg.fnx == tg.nx ? tg.fnx : 0 ).arg( tg.water ).arg( tg.wet )
			: !want ? QStringLiteral( "off(red absolute)" ) : !exterior ? QStringLiteral( "off(interior)" )
			: !( G.s > 0.0f ) ? QStringLiteral( "off(no ground layer)" ) : flat ? QStringLiteral( "off(flat at the frame's ground)" )
			: QStringLiteral( "off(no terrain heights yet)" );
	}
	prog->uni1b( "volCsm", csm );
	prog->uni1i( "volRed", s.red & ~1 );
	prog->uni1f( "volShell", s.shellStep ? 0.0f : 1.0f );
	if ( s.shellStep || s.noAirVol )
		prog->uni1b( "cellGiAirOn", false );	// wwCellLightsUniforms turned it on
	prog->uni1i( "volTerms", s.terms );
	prog->uni1i( "volFarMode", farOn ? 1 : 0 );	// lane FARGRID1: the near pass
	prog->uni1i( "volLayerBase", 0 );
	prog->uni1f( "volJitter", 0.0f );

	/* lane PERF2, temporal: this view (rows sc R | t) and the map from it to the last pass's view,
	 * last = sc' R' R^T (y - t) / sc + t'. Mode 2 when nothing moved (the history read at the same froxel) */
	double curV[3][4];
	for ( int i = 0; i < 3; i++ ) {
		for ( int j = 0; j < 3; j++ )
			curV[i][j] = double( vt.rotation( i, j ) ) * double( sc );
		curV[i][3] = double( vt.translation[i] );
	}
	const float curGrid[4] = { p00, p11, nearD, logRatio };
	g.tmode = 0;
	if ( temporal ) {
		bool same = g.histOk;
		for ( int i = 0; i < 3 && same; i++ )
			for ( int j = 0; j < 4 && same; j++ )
				same = curV[i][j] == g.prevV[i][j];
		for ( int k = 0; k < 4 && same; k++ )
			same = curGrid[k] == g.prevGrid[k];
		same = same && distScale == g.prevScale;
		g.tmode = same ? 2 : 1;
		float rep[3][4];
		const double s0 = double( sc );
		for ( int i = 0; i < 3; i++ ) {
			double off = g.prevV[i][3];
			for ( int j = 0; j < 3; j++ ) {
				// (sc' R' R^T / sc)[i][j] = sum_k prevV[i][k] curV[j][k] / sc^2
				double m = 0.0;
				for ( int k = 0; k < 3; k++ )
					m += g.prevV[i][k] * curV[j][k];
				m /= s0 * s0;
				rep[i][j] = float( m );
				off -= m * curV[j][3];
			}
			rep[i][3] = float( off );
		}
		for ( int i = 0; i < 3; i++ )
			prog->uni4f_l( prog->uniLocation( "volReproj[%d]", i ), FloatVector4( rep[i][0], rep[i][1], rep[i][2], rep[i][3] ) );
		prog->uni4f( "volPrevGrid", FloatVector4( g.prevGrid[0], g.prevGrid[1], g.prevGrid[2] > 0.0f ? g.prevGrid[2] : 1.0f,
			g.prevGrid[3] > 0.0f ? g.prevGrid[3] : 1.0f ) );
		prog->uni1f( "volPrevScale", g.prevScale );
	}
	prog->uni1i( "volTemporal", g.tmode );
	prog->uni1i( "volQ", int( g.frame & 3u ) );
	prog->uni1b( "volHistOk", temporal && g.histOk );
	prog->uni1i( "volHist", kInjectUnit );
	{
		const int pf = wwCellPostPathFrame();
		const bool shifted = s.shiftFrame >= 0 && pf >= s.shiftFrame;
		prog->uni3f( "volLightShift", shifted ? s.shift[0] : 0.0f, shifted ? s.shift[1] : 0.0f, shifted ? s.shift[2] : 0.0f );
	}
	/* lane FOGDRIFT1: the near grid's drifting lumps. Exteriors drift; an interior only when it shows the sky (CELL DATA
	 * bit 7), a sealed one stays smooth (amplitude 0: the same froxels as the row off). No wind: lumps, standing still. */
	VaDrift & dr = vd();
	float driftAmp = 0.0f;
	quint8 wind[4] = { 0, 0, 0, 0 };
	const bool hasWind = wwLookdevWind( wind );
	{
		static GLint combined = -1;
		if ( combined < 0 )
			fn->glGetIntegerv( GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &combined );
		const bool sealed = L && L->interior && !L->showSky;
		dr.moving = false;
		if ( !dr.on ) {
			dr.note = QStringLiteral( "off" );
		} else if ( dr.red == 2 ) {
			dr.note = QStringLiteral( "smooth(red)" );
		} else if ( !uniI( "cellOn" ) || combined <= kNoiseUnit ) {
			dr.note = QStringLiteral( "none(no cell lights or texture units)" );
		} else if ( sealed ) {
			dr.note = QStringLiteral( "still(sealed interior)" );
		} else {
			if ( dr.tex.empty() )
				vaNoiseBuild( dr );
			if ( !g.noise ) {
				fn->glGenTextures( 1, &g.noise );
				fn->glActiveTexture( GLenum( GL_TEXTURE0 + kNoiseUnit ) );
				fn->glBindTexture( GL_TEXTURE_3D, g.noise );
				fn->glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
				fn->glTexImage3D( GL_TEXTURE_3D, 0, GL_R8, kNoiseN, kNoiseN, kNoiseN, 0, GL_RED, GL_UNSIGNED_BYTE, dr.tex.data() );
				fn->glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
				fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
				fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
				for ( GLenum w : { GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T, GL_TEXTURE_WRAP_R } )
					fn->glTexParameteri( GL_TEXTURE_3D, w, GL_REPEAT );
				fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 0 );
			}
			const bool blows = hasWind && wind[0] > 0 && dr.red != 1;
			if ( blows ) {
				if ( !dr.clock.isValid() )
					dr.clock.start();
				double t = dr.timePinned ? dr.tPin : double( dr.clock.nsecsElapsed() ) * 1e-9;
				// lane FOGVID1: WW_CELL_FOGDRIFT_PATH=1 runs the pinned clock along a WW_RENDER_PATH (frame k at T + k / fps)
				if ( dr.timePinned && wwEnvInt( "WW_CELL_FOGDRIFT_PATH" ) > 0 && wwCellPostPathFps() > 0.0f )
					t += double( wwCellPostPathFrame() ) / double( wwCellPostPathFps() );
				vaDriftAdvance( dr, t, wind );
			}
			dr.moving = blows;
			driftAmp = kDriftAmp / std::max( dr.span, 1e-3f );
			dr.note = QStringLiteral( "on(law=%8 wind=%1 dir=%2 range=%3 turb=%4 speed=%5 t=%6%7)" )
				.arg( hasWind ? wind[0] / 255.0 : 0.0, 0, 'f', 3 ).arg( wind[1] / 255.0 * 360.0, 0, 'f', 1 )
				.arg( wind[2] / 255.0 * 180.0, 0, 'f', 1 ).arg( wind[3] / 255.0, 0, 'f', 3 )
				.arg( double( std::hypot( dr.vel[0], dr.vel[1] ) ), 0, 'f', 1 ).arg( dr.tDone, 0, 'f', 3 )
				.arg( !blows ? ( dr.red == 1 ? QStringLiteral( " still(red)" ) : QStringLiteral( " still(no wind)" ) ) : QString() )
				.arg( dr.red == 3 ? QStringLiteral( "wobble(red)" ) : QStringLiteral( "game" ) );
		}
	}
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kNoiseUnit ) );
	fn->glBindTexture( GL_TEXTURE_3D, driftAmp > 0.0f ? g.noise : 0 );
	prog->uni1i( "volNoise", kNoiseUnit );
	prog->uni4f( "volDrift", FloatVector4( driftAmp, kDriftFade0, 1.0f / ( kDriftFade1 - kDriftFade0 ), dr.mean ) );
	for ( int o = 0; o < 2; o++ )
		prog->uni4f_l( prog->uniLocation( "volDriftOfs[%d]", o ), FloatVector4( dr.ofs[o][0], dr.ofs[o][1], dr.ofs[o][2],
			1.0f / ( kDriftTile * ( o ? kDriftDetail : 1.0f ) ) ) );

	/* THE OPTIMISATION (coordinator 10-14; WW_CELL_AIR_OPT, default both): 1 the clustered lights (vaBuildClusters),
	 * 2 the conservative depth skip: stage 0 writes, per froxel column, the farthest and the nearest ray distance of
	 * lane AO1's opaque depth over the column's pixels (one pixel of margin; empty pixels: unbounded); stage 1 skips a
	 * froxel whose PREVIOUS slice already starts past the farthest distance of the column and its 8 neighbours (the
	 * apply's bilinear and its two slices never read it) and writes (0, 1) there, so the integrate stays continuous.
	 * No opaque pass this frame: no skip. The units are always distinct (a sampler2D and an isamplerBuffer on one unit
	 * is a draw-time error). */
	vaCpu.mark( PassTime::CpuAirSetup );
	prog->uni1i( "volClusters", kClusterUnit );
	prog->uni1i( "volGbuf", kGbufUnit );
	prog->uni1i( "volTile", kTileUnit );
	VaClusters cl;
	bool clOn = false;
	double clMs = 0.0;
	// lane GAPLOOK1 (LIGHTSYS-4): the air walks the surfaces' lists (bound by wwCellLightsUniforms above), no build of its own
	// lane FOGPERF1: on with the fog; where the surfaces bound no lists (their clustering off) the air builds its own
	const bool surfCl = wwLookOn( WwLook::AirClusters ) && uniI( "cellClusterOn" );
	prog->uni1b( "volSurfClusters", surfCl );
	prog->uni2f( "volSurfPx", float( vp[2] ) / float( std::max( cols, 1 ) ), float( vp[3] ) / float( std::max( rows, 1 ) ) );
	prog->uni1b( "volSurfRed", surfCl && wwEnvStr( "WW_CELL_AIR_CLUSTERS_RED" ) == QLatin1String( "drop" ) );
	// lane GAPLOOK1 (FOGEXTRAS): the sun's scattering octaves a = b = c = 1/2 (Oz 2013's); red hard: shadows not lifted
	prog->uni4f( "volMs", wwLookOn( WwLook::AirMultiScatter ) ? FloatVector4( 0.5f, 0.5f, 0.5f,
		wwEnvStr( "WW_CELL_AIR_MS_RED" ) == QLatin1String( "hard" ) ? 0.0f : 1.0f ) : FloatVector4( 0.0f, 0.0f, 0.0f, 0.0f ) );
	if ( !surfCl && ( s.opt & 1 ) && g.optUnitsOk && ( s.terms & 2 ) && !( s.red & 16 ) && uniI( "cellOn" ) ) {
		const std::vector<float> * sp = wwCellLightSpheres( scene );
		const int nL = uniI( "cellLightCount" );
		float rowV[3][4];
		for ( int k = 0; k < 3; k++ )
			uniF( "cellRow[%d]", k, rowV[k], 4 );
		QElapsedTimer ct;
		ct.start();
		if ( sp && nL > 0 && int( sp->size() / 4 ) == nL
			&& vaBuildClusters( cl, *sp, rowV, distScale, cols, rows, nearD, logRatio, p00, p11, ( s.red & 4096 ) != 0 ) ) {
			if ( !g.clBuf ) {
				fn->glGenBuffers( 1, &g.clBuf );
				fn->glGenTextures( 1, &g.clTex );
			}
			fn->glBindBuffer( GL_TEXTURE_BUFFER, g.clBuf );
			fn->glBufferData( GL_TEXTURE_BUFFER, GLsizeiptr( cl.buf.size() * sizeof( GLint ) ), cl.buf.data(), GL_STREAM_DRAW );
			fn->glBindBuffer( GL_TEXTURE_BUFFER, 0 );
			clOn = true;
		}
		clMs = double( ct.nsecsElapsed() ) / 1.0e6;
	}
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kClusterUnit ) );
	fn->glBindTexture( GL_TEXTURE_BUFFER, clOn ? g.clTex : 0 );
	if ( clOn )
		fn->glTexBuffer( GL_TEXTURE_BUFFER, GL_R32I, g.clBuf );
	fn->glUniform4i( prog->uniLocation( "volClusterDims" ), clOn ? cl.ncx : 0, cl.ncy, kClusterCols, kClusterSlices );
	vaCpu.mark( PassTime::CpuAirClusters );
	WwCellAoTargets ao;
	/* 10-16: stage 0 runs whenever the obscurance pass is there, the skip or not: the surfaces clamp their lookup to the
	 * column's farthest opaque distance (lookdev_fog.glsl wwVolAirAt), so every mode reads the same froxels. */
	const bool tileOn = ( ( s.opt & 2 ) || !( s.red & 2097152 ) ) && g.optUnitsOk && wwCellAoTargets( scene, ao ) && ao.gbuf
		&& ao.w > 0 && ao.h > 0;
	const bool depthOn = tileOn && ( s.opt & 2 );
	g.tileValid = false;
	if ( tileOn && ( g.tileCols != cols || g.tileRows != rows || !g.tile ) ) {
		if ( !g.tile )
			fn->glGenTextures( 1, &g.tile );
		fn->glBindTexture( GL_TEXTURE_2D, g.tile );
		fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_RG32F, cols, rows, 0, GL_RG, GL_FLOAT, nullptr );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
		g.tileCols = cols;
		g.tileRows = rows;
	}
	/* 10-16: stage 0 bounds the columns by the obscurance pass's DEPTH buffer, not its colour. Every depth-tested
	 * fragment of the frame lies at or before that depth (the pass writes depth for the opaque cell-lit draws only, a
	 * subset of the frame's depth writers), while its colour also takes draws that write no depth: a lamp's edge, a
	 * trim drawn over the wall behind it. The wall's fragment, past the colour's distance, read froxels the skip had
	 * left empty (the Z gate's 3 px). Red zcolour: the colour's alpha, as before. */
	const bool zDepth = tileOn && !( s.red & 1048576 ) && ao.depthRb;
	if ( zDepth ) {
		if ( !g.zTex || g.zW != ao.w || g.zH != ao.h ) {
			if ( !g.zTex )
				fn->glGenTextures( 1, &g.zTex );
			fn->glBindTexture( GL_TEXTURE_2D, g.zTex );
			// the obscurance pass's own format (celllights.cpp): a depth blit needs the same on both sides
			fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, ao.w, ao.h, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr );
			fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
			fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
			fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE );
			fn->glTexParameteri( GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT );
			fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
			if ( !g.zFbo )
				fn->glGenFramebuffers( 1, &g.zFbo );
			fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.zFbo );
			fn->glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, g.zTex, 0 );
			g.zW = ao.w;
			g.zH = ao.h;
		}
		if ( !g.zSrc )
			fn->glGenFramebuffers( 1, &g.zSrc );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.zSrc );
		fn->glFramebufferRenderbuffer( GL_READ_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, GLuint( ao.depthRb ) );
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.zFbo );
		fn->glBlitFramebuffer( 0, 0, ao.w, ao.h, 0, 0, ao.w, ao.h, GL_DEPTH_BUFFER_BIT, GL_NEAREST );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, 0 );
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 );
	}
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kGbufUnit ) );
	fn->glBindTexture( GL_TEXTURE_2D, zDepth ? g.zTex : tileOn ? GLuint( ao.gbuf ) : 0 );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kTileUnit ) );
	fn->glBindTexture( GL_TEXTURE_2D, tileOn ? g.tile : 0 );
	prog->uni1i( "volGbufZ", zDepth ? 1 : 0 );
	prog->uni2f( "volProjZ", pm[2][2], pm[3][2] );
	prog->uni1i( "volDepthSkip", depthOn ? ( ( s.red & 8192 ) ? 2 : 1 ) : 0 );
	if ( tileOn )
		fn->glUniform2i( prog->uniLocation( "volGbufSize" ), ao.w, ao.h );
	for ( int k = 0; k < 3; k++ )
		prog->uni4f_l( prog->uniLocation( "volPhase[%d]", k ), FloatVector4( ph.w[k][0], ph.w[k][1], ph.w[k][2], ph.w[k][3] ) );
	prog->uni4f( "volGi", FloatVector4( ph.gi[0], ph.gi[1], ph.gi[2], ph.gi[3] ) );
	for ( int a = 0; a < 6; a++ )
		prog->uni3f_l( prog->uniLocation( "volFlat[%d]", a ), flat[a][0], flat[a][1], flat[a][2] );

	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = quad;
	fn->glBindFramebuffer( GL_FRAMEBUFFER, g.fbo );
	fn->glViewport( 0, 0, cols, rows );
	bool complete = true;
	PassTime::subEnd( PassTime::AirPre );
	if ( tileOn ) {
		// stage 0: the columns' opaque distances (the tile off its unit while it is the target)
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kTileUnit ) );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.tile, 0 );
		if ( fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
			complete = false;
		} else {
			prog->uni1i( "volStage", 0 );
			prog->uni1i( "volLayer", 0 );
			PassTime::subBegin( PassTime::AirTile );
			r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
			PassTime::subEnd( PassTime::AirTile );
			g.tileValid = true;
		}
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0 );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kTileUnit ) );
		fn->glBindTexture( GL_TEXTURE_2D, g.tile );
	}
	vaCpu.mark( PassTime::CpuAirTile );
	// lane PERF2: the O(S) integrate (stage 3, 7 layers a draw); red oldint: stage 2, every layer re-summed from 0
	const bool oneSum = g.mrtOk == 1 && !( s.red & 268435456 );
	for ( int stage = 1; stage <= 2 && complete; stage++ ) {
		prog->uni1i( "volStage", stage == 2 && oneSum ? 3 : stage );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kInjectUnit ) );
		fn->glBindTexture( GL_TEXTURE_3D, stage == 2 ? g.inject : temporal ? g.hist[g.histCur] : 0 );
		PassTime::subBegin( stage == 1 ? PassTime::AirInject : PassTime::AirIntegrate );
		if ( stage == 1 && temporal ) {
			// lane PERF2, temporal: the inject at attachment 0, the four strata of this layer into the other history
			const GLuint out = g.hist[g.histCur ^ 1];
			const GLenum bufs[5] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4 };
			fn->glDrawBuffers( 5, bufs );
			for ( int k = 0; k < kSlices; k++ ) {
				fn->glFramebufferTextureLayer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, g.inject, 0, k );
				for ( int p = 0; p < 4; p++ )
					fn->glFramebufferTextureLayer( GL_FRAMEBUFFER, GLenum( GL_COLOR_ATTACHMENT1 + p ), out, 0, p * kSlices + k );
				if ( k == 0 && fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
					complete = false;
					break;
				}
				prog->uni1i( "volLayer", k );
				r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
			}
			for ( int p = 1; p <= 4; p++ )
				fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GLenum( GL_COLOR_ATTACHMENT0 + p ), GL_TEXTURE_2D, 0, 0 );
			const GLenum first = GL_COLOR_ATTACHMENT0;
			fn->glDrawBuffers( 1, &first );
		} else if ( stage == 2 && oneSum ) {
			prog->uni1i( "volCarry", kGbufUnit );
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + kGbufUnit ) );
			for ( int k = 0, c = 0; k < kSlices; k += 7, c++ ) {
				const int n = std::min( 7, kSlices - k );
				GLenum bufs[8];
				for ( int i = 0; i < 7; i++ ) {
					fn->glFramebufferTextureLayer( GL_FRAMEBUFFER, GLenum( GL_COLOR_ATTACHMENT0 + i ), i < n ? g.integ : 0, 0, i < n ? k + i : 0 );
					bufs[i] = i < n ? GLenum( GL_COLOR_ATTACHMENT0 + i ) : GLenum( GL_NONE );
				}
				fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT7, GL_TEXTURE_2D, g.carry[( c + 1 ) & 1], 0 );
				bufs[7] = GL_COLOR_ATTACHMENT7;
				fn->glDrawBuffers( 8, bufs );
				fn->glBindTexture( GL_TEXTURE_2D, g.carry[c & 1] );
				if ( k == 0 && fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
					complete = false;
					break;
				}
				prog->uni1i( "volLayer", k );
				prog->uni1i( "volCount", n );
				r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
			}
			fn->glBindTexture( GL_TEXTURE_2D, 0 );
			for ( int i = 1; i < 8; i++ )
				fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GLenum( GL_COLOR_ATTACHMENT0 + i ), GL_TEXTURE_2D, 0, 0 );
			const GLenum first = GL_COLOR_ATTACHMENT0;
			fn->glDrawBuffers( 1, &first );
		} else {
			for ( int k = 0; k < kSlices; k++ ) {
				fn->glFramebufferTextureLayer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, stage == 1 ? g.inject : g.integ, 0, k );
				if ( k == 0 && fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
					complete = false;
					break;
				}
				prog->uni1i( "volLayer", k );
				r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
			}
		}
		PassTime::subEnd( stage == 1 ? PassTime::AirInject : PassTime::AirIntegrate );
		vaCpu.mark( stage == 1 ? PassTime::CpuAirInject : PassTime::CpuAirInteg );
	}
	/* lane FARGRID1: THE FAR GRID. The same program and medium on a fixed grid from the handoff to farEnd: the sun
	 * (shadowed by airFarVis: the cascades where they reach, the terrain's shadow-height map, the clouds' input) and
	 * the sky's ambient; no placed lights, no GI grid (neither reaches 250 m), no depth skip. Rebuilt when what it is
	 * built from moves, else every `refresh` frames (the terrain sweep lands in slices); a still view rebuilds the
	 * same numbers: no crawl. Red farjitter: a per-frame slice offset, rebuilt every frame (the stability gate's red). */
	g.farOn = false;
	if ( farOn && complete ) {
		PassTime::subBegin( PassTime::AirFar );
		if ( !g.farQ[0] )
			fn->glGenQueries( 2, g.farQ );
		if ( g.farQPending ) {
			GLuint avail = 0;
			fn->glGetQueryObjectuiv( g.farQ[1], GL_QUERY_RESULT_AVAILABLE, &avail );
			if ( avail || timed ) {
				GLuint64 t0 = 0, t1 = 0;
				fn->glGetQueryObjectui64v( g.farQ[0], GL_QUERY_RESULT, &t0 );
				fn->glGetQueryObjectui64v( g.farQ[1], GL_QUERY_RESULT, &t1 );
				g.farGpuMs = double( t1 - t0 ) / 1.0e6;
				g.farQPending = false;
			}
		}
		std::vector<float> key;
		for ( int i = 0; i < 3; i++ )
			for ( int j = 0; j < 3; j++ )
				key.push_back( vt.rotation( i, j ) );
		key.insert( key.end(), { vt.translation[0], vt.translation[1], vt.translation[2], sc, p00, p11, M.sigma0, M.zb,
			M.invH, zTop, G.w, G.zn, G.invHn, G.s, G.zg, G.invHg, farStart, farEnd, float( s.farTier ), float( s.terms ),
			float( s.red ), csm ? 1.0f : 0.0f } );
		for ( int k = 0; k < 6; k++ )
			key.insert( key.end(), K[k], K[k] + 4 );
		float sunV[4];
		for ( const char * n : { "cellDirTo", "cellDirColor", "fogSun", "fogSunColour" } ) {
			uniF( n, -1, sunV, 4 );
			key.insert( key.end(), sunV, sunV + 4 );
		}
		WwTerrainShadowMap tm;
		if ( wwTerrainShadowMap( tm ) )
			key.insert( key.end(), { float( tm.tex ), float( tm.elevDeg ), float( tm.azDeg ), float( tm.x0 ), float( tm.y0 ) } );
		const bool jitter = ( s.red & 67108864 ) != 0;
		const bool rebuild = jitter || key != g.farKey || ++g.farAge >= fp.refresh;
		bool farOk = true;
		if ( rebuild ) {
			const float farLog = std::log( farEnd / farStart );
			if ( !g.farQPending )
				fn->glQueryCounter( g.farQ[0], GL_TIMESTAMP );
			fn->glUniform3i( prog->uniLocation( "volDims" ), fp.cols, fp.rows, fp.slices );
			prog->uni4f( "volSlices", FloatVector4( farStart, farLog, float( fp.slices ), 1.0f ) );
			prog->uni1i( "volFarMode", 3 | ( ( s.red & 16777216 ) ? 4 : 0 ) );
			prog->uni1i( "volLayerBase", kSlices );
			prog->uni1i( "volTemporal", 0 );	// BATCH1: PERF2's history is the near grid's only
			prog->uni1b( "volHistOk", false );
			prog->uni1i( "volTerms", s.terms & 9 );
			prog->uni1i( "volDepthSkip", 0 );
			prog->uni4f( "volDrift", FloatVector4( 0.0f, kDriftFade0, 1.0f, dr.mean ) );	// lane FOGDRIFT1: the far air stays smooth
			fn->glUniform4i( prog->uniLocation( "volClusterDims" ), 0, 0, kClusterCols, kClusterSlices );
			static quint32 frame = 0;
			prog->uni1f( "volJitter", jitter ? float( ( ++frame * 2654435761u ) >> 8 ) / 16777216.0f - 0.5f : 0.0f );
			fn->glViewport( 0, 0, fp.cols, fp.rows );
			for ( int stage = 1; stage <= 2 && farOk; stage++ ) {
				prog->uni1i( "volStage", stage );
				fn->glActiveTexture( GLenum( GL_TEXTURE0 + kInjectUnit ) );
				fn->glBindTexture( GL_TEXTURE_3D, stage == 2 ? g.inject : 0 );
				for ( int k = 0; k < fp.slices; k++ ) {
					fn->glFramebufferTextureLayer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, stage == 1 ? g.inject : g.integ, 0, kSlices + k );
					if ( k == 0 && fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
						farOk = false;
						break;
					}
					prog->uni1i( "volLayer", k );
					r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
				}
			}
			if ( !g.farQPending ) {
				fn->glQueryCounter( g.farQ[1], GL_TIMESTAMP );
				g.farQPending = true;
			}
			if ( timed && g.farQPending ) {
				GLuint64 t0 = 0, t1 = 0;
				fn->glGetQueryObjectui64v( g.farQ[0], GL_QUERY_RESULT, &t0 );
				fn->glGetQueryObjectui64v( g.farQ[1], GL_QUERY_RESULT, &t1 );
				g.farGpuMs = double( t1 - t0 ) / 1.0e6;
				g.farQPending = false;
			}
			g.farKey = farOk ? key : std::vector<float>();
			g.farAge = 0;
			g.farBuilds++;
		}
		g.farOn = farOk;
		g.farCols = fp.cols;
		g.farRows = fp.rows;
		g.farSlices = fp.slices;
		g.farStart = farStart;
		g.farLog = std::log( farEnd / farStart );
		PassTime::subEnd( PassTime::AirFar );
		vaCpu.mark( PassTime::CpuAirFar );
	}
	PassTime::subBegin( PassTime::AirPost );
	// the numbers the dump echoes, read before the program stops
	float cellRowV[3][4], cellDirColV[3] = { 0, 0, 0 }, cellDirToV[3] = { 0, 0, 0 };
	for ( int k = 0; k < 3; k++ )
		uniF( "cellRow[%d]", k, cellRowV[k], 4 );
	uniF( "cellDirColor", -1, cellDirColV, 3 );
	uniF( "cellDirTo", -1, cellDirToV, 3 );
	const int cellOnV = uniI( "cellOn" ), cellHasDirV = uniI( "cellHasDir" ), cellLightCountV = uniI( "cellLightCount" );
	const int cellGiOnV = uniI( "cellGiOn" ), cellHasDalcV = uniI( "cellHasDalc" ), cellExtDalcV = uniI( "cellExtDalc" );
	const int cellGiAirOnV = uniI( "cellGiAirOn" );
	r->stopProgram();
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kInjectUnit ) );
	fn->glBindTexture( GL_TEXTURE_3D, 0 );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kNoiseUnit ) );
	fn->glBindTexture( GL_TEXTURE_3D, 0 );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kClusterUnit ) );
	fn->glBindTexture( GL_TEXTURE_BUFFER, 0 );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kGbufUnit ) );
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kTileUnit ) );
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::AirTerrain ) );
	fn->glBindTexture( GL_TEXTURE_2D_ARRAY, 0 );
	if ( !complete ) {
		s.last = QStringLiteral( "refused(froxel framebuffer incomplete)" );
		restore();
		return;
	}
	g.ready = true;
	g.doc = scene->nifModel;
	if ( temporal ) {
		g.histCur ^= 1;
		g.histOk = true;
		g.frame++;
		std::copy( &curV[0][0], &curV[0][0] + 12, &g.prevV[0][0] );
		std::copy( curGrid, curGrid + 4, g.prevGrid );
		g.prevScale = distScale;
		// an interactive window paints until every stratum is fresh (3 more frames after a paint of its own)
		if ( s.settlePending )
			s.settlePending = false;
		else
			s.settleLeft = 3;
	}
	g.near = nearD;
	g.logRatio = logRatio;
	g.med[0] = M.sigma0;
	g.med[1] = M.zb;
	g.med[2] = M.invH;
	g.med[3] = zTop;
	const float layerV[4] = { G.w, G.zn, G.invHn, G.s }, groundV[4] = { G.zg, G.invHg, 0.0f, 0.0f };
	std::copy( layerV, layerV + 4, g.layer );
	std::copy( groundV, groundV + 4, g.ground );
	std::copy( &ph.w[0][0], &ph.w[0][0] + 12, &g.phase[0][0] );	// lane VOLPART1
	std::copy( ph.gi, ph.gi + 4, g.gi );
	g.csm = csm;
	double ms = -1.0;
	if ( timed ) {
		fn->glFinish();
		ms = double( timer.nsecsElapsed() ) / 1.0e6;
	}
	s.last = QStringLiteral( "on(%1x%2x%3 near=%4 far=%5 sigma0=%6 zb=%7 H=%8 top=%9 phase=%10 tint=%11,%12,%13 lights=%14 "
		"gi=%15 csm=%16 terms=%17 red=%18 ms=%19)" )
		.arg( cols ).arg( rows ).arg( kSlices ).arg( double( nearD ), 0, 'f', 1 ).arg( double( farD ), 0, 'f', 1 )
		.arg( double( M.sigma0 ), 0, 'g', 6 ).arg( double( M.zb ), 0, 'f', 1 )
		.arg( M.invH > 0.0f ? QString::number( double( 1.0f / M.invH ), 'f', 1 ) : QStringLiteral( "uniform" ) )
		.arg( double( zTop ), 0, 'f', 1 ).arg( medNote )
		.arg( double( ph.gi[0] ), 0, 'f', 4 ).arg( double( ph.gi[1] ), 0, 'f', 4 ).arg( double( ph.gi[2] ), 0, 'f', 4 )
		.arg( cellLightCountV ).arg( cellGiOnV ).arg( csm ? 1 : 0 ).arg( s.terms ).arg( s.red ).arg( ms, 0, 'f', 3 );
	// the optimisation's echo: the mode, the clusters (all / non-empty / mean list over the non-empty), the CPU build,
	// the depth skip (0 off, 1 on, 2 the red)
	const QString optNote = QStringLiteral( "opt=%1 clusters=%2 nonempty=%3 meanlist=%4 buildms=%5 depthskip=%6 zsource=%7 farclamp=%8 temporal=%9" )
		.arg( s.opt ).arg( clOn ? cl.ncx * cl.ncy * cl.ncz : 0 ).arg( clOn ? cl.nonEmpty : 0 )
		.arg( clOn && cl.nonEmpty ? double( cl.refs ) / double( cl.nonEmpty ) : 0.0, 0, 'f', 2 ).arg( clMs, 0, 'f', 3 )
		.arg( depthOn ? ( ( s.red & 8192 ) ? 2 : 1 ) : 0 )
		.arg( !tileOn ? QStringLiteral( "none" ) : zDepth ? QStringLiteral( "depth" ) : QStringLiteral( "colour" ) )
		.arg( g.tileValid && !( s.red & 2097152 ) ? 1 : 0 )
		.arg( g.tmode );	// lane PERF2: 0 the 4 sub-steps, 1 temporal reprojected, 2 temporal, camera unchanged
	// the ground layer's echo: what was applied (the near share, the strength, thickness and anchor) and its source
	const QString groundNote = QStringLiteral( "nearw=%1 ground=%2 thick=%3 zg=%4 terrain=%7 recx=%5%6" )
		.arg( double( G.w ), 0, 'f', 2 ).arg( double( G.s ), 0, 'g', 4 )
		.arg( G.invHg > 0.0f ? double( 1.0f / G.invHg ) : 0.0, 0, 'f', 1 ).arg( double( G.zg ), 0, 'f', 1 )
		.arg( QString( G.recx ).replace( QLatin1Char( ' ' ), QLatin1Char( '_' ) ) )
		.arg( G.conflict.isEmpty() ? QString() : G.conflict.startsWith( QStringLiteral( "conflict:" ) ) ? QStringLiteral( " recxconflict=1" )
			: QStringLiteral( " recxoverride=1" ) ).arg( g.terrNote );
	// lane FARGRID1: the far grid's echo (the grid, its range, builds, GPU ms of its last build, the volumes' bytes)
	const double volMb = double( g.texW ) * g.texH * g.texD * 16.0 * 2.0 / ( 1024.0 * 1024.0 );
	const QString farNote = !farOn ? QStringLiteral( "far=off(tier=%1%2)" ).arg( s.farTier )
			.arg( s.farTier > 0 ? QStringLiteral( " handoff=%1 end=%2" ).arg( double( kFarHandoff ), 0, 'f', 0 ).arg( double( farEnd ), 0, 'f', 0 ) : QString() )
		: QStringLiteral( "far=on(tier=%1 %2x%3x%4 start=%5 end=%6 refresh=%7 builds=%8 gpums=%9 volmb=%10 terrain=%11)" )
			.arg( s.farTier ).arg( fp.cols ).arg( fp.rows ).arg( fp.slices ).arg( double( farStart ), 0, 'f', 0 )
			.arg( double( farEnd ), 0, 'f', 0 ).arg( fp.refresh ).arg( g.farBuilds ).arg( g.farGpuMs, 0, 'f', 3 )
			.arg( volMb, 0, 'f', 1 ).arg( [] { WwTerrainShadowMap t; return wwTerrainShadowMap( t ) ? 1 : 0; }() );
	s.last.chop( 1 );
	s.last += QStringLiteral( " " ) + optNote + QStringLiteral( " " ) + groundNote + ( s.farTier > 0 ? QStringLiteral( " " ) + farNote : QString() )
		+ ( dr.on ? QStringLiteral( " drift=" ) + dr.note : QString() ) + QStringLiteral( ")" );

	/* WW_CELL_AIR_DUMP=<file>: int32 cols rows slices; the injected and the integrated volumes (cols x rows RGBA float
	 * per layer, layer 0 first, bottom row first); <file>.txt the numbers an independent rebuild needs */
	if ( !s.dump.isEmpty() ) {
		// lane PERF2 gates: WW_CELL_AIR_DUMP_FRAMES=1 keeps one file per camera-path frame (<file>.<frame>)
		WW_SWITCH_STATIC( bool, perFrame, qEnvironmentVariableIntValue( "WW_CELL_AIR_DUMP_FRAMES" ) == 1 );
		const int pf = wwCellPostPathFrame();
		QFile f( perFrame && pf >= 0 ? s.dump + QStringLiteral( ".%1" ).arg( pf, 3, 10, QLatin1Char( '0' ) ) : s.dump );
		if ( f.open( QIODevice::WriteOnly ) ) {
			const qint32 hd[3] = { cols, rows, kSlices };
			f.write( reinterpret_cast<const char *>( hd ), sizeof( hd ) );
			std::vector<float> layer( size_t( cols ) * size_t( rows ) * 4 );
			fn->glPixelStorei( GL_PACK_ALIGNMENT, 4 );
			fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.fbo );
			for ( GLuint tex : { g.inject, g.integ } )
				for ( int k = 0; k < kSlices; k++ ) {
					fn->glFramebufferTextureLayer( GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, k );
					fn->glReadPixels( 0, 0, cols, rows, GL_RGBA, GL_FLOAT, layer.data() );
					f.write( reinterpret_cast<const char *>( layer.data() ), qint64( layer.size() * sizeof( float ) ) );
				}
		}
		QFile t( s.dump + QStringLiteral( ".txt" ) );
		if ( t.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
			auto v = [&]( const float * a, int n ) { QStringList l; for ( int i = 0; i < n; i++ ) l << QString::number( double( a[i] ), 'g', 9 ); return l.join( ' ' ); };
			const float airV[4] = { M.sigma0, M.zb, M.invH, zTop };
			const float sl[2] = { nearD, logRatio }, pr[2] = { p00, p11 };
			QString o;
			o += QStringLiteral( "dims %1 %2 %3\nviewport %4 %5 %6 %7\n" ).arg( cols ).arg( rows ).arg( kSlices ).arg( vp[0] ).arg( vp[1] ).arg( W ).arg( H );
			o += QStringLiteral( "slices %1\nproj %2\n" ).arg( v( sl, 2 ), v( pr, 2 ) );
			for ( int k = 0; k < 6; k++ )
				o += QStringLiteral( "fogK%1 %2\n" ).arg( k ).arg( v( K[k], 4 ) );
			o += QStringLiteral( "fogView %1\nfogSun %2\nfogSunColour %3\nfogDistScale %4\n" ).arg( v( fogView, 4 ), v( fogSun, 4 ), v( fogSunCol, 4 ), v( &distScale, 1 ) );
			o += QStringLiteral( "airMed %1\nband %2\nsceneTopW %3\ncamZw %4\n" ).arg( v( airV, 4 ) ).arg( M.band ? 1 : 0 )
				.arg( double( topW ), 0, 'g', 9 ).arg( double( camZw ), 0, 'g', 9 );
			o += QStringLiteral( "medium %1\n" ).arg( v( med, 12 ) );
			for ( int k = 0; k < 3; k++ )
				o += QStringLiteral( "phase%1 %2\n" ).arg( k ).arg( v( ph.w[k], 4 ) );
			o += QStringLiteral( "gi %1\n" ).arg( v( ph.gi, 4 ) );
			o += QStringLiteral( "cellOn %1\ncellInterior %2\ncellHasDir %3\ncellLightCount %4\ncellGiOn %5\ncellHasDalc %6\ncellExtDalc %7\n" )
				.arg( cellOnV ).arg( L && L->interior ? 1 : 0 ).arg( cellHasDirV ).arg( cellLightCountV ).arg( cellGiOnV )
				.arg( cellHasDalcV ).arg( cellExtDalcV );
			o += QStringLiteral( "cellGiAirOn %1\n" ).arg( cellGiAirOnV );
			for ( int k = 0; k < 3; k++ )
				o += QStringLiteral( "cellRow%1 %2\n" ).arg( k ).arg( v( cellRowV[k], 4 ) );
			o += QStringLiteral( "cellDirColor %1\ncellDirTo %2\n" ).arg( v( cellDirColV, 3 ), v( cellDirToV, 3 ) );
			o += QStringLiteral( "csm %1\nterms %2\nred %3\nmedNote %4\nms %5\n" ).arg( csm ? 1 : 0 )
				.arg( s.terms ).arg( s.red ).arg( medNote ).arg( ms, 0, 'f', 3 );
			o += QStringLiteral( "opt %1\n" ).arg( optNote );
			// lane FOGDRIFT1: the density factor 1 + amp x fade x (a + 0.5 fade b) / 1.5, a b = noise - mean (cell_volair.frag airDrift)
			o += QStringLiteral( "drift %1 amp %2 mean %3 span %4 fade %5 %6 tile %7 %8 ofs %9 %10 %11 %12 %13 %14\n" ).arg( dr.note )
				.arg( double( driftAmp ), 0, 'g', 9 ).arg( double( dr.mean ), 0, 'g', 9 ).arg( double( dr.span ), 0, 'g', 9 )
				.arg( double( kDriftFade0 ) ).arg( double( kDriftFade1 ) ).arg( double( kDriftTile ) ).arg( double( kDriftTile * kDriftDetail ) )
				.arg( double( dr.ofs[0][0] ), 0, 'g', 9 ).arg( double( dr.ofs[0][1] ), 0, 'g', 9 ).arg( double( dr.ofs[0][2] ), 0, 'g', 9 )
				.arg( double( dr.ofs[1][0] ), 0, 'g', 9 ).arg( double( dr.ofs[1][1] ), 0, 'g', 9 ).arg( double( dr.ofs[1][2] ), 0, 'g', 9 );
			o += QStringLiteral( "airLayer %1\nairGround %2\nrecx %3\nrecxConflict %4\n" ).arg( v( layerV, 4 ), v( groundV, 4 ), G.recx,
				G.conflict.isEmpty() ? QStringLiteral( "none" ) : G.conflict );
			// lane LAMPAIR1: every light the air sees (world pos, radius, linear colour with fade, spot cos, shape,
			// shadow kind, DATA constant/scalar/exponent, the placed ref (0: an effect's light))
			if ( L )
				for ( qsizetype i = 0; i < L->lights.size(); i++ ) {
					const WwCellLight & l = L->lights.at( i );
					float dim, off[3];	// lane LIGHTFLICKER1: this frame's flicker, as the light buffer holds it
					wwCellFlickerOf( scene->nifModel, i, &dim, off );
					const float a[13] = { l.pos[0] + off[0], l.pos[1] + off[1], l.pos[2] + off[2], l.radius, l.color[0] * dim,
						l.color[1] * dim, l.color[2] * dim,
						l.spot ? l.cosOuter : -2.0f, float( l.shape ), float( l.shadow ), l.bias, l.scale, l.exponent };
					o += QStringLiteral( "light %1 %2 %3\n" ).arg( i ).arg( v( a, 13 ) ).arg( l.ref, 8, 16, QLatin1Char( '0' ) );
				}
			t.write( o.toUtf8() );
		}
	}
	restore();
}

void wwVolAirUniforms( Scene * scene )
{
	if ( !scene || !scene->renderer )
		return;
	VaState & s = vs();
	if ( s.inPass )
		return;
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->getCurrentProgram();
	if ( !prog || prog->uniLocation( "volOn" ) < 0 )
		return;
	VaGpu & g = vaGpus()[r];
	const bool on = s.on && g.ready && g.doc == scene->nifModel && g.unitsOk > 0;
	prog->uni1b( "volOn", on );
	if ( g.unitsOk <= 0 )
		return;
	// bound whether or not this draw reads it (a sampler left on unit 0 would sit beside BaseMap)
	WwCellUni::Units<std::remove_pointer_t<decltype( r->fn )>> u( r->fn, wwCellUniDsa() );	// lane CELLUNI3: no ask with DSA
	u.section();
	u.bind( kVolTexUnit, GL_TEXTURE_3D, on ? g.integ : 0 );
	// 10-16: and stage 0's column distances, the lookup's clamp (red nofarclamp: none)
	const bool clamp = on && g.tileValid && !( s.red & 2097152 );
	u.bind( kTileUnit, GL_TEXTURE_2D, clamp ? g.tile : 0 );
	// lane FOGTERRAIN2: the pass's terrain grid, so the far tail, the sky and the sun stand on the same ground
	const int terrLoc = prog->uniLocation( "volAirTerr" );
	const bool terr = on && g.terrOn && g.terr && terrLoc >= 0;
	if ( terrLoc >= 0 ) {
		u.bind( WwCellUnit::AirTerrain, GL_TEXTURE_2D_ARRAY, terr ? g.terr : 0 );
		prog->uni1i_l( terrLoc, WwCellUnit::AirTerrain );
	}
	u.done();
	prog->uni1i( "volTex", kVolTexUnit );
	prog->uni1i( "volTile", kTileUnit );
	prog->uni1i( "volFarClamp", clamp ? 1 : 0 );
	if ( !on )
		return;
	if ( terr ) {
		const Transform & vt = scene->view;
		const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
		float row[2][4];
		for ( int k = 0; k < 2; k++ ) {	// celllights.cpp cellRow: world = dot(row.xyz, posView) + row.w
			float w = 0.0f;
			for ( int j = 0; j < 3; j++ )
				w -= vt.rotation( j, k ) * vt.translation[j];
			row[k][0] = vt.rotation( 0, k ) / sc;
			row[k][1] = vt.rotation( 1, k ) / sc;
			row[k][2] = vt.rotation( 2, k ) / sc;
			row[k][3] = w / sc;
		}
		float U[4], V[4];
		wwFogTerrainViewRows( row[0], row[1], g.terrXf, U, V );
		prog->uni4f( "volAirTerrU", FloatVector4( U[0], U[1], U[2], U[3] ) );
		prog->uni4f( "volAirTerrV", FloatVector4( V[0], V[1], V[2], V[3] ) );
		wwFogTerrainViewRows( row[0], row[1], g.terrFineXf, U, V );	// lane FOGTERRAIN3: the fine level's
		prog->uni4f( "volAirTerrFU", FloatVector4( U[0], U[1], U[2], U[3] ) );
		prog->uni4f( "volAirTerrFV", FloatVector4( V[0], V[1], V[2], V[3] ) );
		prog->uni1f( "volAirTerrRef", g.terrRef );
	}
	GLint vp[4] = { 0, 0, 1, 1 };
	WwCellUni::viewport( r->fn, vp );	// lane CELLUNI3: once per armed setup-memo epoch
	prog->uni4f_l( prog->uniLocation( "volRect" ), FloatVector4( float( vp[0] ), float( vp[1] ),
		1.0f / float( std::max( vp[2], 1 ) ), 1.0f / float( std::max( vp[3], 1 ) ) ) );
	prog->uni4f( "volSlices", FloatVector4( g.near, g.logRatio, float( g.slices ), ( s.red & 1 ) ? 0.0f : 1.0f ) );
	prog->uni1i( "volProbe", s.probe );
	prog->uni1i( "volFogBlend", s.fogBlend );
	// lane FARGRID1: the packed volume's layout (volFar.w 0: the near grid alone, the lookups as before)
	prog->uni4f( "volFar", FloatVector4( g.farStart, g.farLog, float( g.farSlices ), g.farOn ? 1.0f : 0.0f ) );
	const float tw = float( std::max( g.texW, 1 ) ), th = float( std::max( g.texH, 1 ) );
	prog->uni4f( "volPack", FloatVector4( float( g.cols ) / tw, float( g.rows ) / th, float( g.farCols ) / tw, float( g.farRows ) / th ) );
	prog->uni4f( "volPackDims", FloatVector4( tw, th, float( std::max( g.texD, 1 ) ), float( g.slices ) ) );
	// FOGSUN1: the surfaces' sun through the air (lookdev_fog.glsl wwVolAirSunT); red nosunt: the sun not dimmed
	// and the far tail past the froxels (lookdev_fog.glsl wwVolAirAt): the medium and what is on (the reds clear a bit)
	const int ml = prog->uniLocation( "volAirMed" );
	if ( ml >= 0 )
		prog->uni4f_l( ml, FloatVector4( g.med[0], g.med[1], g.med[2], g.med[3] ) );
	const int ll = prog->uniLocation( "volAirLayer" );
	if ( ll >= 0 )
		prog->uni4f_l( ll, FloatVector4( g.layer[0], g.layer[1], g.layer[2], g.layer[3] ) );
	const int gloc = prog->uniLocation( "volAirGround" );
	if ( gloc >= 0 )
		prog->uni4f_l( gloc, FloatVector4( g.ground[0], g.ground[1], g.ground[2], g.ground[3] ) );
	const int fl = prog->uniLocation( "volAirFlags" );
	if ( fl >= 0 )
		prog->uni1i_l( fl, ( ( s.red & 512 ) ? 0 : 1 ) | ( ( s.red & 1024 ) ? 0 : 2 ) | ( ( s.red & 256 ) ? 0 : 4 )
			| ( ( s.red & 65536 ) ? 0 : 8 ) | ( ( s.red & 131072 ) ? 0 : 16 ) | ( s.tricubic ? 32 : 0 ) | ( terr ? 64 : 0 ) );
}

/* lane VOLAIR1 (bungo 10-14, cellvolair.h): the sky through the air. The far tower stood out against a sky the air never
 * touched. cell_volsky.prog over the pixels no draw reaches: before the scene, into the linear frame (stencil 1, the
 * resolve composites it over the backdrop by its coverage, cell_hdr.frag mode 2); in a probe frame, after the scene, the
 * raw probe on the far-plane pixels (stencil 2: written back as is). */
void wwVolAirSkyDraw( Scene * scene, bool probes )
{
	if ( !scene || !scene->renderer || scene->selecting )
		return;
	VaState & s = vs();
	if ( s.inPass )
		return;
	WW_SWITCH_STATIC( bool, fogProbeFrame, [] {
		const auto p = wwEnv( "WW_LOOKDEV_FOGPROBE" ).split( ',' );
		return p.size() >= 3 && p[2].trimmed().toFloat() > 8.5f;
	}() );
	const bool probeFrame = s.probe == 1 || s.probe == 2 || fogProbeFrame;
	if ( probes != probeFrame )
		return;
	Renderer * r = scene->renderer;
	VaGpu & g = vaGpus()[r];
	const bool hdr = wwCellHdrActive();
	static QString said;
	auto note = [&]( const QString & line ) {
		if ( line != said ) {
			said = line;
			qInfo().noquote() << "cell air sky:" << line;
		}
	};
	if ( !( s.on && g.ready && g.doc == scene->nifModel && g.unitsOk > 0 ) ) {
		note( QStringLiteral( "off (no air this frame)" ) );
		return;
	}
	if ( s.red & 1 ) {
		note( QStringLiteral( "off (red off: the game's fog)" ) );
		return;
	}
	if ( !wwLookdevFogWanted( scene ) ) {
		note( QStringLiteral( "off (the fog row is off)" ) );
		return;
	}
	if ( !probes && ( s.red & 65536 ) ) {
		note( QStringLiteral( "off (red nosky: the sky without the air)" ) );
		return;
	}
	if ( !probes && !hdr ) {
		note( QStringLiteral( "off (no linear frame: the sky's air composites through the cell's one tone map)" ) );
		return;
	}
	NifSkopeOpenGLContext::Program * prog = r->useProgram( "cell_volsky.prog" );
	if ( !prog ) {
		note( QStringLiteral( "refused (no cell_volsky.prog)" ) );
		return;
	}
	wwLookdevFogUniforms( scene );	// fogOn, the view and the sun; wwVolAirUniforms: the volume, the medium, the flags
	prog->uni1i( "volSkyMode", probes ? 1 : 0 );
	prog->uni1b( "volSkyOn", !( s.red & 65536 ) );
	auto fn = r->fn;
	GLboolean colorMask[4], depthMask = GL_TRUE;
	fn->glGetBooleanv( GL_COLOR_WRITEMASK, colorMask );
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	const bool wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasBlend = fn->glIsEnabled( GL_BLEND );
	const bool wasCull = fn->glIsEnabled( GL_CULL_FACE ), wasStencil = fn->glIsEnabled( GL_STENCIL_TEST );
	const bool wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );
	GLint prevDepthFunc = GL_LESS;
	fn->glGetIntegerv( GL_DEPTH_FUNC, &prevDepthFunc );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	fn->glDepthMask( GL_FALSE );
	if ( probes ) {
		fn->glEnable( GL_DEPTH_TEST );
		fn->glDepthFunc( GL_LEQUAL );	// the fragment's depth is 1.0: only the pixels no opaque draw reached
	} else {
		fn->glDisable( GL_DEPTH_TEST );	// under everything: the scene's draws replace it where they land
	}
	if ( hdr ) {
		fn->glEnable( GL_STENCIL_TEST );
		fn->glStencilMask( 0x03 );
		fn->glStencilFunc( GL_ALWAYS, probes ? 2 : 1, 0xFF );
		fn->glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
	} else {
		fn->glDisable( GL_STENCIL_TEST );
	}
	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = quad;
	r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
	r->stopProgram();
	fn->glDepthFunc( GLenum( prevDepthFunc ) );
	if ( wasDepth ) fn->glEnable( GL_DEPTH_TEST ); else fn->glDisable( GL_DEPTH_TEST );
	if ( wasBlend ) fn->glEnable( GL_BLEND ); else fn->glDisable( GL_BLEND );
	if ( wasCull ) fn->glEnable( GL_CULL_FACE ); else fn->glDisable( GL_CULL_FACE );
	if ( wasScissor ) fn->glEnable( GL_SCISSOR_TEST );
	if ( wasStencil ) fn->glEnable( GL_STENCIL_TEST ); else fn->glDisable( GL_STENCIL_TEST );
	fn->glColorMask( colorMask[0], colorMask[1], colorMask[2], colorMask[3] );
	fn->glDepthMask( depthMask );
	note( probes ? QStringLiteral( "probes (red nosky %1)" ).arg( ( s.red & 65536 ) ? 1 : 0 )
		: QStringLiteral( "through the air (linear frame, composited by 1 - T over the backdrop)" ) );
}

/* ---- lane VOLPART1: volumetric particles (cellvolair.h, res/shaders/particles.frag) ---- */
namespace
{
struct VpState
{
	bool loaded = false;
	bool on = false, pinned = false;
	int red = 0;
	quint64 liveGen = 0;	// fix TDFIX2: a live set of WW_CELL_VOLPART* re-reads them
};

VpState & vps()
{
	static VpState s;
	if ( !s.loaded || wwSwitchMovedFor( s.liveGen, { "WW_CELL_VOLPART" } ) ) {
		s.loaded = true;
		s.liveGen = wwSwitchGen();
		s.pinned = false;	// back to the defaults before the re-read (the reds are or-ed)
		s.red = 0;
		const QByteArray pin = qgetenv( "WW_CELL_VOLPART" );
		if ( !pin.isEmpty() ) {
			s.pinned = true;
			s.on = pin.trimmed() != "0";
		} else {
			s.on = QSettings().value( QStringLiteral( "WW/VolPart" ), false ).toBool();
		}
		for ( const QByteArray & t : qgetenv( "WW_CELL_VOLPART_RED" ).split( ',' ) ) {
			const QByteArray r = t.trimmed();
			s.red |= r == "flat" ? 1 : r == "nothick" ? 2 : r == "nosoft" ? 4 : r == "nolights" ? 8 : r == "white" ? 16
				: r == "nofog" ? 32 : r == "noshadow" ? 64 : r == "noedge" ? 128 : r == "nophase" ? 256 : r == "nonear" ? 512 : 0;
		}
	}
	return s;
}
}	// namespace

bool wwVolPartOn()
{
	return vps().on;
}

namespace
{
int vpFxFlags = 0;	// the effect card being set up: 1 a volumetric card, 2 its soft fade on (depth ALWAYS)
}

void wwVolPartFxSet( int flags )
{
	vpFxFlags = flags;
}

int wwVolPartFx()
{
	return vpFxFlags;
}

bool wwVolPartHidden()
{
	WW_SWITCH_STATIC( bool, hide, qEnvironmentVariableIntValue( "WW_CELL_VOLPART_HIDE" ) == 1 );	// lane LIVESET1: follows a live set
	return hide;
}

void wwVolPartSetOn( bool on )
{
	VpState & s = vps();
	if ( s.pinned )
		return;
	s.on = on;
	QSettings().setValue( QStringLiteral( "WW/VolPart" ), on );
}

int wwVolPartRed()
{
	return vps().red;
}

bool wwVolPartUniforms( Scene * scene )
{
	if ( !scene || !scene->renderer )
		return false;
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->getCurrentProgram();
	if ( !prog || prog->uniLocation( "volPart" ) < 0 )
		return false;
	const VpState & s = vps();
	prog->uni1i( "volPart", s.on ? 1 : 0 );
	prog->uni1i( "volPartDump", 0 );
	if ( !s.on )
		return false;
	// the air's medium and phase this frame; no air drawn: a clear medium (the light alone) and the phase the air
	// would take (the cell's god-ray record indoors, the weather's outdoors, else the fallback)
	const VaGpu & g = vaGpus()[r];
	const bool air = vs().on && g.ready && g.doc == scene->nifModel;
	VaPhase fb;
	if ( !air ) {
		const WwCellLighting * L = wwCellLightsWanted( scene ) ? wwCellLightsFor( scene->nifModel ) : nullptr;
		float med[12] = { 0.60f, 1.52f, 3.31f, 2, 2, 2, 1, 1, 1, 0.75f, 0, 1 };
		if ( L && L->interior )
			std::copy( L->godRay, L->godRay + 12, med );
		else
			wwLookdevGodRays( med, nullptr );
		fb = vaPhase( med, ( vs().red & 524288 ) != 0 );
	}
	prog->uni4f( "airMed", air ? FloatVector4( g.med[0], g.med[1], g.med[2], g.med[3] ) : FloatVector4( 0.0f, 0.0f, 0.0f, 1e30f ) );
	// the landed air's layered law (its near band, its ground layer) and its GPU reds (roomstep, physcol), as the pass
	prog->uni4f( "airLayer", air ? FloatVector4( g.layer[0], g.layer[1], g.layer[2], g.layer[3] ) : FloatVector4( 0.0f, 0.0f, 0.0f, 0.0f ) );
	prog->uni4f( "airGround", air ? FloatVector4( g.ground[0], g.ground[1], g.ground[2], g.ground[3] ) : FloatVector4( 0.0f, 0.0f, 0.0f, 0.0f ) );
	// lane FOGTERRAIN2: the ground layer stands on the pass's terrain grid (cell_volpart.glsl vpLight airGroundAt)
	const bool terr = air && g.terrOn && g.terr;
	const int terrLoc = prog->uniLocation( "airTerr" );
	if ( terrLoc >= 0 ) {
		GLint prevActive = 0;
		r->fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
		r->fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::AirTerrain ) );
		r->fn->glBindTexture( GL_TEXTURE_2D_ARRAY, terr ? g.terr : 0 );
		r->fn->glActiveTexture( GLenum( prevActive ) );
		prog->uni1i_l( terrLoc, WwCellUnit::AirTerrain );
	}
	prog->uni1i( "airTerrOn", terr ? 1 : 0 );
	if ( terr ) {
		prog->uni4f( "airTerrXf", FloatVector4( g.terrXf[0], g.terrXf[1], g.terrXf[2], g.terrXf[3] ) );
		prog->uni4f( "airTerrFineXf", FloatVector4( g.terrFineXf[0], g.terrFineXf[1], g.terrFineXf[2], g.terrFineXf[3] ) );
		prog->uni1f( "airTerrRef", g.terrRef );
	}
	prog->uni1i( "volRed", vs().red & ~1 );
	prog->uni1f( "volShell", vs().shellStep ? 0.0f : 1.0f );
	for ( int k = 0; k < 3; k++ ) {
		const float * w = air ? g.phase[k] : fb.w[k];
		prog->uni4f_l( prog->uniLocation( "volPhase[%d]", k ), FloatVector4( w[0], w[1], w[2], w[3] ) );
	}
	const float * gi = air ? g.gi : fb.gi;
	prog->uni4f( "volGi", FloatVector4( gi[0], gi[1], gi[2], gi[3] ) );
	prog->uni1b( "volCsm", air ? g.csm : wwSunShadowWanted( scene ) );
	prog->uni1i( "volTerms", vs().terms );
	prog->uni1i( "volPartRed", s.red );
	return true;
}

QString wwVolPartSummary()
{
	const VpState & s = vps();
	return QStringLiteral( "volpart=%1%2 red=%3" ).arg( s.on ? QStringLiteral( "on" ) : QStringLiteral( "off" ) )
		.arg( s.pinned ? QStringLiteral( "(pinned)" ) : QString() ).arg( s.red );
}

#include "cellbloom.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store

#include "gl/celllights.h"
#include "gl/celllooks.h"
#include "gl/celluniform.h"	// lane CELLUNI4: the tone map's known state
#include "gl/glscene.h"
#include "gl/renderer.h"
#include "harnesswindow.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QSettings>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// lane BLOOM1b: the FO4CS bloom and physical glare on the cell view's linear frame (cellbloom.h)

namespace {

constexpr int kMaxMips = 12;			// FO4CS kBloomMaxMipLevels
constexpr float kEps = 0.0001f;			// kBloomEpsilon
constexpr float kMinRadius = 0.5f, kMaxRadius = 4.0f, kMaxIntensity = 10.0f;
constexpr int kUnitA = 8, kUnitB = 9;	// the chain's passes (their own programs: no cell sampler beside them)

const char * const kModeKey = "WW/CellBloomMode";

struct Field
{
	const char * id;		// wwCellBloomPinned's name
	const char * env;
	const char * key;
};

const Field kFields[] = {
	{ "threshold", "WW_CELL_BLOOM_THRESHOLD", "WW/CellBloomThreshold" },
	{ "knee", "WW_CELL_BLOOM_KNEE", "WW/CellBloomSoftKnee" },
	{ "intensity", "WW_CELL_BLOOM_INTENSITY", "WW/CellBloomIntensity" },
	{ "radius", "WW_CELL_BLOOM_RADIUS", "WW/CellBloomRadius" },
	{ "mips", "WW_CELL_BLOOM_MIPS", "WW/CellBloomMips" },
	{ "glare", "WW_CELL_GLARE", "WW/CellGlare" },
	{ "strength", "WW_CELL_GLARE_STRENGTH", "WW/CellGlareStrength" },
	{ "fnumber", "WW_CELL_GLARE_FNUMBER", "WW/CellGlareFNumber" },
	{ "dispersion", "WW_CELL_GLARE_DISPERSION", "WW/CellGlareDispersion" },
};
constexpr int kFieldCount = int( sizeof( kFields ) / sizeof( kFields[0] ) );

struct BloomGpu
{
	GLuint srcFbo = 0, resFbo = 0, fbo = 0;
	GLuint msColor = 0, src = 0, finalTex = 0;
	GLuint mip[kMaxMips] = {}, up[kMaxMips] = {};
	int dims[kMaxMips][2] = {};
	int mipCount = 0, w = 0, h = 0, samples = 0;
};

struct BloomState
{
	bool init = false;
	quint64 liveGen = 0;	// lane TESTDRIVER1: the switch generation last read
	WwCellBloomSettings set;
	bool modePinned = false;
	bool pinned[kFieldCount] = {};
	int red = 0;				// 1 nokaris
	float impulse = 0.0f;		// WW_CELL_BLOOM_IMPULSE: the 2x2 block at the frame's centre, this value
	QString dumpDir;
	bool dumped = false;
	// this frame
	bool replacing = false;
	GLuint top = 0;
	bool topMips = false;	// lane GAPLOOK1: Final grew the top's mips this frame
	int w = 0, h = 0;
	float finalWeight = 0.0f, radius = 1.0f, dispersion = 0.0f;
	QString last = QStringLiteral( "vanilla" ), said;
	QString selfTest;
};

QHash<Renderer *, BloomGpu> & bloomGpus()
{
	static QHash<Renderer *, BloomGpu> g;
	return g;
}

QString f6( double v )
{
	return QString::number( v, 'g', 6 );
}

WwCellBloomSettings clamped( WwCellBloomSettings s )
{
	// BloomSettingsCodec::Sanitize's ranges
	auto fin = []( float v, float d ) { return std::isfinite( v ) ? v : d; };
	s.threshold = std::clamp( fin( s.threshold, 1.0f ), 0.0f, 64.0f );
	s.softKnee = std::clamp( fin( s.softKnee, 0.5f ), 0.0f, 1.0f );
	s.intensity = std::clamp( fin( s.intensity, 0.45f ), 0.0f, kMaxIntensity );
	s.radius = std::clamp( fin( s.radius, 1.0f ), kMinRadius, kMaxRadius );
	s.mips = std::clamp( s.mips, 0, kMaxMips );
	s.glareStrength = std::clamp( fin( s.glareStrength, 0.1f ), 0.0f, 4.0f );
	s.glareFNumber = std::clamp( fin( s.glareFNumber, 4.0f ), 1.0f, 22.0f );
	s.glareDispersion = std::clamp( fin( s.glareDispersion, 0.15f ), 0.0f, 2.0f );
	return s;
}

float * floatField( WwCellBloomSettings & s, int i )
{
	switch ( i ) {
	case 0: return &s.threshold;
	case 1: return &s.softKnee;
	case 2: return &s.intensity;
	case 3: return &s.radius;
	case 6: return &s.glareStrength;
	case 7: return &s.glareFNumber;
	case 8: return &s.glareDispersion;
	default: return nullptr;
	}
}

bool truthy( const QString & v )
{
	return v != QLatin1StringView( "0" ) && v.compare( QLatin1StringView( "off" ), Qt::CaseInsensitive ) != 0
		&& v.compare( QLatin1StringView( "false" ), Qt::CaseInsensitive ) != 0;
}

BloomState & bs()
{
	static BloomState s;
	// lane TESTDRIVER1: a live set of these switches re-reads them (pins and or-ed reds back to their defaults first)
	if ( s.init && !wwSwitchMovedFor( s.liveGen, { "WW_CELL_BLOOM" } ) )
		return s;
	const bool again = s.init;
	s.init = true;
	s.liveGen = wwSwitchGen();
	{
		const BloomState d {};
		s.set = d.set;
		s.modePinned = d.modePinned;
		for ( int i = 0; i < kFieldCount; i++ )
			s.pinned[i] = d.pinned[i];
	}
	QSettings q;
	// the mode: a pin wins; a harness without the pin runs Vanilla (it measures what it pins); else the saved row
	const QString modePin = wwEnvStr( "WW_CELL_BLOOM" ).trimmed();
	if ( !modePin.isEmpty() ) {
		s.modePinned = true;
		s.set.fo4cs = modePin.compare( QLatin1StringView( "fo4cs" ), Qt::CaseInsensitive ) == 0;
	} else if ( wwHarnessRun() ) {
		s.modePinned = true;
		s.set.fo4cs = false;
	} else {
		s.set.fo4cs = q.value( QLatin1StringView( kModeKey ), QStringLiteral( "vanilla" ) ).toString()
			.compare( QLatin1StringView( "fo4cs" ), Qt::CaseInsensitive ) == 0;
	}
	for ( int i = 0; i < kFieldCount; i++ ) {
		const QString pin = wwEnvStr( kFields[i].env ).trimmed();
		s.pinned[i] = !pin.isEmpty();
		// a harness reads only pins (the defaults otherwise), never what the window saved
		const bool useSaved = !s.pinned[i] && !wwHarnessRun() && q.contains( QLatin1StringView( kFields[i].key ) );
		if ( float * f = floatField( s.set, i ) ) {
			bool ok = false;
			const float v = s.pinned[i] ? pin.toFloat( &ok ) : useSaved ? q.value( QLatin1StringView( kFields[i].key ) ).toFloat( &ok ) : 0.0f;
			if ( ok )
				*f = v;
		} else if ( i == 4 ) {
			bool ok = false;
			const int v = s.pinned[i] ? pin.toInt( &ok ) : useSaved ? q.value( QLatin1StringView( kFields[i].key ) ).toInt( &ok ) : 0;
			if ( ok )
				s.set.mips = v;
		} else if ( i == 5 ) {
			if ( s.pinned[i] )
				s.set.glare = truthy( pin );
			else if ( useSaved )
				s.set.glare = q.value( QLatin1StringView( kFields[i].key ) ).toBool();
		}
	}
	s.set = clamped( s.set );
	s.red = wwEnvStr( "WW_CELL_BLOOM_RED" ).contains( QLatin1StringView( "nokaris" ) ) ? 1 : 0;
	s.impulse = wwEnvStr( "WW_CELL_BLOOM_IMPULSE" ).toFloat();
	s.dumpDir = wwEnvStr( "WW_CELL_BLOOM_DUMP" ).trimmed();
	if ( !again ) {
		s.selfTest = wwCellBloomSelfTest();
		qInfo().noquote() << "cell bloom: FO4CS checks" << s.selfTest;
	}
	return s;
}

void note( BloomState & s, const QString & line )
{
	s.last = line;
	if ( s.said != line ) {	// the notes get a line when it changes, not once a frame
		s.said = line;
		qInfo().noquote() << "cell bloom:" << line;
	}
}

GLuint makeTex( Renderer * r, int w, int h )
{
	auto fn = r->fn;
	GLuint t = 0;
	fn->glGenTextures( 1, &t );
	fn->glBindTexture( GL_TEXTURE_2D, t );
	// half floats, as FO4CS forces its pyramid; bilinear and clamped (its LinearClamp sampler)
	fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	return t;
}

void freeGpu( Renderer * r, BloomGpu & g )
{
	auto fn = r->fn;
	std::vector<GLuint> t = { g.msColor, g.src, g.finalTex };
	for ( int i = 0; i < kMaxMips; i++ ) {
		t.push_back( g.mip[i] );
		t.push_back( g.up[i] );
		g.mip[i] = g.up[i] = 0;
	}
	for ( GLuint x : t )
		if ( x )
			fn->glDeleteTextures( 1, &x );
	g.msColor = g.src = g.finalTex = 0;
	g.mipCount = 0;
}

// cellpost.cpp's dump format: int32 w h channels, then rows bottom-up, channels interleaved
void dumpTex( Renderer * r, GLuint fbo, GLuint tex, int w, int h, const QString & path )
{
	auto fn = r->fn;
	std::vector<float> v( size_t( w ) * size_t( h ) * 4 );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, fbo );
	fn->glFramebufferTexture2D( GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0 );
	fn->glReadBuffer( GL_COLOR_ATTACHMENT0 );
	fn->glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	fn->glReadPixels( 0, 0, w, h, GL_RGBA, GL_FLOAT, v.data() );
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly ) )
		return;
	const std::int32_t hdr[3] = { w, h, 4 };
	f.write( reinterpret_cast<const char *>( hdr ), sizeof( hdr ) );
	f.write( reinterpret_cast<const char *>( v.data() ), qint64( v.size() * sizeof( float ) ) );
}

// the band weights of one up pass (Bloom.cpp UpdateUpsampleConstants)
void bandWeights( const WwCellBloomSettings & s, int sourceLevel, int accumulatedLevel, int mipCount, float & src, float & acc )
{
	const float base = std::max( s.intensity, 0.0f );
	const float glare = s.glare ? std::max( s.glareStrength, 0.0f ) : 0.0f;
	const float total = std::max( base + glare, kEps );
	src = base / total + ( glare / total ) * wwBloomGlareBand( s, sourceLevel, mipCount );
	acc = accumulatedLevel + 1 == mipCount ? base / total + ( glare / total ) * wwBloomGlareBand( s, accumulatedLevel, mipCount ) : 1.0f;
}

}	// namespace

// ---- FO4CS's CPU maths (src/Post/Bloom.cpp), names as there

int wwBloomPlanMips( int w, int h, int maxLevels, int dims[][2] )
{
	int width = std::max( w, 1 ), height = std::max( h, 1 ), n = 0;
	const int levels = std::min( std::max( maxLevels, 0 ), kMaxMips );
	for ( int level = 0; level < levels && ( width > 1 || height > 1 ); level++ ) {
		width = std::max( ( width + 1 ) / 2, 1 );
		height = std::max( ( height + 1 ) / 2, 1 );
		dims[n][0] = width;
		dims[n][1] = height;
		n++;
	}
	return n;
}

float wwBloomThresholdWeight( float a_luminance, float a_threshold, float a_softKnee )
{
	const float luminance = std::max( a_luminance, 0.0f );
	if ( luminance <= kEps )
		return 0.0f;
	const float threshold = std::max( a_threshold, 0.0f );
	const float knee = threshold * std::max( a_softKnee, 0.0f );
	const float hard = luminance - threshold;
	float contribution = std::max( hard, 0.0f );
	if ( knee > kEps ) {
		const float soft = std::clamp( hard + knee, 0.0f, 2.0f * knee );
		contribution = std::max( contribution, ( soft * soft ) / ( 4.0f * knee ) );
	}
	return std::clamp( contribution / luminance, 0.0f, 1.0f );
}

float wwBloomExtractLuminance( float a_luminance, const WwCellBloomSettings & s )
{
	if ( !s.fo4cs )
		return 0.0f;
	const float luminance = std::max( a_luminance, 0.0f );
	return luminance * wwBloomThresholdWeight( luminance, s.threshold, s.softKnee );
}

float wwBloomWeight( const WwCellBloomSettings & s )
{
	if ( !s.fo4cs )
		return 0.0f;
	const float glare = s.glare ? std::max( s.glareStrength, 0.0f ) : 0.0f;
	return std::clamp( s.intensity + glare, 0.0f, kMaxIntensity );
}

float wwBloomRadius( const WwCellBloomSettings & s )
{
	return std::clamp( s.radius, kMinRadius, kMaxRadius );
}

float wwBloomGlareBand( const WwCellBloomSettings & s, int level, int mipCount, float displayPeakNits )
{
	if ( !s.glare || mipCount <= 0 || level >= mipCount )
		return 0.0f;
	// Fraunhofer width grows with the F-number; display peak over the 250-nit SDR reference narrows it
	const float peakRatio = std::max( displayPeakNits, 1.0f ) / 250.0f;
	const float effF = std::clamp( s.glareFNumber / std::sqrt( std::max( peakRatio, 1.0f ) ), 1.0f, 22.0f );
	const float widthBands = std::clamp( 0.75f + 0.65f * std::log2( effF / 1.4f ), 0.5f, 3.5f );
	auto raw = [widthBands]( int l ) { return std::exp2( -float( l ) / widthBands ); };
	float norm = 0.0f;
	for ( int i = 0; i < mipCount; i++ )
		norm += raw( i );
	return raw( level ) / std::max( norm, kEps );
}

QString wwCellBloomSelfTest()
{
	// tests/post_bloom_tests.cpp, ported: the doctest Approx tolerance (1e-5 relative, scale 1)
	int pass = 0, total = 0;
	QString first;
	auto check = [&]( bool ok, const char * what ) {
		total++;
		if ( ok )
			pass++;
		else if ( first.isEmpty() )
			first = QLatin1String( what );
	};
	auto approx = []( float a, float b ) { return std::fabs( a - b ) <= 1e-5f * ( 1.0f + std::max( std::fabs( a ), std::fabs( b ) ) ); };
	int d[kMaxMips][2];
	int n = wwBloomPlanMips( 1920, 1080, 4, d );
	check( n == 4 && d[0][0] == 960 && d[0][1] == 540 && d[1][0] == 480 && d[1][1] == 270 && d[2][0] == 240 && d[2][1] == 135
		&& d[3][0] == 120 && d[3][1] == 68, "mip plan 1920x1080 x4" );
	check( wwBloomPlanMips( 1, 1, 6, d ) == 0, "mip plan 1x1" );
	n = wwBloomPlanMips( 2, 1, 6, d );
	check( n == 1 && d[0][0] == 1 && d[0][1] == 1, "mip plan 2x1" );
	n = wwBloomPlanMips( 1920, 1080, 3, d );
	check( n == 3 && d[0][0] == 960 && d[0][1] == 540 && d[2][0] == 240 && d[2][1] == 135, "dispatch mips 1920x1080 x3" );
	check( approx( wwBloomThresholdWeight( 0.25f, 1.0f, 0.5f ), 0.0f ), "threshold 0.25" );
	check( approx( wwBloomThresholdWeight( 1.0f, 1.0f, 0.0f ), 0.0f ), "threshold 1 hard" );
	check( approx( wwBloomThresholdWeight( 1.0f, 1.0f, 0.5f ), 0.125f ), "threshold 1 knee" );
	check( approx( wwBloomThresholdWeight( 1.5f, 1.0f, 0.5f ), 1.0f / 3.0f ), "threshold 1.5" );
	check( approx( wwBloomThresholdWeight( 3.0f, 1.0f, 0.5f ), 2.0f / 3.0f ), "threshold 3" );
	WwCellBloomSettings s;
	s.threshold = 1.0f;
	s.softKnee = 0.5f;
	s.fo4cs = false;
	check( approx( wwBloomExtractLuminance( 1.5f, s ), 0.0f ), "extract off" );
	s.fo4cs = true;
	check( approx( wwBloomExtractLuminance( 1.5f, s ), 0.5f ), "extract on" );
	WwCellBloomSettings u;
	u.fo4cs = true;
	u.intensity = 99.0f;
	u.radius = 99.0f;
	check( approx( wwBloomRadius( u ), 4.0f ) && approx( wwBloomWeight( u ), 10.0f ), "upsample clamps" );
	WwCellBloomSettings dsc;
	dsc.fo4cs = true;
	dsc.intensity = 1.5f;
	dsc.radius = 2.0f;
	check( approx( wwBloomRadius( dsc ), 2.0f ) && approx( wwBloomWeight( dsc ), 1.5f ), "dispatch weights" );
	WwCellBloomSettings g;
	g.fo4cs = true;
	g.intensity = 0.0f;
	g.glare = true;
	g.glareStrength = 0.2f;
	g.glareFNumber = 4.0f;
	check( approx( wwBloomWeight( g ), 0.2f ), "glare adds energy" );
	float sum = 0.0f;
	for ( int l = 0; l < 6; l++ )
		sum += wwBloomGlareBand( g, l, 6, 250.0f );
	check( approx( sum, 1.0f ), "glare bands sum to 1" );
	g.glareFNumber = 8.0f;
	check( wwBloomGlareBand( g, 5, 6, 1000.0f ) < wwBloomGlareBand( g, 5, 6, 250.0f ), "glare narrows with peak" );
	WwCellBloomSettings off;
	check( !off.fo4cs && approx( wwBloomWeight( off ), 0.0f ), "default off" );
	return first.isEmpty() ? QStringLiteral( "%1/%2" ).arg( pass ).arg( total )
		: QStringLiteral( "%1/%2 FAILED %3" ).arg( pass ).arg( total ).arg( first );
}

// ---- the rows

WwCellBloomSettings wwCellBloomSettings()
{
	return bs().set;
}

bool wwCellBloomPinned( const char * which )
{
	BloomState & s = bs();
	if ( QLatin1StringView( which ) == QLatin1StringView( "mode" ) )
		return s.modePinned;
	for ( int i = 0; i < kFieldCount; i++ )
		if ( QLatin1StringView( which ) == QLatin1StringView( kFields[i].id ) )
			return s.pinned[i];
	return false;
}

void wwCellBloomSet( const WwCellBloomSettings & in )
{
	BloomState & s = bs();
	WwCellBloomSettings v = clamped( in );
	QSettings q;
	if ( s.modePinned )
		v.fo4cs = s.set.fo4cs;
	else if ( v.fo4cs != s.set.fo4cs || !q.contains( QLatin1StringView( kModeKey ) ) )
		q.setValue( QLatin1StringView( kModeKey ), v.fo4cs ? QStringLiteral( "fo4cs" ) : QStringLiteral( "vanilla" ) );
	for ( int i = 0; i < kFieldCount; i++ ) {
		float * nf = floatField( v, i );
		float * of = floatField( s.set, i );
		if ( s.pinned[i] ) {	// a pin stays: the row shows it, greyed
			if ( nf )
				*nf = *of;
			else if ( i == 4 )
				v.mips = s.set.mips;
			else if ( i == 5 )
				v.glare = s.set.glare;
			continue;
		}
		if ( nf ) {
			if ( *nf != *of )
				q.setValue( QLatin1StringView( kFields[i].key ), double( *nf ) );
		} else if ( i == 4 ) {
			if ( v.mips != s.set.mips )
				q.setValue( QLatin1StringView( kFields[i].key ), v.mips );
		} else if ( i == 5 ) {
			if ( v.glare != s.set.glare )
				q.setValue( QLatin1StringView( kFields[i].key ), v.glare );
		}
	}
	s.set = v;
}

// ---- the frame

bool wwCellBloomReplacing()
{
	return bs().replacing;
}

QString wwCellBloomEcho()
{
	return QStringLiteral( "cell bloom: " ) + bs().last;
}

bool wwCellBloomRun( Scene * scene, unsigned hdrColor, unsigned ds, int W, int H, int samples, unsigned dsFormat, bool dsTexture )
{
	BloomState & s = bs();
	s.replacing = false;
	s.top = 0;
	Q_UNUSED( dsFormat );
	if ( !s.set.fo4cs ) {
		note( s, QStringLiteral( "vanilla (the imagespace's bloom)" ) );
		return false;
	}
	if ( !scene || !scene->renderer || !hdrColor || !ds || W < 2 || H < 2 )
		return false;
	const WwCellBloomSettings & set = s.set;
	const float weight = wwBloomWeight( set );
	int dims[kMaxMips][2];
	const int mipCount = wwBloomPlanMips( W, H, set.mips, dims );
	if ( weight <= 0.0f || set.mips == 0 ) {	// FO4CS PassDisabled: no bloom at all, the vanilla one stays off too
		note( s, QStringLiteral( "fo4cs refused (intensity + glare 0 or 0 mips: pass disabled)" ) );
		return false;
	}
	if ( mipCount == 0 ) {
		note( s, QStringLiteral( "fo4cs refused (no mip chain for %1x%2)" ).arg( W ).arg( H ) );
		return false;
	}
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	while ( fn->glGetError() != GL_NO_ERROR ) {
	}
	BloomGpu & g = bloomGpus()[r];
	const int S = std::max( samples, 1 );
	bool made = false;
	if ( !g.srcFbo || g.w != W || g.h != H || g.samples != S || g.mipCount != mipCount
		|| ( mipCount && ( g.dims[0][0] != dims[0][0] || g.dims[0][1] != dims[0][1] ) ) ) {
		made = true;
		freeGpu( r, g );
		if ( !g.srcFbo ) {
			fn->glGenFramebuffers( 1, &g.srcFbo );
			fn->glGenFramebuffers( 1, &g.resFbo );
			fn->glGenFramebuffers( 1, &g.fbo );
		}
		fn->glGenTextures( 1, &g.msColor );
		fn->glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, g.msColor );
		fn->glTexImage2DMultisample( GL_TEXTURE_2D_MULTISAMPLE, S, GL_RGBA16F, W, H, GL_TRUE );
		fn->glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, 0 );
		g.src = makeTex( r, W, H );
		for ( int i = 0; i < mipCount; i++ ) {
			g.mip[i] = makeTex( r, dims[i][0], dims[i][1] );
			if ( i + 1 < mipCount )
				g.up[i] = makeTex( r, dims[i][0], dims[i][1] );
			g.dims[i][0] = dims[i][0];
			g.dims[i][1] = dims[i][1];
		}
		fn->glBindFramebuffer( GL_FRAMEBUFFER, g.resFbo );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.src, 0 );
		g.w = W;
		g.h = H;
		g.samples = S;
		g.mipCount = mipCount;
	}

	/* state kept for the restore. lane CELLUNI4: the tone map's known state, unless the block above made the targets
	 * (it bound its own framebuffer first, so the old asks read that one: kept as it was) */
	WwCellUni::KnownGl kg;
	if ( const WwCellUni::KnownGl * kn = made ? nullptr : WwCellUni::known( fn ) )
		kg = *kn;
	else
		WwCellUni::askAll( fn, kg );
	if ( made )
		WwCellUni::knownSlot() = nullptr;	// the restore binds what was asked: the passes after it ask too
	const GLint prevDraw = kg.draw, prevRead = kg.read, prevActive = kg.active;
	const GLint * vp = kg.vp;
	const GLboolean * colorMask = kg.colorMask;
	const GLboolean depthMask = kg.depthMask;
	const GLfloat * clear = kg.clear;
	const bool wasDepth = kg.depth, wasBlend = kg.blend, wasCull = kg.cull, wasStencil = kg.stencil, wasScissor = kg.scissor;
	const GLint stFunc = kg.sFunc, stRef = kg.sRef, stValMask = kg.sMask, stWriteMask = kg.sWrite;
	const GLint stFail = kg.sFail, stZFail = kg.sZFail, stZPass = kg.sPass;

	fn->glDisable( GL_DEPTH_TEST );
	fn->glDepthMask( GL_FALSE );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );

	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = quad;
	const GLenum one = GL_COLOR_ATTACHMENT0;
	bool ok = true;
	QString why;
	auto bind = [&]( int unit, GLenum target, GLuint tex ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
		fn->glBindTexture( target, tex );
	};
	auto target = [&]( GLuint tex, int w, int h ) {
		fn->glBindFramebuffer( GL_FRAMEBUFFER, g.fbo );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0 );
		fn->glDrawBuffers( 1, &one );
		fn->glViewport( 0, 0, w, h );
	};

	// 1. the source: the cell-lit samples (stencil 1, the frame's own depth / stencil), exposed; then resolved
	fn->glBindFramebuffer( GL_FRAMEBUFFER, g.srcFbo );
	fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, g.msColor, 0 );
	if ( dsTexture )	// lane TONEMAP1: the frame's depth / stencil is a multisample texture
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, ds, 0 );
	else
		fn->glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, ds );
	fn->glDrawBuffers( 1, &one );
	if ( fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
		ok = false;
		why = QStringLiteral( "the source target is incomplete" );
	}
	if ( ok ) {
		fn->glViewport( 0, 0, W, H );
		fn->glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
		fn->glClear( GL_COLOR_BUFFER_BIT );
		const bool impulse = s.impulse > 0.0f;
		if ( !impulse ) {
			fn->glEnable( GL_STENCIL_TEST );
			fn->glStencilMask( 0 );
			fn->glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
			fn->glStencilFunc( GL_EQUAL, 1, 0x03 );	// a cell program or a cell effect wrote it last: light
		} else {
			fn->glDisable( GL_STENCIL_TEST );
		}
		auto prog = r->useProgram( "cell_bloom_src.prog" );
		if ( !prog ) {
			ok = false;
			why = QStringLiteral( "no cell_bloom_src.prog" );
		} else {
			wwCellLightsUniforms( scene );	// the exposure (and the red's noexp)
			bind( kUnitA, GL_TEXTURE_2D_MULTISAMPLE, hdrColor );
			prog->uni1i( "hdrSrc", kUnitA );
			prog->uni3f( "bloomImpulse", float( W / 2 ), float( H / 2 ), impulse ? s.impulse : 0.0f );
			r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
			r->stopProgram();
			bind( kUnitA, GL_TEXTURE_2D_MULTISAMPLE, 0 );
		}
		fn->glDisable( GL_STENCIL_TEST );
		if ( dsTexture )
			fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, 0, 0 );
		else
			fn->glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0 );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.srcFbo );
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.resFbo );
		fn->glBlitFramebuffer( 0, 0, W, H, 0, 0, W, H, GL_COLOR_BUFFER_BIT, GL_NEAREST );
		const GLenum err = fn->glGetError();
		if ( ok && err != GL_NO_ERROR ) {
			ok = false;
			why = QStringLiteral( "the source pass or its resolve failed, GL error %1" ).arg( err );
		}
	}

	// 2. the down chain (BloomDownCS): level 0 from the source with the threshold and the Karis average
	const float thr = std::clamp( set.threshold, 0.0f, 64.0f ), knee = std::clamp( set.softKnee, 0.0f, 1.0f );
	for ( int i = 0; ok && i < mipCount; i++ ) {
		target( g.mip[i], dims[i][0], dims[i][1] );
		auto prog = r->useProgram( "cell_bloom_down.prog" );
		if ( !prog ) {
			ok = false;
			why = QStringLiteral( "no cell_bloom_down.prog" );
			break;
		}
		bind( kUnitA, GL_TEXTURE_2D, i == 0 ? g.src : g.mip[i - 1] );
		prog->uni1i( "bloomSrc", kUnitA );
		prog->uni2f( "bloomDst", float( dims[i][0] ), float( dims[i][1] ) );
		prog->uni4f( "bloomParams", FloatVector4( thr, knee, i == 0 ? 1.0f : 0.0f, 1.0f ) );
		prog->uni1i( "bloomRed", s.red );
		r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
		r->stopProgram();
	}

	// 3. the up chain (BloomUpCS): from the deepest mip up to level 0, each band at its weight
	const float radius = wwBloomRadius( set );
	const float dispersion = set.glare ? std::clamp( set.glareDispersion, 0.0f, 2.0f ) : 0.0f;
	GLuint accumulated = mipCount ? g.mip[mipCount - 1] : 0;
	QStringList bands;
	for ( int offset = mipCount - 1; ok && offset > 0; offset-- ) {
		const int level = offset - 1;
		float srcW = 1.0f, accW = 1.0f;
		bandWeights( set, level, offset, mipCount, srcW, accW );
		bands << QStringLiteral( "%1:%2/%3" ).arg( level ).arg( f6( srcW ), f6( accW ) );
		target( g.up[level], dims[level][0], dims[level][1] );
		auto prog = r->useProgram( "cell_bloom_up.prog" );
		if ( !prog ) {
			ok = false;
			why = QStringLiteral( "no cell_bloom_up.prog" );
			break;
		}
		bind( kUnitA, GL_TEXTURE_2D, g.mip[level] );
		bind( kUnitB, GL_TEXTURE_2D, accumulated );
		prog->uni1i( "bloomSrc", kUnitA );
		prog->uni1i( "bloomMip", kUnitB );
		prog->uni2f( "bloomDst", float( dims[level][0] ), float( dims[level][1] ) );
		prog->uni4f( "bloomParams", FloatVector4( radius, srcW, accW, dispersion ) );
		r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
		r->stopProgram();
		accumulated = g.up[level];
	}
	// the final combine's weight; one mip: its band weight too (UpdateUpsampleConstants, a_finalCombine)
	float finalWeight = weight;
	if ( mipCount == 1 ) {
		const float base = std::max( set.intensity, 0.0f );
		const float glare = set.glare ? std::max( set.glareStrength, 0.0f ) : 0.0f;
		const float total = std::max( base + glare, kEps );
		finalWeight *= base / total + ( glare / total ) * wwBloomGlareBand( set, 0, 1 );
	}
	if ( ok ) {
		const GLenum err = fn->glGetError();
		if ( err != GL_NO_ERROR ) {
			ok = false;
			why = QStringLiteral( "the chain failed, GL error %1" ).arg( err );
		}
	}

	// the dump (the gate's food): the source, every mip, every up level, the final bloom over a zero source
	if ( ok && !s.dumpDir.isEmpty() && !s.dumped ) {
		s.dumped = true;
		QDir().mkpath( s.dumpDir );
		const QString dd = s.dumpDir + QLatin1Char( '/' );
		dumpTex( r, g.fbo, g.src, W, H, dd + "src.bin" );
		for ( int i = 0; i < mipCount; i++ ) {
			dumpTex( r, g.fbo, g.mip[i], dims[i][0], dims[i][1], dd + QStringLiteral( "mip%1.bin" ).arg( i ) );
			if ( i + 1 < mipCount )
				dumpTex( r, g.fbo, g.up[i], dims[i][0], dims[i][1], dd + QStringLiteral( "up%1.bin" ).arg( i ) );
		}
		// the final combine as the tone map runs it, its source weight 0: tent(top) x the weight, full size, fp32
		GLuint fin = 0;
		fn->glGenTextures( 1, &fin );
		fn->glBindTexture( GL_TEXTURE_2D, fin );
		fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, W, H, 0, GL_RGBA, GL_FLOAT, nullptr );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		target( fin, W, H );
		if ( auto prog = r->useProgram( "cell_bloom_up.prog" ) ) {
			bind( kUnitA, GL_TEXTURE_2D, g.src );
			bind( kUnitB, GL_TEXTURE_2D, accumulated );
			prog->uni1i( "bloomSrc", kUnitA );
			prog->uni1i( "bloomMip", kUnitB );
			prog->uni2f( "bloomDst", float( W ), float( H ) );
			prog->uni4f( "bloomParams", FloatVector4( radius, 0.0f, finalWeight, dispersion ) );
			r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
			r->stopProgram();
			dumpTex( r, g.fbo, fin, W, H, dd + "final.bin" );
		}
		fn->glDeleteTextures( 1, &fin );
		QFile f( dd + "bloom.txt" );
		if ( f.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
			QTextStream t( &f );
			t << "frame " << W << " " << H << " samples " << S << "\n";
			t << "mips " << mipCount << "\n";
			for ( int i = 0; i < mipCount; i++ )
				t << "mip " << i << " " << dims[i][0] << " " << dims[i][1] << "\n";
			t << "threshold " << f6( thr ) << "\nknee " << f6( knee ) << "\nintensity " << f6( set.intensity ) << "\nradius " << f6( radius )
			  << "\nglare " << ( set.glare ? 1 : 0 ) << "\nstrength " << f6( set.glareStrength ) << "\nfnumber " << f6( set.glareFNumber )
			  << "\ndispersion " << f6( dispersion ) << "\nweight " << f6( weight ) << "\nfinalweight " << f6( finalWeight )
			  << "\nred " << s.red << "\nimpulse " << f6( s.impulse ) << " " << W / 2 << " " << H / 2 << "\n";
		}
	}

	// restore
	bind( kUnitA, GL_TEXTURE_2D, 0 );
	bind( kUnitB, GL_TEXTURE_2D, 0 );
	fn->glActiveTexture( GLenum( prevActive ) );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
	fn->glClearColor( clear[0], clear[1], clear[2], clear[3] );
	fn->glColorMask( colorMask[0], colorMask[1], colorMask[2], colorMask[3] );
	fn->glDepthMask( depthMask );
	fn->glStencilFunc( GLenum( stFunc ), stRef, GLuint( stValMask ) );
	fn->glStencilMask( GLuint( stWriteMask ) );
	fn->glStencilOp( GLenum( stFail ), GLenum( stZFail ), GLenum( stZPass ) );
	if ( wasStencil ) fn->glEnable( GL_STENCIL_TEST ); else fn->glDisable( GL_STENCIL_TEST );
	if ( wasDepth ) fn->glEnable( GL_DEPTH_TEST ); else fn->glDisable( GL_DEPTH_TEST );
	if ( wasBlend ) fn->glEnable( GL_BLEND ); else fn->glDisable( GL_BLEND );
	if ( wasCull ) fn->glEnable( GL_CULL_FACE ); else fn->glDisable( GL_CULL_FACE );
	if ( wasScissor ) fn->glEnable( GL_SCISSOR_TEST );

	if ( !ok ) {
		note( s, QStringLiteral( "fo4cs refused (%1): the vanilla bloom drawn" ).arg( why ) );
		return false;
	}
	s.replacing = true;
	s.top = accumulated;
	s.w = W;
	s.h = H;
	s.finalWeight = finalWeight;
	s.radius = radius;
	s.dispersion = dispersion;
	QString glare = QStringLiteral( "glare=off" );
	if ( set.glare ) {
		QStringList b;
		for ( int i = 0; i < mipCount; i++ )
			b << f6( wwBloomGlareBand( set, i, mipCount ) );
		glare = QStringLiteral( "glare=on strength=%1 f=%2 dispersion=%3 bands=[%4]" )
			.arg( f6( set.glareStrength ), f6( set.glareFNumber ), f6( dispersion ), b.join( QLatin1Char( ',' ) ) );
	}
	note( s, QStringLiteral( "fo4cs mips=%1 (%2x%3..%4x%5) threshold=%6 knee=%7 intensity=%8 radius=%9 weight=%10 %11%12 (checks %13)" )
		.arg( mipCount ).arg( dims[0][0] ).arg( dims[0][1] ).arg( dims[mipCount - 1][0] ).arg( dims[mipCount - 1][1] )
		.arg( f6( thr ), f6( knee ), f6( set.intensity ), f6( radius ), f6( finalWeight ), glare,
			s.red ? QStringLiteral( " RED nokaris" ) : s.impulse > 0.0f ? QStringLiteral( " IMPULSE %1" ).arg( f6( s.impulse ) ) : QString(),
			s.selfTest ) );
	return true;
}

void wwCellBloomFinal( Renderer * r, int unit )
{
	BloomState & s = bs();
	auto prog = r ? r->getCurrentProgram() : nullptr;
	if ( !prog )
		return;
	prog->uni1i( "cellIsFoBloom", unit );	// always its own unit: a sampler2D, never left on 0 beside another type
	prog->uni1b( "cellIsFoBloomOn", s.replacing );
	if ( !s.replacing )
		return;
	auto fn = r->fn;
	GLint prevActive = 0;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
	fn->glBindTexture( GL_TEXTURE_2D, s.top );
	/* lane GAPLOOK1 (BLOOMMIP): the top's own mips, so the composite picks a wider blur per pixel; w = 1 the fog's
	 * gain picks it, -1 the red (lod 2 everywhere), 0 off (the base level only, today's bits) */
	float perPixel = 0.0f;
	if ( wwLookOn( WwLook::BloomMip ) ) {
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3 );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
		fn->glGenerateMipmap( GL_TEXTURE_2D );
		s.topMips = true;
		perPixel = wwEnvStr( "WW_CELL_BLOOM_PERPIX_RED" ) == QLatin1String( "wide" ) ? -1.0f : 1.0f;
	}
	fn->glActiveTexture( GLenum( prevActive ) );
	prog->uni4f( "cellIsFoBloomParams", FloatVector4( s.radius, s.finalWeight, s.dispersion, perPixel ) );
	prog->uni2f( "cellIsFoBloomFrame", float( s.w ), float( s.h ) );
}

void wwCellBloomDone( Renderer * r, int unit )
{
	BloomState & s = bs();
	if ( s.replacing && r ) {
		auto fn = r->fn;
		GLint prevActive = 0;
		fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
		if ( s.topMips && s.top ) {
			// the chain renders into the top again next frame: back to its one level
			fn->glBindTexture( GL_TEXTURE_2D, s.top );
			fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
			fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
		}
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
		fn->glActiveTexture( GLenum( prevActive ) );
	}
	s.topMips = false;
	s.replacing = false;
	s.top = 0;
}

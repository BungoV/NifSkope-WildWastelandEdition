/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "esmweather.h"

#include "esmfile.hpp"
#include "gamemanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>

namespace {

constexpr unsigned int GRUP = 0x50555247U;
constexpr float kEngineDegToRad = 0.0174532924f;	// the engine's DEG_TO_RAD constant, not pi/180 in double
constexpr float kSunXExtreme = 400.0f;	// fSunXExtreme, 1.10.155 .data
constexpr float kSunYExtreme = 25.0f;	// fSunYExtreme
constexpr float kSunShadowScale = -15.0f;	// fSunShadowScale as Fallout4.esm sets it
constexpr float kSunShadowMinAngle = 30.0f;	// fSunShadowMinAngle

QString fieldString( const ESMFile::ESMField & f )
{
	const char * p = reinterpret_cast<const char *>( f.data() );
	size_t n = f.size();
	while ( n > 0 && p[n - 1] == '\0' )
		n--;
	return QString::fromLatin1( p, qsizetype( n ) );
}

QString baseName( const QString & path )
{
	return QFileInfo( path ).fileName();
}

QString hex8( quint32 v )
{
	return QString( "%1" ).arg( v, 8, 16, QChar( '0' ) ).toUpper();
}

//! the 4-ToD records (fv < 111) store Sunrise, Day, Sunset, Night only
int storedSlot( int slot, int tods )
{
	if ( tods >= 8 )
		return slot;
	switch ( slot ) {
	case WwTodEarlySunrise:
	case WwTodLateSunrise:
		return WwTodSunrise;
	case WwTodEarlySunset:
	case WwTodLateSunset:
		return WwTodSunset;
	default:
		return slot;
	}
}

} // namespace

const char * wwTodName( int slot )
{
	static const char * const n[8] = { "Sunrise", "Day", "Sunset", "Night",
		"EarlySunrise", "LateSunrise", "EarlySunset", "LateSunset" };
	return ( slot >= 0 && slot < 8 ) ? n[slot] : "?";
}

bool wwLookdevRed( const char * name )
{
	static const QByteArray red = qgetenv( "WW_LOOKDEV_RED" ).trimmed().toLower();
	if ( red.isEmpty() )
		return false;
	for ( const QByteArray & p : red.split( ',' ) )
		if ( p.trimmed() == name )
			return true;
	return false;
}

QString WwTodKeys::describe() const
{
	if ( a == b )
		return QString::fromLatin1( wwTodName( a ) );
	return QString( "%1->%2 %3%" ).arg( wwTodName( a ), wwTodName( b ) ).arg( int( std::lround( t * 100.0f ) ) );
}

WwTodKeys wwTodKeys( double hour, const unsigned char tnam[4], double ext )
{
	if ( wwLookdevRed( "hourstuck" ) )
		hour = 12.0;
	WwTodKeys k;
	const double rise0 = tnam[0] / 6.0, rise1 = tnam[1] / 6.0;
	const double set0 = tnam[2] / 6.0, set1 = tnam[3] / 6.0;
	const double rampA = std::max( 0.0, rise0 - ext );
	const double rampB = std::min( 23.99, set1 + ext );
	// the engine's order inserts the early/late slots; the red walks xEdit's 0..3 order only
	static const int riseSeq[5] = { WwTodNight, WwTodEarlySunrise, WwTodSunrise, WwTodLateSunrise, WwTodDay };
	static const int setSeq[5] = { WwTodDay, WwTodEarlySunset, WwTodSunset, WwTodLateSunset, WwTodNight };
	static const int riseRed[5] = { WwTodNight, WwTodSunrise, WwTodSunrise, WwTodDay, WwTodDay };
	static const int setRed[5] = { WwTodDay, WwTodSunset, WwTodSunset, WwTodNight, WwTodNight };
	const bool red = wwLookdevRed( "todorder" );
	const int * rs = red ? riseRed : riseSeq;
	const int * ss = red ? setRed : setSeq;

	auto ramp = [&k]( const int * seq, double h, double a, double b ) {
		const double len = b - a;
		double u = len > 1e-9 ? ( h - a ) / len * 4.0 : 4.0;
		u = std::clamp( u, 0.0, 3.999999 );
		const int q = int( std::floor( u ) );
		k.a = seq[q];
		k.b = seq[q + 1];
		k.t = float( u - q );
		if ( k.t < 1e-6f )
			k.t = 0.0f;
	};

	if ( hour < rampA || hour >= rampB ) {
		k.a = k.b = WwTodNight;
	} else if ( hour < rise1 ) {
		ramp( rs, hour, rampA, rise1 );
	} else if ( hour <= set0 ) {
		k.a = k.b = WwTodDay;
	} else {
		ramp( ss, hour, set0, rampB );
	}
	return k;
}

void wwVanillaSunPos( double hour, const unsigned char tnam[4], float pos[3] )
{
	if ( wwLookdevRed( "hourstuck" ) )
		hour = 12.0;
	const double rise0 = tnam[0] / 6.0, set1 = tnam[3] / 6.0;
	const double span = std::max( 1e-6, set1 - rise0 );
	const double ramp = 1.0 - 2.0 * ( hour - rise0 ) / span;
	pos[0] = float( ramp * kSunXExtreme );
	pos[1] = kSunYExtreme;
	pos[2] = float( std::fabs( kSunXExtreme ) - std::fabs( ramp * kSunXExtreme ) );
}

void wwVanillaSunLightDir( double hour, const unsigned char tnam[4], float dir[3] )
{
	float p[3];
	wwVanillaSunPos( hour, tnam, p );
	float l = std::sqrt( p[0] * p[0] + p[1] * p[1] + p[2] * p[2] );
	if ( l < 1e-9f )
		l = 1.0f;
	const float nx = p[0] / l, ny = p[1] / l;
	float z = p[2] / l + kSunShadowScale * kEngineDegToRad;
	const float floorZ = kSunShadowMinAngle * kEngineDegToRad;
	if ( floorZ > z )
		z = floorZ;
	l = std::sqrt( nx * nx + ny * ny + z * z );
	dir[0] = nx / l;
	dir[1] = ny / l;
	dir[2] = z / l;
}

/* ------------------------------------------------------------------------
 * The engine clock (lane PBRWX1). See esmweather.h's header comment.
 * ---------------------------------------------------------------------- */

QString WwSkyGmst::describe() const
{
	auto one = [this]( const char * name, double v ) {
		const bool esm = fromEsm.contains( QString::fromLatin1( name ), Qt::CaseInsensitive );
		return QString( "%1=%2(%3)" ).arg( QString::fromLatin1( name ) ).arg( v, 0, 'g', 7 ).arg( esm ? "esm" : "exe" );
	};
	QStringList o;
	o << one( "fSunXExtreme", sunX ) << one( "fSunYExtreme", sunY ) << one( "fSunAlphaTransTime", alphaTrans )
	  << one( "fDaytimeColorExtension", colorExt ) << one( "fSunShadowScale", shadowScale )
	  << one( "fSunShadowMinAngle", shadowMin ) << one( "fSunBaseSize", sunBase ) << one( "fSunGlareSize", sunGlare )
	  << one( "fWeatherCloudSpeedMax", cloudSpeedMax ) << one( "iSecundaSize", secundaSize )
	  << one( "iMasserSize", masserSize ) << one( "fSecundaAngleFadeStart", secundaFadeStart )
	  << one( "fSecundaAngleFadeEnd", secundaFadeEnd ) << one( "fMasserAngleFadeStart", masserFadeStart )
	  << one( "fMasserAngleFadeEnd", masserFadeEnd ) << one( "fDirectionalFogPower", dirFogPower );
	return o.join( QChar( ' ' ) );
}

WwSkyClock wwSkyClock( double hour, const unsigned char tnam[4], const WwSkyGmst & gIn )
{
	WwSkyGmst g = gIn;
	if ( wwLookdevRed( "sunfade2h" ) )
		g.alphaTrans = 2.0f;
	if ( wwLookdevRed( "colorext05" ) )
		g.colorExt = 0.5f;
	if ( wwLookdevRed( "hourstuck" ) )
		hour = 12.0;
	WwSkyClock c;
	c.hour = hour;
	const double t = hour;
	const double srMid = ( double( tnam[0] ) + double( tnam[1] ) ) / 12.0;
	const double ssMid = ( double( tnam[2] ) + double( tnam[3] ) ) / 12.0;
	const double hh = double( g.alphaTrans ) * 0.5;
	c.A = srMid - hh;
	c.B = srMid + hh;
	c.C = ssMid - hh;
	c.D = ssMid + hh;

	// the disc alpha (Sun::Update)
	if ( t < c.A || t > c.D )
		c.sunAlpha = 0.0f;
	else if ( t < c.B )
		c.sunAlpha = float( ( t - c.A ) / std::max( 1e-9, c.B - c.A ) );
	else if ( t <= c.C )
		c.sunAlpha = 1.0f;
	else
		c.sunAlpha = float( 1.0 - ( t - c.C ) / std::max( 1e-9, c.D - c.C ) );
	if ( wwLookdevRed( "sunalpha1" ) )
		c.sunAlpha = 1.0f;

	// the position, with the night branch
	double x;
	if ( ( t >= c.A && t <= c.D ) || wwLookdevRed( "nonightbranch" ) ) {
		x = 1.0 - 2.0 * ( t - c.A ) / std::max( 1e-9, c.D - c.A );
	} else {
		const double tt = t >= c.D ? t - c.D : 24.0 - c.D + t;
		x = 2.0 * tt / std::max( 1e-9, 24.0 - ( c.D - c.A ) ) - 1.0;
	}
	const float X = g.sunX;
	c.sunPos[0] = float( x * X );
	c.sunPos[1] = g.sunY;
	c.sunPos[2] = float( std::fabs( X ) - std::fabs( x * X ) );

	// the light: normalise, bias z, floor z, renormalise (Sun+0x38, TO the light here)
	float l = std::sqrt( c.sunPos[0] * c.sunPos[0] + c.sunPos[1] * c.sunPos[1] + c.sunPos[2] * c.sunPos[2] );
	if ( l < 1e-9f )
		l = 1.0f;
	const float nx = c.sunPos[0] / l, ny = c.sunPos[1] / l;
	float z = c.sunPos[2] / l + g.shadowScale * kEngineDegToRad;
	const float floorZ = g.shadowMin * kEngineDegToRad;
	if ( floorZ > z )
		z = floorZ;
	l = std::sqrt( nx * nx + ny * ny + z * z );
	c.lightDir[0] = nx / l;
	c.lightDir[1] = ny / l;
	c.lightDir[2] = z / l;

	// the stars alpha (Stars::Update)
	{
		const double ext = g.colorExt;
		const double rise1 = tnam[1] / 6.0, set0 = tnam[2] / 6.0;
		const double P = tnam[0] / 6.0 - ext, Q = tnam[3] / 6.0 + ext;
		const double sM = rise1 - ( rise1 - P ) / 2.0, uM = Q - ( Q - set0 ) / 2.0;
		double a;
		if ( t <= P || t >= Q )
			a = 1.0;
		else if ( t < sM )
			a = ( sM - t ) / std::max( 1e-9, sM - P );
		else if ( t <= uM )
			a = 0.0;
		else
			a = ( t - uM ) / std::max( 1e-9, Q - uM );
		c.starsAlpha = float( std::clamp( a, 0.0, 1.0 ) );
	}
	c.keys = wwTodKeys( hour, tnam, double( g.colorExt ) );
	return c;
}

float wwMoonAlpha( const WwSkyClock & c, const WwSkyGmst & gIn, float fadeStart, float fadeEnd )
{
	if ( wwLookdevRed( "moonalpha1" ) )
		return 1.0f;
	WwSkyGmst g = gIn;
	if ( wwLookdevRed( "sunfade2h" ) )
		g.alphaTrans = 2.0f;
	const double hh = double( g.alphaTrans ) * 0.5;
	const double t = c.hour;
	const double inA = c.D + hh * fadeStart, inB = c.D + hh * fadeEnd;
	const double outA = c.A - hh * fadeEnd, outB = c.A - hh * fadeStart;
	double a;
	if ( t >= inB || t <= outA )
		a = 1.0;
	else if ( t > inA && t < inB )
		a = ( t - inA ) / std::max( 1e-9, inB - inA );
	else if ( t > outA && t < outB )
		a = ( outB - t ) / std::max( 1e-9, outB - outA );
	else
		a = 0.0;
	return float( std::clamp( a, 0.0, 1.0 ) );
}

int wwMoonPhase( double gameDays, unsigned char moons )
{
	if ( wwLookdevRed( "phasestuck" ) )
		return 0;
	const int L = moons & 0x3F;
	if ( L <= 0 )
		return -1;
	const long long d = (long long) std::floor( std::max( 0.0, gameDays ) );
	return int( ( d % ( 8LL * L ) ) / L );
}

const char * wwMoonPhaseSuffix( int phase )
{
	static const char * const n[8] = { "full", "three_wan", "half_wan", "one_wan", "new", "one_wax", "half_wax",
		"three_wax" };
	return ( phase >= 0 && phase < 8 ) ? n[phase] : n[0];
}

float wwCloudOffset( float speed, double seconds )
{
	if ( wwLookdevRed( "cloudgametime" ) )
		seconds *= 20.0;	// the game's default timescale: what a game-time clock would do
	const double o = double( speed ) * 0.1 * seconds;
	return float( o - std::floor( o ) );
}

void wwSkyAngles( const float v[3], double * elevDeg, double * azimDeg )
{
	const double l = std::sqrt( double( v[0] ) * v[0] + double( v[1] ) * v[1] + double( v[2] ) * v[2] );
	const double r2d = 180.0 / 3.14159265358979323846;
	if ( elevDeg )
		*elevDeg = l > 1e-12 ? std::asin( std::clamp( double( v[2] ) / l, -1.0, 1.0 ) ) * r2d : 0.0;
	if ( azimDeg ) {
		double a = std::atan2( double( v[0] ), double( v[1] ) ) * r2d;
		if ( a < 0.0 )
			a += 360.0;
		*azimDeg = a;
	}
}

void wwRgbToLab( const float rgb[3], float lab[3] )
{
	float c[3];
	for ( int i = 0; i < 3; i++ ) {
		const float v = rgb[i];
		c[i] = ( v > 0.04045f ? std::pow( ( v + 0.055f ) * 0.9478673f, 2.4f ) : v * 0.07739938f ) * 100.0f;
	}
	float X = c[0] * 0.4124f + c[1] * 0.3576f + c[2] * 0.1805f;
	float Y = c[0] * 0.2126f + c[1] * 0.7152f + c[2] * 0.0722f;
	float Z = c[0] * 0.0193f + c[1] * 0.1192f + c[2] * 0.9505f;
	X *= 0.010521111f;
	Y *= 0.01f;
	Z *= 0.0091841696f;
	auto f = []( float v ) { return v > 0.008856f ? std::cbrt( v ) : v * 7.787f + 0.13793103f; };
	const float fx = f( X ), fy = f( Y ), fz = f( Z );
	lab[0] = 116.0f * fy - 16.0f;
	lab[1] = 500.0f * ( fx - fy );
	lab[2] = 200.0f * ( fy - fz );
}

void wwLabToRgb( const float lab[3], float rgb[3] )
{
	const float fy = ( lab[0] + 16.0f ) * 0.00862069f;
	const float fx = lab[1] * 0.002f + fy;
	const float fz = fy - lab[2] * 0.005f;
	auto g = []( float v ) {
		const float v3 = v * v * v;
		return v3 > 0.008856f ? v3 : ( v - 0.13793103f ) * 0.12841916f;
	};
	const float X = g( fx ) * 95.047f * 0.01f;
	const float Y = g( fy ) * 100.0f * 0.01f;
	const float Z = g( fz ) * 108.883f * 0.01f;
	float c[3];
	c[0] = X * 3.2406f + Y * -1.5372f + Z * -0.4986f;
	c[1] = X * -0.9689f + Y * 1.8758f + Z * 0.0415f;
	c[2] = X * 0.0557f + Y * -0.2040f + Z * 1.0570f;
	for ( int i = 0; i < 3; i++ ) {
		const float v = c[i];
		const float e = v > 0.0031308f ? 1.055f * std::pow( v, 1.0f / 2.4f ) - 0.055f : v * 12.92f;
		rgb[i] = std::clamp( e, 0.0f, 1.0f );
	}
}

void wwLabBlend( const unsigned char a[3], const unsigned char b[3], float t, float rgb[3] )
{
	if ( wwLookdevRed( "rgbblend" ) ) {
		for ( int c = 0; c < 3; c++ )
			rgb[c] = ( float( a[c] ) * ( 1.0f - t ) + float( b[c] ) * t ) / 255.0f;
		return;
	}
	float ca[3], cb[3], la[3], lb[3], l[3];
	for ( int c = 0; c < 3; c++ ) {
		ca[c] = float( a[c] ) / 255.0f;
		cb[c] = float( b[c] ) / 255.0f;
	}
	wwRgbToLab( ca, la );
	wwRgbToLab( cb, lb );
	for ( int c = 0; c < 3; c++ )
		l[c] = la[c] * ( 1.0f - t ) + lb[c] * t;
	wwLabToRgb( l, rgb );
}

EsmWeather::EsmWeather() = default;
EsmWeather::~EsmWeather() = default;

QStringList EsmWeather::mastersOf( const QString & file, QString * why )
{
	QStringList out;
	QFile f( file );
	if ( !f.open( QIODevice::ReadOnly ) ) {
		if ( why )
			*why = QString( "cannot open %1" ).arg( baseName( file ) );
		return out;
	}
	const QByteArray hdr = f.read( 24 );
	if ( hdr.size() < 24 || std::memcmp( hdr.constData(), "TES4", 4 ) != 0 ) {
		if ( why )
			*why = QString( "%1 is not a TES4 plugin" ).arg( baseName( file ) );
		return out;
	}
	quint32 size = 0;
	std::memcpy( &size, hdr.constData() + 4, 4 );
	if ( size > 16u * 1024u * 1024u ) {
		if ( why )
			*why = QString( "%1: TES4 header too large" ).arg( baseName( file ) );
		return out;
	}
	const QByteArray body = f.read( qint64( size ) );
	int p = 0;
	while ( p + 6 <= body.size() ) {
		const char * t = body.constData() + p;
		quint16 len = 0;
		std::memcpy( &len, t + 4, 2 );
		if ( p + 6 + len > body.size() )
			break;
		if ( std::memcmp( t, "MAST", 4 ) == 0 && len > 0 ) {
			QByteArray n( t + 6, len );
			while ( !n.isEmpty() && ( n.endsWith( '\0' ) || n.endsWith( ' ' ) ) )
				n.chop( 1 );
			out << QString::fromLatin1( n );
		}
		p += 6 + len;
	}
	if ( why )
		why->clear();
	return out;
}

QString EsmWeather::checkMasters( const QStringList & files )
{
	if ( files.isEmpty() )
		return QStringLiteral( "no plugin to load" );
	if ( files.size() > 256 )
		return QString( "%1 plugins, more than the 256 one load can hold" ).arg( files.size() );
	QStringList seen;	// case-folded basenames, in load order
	for ( const QString & file : files ) {
		const QString me = baseName( file );
		if ( !QFileInfo::exists( file ) )
			return QString( "%1 not found" ).arg( me );
		QString why;
		const QStringList ms = mastersOf( file, &why );
		if ( !why.isEmpty() )
			return why;
		if ( !wwLookdevRed( "nomaster" ) ) {
			for ( const QString & m : ms ) {
				if ( seen.contains( m.toLower() ) )
					continue;
				bool later = false;
				for ( const QString & g : files )
					if ( baseName( g ).compare( m, Qt::CaseInsensitive ) == 0 )
						later = true;
				return later ? QString( "%1 needs master %2, which loads after it" ).arg( me, m )
				             : QString( "%1 needs master %2, which is not loaded" ).arg( me, m );
			}
		}
		seen << me.toLower();
	}
	return QString();
}

QStringList EsmWeather::loadListFor( const QString & plugin, const QString & dataDir )
{
	QStringList order;
	QSet<QString> done;
	const QString besideDir = QFileInfo( plugin ).absolutePath();
	std::function<void( const QString &, int )> add = [&]( const QString & path, int depth ) {
		const QString key = baseName( path ).toLower();
		if ( done.contains( key ) || depth > 32 )
			return;
		done.insert( key );
		if ( QFileInfo::exists( path ) ) {
			for ( const QString & m : mastersOf( path ) ) {
				QString found;
				for ( const QString & dir : { besideDir, dataDir } ) {
					if ( dir.isEmpty() )
						continue;
					const QString c = QDir( dir ).filePath( m );
					if ( QFileInfo::exists( c ) ) {
						found = c;
						break;
					}
				}
				add( found.isEmpty() ? m : found, depth + 1 );
			}
		}
		order << path;
	};
	add( plugin, 0 );
	return order;
}

QString EsmWeather::gameDataDir()
{
	const QByteArray pin = qgetenv( "WW_LOOKDEV_DATA" );
	if ( !pin.isEmpty() )
		return QDir::fromNativeSeparators( QString::fromLocal8Bit( pin ) );
	for ( const QString & f : Game::GameManager::folders( Game::FALLOUT_4 ) )
		if ( QFile::exists( f + QStringLiteral( "/Fallout4.esm" ) ) )
			return f;
	return QString();
}

bool EsmWeather::load( const QStringList & files, QString * refusal )
{
	esm.reset();
	loaded.clear();
	const QString why = checkMasters( files );
	if ( !why.isEmpty() ) {
		if ( refusal )
			*refusal = why;
		return false;
	}
	QStringList native;
	for ( const QString & f : files )
		native << QDir::toNativeSeparators( f );
	try {
		esm = std::make_unique<ESMFile>( native.join( QChar( ',' ) ).toLocal8Bit().constData() );
	} catch ( std::exception & e ) {
		esm.reset();
		if ( refusal )
			*refusal = QString::fromLatin1( e.what() );
		return false;
	}
	loaded = files;
	if ( refusal )
		refusal->clear();
	return true;
}

QString EsmWeather::loadSummary() const
{
	QStringList b;
	for ( const QString & f : loaded )
		b << baseName( f );
	return b.join( QChar( ',' ) );
}

QVector<WwWeatherEntry> EsmWeather::list()
{
	QVector<WwWeatherEntry> out;
	if ( !esm )
		return out;
	QSet<quint32> seen;
	const ESMFile::ESMRecord * r0 = esm->findRecord( 0U );
	if ( !r0 )
		return out;
	for ( unsigned int g = r0->next; g; ) {
		const ESMFile::ESMRecord * grp = esm->findRecord( g );
		if ( !grp )
			break;
		if ( grp->type == GRUP && FileBuffer::checkType( grp->flags, "WTHR" ) ) {
			for ( unsigned int id = grp->children; id; ) {
				const ESMFile::ESMRecord * r = esm->findRecord( id );
				if ( !r )
					break;
				if ( r->type != GRUP && *r == "WTHR" && !seen.contains( r->formID ) ) {
					seen.insert( r->formID );
					WwWeatherEntry e;
					e.formID = r->formID;
					ESMFile::ESMField f( *esm, *r );
					while ( f.next() )
						if ( f == "EDID" ) {
							e.edid = fieldString( f );
							break;
						}
					if ( int( r->srcFile ) < loaded.size() )
						e.srcFile = baseName( loaded.at( int( r->srcFile ) ) );
					const int owner = int( r->formID >> 24 );
					e.ownerFile = owner < loaded.size() ? baseName( loaded.at( owner ) ) : QString( "?" );
					out.append( e );
				}
				id = r->next;
			}
		}
		g = grp->next;
	}
	std::sort( out.begin(), out.end(), []( const WwWeatherEntry & a, const WwWeatherEntry & b ) {
		return a.edid.compare( b.edid, Qt::CaseInsensitive ) < 0;
	} );
	return out;
}

quint32 EsmWeather::find( const QString & key )
{
	if ( !esm || key.isEmpty() )
		return 0;
	QString k = key.trimmed();
	// "EDID [0002B52A]" as the Scene window shows it
	const int br = k.indexOf( QChar( '[' ) );
	if ( br > 0 && k.endsWith( QChar( ']' ) ) )
		k = k.mid( br + 1, k.size() - br - 2 );
	QString hex = k;
	if ( hex.startsWith( QLatin1String( "0x" ), Qt::CaseInsensitive ) )
		hex = hex.mid( 2 );
	bool ok = false;
	const quint32 v = hex.toUInt( &ok, 16 );
	if ( ok && hex.size() == 8 ) {
		const ESMFile::ESMRecord * r = esm->findRecord( v );
		if ( r && *r == "WTHR" )
			return v;
	}
	for ( const WwWeatherEntry & e : list() )
		if ( e.edid.compare( k, Qt::CaseInsensitive ) == 0 )
			return e.formID;
	return 0;
}

bool EsmWeather::read( quint32 formID, WwWeatherData & out, QString * why )
{
	out = WwWeatherData();
	if ( !esm ) {
		if ( why )
			*why = QStringLiteral( "no plugin loaded" );
		return false;
	}
	const ESMFile::ESMRecord * r = esm->findRecord( formID );
	if ( !r || !( *r == "WTHR" ) ) {
		if ( why )
			*why = QString( "%1 is not a WTHR" ).arg( hex8( formID ) );
		return false;
	}
	out.formID = formID;
	out.formVersion = esm->getRecordFormVersion( *r );
	if ( int( r->srcFile ) < loaded.size() )
		out.srcFile = baseName( loaded.at( int( r->srcFile ) ) );
	// the counts come from the FORM VERSION (xEdit wbWeatherColors), never from the size
	out.nam0Rows = out.formVersion >= 119 ? 19 : 17;
	out.nam0Tods = out.formVersion >= 111 ? 8 : 4;
	QVector<QByteArray> dalcs;
	QByteArray pnam, jnam, qnam, rnam, onam, imsp;
	quint32 present = 0;
	for ( int i = 0; i < 32; i++ )
		out.cloudSpeedX[i] = out.cloudSpeedY[i] = 127;
	try {
		ESMFile::ESMField f( *esm, *r );
		auto bytes = [&f]() { return QByteArray( reinterpret_cast<const char *>( f.data() ), qsizetype( f.size() ) ); };
		while ( f.next() ) {
			if ( f == "EDID" )
				out.edid = fieldString( f );
			else if ( f == "NAM0" )
				out.nam0 = bytes();
			else if ( f == "DALC" )
				dalcs << bytes();
			else if ( f == "DATA" && f.size() >= 5 )
				out.sunGlare = quint8( f.data()[4] );
			else if ( f == "IMSP" )
				imsp = bytes();
			else if ( f == "PNAM" )
				pnam = bytes();
			else if ( f == "JNAM" )
				jnam = bytes();
			else if ( f == "QNAM" )
				qnam = bytes();
			else if ( f == "RNAM" )
				rnam = bytes();
			else if ( f == "ONAM" )
				onam = bytes();
			else if ( f == "LNAM" && f.size() >= 4 ) {
				std::memcpy( &out.cloudLayers, f.data(), 4 );
				out.hasLnam = true;
			} else if ( f == "NAM1" && f.size() >= 4 )
				std::memcpy( &out.cloudNam1, f.data(), 4 );
			else if ( f == "FNAM" ) {
				// lane FOG1: as many floats as stored (18 / 14 / 8); the rest keep the defaults
				out.fogFnamSize = int( f.size() );
				std::memcpy( out.fog, f.data(), std::min<size_t>( sizeof( out.fog ), f.size() & ~size_t( 3 ) ) );
			} else if ( f == "NAM4" ) {
				std::memcpy( out.fogScale, f.data(), std::min<size_t>( sizeof( out.fogScale ), f.size() & ~size_t( 3 ) ) );
				out.hasNam4 = true;
			} else {
				// x0TX: chr(0x30 + i) + "0TX", i = 0..31
				for ( int i = 0; i < 32; i++ ) {
					const char sig[5] = { char( 0x30 + i ), '0', 'T', 'X', 0 };
					if ( f == sig ) {
						out.cloudTex[i] = fieldString( f );
						if ( !out.cloudTex[i].isEmpty() )
							present |= ( 1u << i );
						break;
					}
				}
			}
		}
	} catch ( std::exception & e ) {
		if ( why )
			*why = QString::fromLatin1( e.what() );
		return false;
	}
	// TESWeather::Load: an FNAM that is not 0x48 bytes gets the near height pair copied into the far pair
	if ( out.fogFnamSize != 72 )
		std::copy( out.fog + 8, out.fog + 12, out.fog + 14 );
	out.nam0Size = out.nam0.size();
	const int need = out.nam0Rows * out.nam0Tods * 4;
	if ( out.nam0Size < need ) {
		if ( why )
			*why = QString( "%1: NAM0 is %2 bytes, form version %3 needs %4" )
				.arg( out.edid ).arg( out.nam0Size ).arg( out.formVersion ).arg( need );
		return false;
	}
	const int stride = wwLookdevRed( "stride" ) ? 4 : out.nam0Tods;
	const unsigned char * n0 = reinterpret_cast<const unsigned char *>( out.nam0.constData() );
	for ( int tod = 0; tod < 8; tod++ ) {
		const int s = storedSlot( tod, out.nam0Tods );
		for ( int row = 0; row < out.nam0Rows; row++ ) {
			int src = row;
			if ( wwLookdevRed( "rowswap" ) && ( row == WwRowAmbient || row == WwRowSunlight ) )
				src = row == WwRowAmbient ? WwRowSunlight : WwRowAmbient;
			const int o = ( src * stride + s ) * 4;
			if ( o + 3 > out.nam0Size )
				continue;
			std::memcpy( out.color[tod][row], n0 + o, 3 );
		}
	}
	out.dalcCount = dalcs.size();
	out.hasDalc = !dalcs.isEmpty();
	for ( int tod = 0; tod < 8 && out.hasDalc; tod++ ) {
		int s = storedSlot( tod, dalcs.size() >= 8 ? 8 : 4 );
		if ( s >= dalcs.size() )
			s = dalcs.size() - 1;
		const QByteArray & d = dalcs.at( s );
		const unsigned char * p = reinterpret_cast<const unsigned char *>( d.constData() );
		for ( int ax = 0; ax < 7; ax++ )
			if ( ax * 4 + 3 <= d.size() )
				std::memcpy( out.dalc[tod][ax], p + ax * 4, 3 );
		if ( d.size() >= 32 )
			std::memcpy( &out.dalcFresnel[tod], p + 28, 4 );
	}

	// the clouds (spec_clouds.md s1): the engine ORs the missing-texture mask into NAM1 at load
	out.cloudDisabled = wwLookdevRed( "nam1ignore" ) ? ~present : ( out.cloudNam1 | ~present );
	{
		const int tods = out.formVersion >= 111 ? 8 : 4;
		const int layersP = pnam.size() / ( tods * 4 );
		const unsigned char * pp = reinterpret_cast<const unsigned char *>( pnam.constData() );
		const int layersJ = jnam.size() / ( tods * 4 );
		for ( int tod = 0; tod < 8; tod++ ) {
			const int st = storedSlot( tod, tods );
			for ( int layer = 0; layer < 32; layer++ ) {
				if ( layer < layersP )
					std::memcpy( out.cloudColor[tod][layer], pp + ( layer * tods + st ) * 4, 3 );
				if ( layer < layersJ )
					std::memcpy( &out.cloudAlpha[tod][layer], jnam.constData() + ( layer * tods + st ) * 4, 4 );
			}
		}
		if ( !qnam.isEmpty() || !rnam.isEmpty() ) {
			for ( int i = 0; i < 32; i++ ) {
				if ( i < qnam.size() )
					out.cloudSpeedX[i] = quint8( qnam.at( i ) );
				if ( i < rnam.size() )
					out.cloudSpeedY[i] = quint8( rnam.at( i ) );
			}
		} else {
			for ( int i = 0; i < 4 && i < onam.size(); i++ )
				out.cloudSpeedX[i] = quint8( quint8( onam.at( i ) ) / 2 + 127 );
		}
		if ( wwLookdevRed( "speedswap" ) )
			for ( int i = 0; i < 32; i++ )
				std::swap( out.cloudSpeedX[i], out.cloudSpeedY[i] );
	}

	// IMSP -> IMGS HNAM[7], the Sky Scale (spec_weather_sky.md s2.5)
	{
		const int n = imsp.size() / 4;
		for ( int i = 0; i < n && i < 8; i++ ) {
			quint32 raw = 0;
			std::memcpy( &raw, imsp.constData() + i * 4, 4 );
			out.imsp[i] = raw ? esm->mapFormID( *r, raw ) : 0;
		}
		if ( n > 0 && n < 8 )
			for ( int tod = 0; tod < 8; tod++ )
				out.imsp[tod] = out.imsp[storedSlot( tod, 4 )];
		for ( int tod = 0; tod < 8; tod++ ) {
			if ( !out.imsp[tod] )
				continue;
			const ESMFile::ESMRecord * ig = esm->findRecord( out.imsp[tod] );
			if ( !ig || !( *ig == "IMGS" ) )
				continue;
			try {
				ESMFile::ESMField g( *esm, *ig );
				while ( g.next() ) {
					if ( g == "HNAM" && g.size() >= 32 ) {
						std::memcpy( &out.skyScale[tod], g.data() + 28, 4 );
						out.skyScaleFound++;
						break;
					}
				}
			} catch ( std::exception & ) {
			}
		}
	}
	if ( why )
		why->clear();
	return true;
}

bool EsmWeather::climateData( quint32 formID, WwClimateData & out, QString * why )
{
	out = WwClimateData();
	if ( formID == 0 )
		formID = 0x0000015FU;	// DefaultClimate
	if ( !esm ) {
		if ( why )
			*why = QStringLiteral( "no plugin loaded" );
		return false;
	}
	const ESMFile::ESMRecord * r = esm->findRecord( formID );
	if ( !r || !( *r == "CLMT" ) ) {
		if ( why )
			*why = QString( "%1 is not a CLMT" ).arg( hex8( formID ) );
		return false;
	}
	bool got = false;
	ESMFile::ESMField f( *esm, *r );
	while ( f.next() ) {
		if ( f == "EDID" )
			out.edid = fieldString( f );
		else if ( f == "FNAM" )
			out.sunTex = fieldString( f );
		else if ( f == "GNAM" )
			out.glareTex = fieldString( f );
		else if ( f == "TNAM" && f.size() >= 4 ) {
			std::memcpy( out.tnam, f.data(), std::min<size_t>( 6, f.size() ) );
			got = true;
		}
	}
	out.ok = got;
	if ( !got && why )
		*why = QString( "%1 has no TNAM" ).arg( hex8( formID ) );
	return got;
}

WwSkyGmst EsmWeather::gmst()
{
	WwSkyGmst g;
	if ( !esm || wwLookdevRed( "exegmst" ) )
		return g;
	struct Slot { const char * name; float * v; bool isInt; };
	const Slot slots_[] = {
		{ "fSunXExtreme", &g.sunX, false }, { "fSunYExtreme", &g.sunY, false },
		{ "fSunAlphaTransTime", &g.alphaTrans, false }, { "fDaytimeColorExtension", &g.colorExt, false },
		{ "fSunShadowScale", &g.shadowScale, false }, { "fSunShadowMinAngle", &g.shadowMin, false },
		{ "fSunBaseSize", &g.sunBase, false }, { "fSunGlareSize", &g.sunGlare, false },
		{ "fWeatherCloudSpeedMax", &g.cloudSpeedMax, false }, { "iSecundaSize", &g.secundaSize, true },
		{ "iMasserSize", &g.masserSize, true }, { "fSecundaAngleFadeStart", &g.secundaFadeStart, false },
		{ "fSecundaAngleFadeEnd", &g.secundaFadeEnd, false }, { "fMasserAngleFadeStart", &g.masserFadeStart, false },
		{ "fMasserAngleFadeEnd", &g.masserFadeEnd, false },
		{ "fDirectionalFogPower", &g.dirFogPower, false },
	};
	const ESMFile::ESMRecord * r0 = esm->findRecord( 0U );
	if ( !r0 )
		return g;
	for ( unsigned int gi = r0->next; gi; ) {
		const ESMFile::ESMRecord * grp = esm->findRecord( gi );
		if ( !grp )
			break;
		if ( grp->type == GRUP && FileBuffer::checkType( grp->flags, "GMST" ) ) {
			for ( unsigned int id = grp->children; id; ) {
				const ESMFile::ESMRecord * c = esm->findRecord( id );
				if ( !c )
					break;
				if ( c->type != GRUP && *c == "GMST" ) {
					// the winning version of this FormID (the last plugin that sets it)
					const ESMFile::ESMRecord * w = esm->findRecord( c->formID );
					if ( w ) {
						QString edid;
						QByteArray data;
						try {
							ESMFile::ESMField f( *esm, *w );
							while ( f.next() ) {
								if ( f == "EDID" )
									edid = fieldString( f );
								else if ( f == "DATA" )
									data = QByteArray( reinterpret_cast<const char *>( f.data() ), qsizetype( f.size() ) );
							}
						} catch ( std::exception & ) {
						}
						for ( const Slot & s : slots_ ) {
							if ( data.size() < 4 || edid.compare( QLatin1String( s.name ), Qt::CaseInsensitive ) != 0 )
								continue;
							if ( s.isInt ) {
								qint32 iv = 0;
								std::memcpy( &iv, data.constData(), 4 );
								*s.v = float( iv );
							} else {
								std::memcpy( s.v, data.constData(), 4 );
							}
							if ( !g.fromEsm.contains( QString::fromLatin1( s.name ) ) )
								g.fromEsm << QString::fromLatin1( s.name );
						}
					}
				}
				id = c->next;
			}
		}
		gi = grp->next;
	}
	return g;
}

bool EsmWeather::climate( quint32 formID, unsigned char tnam[4], QString * edid, QString * why )
{
	if ( formID == 0 )
		formID = 0x0000015FU;	// DefaultClimate
	if ( !esm ) {
		if ( why )
			*why = QStringLiteral( "no plugin loaded" );
		return false;
	}
	const ESMFile::ESMRecord * r = esm->findRecord( formID );
	if ( !r || !( *r == "CLMT" ) ) {
		if ( why )
			*why = QString( "%1 is not a CLMT" ).arg( hex8( formID ) );
		return false;
	}
	bool got = false;
	ESMFile::ESMField f( *esm, *r );
	while ( f.next() ) {
		if ( f == "EDID" && edid )
			*edid = fieldString( f );
		else if ( f == "TNAM" && f.size() >= 4 ) {
			std::memcpy( tnam, f.data(), 4 );
			got = true;
		}
	}
	if ( !got && why )
		*why = QString( "%1 has no TNAM" ).arg( hex8( formID ) );
	return got;
}

void EsmWeather::blendRow( const WwWeatherData & w, int row, const WwTodKeys & k, float rgb[3] )
{
	for ( int c = 0; c < 3; c++ )
		rgb[c] = float( w.color[k.a][row][c] ) * ( 1.0f - k.t ) + float( w.color[k.b][row][c] ) * k.t;
}

void EsmWeather::blendDalc( const WwWeatherData & w, int axis, const WwTodKeys & k, float rgb[3] )
{
	for ( int c = 0; c < 3; c++ )
		rgb[c] = float( w.dalc[k.a][axis][c] ) * ( 1.0f - k.t ) + float( w.dalc[k.b][axis][c] ) * k.t;
}

void EsmWeather::blendRowLab( const WwWeatherData & w, int row, const WwTodKeys & k, float rgb[3] )
{
	wwLabBlend( w.color[k.a][row], w.color[k.b][row], k.t, rgb );
	for ( int c = 0; c < 3; c++ )
		rgb[c] *= 255.0f;
}

void EsmWeather::blendCloud( const WwWeatherData & w, int layer, const WwTodKeys & k, float rgb[3], float * alpha )
{
	const int l = ( layer < 0 || quint32( layer ) >= w.cloudLayers || layer >= 32 ) ? 0 : layer;
	wwLabBlend( w.cloudColor[k.a][l], w.cloudColor[k.b][l], k.t, rgb );
	for ( int c = 0; c < 3; c++ )
		rgb[c] *= 255.0f;
	if ( alpha ) {
		*alpha = w.cloudAlpha[k.a][l] * ( 1.0f - k.t ) + w.cloudAlpha[k.b][l] * k.t;
		if ( wwLookdevRed( "cloudalphaone" ) )
			*alpha = 1.0f;
	}
}

float EsmWeather::blendSkyScale( const WwWeatherData & w, const WwTodKeys & k )
{
	return w.skyScale[k.a] * ( 1.0f - k.t ) + w.skyScale[k.b] * k.t;
}

float EsmWeather::cloudSpeed( quint8 b, const WwSkyGmst & g )
{
	return g.cloudSpeedMax * ( 2.0f * float( b ) / 254.0f - 1.0f );
}

/* ------------------------------------------------------------------------
 * The engine fog (lane FOG1). The law: scratchpad/pbrprep1_20260924/spec_fog.md
 * (Sky::UpdateFog 0x64f940@155 for the day weight, Sky::SetColor + UpdateColors
 * for the colours, SetPerFrameConstants 0x1d11150@155 for the packing, the
 * engine composite shader for the formula), checked against fog_model.py.
 * ---------------------------------------------------------------------- */

float wwFogDayWeight( double h, const unsigned char tnam[4], double ext )
{
	const double rb = std::max( 0.0, double( tnam[0] ) / 6.0 - ext );
	const double re = double( tnam[1] ) / 6.0;
	const double sb = double( tnam[2] ) / 6.0;
	const double se = std::min( 23.99, double( tnam[3] ) / 6.0 + ext );
	if ( rb < h && h < re )
		return float( ( h - rb ) / ( re - rb ) );
	if ( re <= h && h <= sb )
		return 1.0f;
	if ( sb < h && h < se )
		return float( ( se - h ) / ( se - sb ) );
	return 0.0f;
}

WwFog wwFogAt( const WwWeatherData & w, double hour, const unsigned char tnam[4], const WwSkyGmst & g )
{
	WwFog o;
	if ( wwLookdevRed( "hourstuck" ) )
		hour = 12.0;
	o.hour = hour;
	o.ext = wwLookdevRed( "fogext05" ) ? 0.5f : g.colorExt;
	o.w = wwLookdevRed( "fognoblend" ) ? 1.0f : wwFogDayWeight( hour, tnam, o.ext );
	const float * F = w.fog;
	auto mix = [&o]( float day, float night ) { return o.w * day + ( 1.0f - o.w ) * night; };
	o.fogNear = mix( F[0], F[2] );
	o.fogFar = mix( F[1], F[3] );
	o.power = mix( F[4], F[5] );
	o.maxv = mix( F[6], F[7] );
	o.nMid = mix( F[8], F[10] );
	o.nRange = mix( F[9], F[11] );
	o.hds = mix( F[12], F[13] );
	o.fMid = mix( F[14], F[16] );
	o.fRange = mix( F[15], F[17] );
	if ( wwLookdevRed( "fogpower1" ) )
		o.power = 1.0f;
	if ( wwLookdevRed( "fognear0" ) )
		o.fogNear = 0.0f;

	// the colours: CIELab on the engine clock's keys, x NAM4 on the same keys, then pow 2.2
	o.keys = wwSkyClock( hour, tnam, g ).keys;
	const int rows[4] = { WwRowFogNear, WwRowFogFar, WwRowFogNearHigh, WwRowFogFarHigh };
	float * dst[4] = { o.nearLow, o.farLow, o.nearHigh, o.farHigh };
	const bool noNam4 = wwLookdevRed( "fognonam4" ), noGamma = wwLookdevRed( "fognogamma" );
	for ( int k = 0; k < 4; k++ ) {
		float rgb[3];
		wwLabBlend( w.color[o.keys.a][rows[k]], w.color[o.keys.b][rows[k]], o.keys.t, rgb );
		o.scale[k] = noNam4 ? 1.0f : w.fogScale[k][o.keys.a] * ( 1.0f - o.keys.t ) + w.fogScale[k][o.keys.b] * o.keys.t;
		for ( int c = 0; c < 3; c++ ) {
			const float v = std::max( 0.0f, rgb[c] * o.scale[k] );
			dst[k][c] = noGamma ? v : std::pow( v, 2.2f );
		}
	}

	// cb12[41..46]
	float fn = o.fogNear, ff = o.fogFar;
	if ( fn == 0.0f && ff == 0.0f ) {
		fn = 1e8f;
		ff = 1e9f;
		o.effOff = true;
	}
	// guards the engine does not need for vanilla data (a zero span would divide by zero)
	const float span = ( ff - fn ) != 0.0f ? ( ff - fn ) : 1.0f;
	const float nR = o.nRange != 0.0f ? o.nRange : 1.0f;
	const float fR = o.fRange != 0.0f ? o.fRange : 1.0f;
	const float K[6][4] = {
		{ 1.0f / span, 1.0f / ( 2.0f * nR ), fn / span, ( o.nMid - nR ) / ( 2.0f * nR ) },
		{ o.nearLow[0], o.nearLow[1], o.nearLow[2], o.power },
		{ o.nearHigh[0], o.nearHigh[1], o.nearHigh[2], o.maxv },
		{ o.farLow[0], o.farLow[1], o.farLow[2], o.hds },
		{ o.farHigh[0], o.farHigh[1], o.farHigh[2], 0.0f },
		{ 1.0f / ( 2.0f * nR ), 1.0f / ( 2.0f * fR ), ( o.nMid - nR ) / ( 2.0f * nR ), ( o.fMid - fR ) / ( 2.0f * fR ) },
	};
	std::memcpy( o.K, K, sizeof( K ) );
	return o;
}

WwFogSample wwFogSample( const WwFog & fog, float d, float z )
{
	WwFogSample s;
	const auto & K = fog.K;
	auto sat = []( float v ) { return std::clamp( v, 0.0f, 1.0f ); };
	s.ramp = d * K[0][0] - K[0][2];
	s.f = sat( s.ramp );
	const float hN = sat( z * K[5][0] - K[5][2] );
	const float hF = sat( z * K[5][1] - K[5][3] );
	s.hb = hN + ( hF - hN ) * s.f;
	const float mx = K[2][3];
	const float clampT = ( s.ramp > 0.75f && !wwLookdevRed( "fogmaxclamp" ) )
		? std::min( ( s.f - 0.75f ) * 4.0f * ( 1.0f - mx ) + mx, 1.0f ) : mx;
	const float esc = ( s.ramp < 0.015f && !wwLookdevRed( "fognoescape" ) ) ? s.f * 66.666672f : 1.0f;
	s.intensity = s.f > 0.0f ? std::min( clampT, std::pow( s.f, K[1][3] ) ) : 0.0f;
	const float weight = s.hb * K[3][3] + ( 1.0f - s.hb );
	s.alpha = weight * s.intensity * esc;
	for ( int c = 0; c < 3; c++ ) {
		const float lo = K[1][c] + ( K[3][c] - K[1][c] ) * s.intensity;
		const float hi = K[2][c] + ( K[4][c] - K[2][c] ) * s.intensity;
		s.color[c] = lo + ( hi - lo ) * s.hb;
	}
	return s;
}

QString WwFog::describe() const
{
	auto c3 = []( const float v[3] ) {
		return QString( "%1,%2,%3" ).arg( double( v[0] ), 0, 'f', 5 ).arg( double( v[1] ), 0, 'f', 5 ).arg( double( v[2] ), 0, 'f', 5 );
	};
	return QString( "w=%1 ext=%2 near=%3 far=%4 power=%5 max=%6 hds=%7 nmid=%8 nrange=%9 fmid=%10 frange=%11 keys=%12,%13,%14 "
		"scale=%15 nearlow=%16 farlow=%17 nearhigh=%18 farhigh=%19 effoff=%20" )
		.arg( double( w ), 0, 'f', 4 ).arg( double( ext ), 0, 'f', 3 ).arg( double( fogNear ), 0, 'f', 1 ).arg( double( fogFar ), 0, 'f', 1 )
		.arg( double( power ), 0, 'f', 4 ).arg( double( maxv ), 0, 'f', 4 ).arg( double( hds ), 0, 'f', 4 )
		.arg( double( nMid ), 0, 'f', 1 ).arg( double( nRange ), 0, 'f', 1 ).arg( double( fMid ), 0, 'f', 1 ).arg( double( fRange ), 0, 'f', 1 )
		.arg( wwTodName( keys.a ), wwTodName( keys.b ) ).arg( double( keys.t ), 0, 'f', 6 )
		.arg( QString( "%1,%2,%3,%4" ).arg( double( scale[0] ), 0, 'f', 4 ).arg( double( scale[1] ), 0, 'f', 4 )
			.arg( double( scale[2] ), 0, 'f', 4 ).arg( double( scale[3] ), 0, 'f', 4 ) )
		.arg( c3( nearLow ), c3( farLow ), c3( nearHigh ), c3( farHigh ) )
		.arg( effOff ? 1 : 0 );
}

/* ------------------------------------------------------------------------
 * `NifSkope.exe -no-gui weather` -- the W1 gates' reader. One verdict line per
 * fact, machine-parsed by tests/spells/pbr_r2b_gates.py.
 *
 *   --plugins a,b,c   load list (full paths; bare names resolve in --data)
 *   --plugin p        ONE plugin: its masters are added from beside it / --data
 *   --data dir        the Data folder (default: the game manager's Fallout 4)
 *   --weather key     EDID or FormID: prints `weather` + `nam0hex` + `row` + `dalc`
 *   --hour h[,h..]    prints `tod` + `sun` for each hour (climate --climate or 0000015F)
 *   --tnam a,b,c,d    pure blend: no plugin needed
 *   --census          every WTHR: parse verdict + the NAM0/DALC histograms
 *   --list            every WTHR, one line each
 *   --sky             (with --weather + --hour) the engine clock (lane PBRWX1): `gmst`, `climate2`, and per
 *                     hour `skyclock`, `skycolor`, `cloud` (layers 0..15) and `moon` (per --day)
 *   --day d[,d..]     game days for the `moon` lines (default 0)
 *   --cloudtime s     real seconds for the `cloud` offsets (default 0)
 *   --fog             (with --weather + --hour) the engine fog (lane FOG1): `fogrec`, and per hour `fog`
 *                     (the blended state + the linear colours) and `fogk` (cb12[41..46])
 *   --fog-probe d,z[;d,z..]  implies --fog: per hour a `fogprobe` line per fragment (eye distance d,
 *                     world height z): ramp, f, hb, intensity, alpha, the fog colour before the sun term
 * ---------------------------------------------------------------------- */
int cmdWeather( const QStringList & args )
{
	auto say = []( const QString & s ) {
		const QByteArray b = s.toUtf8();
		std::fwrite( b.constData(), 1, size_t( b.size() ), stdout );
		std::fputc( '\n', stdout );
		std::fflush( stdout );
	};
	QString plugins, plugin, dataDir, weather, hours, tnamArg, climateArg, daysArg;
	QString fogProbeArg;
	bool census = false, doList = false, sky = false, fog = false;
	double cloudTime = 0.0;
	for ( int i = 0; i < args.size(); i++ ) {
		const QString & a = args.at( i );
		auto next = [&]() { return i + 1 < args.size() ? args.at( ++i ) : QString(); };
		if ( a == "--plugins" )
			plugins = next();
		else if ( a == "--plugin" )
			plugin = next();
		else if ( a == "--data" )
			dataDir = next();
		else if ( a == "--weather" )
			weather = next();
		else if ( a == "--hour" )
			hours = next();
		else if ( a == "--tnam" )
			tnamArg = next();
		else if ( a == "--climate" )
			climateArg = next();
		else if ( a == "--census" )
			census = true;
		else if ( a == "--list" )
			doList = true;
		else if ( a == "--sky" )
			sky = true;
		else if ( a == "--day" )
			daysArg = next();
		else if ( a == "--cloudtime" )
			cloudTime = next().toDouble();
		else if ( a == "--fog" )
			fog = true;
		else if ( a == "--fog-probe" ) {
			fog = true;
			fogProbeArg = next();
		}
		else {
			say( QString( "error unknown argument %1" ).arg( a ) );
			return 2;
		}
	}
	if ( dataDir.isEmpty() )
		dataDir = EsmWeather::gameDataDir();
	const QByteArray red = qgetenv( "WW_LOOKDEV_RED" );
	say( QString( "# WW_WEATHER data=%1 red=%2" ).arg( dataDir.isEmpty() ? "none" : dataDir,
		red.isEmpty() ? "none" : QString::fromLatin1( red ) ) );

	auto hourList = [&]() {
		QVector<double> hs;
		for ( const QString & s : hours.split( QChar( ',' ), Qt::SkipEmptyParts ) ) {
			// "6.75" or "21:30"
			if ( s.contains( QChar( ':' ) ) ) {
				const QStringList hm = s.split( QChar( ':' ) );
				hs << hm.value( 0 ).toDouble() + hm.value( 1 ).toDouble() / 60.0;
			} else {
				hs << s.toDouble();
			}
		}
		return hs;
	};
	auto todLine = [&]( double h, const unsigned char t[4] ) {
		const WwTodKeys k = wwTodKeys( h, t );
		float d[3], p[3];
		wwVanillaSunPos( h, t, p );
		wwVanillaSunLightDir( h, t, d );
		say( QString( "tod hour=%1 a=%2 b=%3 t=%4 keys=%5" ).arg( h, 0, 'f', 4 )
			.arg( wwTodName( k.a ), wwTodName( k.b ) ).arg( double( k.t ), 0, 'f', 4 ).arg( k.describe() ) );
		say( QString( "sun hour=%1 disc=%2,%3,%4 light=%5,%6,%7" ).arg( h, 0, 'f', 4 )
			.arg( double( p[0] ), 0, 'f', 3 ).arg( double( p[1] ), 0, 'f', 3 ).arg( double( p[2] ), 0, 'f', 3 )
			.arg( double( d[0] ), 0, 'f', 5 ).arg( double( d[1] ), 0, 'f', 5 ).arg( double( d[2] ), 0, 'f', 5 ) );
	};

	if ( !tnamArg.isEmpty() ) {
		const QStringList tp = tnamArg.split( QChar( ',' ) );
		unsigned char t[4] = { 0, 0, 0, 0 };
		for ( int i = 0; i < 4 && i < tp.size(); i++ )
			t[i] = (unsigned char) tp.at( i ).toUInt();
		for ( double h : hourList() )
			todLine( h, t );
		if ( plugins.isEmpty() && plugin.isEmpty() )
			return 0;
	}

	QStringList files;
	if ( !plugin.isEmpty() ) {
		QString p = plugin;
		if ( !QFileInfo::exists( p ) && !dataDir.isEmpty() )
			p = QDir( dataDir ).filePath( plugin );
		files = EsmWeather::loadListFor( p, dataDir );
	} else {
		for ( const QString & s : plugins.split( QChar( ',' ), Qt::SkipEmptyParts ) ) {
			QString p = s.trimmed();
			if ( !QFileInfo::exists( p ) && !dataDir.isEmpty() && QFileInfo::exists( QDir( dataDir ).filePath( p ) ) )
				p = QDir( dataDir ).filePath( p );
			files << p;
		}
	}
	if ( files.isEmpty() ) {
		say( "error no plugins" );
		return 2;
	}
	EsmWeather ew;
	QString refusal;
	if ( !ew.load( files, &refusal ) ) {
		say( QString( "load refused reason=\"%1\" records=0" ).arg( refusal ) );
		return 3;
	}
	const QVector<WwWeatherEntry> all = ew.list();
	say( QString( "load ok files=%1 wthr=%2" ).arg( ew.loadSummary() ).arg( all.size() ) );

	if ( doList )
		for ( const WwWeatherEntry & e : all )
			say( QString( "entry id=%1 edid=%2 src=%3 owner=%4 override=%5" ).arg( hex8( e.formID ), e.edid,
				e.srcFile, e.ownerFile ).arg( e.override() ? 1 : 0 ) );

	if ( census ) {
		QMap<int, int> nam0Hist, dalcHist;
		int ok = 0, bad = 0;
		for ( const WwWeatherEntry & e : all ) {
			WwWeatherData w;
			QString why;
			if ( ew.read( e.formID, w, &why ) ) {
				ok++;
				nam0Hist[w.nam0Size]++;
				dalcHist[w.dalcCount]++;
			} else {
				bad++;
				say( QString( "census refused id=%1 reason=\"%2\"" ).arg( hex8( e.formID ), why ) );
			}
		}
		auto fmt = []( const QMap<int, int> & m ) {
			QStringList s;
			for ( auto it = m.cbegin(); it != m.cend(); ++it )
				s << QString( "%1:%2" ).arg( it.key() ).arg( it.value() );
			return s.join( QChar( ',' ) );
		};
		say( QString( "census wthr=%1 parsed=%2 refused=%3 nam0=%4 dalc=%5" ).arg( all.size() ).arg( ok ).arg( bad )
			.arg( fmt( nam0Hist ), fmt( dalcHist ) ) );
	}

	unsigned char tnam[4] = { 30, 54, 102, 126 };
	{
		quint32 cid = 0;
		if ( !climateArg.isEmpty() )
			cid = climateArg.startsWith( "0x", Qt::CaseInsensitive ) ? climateArg.mid( 2 ).toUInt( nullptr, 16 )
			                                                         : climateArg.toUInt( nullptr, 16 );
		QString cedid, why;
		if ( ew.climate( cid, tnam, &cedid, &why ) )
			say( QString( "climate edid=%1 tnam=%2,%3,%4,%5" ).arg( cedid ).arg( tnam[0] ).arg( tnam[1] ).arg( tnam[2] ).arg( tnam[3] ) );
		else
			say( QString( "climate fallback reason=\"%1\" tnam=30,54,102,126" ).arg( why ) );
	}

	if ( !weather.isEmpty() ) {
		const quint32 id = ew.find( weather );
		WwWeatherData w;
		QString why;
		if ( !id || !ew.read( id, w, &why ) ) {
			say( QString( "weather refused key=%1 reason=\"%2\"" ).arg( weather, id ? why : QString( "not found" ) ) );
			return 4;
		}
		QString owner;
		for ( const WwWeatherEntry & e : all )
			if ( e.formID == id )
				owner = e.ownerFile;
		say( QString( "weather id=%1 edid=%2 src=%3 owner=%4 fv=%5 nam0=%6 rows=%7 tods=%8 dalc=%9" )
			.arg( hex8( id ), w.edid, w.srcFile, owner ).arg( w.formVersion ).arg( w.nam0Size )
			.arg( w.nam0Rows ).arg( w.nam0Tods ).arg( w.dalcCount ) );
		say( QString( "nam0hex %1" ).arg( QString::fromLatin1( w.nam0.toHex() ) ) );
		static const char * const rowNames[19] = { "SkyUpper", "FogNear", "Unused", "Ambient", "Sunlight", "Sun",
			"Stars", "SkyLower", "Horizon", "EffectLighting", "CloudLODDiffuse", "CloudLODAmbient", "FogFar",
			"SkyStatics", "WaterMult", "SunGlare", "MoonGlare", "FogNearHigh", "FogFarHigh" };
		for ( int tod = 0; tod < 8; tod++ )
			for ( int row = 0; row < w.nam0Rows; row++ )
				say( QString( "row name=%1 tod=%2 rgb=%3,%4,%5" ).arg( rowNames[row], wwTodName( tod ) )
					.arg( w.color[tod][row][0] ).arg( w.color[tod][row][1] ).arg( w.color[tod][row][2] ) );
		static const char * const ax[7] = { "X+", "X-", "Y+", "Y-", "Z+", "Z-", "Spec" };
		for ( int tod = 0; tod < 8 && w.hasDalc; tod++ )
			for ( int a = 0; a < 7; a++ )
				say( QString( "dalc tod=%1 axis=%2 rgb=%3,%4,%5" ).arg( wwTodName( tod ), ax[a] )
					.arg( w.dalc[tod][a][0] ).arg( w.dalc[tod][a][1] ).arg( w.dalc[tod][a][2] ) );
		for ( double h : hourList() ) {
			todLine( h, tnam );
			const WwTodKeys k = wwTodKeys( h, tnam );
			float sl[3], am[3], zm[3];
			EsmWeather::blendRow( w, WwRowSunlight, k, sl );
			EsmWeather::blendRow( w, WwRowAmbient, k, am );
			EsmWeather::blendDalc( w, 5, k, zm );
			say( QString( "blend hour=%1 sunlight=%2,%3,%4 ambient=%5,%6,%7 dalcZm=%8,%9,%10" ).arg( h, 0, 'f', 4 )
				.arg( double( sl[0] ), 0, 'f', 2 ).arg( double( sl[1] ), 0, 'f', 2 ).arg( double( sl[2] ), 0, 'f', 2 )
				.arg( double( am[0] ), 0, 'f', 2 ).arg( double( am[1] ), 0, 'f', 2 ).arg( double( am[2] ), 0, 'f', 2 )
				.arg( double( zm[0] ), 0, 'f', 2 ).arg( double( zm[1] ), 0, 'f', 2 ).arg( double( zm[2] ), 0, 'f', 2 ) );
		}
		if ( fog ) {
			// lane FOG1: the record as read, then the engine fog per hour and the probed fragments
			const WwSkyGmst g = ew.gmst();
			QStringList fl, sl;
			for ( int i = 0; i < 18; i++ )
				fl << QString::number( double( w.fog[i] ), 'g', 7 );
			for ( int k = 0; k < 4; k++ )
				for ( int t = 0; t < 8; t++ )
					sl << QString::number( double( w.fogScale[k][t] ), 'g', 7 );
			say( QString( "fogrec fnam=%1 nam4=%2 dirfogpower=%3 colorext=%4 fog=%5 scale=%6" ).arg( w.fogFnamSize )
				.arg( w.hasNam4 ? 1 : 0 ).arg( double( g.dirFogPower ), 0, 'g', 7 ).arg( double( g.colorExt ), 0, 'g', 7 )
				.arg( fl.join( QChar( ',' ) ), sl.join( QChar( ',' ) ) ) );
			QVector<QPair<float, float>> probes;
			for ( const QString & p : fogProbeArg.split( QChar( ';' ), Qt::SkipEmptyParts ) ) {
				const QStringList dz = p.split( QChar( ',' ) );
				if ( dz.size() == 2 )
					probes << qMakePair( dz.at( 0 ).toFloat(), dz.at( 1 ).toFloat() );
			}
			for ( double hr : hourList() ) {
				const WwFog f = wwFogAt( w, hr, tnam, g );
				say( QString( "fog hour=%1 %2" ).arg( hr, 0, 'f', 4 ).arg( f.describe() ) );
				for ( int r = 0; r < 6; r++ )
					say( QString( "fogk hour=%1 row=%2 v=%3,%4,%5,%6" ).arg( hr, 0, 'f', 4 ).arg( 41 + r )
						.arg( double( f.K[r][0] ), 0, 'g', 9 ).arg( double( f.K[r][1] ), 0, 'g', 9 )
						.arg( double( f.K[r][2] ), 0, 'g', 9 ).arg( double( f.K[r][3] ), 0, 'g', 9 ) );
				for ( const auto & p : probes ) {
					const WwFogSample s = wwFogSample( f, p.first, p.second );
					say( QString( "fogprobe hour=%1 d=%2 z=%3 ramp=%4 f=%5 hb=%6 intensity=%7 alpha=%8 color=%9,%10,%11" )
						.arg( hr, 0, 'f', 4 ).arg( double( p.first ), 0, 'f', 1 ).arg( double( p.second ), 0, 'f', 1 )
						.arg( double( s.ramp ), 0, 'f', 6 ).arg( double( s.f ), 0, 'f', 6 ).arg( double( s.hb ), 0, 'f', 6 )
						.arg( double( s.intensity ), 0, 'f', 6 ).arg( double( s.alpha ), 0, 'f', 6 )
						.arg( double( s.color[0] ), 0, 'f', 6 ).arg( double( s.color[1] ), 0, 'f', 6 ).arg( double( s.color[2] ), 0, 'f', 6 ) );
				}
			}
		}
		if ( sky ) {
			const WwSkyGmst g = ew.gmst();
			say( QString( "gmst %1" ).arg( g.describe() ) );
			WwClimateData cd;
			{
				quint32 cid = 0;
				if ( !climateArg.isEmpty() )
					cid = climateArg.startsWith( "0x", Qt::CaseInsensitive ) ? climateArg.mid( 2 ).toUInt( nullptr, 16 )
					                                                         : climateArg.toUInt( nullptr, 16 );
				QString why;
				if ( !ew.climateData( cid, cd, &why ) )
					say( QString( "climate2 fallback reason=\"%1\"" ).arg( why ) );
			}
			say( QString( "climate2 edid=%1 tnam=%2,%3,%4,%5,%6,%7 moons=0x%8 sunTex=%9 glareTex=%10" ).arg( cd.edid )
				.arg( cd.tnam[0] ).arg( cd.tnam[1] ).arg( cd.tnam[2] ).arg( cd.tnam[3] ).arg( cd.tnam[4] ).arg( cd.tnam[5] )
				.arg( cd.tnam[5], 2, 16, QChar( '0' ) ).arg( cd.sunTex, cd.glareTex ) );
			say( QString( "sky imsp=%1 skyscale=%2 found=%3 sunglare=%4 lnam=%5 nam1=0x%6 disabled=0x%7" )
				.arg( [&]() { QStringList l; for ( int i = 0; i < 8; i++ ) l << hex8( w.imsp[i] ); return l.join( ',' ); }() )
				.arg( [&]() { QStringList l; for ( int i = 0; i < 8; i++ ) l << QString::number( double( w.skyScale[i] ), 'g', 7 ); return l.join( ',' ); }() )
				.arg( w.skyScaleFound ).arg( w.sunGlare ).arg( w.cloudLayers )
				.arg( hex8( w.cloudNam1 ), hex8( w.cloudDisabled ) ) );
			QVector<double> days;
			for ( const QString & d : daysArg.split( QChar( ',' ), Qt::SkipEmptyParts ) )
				days << d.toDouble();
			if ( days.isEmpty() )
				days << 0.0;
			auto c3 = []( const float v[3] ) {
				return QString( "%1,%2,%3" ).arg( double( v[0] ), 0, 'f', 3 ).arg( double( v[1] ), 0, 'f', 3 ).arg( double( v[2] ), 0, 'f', 3 );
			};
			for ( double hr : hourList() ) {
				const WwSkyClock c = wwSkyClock( hr, cd.tnam, g );
				double el, az;
				wwSkyAngles( c.sunPos, &el, &az );
				say( QString( "skyclock hour=%1 keys=%2,%3,%4 A=%5 B=%6 C=%7 D=%8 sunpos=%9 light=%10 sunalpha=%11 starsalpha=%12 elev=%13 az=%14" )
					.arg( hr, 0, 'f', 4 ).arg( wwTodName( c.keys.a ), wwTodName( c.keys.b ) ).arg( double( c.keys.t ), 0, 'f', 6 )
					.arg( c.A, 0, 'f', 6 ).arg( c.B, 0, 'f', 6 ).arg( c.C, 0, 'f', 6 ).arg( c.D, 0, 'f', 6 )
					.arg( c3( c.sunPos ) )
					.arg( QString( "%1,%2,%3" ).arg( double( c.lightDir[0] ), 0, 'f', 6 ).arg( double( c.lightDir[1] ), 0, 'f', 6 ).arg( double( c.lightDir[2] ), 0, 'f', 6 ) )
					.arg( double( c.sunAlpha ), 0, 'f', 6 ).arg( double( c.starsAlpha ), 0, 'f', 6 )
					.arg( el, 0, 'f', 4 ).arg( az, 0, 'f', 4 ) );
				float up[3], lo[3], hz[3], su[3], gl[3], mg[3], st[3], sl[3], am[3];
				EsmWeather::blendRowLab( w, WwRowSkyUpper, c.keys, up );
				EsmWeather::blendRowLab( w, WwRowSkyLower, c.keys, lo );
				EsmWeather::blendRowLab( w, WwRowHorizon, c.keys, hz );
				EsmWeather::blendRowLab( w, WwRowSun, c.keys, su );
				EsmWeather::blendRowLab( w, WwRowSunGlare, c.keys, gl );
				EsmWeather::blendRowLab( w, WwRowMoonGlare, c.keys, mg );
				EsmWeather::blendRowLab( w, WwRowStars, c.keys, st );
				EsmWeather::blendRowLab( w, WwRowSunlight, c.keys, sl );
				EsmWeather::blendRowLab( w, WwRowAmbient, c.keys, am );
				say( QString( "skycolor hour=%1 upper=%2 lower=%3 horizon=%4 sun=%5 glare=%6 moonglare=%7 stars=%8 sunlight=%9 ambient=%10 skyscale=%11 glarealpha=%12" )
					.arg( hr, 0, 'f', 4 ).arg( c3( up ), c3( lo ), c3( hz ), c3( su ), c3( gl ), c3( mg ), c3( st ), c3( sl ) ).arg( c3( am ) )
					.arg( double( EsmWeather::blendSkyScale( w, c.keys ) ), 0, 'f', 6 )
					.arg( double( c.sunAlpha * float( w.sunGlare ) / 255.0f ), 0, 'f', 6 ) );
				for ( int i = 0; i < 16; i++ ) {
					float rgb[3], al = 0.0f;
					EsmWeather::blendCloud( w, i, c.keys, rgb, &al );
					const quint32 li = quint32( i ) >= w.cloudLayers ? 0u : quint32( i );
					const float sx = EsmWeather::cloudSpeed( w.cloudSpeedX[li], g );
					const float sy = EsmWeather::cloudSpeed( w.cloudSpeedY[li], g );
					const bool drawn = !( w.cloudDisabled & ( 1u << i ) );
					say( QString( "cloud hour=%1 layer=%2 tex=%3 drawn=%4 rgb=%5 alpha=%6 speedx=%7 speedy=%8 offx=%9 offy=%10" )
						.arg( hr, 0, 'f', 4 ).arg( i ).arg( w.cloudTex[i].isEmpty() ? QString( "-" ) : QString( w.cloudTex[i] ).replace( QChar( ' ' ), QChar( '?' ) ) )
						.arg( drawn ? 1 : 0 ).arg( c3( rgb ) ).arg( double( al ), 0, 'f', 6 )
						.arg( double( sx ), 0, 'f', 8 ).arg( double( sy ), 0, 'f', 8 )
						.arg( double( wwCloudOffset( sx, cloudTime ) ), 0, 'f', 6 ).arg( double( wwCloudOffset( sy, cloudTime ) ), 0, 'f', 6 ) );
				}
				const float ma[2] = { wwMoonAlpha( c, g, g.secundaFadeStart, g.secundaFadeEnd ),
					wwMoonAlpha( c, g, g.masserFadeStart, g.masserFadeEnd ) };
				for ( double d : days ) {
					const int ph = wwMoonPhase( d, cd.tnam[5] );
					for ( int m = 0; m < 2; m++ ) {
						const bool on = m == 0 ? ( cd.tnam[5] & 0x40 ) : ( cd.tnam[5] & 0x80 );
						if ( !on )
							continue;
						const float size = m == 0 ? g.secundaSize : g.masserSize;
						const double half = std::atan( double( size ) / std::max( 1e-6, std::fabs( double( g.sunX ) ) ) ) * 180.0 / 3.14159265358979323846;
						say( QString( "moon hour=%1 day=%2 which=%3 phase=%4 suffix=%5 tex=textures/sky/%3_%5.dds alpha=%6 shadowalpha=%7 halfangle=%8 elev=%9 az=%10" )
							.arg( hr, 0, 'f', 4 ).arg( d, 0, 'f', 3 ).arg( m == 0 ? "secunda" : "masser" ).arg( ph )
							.arg( wwMoonPhaseSuffix( ph ) ).arg( double( ma[m] ), 0, 'f', 6 )
							.arg( double( std::min( ma[m], c.starsAlpha ) ), 0, 'f', 6 ).arg( half, 0, 'f', 4 )
							.arg( el, 0, 'f', 4 ).arg( az, 0, 'f', 4 ) );
					}
				}
			}
		}
	} else if ( !hours.isEmpty() && tnamArg.isEmpty() ) {
		for ( double h : hourList() )
			todLine( h, tnam );
	}
	return 0;
}

#include "weatherrecx.h"
#include "wwswitch.h"
#include "gl/celllooks.h"
#include "gl/lookdevstage.h"
#include "features/weather/weathergen.h"
#include "features/weather/wxoverride.h"
#include "features/weather/weatherrecxmath.h"
#include "features/weather/recxphysical.h"	// lane AIRPERF1 A8: the entry's physicalWeather switch
#include "features/clouds/cloudrecx14math.h"
#include "recx.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QPair>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QVector>

#include <cstdio>
#include <cstring>

using namespace WwWeatherRecxMath;

namespace
{

QString redWord()
{
	return wwEnvStr( "WW_WEATHER_RECX_RED" ).trimmed().toLower();
}

//! lane AIRPERF1 A9: the family's red word (noswitch = every family, noswitch-<family>; a comma list), read live
bool switchRed( const char * family )
{
	const QString w = redWord();
	if ( !w.contains( QLatin1String( "noswitch" ) ) )
		return false;
	const QString one = QStringLiteral( "noswitch-" ) + QLatin1String( family );
	for ( const QString & p : w.split( QLatin1Char( ',' ) ) ) {
		const QString t = p.trimmed();
		if ( t == QLatin1String( "noswitch" ) || t == one )
			return true;
	}
	return false;
}

void sayOnce( const QString & msg )
{
	static QSet<QString> said;
	if ( said.contains( msg ) )
		return;
	said.insert( msg );
	std::fprintf( stderr, "%s\n", qPrintable( msg ) );
}

int editDistance( const QString & a, const QString & b )
{
	std::vector<int> row( size_t( b.size() ) + 1 );
	for ( int j = 0; j <= b.size(); j++ )
		row[size_t( j )] = j;
	for ( int i = 1; i <= a.size(); i++ ) {
		int prev = row[0];
		row[0] = i;
		for ( int j = 1; j <= b.size(); j++ ) {
			const int cur = std::min( { row[size_t( j )] + 1, row[size_t( j ) - 1] + 1, prev + ( a[i - 1] != b[j - 1] ? 1 : 0 ) } );
			prev = row[size_t( j )];
			row[size_t( j )] = cur;
		}
	}
	return row[size_t( b.size() )];
}

//! one block (or hour, or layer) object: keys case-free; unknown keys warned with a did-you-mean
struct Obj
{
	QJsonObject o;
	QString where;	// "<file> /atmosphere" for the warnings

	QJsonValue get( const char * name ) const
	{
		const QString n = QLatin1String( name );
		for ( auto it = o.constBegin(); it != o.constEnd(); ++it )
			if ( it.key().compare( n, Qt::CaseInsensitive ) == 0 )
				return it.value();
		return QJsonValue( QJsonValue::Undefined );
	}

	void unknownKeys( const QStringList & known ) const
	{
		for ( auto it = o.constBegin(); it != o.constEnd(); ++it ) {
			const QString k = it.key();
			if ( k.startsWith( QLatin1Char( '_' ) ) || k.compare( QLatin1String( "comment" ), Qt::CaseInsensitive ) == 0 )
				continue;
			bool ok = false;
			for ( const QString & n : known )
				ok = ok || k.compare( n, Qt::CaseInsensitive ) == 0;
			if ( ok )
				continue;
			QString best;
			int bd = 3;
			for ( const QString & n : known ) {
				const int d = editDistance( k.toLower(), n.toLower() );
				if ( d < bd ) {
					bd = d;
					best = n;
				}
			}
			sayOnce( QStringLiteral( "weather recx: %1: unknown key \"%2\"%3" ).arg( where, k,
				best.isEmpty() ? QString() : QStringLiteral( " - did you mean \"%1\"?" ).arg( best ) ) );
		}
	}

	//! a number in lo..hi (clamped, warned); false when absent or not a number (warned)
	bool num( const char * name, float lo, float hi, double & out ) const
	{
		const QJsonValue v = get( name );
		if ( v.isUndefined() )
			return false;
		if ( !v.isDouble() || !std::isfinite( v.toDouble() ) ) {
			sayOnce( QStringLiteral( "weather recx: %1: %2 dropped (not-number)" ).arg( where, QLatin1String( name ) ) );
			return false;
		}
		out = v.toDouble();
		if ( out < lo || out > hi ) {
			sayOnce( QStringLiteral( "weather recx: %1: %2 %3 clamped to %4..%5" ).arg( where, QLatin1String( name ) )
				.arg( out ).arg( double( lo ) ).arg( double( hi ) ) );
			out = std::clamp( out, double( lo ), double( hi ) );
		}
		return true;
	}

	Obj child( const QJsonValue & v, const QString & key ) const
	{
		return Obj{ v.toObject(), where + QLatin1Char( '/' ) + key };
	}
};

struct Key
{
	const char * name;
	float lo, hi;
	float WwWeatherRecx::* field;
};

const Key kAtmo[] = { { "visibility", kVisMin, kVisMax, &WwWeatherRecx::visibilityM },
	{ "hazeHeight", kHazeHMin, kHazeHMax, &WwWeatherRecx::hazeHeightM }, { "ozone", 0.0f, kOzoneMax, &WwWeatherRecx::ozone },
	// lane WEATHERRECX1: the 1.3 keys
	{ "humidity", 0.0f, 100.0f, &WwWeatherRecx::humidity }, { "groundAlbedo", 0.0f, 1.0f, &WwWeatherRecx::groundAlbedo },
	{ "angstrom", 0.0f, 3.0f, &WwWeatherRecx::angstrom }, { "hazeAnisotropy", 0.0f, 0.95f, &WwWeatherRecx::hazeAnisotropy },
	{ "airglow", 0.0f, 10.0f, &WwWeatherRecx::airglow } };
const Key kClouds[] = { { "coverage", 0.0f, 1.0f, &WwWeatherRecx::coverage },
	{ "density", 0.0f, kDensityMax, &WwWeatherRecx::density }, { "type", 0.0f, 1.0f, &WwWeatherRecx::type } };
// lane CLOUDRECX1: the 1.4 clouds keys (SPEC.md 1.4; src/features/clouds/cloudrecx14math.h ranges): kClouds and five more
const Key kClouds14[] = { { "coverage", 0.0f, 1.0f, &WwWeatherRecx::coverage },
	{ "density", 0.0f, kDensityMax, &WwWeatherRecx::density }, { "type", 0.0f, 1.0f, &WwWeatherRecx::type },
	{ "liquidWater", 0.0f, WwCloudRecx14::kLwcMax, &WwWeatherRecx::liquidWater },
	{ "dropletRadius", WwCloudRecx14::kRadiusMin, WwCloudRecx14::kRadiusMax, &WwWeatherRecx::dropletRadius },
	{ "iceFraction", 0.0f, 1.0f, &WwWeatherRecx::iceFraction },
	{ "evolution", WwCloudRecx14::kEvolutionMin, WwCloudRecx14::kEvolutionMax, &WwWeatherRecx::evolution },
	{ "precipitationBase", 0.0f, WwCloudRecx14::kPrecipBaseMax, &WwWeatherRecx::precipitationBase },
	// MODULES1 10c: the cloud shadow law's inputs (weatherrecx.h)
	{ "shadowOpacity", 0.0f, 1.0f, &WwWeatherRecx::shadowOpacity }, { "shadowStrength", 0.0f, 8.0f, &WwWeatherRecx::shadowStrength },
	{ "shadowLayerDensity", 25.0f, 400.0f, &WwWeatherRecx::shadowLayerDensity },
	{ "shadowDeck", 200.0f, 20000.0f, &WwWeatherRecx::shadowDeck } };
// lane WEATHERRECX1: the 1.3 blocks (SPEC.md ranges)
const Key kWind[] = { { "speed", 0.0f, 60.0f, &WwWeatherRecx::windSpeed },
	{ "direction", 0.0f, 360.0f, &WwWeatherRecx::windDirection }, { "gusts", 0.0f, 80.0f, &WwWeatherRecx::gusts },
	{ "turbulence", 0.0f, 1.0f, &WwWeatherRecx::turbulence } };
const Key kTemp[] = { { "air", -60.0f, 60.0f, &WwWeatherRecx::airTemp } };
const Key kLightning[] = { { "frequency", 0.0f, 120.0f, &WwWeatherRecx::lightningFreq },
	{ "cloudToGround", 0.0f, 1.0f, &WwWeatherRecx::cloudToGround }, { "distance", 0.3f, 50.0f, &WwWeatherRecx::lightningDistance } };
const Key kTrans[] = { { "duration", 0.01f, 24.0f, &WwWeatherRecx::transHours },
	{ "precipitationStart", 0.0f, 1.0f, &WwWeatherRecx::precipStart }, { "precipitationEnd", 0.0f, 1.0f, &WwWeatherRecx::precipEnd },
	{ "lightningStart", 0.0f, 1.0f, &WwWeatherRecx::lightningStart }, { "lightningEnd", 0.0f, 1.0f, &WwWeatherRecx::lightningEnd } };
const Key kPrecip[] = { { "wetting", 0.0f, 600.0f, &WwWeatherRecx::wetting }, { "drying", 0.0f, 2880.0f, &WwWeatherRecx::drying } };

/* lane CLOUD5: clouds.type may be a word (the draft's modder words, notes/atmores1/RESEARCH.md sec. 7.2): the block and
 * each hours/<h> object get the word replaced by its number before the keys are read; an unknown word is dropped */
QJsonObject typeWords( QJsonObject o, const QString & where )
{
	static const struct { const char * word; double v; } kWords[] = { { "stratus", 0.0 }, { "stratocumulus", 1.0 / 3.0 },
		{ "cumulus", 2.0 / 3.0 }, { "cumulonimbus", 1.0 } };
	for ( auto it = o.begin(); it != o.end(); ++it ) {
		if ( it.key().compare( QLatin1String( "type" ), Qt::CaseInsensitive ) != 0 || !it.value().isString() )
			continue;
		const QString w = it.value().toString().trimmed();
		bool found = false;
		for ( const auto & k : kWords )
			if ( w.compare( QLatin1String( k.word ), Qt::CaseInsensitive ) == 0 ) {
				it.value() = k.v;
				found = true;
			}
		if ( !found ) {
			sayOnce( QStringLiteral( "weather recx: %1: type \"%2\" dropped (not-type: stratus, stratocumulus, cumulus, "
									 "cumulonimbus or 0..1)" ).arg( where, w ) );
			o.erase( it );
			break;
		}
	}
	const QString hk = QStringLiteral( "hours" );
	for ( auto it = o.begin(); it != o.end(); ++it )
		if ( it.key().compare( hk, Qt::CaseInsensitive ) == 0 && it.value().isObject() ) {
			QJsonObject h = it.value().toObject();
			for ( auto jt = h.begin(); jt != h.end(); ++jt )
				if ( jt.value().isObject() )
					jt.value() = typeWords( jt.value().toObject(), where + QStringLiteral( "/hours/" ) + jt.key() );
			it.value() = h;
		}
	return o;
}
const Key kNight[] = { { "stars", 0.0f, kNightMax, &WwWeatherRecx::stars },
	{ "moonlight", 0.0f, kNightMax, &WwWeatherRecx::moonlight } };

//! the block's keys at the hour: its own value, else hours interpolated (an hour without the key uses the block's own)
template <size_t N>
void readKeys( const Obj & b, const Key ( &keys )[N], double hour, WwWeatherRecx & r, bool hoursOk = true )
{
	const QJsonValue hv = hoursOk ? b.get( "hours" ) : QJsonValue( QJsonValue::Undefined );
	QVector<QPair<double, Obj>> hours;
	if ( !hv.isUndefined() && !hv.isObject() )
		sayOnce( QStringLiteral( "weather recx: %1: hours dropped (not-object)" ).arg( b.where ) );
	const QJsonObject ho = hv.toObject();
	QStringList known;
	for ( const Key & k : keys )
		known << QLatin1String( k.name );
	for ( auto it = ho.constBegin(); it != ho.constEnd(); ++it ) {
		bool ok = false;
		const double h = it.key().toDouble( &ok );
		if ( !ok )
			continue;
		if ( !it.value().isObject() ) {
			sayOnce( QStringLiteral( "weather recx: %1/hours: %2 dropped (not-object)" ).arg( b.where, it.key() ) );
			continue;
		}
		const Obj o = b.child( it.value(), QStringLiteral( "hours/" ) + it.key() );
		o.unknownKeys( known );
		hours.append( { h, o } );
	}
	for ( const Key & k : keys ) {
		double base = 0.0;
		const bool hasBase = b.num( k.name, k.lo, k.hi, base );
		std::vector<std::pair<double, double>> pts;
		for ( const auto & h : hours ) {
			double v = 0.0;
			if ( h.second.num( k.name, k.lo, k.hi, v ) )
				pts.emplace_back( h.first, v );
			else if ( hasBase )
				pts.emplace_back( h.first, base );
		}
		if ( !hasBase && pts.empty() )
			continue;
		r.*( k.field ) = float( hoursAt( pts, hour, base ) );
	}
}

void readLayers( const Obj & b, WwWeatherRecx & r )
{
	const QJsonValue lv = b.get( "layers" );
	if ( lv.isUndefined() )
		return;
	if ( !lv.isObject() ) {
		sayOnce( QStringLiteral( "weather recx: %1: layers dropped (not-object)" ).arg( b.where ) );
		return;
	}
	const QJsonObject lo = lv.toObject();
	for ( auto it = lo.constBegin(); it != lo.constEnd(); ++it ) {
		const QString n = it.key();
		bool ok = false;
		const int i = n.toInt( &ok );
		const bool plain = ok && ( n == QLatin1String( "0" ) || ( !n.startsWith( QLatin1Char( '0' ) ) && n.size() <= 2 ) );
		if ( n.startsWith( QLatin1Char( '_' ) ) || n.compare( QLatin1String( "comment" ), Qt::CaseInsensitive ) == 0 )
			continue;
		if ( !plain || i < 0 || i > 31 ) {
			sayOnce( QStringLiteral( "weather recx: %1/layers: %2 dropped (not-layer)" ).arg( b.where, n ) );
			continue;
		}
		if ( !it.value().isObject() ) {
			sayOnce( QStringLiteral( "weather recx: %1/layers: %2 dropped (not-object)" ).arg( b.where, n ) );
			continue;
		}
		const Obj L = b.child( it.value(), QStringLiteral( "layers/" ) + n );
		// lane WEATHERRECX1 (1.3): a layer may give its type / coverage / density / texture without heights
		QStringList known = { QStringLiteral( "base" ), QStringLiteral( "top" ), QStringLiteral( "type" ), QStringLiteral( "coverage" ),
			QStringLiteral( "density" ), QStringLiteral( "texture" ) };
		if ( !wwCloudRecx14Red() ) {	// lane CLOUDRECX1 (1.4): the layer's water, drops, ice, anvil (and its type for them)
			known << QStringLiteral( "liquidWater" ) << QStringLiteral( "dropletRadius" ) << QStringLiteral( "iceFraction" )
				  << QStringLiteral( "anvil" );
			const Obj Lt{ typeWords( L.o, L.where ), L.where };
			double v = 0.0;
			if ( Lt.num( "liquidWater", 0.0f, WwCloudRecx14::kLwcMax, v ) )
				r.layerLwc[i] = float( v );
			if ( Lt.num( "dropletRadius", WwCloudRecx14::kRadiusMin, WwCloudRecx14::kRadiusMax, v ) )
				r.layerRadius[i] = float( v );
			if ( Lt.num( "iceFraction", 0.0f, 1.0f, v ) )
				r.layerIce[i] = float( v );
			if ( Lt.num( "anvil", 0.0f, WwCloudRecx14::kAnvilMax, v ) )
				r.layerAnvil[i] = float( v );
			if ( ( r.layerLwc[i] >= 0.0f || r.layerRadius[i] >= 0.0f ) && Lt.num( "type", 0.0f, 1.0f, v ) )
				r.layerType[i] = float( v );
		}
		L.unknownKeys( known );
		double base = 0.0, top = 0.0;
		const bool hb = L.num( "base", kLayerMin, kLayerMax, base ), ht = L.num( "top", kLayerMin, kLayerMax, top );
		if ( !hb && !ht )
			continue;
		if ( !hb || !ht ) {
			sayOnce( QStringLiteral( "weather recx: %1: dropped (missing %2)" ).arg( L.where, hb ? "top" : "base" ) );
			continue;
		}
		if ( !( top > base ) ) {
			sayOnce( QStringLiteral( "weather recx: %1: dropped (below-base: top %2 <= base %3)" ).arg( L.where ).arg( top ).arg( base ) );
			continue;
		}
		r.layerBase[i] = float( base );
		r.layerTop[i] = float( top );
		r.layers++;
	}
}

// ---- lane WEATHERRECX1: the 1.3 words, precipitation layers, textures

const char * const kHazeTypes[] = { "clean", "maritime", "urban", "dust", "smoke", "glowing" };
const char * const kChannels[] = { "sky", "sun", "ambient", "fog", "clouds", "imagespace", "lightning", "precip", "wind",
	"transitions" };
const char * const kSlots[] = { "groundFogNoise", "rain", "hail", "snow", "splash", "ripple", "lightning", "dust", "rad" };
const char * const kSlotDefaults[] = { "FO4CS\\Fog\\GroundFogNoise.dds", "FO4CS\\Precip\\rain_d.dds", "FO4CS\\Precip\\hail_d.dds",
	"FO4CS\\Precip\\snow_fine_d.dds", "FO4CS\\Precip\\splash_d.dds", "FO4CS\\Precip\\ripple_n.dds",
	"FO4CS\\Lightning\\bolt_d.dds", "FO4CS\\Particles\\dust_d.dds", "FO4CS\\Particles\\rad_d.dds" };
const char * const kPrecipTypes[] = { "rain", "hail", "snow_fine", "snow_far_dot", "snow_wet_clump", "snow_big_clump" };

void readAtmoWords( const Obj & b, WwWeatherRecx & r, unsigned & useRecord )
{
	const QJsonValue ht = b.get( "hazeType" );
	if ( !ht.isUndefined() ) {
		const QString w = ht.toString().trimmed().toLower();
		bool ok = false;
		for ( const char * k : kHazeTypes )
			ok = ok || w == QLatin1String( k );
		if ( ok )
			r.hazeType = w;
		else
			sayOnce( QStringLiteral( "weather recx: %1: hazeType \"%2\" dropped (not-hazeType: clean, maritime, urban, dust, "
									 "smoke, glowing)" ).arg( b.where, ht.toString() ) );
	}
	const QJsonValue ha = b.get( "hazeAlbedo" );
	if ( ha.isDouble() && ha.toDouble() >= 0.0 && ha.toDouble() <= 1.0 ) {
		r.hazeAlbedo[0] = r.hazeAlbedo[1] = r.hazeAlbedo[2] = float( ha.toDouble() );
	} else if ( ha.isArray() && ha.toArray().size() == 3 ) {
		const QJsonArray a = ha.toArray();
		bool ok = true;
		for ( int i = 0; i < 3; i++ )
			ok = ok && a[i].isDouble() && a[i].toDouble() >= 0.0 && a[i].toDouble() <= 1.0;
		if ( ok )
			for ( int i = 0; i < 3; i++ )
				r.hazeAlbedo[i] = float( a[i].toDouble() );
		else
			sayOnce( QStringLiteral( "weather recx: %1: hazeAlbedo dropped (not-albedo: 0..1 or three of them)" ).arg( b.where ) );
	} else if ( !ha.isUndefined() ) {
		sayOnce( QStringLiteral( "weather recx: %1: hazeAlbedo dropped (not-albedo: 0..1 or three of them)" ).arg( b.where ) );
	}
	const QJsonValue uc = b.get( "useRecordColors" );
	if ( uc.isArray() ) {
		for ( const QJsonValue & v : uc.toArray() ) {
			const unsigned bit = wwWeatherRecordChannels( v.toString() );
			if ( bit & ( WwRecSky | WwRecSun | WwRecAmbient | WwRecFog | WwRecClouds | WwRecImagespace ) )
				useRecord |= bit;
			else
				sayOnce( QStringLiteral( "weather recx: %1: useRecordColors \"%2\" dropped (not-channel: sky, sun, ambient, "
										 "fog, clouds, imagespace)" ).arg( b.where, v.toString() ) );
		}
	} else if ( !uc.isUndefined() ) {
		sayOnce( QStringLiteral( "weather recx: %1: useRecordColors dropped (not-array)" ).arg( b.where ) );
	}
}

void readPrecip( const Obj & b, WwWeatherRecx & r )
{
	const QJsonValue lv = b.get( "layers" );
	if ( lv.isUndefined() )
		return;
	if ( !lv.isArray() ) {
		sayOnce( QStringLiteral( "weather recx: %1: layers dropped (not-array)" ).arg( b.where ) );
		return;
	}
	const QJsonArray la = lv.toArray();
	for ( int i = 0; i < la.size(); i++ ) {
		const Obj L = b.child( la[i], QStringLiteral( "layers/%1" ).arg( i ) );
		L.unknownKeys( { QStringLiteral( "type" ), QStringLiteral( "rate" ), QStringLiteral( "size" ),
			QStringLiteral( "density" ), QStringLiteral( "wind" ), QStringLiteral( "texture" ) } );
		const QString t = L.get( "type" ).toString().trimmed().toLower();
		bool known = false;
		for ( const char * k : kPrecipTypes )
			known = known || t == QLatin1String( k );
		if ( !known ) {
			sayOnce( QStringLiteral( "weather recx: %1: dropped (not-precipitation-type \"%2\")" ).arg( L.where, t ) );
			continue;
		}
		double rate = 0.0;
		if ( !L.num( "rate", 0.0f, 500.0f, rate ) )
			rate = 0.0;
		float & sum = t.startsWith( QLatin1String( "snow" ) ) ? r.snowRate : r.rainRate;
		sum = std::max( 0.0f, sum ) + float( rate );
		if ( r.precipLayer.isEmpty() )
			r.precipLayer = t;
	}
}

void readTextures( const Obj & b, WwWeatherRecx & r )
{
	QStringList known;
	for ( const char * s : kSlots )
		known << QLatin1String( s );
	b.unknownKeys( known );
	for ( int i = 0; i < 9; i++ ) {
		const QJsonValue v = b.get( kSlots[i] );
		if ( v.isUndefined() )
			continue;
		const QString p = v.toString().trimmed();
		if ( !p.endsWith( QLatin1String( ".dds" ), Qt::CaseInsensitive ) ) {
			sayOnce( QStringLiteral( "weather recx: %1: %2 dropped (not-dds \"%3\")" ).arg( b.where, QLatin1String( kSlots[i] ), p ) );
			continue;
		}
		r.textures[i] = p;
	}
}

QString f4( float v )
{
	return v < 0.0f ? QStringLiteral( "-" ) : QString::number( double( v ), 'g', 6 );
}

QString keyOf( const WwWeatherRecx & r )
{
	QString k = QStringLiteral( "vis=%1 hazeHeight=%2 ozone=%3 coverage=%4 density=%5 stars=%6 moonlight=%7" )
		.arg( f4( r.visibilityM ), f4( r.hazeHeightM ), f4( r.ozone ), f4( r.coverage ), f4( r.density ), f4( r.stars ),
			f4( r.moonlight ) );
	if ( r.type >= 0.0f )
		k += QStringLiteral( " type=%1" ).arg( f4( r.type ) );	// lane CLOUD5 (absent: the key text is as before)
	k += QStringLiteral( " layers=%1" ).arg( r.layers );
	for ( int i = 0; i < 32; i++ )
		if ( r.layerBase[i] >= 0.0f )
			k += QStringLiteral( " %1:%2-%3" ).arg( i ).arg( f4( r.layerBase[i] ), f4( r.layerTop[i] ) );
	if ( r.recordFirst )
		k += QStringLiteral( " red=recordfirst" );
	// lane WEATHERRECX1: the 1.3 values (only when a 1.3 value is set: the text above is as before otherwise)
	QString x;
	if ( !r.hazeType.isEmpty() )
		x += QStringLiteral( " hazeType=%1" ).arg( r.hazeType );
	const struct { const char * n; float v; } more[] = { { "humidity", r.humidity }, { "groundAlbedo", r.groundAlbedo },
		{ "angstrom", r.angstrom }, { "hazeAnisotropy", r.hazeAnisotropy }, { "airglow", r.airglow },
		{ "wind", r.windSpeed }, { "windFrom", r.windDirection }, { "gusts", r.gusts }, { "turbulence", r.turbulence },
		{ "lightning", r.lightningFreq }, { "trans", r.transHours }, { "wetting", r.wetting }, { "drying", r.drying },
		{ "rain", r.rainRate }, { "snow", r.snowRate } };
	for ( const auto & m : more )
		if ( m.v >= 0.0f )
			x += QStringLiteral( " %1=%2" ).arg( QLatin1String( m.n ), f4( m.v ) );
	if ( r.airTemp > -999.0f )
		x += QStringLiteral( " air=%1C" ).arg( double( r.airTemp ), 0, 'g', 4 );
	if ( r.recordRed )
		x += QStringLiteral( " recordRed=0x%1" ).arg( r.recordRed, 0, 16 );
	// lane CLOUDRECX1: the 1.4 clouds values (only when one is set: the text above is as before otherwise)
	const struct { const char * n; float v; } c14[] = { { "liquidWater", r.liquidWater }, { "dropletRadius", r.dropletRadius },
		{ "iceFraction", r.iceFraction }, { "evolution", r.evolution }, { "precipitationBase", r.precipitationBase },
		{ "shadowOpacity", r.shadowOpacity }, { "shadowStrength", r.shadowStrength },
		{ "shadowLayerDensity", r.shadowLayerDensity }, { "shadowDeck", r.shadowDeck } };
	for ( const auto & m : c14 )
		if ( m.v >= 0.0f )
			x += QStringLiteral( " %1=%2" ).arg( QLatin1String( m.n ), f4( m.v ) );
	for ( int i = 0; i < 32; i++ )
		if ( r.layerLwc[i] >= 0.0f || r.layerRadius[i] >= 0.0f || r.layerIce[i] >= 0.0f || r.layerAnvil[i] >= 0.0f )
			x += QStringLiteral( " L%1:lwc=%2,r=%3,ice=%4,anvil=%5,type=%6" ).arg( i ).arg( f4( r.layerLwc[i] ), f4( r.layerRadius[i] ),
				f4( r.layerIce[i] ), f4( r.layerAnvil[i] ), f4( r.layerType[i] ) );
	if ( r.phys )	// lane AIRPERF1 A9 (absent: the key text is as before)
		x += QStringLiteral( " phys=0x%1%2%3%4%5" ).arg( r.phys, 0, 16 ).arg( r.coverAlone ? QStringLiteral( " coverAlone" ) : QString(),
			r.densityAlone ? QStringLiteral( " densityAlone" ) : QString(), r.heightsAlone ? QStringLiteral( " heightsAlone" ) : QString(),
			r.stormAlone ? QStringLiteral( " stormAlone" ) : QString() );
	return k + x;
}

/* lane WEATHERRECX1: every number the generated rung can give, its channel (a record red unsets it: the record's own
 * path then decides, as before 1.3) and its block (the source line) */
using GenFn = double ( * )( const WwWeatherGen::Out &, double hour );
struct Input
{
	const char * name;
	float WwWeatherRecx::* field;
	unsigned channel;	// 0: no record path (the record has none, e.g. temperature)
	GenFn gen;	// nullptr: the rung gives none
};

double genVisibility( const WwWeatherGen::Out & o, double hour )
{
	if ( o.visibilityNight < 0.0 )
		return o.visibility;
	return hoursAt( { { 2.0, o.visibilityNight }, { 14.0, o.visibility } }, hour, o.visibility );
}
double genTemp( const WwWeatherGen::Out & o, double hour )
{
	return hoursAt( { { 5.0, o.tempDawn }, { 14.0, o.tempDay } }, hour, o.tempDay );
}

const Input kInputs[] = {
	{ "visibility", &WwWeatherRecx::visibilityM, WwRecFog, genVisibility },
	{ "hazeHeight", &WwWeatherRecx::hazeHeightM, WwRecFog, []( const WwWeatherGen::Out & o, double ) { return o.hazeHeight; } },
	{ "ozone", &WwWeatherRecx::ozone, WwRecSky, []( const WwWeatherGen::Out & o, double ) { return o.ozone; } },
	{ "humidity", &WwWeatherRecx::humidity, WwRecFog, []( const WwWeatherGen::Out & o, double ) { return o.humidity; } },
	{ "groundAlbedo", &WwWeatherRecx::groundAlbedo, WwRecSky, []( const WwWeatherGen::Out & o, double ) { return o.groundAlbedo; } },
	{ "angstrom", &WwWeatherRecx::angstrom, WwRecFog, nullptr },
	{ "hazeAnisotropy", &WwWeatherRecx::hazeAnisotropy, WwRecFog, nullptr },
	{ "airglow", &WwWeatherRecx::airglow, WwRecSky, nullptr },
	{ "coverage", &WwWeatherRecx::coverage, WwRecClouds, []( const WwWeatherGen::Out & o, double ) { return o.coverage; } },
	{ "density", &WwWeatherRecx::density, WwRecClouds, []( const WwWeatherGen::Out & o, double ) { return o.density; } },
	{ "type", &WwWeatherRecx::type, WwRecClouds,
		[]( const WwWeatherGen::Out & o, double ) { return WwWeatherGen::cloudTypeNumber( o.cloudType ); } },
	// lane CLOUDRECX1 (1.4): no generated rung (the type's defaults, cloudrecx14math.h, stand in: today's picture)
	{ "shadowOpacity", &WwWeatherRecx::shadowOpacity, WwRecClouds, nullptr },	// MODULES1 10c: no generated rung (cloudfusionlaw.h)
	{ "shadowStrength", &WwWeatherRecx::shadowStrength, WwRecClouds, nullptr },
	{ "shadowLayerDensity", &WwWeatherRecx::shadowLayerDensity, WwRecClouds, nullptr },
	{ "shadowDeck", &WwWeatherRecx::shadowDeck, WwRecClouds, nullptr },
	{ "liquidWater", &WwWeatherRecx::liquidWater, WwRecClouds, nullptr },
	{ "dropletRadius", &WwWeatherRecx::dropletRadius, WwRecClouds, nullptr },
	{ "iceFraction", &WwWeatherRecx::iceFraction, WwRecClouds, nullptr },
	{ "evolution", &WwWeatherRecx::evolution, WwRecClouds, nullptr },
	{ "precipitationBase", &WwWeatherRecx::precipitationBase, WwRecClouds, nullptr },
	{ "stars", &WwWeatherRecx::stars, WwRecSky, []( const WwWeatherGen::Out & o, double ) { return o.stars; } },
	{ "moonlight", &WwWeatherRecx::moonlight, WwRecSky, []( const WwWeatherGen::Out & o, double ) { return o.moonlight; } },
	{ "wind.speed", &WwWeatherRecx::windSpeed, WwRecWind, []( const WwWeatherGen::Out & o, double ) { return o.windSpeed; } },
	{ "wind.direction", &WwWeatherRecx::windDirection, WwRecWind,
		[]( const WwWeatherGen::Out & o, double ) { return o.windDirection; } },
	{ "wind.gusts", &WwWeatherRecx::gusts, WwRecWind, []( const WwWeatherGen::Out & o, double ) { return o.gusts; } },
	{ "wind.turbulence", &WwWeatherRecx::turbulence, WwRecWind, []( const WwWeatherGen::Out & o, double ) { return o.turbulence; } },
	{ "temperature", &WwWeatherRecx::airTemp, 0u, genTemp },
	{ "lightning.frequency", &WwWeatherRecx::lightningFreq, WwRecLightning,
		[]( const WwWeatherGen::Out & o, double ) { return o.lightning; } },
	{ "lightning.cloudToGround", &WwWeatherRecx::cloudToGround, WwRecLightning, nullptr },
	{ "lightning.distance", &WwWeatherRecx::lightningDistance, WwRecLightning, nullptr },
	{ "transitions.duration", &WwWeatherRecx::transHours, WwRecTransitions,
		[]( const WwWeatherGen::Out & o, double ) { return o.transHours; } },
	{ "transitions.precipitationStart", &WwWeatherRecx::precipStart, WwRecTransitions,
		[]( const WwWeatherGen::Out & o, double ) { return o.precipStart; } },
	{ "transitions.precipitationEnd", &WwWeatherRecx::precipEnd, WwRecTransitions,
		[]( const WwWeatherGen::Out & o, double ) { return o.precipEnd; } },
	{ "transitions.lightningStart", &WwWeatherRecx::lightningStart, WwRecTransitions,
		[]( const WwWeatherGen::Out & o, double ) { return o.lightningStart; } },
	{ "transitions.lightningEnd", &WwWeatherRecx::lightningEnd, WwRecTransitions,
		[]( const WwWeatherGen::Out & o, double ) { return o.lightningEnd; } },
	{ "precipitation.wetting", &WwWeatherRecx::wetting, WwRecPrecip, []( const WwWeatherGen::Out & o, double ) { return o.wetting; } },
	{ "precipitation.drying", &WwWeatherRecx::drying, WwRecPrecip, []( const WwWeatherGen::Out & o, double ) { return o.drying; } },
	{ "precipitation.rain", &WwWeatherRecx::rainRate, WwRecPrecip, []( const WwWeatherGen::Out & o, double ) {
		return o.precipType && std::string( o.precipType ) == "rain" ? o.precipRate : 0.0; } },
	{ "precipitation.snow", &WwWeatherRecx::snowRate, WwRecPrecip, []( const WwWeatherGen::Out & o, double ) {
		return o.precipType && std::string( o.precipType ) == "snow_fine" ? o.precipRate : 0.0; } },
};

bool isSet( const WwWeatherRecx & r, const Input & in )
{
	return in.field == &WwWeatherRecx::airTemp ? r.airTemp > -999.0f : r.*( in.field ) >= 0.0f;
}
void unset( WwWeatherRecx & r, const Input & in )
{
	r.*( in.field ) = in.field == &WwWeatherRecx::airTemp ? -1000.0f : -1.0f;
}

//! the env red WW_WEATHER_RECORD_RED (re-read each resolve: a pin may move it)
unsigned envRecordRed()
{
	return wwWeatherRecordChannels( wwEnvStr( "WW_WEATHER_RECORD_RED" ) );
}

/* lane CLOUDRECX1 (SPEC.md 1.4): a file's liquidWater / dropletRadius ARE the extinction: the drawn density =
 * sigma(file's liquidWater / dropletRadius) / sigma(the type's default), the generated rung's density no longer
 * multiplied in (the generator writes its rung as liquidWater = the type's default x its density, so a regenerated
 * file draws exactly what the 1.3 file did). Neither key set: the ratio is 1 and density is left as it was (the
 * legacy key's or the rung's: today's picture, the same key text). */
void cloud14Density( WwWeatherRecx & r )
{
	r.densityGen = r.density;
	r.densityRatio = 1.0f;
	if ( r.liquidWater < 0.0f && r.dropletRadius < 0.0f )
		return;
	const float type = r.type >= 0.0f ? r.type : 2.0f / 3.0f;	// no type: CLOUD5's Variety default (cumulus)
	r.densityRatio = WwCloudRecx14::densityRatio( r.liquidWater, r.dropletRadius, WwCloudRecx14::lowDefault( type ) );
	// the drawn density stays in the 1.3 range (0..kDensityMax: the march's coverage tables are built for it)
	r.density = std::min( r.densityRatio, kDensityMax );
}

bool cloud14Any( const WwWeatherRecx & r )
{
	if ( r.liquidWater >= 0.0f || r.dropletRadius >= 0.0f || r.iceFraction >= 0.0f || r.evolution >= 0.0f
		|| r.precipitationBase >= 0.0f )
		return true;
	for ( int i = 0; i < 32; i++ )
		if ( r.layerLwc[i] >= 0.0f || r.layerRadius[i] >= 0.0f || r.layerIce[i] >= 0.0f || r.layerAnvil[i] >= 0.0f )
			return true;
	return false;
}

/* lane CLOUDRECX1: the telemetry line at each weather open: every 1.4 input, its value and source, the derived sigma
 * (SPEC.md 1.4: sigma = 3 LWC / (2 rho_w r)) against the type default's, the ratio the march takes, the legacy alias */
QString cloud14Line( const WwWeatherRecx & r )
{
	namespace C = WwCloudRecx14;
	if ( wwCloudRecx14Red() )
		return QStringLiteral( "cloud recx 1.4: red (WW_CLOUD_RECX14_RED): the 1.3 clouds block, density=%1" ).arg( f4( r.density ) );
	const float type = r.type >= 0.0f ? r.type : 2.0f / 3.0f;
	const C::Micro d = C::lowDefault( type );
	const float w = r.liquidWater >= 0.0f ? r.liquidWater : d.lwc, rad = r.dropletRadius >= 0.0f ? r.dropletRadius : d.rUm;
	auto src = []( float v ) { return v >= 0.0f ? "file" : "default"; };
	QString s = QStringLiteral( "cloud recx 1.4: type=%1 liquidWater=%2(%3) dropletRadius=%4(%5) sigma=%6/km default=%7/km "
								"(%8 g/m3, %9 um) ratio=%10 density=%11 legacy=%12" )
		.arg( double( type ), 0, 'f', 3 ).arg( double( w ), 0, 'g', 4 ).arg( QLatin1String( src( r.liquidWater ) ) )
		.arg( double( rad ), 0, 'g', 4 ).arg( QLatin1String( src( r.dropletRadius ) ) )
		.arg( double( C::sigmaPerKm( w, rad ) ), 0, 'f', 1 ).arg( double( C::sigmaPerKm( d.lwc, d.rUm ) ), 0, 'f', 1 )
		.arg( double( d.lwc ), 0, 'g', 3 ).arg( double( d.rUm ), 0, 'g', 3 ).arg( double( r.densityRatio ), 0, 'f', 4 )
		.arg( f4( r.density ) ).arg( r.densityLegacy ? QStringLiteral( "yes" ) : QStringLiteral( "no" ) );
	s += QStringLiteral( " iceFraction=%1 evolution=%2 precipitationBase=%3" )
		.arg( r.iceFraction >= 0.0f ? f4( r.iceFraction ) : QStringLiteral( "0(default)" ) )
		.arg( r.evolution >= 0.0f ? f4( r.evolution ) + QStringLiteral( "min" ) : QStringLiteral( "-(default: still)" ) )
		.arg( r.precipitationBase >= 0.0f
				  ? f4( r.precipitationBase ) + QStringLiteral( "m shaft=%1/km" ).arg( double( C::shaftSigmaPerKm(
						std::max( r.rainRate, 0.0f ), std::max( r.snowRate, 0.0f ) ) ), 0, 'f', 3 )
				  : QStringLiteral( "-(default: no shafts)" ) );
	for ( int i = 0; i < 32; i++ )
		if ( r.layerLwc[i] >= 0.0f || r.layerRadius[i] >= 0.0f || r.layerIce[i] >= 0.0f || r.layerAnvil[i] >= 0.0f )
			s += QStringLiteral( " layer%1=lwc:%2,r:%3,ice:%4,anvil:%5" ).arg( i ).arg( f4( r.layerLwc[i] ), f4( r.layerRadius[i] ),
				f4( r.layerIce[i] ), f4( r.layerAnvil[i] ) );
	return s;
}

struct Who
{
	quint32 formID = 0;
	QString edid, owner;
	WwWeatherGen::In gen;
	bool haveGen = false;
};

/* lane MODULES1 part 24 (SPEC.md 1.5): the red drops the keys a block lists in `_translated` (the WTHR translation of
 * weathertranslate.py), so the generated rung draws them: the files as they were before 1.5 */
static bool weatherTranslateRed()
{
	WW_SWITCH_STATIC( bool, red, wwEnv( "WW_WEATHER_TRANSLATE_RED" ).trimmed() == QLatin1String( "1" ) );
	return red;
}

static QJsonObject untranslated( QJsonObject o )
{
	const QJsonArray keys = o.value( QStringLiteral( "_translated" ) ).toArray();
	for ( const QJsonValue & k : keys )
		o.remove( k.toString() );
	return o;
}

/* lane AIRPERF1 A9: the block families of the physicalWeather switch, and the keys that are a RECORD PART (the WTHR
 * stores the same quantity, SPEC.md "The generated rung" 4: wind.direction = DATA[17] as a bearing, wind.turbulence =
 * DATA[19] / 255): written in a file they replace that part with or without the switch (SPEC.md 1.6 rule 2). Every
 * other key of these blocks is a physical addition. */
struct PhysFam
{
	const char * block;
	unsigned bit;
};
const PhysFam kPhysFam[5] = { { "clouds", WwPhysClouds }, { "night", WwPhysNight }, { "wind", WwPhysWind },
	{ "lightning", WwPhysLightning }, { "precipitation", WwPhysPrecip } };

int physFamOf( const QString & block )
{
	for ( int i = 0; i < 5; i++ )
		if ( block == QLatin1String( kPhysFam[i].block ) )
			return i;
	return -1;
}

bool recordPart( const char * key )
{
	return !std::strcmp( key, "wind.direction" ) || !std::strcmp( key, "wind.turbulence" );
}

/* one weather at the hour: the files' blocks (per key: the file), then the generated rung for every key no file set,
 * then the record reds (which unset their channel's keys); src = "<key>=<source>" per input */
WwWeatherRecx resolveOne( const Who & w, const QStringList & roots, const QStringList & files, double hour, unsigned red,
	QString & fileLine )
{
	WwWeatherRecx r;
	const quint32 lid = ( w.formID >> 24 ) == 0xFEu ? ( w.formID & 0xFFFu ) : ( w.formID & 0xFFFFFFu );
	fileLine = QStringLiteral( "%1 %2|0x%3" ).arg( w.edid, w.owner ).arg( lid, 8, 16, QLatin1Char( '0' ) );
	QMap<QString, QString> src;
	unsigned useRecord = 0;
	QString famState[5], famIgnored[5];	// lane AIRPERF1 A9: per family "<switch>:<who decides>", the keys not read
	for ( const char * block : { "atmosphere", "clouds", "night", "precipitation", "wind", "temperature", "lightning",
			  "transitions", "textures" } ) {
		const QString bn = QLatin1String( block );
		const WwRecxHit h = wwRecxFind( roots, files, QStringLiteral( "Weathers" ), QStringLiteral( "weather" ), w.owner, lid,
			w.edid, bn, true );
		if ( !h.ok )
			continue;
		const QString rel = h.file.section( QLatin1Char( '/' ), -3 );
		fileLine += QStringLiteral( " %1 %2:%3" ).arg( bn, h.rung, rel );
		const bool trRed = weatherTranslateRed() && h.block.contains( QStringLiteral( "_translated" ) );
		if ( trRed )
			fileLine += QStringLiteral( " (translate red: %1 dropped)" ).arg( bn );
		const Obj b{ trRed ? untranslated( h.block ) : h.block, rel + QStringLiteral( " /" ) + bn };
		WwWeatherRecx before = r;
		if ( bn == QLatin1String( "atmosphere" ) ) {
			b.unknownKeys( { QStringLiteral( "visibility" ), QStringLiteral( "hazeHeight" ), QStringLiteral( "ozone" ),
				QStringLiteral( "hours" ), QStringLiteral( "hazeType" ), QStringLiteral( "humidity" ),
				QStringLiteral( "groundAlbedo" ), QStringLiteral( "angstrom" ), QStringLiteral( "hazeAnisotropy" ),
				QStringLiteral( "airglow" ), QStringLiteral( "hazeAlbedo" ), QStringLiteral( "useRecordColors" ) } );
			readKeys( b, kAtmo, hour, r );
			readAtmoWords( b, r, useRecord );
			if ( wwRecxPhysicalWeather( h.entry ) )	// lane AIRPERF1 A8: the entry's switch, read where its values are
				src[QStringLiteral( "physicalWeather" )] = QStringLiteral( "file:" ) + rel;
			if ( !r.hazeType.isEmpty() )
				src[QStringLiteral( "hazeType" )] = QStringLiteral( "file:" ) + rel;
			if ( r.hazeAlbedo[0] >= 0.0f )
				src[QStringLiteral( "hazeAlbedo" )] = QStringLiteral( "file:" ) + rel;
		} else if ( bn == QLatin1String( "clouds" ) ) {
			QStringList known = { QStringLiteral( "coverage" ), QStringLiteral( "density" ), QStringLiteral( "layers" ),
				QStringLiteral( "type" ), QStringLiteral( "hours" ) };
			const bool v14 = !wwCloudRecx14Red();	// lane CLOUDRECX1: the 1.4 keys (the red: the 1.3 block, they are unknown)
			if ( v14 )
				known << QStringLiteral( "liquidWater" ) << QStringLiteral( "dropletRadius" ) << QStringLiteral( "iceFraction" )
					  << QStringLiteral( "evolution" ) << QStringLiteral( "precipitationBase" )
					  << QStringLiteral( "shadowOpacity" ) << QStringLiteral( "shadowStrength" )
					  << QStringLiteral( "shadowLayerDensity" ) << QStringLiteral( "shadowDeck" );
			b.unknownKeys( known );
			const Obj bc{ typeWords( b.o, b.where ), b.where };	// lane CLOUD5: clouds.type words -> numbers
			if ( v14 ) {
				readKeys( bc, kClouds14, hour, r );
				if ( r.density >= 0.0f ) {	// SPEC.md 1.4: density stays one release as the legacy alias
					r.densityLegacy = true;
					if ( r.liquidWater >= 0.0f || r.dropletRadius >= 0.0f ) {
						sayOnce( QStringLiteral( "weather recx: %1: density ignored (liquidWater / dropletRadius set; density is "
												 "the 1.4 legacy alias)" ).arg( b.where ) );
						r.density = -1.0f;	// cloud14Density: the file's sigma ratio alone
						r.densityLegacy = false;
					} else {
						sayOnce( QStringLiteral( "weather recx: %1: density is the 1.4 legacy alias (kept this release; use "
												 "liquidWater g/m^3 and dropletRadius um)" ).arg( b.where ) );
					}
				}
			} else {
				readKeys( bc, kClouds, hour, r );
			}
			readLayers( b, r );
			if ( r.layers )
				src[QStringLiteral( "layers" )] = QStringLiteral( "file:" ) + rel;
		} else if ( bn == QLatin1String( "night" ) ) {
			b.unknownKeys( { QStringLiteral( "stars" ), QStringLiteral( "moonlight" ), QStringLiteral( "hours" ) } );
			readKeys( b, kNight, hour, r );
		} else if ( bn == QLatin1String( "wind" ) ) {
			b.unknownKeys( { QStringLiteral( "speed" ), QStringLiteral( "direction" ), QStringLiteral( "gusts" ),
				QStringLiteral( "turbulence" ), QStringLiteral( "heights" ), QStringLiteral( "hours" ) } );
			readKeys( b, kWind, hour, r );
		} else if ( bn == QLatin1String( "temperature" ) ) {
			b.unknownKeys( { QStringLiteral( "air" ), QStringLiteral( "hours" ) } );
			readKeys( b, kTemp, hour, r );
		} else if ( bn == QLatin1String( "lightning" ) ) {
			b.unknownKeys( { QStringLiteral( "frequency" ), QStringLiteral( "cloudToGround" ), QStringLiteral( "distance" ) } );
			readKeys( b, kLightning, hour, r, false );
		} else if ( bn == QLatin1String( "transitions" ) ) {
			b.unknownKeys( { QStringLiteral( "duration" ), QStringLiteral( "precipitationStart" ), QStringLiteral( "precipitationEnd" ),
				QStringLiteral( "lightningStart" ), QStringLiteral( "lightningEnd" ) } );
			readKeys( b, kTrans, hour, r, false );
		} else if ( bn == QLatin1String( "precipitation" ) ) {
			b.unknownKeys( { QStringLiteral( "layers" ), QStringLiteral( "wetting" ), QStringLiteral( "drying" ) } );
			readKeys( b, kPrecip, hour, r, false );
			readPrecip( b, r );
			if ( !b.get( "layers" ).isUndefined() ) {	// a layers list says it all: a family it lacks is dry
				r.rainRate = std::max( 0.0f, r.rainRate );
				r.snowRate = std::max( 0.0f, r.snowRate );
			}
		} else {
			readTextures( b, r );
			for ( int i = 0; i < 9; i++ )
				if ( !r.textures[i].isEmpty() )
					src[QStringLiteral( "textures.%1" ).arg( QLatin1String( kSlots[i] ) )] = QStringLiteral( "file:" ) + rel;
		}
		/* lane AIRPERF1 A9 (bungo 11-11): the block follows its OWN entry's physicalWeather switch. On: its values
		 * stand (the file decides alone). Off or absent: its physical additions are not read, so the generated rung
		 * below gives what a weather with no file gets; the record parts it writes stay. The family's red: build 62. */
		const int fi = physFamOf( bn );
		if ( fi >= 0 ) {
			const bool sw = wwRecxPhysicalWeather( h.entry ), redF = switchRed( block );
			QStringList ign;
			if ( sw && !redF ) {
				r.phys |= kPhysFam[fi].bit;
			} else if ( !redF ) {
				for ( const Input & in : kInputs )
					if ( r.*( in.field ) != before.*( in.field ) && !recordPart( in.name ) ) {
						r.*( in.field ) = before.*( in.field );
						ign << QLatin1String( in.name );
					}
				if ( kPhysFam[fi].bit == WwPhysClouds ) {
					if ( r.layers != before.layers )
						ign << QStringLiteral( "layers" );
					for ( int i = 0; i < 32; i++ ) {
						r.layerBase[i] = before.layerBase[i];
						r.layerTop[i] = before.layerTop[i];
						r.layerLwc[i] = before.layerLwc[i];
						r.layerRadius[i] = before.layerRadius[i];
						r.layerIce[i] = before.layerIce[i];
						r.layerAnvil[i] = before.layerAnvil[i];
						r.layerType[i] = before.layerType[i];
					}
					r.layers = before.layers;
					r.densityLegacy = before.densityLegacy;
					if ( !r.layers )
						src.remove( QStringLiteral( "layers" ) );
				}
				if ( kPhysFam[fi].bit == WwPhysPrecip )
					r.precipLayer = before.precipLayer;
			}
			famState[fi] = QStringLiteral( "%1:%2%3" ).arg( sw ? QStringLiteral( "on" ) : QStringLiteral( "off" ),
				( sw || redF ) ? QStringLiteral( "recx" ) : QStringLiteral( "record" ),
				redF ? QStringLiteral( " RED=noswitch" ) : QString() );
			famIgnored[fi] = ign.join( QLatin1Char( ',' ) );
		}
		for ( const Input & in : kInputs )
			if ( !isSet( before, in ) && isSet( r, in ) )
				src[QLatin1String( in.name )] = QStringLiteral( "file:" ) + rel;
	}
	// rung 2: the generated rung for every key no file set (weathergen.h; never a colour or fog distance)
	if ( w.haveGen ) {
		const WwWeatherGen::Out o = WwWeatherGen::generate( w.gen );
		r.genClass = QLatin1String( o.cls );
		r.genRegion = QLatin1String( o.region );
		for ( const Input & in : kInputs )
			if ( !isSet( r, in ) && in.gen ) {
				r.*( in.field ) = float( in.gen( o, hour ) );
				src[QLatin1String( in.name )] = QStringLiteral( "generated" );
			}
		if ( r.hazeType.isEmpty() ) {
			r.hazeType = QLatin1String( o.hazeType );
			src[QStringLiteral( "hazeType" )] = QStringLiteral( "generated" );
		}
		if ( r.precipLayer.isEmpty() && o.precipType )
			r.precipLayer = QLatin1String( o.precipType );
	}
	for ( int i = 0; i < 9; i++ )
		if ( r.textures[i].isEmpty() )
			src[QStringLiteral( "textures.%1" ).arg( QLatin1String( kSlots[i] ) )] = QStringLiteral( "default" );
	// the record reds: the channel's keys are unset (the record's own path decides, as before 1.3)
	r.recordRed = red | useRecord;
	for ( const Input & in : kInputs )
		if ( in.channel && ( r.recordRed & in.channel ) ) {
			unset( r, in );
			src[QLatin1String( in.name )] = QStringLiteral( "record" );
		}
	if ( r.recordRed & WwRecClouds ) {
		for ( int i = 0; i < 32; i++ )
			r.layerBase[i] = r.layerTop[i] = r.layerLwc[i] = r.layerRadius[i] = r.layerIce[i] = r.layerAnvil[i] = r.layerType[i]
				= -1.0f;
		r.layers = 0;
		r.densityLegacy = false;
		src.remove( QStringLiteral( "layers" ) );
	}
	cloud14Density( r );
	if ( r.recordRed & WwRecFog ) {
		r.hazeType.clear();
		src[QStringLiteral( "hazeType" )] = QStringLiteral( "record" );
	}
	// lane AIRPERF1 A9: which written values decide alone (a record red or a dropped key is no file value)
	auto fileKey = [&src]( const char * k ) { return src.value( QLatin1String( k ) ).startsWith( QLatin1String( "file:" ) ); };
	if ( r.phys & WwPhysClouds ) {
		r.coverAlone = fileKey( "coverage" );
		r.densityAlone = fileKey( "liquidWater" ) || fileKey( "dropletRadius" );	// the legacy density is a multiple by its own definition
		r.heightsAlone = r.layers > 0;
	}
	r.stormAlone = ( r.phys & WwPhysLightning ) && fileKey( "lightning.frequency" );
	r.lightningFreqOwn = r.lightningFreq;
	{	// the echo: per family the switch, who decides, the numbers in use, the file keys not read
		const QString nums[5] = {
			QStringLiteral( "coverage=%1 density=%2 type=%3 liquidWater=%4 dropletRadius=%5 layers=%6%7%8%9" )
				.arg( f4( r.coverage ), f4( r.density ), f4( r.type ), f4( r.liquidWater ), f4( r.dropletRadius ) ).arg( r.layers )
				.arg( r.coverAlone ? QStringLiteral( " coverAlone" ) : QString(), r.densityAlone ? QStringLiteral( " densityAlone" ) : QString(),
					r.heightsAlone ? QStringLiteral( " heightsAlone" ) : QString() ),
			QStringLiteral( "stars=%1 moonlight=%2" ).arg( f4( r.stars ), f4( r.moonlight ) ),
			QStringLiteral( "speed=%1 direction=%2 turbulence=%3 gusts=%4" ).arg( f4( r.windSpeed ), f4( r.windDirection ),
				f4( r.turbulence ), f4( r.gusts ) ),
			QStringLiteral( "frequency=%1%2" ).arg( f4( r.lightningFreq ), r.stormAlone ? QStringLiteral( " stormAlone" ) : QString() ),
			QStringLiteral( "rain=%1 snow=%2 wetting=%3 drying=%4" ).arg( f4( r.rainRate ), f4( r.snowRate ), f4( r.wetting ),
				f4( r.drying ) ) };
		QString st, ln;
		for ( int i = 0; i < 5; i++ ) {
			const QString s = QStringLiteral( " %1=%2" ).arg( QLatin1String( kPhysFam[i].block ),
				famState[i].isEmpty() ? QStringLiteral( "nofile:record" ) : famState[i] );
			const QString ig = famIgnored[i].isEmpty() ? QString() : QStringLiteral( "; file keys not read: " ) + famIgnored[i];
			st += s + ig;
			ln += s + QLatin1Char( '(' ) + nums[i] + ig + QLatin1Char( ')' );
		}
		r.blocksState = w.edid + st;
		r.blocksLine = QStringLiteral( "weather recx blocks: " ) + w.edid + ln;
	}
	for ( auto it = src.constBegin(); it != src.constEnd(); ++it )
		r.sources << it.key() + QLatin1Char( '=' ) + it.value();
	return r;
}

float lerpSet( float a, float b, float t, bool angle = false )
{
	if ( a < 0.0f )
		return b;	// only the new weather sets it
	if ( b < 0.0f )
		return t < 1.0f ? a : b;
	if ( angle ) {
		float d = std::fmod( b - a + 540.0f, 360.0f ) - 180.0f;	// the short way round
		return std::fmod( a + d * t + 360.0f, 360.0f );
	}
	return a + ( b - a ) * t;
}

//! lane WEATHERRECX1: while a change blends, every number from the leaving weather (a) to the new one (b) by pct
void blendInto( WwWeatherRecx & b, const WwWeatherRecx & a, float t )
{
	for ( const Input & in : kInputs ) {
		if ( in.field == &WwWeatherRecx::airTemp ) {
			if ( a.airTemp > -999.0f && b.airTemp > -999.0f )
				b.airTemp = a.airTemp + ( b.airTemp - a.airTemp ) * t;
			continue;
		}
		b.*( in.field ) = lerpSet( a.*( in.field ), b.*( in.field ), t, in.field == &WwWeatherRecx::windDirection );
	}
	for ( int i = 0; i < 3; i++ )
		b.hazeAlbedo[i] = lerpSet( a.hazeAlbedo[i], b.hazeAlbedo[i], t );
}

}	// namespace

unsigned wwWeatherRecordChannels( const QString & words )
{
	unsigned bits = 0;
	for ( const QString & raw : words.split( QRegularExpression( QStringLiteral( "[,;\\s]+" ) ), Qt::SkipEmptyParts ) ) {
		const QString w = raw.trimmed().toLower();
		if ( w == QLatin1String( "all" ) ) {
			bits |= WwRecAll;
			continue;
		}
		bool ok = false;
		for ( int i = 0; i < 10; i++ )
			if ( w == QLatin1String( kChannels[i] ) ) {
				bits |= 1u << i;
				ok = true;
			}
		if ( !ok )
			sayOnce( QStringLiteral( "weather recx: record red \"%1\" not a channel (sky, sun, ambient, fog, clouds, imagespace, "
									 "lightning, precip, wind, transitions, all)" ).arg( raw ) );
	}
	return bits;
}

bool wwWeatherRecxSwitchRed( const char * family )
{
	return switchRed( family );
}

bool wwCloudRecx14Red()
{
	const QString v = wwEnvStr( "WW_CLOUD_RECX14_RED" ).trimmed().toLower();
	return v == QLatin1String( "1" ) || v == QLatin1String( "on" ) || v == QLatin1String( "true" ) || v == QLatin1String( "yes" );
}

bool wwWeatherRecordRed( unsigned channel )
{
	const WwWeatherRecx & r = wwWeatherRecx();
	if ( !r.on )
		return true;	// the row off / no weather: the record's own path (as before 1.3)
	return ( r.recordRed & channel ) != 0;
}

int wwWeatherRecxTextureSlot( const char * name )
{
	for ( int i = 0; i < 9; i++ )
		if ( QString::fromLatin1( name ).compare( QLatin1String( kSlots[i] ), Qt::CaseInsensitive ) == 0 )
			return i;
	return -1;
}

QString wwWeatherRecxTexture( const char * slot )
{
	const int i = wwWeatherRecxTextureSlot( slot );
	if ( i < 0 )
		return QString();
	const WwWeatherRecx & r = wwWeatherRecx();
	return r.on && !r.textures[i].isEmpty() ? r.textures[i] : QLatin1String( kSlotDefaults[i] );
}

const WwWeatherRecx & wwWeatherRecx()
{
	static const WwWeatherRecx off;
	static WwWeatherRecx cur;
	static QElapsedTimer since;
	static double lastHour = -1.0;
	static QString lastWeather, lastRed;
	static bool wasOn = false;
	if ( !wwLookOn( WwLook::WeatherRecx ) ) {
		wasOn = false;
		return off;
	}
	const QString rw = redWord();
	if ( rw == QLatin1String( "ignore" ) ) {
		sayOnce( QStringLiteral( "weather recx: no file read (red ignore)" ) );
		wasOn = false;
		return off;
	}
	const double hour = wwLookdevHour();
	const QString wk = wwLookdevWeatherKey();
	const QString redKey = rw + QLatin1Char( '|' ) + wwEnvStr( "WW_WEATHER_RECORD_RED" ) + QLatin1Char( '|' )
		+ QString::number( wwWxOvStamp() );	// lane MODULES1 item 12: a live haze / cloud override
	/* MODULES1 item 8 (PERFCPU1: ~10 calls a frame, each a full resolve once frames pass 100 ms: 6 % of the frame's
	 * CPU): within one frame and one switch state the answer is kept too. Red WW_RECX_FRAMECACHE_RED=1: the 100 ms only */
	static quint64 lastEpoch = ~quint64( 0 );
	WW_SWITCH_STATIC( bool, frameRed, wwEnv( "WW_RECX_FRAMECACHE_RED" ).trimmed() == "1" );
	const quint64 epoch = wwEnvEpoch();
	const bool sameFrame = !frameRed && epoch == lastEpoch;
	if ( wasOn && ( sameFrame || ( since.isValid() && since.elapsed() < 100 ) ) && hour == lastHour && wk == lastWeather
		&& redKey == lastRed )
		return cur;
	lastEpoch = epoch;
	wasOn = true;
	since.start();
	lastHour = hour;
	lastWeather = wk;
	lastRed = redKey;

	WwWeatherRecx next;
	quint32 formID = 0;
	QString edid, owner, dataDir;
	QStringList files;
	QString src, srcLast;
	float pct = 1.0f;
	if ( wwLookdevWeatherId( formID, edid, owner, files, dataDir ) ) {
		static QString rootsFor;
		static QStringList roots;
		const QString rk = files.join( QLatin1Char( ';' ) ) + QLatin1Char( '|' ) + dataDir;
		if ( rk != rootsFor ) {
			rootsFor = rk;
			roots = wwRecxRoots( files, dataDir );
		}
		// lane WEATHERRECX1: the generated rung's inputs of this weather and, while a change blends, the leaving one
		WwWeatherGen::In gin[2];
		quint32 gid[2] = { 0, 0 };
		const int n = wwLookdevWeatherGen( gin, gid, pct );
		Who now;
		now.formID = formID;
		now.edid = edid;
		now.owner = owner;
		now.haveGen = n >= 1 && gid[0] == formID;
		if ( now.haveGen )
			now.gen = gin[0];
		const unsigned red = envRecordRed();
		next = resolveOne( now, roots, files, hour, red, src );
		if ( n == 2 ) {
			Who was;
			was.formID = gid[1];
			was.edid = QString::fromStdString( gin[1].edid );
			was.owner = QString::fromStdString( gin[1].owner );
			was.gen = gin[1];
			was.haveGen = true;
			const WwWeatherRecx left = resolveOne( was, roots, files, hour, red, srcLast );
			blendInto( next, left, pct );
			next.stormAloneLast = left.stormAlone;	// lane AIRPERF1 A9: the leaving weather's own storm answer
			next.lightningFreqLast = left.lightningFreqOwn;
		}
	} else {
		src = QStringLiteral( "none(no weather)" );
	}
	// lane MODULES1 item 12: the live overrides (unset: the file's value, shown in the Scene window)
	next.visibilityM = wwWxOv( WwWx::HazeVisibility, next.visibilityM );
	next.hazeHeightM = wwWxOv( WwWx::HazeHeight, next.hazeHeightM );
	next.coverage = wwWxOv( WwWx::CloudCoverage, next.coverage );
	next.density = wwWxOv( WwWx::CloudDensity, next.density );
	// lane AIRPERF1 A9: a live cover / density is the user's own multiple-style value: today's law, no "alone"
	if ( wwWxOv( WwWx::CloudCoverage, -2.0f ) != -2.0f )
		next.coverAlone = false;
	if ( wwWxOv( WwWx::CloudDensity, -2.0f ) != -2.0f )
		next.densityAlone = false;
	next.recordFirst = rw == QLatin1String( "recordfirst" );
	next.on =next.visibilityM >= 0.0f || next.hazeHeightM >= 0.0f || next.ozone >= 0.0f || next.coverage >= 0.0f
		|| next.density >= 0.0f || next.layers > 0 || next.stars >= 0.0f || next.moonlight >= 0.0f || next.type >= 0.0f
		|| !next.hazeType.isEmpty() || next.windSpeed >= 0.0f || next.lightningFreq >= 0.0f || next.transHours >= 0.0f
		|| next.recordRed != 0 || cloud14Any( next );
	next.key = next.on ? keyOf( next ) : QString();
	next.stamp = cur.stamp;
	// lane WEATHERRECX1: one line per weather open, every input's source (file:<path> | generated | record | default)
	static QString lastOpen;
	if ( wk != lastOpen ) {
		lastOpen = wk;
		std::fprintf( stderr, "weather recx open: %s class=%s region=%s recordRed=%s sources: %s\n", qPrintable( src ),
			qPrintable( next.genClass.isEmpty() ? QStringLiteral( "-" ) : next.genClass ),
			qPrintable( next.genRegion.isEmpty() ? QStringLiteral( "-" ) : next.genRegion ),
			qPrintable( wwEnvStr( "WW_WEATHER_RECORD_RED" ).isEmpty() ? QStringLiteral( "none" ) : wwEnvStr( "WW_WEATHER_RECORD_RED" ) ),
			qPrintable( next.sources.join( QLatin1Char( ' ' ) ) ) );
		std::fprintf( stderr, "%s\n", qPrintable( cloud14Line( next ) ) );	// lane CLOUDRECX1: the 1.4 clouds line
	}
	// lane AIRPERF1 A9: per block family the switch and who decides, when that (not a clock-driven number) changes
	static QString lastBlocks;
	if ( !next.blocksState.isEmpty() && next.blocksState != lastBlocks ) {
		lastBlocks = next.blocksState;
		std::fprintf( stderr, "%s\n", qPrintable( next.blocksLine ) );
	}
	if ( next.key != cur.key ) {
		next.stamp = cur.stamp + 1;
		// the census: one line when the weather, its files or a value moves (an hour-keyed value moves with the clock,
		// and a change's blend moves every frame: at most one line a second then)
		static QElapsedTimer lastSaid;
		static QString lastSrc;
		static double lastSaidHour = -1.0;
		static QString lastSaidWeather;
		const bool clockOnly = src == lastSrc && wk == lastSaidWeather && ( hour != lastSaidHour || pct < 1.0f );
		if ( !clockOnly || !lastSaid.isValid() || lastSaid.elapsed() >= 1000 ) {
			lastSaidHour = hour;
			lastSaidWeather = wk;
			std::fprintf( stderr, "weather recx: %s -> %s%s\n", qPrintable( src ),
				next.on ? qPrintable( next.key ) : "no value set",
				pct < 1.0f ? qPrintable( QStringLiteral( " (blend %1 from %2)" ).arg( double( pct ), 0, 'f', 3 ).arg( srcLast ) ) : "" );
			lastSaid.start();
			lastSrc = src;
		}
	}
	cur = next;
	return cur;
}

void wwWeatherRecxHaze( float haze[3], bool recordHasFog, float clearAirPerKm )
{
	const WwWeatherRecx & r = wwWeatherRecx();
	if ( !r.on || ( r.recordFirst && recordHasFog ) )
		return;	// red recordfirst: the record's fog wins over the file (the wrong order)
	/* lane AIRPERF1 A8 (bungo 11-11, the augmentation rule): a value a FILE gave replaces the record's haze only under
	 * the entry's physicalWeather switch (or the Scene window's live value); without it the record's fog decides. The
	 * generated rung's values are NifSkope's own and stand as before. Under the switch the record is no floor either:
	 * the haze height's flat part (the record's fog height) is 0. Red WW_WEATHER_RECX_RED=noswitch: files replace
	 * without the switch and the flat part stays the record's (before). */
	const bool before = redWord() == QLatin1String( "noswitch" );
	const bool phys = wwRecxSourceIsFile( r.sources, "physicalWeather" );
	const bool visFile = wwRecxSourceIsFile( r.sources, "visibility" ), hhFile = wwRecxSourceIsFile( r.sources, "hazeHeight" );
	const bool visLive = wwWxOv( WwWx::HazeVisibility, -2.0f ) != -2.0f, hhLive = wwWxOv( WwWx::HazeHeight, -2.0f ) != -2.0f;
	if ( r.visibilityM >= 0.0f && ( before || phys || visLive || !visFile ) )
		haze[0] = hazeFromVisibility( r.visibilityM, clearAirPerKm );
	if ( r.hazeHeightM >= 0.0f && ( before || phys || hhLive || !hhFile ) ) {
		haze[1] = std::max( 0.02f, r.hazeHeightM / 1000.0f );
		if ( phys && hhFile && !before )
			haze[2] = 0.0f;
	}
	static int said = -1;
	const int state = ( before ? 1 : 0 ) | ( phys ? 2 : 0 ) | ( visFile ? 4 : 0 ) | ( hhFile ? 8 : 0 );
	if ( state != said ) {
		said = state;
		std::fprintf( stderr, "weather recx haze: switch=%s visibility=%s hazeHeight=%s flat=%s%s\n", phys ? "on" : "off",
			!visFile ? "generated" : ( before || phys || visLive ) ? "recx" : "record",
			!hhFile ? "generated" : ( before || phys || hhLive ) ? "recx" : "record",
			phys && hhFile && !before ? "0(recx)" : "record", before ? " RED=noswitch" : "" );
	}
}

float wwWeatherRecxOzone()
{
	const WwWeatherRecx & r = wwWeatherRecx();
	return r.on && r.ozone >= 0.0f ? r.ozone : 1.0f;
}

bool wwWeatherRecxHazeOptics( float albedo[3], float & angstrom, float & g, float & airglow )
{
	const WwWeatherRecx & r = wwWeatherRecx();
	if ( !r.on || r.hazeType.isEmpty() )
		return false;
	const WwWeatherGen::HazeType * h = WwWeatherGen::hazeType( r.hazeType.toStdString() );
	if ( !h )
		return false;
	for ( int i = 0; i < 3; i++ )
		albedo[i] = r.hazeAlbedo[i] >= 0.0f ? r.hazeAlbedo[i] : h->albedo[i];
	angstrom = r.angstrom >= 0.0f ? r.angstrom : h->angstrom;
	g = r.hazeAnisotropy >= 0.0f ? r.hazeAnisotropy : h->g;
	airglow = r.airglow >= 0.0f ? r.airglow : h->airglow;
	if ( r.humidity >= 0.0f ) {	// the growth applies to the preset's values only (an override is the author's own)
		float a2[3] = { albedo[0], albedo[1], albedo[2] }, an2 = angstrom, g2 = g;
		WwWeatherGen::humidityGrowth( r.humidity, a2, an2, g2 );
		if ( r.hazeAlbedo[0] < 0.0f )
			for ( int i = 0; i < 3; i++ )
				albedo[i] = a2[i];
		if ( r.angstrom < 0.0f )
			angstrom = an2;
		if ( r.hazeAnisotropy < 0.0f )
			g = g2;
	}
	return true;
}

bool wwWeatherRecxWindBytes( float & speed, float & dir, float & turb )
{
	const WwWeatherRecx & r = wwWeatherRecx();
	if ( !r.on || r.windSpeed < 0.0f )
		return false;
	speed = std::clamp( 255.0f * r.windSpeed / 20.0f, 0.0f, 255.0f );	// the consumers' byte scale: 255 = 20 m/s
	const float a = std::fmod( 270.0f - ( r.windDirection >= 0.0f ? r.windDirection : 270.0f ) + 720.0f, 360.0f );
	dir = a / 360.0f * 255.0f;	// the inverse of the rung's FROM = (270 - byte / 255 * 360) mod 360
	turb = r.turbulence >= 0.0f ? 255.0f * r.turbulence : 0.0f;
	return true;
}

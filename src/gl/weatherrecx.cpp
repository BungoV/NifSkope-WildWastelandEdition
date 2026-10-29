#include "weatherrecx.h"
#include "wwswitch.h"
#include "gl/celllooks.h"
#include "gl/lookdevstage.h"
#include "gl/weatherrecxmath.h"
#include "recx.h"

#include <QElapsedTimer>
#include <QJsonObject>
#include <QPair>
#include <QSet>
#include <QStringList>
#include <QVector>

#include <cstdio>

using namespace WwWeatherRecxMath;

namespace
{

QString redWord()
{
	return wwEnvStr( "WW_WEATHER_RECX_RED" ).trimmed().toLower();
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
	{ "hazeHeight", kHazeHMin, kHazeHMax, &WwWeatherRecx::hazeHeightM }, { "ozone", 0.0f, kOzoneMax, &WwWeatherRecx::ozone } };
const Key kClouds[] = { { "coverage", 0.0f, 1.0f, &WwWeatherRecx::coverage },
	{ "density", 0.0f, kDensityMax, &WwWeatherRecx::density }, { "type", 0.0f, 1.0f, &WwWeatherRecx::type } };

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
void readKeys( const Obj & b, const Key ( &keys )[N], double hour, WwWeatherRecx & r )
{
	const QJsonValue hv = b.get( "hours" );
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
		L.unknownKeys( { QStringLiteral( "base" ), QStringLiteral( "top" ) } );
		double base = 0.0, top = 0.0;
		const bool hb = L.num( "base", kLayerMin, kLayerMax, base ), ht = L.num( "top", kLayerMin, kLayerMax, top );
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
	return k;
}

}	// namespace

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
	if ( wasOn && since.isValid() && since.elapsed() < 100 && hour == lastHour && wk == lastWeather && rw == lastRed )
		return cur;
	wasOn = true;
	since.start();
	lastHour = hour;
	lastWeather = wk;
	lastRed = rw;

	WwWeatherRecx next;
	next.recordFirst = rw == QLatin1String( "recordfirst" );
	quint32 formID = 0;
	QString edid, owner, dataDir;
	QStringList files;
	QString src;
	if ( wwLookdevWeatherId( formID, edid, owner, files, dataDir ) ) {
		const quint32 lid = ( formID >> 24 ) == 0xFEu ? ( formID & 0xFFFu ) : ( formID & 0xFFFFFFu );
		static QString rootsFor;
		static QStringList roots;
		const QString rk = files.join( QLatin1Char( ';' ) ) + QLatin1Char( '|' ) + dataDir;
		if ( rk != rootsFor ) {
			rootsFor = rk;
			roots = wwRecxRoots( files, dataDir );
		}
		src = QStringLiteral( "%1 %2|0x%3" ).arg( edid, owner ).arg( lid, 8, 16, QLatin1Char( '0' ) );
		for ( const char * block : { "atmosphere", "clouds", "night" } ) {
			const QString bn = QLatin1String( block );
			const WwRecxHit h = wwRecxFind( roots, files, QStringLiteral( "Weathers" ), QStringLiteral( "weather" ), owner, lid,
				edid, bn, true );
			src += QStringLiteral( " %1 %2" ).arg( bn, h.ok ? h.rung + QLatin1Char( ':' ) + h.file.section( QLatin1Char( '/' ), -3 )
				: QStringLiteral( "none" ) );
			if ( !h.ok )
				continue;
			const Obj b{ h.block, h.file.section( QLatin1Char( '/' ), -3 ) + QStringLiteral( " /" ) + bn };
			if ( bn == QLatin1String( "atmosphere" ) ) {
				b.unknownKeys( { QStringLiteral( "visibility" ), QStringLiteral( "hazeHeight" ), QStringLiteral( "ozone" ),
					QStringLiteral( "hours" ) } );
				readKeys( b, kAtmo, hour, next );
			} else if ( bn == QLatin1String( "clouds" ) ) {
				b.unknownKeys( { QStringLiteral( "coverage" ), QStringLiteral( "density" ), QStringLiteral( "layers" ),
					QStringLiteral( "type" ), QStringLiteral( "hours" ) } );
				const Obj bc{ typeWords( b.o, b.where ), b.where };	// lane CLOUD5: clouds.type words -> numbers
				readKeys( bc, kClouds, hour, next );
				readLayers( b, next );
			} else {
				b.unknownKeys( { QStringLiteral( "stars" ), QStringLiteral( "moonlight" ), QStringLiteral( "hours" ) } );
				readKeys( b, kNight, hour, next );
			}
		}
	} else {
		src = QStringLiteral( "none(no weather)" );
	}
	next.on = next.visibilityM >= 0.0f || next.hazeHeightM >= 0.0f || next.ozone >= 0.0f || next.coverage >= 0.0f
		|| next.density >= 0.0f || next.layers > 0 || next.stars >= 0.0f || next.moonlight >= 0.0f || next.type >= 0.0f;
	next.key = next.on ? keyOf( next ) : QString();
	next.stamp = cur.stamp;
	if ( next.key != cur.key ) {
		next.stamp = cur.stamp + 1;
		// the census: one line when the weather, its files or a value moves (an hour-keyed value moves with the clock:
		// at most one line a second then)
		static QElapsedTimer lastSaid;
		static QString lastSrc;
		static double lastSaidHour = -1.0;
		static QString lastSaidWeather;
		// only the clock moved an hour-keyed value (same weather, same files)
		const bool clockOnly = src == lastSrc && wk == lastSaidWeather && hour != lastSaidHour;
		if ( !clockOnly || !lastSaid.isValid() || lastSaid.elapsed() >= 1000 ) {
			lastSaidHour = hour;
			lastSaidWeather = wk;
			std::fprintf( stderr, "weather recx: %s -> %s\n", qPrintable( src ),
				next.on ? qPrintable( next.key ) : "no value set" );
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
	if ( r.visibilityM >= 0.0f )
		haze[0] = hazeFromVisibility( r.visibilityM, clearAirPerKm );
	if ( r.hazeHeightM >= 0.0f )
		haze[1] = std::max( 0.02f, r.hazeHeightM / 1000.0f );
}

float wwWeatherRecxOzone()
{
	const WwWeatherRecx & r = wwWeatherRecx();
	return r.on && r.ozone >= 0.0f ? r.ozone : 1.0f;
}

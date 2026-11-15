#ifndef WW_WEATHERRECXCANON_H
#define WW_WEATHERRECXCANON_H

/* lane WEATHERRECX2 (SPEC.md 1.8): a weather block in the spec's 1.8 spelling turned into the names the reader keeps
 * (weatherrecx.cpp Key tables), before any key is read. Header only (Qt Core), so the offline gates read the same rule.
 *
 *   1.8 name                               reader name                       old name (read as it, with a line)
 *   atmosphere.haze.{height,type,albedo,   hazeHeight, hazeType, hazeAlbedo, atmosphere.hazeHeight ...
 *     glow,anisotropy}                     hazeGlow, hazeAnisotropy
 *   atmosphere.volumetric.emissive         hazeGlow                          (an alias of haze.glow)
 *   atmosphere.directionalInscatter.       directionalInscatter, ...Exponent atmosphere.directionalInscatter (rgb),
 *     {color,exponent,start}               ...Start                          ...Exponent, ...Start
 *   clouds.shadow.{opacity,strength,       shadowOpacity ...                 clouds.shadowOpacity ...
 *     layerDensity,deck}
 *   precipitation.base                     precipitationBase                 clouds.precipitationBase
 *   precipitation.fadeIn / fadeOut         precipitationStart / End          transitions.precipitationStart / End
 *   lightning.fadeIn / fadeOut             lightningStart / End              transitions.lightningStart / End
 * Units by the entry's version (the file's "version", recx.cpp): from 1.8 lightning.distance is metres and
 * precipitation.wetting / drying are hours; older files keep km and minutes (a line says which was read).
 * Any other nested group of a block (secondFog, volumetric, multiScattering, snowCover) becomes dotted keys
 * ("volumetric.extinctionScale") for the stored-for-later keys. Both spellings at once: the 1.8 one wins, the old one is
 * logged "ignored". Each `hours` object gets the same rule. */

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

namespace WwWeatherCanon
{

//! the entry's "version" ("1.8" or 1.8); 0 when absent or unreadable
inline double versionOf( const QJsonObject & entry )
{
	const QJsonValue v = entry.value( QStringLiteral( "version" ) );
	if ( v.isDouble() )
		return v.toDouble();
	bool ok = false;
	const double d = v.toString().trimmed().toDouble( &ok );
	return ok ? d : 0.0;
}

constexpr double kUnitsVersion = 1.8;	// metres / hours from here on

struct Ren
{
	const char * block;
	const char * group;	// nullptr: a flat key of the block
	const char * sub;	// the 1.8 key (in the group, or flat)
	const char * internal;	// the reader's name
	const char * old;	// the old flat name of the same block (nullptr: none in this block)
};

inline const Ren * renames( int & n )
{
	static const Ren k[] = {
		{ "atmosphere", "haze", "height", "hazeHeight", "hazeHeight" },
		{ "atmosphere", "haze", "type", "hazeType", "hazeType" },
		{ "atmosphere", "haze", "albedo", "hazeAlbedo", "hazeAlbedo" },
		{ "atmosphere", "haze", "glow", "hazeGlow", "hazeGlow" },
		{ "atmosphere", "haze", "anisotropy", "hazeAnisotropy", "hazeAnisotropy" },
		{ "atmosphere", "volumetric", "emissive", "hazeGlow", nullptr },
		{ "atmosphere", "directionalInscatter", "color", "directionalInscatter", nullptr },
		{ "atmosphere", "directionalInscatter", "exponent", "directionalInscatterExponent", "directionalInscatterExponent" },
		{ "atmosphere", "directionalInscatter", "start", "directionalInscatterStart", "directionalInscatterStart" },
		{ "clouds", "shadow", "opacity", "shadowOpacity", "shadowOpacity" },
		{ "clouds", "shadow", "strength", "shadowStrength", "shadowStrength" },
		{ "clouds", "shadow", "layerDensity", "shadowLayerDensity", "shadowLayerDensity" },
		{ "clouds", "shadow", "deck", "shadowDeck", "shadowDeck" },
		{ "precipitation", nullptr, "base", "precipitationBase", nullptr },
		{ "precipitation", nullptr, "fadeIn", "precipitationStart", nullptr },
		{ "precipitation", nullptr, "fadeOut", "precipitationEnd", nullptr },
		{ "lightning", nullptr, "fadeIn", "lightningStart", nullptr },
		{ "lightning", nullptr, "fadeOut", "lightningEnd", nullptr },
	};
	n = int( sizeof( k ) / sizeof( k[0] ) );
	return k;
}

//! old names that now live in ANOTHER block: block, old key, its 1.8 home
struct Moved
{
	const char * block;
	const char * old;
	const char * now;
};
inline const Moved * moved( int & n )
{
	static const Moved k[] = { { "clouds", "precipitationBase", "precipitation.base" },
		{ "transitions", "precipitationStart", "precipitation.fadeIn" }, { "transitions", "precipitationEnd", "precipitation.fadeOut" },
		{ "transitions", "lightningStart", "lightning.fadeIn" }, { "transitions", "lightningEnd", "lightning.fadeOut" },
		{ "clouds", "density", "clouds.liquidWater" } };
	n = int( sizeof( k ) / sizeof( k[0] ) );
	return k;
}

inline QString keyOf( const QJsonObject & o, const QString & name )
{
	for ( auto it = o.constBegin(); it != o.constEnd(); ++it )
		if ( it.key().compare( name, Qt::CaseInsensitive ) == 0 )
			return it.key();
	return QString();
}

//! one block object (or one of its hours) in reader names; `notes` gets one line per alias, ignore, unit or drop
inline QJsonObject canon( const QString & block, QJsonObject o, double version, const QString & where, QStringList & notes,
	bool isHour = false )
{
	int nr = 0;
	const Ren * rn = renames( nr );
	QJsonObject from18;	// internal name -> the 1.8 value
	QJsonObject from18Names;	// internal name -> the 1.8 spelling (for the lines)
	// 1. nested groups: mapped subkeys to reader names, the rest to dotted keys
	QStringList groups;
	for ( auto it = o.constBegin(); it != o.constEnd(); ++it )
		if ( it.value().isObject() && it.key().compare( QLatin1String( "hours" ), Qt::CaseInsensitive ) != 0
			&& it.key().compare( QLatin1String( "layers" ), Qt::CaseInsensitive ) != 0 )
			groups << it.key();
	for ( const QString & g : groups ) {
		const QJsonObject go = o.value( g ).toObject();
		o.remove( g );
		for ( auto jt = go.constBegin(); jt != go.constEnd(); ++jt ) {
			if ( jt.key().startsWith( QLatin1Char( '_' ) ) )
				continue;
			bool mapped = false;
			for ( int i = 0; i < nr && !mapped; i++ )
				if ( block == QLatin1String( rn[i].block ) && rn[i].group && g.compare( QLatin1String( rn[i].group ), Qt::CaseInsensitive ) == 0
					&& jt.key().compare( QLatin1String( rn[i].sub ), Qt::CaseInsensitive ) == 0 ) {
					mapped = true;
					const QString in = QLatin1String( rn[i].internal );
					if ( from18.contains( in ) ) {
						notes << QStringLiteral( "%1: %2.%3 ignored (%4 set)" ).arg( where, g, jt.key(), from18Names.value( in ).toString() );
					} else {
						from18.insert( in, jt.value() );
						from18Names.insert( in, g + QLatin1Char( '.' ) + jt.key() );
					}
				}
			if ( !mapped )
				o.insert( g + QLatin1Char( '.' ) + jt.key(), jt.value() );
		}
	}
	// 2. flat 1.8 keys of the block
	for ( int i = 0; i < nr; i++ ) {
		if ( rn[i].group || block != QLatin1String( rn[i].block ) )
			continue;
		const QString k = keyOf( o, QLatin1String( rn[i].sub ) );
		if ( k.isEmpty() )
			continue;
		const QJsonValue v = o.value( k );
		o.remove( k );
		from18.insert( QLatin1String( rn[i].internal ), v );
		from18Names.insert( QLatin1String( rn[i].internal ), k );
	}
	// 3. old names: read as the 1.8 key unless that is set
	for ( int i = 0; i < nr; i++ ) {
		if ( block != QLatin1String( rn[i].block ) )
			continue;
		const QString in = QLatin1String( rn[i].internal );
		const QString now = rn[i].group ? QStringLiteral( "%1.%2" ).arg( QLatin1String( rn[i].group ), QLatin1String( rn[i].sub ) )
										: QLatin1String( rn[i].sub );
		QString k = rn[i].old ? keyOf( o, QLatin1String( rn[i].old ) ) : QString();
		// the old rgb directionalInscatter (a colour, not an object)
		if ( !rn[i].old && rn[i].group && QLatin1String( rn[i].sub ) == QLatin1String( "color" ) ) {
			const QString d = keyOf( o, QLatin1String( rn[i].internal ) );
			if ( !d.isEmpty() && !o.value( d ).isObject() )
				k = d;
		}
		if ( k.isEmpty() )
			continue;
		if ( from18.contains( in ) ) {
			notes << QStringLiteral( "%1: %2 ignored (%3 set)" ).arg( where, k, now );
			o.remove( k );
		} else {
			notes << QStringLiteral( "%1: \"%2\" is the old name of \"%3\" (read as it)" ).arg( where, k, now );
		}
	}
	for ( auto it = from18.constBegin(); it != from18.constEnd(); ++it ) {
		const QString k = keyOf( o, it.key() );
		if ( !k.isEmpty() )
			o.remove( k );
		o.insert( it.key(), it.value() );
	}
	int nm = 0;
	const Moved * mv = moved( nm );
	for ( int i = 0; i < nm; i++ )
		if ( block == QLatin1String( mv[i].block ) && !keyOf( o, QLatin1String( mv[i].old ) ).isEmpty()
			&& QLatin1String( mv[i].old ) != QLatin1String( "density" ) )	// density keeps its own 1.4 legacy line
			notes << QStringLiteral( "%1: \"%2\" is the old name of \"%3\" (read as it)" ).arg( where, QLatin1String( mv[i].old ),
				QLatin1String( mv[i].now ) );
	// 4. units by version
	auto scale = [&]( const char * key, double f, const char * newUnit, const char * oldUnit ) {
		const QString k = keyOf( o, QLatin1String( key ) );
		if ( k.isEmpty() || !o.value( k ).isDouble() )
			return;
		if ( version >= kUnitsVersion ) {
			o.insert( k, o.value( k ).toDouble() * f );
		} else {
			notes << QStringLiteral( "%1: %2 read in %3 (file version %4 < 1.8; from 1.8 it is %5)" ).arg( where, k,
				QLatin1String( oldUnit ), version > 0.0 ? QString::number( version ) : QStringLiteral( "none" ), QLatin1String( newUnit ) );
		}
	};
	if ( block == QLatin1String( "lightning" ) )
		scale( "distance", 0.001, "m", "km" );
	if ( block == QLatin1String( "precipitation" ) ) {
		scale( "wetting", 60.0, "hours", "minutes" );
		scale( "drying", 60.0, "hours", "minutes" );
	}
	// 5. hours
	if ( !isHour ) {
		const QString hk = keyOf( o, QStringLiteral( "hours" ) );
		if ( !hk.isEmpty() && o.value( hk ).isObject() ) {
			QJsonObject h = o.value( hk ).toObject();
			for ( auto jt = h.begin(); jt != h.end(); ++jt )
				if ( jt.value().isObject() )
					jt.value() = canon( block, jt.value().toObject(), version, where + QStringLiteral( "/hours/" ) + jt.key(), notes, true );
			o.insert( hk, h );
		}
	}
	return o;
}

}	// namespace WwWeatherCanon

#endif	// WW_WEATHERRECXCANON_H

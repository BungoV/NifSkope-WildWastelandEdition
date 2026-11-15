#ifndef WW_RECXPHYSICAL_H
#define WW_RECXPHYSICAL_H

/* lane AIRPERF1 A8 (bungo 11-11): a .recx is an augmentation of the records. A record's look part is replaced only
 * by a switch or when that part is written in the file. The one switch of a weather entry (.recx spec,
 * "physicalWeather": true beside the entry's match keys; any case, default off): the entry's physically based
 * values replace the record's weather look for the parts they cover, and the record is no fit target, clamp, floor
 * or blend for them. Off or absent: the record decides; the values are additional information.
 * lane WEATHERRECX2 (spec 1.8): "fromRecord" replaces physicalWeather and atmosphere.useRecordColors. true = the
 * record decides (the switch off); false or a list = the switch on, and a listed function (a colour channel sky, sun,
 * ambient, fog, clouds, imagespace, or a family night, wind, lightning, precipitation) stays the record's. */

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

inline QJsonValue wwRecxFromRecord( const QJsonObject & entry )
{
	for ( auto it = entry.constBegin(); it != entry.constEnd(); ++it )
		if ( it.key().compare( QLatin1String( "fromRecord" ), Qt::CaseInsensitive ) == 0 )
			return it.value();
	return QJsonValue( QJsonValue::Undefined );
}

//! the entry's fromRecord list names this function (case-free)
inline bool wwRecxFromRecordNames( const QJsonObject & entry, const QString & name )
{
	const QJsonValue v = wwRecxFromRecord( entry );
	if ( !v.isArray() )
		return false;
	for ( const QJsonValue & x : v.toArray() )
		if ( x.toString().compare( name, Qt::CaseInsensitive ) == 0 )
			return true;
	return false;
}

inline bool wwRecxPhysicalWeather( const QJsonObject & entry )
{
	const QJsonValue fr = wwRecxFromRecord( entry );
	if ( fr.isBool() || fr.isArray() )
		return fr.isArray() || !fr.toBool();
	for ( auto it = entry.constBegin(); it != entry.constEnd(); ++it )
		if ( it.key().compare( QLatin1String( "physicalWeather" ), Qt::CaseInsensitive ) == 0 )
			return it.value().isBool() && it.value().toBool();
	return false;
}

//! a resolved weather's sources list ("<key>=<source>") names a FILE for this key
inline bool wwRecxSourceIsFile( const QStringList & sources, const char * key )
{
	const QString pre = QLatin1String( key ) + QStringLiteral( "=file:" );
	for ( const QString & s : sources )
		if ( s.startsWith( pre ) )
			return true;
	return false;
}

#endif

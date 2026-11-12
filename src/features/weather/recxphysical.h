#ifndef WW_RECXPHYSICAL_H
#define WW_RECXPHYSICAL_H

/* lane AIRPERF1 A8 (bungo 11-11): a .recx is an augmentation of the records. A record's look part is replaced only
 * by a switch or when that part is written in the file. The one switch of a weather entry (.recx spec,
 * "physicalWeather": true beside the entry's match keys; any case, default off): the entry's physically based
 * values replace the record's weather look for the parts they cover, and the record is no fit target, clamp, floor
 * or blend for them. Off or absent: the record decides; the values are additional information. */

#include <QJsonObject>
#include <QString>
#include <QStringList>

inline bool wwRecxPhysicalWeather( const QJsonObject & entry )
{
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

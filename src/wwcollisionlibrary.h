#ifndef WWCOLLISIONLIBRARY_H
#define WWCOLLISIONLIBRARY_H

#include "wwlibrary.h"

#include <QJsonObject>
#include <QVariantMap>

namespace WwCollisionLibrary
{

inline constexpr auto Feature = "Collision";
inline constexpr auto PresetsFile = "Presets.json";
inline constexpr auto MaterialsFile = "CustomMaterials.json";

inline bool canReplaceLibraryFile( const QString & filename )
{
	bool valid = false, exists = false;
	const QJsonDocument json = WwLibrary::readJson(
		QLatin1String( Feature ), filename, &valid, &exists );
	return !exists || ( valid && json.isObject() );
}

inline bool writePresets( const QVariantMap & presets )
{
	if ( !canReplaceLibraryFile( QLatin1String( PresetsFile ) ) ) return false;
	QJsonObject root;
	root.insert( QStringLiteral( "formatVersion" ), 1 );
	root.insert( QStringLiteral( "presets" ), QJsonObject::fromVariantMap( presets ) );
	return WwLibrary::writeJson( QLatin1String( Feature ),
		QLatin1String( PresetsFile ), QJsonDocument( root ) );
}

inline QVariantMap presets()
{
	bool valid = false, exists = false;
	const QJsonDocument json = WwLibrary::readJson( QLatin1String( Feature ),
		QLatin1String( PresetsFile ), &valid, &exists );
	if ( exists )
		return valid && json.isObject()
			? json.object().value( QStringLiteral( "presets" ) ).toObject().toVariantMap()
			: QVariantMap();

	// One-time import from builds that stored authored presets in the registry.
	QSettings settings;
	settings.beginGroup( QStringLiteral( "CollisionManager/Presets" ) );
	QVariantMap migrated;
	for ( const QString & name : settings.childGroups() ) {
		settings.beginGroup( name );
		QVariantMap values;
		for ( const QString & key : settings.childKeys() ) values.insert( key, settings.value( key ) );
		settings.endGroup();
		migrated.insert( name, values );
	}
	settings.endGroup();
	if ( !migrated.isEmpty() && writePresets( migrated ) )
		settings.remove( QStringLiteral( "CollisionManager/Presets" ) );
	return migrated;
}

inline bool writeCustomMaterials( const QVariantMap & materials )
{
	if ( !canReplaceLibraryFile( QLatin1String( MaterialsFile ) ) ) return false;
	QJsonObject root;
	root.insert( QStringLiteral( "formatVersion" ), 1 );
	root.insert( QStringLiteral( "materials" ), QJsonObject::fromVariantMap( materials ) );
	return WwLibrary::writeJson( QLatin1String( Feature ),
		QLatin1String( MaterialsFile ), QJsonDocument( root ) );
}

inline QVariantMap customMaterials()
{
	bool valid = false, exists = false;
	const QJsonDocument json = WwLibrary::readJson( QLatin1String( Feature ),
		QLatin1String( MaterialsFile ), &valid, &exists );
	if ( exists )
		return valid && json.isObject()
			? json.object().value( QStringLiteral( "materials" ) ).toObject().toVariantMap()
			: QVariantMap();

	QSettings settings;
	const QString legacyKey = QStringLiteral( "CollisionManager/CustomMaterials" );
	const QVariantMap migrated = settings.value( legacyKey ).toMap();
	if ( !migrated.isEmpty() && writeCustomMaterials( migrated ) ) settings.remove( legacyKey );
	return migrated;
}

} // namespace WwCollisionLibrary

#endif // WWCOLLISIONLIBRARY_H

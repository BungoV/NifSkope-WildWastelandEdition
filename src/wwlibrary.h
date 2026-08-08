#ifndef WWLIBRARY_H
#define WWLIBRARY_H

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

namespace WwLibrary
{

inline constexpr auto SettingsKey = "Settings/Library/Library Folder";

inline QString rootFolder( bool create = true )
{
	QString root = QSettings().value( QLatin1String( SettingsKey ) ).toString();
	if ( root.isEmpty() ) {
		const QString documents = QStandardPaths::writableLocation(
			QStandardPaths::DocumentsLocation );
		root = QDir( documents.isEmpty() ? QDir::homePath() : documents )
			.filePath( QStringLiteral( "NifSkope" ) );
	}
	if ( create && !QDir().mkpath( root ) ) return QString();
	return QDir::cleanPath( root );
}

inline QString featureFile( const QString & feature, const QString & filename,
	bool createFolder = false )
{
	const QString root = rootFolder( createFolder );
	if ( root.isEmpty() ) return QString();
	const QString folder = QDir( root ).filePath( feature );
	if ( createFolder && !QDir().mkpath( folder ) ) return QString();
	return QDir( folder ).filePath( filename );
}

inline QJsonDocument readJson( const QString & feature, const QString & filename,
	bool * valid = nullptr, bool * exists = nullptr )
{
	if ( valid ) *valid = true;
	const QString path = featureFile( feature, filename );
	QFile file( path );
	if ( exists ) *exists = file.exists();
	if ( !file.exists() ) return {};
	if ( !file.open( QIODevice::ReadOnly ) ) {
		if ( valid ) *valid = false;
		return {};
	}
	QJsonParseError error;
	const QJsonDocument json = QJsonDocument::fromJson( file.readAll(), &error );
	if ( error.error != QJsonParseError::NoError || json.isNull() ) {
		if ( valid ) *valid = false;
		return {};
	}
	return json;
}

inline bool writeJson( const QString & feature, const QString & filename,
	const QJsonDocument & json )
{
	const QString path = featureFile( feature, filename, true );
	if ( path.isEmpty() ) return false;
	QSaveFile file( path );
	if ( !file.open( QIODevice::WriteOnly ) ) return false;
	const QByteArray bytes = json.toJson( QJsonDocument::Indented );
	if ( file.write( bytes ) != bytes.size() ) {
		file.cancelWriting();
		return false;
	}
	return file.commit();
}

} // namespace WwLibrary

#endif // WWLIBRARY_H

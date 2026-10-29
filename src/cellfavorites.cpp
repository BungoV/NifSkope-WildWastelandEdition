/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellfavorites.h"
#include "wwswitch.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kCell = 4096.0f;

QJsonArray vec3( const float * v )
{
	return QJsonArray{ double( v[0] ), double( v[1] ), double( v[2] ) };
}

bool readVec3( const QJsonValue & j, float * v )
{
	const QJsonArray a = j.toArray();
	if ( a.size() != 3 )
		return false;
	for ( int i = 0; i < 3; i++ )
		v[i] = float( a.at( i ).toDouble() );
	return true;
}
}

QString cellFavoritesPath()
{
	const QString env = wwEnvStr( "WW_CELL_FAVORITES" ).trimmed();
	if ( !env.isEmpty() )
		return env;
	return QStandardPaths::writableLocation( QStandardPaths::AppDataLocation ) + QStringLiteral( "/camera_favorites.json" );
}

QString cellFavoritesKey( bool interior, const QString & world, const QString & interiorCell )
{
	return interior ? QStringLiteral( "interior/" ) + interiorCell.trimmed().toLower()
		: QStringLiteral( "world/" ) + world.trimmed().toLower();
}

bool cellFavoritesRead( const QString & path, CellFavoriteStore & out, QString * err )
{
	out = CellFavoriteStore();
	QFile f( path );
	if ( !f.exists() )
		return true;
	if ( !f.open( QIODevice::ReadOnly ) ) {
		if ( err )
			*err = QStringLiteral( "cannot read %1" ).arg( path );
		return false;
	}
	QJsonParseError pe;
	const QJsonDocument doc = QJsonDocument::fromJson( f.readAll(), &pe );
	if ( !doc.isObject() ) {
		if ( err )
			*err = QStringLiteral( "%1: %2" ).arg( path, pe.errorString() );
		return false;
	}
	const QJsonObject root = doc.object();
	out.counter = qint64( root.value( QStringLiteral( "counter" ) ).toDouble() );
	const QJsonObject lists = root.value( QStringLiteral( "lists" ) ).toObject();
	for ( auto it = lists.begin(); it != lists.end(); ++it ) {
		QVector<CellFavorite> & list = out.lists[it.key()];
		for ( const QJsonValue & v : it.value().toArray() ) {
			const QJsonObject o = v.toObject();
			CellFavorite c;
			c.name = o.value( QStringLiteral( "name" ) ).toString();
			if ( c.name.isEmpty() || !readVec3( o.value( QStringLiteral( "eye" ) ), c.eye ) )
				continue;
			readVec3( o.value( QStringLiteral( "at" ) ), c.at );
			c.yawDeg = float( o.value( QStringLiteral( "yaw" ) ).toDouble() );
			c.pitchDeg = float( o.value( QStringLiteral( "pitch" ) ).toDouble() );
			c.rollDeg = float( o.value( QStringLiteral( "roll" ) ).toDouble() );
			c.fovDeg = float( o.value( QStringLiteral( "fov" ) ).toDouble() );
			c.dist = float( o.value( QStringLiteral( "dist" ) ).toDouble( 64.0 ) );
			c.used = qint64( o.value( QStringLiteral( "used" ) ).toDouble() );
			out.counter = std::max( out.counter, c.used );
			list.append( c );
		}
	}
	return true;
}

bool cellFavoritesWrite( const QString & path, const CellFavoriteStore & store, QString * err )
{
	const bool red = wwEnvStr( "WW_CELL_FAVORITES_RED" ).trimmed() == QLatin1String( "angles" );
	QJsonObject lists;
	for ( auto it = store.lists.begin(); it != store.lists.end(); ++it ) {
		if ( it.value().isEmpty() )
			continue;
		QJsonArray a;
		for ( const CellFavorite & c : it.value() ) {
			QJsonObject o;
			o.insert( QStringLiteral( "name" ), c.name );
			o.insert( QStringLiteral( "eye" ), vec3( c.eye ) );
			o.insert( QStringLiteral( "at" ), vec3( c.at ) );
			o.insert( QStringLiteral( "yaw" ), red ? 0.0 : double( c.yawDeg ) );
			o.insert( QStringLiteral( "pitch" ), red ? 0.0 : double( c.pitchDeg ) );
			o.insert( QStringLiteral( "roll" ), double( c.rollDeg ) );
			o.insert( QStringLiteral( "fov" ), double( c.fovDeg ) );
			o.insert( QStringLiteral( "dist" ), double( c.dist ) );
			o.insert( QStringLiteral( "used" ), double( c.used ) );
			a.append( o );
		}
		lists.insert( it.key(), a );
	}
	QJsonObject root;
	root.insert( QStringLiteral( "version" ), 1 );
	root.insert( QStringLiteral( "counter" ), double( store.counter ) );
	root.insert( QStringLiteral( "lists" ), lists );
	QDir().mkpath( QFileInfo( path ).absolutePath() );
	QSaveFile f( path );
	if ( !f.open( QIODevice::WriteOnly ) || f.write( QJsonDocument( root ).toJson( QJsonDocument::Indented ) ) < 0
		|| !f.commit() ) {
		if ( err )
			*err = QStringLiteral( "cannot write %1" ).arg( path );
		return false;
	}
	return true;
}

int cellFavoritesStart( const QVector<CellFavorite> & list )
{
	int best = -1;
	for ( int i = 0; i < list.size(); i++ )
		if ( best < 0 || list.at( i ).used > list.at( best ).used )
			best = i;
	return best;
}

int cellFavoritesPick( const QVector<CellFavorite> & list, bool interior, int cx, int cy, int n )
{
	const bool red = wwEnvStr( "WW_CELL_FAVORITES_RED" ).trimmed() == QLatin1String( "range" );
	const int h = std::max( n, 1 ) / 2;
	int best = -1;
	for ( int i = 0; i < list.size(); i++ ) {
		const CellFavorite & c = list.at( i );
		if ( !interior && !red ) {
			const int fx = int( std::floor( c.eye[0] / kCell ) ), fy = int( std::floor( c.eye[1] / kCell ) );
			if ( fx < cx - h || fx > cx + h || fy < cy - h || fy > cy + h )
				continue;
		}
		if ( best < 0 || c.used > list.at( best ).used )
			best = i;
	}
	return best;
}

void cellFavoritesTouch( CellFavoriteStore & store, CellFavorite & f )
{
	f.used = ++store.counter;
}

QString cellFavoritesNewName( const QVector<CellFavorite> & list )
{
	for ( int k = 1;; k++ ) {
		const QString n = QStringLiteral( "Favorite %1" ).arg( k );
		bool taken = false;
		for ( const CellFavorite & c : list )
			taken = taken || c.name.compare( n, Qt::CaseInsensitive ) == 0;
		if ( !taken )
			return n;
	}
}

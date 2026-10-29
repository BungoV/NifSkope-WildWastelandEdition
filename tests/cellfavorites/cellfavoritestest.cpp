// FAVCAM1 unit test: src/cellfavorites.cpp -- the favorites file round trip and the start pick.
// Usage: cellfavoritestest <scratch dir>. Exit 0 = every check passed.
#include "cellfavorites.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>

#include <cmath>
#include <cstdio>

static int g_checks = 0, g_fails = 0;

static void check( bool ok, const char * what )
{
	g_checks++;
	if ( !ok )
		g_fails++;
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what );
}

static bool approx( float a, float b )
{
	return std::fabs( a - b ) < 1.0e-3f;
}

static CellFavorite fav( const char * name, float x, float y, float z, float yaw, float pitch, qint64 used )
{
	CellFavorite c;
	c.name = QString::fromLatin1( name );
	c.eye[0] = x;
	c.eye[1] = y;
	c.eye[2] = z;
	c.yawDeg = yaw;
	c.pitchDeg = pitch;
	c.rollDeg = 0.5f;
	c.fovDeg = 72.5f;
	c.dist = 300.0f;
	c.at[0] = x + 1.0f;
	c.at[1] = y + 2.0f;
	c.at[2] = z + 3.0f;
	c.used = used;
	return c;
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	QCoreApplication app( argc, argv );
	if ( argc < 2 ) {
		std::printf( "usage: cellfavoritestest <scratch dir>\n" );
		return 2;
	}
	const QString dir = QString::fromLocal8Bit( argv[1] );
	QDir().mkpath( dir );
	const QString path = dir + QStringLiteral( "/favorites_test.json" );
	QFile::remove( path );

	// ---- keys
	check( cellFavoritesKey( false, QStringLiteral( "Commonwealth" ), QString() ) == QLatin1String( "world/commonwealth" ),
		"exterior key is the worldspace, lower case" );
	check( cellFavoritesKey( true, QStringLiteral( "interior" ), QStringLiteral( "MemoryDen01" ) )
			== QLatin1String( "interior/memoryden01" ), "interior key is the cell" );

	// ---- a missing file is an empty store, not an error
	CellFavoriteStore s0;
	check( cellFavoritesRead( path, s0 ) && s0.lists.isEmpty() && s0.counter == 0, "missing file reads empty" );

	// ---- round trip
	CellFavoriteStore s;
	QVector<CellFavorite> & cw = s.lists[cellFavoritesKey( false, QStringLiteral( "Commonwealth" ), QString() )];
	cw.append( fav( "Hardware store", -20000.0f, 90000.0f, 1500.0f, 135.0f, -12.5f, 3 ) );
	cw.append( fav( "Red Rocket", 2000.0f, 2000.0f, 900.0f, 45.0f, 20.0f, 7 ) );
	s.lists[cellFavoritesKey( true, QString(), QStringLiteral( "MemoryDen01" ) )].append(
		fav( "Bar", 10.0f, 20.0f, 130.0f, -90.0f, 5.0f, 5 ) );
	s.counter = 7;
	QString err;
	check( cellFavoritesWrite( path, s, &err ), "write" );
	CellFavoriteStore r;
	check( cellFavoritesRead( path, r, &err ), "read back" );
	const QVector<CellFavorite> rw = r.lists.value( QStringLiteral( "world/commonwealth" ) );
	check( rw.size() == 2 && r.lists.value( QStringLiteral( "interior/memoryden01" ) ).size() == 1, "two lists, 2 + 1 rows" );
	bool same = rw.size() == 2;
	for ( int i = 0; same && i < 2; i++ ) {
		const CellFavorite & a = cw.at( i );
		const CellFavorite & b = rw.at( i );
		same = a.name == b.name && a.used == b.used && approx( a.fovDeg, b.fovDeg ) && approx( a.dist, b.dist )
			&& approx( a.rollDeg, b.rollDeg );
		for ( int k = 0; k < 3; k++ )
			same = same && approx( a.eye[k], b.eye[k] ) && approx( a.at[k], b.at[k] );
	}
	check( same, "names, eye, at, roll, fov, dist and order read back" );
	bool angles = rw.size() == 2;
	for ( int i = 0; angles && i < 2; i++ )
		angles = approx( cw.at( i ).yawDeg, rw.at( i ).yawDeg ) && approx( cw.at( i ).pitchDeg, rw.at( i ).pitchDeg );
	check( angles, "where it points (yaw, pitch) reads back" );
	check( r.counter == 7, "counter reads back" );

	// ---- a broken file is refused, not read as empty
	const QString bad = dir + QStringLiteral( "/favorites_bad.json" );
	{
		QFile f( bad );
		f.open( QIODevice::WriteOnly );
		f.write( "{ not json" );
	}
	CellFavoriteStore b;
	check( !cellFavoritesRead( bad, b, &err ) && !err.isEmpty(), "broken file refused with a reason" );

	// ---- the start and the pick
	check( cellFavoritesStart( rw ) == 1, "start = the last saved or jumped to (Red Rocket, used 7)" );
	check( cellFavoritesStart( QVector<CellFavorite>() ) == -1, "empty list has no start" );
	// Red Rocket stands in cell 0,0; the hardware store in cell -5,21
	check( cellFavoritesPick( rw, false, 0, 0, 1 ) == 1, "open 0,0: the start, in range" );
	check( cellFavoritesPick( rw, false, -5, 21, 1 ) == 0, "open -5,21: the in-range one wins over the higher start" );
	check( cellFavoritesPick( rw, false, 10, 10, 5 ) == -1, "open 10,10 n5: none in range, the spawn rule" );
	check( cellFavoritesPick( rw, false, -4, 20, 3 ) == 0, "open -4,20 n3: -5,21 is inside the 3x3" );
	check( cellFavoritesPick( rw, false, -3, 21, 3 ) == -1, "open -3,21 n3: -5 is outside" );
	QVector<CellFavorite> neg{ fav( "edge", -1.0f, 4095.0f, 0.0f, 0.0f, 0.0f, 1 ) };
	check( cellFavoritesPick( neg, false, -1, 0, 1 ) == 0 && cellFavoritesPick( neg, false, 0, 0, 1 ) == -1,
		"x -1 is cell -1, y 4095 is cell 0 (floor)" );
	check( cellFavoritesPick( r.lists.value( QStringLiteral( "interior/memoryden01" ) ), true, 99, 99, 1 ) == 0,
		"an interior takes its start whatever the cell numbers" );

	// ---- touch and names
	QVector<CellFavorite> t = rw;
	cellFavoritesTouch( r, t[0] );
	check( t[0].used == 8 && r.counter == 8 && cellFavoritesStart( t ) == 0, "a jump makes it the start" );
	QVector<CellFavorite> names{ fav( "Favorite 1", 0, 0, 0, 0, 0, 1 ), fav( "favorite 3", 0, 0, 0, 0, 0, 2 ) };
	check( cellFavoritesNewName( names ) == QLatin1String( "Favorite 2" ), "new name takes the first free number" );
	names.append( fav( "Favorite 2", 0, 0, 0, 0, 0, 3 ) );
	check( cellFavoritesNewName( names ) == QLatin1String( "Favorite 4" ), "names compare without case" );

	QFile::remove( path );
	QFile::remove( bad );
	std::printf( "%d checks, %d failures\n%s\n", g_checks, g_fails, g_fails ? "FAIL" : "PASS" );
	return g_fails ? 1 : 0;
}

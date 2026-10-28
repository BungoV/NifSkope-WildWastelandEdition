/* tests/framespeed1 (lane FRAMESPEED1, CONCORDQUIT1): Concord's stream walks quit at their first frames with "refused:
 * cannot read .../con.fly.path". "con" is a Windows device name: QFile opens the console, never the file. The test
 * writes con.fly.path (through \\?\) and a plain-named file held by another handle with DELETE access, then reads
 * both with wwReadFileShared: both must come back whole, with a reason for the first. A path given as argv[2] (the
 * Concord hold's own file) is read too. -DWWSHAREDREAD_RED (QFile only, the code before this lane): SHAREDREAD FAIL.
 * Usage: sharedread <scratch dir> [extra file]   exit 0 = ok */
#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QString>

#include <windows.h>

#include <cstdio>

#include "wwsharedread.h"

static int fails = 0;
static void check( bool ok, const char * what )
{
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what );
	fails += ok ? 0 : 1;
}

static std::wstring longForm( const QString & p )
{
	return ( QStringLiteral( "\\\\?\\" ) + QDir::toNativeSeparators( QFileInfo( p ).absoluteFilePath() ) ).toStdWString();
}

static bool writeLong( const QString & p, const QByteArray & b )
{
	const std::wstring w = longForm( p );
	HANDLE h = CreateFileW( w.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr );
	if ( h == INVALID_HANDLE_VALUE )
		return false;
	DWORD n = 0;
	const bool good = WriteFile( h, b.constData(), DWORD( b.size() ), &n, nullptr ) && n == DWORD( b.size() );
	CloseHandle( h );
	return good;
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	const QString dir = argc > 1 ? QString::fromLocal8Bit( argv[1] ) : QStringLiteral( "." );
	const QByteArray body( "# path\r\n-15 17 0 90\r\n-14 17 0 90\r\n" );

	check( wwDeviceNameStem( QStringLiteral( "E:/x/con.fly.path" ) ) == QStringLiteral( "con" )
	       && wwDeviceNameStem( QStringLiteral( "E:/x/CON" ) ) == QStringLiteral( "con" )
	       && wwDeviceNameStem( QStringLiteral( "E:/x/com7.txt" ) ) == QStringLiteral( "com7" )
	       && wwDeviceNameStem( QStringLiteral( "E:/x/con_a.stream.txt" ) ).isEmpty()
	       && wwDeviceNameStem( QStringLiteral( "E:/x/bos.fly.path" ) ).isEmpty()
	       && wwDeviceNameStem( QStringLiteral( "E:/x/com10.txt" ) ).isEmpty(), "device names: con, CON, com7 yes; con_a, bos, com10 no" );

	// 1. the Concord name
	const QString con = dir + QStringLiteral( "/con.fly.path" );
	check( writeLong( con, body ), "con.fly.path written through \\\\?\\" );
	bool ok = false;
	QString why;
	QByteArray got = wwReadFileShared( con, &ok, &why );
	std::printf( "con why: %s\n", qPrintable( why ) );
	check( ok && got == body, "con.fly.path: read whole" );
	check( why.contains( QStringLiteral( "device name con" ) ), "con.fly.path: the reason names the device" );

	// 2. a plain name, held by another handle with DELETE access (QFile's open has no FILE_SHARE_DELETE)
	const QString held = dir + QStringLiteral( "/held.fly.path" );
	check( writeLong( held, body ), "held.fly.path written" );
	const std::wstring hw = longForm( held );
	HANDLE h = CreateFileW( hw.c_str(), DELETE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
	check( h != INVALID_HANDLE_VALUE, "holder handle opened" );
	ok = false;
	why.clear();
	got = wwReadFileShared( held, &ok, &why );
	std::printf( "held why: %s\n", qPrintable( why ) );
	check( ok && got == body, "held file: read whole" );
	if ( h != INVALID_HANDLE_VALUE )
		CloseHandle( h );

	// 3. a free plain file: QFile reads it, no reason
	ok = false;
	why.clear();
	got = wwReadFileShared( held, &ok, &why );
	check( ok && got == body && why.isEmpty(), "free file: QFile reads it, no reason given" );

	// 4. missing
	ok = true;
	got = wwReadFileShared( dir + QStringLiteral( "/no_such.path" ), &ok, &why );
	check( !ok && got.isEmpty() && !why.isEmpty(), "missing file: refused with a reason" );

	// 5. the Concord hold's own file, read only
	if ( argc > 2 ) {
		const QString extra = QString::fromLocal8Bit( argv[2] );
		ok = false;
		why.clear();
		got = wwReadFileShared( extra, &ok, &why );
		std::printf( "extra %s: %d bytes, why: %s\n", qPrintable( extra ), int( got.size() ), qPrintable( why ) );
		check( ok && got.size() > 0, "the hold's path file: read" );
	}

	std::printf( "SHAREDREAD %s (%d failed)\n", fails ? "FAIL" : "PASS", fails );
	return fails ? 1 : 0;
}

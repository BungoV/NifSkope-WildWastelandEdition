#include "wwsharedread.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

QString wwDeviceNameStem( const QString & path )
{
	static const char * const names[] = { "con", "prn", "aux", "nul", "com1", "com2", "com3", "com4", "com5", "com6",
		"com7", "com8", "com9", "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9" };
	const QString file = QFileInfo( path ).fileName();
	const QString stem = file.section( QLatin1Char( '.' ), 0, 0 ).trimmed().toLower();
	for ( const char * n : names )
		if ( stem == QLatin1String( n ) )
			return stem;
	return QString();
}

QByteArray wwReadFileShared( const QString & path, bool * ok, QString * why )
{
	if ( ok )
		*ok = false;
	const QString device = wwDeviceNameStem( path );
	QString reason;
	if ( device.isEmpty() ) {
		QFile f( path );
		if ( f.open( QIODevice::ReadOnly ) ) {
			if ( ok )
				*ok = true;
			return f.readAll();
		}
		reason = QStringLiteral( "QFile: %1" ).arg( f.errorString() );
	} else {
		// QFile would open the device (CON: the console, a read that never ends)
		reason = QStringLiteral( "the name starts with the Windows device name %1" ).arg( device );
	}
#if defined( Q_OS_WIN ) && !defined( WWSHAREDREAD_RED )
	// \\?\ passes the name to the file system as written, so CON.x is the file again; every share flag, so a
	// scanner or editor holding the file does not refuse the read
	const QString native = QStringLiteral( "\\\\?\\" ) + QDir::toNativeSeparators( QFileInfo( path ).absoluteFilePath() );
	const std::wstring w = native.toStdWString();
	HANDLE h = CreateFileW( w.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
	const DWORD err = h == INVALID_HANDLE_VALUE ? GetLastError() : 0;
	reason += h == INVALID_HANDLE_VALUE ? QStringLiteral( "; the \\\\?\\ open refused, system error %1" ).arg( err )
	                                    : QStringLiteral( "; read through \\\\?\\" );
	QByteArray out;
	if ( h != INVALID_HANDLE_VALUE ) {
		char buf[65536];
		DWORD n = 0;
		bool good = true;
		while ( ( good = ReadFile( h, buf, sizeof buf, &n, nullptr ) != 0 ) && n > 0 )
			out.append( buf, int( n ) );
		CloseHandle( h );
		if ( ok )
			*ok = good;
	}
	if ( why )
		*why = reason;
	return out;
#else
	if ( why )
		*why = reason;
	return QByteArray();
#endif
}

// lane BUGS1102C (SHOTCAPS-COLD): gl/progcachekeep.h on a scratch folder. usage: progcachekeep_test <scratch dir> <keep> [notouch]
// Prints one `case <name> <PASS|FAIL> ...` line a case and `alternate relinks <n>` (two define sets over 8 launches).
#include "gl/progcachekeep.h"

#include <QDateTime>
#include <QDir>
#include <QFile>

#include <cstdio>
#include <cstdlib>
#include <string>

static QString D;
static int fails = 0;

static void touch( const QString & f, int ageSec )
{
	QFile	q( D + '/' + f );
	q.open( QIODevice::WriteOnly );
	q.write( "WWPB" );
	q.setFileTime( QDateTime::currentDateTime().addSecs( -ageSec ), QFileDevice::FileModificationTime );
	q.close();
}

static void check( const char * name, bool ok, const QString & msg )
{
	std::printf( "case %s %s %s\n", name, ok ? "PASS" : "FAIL", msg.toLatin1().constData() );
	if ( !ok )
		fails++;
}

static void clear()
{
	QDir	d( D );
	for ( const QString & f : d.entryList( QDir::Files ) )
		d.remove( f );
}

int main( int argc, char ** argv )
{
	if ( argc < 3 )
		return 2;
	D = QString::fromLocal8Bit( argv[1] );
	const int	keep = wwProgCacheKeep( QByteArray( argv[2] ) );
	const bool	touchOn = argc < 4 || std::string( argv[3] ) != "notouch";	// BUGS1102F: argv[3] notouch = the stamp red
	QDir().mkpath( D );
	clear();
	// 8 older variants of one program, a twin-prefix program, then the new binary
	for ( int i = 0; i < 8; i++ )
		touch( QStringLiteral( "fo4_default.prog-%1.bin" ).arg( 100 + i ), 1000 - i * 10 );
	touch( "fo4_defaultbl.prog-aa.bin", 5000 );
	touch( "fo4_default.prog-new.bin", 0 );
	const int	gone = wwProgCachePrune( D, "fo4_default.prog", "fo4_default.prog-new.bin", keep );
	QDir	d( D );
	const QStringList	left = d.entryList( { "fo4_default.prog-*.bin" }, QDir::Files );
	check( "count", left.size() == std::min( keep, 9 ) && gone == 9 - left.size(),
		QStringLiteral( "left %1 (want %2), deleted %3" ).arg( left.size() ).arg( std::min( keep, 9 ) ).arg( gone ) );
	check( "new_kept", left.contains( "fo4_default.prog-new.bin" ), QStringLiteral( "the binary just written stays" ) );
	bool	newest = true;
	for ( int i = 0; i < 8; i++ ) {
		const bool	want = i >= 8 - ( keep - 1 );	// the newest keep-1 older ones
		newest = newest && left.contains( QStringLiteral( "fo4_default.prog-%1.bin" ).arg( 100 + i ) ) == want;
	}
	check( "newest_kept", newest, QStringLiteral( "the newest %1 older variants stay, the rest go" ).arg( keep - 1 ) );
	check( "twin_prefix", QFile::exists( D + "/fo4_defaultbl.prog-aa.bin" ), QStringLiteral( "another program's binary untouched" ) );

	// two define sets alternating over 8 launches (default rows, then a FOGLEAK row, ...): a launch relinks when its binary is gone
	clear();
	int	relinks = 0;
	for ( int launch = 0; launch < 8; launch++ ) {
		const QString	f = QStringLiteral( "cell_volair.prog-%1.bin" ).arg( launch % 2 ? "fogleak" : "default" );
		if ( !QFile::exists( D + '/' + f ) ) {
			relinks++;
			touch( f, 100 - launch );
			wwProgCachePrune( D, "cell_volair.prog", f, keep );
		}
	}
	std::printf( "alternate relinks %d\n", relinks );
	check( "alternate", relinks == 2, QStringLiteral( "relinks %1 (want 2: one a define set; the red relinks every launch)" ).arg( relinks ) );

	/* lane BUGS1102F (SHOTCAPS-MTIME): set A (the default rows) every other launch, keep+2 rare sets between them, twice
	 * round. A hit stamps A, so A is never the oldest and links once; without the stamp (the red) the rare writes push A out */
	clear();
	int	relinksA = 0;
	const int	rare = keep + 2;
	for ( int launch = 0; launch < 4 * rare; launch++ ) {
		const QString	f = launch % 2 ? QStringLiteral( "fo4_effect.prog-r%1.bin" ).arg( ( launch / 2 ) % rare ) : QStringLiteral( "fo4_effect.prog-A.bin" );
		const int	age = 1000 - launch;
		if ( QFile::exists( D + '/' + f ) ) {
			if ( touchOn )
				wwProgCacheTouch( D + '/' + f, QDateTime::currentDateTime().addSecs( -age ) );
			continue;
		}
		if ( launch % 2 == 0 )
			relinksA++;
		touch( f, age );
		wwProgCachePrune( D, "fo4_effect.prog", f, keep );
	}
	std::printf( "lru relinks A %d (touch %d)\n", relinksA, int( touchOn ) );
	check( "lru", relinksA == 1, QStringLiteral( "set A linked %1 times (want 1: hits keep it; the red without the stamp relinks it)" ).arg( relinksA ) );
	{
		touch( "stamp.bin", 500 );
		const QDateTime	t = QDateTime::currentDateTime().addSecs( -7 );
		const bool	ok = wwProgCacheTouch( D + "/stamp.bin", t );
		const qint64	dt = QFileInfo( D + "/stamp.bin" ).lastModified().secsTo( t );
		check( "stamp", ok && dt >= -1 && dt <= 1 && !wwProgCacheTouch( D + "/missing.bin" ),
			QStringLiteral( "a hit sets the file time (off %1 s), a missing file is refused" ).arg( dt ) );
	}
	check( "touch_env", wwProgCacheTouchOn( "" ) && wwProgCacheTouchOn( "0" ) && !wwProgCacheTouchOn( "1" ), QStringLiteral( "stamp on unless the red =1" ) );
	check( "env", wwProgCacheKeep( "" ) == 6 && wwProgCacheKeep( "1" ) == 1 && wwProgCacheKeep( "x" ) == 6
		&& wwProgCacheKeep( "0" ) == 6 && wwProgCacheKeep( "999" ) == 64, QStringLiteral( "default 6, red 1, clamp 64" ) );
	clear();
	std::printf( "PROGCACHEKEEP %s keep %d\n", fails ? "FAIL" : "PASS", keep );
	return fails ? 1 : 0;
}

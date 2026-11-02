#ifndef WW_PROGCACHEKEEP_H
#define WW_PROGCACHEKEEP_H

/* lane BUGS1102C (SHOTCAPS-COLD): which cached program binaries stay in shadercache/ when a program writes a new one.
 * The key mixes the sources AFTER the switch defines (WW_CELL_FOGLEAK, WW_CELL_RESTIR, ...), so one program has one
 * binary per define set. Keeping only the newest made rows that alternate define sets relink every program each launch
 * (~46 programs, ~7 min a launch on a fresh snapshot). Now the newest `keep` binaries of a program stay (by mtime, the
 * one just written always). WW_PROGCACHE_KEEP=<n> (1..64, default 6); WW_PROGCACHE_KEEP=1 is the red (the old rule).
 * lane BUGS1102F (SHOTCAPS-MTIME): a cache hit stamps its binary's file time, so the prune keeps the most recently USED
 * binaries, not the most recently written (a define set used every launch is never pruned by a run of rare ones).
 * WW_PROGCACHE_TOUCH_RED=1 is the red (no stamp: the newest WRITTEN stay). */

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QStringList>

#include <algorithm>

inline int wwProgCacheKeep( const QByteArray & env )
{
	bool	ok = false;
	const int	n = env.trimmed().toInt( &ok );
	return ok && n >= 1 ? std::min( n, 64 ) : 6;
}

//! deletes `<program>-*.bin` in `dir` beyond the newest `keep` (the file `justWritten` always stays); returns the count deleted
inline int wwProgCachePrune( const QString & dir, const QString & program, const QString & justWritten, int keep )
{
	QDir	d( dir );
	const QFileInfoList	all = d.entryInfoList( { program + QStringLiteral( "-*.bin" ) }, QDir::Files, QDir::Time );	// newest first
	int	kept = 1, gone = 0;
	for ( const QFileInfo & fi : all ) {
		if ( fi.fileName() == justWritten )
			continue;
		if ( kept < keep ) {
			kept++;
			continue;
		}
		if ( d.remove( fi.fileName() ) )
			gone++;
	}
	return gone;
}

//! the hit stamp is on unless WW_PROGCACHE_TOUCH_RED=1
inline bool wwProgCacheTouchOn( const QByteArray & redEnv )
{
	return redEnv.trimmed() != "1";
}

//! a cache hit: stamps `file`'s modification time (`when`, default now) so wwProgCachePrune counts it as recently used
inline bool wwProgCacheTouch( const QString & file, const QDateTime & when = QDateTime::currentDateTime() )
{
	QFile	f( file );
	if ( !f.exists() || !f.open( QIODevice::ReadWrite | QIODevice::ExistingOnly ) )	// a stamp never makes a file
		return false;
	const bool	ok = f.setFileTime( when, QFileDevice::FileModificationTime );
	f.close();
	return ok;
}

#endif	// WW_PROGCACHEKEEP_H

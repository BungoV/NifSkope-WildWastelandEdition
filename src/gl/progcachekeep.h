#ifndef WW_PROGCACHEKEEP_H
#define WW_PROGCACHEKEEP_H

/* lane BUGS1102C (SHOTCAPS-COLD): which cached program binaries stay in shadercache/ when a program writes a new one.
 * The key mixes the sources AFTER the switch defines (WW_CELL_FOGLEAK, WW_CELL_RESTIR, ...), so one program has one
 * binary per define set. Keeping only the newest made rows that alternate define sets relink every program each launch
 * (~46 programs, ~7 min a launch on a fresh snapshot). Now the newest `keep` binaries of a program stay (by mtime, the
 * one just written always). WW_PROGCACHE_KEEP=<n> (1..64, default 6); WW_PROGCACHE_KEEP=1 is the red (the old rule). */

#include <QDir>
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

#endif	// WW_PROGCACHEKEEP_H

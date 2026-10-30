#ifndef WWREDTWIN_H
#define WWREDTWIN_H

/* lane REDRETIRE1 (bungo 10-27: "Reds are scaffolding"): a red whose lane bungo confirmed live leaves the shipping
 * build -- no WW_*_RED reader, no switch-table row -- but a committed unit test that used it as its red keeps a way to
 * fail. That test's RED build is compiled with -DWW_REDTWIN (qmake REDTWIN=1); its run then picks one retired red
 * with the environment variable REDTWIN=<GROUP>=<value> (for example REDTWIN=CELLSHOW=nest).
 *
 * Without WW_REDTWIN (NifSkope.exe and every green build) wwRedTwin() is a constant false, so the compiler drops the
 * old path behind it: the shipping code is the new path only. The retired names and their tests are listed in
 * WW_CHANGES.md (REDRETIRE1) and printed by tests/spells/red_census.py. */

#ifdef WW_REDTWIN
#include <QByteArray>
#include <cstring>

inline bool wwRedTwin( const char * group, const char * value )
{
	const QByteArray e = qgetenv( "REDTWIN" );
	const qsizetype g = qsizetype( std::strlen( group ) );
	return e.size() == g + 1 + qsizetype( std::strlen( value ) ) && e.startsWith( group ) && e.at( g ) == '='
		&& e.endsWith( value );
}
#else
constexpr bool wwRedTwin( const char *, const char * ) { return false; }
#endif

#endif	// WWREDTWIN_H

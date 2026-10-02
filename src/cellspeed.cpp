#include "cellspeed.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QTextStream>

#include <vector>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

// lane SPEED1: see cellspeed.h

namespace CellSpeed
{

namespace
{

struct Row
{
	QByteArray stage;
	qint64 wallNs = 0;
	qint64 cpuMs = 0;
	qint64 rssMb = 0;
	qint64 peakMb = 0;
};

struct Sub
{
	QByteArray stage;
	qint64 ns = 0;
	qint64 calls = 0;
};

struct Cnt
{
	QByteArray what;
	qint64 n = 0;
};

struct State
{
	QMutex lock;                 // add() is called from worker threads
	QElapsedTimer clock;
	bool open = false;
	qint64 lastNs = 0;
	qint64 cpu0 = 0;
	qint64 lastCpu = 0;
	std::vector<Row> rows;
	std::vector<Sub> subs;
	std::vector<Cnt> counts;
};

State & st()
{
	static State s;
	return s;
}

//! This process's processor time (user + kernel) in ms, and its working set / peak in MB.
void sample( qint64 & cpuMs, qint64 & rssMb, qint64 & peakMb )
{
	cpuMs = rssMb = peakMb = 0;
#ifdef Q_OS_WIN
	FILETIME c, e, k, u;
	if ( GetProcessTimes( GetCurrentProcess(), &c, &e, &k, &u ) ) {
		const quint64 kk = ( quint64( k.dwHighDateTime ) << 32 ) | k.dwLowDateTime;
		const quint64 uu = ( quint64( u.dwHighDateTime ) << 32 ) | u.dwLowDateTime;
		cpuMs = qint64( ( kk + uu ) / 10000ULL );
	}
	PROCESS_MEMORY_COUNTERS pmc;
	if ( GetProcessMemoryInfo( GetCurrentProcess(), &pmc, sizeof( pmc ) ) ) {
		rssMb = qint64( pmc.WorkingSetSize >> 20 );
		peakMb = qint64( pmc.PeakWorkingSetSize >> 20 );
	}
#endif
}

void atExit()
{
	mark( "after the load, until exit (frames, a shot's fixed wait)" );
	end();
}

} // namespace

bool on()
{
	static const bool v = !qEnvironmentVariableIsEmpty( "WW_CELL_SPEED_DUMP" );
	return v;
}

qint64 nowNs()
{
	State & s = st();
	return s.clock.isValid() ? s.clock.nsecsElapsed() : 0;
}

void begin()
{
	if ( !on() )
		return;
	State & s = st();
	static bool hooked = false;
	if ( !hooked ) {
		hooked = true;
		qAddPostRoutine( atExit );
	}
	if ( s.open )
		end();
	QMutexLocker l( &s.lock );
	s.rows.clear();
	s.subs.clear();
	s.counts.clear();
	s.clock.start();
	s.lastNs = 0;
	qint64 rss, peak;
	sample( s.cpu0, rss, peak );
	s.lastCpu = s.cpu0;
	s.open = true;
	Row r;
	r.stage = "(at start)";
	r.rssMb = rss;
	r.peakMb = peak;
	s.rows.push_back( r );
}

void mark( const char * stage )
{
	if ( !on() )
		return;
	State & s = st();
	QMutexLocker l( &s.lock );
	if ( !s.open )
		return;
	const qint64 t = s.clock.nsecsElapsed();
	Row r;
	r.stage = stage;
	r.wallNs = t - s.lastNs;
	s.lastNs = t;
	qint64 cpu;
	sample( cpu, r.rssMb, r.peakMb );
	r.cpuMs = cpu - s.lastCpu;
	s.lastCpu = cpu;
	// the same stage twice (a second frame, a second pass) adds up
	for ( Row & o : s.rows ) {
		if ( o.stage == r.stage ) {
			o.wallNs += r.wallNs;
			o.cpuMs += r.cpuMs;
			o.rssMb = r.rssMb;
			o.peakMb = r.peakMb;
			return;
		}
	}
	s.rows.push_back( r );
}

void add( const char * stage, qint64 ns )
{
	if ( !on() )
		return;
	State & s = st();
	QMutexLocker l( &s.lock );
	if ( !s.open )
		return;
	for ( Sub & o : s.subs ) {
		if ( o.stage == stage ) {
			o.ns += ns;
			o.calls++;
			return;
		}
	}
	Sub n;
	n.stage = stage;
	n.ns = ns;
	n.calls = 1;
	s.subs.push_back( n );
}

void count( const char * what, qint64 n )
{
	if ( !on() )
		return;
	State & s = st();
	QMutexLocker l( &s.lock );
	if ( !s.open )
		return;
	for ( Cnt & o : s.counts ) {
		if ( o.what == what ) {
			o.n += n;
			return;
		}
	}
	Cnt c;
	c.what = what;
	c.n = n;
	s.counts.push_back( c );
}

void end()
{
	if ( !on() )
		return;
	State & s = st();
	QMutexLocker l( &s.lock );
	if ( !s.open )
		return;
	s.open = false;
	const qint64 total = s.clock.nsecsElapsed();
	qint64 cpu, rss, peak;
	sample( cpu, rss, peak );
	QFile f( qEnvironmentVariable( "WW_CELL_SPEED_DUMP" ) );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text ) )
		return;
	QTextStream o( &f );
	const QString tag = qEnvironmentVariable( "WW_CELL_SPEED_TAG" );
	o << "CELLSPEED v1 " << ( tag.isEmpty() ? QStringLiteral( "-" ) : tag ) << "\n";
	for ( const Row & r : s.rows )
		o << "stage\t" << r.stage << "\t" << r.wallNs / 1000000 << "\t" << r.cpuMs << "\t"
		  << r.rssMb << "\t" << r.peakMb << "\n";
	for ( const Sub & b : s.subs )
		o << "within\t" << b.stage << "\t" << b.ns / 1000000 << "\t" << b.calls << "\n";
	for ( const Cnt & c : s.counts )
		o << "count\t" << c.what << "\t" << c.n << "\n";
	o << "total\twall_ms\t" << total / 1000000 << "\tcpu_ms\t" << ( cpu - s.cpu0 )
	  << "\trss_mb\t" << rss << "\tpeak_mb\t" << peak << "\n";
	o << "END\n";
}

} // namespace CellSpeed

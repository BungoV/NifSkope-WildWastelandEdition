#ifndef WWSWITCH_H
#define WWSWITCH_H

/* lane TESTDRIVER1 (2026-10-22): ONE STORE FOR THE WW_* TEST SWITCHES, SO A TEST FLIPS THEM IN A LOADED VIEW.
 *
 * bungo 10-22: "by relaunching nifskope, you introduce a significant delay by loading everything again".
 * The store is the process environment: the launch env gives the initial values, and every reader goes through the
 * wwEnv* accessors below (exact equivalents of qgetenv / qEnvironmentVariable*). A live `set NAME=VALUE` (the
 * WW_LIVE_STEPS file, or "NifSkope::live <step>" on the --port) writes the value and moves the generation; a cached
 * reader (WW_SWITCH_STATIC, wwSwitchMoved) re-reads when the generation it saw is not the current one.
 * With no set, the generation stays 0 and every reader reads exactly once, as before: the output is unchanged.
 * Each switch has a stage (tools/wwswitch_survey.py writes the table): live, shaders (reload the programs),
 * reopen (rebuild the cell document in the same window) or relaunch (process-wide startup state; the set is refused).
 * WW_TESTDRIVER_RED=relaunch refuses every set, so the harness falls back to one launch per row. */

#include <QByteArray>
#include <QList>
#include <QString>
#include <QtGlobal>

#include <atomic>
#include <initializer_list>
#include <mutex>
#include <utility>

// ---- the accessors: every WW_* read goes through these
QByteArray wwEnv( const char * name );
QString wwEnvStr( const char * name );
QString wwEnvStr( const char * name, const QString & defaultValue );
int wwEnvInt( const char * name, bool * ok = nullptr );
bool wwEnvIsSet( const char * name );
bool wwEnvIsEmpty( const char * name );
inline QByteArray wwEnv( const QByteArray & name ) { return wwEnv( name.constData() ); }

// ---- the generation
extern std::atomic<quint64> g_wwSwitchGen;

//! 0 until the first live set; moves by one per set/unset
inline quint64 wwSwitchGen()
{
	return g_wwSwitchGen.load( std::memory_order_acquire );
}

//! True once per new generation for the caller's stamp (start the stamp at 0: the first call is false)
inline bool wwSwitchMoved( quint64 & seen )
{
	const quint64 g = wwSwitchGen();
	if ( g == seen )
		return false;
	seen = g;
	return true;
}

//! True when a name starting with one of `prefixes` was set after `seen`; `seen` moves to the current generation
bool wwSwitchMovedFor( quint64 & seen, std::initializer_list<const char *> prefixes );

// ---- what a live change needs
enum class WwSwitchStage { Live = 0, Reopen = 1, Relaunch = 2, Tool = 3, Shaders = 4 };

const char * wwSwitchStageName( WwSwitchStage s );
//! The stage of one switch (the survey's table); a name nobody reads is Live, `where` says so
WwSwitchStage wwSwitchStageOf( const QByteArray & name, QString * where = nullptr );
//! WW_TESTDRIVER_RED=relaunch: every live set is refused (the harness launches once per row)
bool wwSwitchRedRelaunch();

/* Sets (unset: an empty value with `unset`) one switch and moves the generation, unless its stage is Relaunch or the
 * red is on: then nothing changes and the reply starts "refused". The reply names the stage still owed. */
QString wwSwitchSet( const QByteArray & name, const QByteArray & value, bool unset, WwSwitchStage * stage = nullptr );
//! Puts a switch back to its launch value (set or unset as the process started); empty name: every switch set so far
QString wwSwitchReset( const QByteArray & name, QList<std::pair<QByteArray, WwSwitchStage>> * moved = nullptr );

// ---- a function-static that follows the generation (class (b) readers)
/* The first call computes the value (as the static's initializer did); a call after a live set computes it again.
 * Old values stay allocated: a worker thread may still be reading one (a few objects per set, harness only). */
template <typename T>
class WwSwitchCache
{
public:
	template <typename F>
	const T & get( F && make )
	{
		const quint64 g = wwSwitchGen();
		const T * p = val.load( std::memory_order_acquire );
		if ( p && seen.load( std::memory_order_acquire ) == g )
			return *p;
		std::lock_guard<std::mutex> lock( mutex );
		p = val.load( std::memory_order_relaxed );
		if ( !p || seen.load( std::memory_order_relaxed ) != g ) {
			p = new T( make() );
			val.store( p, std::memory_order_release );
			seen.store( g, std::memory_order_release );
		}
		return *p;
	}

private:
	std::atomic<const T *> val { nullptr };
	std::atomic<quint64> seen { ~quint64( 0 ) };
	std::mutex mutex;
};

//! `static const T var = expr;` becomes `WW_SWITCH_STATIC( T, var, expr );`
#define WW_SWITCH_STATIC( T, var, ... ) \
	static WwSwitchCache<T> var##_wwsc; \
	const T & var = var##_wwsc.get( [&]() -> T { return ( __VA_ARGS__ ); } )

// ---- the live driver (src/nifskope_ui.cpp): one step, from the steps file or the port
class NifSkope;
//! Runs one step line ("set N=V", "unset N", "wait F", "shot OUT", "path FILE OUT", "gpums F OUT", "apply", "quit")
QString wwLiveCommand( NifSkope * skope, const QString & line );

#endif // WWSWITCH_H

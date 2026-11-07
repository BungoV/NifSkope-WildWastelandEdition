#include "wwswitch.h"
#include "wwenvput.h"	// lane PERFFIX6: wwEnvPut (the accessors cache)

#include <QHash>
#include <QMutex>
#include <QStringList>
#include <QMutexLocker>
#include <QVector>

#include <algorithm>
#include <cstring>

// lane TESTDRIVER1: see wwswitch.h

std::atomic<quint64> g_wwSwitchGen { 0 };
std::atomic<quint64> g_wwEnvGen { 0 };

/* lane PERFFIX6 (F1): the accessors read the process environment once per name and generation. qgetenv /
 * qEnvironmentVariable* take Qt's environment mutex, scan the whole environment block and convert, per call: 6-9 % of
 * the frame's CPU (PERFCPU1). Each field is filled by the same Qt call as before, the first time it is asked after
 * the generation moved, so a hit returns exactly what the uncached call would. The generation is g_wwSwitchGen (a
 * live set) plus g_wwEnvGen (every other environment write in src/ goes through wwEnvPut / wwEnvUnset). Entries
 * are keyed by the name's address and checked by content (a temporary's address may be reused for another name).
 * Red WW_ENV_CACHE=0 (live): every call reads the environment, as before. */
namespace {

struct EnvEntry
{
	QByteArray name;
	unsigned have = 0;	// 1 raw, 2 str, 4 int, 8 set, 16 empty
	QByteArray raw;
	QString str;
	int ival = 0;
	bool iok = false, set = false, empty = false;
};

struct EnvCache
{
	QMutex lock;
	QHash<quintptr, EnvEntry> map;
	quint64 gen = ~quint64( 0 );
};

EnvCache & envCache()
{
	static EnvCache c;
	return c;
}

bool envCacheOn()
{
	// read again whenever a live set moved the switch generation (so a row can turn the red on and off in one launch)
	static std::atomic<quint64> seen { ~quint64( 0 ) };
	static std::atomic<bool> on { true };
	const quint64 g = g_wwSwitchGen.load( std::memory_order_acquire );
	if ( seen.load( std::memory_order_acquire ) != g ) {
		on.store( qgetenv( "WW_ENV_CACHE" ).trimmed() != "0", std::memory_order_relaxed );
		seen.store( g, std::memory_order_release );
	}
	return on.load( std::memory_order_relaxed );
}

inline quint64 envGenNow()
{
	return g_wwSwitchGen.load( std::memory_order_acquire ) + g_wwEnvGen.load( std::memory_order_acquire );
}

//! the entry for `name` under the current generation (the cache's lock held by the caller)
EnvEntry & envEntry( EnvCache & c, const char * name )
{
	const quint64 g = envGenNow();
	if ( c.gen != g || c.map.size() > 4096 ) {
		c.map.clear();
		c.gen = g;
	}
	EnvEntry & e = c.map[quintptr( name )];
	if ( e.name != name ) {
		e = EnvEntry();
		e.name = QByteArray( name );
	}
	return e;
}

} // namespace

QByteArray wwEnv( const char * name )
{
	if ( !envCacheOn() )
		return qgetenv( name );
	EnvCache & c = envCache();
	QMutexLocker l( &c.lock );
	EnvEntry & e = envEntry( c, name );
	if ( !( e.have & 1 ) ) {
		e.raw = qgetenv( name );
		e.have |= 1;
	}
	return e.raw;
}

QString wwEnvStr( const char * name )
{
	if ( !envCacheOn() )
		return qEnvironmentVariable( name );
	EnvCache & c = envCache();
	QMutexLocker l( &c.lock );
	EnvEntry & e = envEntry( c, name );
	if ( !( e.have & 2 ) ) {
		e.str = qEnvironmentVariable( name );
		e.have |= 2;
	}
	return e.str;
}

QString wwEnvStr( const char * name, const QString & defaultValue )
{
	if ( !envCacheOn() )
		return qEnvironmentVariable( name, defaultValue );
	return wwEnvIsSet( name ) ? wwEnvStr( name ) : defaultValue;
}

int wwEnvInt( const char * name, bool * ok )
{
	if ( !envCacheOn() )
		return qEnvironmentVariableIntValue( name, ok );
	EnvCache & c = envCache();
	QMutexLocker l( &c.lock );
	EnvEntry & e = envEntry( c, name );
	if ( !( e.have & 4 ) ) {
		e.ival = qEnvironmentVariableIntValue( name, &e.iok );
		e.have |= 4;
	}
	if ( ok )
		*ok = e.iok;
	return e.ival;
}

bool wwEnvIsSet( const char * name )
{
	if ( !envCacheOn() )
		return qEnvironmentVariableIsSet( name );
	EnvCache & c = envCache();
	QMutexLocker l( &c.lock );
	EnvEntry & e = envEntry( c, name );
	if ( !( e.have & 8 ) ) {
		e.set = qEnvironmentVariableIsSet( name );
		e.have |= 8;
	}
	return e.set;
}

bool wwEnvIsEmpty( const char * name )
{
	if ( !envCacheOn() )
		return qEnvironmentVariableIsEmpty( name );
	EnvCache & c = envCache();
	QMutexLocker l( &c.lock );
	EnvEntry & e = envEntry( c, name );
	if ( !( e.have & 16 ) ) {
		e.empty = qEnvironmentVariableIsEmpty( name );
		e.have |= 16;
	}
	return e.empty;
}

void wwEnvWritten()
{
	g_wwEnvGen.fetch_add( 1, std::memory_order_acq_rel );
}

bool wwEnvPut( const char * name, const QByteArray & value )
{
	const bool r = qputenv( name, value );
	wwEnvWritten();
	return r;
}

bool wwEnvUnset( const char * name )
{
	const bool r = qunsetenv( name );
	wwEnvWritten();
	return r;
}

namespace {

struct Change { quint64 gen; QByteArray name; };

QMutex & changeLock()
{
	static QMutex m;
	return m;
}

QVector<Change> & changes()
{
	static QVector<Change> c;
	return c;
}

struct StageRow { const char * name; int stage; const char * where; };

const StageRow kStages[] = {
#include "wwswitchstages.inc"
};

} // namespace

bool wwSwitchMovedFor( quint64 & seen, std::initializer_list<const char *> prefixes )
{
	const quint64 g = wwSwitchGen();
	if ( g == seen )
		return false;
	bool hit = false;
	{
		QMutexLocker l( &changeLock() );
		for ( const Change & c : changes() ) {
			if ( c.gen <= seen )
				continue;
			for ( const char * p : prefixes )
				hit = hit || c.name.startsWith( p );
		}
	}
	seen = g;
	return hit;
}

const char * wwSwitchStageName( WwSwitchStage s )
{
	switch ( s ) {
	case WwSwitchStage::Live: return "live";
	case WwSwitchStage::Reopen: return "reopen";
	case WwSwitchStage::Relaunch: return "relaunch";
	case WwSwitchStage::Tool: return "tool";
	case WwSwitchStage::Shaders: return "shaders";
	}
	return "?";
}

WwSwitchStage wwSwitchStageOf( const QByteArray & name, QString * where )
{
	const StageRow * end = kStages + sizeof( kStages ) / sizeof( kStages[0] );
	const StageRow * r = std::lower_bound( kStages, end, name, []( const StageRow & a, const QByteArray & n ) {
		return std::strcmp( a.name, n.constData() ) < 0;
	} );
	if ( r != end && name == r->name ) {
		if ( where )
			*where = QString::fromLatin1( r->where );
		return WwSwitchStage( r->stage );
	}
	// fix 10-22: a name the table lacks (a lane's new switch, or a direct env read) may be cached once: relaunch
	if ( where )
		*where = QStringLiteral( "not in the stage table: unknown = relaunch" );
	return WwSwitchStage::Relaunch;
}

namespace {
std::atomic<bool> g_liveSession { false };
std::atomic<quint64> g_shaderRefusals { 0 };
QString & lastShaderRefusal()
{
	static QString s;
	return s;
}
}	// namespace

void wwLiveSessionStart()
{
	g_liveSession.store( true, std::memory_order_release );
}

bool wwLiveSessionOn()
{
	return g_liveSession.load( std::memory_order_acquire );
}

void wwLiveShaderRefused( const QString & text )
{
	QMutexLocker l( &changeLock() );
	lastShaderRefusal() = text;
	g_shaderRefusals.fetch_add( 1, std::memory_order_acq_rel );
}

quint64 wwLiveShaderRefusals( QString * last )
{
	QMutexLocker l( &changeLock() );
	if ( last )
		*last = lastShaderRefusal();
	return g_shaderRefusals.load( std::memory_order_acquire );
}

static QHash<QByteArray, std::pair<bool, QByteArray>> & launchValues()
{
	static QHash<QByteArray, std::pair<bool, QByteArray>> v;	// name -> (was set, value) before its first live set
	return v;
}

QString wwSwitchReset( const QByteArray & name, QList<std::pair<QByteArray, WwSwitchStage>> * moved )
{
	QList<QByteArray> names;
	QHash<QByteArray, std::pair<bool, QByteArray>> launch;
	{
		QMutexLocker l( &changeLock() );
		launch = launchValues();
	}
	if ( !name.isEmpty() )
		names << name;
	else
		names = launch.keys();
	std::sort( names.begin(), names.end() );
	QStringList r;
	for ( const QByteArray & n : names ) {
		const auto it = launch.constFind( n );
		WwSwitchStage s = WwSwitchStage::Live;
		if ( it == launch.constEnd() )
			r << QStringLiteral( "same %1 (never set live)" ).arg( QString::fromLatin1( n ) );
		else
			r << wwSwitchSet( n, it->second, !it->first, &s );
		if ( moved && r.last().startsWith( QLatin1StringView( "ok" ) ) )
			moved->append( { n, s } );
	}
	return r.isEmpty() ? QStringLiteral( "same (nothing set live)" ) : r.join( QStringLiteral( "; " ) );
}

bool wwSwitchRedRelaunch()
{
	static const bool red = qgetenv( "WW_TESTDRIVER_RED" ).trimmed() == "relaunch";
	return red;
}

QString wwSwitchSet( const QByteArray & name, const QByteArray & value, bool unset, WwSwitchStage * stageOut )
{
	QString where;
	const WwSwitchStage stage = wwSwitchStageOf( name, &where );
	if ( stageOut )
		*stageOut = stage;
	if ( wwSwitchRedRelaunch() )
		return QStringLiteral( "refused relaunch (WW_TESTDRIVER_RED=relaunch) %1" ).arg( QString::fromLatin1( name ) );
	if ( stage == WwSwitchStage::Relaunch )
		return QStringLiteral( "refused relaunch %1 (%2)" ).arg( QString::fromLatin1( name ), where );
	const QByteArray now = qgetenv( name.constData() );
	const bool wasSet = qEnvironmentVariableIsSet( name.constData() );
	if ( unset ? !wasSet : ( wasSet && now == value ) )
		return QStringLiteral( "same %1 stage %2" ).arg( QString::fromLatin1( name ), QLatin1String( wwSwitchStageName( stage ) ) );
	{
		QMutexLocker l( &changeLock() );
		if ( !launchValues().contains( name ) )
			launchValues().insert( name, { wasSet, now } );
	}
	if ( unset )
		qunsetenv( name.constData() );
	else
		qputenv( name.constData(), value );
	{
		QMutexLocker l( &changeLock() );
		const quint64 g = g_wwSwitchGen.load( std::memory_order_relaxed ) + 1;
		changes().append( { g, name } );
		g_wwSwitchGen.store( g, std::memory_order_release );
	}
	return QStringLiteral( "ok %1 stage %2 gen %3" ).arg( QString::fromLatin1( name ), QLatin1String( wwSwitchStageName( stage ) ) )
		.arg( wwSwitchGen() );
}

#include "wwswitch.h"

#include <QHash>
#include <QMutex>
#include <QStringList>
#include <QMutexLocker>
#include <QReadWriteLock>
#include <QVector>

#include <algorithm>
#include <cstring>

// lane TESTDRIVER1: see wwswitch.h

std::atomic<quint64> g_wwSwitchGen { 0 };

/* MODULES1 item 8 (PERFCPU1's finding: qgetenv / qEnvironmentVariable take Qt's environment lock, scan the whole
 * block and convert, per call, uncached: 6-9 % of the frame's CPU, thousands of calls a frame): a name's answer is
 * kept until the environment may have moved (wwEnvChanged: every writer in the tree, a live set, and each frame's
 * start as a backstop). Same answers as the direct reads. Red WW_ENV_CACHE_RED=1 (read at the first call): direct. */
namespace {
struct EnvEntry
{
	bool set = false;
	QByteArray bytes;
	bool strDone = false;
	QString str;
};
QReadWriteLock & envLock()
{
	static QReadWriteLock l;
	return l;
}
QHash<QByteArray, EnvEntry> & envCache()
{
	static QHash<QByteArray, EnvEntry> c;
	return c;
}
bool envCacheRed()
{
	static const bool red = qgetenv( "WW_ENV_CACHE_RED" ).trimmed() == "1";
	return red;
}
EnvEntry envEntry( const char * name, bool wantStr )
{
	const QByteArray key = QByteArray::fromRawData( name, int( std::strlen( name ) ) );
	{
		QReadLocker l( &envLock() );
		const auto it = envCache().constFind( key );
		if ( it != envCache().constEnd() && ( !wantStr || it->strDone ) )
			return *it;
	}
	EnvEntry e;
	e.set = qEnvironmentVariableIsSet( name );
	e.bytes = qgetenv( name );
	if ( wantStr ) {
		e.str = qEnvironmentVariable( name );
		e.strDone = true;
	}
	QWriteLocker l( &envLock() );
	envCache().insert( QByteArray( name ), e );
	return e;
}
}	// namespace

namespace {
std::atomic<quint64> g_wwEnvEpoch { 0 };
}

void wwEnvChanged()
{
	QWriteLocker l( &envLock() );
	envCache().clear();
	g_wwEnvEpoch.fetch_add( 1, std::memory_order_acq_rel );
}

quint64 wwEnvEpoch()
{
	return g_wwEnvEpoch.load( std::memory_order_acquire );
}

QByteArray wwEnv( const char * name )
{
	if ( envCacheRed() )
		return qgetenv( name );
	return envEntry( name, false ).bytes;
}

QString wwEnvStr( const char * name )
{
	if ( envCacheRed() )
		return qEnvironmentVariable( name );
	return envEntry( name, true ).str;
}

QString wwEnvStr( const char * name, const QString & defaultValue )
{
	if ( envCacheRed() )
		return qEnvironmentVariable( name, defaultValue );
	const EnvEntry e = envEntry( name, true );
	return e.set ? e.str : defaultValue;
}

int wwEnvInt( const char * name, bool * ok )
{
	if ( envCacheRed() )
		return qEnvironmentVariableIntValue( name, ok );
	return envEntry( name, false ).bytes.toInt( ok, 0 );	// qEnvironmentVariableIntValue = qgetenv( name ).toInt( ok, 0 )
}

bool wwEnvIsSet( const char * name )
{
	if ( envCacheRed() )
		return qEnvironmentVariableIsSet( name );
	return envEntry( name, false ).set;
}

bool wwEnvIsEmpty( const char * name )
{
	if ( envCacheRed() )
		return qEnvironmentVariableIsEmpty( name );
	return envEntry( name, false ).bytes.isEmpty();
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
	wwEnvChanged();	// MODULES1 item 8: the accessors' cache
	{
		QMutexLocker l( &changeLock() );
		const quint64 g = g_wwSwitchGen.load( std::memory_order_relaxed ) + 1;
		changes().append( { g, name } );
		g_wwSwitchGen.store( g, std::memory_order_release );
	}
	return QStringLiteral( "ok %1 stage %2 gen %3" ).arg( QString::fromLatin1( name ), QLatin1String( wwSwitchStageName( stage ) ) )
		.arg( wwSwitchGen() );
}

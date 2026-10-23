#include "wwswitch.h"

#include <QHash>
#include <QMutex>
#include <QStringList>
#include <QMutexLocker>
#include <QVector>

#include <algorithm>
#include <cstring>

// lane TESTDRIVER1: see wwswitch.h

std::atomic<quint64> g_wwSwitchGen { 0 };

QByteArray wwEnv( const char * name )
{
	return qgetenv( name );
}

QString wwEnvStr( const char * name )
{
	return qEnvironmentVariable( name );
}

QString wwEnvStr( const char * name, const QString & defaultValue )
{
	return qEnvironmentVariable( name, defaultValue );
}

int wwEnvInt( const char * name, bool * ok )
{
	return qEnvironmentVariableIntValue( name, ok );
}

bool wwEnvIsSet( const char * name )
{
	return qEnvironmentVariableIsSet( name );
}

bool wwEnvIsEmpty( const char * name )
{
	return qEnvironmentVariableIsEmpty( name );
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
	if ( where )
		*where = QStringLiteral( "no reader in src/ (the survey's table)" );
	return WwSwitchStage::Live;
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

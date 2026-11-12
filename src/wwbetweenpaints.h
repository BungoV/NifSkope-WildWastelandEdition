#pragma once

#include <QAbstractEventDispatcher>
#include <QAbstractNativeEventFilter>
#include <QByteArray>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QObject>
#include <QVariant>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#ifdef Q_OS_WIN
extern "C" __declspec( dllimport ) unsigned long __stdcall GetQueueStatus( unsigned int flags );
#endif

/* lane EMPTY240 R5-3: build 77's empty spot spent 1.47 ms a frame between a paint's swap and the next paint's start
 * (the gears 0.02 of it). From frameSwapped to the next paintGL every event the application delivers (its type and its
 * receiver's class), every native window message (by its id) and the event loop's sleep are timed; each mark's time
 * runs to the next mark. The line `between paints:` every 5 s, its parts biggest first. GUI thread only. */
namespace WwBetween
{

enum Kind { AfterSwap, Event, Native, Asleep, Woken, KindCount };

struct Key
{
	int kind = AfterSwap, code = 0;
	const char * cls = nullptr;
	bool operator<( const Key & o ) const { return std::tie( kind, code, cls ) < std::tie( o.kind, o.code, o.cls ); }
	bool operator==( const Key & o ) const { return kind == o.kind && code == o.code && cls == o.cls; }
};

struct Cost
{
	double ms = 0.0;
	long long n = 0;
};

/* lane EMPTY240 R6-1: build 78 timed every gap through a std::map insert per event; now one gap in 8 is timed (the
 * others leave the filters at one bool test) into a fixed table searched in place. Red WW_BETWEENSAMPLE_RED=1 = every
 * gap, build 78. Paint and Timer receivers are named by their parents ("QLineEdit < QDoubleSpinBox < CellEditPanel"). */
struct State
{
	static constexpr int kSlots = 48;
	bool installed = false, on = false;
	QElapsedTimer clock;
	qint64 last = 0, winAt = 0;
	Key at;
	std::pair<Key, Cost> by[kSlots];
	int used = 0;
	Cost spill;	// parts past the table
	double totalMs = 0.0;
	long long frames = 0, gaps = 0;
	int every = 8;
	std::set<std::string> names;	// interned receiver names
};

inline State & st()
{
	static State s;
	return s;
}

inline void mark( const Key & next )
{
	State & s = st();
	if ( !s.on )
		return;
	const qint64 now = s.clock.nsecsElapsed();
	Cost * c = &s.spill;
	int i = 0;
	while ( i < s.used && !( s.by[i].first == s.at ) )
		i++;
	if ( i < s.used )
		c = &s.by[i].second;
	else if ( s.used < State::kSlots ) {
		s.by[s.used].first = s.at;
		s.by[s.used].second = Cost();
		c = &s.by[s.used++].second;
	}
	c->ms += double( now - s.last ) * 1e-6;
	c->n++;
	s.totalMs += double( now - s.last ) * 1e-6;
	s.last = now;
	s.at = next;
}

//! lane EMPTY240 R6-2: a Paint receiver by its parents up to the first named or non-Qt one; a QTimer by its owner
inline const char * receiverName( QObject * o, int type )
{
	if ( !o )
		return "none";
	const char * cls = o->metaObject()->className();
	const bool paint = type == QEvent::Paint, timer = type == QEvent::Timer && o->inherits( "QTimer" );
	if ( !paint && !timer )
		return cls;
	std::string n = cls;
	if ( !o->objectName().isEmpty() )
		n += " '" + o->objectName().toStdString() + "'";
	if ( timer )
		n += " (interval " + std::to_string( o->property( "interval" ).toInt() ) + " ms)";
	QObject * p = o->parent();
	for ( int up = 0; p && up < 4; up++, p = p->parent() ) {
		const char * pc = p->metaObject()->className();
		n += std::string( timer ? " of " : " < " ) + pc;
		const bool named = !p->objectName().isEmpty();
		if ( named )
			n += " '" + p->objectName().toStdString() + "'";
		if ( named || timer || pc[0] != 'Q' )
			break;
	}
	return st().names.insert( n ).first->c_str();
}

struct AppFilter : QObject
{
	bool eventFilter( QObject * o, QEvent * e ) override
	{
		if ( st().on ) {
			Key k;
			k.kind = Event;
			k.code = int( e->type() );
			k.cls = receiverName( o, k.code );
			mark( k );
		}
		return false;
	}
};

struct NativeFilter : QAbstractNativeEventFilter
{
	bool nativeEventFilter( const QByteArray & type, void * message, qintptr * ) override
	{
		if ( st().on && message && type == "windows_generic_MSG" ) {
			struct Msg
			{
				void * hwnd;
				unsigned int id;
			};
			Key k;
			k.kind = Native;
			k.code = int( static_cast<const Msg *>( message )->id );
			mark( k );
		}
		return false;
	}
};

inline void install()
{
	State & s = st();
	if ( s.installed || !QCoreApplication::instance() )
		return;
	s.installed = true;
	s.clock.start();
	s.every = qEnvironmentVariableIntValue( "WW_BETWEENSAMPLE_RED" ) == 1 ? 1 : 8;
	std::fprintf( stderr, "between paints: timing one gap in %d (WW_BETWEENSAMPLE_RED=1 = every gap, build 78)\n", s.every );
	static AppFilter app;
	static NativeFilter native;
	QCoreApplication::instance()->installEventFilter( &app );
	QCoreApplication::instance()->installNativeEventFilter( &native );
	if ( QAbstractEventDispatcher * d = QAbstractEventDispatcher::instance() ) {
		QObject::connect( d, &QAbstractEventDispatcher::aboutToBlock, &app, []() { Key k; k.kind = Asleep; mark( k ); } );
		QObject::connect( d, &QAbstractEventDispatcher::awake, &app, []() { Key k; k.kind = Woken; mark( k ); } );
	}
}

//! frameSwapped: the gap opens
inline void swapped()
{
	install();
	State & s = st();
	if ( s.gaps++ % s.every != 0 )
		return;
	s.on = true;
	s.last = s.clock.nsecsElapsed();
	s.at = Key();
}

inline QByteArray eventName( int t )
{
	switch ( t ) {
	case QEvent::Timer: return "Timer";
	case QEvent::MetaCall: return "MetaCall (a queued call or singleShot)";
	case QEvent::UpdateRequest: return "UpdateRequest";
	case QEvent::Paint: return "Paint";
	case QEvent::Expose: return "Expose";
	case QEvent::MouseMove: return "MouseMove";
	case QEvent::HoverMove: return "HoverMove";
	case QEvent::DeferredDelete: return "DeferredDelete";
	case QEvent::SockAct: return "SockAct";
	case QEvent::ZeroTimerEvent: return "ZeroTimerEvent";
	case QEvent::PlatformSurface: return "PlatformSurface";
	default: return "event " + QByteArray::number( t );
	}
}

//! paintGL's start: the gap closes; every 5 s the line
inline void paintStart()
{
	State & s = st();
	if ( !s.on )
		return;
	mark( Key() );
	s.on = false;
	s.frames++;
	const qint64 now = s.clock.nsecsElapsed();
	if ( now - s.winAt < 5000000000LL )
		return;
	s.winAt = now;
	std::vector<std::pair<Key, Cost>> v( s.by, s.by + s.used );
	std::sort( v.begin(), v.end(), []( const auto & a, const auto & b ) { return a.second.ms > b.second.ms; } );
	const double f = double( std::max<long long>( 1, s.frames ) );
	QByteArray line = "between paints: " + QByteArray::number( s.totalMs / f, 'f', 2 ) + " ms a frame ("
		+ QByteArray::number( s.frames ) + " frames timed, one gap in " + QByteArray::number( s.every )
		+ ", the swap's return to the next paint) =";
	int shown = 0;
	double rest = s.totalMs;
	for ( const auto & kc : v ) {
		if ( shown >= 8 )
			break;
		const Key & k = kc.first;
		QByteArray name;
		switch ( k.kind ) {
		case AfterSwap: name = "after the swap, before any event"; break;
		case Asleep: name = "the loop asleep"; break;
		case Woken: name = "the loop woken, before its first event"; break;
		case Native: name = "window message 0x" + QByteArray::number( k.code, 16 ); break;
		default: name = eventName( k.code ) + " to " + QByteArray( k.cls ? k.cls : "?" ); break;
		}
		line += QByteArray( shown ? " +" : "" ) + " " + name + " " + QByteArray::number( kc.second.ms / f, 'f', 3 ) + " ("
			+ QByteArray::number( double( kc.second.n ) / f, 'f', 1 ) + "/frame)";
		rest -= kc.second.ms;
		shown++;
	}
	line += " + the rest " + QByteArray::number( std::max( 0.0, rest ) / f, 'f', 3 )
		+ " (each part runs from its mark to the next one)";
	std::fprintf( stderr, "%s\n", line.constData() );
	s.used = 0;
	s.spill = Cost();
	s.totalMs = 0.0;
	s.frames = 0;
}

//! lane EMPTY240 R5-3: true when the thread's message queue holds nothing (no input, timer, paint, posted or sent message)
inline bool queueEmpty()
{
#ifdef Q_OS_WIN
	return ( GetQueueStatus( 0x04FF /* QS_ALLINPUT */ ) >> 16 ) == 0;
#else
	return false;
#endif
}

/* lane EMPTY240 R7-3: what waits in the thread's queue. 0 = nothing; 1 = only posted messages or due timers (Qt's own
 * wake message among them: every QCoreApplication::postEvent posts one, and removePostedEvents leaves it); 2 = input,
 * a paint or a sent message. */
inline int queueWaits()
{
#ifdef Q_OS_WIN
	const unsigned w = unsigned( GetQueueStatus( 0x04FF /* QS_ALLINPUT */ ) >> 16 );
	if ( w & 0x04E7u )	// QS_ALLINPUT without QS_POSTMESSAGE (0x08) and QS_TIMER (0x10)
		return 2;
	return w ? 1 : 0;
#else
	return 2;
#endif
}

}	// namespace WwBetween

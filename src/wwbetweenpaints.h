#pragma once

#include <QAbstractEventDispatcher>
#include <QAbstractNativeEventFilter>
#include <QByteArray>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QObject>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
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
};

struct Cost
{
	double ms = 0.0;
	long long n = 0;
};

struct State
{
	bool installed = false, on = false;
	QElapsedTimer clock;
	qint64 last = 0, winAt = 0;
	Key at;
	std::map<Key, Cost> by;
	double totalMs = 0.0;
	long long frames = 0;
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
	Cost & c = s.by[s.at];
	c.ms += double( now - s.last ) * 1e-6;
	c.n++;
	s.totalMs += double( now - s.last ) * 1e-6;
	s.last = now;
	s.at = next;
}

struct AppFilter : QObject
{
	bool eventFilter( QObject * o, QEvent * e ) override
	{
		if ( st().on ) {
			Key k;
			k.kind = Event;
			k.code = int( e->type() );
			k.cls = o ? o->metaObject()->className() : "none";
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
	std::vector<std::pair<Key, Cost>> v( s.by.begin(), s.by.end() );
	std::sort( v.begin(), v.end(), []( const auto & a, const auto & b ) { return a.second.ms > b.second.ms; } );
	const double f = double( std::max<long long>( 1, s.frames ) );
	QByteArray line = "between paints: " + QByteArray::number( s.totalMs / f, 'f', 2 ) + " ms a frame ("
		+ QByteArray::number( s.frames ) + " frames, the swap's return to the next paint) =";
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
	s.by.clear();
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

}	// namespace WwBetween

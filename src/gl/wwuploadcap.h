#pragma once

#include "wwswitch.h"

#include <QElapsedTimer>
#include <QtGlobal>

#include <algorithm>
#include <cstdio>

/* lane EMPTY240 R4-1: no paint over ~100 ms on the GPU. Build 76's first cell paint uploaded 666 textures (87.5 MB) and
 * 510 meshes (64.2 MB) inside its passes: 1862 ms on the GPU (the zones hold the CPU's loads, the GPU idles meanwhile).
 * Each paint now has an upload budget (kBudgetMs of load time or kBudgetMB of bytes, whichever comes first, at least one
 * upload a paint); a texture or mesh over it is left for the next paint (the draw skips it this paint, as a texture
 * still on the disk would). Red WW_UPLOAD_RED=1: no budget (build 76). GUI thread only (the GL thread). */
namespace WwUp
{

constexpr qint64 kBudgetMs = 16;
constexpr qint64 kBudgetMB = 64;

struct State
{
	bool inPaint = false, red = false;
	int depth = 0, items = 0;
	qint64 bytes = 0, ns = 0, bytes0 = 0;
	const qint64 * meter = nullptr;	// the texture cache's uploaded bytes (PassTime::cacheCounts().texBytes)
	QElapsedTimer clock;	// the outermost charge's
	bool meshMayDefer = false, meshDeferredHit = false;
	quint64 deferredAll = 0;	// every deferral so far (a cached picture drawn with one is drawn again)
	// the 5 s window of the `upload cap:` line
	QElapsedTimer win;
	qint64 winPaints = 0, winCapped = 0, winTex = 0, winMesh = 0, winBytes = 0, winNs = 0, winMaxNs = 0;
};

inline State & st()
{
	static State s;
	return s;
}

//! true when this paint's budget is used up (never outside a paint, never for the red, never before the first upload)
inline bool spent()
{
	const State & s = st();
	if ( !s.inPaint || s.red || s.items <= 0 )
		return false;
	qint64 b = s.bytes, n = s.ns;
	if ( s.depth > 0 ) {
		if ( s.meter )
			b += *s.meter - s.bytes0;
		n += s.clock.nsecsElapsed();
	}
	return b >= ( kBudgetMB << 20 ) || n >= kBudgetMs * 1000000;
}

//! one upload (outermost only: a texture load that drains others charges them all once)
struct Charge
{
	bool top;
	Charge()
	{
		State & s = st();
		top = s.depth++ == 0;
		if ( top ) {
			s.items++;
			s.bytes0 = s.meter ? *s.meter : 0;
			s.clock.start();
		}
	}
	~Charge()
	{
		State & s = st();
		s.depth--;
		if ( top ) {
			s.ns += s.clock.nsecsElapsed();
			if ( s.meter )
				s.bytes += *s.meter - s.bytes0;
		}
	}
	static void addBytes( qint64 b ) { st().bytes += b; }	// bytes the meter does not see (meshes)
};

inline void deferTex()
{
	State & s = st();
	s.winTex++;
	s.deferredAll++;
}

inline void deferMesh()
{
	State & s = st();
	s.winMesh++;
	s.deferredAll++;
	s.meshDeferredHit = true;
}

//! glview paintGL: the paint's budget opens here and closes when this goes out of scope
struct Paint
{
	explicit Paint( const qint64 * texBytes )
	{
		State & s = st();
		s.red = wwEnvInt( "WW_UPLOAD_RED" ) == 1;
		s.inPaint = true;
		s.items = 0;
		s.bytes = s.ns = 0;
		s.meter = texBytes;
		if ( !s.win.isValid() )
			s.win.start();
	}
	~Paint()
	{
		State & s = st();
		s.inPaint = false;
		s.winPaints++;
		s.winBytes += s.bytes;
		s.winNs += s.ns;
		s.winMaxNs = std::max( s.winMaxNs, s.ns );
		if ( spentAfter( s ) )
			s.winCapped++;
		if ( s.win.elapsed() < 5000 )
			return;
		if ( s.winTex + s.winMesh > 0 )
			std::fprintf( stderr, "upload cap: %lld of %lld paints reached the budget (%lld ms or %lld MB a paint); %lld texture binds "
				"and %lld mesh draws waited for a later paint; uploads %.1f MB, %.1f ms load time in all, longest paint %.1f ms "
				"(red WW_UPLOAD_RED=1: no budget, build 76)\n",
				(long long) s.winCapped, (long long) s.winPaints, (long long) kBudgetMs, (long long) kBudgetMB,
				(long long) s.winTex, (long long) s.winMesh, double( s.winBytes ) / 1048576.0, double( s.winNs ) * 1e-6,
				double( s.winMaxNs ) * 1e-6 );
		s.win.start();
		s.winPaints = s.winCapped = s.winTex = s.winMesh = s.winBytes = s.winNs = s.winMaxNs = 0;
	}
	static bool spentAfter( const State & s )
	{
		return !s.red && s.items > 0 && ( s.bytes >= ( kBudgetMB << 20 ) || s.ns >= kBudgetMs * 1000000 );
	}
};

}	// namespace WwUp

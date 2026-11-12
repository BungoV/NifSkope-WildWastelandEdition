#pragma once

#include "wwsyncred.h"

#include <QElapsedTimer>

#include <algorithm>
#include <cstdio>

/* lane EMPTY240 R4-2: do the CPU and the GPU overlap? Build 76's empty-spot frame read 4.68 ms of wall for 1.67 ms of
 * passes. Every GL call that can hold the draw thread until the GPU catches up is timed here by name (WwSync::Wait), and
 * the `frame overlap:` line (every 5 s, beside `hardware use:`) sets a paint's CPU time (its start to the swap's
 * return), its GPU time (the timestamp ring) and the wall side by side: a wall near max(CPU, GPU) = they overlap, near
 * their sum = they take turns. GL thread only. */
namespace WwSync
{

enum Kind
{
	Swap,	// the swap's return (vsync or a full GPU queue)
	CapFence,	// the frames-in-flight cap (glview wwGpuFrameBegin: the paint 2 back)
	Finish,	// glFinish
	ClientWait,	// glClientWaitSync with a timeout (a read-back fence, the bindless ring's wrap)
	QueryRead,	// a query result read without its availability first
	ReadPixels,	// glReadPixels into client memory
	GetTexImage,	// glGetTexImage into client memory
	MapRead,	// a buffer map without GL_MAP_UNSYNCHRONIZED_BIT
	BufferRead,	// glGetBufferSubData
	KindCount
};

inline const char * kindName( int k )
{
	static const char * const n[KindCount] = { "swap", "frame cap fence", "glFinish", "glClientWaitSync",
		"query read unchecked", "glReadPixels", "glGetTexImage", "buffer map", "glGetBufferSubData" };
	return k >= 0 && k < KindCount ? n[k] : "?";
}

struct Stats
{
	double ns[KindCount] = {};
	long long n[KindCount] = {};
	double cpuMs = 0.0, gpuMs = 0.0;
	int cpuFrames = 0, gpuFrames = 0;
};

inline Stats & stats()
{
	static Stats s;
	return s;
}

inline void add( int k, double ns )
{
	Stats & s = stats();
	s.ns[k] += ns;
	s.n[k]++;
}

//! times one blocking GL call by name
struct Wait
{
	int k;
	QElapsedTimer t;
	explicit Wait( int kind ) : k( kind ) { t.start(); }
	~Wait() { add( k, double( t.nsecsElapsed() ) ); }
};

//! one whole paint: its start to the swap's return, and the swap's own part (ms)
inline void cpuFrame( double ms, double swapMs )
{
	Stats & s = stats();
	s.cpuMs += ms;
	s.cpuFrames++;
	add( Swap, swapMs * 1.0e6 );
}

inline void gpuFrame( double ms )
{
	Stats & s = stats();
	s.gpuMs += ms;
	s.gpuFrames++;
}

//! the `frame overlap:` line over `frames` paints in `wallMs`, then a new window
inline void line( double wallMs, int frames )
{
	Stats & s = stats();
	const double f = double( std::max( 1, frames ) );
	const double wall = wallMs / f, cpu = s.cpuMs / double( std::max( 1, s.cpuFrames ) ), gpu = s.gpuMs / double( std::max( 1, s.gpuFrames ) );
	const double lo = std::min( cpu, gpu ), hi = std::max( cpu, gpu );
	const double ov = lo > 0.0 ? std::clamp( ( cpu + gpu - wall ) / lo, 0.0, 1.0 ) : 0.0;
	double blocked = 0.0;
	char parts[640];
	int at = 0;
	for ( int k = 0; k < KindCount; k++ ) {
		const double ms = s.ns[k] * 1.0e-6 / f;
		if ( k != Swap )
			blocked += ms;
		at += std::snprintf( parts + at, sizeof( parts ) - size_t( at ), "%s%s %.2f (%lld)", k ? ", " : "", kindName( k ), ms,
			(long long) s.n[k] );
		if ( at >= int( sizeof( parts ) ) - 1 )
			break;
	}
	std::fprintf( stderr, "frame overlap: wall %.2f ms a frame, CPU %.2f (paint start to swap return), GPU %.2f (timestamps); "
		"max %.2f, sum %.2f: %s (%.0f %% of the shorter side runs beside the other); the CPU blocked a frame, ms (calls): %s; "
		"blocked outside the swap %.2f (red WW_SYNC_RED=1: the old waits)\n",
		wall, cpu, gpu, hi, cpu + gpu, wall <= hi + 0.25 * lo ? "they overlap" : "they take turns", 100.0 * ov, parts, blocked );
	s = Stats();
}

}	// namespace WwSync

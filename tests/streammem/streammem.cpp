// lane STREAMMEM1: the cell-stream walk's geometry cache, offline, on the real driver. Each cell's meshes go up as
// GL buffers through a copy of glcontext.cpp's draw-ordered cache (insert/use, shrinkCache's rule, evictUndrawnSince)
// and the view's real GeoKeepClock (src/gl/geokeepclock.h). 20 crossings out and back, 10 paints a virtual second.
// Green: private bytes after crossing 20 within 5% of crossing 1, no mesh of the last paint uploaded again.
// Red (argument "red" = WW_STREAM1_RED=geokeep) grows and must FAIL. The driver keeps its pools: private never falls back.
#include "geokeepclock.h"

#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>

#include <windows.h>
#include <psapi.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <vector>

static double privateMb()
{
	PROCESS_MEMORY_COUNTERS_EX pmc = {};
	GetProcessMemoryInfo( GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>( &pmc ), sizeof( pmc ) );
	return double( pmc.PrivateUsage ) / 1048576.0;
}

struct Entry
{
	std::uint64_t key = 0;
	Entry * prev = nullptr, * next = nullptr;
	std::uint32_t usedEpoch = 0;
	GLuint vao = 0, vbo = 0, ebo = 0;
	size_t bytes = 0;
};

// glcontext.cpp's cache, its list and rules kept line for line; buffers as ShapeData makes them
struct Cache
{
	QOpenGLExtraFunctions * f = nullptr;
	std::unordered_map<std::uint64_t, Entry *> map;
	Entry * last = nullptr;
	size_t used = 0, maxBytes = 0x08000000, budget = size_t( 1536 ) << 20;
	std::uint32_t epoch = 1;
	qint64 uploads = 0, evicted = 0;
	std::vector<char> blob;

	void drop( Entry * d )
	{
		used -= d->bytes + 24576;
		if ( d->prev == d ) {
			last = nullptr;
		} else {
			d->prev->next = d->next;
			d->next->prev = d->prev;
		}
		f->glDeleteVertexArrays( 1, &d->vao );
		f->glDeleteBuffers( 1, &d->vbo );
		f->glDeleteBuffers( 1, &d->ebo );
		map.erase( d->key );
		delete d;
		evicted++;
	}
	void bind( std::uint64_t key, size_t bytes )
	{
		auto it = map.find( key );
		if ( it != map.end() ) {
			Entry * d = it->second;
			if ( d != last ) {
				d->prev->next = d->next;
				d->next->prev = d->prev;
				d->prev = last;
				d->next = last->next;
				d->prev->next = d;
				d->next->prev = d;
			}
			last = d;
			d->usedEpoch = epoch;
			f->glBindVertexArray( d->vao );
			return;
		}
		Entry * d = new Entry;
		d->key = key;
		d->bytes = bytes;
		if ( blob.size() < bytes )
			blob.assign( bytes, char( 1 ) );
		f->glGenVertexArrays( 1, &d->vao );
		f->glBindVertexArray( d->vao );
		f->glGenBuffers( 1, &d->vbo );
		f->glBindBuffer( GL_ARRAY_BUFFER, d->vbo );
		f->glBufferData( GL_ARRAY_BUFFER, GLsizeiptr( bytes * 3 / 4 ), blob.data(), GL_STATIC_DRAW );
		f->glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, nullptr );
		f->glEnableVertexAttribArray( 0 );
		f->glGenBuffers( 1, &d->ebo );
		f->glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, d->ebo );
		f->glBufferData( GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr( bytes / 4 ), blob.data(), GL_STATIC_DRAW );
		if ( !last ) {
			d->prev = d;
			d->next = d;
		} else {
			d->prev = last;
			d->next = last->next;
			d->prev->next = d;
			d->next->prev = d;
		}
		d->usedEpoch = epoch;
		last = d;
		map[key] = d;
		used += bytes + 24576;
		uploads++;
	}
	void shrink()	// shrinkCache( false ), the budget on
	{
		constexpr std::uint32_t keepEpochs = 1800;
		while ( last ) {
			Entry * d = last->next;
			if ( used < maxBytes || d->usedEpoch == epoch )
				break;
			if ( used < budget && epoch - d->usedEpoch < keepEpochs )
				break;
			drop( d );
		}
		epoch++;
	}
	int evictUndrawnSince( std::uint32_t since )	// as glcontext.cpp
	{
		int n = 0;
		while ( last ) {
			Entry * d = last->next;
			if ( d->usedEpoch == epoch || std::int32_t( d->usedEpoch - since ) >= 0 )
				break;
			drop( d );
			n++;
		}
		return n;
	}
};

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	const bool red = argc > 1 && std::strcmp( argv[1], "red" ) == 0;
	QGuiApplication app( argc, argv );
	QSurfaceFormat fmt;
	fmt.setVersion( 3, 3 );
	fmt.setProfile( QSurfaceFormat::CoreProfile );
	QOpenGLContext ctx;
	ctx.setFormat( fmt );
	QOffscreenSurface surf;
	surf.setFormat( fmt );
	surf.create();
	if ( !ctx.create() || !ctx.makeCurrent( &surf ) ) {
		std::printf( "FAIL no GL context\n" );
		return 2;
	}
	Cache c;
	c.f = ctx.extraFunctions();
	GeoKeepClock clock;
	const std::uint32_t keep = 5;
	constexpr int kShapes = 40, kFar = 24, kPaints = 130;   // a cell's meshes; far shapes re-masked per step
	constexpr size_t kShape = size_t( 192 ) << 10, kFarBytes = size_t( 1 ) << 20;
	int cx = 0, failures = 0;
	qint64 hotReuploads = 0;
	double first = 0.0, lastMb = 0.0, peak = 0.0;
	std::uint32_t paint = 0;
	auto cellKey = []( int x, int y, int i ) {
		return ( std::uint64_t( std::uint32_t( x + 1000 ) ) << 40 ) | ( std::uint64_t( std::uint32_t( y + 1000 ) ) << 20 ) | std::uint64_t( i );
	};
	for ( int step = 0; step <= 20; step++ ) {
		if ( step > 0 )
			cx += step <= 10 ? 1 : -1;	// out 10 cells east, back to the start
		for ( int p = 0; p < kPaints; p++, paint++ ) {
			const std::uint32_t sec = paint / 10U + 1U;
			if ( !red ) {
				if ( const std::uint32_t since = clock.paint( sec, c.epoch, keep ) )
					c.evictUndrawnSince( since );
			}
			const qint64 before = c.uploads;
			for ( int y = -2; y <= 2; y++ )
				for ( int x = cx - 2; x <= cx + 2; x++ )
					for ( int i = 0; i < kShapes; i++ )
						c.bind( cellKey( x, y, i ), kShape );
			for ( int j = 0; j < kFar; j++ )	// the far mask's variant for this block (FARHOLE2 re-uploads it)
				c.bind( ( std::uint64_t( 1 ) << 62 ) | ( std::uint64_t( cx + 1000 ) << 8 ) | std::uint64_t( j ), kFarBytes );
			if ( p > 0 )
				hotReuploads += c.uploads - before;	// the same set as the last paint: never uploaded again
			c.shrink();
		}
		c.f->glFinish();
		const double mb = privateMb();
		if ( step == 1 )	// after one crossing: the driver's pool for one crossing's churn is made once (green +150 MB)
			first = mb;
		lastMb = mb;
		peak = std::max( peak, mb );
		std::printf( "step %d centre %d private %.0f MB cache %.0f MB shapes %zu uploads %lld evicted %lld\n", step, cx,
			mb, double( c.used ) / 1048576.0, c.map.size(), (long long) c.uploads, (long long) c.evicted );
	}
	const double ratio = lastMb / first;
	const bool flat = ratio <= 1.05;
	std::printf( "%s: crossing 1 %.0f MB, crossing 20 %.0f MB (x%.3f), peak %.0f MB\n", red ? "red" : "green", first, lastMb, ratio, peak );
	if ( hotReuploads != 0 ) {
		std::printf( "FAIL a mesh drawn the paint before was uploaded again: %lld\n", (long long) hotReuploads );
		failures++;
	}
	if ( !flat ) {	// the red must land here
		std::printf( "FAIL memory grew past 5%% (x%.3f)\n", ratio );
		failures++;
	}
	std::printf( "%d failures\n%s\n", failures, failures ? "FAIL" : "PASS" );
	return failures ? 1 : 0;
}

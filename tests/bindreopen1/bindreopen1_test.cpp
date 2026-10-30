// lane BINDREOPEN1: the bindless geometry pool over a stream walk, and the reopen teardown order, with no GL.
//   bindreopen1_test pool             the real arena (src/gl/cellbindlesspool.h) under a twin of cellbindless.cpp's
//                                     pool policy (600-frame sweep, a fence per frame 2 frames late, shrink check every
//                                     64 frames): a 22-step walk of a 5x5 window. POOL PASS / POOL FAIL
//   bindreopen1_test pool poolgrow    the red (WW_BINDLESS_RED=poolgrow, the append-only arena): must print POOL FAIL
//   bindreopen1_test reopen           the real order (src/gl/cellbindlessreopen.h) on a mock GL that aborts on a use
//                                     after delete, 60 reopens + a context re-made every 10th. REOPEN PASS
//   bindreopen1_test reopen reopenraw the red (WW_BINDLESS_RED=reopenraw): must print REOPEN ABORT
// rc: 0 pass, 1 fail/abort, 2 usage.
#include "gl/cellbindlesspool.h"
#include "gl/cellbindlessreopen.h"

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace
{
// ---------------------------------------------------------------- the pool walk
struct ShapeT
{
	std::uint32_t id = 0, verts = 0, idx = 0;
	int cls = 0;
};

const int kFloats[2] = { 12, 16 };	// two vertex layouts (floats per vertex)

std::vector<ShapeT> cellShapes( int cx, int cy, std::uint32_t & nextId )
{
	std::mt19937 rng( std::uint32_t( ( cx + 1000 ) * 7919 + ( cy + 1000 ) * 104729 ) );
	std::vector<ShapeT> v( 40 + rng() % 41 );
	for ( ShapeT & s : v ) {
		s.id = nextId++;
		const std::uint32_t r = rng() % 100;
		s.verts = r < 70 ? 30 + rng() % 2000 : 2000 + rng() % 20000;	// many small, some big shapes
		s.idx = s.verts * ( 3 + rng() % 4 ) / 2;
		s.cls = int( rng() % 2 );
	}
	return v;
}

struct Entry
{
	WwBlPool::Span span;
	int cls = 0;
	std::uint64_t lastFrame = 0;
};

struct ClassT
{
	WwBlPool::Arena a;
	std::vector<std::uint32_t> v, i;	// the "GPU" buffers: the owner id of each element (0 = none)
};

double mb( const ClassT & k, int c )
{
	return ( double( k.a.vertCap ) * kFloats[c] * 4.0 + double( k.a.idxCap ) * 2.0 ) / 1048576.0;
}
double liveMb( const ClassT & k, int c )
{
	return ( double( k.a.liveVerts() ) * kFloats[c] * 4.0 + double( k.a.liveIdx() ) * 2.0 ) / 1048576.0;
}

int poolWalk( bool red )
{
	ClassT cls[2];
	for ( ClassT & k : cls )
		k.a.grow = red;
	std::map<std::uint32_t, Entry> entries;	// keyed by shape (cellbindless.cpp: Shape *)
	std::map<std::uint32_t, ShapeT> shapeOf;
	std::map<std::pair<int, int>, std::vector<ShapeT>> cellCache;
	std::uint32_t nextId = 1;
	std::uint64_t frameNo = 1, doneEpoch = 0;
	std::vector<std::uint64_t> fences;	// frames fenced, signalled 2 frames later
	struct Flight { int cls; WwBlPool::Span s; std::uint64_t epoch; };
	std::vector<Flight> inFlight;	// released spans the GPU may still read (until their frame's fence)
	long long rebuilds = 0, shrinks = 0, inflightHits = 0, bad = 0, overlaps = 0;

	auto upload = [&]( ClassT & k, const WwBlPool::Span & s, std::uint32_t id ) {
		for ( std::uint32_t x = 0; x < s.numVerts; x++ )
			k.v[s.firstVert + x] = id;
		for ( std::uint32_t x = 0; x < s.numIndices; x++ )
			k.i[s.firstIndex + x] = id;
	};
	auto rebuild = [&]( int c, std::uint32_t verts, std::uint32_t idx ) {
		ClassT & k = cls[c];
		std::uint32_t nv = 0, ni = 0;
		k.a.plan( verts, idx, nv, ni );
		std::vector<std::pair<std::uint32_t, Entry *>> live;
		for ( auto & e : entries )
			if ( e.second.cls == c )
				live.push_back( { e.first, &e.second } );
		std::sort( live.begin(), live.end(), []( auto & x, auto & y ) { return x.second->span.firstVert < y.second->span.firstVert; } );
		std::vector<WwBlPool::Span> spans;
		for ( auto & l : live )
			spans.push_back( l.second->span );
		WwBlPool::pack( spans, k.a, nv, ni );
		std::vector<std::uint32_t> v( nv, 0 ), i( ni, 0 );
		k.v.swap( v );
		k.i.swap( i );
		for ( std::size_t j = 0; j < live.size(); j++ ) {
			live[j].second->span = spans[j];
			upload( k, spans[j], live[j].first );
		}
		// a rebuild copies into NEW buffers: the old buffer's in-flight reads are not this buffer's
		inFlight.erase( std::remove_if( inFlight.begin(), inFlight.end(), [c]( const Flight & f ) { return f.cls == c; } ), inFlight.end() );
		rebuilds++;
	};
	auto overlapsFlight = [&]( int c, const WwBlPool::Span & s ) {
		for ( const Flight & f : inFlight )
			if ( f.cls == c && f.epoch > doneEpoch
				&& ( ( s.firstVert < f.s.firstVert + f.s.numVerts && f.s.firstVert < s.firstVert + s.numVerts && s.numVerts && f.s.numVerts )
					|| ( s.firstIndex < f.s.firstIndex + f.s.numIndices && f.s.firstIndex < s.firstIndex + s.numIndices && s.numIndices
						&& f.s.numIndices ) ) )
				return true;
		return false;
	};
	auto release = [&]( std::uint32_t id, Entry & e ) {
		cls[e.cls].a.release( e.span, frameNo );
		if ( !red )
			inFlight.push_back( { e.cls, e.span, frameNo } );
		(void)id;
	};
	auto draw = [&]( const ShapeT & s ) {
		auto it = entries.find( s.id );
		if ( it == entries.end() ) {
			ClassT & k = cls[s.cls];
			WwBlPool::Span sp;
			if ( !k.a.alloc( s.verts, s.idx, sp ) ) {
				rebuild( s.cls, s.verts, s.idx );
				if ( !k.a.alloc( s.verts, s.idx, sp ) ) {
					std::printf( "POOL FAIL: no room after a rebuild\n" );
					std::exit( 1 );
				}
			}
			if ( overlapsFlight( s.cls, sp ) )
				inflightHits++;
			upload( k, sp, s.id );
			Entry e;
			e.span = sp;
			e.cls = s.cls;
			it = entries.emplace( s.id, e ).first;
			shapeOf[s.id] = s;
		}
		it->second.lastFrame = frameNo;
	};
	auto frame = [&]() {	// cellbindless.cpp WwBl::frame
		bool pending = false;
		for ( ClassT & k : cls )
			pending |= !k.a.graves.empty();
		if ( pending )
			fences.push_back( frameNo );
		while ( !fences.empty() && fences.front() + 2 <= frameNo ) {	// the GPU two frames behind
			doneEpoch = std::max( doneEpoch, fences.front() );
			fences.erase( fences.begin() );
		}
		for ( ClassT & k : cls )
			k.a.retire( doneEpoch );
		inFlight.erase( std::remove_if( inFlight.begin(), inFlight.end(), [&]( const Flight & f ) { return f.epoch <= doneEpoch; } ),
			inFlight.end() );
		frameNo++;
		if ( frameNo % 64 == 0 ) {
			for ( auto it = entries.begin(); it != entries.end(); ) {
				if ( frameNo - it->second.lastFrame > 600 ) {
					release( it->first, it->second );
					it = entries.erase( it );
				} else {
					++it;
				}
			}
			for ( int c = 0; c < 2; c++ )
				if ( cls[c].a.shrinkWanted() ) {
					rebuild( c, 0, 0 );
					shrinks++;
				}
		}
	};
	auto check = [&]() {
		for ( int c = 0; c < 2; c++ ) {
			std::vector<std::pair<std::uint32_t, std::uint32_t>> vr, ir;
			for ( auto & e : entries ) {
				if ( e.second.cls != c )
					continue;
				const WwBlPool::Span & s = e.second.span;
				for ( std::uint32_t x = 0; x < s.numVerts; x++ )
					if ( cls[c].v[s.firstVert + x] != e.first ) {
						bad++;
						break;
					}
				for ( std::uint32_t x = 0; x < s.numIndices; x++ )
					if ( cls[c].i[s.firstIndex + x] != e.first ) {
						bad++;
						break;
					}
				if ( s.numVerts )
					vr.push_back( { s.firstVert, s.firstVert + s.numVerts } );
				if ( s.numIndices )
					ir.push_back( { s.firstIndex, s.firstIndex + s.numIndices } );
			}
			for ( auto * r : { &vr, &ir } ) {
				std::sort( r->begin(), r->end() );
				for ( std::size_t j = 1; j < r->size(); j++ )
					if ( ( *r )[j].first < ( *r )[j - 1].second )
						overlaps++;
			}
		}
	};

	const int kSteps = 22, kFrames = 240;
	std::vector<double> capAt, heldAt, trueAt;
	double peakCap = 0.0, peakHeld = 0.0, peakTrue = 0.0;
	std::printf( "step  cap MB  held-live MB  window MB  entries\n" );
	// steps 22-25 (the tail): the window falls to the one centre cell (a small interior after the walk); the pool must
	// give its memory back
	for ( int step = 0; step < kSteps + 4; step++ ) {
		// the walk: one cell east per step, a turn north after step 10 (cell_stream.sh's walk.path shape)
		const int ws = std::min( step, kSteps - 1 );
		const int ccx = -15 + ( ws < 11 ? ws : 10 ), ccy = 17 + ( ws < 11 ? 0 : ws - 10 );
		const int rad = step < kSteps ? 2 : 0;
		std::vector<const ShapeT *> window;
		double trueMb = 0.0;
		for ( int dx = -rad; dx <= rad; dx++ )
			for ( int dy = -rad; dy <= rad; dy++ ) {
				auto key = std::make_pair( ccx + dx, ccy + dy );
				if ( !cellCache.count( key ) )
					cellCache[key] = cellShapes( key.first, key.second, nextId );
				for ( const ShapeT & s : cellCache[key] ) {
					window.push_back( &s );
					trueMb += ( double( s.verts ) * kFloats[s.cls] * 4.0 + double( s.idx ) * 2.0 ) / 1048576.0;
				}
			}
		for ( int f = 0; f < kFrames; f++ ) {
			for ( const ShapeT * s : window )
				draw( *s );
			frame();
		}
		check();
		const double cap = mb( cls[0], 0 ) + mb( cls[1], 1 ), held = liveMb( cls[0], 0 ) + liveMb( cls[1], 1 );
		capAt.push_back( cap );
		heldAt.push_back( held );
		trueAt.push_back( trueMb );
		peakCap = std::max( peakCap, cap );
		peakHeld = std::max( peakHeld, held );
		peakTrue = std::max( peakTrue, trueMb );
		std::printf( "%4d  %7.1f  %12.1f  %9.1f  %zu\n", step, cap, held, trueMb, entries.size() );
	}
	const double floorMb = 2.0 * ( double( WwBlPool::Arena::kFloorVerts ) * 16 * 4 + double( WwBlPool::Arena::kFloorIdx ) * 2 ) / 1048576.0;
	const long long reused = (long long)( cls[0].a.reused + cls[1].a.reused );
	const double endRatio = capAt[kSteps - 1] / capAt[3], peakRatio = peakCap / ( peakHeld + floorMb );
	const double tailRatio = capAt.back() / ( heldAt.back() * 1.6 + floorMb );	// after the tail: <= 1 (it shrank)
	std::printf( "pool %s: cap step3 %.1f MB, step21 %.1f MB (x%.3f, bar 1.3), peak cap %.1f MB vs peak held %.1f MB (x%.3f, bar 1.6), "
		"tail cap %.1f MB vs held %.1f MB (x%.3f of 1.6 held + floor, bar 1.0), peak window %.1f MB, reused %lld, rebuilds %lld, "
		"shrinks %lld, bad %lld, overlaps %lld, in-flight reuse %lld, free runs %zu/%zu\n",
		red ? "RED poolgrow" : "green", capAt[3], capAt[kSteps - 1], endRatio, peakCap, peakHeld, peakRatio, capAt.back(), heldAt.back(),
		tailRatio, peakTrue, reused, rebuilds, shrinks, bad, overlaps, inflightHits, cls[0].a.fv.runs.size() + cls[1].a.fv.runs.size(),
		cls[0].a.fi.runs.size() + cls[1].a.fi.runs.size() );

	// coalescing alone: three adjacent frees are one run; the tail run goes back to the append point
	WwBlPool::Arena t;
	t.vertCap = 1000;
	t.idxCap = 1000;
	WwBlPool::Span s1, s2, s3;
	t.alloc( 10, 10, s1 );
	t.alloc( 20, 20, s2 );
	t.alloc( 30, 30, s3 );
	t.release( s1, 1 );
	t.release( s2, 1 );
	t.retire( 1 );
	const bool coalesced = t.fv.runs.size() == 1 && t.fv.runs.begin()->second == 30 && t.vertUsed == 60;
	t.release( s3, 2 );
	t.retire( 2 );
	const bool trimmed = t.fv.runs.empty() && t.vertUsed == 0 && t.idxUsed == 0;
	std::printf( "coalesce: one run of 30 %s, tail trimmed to 0 %s\n", coalesced ? "yes" : "NO", trimmed ? "yes" : "NO" );

	const bool pass = endRatio <= 1.3 && peakRatio <= 1.6 && tailRatio <= 1.0 && reused > 0 && !bad && !overlaps && !inflightHits
		&& coalesced && trimmed;
	std::printf( pass ? "POOL PASS\n" : "POOL FAIL\n" );
	return pass ? 0 : 1;
}

// ---------------------------------------------------------------- the reopen order on a mock GL
struct Mock
{
	bool raw = false;
	std::set<std::uint32_t> texAlive;
	std::map<std::uint32_t, std::uint64_t> handleOf;	// the Ctx's handle table (name -> handle)
	std::map<std::uint64_t, std::uint32_t> texOfHandle;	// the driver: handle -> texture object (serial)
	std::set<std::uint64_t> resident;
	std::map<std::uint32_t, std::uint32_t> serialOfName;	// texture name -> object serial (names recycle)
	std::set<std::uint32_t> serialAlive;
	std::uint32_t nextSerial = 1;
	std::uint64_t staged[8] = {};
	std::vector<std::uint64_t> rowsInFlight;	// handles named by rows the GPU has not finished
	std::size_t poolEntries = 0, poolEntriesDead = 0;
	bool ringAlive = true, mapValid = true;	// the ring buffer and the Ctx's mapped pointer
	bool ctxForgotten = false;
	std::string abortWhy;

	void fail( const std::string & why )
	{
		if ( abortWhy.find( why ) == std::string::npos )
			abortWhy += ( abortWhy.empty() ? "" : "; " ) + why;
	}
	// ---- the steps (src/gl/cellbindlessreopen.h)
	void finish() { rowsInFlight.clear(); }
	void fenceWait() { rowsInFlight.clear(); }
	void ringReset() {}
	void rowZero() { std::memset( staged, 0, sizeof( staged ) ); }
	void handlesNonResident()
	{
		for ( auto & h : handleOf ) {
			if ( std::find( rowsInFlight.begin(), rowsInFlight.end(), h.second ) != rowsInFlight.end() )
				fail( "a handle made non-resident while a row in flight names it" );
			resident.erase( h.second );
		}
		handleOf.clear();
	}
	void frozenClear() {}
	void poolDrop() { poolEntries = poolEntriesDead = 0; }
	void unmapRings() { mapValid = false; }
	void deleteBuffers() { ringAlive = false; }
	void forgetCtx() { ctxForgotten = true; }

	// ---- the world around them
	std::uint32_t genTexture()
	{
		std::uint32_t n = 1;
		while ( texAlive.count( n ) )
			n++;	// GL hands out the lowest free name
		texAlive.insert( n );
		serialOfName[n] = nextSerial;
		serialAlive.insert( nextSerial++ );
		return n;
	}
	void deleteTexture( std::uint32_t n )	// TexCache::flush after WwBl::forgetAll
	{
		for ( std::uint64_t h : resident )
			if ( texOfHandle[h] == serialOfName[n] )
				fail( "a texture deleted while its handle is resident" );
		serialAlive.erase( serialOfName[n] );
		texAlive.erase( n );
	}
	std::uint64_t handleOfBound( std::uint32_t n )	// WwBl::handleOfBound
	{
		auto it = handleOf.find( n );
		if ( it != handleOf.end() )
			return it->second;
		const std::uint64_t h = ( std::uint64_t( serialOfName[n] ) << 20 ) | 0xB1;
		texOfHandle[h] = serialOfName[n];
		resident.insert( h );
		handleOf[n] = h;
		return h;
	}
	void commit()	// WwBl::commit: the staged row goes to the ring
	{
		if ( !mapValid || !ringAlive )
			fail( "a row written through the mapped pointer of a buffer that is gone" );
		for ( std::uint64_t h : staged ) {
			if ( !h )
				continue;
			if ( !serialAlive.count( texOfHandle[h] ) )
				fail( "a row names the handle of a deleted texture (the staged row kept it)" );
			rowsInFlight.push_back( h );
		}
	}
};

int reopenRun( bool raw )
{
	Mock m;
	m.raw = raw;
	std::vector<std::uint32_t> tex;
	auto open = [&]() {
		tex.clear();
		for ( int k = 0; k < 64; k++ )
			tex.push_back( m.genTexture() );
		m.poolEntries = 200;
	};
	auto drawFrame = [&]( int cyc ) {
		for ( int d = 0; d < 200; d++ ) {
			/* a material restages the slots its program samples: all seven, or (no greyscale / env map) five. The case
			 * tested: the first draw of a frame is a five-slot one, so its row carries slots 5-6 from the draw before
			 * (after a reopen: the old document's) -- whether a real program then samples them is not modelled */
			(void)cyc;
			const int slots = d % 3 == 0 ? 5 : 7;
			for ( int s = 0; s < slots; s++ )
				m.staged[s] = m.handleOfBound( tex[std::size_t( d * 7 + s ) % tex.size()] );
			m.commit();
		}
	};
	open();
	drawFrame( 0 );
	for ( int cyc = 1; cyc <= 60 && m.abortWhy.empty(); cyc++ ) {
		drawFrame( cyc );	// the old document's last frame, not finished on the GPU
		// the reopen: Scene::clear deletes the shapes, then TexCache::flush -> WwBl::forgetAll, then the textures go
		m.poolEntriesDead = m.poolEntries;
		WwBlReopen::teardown( m, raw );
		if ( m.poolEntriesDead )
			m.fail( "the pool kept " + std::to_string( m.poolEntriesDead ) + " entries of deleted shapes across the reopen" );
		for ( std::uint32_t n : tex )
			m.deleteTexture( n );
		if ( cyc % 10 == 0 ) {	// the view's context re-made (a new context at the same address)
			WwBlReopen::contextGone( m, raw );
			if ( !m.ctxForgotten ) {	// the red: the old Ctx (and its pointer) is found again
				m.ringAlive = false;
			} else {	// a fresh Ctx: init makes a new ring and maps it
				m.ringAlive = m.mapValid = true;
				m.ctxForgotten = false;
			}
		}
		open();
		drawFrame( cyc );
		if ( !m.abortWhy.empty() ) {
			std::printf( "REOPEN ABORT cycle %d: %s%s\n", cyc, m.abortWhy.c_str(), raw ? " (RED reopenraw)" : "" );
			return 1;
		}
	}
	std::printf( "reopen %s: 60 reopens, 6 contexts re-made, no use after delete\nREOPEN PASS\n", raw ? "RED reopenraw" : "green" );
	return 0;
}
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	const std::string what = argc > 1 ? argv[1] : "", red = argc > 2 ? argv[2] : "";
	if ( what == "pool" )
		return poolWalk( red == "poolgrow" );
	if ( what == "reopen" )
		return reopenRun( red == "reopenraw" );
	std::printf( "usage: bindreopen1_test pool|reopen [poolgrow|reopenraw]\n" );
	return 2;
}

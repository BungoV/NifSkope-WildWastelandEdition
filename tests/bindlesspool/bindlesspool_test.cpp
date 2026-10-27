// lane BINDLESS2: the geometry pool's offsets (src/gl/cellbindlesspool.h), run on a CPU copy of the arena.
// g++ -std=c++17 -I<tree>/src tests/bindlesspool/bindlesspool_test.cpp            -> POOL PASS, rc 0
// g++ -std=c++17 -I<tree>/src -DPOOL_RED_OFF1 tests/bindlesspool/bindlesspool_test.cpp -> the red: draws the span
//     allocated before each shape's (WW_BINDLESS_RED=pooloff1), must print POOL FAIL, rc 1
#include "gl/cellbindlesspool.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

namespace
{
struct ShapeT
{
	std::vector<float> verts;	// 3 per vertex
	std::vector<std::uint16_t> idx;
	WwBlPool::Span span;
	bool live = false;
	int order = 0;	// allocation order (the red's "previous")
};

struct Gpu
{
	std::vector<float> v;
	std::vector<std::uint16_t> i;
};

void upload( Gpu & g, const ShapeT & s )
{
	std::memcpy( g.v.data() + size_t( s.span.firstVert ) * 3, s.verts.data(), s.verts.size() * 4 );
	std::memcpy( g.i.data() + s.span.firstIndex, s.idx.data(), s.idx.size() * 2 );
}
}

int main()
{
	std::mt19937 rng( 1026 );
	WwBlPool::Arena a;
	Gpu g;
	std::vector<ShapeT> shapes( 400 );
	int order = 0, grows = 0, packs = 0;
	long long bad = 0, checked = 0, overlaps = 0;

	auto place = [&]( ShapeT & s ) {
		WwBlPool::Span sp;
		if ( !a.alloc( std::uint32_t( s.verts.size() / 3 ), std::uint32_t( s.idx.size() ), sp ) ) {
			// rebuild: the live spans packed in their order into a bigger (or equal) arena, contents copied
			std::uint32_t nv = 0, ni = 0;
			a.plan( std::uint32_t( s.verts.size() / 3 ), std::uint32_t( s.idx.size() ), nv, ni );
			std::vector<ShapeT *> live;
			for ( ShapeT & t : shapes )
				if ( t.live )
					live.push_back( &t );
			std::sort( live.begin(), live.end(), []( ShapeT * x, ShapeT * y ) { return x->span.firstVert < y->span.firstVert; } );
			std::vector<WwBlPool::Span> sp2;
			for ( ShapeT * t : live )
				sp2.push_back( t->span );
			std::vector<WwBlPool::Span> old = sp2;
			WwBlPool::pack( sp2, a, nv, ni );
			Gpu n;
			n.v.assign( size_t( nv ) * 3, -1.0f );
			n.i.assign( ni, 0xFFFF );
			for ( size_t k = 0; k < live.size(); k++ ) {
				std::memcpy( n.v.data() + size_t( sp2[k].firstVert ) * 3, g.v.data() + size_t( old[k].firstVert ) * 3,
					size_t( old[k].numVerts ) * 12 );
				std::memcpy( n.i.data() + sp2[k].firstIndex, g.i.data() + old[k].firstIndex, size_t( old[k].numIndices ) * 2 );
				live[k]->span = sp2[k];
			}
			g = std::move( n );
			grows++;
			packs++;
			if ( !a.alloc( std::uint32_t( s.verts.size() / 3 ), std::uint32_t( s.idx.size() ), sp ) ) {
				std::printf( "POOL FAIL: no room after a rebuild\n" );
				std::exit( 1 );
			}
		}
		s.span = sp;
		s.live = true;
		s.order = order++;
		upload( g, s );
	};

	for ( int round = 0; round < 40; round++ ) {
		for ( ShapeT & s : shapes ) {
			const int r = int( rng() % 10 );
			if ( s.live && r < 2 ) {	// released (a closed cell)
				a.release( s.span );
				s.live = false;
			} else if ( !s.live && r < 6 ) {	// a new shape
				const std::uint32_t nv = 3 + rng() % 2000;
				const std::uint32_t nt = 1 + rng() % 3000;
				s.verts.resize( size_t( nv ) * 3 );
				for ( float & f : s.verts )
					f = float( rng() % 100000 ) * 0.01f;
				s.idx.resize( size_t( nt ) * 3 );
				for ( auto & x : s.idx )
					x = std::uint16_t( rng() % nv );
				place( s );
			}
		}
		// the draws: every live shape's triangles through its draw arguments must read its own vertices
		std::vector<const ShapeT *> byOrder;
		for ( const ShapeT & s : shapes )
			if ( s.live )
				byOrder.push_back( &s );
		std::sort( byOrder.begin(), byOrder.end(), []( const ShapeT * x, const ShapeT * y ) { return x->order < y->order; } );
		for ( size_t k = 0; k < byOrder.size(); k++ ) {
			const ShapeT & s = *byOrder[k];
#ifdef POOL_RED_OFF1
			const WwBlPool::Span * prev = k ? &byOrder[k - 1]->span : nullptr;
#else
			const WwBlPool::Span * prev = nullptr;
#endif
			std::uintptr_t off = 0;
			std::int32_t base = 0;
			WwBlPool::drawArgs( s.span, prev, off, base );
			const std::uint16_t * ix = g.i.data() + off / 2;
			for ( size_t t = 0; t < s.idx.size(); t++ ) {
				checked++;
				const size_t gv = size_t( base ) + ix[t];
				if ( gv * 3 + 2 >= g.v.size() || std::memcmp( g.v.data() + gv * 3, s.verts.data() + size_t( s.idx[t] ) * 3, 12 ) ) {
					bad++;
					break;
				}
			}
			if ( k && byOrder[k - 1]->span.firstVert + byOrder[k - 1]->span.numVerts > s.span.firstVert
				&& byOrder[k - 1]->span.firstVert < s.span.firstVert + s.span.numVerts )
				overlaps++;
		}
	}
	std::printf( "pool: checked %lld indices, bad shapes %lld, overlaps %lld, rebuilds %d (packs %d), cap %u verts %u indices\n",
		checked, bad, overlaps, grows, packs, a.vertCap, a.idxCap );
	if ( bad || overlaps || !checked || !grows ) {
		std::printf( "POOL FAIL\n" );
		return 1;
	}
	std::printf( "POOL PASS\n" );
	return 0;
}

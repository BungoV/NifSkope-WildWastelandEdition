// BC7 encode of card mask images (lane SKYVIS1127B, bungo 11-27): the repo's own block encoder
// (src/lodgenbc7.h, the one lodgen uses for the card _n sheets) over a job file, all cores.
//   card_mask_bc7 <job.bin> <out.bin>
// job: u32 count, then per image u32 w, u32 h (multiples of 4), w*h*4 bytes RGBA row-major.
// out: the BC7 blocks of every image in job order (row-major blocks, 16 B each).
// Build: g++ -O2 -std=c++17 -I src tools/card_mask_bc7.cpp lib/libfo76utils/src/bptc-tables.c -static
#include "lodgenbc7.h"

#include <cstdio>
#include <thread>
#include <vector>
#include <atomic>

int main( int argc, char ** argv )
{
	if ( argc < 3 ) { std::fprintf( stderr, "usage: card_mask_bc7 job.bin out.bin\n" ); return 2; }
	FILE * f = std::fopen( argv[1], "rb" );
	if ( !f ) return 3;
	std::vector<uint8_t> job;
	uint8_t buf[1 << 16];
	size_t n;
	while ( ( n = std::fread( buf, 1, sizeof buf, f ) ) > 0 ) job.insert( job.end(), buf, buf + n );
	std::fclose( f );
	auto u32 = [&]( size_t o ) { uint32_t v; std::memcpy( &v, job.data() + o, 4 ); return v; };
	struct Img { size_t src, dst; uint32_t w, h; };
	std::vector<Img> imgs;
	size_t o = 4, dst = 0;
	for ( uint32_t i = 0, c = u32( 0 ); i < c; i++ ) {
		Img m{ o + 8, dst, u32( o ), u32( o + 4 ) };
		if ( m.w % 4 || m.h % 4 ) { std::fprintf( stderr, "image %u not a multiple of 4\n", i ); return 4; }
		imgs.push_back( m );
		o += 8 + size_t( m.w ) * m.h * 4;
		dst += size_t( m.w / 4 ) * ( m.h / 4 ) * 16;
	}
	if ( o != job.size() ) { std::fprintf( stderr, "job size %zu, parsed %zu\n", job.size(), o ); return 5; }
	std::vector<uint8_t> out( dst );
	struct Blk { uint32_t img, bx, by; };
	std::vector<Blk> blks;
	for ( uint32_t i = 0; i < imgs.size(); i++ )
		for ( uint32_t by = 0; by < imgs[i].h / 4; by++ )
			for ( uint32_t bx = 0; bx < imgs[i].w / 4; bx++ ) blks.push_back( { i, bx, by } );
	static const int wt[4] = { 1, 1, 1, 1 };
	std::atomic<size_t> next{ 0 };
	auto work = [&]() {
		for ( size_t k; ( k = next.fetch_add( 1 ) ) < blks.size(); ) {
			const Blk & b = blks[k];
			const Img & m = imgs[b.img];
			uint8_t px[16][4];
			for ( int y = 0; y < 4; y++ )
				for ( int x = 0; x < 4; x++ )
					std::memcpy( px[y * 4 + x], job.data() + m.src + ( size_t( b.by * 4 + y ) * m.w + b.bx * 4 + x ) * 4, 4 );
			LodgenBc7::encodeBlock( px, wt, out.data() + m.dst + ( size_t( b.by ) * ( m.w / 4 ) + b.bx ) * 16 );
		}
	};
	unsigned t = std::thread::hardware_concurrency();
	std::vector<std::thread> pool;
	for ( unsigned i = 0; i < ( t ? t : 4 ); i++ ) pool.emplace_back( work );
	for ( auto & th : pool ) th.join();
	FILE * g = std::fopen( argv[2], "wb" );
	if ( !g || std::fwrite( out.data(), 1, out.size(), g ) != out.size() ) return 6;
	std::fclose( g );
	std::printf( "bc7 images %zu blocks %zu bytes %zu\n", imgs.size(), blks.size(), out.size() );
	return 0;
}

// bc7drv -- standalone driver for src/lodgenbc7.h (lane IMPOSTORDEPTH2).
// usage: bc7drv <in.png> <out.dds> <wR> <wG> <wB> <wA> <mips>
// Encodes with the lodgen box-mip chain (alpha kept, round to nearest), writes a
// DX10 BC7_UNORM DDS, then decodes EVERY block with the vendored detex decoder
// and checks that the error the encoder claimed is the error the decoder sees.
#include "lodgenbc7.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
extern "C" {
#include "detex.h"
}
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <chrono>

int main( int argc, char ** argv )
{
	if ( argc < 8 ) { std::fprintf( stderr, "usage\n" ); return 2; }
	int w, h, n;
	unsigned char * img = stbi_load( argv[1], &w, &h, &n, 4 );
	if ( !img ) { std::fprintf( stderr, "load failed\n" ); return 1; }
	const int wt[4] = { atoi( argv[3] ), atoi( argv[4] ), atoi( argv[5] ), atoi( argv[6] ) };
	const int maxMips = atoi( argv[7] );
	std::vector<std::vector<uint8_t>> mips;   // RGBA
	std::vector<int> mw, mh;
	mips.emplace_back( img, img + size_t( w ) * h * 4 );
	mw.push_back( w ); mh.push_back( h );
	while ( mw.back() > 4 && mh.back() > 4 && ( maxMips <= 0 || int( mips.size() ) < maxMips ) ) {
		const int pw = mw.back(), ph = mh.back(), nw = pw / 2, nh = ph / 2;
		const std::vector<uint8_t> & p = mips.back();
		std::vector<uint8_t> nx( size_t( nw ) * nh * 4 );
		for ( int y = 0; y < nh; y++ )
			for ( int x = 0; x < nw; x++ )
				for ( int c = 0; c < 4; c++ ) {
					int acc = 0;
					for ( int sy = 0; sy < 2; sy++ )
						for ( int sx = 0; sx < 2; sx++ )
							acc += p[( size_t( y * 2 + sy ) * pw + x * 2 + sx ) * 4 + c];
					nx[( size_t( y ) * nw + x ) * 4 + c] = uint8_t( ( acc + 2 ) >> 2 );
				}
		mips.push_back( std::move( nx ) );
		mw.push_back( nw ); mh.push_back( nh );
	}
	auto t0 = std::chrono::steady_clock::now();
	std::vector<uint8_t> data;
	long long mismatches = 0, blocks = 0;
	for ( size_t mi = 0; mi < mips.size(); mi++ ) {
		const int W = mw[mi], H = mh[mi], bw = ( W + 3 ) / 4, bh = ( H + 3 ) / 4;
		const size_t at = data.size();
		data.resize( at + size_t( bw ) * bh * 16 );
		const std::vector<uint8_t> & m = mips[mi];
#pragma omp parallel for schedule(dynamic, 1) reduction(+:mismatches, blocks)
		for ( int by = 0; by < bh; by++ )
			for ( int bx = 0; bx < bw; bx++ ) {
				uint8_t px[16][4];
				for ( int i = 0; i < 16; i++ ) {
					const int sx = std::min( bx * 4 + ( i & 3 ), W - 1 ), sy = std::min( by * 4 + ( i >> 2 ), H - 1 );
					for ( int c = 0; c < 4; c++ )
						px[i][c] = m[( size_t( sy ) * W + sx ) * 4 + c];
				}
				uint8_t * o = data.data() + at + ( size_t( by ) * bw + bx ) * 16;
				const int64_t claimed = LodgenBc7::encodeBlock( px, wt, o );
				uint8_t dec[64];
				bool ok = detexDecompressBlockBPTC( o, 0xFF, 0, dec );
				int64_t e = 0;
				for ( int i = 0; i < 16; i++ )
					for ( int c = 0; c < 4; c++ ) {
						const int d = int( dec[i * 4 + c] ) - px[i][c];
						e += int64_t( wt[c] ) * d * d;
					}
				blocks++;
				if ( !ok || e != claimed ) {
					if ( mismatches < 5 )
						std::fprintf( stderr, "mismatch mip %zu block %d,%d mode byte %02x claimed %lld decoded %lld ok %d\n",
							mi, bx, by, o[0], (long long)claimed, (long long)e, int( ok ) );
					mismatches++;
				}
			}
	}
	const double secs = std::chrono::duration<double>( std::chrono::steady_clock::now() - t0 ).count();
	uint32_t hdr[32] = { 0 };
	hdr[0] = 0x20534444; hdr[1] = 124; hdr[2] = 0x000A1007; hdr[3] = h; hdr[4] = w;
	hdr[5] = ( ( w + 3 ) / 4 ) * ( ( h + 3 ) / 4 ) * 16; hdr[7] = uint32_t( mips.size() );
	hdr[19] = 32; hdr[20] = 0x4; hdr[21] = 0x30315844U; hdr[27] = 0x401008;
	const uint32_t dx10[5] = { 98U, 3U, 0U, 1U, 0U };
	FILE * f = std::fopen( argv[2], "wb" );
	std::fwrite( hdr, 4, 32, f ); std::fwrite( dx10, 4, 5, f ); std::fwrite( data.data(), 1, data.size(), f );
	std::fclose( f );
	// mode histogram of the top level
	long long hist[9] = {};
	const size_t top = size_t( ( w + 3 ) / 4 ) * ( ( h + 3 ) / 4 );
	for ( size_t b = 0; b < top; b++ ) {
		const uint8_t m0 = data[b * 16];
		int mode = 8;
		for ( int k = 0; k < 8; k++ ) if ( m0 & ( 1 << k ) ) { mode = k; break; }
		hist[mode]++;
	}
	std::printf( "encoded %lld blocks in %.2f s, %lld decode mismatches; modes top level:", blocks, secs, mismatches );
	for ( int k = 0; k < 8; k++ ) std::printf( " m%d=%lld", k, hist[k] );
	std::printf( "\n" );
	return mismatches ? 1 : 0;
}

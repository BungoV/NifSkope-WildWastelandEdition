// FARSKYMAP1 G2: skymap_kernel.hlsl (the far sky map's shader math) on a headless Vulkan device against the C++ twin
// (src/features/far-lod/farskymap.h). The Vulkan scaffolding is tests/hlslmatch/cullmatch.cpp's (its main renamed).
//   skymapmatch <kernel.spv>
// Probes off (farSkyOn 0): every case's result must equal the per-vertex byte BIT FOR BIT (identity). Probes on: the
// direction and the combine within 4e-6 of the twin (the GPU may fuse a multiply-add). Exit 0 pass, 1 fail, 3 setup.
#define main cullmatch_main
#include "../../../hlslmatch/cullmatch.cpp"
#undef main
#include "../../../../src/features/far-lod/farskymap.h"

#include <random>

int main( int argc, char ** argv )
{
	if ( argc < 2 )
		die( "usage: skymapmatch <kernel.spv>" );
	initVk();
	std::vector<uint32_t> spv = loadSpv( argv[1] );
	const uint32_t N = 4096;
	std::mt19937 rng( 77 );
	std::uniform_real_distribution<float> U( 0.0f, 1.0f ), S( -1.0f, 1.0f ), P( -400000.0f, 400000.0f );
	std::vector<float> in( size_t( N ) * 24, 0.0f );
	for ( uint32_t i = 0; i < N; i++ ) {
		float * c = &in[size_t( i ) * 24];
		float valid = ( i % 7 == 0 ) ? 0.0f : ( i % 5 == 0 ? U( rng ) : 1.0f );
		for ( int k = 0; k < 4; k++ ) c[k] = U( rng ) * valid;	// layer 0, premultiplied
		c[4] = U( rng ) * valid; c[5] = U( rng ) * valid; c[6] = valid; c[7] = 0;
		float n[3] = { S( rng ), S( rng ), S( rng ) };
		if ( i % 11 == 0 ) { n[0] = 0; n[1] = 0; n[2] = ( i & 1 ) ? 1.0f : -1.0f; }
		float l = std::sqrt( n[0] * n[0] + n[1] * n[1] + n[2] * n[2] ) + 1e-12f;
		for ( int k = 0; k < 3; k++ ) c[8 + k] = n[k] / l;
		c[11] = 1.0f - ( 1.0f - float( i & 255 ) / 255.0f ) * U( rng ); // the per-vertex byte's visibility
		c[12] = P( rng ); c[13] = P( rng ); c[14] = U( rng ) * 9000.0f;
		c[15] = float( i % 5 ); // the shape: 0 near, 1..3 a far draw's baked type, 4 a far placement
	}
	float rect[4];
	FarSkyMap::Map m; m.x0 = -96; m.y0 = -96; m.w = 192; m.h = 192;
	FarSkyMap::rect( m, 1234.5f, -987.25f, rect );
	int bad = 0, offBad = 0, onBad = 0, nearBad = 0;
	double worst = 0;
	for ( int on = 0; on < 2; on++ ) {
		Buf io = ssbo( in.size() * 4 );
		std::memcpy( io.p, in.data(), in.size() * 4 );
		Buf ub = makeBuf( 64, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT );	// farSkyOn, farSkyRect, farInst, farVcAo
		uint32_t onU = uint32_t( on );
		std::memset( ub.p, 0, 64 );
		std::memcpy( ub.p, &onU, 4 );
		std::memcpy( (char *)ub.p + 16, rect, 16 );
		run( spv, { { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, ub.b, VK_NULL_HANDLE }, { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, io.b, VK_NULL_HANDLE } },
			nullptr, 0, N / 64, 1 );
		const float * o = (const float *)io.p;
		for ( uint32_t i = 0; i < N; i++ ) {
			const float * c = &in[size_t( i ) * 24];
			const float * g = &o[size_t( i ) * 24 + 16];
			float t[8] = { c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7] }, va = 0;
			float v = FarSkyMap::evalTexel( t, c + 8, &va );
			const bool isFar = FarSkyMap::shapeFar( c[15] > 3.5f, c[15] > 3.5f ? 0.0f : c[15] );	// (far: a Windows macro)
			float comb = FarSkyMap::combine( c[11], on != 0 && isFar, v, va );
			float u = ( c[12] - rect[0] ) * rect[2], w = ( c[13] - rect[1] ) * rect[3];
			// near shapes with the map on: the byte bit for bit, and the shape test must agree with the twin's
			if ( ( g[5] > 0.5f ) != isFar || ( on && c[15] < 0.5f && std::memcmp( &g[2], &c[11], 4 ) ) ) {
				nearBad++;
				if ( nearBad <= 5 ) printf( "near case %u: shape %g far %d, gpu %.9g vs byte %.9g\n", i, c[15], int( g[5] > 0.5f ), g[2], c[11] );
			}
			if ( !on ) {
				uint32_t x, y; std::memcpy( &x, &g[2], 4 ); std::memcpy( &y, &c[11], 4 );
				if ( x != y ) { offBad++; if ( offBad <= 5 ) printf( "off case %u: %.9g vs byte %.9g\n", i, g[2], c[11] ); }
				continue;
			}
			double e = std::max( { std::fabs( double( g[0] ) - v ), std::fabs( double( g[1] ) - va ), std::fabs( double( g[2] ) - comb ),
				std::fabs( double( g[3] ) - u ), std::fabs( double( g[4] ) - w ) } );
			worst = std::max( worst, e );
			if ( e > 4e-6 ) { onBad++; if ( onBad <= 5 ) printf( "on case %u: gpu %.7g %.7g %.7g vs %.7g %.7g %.7g\n", i, g[0], g[1], g[2], v, va, comb ); }
		}
	}
	bad = offBad + onBad + nearBad;
	printf( "skymapmatch: %s; probes off identity %d of %u differ (0 needed); probes on %d of %u past 4e-6, worst %.3g; near shapes %d bad (0 needed)\n",
		bad ? "FAIL" : "PASS", offBad, N, onBad, N, worst, nearBad );
	return bad ? 1 : 0;
}

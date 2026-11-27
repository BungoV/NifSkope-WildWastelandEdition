/* Card sky visibility, the ray half (lane SKYVIS1127, bungo 11-27: "rebake then the gsaos with sky
 * visibility, for all of our trees"). Plain C++, no Qt: it builds with g++ beside the tree's own
 * headers, so the gate and its red need no NifSkope build.
 *
 *   g++ -O2 -std=c++17 -I src tools/card_skyvis_rays.cpp src/impostoroct.cpp -o card_skyvis_rays.exe
 *   card_skyvis_rays <job.bin> <out.bin> <rays> [--red const1]
 *
 * One job = one card layer at mip 0 (tools/card_skyvis.py writes it): the frame layout and camera of
 * docs/LODGEN_IMPOSTOR_SPEC.md ("Frames", "The camera for a frame"), the layer's coverage (`_d` A),
 * its packed normal and height (`_n` R G B), and the source model's triangle soup with its alpha-test
 * masks (the shapes the bake photographed).
 *
 * Per covered texel (`_d` A >= the layer's coverage test):
 *  1. the SURFACE POINT: an orthographic ray from the texel's centre on the frame's picture plane,
 *     along the frame's view direction, against the soup with its alpha holes. Where it misses (a
 *     texel covered only in part of its 4 x 4 bake samples) the point comes from the baked height,
 *     units = (h - 0.5) x depthSpan away from the camera. Both are counted, and on the hits the
 *     baked height is compared with the hit's depth: the check that the frame geometry is right.
 *  2. SKY VISIBILITY: `rays` directions, cosine-weighted about the baked normal (rotated out of the
 *     frame's view space), stratified with a per-texel rotation. A direction counts when it points
 *     into the sky dome (model +Z > 0) and leaves the soup without a hit. Value = counted / rays,
 *     so an open texel facing straight up reads 1, an open vertical one 0.5, one facing down 0.
 * Output: one byte per texel (0..255, 0 where uncovered) and one state byte (0 uncovered, 1 point
 * from the primary ray, 2 from the baked height), then the surface point (3 floats, model space).
 */
#include "probebvh.h"
#include "impostoroct.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>

namespace {

struct Reader
{
	std::vector<unsigned char> b;
	size_t o = 0;
	bool ok = true;
	template<class T> T get()
	{
		T v{};
		if ( o + sizeof( T ) > b.size() ) { ok = false; return v; }
		std::memcpy( &v, &b[o], sizeof( T ) );
		o += sizeof( T );
		return v;
	}
	void bytes( void * dst, size_t n )
	{
		if ( o + n > b.size() ) { ok = false; return; }
		std::memcpy( dst, &b[o], n );
		o += n;
	}
};

uint32_t hash32( uint32_t x )
{
	x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
	return x;
}

} // namespace

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	if ( argc < 4 ) {
		std::fprintf( stderr, "usage: card_skyvis_rays <job.bin> <out.bin> <rays> [--red const1]\n" );
		return 2;
	}
	const int rays = std::atoi( argv[3] );
	const bool redConst = argc > 5 && std::string( argv[4] ) == "--red" && std::string( argv[5] ) == "const1";
	Reader r;
	{
		FILE * f = std::fopen( argv[1], "rb" );
		if ( !f ) { std::fprintf( stderr, "cannot open %s\n", argv[1] ); return 2; }
		std::fseek( f, 0, SEEK_END );
		r.b.resize( size_t( std::ftell( f ) ) );
		std::fseek( f, 0, SEEK_SET );
		if ( std::fread( r.b.data(), 1, r.b.size(), f ) != r.b.size() ) { std::fclose( f ); return 2; }
		std::fclose( f );
	}
	if ( r.get<uint32_t>() != 0x31564B53U ) { std::fprintf( stderr, "not a SKV1 job\n" ); return 2; }
	const int W = r.get<int32_t>(), H = r.get<int32_t>();
	const int cols = r.get<int32_t>(), rows = r.get<int32_t>();
	const int oct = r.get<int32_t>(), views = r.get<int32_t>(), conv = r.get<int32_t>();
	float half[2], centre[3];
	half[0] = r.get<float>(); half[1] = r.get<float>();
	for ( float & c : centre ) c = r.get<float>();
	const float depthSpan = r.get<float>();
	std::vector<float> off( size_t( 2 * cols * rows ) );
	for ( float & v : off ) v = r.get<float>();
	const int covTest = r.get<int32_t>();
	std::vector<unsigned char> cov( size_t( W ) * H ), nrm( size_t( W ) * H * 3 );
	r.bytes( cov.data(), cov.size() );
	r.bytes( nrm.data(), nrm.size() );
	probebvh::Bvh bvh;
	probebvh::AlphaMask mask;
	const int ntri = r.get<int32_t>();
	bvh.t.resize( size_t( ntri ) * 9 );
	mask.triOf.assign( size_t( ntri ), -1 );
	for ( int i = 0; i < ntri; i++ ) {
		for ( int k = 0; k < 9; k++ ) bvh.t[size_t( i ) * 9 + k] = r.get<float>();
		probebvh::AlphaMask::Tri t;
		t.map = r.get<int32_t>();
		t.thr = (unsigned char)r.get<int32_t>();
		for ( float & u : t.uv ) u = r.get<float>();
		if ( t.map >= 0 ) {
			mask.triOf[size_t( i )] = int( mask.tris.size() );
			mask.tris.push_back( t );
		}
	}
	const int nmap = r.get<int32_t>();
	for ( int m = 0; m < nmap; m++ ) {
		probebvh::AlphaMask::Map mp;
		mp.w = r.get<int32_t>(); mp.h = r.get<int32_t>();
		mp.a.resize( size_t( mp.w ) * mp.h );
		r.bytes( mp.a.data(), mp.a.size() );
		mask.maps.push_back( std::move( mp ) );
	}
	if ( !r.ok || W <= 0 || H <= 0 || cols <= 0 || rows <= 0 ) { std::fprintf( stderr, "job truncated\n" ); return 2; }
	if ( mask.tris.empty() ) mask.triOf.clear();
	bvh.mask = &mask;
	bvh.build();

	// the frame cameras
	ImpostorOct::g_convention = conv ? ImpostorOct::Convention::SpecLiteral : ImpostorOct::Convention::AsBaked;
	struct Cam { float right[3], up[3], fwd[3]; int x0, y0, fw, fh; float offR, offU; };
	std::vector<Cam> cams( size_t( cols * rows ) );
	for ( int j = 0; j < rows; j++ )
		for ( int i = 0; i < cols; i++ ) {
			Cam & c = cams[size_t( j * cols + i )];
			float dir[3];
			if ( oct > 0 ) {
				ImpostorOct::frameDir( i, j, oct, dir );
				ImpostorOct::frameBasis( dir, c.right, c.up, c.fwd );
			} else {
				const float p = 6.28318530718f * float( i ) / float( views );
				dir[0] = std::cos( p ); dir[1] = std::sin( p ); dir[2] = 0.0f;
				const ImpostorOct::Convention s = ImpostorOct::g_convention;
				ImpostorOct::g_convention = ImpostorOct::Convention::SpecLiteral;
				ImpostorOct::frameBasis( dir, c.right, c.up, c.fwd );
				ImpostorOct::g_convention = s;
			}
			c.fw = W / cols; c.fh = H / rows;
			c.x0 = i * c.fw; c.y0 = j * c.fh;
			c.offR = off[size_t( 2 * ( j * cols + i ) )];
			c.offU = off[size_t( 2 * ( j * cols + i ) + 1 )];
		}

	std::vector<unsigned char> vis( size_t( W ) * H, 0 ), state( size_t( W ) * H, 0 );
	std::vector<float> pos( size_t( W ) * H * 3, 0.0f );
	std::atomic<int> next( 0 );
	std::atomic<long long> nHit( 0 ), nFall( 0 ), nCov( 0 );
	std::atomic<long long> dSumQ( 0 ), dWithin( 0 );   // |baked - hit| depth, in 1/16 u; within 1.5 height steps
	const double big = 4.0 * ( depthSpan + half[0] + half[1] );
	const double step = depthSpan / 255.0;
	const int strata = std::max( 1, int( std::sqrt( double( rays ) ) + 0.5 ) );
	auto work = [&]() {
		for ( ;; ) {
			const int y = next++;
			if ( y >= H ) break;
			for ( int x = 0; x < W; x++ ) {
				const size_t p = size_t( y ) * W + x;
				if ( cov[p] < covTest ) continue;
				nCov++;
				const Cam & c = cams[size_t( ( y / ( H / rows ) ) * cols + x / ( W / cols ) )];
				const double sx = ( ( x - c.x0 + 0.5 ) / c.fw * 2.0 - 1.0 ) * half[0] + c.offR;
				const double sy = ( 1.0 - ( y - c.y0 + 0.5 ) / c.fh * 2.0 ) * half[1] + c.offU;
				double plane[3], o[3], d[3];
				for ( int k = 0; k < 3; k++ ) {
					plane[k] = centre[k] + c.right[k] * sx + c.up[k] * sy;
					o[k] = plane[k] + c.fwd[k] * big;
					d[k] = -c.fwd[k];
				}
				const double hb = ( nrm[p * 3 + 2] / 255.0 - 0.5 ) * depthSpan;   // baked, away from the camera
				double t = 0.0, P[3];
				double bias;
				// the hit stands where it agrees with the baked height (within 1.5 height steps); elsewhere the
				// card shows a surface the soup does not reproduce at this texel, so the baked height places it
				bool agree = false;
				if ( bvh.ray( o, d, 2.0 * big, &t ) ) {
					const double e = std::fabs( t - big - hb );
					nHit++;
					dSumQ += (long long)( e * 16.0 + 0.5 );
					agree = e <= 1.5 * step;
				}
				if ( agree ) {
					for ( int k = 0; k < 3; k++ ) P[k] = o[k] + d[k] * t;
					dWithin++;
					state[p] = 1;
					bias = 0.5;
				} else {
					for ( int k = 0; k < 3; k++ ) P[k] = plane[k] + d[k] * hb;
					nFall++;
					state[p] = 2;
					bias = step;
				}
				for ( int k = 0; k < 3; k++ ) pos[p * 3 + k] = float( P[k] );
				// the baked normal, out of the frame's view space
				float n[3];
				ImpostorOct::unpackNormalToModel( nrm[p * 3] / 255.0f, nrm[p * 3 + 1] / 255.0f, c.right, c.up, c.fwd, n );
				if ( redConst ) { vis[p] = 255; continue; }
				// tangent frame about n
				double a[3] = { std::fabs( n[2] ) < 0.9 ? 0.0 : 1.0, 0.0, std::fabs( n[2] ) < 0.9 ? 1.0 : 0.0 };
				double tx[3] = { a[1] * n[2] - a[2] * n[1], a[2] * n[0] - a[0] * n[2], a[0] * n[1] - a[1] * n[0] };
				const double tl = std::sqrt( tx[0] * tx[0] + tx[1] * tx[1] + tx[2] * tx[2] );
				for ( double & v : tx ) v /= tl;
				const double ty[3] = { n[1] * tx[2] - n[2] * tx[1], n[2] * tx[0] - n[0] * tx[2], n[0] * tx[1] - n[1] * tx[0] };
				const uint32_t h0 = hash32( uint32_t( p ) * 2654435761U + 0x9e3779b9U );
				const double ju = ( h0 & 0xFFFF ) / 65536.0, jv = ( h0 >> 16 ) / 65536.0;
				double org[3];
				for ( int k = 0; k < 3; k++ ) org[k] = P[k] + n[k] * bias;
				int sky = 0;
				for ( int s = 0; s < rays; s++ ) {
					double u1 = ( ( s % strata ) + 0.5 ) / strata + ju, u2 = ( ( s / strata ) % strata + 0.5 ) / strata + jv;
					u1 -= std::floor( u1 ); u2 -= std::floor( u2 );
					const double rr = std::sqrt( u1 ), ph = 6.283185307179586 * u2;
					const double lx = rr * std::cos( ph ), ly = rr * std::sin( ph ), lz = std::sqrt( std::max( 0.0, 1.0 - u1 ) );
					double dd[3];
					for ( int k = 0; k < 3; k++ ) dd[k] = tx[k] * lx + ty[k] * ly + n[k] * lz;
					if ( dd[2] <= 0.0 ) continue;   // below the horizon: the ground, not the sky
					if ( !bvh.anyHitIn( org, dd, 1e-4, 1e30, []( int ) { return true; } ) ) sky++;
				}
				vis[p] = (unsigned char)std::min( 255, int( 255.0 * sky / rays + 0.5 ) );
			}
		}
	};
	unsigned nt = std::max( 1u, std::thread::hardware_concurrency() );
	// the plane-mapping check: the primary ray WITHOUT the alpha holes hits nearly every covered texel when the
	// frame layout, camera and soup are right (2048x2048 TreeHero01: 97 %); a wrong soup or frame reads far lower
	std::atomic<long long> nSolid( 0 );
	{
		bvh.mask = nullptr;
		std::atomic<int> row( 0 );
		auto solid = [&]() {
			for ( int y; ( y = row++ ) < H; )
				for ( int x = 0; x < W; x++ ) {
					const size_t p = size_t( y ) * W + x;
					if ( cov[p] < covTest )
						continue;
					const Cam & c = cams[size_t( ( y / ( H / rows ) ) * cols + x / ( W / cols ) )];
					const double sx = ( ( x - c.x0 + 0.5 ) / c.fw * 2.0 - 1.0 ) * half[0] + c.offR;
					const double sy = ( 1.0 - ( y - c.y0 + 0.5 ) / c.fh * 2.0 ) * half[1] + c.offU;
					double o[3], d[3], t;
					for ( int k = 0; k < 3; k++ ) {
						o[k] = centre[k] + c.right[k] * sx + c.up[k] * sy + c.fwd[k] * big;
						d[k] = -c.fwd[k];
					}
					if ( bvh.ray( o, d, 2.0 * big, &t ) )
						nSolid++;
				}
		};
		std::vector<std::thread> ts;
		for ( unsigned i = 0; i < nt; i++ )
			ts.emplace_back( solid );
		for ( auto & x : ts )
			x.join();
		bvh.mask = &mask;
	}
	const auto t0 = std::chrono::steady_clock::now();
	std::vector<std::thread> th;
	for ( unsigned i = 0; i < nt; i++ ) th.emplace_back( work );
	for ( auto & x : th ) x.join();
	const double ms = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t0 ).count();
	FILE * f = std::fopen( argv[2], "wb" );
	if ( !f ) return 2;
	std::fwrite( vis.data(), 1, vis.size(), f );
	std::fwrite( state.data(), 1, state.size(), f );
	std::fwrite( pos.data(), sizeof( float ), pos.size(), f );
	std::fclose( f );
	const long long h = nHit.load();
	std::printf( "SKV tris %d masked %zu maps %d covered %lld solidHit %.1f %% hit %lld (|depth - baked| mean %.2f u) placedByHit %lld placedByBakedHeight %lld (step %.2f u) rays %d ms %.0f threads %u%s\n",
		ntri, mask.tris.size(), nmap, nCov.load(), nCov.load() ? 100.0 * double( nSolid.load() ) / double( nCov.load() ) : 0.0, h,
		h ? dSumQ.load() / 16.0 / double( h ) : 0.0, dWithin.load(), nFall.load(), step, rays, ms, nt, redConst ? " RED-CONST1" : "" );
	return 0;
}

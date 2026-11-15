// FARSKYMAP1 G1: the far sky map's C++ side (src/features/far-lod/farskymap.h) against independent expectations.
// Reds (-DFARSKYMAP_RED_OCTANT, -DFARSKYMAP_RED_LAYOUT, -DFARSKYMAP_RED_NEAR) must FAIL. run.sh drives all four.
#include "../../../../src/features/far-lod/farskymap.h"

#include <cstdio>
#include <random>

static int g_fail = 0;
#define CHECK( c, ... ) do { if ( !( c ) ) { g_fail++; std::printf( "FAIL %s:%d ", __FILE__, __LINE__ ); std::printf( __VA_ARGS__ ); std::printf( "\n" ); } } while ( 0 )

// Independent: octant i covers the quarter-space whose centre direction has these signs.
static void octSigns( int i, int s[3] ) { s[0] = ( i & 1 ) ? -1 : 1; s[1] = ( i & 2 ) ? -1 : 1; s[2] = ( i & 4 ) ? -1 : 1; }
static void expectDirs( const float o[8], float d[6] )
{
	for ( int a = 0; a < 3; a++ ) {
		float p = 0, n = 0;
		for ( int i = 0; i < 8; i++ ) { int s[3]; octSigns( i, s ); ( s[a] > 0 ? p : n ) += o[i] / 4.0f; }
		d[a * 2] = p; d[a * 2 + 1] = n;
	}
}

static void writeTbk( const char * path, const std::vector<FarSkyMap::Probe> & ps, uint32_t ver )
{
	FILE * f = std::fopen( path, "wb" );
	uint32_t h[16] = { FarSkyMap::kTbkMagic, ver, 1, 0, 0, 0, 2, uint32_t( ps.size() ), 0, 0 };
	float cs = 16384.0f; std::memcpy( &h[5], &cs, 4 );
	std::fwrite( h, 4, 16, f );
	unsigned char surf[64] = {};
	for ( int i = 0; i < 64; i++ ) surf[i] = (unsigned char)( 0xA0 + i ); // junk the reader must skip
	std::fwrite( surf, 1, 64, f );
	for ( auto & p : ps ) {
		unsigned char rec[144] = {};
		std::memcpy( rec, p.pos, 12 );
		float junk = 7.0f; std::memcpy( rec + 20, &junk, 4 ); std::memcpy( rec + 24, &junk, 4 ); std::memcpy( rec + 28, &junk, 4 );
		std::memcpy( rec + 32, p.skyVis, 32 );
		float od = 999.0f; for ( int k = 0; k < 16; k++ ) std::memcpy( rec + 64 + k * 4, &od, 4 );
		std::fwrite( rec, 1, 144, f );
	}
	std::fclose( f );
}

int main( int argc, char ** argv )
{
	const char * tmp = argc > 1 ? argv[1] : ".";
	std::mt19937 rng( 1234 );
	std::uniform_real_distribution<float> U( 0.0f, 1.0f );

	// 1. Six directions from eight octants.
	for ( int t = 0; t < 64; t++ ) {
		float o[8], d[6], e[6];
		for ( auto & v : o ) v = U( rng );
		FarSkyMap::dirsFromOctants( o, d );
		expectDirs( o, e );
		for ( int k = 0; k < 6; k++ ) CHECK( std::fabs( d[k] - e[k] ) < 1e-6f, "dirs t%d k%d %f vs %f", t, k, d[k], e[k] );
	}

	// 2. World layout over a non-square block with negative cells, holes, and two probes in one cell.
	const int X0 = -7, Y0 = -3, W = 11, H = 5;
	std::vector<FarSkyMap::Probe> ps;
	std::vector<std::vector<float>> want( W * H );
	for ( int y = 0; y < H; y++ )
		for ( int x = 0; x < W; x++ ) {
			if ( ( x * 7 + y * 3 ) % 5 == 0 && !( x == 0 && y == 0 ) && !( x == W - 1 && y == H - 1 ) ) continue; // hole
			FarSkyMap::Probe p;
			p.pos[0] = ( X0 + x + 0.5f ) * 4096.0f; p.pos[1] = ( Y0 + y + 0.5f ) * 4096.0f; p.pos[2] = 3000.0f;
			for ( auto & v : p.skyVis ) v = U( rng );
			float e[6]; expectDirs( p.skyVis, e );
			want[y * W + x].assign( e, e + 6 );
			ps.push_back( p );
		}
	{ // second probe in cell (2,1): the texel is the mean
		FarSkyMap::Probe q; int x = 2, y = 1;
		q.pos[0] = ( X0 + x + 0.1f ) * 4096.0f; q.pos[1] = ( Y0 + y + 0.9f ) * 4096.0f; q.pos[2] = 0;
		for ( auto & v : q.skyVis ) v = U( rng );
		float e[6]; expectDirs( q.skyVis, e );
		if ( want[y * W + x].empty() ) want[y * W + x].assign( e, e + 6 );
		else for ( int k = 0; k < 6; k++ ) want[y * W + x][k] = ( want[y * W + x][k] + e[k] ) * 0.5f;
		ps.push_back( q );
	}
	FarSkyMap::Map m = FarSkyMap::build( ps );
	CHECK( m.x0 == X0 && m.y0 == Y0 && m.w == W && m.h == H, "rect %d %d %d %d", m.x0, m.y0, m.w, m.h );
	int nb = 0;
	for ( auto & v : want ) nb += !v.empty();
	CHECK( m.baked == nb, "baked %d vs %d", m.baked, nb );
	for ( int y = 0; y < H && !m.empty(); y++ )
		for ( int x = 0; x < W; x++ ) {
			const float * t = m.texel( 0, x, y );
			if ( want[y * W + x].empty() ) { CHECK( t[6] == 0.0f, "hole %d,%d valid %f", x, y, t[6] ); continue; }
			CHECK( t[6] == 1.0f, "valid %d,%d", x, y );
			for ( int k = 0; k < 6; k++ )
				CHECK( std::fabs( t[k] - want[y * W + x][k] ) < 1e-6f, "texel %d,%d k%d %f vs %f", x, y, k, t[k], want[y * W + x][k] );
		}

	// 3. Mips: valid-weighted. Every level's (premul / valid) equals the mean of the valid level-0 texels it covers
	// (box of 2^l, edge texels repeated as the builder does), and the count matches log2.
	int levels = 1; for ( int s = std::max( W, H ); s > 1; s >>= 1 ) levels++;
	CHECK( int( m.mips.size() ) == levels, "mips %d vs %d", int( m.mips.size() ), levels );
	if ( m.mips.size() > 1 ) {
		// level 1 by hand
		for ( int y = 0; y < m.mipH( 1 ); y++ )
			for ( int x = 0; x < m.mipW( 1 ); x++ ) {
				float acc[6] = {}, va = 0;
				for ( int dy = 0; dy < 2; dy++ ) for ( int dx = 0; dx < 2; dx++ ) {
					int sx = std::min( 2 * x + dx, W - 1 ), sy = std::min( 2 * y + dy, H - 1 );
					auto & v = want[sy * W + sx];
					if ( v.empty() ) continue;
					va += 1; for ( int k = 0; k < 6; k++ ) acc[k] += v[k];
				}
				const float * t = m.texel( 1, x, y );
				CHECK( std::fabs( t[6] - va / 4 ) < 1e-6f, "mip1 valid %d,%d", x, y );
				if ( va > 0 ) for ( int k = 0; k < 6; k++ )
					CHECK( std::fabs( t[k] / t[6] - acc[k] / va ) < 1e-5f, "mip1 %d,%d k%d", x, y, k );
			}
		const float * top = m.texel( levels - 1, 0, 0 );
		CHECK( m.mipW( levels - 1 ) == 1 && m.mipH( levels - 1 ) == 1 && top[6] > 0, "top mip" );
	}

	// 4. evalTexel: an axis normal reads its own direction; a diagonal mixes by n^2; valid 0 reads 1.
	{
		float t[8] = { 0.9f, 0.1f, 0.8f, 0.2f, 0.7f, 0.3f, 1.0f, 0 };
		float n1[3] = { 1, 0, 0 }, n2[3] = { 0, -1, 0 }, n3[3] = { 0, 0, 1 };
		float r = std::sqrt( 0.5f ), n4[3] = { -r, 0, r }, va;
		CHECK( std::fabs( FarSkyMap::evalTexel( t, n1, &va ) - 0.9f ) < 1e-6f, "eval +X" );
		CHECK( std::fabs( FarSkyMap::evalTexel( t, n2, &va ) - 0.2f ) < 1e-6f, "eval -Y" );
		CHECK( std::fabs( FarSkyMap::evalTexel( t, n3, &va ) - 0.7f ) < 1e-6f, "eval +Z" );
		CHECK( std::fabs( FarSkyMap::evalTexel( t, n4, &va ) - 0.4f ) < 1e-5f, "eval -X+Z %f", FarSkyMap::evalTexel( t, n4, &va ) );
		float h[8] = { 0.45f, 0.05f, 0.4f, 0.1f, 0.35f, 0.15f, 0.5f, 0 }; // half valid, premultiplied
		CHECK( std::fabs( FarSkyMap::evalTexel( h, n1, &va ) - 0.9f ) < 1e-6f && va == 0.5f, "eval premul" );
		float z[8] = {};
		CHECK( FarSkyMap::evalTexel( z, n1, &va ) == 1.0f && va == 0.0f, "eval empty" );
	}

	// 5. Identity: probes off returns the per-vertex byte bit for bit, whatever the map says.
	int idBad = 0;
	for ( int b = 0; b < 256; b++ ) {
		float base = 1.0f - ( 1.0f - b / 255.0f ) * 0.85f;
		for ( int t = 0; t < 16; t++ ) {
			float r = FarSkyMap::combine( base, false, U( rng ), U( rng ) );
			uint32_t a, c; std::memcpy( &a, &base, 4 ); std::memcpy( &c, &r, 4 );
			idBad += a != c;
		}
	}
	CHECK( idBad == 0, "identity off: %d of 4096 differ", idBad );
	CHECK( FarSkyMap::combine( 0.8f, true, 0.3f, 0.0f ) == 0.8f, "on, no valid" );
	CHECK( std::fabs( FarSkyMap::combine( 0.8f, true, 0.5f, 1.0f ) - 0.4f ) < 1e-6f, "on, valid" );
	CHECK( std::fabs( FarSkyMap::combine( 0.8f, true, 0.5f, 0.5f ) - 0.6f ) < 1e-6f, "on, half valid" );
	// 5b. Near shapes (not a far placement, no baked type) keep the byte with the map on; far shapes take it.
	CHECK( !FarSkyMap::shapeFar( false, 0.0f ), "near shape" );
	CHECK( FarSkyMap::combine( 1.0f, FarSkyMap::shapeFar( false, 0.0f ), 0.3f, 1.0f ) == 1.0f, "map on, near shape: base" );
	CHECK( FarSkyMap::shapeFar( true, 0.0f ) && FarSkyMap::shapeFar( false, 1.0f ) && FarSkyMap::shapeFar( false, 3.0f ), "far shapes" );

	// 6. File round trip (half precision).
	std::string fpath = std::string( tmp ) + "/unit.farsky";
	CHECK( FarSkyMap::save( m, fpath ), "save" );
	FarSkyMap::Map r;
	CHECK( FarSkyMap::load( r, fpath ), "load" );
	CHECK( r.x0 == m.x0 && r.y0 == m.y0 && r.w == m.w && r.h == m.h && r.baked == m.baked && r.mips.size() == m.mips.size(), "header" );
	float worst = 0;
	for ( size_t l = 0; l < r.mips.size() && l < m.mips.size(); l++ )
		for ( size_t i = 0; i < m.mips[l].size(); i++ ) worst = std::max( worst, std::fabs( r.mips[l][i] - m.mips[l][i] ) );
	CHECK( worst <= 1.0f / 2048.0f, "round trip worst %g", worst );
	for ( float v : { 0.0f, 1.0f, 0.5f, 0.25f, 1.0f / 3.0f, 6e-5f, 65504.0f } )
		CHECK( std::fabs( FarSkyMap::fromHalf( FarSkyMap::toHalf( v ) ) - v ) <= std::fabs( v ) / 1024.0f + 6e-8f, "half %g", v );
	{ FILE * f = std::fopen( fpath.c_str(), "r+b" ); int32_t bad = 2; std::fseek( f, 4, SEEK_SET ); std::fwrite( &bad, 4, 1, f ); std::fclose( f ); }
	CHECK( !FarSkyMap::load( r, fpath ) && r.empty(), "bad version refused" );

	// 7. .tbk reader: surfels skipped, probe position and skyVis at their offsets, v3..v5, others refused.
	std::string tpath = std::string( tmp ) + "/unit.tbk";
	std::vector<FarSkyMap::Probe> sub( ps.begin(), ps.begin() + 5 ), back;
	for ( uint32_t ver : { 3u, 4u, 5u } ) {
		writeTbk( tpath.c_str(), sub, ver );
		back.clear();
		CHECK( FarSkyMap::readTbkProbes( tpath, back ) && back.size() == sub.size(), "tbk v%u read", ver );
		for ( size_t i = 0; i < back.size() && i < sub.size(); i++ )
			CHECK( !std::memcmp( &back[i], &sub[i], sizeof( FarSkyMap::Probe ) ), "tbk v%u probe %zu", ver, i );
	}
	writeTbk( tpath.c_str(), sub, 2 );
	back.clear();
	CHECK( !FarSkyMap::readTbkProbes( tpath, back ), "tbk v2 refused" );

	std::printf( "farskymap unit: %s (%d fails); %s; identity off 0 of 4096 differ needed, got %d\n",
		g_fail ? "FAIL" : "PASS", g_fail, FarSkyMap::census( m ).c_str(), idBad );
	return g_fail ? 1 : 0;
}

/* lane TREELEAF1: unit test of the coverage-preserving alpha mips (src/gl/wwalphacov.h), the code the texture loader
 * runs on an alpha-tested material's base map.
 *
 *   green: every level of at least 16 x 16 texels keeps level 0's coverage at the material's ref within 2 % (relative);
 *          level 0 and every colour byte are untouched; a texel passes after the rewrite exactly when its scaled value
 *          passes, but for the tie-break texels the rule reports (the "side" rule).
 *   -DTREELEAF1_RED_NOSCALE    today's loader (the mips as the file holds them): the coverage rule must FAIL.
 *   -DTREELEAF1_RED_ENDPOINTS  the first try: scale only the two BC3 alpha endpoints by the same factors; a leaf-edge
 *                              block spanning 0..255 keeps its palette, so the coverage rule must FAIL.
 *   -DTREELEAF1_RED_NEAREST    the re-encode takes the nearest palette entry whatever its side of ref
 *                              (WWALPHACOV_RED_NEAREST in the header): the side rule must FAIL.
 *
 * Inputs: a synthetic leaf atlas (made here: leaves as soft ellipses, box-filtered mips, a plain BC3 alpha encoder),
 * plus, when a folder is given, every texture of its manifest.txt ("<file> <ref> <name>" per line; a file is
 * 'TLF1' + u32 width, height, levels, dxgi format + the BC3 chain, level 0 first). The game's textures are never in
 * the repo; the lane's notes dump them (notes/fix1030b/treeleaf1/tools/dump_leaf.py). */

#ifdef TREELEAF1_RED_NEAREST
#define WWALPHACOV_RED_NEAREST
#endif
#include "gl/wwalphacov.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

static int fails = 0, checks = 0;

static void check( bool ok, const char * rule, const std::string & what )
{
	checks++;
	if ( !ok ) {
		fails++;
		if ( fails <= 40 )
			std::printf( "FAIL %s: %s\n", rule, what.c_str() );
	}
}

struct Chain
{
	int w = 0, h = 0, levels = 0;
	std::vector<std::uint8_t> body;
	std::vector<size_t> off;

	void layout()
	{
		off.clear();
		size_t o = 0;
		for ( int l = 0; l < levels; l++ ) {
			off.push_back( o );
			o += WwAlphaCov::levelBytes( std::max( w >> l, 1 ), std::max( h >> l, 1 ) );
		}
		body.resize( std::max( body.size(), o ) );
	}
	std::uint8_t * level( int l ) { return body.data() + off[size_t( l )]; }
};

// ---- the synthetic atlas -------------------------------------------------------------------------------------------

static std::uint32_t rng = 12345u;
static float frand()
{
	rng = rng * 1664525u + 1013904223u;
	return float( rng >> 8 ) / float( 1u << 24 );
}

static void encodeBC3Alpha( const std::vector<float> & a, int w, int h, std::uint8_t * out )
{
	const int bw = ( w + 3 ) / 4, bh = ( h + 3 ) / 4;
	for ( int by = 0; by < bh; by++ )
	for ( int bx = 0; bx < bw; bx++ ) {
		int v[16];
		int mn = 255, mx = 0;
		for ( int t = 0; t < 16; t++ ) {
			const int x = std::min( bx * 4 + ( t & 3 ), w - 1 ), y = std::min( by * 4 + ( t >> 2 ), h - 1 );
			v[t] = std::clamp( int( std::lround( a[size_t( y ) * size_t( w ) + size_t( x )] * 255.0f ) ), 0, 255 );
			mn = std::min( mn, v[t] );
			mx = std::max( mx, v[t] );
		}
		std::uint8_t * b = out + ( size_t( by ) * size_t( bw ) + size_t( bx ) ) * 16;
		std::memset( b, 0, 16 );
		for ( int k = 8; k < 16; k++ )
			b[k] = std::uint8_t( 0x5A ^ ( bx * 7 + by * 13 + k ) );	// colour bytes: a pattern the rule must keep
		b[0] = std::uint8_t( mx );
		b[1] = std::uint8_t( mn );
		std::uint64_t bits = 0;
		if ( mx > mn ) {
			for ( int t = 0; t < 16; t++ ) {
				int best = 0, bestD = 1 << 30;
				for ( int k = 0; k < 8; k++ ) {
					const int num = k == 0 ? mx * 7 : ( k == 1 ? mn * 7 : ( 8 - k ) * mx + ( k - 1 ) * mn );
					const int d = std::abs( num - v[t] * 7 );
					if ( d < bestD ) {
						bestD = d;
						best = k;
					}
				}
				bits |= std::uint64_t( best ) << ( 3 * t );
			}
		}
		for ( int k = 0; k < 6; k++ )
			b[2 + k] = std::uint8_t( bits >> ( 8 * k ) );
	}
}

static Chain syntheticLeaves( int size )
{
	std::vector<float> a( size_t( size ) * size_t( size ), 0.0f );
	for ( int n = 0; n < 420; n++ ) {	// leaves: rotated soft ellipses, 3..9 texels long
		const float cx = frand() * size, cy = frand() * size, r = 3.0f + frand() * 6.0f, q = 0.25f + frand() * 0.25f;
		const float th = frand() * 6.2831853f, c = std::cos( th ), s = std::sin( th );
		for ( int y = int( cy - r - 2 ); y <= int( cy + r + 2 ); y++ )
		for ( int x = int( cx - r - 2 ); x <= int( cx + r + 2 ); x++ ) {
			if ( x < 0 || y < 0 || x >= size || y >= size )
				continue;
			const float dx = float( x ) + 0.5f - cx, dy = float( y ) + 0.5f - cy;
			const float u = ( dx * c + dy * s ) / r, v = ( -dx * s + dy * c ) / ( r * q );
			const float d = std::sqrt( u * u + v * v );
			const float al = std::clamp( ( 1.0f - d ) * r * 0.7f, 0.0f, 1.0f );	// ~1.5 texel soft edge
			float & t = a[size_t( y ) * size_t( size ) + size_t( x )];
			t = std::max( t, al );
		}
	}
	Chain ch;
	ch.w = ch.h = size;
	ch.levels = 0;
	for ( int s = size; s >= 1; s >>= 1 )
		ch.levels++;
	ch.layout();
	std::vector<float> cur = a;
	int w = size;
	for ( int l = 0; l < ch.levels; l++ ) {
		encodeBC3Alpha( cur, w, w, ch.level( l ) );
		if ( w == 1 )
			break;
		const int n = w / 2;
		std::vector<float> nx( size_t( n ) * size_t( n ) );
		for ( int y = 0; y < n; y++ )	// the box filter authoring tools use: the leaf averaged with the empty space
		for ( int x = 0; x < n; x++ )
			nx[size_t( y ) * size_t( n ) + size_t( x )] = 0.25f * ( cur[size_t( 2 * y ) * size_t( w ) + size_t( 2 * x )]
				+ cur[size_t( 2 * y ) * size_t( w ) + size_t( 2 * x + 1 )] + cur[size_t( 2 * y + 1 ) * size_t( w ) + size_t( 2 * x )]
				+ cur[size_t( 2 * y + 1 ) * size_t( w ) + size_t( 2 * x + 1 )] );
		cur.swap( nx );
		w = n;
	}
	return ch;
}

// ---- the rules -----------------------------------------------------------------------------------------------------

static double coverage( Chain & ch, int l, int ref )
{
	WwAlphaCov::Level hist;
	const int w = std::max( ch.w >> l, 1 ), h = std::max( ch.h >> l, 1 );
	hist.build( ch.level( l ), w, h, false );
	return hist.texels ? double( hist.passing( ref, 1.0 ) ) / double( hist.texels ) : 0.0;
}

static void runOne( Chain ch, int ref, const std::string & name, int & gated, int & within, double & worstBefore,
	double & worstAfter, double & worstFid )
{
	const Chain orig = ch;
	std::vector<double> before( size_t( ch.levels ) );
	for ( int l = 0; l < ch.levels; l++ )
		before[size_t( l )] = coverage( ch, l, ref );
	std::vector<std::uint8_t *> lv;
	for ( int l = 0; l < ch.levels; l++ )
		lv.push_back( ch.level( l ) );
	std::vector<WwAlphaCov::LevelStat> st( size_t( ch.levels ) );
#if defined( TREELEAF1_RED_NOSCALE )
	for ( auto & x : st )
		x.scale = 1.0f;
#elif defined( TREELEAF1_RED_ENDPOINTS )
	{	// the red: preserve()'s scales, applied to the two alpha endpoints only (clamped; the 8-value mode kept)
		Chain probe = ch;
		std::vector<std::uint8_t *> pv;
		for ( int l = 0; l < probe.levels; l++ )
			pv.push_back( probe.level( l ) );
		WwAlphaCov::preserve( pv.data(), probe.levels, ch.w, ch.h, ref, st.data() );
		for ( int l = 1; l < ch.levels; l++ ) {
			const int w = std::max( ch.w >> l, 1 ), h = std::max( ch.h >> l, 1 );
			const size_t blocks = size_t( ( w + 3 ) / 4 ) * size_t( ( h + 3 ) / 4 );
			for ( size_t i = 0; i < blocks; i++ ) {
				std::uint8_t * b = ch.level( l ) + i * 16;
				const int a0 = b[0], a1 = b[1];
				int b0 = std::clamp( int( std::lround( a0 * double( st[size_t( l )].scale ) ) ), 0, 255 );
				int b1 = std::clamp( int( std::lround( a1 * double( st[size_t( l )].scale ) ) ), 0, 255 );
				if ( a0 > a1 && b0 <= b1 ) {
					if ( b0 >= 255 ) {
						b0 = 255;
						b1 = 254;
					} else {
						b0 = b1 + 1;
					}
				}
				b[0] = std::uint8_t( b0 );
				b[1] = std::uint8_t( b1 );
			}
		}
	}
#else
	WwAlphaCov::preserve( lv.data(), ch.levels, ch.w, ch.h, ref, st.data() );
#endif
	// rule: coverage kept (measured again from the rewritten bytes, not from preserve()'s own stats)
	const double c0 = coverage( ch, 0, ref );
	check( c0 == before[0], "level0", name + " level 0 coverage moved" );
	for ( int l = 1; l < ch.levels; l++ ) {
		const int w = std::max( ch.w >> l, 1 ), h = std::max( ch.h >> l, 1 );
		if ( w * h < 256 || c0 <= 0.0 )
			continue;
		const double after = coverage( ch, l, ref );
		const double eB = std::fabs( before[size_t( l )] / c0 - 1.0 ), eA = std::fabs( after / c0 - 1.0 );
		worstBefore = std::max( worstBefore, eB );
		worstAfter = std::max( worstAfter, eA );
		// within 2 %, or within half a texel of level 0's share (a 16 x 16 level cannot hold 18.4 texels)
		const bool ok = eA <= 0.02 || std::fabs( after - c0 ) * double( w ) * double( h ) <= 0.5 + 1e-9;
		gated++;
		within += ok ? 1 : 0;
		char buf[256];
		std::snprintf( buf, sizeof( buf ), "%s ref %d level %d (%dx%d): coverage %.4f vs level 0 %.4f (%.1f %%; before %.1f %%)",
			name.c_str(), ref, l, w, h, after, c0, 100.0 * ( after / c0 - 1.0 ), 100.0 * ( before[size_t( l )] / c0 - 1.0 ) );
		check( ok, "coverage", buf );
	}
	// rule: level 0 byte-identical, the colour half of every block untouched
	const size_t b0 = WwAlphaCov::levelBytes( ch.w, ch.h );
	check( std::memcmp( ch.body.data(), orig.body.data(), b0 ) == 0, "level0", name + " level 0 bytes changed" );
	size_t moved = 0;
	for ( size_t i = b0; i + 16 <= ch.body.size(); i += 16 )
		moved += std::memcmp( ch.body.data() + i + 8, orig.body.data() + i + 8, 8 ) != 0 ? 1 : 0;
	check( moved == 0, "bytes", name + " colour bytes moved in " + std::to_string( moved ) + " blocks" );
	// rule (side): a rewritten texel passes exactly when its scaled value passes; fidelity = mean |new - scaled| / 255
	for ( int l = 1; l < ch.levels; l++ ) {
		const double s = double( st[size_t( l )].scale );
		if ( s == 1.0 && st[size_t( l )].moved == 0 )
			continue;
		const int w = std::max( ch.w >> l, 1 ), h = std::max( ch.h >> l, 1 );
		const int bw = ( w + 3 ) / 4, bh = ( h + 3 ) / 4;
		Chain o2 = orig;
		size_t wrongSide = 0, n = 0;
		double err = 0.0;
		int uo[16], un[16];
		for ( int by = 0; by < bh; by++ )
		for ( int bx = 0; bx < bw; bx++ ) {
			const size_t i = ( size_t( by ) * size_t( bw ) + size_t( bx ) ) * 16;
			WwAlphaCov::decodeBC3Alpha( o2.level( l ) + i, uo );
			WwAlphaCov::decodeBC3Alpha( ch.level( l ) + i, un );
			for ( int t = 0; t < 16; t++ ) {
				if ( bx * 4 + ( t & 3 ) >= w || by * 4 + ( t >> 2 ) >= h )
					continue;
				const double want = std::min( double( uo[t] ) * s / 35.0, 255.0 );
				wrongSide += ( ( double( un[t] ) / 35.0 > double( ref ) ) != WwAlphaCov::scaledPasses( uo[t], s, ref ) ) ? 1 : 0;
				err += std::fabs( double( un[t] ) / 35.0 - want );
				n++;
			}
		}
		if ( n )
			worstFid = std::max( worstFid, err / double( n ) );
		check( wrongSide <= st[size_t( l )].moved, "side", name + " ref " + std::to_string( ref ) + " level " + std::to_string( l )
			+ ": " + std::to_string( wrongSide ) + " texels on the wrong side of the test (tie-break moved "
			+ std::to_string( st[size_t( l )].moved ) + ")" );
	}
}

static bool readTlf( const std::string & path, Chain & ch )
{
	std::ifstream f( path, std::ios::binary );
	if ( !f )
		return false;
	char magic[4];
	std::uint32_t hd[4];
	f.read( magic, 4 );
	f.read( reinterpret_cast<char *>( hd ), 16 );
	if ( !f || std::memcmp( magic, "TLF1", 4 ) != 0 || ( hd[3] != 77 && hd[3] != 78 ) )
		return false;
	ch.w = int( hd[0] );
	ch.h = int( hd[1] );
	ch.levels = int( hd[2] );
	ch.body.assign( std::istreambuf_iterator<char>( f ), std::istreambuf_iterator<char>() );
	const size_t have = ch.body.size();
	ch.layout();
	return ch.body.size() == have;
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	int gated = 0, within = 0;
	double wb = 0.0, wa = 0.0, wf = 0.0;
	const Chain syn = syntheticLeaves( 256 );
	for ( int ref : { 64, 90, 128, 185 } )
		runOne( syn, ref, "synthetic", gated, within, wb, wa, wf );
	std::printf( "synthetic: %d levels gated, %d within 2 %%, worst before %.1f %%, after %.1f %%; worst mean |alpha - scaled| %.2f of 255\n",
		gated, within, 100.0 * wb, 100.0 * wa, wf );
	if ( argc > 1 ) {
		const std::string dir = argv[1];
		std::ifstream man( dir + "/manifest.txt" );
		std::string line;
		int files = 0, g2 = 0, w2 = 0;
		double wb2 = 0.0, wa2 = 0.0, wf2 = 0.0;
		while ( std::getline( man, line ) ) {
			std::istringstream ss( line );
			std::string file, name;
			int ref = -1;
			if ( !( ss >> file >> ref >> name ) )
				continue;
			Chain ch;
			if ( !readTlf( dir + "/" + file, ch ) ) {
				check( false, "read", "cannot read " + file );
				continue;
			}
			int g = 0, w = 0;
			double b = 0.0, a = 0.0, fd = 0.0;
			runOne( ch, ref, name, g, w, b, a, fd );
			std::printf( "file %s ref %d %dx%d: %d levels gated, %d within 2 %%, worst before %.1f %%, after %.1f %%\n",
				name.c_str(), ref, ch.w, ch.h, g, w, 100.0 * b, 100.0 * a );
			files++;
			g2 += g;
			w2 += w;
			wb2 = std::max( wb2, b );
			wa2 = std::max( wa2, a );
			wf2 = std::max( wf2, fd );
		}
		std::printf( "game textures: %d (texture, ref) pairs, %d levels gated, %d within 2 %%, worst before %.1f %%, after %.1f %%; "
			"worst mean |alpha - scaled| %.2f of 255\n", files, g2, w2, 100.0 * wb2, 100.0 * wa2, wf2 );
		check( files > 0, "read", "no texture read from " + dir );
	}
	std::printf( "TREELEAF1 UNIT %d checks, %d failed: %s\n", checks, fails, fails ? "FAIL" : "PASS" );
	return fails ? 1 : 0;
}

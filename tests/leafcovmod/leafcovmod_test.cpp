/* lane FIX1031B (TREELEAF1's fix): a shape whose colour test multiplies the texture alpha by a vertex / material alpha
 * under 1 binds the plain file, not the coverage copy (src/gl/wwleafcov.h In::modulated, vertexAlphaModulates).
 *
 * bungo's sheet_3 (10-31, SHOTS1031B run4a c3_p3i_e700_* / c3_p3i_e1700_*): with the copy the Concord church's gothic
 * window tracery is gone, WW_TREELEAF_RED=1 draws it. The colour pass tests C.a x tex.a x alpha > ref
 * (fo4_default.frag); bldchurch01open.nif's ChurchSidingWindow01 quad has vertex alpha 25 / 255 (material ref 10: the
 * panes, tex.a 80, fail; the frame, 255, passes), so it tests the texture at T = 10 x 255 / 25 = 102. The copy, built
 * at ref 10, multiplies the small levels' alpha by ~0.3 and the frame fails T everywhere.
 *
 *   rule "church":    the map the colour pass binds for that quad keeps, on every level of >= 16 x 16 texels, at least
 *                     half of level 0's coverage at the quad's own threshold T (the plain mips keep ~100 %).
 *   rule "predicate": vertexAlphaModulates on the drawn cases: the church quad (vA 25, no tree) true; a tree shape (the
 *                     wind weight in vertex alpha, C.a forced 1) false; no vertex colours false; material alpha 0.5 true.
 *   rule "leaves":    a leaf shape (tree, or vertex alpha 1) still binds the copy at its ref, and every gated level keeps
 *                     level 0's coverage at ref within 2 % (or half a texel): the leaves keep TREELEAF1's cover.
 *   -DLEAFCOVMOD_RED  main 5422be6c's colourRef (the copy whatever multiplies the alpha): "church" must FAIL.
 *
 * Inputs: a synthetic gothic window (frame 255, panes 80, bars 4 texels) and a synthetic leaf atlas, plus, when a folder
 * is given, its manifest.txt: "<file> <ref> <vertex alpha 1..255 of the drawn shape, 255 = leaf> <name>" (.tlf = 'TLF1' +
 * u32 w, h, levels, dxgi format + the BC3 chain; a file given as an absolute path is read there). The game's textures are
 * never in the repo (notes/fix1031b/leaves/tools/church_census.py + mk_manifest.py write them). */

#ifdef LEAFCOVMOD_RED
#define WWLEAFCOV_RED_MODULATE
#endif
#include "gl/wwalphacov.h"
#include "gl/wwleafcov.h"

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
		if ( fails <= 60 )
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
	const std::uint8_t * level( int l ) const { return body.data() + off[size_t( l )]; }
	int lw( int l ) const { return std::max( w >> l, 1 ); }
	int lh( int l ) const { return std::max( h >> l, 1 ); }
};

// ---- synthetic inputs (box-filtered mips, a plain BC3 alpha encoder, as tests/treeleaf1) ---------------------------

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

static Chain fromLevel0( const std::vector<float> & a0, int size )
{
	Chain ch;
	ch.w = ch.h = size;
	for ( int s = size; s >= 1; s >>= 1 )
		ch.levels++;
	ch.layout();
	std::vector<float> cur = a0;
	int w = size;
	for ( int l = 0; l < ch.levels; l++ ) {
		encodeBC3Alpha( cur, w, w, ch.level( l ) );
		if ( w == 1 )
			break;
		const int n = w / 2;
		std::vector<float> nx( size_t( n ) * size_t( n ) );
		for ( int y = 0; y < n; y++ )
		for ( int x = 0; x < n; x++ )
			nx[size_t( y ) * size_t( n ) + size_t( x )] = 0.25f * ( cur[size_t( 2 * y ) * size_t( w ) + size_t( 2 * x )]
				+ cur[size_t( 2 * y ) * size_t( w ) + size_t( 2 * x + 1 )] + cur[size_t( 2 * y + 1 ) * size_t( w ) + size_t( 2 * x )]
				+ cur[size_t( 2 * y + 1 ) * size_t( w ) + size_t( 2 * x + 1 )] );
		cur.swap( nx );
		w = n;
	}
	return ch;
}

//! a gothic window like ChurchSidingWindow01's: background 0, a frame 255 (12 / 256 of the size), panes 80, bars 255
static Chain syntheticWindow( int size )
{
	std::vector<float> a( size_t( size ) * size_t( size ), 0.0f );
	const float cx = size * 0.5f, half = size * 0.36f, top = size * 0.08f, spring = size * 0.45f, bot = size * 0.94f;
	auto inside = [&]( float x, float y, float inset ) -> bool {
		if ( y > bot - inset || x < cx - half + inset || x > cx + half - inset || y < top + inset )
			return false;
		if ( y >= spring )
			return true;
		const float R = 2.0f * half - inset;	// the pointed arch: two circles centred on the springing line
		return std::hypot( x - ( cx - half ), y - spring ) <= R && std::hypot( x - ( cx + half ), y - spring ) <= R;
	};
	for ( int y = 0; y < size; y++ )
	for ( int x = 0; x < size; x++ ) {
		const float fx = x + 0.5f, fy = y + 0.5f;
		float v = 0.0f;
		if ( inside( fx, fy, 0.0f ) ) {
			v = 1.0f;
			if ( inside( fx, fy, size * 12.0f / 256.0f ) ) {
				v = 80.0f / 255.0f;
				const float bar = size * 2.0f / 256.0f;
				for ( float m : { cx - half / 3.0f, cx + half / 3.0f } )
					if ( std::fabs( fx - m ) <= bar )
						v = 1.0f;
				if ( std::fabs( fy - ( spring + bot ) * 0.5f ) <= bar )
					v = 1.0f;
			}
		}
		a[size_t( y ) * size_t( size ) + size_t( x )] = v;
	}
	return fromLevel0( a, size );
}

static std::uint32_t rng = 12345u;
static float frand()
{
	rng = rng * 1664525u + 1013904223u;
	return float( rng >> 8 ) / float( 1u << 24 );
}

static Chain syntheticLeaves( int size )	// tests/treeleaf1's atlas
{
	std::vector<float> a( size_t( size ) * size_t( size ), 0.0f );
	for ( int n = 0; n < 420; n++ ) {
		const float cx = frand() * size, cy = frand() * size, r = 3.0f + frand() * 6.0f, q = 0.25f + frand() * 0.25f;
		const float th = frand() * 6.2831853f, c = std::cos( th ), s = std::sin( th );
		for ( int y = int( cy - r - 2 ); y <= int( cy + r + 2 ); y++ )
		for ( int x = int( cx - r - 2 ); x <= int( cx + r + 2 ); x++ ) {
			if ( x < 0 || y < 0 || x >= size || y >= size )
				continue;
			const float dx = float( x ) + 0.5f - cx, dy = float( y ) + 0.5f - cy;
			const float u = ( dx * c + dy * s ) / r, v = ( -dx * s + dy * c ) / ( r * q );
			const float d = std::sqrt( u * u + v * v );
			const float al = std::clamp( ( 1.0f - d ) * r * 0.7f, 0.0f, 1.0f );
			float & t = a[size_t( y ) * size_t( size ) + size_t( x )];
			t = std::max( t, al );
		}
	}
	return fromLevel0( a, size );
}

// ---- the rules -----------------------------------------------------------------------------------------------------

//! texels of level l with alpha x vA > ref x 255 (the colour test with vertex alpha vA / 255), as a share
static double share( const Chain & ch, int l, int ref, int vA )
{
	WwAlphaCov::Level L;
	L.build( ch.level( l ), ch.lw( l ), ch.lh( l ), true );
	size_t n = 0;
	for ( int v : L.u )
		n += ( std::int64_t( v ) * vA > std::int64_t( ref ) * 255 * 35 ) ? 1 : 0;
	return L.u.empty() ? 0.0 : double( n ) / double( L.u.size() );
}

//! the shape the colour pass draws: an alpha-tested, unblended material at `ref`, vertex alpha vA / 255 (255: none)
static WwLeafCov::In drawn( int ref, int vA, bool tree )
{
	WwLeafCov::In in;
	in.lighting = true;
	in.blendingOn = true;
	in.hasMat = true;
	in.matTest = true;
	in.matRef = ref;
	in.modulated = WwLeafCov::vertexAlphaModulates( vA < 255, true, tree, false, float( vA ) / 255.0f, 1.0f );
	return in;
}

//! the chain the colour pass binds: the copy at colourRef, or the plain file
static Chain bound( const Chain & plain, const WwLeafCov::In & in, int * refOut = nullptr )
{
	Chain ch = plain;
	const int r = WwLeafCov::colourRef( in );
	if ( refOut )
		*refOut = r;
	if ( r > 0 ) {
		std::vector<std::uint8_t *> lv;
		for ( int l = 0; l < ch.levels; l++ )
			lv.push_back( ch.level( l ) );
		WwAlphaCov::preserve( lv.data(), ch.levels, ch.w, ch.h, r );
	}
	return ch;
}

static void ruleChurch( const Chain & plain, int ref, int vA, const std::string & name, double & worst )
{
	const Chain ch = bound( plain, drawn( ref, vA, false ) );
	const double c0 = share( plain, 0, ref, vA );
	for ( int l = 1; l < plain.levels; l++ ) {
		if ( plain.lw( l ) * plain.lh( l ) < 256 || c0 <= 0.0 )
			continue;
		const double c = share( ch, l, ref, vA ), p = share( plain, l, ref, vA );
		worst = std::min( worst, c / c0 );
		char buf[320];
		std::snprintf( buf, sizeof( buf ), "%s ref %d vA %d level %d (%dx%d): bound %.4f, plain %.4f, level 0 %.4f (%.1f %% of level 0)",
			name.c_str(), ref, vA, l, plain.lw( l ), plain.lh( l ), c, p, c0, 100.0 * c / c0 );
		std::printf( "  %s\n", buf );
		check( c >= 0.5 * c0, "church", buf );
	}
}

static void ruleLeaves( const Chain & plain, int ref, const std::string & name, double & worst )
{
	for ( bool tree : { true, false } ) {
		int r = 0;
		const Chain ch = bound( plain, drawn( ref, tree ? 40 : 255, tree ), &r );	// a tree's vertex alpha is its wind weight
		check( r == ref, "leaves", name + ( tree ? " (tree)" : " (no vertex alpha)" ) + " binds the plain file, not the copy" );
		const double c0 = share( plain, 0, ref, 255 );
		for ( int l = 1; l < plain.levels; l++ ) {
			const int w = plain.lw( l ), h = plain.lh( l );
			if ( w * h < 256 || c0 <= 0.0 )
				continue;
			const double c = share( ch, l, ref, 255 );
			worst = std::max( worst, std::fabs( c / c0 - 1.0 ) );
			char buf[256];
			std::snprintf( buf, sizeof( buf ), "%s ref %d level %d (%dx%d): coverage %.4f vs level 0 %.4f", name.c_str(), ref, l, w, h, c, c0 );
			check( std::fabs( c / c0 - 1.0 ) <= 0.02 || std::fabs( c - c0 ) * double( w ) * double( h ) <= 0.5 + 1e-9, "leaves", buf );
		}
	}
}

static void rulePredicate()
{
	using WwLeafCov::vertexAlphaModulates;
	check( vertexAlphaModulates( true, true, false, false, 25.0f / 255.0f, 1.0f ), "predicate", "church quad (vA 25) not modulated" );
	check( !vertexAlphaModulates( true, true, true, false, 0.1f, 1.0f ), "predicate", "tree shape (C.a forced 1) modulated" );
	check( !vertexAlphaModulates( false, true, false, false, 1.0f, 1.0f ), "predicate", "no vertex colours modulated" );
	check( !vertexAlphaModulates( true, true, false, false, 1.0f, 1.0f ), "predicate", "vertex alpha 1 modulated" );
	check( !vertexAlphaModulates( true, false, false, false, 0.1f, 1.0f ), "predicate", "Vertex Alpha option off modulated" );
	check( !vertexAlphaModulates( true, true, false, true, 0.1f, 1.0f ), "predicate", "pre-130 without SLSF1 Vertex_Alpha modulated" );
	check( vertexAlphaModulates( false, true, false, false, 1.0f, 0.5f ), "predicate", "material alpha 0.5 not modulated" );
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
	rulePredicate();
	double wc = 1e9, wl = 0.0;
	ruleChurch( syntheticWindow( 512 ), 10, 25, "synthetic window", wc );
	std::printf( "synthetic window: worst level keeps %.1f %% of level 0's coverage at T\n", 100.0 * wc );
	const Chain leaves = syntheticLeaves( 256 );
	for ( int ref : { 64, 90, 128, 185 } )
		ruleLeaves( leaves, ref, "synthetic leaves", wl );
	std::printf( "synthetic leaves: worst coverage drift at ref %.1f %%\n", 100.0 * wl );
	if ( argc > 1 ) {
		const std::string dir = argv[1];
		std::ifstream man( dir + "/manifest.txt" );
		std::string line;
		int files = 0, church = 0;
		while ( std::getline( man, line ) ) {
			std::istringstream ss( line );
			std::string file, name;
			int ref = -1, vA = 255;
			if ( !( ss >> file >> ref >> vA >> name ) )
				continue;
			const bool abs = file.size() > 2 && ( file[0] == '/' || file[1] == ':' );
			Chain ch;
			if ( !readTlf( abs ? file : dir + "/" + file, ch ) ) {
				check( false, "read", "cannot read " + file );
				continue;
			}
			if ( vA < 255 ) {
				double w = 1e9;
				ruleChurch( ch, ref, vA, name, w );
				std::printf( "file %s ref %d vA %d: worst level keeps %.1f %% of level 0's coverage at T\n", name.c_str(), ref, vA, 100.0 * w );
				church++;
			} else {
				double w = 0.0;
				ruleLeaves( ch, ref, name, w );
				std::printf( "file %s ref %d: worst coverage drift at ref %.1f %%\n", name.c_str(), ref, 100.0 * w );
			}
			files++;
		}
		check( files > 0, "read", "no texture read from " + dir );
		check( church > 0, "read", "no modulated (vA < 255) row in " + dir + "/manifest.txt" );
	}
	std::printf( "LEAFCOVMOD UNIT %d checks, %d failed: %s\n", checks, fails, fails ? "FAIL" : "PASS" );
	return fails ? 1 : 0;
}

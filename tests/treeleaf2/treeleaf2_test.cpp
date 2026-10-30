/* lane TREELEAF2: unit test of the side passes' alpha test (src/gl/wwleafcov.h): the sun cascades, the light cubes, the
 * TAA inputs and the reflection probes keep the same texel set of a leaf's base map as the colour pass.
 *
 *   rule:   colourRef() is TREELEAF1's setupProgramCE1 rule (material / NIF, blend, land, translucent, DoBlending, refs 0
 *           and 255) plus the WW_LODL_TREE_ALPHA dial; passRed() parses WW_TREELEAF_RED; sideThreshold() turns a strict
 *           colour test into the side shaders' `discard a < t`.
 *   texels: on game textures (a folder of TREELEAF1 .tlf dumps + manifest.txt), every level of at least 16 x 16: the
 *           colour pass's texels (the coverage copy, `a > ref / 255` in float) and each side pass's texels (what that
 *           pass binds, `!( a < t )` with the threshold it uploads) must be the SAME set. Coverage per pass is printed.
 *   -DTREELEAF2_RED_PLAIN     today's side passes (d05945bb): the plain file and the threshold ref / 255. Must FAIL.
 *   -DTREELEAF2_RED_NOSTRICT  the coverage copy but the old threshold (texels AT the ref pass in the side passes only).
 *                             Must FAIL.
 * The float model: a texel's alpha as the GPU hands it to the shader is the BC3 palette value / 255 as a float (exact
 * fractions in 1/35 units here); the test multiplies by the material alpha 1.0. Bilinear filtering is not modelled: the
 * count is per texel, the set the filter blends between. */

#include "gl/wwalphacov.h"
#include "gl/wwleafcov.h"

#include <cstdio>
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

// ---- the rule ------------------------------------------------------------------------------------------------------

static WwLeafCov::In mat( int ref, bool blend = false )
{
	WwLeafCov::In i;
	i.lighting = true;
	i.hasMat = true;
	i.matTest = true;
	i.matBlend = blend;
	i.matRef = ref;
	return i;
}

static WwLeafCov::In nif( int func, float thr )
{
	WwLeafCov::In i;
	i.lighting = true;
	i.nifTest = true;
	i.nifFunc = func;
	i.nifThreshold = thr;
	return i;
}

// TREELEAF1's rule as it stood in setupProgramCE1 (d05945bb), for the identity check
static int treeleaf1Rule( const WwLeafCov::In & s )
{
	int covRef = 0;
	if ( s.blendingOn && !s.land ) {
		if ( s.hasMat ) {
			if ( s.matTest && !s.matBlend )
				covRef = s.matRef;
		} else if ( !s.translucent && s.nifTest && !s.nifBlend ) {
			const int r = int( std::lround( s.nifThreshold * 255.0f ) );
			if ( s.nifFunc == 4 )
				covRef = r;
			else if ( s.nifFunc == 6 )
				covRef = r - 1;
		}
	}
	return ( covRef > 0 && covRef < 255 ) ? covRef : 0;
}

static void ruleChecks()
{
	using namespace WwLeafCov;
	check( colourRef( mat( 185 ) ) == 185, "rule", "material ref 185" );
	check( colourRef( mat( 185, true ) ) == 0, "rule", "blended material: plain file" );
	check( colourRef( mat( 0 ) ) == 0 && colourRef( mat( 255 ) ) == 0, "rule", "ref 0 / 255: plain file" );
	In e = mat( 90 );
	e.lighting = false;
	check( colourRef( e ) == 0, "rule", "effect shader: plain file (the colour pass arms only a lighting base map)" );
	In l = mat( 90 );
	l.land = true;
	check( colourRef( l ) == 0, "rule", "blended ground: plain file" );
	In b = mat( 90 );
	b.blendingOn = false;
	check( colourRef( b ) == 0, "rule", "DoBlending off: plain file" );
	In d = mat( 90 );
	d.treeDial = 128;
	check( colourRef( d ) == 128, "rule", "WW_LODL_TREE_ALPHA dial: the copy at the dial the colour pass tests" );
	check( colourRef( nif( 4, 128.0f / 255.0f ) ) == 128, "rule", "NIF > 128" );
	check( colourRef( nif( 6, 128.0f / 255.0f ) ) == 127, "rule", "NIF >= 128 is > 127" );
	check( colourRef( nif( 1, 0.5f ) ) == 0, "rule", "NIF < : plain file" );
	In t = nif( 4, 0.5f );
	t.translucent = true;
	check( colourRef( t ) == 0, "rule", "translucent NIF: plain file" );
	// identity with TREELEAF1's rule over a grid of inputs (lighting shapes, no dial)
	int same = 0, n = 0;
	for ( int m = 0; m < 2; m++ )
	for ( int r = 0; r < 256; r += 5 )
	for ( int bits = 0; bits < 64; bits++ ) {
		In i;
		i.lighting = true;
		i.hasMat = m;
		i.matTest = bits & 1;
		i.matBlend = bits & 2;
		i.matRef = r;
		i.land = bits & 4;
		i.blendingOn = !( bits & 8 );
		i.translucent = bits & 16;
		i.nifTest = !( bits & 32 );
		i.nifFunc = ( r % 2 ) ? 4 : 6;
		i.nifThreshold = float( r ) / 255.0f;
		same += colourRef( i ) == treeleaf1Rule( i ) ? 1 : 0;
		n++;
	}
	check( same == n, "rule", "colourRef == TREELEAF1's setupProgramCE1 rule on " + std::to_string( n ) + " inputs ("
		+ std::to_string( same ) + " equal)" );
	// the red's parse
	check( passRed( "1", "colour" ) && passRed( "1", "sun" ), "red", "1: every pass" );
	check( !passRed( "side", "colour" ) && passRed( "side", "cube" ) && passRed( "side", "refl" ), "red", "side: side passes only" );
	check( passRed( "sun,taa", "taa" ) && !passRed( "sun,taa", "cube" ) && !passRed( "sun,taa", "su" ), "red", "a list" );
	check( !passRed( "", "sun" ) && !passRed( nullptr, "sun" ) && !passRed( "0", "sun" ), "red", "unset / 0: no red" );
	// the threshold: next float above ref / 255 for a strict colour test; untouched otherwise
	for ( int r = 1; r < 255; r++ ) {
		const float c = float( r ) / 255.0f, t = sideThreshold( mat( r ), true, c );
		check( t > c && !( std::nextafter( c, 2.0f ) < t ), "threshold", "material ref " + std::to_string( r ) );
	}
	check( sideThreshold( nif( 6, 0.5f ), false, 0.5f ) == 0.5f, "threshold", "NIF >=: side test already agrees" );
	check( sideThreshold( nif( 4, 0.5f ), false, 0.5f ) > 0.5f, "threshold", "NIF >: strict" );
	check( sideThreshold( e, true, 0.5f ) == 0.5f, "threshold", "effect shader: untouched" );
	In dd = mat( 90 );
	dd.treeDial = 128;
	check( sideThreshold( dd, true, 90.0f / 255.0f ) > 128.0f / 255.0f
		&& sideThreshold( dd, true, 90.0f / 255.0f ) < 129.0f / 255.0f, "threshold", "dial: the colour pass's 128" );
}

// ---- the texels ----------------------------------------------------------------------------------------------------

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

// one level's alpha in 1/35 units (w x h, row-major)
static std::vector<int> alphaOf( const std::uint8_t * d, int w, int h )
{
	std::vector<int> u( size_t( w ) * size_t( h ) );
	const int bw = ( w + 3 ) / 4;
	for ( int by = 0; by < ( h + 3 ) / 4; by++ )
	for ( int bx = 0; bx < bw; bx++ ) {
		int v[16];
		WwAlphaCov::decodeBC3Alpha( d + 16 * ( size_t( by ) * size_t( bw ) + size_t( bx ) ), v );
		for ( int t = 0; t < 16; t++ ) {
			const int x = bx * 4 + ( t & 3 ), y = by * 4 + ( t >> 2 );
			if ( x < w && y < h )
				u[size_t( y ) * size_t( w ) + size_t( x )] = v[t];
		}
	}
	return u;
}

static float gpuAlpha( int u35 )
{
	return float( double( u35 ) / ( 35.0 * 255.0 ) );
}

static void texels( const std::string & dir )
{
	std::ifstream man( dir + "/manifest.txt" );
	std::string line;
	int files = 0, levels = 0, equalLevels = 0;
	std::uint64_t mism = 0, atRef = 0, atRefPlain = 0;
	while ( std::getline( man, line ) ) {
		std::istringstream ss( line );
		std::string file, name;
		int ref = -1;
		if ( !( ss >> file >> ref >> name ) )
			continue;
		Chain plain;
		if ( !readTlf( dir + "/" + file, plain ) ) {
			check( false, "read", "cannot read " + file );
			continue;
		}
		files++;
		Chain cov = plain;
		std::vector<std::uint8_t *> lv;
		for ( int l = 0; l < cov.levels; l++ )
			lv.push_back( cov.level( l ) );
		const WwLeafCov::In in = mat( ref );
		WwAlphaCov::preserve( lv.data(), cov.levels, cov.w, cov.h, WwLeafCov::colourRef( in ) );
		const float colourT = float( ref ) / 255.0f;	// setupProgramCE1: alphaThreshold, alphaFlags 4 (a > t)
#if defined( TREELEAF2_RED_PLAIN ) || defined( TREELEAF2_RED_NOSTRICT )
		const float sideT = colourT;	// Shape::wwDrawAlpha's threshold as today's side passes upload it
#else
		const float sideT = WwLeafCov::sideThreshold( in, true, colourT );
#endif
		std::printf( "file %s ref %d %dx%d\n  level    texels  colour(copy,>)  side-before(plain,>=)  side-now  mismatch-now  "
			"mismatch-before  at-ref(copy)\n", name.c_str(), ref, plain.w, plain.h );
		for ( int l = 0; l < plain.levels; l++ ) {
			const int w = std::max( plain.w >> l, 1 ), h = std::max( plain.h >> l, 1 );
			if ( w < 16 || h < 16 )
				break;
			const std::vector<int> up = alphaOf( plain.level( l ), w, h ), uc = alphaOf( cov.level( l ), w, h );
#ifdef TREELEAF2_RED_PLAIN
			const std::vector<int> & us = up;	// the side passes bind the plain file
#else
			const std::vector<int> & us = uc;	// the side passes bind the colour pass's copy
#endif
			std::uint64_t nc = 0, nb = 0, ns = 0, mNow = 0, mBefore = 0, ar = 0, arp = 0;
			for ( size_t i = 0; i < uc.size(); i++ ) {
				const bool c = gpuAlpha( uc[i] ) * 1.0f > colourT;
				const bool before = !( gpuAlpha( up[i] ) * 1.0f < colourT );
				const bool side = !( gpuAlpha( us[i] ) * 1.0f < sideT );
				nc += c;
				nb += before;
				ns += side;
				mNow += c != side;
				mBefore += c != before;
				ar += uc[i] == ref * 35;
				arp += up[i] == ref * 35;
			}
			const double N = double( uc.size() );
			std::printf( "  %2d %9llu      %.4f            %.4f            %.4f   %10llu   %12llu   %10llu\n", l,
				(unsigned long long) uc.size(), nc / N, nb / N, ns / N, (unsigned long long) mNow,
				(unsigned long long) mBefore, (unsigned long long) ar );
			levels++;
			equalLevels += mNow == 0;
			mism += mNow;
			atRef += ar;
			atRefPlain += arp;
			check( mNow == 0, "texels", name + " level " + std::to_string( l ) + ": " + std::to_string( mNow )
				+ " texels differ between the colour pass and the side passes" );
		}
	}
	std::printf( "game textures: %d files, %d levels >= 16x16, %d with the side passes' texel set == the colour pass's; "
		"%llu texels differ; texels exactly at ref: copy %llu, plain %llu\n", files, levels, equalLevels,
		(unsigned long long) mism, (unsigned long long) atRef, (unsigned long long) atRefPlain );
	check( files > 0, "read", "no texture read from " + dir );
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	ruleChecks();
	if ( argc > 1 )
		texels( argv[1] );
	std::printf( "TREELEAF2 UNIT %d checks, %d failed: %s\n", checks, fails, fails ? "FAIL" : "PASS" );
	return fails ? 1 : 0;
}

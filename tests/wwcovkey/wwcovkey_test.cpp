// lane STREAMTEX5 (STREAMTEX4-WWCOVKEY): a coverage copy's pool key (src/gl/streamtexpool.h streamTexCovAt), no Qt, no GL.
//   wwcovkey_test        -> keys stripped as src/cellstream.cpp texPoolKey does: WWCOVKEY PASS, rc 0
//   wwcovkey_test --red  -> WW_CELL_STREAMTEX_RED=rawkey, the cache's raw name: must FAIL, rc 1
// Floor: a loaded cell notes "leaf.dds"; the colour pass binds its coverage copy "leaf.dds|wwcov128" (TREELEAF1). That
// copy must sit in kTierKeep (rule Keep) and the crossing's census must count no unnoted texture.
#include "gl/streamtexpool.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace
{
bool red = false;

std::string poolKey( const std::string & name )	// texPoolKey in src/cellstream.cpp, std::string for QStringView
{
	const std::ptrdiff_t at = red ? -1 : streamTexCovAt( name.data(), std::ptrdiff_t( name.size() ) );
	return at < 0 ? name : name.substr( 0, std::size_t( at ) );
}

int fails = 0;
void check( bool ok, const char * what )
{
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what );
	if ( !ok )
		fails++;
}
}

int main( int argc, char ** argv )
{
	red = argc > 1 && !std::strcmp( argv[1], "--red" );

	// the suffix rule itself (both modes: it is the helper, not the red)
	const auto at = []( const char * s ) { return streamTexCovAt( s, std::ptrdiff_t( std::strlen( s ) ) ); };
	check( at( "textures\\leaf.dds|wwcov128" ) == 17, "suffix found: textures\\leaf.dds|wwcov128 -> 17" );
	check( at( "leaf.dds|wwcov5" ) == 8, "one-digit ref" );
	check( at( "leaf.dds" ) == -1, "plain name kept" );
	check( at( "leaf.dds|wwcov" ) == -1, "no digits: not a coverage copy" );
	check( at( "leaf.dds|wwcovx7" ) == -1, "letters before the digits: not a coverage copy" );
	check( at( "wwcov12" ) == -1, "no bar: not a coverage copy" );
	const char16_t wide[] = u"leaf.dds|wwcov64";
	check( streamTexCovAt( wide, std::ptrdiff_t( sizeof( wide ) / sizeof( wide[0] ) - 1 ) ) == 8, "UTF-16 name (QStringView's storage)" );

	using Pool = StreamTexPool<std::string>;
	Pool pool( 256ull << 20, 2, false );
	const Pool::Cell cell = Pool::cellId( 3, -7 );
	pool.note( cell, "textures\\leaf.dds" );
	pool.note( cell, "textures\\bark.dds" );
	pool.loaded( poolKey( "textures\\leaf.dds|wwcov128" ), 4u << 20 );	// the colour pass's coverage copy
	pool.loaded( poolKey( "textures\\bark.dds" ), 2u << 20 );

	check( pool.rule( poolKey( "textures\\leaf.dds|wwcov128" ) ) == Pool::Rule::Keep,
		"coverage copy of a noted file: rule Keep (kTierKeep) while its cell is loaded" );
	check( pool.rule( poolKey( "textures\\bark.dds" ) ) == Pool::Rule::Keep, "plain noted file: rule Keep" );
	check( pool.isResident( "textures\\leaf.dds" ), "the noted name is resident" );
	const Pool::Census c = pool.cross();
	std::printf( "census: refd %d kept %d baseOnly %d unnoted %d (unnoted bytes %llu)\n", c.refd, c.kept, c.baseOnly,
		c.unnoted, (unsigned long long)c.unnotedBytes );
	check( c.unnoted == 0, "no unnoted texture" );
	check( c.refd == 2, "both textures counted by their cell" );

	// the cell leaves: the copy goes to the pool (tier 0) like its file, and its free is counted on that file
	pool.dropCell( cell );
	check( pool.rule( poolKey( "textures\\leaf.dds|wwcov128" ) ) == Pool::Rule::Pool, "cell left: the copy is the pool's" );
	pool.freed( poolKey( "textures\\leaf.dds|wwcov128" ) );
	check( !pool.isResident( "textures\\leaf.dds" ), "its free reaches the noted name" );

	std::printf( "%s%s (%d failed)\n", fails ? "WWCOVKEY FAIL" : "WWCOVKEY PASS", red ? " [red rawkey]" : "", fails );
	return fails ? 1 : 0;
}

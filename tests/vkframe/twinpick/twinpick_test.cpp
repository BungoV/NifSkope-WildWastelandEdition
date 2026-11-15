/* lane VKOBJ5 item 2: the table gate over the RT-twin pick (src/features/near/celltwinpick.h, vkCellDraw / vkPfxDraw).
 *   tests/vkframe/twinpick/run.sh <out dir>        (from the tree root)
 * For every twin id 2-9, its GL program and its loader keys (base on the base route, RT on the all-on route):
 *   pick:  the key picks exactly its own twin and variant, nothing refused;
 *   near:  the RT key with one RT define missing (RESTIR, RTDDGI, RTDDGI_CASCADES), and the base key, on the all-on route: refused;
 *   other: a program no twin draws: twin 0, refused;
 *   module: the picked (twin, variant) is drawn by the module objCellPs / objPfxShade names (read from vkobjpipe.inc), whose
 *          vkframe_spirv.sh line compiles the program's own pixel source with exactly the key's defines, less the defines
 *          run.sh proved inert for that source (dxc builds it with and without them to the same bytes; `--inert src:D1,D2`):
 *          pbrm_default's RT defines (twin 4's RT key drawn by its base module), fo4_effectdecalcsm's air zones.
 * Red (-DTWINPICK_RED_SWAP, must print "FAIL module:" and exit 1): the RT modules of twins 2 and 3 swapped.
 * Prints "TWINPICK PASS <n>" and exits 0 when every check holds. */
#include "features/near/celltwinpick.h"

#include <cstdio>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
int checks = 0, failed = 0;

void check( bool ok, const char * name, const std::string & what )
{
	checks++;
	if ( !ok ) {
		failed++;
		std::printf( "FAIL %s: %s\n", name, what.c_str() );
	}
}

std::string slurp( const std::string & path )
{
	std::ifstream f( path, std::ios::binary );
	std::stringstream s;
	s << f.rdbuf();
	return s.str();
}

std::set<std::string> defsOf( const std::string & key )
{
	std::set<std::string> out;
	std::string cur;
	for ( char c : key + "+" ) {
		if ( c == '+' ) {
			if ( !cur.empty() )
				out.insert( cur );
			cur.clear();
		} else {
			cur += c;
		}
	}
	return out;
}

std::string keyOf( const std::set<std::string> & d )
{
	std::string k;
	for ( const std::string & s : d )
		k += ( k.empty() ? "" : "+" ) + s;
	return k;
}

struct Module
{
	std::string source;
	std::set<std::string> defs;
};
}  // namespace

int main( int argc, char ** argv )
{
	std::string root = ".";
	std::map<std::string, std::set<std::string>> inert;  // pixel source -> defines dxc proved make no difference to it (run.sh)
	for ( int i = 1; i < argc; i++ ) {
		const std::string a = argv[i];
		if ( a == "--inert" && i + 1 < argc ) {
			const std::string v = argv[++i];
			const std::size_t c = v.find( ':' );
			std::string list = v.substr( c + 1 );
			for ( char & ch : list )
				ch = ch == ',' ? '+' : ch;
			if ( c != std::string::npos )
				for ( const std::string & d : defsOf( list ) )
					inert[v.substr( 0, c )].insert( d );
		} else {
			root = a;
		}
	}
	const std::string inc = slurp( root + "/src/features/vkframe/vkobjpipe.inc" );
	const std::string sh = slurp( root + "/src/features/vkframe/vkframe_spirv.sh" );
	check( !inc.empty() && !sh.empty(), "read", "vkobjpipe.inc / vkframe_spirv.sh" );

	// objCellPs: (twin, rt) -> module; the rt switch ends at its `default: break;`, the base switch's default is twin 2's
	std::map<std::pair<int, bool>, std::string> mod;
	const std::size_t at = inc.find( "void objCellPs(" );
	const std::size_t mid = at == std::string::npos ? at : inc.find( "default: break;", at );
	const std::size_t end = mid == std::string::npos ? mid : inc.find( "\n}\n", mid );
	check( end != std::string::npos, "read", "objCellPs not found in vkobjpipe.inc" );
	if ( end != std::string::npos ) {
		const std::regex cs( "case (\\d+): words = vkframe_(\\w+)_spv;" );
		const std::string rtPart = inc.substr( at, mid - at ), basePart = inc.substr( mid, end - mid );
		for ( std::sregex_iterator i( rtPart.begin(), rtPart.end(), cs ), e; i != e; ++i )
			mod[{ std::stoi( ( *i )[1] ), true }] = ( *i )[2];
		for ( std::sregex_iterator i( basePart.begin(), basePart.end(), cs ), e; i != e; ++i )
			mod[{ std::stoi( ( *i )[1] ), false }] = ( *i )[2];
		std::smatch m;
		const std::regex def( "default: words = vkframe_(\\w+)_spv;" );
		if ( std::regex_search( basePart, m, def ) )
			mod[{ 2, false }] = m[1];
		for ( int t = 2; t <= 8; t++ )
			if ( !mod.count( { t, true } ) )
				mod[{ t, true }] = mod[{ t, false }];  // the rt switch's default: break falls to the base module
	}
	{
		std::smatch m;
		const std::regex pfx( "d\\.ps = s\\.rt \\? vkframe_(\\w+)_spv : vkframe_(\\w+)_spv;" );
		check( std::regex_search( inc, m, pfx ), "read", "objPfxShade's module line not found" );
		if ( !m.empty() ) {
			mod[{ 9, true }] = m[1];
			mod[{ 9, false }] = m[2];
		}
	}
#ifdef TWINPICK_RED_SWAP
	std::swap( mod[{ 2, true }], mod[{ 3, true }] );
#endif

	// vkframe_spirv.sh: module -> its pixel source and defines (fxc: FOGLEAK + FRAME_UBO; fxp: + AIRZONE; then the line's -D)
	std::map<std::string, Module> spv;
	{
		const std::regex line( "^(fxc|fxp) (\\w+) (\\w+) ps_6_0 \"\\$PS_SHIFT\"(?: \"([^\"]*)\")?\\s*$" );
		std::istringstream in( sh );
		for ( std::string l; std::getline( in, l ); ) {
			if ( !l.empty() && l.back() == '\r' )
				l.pop_back();
			std::smatch m;
			if ( !std::regex_match( l, m, line ) )
				continue;
			Module x;
			x.source = m[3];
			x.defs = { "WW_CELL_FOGLEAK", "WW_FRAME_UBO" };
			if ( m[1] == "fxp" )
				x.defs.insert( "WW_CELL_AIRZONE" );
			const std::string extra = m[4];
			const std::regex d( "-D (\\w+)=1" );
			for ( std::sregex_iterator i( extra.begin(), extra.end(), d ), e; i != e; ++i )
				x.defs.insert( ( *i )[1] );
			spv[m[2]] = x;
		}
	}

	struct Row
	{
		const char * prog;
		int twin;
		std::vector<const char *> base, rt;  // the loader keys GL builds for it on the base / all-on route
	};
	const std::string B = "WW_CELL_FOGLEAK+WW_FRAME_UBO", BA = "WW_CELL_AIRZONE+WW_CELL_FOGLEAK+WW_FRAME_UBO";
	const std::vector<Row> rows = {
		{ "fo4_cell.prog", 2, { B.c_str() }, { kCellTwinRtKey } },
		{ "fo4_cellcsm.prog", 3, { B.c_str() }, { kCellTwinRtKey } },
		{ "pbrm_default.prog", 4, { B.c_str() }, { kCellTwinRtKey } },
		{ "pbrm_cell.prog", 5, { B.c_str() }, { kCellTwinRtKey } },
		{ "pbrm_cellcsm.prog", 6, { B.c_str() }, { kCellTwinRtKey } },
		{ "fo4_effectcell.prog", 7, { BA.c_str() }, { kCellTwinRtKeyAir } },
		{ "fo4_effectdecalcsm.prog", 8, { B.c_str(), BA.c_str() }, { kCellTwinRtKey, kCellTwinRtKeyAir } },
		{ "particles_cell.prog", 9, { BA.c_str() }, { kCellTwinRtKeyAir } },
	};
	CellTwinRoute base, allOn;
	allOn.rtddgi = allOn.restir = true;
	const char * rtDefs[3] = { "WW_CELL_RESTIR", "WW_CELL_RTDDGI", "WW_CELL_RTDDGI_CASCADES" };
	for ( const Row & r : rows ) {
		const std::string prog = r.prog;
		const std::string stem = prog.substr( 0, prog.size() - 5 ) + "_ps";
		for ( int v = 0; v < 2; v++ ) {
			const bool rt = v == 1;
			for ( const char * key : rt ? r.rt : r.base ) {
				const CellTwinPick p = cellTwinPick( prog, key, rt ? allOn : base );
				const std::string at = prog + " @" + key;
				check( p.twin == r.twin && p.rt == rt && !p.no, "pick",
					at + ": twin " + std::to_string( p.twin ) + ( p.rt ? " rt" : " base" ) + ( p.no ? std::string( ", refused: " ) + p.no : "" ) );
				// the module that draws it compiles this program's pixel source with exactly the key's defines
				const auto mi = mod.find( { r.twin, rt } );
				const auto si = mi == mod.end() ? spv.end() : spv.find( mi->second );
				if ( si == spv.end() ) {
					check( false, "module", at + ": no module / no vkframe_spirv.sh line for it" );
					continue;
				}
				std::set<std::string> want = defsOf( key ), got = si->second.defs;
				for ( const std::string & d : inert[si->second.source] ) {
					want.erase( d );
					got.erase( d );
				}
				check( si->second.source == stem && want == got, "module",
					at + ": " + mi->second + " compiles " + si->second.source + " @" + keyOf( si->second.defs ) );
				if ( !rt )
					continue;
				// the near misses: one RT define short, on the all-on route
				for ( const char * drop : rtDefs ) {
					std::set<std::string> d = defsOf( key );
					d.erase( drop );
					const CellTwinPick n = cellTwinPick( prog, keyOf( d ), allOn );
					check( n.no != nullptr && !n.rt, "near", prog + " @" + keyOf( d ) + " (no " + drop + ") was not refused" );
				}
			}
		}
		for ( const char * key : r.base ) {
			const CellTwinPick n = cellTwinPick( prog, key, allOn );
			check( n.no != nullptr && !n.rt, "near", prog + " @" + key + " (the base key on the all-on route) was not refused" );
		}
	}
	for ( const char * other : { "fo4_cellbl.prog", "fo4_default.prog", "particles.prog" } ) {
		const CellTwinPick n = cellTwinPick( other, kCellTwinRtKeyAir, allOn );
		check( n.twin == 0 && n.no != nullptr, "other", std::string( other ) + " picked twin " + std::to_string( n.twin ) );
	}
	if ( failed ) {
		std::printf( "TWINPICK FAIL %d of %d\n", failed, checks );
		return 1;
	}
	std::printf( "TWINPICK PASS %d\n", checks );
	return 0;
}

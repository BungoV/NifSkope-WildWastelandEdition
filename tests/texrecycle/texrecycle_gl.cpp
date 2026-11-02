// lane PERFFIX5: the cell-stream walk's FILE TEXTURES, offline, on the real driver. Each cell has its own textures
// plus a share of its region's (BC1/BC3/BC5/BC7, 256..2048 px, full mip chains, glTexStorage2D + compressed sub-image
// uploads as gltexloaders.cpp's GLI_create_texture). The view walks the c3 PERFFIX4-WALK path (20 crossings, back to
// the start). The view looks +x: a paint draws the window but its back column. A crossing is 36 paints (6 s at 6 a
// second): paint 14 is the step's dump line, paints 15..23 the read-ahead pumps the next centre's new textures
// (loaded, not drawn yet), paint 24 the window moves. The pool's real rule (src/gl/texpool.h; budget at 89 %
// of the first 5x5 set, as Ultra 6144 MB is under c3's ~6.9 GB) picks what goes.
//   texrecycle_gl.exe <mode> [scale%]
//   timer   : the pool every 2 s (12 paints), evicted textures deleted -- PERFFIX4 as landed (the red)
//   recycle : timer + the storage recycler (src/gl/texrecycle.h)
//   room    : PERFFIX5: the pool also runs BEFORE each upload that would pass the budget (room is made first, so the
//             driver re-uses the freed bytes), and a pumped texture is held until its first draw
// Gate (bungo's walk rule): private bytes at crossing 20 within 5 % of crossing 0. The red must exceed it.
#include "texpool.h"
#include "texrecycle.h"

#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>

#include <windows.h>
#include <psapi.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

static double privateMb()
{
	PROCESS_MEMORY_COUNTERS_EX pmc = {};
	GetProcessMemoryInfo( GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>( &pmc ), sizeof( pmc ) );
	return double( pmc.PrivateUsage ) / 1048576.0;
}

static std::uint32_t hash32( std::uint32_t x )
{
	x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
	return x;
}

struct Shape
{
	GLenum fmt = 0;
	int w = 0, levels = 0, block = 0;
	std::int64_t bytes = 0;
};

// a texture's shape from its id: FO4-like sizes and formats
static Shape shapeOf( std::uint32_t id )
{
	const std::uint32_t h = hash32( id * 2654435761U + 17U );
	static const GLenum fmts[4] = { 0x83F1, 0x83F3, 0x8DBD, 0x8E8C };	// BC1, BC3, BC5, BC7
	static const int blocks[4] = { 8, 16, 16, 16 };
	Shape s;
	const int f = int( h & 3U );
	s.fmt = fmts[f];
	s.block = blocks[f];
	const std::uint32_t r = ( h >> 8 ) % 100U;
	s.w = r < 15 ? 256 : r < 45 ? 512 : r < 88 ? 1024 : 2048;
	for ( int d = s.w; d >= 1; d >>= 1 ) {
		s.bytes += std::int64_t( std::max( 1, ( d + 3 ) / 4 ) ) * std::max( 1, ( d + 3 ) / 4 ) * s.block;
		s.levels++;
	}
	return s;
}

struct Tex
{
	GLuint id = 0;
	std::uint32_t useTick = 0;
	std::int64_t bytes = 0;
	bool pinned = false;	// pumped, not drawn yet
};

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	std::string mode = "timer";
	int scale = 100;
	for ( int a = 1; a < argc; a++ ) {
		if ( std::atoi( argv[a] ) > 0 )
			scale = std::atoi( argv[a] );
		else
			mode = argv[a];
	}
	const bool recycle = mode == "recycle", room = mode == "room";
	if ( mode != "timer" && !recycle && !room ) {
		std::printf( "usage: texrecycle_gl timer|recycle|room [scale%%]\n" );
		return 2;
	}
	QGuiApplication app( argc, argv );
	QSurfaceFormat fmt;
	fmt.setVersion( 4, 3 );
	fmt.setProfile( QSurfaceFormat::CoreProfile );
	QOpenGLContext ctx;
	ctx.setFormat( fmt );
	QOffscreenSurface surf;
	surf.setFormat( fmt );
	surf.create();
	if ( !ctx.create() || !ctx.makeCurrent( &surf ) ) {
		std::printf( "FAIL no GL context\n" );
		return 2;
	}
	QOpenGLExtraFunctions * f = ctx.extraFunctions();
	const int own = std::max( 1, 36 * scale / 100 ), shared = std::max( 1, 36 * scale / 100 ), regionSet = shared * 4;
	auto cellTex = [&]( int x, int y, std::vector<std::uint32_t> & out ) {
		const std::uint32_t c = std::uint32_t( ( x + 512 ) * 1024 + ( y + 512 ) );
		for ( int i = 0; i < own; i++ )
			out.push_back( 0x80000000U | ( c << 7 ) | std::uint32_t( i ) );
		const int rx = ( x + 512 ) / 3, ry = ( y + 512 ) / 3;	// a 3x3-cell region shares its material set
		for ( int i = 0; i < shared; i++ )
			out.push_back( std::uint32_t( ( rx * 1024 + ry ) * 4096 ) + hash32( c * 31U + std::uint32_t( i ) ) % std::uint32_t( regionSet ) );
	};
	// the 5x5 window (back 2), or what a paint draws (back 1: the view looks +x, the column behind is not drawn)
	auto windowSet = [&]( int cx, int cy, int back = 2 ) {
		std::vector<std::uint32_t> set;
		for ( int y = cy - 2; y <= cy + 2; y++ )
			for ( int x = cx - back; x <= cx + 2; x++ )
				cellTex( x, y, set );
		std::sort( set.begin(), set.end() );
		set.erase( std::unique( set.begin(), set.end() ), set.end() );
		return set;
	};
	// the PERFFIX4-WALK centres (walk.path, Concord -15,17): 20 crossings and back to the start
	static const int path[21][2] = { { -15, 17 }, { -14, 17 }, { -15, 17 }, { -14, 17 }, { -13, 17 }, { -12, 17 },
		{ -12, 18 }, { -12, 19 }, { -13, 19 }, { -14, 19 }, { -15, 19 }, { -16, 19 }, { -17, 19 }, { -17, 18 },
		{ -17, 17 }, { -17, 16 }, { -17, 15 }, { -16, 15 }, { -15, 15 }, { -15, 16 }, { -15, 17 } };

	std::vector<char> blob( size_t( 6 ) << 20 );
	for ( size_t i = 0; i < blob.size(); i++ )
		blob[i] = char( hash32( std::uint32_t( i ) ) );
	std::unordered_map<std::uint32_t, Tex> tex;
	std::unordered_map<GLuint, WwTexRecycle::Key> keyOf;
	std::unordered_set<std::uint32_t> seen;
	WwTexRecycle::List list;
	std::int64_t resident = 0, budget = 0, made = 0, refilled = 0, evicted = 0, reloads = 0, highWater = 0;
	std::uint32_t tick = 0;
	std::uint32_t paintTick[4] = { 0, 0, 0, 0 };
	const int capMb = qEnvironmentVariableIsSet( "WW_CELL_STREAM_TEXRECYCLE" ) ? qEnvironmentVariableIntValue( "WW_CELL_STREAM_TEXRECYCLE" ) : -1;
	double res0Mb = 0;
	auto evict = [&]( std::int64_t incoming ) {
		std::vector<WwTexPool::Entry> e;
		std::vector<std::uint32_t> names;
		for ( auto & kv : tex ) {
			if ( !kv.second.id || kv.second.pinned )
				continue;
			e.push_back( { int( names.size() ), kv.second.useTick, kv.second.bytes } );
			names.push_back( kv.first );
		}
		for ( const int i : WwTexPool::pick( std::move( e ), resident + incoming, budget, paintTick[tick & 3U] ) ) {
			Tex & t = tex[names[size_t( i )]];
			resident -= t.bytes;
			evicted++;
			std::vector<std::uint32_t> del;
			if ( !recycle )
				del.push_back( t.id );
			else
				del = list.put( keyOf[t.id], t.id, t.bytes, WwTexRecycle::capBytes( budget, capMb ) );
			for ( std::uint32_t id : del ) {
				GLuint g = id;
				f->glDeleteTextures( 1, &g );
				keyOf.erase( g );
			}
			t.id = 0;
		}
	};
	auto load = [&]( std::uint32_t name, Tex & t, bool pumped ) {
		const Shape s = shapeOf( name );
		if ( room && resident + s.bytes > budget )
			evict( s.bytes );	// room first: the driver hands the freed bytes to this upload
		const WwTexRecycle::Key k{ GL_TEXTURE_2D, s.fmt, s.w, s.w, s.levels };
		GLuint id = recycle ? list.take( k ) : 0;
		const bool reuse = id != 0;
		if ( !id )
			f->glGenTextures( 1, &id );
		f->glBindTexture( GL_TEXTURE_2D, id );
		if ( !reuse ) {
			f->glTexStorage2D( GL_TEXTURE_2D, s.levels, s.fmt, s.w, s.w );
			keyOf[id] = k;
			made++;
		} else {
			refilled++;
		}
		int d = s.w;
		for ( int l = 0; l < s.levels; l++, d = std::max( 1, d / 2 ) ) {
			const GLsizei b = GLsizei( std::max( 1, ( d + 3 ) / 4 ) * std::max( 1, ( d + 3 ) / 4 ) * s.block );
			f->glCompressedTexSubImage2D( GL_TEXTURE_2D, l, 0, 0, d, d, s.fmt, b, blob.data() + ( name % 64U ) * 4096U );
		}
		t.id = id;
		t.bytes = s.bytes;
		t.useTick = tick;
		t.pinned = room && pumped;
		resident += s.bytes;
		highWater = std::max( highWater, resident );
		reloads += seen.insert( name ).second ? 0 : 1;
	};
	double p0 = 0, plast = 0, peak = 0;
	std::int64_t res0 = 0;
	std::vector<std::uint32_t> set = windowSet( path[0][0], path[0][1] );
	{
		std::int64_t ws = 0;
		for ( std::uint32_t n : set )
			ws += shapeOf( n ).bytes;
		budget = ws * 89 / 100;
		std::printf( "mode %s: working set %zu textures %.0f MB, budget %.0f MB\n", mode.c_str(), set.size(),
			double( ws ) / 1048576.0, double( budget ) / 1048576.0 );
	}
	tick = 1;
	for ( std::uint32_t n : set )	// the cell open loads the whole window
		load( n, tex[n], false );
	std::vector<std::uint32_t> drawn = windowSet( path[0][0], path[0][1], 1 );
	for ( int step = 0; step <= 20; step++ ) {
		std::vector<std::uint32_t> ahead;	// the next centre's textures not in this window: what the pump reads ahead
		if ( step < 20 ) {
			const std::vector<std::uint32_t> next = windowSet( path[step + 1][0], path[step + 1][1] );
			std::set_difference( next.begin(), next.end(), set.begin(), set.end(), std::back_inserter( ahead ) );
		}
		size_t pumped = 0;
		for ( int p = 0; p < 36; p++ ) {
			tick++;
			paintTick[tick & 3U] = tick;
			for ( std::uint32_t n : drawn ) {
				Tex & t = tex[n];
				if ( !t.id )
					load( n, t, false );
				t.useTick = tick;
				t.pinned = false;
			}
			if ( p >= 15 && p < 24 ) {	// the pump: a ninth of the read-ahead a paint
				const size_t end = std::min( ahead.size(), ( ahead.size() * size_t( p - 14 ) + 8 ) / 9 );
				for ( ; pumped < end; pumped++ ) {
					Tex & t = tex[ahead[pumped]];
					if ( !t.id )
						load( ahead[pumped], t, true );
				}
			}
			if ( tick % 12U == 0 )
				evict( 0 );	// the 2 s pool
			if ( p == 24 && step < 20 ) {	// the window moves
				set = windowSet( path[step + 1][0], path[step + 1][1] );
				drawn = windowSet( path[step + 1][0], path[step + 1][1], 1 );
			}
			if ( p == 14 ) {	// the dump's step line: the window settled (a pool ran), the next ring not read yet
				f->glFinish();
				const double mb = privateMb();
				if ( step == 0 ) {
					p0 = mb;
					res0 = resident;
				}
				plast = mb;
				peak = std::max( peak, mb );
				// the driver's residue: private past what is resident and what the recycler holds
				const double residue = mb - double( resident + list.heldBytes() ) / 1048576.0;
				if ( step == 0 )
					res0Mb = residue;
				std::printf( "step %2d centre %d,%d private %.0f MB resident %.0f MB high %.0f MB list %.0f MB residue %+.0f MB made %lld refilled %lld evicted %lld reloads %lld\n",
					step, path[step][0], path[step][1], mb, double( resident ) / 1048576.0, double( highWater ) / 1048576.0,
					double( list.heldBytes() ) / 1048576.0, residue - res0Mb, (long long) made, (long long) refilled,
					(long long) evicted, (long long) reloads );
			}
		}
	}
	const double ratio = plast / p0;
	std::printf( "%s: crossing 0 %.0f MB, crossing 20 %.0f MB (x%.3f), peak %.0f MB, resident %.0f -> %.0f MB high %.0f, made %lld reloads %lld\n",
		mode.c_str(), p0, plast, ratio, peak, double( res0 ) / 1048576.0, double( resident ) / 1048576.0,
		double( highWater ) / 1048576.0, (long long) made, (long long) reloads );
	const bool flat = ratio <= 1.05;
	std::printf( "%s\n", flat ? "FLAT (<= 5 %)" : "GREW (> 5 %)" );
	return flat ? 0 : 1;
}

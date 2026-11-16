// Lane VKFAR3 (2026-11-14, row G3): the far ground's GL uploads with the Vulkan view off are the rung's (c7e7d1a2), call for
// call. fargroundoff_prep.py cuts struct Gpu / Doc and the upload / free functions out of both cellfarground.cpp texts
// (pass()'s upload lines are uploadInputs on this tree, wrapped verbatim from the rung's pass()); both compile here against
// a GL that records every call (name, arguments, an FNV-1a of the pixels / buffer bytes) and a fake present device.
//  off arm: the same frames on both (lookup, lookup again, the sheet's base, freeDecals, freeKit, freeAll as the graveyard
//    does, three documents); the traces must match entry for entry, and the fake device sees no call at all.
//    Comparator reds first: one flipped argument, one removed call. Subject red: this tree with share on must differ.
//  route arm (this tree only): view on, a frame with no view (keep), off, on, a present shutdown, a refusal, no key slot;
//    the shared images move only when the view changes by its switch. Red: keep = share (the old reshare) churns.
// Last line `FARGROUNDOFF GATE OK` or `FARGROUNDOFF GATE FAILED`.
#include <windows.h>
#include <GL/gl.h>
#include <GL/glext.h>

#include <algorithm>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <random>
#include <string>
#include <vector>

using qint64 = long long;
using quint64 = unsigned long long;
using quint32 = std::uint32_t;
using quint16 = std::uint16_t;
using qsizetype = std::ptrdiff_t;
struct QOpenGLContext;
struct QString
{
};
struct QStringList
{
	std::vector<std::string> v;
	qsizetype size() const { return qsizetype( v.size() ); }
};
namespace FgSched
{
struct Sched
{
	std::vector<int> cells;
};
}

namespace NifSkopeOpenGLContext
{
std::uint64_t fnv( const void * p, size_t n )
{
	std::uint64_t h = 1469598103934665603ull;
	const unsigned char * b = static_cast<const unsigned char *>( p );
	for ( size_t i = 0; p && i < n; i++ )
		h = ( h ^ b[i] ) * 1099511628211ull;
	return p ? h : 0;
}

struct GLFunctions
{
	std::vector<std::string> log;
	GLuint next = 1;
	void rec( const char * fmt, ... )
	{
		char b[512];
		va_list a;
		va_start( a, fmt );
		std::vsnprintf( b, sizeof b, fmt, a );
		va_end( a );
		log.emplace_back( b );
	}
	static size_t px( GLenum fmt, GLenum type )
	{
		const size_t c = fmt == GL_RED ? 1 : fmt == GL_RG ? 2 : 4;
		return c * ( type == GL_UNSIGNED_BYTE ? 1 : 4 );
	}
	void glGenTextures( GLsizei n, GLuint * t )
	{
		for ( GLsizei i = 0; i < n; i++ )
			t[i] = next++;
		rec( "glGenTextures %d -> %u", n, t[0] );
	}
	void glGenBuffers( GLsizei n, GLuint * t )
	{
		for ( GLsizei i = 0; i < n; i++ )
			t[i] = next++;
		rec( "glGenBuffers %d -> %u", n, t[0] );
	}
	void glDeleteTextures( GLsizei n, const GLuint * t ) { rec( "glDeleteTextures %d %u", n, t[0] ); }
	void glDeleteBuffers( GLsizei n, const GLuint * t ) { rec( "glDeleteBuffers %d %u", n, t[0] ); }
	void glDeleteFramebuffers( GLsizei n, const GLuint * t ) { rec( "glDeleteFramebuffers %d %u", n, t[0] ); }
	void glDeleteSamplers( GLsizei n, const GLuint * t ) { rec( "glDeleteSamplers %d %u", n, t[0] ); }	// VKGROUND8: paintJobs' ltexSmp
	void glBindTexture( GLenum a, GLuint t ) { rec( "glBindTexture %x %u", a, t ); }
	void glBindBuffer( GLenum a, GLuint t ) { rec( "glBindBuffer %x %u", a, t ); }
	void glPixelStorei( GLenum a, GLint v ) { rec( "glPixelStorei %x %d", a, v ); }
	void glGenerateMipmap( GLenum a ) { rec( "glGenerateMipmap %x", a ); }
	void glTexParameteri( GLenum a, GLenum p, GLint v ) { rec( "glTexParameteri %x %x %d", a, p, v ); }
	void glTexParameterf( GLenum a, GLenum p, GLfloat v ) { rec( "glTexParameterf %x %x %g", a, p, double( v ) ); }
	void glTexImage2D( GLenum a, GLint l, GLint ifmt, GLsizei w, GLsizei h, GLint b, GLenum fmt, GLenum type, const void * d )
	{
		rec( "glTexImage2D %x %d %x %dx%d %d %x %x %016llx", a, l, ifmt, w, h, b, fmt, type,
			(unsigned long long) fnv( d, size_t( w ) * h * px( fmt, type ) ) );
	}
	void glTexSubImage2D( GLenum a, GLint l, GLint x, GLint y, GLsizei w, GLsizei h, GLenum fmt, GLenum type, const void * d )
	{
		rec( "glTexSubImage2D %x %d %d,%d %dx%d %x %x %016llx", a, l, x, y, w, h, fmt, type,
			(unsigned long long) fnv( d, size_t( w ) * h * px( fmt, type ) ) );
	}
	void glTexImage3D( GLenum a, GLint l, GLint ifmt, GLsizei w, GLsizei h, GLsizei n, GLint b, GLenum fmt, GLenum type, const void * d )
	{
		rec( "glTexImage3D %x %d %x %dx%dx%d %d %x %x %016llx", a, l, ifmt, w, h, n, b, fmt, type,
			(unsigned long long) fnv( d, size_t( w ) * h * n * px( fmt, type ) ) );
	}
	void glTexSubImage3D( GLenum a, GLint l, GLint x, GLint y, GLint z, GLsizei w, GLsizei h, GLsizei n, GLenum fmt, GLenum type,
		const void * d )
	{
		rec( "glTexSubImage3D %x %d %d,%d,%d %dx%dx%d %x %x %016llx", a, l, x, y, z, w, h, n, fmt, type,
			(unsigned long long) fnv( d, size_t( w ) * h * n * px( fmt, type ) ) );
	}
	void glBufferData( GLenum a, GLsizeiptr n, const void * d, GLenum u )
	{
		rec( "glBufferData %x %lld %x %016llx", a, (long long) n, u, (unsigned long long) fnv( d, size_t( n ) ) );
	}
	void glTexBuffer( GLenum a, GLenum f, GLuint b ) { rec( "glTexBuffer %x %x %u", a, f, b ); }
};
}  // namespace NifSkopeOpenGLContext

// the present device, faked: names from 50000, a refusal on ask, a shutdown that forgets every target
namespace WwVkPresent
{
struct Fake
{
	std::map<std::uintptr_t, unsigned> live;
	int made = 0, dropped = 0, fails = 0, isCalls = 0;
	bool refuse = false;
	unsigned next = 50000;
};
Fake & fk()
{
	static Fake f;
	return f;
}
bool sharedTarget( std::uintptr_t key, int, int, int, int, unsigned, unsigned & tex, std::string & why, int = 1, int = 1 )
{
	if ( fk().refuse ) {
		why = "refused (fake)";
		fk().fails++;
		return false;
	}
	tex = fk().next++;
	fk().live[key] = tex;
	fk().made++;
	return true;
}
bool sharedTargetIs( std::uintptr_t key, unsigned tex )
{
	fk().isCalls++;
	const auto it = fk().live.find( key );
	return it != fk().live.end() && tex && it->second == tex;
}
void sharedTargetDrop( std::uintptr_t key )
{
	if ( fk().live.erase( key ) )
		fk().dropped++;
}
// lane VKGROUND8: VKGROUND5's far twins (slots 4, 5, the page pools) on the same fake: one table of keys, made and dropped
bool farTwin( std::uintptr_t key, int type, int w, int h, int layers, unsigned glFmt, unsigned & tex, std::string & why, int levels = 1 )
{
	return sharedTarget( key, type, w, h, layers, glFmt, tex, why, levels );
}
void farTwinDrop( std::uintptr_t key )
{
	sharedTargetDrop( key );
}
bool farTwinIs( std::uintptr_t key, unsigned tex )
{
	return sharedTargetIs( key, tex );
}
}  // namespace WwVkPresent

#include "../../src/features/far-ground/cellfgcut.h"	// lane VKFAR14: Doc's cut (this tree)

namespace Rung
{
#include "rung_tu.inc"
}
namespace Head
{
#include "head_tu.inc"
}

using Fn = NifSkopeOpenGLContext::GLFunctions;

template <class D>
void fill( D & d, int seed, int layers, bool atlas, bool decals )
{
	std::mt19937 rng { unsigned( seed ) };
	d.nx = 33 + seed;
	d.ny = 17 + 2 * seed;
	d.grid.resize( size_t( d.nx ) * d.ny * 4 );
	for ( auto & w : d.grid )
		w = rng();
	d.height.assign( size_t( d.nx ) * d.ny, 1.5f );
	d.ltexPaths.v.assign( size_t( layers ), "x" );
	const size_t layer = size_t( Head::kLtexSide ) * Head::kLtexSide;
	d.ltexPix.resize( layer * size_t( std::max( 1, layers ) ) );
	for ( size_t i = 0; i < d.ltexPix.size(); i += 97 )
		d.ltexPix[i] = rng();
	if ( atlas ) {
		d.atlasW = 64;
		d.atlasH = 32 + seed;
		d.atlasPix.resize( size_t( d.atlasW ) * d.atlasH );
		for ( auto & p : d.atlasPix )
			p = rng();
	}
	if ( decals ) {
		d.recs.assign( 40, 0.25f * float( seed ) );
		d.binTex.assign( 8, 3.0f );
		d.ids.assign( 4, 1.0f );
	}
}

// the frames a document sees; up(lookup) is pass()'s uploadInputs, the rest the free paths
template <class D, class Up, class FreeDecals, class FreeKit, class FreeAll>
void frames( Fn & fn, D & d, Up up, FreeDecals fd, FreeKit fk, FreeAll fa )
{
	up( true );
	up( true );
	fa( d.gpu );	// pass(): want 0 (the baked sheet), then the lookup again
	up( true );
	fd( d.gpu );
	up( false );	// the old pages' base only
	up( true );
	fk( d.gpu );	// the settle (cached mode's kit)
	up( true );
	auto grave = d.gpu;	// forget(): the graveyard frees a copy
	fa( grave );
	d.gpu = decltype( d.gpu )();
	up( true );
	fa( d.gpu );
	(void) fn;
}

std::vector<std::string> runRung( int & docs )
{
	Fn fn;
	docs = 0;
	const int cfg[3][4] = { { 1, 3, 1, 1 }, { 2, 0, 0, 1 }, { 3, 2, 1, 0 } };
	for ( const auto & c : cfg ) {
		Rung::Doc d;
		fill( d, c[0], c[1], c[2] != 0, c[3] != 0 );
		frames( fn, d, [&]( bool lookup ) { Rung::uploadInputs( &fn, d, lookup, false, false ); },
			[&]( Rung::Gpu & g ) { Rung::freeDecals( &fn, g ); }, [&]( Rung::Gpu & g ) { Rung::freeKit( &fn, g ); },
			[&]( Rung::Gpu & g ) { Rung::freeAll( &fn, g ); } );
		docs++;
	}
	return fn.log;
}

std::vector<std::string> runHead( bool share )
{
	Fn fn;
	const int cfg[3][4] = { { 1, 3, 1, 1 }, { 2, 0, 0, 1 }, { 3, 2, 1, 0 } };
	for ( const auto & c : cfg ) {
		Head::Doc d;
		fill( d, c[0], c[1], c[2] != 0, c[3] != 0 );
		frames( fn, d, [&]( bool lookup ) { Head::uploadInputs( &fn, d, lookup, lookup && share, lookup && share ); },
			[&]( Head::Gpu & g ) { Head::freeDecals( &fn, g ); }, [&]( Head::Gpu & g ) { Head::freeKit( &fn, g ); },
			[&]( Head::Gpu & g ) { Head::freeAll( &fn, g ); } );
	}
	return fn.log;
}

// entries compared, entries that differ (by position), and the first difference
struct Cmp
{
	size_t a = 0, b = 0, same = 0, differ = 0;
	std::string first;
};
Cmp compare( const std::vector<std::string> & a, const std::vector<std::string> & b )
{
	Cmp c;
	c.a = a.size();
	c.b = b.size();
	for ( size_t i = 0; i < std::max( a.size(), b.size() ); i++ ) {
		const bool eq = i < a.size() && i < b.size() && a[i] == b[i];
		eq ? c.same++ : c.differ++;
		if ( !eq && c.first.empty() )
			c.first = "#" + std::to_string( i ) + ": [" + ( i < a.size() ? a[i] : "-" ) + "] vs [" + ( i < b.size() ? b[i] : "-" ) + "]";
	}
	return c;
}
void say( const char * what, const Cmp & c )
{
	std::printf( "FARGROUNDOFF %s: %zu vs %zu calls, %zu same, %zu differ%s%s\n", what, c.a, c.b, c.same, c.differ,
		c.first.empty() ? "" : ", first ", c.first.c_str() );
}

int main()
{
	bool ok = true;
	int docs = 0;
	const std::vector<std::string> rung = runRung( docs );
	WwVkPresent::fk() = WwVkPresent::Fake();
	const std::vector<std::string> head = runHead( false );
	const WwVkPresent::Fake offFake = WwVkPresent::fk();
	int tex = 0, sub = 0;
	for ( const auto & l : rung ) {
		tex += l.rfind( "glTexImage", 0 ) == 0;
		sub += l.rfind( "glTexSub", 0 ) == 0;
	}
	std::printf( "FARGROUNDOFF off arm: %d documents, %zu rung calls (%d texture images, %d sub-images)\n", docs, rung.size(), tex, sub );

	// comparator reds first: a flipped argument, a removed call
	std::vector<std::string> flip = rung, drop = rung;
	flip[flip.size() / 2].back() ^= 1;
	drop.erase( drop.begin() + std::ptrdiff_t( drop.size() / 3 ) );
	const Cmp rf = compare( rung, flip ), rd = compare( rung, drop );
	say( "red flipped argument", rf );
	say( "red removed call", rd );
	if ( rf.differ != 1 || rd.differ == 0 ) {
		std::printf( "FARGROUNDOFF comparator blind to its reds\n" );
		ok = false;
	}
	const Cmp off = compare( rung, head );
	say( "rung vs this tree, view off", off );
	std::printf( "FARGROUNDOFF off arm present device: %d made, %d dropped, %d asked, %d refused\n", offFake.made, offFake.dropped,
		offFake.isCalls, offFake.fails );
	if ( off.differ || rung.size() < 100 || offFake.made || offFake.dropped || offFake.isCalls || offFake.fails )
		ok = false;
	WwVkPresent::fk() = WwVkPresent::Fake();
	const Cmp on = compare( rung, runHead( true ) );
	say( "red: this tree with share on", on );
	std::printf( "FARGROUNDOFF red share on: %d made, %d dropped\n", WwVkPresent::fk().made, WwVkPresent::fk().dropped );
	if ( !on.differ || !WwVkPresent::fk().made )
		ok = false;

	// route arm: one document, frame by frame; keepOld = the old reshare (keep = share) for the red
	for ( int arm = 0; arm < 2; arm++ ) {
		const bool keepOld = arm == 1;
		WwVkPresent::Fake & F = WwVkPresent::fk();
		F = WwVkPresent::Fake();
		Fn fn;
		Head::Doc d;
		fill( d, 1, 3, true, true );
		int bad = 0, step = 0;
		auto frame = [&]( const char * what, bool share, bool keep, int made, int dropped, int glDel, unsigned shared ) {
			const int m0 = F.made, d0 = F.dropped;
			const size_t l0 = fn.log.size();
			Head::uploadInputs( &fn, d, true, share, keepOld ? share : keep );
			int del = 0;
			for ( size_t i = l0; i < fn.log.size(); i++ )
				del += fn.log[i].rfind( "glDeleteTextures", 0 ) == 0;
			const bool good = F.made - m0 == made && F.dropped - d0 == dropped && del == glDel && unsigned( d.gpu.vkShared ) == shared
				&& d.gpu.grid && d.gpu.ltex && d.gpu.atlas;
			if ( !good )
				bad++;
			if ( !keepOld || !good )
				std::printf( "FARGROUNDOFF route%s %d %s: %d made, %d dropped, %d GL deletes, shared bits %d, slot %d%s\n",
					keepOld ? " red" : "", ++step, what, F.made - m0, F.dropped - d0, del, d.gpu.vkShared, d.gpu.vkSlot,
					good ? "" : " (NOT as expected)" );
		};
		frame( "view on", true, true, 3, 0, 0, 7 );
		frame( "no view this frame (resize, grab)", false, true, 0, 0, 0, 7 );
		frame( "view on again", true, true, 0, 0, 0, 7 );
		frame( "view off by its switch", false, false, 0, 3, 0, 0 );
		frame( "view on", true, true, 3, 0, 3, 7 );
		F.live.clear();	// a present shutdown took the three
		frame( "after a present shutdown", true, true, 3, 0, 0, 7 );
		frame( "view off", false, false, 0, 3, 0, 0 );
		F.refuse = true;
		frame( "view on, the device refuses (GL's own made again)", true, true, 0, 0, 3, 0 );
		const int asked = F.fails;
		frame( "refused before: no ask again", true, true, 0, 0, 0, 0 );
		if ( F.fails != asked || asked != 3 )
			bad++;
		F.refuse = false;
		frame( "view off clears the refusals", false, false, 0, 0, 0, 0 );
		for ( bool & u : Head::g_vkUsed )
			u = true;
		frame( "view on, no free key slot (GL's own made again)", true, true, 0, 0, 3, 0 );
		for ( bool & u : Head::g_vkUsed )
			u = false;
		frame( "view off", false, false, 0, 0, 0, 0 );
		frame( "view on", true, true, 3, 0, 3, 7 );
		Head::freeAll( &fn, d.gpu );
		int used = 0;
		for ( bool u : Head::g_vkUsed )
			used += u;
		std::printf( "FARGROUNDOFF route%s end: %zu live targets, %d key slots used, %d frames off their expectation\n",
			keepOld ? " red" : "", F.live.size(), used, bad );
		if ( !keepOld && ( bad || !F.live.empty() || used ) )
			ok = false;
		if ( keepOld && !bad ) {
			std::printf( "FARGROUNDOFF route red: the old reshare did not churn (the route arm is blind)\n" );
			ok = false;
		}
	}
	std::printf( ok ? "FARGROUNDOFF GATE OK\n" : "FARGROUNDOFF GATE FAILED\n" );
	return ok ? 0 : 1;
}

// Lane VKAIR7 (2026-11-16): the air's sharedTarget keys survive a move of their QHash value (cellvolairkeys.h).
// cellvolair.cpp keeps its GL state in QHash<const void *, VaGpu> (vaGpus(), keyed by renderer). It keyed the shared
// inject / integ / tile / depth targets by &g.<name>, an address inside the value; Qt6 moves the value when the hash grows.
// The harness runs the app's ensure logic (cellvolair.cpp tile site: remake when !sharedTargetIs( key, name ), drop only
// the CURRENT key) on the shipped vkpresent.cpp (a real present device) with the shipped VaVkKeys, in a real QHash:
// make the 4 targets of entry 1, insert entries until entry 1's value address moves, ensure again, count shareds().
// Bar (set before the first run, never widened): the value moved (else TESTS NOTHING); after the move 0 re-makes,
// live shared targets == 4, each key still sharedTargetIs; GL raised no error.
// Red (must FAIL): keys = the member addresses (the old route): after the move the count grows past 4.
// tests/vkframe/airkeys.sh also checks the wire: cellvolair.cpp has no `uintptr_t>( &g.` key left and uses vkKeys.at.
// Last line `AIRKEYS GATE OK` or `AIRKEYS GATE FAILED`.
#define main fxview_main
#include "fxview.cpp"
#undef main

#include <QHash>
#include <algorithm>
#include "features/air/cellvolairkeys.h"

namespace akeys
{
using namespace fv;

struct Val
{
	unsigned tex[VaVkKeys::kSlots] = {};
	bool shared[VaVkKeys::kSlots] = {};
	float pad[64] = {};	// a value of some size, as VaGpu
	VaVkKeys keys;
};

std::uintptr_t keyOf( Val & v, int s, bool red )
{
	return red ? reinterpret_cast<std::uintptr_t>( &v.tex[s] ) : v.keys.at( s );
}

// cellvolair.cpp's ensure, per target; returns the number made
int ensure( Val & v, bool red )
{
	int made = 0;
	for ( int s = 0; s < VaVkKeys::kSlots; s++ ) {
		const std::uintptr_t key = keyOf( v, s, red );
		if ( v.tex[s] && !( v.shared[s] && !WwVkPresent::sharedTargetIs( key, v.tex[s] ) ) )
			continue;
		if ( v.shared[s] ) {
			if ( WwVkPresent::sharedTargetIs( key, v.tex[s] ) )
				WwVkPresent::sharedTargetDrop( key );
			v.tex[s] = 0;
			v.shared[s] = false;
		}
		unsigned t = 0;
		std::string why;
		v.shared[s] = WwVkPresent::sharedTarget( key, 0, 16, 8, 1, 0x8230, t, why );
		if ( v.shared[s] ) {
			v.tex[s] = t;
			made++;
		} else
			std::printf( "AIRKEYS slot %d refused: %s\n", s, why.c_str() );
	}
	return made;
}

// one arm; returns true when it meets the bar
bool arm( bool red )
{
	const char * name = red ? "red (member-address keys)" : "green (VaVkKeys)";
	const size_t before = shareds().size();
	std::vector<std::uintptr_t> theirs;
	for ( auto & kv : shareds() )
		theirs.push_back( kv.first );
	QHash<const void *, Val> h;
	const void * k1 = reinterpret_cast<const void *>( std::uintptr_t( 1 ) );
	const int made0 = ensure( h[k1], red );
	const std::uintptr_t addr0 = reinterpret_cast<std::uintptr_t>( &h[k1] );
	std::uintptr_t keys0[VaVkKeys::kSlots];
	for ( int s = 0; s < VaVkKeys::kSlots; s++ )
		keys0[s] = keyOf( h[k1], s, red );
	int inserts = 0;
	while ( reinterpret_cast<std::uintptr_t>( &h[k1] ) == addr0 && inserts < 100000 ) {
		inserts++;
		h[reinterpret_cast<const void *>( std::uintptr_t( 1 + inserts ) )];
	}
	const bool moved = reinterpret_cast<std::uintptr_t>( &h[k1] ) != addr0;
	const int made1 = ensure( h[k1], red );
	int keysKept = 0, isShared = 0;
	for ( int s = 0; s < VaVkKeys::kSlots; s++ ) {
		keysKept += keyOf( h[k1], s, red ) == keys0[s] ? 1 : 0;
		isShared += WwVkPresent::sharedTargetIs( keyOf( h[k1], s, red ), h[k1].tex[s] ) ? 1 : 0;
	}
	const size_t live = shareds().size() - before;
	const GLenum err = glGetError();
	const bool pass = made0 == 4 && moved && made1 == 0 && live == 4 && keysKept == 4 && isShared == 4 && err == GL_NO_ERROR;
	std::printf( "AIRKEYS %s: made %d, value moved %s after %d inserts (0x%llx -> 0x%llx), re-made after the move %d, keys kept %d of 4, "
				 "shared %d of 4, live shared targets %zu (bar 4), GL error 0x%x: %s\n",
		name, made0, moved ? "yes" : "NO", inserts, (unsigned long long)addr0, (unsigned long long)reinterpret_cast<std::uintptr_t>( &h[k1] ),
		made1, keysKept, isShared, live, unsigned( err ), pass ? "PASS" : "FAIL" );
	if ( !moved )
		std::printf( "AIRKEYS %s: TESTS NOTHING (the value never moved)\n", name );
	// drop everything this arm made (the red's old keys included)
	std::vector<std::uintptr_t> mine;
	for ( auto & kv : shareds() )
		if ( std::find( theirs.begin(), theirs.end(), kv.first ) == theirs.end() )
			mine.push_back( kv.first );
	for ( std::uintptr_t k : mine )
		WwVkPresent::sharedTargetDrop( k );
	return pass && moved;
}

}  // namespace akeys

int main( int argc, char ** argv )
{
	using namespace akeys;
	(void)argc;
	(void)argv;
	setvbuf( stdout, nullptr, _IONBF, 0 );
	auto fatal = []( const std::string & m ) {
		std::printf( "AIRKEYS %s\nAIRKEYS GATE FAILED\n", m.c_str() );
		return 2;
	};
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwairkeys";
	RegisterClassA( &wc );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwairkeys", "air keys harness", WS_OVERLAPPEDWINDOW, 40, 40, 160, 90, nullptr,
		nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( dc, ChoosePixelFormat( dc, &pfd ), &pfd );
	HGLRC glrc = wglCreateContext( dc );
	wglMakeCurrent( dc, glrc );
	pump();
	if ( !loadAll() )
		return fatal( "GL entry points" );
	std::printf( "AIRKEYS GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	std::string why;
	if ( !WwVkPresent::viewBegin( 160, 90, 1, glProc, why ) )
		return fatal( "the view: " + why );
	while ( glGetError() != GL_NO_ERROR ) {
	}
	const bool green = arm( false );
	const bool red = arm( true );
	std::printf( "AIRKEYS red %s\n", red ? "PASSED (TESTS NOTHING: it must FAIL)" : "FAIL (as it must)" );
	const bool ok = green && !red;
	std::printf( "AIRKEYS GATE %s\n", ok ? "OK" : "FAILED" );
	return ok ? 0 : 1;
}

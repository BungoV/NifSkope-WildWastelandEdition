// Lane VKBUF1 (2026-11-16): the RT GI's texel buffers on the present device by name (vkrtbuf.inc: rtBufShare / rtBufCopy /
// rtBufOwner / rtBufOf, the functions rtddgicell.cpp calls). Two owners as the app has them:
//   cellDdgiAtlas RGBA32F: GL copies the cascades' atlas into the shared buffer's GL name (combinedUpdate's copies);
//   cellRestir RG32F: GL's own buffer (bakert's output in the app) copied into the shared buffer by the owner's callback,
//   once a step, when a consumer looks it up.
// Bar (set before the first run, never widened): after a GL write, after a resize (new GL names, rtBufGen moved) and after a
// drop + remake (the lookup refuses while dropped), the present device's bytes (farTexelBufferRead through the looked-up key)
// == GL's bytes, byte for byte; GL's own read of the shared buffer == the same bytes; the view's size == the owner's size;
// compared count > 0; GL's texture view names the buffer and the format. An unknown name refuses.
// Reds (each must FAIL): stale (a GL write the owner's copy skipped), wrong size (the shared buffer one texel short),
// unshared (WW_VKVIEW_RTBUF_RED=1: the lookup refuses, nothing compared). Last line `RTBUF GATE OK` or `RTBUF GATE FAILED`.
#define main fxview_main
#include "fxview.cpp"
#undef main

#include <cstdlib>
#include <random>

namespace rtbufv
{
using namespace fv;
using NBufData = void( WINAPI * )( GLuint, std::intptr_t, const void *, GLenum );
using NBufSub = void( WINAPI * )( GLuint, std::intptr_t, std::intptr_t, const void * );
using NBufGet = void( WINAPI * )( GLuint, std::intptr_t, std::intptr_t, void * );
using NBufCopy = void( WINAPI * )( GLuint, GLuint, std::intptr_t, std::intptr_t, std::intptr_t );
using CreateBufs = void( WINAPI * )( GLsizei, GLuint * );
using DelBufs = void( WINAPI * )( GLsizei, const GLuint * );
NBufData nData = nullptr;
NBufSub nSub = nullptr;
NBufGet nGet = nullptr;
NBufCopy nCopy = nullptr;
CreateBufs createBufs = nullptr;
DelBufs delBufs = nullptr;

struct Own
{
	int slot;
	unsigned fmt;
	size_t texel;
	size_t bytes = 0;
	unsigned src = 0;           // GL's own buffer (the cascades' atlas / bakert's ratio)
	unsigned buf = 0, tex = 0;  // the shared buffer's GL names
	std::uint64_t step = 0, copied = 0;
	bool copyOn = true;         // red stale: the owner's copy skipped
	std::vector<std::uint8_t> data;
};

size_t differ( const std::vector<std::uint8_t> & a, const std::vector<std::uint8_t> & b )
{
	size_t n = a.size() > b.size() ? a.size() - b.size() : b.size() - a.size();
	for ( size_t i = 0; i < std::min( a.size(), b.size() ); i++ )
		n += a[i] != b[i] ? 1 : 0;
	return n;
}

bool share( Own & o, size_t bytes, std::string & why )
{
	if ( !WwVkPresent::rtBufShare( o.slot, o.fmt, bytes, o.buf, o.tex, why ) )
		return false;
	o.bytes = bytes;
	if ( o.src )
		delBufs( 1, &o.src );
	createBufs( 1, &o.src );
	nData( o.src, std::intptr_t( bytes ), nullptr, 0x88E8 /* GL_DYNAMIC_DRAW */ );
	return true;
}

// a GL write: new bytes into GL's own buffer; the atlas's owner copies them into the shared buffer at once (combinedUpdate),
// the ratio's owner at the next lookup (its callback)
void write( Own & o, std::mt19937 & rng )
{
	o.data.resize( o.bytes );
	for ( auto & x : o.data )
		x = std::uint8_t( rng() );
	nSub( o.src, 0, std::intptr_t( o.bytes ), o.data.data() );
	o.step++;
	if ( o.slot == WwVkPresent::kRtBufDdgiAtlas && o.copyOn ) {
		nCopy( o.src, o.buf, 0, 0, std::intptr_t( o.bytes ) );
		o.copied = o.step;
	}
}

struct Cmp
{
	bool looked = false;
	size_t compared = 0, bad = 0, glBad = 0;
	std::uint64_t viewBytes = 0;
	WwVkPresent::RtBuf r;
	std::string why;
};

Cmp compare( Own & o )
{
	Cmp c;
	c.looked = WwVkPresent::rtBufOf( WwVkPresent::rtBufName( o.slot ), c.r, c.why );
	if ( !c.looked )
		return c;
	glFinish();
	std::vector<std::uint8_t> got, glGot( size_t( c.r.bytes ) );
	if ( !WwVkPresent::farTexelBufferRead( c.r.key, got, c.why ) )
		return c;
	nGet( c.r.glBuf, 0, std::intptr_t( c.r.bytes ), glGot.data() );
	c.viewBytes = c.r.bytes;
	c.compared = std::min( got.size(), o.data.size() );
	c.bad = differ( got, o.data );
	c.glBad = differ( glGot, o.data );
	if ( c.r.view == 0 || c.r.texels * o.texel != c.r.bytes )
		c.bad += 1;
	return c;
}

std::string verdict( const Cmp & c, const Own & o )
{
	char b[512];
	if ( !c.looked )
		std::snprintf( b, sizeof b, "lookup refused (%s), compared 0 bytes", c.why.c_str() );
	else
		std::snprintf( b, sizeof b, "compared %zu bytes (view %llu bytes, %llu texels, owner %zu bytes), %zu differ, GL's own read %zu differ, gen %llu",
			c.compared, static_cast<unsigned long long>( c.viewBytes ), static_cast<unsigned long long>( c.r.texels ), o.bytes, c.bad,
			c.glBad, static_cast<unsigned long long>( c.r.gen ) );
	return b;
}

bool exact( const Cmp & c, const Own & o )
{
	return c.looked && c.compared > 0 && c.viewBytes == o.bytes && c.bad == 0 && c.glBad == 0;
}
}  // namespace rtbufv

int main( int argc, char ** argv )
{
	using namespace rtbufv;
	( void )argc;
	( void )argv;
	setvbuf( stdout, nullptr, _IONBF, 0 );
	auto fatal = []( const std::string & m ) {
		std::printf( "RTBUF %s\nRTBUF GATE FAILED\n", m.c_str() );
		return 2;
	};
	_putenv_s( "WW_VKVIEW_RTBUF_RED", "" );
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwrtbufview";
	RegisterClassA( &wc );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwrtbufview", "rt buffer harness", WS_OVERLAPPEDWINDOW, 40, 40, 160, 90, nullptr,
		nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( dc, ChoosePixelFormat( dc, &pfd ), &pfd );
	HGLRC glrc = wglCreateContext( dc );
	wglMakeCurrent( dc, glrc );
	pump();
	if ( !loadAll() || !load( nData, "glNamedBufferData" ) || !load( nSub, "glNamedBufferSubData" ) || !load( nGet, "glGetNamedBufferSubData" )
		|| !load( nCopy, "glCopyNamedBufferSubData" ) || !load( createBufs, "glCreateBuffers" ) || !load( delBufs, "glDeleteBuffers" ) )
		return fatal( "GL entry points" );
	std::printf( "RTBUF GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	std::string why;
	if ( !WwVkPresent::viewBegin( 160, 90, 1, glProc, why ) )
		return fatal( "the view: " + why );
	bool ok = true;
	std::mt19937 rng( 1116u );
	Own own[2] = { { WwVkPresent::kRtBufDdgiAtlas, 0x8814 /* GL_RGBA32F */, 16 }, { WwVkPresent::kRtBufRestir, 0x8230 /* GL_RG32F */, 8 } };
	const size_t size0[2] = { 40u * 193u * 16u, 160u * 90u * 8u }, size1[2] = { 64u * 193u * 16u, 200u * 120u * 8u };
	for ( Own & o : own ) {
		Own * p = &o;
		WwVkPresent::rtBufOwner( o.slot, [p]( std::string & w ) {
			if ( !p->buf ) {
				w = "the owner has no data";
				return false;
			}
			if ( p->slot == WwVkPresent::kRtBufRestir && p->copyOn && p->copied != p->step ) {
				if ( !WwVkPresent::rtBufCopy( p->slot, p->src, p->bytes, w ) )
					return false;
				p->copied = p->step;
			}
			return true;
		} );
	}
	{
		WwVkPresent::RtBuf r;
		const bool found = WwVkPresent::rtBufOf( "cellNope", r, why );
		std::printf( "RTBUF an unknown name: %s (%s)\n", found ? "FOUND (wrong)" : "refused", why.c_str() );
		ok = ok && !found;
	}
	for ( Own & o : own ) {
		const char * nm = WwVkPresent::rtBufName( o.slot );
		auto line = [&]( const char * label, bool green, const Cmp & c ) {
			const bool ex = exact( c, o );
			std::printf( "RTBUF %s %s: %s: %s\n", nm, label, verdict( c, o ).c_str(), green ? ( ex ? "EXACT" : "FAIL" ) : ( ex ? "EXACT (red did not bite)" : "FAIL (red bites)" ) );
			ok = ok && ( green ? ex : !ex );
		};
		if ( !share( o, size0[o.slot], why ) )
			return fatal( std::string( nm ) + ": " + why );
		{
			glBindTexture( 0x8C2A, o.tex );
			GLint store = 0, ifmt = 0;
			glGetTexLevelParameteriv( 0x8C2A, 0, 0x8C2D, &store );  // GL_TEXTURE_BUFFER_DATA_STORE_BINDING
			glGetTexLevelParameteriv( 0x8C2A, 0, GL_TEXTURE_INTERNAL_FORMAT, &ifmt );
			glBindTexture( 0x8C2A, 0 );
			const GLenum e = glGetError();
			const bool v = e == GL_NO_ERROR && GLuint( store ) == o.buf && unsigned( ifmt ) == o.fmt;
			std::printf( "RTBUF %s: GL view on buffer %d (want %u), format 0x%04X (want 0x%04X), GL error 0x%04X: %s\n", nm, store, o.buf,
				unsigned( ifmt ), o.fmt, unsigned( e ), v ? "OK" : "FAIL" );
			ok = ok && v;
		}
		write( o, rng );
		line( "after a GL write", true, compare( o ) );
		// a resize: new GL names, the generation moves
		const std::uint64_t gen0 = WwVkPresent::rtBufGen();
		const unsigned buf0 = o.buf;
		if ( !share( o, size1[o.slot], why ) )
			return fatal( std::string( nm ) + " resize: " + why );
		write( o, rng );
		{
			const Cmp c = compare( o );
			line( "after a resize", true, c );
			const bool moved = WwVkPresent::rtBufGen() != gen0 && o.buf != 0;
			std::printf( "RTBUF %s resize: gen %llu -> %llu, GL buffer %u -> %u: %s\n", nm, static_cast<unsigned long long>( gen0 ),
				static_cast<unsigned long long>( WwVkPresent::rtBufGen() ), buf0, o.buf, moved ? "OK" : "FAIL" );
			ok = ok && moved;
		}
		// a drop: the lookup refuses; a remake: the bytes again
		WwVkPresent::rtBufDrop( o.slot );
		{
			WwVkPresent::RtBuf r;
			const bool found = WwVkPresent::rtBufOf( nm, r, why );
			std::printf( "RTBUF %s dropped: the lookup %s (%s)\n", nm, found ? "FOUND (wrong)" : "refused", why.c_str() );
			ok = ok && !found;
		}
		o.buf = o.tex = 0;
		if ( !share( o, size0[o.slot], why ) )
			return fatal( std::string( nm ) + " remake: " + why );
		write( o, rng );
		line( "after a drop + remake", true, compare( o ) );
		// red stale: a GL write the owner's copy skipped
		o.copyOn = false;
		write( o, rng );
		line( "red stale (the owner's copy skipped)", false, compare( o ) );
		o.copyOn = true;
		write( o, rng );
		line( "after the stale red", true, compare( o ) );
		// red wrong size: the shared buffer one texel short of the owner's
		{
			const size_t full = o.bytes;
			unsigned b = 0, t = 0;
			if ( !WwVkPresent::rtBufShare( o.slot, o.fmt, full - o.texel, b, t, why ) )
				return fatal( std::string( nm ) + " short: " + why );
			o.buf = b;
			o.tex = t;
			nCopy( o.src, o.buf, 0, 0, std::intptr_t( full - o.texel ) );
			o.copied = o.step;
			line( "red wrong size (one texel short)", false, compare( o ) );
			share( o, full, why );
			write( o, rng );
			line( "after the size red", true, compare( o ) );
		}
		// red unshared: the app's red
		_putenv_s( "WW_VKVIEW_RTBUF_RED", "1" );
		write( o, rng );
		line( "red unshared (WW_VKVIEW_RTBUF_RED=1)", false, compare( o ) );
		_putenv_s( "WW_VKVIEW_RTBUF_RED", "" );
		line( "after the unshared red", true, compare( o ) );
	}
	std::printf( "RTBUF %s\n", WwVkPresent::rtBufLine().c_str() );
	for ( Own & o : own ) {
		WwVkPresent::rtBufOwner( o.slot, nullptr );
		WwVkPresent::rtBufDrop( o.slot );
		if ( o.src )
			delBufs( 1, &o.src );
	}
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	DestroyWindow( wnd );
	std::printf( "%s\n", ok ? "RTBUF GATE OK" : "RTBUF GATE FAILED" );
	return ok ? 0 : 1;
}

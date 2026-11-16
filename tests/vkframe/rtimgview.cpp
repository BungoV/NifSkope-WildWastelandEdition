// Lane VKBUF2 (2026-11-16): cellGiHalf / cellGiHalfKey (the screen GI and its key, RGBA32F) on the present device by name
// (vkrtbuf.h rtImg*), against GL's bytes. The owner here does what ao.cpp giShare / giHalfPass do: one FBO, the GI on
// attachment 0 (attached by rtImgShare), the key on attachment 1 (attached every pass), one GL draw into both.
// Bar (set before the first run, never widened), per slot and check: the lookup answers; its size is the view over giStep
// rounded UP (an odd view included); its format GL_RGBA32F; farSharedOf( glTex ) == its key and sharedTargetIs( key, glTex );
// the present device's read (twice, equal) == GL's own read of the shared name == GL's draw of the same program into a
// plain texture of the right size, byte for byte (compared count > 0, no sentinel texel left); rtImgGen() moved after a
// resize and after a drop + remake. Checks: after a write, a second write, a resize (160x90 -> 161x91 at step 2), a drop +
// remake, and after each red. An unknown name refuses.
// Reds (each must FAIL): dropped (the lookup after a drop), unshared (WW_VKVIEW_GIHALF_RED=1), not read this frame (the
// owner's fresh() false), wrong size (the size rounded down at 161x91), stale (cellGiHalf: a remake whose FBO still holds
// the old attachment, so GL's draw misses the shared image). Last line `RTIMG GATE OK` or `RTIMG GATE FAILED`.
#define main fxview_main
#include "fxview.cpp"
#undef main

#include <cmath>
#include <cstdlib>

namespace rtimgv
{
using namespace fv;
using ClearTexImage = void( WINAPI * )( GLuint, GLint, GLenum, GLenum, const void * );
using DelFbos = void( WINAPI * )( GLsizei, const GLuint * );
ClearTexImage clearTex = nullptr;
DelFbos delFbos = nullptr;
constexpr unsigned riRgba32f = 0x8814;
constexpr float riSentinel = -7.25f;

const char * kRiVs = R"(#version 430
void main()
{
	const vec2 p = vec2( ( gl_VertexID & 1 ) * 4.0 - 1.0, ( gl_VertexID & 2 ) * 2.0 - 1.0 );
	gl_Position = vec4( p, 0.0, 1.0 );
}
)";
// two outputs like cell_gihalf: the GI sample and its key, deterministic in the pixel and the seed
const char * kRiFs = R"(#version 430
uniform int riSeed;
layout( location = 0 ) out vec4 gi;
layout( location = 1 ) out vec4 giKey;
void main()
{
	const float s = float( riSeed );
	const vec2 c = gl_FragCoord.xy;
	gi = vec4( c.x * 0.37 + s, c.y * 1.13 - s, sin( c.x * c.y * 0.01 + s ), 1.0 / ( c.x + c.y + s ) );
	giKey = vec4( c.x * 3.0 - s * 0.5, -c.y * 0.25 + s, cos( c.x * 0.3 - c.y * 0.7 + s ), float( int( c.x ) ^ int( c.y ) ) );
}
)";

// the owner: AoGpu's giHalf.fbo, giHalf.tex, giKey and the two heap keys
struct Owner
{
	std::shared_ptr<char> kHalf = std::make_shared<char>( 0 ), kKey = std::make_shared<char>( 0 );
	unsigned fbo = 0, half = 0, key = 0;
	int w = 0, h = 0;
	bool ready = true;     // red not read: fresh() false
	bool attachOn = true;  // red stale: the remake not attached
	bool floorSize = false;  // red wrong size: the size rounded down
	std::uintptr_t k( int slot ) const
	{
		return reinterpret_cast<std::uintptr_t>( slot == WwVkPresent::kRtImgGiHalf ? kHalf.get() : kKey.get() );
	}
};

bool share( Owner & o, int W, int H, int step, std::string & why )
{
	const int w = o.floorSize ? W / step : WwVkPresent::rtImgSize( W, step ), h = o.floorSize ? H / step : WwVkPresent::rtImgSize( H, step );
	unsigned a = 0, b = 0;
	if ( !WwVkPresent::rtImgShare( WwVkPresent::kRtImgGiHalfKey, o.k( WwVkPresent::kRtImgGiHalfKey ), w, h, 0, 0, b, why ) )
		return false;
	if ( !WwVkPresent::rtImgShare( WwVkPresent::kRtImgGiHalf, o.k( WwVkPresent::kRtImgGiHalf ), w, h, o.attachOn ? o.fbo : 0, 0, a, why ) )
		return false;
	f.bindFramebuffer( fFb, 0 );
	o.half = a;
	o.key = b;
	o.w = w;
	o.h = h;
	return true;
}

void drop( Owner & o )
{
	WwVkPresent::rtImgDrop( WwVkPresent::kRtImgGiHalf, o.k( WwVkPresent::kRtImgGiHalf ) );
	WwVkPresent::rtImgDrop( WwVkPresent::kRtImgGiHalfKey, o.k( WwVkPresent::kRtImgGiHalfKey ) );
	o.half = o.key = 0;
}

// GL's pass: the key attached on 1 (giHalfPass does it every pass), two draw buffers, one draw at the owner's size
bool draw( GLuint prog, unsigned fbo, unsigned key, int w, int h, int seed )
{
	f.bindFramebuffer( fFb, fbo );
	f.framebufferTexture2D( fFb, fCol0 + 1, GL_TEXTURE_2D, key, 0 );
	const GLenum bufs[2] = { fCol0, fCol0 + 1 };
	f.drawBuffers( 2, bufs );
	const bool complete = f.checkFramebufferStatus( fFb ) == 0x8CD5;
	glViewport( 0, 0, w, h );
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_BLEND );
	u1i( prog, "riSeed", seed );
	glDrawArrays( GL_TRIANGLES, 0, 3 );
	f.bindFramebuffer( fFb, 0 );
	return complete;
}

std::vector<std::uint8_t> glRead( unsigned tex, int w, int h )
{
	std::vector<std::uint8_t> b( static_cast<size_t>( w ) * static_cast<size_t>( h ) * 16u, 0 );
	glBindTexture( GL_TEXTURE_2D, tex );
	GLint tw = 0, th = 0;
	glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &tw );
	glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &th );
	if ( tw == w && th == h )
		glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, b.data() );
	else
		b.assign( static_cast<size_t>( std::max( tw, 0 ) ) * static_cast<size_t>( std::max( th, 0 ) ) * 16u, 0xEE );
	glBindTexture( GL_TEXTURE_2D, 0 );
	return b;
}

size_t differ( const std::vector<std::uint8_t> & a, const std::vector<std::uint8_t> & b )
{
	size_t n = a.size() > b.size() ? a.size() - b.size() : b.size() - a.size();
	for ( size_t i = 0; i < std::min( a.size(), b.size() ); i++ )
		n += a[i] != b[i] ? 1 : 0;
	return n;
}

size_t sentinels( const std::vector<std::uint8_t> & a )
{
	size_t n = 0;
	for ( size_t i = 0; i + 16 <= a.size(); i += 16 ) {
		float x;
		std::memcpy( &x, &a[i], 4 );
		n += x == riSentinel ? 1 : 0;
	}
	return n;
}

// level 0 of a shared 2D target as the present device sees it (sharedfmt.cpp's read, GL finished first)
bool readShared( std::uintptr_t key, int level, int layer, int bpt, std::vector<std::uint8_t> & out, std::string & why )
{
	Vk & v = vk();
	const Shared * s = sharedFind( key );
	if ( !s || !s->img ) {
		why = "no shared target of that key";
		return false;
	}
	const int lw = std::max( s->w >> level, 1 ), lh = std::max( s->h >> level, 1 );
	const VkDeviceSize bytes = VkDeviceSize( lw ) * VkDeviceSize( lh ) * VkDeviceSize( bpt );
	drain();
	v.vkDeviceWaitIdle( v.dev );
	VkBuffer buf = VK_NULL_HANDLE;
	VkDeviceMemory mem = VK_NULL_HANDLE;
	VkBufferCreateInfo bci { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bci.size = bytes;
	bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bool ok = v.vkCreateBuffer( v.dev, &bci, nullptr, &buf ) == VK_SUCCESS;
	if ( ok ) {
		VkMemoryRequirements br;
		v.vkGetBufferMemoryRequirements( v.dev, buf, &br );
		VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		ai.allocationSize = br.size;
		ai.memoryTypeIndex = memType( br.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		ok = ai.memoryTypeIndex != UINT32_MAX && v.vkAllocateMemory( v.dev, &ai, nullptr, &mem ) == VK_SUCCESS
			&& v.vkBindBufferMemory( v.dev, buf, mem, 0 ) == VK_SUCCESS;
	}
	VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cai.commandPool = v.pool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = 1;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	ok = ok && v.vkAllocateCommandBuffers( v.dev, &cai, &cmd ) == VK_SUCCESS && v.vkCreateFence( v.dev, &fi, nullptr, &fence ) == VK_SUCCESS;
	if ( ok ) {
		VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		v.vkBeginCommandBuffer( cmd, &bi );
		VkBufferImageCopy c {};
		c.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, std::uint32_t( level ), std::uint32_t( layer ), 1 };
		c.imageExtent = { std::uint32_t( lw ), std::uint32_t( lh ), 1 };
		v.vkCmdCopyImageToBuffer( cmd, s->img, VK_IMAGE_LAYOUT_GENERAL, buf, 1, &c );
		v.vkEndCommandBuffer( cmd );
		ok = oneShot( cmd, fence );
	}
	if ( ok ) {
		void * p = nullptr;
		ok = v.vkMapMemory( v.dev, mem, 0, bytes, 0, &p ) == VK_SUCCESS;
		if ( ok ) {
			out.assign( static_cast<const std::uint8_t *>( p ), static_cast<const std::uint8_t *>( p ) + bytes );
			v.vkUnmapMemory( v.dev, mem );
		}
	}
	if ( cmd )
		v.vkFreeCommandBuffers( v.dev, v.pool, 1, &cmd );
	if ( fence )
		v.vkDestroyFence( v.dev, fence, nullptr );
	if ( buf )
		v.vkDestroyBuffer( v.dev, buf, nullptr );
	if ( mem )
		v.vkFreeMemory( v.dev, mem, nullptr );
	if ( !ok )
		why = "the read back failed";
	return ok;
}

struct Res
{
	bool looked = false, ok = false;
	std::string text;
};

// one check of both slots: look up, clear the shared images to the sentinel, the owner's pass, GL's reference pass at the
// right size, then every comparison of the bar. wantGen: rtImgGen() must differ from lastGen
void check( GLuint prog, Owner & o, int W, int H, int step, int seed, bool wantGen, std::uint64_t & lastGen, Res out[2] )
{
	const int ew = int( std::ceil( double( W ) / double( step ) ) ), eh = int( std::ceil( double( H ) / double( step ) ) );
	WwVkPresent::RtImg r[2];
	std::string why[2];
	bool found[2];
	for ( int s = 0; s < 2; s++ ) {
		found[s] = WwVkPresent::rtImgOf( WwVkPresent::rtImgName( s ), r[s], why[s] );
		if ( found[s] && r[s].glTex )
			clearTex( r[s].glTex, 0, GL_RGBA, GL_FLOAT, &riSentinel );
	}
	const bool complete = draw( prog, o.fbo, o.key, o.w, o.h, seed );
	// GL's reference: the same program into plain textures of the right size
	GLuint ref[2] = {};
	glGenTextures( 2, ref );
	for ( GLuint t : ref ) {
		glBindTexture( GL_TEXTURE_2D, t );
		glTexImage2D( GL_TEXTURE_2D, 0, GLint( riRgba32f ), ew, eh, 0, GL_RGBA, GL_FLOAT, nullptr );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	}
	glBindTexture( GL_TEXTURE_2D, 0 );
	GLuint rfb = 0;
	f.genFramebuffers( 1, &rfb );
	f.bindFramebuffer( fFb, rfb );
	f.framebufferTexture2D( fFb, fCol0, GL_TEXTURE_2D, ref[0], 0 );
	draw( prog, rfb, ref[1], ew, eh, seed );
	glFinish();
	const GLenum err = glGetError();
	for ( int s = 0; s < 2; s++ ) {
		Res & c = out[s];
		c = Res();
		c.looked = found[s];
		char b[512];
		if ( !found[s] ) {
			std::snprintf( b, sizeof b, "lookup refused (%s), compared 0 bytes", why[s].c_str() );
			c.text = b;
			continue;
		}
		const std::vector<std::uint8_t> want = glRead( ref[s], ew, eh );
		std::vector<std::uint8_t> vk1, vk2;
		std::string w1, w2;
		const bool rd = readShared( r[s].key, 0, 0, 16, vk1, w1 ) && readShared( r[s].key, 0, 0, 16, vk2, w2 );
		const std::vector<std::uint8_t> glOwn = glRead( r[s].glTex, r[s].w, r[s].h );
		const size_t bad = differ( vk1, want ), twice = differ( vk1, vk2 ), glBad = differ( glOwn, want ), sent = sentinels( vk1 );
		const std::uintptr_t fk = farSharedOf( r[s].glTex );
		const bool is = WwVkPresent::sharedTargetIs( r[s].key, r[s].glTex );
		const bool genOk = !wantGen || r[s].gen != lastGen;
		std::snprintf( b, sizeof b,
			"%dx%d (want %dx%d), fmt 0x%04X, farSharedOf %s, sharedTargetIs %d, read %s, compared %zu bytes, %zu differ, read twice %zu differ, "
			"GL's own read %zu differ, sentinel texels %zu, gen %llu%s, fbo %s, GL error 0x%04X",
			r[s].w, r[s].h, ew, eh, r[s].glFmt, fk == r[s].key ? "= key" : "!= key", is ? 1 : 0, rd ? "ok" : w1.c_str(), std::min( vk1.size(), want.size() ),
			bad, twice, glBad, sent, static_cast<unsigned long long>( r[s].gen ), wantGen ? ( genOk ? " (moved)" : " (did NOT move)" ) : "",
			complete ? "complete" : "INCOMPLETE", unsigned( err ) );
		c.text = b;
		c.ok = rd && r[s].w == ew && r[s].h == eh && r[s].glFmt == riRgba32f && fk == r[s].key && is && !want.empty() && bad == 0 && twice == 0
			&& glBad == 0 && sent == 0 && genOk && complete && err == GL_NO_ERROR;
	}
	if ( found[0] )
		lastGen = r[0].gen;
	glDeleteTextures( 2, ref );
	delFbos( 1, &rfb );
}
}  // namespace rtimgv

int main( int argc, char ** argv )
{
	using namespace rtimgv;
	( void )argc;
	( void )argv;
	setvbuf( stdout, nullptr, _IONBF, 0 );
	auto fatal = []( const std::string & m ) {
		std::printf( "RTIMG %s\nRTIMG GATE FAILED\n", m.c_str() );
		return 2;
	};
	_putenv_s( "WW_VKVIEW_GIHALF_RED", "" );
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwrtimgview";
	RegisterClassA( &wc );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwrtimgview", "rt image harness", WS_OVERLAPPEDWINDOW, 40, 40, 160, 90, nullptr,
		nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( dc, ChoosePixelFormat( dc, &pfd ), &pfd );
	HGLRC glrc = wglCreateContext( dc );
	wglMakeCurrent( dc, glrc );
	pump();
	if ( !loadAll() || !load( clearTex, "glClearTexImage" ) || !load( delFbos, "glDeleteFramebuffers" ) )
		return fatal( "GL entry points" );
	std::printf( "RTIMG GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	std::string why;
	if ( !WwVkPresent::viewBegin( 160, 90, 1, glProc, why ) )
		return fatal( "the view: " + why );
	const GLuint prog = program( kRiVs, kRiFs, "rtimg" );
	if ( !prog )
		return fatal( "the GL program" );
	bool ok = true;
	int nGreen = 0, nRed = 0;
	Owner o;
	f.genFramebuffers( 1, &o.fbo );
	Owner * p = &o;
	for ( int s = 0; s < WwVkPresent::kRtImgSlots; s++ )
		WwVkPresent::rtImgOwner( s, [p]( std::string & w ) {
			if ( !p->half ) {
				w = "the owner has no screen GI";
				return false;
			}
			if ( !p->ready ) {
				w = "no screen GI this frame";
				return false;
			}
			return true;
		} );
	{
		WwVkPresent::RtImg r;
		const bool found = WwVkPresent::rtImgOf( "cellNope", r, why );
		std::printf( "RTIMG an unknown name: %s (%s)\n", found ? "FOUND (wrong)" : "refused", why.c_str() );
		ok = ok && !found;
	}
	std::uint64_t lastGen = WwVkPresent::rtImgGen();
	int seed = 3;
	Res res[2];
	// slots: -1 both, else that slot only
	auto line = [&]( const char * label, bool green, int only ) {
		for ( int s = 0; s < 2; s++ ) {
			if ( only >= 0 && s != only )
				continue;
			const bool ex = res[s].ok;
			std::printf( "RTIMG %s %s: %s: %s\n", WwVkPresent::rtImgName( s ), label, res[s].text.c_str(),
				green ? ( ex ? "EXACT" : "FAIL" ) : ( ex ? "EXACT (red did not bite)" : "FAIL (red bites)" ) );
			ok = ok && ( green ? ex : !ex );
			( green ? nGreen : nRed )++;
		}
	};
	auto run = [&]( int W, int H, bool wantGen, const char * label, bool green, int only = -1 ) {
		check( prog, o, W, H, 2, seed++, wantGen, lastGen, res );
		line( label, green, only );
	};
	if ( !share( o, 160, 90, 2, why ) )
		return fatal( "share: " + why );
	run( 160, 90, true, "after a write", true );
	run( 160, 90, false, "after a second write", true );
	// resize: 161 x 91 at step 2 -> 81 x 46 (rounded up)
	if ( !share( o, 161, 91, 2, why ) )
		return fatal( "resize: " + why );
	run( 161, 91, true, "after a resize to 161x91", true );
	// drop + remake
	drop( o );
	run( 161, 91, false, "red dropped (lookup after the drop)", false );
	if ( !share( o, 161, 91, 2, why ) )
		return fatal( "remake: " + why );
	run( 161, 91, true, "after a drop + remake", true );
	// red unshared
	_putenv_s( "WW_VKVIEW_GIHALF_RED", "1" );
	run( 161, 91, false, "red unshared (WW_VKVIEW_GIHALF_RED=1)", false );
	_putenv_s( "WW_VKVIEW_GIHALF_RED", "" );
	run( 161, 91, false, "after the unshared red", true );
	// red not read this frame
	o.ready = false;
	run( 161, 91, false, "red not read this frame (fresh false)", false );
	o.ready = true;
	run( 161, 91, false, "after the not-read red", true );
	// red wrong size: the size rounded down at an odd view
	o.floorSize = true;
	if ( !share( o, 161, 91, 2, why ) )
		return fatal( "size red: " + why );
	run( 161, 91, false, "red wrong size (rounded down: 80x45)", false );
	o.floorSize = false;
	if ( !share( o, 161, 91, 2, why ) )
		return fatal( "after the size red: " + why );
	run( 161, 91, true, "after the size red", true );
	// red stale: a remake whose FBO keeps the old attachment (cellGiHalf; the key is attached every pass)
	drop( o );
	o.attachOn = false;
	if ( !share( o, 161, 91, 2, why ) )
		return fatal( "stale red: " + why );
	run( 161, 91, true, "red stale (the remake not attached)", false, WwVkPresent::kRtImgGiHalf );
	o.attachOn = true;
	drop( o );
	if ( !share( o, 161, 91, 2, why ) )
		return fatal( "after the stale red: " + why );
	run( 161, 91, true, "after the stale red", true );
	std::printf( "RTIMG %s\n", WwVkPresent::rtImgLine().c_str() );
	const WwVkPresent::RtImgStat st = WwVkPresent::rtImgStat();
	const bool statOk = st.made[0] >= 5 && st.dropped[0] >= 3 && st.lookups[0] > 0 && st.refused[0] >= 3;
	std::printf( "RTIMG stats: cellGiHalf made %d (want >= 5), dropped %d (want >= 3), lookups %llu, refused %d (want >= 3): %s\n", st.made[0],
		st.dropped[0], static_cast<unsigned long long>( st.lookups[0] ), st.refused[0], statOk ? "OK" : "FAIL" );
	ok = ok && statOk;
	drop( o );
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	ReleaseDC( wnd, dc );
	DestroyWindow( wnd );
	std::printf( "RTIMG greens %d, reds %d\n", nGreen, nRed );
	std::printf( ok ? "RTIMG GATE OK\n" : "RTIMG GATE FAILED\n" );
	return ok ? 0 : 1;
}

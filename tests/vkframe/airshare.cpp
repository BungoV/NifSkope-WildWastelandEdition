// Lane VKAIR11 (2026-11-16): cell_volair's stage-1 inputs made on the present device by their owner
// (src/features/air/cellvolairshare.h vaShareEnsure, the helper cellvolair.cpp calls at each site) read by GL exactly as
// GL's own textures made by the old calls (the code in cellvolair.cpp's not-asked branch, copied here per site).
// Per input (hist, lvHist, lvNow, stbn, noise, terr: format, type, filter and wrap as the app makes them):
//   shared copy: vaShareEnsure( want ) on the shipped vkpresent.cpp (a real present device), filled by glTexSubImage3D;
//   plain copy: the old calls (glTexImage3D + that site's own sampler state), filled with the same bytes.
// Bar (set before the first run, never widened):
//   made 1 and sharedTargetIs; glGetTexImage bytes EXACT; GL-sampled values EXACT at coordinates in [-0.6, 1.6] (filter
//   and wrap), the counts printed (> 0); the ask-false arm leaves shareds() unchanged; a drop behind the owner's back
//   (a present shutdown) makes ensure return 1 and the refill is EXACT again; a second ensure returns 0; GL error 0.
// Reds (each must FAIL; the gate prints which inputs each moved):
//   memo   the fill memo is not reset after the drop (no refill)
//   params the shared path skips the sampler state (vaShareRed bit 1)
//   layer  the shared copy is filled one layer off
// Last line `AIRSHARE GATE OK` or `AIRSHARE GATE FAILED`.
#define main fxview_main
#include "fxview.cpp"
#undef main

#include <algorithm>
#include "features/air/cellvolairkeys.h"
#include "features/air/cellvolairshare.h"

namespace ashare
{
using namespace fv;

// the GL calls vaShareEnsure makes, named as the app's Qt functions name them
struct GateFn
{
	void glGenTextures( GLsizei n, GLuint * t ) { ::glGenTextures( n, t ); }
	void glDeleteTextures( GLsizei n, const GLuint * t ) { ::glDeleteTextures( n, t ); }
	void glBindTexture( GLenum a, GLuint t ) { ::glBindTexture( a, t ); }
	void glTexParameteri( GLenum a, GLenum p, GLint v ) { ::glTexParameteri( a, p, v ); }
	void glTexImage3D( GLenum a, GLint l, GLint fmt, GLsizei w, GLsizei h, GLsizei d, GLint b, GLenum pf, GLenum ty, const void * px )
	{
		f.texImage3D( a, l, fmt, w, h, d, b, pf, ty, px );
	}
};
GateFn gfn;

constexpr GLenum kRgba = 0x1908, kFloat = 0x1406, kUbyte = 0x1401, kLinear = 0x2601, kNearest = 0x2600, kRepeat = 0x2901;
constexpr GLenum kMinF = 0x2801, kMagF = 0x2800, kWrapS = 0x2802, kWrapT = 0x2803;

struct Input
{
	const char * name;
	int slot;
	VaShareSpec sp;
	// the old site's sampler state: wrapR set (vaAlloc3D, noise) or not (stbn, terr)
	bool oldWrapR;
	int bytesPerTexel;  // read back in the storage's own form
	GLenum readFmt, readType;
};

std::vector<Input> inputs()
{
	// sizes: small, the app's formats and states (cellvolair.cpp hist / lv / stbn / noise / terr sites)
	const int cols = 24, rows = 14, kSl = 16, strata = 6;
	return {
		{ "hist", VaVkKeys::Hist0, { 1, fRgba32f, kRgba, kFloat, cols, rows, strata * kSl, true, false }, true, 16, kRgba, kFloat },
		{ "lvHist", VaVkKeys::LvHist0, { 1, fRgba16f, kRgba, kFloat, cols, rows, kSl, false, false }, true, 8, kRgba, fHalf },
		{ "lvNow", VaVkKeys::LvNow0, { 1, fRgba32f, kRgba, kFloat, cols, rows, kSl, false, false }, true, 16, kRgba, kFloat },
		{ "stbn", VaVkKeys::Stbn, { 2, fR8, fRed, kUbyte, 128, 128, 64, false, true }, false, 1, fRed, kUbyte },
		{ "noise", VaVkKeys::Noise, { 1, fR8, fRed, kUbyte, 32, 32, 32, true, true }, true, 1, fRed, kUbyte },
		{ "terr", VaVkKeys::Terr, { 2, fR32f, fRed, kFloat, 33, 17, 2, true, false }, false, 4, fRed, kFloat },
	};
}

GLenum targetOf( const Input & in )
{
	return in.sp.type == 2 ? fT2DA : fT3D;
}

int channels( const Input & in )
{
	return in.sp.pf == kRgba ? 4 : 1;
}

// deterministic contents in the upload form (floats or bytes), a value per channel
std::vector<unsigned char> content( const Input & in )
{
	const size_t n = static_cast<size_t>( in.sp.w ) * in.sp.h * in.sp.d * channels( in );
	std::vector<unsigned char> b;
	std::uint32_t s = 0x9E3779B9u ^ std::uint32_t( in.slot * 7919 );
	auto next = [&]() {
		s ^= s << 13;
		s ^= s >> 17;
		s ^= s << 5;
		return s;
	};
	if ( in.sp.ty == kUbyte ) {
		b.resize( n );
		for ( size_t i = 0; i < n; i++ )
			b[i] = static_cast<unsigned char>( next() & 255u );
	} else {
		b.resize( n * 4 );
		for ( size_t i = 0; i < n; i++ ) {
			const float v = float( next() % 200001u ) * 1e-4f - 10.0f;
			std::memcpy( &b[i * 4], &v, 4 );
		}
	}
	return b;
}

// fill a texture's whole storage from layer `zOff` on (layer i gets the data's layer i - zOff); 0 = as the app
void fill( const Input & in, GLuint t, const std::vector<unsigned char> & data, int zOff )
{
	const GLenum tg = targetOf( in );
	const size_t layerBytes = data.size() / size_t( in.sp.d );
	glBindTexture( tg, t );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	for ( int z = zOff; z < in.sp.d; z++ )
		f.texSubImage3D( tg, 0, 0, 0, z, in.sp.w, in.sp.h, 1, in.sp.pf, in.sp.ty, data.data() + layerBytes * size_t( z - zOff ) );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
	glBindTexture( tg, 0 );
}

// the old site's calls: gen, glTexImage3D, its own sampler state
GLuint plainCopy( const Input & in, const std::vector<unsigned char> & data )
{
	const GLenum tg = targetOf( in );
	GLuint t = 0;
	glGenTextures( 1, &t );
	glBindTexture( tg, t );
	f.texImage3D( tg, 0, GLint( in.sp.fmt ), in.sp.w, in.sp.h, in.sp.d, 0, in.sp.pf, in.sp.ty, nullptr );
	const GLint flt = in.sp.linear ? kLinear : kNearest, wr = in.sp.repeat ? kRepeat : fClamp;
	glTexParameteri( tg, kMinF, flt );
	glTexParameteri( tg, kMagF, flt );
	glTexParameteri( tg, kWrapS, wr );
	glTexParameteri( tg, kWrapT, wr );
	if ( in.oldWrapR )
		glTexParameteri( tg, fWrapR, wr );
	glTexParameteri( tg, fMaxLevel, 0 );
	glBindTexture( tg, 0 );
	fill( in, t, data, 0 );
	return t;
}

std::vector<unsigned char> readBack( const Input & in, GLuint t )
{
	const GLenum tg = targetOf( in );
	std::vector<unsigned char> out( static_cast<size_t>( in.sp.w ) * in.sp.h * in.sp.d * size_t( in.bytesPerTexel ) );
	glBindTexture( tg, t );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glGetTexImage( tg, 0, in.readFmt, in.readType, out.data() );
	glPixelStorei( GL_PACK_ALIGNMENT, 4 );
	glBindTexture( tg, 0 );
	return out;
}

const char * kSampleFs = "#version 410 core\n"
						 "uniform sampler3D s3; uniform sampler2DArray sa; uniform int mode; uniform float wz; uniform vec2 sz;\n"
						 "out vec4 o;\n"
						 "void main() { vec2 uv = gl_FragCoord.xy / sz * 2.2 - 0.6;\n"
						 "  o = mode == 0 ? texture( s3, vec3( uv, wz ) ) : texture( sa, vec3( uv, wz ) ); }\n";
GLuint gProg = 0, gFb = 0, gCol = 0;
constexpr int kSW = 61, kSH = 47;

// GL-sampled values of a texture at coordinates past both edges, several depths / layers
std::vector<float> sampled( const Input & in, GLuint t )
{
	const bool arr = in.sp.type == 2;
	std::vector<float> ws = arr ? std::vector<float> { -0.6f, 0.3f, 0.77f, 1.5f, float( in.sp.d ) - 0.5f, float( in.sp.d ) + 1.0f }
								: std::vector<float> { -0.6f, -0.1f, 0.13f, 0.5f, 0.91f, 1.2f, 1.6f };
	std::vector<float> out;
	f.bindFramebuffer( fFb, gFb );
	glViewport( 0, 0, kSW, kSH );
	glDisable( GL_BLEND );
	glDisable( GL_DEPTH_TEST );
	f.useProgram( gProg );
	bindUnit( 1, fT3D, arr ? 0 : t );
	bindUnit( 2, fT2DA, arr ? t : 0 );
	f.uniform1i( f.getUniformLocation( gProg, "s3" ), 1 );
	f.uniform1i( f.getUniformLocation( gProg, "sa" ), 2 );
	f.uniform1i( f.getUniformLocation( gProg, "mode" ), arr ? 1 : 0 );
	const float sz[2] = { float( kSW ), float( kSH ) };
	f.uniform2fv( f.getUniformLocation( gProg, "sz" ), 1, sz );
	for ( float w : ws ) {
		f.uniform1fv( f.getUniformLocation( gProg, "wz" ), 1, &w );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		std::vector<float> px( size_t( kSW ) * kSH * 4 );
		glReadPixels( 0, 0, kSW, kSH, kRgba, kFloat, px.data() );
		out.insert( out.end(), px.begin(), px.end() );
	}
	bindUnit( 1, fT3D, 0 );
	bindUnit( 2, fT2DA, 0 );
	f.bindFramebuffer( fFb, 0 );
	return out;
}

size_t bytesOff( const std::vector<unsigned char> & a, const std::vector<unsigned char> & b )
{
	size_t n = a.size() != b.size() ? std::max( a.size(), b.size() ) : 0;
	for ( size_t i = 0; i < std::min( a.size(), b.size() ); i++ )
		n += a[i] != b[i] ? 1 : 0;
	return n;
}

size_t valuesOff( const std::vector<float> & a, const std::vector<float> & b )
{
	size_t n = a.size() != b.size() ? std::max( a.size(), b.size() ) : 0;
	for ( size_t i = 0; i < std::min( a.size(), b.size() ); i++ )
		n += std::memcmp( &a[i], &b[i], 4 ) != 0 ? 1 : 0;
	return n;
}

struct Cmp
{
	size_t bytes = 0, bytesOff = 0, values = 0, valuesOff = 0;
	bool exact() const { return bytes > 0 && values > 0 && bytesOff == 0 && valuesOff == 0; }
};

Cmp compare( const Input & in, GLuint shared, GLuint plain )
{
	Cmp c;
	const auto a = readBack( in, shared ), b = readBack( in, plain );
	c.bytes = b.size();
	c.bytesOff = bytesOff( a, b );
	const auto sa = sampled( in, shared ), sb = sampled( in, plain );
	c.values = sb.size();
	c.valuesOff = valuesOff( sa, sb );
	return c;
}

// one input through the green bar and the three reds; red results land in moved[r] (true = the red moved this input)
bool runInput( const Input & in, VaVkKeys & keys, bool moved[3] )
{
	const std::uintptr_t key = keys.at( in.slot );
	const auto data = content( in );
	const GLuint plain = plainCopy( in, data );
	bool ok = true;
	auto say = [&]( const char * what, const Cmp & c, bool pass ) {
		std::printf( "AIRSHARE %-6s %-26s bytes %zu off %zu, sampled values %zu off %zu: %s\n", in.name, what, c.bytes, c.bytesOff,
			c.values, c.valuesOff, pass ? "PASS" : "FAIL" );
	};

	// ask false: GL's own storage, no share made
	{
		const size_t before = shareds().size();
		unsigned t = 0;
		VaShareState st;
		const int made = vaShareEnsure( &gfn, key, in.sp, false, t, st );
		fill( in, t, data, 0 );
		const Cmp c = compare( in, t, plain );
		const bool pass = made == 1 && !st.shared && shareds().size() == before && c.exact();
		std::printf( "AIRSHARE %-6s ask false: made %d shared %d, live shared %zu -> %zu\n", in.name, made, int( st.shared ), before,
			shareds().size() );
		say( "ask false vs old calls", c, pass );
		ok = ok && pass;
		glDeleteTextures( 1, &t );
	}

	// green: shared, filled, EXACT; ensure again makes nothing
	unsigned t = 0;
	VaShareState st;
	{
		const int made = vaShareEnsure( &gfn, key, in.sp, true, t, st );
		const bool is = WwVkPresent::sharedTargetIs( key, t );
		if ( !st.shared )
			std::printf( "AIRSHARE %-6s refused: %s\n", in.name, st.why.c_str() );
		fill( in, t, data, 0 );
		const int again = vaShareEnsure( &gfn, key, in.sp, true, t, st );
		const Cmp c = compare( in, t, plain );
		const bool pass = made == 1 && st.shared && is && again == 0 && c.exact();
		std::printf( "AIRSHARE %-6s shared: made %d is %d, ensure again %d\n", in.name, made, int( is ), again );
		say( "shared vs old calls", c, pass );
		ok = ok && pass;
	}

	// a drop behind the owner's back (a present shutdown): ensure makes it again, the refill is EXACT
	for ( int arm = 0; arm < 2; arm++ ) {
		const bool red = arm == 1;
		WwVkPresent::sharedTargetDrop( key );
		const int made = vaShareEnsure( &gfn, key, in.sp, true, t, st );
		if ( !red && made )
			fill( in, t, data, 0 );
		const Cmp c = compare( in, t, plain );
		const bool pass = made == 1 && st.shared && c.exact();
		if ( red )
			moved[0] = !pass;
		else
			ok = ok && pass;
		say( red ? "red memo (no refill)" : "dropped, made again", c, pass );
	}

	// red params: a fresh share whose sampler state was skipped
	{
		vaShareRelease( key, t, st );
		vaShareRed() = 1;
		vaShareEnsure( &gfn, key, in.sp, true, t, st );
		vaShareRed() = 0;
		fill( in, t, data, 0 );
		const Cmp c = compare( in, t, plain );
		moved[1] = !c.exact();
		say( "red params skipped", c, c.exact() );
	}

	// red layer: filled one layer off
	{
		vaShareRelease( key, t, st );
		vaShareEnsure( &gfn, key, in.sp, true, t, st );
		fill( in, t, data, 1 );
		const Cmp c = compare( in, t, plain );
		moved[2] = !c.exact();
		say( "red one layer off", c, c.exact() );
	}

	vaShareRelease( key, t, st );
	glDeleteTextures( 1, &plain );
	const GLenum err = glGetError();
	if ( err != GL_NO_ERROR ) {
		std::printf( "AIRSHARE %-6s GL error 0x%x: FAIL\n", in.name, unsigned( err ) );
		ok = false;
	}
	return ok;
}

}  // namespace ashare

int main( int argc, char ** argv )
{
	using namespace ashare;
	(void)argc;
	(void)argv;
	setvbuf( stdout, nullptr, _IONBF, 0 );
	auto fatal = []( const std::string & m ) {
		std::printf( "AIRSHARE %s\nAIRSHARE GATE FAILED\n", m.c_str() );
		return 2;
	};
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwairshare";
	RegisterClassA( &wc );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwairshare", "air share harness", WS_OVERLAPPEDWINDOW, 40, 40, 160, 90, nullptr,
		nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( dc, ChoosePixelFormat( dc, &pfd ), &pfd );
	HGLRC glrc = wglCreateContext( dc );
	wglMakeCurrent( dc, glrc );
	pump();
	if ( !loadAll() )
		return fatal( "GL entry points" );
	std::printf( "AIRSHARE GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	std::string why;
	if ( !WwVkPresent::viewBegin( 160, 90, 1, glProc, why ) )
		return fatal( "the view: " + why );
	gProg = program( kTriVs, kSampleFs, "sample" );
	gCol = tex2d( kSW, kSH, fRgba32f, kRgba, kFloat, nullptr, false );
	gFb = gProg && gCol ? fbo( GL_TEXTURE_2D, gCol, 0, 0 ) : 0;
	if ( !gFb )
		return fatal( "GL sampling program / target" );
	while ( glGetError() != GL_NO_ERROR ) {
	}
	const size_t before = shareds().size();
	VaVkKeys keys;
	bool green = true;
	const char * redName[3] = { "memo", "params", "layer" };
	std::string movedBy[3];
	int movedN[3] = { 0, 0, 0 };
	const auto ins = inputs();
	for ( const Input & in : ins ) {
		bool moved[3] = { false, false, false };
		green = runInput( in, keys, moved ) && green;
		for ( int r = 0; r < 3; r++ )
			if ( moved[r] ) {
				movedN[r]++;
				movedBy[r] += std::string( movedBy[r].empty() ? "" : " " ) + in.name;
			}
	}
	const size_t after = shareds().size();
	std::printf( "AIRSHARE inputs %zu, live shared targets before %zu after %zu (bar equal)\n", ins.size(), before, after );
	green = green && after == before && !ins.empty();
	bool reds = true;
	for ( int r = 0; r < 3; r++ ) {
		std::printf( "AIRSHARE red %-6s moved %d of %zu inputs (%s): %s\n", redName[r], movedN[r], ins.size(),
			movedBy[r].empty() ? "none" : movedBy[r].c_str(), movedN[r] > 0 ? "FAIL (as it must)" : "PASSED (TESTS NOTHING: it must FAIL)" );
		reds = reds && movedN[r] > 0;
	}
	// memo and layer must move every input (every input is filled; a layer shift moves every texture of depth > 1)
	reds = reds && movedN[0] == int( ins.size() ) && movedN[2] == int( ins.size() );
	std::printf( "AIRSHARE green %s\n", green ? "PASS" : "FAIL" );
	const bool ok = green && reds;
	std::printf( "AIRSHARE GATE %s\n", ok ? "OK" : "FAILED" );
	return ok ? 0 : 1;
}

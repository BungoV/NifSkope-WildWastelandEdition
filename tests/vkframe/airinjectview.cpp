// Lane VKAIR4 (2026-11-16): the air inject's stage 0 (cellvolair.cpp vaPassInner, the first drawShape: each froxel column's
// opaque ray distances (far, near) into the RG32F tile) on the present device: vkairinject.inc airInjectRun draws the app's
// full-screen quad into a SHARED RG32F tile (sharedTarget type 0, cols x rows) with the airinject twins of cell_volair,
// reading volGbuf in place when it is the shared D24S8 (the depth path, volGbufZ 1) or through one copy (the alpha path,
// volGbufZ 0: the AO pass's GL-owned RGBA32F g-buffer, linear distance in alpha). GL's side is GL's own cell_volair program
// (airinjectview_prep.py, both pixel-stage keys) into a GL-owned RG32F tile, with the app's state (blend, depth, scissor,
// cull off; the quad { -1 -1 0, 1 -1 0, -1 1 0, 1 1 0 } / { 0 1 2 2 1 3 } at location 0; viewport cols x rows), every loose
// uniform seeded by farloddraw's seedUnis (profile fogOn), every sampler on its own unit with nothing bound but volGbuf,
// every block bound zero, then the stage's own: volStage 0, volLayer 0, volDims (21, 12, 64), volProj (1.2, 2.1),
// volGbufZ, volProjZ (P22, P32) of near 1 far 60, volGbufSize (251, 143), fogDistScale 70, cellRow (1.2 0 1.6 100,
// 0 1 0 -50, 0 0 1 30): k = 35.
// Fixture (by construction, worked out before the first run): the g-buffer 251 x 143 over 21 x 12 columns (11.95 x 11.92 px a
// column: a non-integer ratio, asymmetric in y). Sky = x < 40 and y >= 100, plus the one pixel (200, 20): depth 0xFFFFFF /
// alpha 1e6. Ground: depth from view distances 1.5..45 (24-bit, never above 0xFFF000) / alpha 5..4000; the alpha path's
// pixels x 120..129, y 60..69 hold alpha -3 (a distance of 0). Column windows (the shader's one-pixel margin) give:
//   all-sky columns (far and near 3e38) = 9 (cx 0..2, cy 9..11), mixed (far 3e38, near finite) = 8 (cx 0..3 x cy 8..11
//   less those 9, plus (16, 1)), ground = 235; near == 0 columns: alpha path 4 (cx 9..10 x cy 4..5), depth path 0.
// Bar (set before the first run, never widened):
//   GL coverage EXACTLY the counts above, per arm (the fixture does what it says);
//   depth path: the shared D24S8 holds GL's own bytes (EXACT) and GL drawn on the shared depth = GL on its own (EXACT);
//   Vulkan vs GL, every texel of the tile: written (neither side keeps the sentinel 7 in R and G), no NaN / Inf, and
//   |vk - gl| <= 1e-5 max(1, |gl|) on every value (fogview's RGBA32F clause); EXACT = every value equal;
//   copies: depth path 0 a run (the shared depth read in place), alpha path 1 a run; a rerun after the reds is EXACT
//   against the first Vulkan run (no state left behind).
// Arms: depth def, depth rt, alpha def, alpha rt (def = WW_CELL_AIR_SKYVIEW+WW_CELL_FOGLEAK+WW_FRAME_UBO, rt = the all-on key
// + WW_CELL_RESTIR+WW_CELL_RTDDGI+WW_CELL_RTDDGI_CASCADES).
// Reds, every arm, each must FAIL (never pass); route reds change one data field of the twin's b31 (never a loop bound's
// whole block: a shifted slice can feed volDims / volGbufSize garbage and lose the device): 1 fogDistScale doubled;
// 2 volGbufZ flipped; 4 volGbufSize.x one short; 8 volDims.y one more; gate red 16 the g-buffer Vulkan reads rewritten after
// GL's draw and the first Vulkan run (a live read, not a stale copy); gate red 32 the other key's wwDefs: must REFUSE,
// named "key". (A flipped viewport is no red here: the quad covers every pixel and the stage reads only FragCoord.)
// GL probes (moved = texels failing the bar's clause against GL's own; LIVE >= 50 of 252, else TESTS NOTHING and the gate
// fails): GL rerun with each route red's value (fogDistScale 140, volGbufZ flipped, volGbufSize.x 250, volDims.y 13) and
// GL on the rewritten input.
// Last line `AIRINJECTVIEW GATE OK` or `AIRINJECTVIEW GATE FAILED`.
#define FLD_NO_MAIN
#include "farloddraw.cpp"
#undef FLD_NO_MAIN

namespace aiv
{
using namespace fv;
using namespace fld;

const int kAiGw = 251, kAiGh = 143, kAiCols = 21, kAiRows = 12, kAiSlices = 64, kAiLive = 50;
const int kAiSkyCols = 9, kAiMixedCols = 8, kAiGroundCols = 235, kAiZeroNearAlpha = 4;
const float kAiSentinel = 7.0f, kAiFar = 1.0e38f, kAiDistScale = 70.0f;
const double kAiP22 = -61.0 / 59.0, kAiP32 = -120.0 / 59.0;	// near 1, far 60
const GLenum kAiDepthStencil = 0x84F9, kAiU248 = 0x84FA, kAiDsMode = 0x90EA;

struct Ai
{
	void( WINAPI * vertexAttribPointer )( GLuint, GLint, GLenum, GLboolean, GLsizei, const void * ) = nullptr;
	void( WINAPI * enableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * genVertexArrays )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindVertexArray )( GLuint ) = nullptr;
	void( WINAPI * getActiveUniformBlockName )( GLuint, GLuint, GLsizei, GLsizei *, char * ) = nullptr;
};
Ai ai;

bool loadAi()
{
	return load( ai.vertexAttribPointer, "glVertexAttribPointer" ) && load( ai.enableVertexAttribArray, "glEnableVertexAttribArray" )
		&& load( ai.genVertexArrays, "glGenVertexArrays" ) && load( ai.bindVertexArray, "glBindVertexArray" )
		&& load( ai.getActiveUniformBlockName, "glGetActiveUniformBlockName" );
}

bool isSky( int x, int y )
{
	return ( x < 40 && y >= 100 ) || ( x == 200 && y == 20 );
}

bool isNeg( int x, int y )
{
	return x >= 120 && x < 130 && y >= 60 && y < 70;
}

std::vector<std::uint32_t> depthData( unsigned seed )
{
	std::mt19937 rng( seed * 7919u + 13u );
	std::uniform_real_distribution<double> u( 0.0, 1.0 );
	std::vector<std::uint32_t> v( size_t( kAiGw ) * size_t( kAiGh ) );
	for ( int y = 0; y < kAiGh; y++ )
		for ( int x = 0; x < kAiGw; x++ ) {
			std::uint32_t q = 0xFFFFFFu;
			if ( !isSky( x, y ) ) {
				const double d = 1.5 + 43.5 * u( rng ), ndc = kAiP32 / d - kAiP22, z = 0.5 * ( ndc + 1.0 );
				q = std::min<std::uint32_t>( std::uint32_t( std::llround( z * 16777215.0 ) ), 0xFFF000u );
			}
			v[size_t( y ) * size_t( kAiGw ) + size_t( x )] = ( q << 8 ) | 0x5Au;
		}
	return v;
}

std::vector<float> alphaData( unsigned seed )
{
	std::mt19937 rng( seed * 7919u + 17u );
	std::uniform_real_distribution<float> u( 0.0f, 1.0f );
	std::vector<float> v( size_t( kAiGw ) * size_t( kAiGh ) * 4 );
	for ( int y = 0; y < kAiGh; y++ )
		for ( int x = 0; x < kAiGw; x++ ) {
			const size_t i = ( size_t( y ) * size_t( kAiGw ) + size_t( x ) ) * 4;
			for ( int c = 0; c < 3; c++ )
				v[i + size_t( c )] = u( rng );
			const float a = 5.0f + 3995.0f * u( rng );
			v[i + 3] = isSky( x, y ) ? 1.0e6f : isNeg( x, y ) ? -3.0f : a;
		}
	return v;
}

void putDepth( GLuint t, const std::vector<std::uint32_t> & d )
{
	glBindTexture( GL_TEXTURE_2D, t );
	glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, kAiGw, kAiGh, kAiDepthStencil, kAiU248, d.data() );
	glBindTexture( GL_TEXTURE_2D, 0 );
}

std::vector<std::uint32_t> readDepth( GLuint t )
{
	std::vector<std::uint32_t> v( size_t( kAiGw ) * size_t( kAiGh ) );
	glBindTexture( GL_TEXTURE_2D, t );
	glGetTexImage( GL_TEXTURE_2D, 0, kAiDepthStencil, kAiU248, v.data() );
	glBindTexture( GL_TEXTURE_2D, 0 );
	return v;
}

void putAlpha( GLuint t, const std::vector<float> & d )
{
	glBindTexture( GL_TEXTURE_2D, t );
	glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, kAiGw, kAiGh, GL_RGBA, GL_FLOAT, d.data() );
	glBindTexture( GL_TEXTURE_2D, 0 );
}

void fillTile( GLuint t )
{
	const std::vector<float> s( size_t( kAiCols ) * size_t( kAiRows ) * 2, kAiSentinel );
	glBindTexture( GL_TEXTURE_2D, t );
	glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, kAiCols, kAiRows, fRg, GL_FLOAT, s.data() );
	glBindTexture( GL_TEXTURE_2D, 0 );
}

std::vector<float> readTile( GLuint t )
{
	std::vector<float> v( size_t( kAiCols ) * size_t( kAiRows ) * 2 );
	glBindTexture( GL_TEXTURE_2D, t );
	glGetTexImage( GL_TEXTURE_2D, 0, fRg, GL_FLOAT, v.data() );
	glBindTexture( GL_TEXTURE_2D, 0 );
	return v;
}

struct TileV
{
	int texels = 0, unwritten = 0, nonFinite = 0, diff = 0, bad = 0, moved = 0;
	double maxRel = 0.0;
	bool ok() const { return texels > 0 && unwritten == 0 && nonFinite == 0 && bad == 0; }
	const char * word() const { return !ok() ? "FAIL" : diff == 0 ? "EXACT" : "BOUND"; }
};

// the bar's clause per value; moved = texels with any value past it (or unwritten / non-finite on either side)
TileV judgeTile( const std::vector<float> & gl, const std::vector<float> & vk )
{
	TileV v;
	for ( size_t t = 0; t < gl.size() / 2; t++ ) {
		v.texels++;
		bool m = false;
		const bool uw = ( gl[2 * t] == kAiSentinel && gl[2 * t + 1] == kAiSentinel ) || ( vk[2 * t] == kAiSentinel && vk[2 * t + 1] == kAiSentinel );
		v.unwritten += uw;
		m = m || uw;
		for ( int c = 0; c < 2; c++ ) {
			const double g = gl[2 * t + size_t( c )], k = vk[2 * t + size_t( c )];
			if ( !std::isfinite( g ) || !std::isfinite( k ) ) {
				v.nonFinite++;
				m = true;
				continue;
			}
			const double rel = std::fabs( k - g ) / std::max( 1.0, std::fabs( g ) );
			v.maxRel = std::max( v.maxRel, rel );
			v.diff += k != g;
			v.bad += rel > 1e-5;
			m = m || rel > 1e-5;
		}
		v.moved += m;
	}
	return v;
}

struct Cov
{
	int sky = 0, mixed = 0, ground = 0, zeroNear = 0;
};

Cov coverage( const std::vector<float> & v )
{
	Cov c;
	for ( size_t t = 0; t < v.size() / 2; t++ ) {
		const float tFar = v[2 * t], tNear = v[2 * t + 1];	// (far / near are Windows macros)
		if ( tFar >= kAiFar && tNear >= kAiFar )
			c.sky++;
		else if ( tFar >= kAiFar )
			c.mixed++;
		else
			c.ground++;
		c.zeroNear += tNear == 0.0f;
	}
	return c;
}

//! the stage's own uniforms (cellvolair.cpp :2438-2899 + vaPassInner); "" or the first one GL's program does not have
std::string stage0( GLuint p, int zDepth, int sizeShort, int rowsMore = 0, float distScale = kAiDistScale )
{
	f.useProgram( p );
	auto loc = [&]( const char * n ) { return f.getUniformLocation( p, n ); };
	const char * names[9] = { "volStage", "volLayer", "volDims", "volProj", "volGbufZ", "volProjZ", "volGbufSize", "fogDistScale", "cellRow" };
	for ( const char * n : names )
		if ( loc( n ) < 0 )
			return n;
	const GLint zero = 0, dims[3] = { kAiCols, kAiRows + rowsMore, kAiSlices }, gsz[2] = { kAiGw - sizeShort, kAiGh };
	const float proj[2] = { 1.2f, 2.1f }, projZ[2] = { float( kAiP22 ), float( kAiP32 ) }, ds = distScale;
	const float row[12] = { 1.2f, 0.0f, 1.6f, 100.0f, 0.0f, 1.0f, 0.0f, -50.0f, 0.0f, 0.0f, 1.0f, 30.0f };
	f.uniform1iv( loc( "volStage" ), 1, &zero );
	f.uniform1iv( loc( "volLayer" ), 1, &zero );
	f.uniform3iv( loc( "volDims" ), 1, dims );
	f.uniform2fv( loc( "volProj" ), 1, proj );
	f.uniform1iv( loc( "volGbufZ" ), 1, &zDepth );
	f.uniform2fv( loc( "volProjZ" ), 1, projZ );
	f.uniform2iv( loc( "volGbufSize" ), 1, gsz );
	f.uniform1fv( loc( "fogDistScale" ), 1, &ds );
	f.uniform4fv( loc( "cellRow" ), 3, row );
	return "";
}

//! blocks bound zero, loose uniforms seeded, samplers on their own units (nothing bound); volGbuf's unit back
std::string setupProg( GLuint p, const char * tag, int & gbufUnit )
{
	GLint nb = 0;
	f.getProgramiv( p, 0x8A36 /* GL_ACTIVE_UNIFORM_BLOCKS */, &nb );
	std::vector<GLuint> ubo( size_t( std::max( nb, 1 ) ) );
	f.genBuffers( GLsizei( ubo.size() ), ubo.data() );
	for ( GLint b = 0; b < nb; b++ ) {
		char nm[128] = {};
		ai.getActiveUniformBlockName( p, GLuint( b ), 128, nullptr, nm );
		GLint bytes = 16;
		f.getActiveUniformBlockiv( p, GLuint( b ), fBlockSize, &bytes );
		f.uniformBlockBinding( p, GLuint( b ), GLuint( b ) );
		const std::vector<std::uint8_t> z( size_t( std::max( bytes, 16 ) ), 0 );
		f.bindBuffer( fUbo, ubo[size_t( b )] );
		f.bufferData( fUbo, std::ptrdiff_t( z.size() ), z.data(), fStatic );
		f.bindBufferBase( fUbo, GLuint( b ), ubo[size_t( b )] );
		std::printf( "AIRINJECTVIEW %s block %s %d bytes (zero)\n", tag, nm, bytes );
	}
	_putenv( "FLD_ON=fogOn" );
	std::mt19937 rng( 23 );
	const int seeded = seedUnis( p, rng );
	f.useProgram( p );
	GLint nu = 0;
	f.getProgramiv( p, fActiveUnis, &nu );
	int unit = 1, samplers = 0;
	gbufUnit = -1;
	for ( GLint i = 0; i < nu; i++ ) {
		char nm[256] = {};
		GLint sz = 0;
		GLenum ty = 0;
		f.getActiveUniform( p, GLuint( i ), 256, nullptr, &sz, &ty, nm );
		const bool image = ty >= 0x904D && ty <= 0x906C;
		const bool sampler = !image && ( ( ty >= 0x8B5D && ty <= 0x8B64 ) || ( ty >= 0x8DC0 && ty <= 0x8DD8 ) || ( ty >= 0x9008 && ty <= 0x9110 ) );
		if ( !sampler )
			continue;
		std::vector<GLint> u( size_t( std::max( sz, 1 ) ) );
		for ( GLint & x : u )
			x = unit++;
		if ( std::string( nm ) == "volGbuf" )
			gbufUnit = u[0];
		f.uniform1iv( f.getUniformLocation( p, nm ), GLsizei( u.size() ), u.data() );
		samplers += int( u.size() );
	}
	std::printf( "AIRINJECTVIEW %s: %d loose uniforms seeded, %d samplers on units 1..%d, volGbuf on unit %d\n", tag, seeded, samplers, unit - 1,
		gbufUnit );
	if ( gbufUnit < 0 )
		return "no volGbuf sampler";
	if ( unit > 160 )
		return "more sampler units than the gate binds";
	return "";
}
}  // namespace aiv

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace fld;
	using namespace aiv;
	gTag = "AIRINJECTVIEW";
	if ( argc < 2 ) {
		std::printf( "usage: airinjectview <prep dir>\nAIRINJECTVIEW GATE FAILED\n" );
		return 2;
	}
	const std::string dir = argv[1];
	auto fatal = [&]( const std::string & why ) {
		std::printf( "AIRINJECTVIEW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	const int S = 4, W = 256, H = 160;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwairinjectview";
	RegisterClassA( &wc );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwairinjectview", "air inject harness", WS_OVERLAPPEDWINDOW, 40, 40, 320, 200, nullptr,
		nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( dc, ChoosePixelFormat( dc, &pfd ), &pfd );
	HGLRC glrc = wglCreateContext( dc );
	wglMakeCurrent( dc, glrc );
	ShowWindow( wnd, SW_SHOWNOACTIVATE );
	SetWindowPos( wnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
	pump();
	if ( !loadAll() || !loadMore() || !loadAi() )
		return fatal( "GL entry points" );
	std::printf( "AIRINJECTVIEW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	std::string why;
	if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) )
		return fatal( "the view: " + why );

	// ---- the shared tile and depth as the app's owner makes them with the view on; GL's own twins of both
	static char keys[2];
	unsigned vkTile = 0, vkDs = 0;
	if ( !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &keys[0] ), 0, kAiCols, kAiRows, 1, fRg32f, vkTile, why ) )
		return fatal( "the shared tile: " + why );
	if ( !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &keys[1] ), 0, kAiGw, kAiGh, 1, fD24s8, vkDs, why ) )
		return fatal( "the shared depth: " + why );
	const GLuint glTile = tex2d( kAiCols, kAiRows, fRg32f, fRg, GL_FLOAT, nullptr, false );
	const GLuint glDs = tex2d( kAiGw, kAiGh, fD24s8, kAiDepthStencil, kAiU248, nullptr, false );
	const GLuint gb = tex2d( kAiGw, kAiGh, fRgba32f, GL_RGBA, GL_FLOAT, nullptr, false );
	for ( GLuint t : { GLuint( glDs ), GLuint( vkDs ) } ) {
		glBindTexture( GL_TEXTURE_2D, t );
		glTexParameteri( GL_TEXTURE_2D, kAiDsMode, GL_DEPTH_COMPONENT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	}
	glBindTexture( GL_TEXTURE_2D, 0 );
	const GLuint fbGl = fbo( GL_TEXTURE_2D, glTile, 0, 0 );
	f.bindFramebuffer( fFb, 0 );
	if ( !fbGl )
		return fatal( "the GL tile framebuffer" );
	GLuint vao = 0, vb = 0, ib = 0;
	ai.genVertexArrays( 1, &vao );
	ai.bindVertexArray( vao );
	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	f.genBuffers( 1, &vb );
	f.genBuffers( 1, &ib );
	f.bindBuffer( 0x8892, vb );
	f.bufferData( 0x8892, sizeof( quad ), quad, fStatic );
	f.bindBuffer( 0x8893, ib );
	f.bufferData( 0x8893, sizeof( idx ), idx, fStatic );
	ai.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 12, nullptr );
	ai.enableVertexAttribArray( 0 );

	const char * keyName[2] = { "def", "rt" };
	GLuint progs[2] = {};
	int gbufUnit[2] = { -1, -1 };
	for ( int key = 0; key < 2; key++ ) {
		progs[key] = program( readFile( dir + "/" + keyName[key] + ".vert.glsl" ), readFile( dir + "/" + keyName[key] + ".frag.glsl" ), keyName[key] );
		if ( !progs[key] )
			return fatal( std::string( "GL program " ) + keyName[key] );
		const std::string e = setupProg( progs[key], keyName[key], gbufUnit[key] );
		if ( !e.empty() )
			return fatal( std::string( keyName[key] ) + ": " + e );
		const std::string m = stage0( progs[key], 1, 0 );
		if ( !m.empty() )
			return fatal( std::string( keyName[key] ) + ": GL's program has no uniform " + m );
	}

	// GL's stage 0 as vaPassInner draws it, into GL's own tile (the sentinel first)
	auto glRun = [&]( int key, GLuint gbuf, int zDepth, int sizeShort, std::string & err, int rowsMore = 0, float distScale = kAiDistScale ) {
		fillTile( glTile );
		stage0( progs[key], zDepth, sizeShort, rowsMore, distScale );
		bindUnit( gbufUnit[key], GL_TEXTURE_2D, gbuf );
		f.bindFramebuffer( fFb, fbGl );
		drawState0();
		glViewport( 0, 0, kAiCols, kAiRows );
		ai.bindVertexArray( vao );
		glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr );
		f.bindFramebuffer( fFb, 0 );
		bindUnit( gbufUnit[key], GL_TEXTURE_2D, 0 );
		glFinish();
		if ( const GLenum e = glGetError() )
			err = "GL error " + std::to_string( e ) + " in GL's stage 0";
		return readTile( glTile );
	};

	int fails = 0, probesDead = 0, redsRun = 0, redsFired = 0;
	for ( int path = 0; path < 2; path++ )
		for ( int key = 0; key < 2; key++ ) {
			const bool zPath = path == 0;
			const std::string arm = std::string( zPath ? "depth " : "alpha " ) + keyName[key];
			const char * an = arm.c_str();
			const unsigned seed = unsigned( 1 + path * 2 + key );
			const std::vector<std::uint32_t> dz = depthData( seed ), dz2 = depthData( seed + 100 );
			const std::vector<float> da = alphaData( seed ), da2 = alphaData( seed + 100 );
			if ( zPath ) {
				putDepth( glDs, dz );
				putDepth( vkDs, dz );
				const auto a = readDepth( glDs ), b = readDepth( vkDs );
				int bad = 0, sky = 0;
				for ( size_t i = 0; i < a.size(); i++ ) {
					bad += a[i] != b[i] || a[i] != dz[i];
					sky += ( a[i] >> 8 ) == 0xFFFFFFu;
				}
				std::printf( "AIRINJECTVIEW %s depth twin: %d of %zu texels differ (sky %d) %s\n", an, bad, a.size(), sky, bad ? "FAIL" : "EXACT" );
				fails += bad != 0;
			} else {
				putAlpha( gb, da );
			}
			const GLuint gbufGl = zPath ? glDs : gb, gbufVk = zPath ? GLuint( vkDs ) : gb;
			const int z = zPath ? 1 : 0;
			std::string err;
			const std::vector<float> g = glRun( key, gbufGl, z, 0, err );
			if ( !err.empty() )
				return fatal( arm + ": " + err );
			{
				const Cov c = coverage( g );
				const int wantZero = zPath ? 0 : kAiZeroNearAlpha;
				const bool ok = c.sky == kAiSkyCols && c.mixed == kAiMixedCols && c.ground == kAiGroundCols && c.zeroNear == wantZero;
				std::printf( "AIRINJECTVIEW %s GL coverage: sky %d (want %d) mixed %d (want %d) ground %d (want %d) near 0 %d (want %d) %s\n", an, c.sky,
					kAiSkyCols, c.mixed, kAiMixedCols, c.ground, kAiGroundCols, c.zeroNear, wantZero, ok ? "OK" : "FAIL" );
				fails += !ok;
				const TileV self = judgeTile( g, g );
				std::printf( "AIRINJECTVIEW %s GL written: unwritten %d non-finite %d of %d\n", an, self.unwritten, self.nonFinite, self.texels );
				fails += self.unwritten != 0 || self.nonFinite != 0;
			}
			if ( zPath ) {	// GL on the shared depth = GL on its own
				const TileV s = judgeTile( g, glRun( key, vkDs, z, 0, err ) );
				const bool ok = s.ok() && s.diff == 0;
				std::printf( "AIRINJECTVIEW %s GL on the shared depth: %s (differ %d)\n", an, ok ? "EXACT" : "FAIL", s.diff );
				fails += !ok;
			}
			// ---- GL probes: each red's change reaches the stage
			auto probe = [&]( const char * what, const std::vector<float> & other ) {
				const TileV p = judgeTile( g, other );
				const bool live = p.moved >= kAiLive;
				std::printf( "AIRINJECTVIEW %s probe %s: moved %d of %d %s\n", an, what, p.moved, p.texels, live ? "LIVE" : "TESTS NOTHING" );
				probesDead += !live;
			};
			probe( "fogDistScale doubled", glRun( key, gbufGl, z, 0, err, 0, 2.0f * kAiDistScale ) );
			probe( "volGbufZ flipped", glRun( key, gbufGl, 1 - z, 0, err ) );
			probe( "volGbufSize.x one short", glRun( key, gbufGl, z, 1, err ) );
			probe( "volDims.y one more", glRun( key, gbufGl, z, 0, err, 1 ) );
			if ( zPath )
				putDepth( glDs, dz2 );
			else
				putAlpha( gb, da2 );
			probe( "input rewritten", glRun( key, gbufGl, z, 0, err ) );
			if ( zPath )
				putDepth( glDs, dz );
			else
				putAlpha( gb, da );
			if ( !err.empty() )
				return fatal( arm + " (probes): " + err );

#ifdef WW_VK_AIRINJECT
			auto vkRun = [&]( int red, std::string & w, WwVkPresent::AirInjectStat & st ) {
				fillTile( vkTile );
				stage0( progs[key], z, 0 );
				bindUnit( gbufUnit[key], GL_TEXTURE_2D, gbufVk );
				( void )WwVkPresent::takeAirInjectStat();
				WwVkPresent::AirInjectRun r;
				r.prog = progs[key];
				r.rt = key == 1;
				r.wwDefs = WwVkPresent::airInjectKey( ( key == 1 ) != ( ( red & 32 ) != 0 ) );
				r.cols = kAiCols;
				r.rows = kAiRows;
				r.tileKey = reinterpret_cast<std::uintptr_t>( &keys[0] );
				r.red = red & 15;
				const bool ok = WwVkPresent::airInjectRun( r, gw, w );
				st = WwVkPresent::takeAirInjectStat();
				bindUnit( gbufUnit[key], GL_TEXTURE_2D, 0 );
				return ok;
			};
			std::string w;
			WwVkPresent::AirInjectStat st;
			if ( !vkRun( 0, w, st ) ) {
				std::printf( "AIRINJECTVIEW %s: Vulkan refused: %s\n", an, w.c_str() );
				fails++;
				continue;
			}
			const std::vector<float> v0 = readTile( vkTile );
			{
				const TileV v = judgeTile( g, v0 );
				std::printf( "AIRINJECTVIEW %s Vulkan vs GL: %s (unwritten %d, non-finite %d, differ %d, past 1e-5 rel %d, max rel %.3g)\n", an, v.word(),
					v.unwritten, v.nonFinite, v.diff, v.bad, v.maxRel );
				fails += !v.ok();
				const int want = zPath ? 0 : 1;
				const bool cp = st.runs == 1 && st.copies == want;
				std::printf( "AIRINJECTVIEW %s copies: runs %d copies %d (want 1 run, %d copies) %s\n", an, st.runs, st.copies, want, cp ? "OK" : "FAIL" );
				fails += !cp;
			}
			for ( int red : { 1, 2, 4, 8, 16, 32 } ) {
				redsRun++;
				if ( red == 16 ) {
					if ( zPath )
						putDepth( vkDs, dz2 );
					else
						putAlpha( gb, da2 );
				}
				std::string rw;
				WwVkPresent::AirInjectStat rs;
				const bool ran = vkRun( red == 16 ? 0 : red, rw, rs );
				if ( red == 16 ) {
					if ( zPath )
						putDepth( vkDs, dz );
					else
						putAlpha( gb, da );
				}
				bool fired = false;
				if ( red == 32 )
					fired = !ran && rw.find( "key" ) != std::string::npos;
				else if ( ran )
					fired = !judgeTile( g, readTile( vkTile ) ).ok();
				std::printf( "AIRINJECTVIEW %s red %d: %s%s%s\n", an, red, fired ? "FAIL (fired)" : "PASS (dead)", ran ? "" : " refused: ",
					ran ? "" : rw.c_str() );
				redsFired += fired;
			}
			{
				std::string aw;
				WwVkPresent::AirInjectStat as;
				const bool ran = vkRun( 0, aw, as );
				const TileV v = judgeTile( v0, readTile( vkTile ) );
				const bool ok = ran && v.ok() && v.diff == 0;
				std::printf( "AIRINJECTVIEW %s rerun after the reds: %s (differ %d)%s%s\n", an, ok ? "EXACT" : "FAIL", v.diff, ran ? "" : " refused: ",
					ran ? "" : aw.c_str() );
				fails += !ok;
			}
#else
			( void )gbufVk;
			( void )da2;
#endif
		}
	std::printf( "AIRINJECTVIEW GL: %d fails, %d probes TESTS NOTHING\n", fails, probesDead );
#ifdef WW_VK_AIRINJECT
	const bool ok = fails == 0 && probesDead == 0 && redsRun > 0 && redsFired == redsRun;
	std::printf( "AIRINJECTVIEW %d fails, %d dead probes, reds %d of %d fired\n", fails, probesDead, redsFired, redsRun );
	std::printf( "AIRINJECTVIEW GATE %s\n", ok ? "OK" : "FAILED" );
	return ok ? 0 : 1;
#else
	( void )redsRun;
	( void )redsFired;
	( void )vkTile;
	std::printf( "AIRINJECTVIEW GATE FAILED (no route: WW_VK_AIRINJECT not defined; GL coverage and probes above)\n" );
	return 1;
#endif
}

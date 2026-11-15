// Lane VKFAR18 (2026-11-15, route (a) step 5): the reflection probe relight (cellreflprobe.cpp, cell_reflrelight.prog) on
// the present device: vkreflrelight.inc reflRelightRun draws one full-screen quad per todo tile into the SHARED probe atlas
// (sharedTarget type 0, 512 x 320 R11F_G11F_B10F, 5 levels) with the reflrelight_view_vs + reflrelight_view_ps /
// reflrelight_view_rt_ps twins, reading rpAlb / rpNd (shared colour cube arrays, 64 x 64 x 6 * 33, RGBA8 / RGBA16F) in place.
// GL's side is GL's own program (reflrelightdraw_prep.py, both define keys) into a GL-owned atlas of the same format, with
// the app's state: blend, depth, scissor off, colour mask all, the quad { -1 -1 0, 1 -1 0, -1 1 0, 1 1 0 } / { 0 1 2 2 1 3 }
// at location 0, viewport = the tile (k % 8 * 64, k / 8 * 64, 64, 64), rpLayer / rpTile / rpPos per draw. farloddraw.cpp's
// helpers (FLD_NO_MAIN) make and seed every other sampler and loose uniform; the switch profile is the app site's (it sets
// cellGiHalfOn, cellClusterOn, cellAoDecalOn off and ussContactOn 0). cellRestir and cellDdgiAtlas hold seeded floats.
// Both atlases are cleared to (0.25, 0.5, 0.75) first; tiles 0, 3, 9, 17 are drawn, tile 5 is held.
// Bar (set before the first run, never widened; farloddraw's colour bar in R11G11B10F codes):
//   per drawn tile, lit = texels where GL's level-0 word differs from the clear; lit >= 1000 of 4096 (an empty tile FAILS);
//   per channel (R bits 0-10, G 11-21, B 22-31: unsigned floats, monotonic as integers) |vk - gl| <= 1 code step on
//   >= 99.9 % of the tile's texels; EXACT = every word equal. The held tile reads the clear word on both sides (EXACT,
//   proves LOAD / STORE and the viewport). Levels 1-4 (glGenerateMipmap on both, GL's own box filter from level 0) are
//   printed, not judged: they follow level 0. Both keys judged.
// Reds (each must FAIL, never pass): red 1 rpTile one pixel off (the Vulkan side's b31), red 2 cellRestir rewritten from
// texel 1 on after GL's draw (RT key; a twin that does not read it passes), red 4 cellDdgiAtlas shifted one texel after
// GL's draw (RT key), red 8 the base twin asked for the RT program (wwDefs key): must REFUSE, named "key".
// Last line `REFLRELIGHTDRAW GATE OK` or `REFLRELIGHTDRAW GATE FAILED`.
#define FLD_NO_MAIN
#include "farloddraw.cpp"
#undef FLD_NO_MAIN

namespace rrd
{
using namespace fv;
using namespace fld;

const int kFace = 64, kCols = 8, kAtW = 512, kAtH = 320, kLevels = 5, kLayers = 6 * 33;
const int kTodo[4] = { 0, 3, 9, 17 }, kHeld = 5;

struct Xf
{
	void( WINAPI * vertexAttribPointer )( GLuint, GLint, GLenum, GLboolean, GLsizei, const void * ) = nullptr;
	void( WINAPI * enableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * texStorage2D )( GLenum, GLsizei, GLenum, GLsizei, GLsizei ) = nullptr;
	void( WINAPI * texStorage3D )( GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei ) = nullptr;
	void( WINAPI * clearTexImage )( GLuint, GLint, GLenum, GLenum, const void * ) = nullptr;
	void( WINAPI * genVertexArrays )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindVertexArray )( GLuint ) = nullptr;
	void( WINAPI * uniform1f )( GLint, GLfloat ) = nullptr;
	void( WINAPI * uniform3f )( GLint, GLfloat, GLfloat, GLfloat ) = nullptr;
	void( WINAPI * getActiveUniformBlockName )( GLuint, GLuint, GLsizei, GLsizei *, char * ) = nullptr;
};
Xf xf;

bool loadXf()
{
	return load( xf.vertexAttribPointer, "glVertexAttribPointer" ) && load( xf.enableVertexAttribArray, "glEnableVertexAttribArray" )
		&& load( xf.texStorage2D, "glTexStorage2D" ) && load( xf.texStorage3D, "glTexStorage3D" ) && load( xf.clearTexImage, "glClearTexImage" )
		&& load( xf.genVertexArrays, "glGenVertexArrays" ) && load( xf.bindVertexArray, "glBindVertexArray" )
		&& load( xf.uniform1f, "glUniform1f" ) && load( xf.uniform3f, "glUniform3f" )
		&& load( xf.getActiveUniformBlockName, "glGetActiveUniformBlockName" );
}

// one channel's code: R 0-10, G 11-21, B 22-31
int code( std::uint32_t w, int c )
{
	return c == 0 ? int( w & 0x7FFu ) : c == 1 ? int( ( w >> 11 ) & 0x7FFu ) : int( w >> 22 );
}

std::vector<std::uint32_t> readAtlas( GLuint tex, int level )
{
	const int w = std::max( kAtW >> level, 1 ), h = std::max( kAtH >> level, 1 );
	std::vector<std::uint32_t> out( size_t( w ) * size_t( h ) );
	glBindTexture( GL_TEXTURE_2D, tex );
	glGetTexImage( GL_TEXTURE_2D, level, GL_RGB, 0x8C3B /* GL_UNSIGNED_INT_10F_11F_11F_REV */, out.data() );
	glBindTexture( GL_TEXTURE_2D, 0 );
	return out;
}

struct TileV
{
	int lit = 0, differ = 0, past = 0, maxStep = 0;
	bool ok() const { return lit >= 1000 && past * 1000 <= kFace * kFace; }
	const char * word() const { return lit < 1000 ? "FAIL" : differ == 0 ? "EXACT" : ok() ? "BOUND" : "FAIL"; }
};

TileV judgeTile( const std::vector<std::uint32_t> & gl, const std::vector<std::uint32_t> & vk, std::uint32_t clear, int k )
{
	TileV v;
	const int tx = ( k % kCols ) * kFace, ty = ( k / kCols ) * kFace;
	for ( int y = ty; y < ty + kFace; y++ )
		for ( int x = tx; x < tx + kFace; x++ ) {
			const size_t i = size_t( y ) * size_t( kAtW ) + size_t( x );
			v.lit += gl[i] != clear;
			if ( gl[i] == vk[i] )
				continue;
			v.differ++;
			int m = 0;
			for ( int c = 0; c < 3; c++ )
				m = std::max( m, std::abs( code( gl[i], c ) - code( vk[i], c ) ) );
			v.maxStep = std::max( v.maxStep, m );
			v.past += m > 1;
		}
	return v;
}
}  // namespace rrd

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace fld;
	using namespace rrd;
	gTag = "REFLRELIGHTDRAW";
	if ( argc < 2 ) {
		std::printf( "usage: reflrelightdraw <prep dir>\nREFLRELIGHTDRAW GATE FAILED\n" );
		return 2;
	}
	const std::string dir = argv[1];
	auto fatal = [&]( const std::string & why ) {
		std::printf( "REFLRELIGHTDRAW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	const int S = 4, W = 256, H = 160;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwreflrelightdraw";
	RegisterClassA( &wc );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwreflrelightdraw", "reflection relight harness", WS_OVERLAPPEDWINDOW, 40, 40, 320, 200,
		nullptr, nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( dc, ChoosePixelFormat( dc, &pfd ), &pfd );
	HGLRC glrc = wglCreateContext( dc );
	wglMakeCurrent( dc, glrc );
	ShowWindow( wnd, SW_SHOWNOACTIVATE );
	SetWindowPos( wnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
	pump();
	if ( !loadAll() || !loadMore() || !loadXf() )
		return fatal( "GL entry points" );
	std::printf( "REFLRELIGHTDRAW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	std::string why;
	if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) )
		return fatal( "the view: " + why );

	// ---- the shared inputs as the app site makes them with the view on (cellreflprobe.cpp)
	static char keys[3];
	GLuint vkAtlas = 0, alb = 0, nd = 0;
	if ( !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &keys[0] ), 0, kAtW, kAtH, 1, 0x8C3A, vkAtlas, why, kLevels ) )
		return fatal( "the shared atlas: " + why );
	if ( !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &keys[1] ), 3, kFace, kFace, kLayers, 0x8058, alb, why )
		|| !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &keys[2] ), 3, kFace, kFace, kLayers, 0x881A, nd, why ) )
		return fatal( "the shared G-buffer cubes: " + why );
	GLuint glAtlas = 0;
	glGenTextures( 1, &glAtlas );
	glBindTexture( GL_TEXTURE_2D, glAtlas );
	xf.texStorage2D( GL_TEXTURE_2D, kLevels, 0x8C3A, kAtW, kAtH );
	glBindTexture( GL_TEXTURE_2D, 0 );
	{	// the G-buffer: albedo alpha 0 on a strip of every face (the miss branch), distance 50..500
		std::mt19937 rng( 7 );
		std::uniform_real_distribution<float> u01( 0.0f, 1.0f );
		std::vector<std::uint8_t> a( size_t( kFace ) * kFace * 4 );
		std::vector<std::uint16_t> n( size_t( kFace ) * kFace * 4 );
		auto f2h = []( float x ) {
			std::uint32_t b;
			std::memcpy( &b, &x, 4 );
			const int e = int( ( b >> 23 ) & 0xFF ) - 127 + 15;
			const std::uint32_t m = ( b >> 13 ) & 0x3FF, s = ( b >> 16 ) & 0x8000;
			return std::uint16_t( e <= 0 ? s : e >= 31 ? s | 0x7BFF : s | ( std::uint32_t( e ) << 10 ) | m );
		};
		for ( int z = 0; z < kLayers; z++ ) {
			for ( int y = 0; y < kFace; y++ )
				for ( int x = 0; x < kFace; x++ ) {
					const size_t i = ( size_t( y ) * kFace + size_t( x ) ) * 4;
					for ( int c = 0; c < 3; c++ )
						a[i + size_t( c )] = std::uint8_t( 32 + int( u01( rng ) * 200.0f ) );
					a[i + 3] = x < 6 ? 0 : 255;
					float nx = u01( rng ) * 2.0f - 1.0f, ny = u01( rng ) * 2.0f - 1.0f, nz = u01( rng ) * 2.0f - 1.0f;
					const float l = std::max( 1e-3f, std::sqrt( nx * nx + ny * ny + nz * nz ) );
					n[i] = f2h( nx / l );
					n[i + 1] = f2h( ny / l );
					n[i + 2] = f2h( nz / l );
					n[i + 3] = f2h( 50.0f + 450.0f * u01( rng ) );
				}
			glBindTexture( 0x9009, alb );
			f.texSubImage3D( 0x9009, 0, 0, 0, z, kFace, kFace, 1, GL_RGBA, GL_UNSIGNED_BYTE, a.data() );
			glBindTexture( 0x9009, nd );
			f.texSubImage3D( 0x9009, 0, 0, 0, z, kFace, kFace, 1, GL_RGBA, 0x140B /* GL_HALF_FLOAT */, n.data() );
		}
		glBindTexture( 0x9009, 0 );
	}

	const char * keyName[2] = { "base", "rt" };
	int fails = 0, redsFired = 0, redsRun = 0;
	for ( int key = 0; key < 2; key++ ) {
		const GLuint prog = program( readFile( dir + "/" + keyName[key] + ".vert.glsl" ), readFile( dir + "/" + keyName[key] + ".frag.glsl" ),
			keyName[key] );
		if ( !prog )
			return fatal( std::string( "GL program " ) + keyName[key] );
		// ---- samplers: rpAlb / rpNd the shared cubes on the app's units, the rest made and seeded by farloddraw's table
		std::vector<Smp> smp;
		int iRestir = -1, iDdgi = -1;
		GLint nu = 0;
		f.getProgramiv( prog, fActiveUnis, &nu );
		for ( GLint i = 0; i < nu; i++ ) {
			char nm[256] = {};
			GLint sz = 0;
			GLenum ty = 0;
			f.getActiveUniform( prog, GLuint( i ), 256, nullptr, &sz, &ty, nm );
			Smp s;
			s.name = nm;
			s.glType = ty;
			if ( s.name == "rpAlb" || s.name == "rpNd" ) {
				s.kind = 4;
				s.tex = s.name == "rpAlb" ? alb : nd;
			} else if ( !smpOf( s, W, H ) ) {
				const bool sampler = ( ty >= 0x8B5D && ty <= 0x8B64 ) || ( ty >= 0x8DC0 && ty <= 0x8DD8 ) || ( ty >= 0x9008 && ty <= 0x9110 );
				if ( sampler )
					return fatal( "sampler " + s.name + " of a GL type the gate does not make (" + std::to_string( ty ) + ")" );
				continue;
			}
			if ( s.name == "cellRestir" || s.name == "cellDdgiAtlas" ) {
				s.fmt = 0x8814;
				s.bufBytes = 1 << 20;
				( s.name == "cellRestir" ? iRestir : iDdgi ) = int( smp.size() );
			}
			s.unit = int( smp.size() ) + 1;
			smp.push_back( s );
		}
		std::printf( "REFLRELIGHTDRAW %s: %zu samplers active in GL's program:", keyName[key], smp.size() );
		for ( const Smp & s : smp )
			std::printf( " %s%s", s.name.c_str(), s.unbound ? "(unbound)" : "" );
		std::printf( "\n" );
		if ( key == 1 && ( iRestir < 0 || iDdgi < 0 ) )
			return fatal( "the RT program has no cellRestir / cellDdgiAtlas (not the RT GI key)" );
		std::mt19937 rng( 11 + unsigned( key ) );
		for ( Smp & s : smp )
			if ( s.kind != 4 || s.compare ) {
				if ( !fill( s, rng ) )
					return fatal( "fill " + s.name );
			}
		std::vector<float> seeds[2];
		for ( int at : { iRestir, iDdgi } )
			if ( at >= 0 ) {
				std::vector<float> & v = seeds[at == iDdgi];
				v.resize( size_t( smp[size_t( at )].bufBytes ) / 4 );
				std::uniform_real_distribution<float> u( 0.05f, 1.0f );
				for ( float & x : v )
					x = u( rng );
				f.bindBuffer( 0x8C2A, smp[size_t( at )].buf );
				f.bufferData( 0x8C2A, std::ptrdiff_t( v.size() * 4 ), v.data(), fStatic );
				f.bindBuffer( 0x8C2A, 0 );
			}
		// ---- blocks: every one bound to its index, zero, globalUniforms with an identity view and a sun (fargrounddraw's layout)
		GLint nb = 0;
		f.getProgramiv( prog, 0x8A36 /* GL_ACTIVE_UNIFORM_BLOCKS */, &nb );
		std::vector<GLuint> ubo( size_t( std::max( nb, 1 ) ) );
		f.genBuffers( GLsizei( ubo.size() ), ubo.data() );
		for ( GLint b = 0; b < nb; b++ ) {
			char nm[128] = {};
			xf.getActiveUniformBlockName( prog, GLuint( b ), 128, nullptr, nm );
			GLint bytes = 16;
			f.getActiveUniformBlockiv( prog, GLuint( b ), fBlockSize, &bytes );
			f.uniformBlockBinding( prog, GLuint( b ), GLuint( b ) );
			std::vector<std::uint8_t> z( size_t( std::max( bytes, 16 ) ), 0 );
			if ( std::string( nm ) == "globalUniforms" && bytes >= 176 ) {
				const float one = 1.0f, L[3] = { 0.3f, 0.8f, 0.52f };
				for ( int c = 0; c < 3; c++ ) {
					std::memcpy( &z[size_t( 16 * c + 4 * c )], &one, 4 );
					std::memcpy( &z[size_t( 160 + 4 * c )], &L[c], 4 );
				}
			}
			f.bindBuffer( fUbo, ubo[size_t( b )] );
			f.bufferData( fUbo, std::ptrdiff_t( z.size() ), z.data(), fStatic );
			f.bindBufferBase( fUbo, GLuint( b ), ubo[size_t( b )] );
			std::printf( "REFLRELIGHTDRAW %s block %s %d bytes\n", keyName[key], nm, bytes );
		}
		// ---- loose uniforms: the app site's switch profile, rpViewFromWorld identity, rpRed 0, rpSkyBelow 1
		_putenv( "FLD_ON=cellOn,fogOn,volOn,cellShadowOn,cellShadowFadeOn,ussOn,ussCsmOn,ussTerrOn,ussCloudOn,cellHasDir,cellGiOn,"
				 "cellLightCount,cellRestirOn,cellDdgiOn,rpSkyBelow" );
		const int seeded = seedUnis( prog, rng );
		f.useProgram( prog );
		const float I[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
		f.uniformMatrix4fv( f.getUniformLocation( prog, "rpViewFromWorld" ), 1, GL_FALSE, I );
		for ( const Smp & s : smp )
			f.uniform1i( f.getUniformLocation( prog, s.name.c_str() ), s.unit );
		std::printf( "REFLRELIGHTDRAW %s: %d loose uniforms seeded\n", keyName[key], seeded );

		// ---- both atlases cleared to (0.25, 0.5, 0.75) at every level
		const float clr[3] = { 0.25f, 0.5f, 0.75f };
		for ( int l = 0; l < kLevels; l++ ) {
			xf.clearTexImage( glAtlas, l, GL_RGB, GL_FLOAT, clr );
			xf.clearTexImage( vkAtlas, l, GL_RGB, GL_FLOAT, clr );
		}
		const std::uint32_t clearWord = readAtlas( glAtlas, 0 )[0];

		// ---- GL's relight: the app's state and quad, one draw per todo tile
		GLuint vao = 0, vb = 0, ib = 0;
		xf.genVertexArrays( 1, &vao );
		xf.bindVertexArray( vao );
		static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
		static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
		f.genBuffers( 1, &vb );
		f.genBuffers( 1, &ib );
		f.bindBuffer( 0x8892, vb );
		f.bufferData( 0x8892, sizeof( quad ), quad, fStatic );
		f.bindBuffer( 0x8893, ib );
		f.bufferData( 0x8893, sizeof( idx ), idx, fStatic );
		xf.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 12, nullptr );
		xf.enableVertexAttribArray( 0 );
		for ( const Smp & s : smp )
			bindUnit( s.unit, targetOf( s.kind ), s.unbound ? 0 : s.tex );
		const GLuint fbGl = fbo( GL_TEXTURE_2D, glAtlas, 0, 0 );
		if ( !fbGl )
			return fatal( "the GL atlas framebuffer" );
		drawState0();
		const float pos[3] = { 120.0f, -340.0f, 96.0f };
		for ( int k : kTodo ) {
			const int tx = ( k % kCols ) * kFace, ty = ( k / kCols ) * kFace;
			glViewport( tx, ty, kFace, kFace );
			xf.uniform1f( f.getUniformLocation( prog, "rpLayer" ), float( k ) );
			xf.uniform3f( f.getUniformLocation( prog, "rpTile" ), float( tx ), float( ty ), float( kFace ) );
			xf.uniform3f( f.getUniformLocation( prog, "rpPos" ), pos[0] + 40.0f * float( k ), pos[1], pos[2] );
			glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr );
		}
		f.bindFramebuffer( fFb, 0 );
		glFinish();
		if ( const GLenum e = glGetError() )
			return fatal( "GL error " + std::to_string( e ) + " in GL's relight" );
		const std::vector<std::uint32_t> gl0 = readAtlas( glAtlas, 0 );
		for ( int k : kTodo )	// GL's own coverage first: an empty tile proves nothing
			std::printf( "REFLRELIGHTDRAW %s GL tile %d: lit %d of %d\n", keyName[key], k, judgeTile( gl0, gl0, clearWord, k ).lit, kFace * kFace );

		// ---- the present device's relight into the shared atlas (the reds after GL's draw)
		auto vkRun = [&]( int red, std::string & w ) -> bool {
#ifdef WW_VK_REFLRELIGHT
			for ( int l = 0; l < kLevels; l++ )
				xf.clearTexImage( vkAtlas, l, GL_RGB, GL_FLOAT, clr );
			if ( red & 6 ) {	// gate-side reds: an RT GI input moved after GL's draw (the twin must read it)
				const Smp & s = smp[size_t( red & 2 ? iRestir : iDdgi )];
				std::vector<float> v( size_t( s.bufBytes ) / 4, 0.0f );
				f.bindBuffer( 0x8C2A, s.buf );
				( reinterpret_cast<void( WINAPI * )( GLenum, std::ptrdiff_t, std::ptrdiff_t, void * )>( glProc( "glGetBufferSubData" ) ) )(
					0x8C2A, 0, std::ptrdiff_t( v.size() * 4 ), v.data() );
				if ( red & 2 )
					std::fill( v.begin() + 4, v.end(), 0.5f );
				else
					std::rotate( v.begin(), v.begin() + 4, v.end() );
				f.bufferData( 0x8C2A, std::ptrdiff_t( v.size() * 4 ), v.data(), fStatic );
				f.bindBuffer( 0x8C2A, 0 );
			}
			WwVkPresent::ReflRelightRun r;
			r.prog = prog;
			r.wwDefs = ( key == 1 ) != ( ( red & 8 ) != 0 ) ? "WW_CELL_FOGLEAK+WW_CELL_RESTIR+WW_CELL_RTDDGI+WW_CELL_RTDDGI_CASCADES+WW_FRAME_UBO"
														 : "WW_CELL_FOGLEAK+WW_FRAME_UBO";
			r.rt = key == 1;
			r.atlasKey = reinterpret_cast<std::uintptr_t>( &keys[0] );
			r.albKey = reinterpret_cast<std::uintptr_t>( &keys[1] );
			r.ndKey = reinterpret_cast<std::uintptr_t>( &keys[2] );
			for ( int k : kTodo ) {
				WwVkPresent::ReflRelightTile t;
				t.layer = k;
				t.tile[0] = float( ( k % kCols ) * kFace ) + ( red & 1 ? 1.0f : 0.0f );
				t.tile[1] = float( ( k / kCols ) * kFace );
				t.tile[2] = float( kFace );
				t.pos[0] = pos[0] + 40.0f * float( k );
				t.pos[1] = pos[1];
				t.pos[2] = pos[2];
				r.tiles.push_back( t );
			}
			const bool ok = WwVkPresent::reflRelightRun( r, gw, w );
			if ( red & 6 ) {	// put GL's input back for the next run
				f.bindBuffer( 0x8C2A, smp[size_t( red & 2 ? iRestir : iDdgi )].buf );
				const std::vector<float> & v = seeds[( red & 2 ) == 0];
				f.bufferData( 0x8C2A, std::ptrdiff_t( v.size() * 4 ), v.data(), fStatic );
				f.bindBuffer( 0x8C2A, 0 );
			}
			return ok;
#else
			( void )red;
			w = "no route (vkreflrelight.inc is not built: WW_VK_REFLRELIGHT undefined)";
			return false;
#endif
		};
		std::string w;
		if ( !vkRun( 0, w ) ) {
			std::printf( "REFLRELIGHTDRAW %s: Vulkan refused: %s\n", keyName[key], w.c_str() );
			fails++;
			continue;
		}
		const std::vector<std::uint32_t> vk0 = readAtlas( vkAtlas, 0 );
		for ( int k : kTodo ) {
			const TileV v = judgeTile( gl0, vk0, clearWord, k );
			std::printf( "REFLRELIGHTDRAW %s tile %d: %s (lit %d, differ %d, past 1 step %d, max step %d)\n", keyName[key], k, v.word(), v.lit,
				v.differ, v.past, v.maxStep );
			fails += !v.ok();
		}
		{
			const TileV h = judgeTile( gl0, vk0, clearWord, kHeld );
			const bool held = h.lit == 0 && h.differ == 0;
			std::printf( "REFLRELIGHTDRAW %s held tile %d: %s (GL moved %d, differ %d)\n", keyName[key], kHeld, held ? "EXACT" : "FAIL", h.lit, h.differ );
			fails += !held;
		}
		for ( int l = 1; l < kLevels; l++ ) {	// info: GL's mips of each side
			glBindTexture( GL_TEXTURE_2D, glAtlas );
			f.generateMipmap( GL_TEXTURE_2D );
			glBindTexture( GL_TEXTURE_2D, vkAtlas );
			f.generateMipmap( GL_TEXTURE_2D );
			glBindTexture( GL_TEXTURE_2D, 0 );
			const std::vector<std::uint32_t> a = readAtlas( glAtlas, l ), b = readAtlas( vkAtlas, l );
			size_t d = 0;
			for ( size_t i = 0; i < a.size(); i++ )
				d += a[i] != b[i];
			std::printf( "REFLRELIGHTDRAW %s level %d (info): %zu of %zu words differ\n", keyName[key], l, d, a.size() );
			break;
		}
		// ---- reds
		for ( int red : { 1, 2, 4, 8 } ) {
			if ( ( red & 6 ) && key != 1 )
				continue;
			redsRun++;
			std::string rw;
			const bool ran = vkRun( red, rw );
			bool fired = false;
			if ( red == 8 )
				fired = !ran && rw.find( "key" ) != std::string::npos;
			else if ( ran ) {
				const std::vector<std::uint32_t> vr = readAtlas( vkAtlas, 0 );
				for ( int k : kTodo )
					fired = fired || !judgeTile( gl0, vr, clearWord, k ).ok();
			}
			std::printf( "REFLRELIGHTDRAW %s red %d: %s%s%s\n", keyName[key], red, fired ? "FAIL (fired)" : "PASS (dead)", ran ? "" : " refused: ",
				ran ? "" : rw.c_str() );
			redsFired += fired;
		}
	}
	const bool ok = fails == 0 && redsRun > 0 && redsFired == redsRun;
	std::printf( "REFLRELIGHTDRAW %d fails, reds %d of %d fired\n", fails, redsFired, redsRun );
	std::printf( "REFLRELIGHTDRAW GATE %s\n", ok ? "OK" : "FAILED" );
	return ok ? 0 : 1;
}

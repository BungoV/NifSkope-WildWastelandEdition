// Lane VKFAR3 (2026-11-14, row G3): the far ring 0's live ground in the shader-decals option (vkfarground.inc farGroundRun:
// the fargnd_view_vs / fargnd_view_ps twins of fo4_default.vert + far-ground/fo4_cellcsmfgl.frag, one non-instanced command
// with firstIndex / baseVertex as CellFarGround::vkDraw passes them) against GL's own program (fargrounddraw_prep.py), per
// sample, into the same 4x MSAA RGBA16F + D24S8 frame contents. farloddraw.cpp's helpers are reused (FLD_NO_MAIN): the
// sampler table, the seeded uniforms and blocks, the judge and its bar. GL's side is read by the shipped farGroundStateOf.
// Scene: one terrain grid (a third of its rows wound clockwise) in view space behind 900 leading degenerate elements and a
// dummy vertex block (firstIndex 900, baseVertex 10), a depth slab over the left half, GL's polygon offset (1, 2) on. The far
// ground's own data as cellfarground.cpp uploads it: fgGrid RGBA32UI 16 x 16 (four LTEX ids, unorm8 weights, vertex colour),
// fgLtex RGBA8 2D array 64 px x 4 layers, mipped, REPEAT, anisotropy 8, fgDecal RGBA32F records + 4 x 4 bins + ids, fgAtlas
// RGBA8 mipped; cellRow maps view space onto the grid's world square, fgMode 1, fgDecalsOn 1. fgGrid, fgLtex and fgAtlas are
// shared targets as on the view (no copy a run): GL on them vs GL on plain twins must be EXACT, and seed 1's plain-twin run
// must copy exactly three more images; seed 2 lifts the atlas lod clamp (fgBinMap.w 3).
// Bar (farloddraw's, never widened): covered >= 1000; one side only <= 0.05 %; depth within 1 D24 unit; colour rel <= 1e-2 on
// >= 99.9 % of the samples covered on both sides; GL >= 1000 distinct colours over the ground.
// Reds (each must FAIL, never refuse; seed 1): 1 the ps loose block a vec4 off, 2 plain samplers, 4 cull flipped, 8 depth
// ALWAYS, 16 polygon offset dropped (vkwater.inc); gate-side: 64 the element offset passed in bytes (vkDraw's idxOff / 2
// dropped), 128 the grid's weights rewritten after GL's draw (a twin that does not read fgGrid passes), 256 the decal records
// moved after GL's draw (fgDecal not read). Last line `FARGROUNDDRAW GATE OK` or `FARGROUNDDRAW GATE FAILED`.
#define FLD_NO_MAIN
#include "farloddraw.cpp"
#undef FLD_NO_MAIN

namespace fgd
{
using namespace fv;
using namespace fld;

const int kGn = 16, kBins = 4, kRec = 8, kBinOff = kRec * 5, kIdOff = kBinOff + kBins * kBins;
const float kOx = 4300.0f, kOy = 7000.0f, kSpacing = 128.0f, kBin = 512.0f, kTiling = 400.0f;

std::uint32_t pack4x8( float a, float b, float c, float d )
{
	auto q = []( float v ) { return std::uint32_t( std::lround( std::clamp( v, 0.0f, 1.0f ) * 255.0f ) ); };
	return q( a ) | ( q( b ) << 8 ) | ( q( c ) << 16 ) | ( q( d ) << 24 );
}

// the grid: per sample ids 0|1, ids 2|3 (65535 = the engine default's grey), weights unorm8 x 4, colour RGB8 + page 255
std::vector<std::uint32_t> gridData( std::mt19937 & rng, bool redo )
{
	std::uniform_real_distribution<float> u01( 0.0f, 1.0f );
	std::vector<std::uint32_t> g( size_t( kGn * kGn * 4 ) );
	for ( int k = 0; k < kGn * kGn; k++ ) {
		std::uint32_t id[4];
		for ( auto & i : id )
			i = ( rng() & 7u ) == 7u ? 65535u : rng() & 3u;
		float w[4];
		for ( auto & x : w )
			x = ( rng() & 3u ) == 0u ? 0.0f : u01( rng );
		if ( redo )	// red 128: the weights turned round
			std::swap( w[0], w[3] ), std::swap( w[1], w[2] );
		g[size_t( k * 4 )] = id[0] | ( id[1] << 16 );
		g[size_t( k * 4 + 1 )] = id[2] | ( id[3] << 16 );
		g[size_t( k * 4 + 2 )] = pack4x8( w[0], w[1], w[2], w[3] );
		g[size_t( k * 4 + 3 )] = pack4x8( 0.5f + 0.5f * u01( rng ), 0.5f + 0.5f * u01( rng ), 0.5f + 0.5f * u01( rng ), 1.0f );
	}
	return g;
}

// the decal buffer: kRec records of 5 texels (position + scale, rotation, box xy, box z + texel size, atlas rect), then
// (start, count) a bin, then the record ids 4 a texel; every bin lists every record (the walk's box test picks)
std::vector<float> decalData( std::mt19937 & rng, float shift, int texels )
{
	std::uniform_real_distribution<float> u01( 0.0f, 1.0f );
	std::vector<float> d( size_t( texels * 4 ), 0.0f );
	for ( int r = 0; r < kRec; r++ ) {
		float * p = &d[size_t( r * 5 * 4 )];
		const float a = r % 3 == 0 ? 0.6f * float( r + 1 ) : 0.0f, sz = 0.8f + 0.6f * u01( rng );
		const float px = 4450.0f + 1100.0f * u01( rng ) + shift, py = 7200.0f + 1300.0f * u01( rng );
		const float rec[20] = { px, py, 300.0f, sz, 0.0f, 0.0f, std::sin( 0.5f * a ), std::cos( 0.5f * a ),
			-150.0f - 60.0f * u01( rng ), -110.0f, 150.0f, 110.0f + 50.0f * u01( rng ), -600.0f, 600.0f, 1.0f, 0.0f,
			0.05f + 0.2f * u01( rng ), 0.05f + 0.2f * u01( rng ), 0.6f + 0.3f * u01( rng ), 0.6f + 0.3f * u01( rng ) };
		std::copy( rec, rec + 20, p );
	}
	for ( int b = 0; b < kBins * kBins; b++ ) {
		d[size_t( ( kBinOff + b ) * 4 )] = float( b * kRec );
		d[size_t( ( kBinOff + b ) * 4 + 1 )] = float( kRec );
	}
	for ( int k = 0; k < kBins * kBins * kRec; k++ )
		d[size_t( kIdOff * 4 + k )] = float( ( k + k / kRec ) % kRec );	// each bin its own record order
	return d;
}

// BATCH1117: GL's program from the HLSL-built SPIR-V pair (fargrounddraw_prep.py), as the app's wwSpvAttach links it
// (glcontext.cpp): glShaderBinary 0x9551 + glSpecializeShader("main"); the blocks keep their decorated bindings.
GLuint spvProgram( const std::string & vsBin, const std::string & psBin )
{
	using ShaderBinary = void( WINAPI * )( GLsizei, const GLuint *, GLenum, const void *, GLsizei );
	using Specialize = void( WINAPI * )( GLuint, const char *, GLuint, const GLuint *, const GLuint * );
	auto bin = reinterpret_cast<ShaderBinary>( wglGetProcAddress( "glShaderBinary" ) );
	auto spec = reinterpret_cast<Specialize>( wglGetProcAddress( "glSpecializeShader" ) );
	if ( !bin || !spec || vsBin.size() < 20 || psBin.size() < 20 ) {
		std::printf( "FARGROUNDDRAW SPIR-V: %s\n", !bin || !spec ? "no glShaderBinary / glSpecializeShader" : "no prog.vert.spv / prog.frag.spv" );
		return 0;
	}
	const GLuint p = f.createProgram();
	const std::string * src[2] = { &vsBin, &psBin };
	for ( int s = 0; s < 2; s++ ) {
		const GLuint sh = f.createShader( s ? fFrag : fVert );
		bin( 1, &sh, 0x9551, src[s]->data(), GLsizei( src[s]->size() ) );
		spec( sh, "main", 0, nullptr, nullptr );
		GLint ok = 0;
		f.getShaderiv( sh, fCompiled, &ok );
		if ( !ok ) {
			std::vector<char> log( 65536 );
			f.getShaderInfoLog( sh, GLsizei( log.size() ), nullptr, log.data() );
			std::printf( "FARGROUNDDRAW SPIR-V %s stage refused: %.1500s\n", s ? "pixel" : "vertex", log.data() );
			return 0;
		}
		f.attachShader( p, sh );
	}
	f.linkProgram( p );
	GLint ok = 0;
	f.getProgramiv( p, fLinked, &ok );
	if ( !ok ) {
		std::vector<char> log( 65536 );
		f.getProgramInfoLog( p, GLsizei( log.size() ), nullptr, log.data() );
		std::printf( "FARGROUNDDRAW SPIR-V link failed: %.1500s\n", log.data() );
		return 0;
	}
	return p;
}
}  // namespace fgd

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace fld;
	using namespace fgd;
	gTag = "FARGROUNDDRAW";
	if ( argc < 2 ) {
		std::printf( "usage: fargrounddraw <prep dir>\nFARGROUNDDRAW GATE FAILED\n" );
		return 2;
	}
	const std::string dir = argv[1];
	auto fatal = [&]( const std::string & why ) {
		std::printf( "FARGROUNDDRAW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	const int S = 4, W = 256, H = 160, WS = W * S;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwfargrounddraw";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwfargrounddraw", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, 320, 180 };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwfargrounddraw", "far ground draw harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
		rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	const int attr[] = { 0x2001, 1, 0x2010, 1, 0x2011, 1, 0x2003, 0x2027, 0x2013, 0x202B, 0x2015, 8, 0x2017, 8, 0x2019, 8,
		0x201B, 8, 0x2022, 24, 0x2023, 8, 0x2041, 1, 0x2042, 4, 0 };
	int fmt = 0;
	UINT nf = 0;
	if ( !choose || !choose( dc, attr, nullptr, 1, &fmt, &nf ) || !nf )
		return fatal( "no 4x MSAA pixel format" );
	PIXELFORMATDESCRIPTOR got {};
	DescribePixelFormat( dc, fmt, sizeof( got ), &got );
	SetPixelFormat( dc, fmt, &got );
	HGLRC glrc = wglCreateContext( dc );
	wglMakeCurrent( dc, glrc );
	wglDeleteContext( drc );
	ReleaseDC( dummy, ddc );
	DestroyWindow( dummy );
	ShowWindow( wnd, SW_SHOWNOACTIVATE );
	SetWindowPos( wnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
	pump();
	if ( !loadAll() || !loadMore() )
		return fatal( "GL entry points" );
	std::printf( "FARGROUNDDRAW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	const GLuint pUnpack = program( kTriVs, kUnpackFs, "unpack" ), pDepth = program( kTriVs, kDepthFs, "depth" ),
		pSlab = program( kTriVs, kSlabFs, "slab" );
	// BATCH1117: GL's side is the HLSL-built program (the HLSL is the reference); the legacy .frag only names the sampler
	// types SPIR-V does not carry (a shadow sampler reads as its plain type) and is run instead with FARGROUNDDRAW_LEGACY=1
	const char * legacyEnv = std::getenv( "FARGROUNDDRAW_LEGACY" );
	const bool legacy = legacyEnv && std::string( legacyEnv ) == "1";
	const GLuint pLegacy = program( readFile( dir + "/prog.vert.glsl" ), readFile( dir + "/prog.frag.glsl" ), "fo4_cellcsmfgl legacy" );
	const GLuint pFg = legacy ? pLegacy : spvProgram( readFile( dir + "/prog.vert.spv" ), readFile( dir + "/prog.frag.spv" ) );
	std::printf( "FARGROUNDDRAW GL program: %s (legacy fo4_cellcsmfgl.frag %s, info only)\n",
		legacy ? "the legacy fo4_cellcsmfgl.frag (FARGROUNDDRAW_LEGACY=1)" : "HLSL-built SPIR-V fo4_cellcsmfgl@WW_CELL_FOGLEAK+WW_FRAME_UBO",
		pLegacy ? "links" : "does not link" );
	if ( !pUnpack || !pDepth || !pSlab || !pFg || !pLegacy )
		return fatal( "GL programs" );
	std::map<std::string, GLenum> legacyType;
	{
		GLint n = 0;
		f.getProgramiv( pLegacy, fActiveUnis, &n );
		for ( GLint i = 0; i < n; i++ ) {
			char nm[256] = {};
			GLint sz = 0;
			GLenum ty = 0;
			f.getActiveUniform( pLegacy, GLuint( i ), 256, nullptr, &sz, &ty, nm );
			legacyType[nm] = ty;
		}
	}
	std::string why;
	unsigned fc = 0, fds = 0;
	if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) )
		return fatal( "the view: " + why );
	if ( !WwVkPresent::toneFrame( W, H, S, fc, fds, why ) )
		return fatal( "toneFrame: " + why );
	const GLuint fbFrame = fbo( fT2DMS, fc, 0, fds );
	const GLuint refCol = msTex( W, H, fRgba16f, S ), refDs = msTex( W, H, fD24s8, S );
	const GLuint fbRef = fbo( fT2DMS, refCol, 0, refDs );
	const GLuint unp = tex2d( WS, H, fRgba16f, GL_RGBA, GL_FLOAT, nullptr, false );
	const GLuint fbUnp = fbo( GL_TEXTURE_2D, unp, 0, 0 );
	const GLuint unpD = tex2d( WS, H, fRgba32f, GL_RGBA, GL_FLOAT, nullptr, false );
	const GLuint fbUnpD = fbo( GL_TEXTURE_2D, unpD, 0, 0 );
	if ( !fbFrame || !fbRef || !fbUnp || !fbUnpD )
		return fatal( "GL targets" );

	// ---- the program's samplers: one unit each; the far ground's four as cellfarground.cpp makes them
	std::vector<Smp> smp;
	int iGrid = -1, iLtex = -1, iDecal = -1, iAtlas = -1;
	{
		GLint n = 0;
		f.getProgramiv( pFg, fActiveUnis, &n );
		for ( GLint i = 0; i < n; i++ ) {
			char nm[256] = {};
			GLint sz = 0;
			GLenum ty = 0;
			f.getActiveUniform( pFg, GLuint( i ), 256, nullptr, &sz, &ty, nm );
			Smp s;
			s.name = nm;
			s.glType = legacyType.count( s.name ) ? legacyType[s.name] : ty;
			if ( !smpOf( s, W, H ) ) {
				const bool sampler = ( ty >= 0x8B5D && ty <= 0x8B64 ) || ( ty >= 0x8DC0 && ty <= 0x8DD8 ) || ( ty >= 0x9008 && ty <= 0x9110 );
				if ( sampler )
					return fatal( "sampler " + s.name + " of a GL type the gate does not make (0x" + std::to_string( ty ) + ")" );
				continue;
			}
			const int at = int( smp.size() );
			if ( s.name == "fgGrid" ) {
				s.unbound = false;	// made below (RGBA32UI, nearest)
				s.fmt = 0x8D70;
				s.w = s.h = kGn;
				iGrid = at;
			} else if ( s.name == "fgLtex" ) {
				s.fmt = 0x8058;	// RGBA8, mipped, REPEAT (fill), anisotropy 8 (below)
				s.w = s.h = 64;
				s.d = 4;
				s.levels = 7;
				iLtex = at;
			} else if ( s.name == "fgDecal" ) {
				iDecal = at;	// RGBA32F texel buffer (smpOf), its contents below
			} else if ( s.name == "fgAtlas" ) {
				s.fmt = 0x8058;	// mipped (cellfarground.cpp's atlas; seed 2 lifts fgBinMap.w's lod clamp off 0)
				s.w = s.h = 64;
				s.levels = 7;
				iAtlas = at;
			}
			s.unit = at + 1;
			smp.push_back( s );
		}
	}
	if ( iGrid < 0 || iLtex < 0 || iDecal < 0 || iAtlas < 0 )
		return fatal( "the program has no fgGrid / fgLtex / fgDecal / fgAtlas (not the shader-decals far ground)" );
	{
		std::string miss;
		int nMiss = 0;
		for ( const WwVkPresent::FxField & fd : WwVkPresent::kT_fargndPs )
			if ( f.getUniformLocation( pFg, fd.name ) < 0 ) {
				nMiss++;
				miss += std::string( " " ) + fd.name;
			}
		for ( const WwVkPresent::FxField & fd : WwVkPresent::kT_waterVs )
			if ( f.getUniformLocation( pFg, fd.name ) < 0 ) {
				nMiss++;
				miss += std::string( " vs:" ) + fd.name;
			}
		std::printf( "FARGROUNDDRAW %d twin fields GL has no location for:%s\n", nMiss, miss.c_str() );
	}
	std::printf( "FARGROUNDDRAW %zu samplers active in GL's program:", smp.size() );
	for ( const Smp & s : smp )
		std::printf( " %s%s", s.name.c_str(), s.unbound ? "(unbound)" : "" );
	std::printf( "\n" );

	// ---- the blocks: globalUniforms (the frame's camera), wwShadowFrame (seeded per member), skinningUniforms (zero)
	const float n0 = 10.0f, f0 = 100000.0f, fy = 1.0f / std::tan( 0.5f * 1.0471976f ), asp = float( W ) / float( H );
	float proj[16] = {};
	proj[0] = fy / asp;
	proj[5] = fy;
	proj[10] = ( f0 + n0 ) / ( n0 - f0 );
	proj[11] = -1.0f;
	proj[14] = 2.0f * f0 * n0 / ( n0 - f0 );
	GLuint ubo[3] = {};
	f.genBuffers( 3, ubo );
	const char * blockName[3] = { "globalUniforms", "wwShadowFrame", "skinningUniforms" };
	GLint blockSize[3] = { 336, 248, 12288 };
	GLint blockAt[3] = { 0, 1, 2 };  // BATCH1117: a SPIR-V program keeps its decorated binding (the app sets none), read here
	for ( int k = 0; k < 3; k++ ) {
		const GLuint bi = f.getUniformBlockIndex( pFg, blockName[k] );
		if ( bi != 0xFFFFFFFFu ) {
			if ( legacy )
				f.uniformBlockBinding( pFg, bi, GLuint( k ) );
			else
				f.getActiveUniformBlockiv( pFg, bi, 0x8A3F, &blockAt[k] );  // GL_UNIFORM_BLOCK_BINDING
			f.getActiveUniformBlockiv( pFg, bi, fBlockSize, &blockSize[k] );
		}
		for ( int j = 0; j < k; j++ )
			if ( blockAt[j] == blockAt[k] )
				return fatal( std::string( "blocks " ) + blockName[j] + " and " + blockName[k] + " share binding " + std::to_string( blockAt[k] ) );
		std::vector<std::uint8_t> z( size_t( std::max( blockSize[k], 16 ) ), 0 );
		f.bindBuffer( fUbo, ubo[k] );
		f.bufferData( fUbo, std::ptrdiff_t( z.size() ), z.data(), fStatic );
	}
	std::printf( "FARGROUNDDRAW blocks: globalUniforms %d, wwShadowFrame %d, skinningUniforms %d bytes; bindings %d %d %d\n", blockSize[0],
		blockSize[1], blockSize[2], blockAt[0], blockAt[1], blockAt[2] );
	struct ShMember
	{
		std::string name;
		GLint off = 0, type = 0, size = 1, stride = 0;
	};
	auto membersOf = [&]( GLuint q ) {
		std::vector<ShMember> out;
		const GLuint sbi = f.getUniformBlockIndex( q, "wwShadowFrame" );
		GLint n = 0;
		f.getProgramiv( q, fActiveUnis, &n );
		for ( GLint i = 0; i < n && sbi != 0xFFFFFFFFu; i++ ) {
			const GLuint idx = GLuint( i );
			GLint bi = -1;
			f.getActiveUniformsiv( q, 1, &idx, fBlockIndex, &bi );
			if ( bi != GLint( sbi ) )
				continue;
			ShMember m;
			char nm[256] = {};
			GLenum ty = 0;
			f.getActiveUniform( q, idx, 256, nullptr, &m.size, &ty, nm );
			m.name = nm;
			m.type = GLint( ty );
			f.getActiveUniformsiv( q, 1, &idx, 0x8A3B, &m.off );     // GL_UNIFORM_OFFSET
			f.getActiveUniformsiv( q, 1, &idx, 0x8A3C, &m.stride );  // GL_UNIFORM_ARRAY_STRIDE
			out.push_back( m );
		}
		return out;
	};
	// BATCH1117: seeded in the legacy program's member order (the same world on either program); the SPIR-V block must hold
	// the same members at the same offsets (a bool reads uint there)
	const std::vector<ShMember> shMembers = membersOf( pLegacy );
	{
		auto key = []( std::vector<ShMember> v ) {
			std::vector<std::string> k;
			for ( const ShMember & m : v ) {
				const GLint t = m.type == 0x8B56 || m.type == 0x1404 ? 0x1405 : m.type;  // bool / int / uint: one 4-byte word
				k.push_back( std::to_string( m.off ) + "/" + std::to_string( t ) + "/" + std::to_string( m.size ) + "/" + std::to_string( m.stride ) );
			}
			std::sort( k.begin(), k.end() );
			return k;
		};
		const std::vector<ShMember> spv = membersOf( pFg );
		const bool same = key( shMembers ) == key( spv );
		std::printf( "FARGROUNDDRAW wwShadowFrame members: legacy %zu, GL's program %zu, layout %s\n", shMembers.size(), spv.size(),
			same ? "the same" : "DIFFERS" );
		if ( !same && !shMembers.empty() && !spv.empty() )
			return fatal( "wwShadowFrame's layout differs between the legacy and the HLSL-built program" );
	}
	auto blocks = [&]( std::mt19937 & rng ) {
		std::uniform_real_distribution<float> u01( 0.05f, 1.0f );
		std::vector<std::uint8_t> g( size_t( blockSize[0] ), 0 );
		auto putF = [&]( size_t at, float x ) { std::memcpy( &g[at], &x, 4 ); };
		for ( int c = 0; c < 3; c++ ) {
			putF( size_t( 16 * c + 4 * c ), 1.0f );       // viewMatrix identity
			putF( size_t( 48 + 16 * c + 4 * c ), 1.0f );  // envMapRotation identity
		}
		for ( int k = 0; k < 16; k++ )
			putF( size_t( 96 + 4 * k ), proj[k] );
		const float L[3] = { 0.3f, 0.8f, 0.52f };
		for ( int k = 0; k < 3; k++ )
			putF( size_t( 160 + 4 * k ), L[k] );
		for ( size_t at = 176; at < 272; at += 4 )
			if ( at >= 208 || ( at % 16 ) != 12 )
				putF( at, u01( rng ) );
		for ( size_t at = 272; at < 288; at += 4 )
			putF( at, u01( rng ) );
		const GLint vpd[4] = { 0, 0, W, H };
		std::memcpy( &g[288], vpd, 16 );
		putF( 320, u01( rng ) );
		putF( 324, u01( rng ) );
		f.bindBuffer( fUbo, ubo[0] );
		ex.bufferSubData( fUbo, 0, std::ptrdiff_t( g.size() ), g.data() );
		std::vector<std::uint8_t> sh( size_t( blockSize[1] ), 0 );
		for ( const ShMember & m : shMembers ) {
			int comp = 1;
			bool isInt = false;
			switch ( m.type ) {
			case 0x1406: comp = 1; break;
			case 0x8B50: comp = 2; break;
			case 0x8B51: comp = 3; break;
			case 0x8B52: comp = 4; break;
			case 0x8B5C: comp = 16; break;
			case 0x1404: case 0x8B56: case 0x1405: comp = 1, isInt = true; break;
			case 0x8B53: case 0x8B57: case 0x8DC6: comp = 2, isInt = true; break;
			case 0x8B54: case 0x8B58: case 0x8DC7: comp = 3, isInt = true; break;
			case 0x8B55: case 0x8B59: case 0x8DC8: comp = 4, isInt = true; break;
			default: continue;
			}
			const bool off = m.name.find( "View" ) != std::string::npos || m.name.find( "Probe" ) != std::string::npos
				|| m.name.find( "Red" ) != std::string::npos || m.name.find( "red" ) != std::string::npos
				|| m.name.find( "Dbg" ) != std::string::npos || m.name.find( "Debug" ) != std::string::npos || extraZero( m.name );
			for ( GLint e = 0; e < std::max( m.size, 1 ); e++ )
				for ( int c = 0; c < comp; c++ ) {
					const size_t at = size_t( m.off ) + size_t( e ) * size_t( m.stride ) + size_t( 4 * c );
					if ( at + 4 > sh.size() )
						continue;
					if ( isInt ) {
						const GLint v = off ? 0 : GLint( rng() & 1u );
						std::memcpy( &sh[at], &v, 4 );
					} else {
						const float x = off ? 0.0f : u01( rng );
						std::memcpy( &sh[at], &x, 4 );
					}
				}
		}
		f.bindBuffer( fUbo, ubo[1] );
		ex.bufferSubData( fUbo, 0, std::ptrdiff_t( sh.size() ), sh.data() );
	};

	// ---- the terrain shape in view space (modelViewMatrix identity): a dummy vertex block first (baseVertex 10) and 900
	// leading degenerate elements (firstIndex 900, idxOff 1800 bytes); 1800 trailing ones keep red 64's shifted range inside
	struct Vtx
	{
		float p[3];
		std::uint8_t c[4];
		std::uint32_t n, t, b;
		float uv[2];
	};
	static_assert( sizeof( Vtx ) == 36, "the interleaved stride" );
	std::vector<Vtx> vtx( 10, Vtx {} );
	std::vector<std::uint16_t> idx( 900, 0 );
	const int GX = 40, GZ = 48, base = int( vtx.size() );
	auto height = []( float x, float z ) { return -110.0f + 22.0f * std::sin( x * 0.009f ) * std::cos( z * 0.011f ) + 6.0f * std::sin( x * 0.05f ); };
	for ( int j = 0; j < GZ; j++ )
		for ( int i = 0; i < GX; i++ ) {
			const float x = -650.0f + 1300.0f * float( i ) / float( GX - 1 ), z = -150.0f - 1450.0f * float( j ) / float( GZ - 1 );
			const float e = 0.5f, y = height( x, z ), dx = ( height( x + e, z ) - height( x - e, z ) ) / ( 2.0f * e ),
				dz = ( height( x, z + e ) - height( x, z - e ) ) / ( 2.0f * e );
			float nn[3] = { -dx, 1.0f, -dz };
			const float nl = std::sqrt( nn[0] * nn[0] + nn[1] * nn[1] + nn[2] * nn[2] );
			const float tl = std::sqrt( 1.0f + dx * dx ), bl = std::sqrt( 1.0f + dz * dz );
			Vtx v {};
			v.p[0] = x, v.p[1] = y, v.p[2] = z;
			v.c[0] = std::uint8_t( 120 + 135 * i / GX ), v.c[1] = 180, v.c[2] = std::uint8_t( 120 + 135 * j / GZ ), v.c[3] = 255;
			v.n = pack1010102( nn[0] / nl, nn[1] / nl, nn[2] / nl, 1.0f );
			v.t = pack1010102( 1.0f / tl, dx / tl, 0.0f, 1.0f );
			v.b = pack1010102( 0.0f, -dz / bl, -1.0f / bl, 1.0f );
			v.uv[0] = x / 250.0f, v.uv[1] = z / 250.0f;
			vtx.push_back( v );
		}
	const std::uint32_t first = std::uint32_t( idx.size() );
	for ( int j = 0; j + 1 < GZ; j++ )
		for ( int i = 0; i + 1 < GX; i++ ) {
			const std::uint16_t a = std::uint16_t( j * GX + i ), b = std::uint16_t( a + 1 ), c = std::uint16_t( a + GX ), d = std::uint16_t( c + 1 );
			if ( j % 3 == 1 )
				idx.insert( idx.end(), { a, c, b, b, c, d } );  // clockwise seen from above: culled
			else
				idx.insert( idx.end(), { a, b, c, b, d, c } );
		}
	const std::uint32_t count = std::uint32_t( idx.size() ) - first;
	idx.insert( idx.end(), 1800, std::uint16_t( 0 ) );
	const long long idxOff = (long long) first * 2;	// what bsshape passes (WwBl::geo.idxOff, bytes)
	GLuint vao = 0, vb = 0, eb = 0;
	ex.genVertexArrays( 1, &vao );
	ex.bindVertexArray( vao );
	f.genBuffers( 1, &vb );
	f.bindBuffer( 0x8892, vb );
	f.bufferData( 0x8892, std::ptrdiff_t( vtx.size() * sizeof( Vtx ) ), vtx.data(), fStatic );
	ex.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 36, reinterpret_cast<const void *>( 0 ) );
	ex.vertexAttribPointer( 1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 36, reinterpret_cast<const void *>( 12 ) );
	for ( int k = 0; k < 3; k++ )
		ex.vertexAttribPointer( GLuint( 2 + k ), 4, 0x8D9F, GL_TRUE, 36, reinterpret_cast<const void *>( std::uintptr_t( 16 + 4 * k ) ) );
	ex.vertexAttribPointer( 7, 2, GL_FLOAT, GL_FALSE, 36, reinterpret_cast<const void *>( 28 ) );
	for ( GLuint l : { 0u, 1u, 2u, 3u, 4u, 7u } )
		ex.enableVertexAttribArray( l );
	f.genBuffers( 1, &eb );
	f.bindBuffer( 0x8893, eb );
	f.bufferData( 0x8893, std::ptrdiff_t( idx.size() * 2 ), idx.data(), fStatic );
	ex.bindVertexArray( 0 );
	const float zm = -700.0f, zd = ( ( proj[10] * zm + proj[14] ) / -zm ) * 0.5f + 0.5f;
	std::printf( "FARGROUNDDRAW job: %dx%d x%d, %zu vertices (baseVertex %d), %u elements from %u, grid %d x %d, %d decals in %d x %d bins, "
		"slab depth %.6f\n", W, H, S, vtx.size(), base, count, first, kGn, kGn, kRec, kBins, kBins, zd );

	auto readColor = [&]( GLuint msColor ) {
		std::vector<std::uint16_t> out( size_t( WS ) * H * 4 );
		f.bindFramebuffer( fFb, fbUnp );
		glViewport( 0, 0, WS, H );
		drawState0();
		f.useProgram( pUnpack );
		bindUnit( 0, fT2DMS, msColor );
		u1i( pUnpack, "src", 0 );
		u1i( pUnpack, "S", S );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		glReadPixels( 0, 0, WS, H, GL_RGBA, fHalf, out.data() );
		bindUnit( 0, fT2DMS, 0 );
		return out;
	};
	auto readDepth = [&]( GLuint msDs, bool shared ) {
		std::vector<float> out( size_t( WS ) * H );
		glBindTexture( fT2DMS, msDs );
		glTexParameteri( fT2DMS, 0x90EA, GL_DEPTH_COMPONENT );
		glBindTexture( fT2DMS, 0 );
		f.bindFramebuffer( fFb, fbUnpD );
		glViewport( 0, 0, WS, H );
		drawState0();
		f.useProgram( pDepth );
		bindUnit( 0, fT2DMS, msDs );
		u1i( pDepth, "src", 0 );
		u1i( pDepth, "S", S );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		glReadPixels( 0, 0, WS, H, fRed, GL_FLOAT, out.data() );
		bindUnit( 0, fT2DMS, 0 );
		if ( shared ) {
			glBindTexture( fT2DMS, msDs );
			glTexParameteri( fT2DMS, 0x90EA, GL_STENCIL_INDEX );
			glBindTexture( fT2DMS, 0 );
		}
		return out;
	};
	auto before = [&]( GLuint fb ) {
		f.bindFramebuffer( fFb, fb );
		glViewport( 0, 0, W, H );
		drawState0();
		glDepthMask( GL_TRUE );
		glStencilMask( 0xFF );
		glClearColor( 0.3f, 0.45f, 0.7f, 0.5f );
		glClearDepth( 1.0 );
		glClearStencil( 0 );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
		glEnable( GL_SCISSOR_TEST );
		glScissor( 0, 0, W / 2, H );
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_ALWAYS );
		f.useProgram( pSlab );
		f.uniform1fv( f.getUniformLocation( pSlab, "zd" ), 1, &zd );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		drawState0();
		glDepthFunc( GL_LESS );
	};
	// the terrain's state: opaque, depth LEQUAL written, back faces culled, a polygon offset
	auto drawState = [&]( GLuint fb, bool scissor ) {
		f.bindFramebuffer( fFb, fb );
		glViewport( 0, 0, W, H );
		drawState0();
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		glDepthMask( GL_TRUE );
		glEnable( GL_CULL_FACE );
		glCullFace( GL_BACK );
		glFrontFace( GL_CCW );
		glEnable( 0x8037 );  // GL_POLYGON_OFFSET_FILL
		glPolygonOffset( 1.0f, 2.0f );
		if ( scissor ) {
			glEnable( GL_SCISSOR_TEST );
			glScissor( W / 5, H / 6, W / 2, H / 2 );
		}
		f.useProgram( pFg );
		for ( int k = 0; k < 3; k++ )
			f.bindBufferBase( fUbo, GLuint( blockAt[k] ), ubo[k] );
		for ( const Smp & s : smp ) {
			bindUnit( s.unit, targetOf( s.kind ), s.tex );
			f.uniform1i( f.getUniformLocation( pFg, s.name.c_str() ), s.unit );
		}
		ex.bindVertexArray( vao );
	};
	auto stateEnd = [&]() {
		ex.bindVertexArray( 0 );
		for ( const Smp & s : smp )
			bindUnit( s.unit, targetOf( s.kind ), 0 );
		glDisable( 0x8037 );
		glPolygonOffset( 0.0f, 0.0f );
		drawState0();
		glDepthFunc( GL_LESS );
	};
	auto setI = [&]( const char * n, GLint v ) {
		const GLint l = f.getUniformLocation( pFg, n );
		if ( l >= 0 )
			f.uniform1iv( l, 1, &v );
	};
	auto set4 = [&]( const char * n, float a, float b, float c, float d ) {
		const GLint l = f.getUniformLocation( pFg, n );
		const float v[4] = { a, b, c, d };
		if ( l >= 0 )
			f.uniform4fv( l, 1, v );
	};
	auto putGrid = [&]( const std::vector<std::uint32_t> & g ) {
		glBindTexture( GL_TEXTURE_2D, smp[size_t( iGrid )].tex );
		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, kGn, kGn, 0x8D99 /* GL_RGBA_INTEGER */, GL_UNSIGNED_INT, g.data() );
		glBindTexture( GL_TEXTURE_2D, 0 );
	};
	auto putDecals = [&]( const std::vector<float> & d ) {
		f.bindBuffer( 0x8C2A, smp[size_t( iDecal )].buf );
		ex.bufferSubData( 0x8C2A, 0, std::ptrdiff_t( d.size() * 4 ), d.data() );
	};
	const int decalTexels = smp[size_t( iDecal )].bufBytes / 16;
	if ( kIdOff + ( kBins * kBins * kRec + 3 ) / 4 > decalTexels )
		return fatal( "the decal buffer is too small for the fixture" );

	static char twinKeys[192];
	bool gateOk = true;
	int runs = 0;
	// fgGrid, fgLtex and fgAtlas on the present device as cellfarground.cpp makes them on the view (sharedTarget: the twin
	// reads them in place, no copy a run); plain GL twins of the same contents for the GL-vs-GL check and the copy count
	using CopyImg = void( WINAPI * )( GLuint, GLenum, GLint, GLint, GLint, GLint, GLuint, GLenum, GLint, GLint, GLint, GLint, GLsizei,
		GLsizei, GLsizei );
	const auto copyImg = reinterpret_cast<CopyImg>( wglGetProcAddress( "glCopyImageSubData" ) );
	if ( !copyImg )
		return fatal( "glCopyImageSubData" );
	static char sharedKeys[3];
	const int iShared[3] = { iGrid, iLtex, iAtlas };
	GLuint plain[3] = {};
	for ( int k = 0; k < 3; k++ ) {
		Smp & s = smp[size_t( iShared[k] )];
		const unsigned tg = targetOf( s.kind );
		unsigned t = 0;
		if ( !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( sharedKeys + k ), s.kind == 2 ? 2 : 0, s.w, s.h, s.d, s.fmt, t,
				why, s.levels ) )
			return fatal( "sharedTarget " + s.name + ": " + why );
		s.tex = t;
		glGenTextures( 1, &plain[k] );
		for ( const GLuint x : { GLuint( t ), plain[k] } ) {
			glBindTexture( tg, x );
			if ( x == plain[k] ) {
				if ( s.kind == 0 )
					ex.texStorage2D( tg, s.levels, s.fmt, s.w, s.h );
				else
					ex.texStorage3D( tg, s.levels, s.fmt, s.w, s.h, s.d );
			}
			const bool mip = s.levels > 1, lin = k != 0;	// the grid: nearest, clamped
			glTexParameteri( tg, GL_TEXTURE_MIN_FILTER, mip ? GL_LINEAR_MIPMAP_LINEAR : lin ? GL_LINEAR : GL_NEAREST );
			glTexParameteri( tg, GL_TEXTURE_MAG_FILTER, lin ? GL_LINEAR : GL_NEAREST );
			glTexParameteri( tg, GL_TEXTURE_WRAP_S, mip ? GL_REPEAT : 0x812F );
			glTexParameteri( tg, GL_TEXTURE_WRAP_T, mip ? GL_REPEAT : 0x812F );
			glTexParameteri( tg, fMaxLevel, s.levels - 1 );
			if ( k == 1 )
				glTexParameterf( tg, 0x84FE /* GL_TEXTURE_MAX_ANISOTROPY */, 8.0f );
		}
		glBindTexture( tg, 0 );
	}
	auto toPlain = [&]() {	// the shared contents into the plain twins, every level
		for ( int k = 0; k < 3; k++ ) {
			const Smp & s = smp[size_t( iShared[k] )];
			const GLenum tg = targetOf( s.kind );
			for ( int l = 0; l < s.levels; l++ )
				copyImg( s.tex, tg, l, 0, 0, 0, plain[k], tg, l, 0, 0, 0, std::max( s.w >> l, 1 ), std::max( s.h >> l, 1 ), s.kind == 2 ? s.d : 1 );
		}
	};
	auto swapPlain = [&]() {
		for ( int k = 0; k < 3; k++ )
			std::swap( smp[size_t( iShared[k] )].tex, plain[k] );
	};
	WwVkPresent::FxStat tot;
	auto take = [&]() {	// one run's stat into the total
		const WwVkPresent::FxStat st = WwVkPresent::takeFarGroundDrawStat();
		tot.runs += st.runs, tot.fails += st.fails, tot.cpuMs += st.cpuMs, tot.maxMs = std::max( tot.maxMs, st.maxMs );
		tot.pipelines = st.pipelines, tot.buildMs = st.buildMs, tot.copies += st.copies;	// the build counts are running totals
		return st;
	};
	take();
	for ( int seed = 1; seed <= 2; seed++ ) {
		std::mt19937 rng( 20261114u + std::uint32_t( seed ) * 7919u
			+ std::uint32_t( getenv( "FLD_SEED" ) ? std::atoi( getenv( "FLD_SEED" ) ) : 0 ) );
		for ( Smp & s : smp ) {
			if ( int( &s - smp.data() ) == iGrid ) {
				if ( !s.tex ) {
					glGenTextures( 1, &s.tex );
					glBindTexture( GL_TEXTURE_2D, s.tex );
					ex.texStorage2D( GL_TEXTURE_2D, 1, 0x8D70, kGn, kGn );
					glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
					glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
					glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F );
					glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F );
					glBindTexture( GL_TEXTURE_2D, 0 );
				}
				continue;
			}
			if ( !fill( s, rng ) )
				return fatal( "texture " + s.name );
		}
		glBindTexture( 0x8C1A, smp[size_t( iLtex )].tex );
		glTexParameterf( 0x8C1A, 0x84FE /* GL_TEXTURE_MAX_ANISOTROPY */, 8.0f );
		glBindTexture( 0x8C1A, 0 );
		std::mt19937 rngFg( 4242u + std::uint32_t( seed ) );
		const std::vector<std::uint32_t> grid = gridData( rngFg, false );
		std::mt19937 rngRedo( 4242u + std::uint32_t( seed ) );
		const std::vector<std::uint32_t> gridRed = gridData( rngRedo, true );
		std::mt19937 rngDc( 777u + std::uint32_t( seed ) ), rngDcRed( 777u + std::uint32_t( seed ) );
		const std::vector<float> decals = decalData( rngDc, 0.0f, decalTexels ), decalsRed = decalData( rngDcRed, 90.0f, decalTexels );
		putGrid( grid );
		putDecals( decals );
		if ( glGetError() != GL_NO_ERROR )
			return fatal( "the far ground's data upload" );
		g_profileSeed = seed;
		const int nu = seedUnis( pLegacy, rng, pFg );	// the legacy program's order and types: the same world on either program
		blocks( rng );
		const bool scissor = seed == 2;
		f.useProgram( pFg );
		setI( "farInst", 0 );
		setI( "fgMode", 1 );
		setI( "fgDecalsOn", 1 );
		set4( "fgGridMap", kOx, kOy, 1.0f / kSpacing, kTiling );
		{
			const GLint l = f.getUniformLocation( pFg, "fgGridN" );
			const float v[2] = { float( kGn ), float( kGn ) };
			if ( l >= 0 )
				f.uniform2fv( l, 1, v );
		}
		set4( "fgBinMap", kOx, kOy, 1.0f / kBin, seed == 2 ? 3.0f : 0.0f );	// .w: the atlas lod clamp (cellfarground.cpp's maxLod)
		set4( "fgBinN", float( kBins ), float( kBins ), float( kBinOff ), float( kIdOff ) );
		{	// view space onto the grid's world square: world = (x + 5000, -z + 7000, y + 300)
			const GLint l = f.getUniformLocation( pFg, "cellRow" );
			const float rows[12] = { 1.0f, 0.0f, 0.0f, 5000.0f, 0.0f, 0.0f, -1.0f, 7000.0f, 0.0f, 1.0f, 0.0f, 300.0f };
			if ( l >= 0 )
				f.uniform4fv( l, 3, rows );
			else
				std::printf( "FARGROUNDDRAW note: cellRow has no location\n" );
		}
		{
			const GLint l = f.getUniformLocation( pFg, "fogDistScale" );
			const float v = 1.0f / 1500.0f;
			if ( l >= 0 )
				f.uniform1fv( l, 1, &v );
		}
		std::printf( "FARGROUNDDRAW seed %d: %d loose uniforms seeded%s, fgMode 1, decals on\n", seed, nu, scissor ? ", a scissor" : "" );

		auto glDraw = [&]() {
			ex.drawInst( GL_TRIANGLES, GLsizei( count ), GL_UNSIGNED_SHORT, reinterpret_cast<const void *>( std::uintptr_t( idxOff ) ), 1, base, 0 );
		};
		before( fbRef );
		const std::vector<std::uint16_t> bgC = readColor( refCol );
		const std::vector<float> bgD = readDepth( refDs, false );
		before( fbRef );
		drawState( fbRef, scissor );
		glDraw();
		stateEnd();
		const GLenum ge = glGetError();
		if ( ge != GL_NO_ERROR )
			return fatal( "the GL reference draw: GL error " + std::to_string( ge ) );
		const std::vector<std::uint16_t> glC = readColor( refCol );
		const std::vector<float> glD = readDepth( refDs, false );
		{	// GL on the shared inputs vs GL on plain twins of the same bytes: EXACT (the shared storage changes nothing GL reads)
			toPlain();
			swapPlain();
			before( fbRef );
			drawState( fbRef, scissor );
			glDraw();
			stateEnd();
			swapPlain();
			const std::vector<std::uint16_t> plC = readColor( refCol );
			const std::vector<float> plD = readDepth( refDs, false );
			size_t cDiff = 0, dDiff = 0;
			for ( size_t i = 0; i < plC.size() && i < glC.size(); i++ )
				cDiff += plC[i] != glC[i];
			for ( size_t i = 0; i < plD.size() && i < glD.size(); i++ )
				dDiff += plD[i] != glD[i];
			const bool exact = !cDiff && !dDiff && plC.size() == glC.size() && plD.size() == glD.size() && glGetError() == GL_NO_ERROR;
			std::printf( "FARGROUNDDRAW seed %d GL shared vs plain inputs: %s (%zu colour halves, %zu depth samples differ)\n", seed,
				exact ? "EXACT" : "DIFFER", cDiff, dDiff );
			gateOk = gateOk && exact;
		}
		{
			float lo[4] = { 1e30f, 1e30f, 1e30f, 1e30f }, hi[4] = { -1e30f, -1e30f, -1e30f, -1e30f };
			std::set<std::uint64_t> distinct;
			for ( size_t s = 0; s < glD.size(); s++ ) {
				if ( glD[s] == bgD[s] )
					continue;
				std::uint64_t key = 0;
				for ( int c = 0; c < 4; c++ ) {
					const float x = h2f( glC[s * 4 + size_t( c )] );
					lo[c] = std::min( lo[c], x ), hi[c] = std::max( hi[c], x );
					key = ( key << 16 ) | glC[s * 4 + size_t( c )];
				}
				distinct.insert( key );
			}
			std::printf( "FARGROUNDDRAW seed %d GL colour over the ground: r %.4g..%.4g g %.4g..%.4g b %.4g..%.4g a %.4g..%.4g, %zu distinct\n", seed,
				lo[0], hi[0], lo[1], hi[1], lo[2], hi[2], lo[3], hi[3], distinct.size() );
			if ( distinct.size() < 1000 ) {
				std::printf( "FARGROUNDDRAW seed %d: under 1000 distinct colours over the ground: a near-constant layer is blind\n", seed );
				gateOk = false;
			}
		}

		int copiesShared = -1, copiesPlain = -1;
		for ( int pick : { 0, -1, 1, 2, 4, 8, 16, 64, 128, 256 } ) {
			const bool viaPlain = pick < 0;	// seed 1: the same draw on the plain twins (copied a run): the copy count's other side
			const int red = viaPlain ? 0 : pick;
			if ( ( red || viaPlain ) && seed != 1 )
				continue;
			if ( viaPlain )
				swapPlain();
			if ( red == 128 )
				putGrid( gridRed );
			if ( red == 256 )
				putDecals( decalsRed );
			before( fbFrame );
			drawState( fbFrame, scissor );
			WwVkPresent::WaterRun r;
			WwVkPresent::farGroundStateOf( &gw, pFg, r );
			r.w = W;
			r.h = H;
			r.color = fc;
			WwVkPresent::WaterCmd c;	// CellFarGround::vkDraw's command
			c.count = count;
			c.instanceCount = 1;
			c.firstIndex = std::uint32_t( red == 64 ? idxOff : idxOff / 2 );
			c.baseVertex = base;
			c.baseInstance = 0;
			r.cmds.assign( 1, c );
			r.key = reinterpret_cast<std::uintptr_t>( twinKeys );
			r.red = red & 31;
			const bool ran = WwVkPresent::farGroundRun( r, why );
			stateEnd();
			if ( viaPlain )
				swapPlain();
			if ( red == 128 )
				putGrid( grid );
			if ( red == 256 )
				putDecals( decals );
			runs++;
			const int copies = take().copies;
			if ( !red )
				( viaPlain ? copiesPlain : copiesShared ) = copies;
			if ( !ran ) {
				std::printf( "FARGROUNDDRAW seed %d %s: REFUSED (%s)\n", seed, viaPlain ? "plain inputs" : ( "red " + std::to_string( red ) ).c_str(),
					why.c_str() );
				gateOk = false;
				continue;
			}
			const std::vector<std::uint16_t> vkC = readColor( fc );
			const std::vector<float> vkD = readDepth( fds, true );
			const Verdict v = judge( bgC, bgD, vkC, vkD, glC, glD );
			const char * word = v.word();
			const bool fail = std::strcmp( word, "FAIL" ) == 0;
			std::printf( "FARGROUNDDRAW seed %d %s %d: %s covered %zu (both %zu, one side only %zu), depth max %ld D24 units (%zu past 1), "
				"colour %zu halves differ, %zu samples past 1e-2 (max rel %.3g), %d copies%s\n", seed, viaPlain ? "plain inputs, red" : "red", red,
				word, v.covered, v.both, v.onlyOne, v.maxDepthU, v.depthPast, v.halvesDiff, v.colPast, v.maxRel, copies,
				red ? ( fail ? " -> red FAILS as it must" : " -> red PASSES: the gate is blind" ) : "" );
			if ( red ? !fail : fail )
				gateOk = false;
		}
		if ( seed == 1 ) {	// the three shared inputs never copied: exactly three copies fewer a run than on the plain twins
			const bool fewer = copiesShared >= 0 && copiesPlain - copiesShared == 3;
			std::printf( "FARGROUNDDRAW seed 1 copies a run: %d on the shared inputs, %d on plain ones (%s)\n", copiesShared, copiesPlain,
				fewer ? "fgGrid, fgLtex, fgAtlas read in place" : "NOT three fewer" );
			gateOk = gateOk && fewer;
		}
	}
	std::printf( "FARGROUNDDRAW %d runs: %d on Vulkan, %.2f ms CPU each (max %.2f; the harness's hand-off wait included), %d fails, "
		"%d pipelines built in %.1f ms (%.2f ms CPU a run without them)\n", runs, tot.runs, tot.runs ? tot.cpuMs / tot.runs : 0.0, tot.maxMs,
		tot.fails, tot.pipelines, tot.buildMs, tot.runs ? ( tot.cpuMs - tot.buildMs ) / tot.runs : 0.0 );
	{
		// round 2: what a remade fgLtex costs at a real size (128 layers of 512^2 RGBA8 + mips, ~179 MB): made, filled, dropped
		using Clk = std::chrono::steady_clock;
		auto ms = []( Clk::time_point a ) { return std::chrono::duration<double, std::milli>( Clk::now() - a ).count(); };
		const int L = 128, side = 512;
		std::vector<std::uint32_t> pix( size_t( side ) * side * L, 0xFF808080u );
		static char bigKey;
		const auto key = reinterpret_cast<std::uintptr_t>( &bigKey );
		for ( int round = 0; round < 2; round++ ) {
			glFinish();
			const auto t0 = Clk::now();
			unsigned t = 0;
			if ( !WwVkPresent::sharedTarget( key, 2, side, side, L, 0x8058, t, why, 10 ) ) {
				std::printf( "FARGROUNDDRAW remake cost: REFUSED (%s)\n", why.c_str() );
				gateOk = false;
				break;
			}
			const double make = ms( t0 );
			const auto t1 = Clk::now();
			const GLenum arr = 0x8C1A;	// GL_TEXTURE_2D_ARRAY
			glBindTexture( arr, t );
			f.texSubImage3D( arr, 0, 0, 0, 0, side, side, L, 0x80E1 /* GL_BGRA */, GL_UNSIGNED_BYTE, pix.data() );
			f.generateMipmap( arr );
			glFinish();
			const double fill = ms( t1 );
			const auto t2 = Clk::now();
			WwVkPresent::sharedTargetDrop( key );
			const double drop = ms( t2 );
			std::printf( "FARGROUNDDRAW remake cost %d (fgLtex %d layers, %.0f MB): made %.1f ms, filled + mips %.1f ms, dropped %.1f ms\n",
				round + 1, L, double( side ) * side * 4.0 * L * 4.0 / 3.0 / 1048576.0, make, fill, drop );
		}
	}
	{
		/* lane VKFAR4 (batching, option a): three ring-0 tiles (own vertex / element buffers, own modelViewMatrix) in ONE
		 * crossing (farGroundRunN; farGroundQueue + farGroundFlush, the app's route) against GL's three draws (depth max 0,
		 * colour in the bar above) and EXACT against three single crossings. Reds on tile 1 (1024 tile 0's set and host
		 * slice, 2048 the tile dropped) must FAIL; tiles 1 and 2 on another fgDecal buffer split the batch in two crossings;
		 * one single run after the batches equals the one before (twinRun records its own draw again). */
		struct Tile
		{
			GLuint vao = 0, vb = 0, eb = 0;
			float mv[16] = {};
		};
		Tile tiles[3];
		const GLint lMv = f.getUniformLocation( pFg, "modelViewMatrix" );
		if ( lMv < 0 ) {
			std::printf( "FARGROUNDDRAW batch: modelViewMatrix has no location (the tiles' own loose blocks are not exercised)\n" );
			gateOk = false;
		}
		for ( int k = 0; k < 3; k++ ) {
			Tile & t = tiles[k];
			for ( int i = 0; i < 16; i++ )
				t.mv[i] = i % 5 == 0 ? 1.0f : 0.0f;
			t.mv[12] = -300.0f + 300.0f * float( k ), t.mv[13] = 10.0f * float( k ), t.mv[14] = -40.0f * float( k );
			if ( !k ) {
				t.vao = vao, t.vb = vb, t.eb = eb;
				continue;
			}
			std::vector<Vtx> lifted( vtx );
			for ( Vtx & v : lifted )
				v.p[1] += 8.0f * float( k );
			ex.genVertexArrays( 1, &t.vao );
			ex.bindVertexArray( t.vao );
			f.genBuffers( 1, &t.vb );
			f.bindBuffer( 0x8892, t.vb );
			f.bufferData( 0x8892, std::ptrdiff_t( lifted.size() * sizeof( Vtx ) ), lifted.data(), fStatic );
			ex.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 36, reinterpret_cast<const void *>( 0 ) );
			ex.vertexAttribPointer( 1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 36, reinterpret_cast<const void *>( 12 ) );
			for ( int a = 0; a < 3; a++ )
				ex.vertexAttribPointer( GLuint( 2 + a ), 4, 0x8D9F, GL_TRUE, 36, reinterpret_cast<const void *>( std::uintptr_t( 16 + 4 * a ) ) );
			ex.vertexAttribPointer( 7, 2, GL_FLOAT, GL_FALSE, 36, reinterpret_cast<const void *>( 28 ) );
			for ( GLuint l : { 0u, 1u, 2u, 3u, 4u, 7u } )
				ex.enableVertexAttribArray( l );
			f.genBuffers( 1, &t.eb );
			f.bindBuffer( 0x8893, t.eb );
			f.bufferData( 0x8893, std::ptrdiff_t( idx.size() * 2 ), idx.data(), fStatic );
			ex.bindVertexArray( 0 );
		}
		// a second fgDecal texel buffer (the shifted decals): tiles reading it are another batch
		const Smp & sd = smp[size_t( iDecal )];
		GLuint decal2 = 0, decal2Buf = 0;
		{
			std::mt19937 rr( 777u + 2u );
			const std::vector<float> d2 = decalData( rr, 90.0f, decalTexels );
			glGenTextures( 1, &decal2 );
			f.genBuffers( 1, &decal2Buf );
			f.bindBuffer( 0x8C2A, decal2Buf );
			f.bufferData( 0x8C2A, sd.bufBytes, nullptr, fStatic );
			ex.bufferSubData( 0x8C2A, 0, std::ptrdiff_t( d2.size() * 4 ), d2.data() );
			glBindTexture( 0x8C2A, decal2 );
			ex.texBuffer( 0x8C2A, sd.fmt, decal2Buf );
			glBindTexture( 0x8C2A, 0 );
		}
		const GLuint decalsSplit[3] = { 0, decal2, decal2 };
		auto tileOn = [&]( GLuint fb, int k, const GLuint * decal ) {
			drawState( fb, false );
			f.uniformMatrix4fv( lMv, 1, GL_FALSE, tiles[k].mv );
			ex.bindVertexArray( tiles[k].vao );
			if ( decal && decal[k] )
				bindUnit( sd.unit, targetOf( sd.kind ), decal[k] );
		};
		auto tileRun = [&]( int red ) {
			WwVkPresent::WaterRun r;
			WwVkPresent::farGroundStateOf( &gw, pFg, r );
			r.w = W;
			r.h = H;
			r.color = fc;
			WwVkPresent::WaterCmd c;
			c.count = count;
			c.instanceCount = 1;
			c.firstIndex = std::uint32_t( idxOff / 2 );
			c.baseVertex = base;
			c.baseInstance = 0;
			r.cmds.assign( 1, c );
			r.key = reinterpret_cast<std::uintptr_t>( twinKeys );
			r.red = red;
			return r;
		};
		struct Img
		{
			std::vector<std::uint16_t> c;
			std::vector<float> d;
			bool ran = true;
			int runs = 0;
			WwVkPresent::TwinBatchStat bs;
		};
		auto glTiles = [&]( int n, const GLuint * decal ) {
			before( fbRef );
			for ( int k = 0; k < n; k++ ) {
				tileOn( fbRef, k, decal );
				ex.drawInst( GL_TRIANGLES, GLsizei( count ), GL_UNSIGNED_SHORT, reinterpret_cast<const void *>( std::uintptr_t( idxOff ) ), 1, base, 0 );
				stateEnd();
			}
			Img o;
			o.c = readColor( refCol );
			o.d = readDepth( refDs, false );
			return o;
		};
		// mode 0: one crossing a tile (farGroundRun), 1: farGroundRunN, 2: farGroundQueue + farGroundFlush
		auto vkTiles = [&]( int mode, int n, int red1, const GLuint * decal ) {
			Img o;
			before( fbFrame );
			std::vector<WwVkPresent::WaterRun> rs( static_cast<std::size_t>( n ) );
			for ( int k = 0; k < n; k++ ) {
				tileOn( fbFrame, k, decal );
				rs[size_t( k )] = tileRun( k == 1 ? red1 : 0 );
				if ( mode == 0 )
					o.ran = WwVkPresent::farGroundRun( rs[size_t( k )], why ) && o.ran;
				if ( mode == 2 )
					o.ran = WwVkPresent::farGroundQueue( rs[size_t( k )], why ) && o.ran;
				stateEnd();
			}
			if ( mode == 1 ) {
				std::vector<const WwVkPresent::WaterRun *> ps;
				for ( const WwVkPresent::WaterRun & r : rs )
					ps.push_back( &r );
				o.ran = WwVkPresent::farGroundRunN( ps.data(), n, why );
			}
			if ( mode == 2 )
				o.ran = WwVkPresent::farGroundFlush( why ) && o.ran;
			runs++;
			o.runs = take().runs;
			o.bs = WwVkPresent::takeFarGroundBatchStat();
			o.c = readColor( fc );
			o.d = readDepth( fds, true );
			return o;
		};
		before( fbRef );
		const std::vector<std::uint16_t> bC = readColor( refCol );
		const std::vector<float> bD = readDepth( refDs, false );
		const Img gl3 = glTiles( 3, nullptr ), gl1 = glTiles( 1, nullptr ), glSplit = glTiles( 3, decalsSplit );
		auto arm = [&]( const char * what, const Img & vk, const Img & gl, bool mustFail, int wantCross, int wantDraws ) {
			if ( !vk.ran ) {
				std::printf( "FARGROUNDDRAW batch %s: REFUSED (%s)\n", what, why.c_str() );
				gateOk = false;
				return;
			}
			const Verdict v = judge( bC, bD, vk.c, vk.d, gl.c, gl.d );
			const char * word = v.word();
			const bool fail = std::strcmp( word, "FAIL" ) == 0;
			const bool depthOk = v.maxDepthU == 0 && v.depthPast == 0;
			const bool crossOk = wantCross < 0 || ( vk.bs.crossings == wantCross && vk.bs.draws == wantDraws && !vk.bs.lost && !vk.bs.off );
			std::printf( "FARGROUNDDRAW batch %s: %s covered %zu (both %zu, one side only %zu), depth max %ld D24 units (%zu past 1), colour %zu "
				"halves differ, %zu samples past 1e-2 (max rel %.3g); %d single runs, %d batched crossings for %d draws, %d lost%s\n", what, word,
				v.covered, v.both, v.onlyOne, v.maxDepthU, v.depthPast, v.halvesDiff, v.colPast, v.maxRel, vk.runs - vk.bs.draws, vk.bs.crossings,
				vk.bs.draws, vk.bs.lost, mustFail ? ( fail ? " -> red FAILS as it must" : " -> red PASSES: the gate is blind" )
				: crossOk ? "" : " -> NOT the crossings wanted" );
			if ( mustFail ? !fail : ( fail || !depthOk || !crossOk ) )
				gateOk = false;
		};
		auto exact = [&]( const char * what, const Img & a, const Img & b ) {
			size_t cd = 0, dd = 0;
			for ( size_t i = 0; i < a.c.size() && i < b.c.size(); i++ )
				cd += a.c[i] != b.c[i];
			for ( size_t i = 0; i < a.d.size() && i < b.d.size(); i++ )
				dd += a.d[i] != b.d[i];
			const bool ok = !cd && !dd && a.c.size() == b.c.size() && a.d.size() == b.d.size() && a.ran && b.ran;
			std::printf( "FARGROUNDDRAW batch %s: %s (%zu colour halves, %zu depth samples differ)\n", what, ok ? "EXACT" : "DIFFER", cd, dd );
			gateOk = gateOk && ok;
		};
		const Img one0 = vkTiles( 0, 1, 0, nullptr );
		arm( "tile 0, one crossing", one0, gl1, false, -1, 0 );
		const Img singles = vkTiles( 0, 3, 0, nullptr );
		arm( "3 tiles, a crossing each", singles, gl3, false, 0, 0 );
		const Img batch = vkTiles( 1, 3, 0, nullptr );
		arm( "3 tiles, one crossing", batch, gl3, false, 1, 3 );
		exact( "one crossing vs a crossing each", batch, singles );
		const Img queued = vkTiles( 2, 3, 0, nullptr );
		arm( "3 tiles queued + flushed", queued, gl3, false, 1, 3 );
		exact( "queued vs one crossing", queued, batch );
		arm( "red 1024 (tile 1 on tile 0's set and host slice)", vkTiles( 1, 3, 1024, nullptr ), gl3, true, -1, 0 );
		arm( "red 2048 (tile 1 dropped)", vkTiles( 1, 3, 2048, nullptr ), gl3, true, -1, 0 );
		arm( "tiles 1 + 2 on another fgDecal buffer", vkTiles( 1, 3, 0, decalsSplit ), glSplit, false, 2, 3 );
		arm( "the same, queued + flushed", vkTiles( 2, 3, 0, decalsSplit ), glSplit, false, 2, 3 );
		exact( "tile 0 after the batches vs before", vkTiles( 0, 1, 0, nullptr ), one0 );
	}
	std::printf( gateOk ? "FARGROUNDDRAW GATE OK\n" : "FARGROUNDDRAW GATE FAILED\n" );
	return gateOk ? 0 : 1;
}

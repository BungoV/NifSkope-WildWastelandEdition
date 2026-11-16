// Lane VKCLOUD4 (2026-11-16): the cloud map's build on the present device (vkcloudmap.inc cloudMapRun: one render pass over
// the shared RGBA8 n x n map, CLEAR to 0, one indexed draw a layer with GL's union blend and the layer's colour mask) against
// GL's own cloudmap.prog (the tree's cloudmap.vert / .frag) in cloudmap.cpp buildMap's order and state (viewport n x n,
// clear 0, blend ONE_MINUS_DST_COLOR / ONE (the red "max": GL_MAX), colour mask by band, Tex / Tex2 repeat, one drawShape
// a layer), on the same inputs. The Vulkan side takes what the app's call site takes: fxPack over the GL program's own
// uniforms, fxTexOf over units 0 / 1. fxview.cpp's helpers are reused (its main renamed).
// Jobs (made here, deterministic; RGBA8 mipmapped repeat layer textures, as the app's):
//   quad: 5 world-laid layers (drawShape 1: cmQuad, 4 vertices from the sheet fit), R / R+B sheet / G with Tex2 blend 0.4 /
//         A / a second R layer over the first; dome: 4 dome layers (drawShape 2: the dome's projection, w = dir.z, a ring
//         under the horizon clipped); max: the quad job with GL_MAX.
// Bar (set before the first run, never widened), per job, RGBA8 bytes: max 1 level apart, <= 0.1 % of bytes differ;
//   every channel the job writes non-zero on GL at >= 1/50 of texels (the job reaches every band), every other channel 0.
//   EXACT = nothing differs; BOUND = inside the bar; anything else FAILS.
// Reds (each must FAIL, on `quad`): 1 Tex / Tex2 swapped, 2 the render pass LOADs instead of CLEARing (the map is seeded),
// 4 the vertex block's uvOffset.y read 4 bytes early, 8 every layer's colour mask RGBA.
// Last line `CLOUDMAPVIEW GATE OK` or `CLOUDMAPVIEW GATE FAILED`.
#define main fxview_main
#include "fxview.cpp"
#undef main

#include "../../src/features/vkframe/vkcloudmap.h"

namespace cmv
{
using namespace fv;
constexpr GLenum kArrayBuf = 0x8892, kElemBuf = 0x8893, kMax = 0x8008, kAdd = 0x8006;
struct More
{
	void( WINAPI * genVertexArrays )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindVertexArray )( GLuint ) = nullptr;
	void( WINAPI * vertexAttribPointer )( GLuint, GLint, GLenum, GLboolean, GLsizei, const void * ) = nullptr;
	void( WINAPI * enableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * disableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * blendEquation )( GLenum ) = nullptr;
};
More m;

struct Layer
{
	int band = 0, tex = 0, tex2 = -1;
	bool sheet = false;
	float alpha = 1.0f, blend = 0.0f, height = 1.0f, uvo[2] = { 0, 0 };
	std::vector<float> pos, col, uv;
	std::vector<std::uint16_t> idx;
	int mask() const { return band == 0 ? 1 | ( sheet ? 4 : 0 ) : band == 1 ? 2 : 8; }
};
struct Job
{
	std::string name;
	bool quad = true, max = false;
	std::vector<Layer> layers;
};

std::uint32_t hash( std::uint32_t x )
{
	x ^= x >> 16;
	x *= 0x7feb352dU;
	x ^= x >> 15;
	x *= 0x846ca68bU;
	x ^= x >> 16;
	return x;
}

// a cloud-like layer texture: smooth value noise in alpha, a tint in rgb (64 x 64, repeat)
std::vector<std::uint8_t> layerTex( int seed, int w )
{
	std::vector<std::uint8_t> t( size_t( w ) * w * 4 );
	auto cell = [&]( int x, int y ) { return float( hash( std::uint32_t( ( ( y & 7 ) * 8 + ( x & 7 ) ) * 977 + seed * 7919 ) ) & 255 ) / 255.0f; };
	for ( int y = 0; y < w; y++ )
		for ( int x = 0; x < w; x++ ) {
			const float fx = x / 8.0f, fy = y / 8.0f;
			const int ix = int( fx ), iy = int( fy );
			const float ax = fx - ix, ay = fy - iy;
			const float v = ( cell( ix, iy ) * ( 1 - ax ) + cell( ix + 1, iy ) * ax ) * ( 1 - ay ) + ( cell( ix, iy + 1 ) * ( 1 - ax ) + cell( ix + 1, iy + 1 ) * ax ) * ay;
			std::uint8_t * p = &t[( size_t( y ) * w + x ) * 4];
			p[0] = std::uint8_t( 200 + seed * 10 );
			p[1] = std::uint8_t( 180 + x );
			p[2] = std::uint8_t( 220 - y );
			p[3] = std::uint8_t( std::clamp( v * 1.3f - 0.1f, 0.0f, 1.0f ) * 255.0f + 0.5f );
		}
	return t;
}

// cloudmap.cpp's world-laid quad: corners at clip +-1, uv = J w + c over the square, the cap's alpha in colour
void quadMesh( Layer & l, const float jm[4], const float c0[2], float cap )
{
	const float cx[4] = { -1.0f, 1.0f, 1.0f, -1.0f }, cy[4] = { -1.0f, -1.0f, 1.0f, 1.0f };
	for ( int k = 0; k < 4; k++ ) {
		l.pos.insert( l.pos.end(), { cx[k], cy[k], 1.0f } );
		l.col.insert( l.col.end(), { 1.0f, 1.0f, 1.0f, cap } );
		l.uv.insert( l.uv.end(), { jm[0] * cx[k] + jm[1] * cy[k] + c0[0], jm[2] * cx[k] + jm[3] * cy[k] + c0[1] } );
	}
	l.idx = { 0, 1, 2, 0, 2, 3 };
}

// a dome: directions from the eye (rings from -6 to 90 degrees), the edge fade in alpha, uv from the direction
void domeMesh( Layer & l, float uvScale )
{
	const int az = 24, rings = 9;
	for ( int r = 0; r <= rings; r++ ) {
		const float el = ( -6.0f + 96.0f * r / rings ) * 3.14159265f / 180.0f;
		for ( int a = 0; a < az; a++ ) {
			const float ph = 6.2831853f * a / az;
			const float d[3] = { std::cos( el ) * std::cos( ph ), std::cos( el ) * std::sin( ph ), std::sin( el ) };
			l.pos.insert( l.pos.end(), { d[0], d[1], d[2] } );
			l.col.insert( l.col.end(), { 1.0f, 1.0f, 1.0f, std::clamp( d[2] * 4.0f, 0.0f, 1.0f ) } );
			l.uv.insert( l.uv.end(), { d[0] * uvScale + 0.5f, d[1] * uvScale + 0.5f } );
		}
	}
	for ( int r = 0; r < rings; r++ )
		for ( int a = 0; a < az; a++ ) {
			const std::uint16_t i0 = std::uint16_t( r * az + a ), i1 = std::uint16_t( r * az + ( a + 1 ) % az );
			l.idx.insert( l.idx.end(), { i0, i1, std::uint16_t( i1 + az ), i0, std::uint16_t( i1 + az ), std::uint16_t( i0 + az ) } );
		}
}

std::vector<Job> jobs()
{
	std::vector<Job> js;
	Job q;
	q.name = "quad";
	const struct
	{
		int band, tex, tex2;
		bool sheet;
		float alpha, blend, j[4], c[2], cap, uvo[2];
	} ql[5] = {
		{ 0, 0, -1, false, 0.7f, 0.0f, { 1.3f, 0.2f, -0.15f, 1.1f }, { 0.31f, 0.12f }, 0.8f, { 0.13f, 0.27f } },
		{ 0, 1, -1, true, 0.5f, 0.0f, { 0.9f, -0.3f, 0.25f, 0.8f }, { -0.2f, 0.4f }, 1.0f, { 0.05f, -0.4f } },
		{ 1, 2, 3, false, 0.85f, 0.4f, { 2.1f, 0.0f, 0.1f, 1.9f }, { 0.6f, 0.05f }, 0.9f, { 0.71f, 0.33f } },
		{ 2, 3, -1, false, 0.9f, 0.0f, { 0.6f, 0.4f, -0.4f, 0.6f }, { 0.0f, 0.0f }, 0.7f, { -0.25f, 0.6f } },
		{ 0, 2, -1, false, 0.6f, 0.0f, { 1.7f, -0.5f, 0.5f, 1.7f }, { 0.2f, 0.9f }, 1.0f, { 0.4f, 0.1f } },
	};
	for ( const auto & s : ql ) {
		Layer l;
		l.band = s.band;
		l.tex = s.tex;
		l.tex2 = s.tex2;
		l.sheet = s.sheet;
		l.alpha = s.alpha;
		l.blend = s.blend;
		l.uvo[0] = s.uvo[0];
		l.uvo[1] = s.uvo[1];
		quadMesh( l, s.j, s.c, s.cap );
		q.layers.push_back( l );
	}
	js.push_back( q );
	Job d;
	d.name = "dome";
	d.quad = false;
	const struct
	{
		int band, tex, tex2;
		bool sheet;
		float alpha, blend, height, uvs, uvo[2];
	} dl[4] = { { 0, 0, -1, false, 0.8f, 0.0f, 0.35f, 1.4f, { 0.1f, 0.2f } }, { 1, 1, 2, false, 0.7f, 0.7f, 0.5f, 2.2f, { -0.3f, 0.05f } },
		{ 2, 3, -1, false, 0.95f, 0.0f, 0.8f, 0.9f, { 0.45f, -0.15f } }, { 0, 2, -1, true, 0.55f, 0.0f, 0.35f, 3.1f, { 0.0f, 0.5f } } };
	for ( const auto & s : dl ) {
		Layer l;
		l.band = s.band;
		l.tex = s.tex;
		l.tex2 = s.tex2;
		l.sheet = s.sheet;
		l.alpha = s.alpha;
		l.blend = s.blend;
		l.height = s.height;
		l.uvo[0] = s.uvo[0];
		l.uvo[1] = s.uvo[1];
		domeMesh( l, s.uvs );
		d.layers.push_back( l );
	}
	js.push_back( d );
	Job mx = q;
	mx.name = "max";
	mx.max = true;
	js.push_back( mx );
	return js;
}

struct Verdict
{
	size_t bytes = 0, diff = 0, texels = 0;
	int maxd = 0;
	size_t lit[4] = { 0, 0, 0, 0 };
	int want = 0;  // the channels the job writes
	bool reach() const
	{
		for ( int c = 0; c < 4; c++ )
			if ( ( want >> c & 1 ) ? lit[c] * 50 < texels : lit[c] != 0 )
				return false;
		return texels > 0;
	}
	bool ok() const { return bytes > 0 && maxd <= 1 && diff * 1000 <= bytes && reach(); }
	const char * word() const { return !ok() ? "FAIL" : diff == 0 ? "EXACT" : "BOUND"; }
};

Verdict judge( const std::vector<std::uint8_t> & a, const std::vector<std::uint8_t> & ref, int n, int want )
{
	Verdict v;
	v.want = want;
	if ( a.size() != ref.size() || a.size() != size_t( n ) * n * 4 )
		return v;
	v.texels = size_t( n ) * n;
	for ( size_t p = 0; p < a.size(); p++ ) {
		const int d = std::abs( int( a[p] ) - int( ref[p] ) );
		v.bytes++;
		v.diff += d != 0;
		v.maxd = std::max( v.maxd, d );
		v.lit[p & 3] += ref[p] != 0;
	}
	return v;
}
}  // namespace cmv

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace cmv;
	gTag = "CLOUDMAPVIEW";
	if ( argc < 2 ) {
		std::printf( "usage: cloudmapview <tree root>\nCLOUDMAPVIEW GATE FAILED\n" );
		return 2;
	}
	const std::string root = argv[1];
	auto fatal = [&]( const std::string & why ) {
		std::printf( "CLOUDMAPVIEW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwcloudmapview";
	RegisterClassA( &wc );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwcloudmapview", "cloud map view harness", WS_OVERLAPPEDWINDOW, 40, 40, 320, 200,
		nullptr, nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( dc, ChoosePixelFormat( dc, &pfd ), &pfd );
	HGLRC glrc = wglCreateContext( dc );
	wglMakeCurrent( dc, glrc );
	ShowWindow( wnd, SW_SHOWNOACTIVATE );
	SetWindowPos( wnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
	pump();
	if ( !loadAll() || !load( m.genVertexArrays, "glGenVertexArrays" ) || !load( m.bindVertexArray, "glBindVertexArray" )
		|| !load( m.vertexAttribPointer, "glVertexAttribPointer" ) || !load( m.enableVertexAttribArray, "glEnableVertexAttribArray" )
		|| !load( m.disableVertexAttribArray, "glDisableVertexAttribArray" ) || !load( m.blendEquation, "glBlendEquation" ) )
		return fatal( "GL entry points" );
	std::printf( "CLOUDMAPVIEW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	const std::string sh = root + "/res/shaders/features/clouds/";
	const GLuint prog = program( readFile( sh + "cloudmap.vert" ), readFile( sh + "cloudmap.frag" ), "cloudmap" );
	if ( !prog )
		return fatal( "GL program" );
	const int n = 512;  // wwCloudMapSizeFor( 2 or 3 )
	std::string why;
	if ( !WwVkPresent::viewBegin( n, n, 1, glProc, why ) )
		return fatal( "the view: " + why );
	// the layer textures (RGBA8, mipmapped, repeat: the texture cache's sky layers)
	GLuint tex[4] = {};
	for ( int k = 0; k < 4; k++ ) {
		const std::vector<std::uint8_t> px = layerTex( k, 64 );
		glGenTextures( 1, &tex[k] );
		glBindTexture( GL_TEXTURE_2D, tex[k] );
		glTexImage2D( GL_TEXTURE_2D, 0, GLint( fRgba8 ), 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data() );
		f.generateMipmap( GL_TEXTURE_2D );
		texParams( GL_TEXTURE_2D, true, true, true );
	}
	// the maps: GL's own (the reference) and the shared one the Vulkan run writes (the app's s.map.tex as a sharedTarget)
	const GLuint glMap = tex2d( n, n, fRgba8, GL_RGBA, GL_UNSIGNED_BYTE, nullptr, true );
	const GLuint fbGl = fbo( GL_TEXTURE_2D, glMap, 0, 0 );
	static char mapKey;
	GLuint vkMap = 0;
	if ( !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &mapKey ), 0, n, n, 1, fRgba8, vkMap, why ) )
		return fatal( "the shared map: " + why );
	const GLuint fbVk = fbo( GL_TEXTURE_2D, vkMap, 0, 0 );
	if ( !fbGl || !fbVk )
		return fatal( "GL targets" );
	auto readMap = [&]( GLuint fb ) {
		std::vector<std::uint8_t> out( size_t( n ) * n * 4 );
		f.bindFramebuffer( fFb, fb );
		glReadPixels( 0, 0, n, n, GL_RGBA, GL_UNSIGNED_BYTE, out.data() );
		return out;
	};
	GLuint vao = 0, vbo[3] = {}, ebo = 0;
	m.genVertexArrays( 1, &vao );
	f.genBuffers( 3, vbo );
	f.genBuffers( 1, &ebo );
	static char twin[2 * WwVkPresent::kCloudMapLayers];
	const float he = 3000.0f;
	// buildMap's per-layer GL state: units, wrap, colour mask, the uniforms
	auto layerGl = [&]( const Layer & l, bool quad ) {
		f.activeTexture( fTex0 + 1 );
		glBindTexture( GL_TEXTURE_2D, l.tex2 >= 0 ? tex[l.tex2] : 0 );
		f.activeTexture( fTex0 );
		glBindTexture( GL_TEXTURE_2D, tex[l.tex] );
		const int mk = l.mask();
		glColorMask( mk & 1, ( mk >> 1 ) & 1, ( mk >> 2 ) & 1, ( mk >> 3 ) & 1 );
		f.useProgram( prog );
		u1i( prog, "Tex", 0 );
		u1i( prog, "Tex2", 1 );
		auto u1f = [&]( const char * nm, float v ) { f.uniform1fv( f.getUniformLocation( prog, nm ), 1, &v ); };
		u1f( "texBlend", l.tex2 >= 0 ? l.blend : 0.0f );
		u1i( prog, "cmShow", 0 );
		u1i( prog, "cmQuad", quad ? 1 : 0 );
		u1f( "cmHeight", l.height * he );
		u1f( "cmHalfExtent", he );
		f.uniform2fv( f.getUniformLocation( prog, "uvOffset" ), 1, l.uvo );
		u1f( "alpha", l.alpha );
	};
	auto runGl = [&]( const Job & j ) {
		f.bindFramebuffer( fFb, fbGl );
		glViewport( 0, 0, n, n );
		drawState0();
		glClearColor( 0, 0, 0, 0 );
		glClear( GL_COLOR_BUFFER_BIT );
		glEnable( GL_BLEND );
		if ( j.max ) {
			m.blendEquation( kMax );
			glBlendFunc( GL_ONE, GL_ONE );
		} else {
			m.blendEquation( kAdd );
			glBlendFunc( GL_ONE_MINUS_DST_COLOR, GL_ONE );
		}
		m.bindVertexArray( vao );
		for ( const Layer & l : j.layers ) {
			layerGl( l, j.quad );
			const std::vector<float> * a[3] = { &l.pos, &l.col, &l.uv };
			const int loc[3] = { 0, 1, 7 }, comps[3] = { 3, 4, 2 };
			for ( int k = 0; k < 3; k++ ) {
				f.bindBuffer( kArrayBuf, vbo[k] );
				f.bufferData( kArrayBuf, std::ptrdiff_t( a[k]->size() * 4 ), a[k]->data(), fStatic );
				m.vertexAttribPointer( GLuint( loc[k] ), comps[k], GL_FLOAT, GL_FALSE, 0, nullptr );
				m.enableVertexAttribArray( GLuint( loc[k] ) );
			}
			f.bindBuffer( kElemBuf, ebo );
			f.bufferData( kElemBuf, std::ptrdiff_t( l.idx.size() * 2 ), l.idx.data(), fStatic );
			glDrawElements( GL_TRIANGLES, GLsizei( l.idx.size() ), GL_UNSIGNED_SHORT, nullptr );
		}
		m.bindVertexArray( 0 );
		m.blendEquation( kAdd );
		drawState0();
		return readMap( fbGl );
	};
	auto runVk = [&]( const Job & j, int red, std::vector<std::uint8_t> & out ) -> bool {
		f.bindFramebuffer( fFb, fbVk );
		glViewport( 0, 0, n, n );
		drawState0();
		glClearColor( 0.25f, 0.5f, 0.75f, 1.0f );  // a seed the CLEAR must remove
		glClear( GL_COLOR_BUFFER_BIT );
		static WwVkPresent::CloudMapRun t;
		t = WwVkPresent::CloudMapRun();
		t.n = n;
		t.key = reinterpret_cast<std::uintptr_t>( &mapKey );
		t.max = j.max;
		for ( const Layer & l : j.layers ) {
			layerGl( l, j.quad );
			if ( !WwVkPresent::cloudMapAddMesh( t, l.pos.data(), l.col.data(), l.uv.data(), int( l.pos.size() / 3 ), l.idx.data(), int( l.idx.size() ) ) ) {
				std::printf( "CLOUDMAPVIEW cloudMapAddMesh refused\n" );
				return false;
			}
			WwVkPresent::CloudMapLayer & x = t.layer[t.layers - 1];
			if ( !WwVkPresent::fxTexOf( &ga, prog, "Tex", 0, x.tex, why ) || !WwVkPresent::fxTexOf( &ga, prog, "Tex2", 0, x.tex2, why ) ) {
				std::printf( "CLOUDMAPVIEW fxTexOf refused: %s\n", why.c_str() );
				return false;
			}
			x.tex.twin = reinterpret_cast<std::uintptr_t>( &twin[2 * ( t.layers - 1 )] );
			x.tex2.twin = reinterpret_cast<std::uintptr_t>( &twin[2 * ( t.layers - 1 ) + 1] );
			WwVkPresent::fxPack( &ga, prog, WwVkPresent::kT_cloudmap_vs, int( std::size( WwVkPresent::kT_cloudmap_vs ) ), x.vs );
			WwVkPresent::fxPack( &ga, prog, WwVkPresent::kT_cloudmap_ps, int( std::size( WwVkPresent::kT_cloudmap_ps ) ), x.ps );
			x.mask = l.mask();
		}
		drawState0();
		t.red = red;
		if ( !WwVkPresent::cloudMapRun( t, why ) ) {
			std::printf( "CLOUDMAPVIEW cloudMapRun refused: %s\n", why.c_str() );
			return false;
		}
		out = readMap( fbVk );
		return glGetError() == GL_NO_ERROR;
	};

	const std::vector<Job> js = jobs();
	bool ok = true;
	std::vector<std::vector<std::uint8_t>> refs;
	for ( const Job & j : js ) {
		int want = 0;
		for ( const Layer & l : j.layers )
			want |= l.mask();
		const std::vector<std::uint8_t> ref = runGl( j );
		if ( glGetError() != GL_NO_ERROR )
			return fatal( "the GL reference of " + j.name );
		refs.push_back( ref );
		std::vector<std::uint8_t> a;
		if ( !runVk( j, 0, a ) ) {
			std::printf( "CLOUDMAPVIEW %s FAIL (the Vulkan run failed)\n", j.name.c_str() );
			ok = false;
			continue;
		}
		const Verdict v = judge( a, ref, n, want );
		std::printf( "CLOUDMAPVIEW %s %s (%zu layers, %zu bytes, %zu differ, max %d levels; GL lit R %zu G %zu B %zu A %zu of %zu texels)\n",
			j.name.c_str(), v.word(), j.layers.size(), v.bytes, v.diff, v.maxd, v.lit[0], v.lit[1], v.lit[2], v.lit[3], v.texels );
		ok = ok && v.ok();
	}
	{
		const WwVkPresent::FxStat st = WwVkPresent::takeCloudMapStat();
		std::printf( "CLOUDMAPVIEW stat: %d runs, %d pipelines built in %.1f ms, %d input copies\n", st.runs, st.pipelines, st.buildMs, st.copies );
	}
	const struct
	{
		const char * name;
		int red;
	} reds[4] = { { "Tex / Tex2 swapped", 1 }, { "CLEAR turned LOAD", 2 }, { "vertex block uvOffset.y read 4 bytes early", 4 },
		{ "colour masks RGBA", 8 } };
	int want0 = 0;
	for ( const Layer & l : js[0].layers )
		want0 |= l.mask();
	for ( const auto & rd : reds ) {
		std::vector<std::uint8_t> a;
		if ( !runVk( js[0], rd.red, a ) ) {
			std::printf( "CLOUDMAPVIEW red %s: the run failed (counts as a FAIL)\n", rd.name );
			continue;
		}
		const Verdict v = judge( a, refs[0], n, want0 );
		std::printf( "CLOUDMAPVIEW red %s: %s (%zu differ, max %d levels)\n", rd.name, v.word(), v.diff, v.maxd );
		if ( v.ok() ) {
			std::printf( "CLOUDMAPVIEW red %s PASSED: the gate cannot see it\n", rd.name );
			ok = false;
		}
	}
	const WwVkPresent::FxStat st = WwVkPresent::takeCloudMapStat();
	std::printf( "CLOUDMAPVIEW stat (reds): %d runs, %d failed (%s), drawing thread %.3f ms a run (max %.3f)\n", st.runs, st.fails,
		st.failWhy.c_str(), st.runs ? st.cpuMs / st.runs : 0.0, st.maxMs );
	WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &mapKey ) );
	for ( char & k : twin )
		WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &k ) );
	f.bindFramebuffer( fFb, 0 );
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	DestroyWindow( wnd );
	std::printf( "%s\n", ok ? "CLOUDMAPVIEW GATE OK" : "CLOUDMAPVIEW GATE FAILED" );
	return ok ? 0 : 1;
}

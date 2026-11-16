// Lane VKAO4 (2026-11-16): the GI volume mask (ao.cpp giVolMaskPass, cell_givol.prog, vkstatus row 23) on the present device.
// BAR FIRST: this header holds the bar and the reds, set before any route code or run, never widened.
// bash tests/vkframe/volmaskdraw.sh <out dir>.
//
// GL's pass (the reference, copied state for state from giVolMaskPass): target R32F `volMask` (attachment 0, cleared 0), the
// opaque pass's D24S8 depth attached, viewport 0 0 W H; cell_givol.prog with the shapes in an RGBA32F buffer texture (3 texels
// a shape: (centre, type), (size, room), (rotation degrees, 0)) and the g-buffer (view normal, linear depth); depth test
// GEQUAL, depth writes off, stencil off, cull FRONT with front face CCW, depth clamp on, blend MAX ONE ONE; one draw
// glDrawArraysInstanced( TRIANGLES, 0, 36, n ) (the hull from gl_VertexID, the shape from gl_InstanceID).
// Route under test (to be written: WwVkPresent::volMaskRun, under WW_VK_VOLMASK below): the same draw into the SHARED volMask
// (sharedTarget R32F) on the opaque depth from a shared D24S8 target (the owner step: AoGpu::gbufDepthKey), the same state,
// the stages the cell_givol_ps / cell_givol_vs twins (readable HLSL res/shaders/hlsl/ao/cell_givol_*.hlsl), the shapes in a
// uniform texel buffer, the g-buffer copied into a route-owned twin.
//
// Fixture: 256 x 160, a symmetric perspective (near 1, far 60; the pixel stage rebuilds the view point with P00 / P11 only),
// view scale 1.25 with pitch and yaw. Receivers: a ground quad (its far end past the far plane: background texels, g.w 1e6),
// a standing wall and a tilted panel, drawn by a small GL program into the g-buffer and the depth. Shapes, every type once,
// rotated: S0 box (room 2) and S1 cylinder (room 5) overlap (MAX judged where they meet), S2 wedge (room 4), S3 capsule
// (room 7), S4 sphere (room 3), S5 a box holding the camera whose far face lies past the far plane (only cull FRONT plus
// depth clamp draw it), room 0 (the lowest: every other shape inside it still shows), drawn LAST (the count red drops it).
// S2 floats above the ground by less than the bias: only the grown hull reaches the ground under it.
// (Fixture moved after the first run, bar unchanged: run 1 had S2 room 0 under S5 room 1, so value 1 could never show.)
// Known GL defect, kept by the twin (a ruling, not this lane's): cell_givol.vert indexes FC[k - 1] for k = 4, 5, so k = 5
// reads FC[4] of a 4-entry array (undefined) and the second triangle of every hull face is not (0, 2, 3). An info-only GL
// probe with k - 2 prints the texels that defect moves (the hole set); judge prints how many differing texels lie in it.
// Bar:
//   coverage: GL volMask != 0 on >= 3000 texels, == 0 on >= 500 non-background texels (the outdoors), >= 5 distinct values
//     (an empty or one-shape fixture FAILS).
//   volMask: Vulkan == GL on EVERY texel (EXACT: room + 1 values, blended by max; no tolerance).
//   depth twin: the shared D24S8 Vulkan reads equals GL's opaque depth bit for bit (EXACT; a fixture check).
// Reds (Vulkan run with the red bit; each must FAIL, never pass): 1 blend: ADD instead of MAX; 2 depth: LESS instead of
//   GEQUAL; 4 count: n - 1 instances; 8 clamp: no depth clamp; 16 cull: cull BACK; 32 grow: the vertex stage's volGrow 0.
//   Each red's GL probe (GL's pass with the same change) must move >= 50 texels of volMask against GL's own pass, or the red
//   TESTS NOTHING and the gate FAILS. The probes run on GL alone, so the first run (no route) proves them live.
// Last line `VOLMASKDRAW GATE OK` or `VOLMASKDRAW GATE FAILED`.
#define main fxview_main
#include "fxview.cpp"
#undef main
#ifdef WW_VK_VOLMASK
#include "features/vkframe/vkvolmask.h"
#endif

#include <set>

namespace vmd
{
using namespace fv;

const int W = 256, H = 160;
constexpr GLenum kDs = 0x88F0, kDepthStencil = 0x84F9, kU248 = 0x84FA, kDepthClamp = 0x864F, kTexBuf = 0x8C2A, kMax = 0x8008,
	kAdd = 0x8006;

struct Xd
{
	void( WINAPI * genVertexArrays )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindVertexArray )( GLuint ) = nullptr;
	void( WINAPI * enableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * vertexAttribPointer )( GLuint, GLint, GLenum, GLboolean, GLsizei, const void * ) = nullptr;
	void( WINAPI * clearBufferfv )( GLenum, GLint, const GLfloat * ) = nullptr;
	void( WINAPI * texBuffer )( GLenum, GLenum, GLuint ) = nullptr;
	void( WINAPI * drawArraysInstanced )( GLenum, GLint, GLsizei, GLsizei ) = nullptr;
	void( WINAPI * blendEquation )( GLenum ) = nullptr;
	void( WINAPI * uniform2i )( GLint, GLint, GLint ) = nullptr;
};
Xd xd;

bool loadXd()
{
	return load( xd.genVertexArrays, "glGenVertexArrays" ) && load( xd.bindVertexArray, "glBindVertexArray" )
		&& load( xd.enableVertexAttribArray, "glEnableVertexAttribArray" ) && load( xd.vertexAttribPointer, "glVertexAttribPointer" )
		&& load( xd.clearBufferfv, "glClearBufferfv" ) && load( xd.texBuffer, "glTexBuffer" )
		&& load( xd.drawArraysInstanced, "glDrawArraysInstanced" ) && load( xd.blendEquation, "glBlendEquation" )
		&& load( xd.uniform2i, "glUniform2i" );
}

// ---- the world: the view (posView = sc Rv world + tv), the projection (pm[col][row], as the app's Matrix4), the shapes
double Rv[3][3], tv[3], sc = 1.25, cam[3] = { 0.0, 6.0, 14.0 };
float pm[4][4];
const float kBias = 0.6f;

void rotXY( double ax, double ay, double R[3][3] )	// R = Rx(ax) Ry(ay)
{
	const double cx = std::cos( ax ), sx = std::sin( ax ), cy = std::cos( ay ), sy = std::sin( ay );
	const double X[3][3] = { { 1, 0, 0 }, { 0, cx, -sx }, { 0, sx, cx } }, Y[3][3] = { { cy, 0, sy }, { 0, 1, 0 }, { -sy, 0, cy } };
	for ( int i = 0; i < 3; i++ )
		for ( int j = 0; j < 3; j++ ) {
			R[i][j] = 0.0;
			for ( int k = 0; k < 3; k++ )
				R[i][j] += X[i][k] * Y[k][j];
		}
}

void setView()
{
	rotXY( 0.35, 0.2, Rv );
	for ( int i = 0; i < 3; i++ ) {
		tv[i] = 0.0;
		for ( int j = 0; j < 3; j++ )
			tv[i] -= sc * Rv[i][j] * cam[j];
	}
	const double zn = 1.0, zf = 60.0;
	std::memset( pm, 0, sizeof( pm ) );
	pm[0][0] = 1.2f;
	pm[1][1] = float( 1.2 * W / H );
	pm[2][2] = float( -( zf + zn ) / ( zf - zn ) );
	pm[2][3] = -1.0f;
	pm[3][2] = float( -2.0 * zf * zn / ( zf - zn ) );
}

std::vector<float> shapes;	// 12 floats a shape, GiVolumeShape's layout (the app's G->volShapes)

void makeShapes()
{
	auto add = [&]( float x, float y, float z, int type, float sx, float sy, float sz, int room, float rx, float ry, float rz ) {
		const float v[12] = { x, y, z, float( type ), sx, sy, sz, float( room ), rx, ry, rz, 0.0f };
		shapes.insert( shapes.end(), v, v + 12 );
	};
	add( -2.0f, 1.0f, -6.0f, 0, 5.0f, 3.0f, 4.0f, 2, 0.0f, 25.0f, 0.0f );		// S0 box
	add( 1.5f, 1.0f, -7.0f, 1, 4.0f, 3.0f, 2.5f, 5, 90.0f, 0.0f, 10.0f );		// S1 cylinder, standing
	add( 4.5f, 2.3f, -12.0f, 2, 4.0f, 4.0f, 3.0f, 4, 0.0f, -30.0f, 0.0f );		// S2 wedge, its bottom 0.8 over the ground
	add( -5.0f, 1.0f, -14.0f, 3, 3.0f, 3.0f, 7.0f, 7, 0.0f, 70.0f, 60.0f );	// S3 capsule
	add( 0.5f, 0.0f, -21.0f, 4, 8.0f, 5.0f, 8.0f, 3, 10.0f, 0.0f, 20.0f );		// S4 sphere
	add( 0.0f, 8.0f, -10.0f, 0, 16.0f, 20.0f, 140.0f, 0, 0.0f, 0.0f, 0.0f );	// S5 the camera's box, far face past far
}

// clip = volClip[r] . (world, 1) and view -> world rows, the app's arithmetic line for line (ao.cpp giVolMaskPass)
void volMath( float clip[4][4], float row[3][4] )
{
	for ( int r = 0; r < 4; r++ ) {
		double e[4] = { 0, 0, 0, double( pm[3][r] ) };
		for ( int i = 0; i < 3; i++ ) {
			for ( int j = 0; j < 3; j++ )
				e[j] += double( pm[i][r] ) * Rv[i][j] * sc;
			e[3] += double( pm[i][r] ) * tv[i];
		}
		for ( int k = 0; k < 4; k++ )
			clip[r][k] = float( e[k] );
	}
	for ( int k = 0; k < 3; k++ ) {
		float w = 0.0f;
		for ( int j = 0; j < 3; j++ )
			w -= float( Rv[j][k] ) * float( tv[j] );
		const float s = float( sc );
		row[k][0] = float( Rv[0][k] ) / s, row[k][1] = float( Rv[1][k] ) / s, row[k][2] = float( Rv[2][k] ) / s, row[k][3] = w / s;
	}
}

// ---- the opaque pass: receivers into a g-buffer (view normal, linear depth) and a D24S8 depth
const char * kRecVs = "#version 410 core\n"
	"layout(location = 0) in vec3 p; layout(location = 1) in vec3 n;\n"
	"uniform mat4 wClip; uniform mat3 wView; uniform vec4 wZ; uniform float wSc;\n"
	"out vec3 vn; out float lin;\n"
	"void main() { vn = wView * n; lin = -( dot( wZ.xyz, p ) + wZ.w ) / wSc; gl_Position = wClip * vec4( p, 1.0 ); }\n";
const char * kRecFs = "#version 410 core\n"
	"in vec3 vn; in float lin; out vec4 o;\n"
	"void main() { o = vec4( normalize( vn ), lin ); }\n";

std::vector<float> receivers()	// world position + normal per vertex
{
	std::vector<float> v;
	auto tri = [&]( const double a[3], const double b[3], const double c[3] ) {
		const double * p[3] = { a, b, c };
		double e1[3], e2[3], n[3];
		for ( int k = 0; k < 3; k++ )
			e1[k] = b[k] - a[k], e2[k] = c[k] - a[k];
		n[0] = e1[1] * e2[2] - e1[2] * e2[1];
		n[1] = e1[2] * e2[0] - e1[0] * e2[2];
		n[2] = e1[0] * e2[1] - e1[1] * e2[0];
		const double l = std::sqrt( n[0] * n[0] + n[1] * n[1] + n[2] * n[2] );
		for ( int i = 0; i < 3; i++ ) {
			for ( int k = 0; k < 3; k++ )
				v.push_back( float( p[i][k] ) );
			for ( int k = 0; k < 3; k++ )
				v.push_back( float( n[k] / l ) );
		}
	};
	auto quad = [&]( const double a[3], const double b[3], const double c[3], const double d[3] ) {
		tri( a, b, c );
		tri( a, c, d );
	};
	const double g0[3] = { -40, 0, 10 }, g1[3] = { 40, 0, 10 }, g2[3] = { 40, 0, -120 }, g3[3] = { -40, 0, -120 };
	quad( g0, g1, g2, g3 );
	const double w0[3] = { -4, 0, -7.5 }, w1[3] = { 0, 0, -7.5 }, w2[3] = { 0, 2.5, -7.5 }, w3[3] = { -4, 2.5, -7.5 };	// a wall facing +z
	quad( w0, w1, w2, w3 );
	const double p0[3] = { -7, 0.2, -12 }, p1[3] = { -3, 0.2, -12 }, p2[3] = { -3, 2.4, -15 }, p3[3] = { -7, 2.4, -15 };	// a tilted panel
	quad( p0, p1, p2, p3 );
	return v;
}

void drawReceivers( GLuint prog, GLuint vao, int nVerts, GLuint fb )
{
	f.bindFramebuffer( fFb, fb );
	glViewport( 0, 0, W, H );
	const GLenum one[1] = { fCol0 };
	f.drawBuffers( 1, one );
	const GLfloat clr[4] = { 0, 0, 1, 1.0e6f };
	xd.clearBufferfv( GL_COLOR, 0, clr );
	glDepthMask( GL_TRUE );
	glStencilMask( 0xFFu );
	glClearDepth( 1.0 );
	glClearStencil( 0 );
	glClear( GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LESS );
	glDisable( GL_CULL_FACE );
	glDisable( GL_BLEND );
	glDisable( GL_STENCIL_TEST );
	f.useProgram( prog );
	float clip[16], view[9], z[4];
	for ( int col = 0; col < 4; col++ )
		for ( int row = 0; row < 4; row++ ) {	// clip = P (sc Rv w + tv)
			double e = 0.0;
			if ( col < 3 ) {
				for ( int i = 0; i < 3; i++ )
					e += double( pm[i][row] ) * sc * Rv[i][col];
			} else {
				e = double( pm[3][row] );
				for ( int i = 0; i < 3; i++ )
					e += double( pm[i][row] ) * tv[i];
			}
			clip[col * 4 + row] = float( e );
		}
	for ( int col = 0; col < 3; col++ )
		for ( int row = 0; row < 3; row++ )
			view[col * 3 + row] = float( Rv[row][col] );
	for ( int k = 0; k < 3; k++ )
		z[k] = float( sc * Rv[2][k] );
	z[3] = float( tv[2] );
	f.uniformMatrix4fv( f.getUniformLocation( prog, "wClip" ), 1, GL_FALSE, clip );
	f.uniformMatrix3fv( f.getUniformLocation( prog, "wView" ), 1, GL_FALSE, view );
	f.uniform4fv( f.getUniformLocation( prog, "wZ" ), 1, z );
	const float s1 = float( sc );
	f.uniform1fv( f.getUniformLocation( prog, "wSc" ), 1, &s1 );
	xd.bindVertexArray( vao );
	glDrawArrays( GL_TRIANGLES, 0, nVerts );
	xd.bindVertexArray( 0 );
	f.bindFramebuffer( fFb, 0 );
}

// ---- GL's mask pass (giVolMaskPass's state); the knobs are the reds' GL probes
struct Knobs
{
	bool addBlend = false, less = false, fewer = false, noClamp = false, cullBack = false, noGrow = false;
};

void glMask( GLuint prog, GLuint fb, GLuint gbuf, GLuint shapeTex, GLuint vao, const Knobs & k )
{
	f.bindFramebuffer( fFb, fb );
	const GLenum one = fCol0;
	f.drawBuffers( 1, &one );
	glViewport( 0, 0, W, H );
	const GLfloat zero[4] = { 0, 0, 0, 0 };
	xd.clearBufferfv( GL_COLOR, 0, zero );
	f.useProgram( prog );
	auto loc = [&]( const char * n ) { return f.getUniformLocation( prog, n ); };
	float clip[4][4], row[3][4];
	volMath( clip, row );
	for ( int r = 0; r < 4; r++ )
		f.uniform4fv( loc( ( "volClip[" + std::to_string( r ) + "]" ).c_str() ), 1, clip[r] );
	for ( int r = 0; r < 3; r++ )
		f.uniform4fv( loc( ( "volRow[" + std::to_string( r ) + "]" ).c_str() ), 1, row[r] );
	const float grow = k.noGrow ? 0.0f : kBias + 1.0f, proj[2] = { pm[0][0], pm[1][1] };
	f.uniform1fv( loc( "volBias" ), 1, &kBias );
	f.uniform1fv( loc( "volGrow" ), 1, &grow );
	f.uniform2fv( loc( "volProj" ), 1, proj );
	xd.uniform2i( loc( "volFull" ), W, H );
	f.activeTexture( fTex0 );
	glBindTexture( GL_TEXTURE_2D, gbuf );
	f.activeTexture( fTex0 + 1 );
	glBindTexture( kTexBuf, shapeTex );
	f.uniform1i( loc( "volGbuf" ), 0 );
	f.uniform1i( loc( "volShapes" ), 1 );
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( k.less ? GL_LESS : GL_GEQUAL );
	glDepthMask( GL_FALSE );
	glDisable( GL_STENCIL_TEST );
	glEnable( GL_CULL_FACE );
	glFrontFace( GL_CCW );
	glCullFace( k.cullBack ? GL_BACK : GL_FRONT );
	if ( !k.noClamp )
		glEnable( kDepthClamp );
	glEnable( GL_BLEND );
	xd.blendEquation( k.addBlend ? kAdd : kMax );
	glBlendFunc( GL_ONE, GL_ONE );
	xd.bindVertexArray( vao );
	const int n = int( shapes.size() / 12 );
	xd.drawArraysInstanced( GL_TRIANGLES, 0, 36, k.fewer ? n - 1 : n );
	xd.bindVertexArray( 0 );
	glDisable( kDepthClamp );
	xd.blendEquation( kAdd );
	glDisable( GL_BLEND );
	glCullFace( GL_BACK );
	glDisable( GL_CULL_FACE );
	glDepthFunc( GL_LESS );
	glDisable( GL_DEPTH_TEST );
	glDepthMask( GL_TRUE );
	glBindTexture( kTexBuf, 0 );
	f.activeTexture( fTex0 );
	glBindTexture( GL_TEXTURE_2D, 0 );
	f.bindFramebuffer( fFb, 0 );
}

GLuint r32f()
{
	GLuint t = 0;
	glGenTextures( 1, &t );
	glBindTexture( GL_TEXTURE_2D, t );
	glTexImage2D( GL_TEXTURE_2D, 0, GLint( fR32f ), W, H, 0, fRed, GL_FLOAT, nullptr );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, fMaxLevel, 0 );
	glBindTexture( GL_TEXTURE_2D, 0 );
	return t;
}

std::vector<float> readR( GLuint t )
{
	std::vector<float> v( size_t( W ) * H );
	glBindTexture( GL_TEXTURE_2D, t );
	glGetTexImage( GL_TEXTURE_2D, 0, fRed, GL_FLOAT, v.data() );
	glBindTexture( GL_TEXTURE_2D, 0 );
	return v;
}

std::vector<std::uint32_t> readDs( GLuint t )
{
	std::vector<std::uint32_t> v( size_t( W ) * H );
	glBindTexture( GL_TEXTURE_2D, t );
	glGetTexImage( GL_TEXTURE_2D, 0, kDepthStencil, kU248, v.data() );
	glBindTexture( GL_TEXTURE_2D, 0 );
	return v;
}

GLuint fboOf( GLuint c0, GLuint ds )
{
	GLuint b = 0;
	f.genFramebuffers( 1, &b );
	f.bindFramebuffer( fFb, b );
	f.framebufferTexture2D( fFb, fCol0, GL_TEXTURE_2D, c0, 0 );
	f.framebufferTexture2D( fFb, fDsAtt, GL_TEXTURE_2D, ds, 0 );
	const bool ok = f.checkFramebufferStatus( fFb ) == fComplete;
	f.bindFramebuffer( fFb, 0 );
	return ok ? b : 0;
}

int moved( const std::vector<float> & a, const std::vector<float> & b )
{
	int n = 0;
	for ( size_t i = 0; i < a.size(); i++ )
		n += a[i] != b[i] ? 1 : 0;
	return n;
}

std::vector<char> holes;	// texels the FC[4] defect moves (GL k - 1 vs GL k - 2), info

// the bar on one Vulkan pass against GL's; prints one line, returns true when it holds
bool judge( const char * what, const std::vector<float> & gl, const std::vector<float> & vk )
{
	const int bad = moved( gl, vk );
	int vkOn = 0, inHoles = 0;
	for ( size_t i = 0; i < vk.size(); i++ ) {
		vkOn += vk[i] != 0.0f ? 1 : 0;
		inHoles += ( vk[i] != gl[i] && i < holes.size() && holes[i] ) ? 1 : 0;
	}
	std::printf( "VOLMASKDRAW %s: volMask diff %d of %zu texels (in the FC[4] hole set %d; vk non-zero %d) %s\n", what, bad, gl.size(),
		inHoles, vkOn, bad == 0 ? "EXACT" : "FAIL" );
	return bad == 0;
}

std::string histOf( const std::vector<float> & v, int & distinct )
{
	std::map<float, int> hist;
	for ( float x : v )
		hist[x]++;
	std::string h;
	for ( const auto & kv : hist )
		h += " " + std::to_string( int( kv.first ) ) + ":" + std::to_string( kv.second );
	distinct = int( hist.size() ) - ( hist.count( 0.0f ) ? 1 : 0 );
	return h;
}

std::string inlineInclude( std::string s, const std::string & lib )
{
	const std::string inc = "#include \"cell_givol.glsl\"";
	const size_t at = s.find( inc );
	if ( at == std::string::npos )
		return std::string();
	s.replace( at, inc.size(), lib );
	return s;
}
}  // namespace vmd

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace vmd;
	gTag = "VOLMASKDRAW";
	if ( argc < 2 ) {
		std::printf( "usage: volmaskdraw <res/shaders/features/ao>\nVOLMASKDRAW GATE FAILED\n" );
		return 2;
	}
	const std::string dir = argv[1];
	auto fatal = [&]( const std::string & why ) {
		std::printf( "VOLMASKDRAW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwvolmaskdraw";
	RegisterClassA( &wc );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwvolmaskdraw", "volume mask harness", WS_OVERLAPPEDWINDOW, 40, 40, 320, 200, nullptr,
		nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( dc, ChoosePixelFormat( dc, &pfd ), &pfd );
	HGLRC glrc = wglCreateContext( dc );
	wglMakeCurrent( dc, glrc );
	ShowWindow( wnd, SW_SHOWNOACTIVATE );
	SetWindowPos( wnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
	pump();
	if ( !loadAll() || !loadXd() )
		return fatal( "GL entry points" );
	std::printf( "VOLMASKDRAW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	std::string why;
	if ( !WwVkPresent::viewBegin( W, H, 4, glProc, why ) )
		return fatal( "the view: " + why );

	setView();
	makeShapes();
	const std::string lib = readFile( dir + "/cell_givol.glsl" );
	const std::string vs = inlineInclude( readFile( dir + "/cell_givol.vert" ), lib ), fs = inlineInclude( readFile( dir + "/cell_givol.frag" ), lib );
	if ( lib.empty() || vs.empty() || fs.empty() )
		return fatal( "cell_givol sources (or their #include line)" );
	const GLuint prog = program( vs, fs, "cell_givol" );
	const GLuint recProg = program( kRecVs, kRecFs, "receivers" );
	if ( !prog || !recProg )
		return fatal( "programs" );

	// the receivers: one VAO, position + normal; the mask's VAO has no attributes (giVolMaskPass's volVao)
	const std::vector<float> rv = receivers();
	GLuint recVao = 0, recVbo = 0, volVao = 0;
	xd.genVertexArrays( 1, &recVao );
	xd.genVertexArrays( 1, &volVao );
	f.genBuffers( 1, &recVbo );
	xd.bindVertexArray( recVao );
	f.bindBuffer( 0x8892, recVbo );
	f.bufferData( 0x8892, std::ptrdiff_t( rv.size() * sizeof( float ) ), rv.data(), fStatic );
	xd.enableVertexAttribArray( 0 );
	xd.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 24, nullptr );
	xd.enableVertexAttribArray( 1 );
	xd.vertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, 24, reinterpret_cast<const void *>( std::intptr_t( 12 ) ) );
	xd.bindVertexArray( 0 );
	f.bindBuffer( 0x8892, 0 );
	const int recVerts = int( rv.size() / 6 );
	// the shapes: GL_TEXTURE_BUFFER RGBA32F over a buffer (giVolMaskPass's volBuf / volTex)
	GLuint shapeBuf = 0, shapeTex = 0;
	f.genBuffers( 1, &shapeBuf );
	f.bindBuffer( kTexBuf, shapeBuf );
	f.bufferData( kTexBuf, std::ptrdiff_t( shapes.size() * sizeof( float ) ), shapes.data(), fStatic );
	f.bindBuffer( kTexBuf, 0 );
	glGenTextures( 1, &shapeTex );
	glBindTexture( kTexBuf, shapeTex );
	xd.texBuffer( kTexBuf, fRgba32f, shapeBuf );
	glBindTexture( kTexBuf, 0 );

	// GL's side: its own g-buffer, D24S8 and volMask; Vulkan's: the shared D24S8 and volMask (the route's)
	GLuint gbuf = 0, glDs = 0;
	glGenTextures( 1, &gbuf );
	glBindTexture( GL_TEXTURE_2D, gbuf );
	glTexImage2D( GL_TEXTURE_2D, 0, GLint( fRgba32f ), W, H, 0, GL_RGBA, GL_FLOAT, nullptr );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, fMaxLevel, 0 );
	glGenTextures( 1, &glDs );
	glBindTexture( GL_TEXTURE_2D, glDs );
	glTexImage2D( GL_TEXTURE_2D, 0, GLint( kDs ), W, H, 0, kDepthStencil, kU248, nullptr );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glBindTexture( GL_TEXTURE_2D, 0 );
	const GLuint glVol = r32f();
	const GLuint recFb = fboOf( gbuf, glDs ), maskFb = fboOf( glVol, glDs );
	if ( !recFb || !maskFb )
		return fatal( "GL framebuffers incomplete" );
	static char keys[2];
	unsigned vkDs = 0, vkVol = 0;
	if ( !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &keys[0] ), 0, W, H, 1, kDs, vkDs, why )
		|| !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &keys[1] ), 0, W, H, 1, fR32f, vkVol, why ) )
		return fatal( "the shared targets: " + why );
	const GLuint gbuf2 = r32f();	// the shared depth's own receivers pass writes a scratch colour; the route reads `gbuf`
	const GLuint vkRecFb = fboOf( gbuf2, vkDs );
	if ( !vkRecFb )
		return fatal( "the shared depth's framebuffer incomplete" );

	drawReceivers( recProg, recVao, recVerts, recFb );
	drawReceivers( recProg, recVao, recVerts, vkRecFb );
	glFinish();
	{
		const auto a = readDs( glDs ), b = readDs( vkDs );
		int bad = 0, bg = 0;
		for ( size_t i = 0; i < a.size(); i++ ) {
			bad += a[i] != b[i] ? 1 : 0;
			bg += ( a[i] >> 8 ) == 0xFFFFFFu ? 1 : 0;
		}
		std::printf( "VOLMASKDRAW depth twin: %d of %zu texels differ (background %d) %s\n", bad, a.size(), bg, bad == 0 ? "EXACT" : "FAIL" );
		if ( bad )
			return fatal( "the depth twin" );
	}
	const std::vector<float> rowGb = [&] {
		std::vector<float> v( size_t( W ) * H * 4 );
		glBindTexture( GL_TEXTURE_2D, gbuf );
		glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, v.data() );
		glBindTexture( GL_TEXTURE_2D, 0 );
		return v;
	}();

	bool okAll = true;
	auto glRun = [&]( const Knobs & k ) {
		glMask( prog, maskFb, gbuf, shapeTex, volVao, k );
		glFinish();
		return readR( glVol );
	};
	const std::vector<float> gl = glRun( Knobs {} );
	{
		int on = 0, outdoors = 0, bg = 0;
		for ( size_t i = 0; i < gl.size(); i++ ) {
			const bool back = rowGb[i * 4 + 3] >= 1.0e5f;
			bg += back ? 1 : 0;
			on += gl[i] != 0.0f ? 1 : 0;
			outdoors += !back && gl[i] == 0.0f ? 1 : 0;
		}
		int distinct = 0;
		const std::string h = histOf( gl, distinct );
		const bool covOk = on >= 3000 && outdoors >= 500 && distinct >= 5;
		std::printf( "VOLMASKDRAW coverage: non-zero %d texels, outdoors %d, background %d, %d distinct values (value:texels%s) %s\n", on,
			outdoors, bg, distinct, h.c_str(), covOk ? "OK" : "FAIL" );
		okAll = okAll && covOk;
	}
	{	// info: the FC[4] defect, GL's program with the hull's second triangle (0, 2, 3) indexed k - 2
		const std::string idx = "k == 3 ? 0 : k - 1]";
		std::string vs2 = vs;
		const size_t at = vs2.find( idx );
		const GLuint fixed = at == std::string::npos ? 0 : program( vs2.replace( at, idx.size(), "k == 3 ? 0 : k - 2]" ), fs, "cell_givol k-2" );
		if ( !fixed )
			return fatal( "the k - 2 probe program (its index text not found or not linked)" );
		const std::vector<float> full = [&] {
			glMask( fixed, maskFb, gbuf, shapeTex, volVao, Knobs {} );
			glFinish();
			return readR( glVol );
		}();
		holes.assign( gl.size(), 0 );
		int n = 0;
		for ( size_t i = 0; i < gl.size(); i++ ) {
			holes[i] = gl[i] != full[i] ? 1 : 0;
			n += holes[i];
		}
		int distinct = 0;
		const std::string h = histOf( full, distinct );
		std::printf( "VOLMASKDRAW info FC[4] defect: GL with k - 2 moves %d texels against GL's own (k - 2: %d distinct values,%s)\n", n,
			distinct, h.c_str() );
	}
	const char * redName[6] = { "blend", "depth", "count", "clamp", "cull", "grow" };
	for ( int r = 0; r < 6; r++ ) {	// the reds' GL probes
		Knobs k;
		bool * b[6] = { &k.addBlend, &k.less, &k.fewer, &k.noClamp, &k.cullBack, &k.noGrow };
		*b[r] = true;
		const int n = moved( gl, glRun( k ) );
		std::printf( "VOLMASKDRAW GL probe red %d %s: moved %d texels %s\n", 1 << r, redName[r], n, n >= 50 ? "LIVE" : "TESTS NOTHING" );
		okAll = okAll && n >= 50;
	}
#ifdef WW_VK_VOLMASK
	auto vkRun = [&]( int red ) {
		WwVkPresent::VolMaskRun run;
		run.w = W, run.h = H, run.gbuf = gbuf, run.red = red;
		run.ds = reinterpret_cast<std::uintptr_t>( &keys[0] );
		run.target = reinterpret_cast<std::uintptr_t>( &keys[1] );
		run.shapes = shapes;
		volMath( run.clip, run.row );
		run.bias = kBias, run.grow = kBias + 1.0f;
		run.proj[0] = pm[0][0], run.proj[1] = pm[1][1];
		std::string w2;
		if ( !WwVkPresent::volMaskRun( run, w2 ) ) {
			std::printf( "VOLMASKDRAW red %d refused: %s\n", red, w2.c_str() );
			return std::vector<float>();
		}
		return readR( vkVol );
	};
	const std::vector<float> vk = vkRun( 0 );
	okAll = !vk.empty() && judge( "main", gl, vk ) && okAll;
	for ( int r = 0; r < 6; r++ ) {
		const std::vector<float> vr = vkRun( 1 << r );
		const bool held = !vr.empty() && judge( redName[r], gl, vr );
		std::printf( "VOLMASKDRAW red %d %s %s\n", 1 << r, redName[r], held ? "PASSED (a red must FAIL)" : "FAILED as it must" );
		okAll = okAll && !held && !vr.empty();
	}
	const WwVkPresent::VolMaskStat st = WwVkPresent::takeVolMaskStat();
	std::printf( "VOLMASKDRAW stat: runs %d draws %d twin copies %d fails %d pipelines %d\n", st.runs, st.draws, st.twinCopies, st.fails,
		st.pipelines );
	std::printf( okAll ? "VOLMASKDRAW GATE OK\n" : "VOLMASKDRAW GATE FAILED\n" );
	return okAll ? 0 : 1;
#else
	std::printf( "VOLMASKDRAW GATE FAILED (no route: WW_VK_VOLMASK not defined; GL coverage and probes above)\n" );
	return 1;
#endif
}

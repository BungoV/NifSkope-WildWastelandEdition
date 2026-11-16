// Lane VKAO2 (2026-11-16): the baked AO decals (cellaodecalgl.cpp wwCellAoDecalRun, cell_aodecal.prog, vkstatus rows 29-30)
// on the present device. BAR FIRST: this header holds the bar and the reds, set before any route code or run, never widened.
// bash tests/vkframe/aodecaldraw.sh <out dir>.
//
// GL's pass (the reference, copied state for state from wwCellAoDecalRun): targets R32F `target` (attachment 0, cleared 1)
// and R32F `selfId` (attachment 1, cleared 0), the opaque pass's D24S8 depth / stencil attached, viewport 0 0 W H, depth
// writes off, front face CCW, fill. Per copy (culled copies, model < 0, skipped; aoSelf = copy index + 1 counts them):
//   draw 1 (the mask): aoMode 0, draw buffers {NONE, 1}, blend off, cull off, depth test LEQUAL with polygon offset
//     (-1, -4), stencil ALWAYS ref 0x80 mask 0x80, op KEEP KEEP REPLACE, write mask 0x80; the model's maskVerts triangles
//     (none drawn when maskVerts is 0).
//   draw 2 (the AO): aoMode 1, draw buffers {0, NONE}, depth test off, cull FRONT, depth clamp on, blend ONE ONE (aoAdd,
//     the red arm) or DST_COLOR ZERO (the multiply arm, the app's), stencil NOTEQUAL ref 0x80 mask 0x80, ops KEEP, write
//     mask 0; the footprint box's 36 vertices after the mask.
//   then the stencil bit 0x80 cleared for the next copy (glClear with stencil write mask 0x80).
// Route under test (to be written: WwVkPresent::aoDecalRun, under WW_VK_AODECAL below): the same two draws per copy into the
// SHARED target / selfId (sharedTarget R32F) with the opaque depth / stencil from a shared D24S8 target (the owner step:
// ao.cpp's gbufDepth is a renderbuffer today), the same blend, stencil, cull, clamp and offset state, the pixel stage the
// cell_aodecal_ps twin (readable HLSL res/shaders/hlsl/ao/cell_aodecal_ps.hlsl), the vertex stage cell_aodecal_vs.
//
// Fixture (one view, two arms: multiply and add): 256 x 160, an off-centre perspective (P20 0.07, P21 -0.05, near 1, far
// 60), view scale 1.25 with pitch and yaw. Receivers: a ground quad (the far end past the far plane: background texels,
// g.w 1e6) and every copy's own mask triangles, drawn by a small GL program into the g-buffer (view normal, linear depth)
// and the depth / stencil. Copies in order: A (model 0, rotated, s 1.3), a culled copy (model -1: aoSelf skips 2), B
// (model 1, rotated, s 0.9; its box holds A's own surface, so A's pixels take B's AO but never A's own), C (model 2, no
// mask triangles, a box holding the camera whose back face lies past the far plane: only cull FRONT plus depth clamp draw
// it). Footprints of A, B and C overlap (the blend judged where several copies meet); receivers sit inside aoFade and
// aoRcut with distinct aoCoef, so every packed uniform reaches the colour.
// Bar (each arm):
//   coverage: GL selfId != 0 on >= 300 texels; GL target != its clear on >= 5000 texels (an empty fixture FAILS).
//   selfId: Vulkan == GL on EVERY texel (EXACT: the mask's coverage texel for texel).
//   target: |vk - gl| <= 1e-3 on EVERY texel (the 0.1 % bar); the count over 1e-5 is printed, not judged.
//   depth twin: the shared D24S8 Vulkan reads equals GL's opaque depth / stencil bit for bit (EXACT; a fixture check).
// Reds (Vulkan run with the red bit; each must FAIL, never pass): 1 mask: draw 1 dropped; 2 blend: the other arm's blend
//   (ONE ONE in the multiply arm, DST_COLOR ZERO in the add arm); 4 mode: draw 2 with aoMode 0; 8 cull: draw 2 culls BACK;
//   16 clamp: draw 2 without depth clamp; 32 bit: the stencil bit not cleared between copies.
//   Each red's GL probe (GL's pass with the same change) must move >= 50 texels of target or selfId against GL's own pass,
//   or the red TESTS NOTHING and the gate FAILS. The probes run on GL alone, so the first run (no route) proves them live.
// Last line `AODECALDRAW GATE OK` or `AODECALDRAW GATE FAILED`.
#define main fxview_main
#include "fxview.cpp"
#undef main
#ifdef WW_VK_AODECAL
#include "features/vkframe/vkaodecal.h"
#endif

#include <random>

namespace aod
{
using namespace fv;

const int W = 256, H = 160;
constexpr GLenum kDs = 0x88F0, kDepthStencil = 0x84F9, kU248 = 0x84FA, kDepthClamp = 0x864F;

struct Xd
{
	void( WINAPI * genVertexArrays )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindVertexArray )( GLuint ) = nullptr;
	void( WINAPI * enableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * vertexAttribPointer )( GLuint, GLint, GLenum, GLboolean, GLsizei, const void * ) = nullptr;
	void( WINAPI * clearBufferfv )( GLenum, GLint, const GLfloat * ) = nullptr;
};
Xd xd;

bool loadXd()
{
	return load( xd.genVertexArrays, "glGenVertexArrays" ) && load( xd.bindVertexArray, "glBindVertexArray" )
		&& load( xd.enableVertexAttribArray, "glEnableVertexAttribArray" ) && load( xd.vertexAttribPointer, "glVertexAttribPointer" )
		&& load( xd.clearBufferfv, "glClearBufferfv" );
}

// ---- the world: the view, the projection (pm[col][row], as the app's Matrix4), models and copies
double Rv[3][3], tv[3], sc = 1.25, cam[3] = { 0.0, 6.0, 14.0 };
float pm[4][4];

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
	pm[2][0] = 0.07f;
	pm[2][1] = -0.05f;
	pm[2][2] = float( -( zf + zn ) / ( zf - zn ) );
	pm[2][3] = -1.0f;
	pm[3][2] = float( -2.0 * zf * zn / ( zf - zn ) );
}

struct Model
{
	std::vector<float> mask;	// model-space triangles
	float lo[3], hi[3], fade, c[3], coef[9], rcut;
	int dims[3];
	std::vector<float> tex;		// RGBA32F dims.x x dims.y x 3 dims.z, the app's slab layout
	GLuint vol = 0, vao = 0, vbo = 0;
	int maskVerts = 0;
};

struct Copy
{
	int model;
	double R[9], t[3], s;
};

std::vector<Model> models;
std::vector<Copy> copies;

void quadTris( std::vector<float> & v, const float a[3], const float b[3], const float c[3], const float d[3] )
{
	for ( const float * p : { a, b, c, a, c, d } )
		v.insert( v.end(), p, p + 3 );
}

void makeWorld()
{
	std::mt19937 rng( 17 );
	auto u = [&]( float lo, float hi ) { return lo + ( hi - lo ) * float( rng() % 100000 ) / 100000.0f; };
	models.resize( 3 );
	{	// model 0: a standing quad facing +z and a sloped roof
		Model & m = models[0];
		const float a[3] = { -1, 0, 0 }, b[3] = { 1, 0, 0 }, c[3] = { 1, 2, 0 }, d[3] = { -1, 2, 0 };
		const float e[3] = { -1, 2, 0 }, g[3] = { 1, 2, 0 }, h[3] = { 1, 2.6f, -1.2f }, k[3] = { -1, 2.6f, -1.2f };
		quadTris( m.mask, a, b, c, d );
		quadTris( m.mask, e, g, h, k );
		const float lo[3] = { -3, -0.5f, -3 }, hi[3] = { 3, 3.5f, 3 };
		std::memcpy( m.lo, lo, sizeof( lo ) );
		std::memcpy( m.hi, hi, sizeof( hi ) );
		m.dims[0] = 6, m.dims[1] = 4, m.dims[2] = 6;
		m.fade = 1.0f, m.rcut = 8.0f;
		m.c[0] = 0.1f, m.c[1] = 1.0f, m.c[2] = -0.2f;
	}
	{	// model 1: a slanted panel
		Model & m = models[1];
		const float a[3] = { -1.2f, 0, 0.4f }, b[3] = { 1.2f, 0, 0.4f }, c[3] = { 1.0f, 1.8f, -0.3f }, d[3] = { -1.0f, 1.8f, -0.3f };
		quadTris( m.mask, a, b, c, d );
		const float lo[3] = { -4, -1, -4 }, hi[3] = { 4, 4, 4 };
		std::memcpy( m.lo, lo, sizeof( lo ) );
		std::memcpy( m.hi, hi, sizeof( hi ) );
		m.dims[0] = 5, m.dims[1] = 3, m.dims[2] = 7;
		m.fade = 1.5f, m.rcut = 8.0f;
		m.c[0] = -0.3f, m.c[1] = 0.8f, m.c[2] = 0.2f;
	}
	{	// model 2: no mask triangles; a box holding the camera, its back face past the far plane
		Model & m = models[2];
		const float lo[3] = { -80, -5, -90 }, hi[3] = { 80, 60, 60 };
		std::memcpy( m.lo, lo, sizeof( lo ) );
		std::memcpy( m.hi, hi, sizeof( hi ) );
		m.dims[0] = 4, m.dims[1] = 3, m.dims[2] = 4;
		m.fade = 20.0f, m.rcut = 200.0f;
		m.c[0] = 0.0f, m.c[1] = 2.0f, m.c[2] = 30.0f;
	}
	for ( Model & m : models ) {
		const int dx = m.dims[0], dy = m.dims[1], dz = m.dims[2];
		m.tex.assign( size_t( dx ) * dy * dz * 3 * 4, 0.0f );
		for ( int z = 0; z < dz; z++ )
			for ( int y = 0; y < dy; y++ )
				for ( int x = 0; x < dx; x++ )
					for ( int j = 0; j < 9; j++ ) {
						const int slab = j / 4, ch = j % 4;
						const size_t t = ( ( ( size_t( slab ) * dz + z ) * dy + y ) * dx + x ) * 4 + size_t( ch );
						m.tex[t] = j == 0 ? u( 0.05f, 0.25f ) : u( -0.08f, 0.08f );
					}
		m.coef[0] = 4.0f;
		for ( int j = 1; j < 9; j++ )
			m.coef[j] = u( -1.0f, 1.0f );
		m.maskVerts = int( m.mask.size() / 3 );
	}
	auto copyOf = [&]( int model, double ax, double ay, double x, double y, double z, double s ) {
		Copy c {};
		c.model = model;
		double R[3][3];
		rotXY( ax, ay, R );
		for ( int i = 0; i < 9; i++ )
			c.R[i] = R[i / 3][i % 3];
		c.t[0] = x, c.t[1] = y, c.t[2] = z, c.s = s;
		return c;
	};
	copies.push_back( copyOf( 0, 0.0, 0.4, -2.0, 0.0, -6.0, 1.3 ) );
	copies.push_back( copyOf( -1, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0 ) );
	copies.push_back( copyOf( 1, 0.1, -0.7, 0.5, 0.0, -7.0, 0.9 ) );
	copies.push_back( copyOf( 2, 0.0, 0.1, 0.0, 0.0, -40.0, 1.0 ) );
}

// the footprint box after the mask, as uploadModels writes it (outward counter-clockwise)
void uploadModels()
{
	for ( Model & m : models ) {
		glGenTextures( 1, &m.vol );
		glBindTexture( fT3D, m.vol );
		f.texImage3D( fT3D, 0, GLint( fRgba32f ), m.dims[0], m.dims[1], m.dims[2] * 3, 0, GL_RGBA, GL_FLOAT, m.tex.data() );
		glTexParameteri( fT3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( fT3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glTexParameteri( fT3D, fMaxLevel, 0 );
		glBindTexture( fT3D, 0 );
		std::vector<float> vb = m.mask;
		const float * lo = m.lo, * hi = m.hi;
		const float c[8][3] = { { lo[0], lo[1], lo[2] }, { hi[0], lo[1], lo[2] }, { lo[0], hi[1], lo[2] }, { hi[0], hi[1], lo[2] },
			{ lo[0], lo[1], hi[2] }, { hi[0], lo[1], hi[2] }, { lo[0], hi[1], hi[2] }, { hi[0], hi[1], hi[2] } };
		static const int q[6][4] = { { 0, 2, 3, 1 }, { 4, 5, 7, 6 }, { 0, 1, 5, 4 }, { 2, 6, 7, 3 }, { 0, 4, 6, 2 }, { 1, 3, 7, 5 } };
		for ( const auto & fc : q )
			for ( int k : { fc[0], fc[1], fc[2], fc[0], fc[2], fc[3] } )
				vb.insert( vb.end(), c[k], c[k] + 3 );
		xd.genVertexArrays( 1, &m.vao );
		f.genBuffers( 1, &m.vbo );
		xd.bindVertexArray( m.vao );
		f.bindBuffer( 0x8892, m.vbo );
		f.bufferData( 0x8892, std::ptrdiff_t( vb.size() * sizeof( float ) ), vb.data(), fStatic );
		xd.enableVertexAttribArray( 0 );
		xd.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, nullptr );
		xd.bindVertexArray( 0 );
		f.bindBuffer( 0x8892, 0 );
	}
}

// ---- one copy's uniforms, the app's arithmetic line for line (cellaodecalgl.cpp 280-321)
struct CopyU
{
	float clip[4][4], M[3][4], N[3][3];
	float self;
};

CopyU copyU( size_t ci )
{
	const Copy & c = copies[ci];
	CopyU o {};
	double camv[3];
	for ( int k = 0; k < 3; k++ ) {
		camv[k] = 0.0;
		for ( int j = 0; j < 3; j++ )
			camv[k] -= Rv[j][k] * tv[j] / sc;
	}
	double A[3][3], b[3];
	for ( int i = 0; i < 3; i++ ) {
		b[i] = tv[i];
		for ( int j = 0; j < 3; j++ ) {
			double a = 0.0;
			for ( int k = 0; k < 3; k++ )
				a += Rv[i][k] * c.R[k * 3 + j];
			A[i][j] = sc * c.s * a;
			b[i] += sc * Rv[i][j] * c.t[j];
		}
	}
	for ( int row = 0; row < 4; row++ ) {
		double e[4] = { 0, 0, 0, double( pm[3][row] ) };
		for ( int i = 0; i < 3; i++ ) {
			for ( int j = 0; j < 3; j++ )
				e[j] += double( pm[i][row] ) * A[i][j];
			e[3] += double( pm[i][row] ) * b[i];
		}
		for ( int k = 0; k < 4; k++ )
			o.clip[row][k] = float( e[k] );
	}
	for ( int i = 0; i < 3; i++ ) {
		double M[3] = { 0, 0, 0 }, N[3] = { 0, 0, 0 }, bb = 0.0;
		for ( int j = 0; j < 3; j++ ) {
			for ( int k = 0; k < 3; k++ )
				N[j] += c.R[k * 3 + i] * Rv[j][k];
			M[j] = N[j] / ( c.s * sc );
			bb += c.R[j * 3 + i] * ( camv[j] - c.t[j] ) / c.s;
		}
		for ( int j = 0; j < 3; j++ ) {
			o.M[i][j] = float( M[j] );
			o.N[i][j] = float( N[j] );
		}
		o.M[i][3] = float( bb );
	}
	o.self = float( ci + 1 );
	return o;
}

// ---- the opaque pass: receivers into a g-buffer (view normal, linear depth) and a D24S8 depth / stencil
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
	auto tri = [&]( const double p[3][3] ) {
		double e1[3], e2[3], n[3];
		for ( int k = 0; k < 3; k++ )
			e1[k] = p[1][k] - p[0][k], e2[k] = p[2][k] - p[0][k];
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
	const double g[4][3] = { { -40, 0, 10 }, { 40, 0, 10 }, { 40, 0, -120 }, { -40, 0, -120 } };
	const double t0[3][3] = { { g[0][0], g[0][1], g[0][2] }, { g[1][0], g[1][1], g[1][2] }, { g[2][0], g[2][1], g[2][2] } };
	const double t1[3][3] = { { g[0][0], g[0][1], g[0][2] }, { g[2][0], g[2][1], g[2][2] }, { g[3][0], g[3][1], g[3][2] } };
	tri( t0 );
	tri( t1 );
	for ( const Copy & c : copies ) {
		if ( c.model < 0 )
			continue;
		const Model & m = models[size_t( c.model )];
		for ( int t = 0; t + 2 < m.maskVerts; t += 3 ) {
			double p[3][3];
			for ( int i = 0; i < 3; i++ )
				for ( int r = 0; r < 3; r++ ) {
					p[i][r] = c.t[r];
					for ( int k = 0; k < 3; k++ )
						p[i][r] += c.s * c.R[r * 3 + k] * double( m.mask[size_t( ( t + i ) * 3 + k )] );
				}
			tri( p );
		}
	}
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

// ---- GL's decal pass (wwCellAoDecalRun's state); the knobs are the reds' GL probes
struct Knobs
{
	bool noMask = false, otherBlend = false, mode0 = false, cullBack = false, noClamp = false, keepBit = false;
};

void glDecal( GLuint prog, GLuint fb, GLuint gbuf, bool add, const Knobs & k )
{
	f.bindFramebuffer( fFb, fb );
	glViewport( 0, 0, W, H );
	const GLenum both[2] = { fCol0, fCol0 + 1 };
	f.drawBuffers( 2, both );
	glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	const GLfloat one[4] = { 1.0f, 0, 0, 0 }, zero[4] = { 0, 0, 0, 0 };
	xd.clearBufferfv( GL_COLOR, 0, one );
	xd.clearBufferfv( GL_COLOR, 1, zero );
	glStencilMask( 0x80u );
	glClearStencil( 0 );
	glClear( GL_STENCIL_BUFFER_BIT );
	glEnable( GL_STENCIL_TEST );
	glDepthMask( GL_FALSE );
	glEnable( GL_CULL_FACE );
	glFrontFace( GL_CCW );
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	f.useProgram( prog );
	auto loc = [&]( const char * n ) { return f.getUniformLocation( prog, n ); };
	f.activeTexture( fTex0 );
	glBindTexture( GL_TEXTURE_2D, gbuf );
	f.uniform1i( loc( "aoGbuf" ), 0 );
	f.uniform1i( loc( "aoVol" ), 1 );
	f.uniform1i( loc( "aoAdd" ), add ? 1 : 0 );
	const float proj[4] = { pm[0][0], pm[1][1], pm[2][0], pm[2][1] }, size[2] = { float( W ), float( H ) }, s1 = float( sc );
	f.uniform4fv( loc( "aoProj" ), 1, proj );
	f.uniform2fv( loc( "aoSize" ), 1, size );
	f.uniform1fv( loc( "aoSc" ), 1, &s1 );
	for ( size_t ci = 0; ci < copies.size(); ci++ ) {
		const Copy & c = copies[ci];
		if ( c.model < 0 )
			continue;
		const Model & m = models[size_t( c.model )];
		const CopyU u = copyU( ci );
		f.uniform4fv( loc( "aoClip[0]" ), 4, &u.clip[0][0] );
		f.uniform4fv( loc( "aoM[0]" ), 3, &u.M[0][0] );
		f.uniform3fv( loc( "aoN[0]" ), 3, &u.N[0][0] );
		f.uniform3fv( loc( "aoLo" ), 1, m.lo );
		f.uniform3fv( loc( "aoHi" ), 1, m.hi );
		f.uniform3iv( loc( "aoDims" ), 1, m.dims );
		f.uniform1fv( loc( "aoFade" ), 1, &m.fade );
		f.uniform3fv( loc( "aoC" ), 1, m.c );
		f.uniform1fv( loc( "aoCoef[0]" ), 9, m.coef );
		f.uniform1fv( loc( "aoRcut" ), 1, &m.rcut );
		f.uniform1fv( loc( "aoSelf" ), 1, &u.self );
		f.activeTexture( fTex0 + 1 );
		glBindTexture( fT3D, m.vol );
		f.activeTexture( fTex0 );
		xd.bindVertexArray( m.vao );
		// 1. the mask
		f.uniform1i( loc( "aoMode" ), 0 );
		const GLenum idOnly[2] = { GL_NONE, fCol0 + 1 };
		f.drawBuffers( 2, idOnly );
		glDisable( GL_BLEND );
		glDisable( GL_CULL_FACE );
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		glEnable( GL_POLYGON_OFFSET_FILL );
		glPolygonOffset( -1.0f, -4.0f );
		glStencilFunc( GL_ALWAYS, 0x80, 0x80u );
		glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
		glStencilMask( 0x80u );
		if ( m.maskVerts > 0 && !k.noMask )
			glDrawArrays( GL_TRIANGLES, 0, m.maskVerts );
		glDisable( GL_POLYGON_OFFSET_FILL );
		// 2. the AO
		f.uniform1i( loc( "aoMode" ), k.mode0 ? 0 : 1 );
		const GLenum aoOnly[2] = { fCol0, GL_NONE };
		f.drawBuffers( 2, aoOnly );
		glDisable( GL_DEPTH_TEST );
		glEnable( GL_CULL_FACE );
		glCullFace( k.cullBack ? GL_BACK : GL_FRONT );
		glEnable( GL_BLEND );
		if ( add != k.otherBlend )
			glBlendFunc( GL_ONE, GL_ONE );
		else
			glBlendFunc( GL_DST_COLOR, GL_ZERO );
		glStencilFunc( GL_NOTEQUAL, 0x80, 0x80u );
		glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
		glStencilMask( 0 );
		if ( !k.noClamp )
			glEnable( kDepthClamp );
		glDrawArrays( GL_TRIANGLES, m.maskVerts, 36 );
		glDisable( kDepthClamp );
		// 3. the bit cleared
		glStencilMask( 0x80u );
		if ( !k.keepBit )
			glClear( GL_STENCIL_BUFFER_BIT );
	}
	xd.bindVertexArray( 0 );
	glDisable( GL_BLEND );
	glDisable( GL_STENCIL_TEST );
	glStencilMask( 0xFFu );
	glDisable( GL_CULL_FACE );
	glCullFace( GL_BACK );
	glDepthMask( GL_TRUE );
	f.activeTexture( fTex0 + 1 );
	glBindTexture( fT3D, 0 );
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

GLuint fboOf( GLuint c0, GLuint c1, GLuint ds )
{
	GLuint b = 0;
	f.genFramebuffers( 1, &b );
	f.bindFramebuffer( fFb, b );
	f.framebufferTexture2D( fFb, fCol0, GL_TEXTURE_2D, c0, 0 );
	if ( c1 )
		f.framebufferTexture2D( fFb, fCol0 + 1, GL_TEXTURE_2D, c1, 0 );
	f.framebufferTexture2D( fFb, fDsAtt, GL_TEXTURE_2D, ds, 0 );
	const bool ok = f.checkFramebufferStatus( fFb ) == fComplete;
	f.bindFramebuffer( fFb, 0 );
	return ok ? b : 0;
}

struct Pass
{
	std::vector<float> target, self;
};

int moved( const Pass & a, const Pass & b )
{
	int n = 0;
	for ( size_t i = 0; i < a.target.size(); i++ )
		n += ( a.target[i] != b.target[i] || a.self[i] != b.self[i] ) ? 1 : 0;
	return n;
}

// the bar on one Vulkan pass against GL's; prints one line, returns true when it holds
bool judge( const char * arm, const char * what, const Pass & gl, const Pass & vk )
{
	int selfBad = 0, over = 0, small = 0;
	double mx = 0.0;
	for ( size_t i = 0; i < gl.target.size(); i++ ) {
		selfBad += gl.self[i] != vk.self[i] ? 1 : 0;
		const double d = std::fabs( double( vk.target[i] ) - double( gl.target[i] ) );
		mx = std::max( mx, d );
		over += d > 1e-3 ? 1 : 0;
		small += d > 1e-5 ? 1 : 0;
	}
	const bool ok = selfBad == 0 && over == 0;
	std::printf( "AODECALDRAW %s %s: selfId diff %d, target max %.3g over1e-3 %d (over1e-5 %d) %s\n", arm, what, selfBad, mx, over, small,
		ok ? ( mx == 0.0 ? "EXACT" : "BOUND" ) : "FAIL" );
	return ok;
}
}  // namespace aod

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace aod;
	gTag = "AODECALDRAW";
	if ( argc < 2 ) {
		std::printf( "usage: aodecaldraw <res/shaders/features/ao>\nAODECALDRAW GATE FAILED\n" );
		return 2;
	}
	const std::string dir = argv[1];
	auto fatal = [&]( const std::string & why ) {
		std::printf( "AODECALDRAW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwaodecaldraw";
	RegisterClassA( &wc );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwaodecaldraw", "ao decal harness", WS_OVERLAPPEDWINDOW, 40, 40, 320, 200, nullptr,
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
	std::printf( "AODECALDRAW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	std::string why;
	if ( !WwVkPresent::viewBegin( W, H, 4, glProc, why ) )
		return fatal( "the view: " + why );

	setView();
	makeWorld();
	uploadModels();
	const GLuint prog = program( readFile( dir + "/cell_aodecal.vert" ), readFile( dir + "/cell_aodecal.frag" ), "cell_aodecal" );
	const GLuint recProg = program( kRecVs, kRecFs, "receivers" );
	if ( !prog || !recProg )
		return fatal( "programs" );

	// the receivers: one VAO, position + normal
	const std::vector<float> rv = receivers();
	GLuint recVao = 0, recVbo = 0;
	xd.genVertexArrays( 1, &recVao );
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

	// GL's side: its own g-buffer, D24S8, target and selfId; Vulkan's: the shared D24S8, target and selfId (the route's)
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
	const GLuint glTarget = r32f(), glSelf = r32f();
	const GLuint recFb = fboOf( gbuf, 0, glDs ), decalFb = fboOf( glTarget, glSelf, glDs );
	if ( !recFb || !decalFb )
		return fatal( "GL framebuffers incomplete" );
	static char keys[3];
	unsigned vkDs = 0, vkTarget = 0, vkSelf = 0;
	if ( !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &keys[0] ), 0, W, H, 1, kDs, vkDs, why )
		|| !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &keys[1] ), 0, W, H, 1, fR32f, vkTarget, why )
		|| !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &keys[2] ), 0, W, H, 1, fR32f, vkSelf, why ) )
		return fatal( "the shared targets: " + why );
	const GLuint gbuf2 = r32f();	// the shared depth's own receivers pass writes a scratch colour; the route reads `gbuf`
	const GLuint vkRecFb = fboOf( gbuf2, 0, vkDs );
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
		std::printf( "AODECALDRAW depth twin: %d of %zu texels differ (background %d) %s\n", bad, a.size(), bg, bad == 0 ? "EXACT" : "FAIL" );
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
	auto glRun = [&]( bool add, const Knobs & k ) {
		glDecal( prog, decalFb, gbuf, add, k );
		glFinish();
		return Pass { readR( glTarget ), readR( glSelf ) };
	};
	const char * redName[6] = { "mask", "blend", "mode", "cull", "clamp", "bit" };
	for ( int arm = 0; arm < 2; arm++ ) {
		const bool add = arm == 1;
		const char * armName = add ? "add" : "multiply";
		const Pass gl = glRun( add, Knobs {} );
		int selfN = 0, cover = 0, bgIn = 0;
		const float clear = 1.0f;
		for ( size_t i = 0; i < gl.target.size(); i++ ) {
			selfN += gl.self[i] != 0.0f ? 1 : 0;
			cover += gl.target[i] != clear ? 1 : 0;
			bgIn += rowGb[i * 4 + 3] >= 1.0e5f ? 1 : 0;
		}
		const bool covOk = selfN >= 300 && cover >= 5000;
		std::printf( "AODECALDRAW %s coverage: selfId %d texels, target moved %d texels, background %d %s\n", armName, selfN, cover, bgIn,
			covOk ? "OK" : "FAIL" );
		okAll = okAll && covOk;
		// the reds' GL probes
		for ( int r = 0; r < 6; r++ ) {
			Knobs k;
			bool * b[6] = { &k.noMask, &k.otherBlend, &k.mode0, &k.cullBack, &k.noClamp, &k.keepBit };
			*b[r] = true;
			const Pass pr = glRun( add, k );
			const int n = moved( gl, pr );
			int nT = 0;	// info: the target alone (the mask red's stencil effect on the AO, not only its selfId)
			for ( size_t i = 0; i < gl.target.size(); i++ )
				nT += gl.target[i] != pr.target[i] ? 1 : 0;
			std::printf( "AODECALDRAW %s GL probe red %d %s: moved %d texels (target %d) %s\n", armName, 1 << r, redName[r], n, nT,
				n >= 50 ? "LIVE" : "TESTS NOTHING" );
			okAll = okAll && n >= 50;
		}
#ifdef WW_VK_AODECAL
		auto vkRun = [&]( int red ) {
			WwVkPresent::AoDecalRun run;
			run.w = W, run.h = H, run.gbuf = gbuf, run.add = add, run.red = red;
			run.ds = reinterpret_cast<std::uintptr_t>( &keys[0] );
			run.target = reinterpret_cast<std::uintptr_t>( &keys[1] );
			run.selfId = reinterpret_cast<std::uintptr_t>( &keys[2] );
			run.proj[0] = pm[0][0], run.proj[1] = pm[1][1], run.proj[2] = pm[2][0], run.proj[3] = pm[2][1];
			run.sc = float( sc );
			for ( size_t ci = 0; ci < copies.size(); ci++ ) {
				if ( copies[ci].model < 0 )
					continue;
				const Model & m = models[size_t( copies[ci].model )];
				const CopyU u = copyU( ci );
				WwVkPresent::AoDecalCopy c;
				std::memcpy( c.clip, u.clip, sizeof( c.clip ) );
				std::memcpy( c.M, u.M, sizeof( c.M ) );
				std::memcpy( c.N, u.N, sizeof( c.N ) );
				std::memcpy( c.lo, m.lo, sizeof( c.lo ) );
				std::memcpy( c.hi, m.hi, sizeof( c.hi ) );
				std::memcpy( c.dims, m.dims, sizeof( c.dims ) );
				std::memcpy( c.c, m.c, sizeof( c.c ) );
				std::memcpy( c.coef, m.coef, sizeof( c.coef ) );
				c.fade = m.fade, c.rcut = m.rcut, c.self = u.self;
				c.vol = m.vol, c.vbo = m.vbo, c.maskVerts = m.maskVerts;
				run.copies.push_back( c );
			}
			std::string w2;
			if ( !WwVkPresent::aoDecalRun( run, w2 ) ) {
				std::printf( "AODECALDRAW %s red %d refused: %s\n", armName, red, w2.c_str() );
				return Pass {};
			}
			return Pass { readR( vkTarget ), readR( vkSelf ) };
		};
		const Pass vk = vkRun( 0 );
		okAll = !vk.target.empty() && judge( armName, "main", gl, vk ) && okAll;
		for ( int r = 0; r < 6; r++ ) {
			const Pass vr = vkRun( 1 << r );
			const bool held = !vr.target.empty() && judge( armName, redName[r], gl, vr );
			std::printf( "AODECALDRAW %s red %d %s %s\n", armName, 1 << r, redName[r], held ? "PASSED (a red must FAIL)" : "FAILED as it must" );
			okAll = okAll && !held && !vr.target.empty();
		}
#endif
	}
#ifndef WW_VK_AODECAL
	std::printf( "AODECALDRAW GATE FAILED (no route: WW_VK_AODECAL not defined; GL coverage and probes above)\n" );
	return 1;
#else
	std::printf( okAll ? "AODECALDRAW GATE OK\n" : "AODECALDRAW GATE FAILED\n" );
	return okAll ? 0 : 1;
#endif
}

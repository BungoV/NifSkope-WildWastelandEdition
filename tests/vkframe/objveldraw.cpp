/* tests/vkframe/objveldraw.cpp -- lane VKOBJ10 (2026-11-16): the gate for the TAA inputs' moving-object redraw on Vulkan
 * (taainputs.cpp wwTaaInputsPass step 3, taa_objvel.prog; twins objvel_vs / objvel_ps from res/shaders/hlsl/post).
 *
 * BAR, committed before this gate's first run (it is never widened; a seeding change that lets a red reach its branch is
 * allowed, with the reason written here):
 *  - GL is the reference: GL's own taa_objvel.prog draws the fixture's movers into an RG16F vel + RGBA8 mask frame over a
 *    blitted depth, in the pass's state (LEQUAL, depth mask off, glPolygonOffset( -1, -4 ), blend off, cull off, att1
 *    masked to B; coverage pass 1: att0 off, att1 R only). Vulkan draws the same movers through the route into shared twins.
 *  - Depth: the depth attachment is read-only in this pass (GL never writes it): Vulkan's depth after the pass must equal
 *    the blitted input EXACTLY (0 differing texels).
 *  - Mask (RGBA8, values 0 / 255 only): EXACT, 0 differing texels.
 *  - Coverage: the set of texels each side wrote (vel or mask non-zero) EXACT.
 *  - vel (RG16F): max abs difference <= 2 half-steps of the GL value (2^-10 relative, the RG16F rounding of the same
 *    float); a texel past that FAILS. GL vel non-zero on >= 20 % of covered texels or the case is TESTS NOTHING.
 *  - The fixture: rigid movers with clipPrev != clipCur, a skinned mover (client arrays, attr 8 prev != cur), an
 *    alpha-tested mover, a blended mover (coverage passes 0 and 1), one mover coincident with the frame depth so the
 *    depth bias decides it (GL probe: polygon offset off moves its coverage on >= 20 % of its texels, printed).
 *  - Reds, each must FAIL: 1 prev = cur on the skinned mover; 2 pass 1 writes vel; 3 no depth bias; 4 a b31 offset
 *    shift (one row). A refused input (fxMode 2 / 8 / 16, velMode 2) must refuse by name and leave GL's frame untouched.
 *  - Every mover covers >= 64 texels on GL (printed per mover), or the case is TESTS NOTHING.
 *
 * Seeding (lane VKOBJ13, written before the first run; no bar clause changes):
 *  - 256 x 192, the whole fixture once per depth format: D24S8 (0x88F0) and D32F (0x8CAC). Each format has its own shared
 *    g.vel / g.mask / g.depth (sharedTarget) and plain GL inputs; every run starts from the inputs (glCopyImageSubData).
 *  - Inputs: vel a non-zero half pattern, mask G and A a 0 / 255 pattern (R, B 0), depth cleared 0.999 with an
 *    occluder cleared 0.8 over rigid B's right half. "Covered" = the texel's mask or vel differs from the input.
 *  - Movers: 8 x 8 quad grids (4 vertices a quad), a non-symmetric frustum (near 0.5, far 20), model rotations and
 *    translations; clipJit = the frustum jittered by (0.37, -0.21) px times the model. VBOs in one VAO a mover (GL
 *    attributes 0 / 1 / 7 / 8); the route takes the same arrays. The skinned mover's attr 8 is displaced 0.12 local
 *    units and more; its clipPrev = clipCur.
 *  - BaseMap: one 64 x 64 BC3 texture made by the texture cache (objTexMake), 3 levels of flat-alpha blocks in a 4 x 4
 *    patch layout (alpha 38 / 217), LINEAR_MIPMAP_LINEAR, REPEAT, uvScale (1.5, 1.25), uvOffset (0.1, -0.2). The
 *    alpha-tested mover tests 0.5; the blended mover (fxMode 1) has its vertex alpha constant a quad (0.45 or 1.0) and
 *    covThreshold 0.5 (texels on both sides printed, each >= 64).
 *  - The coincident movers: TWO (the bar names one; both are judged). Their depth is laid by GL's own polygon offset
 *    under the same clipJit and geometry: C1 screen-parallel, left half units -2, right half units -6 (the pass's -4
 *    lands inside -2 and beyond -6 with 2 units each way, the tie at 4 skipped); C2 tilted 63 degrees, left half factor
 *    -0.5, right half factor -1.5 (the slope term). Per-band GL / Vulkan counts are printed.
 *  - Not judged here, by construction: modelViewMatrix and normalMatrix feed only the grass fade (packed zero) and the
 *    falloff (fxMode 8, refused), so their packing is sent but nothing reads it; the X8_D24 and D32F_S8 slots.
 *  - Also checked: GL drawing into the shared targets equals GL's plain reference EXACT, and a BaseMap with no
 *    texture-cache twin (a plain RGBA8 texture) refuses by name with the targets untouched.
 * Usage: objveldraw <out dir> <tree root>. Last line `OBJVELDRAW GATE OK` or `OBJVELDRAW GATE FAILED`.
 */
#define main fxview_main
#include "fxview.cpp"
#undef main

#include <array>

namespace ovd
{
using namespace fv;
using WwVkPresent::ObjVelDraw;
using WwVkPresent::ObjVelRun;

constexpr int W = 256, H = 192;
constexpr GLenum kArr = 0x8892, kElem = 0x8893, kDepthAtt = 0x8D00, kDs = 0x84F9, kU248 = 0x84FA, kCmpMode = 0x884C,
	kBc3 = 0x83F3, kD24s8 = 0x88F0, kD32f = 0x8CAC, kRg16f = 0x822F;

struct Ox
{
	void( WINAPI * genVertexArrays )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindVertexArray )( GLuint ) = nullptr;
	void( WINAPI * vertexAttribPointer )( GLuint, GLint, GLenum, GLboolean, GLsizei, const void * ) = nullptr;
	void( WINAPI * enableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * colorMaski )( GLuint, GLboolean, GLboolean, GLboolean, GLboolean ) = nullptr;
	void( WINAPI * compressedTextureSubImage2D )( GLuint, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const void * ) = nullptr;
	void( WINAPI * textureParameteri )( GLuint, GLenum, GLint ) = nullptr;
};
Ox ox;

bool loadMore()
{
	return load( ox.genVertexArrays, "glGenVertexArrays" ) && load( ox.bindVertexArray, "glBindVertexArray" )
		&& load( ox.vertexAttribPointer, "glVertexAttribPointer" ) && load( ox.enableVertexAttribArray, "glEnableVertexAttribArray" )
		&& load( ox.colorMaski, "glColorMaski" ) && load( ox.compressedTextureSubImage2D, "glCompressedTextureSubImage2D" )
		&& load( ox.textureParameteri, "glTextureParameteri" );
}

// ---- matrices (column-major, as glUniformMatrix4fv takes them) ----------------------------------------------------
struct M4
{
	float m[16] = {};
};

M4 ident()
{
	M4 r;
	r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
	return r;
}

M4 mul( const M4 & a, const M4 & b )
{
	M4 r;
	for ( int c = 0; c < 4; c++ )
		for ( int w = 0; w < 4; w++ ) {
			float s = 0.0f;
			for ( int k = 0; k < 4; k++ )
				s += a.m[k * 4 + w] * b.m[c * 4 + k];
			r.m[c * 4 + w] = s;
		}
	return r;
}

M4 trans( float x, float y, float z )
{
	M4 r = ident();
	r.m[12] = x;
	r.m[13] = y;
	r.m[14] = z;
	return r;
}

M4 rotX( float a )
{
	M4 r = ident();
	const float c = std::cos( a ), s = std::sin( a );
	r.m[5] = c;
	r.m[6] = s;
	r.m[9] = -s;
	r.m[10] = c;
	return r;
}

M4 rotY( float a )
{
	M4 r = ident();
	const float c = std::cos( a ), s = std::sin( a );
	r.m[0] = c;
	r.m[2] = -s;
	r.m[8] = s;
	r.m[10] = c;
	return r;
}

M4 rotZ( float a )
{
	M4 r = ident();
	const float c = std::cos( a ), s = std::sin( a );
	r.m[0] = c;
	r.m[1] = s;
	r.m[4] = -s;
	r.m[5] = c;
	return r;
}

M4 frustum( float l, float r, float b, float t, float n, float f )
{
	M4 p;
	p.m[0] = 2.0f * n / ( r - l );
	p.m[5] = 2.0f * n / ( t - b );
	p.m[8] = ( r + l ) / ( r - l );
	p.m[9] = ( t + b ) / ( t - b );
	p.m[10] = -( f + n ) / ( f - n );
	p.m[11] = -1.0f;
	p.m[14] = -2.0f * f * n / ( f - n );
	return p;
}

// ---- the movers ----------------------------------------------------------------------------------------------------
constexpr int kN = 8;
constexpr float kHalf = 0.7f;

struct Mover
{
	std::string name;
	std::vector<float> pos, prev, col, uv;
	std::vector<std::uint16_t> idx;
	M4 model, modelPrev;
	int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	GLuint vao = 0;
};

void build( Mover & mv, bool skin, bool colour, bool coords )
{
	for ( int j = 0; j < kN; j++ )
		for ( int i = 0; i < kN; i++ ) {
			const std::uint16_t base = std::uint16_t( mv.pos.size() / 3 );
			const int ci[4] = { i, i + 1, i + 1, i }, cj[4] = { j, j, j + 1, j + 1 };
			const float qa = ( i + 2 * j ) % 3 == 0 ? 0.45f : 1.0f;  // the blended mover's vertex alpha, flat a quad
			for ( int k = 0; k < 4; k++ ) {
				const float u = float( ci[k] ) / kN, v = float( cj[k] ) / kN;
				const float x = -kHalf + 2.0f * kHalf * u, y = -kHalf + 2.0f * kHalf * v;
				mv.pos.insert( mv.pos.end(), { x, y, 0.0f } );
				if ( skin )
					mv.prev.insert( mv.prev.end(), { x + 0.12f + 0.05f * std::sin( 3.0f * y ), y - 0.06f * std::cos( 2.0f * x ),
						0.04f * std::sin( 2.0f * x + y ) } );
				if ( colour )
					mv.col.insert( mv.col.end(), { 1.0f, 1.0f, 1.0f, qa } );
				if ( coords )
					mv.uv.insert( mv.uv.end(), { u, v } );
			}
			mv.idx.insert( mv.idx.end(), { base, std::uint16_t( base + 1 ), std::uint16_t( base + 2 ), base,
				std::uint16_t( base + 2 ), std::uint16_t( base + 3 ) } );
		}
}

GLuint vbo( GLenum target, const void * data, std::size_t bytes )
{
	GLuint b = 0;
	f.genBuffers( 1, &b );
	f.bindBuffer( target, b );
	f.bufferData( target, std::ptrdiff_t( bytes ), data, fStatic );
	return b;
}

void attr( GLuint loc, const std::vector<float> & v, int n )
{
	if ( v.empty() )
		return;
	vbo( kArr, v.data(), v.size() * 4 );
	ox.vertexAttribPointer( loc, n, GL_FLOAT, GL_FALSE, 0, nullptr );
	ox.enableVertexAttribArray( loc );
}

void makeVao( Mover & mv )
{
	ox.genVertexArrays( 1, &mv.vao );
	ox.bindVertexArray( mv.vao );
	attr( 0, mv.pos, 3 );
	attr( 1, mv.col, 4 );
	attr( 7, mv.uv, 2 );
	attr( 8, mv.prev, 3 );
	vbo( kElem, mv.idx.data(), mv.idx.size() * 2 );
	ox.bindVertexArray( 0 );
	f.bindBuffer( kArr, 0 );
}

void bbox( Mover & mv, const M4 & cj )
{
	float lo[2] = { 1e9f, 1e9f }, hi[2] = { -1e9f, -1e9f };
	const float xs[4] = { -kHalf, kHalf, kHalf, -kHalf }, ys[4] = { -kHalf, -kHalf, kHalf, kHalf };
	for ( int k = 0; k < 4; k++ ) {
		const float cx = cj.m[0] * xs[k] + cj.m[4] * ys[k] + cj.m[12], cy = cj.m[1] * xs[k] + cj.m[5] * ys[k] + cj.m[13];
		const float cw = cj.m[3] * xs[k] + cj.m[7] * ys[k] + cj.m[15];
		const float wx = ( cx / cw * 0.5f + 0.5f ) * W, wy = ( cy / cw * 0.5f + 0.5f ) * H;
		lo[0] = std::min( lo[0], wx );
		lo[1] = std::min( lo[1], wy );
		hi[0] = std::max( hi[0], wx );
		hi[1] = std::max( hi[1], wy );
	}
	mv.x0 = std::max( 0, int( std::floor( lo[0] ) ) );
	mv.y0 = std::max( 0, int( std::floor( lo[1] ) ) );
	mv.x1 = std::min( W - 1, int( std::ceil( hi[0] ) ) );
	mv.y1 = std::min( H - 1, int( std::ceil( hi[1] ) ) );
}

std::vector<Mover> movers;
std::vector<ObjVelDraw> draws;
std::vector<int> drawMover;
const char * drawName[8] = { "rigidA", "rigidB", "skinned", "alphatest", "blend p0", "blend p1", "coincident", "tilted" };
GLuint pObj = 0, pDepth = 0;

ObjVelDraw drawOf( const Mover & mv, const M4 & P, const M4 & Pj )
{
	ObjVelDraw d;
	d.pos = mv.pos.data();
	d.prev = mv.prev.empty() ? nullptr : mv.prev.data();
	d.color = mv.col.empty() ? nullptr : mv.col.data();
	d.uv = mv.uv.empty() ? nullptr : mv.uv.data();
	d.nv = int( mv.pos.size() / 3 );
	d.idx = mv.idx.data();
	d.nIdx = int( mv.idx.size() );
	const M4 cj = mul( Pj, mv.model ), cc = mul( P, mv.model ), cp = mul( P, mv.modelPrev );
	std::memcpy( d.clipJit, cj.m, 64 );
	std::memcpy( d.clipCur, cc.m, 64 );
	std::memcpy( d.clipPrev, cp.m, 64 );
	std::memcpy( d.modelView, mv.model.m, 64 );
	for ( int c = 0; c < 3; c++ )
		for ( int r = 0; r < 3; r++ )
			d.normalMatrix[c * 3 + r] = mv.model.m[c * 4 + r];
	return d;
}

// ---- frames ----------------------------------------------------------------------------------------------------------
struct Frame
{
	std::vector<std::uint16_t> vel;
	std::vector<std::uint8_t> mask;
	std::vector<std::uint32_t> depth;
	bool operator==( const Frame & o ) const { return vel == o.vel && mask == o.mask && depth == o.depth; }
};

struct Set
{
	GLuint v = 0, m = 0, d = 0, fb = 0;
};

bool gD24 = true;

Frame readSet( const Set & s )
{
	Frame fr;
	fr.vel.resize( std::size_t( W ) * H * 2 );
	fr.mask.resize( std::size_t( W ) * H * 4 );
	fr.depth.resize( std::size_t( W ) * H );
	glBindTexture( GL_TEXTURE_2D, s.v );
	glGetTexImage( GL_TEXTURE_2D, 0, fRg, fHalf, fr.vel.data() );
	glBindTexture( GL_TEXTURE_2D, s.m );
	glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, fr.mask.data() );
	glBindTexture( GL_TEXTURE_2D, s.d );
	if ( gD24 )
		glGetTexImage( GL_TEXTURE_2D, 0, kDs, kU248, fr.depth.data() );
	else
		glGetTexImage( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, fr.depth.data() );
	glBindTexture( GL_TEXTURE_2D, 0 );
	return fr;
}

void copyTex( GLuint a, GLuint b )
{
	f.copyImageSubData( a, GL_TEXTURE_2D, 0, 0, 0, 0, b, GL_TEXTURE_2D, 0, 0, 0, 0, W, H, 1 );
}

void reset( const Set & in, const Set & s )
{
	copyTex( in.v, s.v );
	copyTex( in.m, s.m );
	copyTex( in.d, s.d );
}

GLuint makeFb( GLuint c0, GLuint c1, GLuint d )
{
	GLuint b = 0;
	f.genFramebuffers( 1, &b );
	f.bindFramebuffer( fFb, b );
	f.framebufferTexture2D( fFb, fCol0, GL_TEXTURE_2D, c0, 0 );
	if ( c1 )
		f.framebufferTexture2D( fFb, fCol0 + 1, GL_TEXTURE_2D, c1, 0 );
	f.framebufferTexture2D( fFb, gD24 ? fDsAtt : kDepthAtt, GL_TEXTURE_2D, d, 0 );
	const GLenum bufs[2] = { fCol0, fCol0 + 1 };
	f.drawBuffers( c1 ? 2 : 1, bufs );
	const bool ok = f.checkFramebufferStatus( fFb ) == fComplete;
	f.bindFramebuffer( fFb, 0 );
	return ok ? b : 0;
}

GLuint depthTex( GLenum fmt )
{
	GLuint t = 0;
	glGenTextures( 1, &t );
	glBindTexture( GL_TEXTURE_2D, t );
	if ( fmt == kD24s8 )
		glTexImage2D( GL_TEXTURE_2D, 0, GLint( fmt ), W, H, 0, kDs, kU248, nullptr );
	else
		glTexImage2D( GL_TEXTURE_2D, 0, GLint( fmt ), W, H, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr );
	texParams( GL_TEXTURE_2D, false, false, false );
	glTexParameteri( GL_TEXTURE_2D, kCmpMode, GL_NONE );
	return t;
}

bool covered( const Frame & in, const Frame & x, std::size_t i )
{
	return std::memcmp( &in.mask[i * 4], &x.mask[i * 4], 4 ) != 0 || in.vel[i * 2] != x.vel[i * 2] || in.vel[i * 2 + 1] != x.vel[i * 2 + 1];
}

struct Rect
{
	int x = 0, y = 0, w = 0, h = 0;
};

std::size_t covCount( const Frame & in, const Frame & x, const Rect * r = nullptr )
{
	std::size_t n = 0;
	for ( int y = 0; y < H; y++ )
		for ( int xx = 0; xx < W; xx++ ) {
			if ( r && ( xx < r->x || xx >= r->x + r->w || y < r->y || y >= r->y + r->h ) )
				continue;
			n += covered( in, x, std::size_t( y ) * W + xx );
		}
	return n;
}

std::size_t moved( const Frame & a, const Frame & b )
{
	std::size_t n = 0;
	for ( std::size_t i = 0; i < std::size_t( W ) * H; i++ )
		n += std::memcmp( &a.mask[i * 4], &b.mask[i * 4], 4 ) != 0 || a.vel[i * 2] != b.vel[i * 2] || a.vel[i * 2 + 1] != b.vel[i * 2 + 1];
	return n;
}

struct Judge
{
	std::size_t depth = 0, mask = 0, cov = 0, covGl = 0, covVk = 0, velPast2 = 0;
	int velMax = 0;
	bool ok() const { return depth == 0 && mask == 0 && cov == 0 && velPast2 == 0 && covGl > 0; }
};

Judge judge( const Frame & in, const Frame & g, const Frame & v )
{
	Judge j;
	for ( std::size_t i = 0; i < std::size_t( W ) * H; i++ ) {
		j.depth += v.depth[i] != in.depth[i];
		j.mask += std::memcmp( &g.mask[i * 4], &v.mask[i * 4], 4 ) != 0;
		const bool cg = covered( in, g, i ), cv = covered( in, v, i );
		j.covGl += cg;
		j.covVk += cv;
		j.cov += cg != cv;
		int d = 0;
		for ( int c = 0; c < 2; c++ )
			d = std::max( d, std::abs( ordered( g.vel[i * 2 + c] ) - ordered( v.vel[i * 2 + c] ) ) );
		j.velMax = std::max( j.velMax, d );
		j.velPast2 += d > 2;
	}
	return j;
}

void printJudge( const char * fmt, const char * what, const Judge & j, const char * verdict )
{
	std::printf( "OBJVELDRAW %s %s: depth %zu mask %zu cov %zu (gl %zu vk %zu) vel max %d past2 %zu -> %s\n", fmt, what, j.depth,
		j.mask, j.cov, j.covGl, j.covVk, j.velMax, j.velPast2, verdict );
}

// ---- GL's draws --------------------------------------------------------------------------------------------------------
void m4( GLuint p, const char * n, const float * v )
{
	const GLint l = f.getUniformLocation( p, n );
	if ( l >= 0 )
		f.uniformMatrix4fv( l, 1, GL_FALSE, v );
}

void i1( GLuint p, const char * n, int v )
{
	const GLint l = f.getUniformLocation( p, n );
	if ( l >= 0 )
		f.uniform1i( l, v );
}

void f1( GLuint p, const char * n, float v )
{
	const GLint l = f.getUniformLocation( p, n );
	if ( l >= 0 )
		f.uniform1fv( l, 1, &v );
}

void glOne( const ObjVelDraw & d, GLuint vao )
{
	const GLuint p = pObj;
	f.useProgram( p );
	m4( p, "clipJit", d.clipJit );
	m4( p, "clipCur", d.clipCur );
	m4( p, "clipPrev", d.clipPrev );
	m4( p, "modelViewMatrix", d.modelView );
	const GLint ln = f.getUniformLocation( p, "normalMatrix" );
	if ( ln >= 0 )
		f.uniformMatrix3fv( ln, 1, GL_FALSE, d.normalMatrix );
	i1( p, "velMode", d.velMode );
	i1( p, "alphaTest", d.alphaTest );
	i1( p, "fxMode", d.fxMode );
	i1( p, "covMode", d.covMode );
	f1( p, "alpha", d.alpha );
	f1( p, "alphaThreshold", d.alphaThreshold );
	f1( p, "covThreshold", d.covThreshold );
	const GLint ls = f.getUniformLocation( p, "uvScale" ), lo = f.getUniformLocation( p, "uvOffset" );
	if ( ls >= 0 )
		f.uniform2fv( ls, 1, d.uvScale );
	if ( lo >= 0 )
		f.uniform2fv( lo, 1, d.uvOffset );
	i1( p, "BaseMap", 0 );
	f.activeTexture( fTex0 );
	glBindTexture( GL_TEXTURE_2D, d.baseTex );
	if ( d.pass == 1 ) {
		ox.colorMaski( 0, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
		ox.colorMaski( 1, GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE );
	}
	ox.bindVertexArray( vao );
	glDrawElements( GL_TRIANGLES, d.nIdx, GL_UNSIGNED_SHORT, nullptr );
	ox.bindVertexArray( 0 );
	if ( d.pass == 1 ) {
		ox.colorMaski( 0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		ox.colorMaski( 1, GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE );
	}
	glBindTexture( GL_TEXTURE_2D, 0 );
}

// the pass's state (taainputs.cpp step 3), the listed draws in order
GLenum glDrawList( const std::vector<int> & which, bool bias, GLuint fb )
{
	f.bindFramebuffer( fFb, fb );
	glViewport( 0, 0, W, H );
	glDisable( GL_SCISSOR_TEST );
	glDisable( GL_BLEND );
	glDisable( GL_CULL_FACE );
	glDisable( GL_STENCIL_TEST );
	glDepthMask( GL_FALSE );
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LEQUAL );
	if ( bias ) {
		glEnable( GL_POLYGON_OFFSET_FILL );
		glPolygonOffset( -1.0f, -4.0f );
	} else {
		glDisable( GL_POLYGON_OFFSET_FILL );
	}
	const GLenum bufs[2] = { fCol0, fCol0 + 1 };
	f.drawBuffers( 2, bufs );
	ox.colorMaski( 0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	ox.colorMaski( 1, GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE );
	for ( int d : which )
		glOne( draws[std::size_t( d )], movers[std::size_t( drawMover[std::size_t( d )] )].vao );
	ox.colorMaski( 1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	glDisable( GL_POLYGON_OFFSET_FILL );
	glDisable( GL_DEPTH_TEST );
	f.bindFramebuffer( fFb, 0 );
	return glGetError();
}

std::vector<int> allDraws()
{
	std::vector<int> v;
	for ( std::size_t i = 0; i < draws.size(); i++ )
		v.push_back( int( i ) );
	return v;
}

// ---- one depth format ------------------------------------------------------------------------------------------------
struct Result
{
	bool ok = true, nothing = false;
};

Result runFormat( int fi, GLuint plainBase )
{
	Result res;
	const GLenum fmt = fi == 0 ? kD24s8 : kD32f;
	const char * fn = fi == 0 ? "d24s8" : "d32f";
	gD24 = fmt == kD24s8;
	std::string why;
	auto fail = [&]( const std::string & w ) {
		std::printf( "OBJVELDRAW %s FAIL (%s)\n", fn, w.c_str() );
		res.ok = false;
		return res;
	};

	// the inputs
	std::vector<std::uint16_t> velIn( std::size_t( W ) * H * 2 );
	std::vector<std::uint8_t> maskIn( std::size_t( W ) * H * 4, 0 );
	for ( int y = 0; y < H; y++ )
		for ( int x = 0; x < W; x++ ) {
			const std::size_t i = std::size_t( y ) * W + x;
			velIn[i * 2] = std::uint16_t( 0x2000 + ( ( x * 7 + y * 13 ) & 0x3FF ) );
			velIn[i * 2 + 1] = std::uint16_t( 0xA000 + ( ( x * 11 + y * 5 ) & 0x3FF ) );
			maskIn[i * 4 + 1] = ( ( x / 3 + y / 5 ) & 1 ) ? 255 : 0;
			maskIn[i * 4 + 3] = ( ( x / 7 + y / 2 ) & 1 ) ? 255 : 0;
		}
	Set in, gl, gp, sh;
	in.v = tex2d( W, H, kRg16f, fRg, fHalf, velIn.data(), false );
	in.m = tex2d( W, H, fRgba8, GL_RGBA, GL_UNSIGNED_BYTE, maskIn.data(), false );
	in.d = depthTex( fmt );
	gl.v = tex2d( W, H, kRg16f, fRg, fHalf, nullptr, false );
	gl.m = tex2d( W, H, fRgba8, GL_RGBA, GL_UNSIGNED_BYTE, nullptr, false );
	gl.d = depthTex( fmt );
	const std::uintptr_t keyBase = 0x4F560000u + std::uintptr_t( fi ) * 16u;
	unsigned sv = 0, sm = 0, sd = 0;
	if ( !WwVkPresent::sharedTarget( keyBase + 0, 0, W, H, 1, kRg16f, sv, why )
		|| !WwVkPresent::sharedTarget( keyBase + 1, 0, W, H, 1, fRgba8, sm, why )
		|| !WwVkPresent::sharedTarget( keyBase + 2, 0, W, H, 1, fmt, sd, why ) )
		return fail( "sharedTarget: " + why );
	sh.v = sv;
	sh.m = sm;
	sh.d = sd;
	for ( GLuint t : { sh.v, sh.m, sh.d } ) {
		glBindTexture( GL_TEXTURE_2D, t );
		texParams( GL_TEXTURE_2D, false, false, false );
	}
	glBindTexture( GL_TEXTURE_2D, sh.d );
	glTexParameteri( GL_TEXTURE_2D, kCmpMode, GL_NONE );
	glBindTexture( GL_TEXTURE_2D, 0 );
	gl.fb = makeFb( gl.v, gl.m, gl.d );
	sh.fb = makeFb( sh.v, sh.m, sh.d );
	if ( !gl.fb || !sh.fb )
		return fail( "framebuffer incomplete" );
	gp = gl;  // the per-mover and probe draws reuse GL's reference set (G is read before them)

	// the depth input: cleared 0.999, the occluder 0.8 over rigid B's right half, the coincident movers' bands by GL's
	// own polygon offset under their clipJit
	const GLuint scratch = tex2d( W, H, fRgba8, GL_RGBA, GL_UNSIGNED_BYTE, nullptr, false );
	const GLuint seedFb = makeFb( scratch, 0, in.d );
	if ( !seedFb )
		return fail( "seed framebuffer incomplete" );
	f.bindFramebuffer( fFb, seedFb );
	glViewport( 0, 0, W, H );
	glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
	glDepthMask( GL_TRUE );
	glDisable( GL_SCISSOR_TEST );
	glClearDepth( 0.999 );
	glClearStencil( 0 );
	glClear( GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
	const Mover & mb = movers[1];
	const int bm = ( mb.x0 + mb.x1 ) / 2;
	glEnable( GL_SCISSOR_TEST );
	glScissor( bm, mb.y0, mb.x1 - bm + 2, mb.y1 - mb.y0 + 2 );
	glClearDepth( 0.8 );
	glClear( GL_DEPTH_BUFFER_BIT );
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_ALWAYS );
	glEnable( GL_POLYGON_OFFSET_FILL );
	f.useProgram( pDepth );
	Rect bands[4];
	const char * bandName[4] = { "C1 units -2", "C1 units -6", "C2 factor -0.5", "C2 factor -1.5" };
	const float bandOff[4][2] = { { 0.0f, -2.0f }, { 0.0f, -6.0f }, { -0.5f, 0.0f }, { -1.5f, 0.0f } };
	for ( int b = 0; b < 4; b++ ) {
		const int mi = b < 2 ? 5 : 6;
		const Mover & mv = movers[std::size_t( mi )];
		const int xm = ( mv.x0 + mv.x1 ) / 2;
		bands[b] = ( b & 1 ) == 0 ? Rect { mv.x0, mv.y0, xm - mv.x0, mv.y1 - mv.y0 + 1 } : Rect { xm, mv.y0, mv.x1 - xm + 1, mv.y1 - mv.y0 + 1 };
		glScissor( bands[b].x, bands[b].y, bands[b].w, bands[b].h );
		glPolygonOffset( bandOff[b][0], bandOff[b][1] );
		const int di = mi == 5 ? 6 : 7;
		m4( pDepth, "m", draws[std::size_t( di )].clipJit );
		ox.bindVertexArray( mv.vao );
		glDrawElements( GL_TRIANGLES, GLsizei( mv.idx.size() ), GL_UNSIGNED_SHORT, nullptr );
		ox.bindVertexArray( 0 );
	}
	glDisable( GL_POLYGON_OFFSET_FILL );
	glDisable( GL_SCISSOR_TEST );
	glDisable( GL_DEPTH_TEST );
	glDepthFunc( GL_LEQUAL );
	glDepthMask( GL_FALSE );
	glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	f.bindFramebuffer( fFb, 0 );
	if ( const GLenum e = glGetError() )
		return fail( "GL error 0x" + std::to_string( e ) + " laying the depth" );
	const Frame FIN =readSet( in );

	// GL's reference, the per-mover coverage and the bias probe
	reset( in, gl );
	if ( const GLenum e = glDrawList( allDraws(), true, gl.fb ) )
		return fail( "GL error " + std::to_string( e ) + " in the reference" );
	const Frame G = readSet( gl );
	auto glAlone = [&]( const std::vector<int> & which, bool bias ) {
		reset( in, gp );
		glDrawList( which, bias, gp.fb );
		return readSet( gp );
	};
	for ( int mi = 0; mi < int( movers.size() ); mi++ ) {
		std::vector<int> which;
		for ( std::size_t d = 0; d < draws.size(); d++ )
			if ( drawMover[d] == mi )
				which.push_back( int( d ) );
		const std::size_t c = covCount( FIN,glAlone( which, true ) );
		std::printf( "OBJVELDRAW %s mover %s: gl covers %zu\n", fn, movers[std::size_t( mi )].name.c_str(), c );
		if ( c < 64 )
			res.nothing = true;
		if ( mi == 4 ) {
			const std::size_t c0 = covCount( FIN,glAlone( { 4 }, true ) ), c1 = covCount( FIN,glAlone( { 5 }, true ) );
			std::printf( "OBJVELDRAW %s blend sides: covThreshold passed %zu (pass 0), below %zu (pass 1)\n", fn, c0, c1 );
			if ( c0 < 64 || c1 < 64 )
				res.nothing = true;
		}
		if ( mi >= 5 ) {
			const Frame on = glAlone( which, true ), off = glAlone( which, false );
			std::size_t mv = 0;
			for ( std::size_t i = 0; i < std::size_t( W ) * H; i++ )
				mv += covered( FIN,on, i ) != covered( FIN,off, i );
			const double share = c ? double( mv ) / double( c ) : 0.0;
			std::printf( "OBJVELDRAW %s bias probe %s: polygon offset off moves %zu of %zu texels (%.1f %%)\n", fn,
				movers[std::size_t( mi )].name.c_str(), mv, c, share * 100.0 );
			if ( share < 0.2 )
				res.nothing = true;
		}
	}
	std::size_t p0 = 0, nz = 0;
	for ( std::size_t i = 0; i < std::size_t( W ) * H; i++ )
		if ( G.mask[i * 4 + 2] == 255 && FIN.mask[i * 4 + 2] == 0 ) {
			p0++;
			nz += ( G.vel[i * 2] & 0x7FFF ) != 0 || ( G.vel[i * 2 + 1] & 0x7FFF ) != 0;
		}
	std::printf( "OBJVELDRAW %s gl vel non-zero on %zu of %zu pass-0 texels (%.1f %%)\n", fn, nz, p0, p0 ? 100.0 * nz / p0 : 0.0 );
	if ( p0 == 0 || nz * 5 < p0 )
		res.nothing = true;

	// Vulkan through the route
	ObjVelRun base;
	base.draws = draws;
	base.velKey = keyBase + 0;
	base.maskKey = keyBase + 1;
	base.depthKey = keyBase + 2;
	base.w = W;
	base.h = H;
	reset( in, sh );
	if ( !WwVkPresent::objVelRun( base, why ) )
		return fail( "the route refused the fixture: " + why );
	const Frame V = readSet( sh );
	const Judge jm = judge( FIN,G, V );
	printJudge( fn, "main", jm, jm.ok() ? ( jm.velMax == 0 && jm.mask == 0 ? "EXACT" : "BOUND" ) : "FAIL" );
	if ( !jm.ok() )
		res.ok = false;
	for ( int b = 0; b < 4; b++ )
		std::printf( "OBJVELDRAW %s band %s: gl %zu vk %zu\n", fn, bandName[b], covCount( FIN,G, &bands[b] ), covCount( FIN,V, &bands[b] ) );

	// the reds
	const char * redName[5] = { "", "prev=cur", "pass1-vel", "no-bias", "b31-shift" };
	for ( int k = 1; k <= 4; k++ ) {
		ObjVelRun rr = base;
		rr.red = k;
		reset( in, sh );
		std::string w;
		if ( !WwVkPresent::objVelRun( rr, w ) ) {
			std::printf( "OBJVELDRAW %s RED %d %s refused (%s): not a FAIL\n", fn, k, redName[k], w.c_str() );
			res.ok = false;
			continue;
		}
		const Frame R = readSet( sh );
		const Judge jr = judge( FIN,G, R );
		printJudge( fn, ( std::string( "RED " ) + std::to_string( k ) + " " + redName[k] ).c_str(), jr, jr.ok() ? "PASSED (bad)" : "FAIL" );
		std::printf( "OBJVELDRAW %s RED %d moved %zu texels against the unred Vulkan run\n", fn, k, moved( V, R ) );
		if ( jr.ok() )
			res.ok = false;
	}

	// the refusals: by name, the targets untouched
	auto refuseCase = [&]( const char * name, const ObjVelRun & rr, const char * expect ) {
		reset( in, sh );
		const Frame pre = readSet( sh );
		std::string w;
		const bool ran = WwVkPresent::objVelRun( rr, w );
		const Frame post = readSet( sh );
		const bool named = !ran && w.find( expect ) != std::string::npos, same = pre == post;
		std::printf( "OBJVELDRAW %s refuse %s: %s (%s), targets %s -> %s\n", fn, name, ran ? "RAN" : "refused", w.c_str(),
			same ? "byte-equal" : "CHANGED", named && same ? "OK" : "FAIL" );
		if ( !( named && same ) )
			res.ok = false;
	};
	const char * refName[4] = { "fxMode 2", "fxMode 8", "fxMode 16", "velMode 2" };
	for ( int k = 0; k < 4; k++ ) {
		ObjVelRun rr = base;
		if ( k < 3 )
			rr.draws[0].fxMode = k == 0 ? 2 : k == 1 ? 8 : 16;
		else
			rr.draws[0].velMode = 2;
		refuseCase( refName[k], rr, refName[k] );
	}
	{
		ObjVelRun rr = base;
		rr.draws[3].baseTex = plainBase;
		refuseCase( "BaseMap plain RGBA8", rr, "no texture-cache twin" );
	}
	// GL's own draw into the shared targets (what the app does after a refusal) equals the plain reference
	reset( in, sh );
	glDrawList( allDraws(), true, sh.fb );
	const Frame GS = readSet( sh );
	const bool gsSame = GS == G;
	std::printf( "OBJVELDRAW %s gl into the shared targets: %s\n", fn, gsSame ? "EXACT" : "FAIL" );
	if ( !gsSame )
		res.ok = false;
	const WwVkPresent::ObjVelStat st = WwVkPresent::takeObjVelStat();
	std::printf( "OBJVELDRAW %s stat: runs %d draws %d crossings %d gl %d fails %d pipelines %d twins %d cpu %.2f ms (max %.2f, build "
		"%.1f)%s%s\n", fn, st.runs, st.draws, st.crossings, st.gl, st.fails, st.pipelines, st.twins, st.cpuMs, st.maxMs, st.buildMs,
		st.failWhy.empty() ? "" : " last fail: ", st.failWhy.c_str() );
	return res;
}
}  // namespace ovd

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace ovd;
	gTag = "OBJVELDRAW";
	auto fatal = [&]( const std::string & why ) {
		std::printf( "OBJVELDRAW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	if ( argc < 3 )
		return fatal( "usage: objveldraw <out dir> <tree root>" );
	const std::string root = argv[2];
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwobjvel";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwobjvel", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, W, H };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwobjvel", "objvel harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
		rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	const int attr[] = { 0x2001, 1, 0x2010, 1, 0x2011, 1, 0x2003, 0x2027, 0x2013, 0x202B, 0x2015, 8, 0x2017, 8, 0x2019, 8,
		0x201B, 8, 0x2022, 24, 0x2023, 8, 0 };
	int pf = 0;
	UINT nf = 0;
	if ( !choose || !choose( dc, attr, nullptr, 1, &pf, &nf ) || !nf )
		return fatal( "no pixel format" );
	PIXELFORMATDESCRIPTOR got {};
	DescribePixelFormat( dc, pf, sizeof( got ), &got );
	SetPixelFormat( dc, pf, &got );
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
	std::printf( "OBJVELDRAW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	pObj = program( readFile( root + "/res/shaders/features/post/taa_objvel.vert" ),
		readFile( root + "/res/shaders/features/post/taa_objvel.frag" ), "taa_objvel" );
	pDepth = program( "#version 410 core\nlayout ( location = 0 ) in vec3 p;\nuniform mat4 m;\n"
					  "void main() { gl_Position = m * vec4( p, 1.0 ); }\n",
		"#version 410 core\nout vec4 o;\nvoid main() { o = vec4( 0.0 ); }\n", "depth" );
	if ( !pObj || !pDepth )
		return fatal( "GL programs" );
	std::string why;
	if ( !WwVkPresent::viewBegin( W, H, 1, glProc, why ) )
		return fatal( "the view: " + why );
	const std::string refusal = WwVkPresent::objVelRefusal();
	if ( !refusal.empty() )
		return fatal( "objVelRefusal: " + refusal );

	// BaseMap: the texture cache's BC3 twin, flat alpha a block in a 4 x 4 patch layout at every level
	unsigned baseTex = 0;
	if ( !WwVkPresent::objTexMake( kBc3, 64, 64, 3, baseTex, why ) )
		return fatal( "objTexMake: " + why );
	for ( int l = 0; l < 3; l++ ) {
		const int bw = 16 >> l, ps = 4 >> l;  // blocks a side, blocks a patch side
		std::vector<std::uint8_t> blocks( std::size_t( bw ) * bw * 16, 0 );
		for ( int by = 0; by < bw; by++ )
			for ( int bx = 0; bx < bw; bx++ ) {
				const int px = bx / ps, py = by / ps;
				const std::uint8_t a = ( ( px * 3 + py * 5 + px * py ) % 4 ) < 2 ? 217 : 38;
				std::uint8_t * b = &blocks[( std::size_t( by ) * bw + bx ) * 16];
				b[0] = a;
				b[1] = a;
			}
		const int lw = 64 >> l;
		ox.compressedTextureSubImage2D( baseTex, l, 0, 0, lw, lw, kBc3, GLsizei( blocks.size() ), blocks.data() );
	}
	ox.textureParameteri( baseTex, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	ox.textureParameteri( baseTex, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	ox.textureParameteri( baseTex, GL_TEXTURE_WRAP_S, GL_REPEAT );
	ox.textureParameteri( baseTex, GL_TEXTURE_WRAP_T, GL_REPEAT );
	std::printf( "OBJVELDRAW BaseMap: BC3 twin %u, texture-cache key %s\n", baseTex,
		WwVkPresent::objTexKey( baseTex ) ? "present" : "MISSING" );
	std::vector<std::uint8_t> plain( 16 * 4, 200 );
	const GLuint plainBase = tex2d( 4, 4, fRgba8, GL_RGBA, GL_UNSIGNED_BYTE, plain.data(), true );

	// the movers: rigid A, rigid B (occluded right half), skinned, alpha-tested, blended, coincident C1, tilted C2
	const M4 P = frustum( -0.42f, 0.38f, -0.30f, 0.28f, 0.5f, 20.0f );
	M4 Pj = P;
	Pj.m[8] += 2.0f * 0.37f / W;
	Pj.m[9] -= 2.0f * 0.21f / H;
	movers.resize( 7 );
	const char * names[7] = { "rigidA", "rigidB", "skinned", "alphatest", "blend", "coincident", "tilted" };
	for ( int i = 0; i < 7; i++ )
		movers[std::size_t( i )].name = names[i];
	movers[0].model = mul( trans( -3.0f, 1.5f, -5.0f ), mul( rotY( 0.35f ), rotX( -0.25f ) ) );
	movers[0].modelPrev = mul( trans( -2.85f, 1.42f, -5.06f ), mul( rotY( 0.28f ), rotX( -0.2f ) ) );
	movers[1].model = mul( trans( -1.0f, 1.5f, -5.3f ), mul( rotZ( 0.4f ), rotY( -0.3f ) ) );
	movers[1].modelPrev = mul( trans( -1.12f, 1.6f, -5.2f ), mul( rotZ( 0.45f ), rotY( -0.3f ) ) );
	movers[2].model = mul( trans( 1.0f, 1.5f, -5.0f ), rotX( 0.3f ) );
	movers[2].modelPrev = movers[2].model;
	movers[3].model = mul( trans( 3.0f, 1.5f, -5.2f ), rotY( -0.4f ) );
	movers[3].modelPrev = mul( trans( 3.1f, 1.45f, -5.2f ), rotY( -0.35f ) );
	movers[4].model = mul( trans( -3.0f, -1.5f, -5.0f ), mul( rotX( 0.2f ), rotY( 0.2f ) ) );
	movers[4].modelPrev = mul( trans( -2.9f, -1.6f, -4.95f ), mul( rotX( 0.25f ), rotY( 0.15f ) ) );
	movers[5].model = trans( -0.6f, -1.5f, -4.8f );
	movers[5].modelPrev = trans( -0.5f, -1.45f, -4.8f );
	movers[6].model = mul( trans( 2.2f, -1.5f, -5.0f ), rotX( -1.1f ) );
	movers[6].modelPrev = mul( trans( 2.3f, -1.42f, -5.0f ), rotX( -1.05f ) );
	build( movers[0], false, false, false );
	build( movers[1], false, false, false );
	build( movers[2], true, false, false );
	build( movers[3], false, false, true );
	build( movers[4], false, true, true );
	build( movers[5], false, false, false );
	build( movers[6], false, false, false );
	const int order[8] = { 0, 1, 2, 3, 4, 4, 5, 6 };
	for ( int d = 0; d < 8; d++ ) {
		Mover & mv = movers[std::size_t( order[d] )];
		if ( !mv.vao ) {
			makeVao( mv );
			bbox( mv, mul( Pj, mv.model ) );
		}
		ObjVelDraw x = drawOf( mv, P, Pj );
		if ( order[d] == 2 )
			x.velMode = 1;
		if ( order[d] == 3 ) {
			x.alphaTest = 1;
			x.alphaThreshold = 0.5f;
			x.baseTex = baseTex;
			x.uvScale[0] = 1.5f;
			x.uvScale[1] = 1.25f;
			x.uvOffset[0] = 0.1f;
			x.uvOffset[1] = -0.2f;
		}
		if ( order[d] == 4 ) {
			x.fxMode = 1;
			x.covMode = d == 4 ? 1 : 2;
			x.pass = d == 4 ? 0 : 1;
			x.covThreshold = 0.5f;
			x.baseTex = baseTex;
			x.uvScale[0] = 1.5f;
			x.uvScale[1] = 1.25f;
			x.uvOffset[0] = 0.1f;
			x.uvOffset[1] = -0.2f;
		}
		draws.push_back( x );
		drawMover.push_back( order[d] );
	}
	for ( const Mover & mv : movers )
		std::printf( "OBJVELDRAW mover %s: %zu vertices, window box x %d..%d y %d..%d\n", mv.name.c_str(), mv.pos.size() / 3, mv.x0,
			mv.x1, mv.y0, mv.y1 );

	bool ok = true, nothing = false;
	for ( int fi = 0; fi < 2; fi++ ) {
		const Result r = runFormat( fi, plainBase );
		ok = ok && r.ok;
		nothing = nothing || r.nothing;
	}
	WwVkPresent::shutdown();
	if ( nothing )
		std::printf( "OBJVELDRAW TESTS NOTHING (a mover under 64 texels, a blend side under 64, a bias probe under 20 %%, or vel "
					 "non-zero under 20 %%)\n" );
	std::printf( "OBJVELDRAW GATE %s\n", ok && !nothing ? "OK" : "FAILED" );
	return ok && !nothing ? 0 : 1;
}

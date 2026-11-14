// Lane VKFAR2 (2026-11-14, plan row G2): the sky bodies in the sky's Vulkan pass (vkskybody.inc, built in with the shipped
// vkpresent.cpp under WW_VKPRESENT_NO_QT) against GL's own programs drawn the way lookdevstage.cpp draws the backdrop.
// Usage: skybody <glsl dir> [W H]
// The frame: RGBA16F + D24S8, 4 samples, cleared. GL reference, with GL_MULTISAMPLE off as the backdrop runs
// (lookdevstage's SingleSample): the atmo_sky quad, the stars (lookdev_clouds: a CPU-placed mesh of 600 small quads with
// per-vertex extinction colours, a BC7 map with mips, Tex2 unbound, ldTrans = the transmittance LUT), the moon globe
// (lookdev_bill + sky_moon: moon_d BC1 / moon_n BC5 with gli's swizzle, wrap S repeat / T clamp, three LUTs); maps judged
// at 1x anisotropy, the app's 16x logged (see fieldLevel0), then an opaque object (depth 0.5) over a centre rectangle. Vulkan: skyBodiesArm, skyRun (held), skyStars,
// skyMoon, skyBodiesFlush, the same object. The maps are the texture cache's BC twins (objTexMake), uploaded once by GL.
// Read back per sample (texelFetch) as halves.
// Prints `SKYBODY <job> EXACT|BOUND|FAIL`, `SKYBODY RED <name> ...` (each must FAIL), `SKYBODY GATE OK|FAILED`.
// BOUND (stated before the first run, never widened; skyview's): every half within 4 ulp and <= 0.1 % of halves past 1 ulp.
// The edges: GL rasterizes the backdrop single-sample; the bodies' pipelines put every sample at the pixel's centre
// (VK_EXT_sample_locations), so a star's edge pixel is in or out the same way. Red `msaa` (GL with multisampling on)
// measures what that difference would be.
#define WW_VKPRESENT_NO_QT
#include "../../src/features/vkframe/vkpresent.cpp"

#include <GL/gl.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace sb2
{
using WwVkPresent::kSkyFields;
using WwVkPresent::kSkyGlobalBytes;
using WwVkPresent::kSkyLooseBytes;
using WwVkPresent::SkyField;
HMODULE gOpenGl = nullptr;
void * glProc( const char * name )
{
	void * p = reinterpret_cast<void *>( wglGetProcAddress( name ) );
	const intptr_t i = reinterpret_cast<intptr_t>( p );
	if ( !p || i == 1 || i == 2 || i == 3 || i == -1 )
		p = reinterpret_cast<void *>( GetProcAddress( gOpenGl, name ) );
	return p;
}

LRESULT CALLBACK wndProc( HWND hw, UINT m, WPARAM wp, LPARAM lp )
{
	return DefWindowProcA( hw, m, wp, lp );
}

void pump()
{
	MSG msg;
	while ( PeekMessageA( &msg, nullptr, 0, 0, PM_REMOVE ) ) {
		TranslateMessage( &msg );
		DispatchMessageA( &msg );
	}
}

constexpr GLenum kT2DMS = 0x9100, kFb = 0x8D40, kCol0 = 0x8CE0, kDsAtt = 0x821A, kD24s8 = 0x88F0;
constexpr GLenum kClamp = 0x812F, kTex0 = 0x84C0, kFrag = 0x8B30, kVert = 0x8B31, kCompiled = 0x8B81, kLinked = 0x8B82;
constexpr GLenum kComplete = 0x8CD5, kMaxLevel = 0x813D, kHalf = 0x140B, kMultisample = 0x809D;
constexpr GLenum kUbo = 0x8A11, kBlockSize = 0x8A40, kStatic = 0x88E4, kAniso = 0x84FE, kRepeat = 0x2901;
constexpr GLenum kBc1 = 0x83F0, kBc5 = 0x8DBD, kBc7 = 0x8E8C;

struct Fns
{
	GLuint( WINAPI * createShader )( GLenum ) = nullptr;
	void( WINAPI * shaderSource )( GLuint, GLsizei, const char * const *, const GLint * ) = nullptr;
	void( WINAPI * compileShader )( GLuint ) = nullptr;
	void( WINAPI * getShaderiv )( GLuint, GLenum, GLint * ) = nullptr;
	void( WINAPI * getShaderInfoLog )( GLuint, GLsizei, GLsizei *, char * ) = nullptr;
	GLuint( WINAPI * createProgram )() = nullptr;
	void( WINAPI * attachShader )( GLuint, GLuint ) = nullptr;
	void( WINAPI * linkProgram )( GLuint ) = nullptr;
	void( WINAPI * getProgramiv )( GLuint, GLenum, GLint * ) = nullptr;
	void( WINAPI * getProgramInfoLog )( GLuint, GLsizei, GLsizei *, char * ) = nullptr;
	void( WINAPI * useProgram )( GLuint ) = nullptr;
	GLint( WINAPI * getUniformLocation )( GLuint, const char * ) = nullptr;
	void( WINAPI * uniform1i )( GLint, GLint ) = nullptr;
	void( WINAPI * uniform1f )( GLint, GLfloat ) = nullptr;
	void( WINAPI * uniform2f )( GLint, GLfloat, GLfloat ) = nullptr;
	void( WINAPI * uniform3f )( GLint, GLfloat, GLfloat, GLfloat ) = nullptr;
	void( WINAPI * uniform4f )( GLint, GLfloat, GLfloat, GLfloat, GLfloat ) = nullptr;
	void( WINAPI * getUniformfv )( GLuint, GLint, GLfloat * ) = nullptr;
	void( WINAPI * getUniformiv )( GLuint, GLint, GLint * ) = nullptr;
	GLuint( WINAPI * getUniformBlockIndex )( GLuint, const char * ) = nullptr;
	void( WINAPI * uniformBlockBinding )( GLuint, GLuint, GLuint ) = nullptr;
	void( WINAPI * getActiveUniformBlockiv )( GLuint, GLuint, GLenum, GLint * ) = nullptr;
	void( WINAPI * genBuffers )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindBuffer )( GLenum, GLuint ) = nullptr;
	void( WINAPI * bufferData )( GLenum, std::ptrdiff_t, const void *, GLenum ) = nullptr;
	void( WINAPI * bindBufferBase )( GLenum, GLuint, GLuint ) = nullptr;
	void( WINAPI * activeTexture )( GLenum ) = nullptr;
	void( WINAPI * texImage2DMultisample )( GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLboolean ) = nullptr;
	void( WINAPI * genFramebuffers )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindFramebuffer )( GLenum, GLuint ) = nullptr;
	void( WINAPI * framebufferTexture2D )( GLenum, GLenum, GLenum, GLuint, GLint ) = nullptr;
	GLenum( WINAPI * checkFramebufferStatus )( GLenum ) = nullptr;
	void( WINAPI * enableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * disableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * vertexAttribPointer )( GLuint, GLint, GLenum, GLboolean, GLsizei, const void * ) = nullptr;
	void( WINAPI * blendFuncSeparate )( GLenum, GLenum, GLenum, GLenum ) = nullptr;
	void( WINAPI * compressedTextureSubImage2D )( GLuint, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const void * ) = nullptr;
};
Fns f;

template <class P> bool load( P & p, const char * n )
{
	p = reinterpret_cast<P>( glProc( n ) );
	return p != nullptr;
}

bool loadAll()
{
	return load( f.createShader, "glCreateShader" ) && load( f.shaderSource, "glShaderSource" )
		&& load( f.compileShader, "glCompileShader" ) && load( f.getShaderiv, "glGetShaderiv" )
		&& load( f.getShaderInfoLog, "glGetShaderInfoLog" ) && load( f.createProgram, "glCreateProgram" )
		&& load( f.attachShader, "glAttachShader" ) && load( f.linkProgram, "glLinkProgram" )
		&& load( f.getProgramiv, "glGetProgramiv" ) && load( f.getProgramInfoLog, "glGetProgramInfoLog" )
		&& load( f.useProgram, "glUseProgram" ) && load( f.getUniformLocation, "glGetUniformLocation" )
		&& load( f.uniform1i, "glUniform1i" ) && load( f.uniform1f, "glUniform1f" ) && load( f.uniform2f, "glUniform2f" )
		&& load( f.uniform3f, "glUniform3f" ) && load( f.uniform4f, "glUniform4f" ) && load( f.getUniformfv, "glGetUniformfv" )
		&& load( f.getUniformiv, "glGetUniformiv" ) && load( f.getUniformBlockIndex, "glGetUniformBlockIndex" )
		&& load( f.uniformBlockBinding, "glUniformBlockBinding" )
		&& load( f.getActiveUniformBlockiv, "glGetActiveUniformBlockiv" ) && load( f.genBuffers, "glGenBuffers" )
		&& load( f.bindBuffer, "glBindBuffer" ) && load( f.bufferData, "glBufferData" )
		&& load( f.bindBufferBase, "glBindBufferBase" ) && load( f.activeTexture, "glActiveTexture" )
		&& load( f.texImage2DMultisample, "glTexImage2DMultisample" ) && load( f.genFramebuffers, "glGenFramebuffers" )
		&& load( f.bindFramebuffer, "glBindFramebuffer" ) && load( f.framebufferTexture2D, "glFramebufferTexture2D" )
		&& load( f.checkFramebufferStatus, "glCheckFramebufferStatus" )
		&& load( f.enableVertexAttribArray, "glEnableVertexAttribArray" )
		&& load( f.disableVertexAttribArray, "glDisableVertexAttribArray" )
		&& load( f.vertexAttribPointer, "glVertexAttribPointer" ) && load( f.blendFuncSeparate, "glBlendFuncSeparate" )
		&& load( f.compressedTextureSubImage2D, "glCompressedTextureSubImage2D" );
}

// the shipped helpers' GL functions (fxPack, skyBodyUnitTex), as Qt's names
struct GlFn
{
	GLint glGetUniformLocation( unsigned p, const char * n ) { return f.getUniformLocation( p, n ); }
	void glGetUniformiv( unsigned p, GLint l, int * v ) { f.getUniformiv( p, l, v ); }
	void glGetUniformfv( unsigned p, GLint l, float * v ) { f.getUniformfv( p, l, v ); }
	void glGetIntegerv( GLenum e, int * v ) { ::glGetIntegerv( e, v ); }
	void glActiveTexture( GLenum u ) { f.activeTexture( u ); }
};
GlFn fn;

std::string readFile( const std::string & p )
{
	std::ifstream in( p, std::ios::binary );
	std::stringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

GLuint shader( GLenum type, const std::string & src, const char * tag )
{
	const GLuint s = f.createShader( type );
	const char * p = src.c_str();
	f.shaderSource( s, 1, &p, nullptr );
	f.compileShader( s );
	GLint ok = 0;
	f.getShaderiv( s, kCompiled, &ok );
	if ( !ok ) {
		std::vector<char> log( 65536 );
		f.getShaderInfoLog( s, GLsizei( log.size() ), nullptr, log.data() );
		std::printf( "SKYBODY compile failed (%s): %.1500s\n", tag, log.data() );
		return 0;
	}
	return s;
}

GLuint program( const std::string & vs, const std::string & fs, const char * tag )
{
	if ( vs.empty() || fs.empty() ) {
		std::printf( "SKYBODY missing source (%s)\n", tag );
		return 0;
	}
	const GLuint a = shader( kVert, vs, tag ), b = shader( kFrag, fs, tag );
	if ( !a || !b )
		return 0;
	const GLuint p = f.createProgram();
	f.attachShader( p, a );
	f.attachShader( p, b );
	f.linkProgram( p );
	GLint ok = 0;
	f.getProgramiv( p, kLinked, &ok );
	if ( !ok ) {
		std::vector<char> log( 65536 );
		f.getProgramInfoLog( p, GLsizei( log.size() ), nullptr, log.data() );
		std::printf( "SKYBODY link failed (%s): %.1500s\n", tag, log.data() );
		return 0;
	}
	return p;
}

GLint loc( GLuint p, const char * n )
{
	return f.getUniformLocation( p, n );
}
void u1i( GLuint p, const char * n, int v )
{
	const GLint l = loc( p, n );
	if ( l >= 0 )
		f.uniform1i( l, v );
}
void u1f( GLuint p, const char * n, float v )
{
	const GLint l = loc( p, n );
	if ( l >= 0 )
		f.uniform1f( l, v );
}
void u2f( GLuint p, const char * n, float a, float b )
{
	const GLint l = loc( p, n );
	if ( l >= 0 )
		f.uniform2f( l, a, b );
}
void u3f( GLuint p, const char * n, const float * v )
{
	const GLint l = loc( p, n );
	if ( l >= 0 )
		f.uniform3f( l, v[0], v[1], v[2] );
}
void u4f( GLuint p, const char * n, const float * v )
{
	const GLint l = loc( p, n );
	if ( l >= 0 )
		f.uniform4f( l, v[0], v[1], v[2], v[3] );
}

GLuint tex2d( int w, int hgt, GLenum ifmt, GLenum fmt, GLenum typ, const void * data, bool linear )
{
	GLuint t = 0;
	glGenTextures( 1, &t );
	glBindTexture( GL_TEXTURE_2D, t );
	glTexImage2D( GL_TEXTURE_2D, 0, GLint( ifmt ), w, hgt, 0, fmt, typ, data );
	const GLint flt = linear ? GL_LINEAR : GL_NEAREST;
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, flt );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, flt );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, kClamp );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, kClamp );
	glTexParameteri( GL_TEXTURE_2D, kMaxLevel, 0 );
	return t;
}

GLuint msTex( int w, int hgt, GLenum ifmt, int s )
{
	GLuint t = 0;
	glGenTextures( 1, &t );
	glBindTexture( kT2DMS, t );
	f.texImage2DMultisample( kT2DMS, s, ifmt, w, hgt, GL_TRUE );
	glBindTexture( kT2DMS, 0 );
	return t;
}

GLuint fbo( GLenum colType, GLuint col, GLuint ds )
{
	GLuint b = 0;
	f.genFramebuffers( 1, &b );
	f.bindFramebuffer( kFb, b );
	f.framebufferTexture2D( kFb, kCol0, colType, col, 0 );
	if ( ds )
		f.framebufferTexture2D( kFb, kDsAtt, kT2DMS, ds, 0 );
	if ( f.checkFramebufferStatus( kFb ) != kComplete ) {
		std::printf( "SKYBODY framebuffer incomplete\n" );
		return 0;
	}
	return b;
}

void bindUnit( int unit, GLenum type, GLuint t )
{
	f.activeTexture( kTex0 + GLenum( unit ) );
	glBindTexture( type, t );
}

const char * kTriVs = "#version 410 core\n"
	"void main() { vec2 p = vec2( float( ( gl_VertexID << 1 ) & 2 ), float( gl_VertexID & 2 ) ); "
	"gl_Position = vec4( p * 2.0 - 1.0, 0.0, 1.0 ); }\n";
const char * kObjFs = "#version 410 core\nout vec4 o; void main() { o = vec4( 0.25, 0.5, 0.75, 1.0 ); }\n";
const char * kUnpackFs = "#version 410 core\n"
	"uniform sampler2DMS src; uniform int S; out vec4 o;\n"
	"void main() { ivec2 q = ivec2( gl_FragCoord.xy ); o = texelFetch( src, ivec2( q.x / S, q.y ), q.x % S ); }\n";

struct V3
{
	float x, y, z;
};
V3 dirAt( float azDeg, float elDeg )
{
	const float a = azDeg * 3.14159265f / 180.0f, e = elDeg * 3.14159265f / 180.0f;
	return { std::cos( e ) * std::cos( a ), std::cos( e ) * std::sin( a ), std::sin( e ) };
}
V3 cross( V3 a, V3 b )
{
	return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
V3 norm( V3 a )
{
	const float l = std::sqrt( a.x * a.x + a.y * a.y + a.z * a.z );
	return { a.x / l, a.y / l, a.z / l };
}
V3 add( V3 a, V3 b, float s )
{
	return { a.x + b.x * s, a.y + b.y * s, a.z + b.z * s };
}

struct Job
{
	const char * name;
	float camAz, camEl, fov;
	float sunAz, sunEl, moonAz, moonEl;  // the moon's direction relative to the camera's (az, el offsets)
	float moonDeg;                       // the globe's half-angle
	float sunK, moonK;
	int viewTransform;
	float exposure;
	float texBlend;
	int ldApOn, ldFsOn;
	int vp[4];  // 0 0 0 0 = the whole frame
};

// the GLOBAL block as std140 packs it (uniforms.glsl): viewMatrix mat3 at 0 (3 columns, stride 16), projection at 96
void globalBytes( const Job & j, int vw, int vh, std::uint8_t * out, float & fy )
{
	std::memset( out, 0, size_t( kSkyGlobalBytes ) );
	float * fl = reinterpret_cast<float *>( out );
	const V3 fw = dirAt( j.camAz, j.camEl ), r = norm( cross( fw, { 0, 0, 1 } ) ), up = cross( r, fw );
	const V3 bk = { -fw.x, -fw.y, -fw.z };
	const float rows[3][3] = { { r.x, r.y, r.z }, { up.x, up.y, up.z }, { bk.x, bk.y, bk.z } };
	for ( int c = 0; c < 3; c++ )
		for ( int rr = 0; rr < 3; rr++ )
			fl[c * 4 + rr] = rows[rr][c];
	for ( int c = 0; c < 3; c++ )
		fl[12 + c * 4 + c] = 1.0f;
	fy = 1.0f / std::tan( j.fov * 3.14159265f / 360.0f );
	const float n = 0.1f, fa = 10000.0f;
	float * P = fl + 24;
	P[0] = fy * float( vh ) / float( vw );
	P[5] = fy;
	P[10] = ( fa + n ) / ( n - fa );
	P[11] = -1.0f;
	P[14] = 2.0f * fa * n / ( n - fa );
	for ( int i = 40; i < 84; i++ )
		fl[i] = 0.01f * float( i );
}

struct Halves
{
	size_t total = 0, diff = 0, past1 = 0, past4 = 0;
	int maxu = 0;
	const char * word() const { return total == 0 ? "FAIL" : diff == 0 ? "EXACT" : ok() ? "BOUND" : "FAIL"; }
	bool ok() const { return total > 0 && maxu <= 4 && past1 * 1000 <= total; }
};

int ordered( std::uint16_t hv )
{
	return ( hv & 0x8000 ) ? -int( hv & 0x7FFF ) : int( hv );
}

Halves compare( const std::vector<std::uint16_t> & a, const std::vector<std::uint16_t> & b )
{
	Halves v;
	if ( a.size() != b.size() || a.empty() )
		return v;
	v.total = a.size();
	for ( size_t i = 0; i < a.size(); i++ ) {
		const bool nanA = ( a[i] & 0x7C00 ) == 0x7C00 && ( a[i] & 0x3FF ), nanB = ( b[i] & 0x7C00 ) == 0x7C00 && ( b[i] & 0x3FF );
		int d = std::abs( ordered( a[i] ) - ordered( b[i] ) );
		if ( nanA || nanB )
			d = nanA == nanB ? 0 : 1 << 16;
		v.diff += d != 0;
		v.past1 += d > 1;
		v.past4 += d > 4;
		v.maxu = std::max( v.maxu, d );
	}
	return v;
}

/* the maps' content: one smooth repeating field at level 0 (fu x fv periods, seeded phases), each mip the 2x2 box of the
 * one above as a DDS's mips are, encoded to BC1 / BC5 / BC7 (mode 6) blocks. Random blocks per level parted GL and Vulkan
 * by up to 2245 ulp over the stars and the moon with no pixel's samples apart; the smooth chain still parted them by up to
 * 364 ulp at 16x anisotropy, and matched at 1x (and with no mips): NV's anisotropic footprint differs GL vs Vulkan
 * (farloddraw's and waterdraw's finding). So the maps are judged at 1x (the twin still takes GL's anisotropy: red aniso)
 * and the app's 16x is run and logged, not judged */
constexpr float kJudgedAniso = 1.0f;
std::vector<float> fieldLevel0( int w, int h, float fu, float fv, const float ph[4] )
{
	std::vector<float> d( size_t( w ) * size_t( h ) * 4 );
	for ( int j = 0; j < h; j++ )
		for ( int i = 0; i < w; i++ )
			for ( int c = 0; c < 4; c++ ) {
				const float u = ( float( i ) + 0.5f ) / float( w ), v = ( float( j ) + 0.5f ) / float( h ), k = 6.2831853f;
				d[( size_t( j ) * size_t( w ) + size_t( i ) ) * 4 + size_t( c )] =
					0.5f + 0.35f * std::sin( k * ( fu * u + ph[c] ) ) * std::cos( k * ( fv * v + ph[( c + 1 ) & 3] ) );
			}
	return d;
}

std::vector<float> boxDown( const std::vector<float> & s, int w, int h )
{
	const int lw = std::max( w >> 1, 1 ), lh = std::max( h >> 1, 1 );
	std::vector<float> d( size_t( lw ) * size_t( lh ) * 4 );
	for ( int j = 0; j < lh; j++ )
		for ( int i = 0; i < lw; i++ )
			for ( int c = 0; c < 4; c++ ) {
				const int x0 = std::min( 2 * i, w - 1 ), x1 = std::min( 2 * i + 1, w - 1 );
				const int y0 = std::min( 2 * j, h - 1 ), y1 = std::min( 2 * j + 1, h - 1 );
				auto at = [&]( int x, int y ) { return s[( size_t( y ) * size_t( w ) + size_t( x ) ) * 4 + size_t( c )]; };
				d[( size_t( j ) * size_t( lw ) + size_t( i ) ) * 4 + size_t( c )] =
					0.25f * ( at( x0, y0 ) + at( x1, y0 ) + at( x0, y1 ) + at( x1, y1 ) );
			}
	return d;
}

int q8( float x )
{
	return int( std::lround( std::clamp( x, 0.0f, 1.0f ) * 255.0f ) );
}

// BC1: the block's colour box as the endpoints (c0 >= c1: four-colour mode), each texel the nearest of the four
void encBc1( const float t[16][4], unsigned char * o )
{
	float mn[3] = { 1, 1, 1 }, mx[3] = { 0, 0, 0 };
	for ( int i = 0; i < 16; i++ )
		for ( int c = 0; c < 3; c++ )
			mn[c] = std::min( mn[c], t[i][c] ), mx[c] = std::max( mx[c], t[i][c] );
	auto pack = []( const float * c ) {
		return std::uint16_t( std::lround( std::clamp( c[0], 0.0f, 1.0f ) * 31.0f ) << 11
			| std::lround( std::clamp( c[1], 0.0f, 1.0f ) * 63.0f ) << 5 | std::lround( std::clamp( c[2], 0.0f, 1.0f ) * 31.0f ) );
	};
	const std::uint16_t c0 = pack( mx ), c1 = pack( mn );
	auto unpack = []( std::uint16_t p, float * c ) {
		c[0] = float( p >> 11 ) / 31.0f, c[1] = float( ( p >> 5 ) & 63 ) / 63.0f, c[2] = float( p & 31 ) / 31.0f;
	};
	float pal[4][3];
	unpack( c0, pal[0] ), unpack( c1, pal[1] );
	for ( int c = 0; c < 3; c++ )
		pal[2][c] = ( 2.0f * pal[0][c] + pal[1][c] ) / 3.0f, pal[3][c] = ( pal[0][c] + 2.0f * pal[1][c] ) / 3.0f;
	std::uint32_t idx = 0;
	for ( int i = 0; c0 != c1 && i < 16; i++ ) {
		int best = 0;
		float be = 1e30f;
		for ( int k = 0; k < 4; k++ ) {
			float e = 0;
			for ( int c = 0; c < 3; c++ )
				e += ( t[i][c] - pal[k][c] ) * ( t[i][c] - pal[k][c] );
			if ( e < be )
				be = e, best = k;
		}
		idx |= std::uint32_t( best ) << ( 2 * i );
	}
	o[0] = c0 & 0xFF, o[1] = c0 >> 8, o[2] = c1 & 0xFF, o[3] = c1 >> 8;
	for ( int b = 0; b < 4; b++ )
		o[4 + b] = ( idx >> ( 8 * b ) ) & 0xFF;
}

// BC4 (one channel of BC5): r0 = max > r1 = min (the eight-value mode), each texel the nearest
void encBc4( const float t[16][4], int ch, unsigned char * o )
{
	int r0 = 0, r1 = 255;
	for ( int i = 0; i < 16; i++ )
		r0 = std::max( r0, q8( t[i][ch] ) ), r1 = std::min( r1, q8( t[i][ch] ) );
	std::uint64_t idx = 0;
	for ( int i = 0; r0 != r1 && i < 16; i++ ) {
		int best = 0;
		float be = 1e30f;
		for ( int k = 0; k < 8; k++ ) {
			const float p = k == 0 ? float( r0 ) : k == 1 ? float( r1 ) : float( ( 8 - k ) * r0 + ( k - 1 ) * r1 ) / 7.0f;
			const float e = std::fabs( t[i][ch] * 255.0f - p );
			if ( e < be )
				be = e, best = k;
		}
		idx |= std::uint64_t( best ) << ( 3 * i );
	}
	o[0] = static_cast<unsigned char>( r0 ), o[1] = static_cast<unsigned char>( r1 );
	for ( int b = 0; b < 6; b++ )
		o[2 + b] = ( idx >> ( 8 * b ) ) & 0xFF;
}

// BC7 mode 6: one subset, RGBA 7.7.7.7 + a p-bit per endpoint, 4-bit indices (texel 0's top bit 0: swap when needed)
void encBc7( const float t[16][4], unsigned char * o )
{
	static const int wt[16] = { 0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64 };
	int e[2][4];
	for ( int c = 0; c < 4; c++ ) {
		int mn = 255, mx = 0;
		for ( int i = 0; i < 16; i++ )
			mn = std::min( mn, q8( t[i][c] ) ), mx = std::max( mx, q8( t[i][c] ) );
		e[0][c] = mn >> 1, e[1][c] = mx >> 1;  // p0 = 0 (rounds the low end down), p1 = 1 (the high end up)
	}
	int p[2] = { 0, 1 }, ix[16];
	for ( int i = 0; i < 16; i++ ) {
		int best = 0;
		float be = 1e30f;
		for ( int k = 0; k < 16; k++ ) {
			float err = 0;
			for ( int c = 0; c < 4; c++ ) {
				const int a = e[0][c] << 1 | p[0], b = e[1][c] << 1 | p[1];
				const float v = float( ( ( 64 - wt[k] ) * a + wt[k] * b + 32 ) >> 6 ) - t[i][c] * 255.0f;
				err += v * v;
			}
			if ( err < be )
				be = err, best = k;
		}
		ix[i] = best;
	}
	if ( ix[0] >= 8 ) {
		for ( int c = 0; c < 4; c++ )
			std::swap( e[0][c], e[1][c] );
		std::swap( p[0], p[1] );
		for ( int & k : ix )
			k = 15 - k;
	}
	std::memset( o, 0, 16 );
	int bit = 0;
	auto put = [&]( unsigned v, int n ) {
		for ( int b = 0; b < n; b++, bit++ )
			if ( v >> b & 1 )
				o[bit >> 3] |= static_cast<unsigned char>( 1u << ( bit & 7 ) );
	};
	put( 1u << 6, 7 );
	for ( int c = 0; c < 4; c++ )
		put( unsigned( e[0][c] ), 7 ), put( unsigned( e[1][c] ), 7 );
	put( unsigned( p[0] ), 1 ), put( unsigned( p[1] ), 1 );
	for ( int i = 0; i < 16; i++ )
		put( unsigned( ix[i] ), i == 0 ? 3 : 4 );
}

// one level's blocks from a float RGBA level (texels past the edge clamp, as a 2x2 / 1x1 level's block)
std::vector<unsigned char> encodeLevel( const std::vector<float> & d, int w, int h, int fmtKind, int block )
{
	const int bw = ( w + 3 ) / 4, bh = ( h + 3 ) / 4;
	std::vector<unsigned char> out( size_t( bw ) * size_t( bh ) * size_t( block ) );
	for ( int by = 0; by < bh; by++ )
		for ( int bx = 0; bx < bw; bx++ ) {
			float t[16][4];
			for ( int i = 0; i < 16; i++ ) {
				const int x = std::min( bx * 4 + ( i & 3 ), w - 1 ), y = std::min( by * 4 + ( i >> 2 ), h - 1 );
				for ( int c = 0; c < 4; c++ )
					t[i][c] = d[( size_t( y ) * size_t( w ) + size_t( x ) ) * 4 + size_t( c )];
			}
			unsigned char * o = &out[( size_t( by ) * size_t( bw ) + size_t( bx ) ) * size_t( block )];
			if ( fmtKind == 1 )
				encBc1( t, o );
			else if ( fmtKind == 5 )
				encBc4( t, 0, o ), encBc4( t, 1, o + 8 );
			else
				encBc7( t, o );
		}
	return out;
}

// the stars: a CPU-placed mesh in the camera's view (lookdevstage turns Meshes\Sky\Stars.nif on the CPU the same way)
struct Stars
{
	std::vector<float> pos, col, white, uv;
	std::vector<std::uint16_t> idx;
};
Stars makeStars( const Job & j, float pxAng, float aspect, std::mt19937 & rng )
{
	std::uniform_real_distribution<float> u( 0.0f, 1.0f );
	Stars s;
	const V3 fw = dirAt( j.camAz, j.camEl ), r = norm( cross( fw, { 0, 0, 1 } ) ), up = cross( r, fw );
	const float ty = std::tan( j.fov * 3.14159265f / 360.0f ), tx = ty * aspect;
	const V3 md = dirAt( j.camAz + j.moonAz, j.camEl + j.moonEl );
	for ( int k = 0; k < 600; k++ ) {
		V3 d;
		if ( k < 40 )  // some behind and round the moon's disc (the globe covers them)
			d = norm( add( add( md, r, ( u( rng ) - 0.5f ) * 0.4f ), up, ( u( rng ) - 0.5f ) * 0.4f ) );
		else
			d = norm( add( add( fw, r, ( u( rng ) * 2.0f - 1.0f ) * tx * 1.05f ), up, ( u( rng ) * 2.0f - 1.0f ) * ty * 1.05f ) );
		const V3 e1 = norm( cross( d, { 0, 0, 1 } ) ), e2 = cross( e1, d );
		const float a = pxAng * ( k % 10 == 0 ? 6.0f + 6.0f * u( rng ) : 0.4f + 3.6f * u( rng ) ), sx = 1.0f + 2.0f * u( rng );
		const float u0 = u( rng ), v0 = u( rng ), span = 0.05f + 1.45f * u( rng );
		const float m = 1.0f / std::max( d.z + 0.3f, 0.05f );  // an airmass-like path for the extinction colours
		const float alphaV = 0.5f + 0.5f * u( rng );
		const std::uint16_t base = std::uint16_t( s.pos.size() / 3 );
		for ( int c = 0; c < 4; c++ ) {
			const float cx = ( c & 1 ) ? 1.0f : -1.0f, cy = ( c & 2 ) ? 1.0f : -1.0f;
			const V3 p = add( add( d, e1, cx * a * sx ), e2, cy * a );
			s.pos.insert( s.pos.end(), { p.x, p.y, p.z } );
			s.col.insert( s.col.end(), { std::exp( -0.05f * m ), std::exp( -0.12f * m ), std::exp( -0.3f * m ), alphaV * ( c == 3 ? 0.7f : 1.0f ) } );
			s.white.insert( s.white.end(), { 1.0f, 1.0f, 1.0f, 1.0f } );
			s.uv.insert( s.uv.end(), { u0 + ( ( c & 1 ) ? span : 0.0f ), v0 + ( ( c & 2 ) ? span : 0.0f ) } );
		}
		s.idx.insert( s.idx.end(), { base, std::uint16_t( base + 1 ), std::uint16_t( base + 2 ), std::uint16_t( base + 2 ),
			std::uint16_t( base + 1 ), std::uint16_t( base + 3 ) } );
	}
	return s;
}
}  // namespace sb2

int main( int argc, char ** argv )
{
	using namespace sb2;
	if ( argc < 2 ) {
		std::printf( "usage: skybody <glsl dir> [W H]\nSKYBODY GATE FAILED\n" );
		return 2;
	}
	const std::string dir = argv[1];
	const int W = argc > 2 ? std::atoi( argv[2] ) : 640, H = argc > 3 ? std::atoi( argv[3] ) : 360;
	const int S = 4;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwskybody";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwskybody", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, W, H };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwskybody", "sky body harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
		rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	const int attr[] = { 0x2001, 1, 0x2010, 1, 0x2011, 1, 0x2003, 0x2027, 0x2013, 0x202B, 0x2015, 8, 0x2017, 8, 0x2019, 8,
		0x201B, 8, 0x2022, 24, 0x2023, 8, 0x2041, 1, 0x2042, 4, 0 };
	int fmt = 0;
	UINT nf = 0;
	if ( !choose || !choose( dc, attr, nullptr, 1, &fmt, &nf ) || !nf ) {
		std::printf( "SKYBODY GATE FAILED (no 4x MSAA pixel format)\n" );
		return 2;
	}
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
	if ( !loadAll() ) {
		std::printf( "SKYBODY GATE FAILED (GL entry points)\n" );
		return 2;
	}
	std::printf( "frame %dx%d x%d, GL on %s\n", W, H, S, reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );

	const GLuint pObj = program( kTriVs, kObjFs, "object" ), pUnpack = program( kTriVs, kUnpackFs, "unpack" );
	const GLuint pSky = program( readFile( dir + "/cell_ao.vert.glsl" ), readFile( dir + "/atmo_sky.frag.glsl" ), "atmo_sky" );
	const GLuint pStars = program( readFile( dir + "/lookdev_clouds.vert.glsl" ), readFile( dir + "/lookdev_clouds.frag.glsl" ), "lookdev_clouds" );
	const GLuint pMoon = program( readFile( dir + "/lookdev_bill.vert.glsl" ), readFile( dir + "/sky_moon.frag.glsl" ), "sky_moon" );
	if ( !pObj || !pUnpack || !pSky || !pStars || !pMoon ) {
		std::printf( "SKYBODY GATE FAILED (GL programs)\n" );
		return 2;
	}
	for ( const GLuint p : { pSky, pStars, pMoon } ) {  // the global block: binding 0, the 336 bytes the runs copy
		const GLuint bi = f.getUniformBlockIndex( p, "globalUniforms" );
		GLint size = 0;
		if ( bi != 0xFFFFFFFFu )
			f.getActiveUniformBlockiv( p, bi, kBlockSize, &size );
		if ( size != kSkyGlobalBytes ) {
			std::printf( "SKYBODY GATE FAILED (program %u's global block is %d bytes, the runs copy %d)\n", p, size, kSkyGlobalBytes );
			return 2;
		}
		f.uniformBlockBinding( p, bi, 0 );
	}
	GLuint ubo = 0;
	f.genBuffers( 1, &ubo );

	// the LUTs at the app's sizes, seeded smooth fields with a little texel noise (skyview's)
	std::mt19937 rng( 43 );
	std::uniform_real_distribution<float> ur( 0.0f, 1.0f );
	const int lw[4] = { 256, 32, 192, 192 }, lh[4] = { 64, 32, 108, 108 };
	GLuint lut[4] = {};
	for ( int k = 0; k < 4; k++ ) {
		std::vector<float> d( size_t( lw[k] ) * lh[k] * 4 );
		const float ph = 0.7f * float( k + 1 );
		for ( int y = 0; y < lh[k]; y++ )
			for ( int x = 0; x < lw[k]; x++ )
				for ( int c = 0; c < 4; c++ ) {
					const float u = float( x ) / float( lw[k] ), v = float( y ) / float( lh[k] );
					float val;
					if ( k == 0 )
						val = std::exp( -( 0.2f + 0.5f * float( c ) ) * ( 1.0f - u ) * ( 0.3f + v ) * 3.0f );
					else
						val = ( 0.3f + 0.25f * float( c ) ) * ( 1.2f + std::sin( u * 9.0f + ph + float( c ) ) * std::cos( v * 7.0f - ph ) )
							* ( k == 3 ? 0.02f : 1.0f );
					d[( size_t( y ) * lw[k] + x ) * 4 + size_t( c )] = val * ( 1.0f + 0.02f * ( ur( rng ) - 0.5f ) );
				}
		lut[k] = tex2d( lw[k], lh[k], kRgba16f, GL_RGBA, GL_FLOAT, d.data(), true );
	}
	static int keyHome[4];

	std::string why;
	unsigned fc = 0, fds = 0;
	if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) ) {
		std::printf( "SKYBODY GATE FAILED (the view: %s)\n", why.c_str() );
		return 2;
	}
	if ( !WwVkPresent::toneFrame( W, H, S, fc, fds, why ) ) {
		std::printf( "SKYBODY GATE FAILED (toneFrame: %s)\n", why.c_str() );
		return 2;
	}
	std::printf( "device: sample locations at %s samples\n", ( vk().centreSamples & VkSampleCountFlags( S ) ) ? "these" : "NOT these" );

	/* the cache's BC twins (objTexMake: the Vulkan image GL imports and uploads into), random blocks per level as a DDS's
	 * mips: the stars' map BC7 128^2, moon_d BC1 256 x 128, moon_n BC5 256 x 128 with gli's R,G,0,1 swizzle */
	struct Map
	{
		GLenum fmt;
		int w, h, levels, block;
		GLenum wrapS, wrapT;
		unsigned tex = 0;
	};
	Map maps[3] = { { kBc7, 128, 128, 8, 16, kRepeat, kRepeat }, { kBc1, 256, 128, 9, 8, kRepeat, kClamp },
		{ kBc5, 256, 128, 9, 16, kRepeat, kClamp } };
	for ( Map & m : maps ) {
		if ( !WwVkPresent::objTexMake( m.fmt, m.w, m.h, m.levels, m.tex, why ) ) {
			std::printf( "SKYBODY GATE FAILED (objTexMake 0x%X: %s)\n", m.fmt, why.c_str() );
			return 2;
		}
		float ph[4];
		for ( float & x : ph )
			x = float( rng() & 0xFFFF ) / 65536.0f;
		const float fu = m.fmt == kBc7 ? 1.0f : 2.0f;  // the moon's maps: two periods round the globe, one pole to pole
		std::vector<float> lev = fieldLevel0( m.w, m.h, fu, 1.0f, ph );
		for ( int l = 0; l < m.levels; l++ ) {
			const int w = std::max( m.w >> l, 1 ), h = std::max( m.h >> l, 1 );
			if ( l > 0 )
				lev = boxDown( lev, std::max( m.w >> ( l - 1 ), 1 ), std::max( m.h >> ( l - 1 ), 1 ) );
			const std::vector<unsigned char> blocks = encodeLevel( lev, w, h, m.fmt == kBc1 ? 1 : m.fmt == kBc5 ? 5 : 7, m.block );
			f.compressedTextureSubImage2D( m.tex, l, 0, 0, w, h, m.fmt, GLsizei( blocks.size() ), blocks.data() );
		}
		glBindTexture( GL_TEXTURE_2D, m.tex );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GLint( m.wrapS ) );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GLint( m.wrapT ) );
		glTexParameterf( GL_TEXTURE_2D, kAniso, kJudgedAniso );
		if ( m.fmt == kBc5 ) {
			glTexParameteri( GL_TEXTURE_2D, 0x8E42, GL_RED );
			glTexParameteri( GL_TEXTURE_2D, 0x8E43, GL_GREEN );
			glTexParameteri( GL_TEXTURE_2D, 0x8E44, GL_ZERO );
			glTexParameteri( GL_TEXTURE_2D, 0x8E45, GL_ONE );
		}
		glBindTexture( GL_TEXTURE_2D, 0 );
	}
	if ( glGetError() != GL_NO_ERROR ) {
		std::printf( "SKYBODY GATE FAILED (the twins' upload)\n" );
		return 2;
	}

	// GL's reference target and the per-sample read-back target
	const GLuint refCol = msTex( W, H, kRgba16f, S ), refDs = msTex( W, H, kD24s8, S );
	const GLuint fbRef = fbo( kT2DMS, refCol, refDs );
	const int WS = W * S;
	const GLuint unp = tex2d( WS, H, kRgba16f, GL_RGBA, GL_FLOAT, nullptr, false );
	const GLuint fbUnp = fbo( GL_TEXTURE_2D, unp, 0 );
	const GLuint fbFrame = fbo( kT2DMS, fc, fds );
	if ( !fbRef || !fbUnp || !fbFrame ) {
		std::printf( "SKYBODY GATE FAILED (GL targets)\n" );
		return 2;
	}
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	auto readSamples = [&]( GLuint msColor ) {
		std::vector<std::uint16_t> out( size_t( WS ) * H * 4 );
		f.bindFramebuffer( kFb, fbUnp );
		glViewport( 0, 0, WS, H );
		glDisable( GL_DEPTH_TEST );
		glDisable( GL_STENCIL_TEST );
		glDisable( GL_BLEND );
		glDisable( GL_SCISSOR_TEST );
		f.useProgram( pUnpack );
		bindUnit( 8, kT2DMS, msColor );
		u1i( pUnpack, "src", 8 );
		u1i( pUnpack, "S", S );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		glReadPixels( 0, 0, WS, H, GL_RGBA, kHalf, out.data() );
		return out;
	};
	auto clearFrame = [&]( GLuint fb ) {
		f.bindFramebuffer( kFb, fb );
		glViewport( 0, 0, W, H );
		glDisable( GL_SCISSOR_TEST );
		glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		glDepthMask( GL_TRUE );
		glStencilMask( 0xFF );
		glClearColor( 0.05f, 0.1f, 0.15f, 0.0f );
		glClearDepth( 1.0 );
		glClearStencil( 0 );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
	};
	auto object = [&]( GLuint fb ) {
		f.bindFramebuffer( kFb, fb );
		glViewport( 0, 0, W, H );
		glDisable( GL_BLEND );
		glDepthMask( GL_TRUE );
		glEnable( GL_SCISSOR_TEST );
		glScissor( W / 3, H / 3, W / 3, H / 3 );
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LESS );
		f.useProgram( pObj );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		glDisable( GL_DEPTH_TEST );
		glDisable( GL_SCISSOR_TEST );
	};
	auto vpOf = [&]( const Job & j, int vp[4] ) {
		const bool whole = j.vp[2] == 0;
		vp[0] = whole ? 0 : j.vp[0];
		vp[1] = whole ? 0 : j.vp[1];
		vp[2] = whole ? W : j.vp[2];
		vp[3] = whole ? H : j.vp[3];
	};
	float tint[17][3];
	for ( int i = 0; i < 17; i++ )
		for ( int c = 0; c < 3; c++ )
			tint[i][c] = 0.02f + 0.01f * float( i ) * ( 1.0f + 0.3f * float( c ) );
	// the air's uniforms every program here takes (atmosphere.cpp's values; a program without one skips it)
	auto setAir = [&]( GLuint p, const Job & j, const int vp[4] ) {
		const float haze[3] = { 0.11f, 1.2f, 0.5f }, ray[3] = { 5.802e-3f, 13.558e-3f, 33.1e-3f }, oz[3] = { 0.65e-3f, 1.881e-3f, 0.085e-3f };
		const float alb[3] = { 0.97f, 0.96f, 0.95f }, spec[3] = { 0.8f, 1.0f, 1.27f };
		u3f( p, "atHaze", haze );
		u3f( p, "atRay", ray );
		u1f( p, "atMieS", 3.996e-3f );
		u1f( p, "atMieE", 4.40e-3f );
		u3f( p, "atOzone", oz );
		u1f( p, "atG", 0.7f );
		u3f( p, "atHazeAlbedo", alb );
		u3f( p, "atHazeSpec", spec );
		u1f( p, "atMsOld", 0.0f );
		u2f( p, "atViewSize", float( lw[2] ), float( lh[2] ) );
		const float rect[4] = { float( vp[0] ), float( vp[1] ), 1.0f / float( vp[2] ), 1.0f / float( vp[3] ) };
		u4f( p, "atRect", rect );
		u1f( p, "atCamR", 6360.02f );
		const V3 sw = dirAt( j.sunAz, j.sunEl ), mw = dirAt( j.camAz + j.moonAz, j.camEl + j.moonEl );
		const float swf[3] = { sw.x, sw.y, sw.z }, mwf[3] = { mw.x, mw.y, mw.z };
		u3f( p, "atSunW", swf );
		u3f( p, "atMoonW", mwf );
		u1f( p, "atSunK", j.sunK );
		u1f( p, "atMoonK", j.moonK );
		u1f( p, "sceneExposure", j.exposure );
		u1i( p, "viewTransform", j.viewTransform );
		u1f( p, "atTintOn", 1.0f );
		for ( int i = 0; i < 17; i++ ) {
			const std::string n = "atTint[" + std::to_string( i ) + "]";
			u3f( p, n.c_str(), tint[i] );
		}
		u2f( p, "atRecNight", 0.3f, 0.5f );
	};
	auto setSky = [&]( const Job & j, const int vp[4] ) {
		f.useProgram( pSky );
		u1i( pSky, "atTransTex", 0 );
		u1i( pSky, "atMsTex", 1 );
		u1i( pSky, "atSunView", 2 );
		u1i( pSky, "atMoonView", 3 );
		setAir( pSky, j, vp );
		u1f( pSky, "atLimb", 1.0f );
		u1f( pSky, "atDiscCap", 200.0f );
		u1f( pSky, "atSunSize", 1.0f );
		u1f( pSky, "atDiscHue", 0.0f );
		u1f( pSky, "atDiscAir", 0.8f );
		u2f( pSky, "atMsSize", 32.0f, 32.0f );
		u1f( pSky, "atMsOn", 1.0f );
		u1f( pSky, "atGround", 1.0f );
		u1f( pSky, "atPerPixel", 0.0f );
		const float floorC[3] = { 0.02f, 0.03f, 0.05f }, z[3] = { 0, 0, 0 };
		u3f( pSky, "atFloor", floorC );
		u3f( pSky, "atInscatter", z );
		u3f( pSky, "atDirIns", z );
		u1f( pSky, "atDirExp", 1.0f );
	};
	auto packSky = [&]( std::uint8_t * out ) {
		std::memset( out, 0, size_t( kSkyLooseBytes ) );
		for ( const SkyField & fd : kSkyFields )
			for ( int i = 0; i < fd.count; i++ ) {
				const std::string n = fd.count > 1 ? std::string( fd.name ) + "[" + std::to_string( i ) + "]" : std::string( fd.name );
				const GLint l = loc( pSky, n.c_str() );
				if ( l < 0 )
					continue;
				std::uint8_t * at = out + fd.offset + i * 16;
				if ( fd.isInt ) {
					GLint iv[4] = {};
					f.getUniformiv( pSky, l, iv );
					std::memcpy( at, iv, size_t( fd.words ) * 4 );
				} else {
					GLfloat fv[4] = {};
					f.getUniformfv( pSky, l, fv );
					std::memcpy( at, fv, size_t( fd.words ) * 4 );
				}
			}
	};
	// the stars' program as drawStars leaves it before drawShape (the clouds' uniforms it inherits set as a layer would)
	auto setStars = [&]( const Job & j, const int vp[4] ) {
		f.useProgram( pStars );
		u1i( pStars, "Tex", 0 );
		u1i( pStars, "Tex2", 1 );
		u1i( pStars, "ldTrans", 2 );
		setAir( pStars, j, vp );
		u2f( pStars, "uvOffset", 0.1f, 0.2f );
		u2f( pStars, "uvOffset2", -0.3f, 0.05f );
		u1i( pStars, "probe", 0 );
		u1f( pStars, "texBlend", j.texBlend );
		const float col[3] = { 1.2f, 1.1f, 0.9f };
		u3f( pStars, "colLin", col );
		u1f( pStars, "alpha", 0.9f );
		u1f( pStars, "skyScale", 1.5f );
		u1i( pStars, "ldLtOn", 0 );
		u1i( pStars, "ldApOn", j.ldApOn );
		u2f( pStars, "ldApR", 6360.02f, 6362.0f );
		u1i( pStars, "ldFsOn", j.ldFsOn );
		const V3 sw = dirAt( j.sunAz, j.sunEl );
		const float fsSun[4] = { sw.x, sw.y, sw.z, 0.6f }, fsRb[4] = { 0.5f, 0.3f, 2.0f, 0.0f }, fsTint[3] = { 1.0f, 0.9f, 0.8f };
		u4f( pStars, "ldFsSun", fsSun );
		u4f( pStars, "ldFsRb", fsRb );
		u3f( pStars, "ldFsTint", fsTint );
		bindUnit( 0, GL_TEXTURE_2D, maps[0].tex );
		bindUnit( 1, GL_TEXTURE_2D, 0 );
		bindUnit( 2, GL_TEXTURE_2D, lut[0] );
	};
	// drawMoonGlobe's program before its quad
	auto setMoon = [&]( const Job & j, const int vp[4], float fy ) {
		f.useProgram( pMoon );
		u1i( pMoon, "atAlbedo", 0 );
		u1i( pMoon, "atRelief", 1 );
		u1i( pMoon, "atTransTex", 2 );
		u1i( pMoon, "atSunView", 3 );
		u1i( pMoon, "atMoonView", 4 );
		setAir( pMoon, j, vp );
		u1f( pMoon, "atDomeAir", 0.3f );
		const V3 d = dirAt( j.camAz + j.moonAz, j.camEl + j.moonEl ), sn = dirAt( j.sunAz, j.sunEl );
		const float df[3] = { d.x, d.y, d.z }, sf[3] = { sn.x, sn.y, sn.z };
		const float ex[3] = { 1, 0, 0 }, ey[3] = { 0, 1, 0 }, ez[3] = { 0, 0, 1 };
		u3f( pMoon, "moonD", df );
		u3f( pMoon, "moonS", sf );
		u3f( pMoon, "bodyX", ex );
		u3f( pMoon, "bodyY", ey );
		u3f( pMoon, "bodyZ", ez );
		const float r = j.moonDeg * 3.14159265f / 180.0f, pxAng = 2.0f / ( fy * float( vp[3] ) );
		u1f( pMoon, "sinR", std::sin( r ) );
		u1f( pMoon, "pxAng", pxAng );
		u1f( pMoon, "albedoInv", 1.0f / 0.12f );
		u1f( pMoon, "sunTerm", 2.0f );
		u1f( pMoon, "earthTerm", 0.05f );
		u1f( pMoon, "alpha", 1.0f );
		u3f( pMoon, "centerWorld", df );
		u1f( pMoon, "halfSize", std::tan( r ) * 1.5f + 2.0f * pxAng );
		bindUnit( 0, GL_TEXTURE_2D, maps[1].tex );
		bindUnit( 1, GL_TEXTURE_2D, maps[2].tex );
		bindUnit( 2, GL_TEXTURE_2D, lut[0] );
		bindUnit( 3, GL_TEXTURE_2D, lut[2] );
		bindUnit( 4, GL_TEXTURE_2D, lut[3] );
	};
	auto bodyState = [&]( const int vp[4], bool moon ) {
		glViewport( vp[0], vp[1], vp[2], vp[3] );
		glDisable( GL_DEPTH_TEST );
		glDepthMask( GL_FALSE );
		glDisable( GL_CULL_FACE );
		glDisable( GL_SCISSOR_TEST );
		glDisable( GL_STENCIL_TEST );
		glEnable( GL_BLEND );
		f.blendFuncSeparate( moon ? GL_ONE : GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE );
	};
	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t quadIdx[6] = { 0, 1, 2, 2, 1, 3 };
	auto drawStarsGl = [&]( const Stars & s, const int vp[4] ) {
		bodyState( vp, false );
		f.enableVertexAttribArray( 0 );
		f.enableVertexAttribArray( 1 );
		f.enableVertexAttribArray( 7 );
		f.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, s.pos.data() );
		f.vertexAttribPointer( 1, 4, GL_FLOAT, GL_FALSE, 0, s.col.data() );
		f.vertexAttribPointer( 7, 2, GL_FLOAT, GL_FALSE, 0, s.uv.data() );
		glDrawElements( GL_TRIANGLES, GLsizei( s.idx.size() ), GL_UNSIGNED_SHORT, s.idx.data() );
		f.disableVertexAttribArray( 0 );
		f.disableVertexAttribArray( 1 );
		f.disableVertexAttribArray( 7 );
	};
	auto drawMoonGl = [&]( const int vp[4] ) {
		bodyState( vp, true );
		f.enableVertexAttribArray( 0 );
		f.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, quad );
		glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, quadIdx );
		f.disableVertexAttribArray( 0 );
	};
	auto skyQuadGl = [&]( const int vp[4] ) {
		for ( int k = 0; k < 4; k++ )
			bindUnit( k, GL_TEXTURE_2D, lut[k] );
		glViewport( vp[0], vp[1], vp[2], vp[3] );
		glDisable( GL_DEPTH_TEST );
		glDisable( GL_BLEND );
		glDisable( GL_CULL_FACE );
		glDisable( GL_SCISSOR_TEST );
		glDisable( GL_STENCIL_TEST );
		f.enableVertexAttribArray( 0 );
		f.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, quad );
		glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, quadIdx );
		f.disableVertexAttribArray( 0 );
	};

	enum Red { kNone, kMoonD, kSwap, kExt, kOrder, kAniso16, kRoute, kSkyOnly, kStarsOnly, kMoonOnly };
	struct Run
	{
		std::vector<std::uint16_t> out;
		std::string starsWhy, moonWhy;
		bool starsJoined = false, moonJoined = false;
	};
	// the Vulkan arm: the sky's pass held, the bodies joined in GL's order, one flush, then the object
	auto runVk = [&]( const Job & j, const Stars & st, Red red, Run & res ) -> bool {
		int vp[4];
		vpOf( j, vp );
		clearFrame( fbFrame );
		glDisable( kMultisample );  // lookdevstage's SingleSample (GL's quads when a body goes back to GL)
		std::uint8_t g[kSkyGlobalBytes];
		float fy = 1.0f;
		globalBytes( j, vp[2], vp[3], g, fy );
		f.bindBuffer( kUbo, ubo );
		f.bufferData( kUbo, kSkyGlobalBytes, g, kStatic );
		f.bindBufferBase( kUbo, 0, ubo );
		setSky( j, vp );
		WwVkPresent::SkyRun t;
		t.w = W;
		t.h = H;
		std::memcpy( t.vp, vp, sizeof( t.vp ) );
		t.color = fc;
		for ( int k = 0; k < 4; k++ ) {
			t.lut[k] = lut[k];
			t.lutW[k] = lw[k];
			t.lutH[k] = lh[k];
			t.lutKey[k] = reinterpret_cast<std::uintptr_t>( &keyHome[k] );
		}
		std::memcpy( t.global, g, sizeof( t.global ) );
		packSky( t.loose );
		f.bindFramebuffer( kFb, fbFrame );
		glViewport( vp[0], vp[1], vp[2], vp[3] );
		WwVkPresent::skyBodiesArm( red != kRoute );
		const bool skyVk = WwVkPresent::skyRun( t, why );
		WwVkPresent::skyBodiesArm( false );
		if ( !skyVk ) {
			std::printf( "SKYBODY %s: skyRun refused: %s\n", j.name, why.c_str() );
			return false;
		}
		// the stars (drawStars -> skyStarsJoin)
		if ( red != kSkyOnly && red != kMoonOnly ) {
			setStars( j, vp );
			static WwVkPresent::SkyStars s;
			s = WwVkPresent::SkyStars();
			s.nv = int( st.pos.size() / 3 );
			s.ni = int( st.idx.size() );
			s.pos = st.pos.data();
			s.col = red == kExt ? st.white.data() : st.col.data();
			s.uv = st.uv.data();
			s.idx = st.idx.data();
			static const char * const kTex[WwVkPresent::kSkyStarsTex] = { "Tex", "Tex2", "ldTrans" };
			for ( int k = 0; k < WwVkPresent::kSkyStarsTex; k++ )
				s.tex[k] = WwVkPresent::skyBodyUnitTex( &fn, pStars, kTex[k] );
			glGetIntegerv( GL_VIEWPORT, s.vp );
			std::memcpy( s.global, g, sizeof( s.global ) );
			WwVkPresent::fxPack( &fn, pStars, WwVkPresent::kT_skyStarsVs, int( std::size( WwVkPresent::kT_skyStarsVs ) ), s.vs );
			WwVkPresent::fxPack( &fn, pStars, WwVkPresent::kT_skyStarsPs, int( std::size( WwVkPresent::kT_skyStarsPs ) ), s.ps );
			s.gp = glProc;
			res.starsJoined = WwVkPresent::skyStars( s, res.starsWhy );
			if ( !res.starsJoined ) {  // the caller's way: flush what is held, then GL draws the body
				std::string w;
				WwVkPresent::skyBodiesFlush( w );
				f.bindFramebuffer( kFb, fbFrame );
				drawStarsGl( st, vp );
			}
		}
		// the moon globe (drawMoonGlobe -> skyMoonJoin)
		if ( red == kAniso16 )  // GL's sampler state at the join is what the twin reads
			for ( int k = 1; k < 3; k++ ) {
				glBindTexture( GL_TEXTURE_2D, maps[k].tex );
				glTexParameterf( GL_TEXTURE_2D, kAniso, 16.0f );
			}
		if ( red != kSkyOnly && red != kStarsOnly ) {
			setMoon( j, vp, fy );
			static WwVkPresent::SkyMoon m;
			m = WwVkPresent::SkyMoon();
			static const char * const kTex[WwVkPresent::kSkyMoonTex] = { "atAlbedo", "atRelief", "atTransTex", "atSunView", "atMoonView" };
			for ( int k = 0; k < WwVkPresent::kSkyMoonTex; k++ )
				m.tex[k] = WwVkPresent::skyBodyUnitTex( &fn, pMoon, kTex[k] );
			if ( red == kSwap )
				std::swap( m.tex[0], m.tex[1] );
			glGetIntegerv( GL_VIEWPORT, m.vp );
			std::memcpy( m.global, g, sizeof( m.global ) );
			WwVkPresent::fxPack( &fn, pMoon, WwVkPresent::kT_skyMoonVs, int( std::size( WwVkPresent::kT_skyMoonVs ) ), m.vs );
			WwVkPresent::fxPack( &fn, pMoon, WwVkPresent::kT_skyMoonPs, int( std::size( WwVkPresent::kT_skyMoonPs ) ), m.ps );
			if ( red == kMoonD )
				std::memset( m.ps + 448, 0, 12 );  // kT_skyMoonPs moonD
			m.gp = glProc;
			res.moonJoined = WwVkPresent::skyMoon( m, res.moonWhy );
			if ( !res.moonJoined ) {
				std::string w;
				WwVkPresent::skyBodiesFlush( w );
				f.bindFramebuffer( kFb, fbFrame );
				setMoon( j, vp, fy );
				drawMoonGl( vp );
			}
		}
		if ( red == kAniso16 )
			for ( int k = 1; k < 3; k++ ) {
				glBindTexture( GL_TEXTURE_2D, maps[k].tex );
				glTexParameterf( GL_TEXTURE_2D, kAniso, kJudgedAniso );
			}
		glEnable( kMultisample );
		if ( red == kOrder )
			object( fbFrame );
		if ( !WwVkPresent::skyBodiesFlush( why ) ) {
			std::printf( "SKYBODY %s: the flush failed: %s\n", j.name, why.c_str() );
			return false;
		}
		if ( red != kOrder )
			object( fbFrame );
		res.out = readSamples( fc );
		return glGetError() == GL_NO_ERROR;
	};
	// GL's backdrop (single-sample unless msaa), then the object
	auto runGl = [&]( const Job & j, const Stars & st, bool stars, bool moon, bool msaa, std::vector<std::uint16_t> & out ) {
		int vp[4];
		vpOf( j, vp );
		clearFrame( fbRef );
		if ( !msaa )
			glDisable( kMultisample );
		std::uint8_t g[kSkyGlobalBytes];
		float fy = 1.0f;
		globalBytes( j, vp[2], vp[3], g, fy );
		f.bindBuffer( kUbo, ubo );
		f.bufferData( kUbo, kSkyGlobalBytes, g, kStatic );
		f.bindBufferBase( kUbo, 0, ubo );
		setSky( j, vp );
		f.bindFramebuffer( kFb, fbRef );
		skyQuadGl( vp );
		if ( stars ) {
			setStars( j, vp );
			drawStarsGl( st, vp );
		}
		if ( moon ) {
			setMoon( j, vp, fy );
			drawMoonGl( vp );
		}
		glEnable( kMultisample );
		object( fbRef );
		out = readSamples( refCol );
		return glGetError() == GL_NO_ERROR;
	};

	const Job jobs[2] = {
		// night: sRGB output, a big globe low in the view, the stars' Tex2 (unbound) blend, the transmittance LUT and the
		// scatter lobe on
		{ "night", 120.0f, 14.0f, 55.0f, 10.0f, -20.0f, 24.0f, 0.0f, 8.0f, 1.0f, 0.5f, 0, 0.6f, 0.3f, 1, 1, { 0, 0, 0, 0 } },
		// dusk: linear output, a smaller globe, an offset smaller viewport (the pass's viewport and atRect), the lobe off
		{ "dusk", -40.0f, 20.0f, 40.0f, -60.0f, 4.0f, 14.0f, -3.0f, 3.0f, 2.0f, 0.1f, 3, 1.0f, 0.0f, 0, 0, { 0, 0, 0, 0 } },
	};
	Job jobs2[2] = { jobs[0], jobs[1] };
	jobs2[1].vp[0] = W / 8;
	jobs2[1].vp[1] = H / 6;
	jobs2[1].vp[2] = W / 2;
	jobs2[1].vp[3] = H / 2;
	Stars stars[2];
	for ( int ji = 0; ji < 2; ji++ ) {
		int vp[4];
		vpOf( jobs2[ji], vp );
		std::uint8_t g[kSkyGlobalBytes];
		float fy = 1.0f;
		globalBytes( jobs2[ji], vp[2], vp[3], g, fy );
		stars[ji] = makeStars( jobs2[ji], 2.0f / ( fy * float( vp[3] ) ), float( vp[2] ) / float( vp[3] ), rng );
	}

	bool ok = true;
	std::vector<std::uint16_t> refs[2], vkGreen[2];
	for ( int ji = 0; ji < 2; ji++ ) {
		const Job & j = jobs2[ji];
		Run r;
		std::vector<std::uint16_t> noStars, noMoon;
		if ( !runVk( j, stars[ji], kNone, r ) || !runGl( j, stars[ji], true, true, false, refs[ji] )
			|| !runGl( j, stars[ji], false, true, false, noStars ) || !runGl( j, stars[ji], true, false, false, noMoon ) ) {
			std::printf( "SKYBODY %s FAIL (a run failed)\n", j.name );
			ok = false;
			continue;
		}
		vkGreen[ji] = r.out;
		const Halves v = compare( r.out, refs[ji] );
		// the bodies show: each changes the frame (else a body that never drew would pass)
		const Halves vs = compare( refs[ji], noStars ), vm = compare( refs[ji], noMoon );
		const bool joined = r.starsJoined && r.moonJoined;
		const bool shows = vs.diff * 200 > vs.total / 100 && vm.diff * 200 > vm.total / 100;  // each > 0.005 % of halves
		if ( std::getenv( "SKYBODY_DUMP" ) ) {
			for ( int side = 0; side < 2; side++ ) {
				const std::vector<std::uint16_t> & b = side ? refs[ji] : r.out;
				std::ofstream o( dir + ( side ? "/gl_" : "/vk_" ) + j.name + ".bin", std::ios::binary );
				const std::int32_t hd[2] = { WS, H };
				o.write( reinterpret_cast<const char *>( hd ), sizeof( hd ) );
				o.write( reinterpret_cast<const char *>( b.data() ), std::streamsize( b.size() * 2 ) );
			}
		}
		std::printf( "SKYBODY %s %s (%zu of %zu halves differ, %zu past 1 ulp, %zu past 4, max %d ulp; stars %s, moon %s; the stars "
			"change %zu halves, the moon %zu)\n", j.name, joined && shows ? v.word() : "FAIL", v.diff, v.total, v.past1, v.past4, v.maxu,
			r.starsJoined ? "joined" : r.starsWhy.c_str(), r.moonJoined ? "joined" : r.moonWhy.c_str(), vs.diff, vm.diff );
		ok = ok && v.ok() && joined && shows;
	}
	// each part on its own (the sky alone, the stars alone, the globe alone) against GL's same part: where a FAIL sits
	for ( int ji = 0; ji < 2; ji++ ) {
		const struct
		{
			const char * name;
			Red red;
			bool stars, moon;
		} parts[3] = { { "sky", kSkyOnly, false, false }, { "stars", kStarsOnly, true, false }, { "moon", kMoonOnly, false, true } };
		for ( const auto & pt : parts ) {
			Run r;
			std::vector<std::uint16_t> ref;
			const bool ran = runVk( jobs2[ji], stars[ji], pt.red, r ) && runGl( jobs2[ji], stars[ji], pt.stars, pt.moon, false, ref );
			const Halves v = compare( r.out, ref );
			if ( std::getenv( "SKYBODY_DUMP" ) )
				for ( int side = 0; side < 2; side++ ) {
					const std::vector<std::uint16_t> & b = side ? ref : r.out;
					std::ofstream o( dir + ( side ? "/gl_" : "/vk_" ) + jobs2[ji].name + "_" + pt.name + ".bin", std::ios::binary );
					const std::int32_t hd[2] = { WS, H };
					o.write( reinterpret_cast<const char *>( hd ), sizeof( hd ) );
					o.write( reinterpret_cast<const char *>( b.data() ), std::streamsize( b.size() * 2 ) );
				}
			std::printf( "SKYBODY %s part %s %s (%zu halves differ, %zu past 1 ulp, %zu past 4, max %d ulp)\n", jobs2[ji].name, pt.name,
				ran ? v.word() : "FAIL", v.diff, v.past1, v.past4, v.maxu );
			ok = ok && ran && v.ok();
		}
	}
	{
		const WwVkPresent::SkyStat st = WwVkPresent::takeSkyStat();
		const bool good = st.stars == 4 && st.moons == 4 && st.bodyGl == 0 && st.fails == 0 && st.flushFails == 0;
		std::printf( "SKYBODY stat %s: %d sky runs, stars %d, moon %d, bodies to GL %d (%s), fails %d (%s), flush fails %d, %d pipelines "
			"built in %.1f ms\n", good ? "OK" : "FAIL", st.runs, st.stars, st.moons, st.bodyGl, st.bodyWhy.c_str(), st.fails,
			st.failWhy.c_str(), st.flushFails, st.pipelines, st.buildMs );
		ok = ok && good;
	}
	// the reds against the green references: each must FAIL
	const struct
	{
		const char * name;
		Red red;
		int job;
	} reds[5] = { { "moond (moonD dropped from the globe's pixel block)", kMoonD, 0 },
		{ "swap (moon_d and moon_n swapped)", kSwap, 0 }, { "extinction (the stars' vertex colours dropped)", kExt, 0 },
		{ "order (the object drawn before the held crossing)", kOrder, 1 }, { "aniso (the moon's maps at 16x when Vulkan joins, GL's green at 1x)", kAniso16, 0 } };
	for ( const auto & rd : reds ) {
		Run r;
		const bool ran = runVk( jobs2[rd.job], stars[rd.job], rd.red, r );
		const Halves v = compare( r.out, refs[rd.job] );
		const bool redFails = !ran || !v.ok();
		std::printf( "SKYBODY RED %s %s (%zu halves differ, %zu past 4 ulp, max %d; must FAIL)\n", redFails ? "FAIL" : v.word(), rd.name,
			v.diff, v.past4, v.maxu );
		ok = ok && redFails;
	}
	// red msaa: GL's backdrop with multisampling on against the Vulkan bodies (every sample at the centre)
	for ( int ji = 0; ji < 2; ji++ ) {
		std::vector<std::uint16_t> ms;
		const bool ran = runGl( jobs2[ji], stars[ji], true, true, true, ms );
		const Halves v = compare( vkGreen[ji], ms );
		const bool redFails = !ran || !v.ok();
		std::printf( "SKYBODY RED %s msaa %s (GL's backdrop multisampled: %zu halves differ, %zu past 4 ulp, max %d; must FAIL)\n",
			redFails ? "FAIL" : v.word(), jobs2[ji].name, v.diff, v.past4, v.maxu );
		ok = ok && redFails;
	}
	// logged, not judged: the app's 16x anisotropy on every map (NV's anisotropic footprint differs GL vs Vulkan)
	for ( int ji = 0; ji < 2; ji++ ) {
		for ( const Map & m : maps ) {
			glBindTexture( GL_TEXTURE_2D, m.tex );
			glTexParameterf( GL_TEXTURE_2D, kAniso, 16.0f );
		}
		Run r;
		std::vector<std::uint16_t> ref;
		const bool ran = runVk( jobs2[ji], stars[ji], kNone, r ) && runGl( jobs2[ji], stars[ji], true, true, false, ref );
		const Halves v = compare( r.out, ref );
		std::printf( "SKYBODY logged aniso16 %s %s (%zu halves differ, %zu past 4 ulp, max %d ulp; not judged)\n", jobs2[ji].name,
			ran ? v.word() : "did not run", v.diff, v.past4, v.maxu );
		for ( const Map & m : maps ) {
			glBindTexture( GL_TEXTURE_2D, m.tex );
			glTexParameterf( GL_TEXTURE_2D, kAniso, kJudgedAniso );
		}
	}
	// logged, not judged: the night camera 2 deg lower (el 12) puts the sky pass's own horizon on two pixels where GL and
	// Vulkan pick ground vs sky (G1's atmo_sky, not the bodies): the sky part alone
	{
		Job jl = jobs2[0];
		jl.camEl = 12.0f;
		Run r;
		std::vector<std::uint16_t> ref;
		const bool ran = runVk( jl, stars[0], kSkyOnly, r ) && runGl( jl, stars[0], false, false, false, ref );
		const Halves v = compare( r.out, ref );
		std::printf( "SKYBODY logged night el 12 part sky %s (%zu halves differ, %zu past 4 ulp, max %d ulp; not judged)\n",
			ran ? v.word() : "did not run", v.diff, v.past4, v.maxu );
	}
	WwVkPresent::takeSkyStat();
	// the route red (WW_VKVIEW_SKYBODY_RED=1: the bodies not armed): the sky crosses alone, each body refuses with a why and
	// draws on GL after it; the picture is GL's
	{
		Run r;
		const bool ran = runVk( jobs2[0], stars[0], kRoute, r );
		const Halves v = compare( r.out, refs[0] );
		const WwVkPresent::SkyStat st = WwVkPresent::takeSkyStat();
		const bool good = ran && v.ok() && !r.starsJoined && !r.moonJoined && !r.starsWhy.empty() && st.bodyGl == 2 && st.stars == 0;
		std::printf( "SKYBODY route %s (red WW_VKVIEW_SKYBODY_RED: %s, %zu halves differ, max %d ulp; refused: \"%s\"; bodies to GL %d)\n",
			good ? "OK" : "FAIL", v.word(), v.diff, v.maxu, r.starsWhy.c_str(), st.bodyGl );
		ok = ok && good;
	}
	// a held crossing never flushed is dropped at the next skyRun, and said
	{
		int vp[4];
		vpOf( jobs2[0], vp );
		clearFrame( fbFrame );
		std::uint8_t g[kSkyGlobalBytes];
		float fy = 1.0f;
		globalBytes( jobs2[0], vp[2], vp[3], g, fy );
		setSky( jobs2[0], vp );
		WwVkPresent::SkyRun t;
		t.w = W;
		t.h = H;
		std::memcpy( t.vp, vp, sizeof( t.vp ) );
		t.color = fc;
		for ( int k = 0; k < 4; k++ ) {
			t.lut[k] = lut[k];
			t.lutW[k] = lw[k];
			t.lutH[k] = lh[k];
			t.lutKey[k] = reinterpret_cast<std::uintptr_t>( &keyHome[k] );
		}
		std::memcpy( t.global, g, sizeof( t.global ) );
		packSky( t.loose );
		f.bindFramebuffer( kFb, fbFrame );
		glViewport( vp[0], vp[1], vp[2], vp[3] );
		WwVkPresent::skyBodiesArm( true );
		const bool a = WwVkPresent::skyRun( t, why ) && WwVkPresent::skyBodiesHeld();
		const bool b = WwVkPresent::skyRun( t, why ) && !WwVkPresent::skyBodiesHeld();
		const WwVkPresent::SkyStat st = WwVkPresent::takeSkyStat();
		const bool good = a && b && st.bodyWhy.find( "dropped" ) != std::string::npos;
		std::printf( "SKYBODY drop %s (held %d, then crossed alone %d; \"%s\")\n", good ? "OK" : "FAIL", int( a ), int( b ), st.bodyWhy.c_str() );
		ok = ok && good;
	}
	f.bindFramebuffer( kFb, 0 );
	WwVkPresent::toneFrameDrop();
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	DestroyWindow( wnd );
	std::printf( "SKYBODY GATE %s\n", ok ? "OK" : "FAILED" );
	return ok ? 0 : 1;
}

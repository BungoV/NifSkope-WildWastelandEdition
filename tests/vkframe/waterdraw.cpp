// Lane VKWATER3 (2026-11-13, row G5): the water's shaded draw on the present device (vkwater.inc waterRun: the mesh pipeline
// over GL's own vertex / element buffers, the water_view_vs / water_view_ps twins, the loose uniforms through the FxField
// tables, every input through waterTwin) against GL's own program (fo4_default.vert + fo4_waterlook.frag as the app links
// it, written by waterdraw_prep.py), per sample, into the same 4x MSAA RGBA16F + D24S8 frame contents. fxview.cpp's
// helpers are reused (its main renamed). GL's side of the draw is read by the shipped waterStateOf.
// Scene: a wavy grid (a third of its rows wound clockwise, so the cull is tested both ways), a depth slab over the left half
// of the frame before the water (LEQUAL against it, depth written), every sampler of the program on its own unit with a
// texture of the app's format holding seeded data (integer and texel-buffer inputs zero; the scene depth a smooth seeded
// field, since GL truncates its fetch coordinates), every loose float seeded from its own stream (the seed's draw ^ FNV-1a
// of its name, so a name holds one value in every variant). Switches are never a coin (ww-gl-vk-interop): every int is 0,
// then each seed's app-like profile turns its switches on (seed 1 every look row, seed 2 that plus the air, seed 3 the sea),
// and every profile is proven by its judged reds failing. Seed 2 also draws through a scissor and from an element offset,
// with waterProbe 25 (0 elsewhere). A tessellated twin (argv[2], e.g. fo4_waterlookd: hs + ds on 3-vertex patches) adds
// seed 3 with the sea on and displaced (app-sized ocean values).
// Bar (set before the first run, never widened; notes/vkwater3/README.md):
//   covered = samples whose depth or colour differs from the frame before the water on either side; must be >= 1000
//   (an empty layer FAILS); coverage mismatch (covered on one side only) <= 0.05 % of covered; depth within 1 D24 unit
//   on every sample covered on both sides; colour |vk - gl| <= 1e-2 * max(1, |gl|) per half on >= 99.9 % of the samples
//   covered on both sides (NaN equals NaN). EXACT = no half and no depth differs and coverage equal; BOUND = within the bar.
// Reds (each must FAIL, never refuse): red 1 the ps loose block one vec4 off, red 2 plain samplers, red 4 cull flipped,
// red 8 depth ALWAYS; tessellated only: red 16 the domain origin upper-left (seed 1), red 32 the ds loose block one vec4
// off (seed 3, where a tessellated twin's red 2 is judged too). Seed 2 (the air) judges reds 1 and 2.
// Lane VKWATER4: the m twin adds the SSR water ray arm (`WATERDRAW ray red N`: 200x120 RGBA32F over a D24S8
// renderbuffer with an occluder, depth mask off, waterSslr 1; same colour bar per pixel; reds 1 / 2 / 4 / 8 FAIL).
// G5 (11-15): seed 3 also draws with waterOceanB / C / D as shared targets (no waterTwin copy) EXACT against the copy
// route (ocean copies 3 there, 0 in place); red prev (the other shared set) FAILS and is EXACT against its own copy route.
// All-on census (11-15): `WATERDRAW unslotted: none` (every GL input has a twin slot) or the gate fails; red: a fake
// sampler + loose uniform must be refused by name. Argv 3 = extra defines for GL's program (the all-on RT set: every
// bar above is judged against GL's RT build).
// Last line `WATERDRAW GATE OK` or `WATERDRAW GATE FAILED`.
// Sampling: judged at anisotropy 1 on both sides (NV's anisotropic footprint differs GL vs Vulkan at the same maxAnisotropy:
// 10 248 samples past at the app's 16, BOUND at 1). The app's 16 is drawn once more on seed 1 and logged, never judged.
// The noise's 16 is intended (the game's slots 4..6); the misses are its normal bending the SSR lookup over seeded noise.
#define main fxview_main
#include "fxview.cpp"
#undef main

#include <chrono>
#include <random>

namespace wdr
{
using namespace fv;

//! GL's entry points under GL's own names (waterStateOf / waterCensusOf / fxPack call fn->glX as on Qt's functions)
struct GlFn
{
	void( WINAPI * glGetProgramiv )( GLuint, GLenum, GLint * ) = nullptr;
	void( WINAPI * glGetActiveUniform )( GLuint, GLuint, GLsizei, GLsizei *, GLint *, GLenum *, char * ) = nullptr;
	void( WINAPI * glGetActiveUniformsiv )( GLuint, GLsizei, const GLuint *, GLenum, GLint * ) = nullptr;
	GLint( WINAPI * glGetUniformLocation )( GLuint, const char * ) = nullptr;
	void( WINAPI * glGetUniformiv )( GLuint, GLint, GLint * ) = nullptr;
	void( WINAPI * glGetUniformfv )( GLuint, GLint, GLfloat * ) = nullptr;
	void( WINAPI * glActiveTexture )( GLenum ) = nullptr;
	void( WINAPI * glGetIntegerv )( GLenum, GLint * ) = nullptr;
	void( WINAPI * glGetBooleanv )( GLenum, GLboolean * ) = nullptr;
	void( WINAPI * glGetFloatv )( GLenum, GLfloat * ) = nullptr;  // lane VKFAR2: twinStateOf reads the polygon offset
	GLboolean( WINAPI * glIsEnabled )( GLenum ) = nullptr;
	void( WINAPI * glGetTexLevelParameteriv )( GLenum, GLint, GLenum, GLint * ) = nullptr;
	void( WINAPI * glGetTexParameteriv )( GLenum, GLenum, GLint * ) = nullptr;
	void( WINAPI * glGetTexParameterfv )( GLenum, GLenum, GLfloat * ) = nullptr;
	void( WINAPI * glGetAttachedShaders )( GLuint, GLsizei, GLsizei *, GLuint * ) = nullptr;
	void( WINAPI * glGetShaderiv )( GLuint, GLenum, GLint * ) = nullptr;
	void( WINAPI * glBindBuffer )( GLenum, GLuint ) = nullptr;
	void( WINAPI * glGetBufferParameteriv )( GLenum, GLenum, GLint * ) = nullptr;
	void( WINAPI * glGetVertexAttribiv )( GLuint, GLenum, GLint * ) = nullptr;
	void( WINAPI * glGetVertexAttribfv )( GLuint, GLenum, GLfloat * ) = nullptr;
	void( WINAPI * glGetVertexAttribPointerv )( GLuint, GLenum, void ** ) = nullptr;
	void( WINAPI * glGetIntegeri_v )( GLenum, GLuint, GLint * ) = nullptr;
	void( WINAPI * glGetInteger64i_v )( GLenum, GLuint, long long * ) = nullptr;
	GLuint( WINAPI * glGetUniformBlockIndex )( GLuint, const char * ) = nullptr;
	void( WINAPI * glGetActiveUniformBlockiv )( GLuint, GLuint, GLenum, GLint * ) = nullptr;
};
GlFn gw;

//! setup-only entry points
struct Ex
{
	void( WINAPI * genVertexArrays )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindVertexArray )( GLuint ) = nullptr;
	void( WINAPI * vertexAttribPointer )( GLuint, GLint, GLenum, GLboolean, GLsizei, const void * ) = nullptr;
	void( WINAPI * enableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * texStorage2D )( GLenum, GLsizei, GLenum, GLsizei, GLsizei ) = nullptr;
	void( WINAPI * texStorage3D )( GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei ) = nullptr;
	void( WINAPI * compressedTexSubImage2D )( GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const void * ) = nullptr;
	void( WINAPI * texBuffer )( GLenum, GLenum, GLuint ) = nullptr;
	void( WINAPI * bufferSubData )( GLenum, std::ptrdiff_t, std::ptrdiff_t, const void * ) = nullptr;
	void( WINAPI * uniform2uiv )( GLint, GLsizei, const GLuint * ) = nullptr;
	void( WINAPI * uniform3uiv )( GLint, GLsizei, const GLuint * ) = nullptr;
	void( WINAPI * uniform4uiv )( GLint, GLsizei, const GLuint * ) = nullptr;
	void( WINAPI * uniformMatrix2fv )( GLint, GLsizei, GLboolean, const GLfloat * ) = nullptr;
	// lane VKWATER4: the ray pass's depth is a renderbuffer (ao.depthRb)
	void( WINAPI * genRenderbuffers )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindRenderbuffer )( GLenum, GLuint ) = nullptr;
	void( WINAPI * renderbufferStorage )( GLenum, GLenum, GLsizei, GLsizei ) = nullptr;
	void( WINAPI * framebufferRenderbuffer )( GLenum, GLenum, GLenum, GLuint ) = nullptr;
	void( WINAPI * getRenderbufferParameteriv )( GLenum, GLenum, GLint * ) = nullptr;
};
Ex ex;

template <class P> bool ldx( P & p, const char * n )
{
	p = reinterpret_cast<P>( glProc( n ) );
	if ( !p )
		std::printf( "WATERDRAW missing %s\n", n );
	return p != nullptr;
}

bool loadMore()
{
	bool ok = true;
#define WD_L( n ) ok = ldx( gw.n, #n ) && ok
	WD_L( glGetProgramiv ); WD_L( glGetActiveUniform ); WD_L( glGetActiveUniformsiv ); WD_L( glGetUniformLocation );
	WD_L( glGetUniformiv ); WD_L( glGetUniformfv ); WD_L( glActiveTexture ); WD_L( glGetIntegerv ); WD_L( glGetBooleanv ); WD_L( glGetFloatv );
	WD_L( glIsEnabled ); WD_L( glGetTexLevelParameteriv ); WD_L( glGetTexParameteriv ); WD_L( glGetTexParameterfv );
	WD_L( glGetAttachedShaders ); WD_L( glGetShaderiv ); WD_L( glBindBuffer ); WD_L( glGetBufferParameteriv );
	WD_L( glGetVertexAttribiv ); WD_L( glGetVertexAttribfv ); WD_L( glGetVertexAttribPointerv ); WD_L( glGetIntegeri_v );
	WD_L( glGetInteger64i_v ); WD_L( glGetUniformBlockIndex ); WD_L( glGetActiveUniformBlockiv );
#undef WD_L
#define WD_X( m, n ) ok = ldx( ex.m, n ) && ok
	WD_X( genVertexArrays, "glGenVertexArrays" ); WD_X( bindVertexArray, "glBindVertexArray" );
	WD_X( vertexAttribPointer, "glVertexAttribPointer" ); WD_X( enableVertexAttribArray, "glEnableVertexAttribArray" );
	WD_X( texStorage2D, "glTexStorage2D" ); WD_X( texStorage3D, "glTexStorage3D" );
	WD_X( compressedTexSubImage2D, "glCompressedTexSubImage2D" ); WD_X( texBuffer, "glTexBuffer" );
	WD_X( bufferSubData, "glBufferSubData" ); WD_X( uniform2uiv, "glUniform2uiv" ); WD_X( uniform3uiv, "glUniform3uiv" );
	WD_X( uniform4uiv, "glUniform4uiv" ); WD_X( uniformMatrix2fv, "glUniformMatrix2fv" );
	WD_X( genRenderbuffers, "glGenRenderbuffers" ); WD_X( bindRenderbuffer, "glBindRenderbuffer" );
	WD_X( renderbufferStorage, "glRenderbufferStorage" ); WD_X( framebufferRenderbuffer, "glFramebufferRenderbuffer" );
	WD_X( getRenderbufferParameteriv, "glGetRenderbufferParameteriv" );
#undef WD_X
	return ok;
}

const char * kDepthFs = "#version 410 core\n"
	"uniform sampler2DMS src; uniform int S; out vec4 o;\n"
	"void main() { ivec2 q = ivec2( gl_FragCoord.xy ); o = vec4( texelFetch( src, ivec2( q.x / S, q.y ), q.x % S ).r ); }\n";
const char * kSlabFs = "#version 410 core\nuniform float zd; out vec4 o;\n"
	"void main() { gl_FragDepth = zd; o = vec4( 0.6, 0.2, 0.1, 0.5 ); }\n";

//! one sampler of the program as the gate binds it
struct Smp
{
	std::string name;
	GLenum glType = 0;
	int kind = 0;       // 0 2D, 1 3D, 2 2D array, 4 cube-map array, 5 texel buffer
	GLenum fmt = 0x881A;
	int w = 64, h = 64, d = 1, levels = 1, unit = 0;
	float aniso = 1.0f, appAniso = 1.0f;
	bool intF = false, depth = false, compare = false, bc = false;
	GLuint tex = 0, buf = 0;
};

bool smpOf( Smp & s, int W, int H )
{
	switch ( s.glType ) {
	case 0x8B5E: s.kind = 0; break;                          // sampler2D
	case 0x8B5F: s.kind = 1; break;                          // sampler3D
	case 0x8DC1: s.kind = 2; break;                          // sampler2DArray
	case 0x8DC4: s.kind = 2; s.compare = true; break;        // sampler2DArrayShadow
	case 0x900D: s.kind = 4; s.compare = true; break;        // samplerCubeMapArrayShadow
	case 0x8DC2: s.kind = 5; s.fmt = 0x8814; break;          // samplerBuffer
	case 0x8DD0: s.kind = 5; s.fmt = 0x823B; s.intF = true; break;  // isamplerBuffer: RG32I
	case 0x8DD8: s.kind = 5; s.fmt = 0x8236; s.intF = true; break;  // usamplerBuffer
	case 0x8DD2: s.kind = 0; s.fmt = 0x8236; s.intF = true; break;  // usampler2D
	case 0x8DCA: s.kind = 0; s.fmt = 0x8235; s.intF = true; break;  // isampler2D: R32I
	default: return false;
	}
	const std::string & n = s.name;
	if ( s.kind == 0 && !s.intF ) {
		s.w = W;
		s.h = H;
	}
	if ( s.kind == 1 ) {
		s.w = s.h = 16;
		s.d = 8;
	}
	if ( s.kind == 2 ) {
		s.d = 4;
	}
	if ( n == "waterNoise0" || n == "waterNoise1" || n == "waterNoise2" ) {
		s.fmt = 0x8DBD;  // BC5 (RGTC2) as the DDS noise loads; trilinear repeat, aniso 16
		s.bc = true;
		s.w = s.h = 64;
		s.levels = 7;
		s.aniso = 16.0f;
	} else if ( n == "waterInfoIndex" || n == "waterFlowIndex" ) {
		s.fmt = 0x823C;  // RG32UI
	} else if ( n == "waterSkyAtlas" ) {
		s.fmt = 0x8232;  // R8UI
	} else if ( n == "waterInfoBodies" || n == "waterOceanC" || n == "cellCtGbuf" ) {
		s.fmt = 0x8814;  // RGBA32F
	} else if ( n == "waterMoment0" || n == "waterMoment1" || n == "waterMoment2" ) {
		s.fmt = 0x8230;  // RG32F, mipped, the noise's sampler
		s.w = s.h = 64;
		s.levels = 7;
		s.aniso = 16.0f;
	} else if ( n == "waterDepth" ) {
		s.fmt = 0x8CAD;  // DEPTH32F_STENCIL8 (glscene.cpp's float depth frame)
		s.depth = true;
	} else if ( n == "csmMap" ) {
		s.fmt = 0x81A5;  // D16 array
		s.depth = true;
	} else if ( n == "cellShadow" ) {
		s.fmt = 0x81A5;
		s.depth = true;
		s.w = s.h = 16;
		s.d = 6;
	} else if ( n == "ussTerrMap" || n == "ussContactMap" ) {
		s.fmt = 0x822E;  // R32F
	} else if ( n == "waterOceanB" || n == "waterOceanD" ) {
		s.w = s.h = 64;
		s.d = 6;
		s.levels = 7;
	}
	if ( n == "waterOceanC" ) {
		s.w = s.h = 64;
		s.d = 6;
		s.levels = 7;
	}
	// lane VKFLIGHT1: WD_LUTFMT (waterdraw.sh reads it off the app's upload) = the app's own 16^3 grade LUT format
	if ( n == "cellIsLut" ) {
		if ( const char * lf = getenv( "WD_LUTFMT" ) ) {
			s.fmt = GLenum( std::strtoul( lf, nullptr, 0 ) );
			s.w = s.h = s.d = 16;
		}
	}
	// the judged runs sample at anisotropy 1 on both sides (NV's anisotropic footprint differs GL vs Vulkan at the same
	// maxAnisotropy: skill ww-gl-vk-interop, VKOBJ4's colour arm); the app's 16 is drawn once more and logged, not judged
	s.appAniso = s.aniso;
	s.aniso = 1.0f;
	return true;
}

unsigned targetOf( int kind )
{
	static const unsigned t[6] = { 0x0DE1, 0x806F, 0x8C1A, 0x8513, 0x9009, 0x8C2A };
	return t[kind];
}

// the texture (made once, its contents per seed): float data in [0, 1), depth in [0, 1), integer and texel buffers zero
bool fill( Smp & s, std::mt19937 & rng )
{
	std::uniform_real_distribution<float> u01( 0.0f, 1.0f );
	const unsigned tg = targetOf( s.kind );
	if ( s.kind == 5 ) {
		if ( !s.tex ) {
			glGenTextures( 1, &s.tex );
			f.genBuffers( 1, &s.buf );
			f.bindBuffer( 0x8C2A, s.buf );
			f.bufferData( 0x8C2A, 16 * 256, nullptr, fStatic );
			glBindTexture( tg, s.tex );
			ex.texBuffer( tg, s.fmt, s.buf );
		}
		std::vector<std::uint8_t> z( 16 * 256, 0 );
		f.bindBuffer( 0x8C2A, s.buf );
		ex.bufferSubData( 0x8C2A, 0, std::ptrdiff_t( z.size() ), z.data() );
		return glGetError() == GL_NO_ERROR;
	}
	const bool fresh = !s.tex;
	if ( fresh )
		glGenTextures( 1, &s.tex );
	glBindTexture( tg, s.tex );
	if ( fresh ) {
		if ( s.kind == 0 )
			ex.texStorage2D( tg, s.levels, s.fmt, s.w, s.h );
		else
			ex.texStorage3D( tg, s.levels, s.fmt, s.w, s.h, s.d );
		const bool mip = s.levels > 1;
		glTexParameteri( tg, GL_TEXTURE_MIN_FILTER, s.intF ? GL_NEAREST : mip ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR );
		glTexParameteri( tg, GL_TEXTURE_MAG_FILTER, s.intF ? GL_NEAREST : GL_LINEAR );
		glTexParameteri( tg, GL_TEXTURE_WRAP_S, mip ? GL_REPEAT : 0x812F );
		glTexParameteri( tg, GL_TEXTURE_WRAP_T, mip ? GL_REPEAT : 0x812F );
		glTexParameteri( tg, 0x8072, 0x812F );
		glTexParameteri( tg, fMaxLevel, s.levels - 1 );
		if ( s.aniso > 1.0f )
			glTexParameterf( tg, 0x84FE, s.aniso );
		if ( s.compare ) {
			glTexParameteri( tg, 0x884C, 0x884E );  // COMPARE_REF_TO_TEXTURE
			glTexParameteri( tg, 0x884D, GL_LEQUAL );
		}
	}
	for ( int l = 0; l < s.levels; l++ ) {
		const int lw = std::max( s.w >> l, 1 ), lh = std::max( s.h >> l, 1 );
		const int ld = s.kind == 1 ? std::max( s.d >> l, 1 ) : s.kind == 0 ? 1 : s.d;
		const size_t n = size_t( lw ) * size_t( lh ) * size_t( ld );
		if ( s.bc ) {
			const GLsizei bytes = GLsizei( ( ( lw + 3 ) / 4 ) * ( ( lh + 3 ) / 4 ) * 16 );
			std::vector<std::uint8_t> b( static_cast<size_t>( bytes ) );
			for ( auto & x : b )
				x = std::uint8_t( rng() & 0xFF );
			ex.compressedTexSubImage2D( tg, l, 0, 0, lw, lh, s.fmt, bytes, b.data() );
			continue;
		}
		GLenum pf = GL_RGBA, ty = GL_FLOAT;
		std::vector<std::uint32_t> data;
		if ( s.intF ) {
			pf = ( s.fmt == 0x823C ) ? 0x8228 : 0x8D94;  // RG_INTEGER / RED_INTEGER
			ty = s.fmt == 0x8232 ? GL_UNSIGNED_BYTE : s.fmt == 0x8235 ? GL_INT : GL_UNSIGNED_INT;
			data.assign( n * 2, 0u );
		} else if ( s.fmt == 0x8CAD ) {
			pf = 0x84F9;  // DEPTH_STENCIL
			ty = 0x8DAD;  // FLOAT_32_UNSIGNED_INT_24_8_REV
			/* piece 2: a seeded smooth scene depth (two low waves per axis), as a drawn scene's: per-texel noise turned the
			 * refraction fetch's truncated texel (ivec2( offUV * size )) into a coin, one ulp of offUV a different depth
			 * (base and d 1665 .. 37453 samples past on other draws, every one gone with waterHaveDepth 0) */
			data.resize( n * 2 );
			const float fx = float( 1 + ( rng() & 1u ) ) * 6.2831853f / float( lw ), fy = float( 1 + ( rng() & 1u ) ) * 6.2831853f / float( lh );
			const float p0 = 6.2831853f * u01( rng ), p1 = 6.2831853f * u01( rng );
			for ( size_t i = 0; i < n; i++ ) {
				const float x = float( i % size_t( lw ) ), y = float( ( i / size_t( lw ) ) % size_t( lh ) );
				const float dv = 0.3f + 0.7f * ( 0.5f + 0.5f * std::sin( fx * x + p0 ) * std::cos( fy * y + p1 ) );
				std::memcpy( &data[i * 2], &dv, 4 );
				data[i * 2 + 1] = 0;
			}
		} else {
			int comps = 4;
			if ( s.fmt == 0x81A5 ) {
				pf = GL_DEPTH_COMPONENT;
				comps = 1;
			} else if ( s.fmt == 0x822E ) {
				pf = fRed;
				comps = 1;
			} else if ( s.fmt == 0x8230 ) {
				pf = fRg;
				comps = 2;
			}
			data.resize( n * size_t( comps ) );
			for ( auto & x : data ) {
				const float v = u01( rng );
				std::memcpy( &x, &v, 4 );
			}
			/* the air's (L, T) as a drawn air holds it (thin near the eye: T 0.6..1, L under 0.25): T 0..1 hid the
			 * surface under the air on some draws (seed 2's red 2 blind, the surface times a T near 0); the aerial
			 * perspective's froxels (atApTex) alike (d seed 2 draw 4: red 2 blind under it, failing with atAp.x 0) */
			if ( ( s.name == "volTex" || s.name == "atApTex" ) && comps == 4 )
				for ( size_t i = 0; i < data.size(); i++ ) {
					float v;
					std::memcpy( &v, &data[i], 4 );
					v = i % 4 == 3 ? 0.6f + 0.4f * v : 0.25f * v;
					std::memcpy( &data[i], &v, 4 );
				}
		}
		if ( s.kind == 0 )
			glTexSubImage2D( tg, l, 0, 0, lw, lh, pf, ty, data.data() );
		else
			f.texSubImage3D( tg, l, 0, 0, 0, lw, lh, ld, pf, ty, data.data() );
	}
	const GLenum e = glGetError();
	if ( e != GL_NO_ERROR )
		std::printf( "WATERDRAW texture %s: GL error 0x%X\n", s.name.c_str(), e );
	return e == GL_NO_ERROR;
}

// every active loose uniform seeded by its GL type (block members and samplers skipped), the vertex stage's placement ones
// fixed (the shape is drawn in view space, no instancing / wind / skinning), names with "Red" / "red" zero
int seedUnis( GLuint p, std::mt19937 & rng )
{
	std::uniform_real_distribution<float> u01( 0.05f, 1.0f );
	f.useProgram( p );
	GLint n = 0;
	f.getProgramiv( p, fActiveUnis, &n );
	int set = 0;
	/* piece 2: each name draws from its own stream (the seed's one draw ^ FNV-1a of the name), so a name holds the same
	 * value in every variant: one stream in GL's active order shifted every later draw by the variant's uniform set */
	std::uint32_t s0 = std::uint32_t( rng() );
	if ( const char * sa = std::getenv( "WATERDRAW_SALT" ) )	// diagnostic only (never in a gate run): another draw per name
		s0 ^= std::uint32_t( std::strtoul( sa, nullptr, 10 ) ) * 0x9E3779B9u;
	for ( GLint i = 0; i < n; i++ ) {
		char nm[256] = {};
		GLint sz = 0;
		GLenum ty = 0;
		f.getActiveUniform( p, GLuint( i ), 256, nullptr, &sz, &ty, nm );
		const GLuint idx = GLuint( i );
		GLint bi = -1;
		f.getActiveUniformsiv( p, 1, &idx, fBlockIndex, &bi );
		if ( bi >= 0 )
			continue;
		std::string base( nm );
		if ( base.size() > 3 && base.compare( base.size() - 3, 3, "[0]" ) == 0 )
			base.resize( base.size() - 3 );
		int comp = 0;
		char kind = 0;
		switch ( ty ) {
		case 0x1406: kind = 'f'; comp = 1; break;
		case 0x8B50: kind = 'f'; comp = 2; break;
		case 0x8B51: kind = 'f'; comp = 3; break;
		case 0x8B52: kind = 'f'; comp = 4; break;
		case 0x1404: case 0x8B56: kind = 'i'; comp = 1; break;
		case 0x8B53: case 0x8B57: kind = 'i'; comp = 2; break;
		case 0x8B54: case 0x8B58: kind = 'i'; comp = 3; break;
		case 0x8B55: case 0x8B59: kind = 'i'; comp = 4; break;
		case 0x1405: kind = 'u'; comp = 1; break;
		case 0x8DC6: kind = 'u'; comp = 2; break;
		case 0x8DC7: kind = 'u'; comp = 3; break;
		case 0x8DC8: kind = 'u'; comp = 4; break;
		case 0x8B5A: kind = '2'; comp = 4; break;
		case 0x8B5B: kind = '3'; comp = 9; break;
		case 0x8B5C: kind = '4'; comp = 16; break;
		default: continue;  // a sampler: bound by unit
		}
		const GLint l = f.getUniformLocation( p, nm );
		const size_t total = size_t( std::max( sz, 1 ) ) * size_t( comp );
		std::vector<float> fv( total );
		std::vector<GLint> iv( total );
		const bool zero = base == "farInst" || base == "farStreamOn" || base == "farSwayAmp" || base == "lodTreeAnim"
			|| base == "wwGrassWind2" || base == "wwTreeWind2" || base == "farVcAo" || base == "lodViewCode"
			|| base == "vertexColorOverride" || base.find( "Red" ) != std::string::npos || base.find( "red" ) != std::string::npos;
		std::uint32_t h = 2166136261u;
		for ( const char c : base )
			h = ( h ^ std::uint8_t( c ) ) * 16777619u;
		std::mt19937 nr( s0 ^ h );
		// every int / bool 0 here: switches come from the seed's profile (wdrProfile), never a coin (ww-gl-vk-interop)
		for ( size_t k = 0; k < total; k++ ) {
			fv[k] = zero ? 0.0f : u01( nr );
			iv[k] = 0;
		}
		if ( base == "modelViewMatrix" || base == "normalMatrix" ) {
			const int dim = kind == '4' ? 4 : 3;
			for ( int r = 0; r < dim; r++ )
				for ( int c = 0; c < dim; c++ )
					fv[size_t( r * dim + c )] = r == c ? 1.0f : 0.0f;
		}
		const GLsizei cnt = GLsizei( std::max( sz, 1 ) );
		switch ( kind ) {
		case 'f': ( comp == 1 ? f.uniform1fv : comp == 2 ? f.uniform2fv : comp == 3 ? f.uniform3fv : f.uniform4fv )( l, cnt, fv.data() ); break;
		case 'i': ( comp == 1 ? f.uniform1iv : comp == 2 ? f.uniform2iv : comp == 3 ? f.uniform3iv : f.uniform4iv )( l, cnt, iv.data() ); break;
		case 'u': {
			const GLuint * u = reinterpret_cast<const GLuint *>( iv.data() );
			( comp == 1 ? f.uniform1uiv : comp == 2 ? ex.uniform2uiv : comp == 3 ? ex.uniform3uiv : ex.uniform4uiv )( l, cnt, u );
			break;
		}
		case '2': ex.uniformMatrix2fv( l, cnt, GL_FALSE, fv.data() ); break;
		case '3': f.uniformMatrix3fv( l, cnt, GL_FALSE, fv.data() ); break;
		default: f.uniformMatrix4fv( l, cnt, GL_FALSE, fv.data() ); break;
		}
		set++;
	}
	return set;
}

struct Verdict
{
	size_t covered = 0, both = 0, onlyOne = 0, depthPast = 0, colPast = 0, halvesDiff = 0, depthDiff = 0;
	double maxRel = 0.0;
	long maxDepthU = 0;
	bool ok() const { return covered >= 1000 && onlyOne * 2000 <= covered && depthPast == 0 && colPast * 1000 <= both; }
	const char * word() const
	{
		return covered < 1000 ? "FAIL" : ( halvesDiff == 0 && depthDiff == 0 && onlyOne == 0 ) ? "EXACT" : ok() ? "BOUND" : "FAIL";
	}
};

Verdict judge( const std::vector<std::uint16_t> & bgC, const std::vector<float> & bgD, const std::vector<std::uint16_t> & vkC,
	const std::vector<float> & vkD, const std::vector<std::uint16_t> & glC, const std::vector<float> & glD )
{
	Verdict v;
	const size_t n = bgD.size();
	if ( !n || vkD.size() != n || glD.size() != n || bgC.size() != n * 4 || vkC.size() != n * 4 || glC.size() != n * 4 )
		return v;
	for ( size_t s = 0; s < n; s++ ) {
		bool cv = vkD[s] != bgD[s], cg = glD[s] != bgD[s];
		for ( int c = 0; c < 4; c++ ) {
			cv = cv || vkC[s * 4 + size_t( c )] != bgC[s * 4 + size_t( c )];
			cg = cg || glC[s * 4 + size_t( c )] != bgC[s * 4 + size_t( c )];
		}
		if ( !cv && !cg )
			continue;
		v.covered++;
		if ( cv != cg ) {
			v.onlyOne++;
			continue;
		}
		v.both++;
		const long du = std::lround( std::fabs( double( vkD[s] ) - double( glD[s] ) ) * 16777215.0 );
		v.depthDiff += vkD[s] != glD[s];
		v.maxDepthU = std::max( v.maxDepthU, du );
		v.depthPast += du > 1;
		bool past = false;
		for ( int c = 0; c < 4; c++ ) {
			const std::uint16_t a = vkC[s * 4 + size_t( c )], b = glC[s * 4 + size_t( c )];
			if ( a == b )
				continue;
			v.halvesDiff++;
			const float fa = h2f( a ), fb = h2f( b );
			if ( std::isnan( fa ) && std::isnan( fb ) )
				continue;
			const double rel = ( std::isnan( fa ) || std::isnan( fb ) ) ? 1e30
				: std::fabs( double( fa ) - double( fb ) ) / std::max( 1.0, std::fabs( double( fb ) ) );
			v.maxRel = std::max( v.maxRel, rel );
			past = past || rel > 1e-2;
		}
		v.colPast += past;
	}
	return v;
}

// where the colour misses sit (red 0 only; the verdict is judge's): their box, per channel, inside seed 2's scissor
void where( const std::vector<std::uint16_t> & vkC, const std::vector<std::uint16_t> & glC, int W, int H, int S )
{
	int x0 = W, y0 = H, x1 = -1, y1 = -1;
	size_t ch[4] = {}, inSc = 0, n = 0, slab = 0;
	for ( int y = 0; y < H; y++ )
		for ( int x = 0; x < W; x++ )
			for ( int k = 0; k < S; k++ ) {
				const size_t s = ( size_t( y ) * size_t( W ) + size_t( x ) ) * size_t( S ) + size_t( k );
				bool past = false;
				for ( int c = 0; c < 4; c++ ) {
					const float a = h2f( vkC[s * 4 + size_t( c )] ), b = h2f( glC[s * 4 + size_t( c )] );
					if ( std::fabs( double( a ) - double( b ) ) / std::max( 1.0, std::fabs( double( b ) ) ) > 1e-2 ) {
						ch[c]++;
						past = true;
					}
				}
				if ( !past )
					continue;
				n++;
				x0 = std::min( x0, x ), x1 = std::max( x1, x ), y0 = std::min( y0, y ), y1 = std::max( y1, y );
				inSc += x >= W / 5 && x < W / 5 + W / 2 && y >= H / 6 && y < H / 6 + H / 2;
				slab += x < W / 2;
			}
	std::printf( "WATERDRAW where: %zu samples past, box x %d..%d y %d..%d, r %zu g %zu b %zu a %zu, %zu inside seed 2's scissor, %zu over the slab half\n",
		n, x0, x1, y0, y1, ch[0], ch[1], ch[2], ch[3], inSc, slab );
}
}  // namespace wdr

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace wdr;
	gTag = "WATERDRAW";
	if ( argc < 2 ) {
		std::printf( "usage: waterdraw <prep dir> [program, default fo4_waterlook]\nWATERDRAW GATE FAILED\n" );
		return 2;
	}
	const std::string dir = argv[1];
	// item 4: the variant (waterdraw_prep.py wrote its stages); a tessellated one links 4 stages and draws 3-vertex patches
	const std::string progName = std::string( argc > 2 ? argv[2] : "fo4_waterlook" ) + ".prog";
	const int variant = WwVkPresent::waterVariantOf( progName );
	if ( variant < 0 ) {
		std::printf( "WATERDRAW GATE FAILED (%s has no Vulkan twin)\n", progName.c_str() );
		return 2;
	}
	const WwVkPresent::WaterTables & tab = WwVkPresent::waterTables( variant );
	const GLenum drawMode = tab.tess ? 0x000E : GL_TRIANGLES;  // GL_PATCHES (GL_PATCH_VERTICES stays 3, as in the app)
	auto fatal = [&]( const std::string & why ) {
		std::printf( "WATERDRAW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	const int S = 4, W = 256, H = 160, WS = W * S;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwwaterdraw";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwwaterdraw", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, 320, 180 };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwwaterdraw", "water draw harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
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
	std::printf( "WATERDRAW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	const GLuint pUnpack = program( kTriVs, kUnpackFs, "unpack" ), pDepth = program( kTriVs, kDepthFs, "depth" ),
		pSlab = program( kTriVs, kSlabFs, "slab" );
	GLuint pWater = 0;
	if ( !tab.tess )
		pWater = program( readFile( dir + "/prog.vert.glsl" ), readFile( dir + "/prog.frag.glsl" ), progName.c_str() );
	else {
		const GLuint a = shader( fVert, readFile( dir + "/prog.vert.glsl" ), "vert" ),
			b = shader( 0x8E88, readFile( dir + "/prog.tesc.glsl" ), "tesc" ),  // GL_TESS_CONTROL_SHADER
			c = shader( 0x8E87, readFile( dir + "/prog.tese.glsl" ), "tese" ),  // GL_TESS_EVALUATION_SHADER
			d = shader( fFrag, readFile( dir + "/prog.frag.glsl" ), "frag" );
		if ( a && b && c && d ) {
			pWater = f.createProgram();
			for ( GLuint s : { a, b, c, d } )
				f.attachShader( pWater, s );
			f.linkProgram( pWater );
			GLint ok = 0;
			f.getProgramiv( pWater, fLinked, &ok );
			if ( !ok ) {
				std::vector<char> log( 65536 );
				f.getProgramInfoLog( pWater, GLsizei( log.size() ), nullptr, log.data() );
				std::printf( "WATERDRAW link failed (%s): %.1500s\n", progName.c_str(), log.data() );
				pWater = 0;
			}
		}
	}
	std::printf( "WATERDRAW program %s (variant %d%s)\n", progName.c_str(), variant, tab.tess ? ", tessellated: 3-vertex patches" : "" );
	if ( !pUnpack || !pDepth || !pSlab || !pWater )
		return fatal( "GL programs" );
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

	// ---- the program's samplers: one unit each, a texture of the app's format
	std::vector<Smp> smp;
	{
		GLint n = 0;
		f.getProgramiv( pWater, fActiveUnis, &n );
		for ( GLint i = 0; i < n; i++ ) {
			char nm[256] = {};
			GLint sz = 0;
			GLenum ty = 0;
			f.getActiveUniform( pWater, GLuint( i ), 256, nullptr, &sz, &ty, nm );
			Smp s;
			s.name = nm;
			s.glType = ty;
			if ( !smpOf( s, W, H ) ) {
				const bool sampler = ( ty >= 0x8B5D && ty <= 0x8B64 ) || ( ty >= 0x8DC0 && ty <= 0x8DD8 ) || ( ty >= 0x9008 && ty <= 0x9110 );
				if ( sampler )
					return fatal( "sampler " + s.name + " of a GL type the gate does not make (0x" + std::to_string( ty ) + ")" );
				continue;
			}
			s.unit = int( smp.size() ) + 1;
			smp.push_back( s );
		}
	}
	{
		// the twins' loose fields GL's program has no location for (compiled out of the GLSL, read by the HLSL)
		std::string miss;
		int nMiss = 0;
		for ( int k = 0; k < tab.psN; k++ )
			if ( f.getUniformLocation( pWater, tab.ps[k].name ) < 0 ) {
				nMiss++;
				miss += std::string( " " ) + tab.ps[k].name;
			}
		if ( tab.tess ) {
			for ( const WwVkPresent::FxField & fd : WwVkPresent::kT_waterHs )
				if ( f.getUniformLocation( pWater, fd.name ) < 0 ) {
					nMiss++;
					miss += std::string( " hs:" ) + fd.name;
				}
			for ( const WwVkPresent::FxField & fd : WwVkPresent::kT_waterDs )
				if ( f.getUniformLocation( pWater, fd.name ) < 0 ) {
					nMiss++;
					miss += std::string( " ds:" ) + fd.name;
				}
		}
		for ( const WwVkPresent::FxField & fd : WwVkPresent::kT_waterVs )
			if ( f.getUniformLocation( pWater, fd.name ) < 0 ) {
				nMiss++;
				miss += std::string( " vs:" ) + fd.name;
			}
		std::printf( "WATERDRAW %d twin fields GL has no location for:%s\n", nMiss, miss.c_str() );
	}
	std::printf( "WATERDRAW %zu samplers active in GL's program:", smp.size() );
	for ( const Smp & s : smp )
		std::printf( " %s", s.name.c_str() );
	std::printf( "\n" );

	// ---- the blocks: globalUniforms (the frame's camera), wwShadowFrame (seeded), skinningUniforms (zero)
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
	for ( int k = 0; k < 3; k++ ) {
		const GLuint bi = f.getUniformBlockIndex( pWater, blockName[k] );
		if ( bi != 0xFFFFFFFFu ) {
			f.uniformBlockBinding( pWater, bi, GLuint( k ) );
			f.getActiveUniformBlockiv( pWater, bi, fBlockSize, &blockSize[k] );
		}
		std::vector<std::uint8_t> z( size_t( std::max( blockSize[k], 16 ) ), 0 );
		f.bindBuffer( fUbo, ubo[k] );
		f.bufferData( fUbo, std::ptrdiff_t( z.size() ), z.data(), fStatic );
	}
	std::printf( "WATERDRAW blocks: globalUniforms %d, wwShadowFrame %d, skinningUniforms %d bytes\n", blockSize[0], blockSize[1], blockSize[2] );
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
				putF( at, u01( rng ) );  // the other lights, diffuse, ambient
		for ( size_t at = 272; at < 288; at += 4 )
			putF( at, u01( rng ) );  // toneMapScale, brightnessScale, glowScale, glowScaleSRGB
		const GLint vpd[4] = { 0, 0, W, H };
		std::memcpy( &g[288], vpd, 16 );
		putF( 320, u01( rng ) );
		putF( 324, u01( rng ) );
		f.bindBuffer( fUbo, ubo[0] );
		ex.bufferSubData( fUbo, 0, std::ptrdiff_t( g.size() ), g.data() );
		std::vector<float> sh( size_t( blockSize[1] ) / 4 );
		for ( auto & x : sh )
			x = u01( rng );
		f.bindBuffer( fUbo, ubo[1] );
		ex.bufferSubData( fUbo, 0, std::ptrdiff_t( sh.size() * 4 ), sh.data() );
	};

	// ---- the shape: a wavy grid in view space below the eye, a third of its rows wound clockwise
	const int GX = 48, GZ = 48;
	std::vector<float> pos, col, nor, tan, bit, uv;
	for ( int j = 0; j < GZ; j++ )
		for ( int i = 0; i < GX; i++ ) {
			const float x = -900.0f + 1800.0f * float( i ) / float( GX - 1 ), z = -150.0f - 2350.0f * float( j ) / float( GZ - 1 );
			const float y = -120.0f + 30.0f * std::sin( x * 0.011f ) * std::cos( z * 0.013f );
			const float dx = 30.0f * 0.011f * std::cos( x * 0.011f ) * std::cos( z * 0.013f );
			const float dz = -30.0f * 0.013f * std::sin( x * 0.011f ) * std::sin( z * 0.013f );
			float nn[3] = { -dx, 1.0f, -dz };
			const float nl = std::sqrt( nn[0] * nn[0] + nn[1] * nn[1] + nn[2] * nn[2] );
			pos.insert( pos.end(), { x, y, z } );
			col.insert( col.end(), { 0.5f + 0.5f * float( i ) / GX, 0.7f, 0.5f + 0.5f * float( j ) / GZ, 1.0f } );
			nor.insert( nor.end(), { nn[0] / nl, nn[1] / nl, nn[2] / nl } );
			tan.insert( tan.end(), { 1.0f, dx, 0.0f } );
			bit.insert( bit.end(), { 0.0f, -dz, -1.0f } );
			uv.insert( uv.end(), { x / 300.0f, z / 300.0f } );
		}
	std::vector<std::uint16_t> idx( 6, 0 );  // seed 2 starts past these (an element offset)
	for ( int j = 0; j + 1 < GZ; j++ )
		for ( int i = 0; i + 1 < GX; i++ ) {
			const std::uint16_t a = std::uint16_t( j * GX + i ), b = std::uint16_t( a + 1 ), c = std::uint16_t( a + GX ),
				d = std::uint16_t( c + 1 );
			if ( j % 3 == 1 )
				idx.insert( idx.end(), { a, c, b, b, c, d } );  // clockwise seen from above: culled
			else
				idx.insert( idx.end(), { a, b, c, b, d, c } );
		}
	GLuint vao = 0, vb[6] = {}, eb = 0;
	ex.genVertexArrays( 1, &vao );
	ex.bindVertexArray( vao );
	f.genBuffers( 6, vb );
	const std::vector<float> * arr[6] = { &pos, &col, &nor, &tan, &bit, &uv };
	const int loc[6] = { 0, 1, 2, 3, 4, 7 }, comps[6] = { 3, 4, 3, 3, 3, 2 };
	for ( int k = 0; k < 6; k++ ) {
		f.bindBuffer( 0x8892, vb[k] );
		f.bufferData( 0x8892, std::ptrdiff_t( arr[k]->size() * 4 ), arr[k]->data(), fStatic );
		ex.vertexAttribPointer( GLuint( loc[k] ), comps[k], GL_FLOAT, GL_FALSE, 0, nullptr );
		ex.enableVertexAttribArray( GLuint( loc[k] ) );
	}
	f.genBuffers( 1, &eb );
	f.bindBuffer( 0x8893, eb );  // GL_ELEMENT_ARRAY_BUFFER (the vertex array's)
	f.bufferData( 0x8893, std::ptrdiff_t( idx.size() * 2 ), idx.data(), fStatic );
	ex.bindVertexArray( 0 );
	const int triIdx = int( idx.size() ) - 6;

	// the slab's depth: the grid's middle distance
	const float zm = -900.0f, zd = ( ( proj[10] * zm + proj[14] ) / -zm ) * 0.5f + 0.5f;
	std::printf( "WATERDRAW job: %dx%d x%d, %d vertices, %d triangles, slab depth %.6f\n", W, H, S, GX * GZ, triIdx / 3, zd );

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
		glTexParameteri( fT2DMS, 0x90EA, GL_DEPTH_COMPONENT );  // DEPTH_STENCIL_TEXTURE_MODE
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
	// cellwater.cpp's state: opaque, depth LEQUAL written; the shape's back-face cull
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
		if ( scissor ) {
			glEnable( GL_SCISSOR_TEST );
			glScissor( W / 5, H / 6, W / 2, H / 2 );
		}
		f.useProgram( pWater );
		for ( int k = 0; k < 3; k++ )
			f.bindBufferBase( fUbo, GLuint( k ), ubo[k] );
		for ( const Smp & s : smp ) {
			bindUnit( s.unit, targetOf( s.kind ), s.tex );
			f.uniform1i( f.getUniformLocation( pWater, s.name.c_str() ), s.unit );
		}
		ex.bindVertexArray( vao );
	};
	auto stateEnd = [&]() {
		ex.bindVertexArray( 0 );
		for ( const Smp & s : smp )
			bindUnit( s.unit, targetOf( s.kind ), 0 );
		drawState0();
		glDepthFunc( GL_LESS );
	};

	static char twinKeys[128];
	// lane VKWATER4 (all-on census): GL's inputs the twin has no slot for (waterUnslotted) must be none (argv 3 adds
	// defines, e.g. the all-on RT set, to GL's program). Red: a sampler and a loose uniform the twin never declares added
	// to the census; waterRun must refuse with exactly waterUnslotted's text naming both, never draw.
	bool unslottedOk = true;
	{
		before( fbFrame );
		drawState( fbFrame, false );
		WwVkPresent::WaterRun r;
		r.variant = variant;
		WwVkPresent::waterStateOf( &gw, pWater, r );
		r.w = W;
		r.h = H;
		r.color = fc;
		r.idxOff = 12;
		r.count = triIdx;
		r.key = reinterpret_cast<std::uintptr_t>( twinKeys );
		const std::string un = WwVkPresent::waterUnslotted( variant, r.census );
		std::printf( "WATERDRAW unslotted: %s\n", un.empty() ? "none" : un.c_str() );
		WwVkPresent::WaterSlot fake;
		fake.name = "wwUnslottedRedTex";
		fake.kind = 0;
		r.census.inputs.push_back( fake );
		r.census.looseNames.push_back( "wwUnslottedRed[0]" );
		const std::string unR = WwVkPresent::waterUnslotted( variant, r.census );
		const bool ranR = WwVkPresent::waterRun( r, why );
		stateEnd();
		const std::string tail = " reads 2 inputs its Vulkan twin has no slot for: wwUnslottedRedTex wwUnslottedRed";
		const bool redOk = !ranR && why == unR && unR.size() >= tail.size() && unR.compare( unR.size() - tail.size(), tail.size(), tail ) == 0;
		std::printf( "WATERDRAW unslotted red: drew %d, refusal: %s -> %s\n", int( ranR ), why.c_str(),
			redOk ? "red refused as it must" : "red NOT refused by name: the check is blind" );
		unslottedOk = un.empty() && redOk;
	}
	// GL's draw and the twin's into the same frame contents, no scissor, judged against the frame before the water
	auto pairRun = [&]( const std::vector<std::uint16_t> & bgC, const std::vector<float> & bgD, long long idxOff, int count, bool & ran ) {
		const bool bsc = std::getenv( "WATERDRAW_BISSCI" ) != nullptr;	// diagnostic only: the bisect through the scissor
		before( fbRef );
		drawState( fbRef, bsc );
		glDrawElements( drawMode, count, GL_UNSIGNED_SHORT, reinterpret_cast<const void *>( std::uintptr_t( idxOff ) ) );
		stateEnd();
		const std::vector<std::uint16_t> gC = readColor( refCol );
		const std::vector<float> gD = readDepth( refDs, false );
		before( fbFrame );
		drawState( fbFrame, bsc );
		WwVkPresent::WaterRun r;
		r.variant = variant;
		WwVkPresent::waterStateOf( &gw, pWater, r );
		r.w = W;
		r.h = H;
		r.color = fc;
		r.idxOff = idxOff;
		r.count = count;
		r.key = reinterpret_cast<std::uintptr_t>( twinKeys );
		if ( const char * br = std::getenv( "WATERDRAW_BISRED" ) )	// diagnostic only: the bisect under a red
			r.red = std::atoi( br );
		ran = WwVkPresent::waterRun( r, why );
		stateEnd();
		if ( !ran )
			return Verdict();
		const std::vector<std::uint16_t> kC = readColor( fc );
		const std::vector<float> kD = readDepth( fds, true );
		return judge( bgC, bgD, kC, kD, gC, gD );
	};
	bool gateOk = true;
	int runs = 0;
	int forcedHits = 0, forcedMisses = 0, forcedFails = 0;  // WW_WATERDRAW_RAYREUSE: twin back at its old handle / not / remake failed
	for ( int seed = 1; seed <= ( tab.tess ? 3 : 2 ); seed++ ) {
		std::mt19937 rng( 20261113u + std::uint32_t( seed ) * 7919u );
		for ( Smp & s : smp )
			if ( !fill( s, rng ) )
				return fatal( "texture " + s.name );
		const int nu = seedUnis( pWater, rng );
		/* piece 2: the seed's switch profile, the app's (seedUnis leaves every int 0; random ints switched debug views and
		 * the far mask on, a coin per variant). Every seed: the water look rows Ultra draws. Seed 2: + the air (fog, the
		 * volume). Seed 3: + the sea (below). Each profile is judged by reds that must FAIL on it */
		{
			f.useProgram( pWater );
			std::vector<const char *> on = { "waterHaveDepth", "waterDepthOn", "waterClearOn", "waterReflOn", "waterSsrOn",
				"waterSkyOn", "waterSkyVisOn", "waterShoreOn", "waterShoreLaw", "waterShoreEdgeLaw", "waterFlowOn", "waterGlintOn",
				"waterRiverFoamOn", "waterCauOn", "waterCauShadow", "waterContactOn", "waterWavesOn", "waterSurfOn", "cellIsLinear" };
			if ( seed == 2 )
				on.insert( on.end(), { "fogOn", "volOn", "volAirFlags" } );
			const GLint one = 1;
			for ( const char * nm : on )
				if ( f.getUniformLocation( pWater, nm ) >= 0 )
					f.uniform1iv( f.getUniformLocation( pWater, nm ), 1, &one );
			auto set4 = [&]( const char * nm, const float * v ) {
				const GLint l = f.getUniformLocation( pWater, nm );
				if ( l >= 0 )
					f.uniform4fv( l, 1, v );
			};
			/* the record's distances app-sized (esmwater.h): the noise layers' uv scale (world units a tile; the seeded
			 * 0..1 tiled them hundreds of times a texel over the grid 150..2500 u away) and params1's depth amount (.w:
			 * 0..1 put every scene point past it, rz 0, and half the draws flattened the normal to (0, 0, 1) by params2's
			 * order: no texture reached the colour, red 2 blind); params1.y the record's 0.5 */
			if ( const GLint l = f.getUniformLocation( pWater, "waterMat[12]" ); l >= 0 ) {
				GLfloat uv[4] = {};
				gw.glGetUniformfv( pWater, l, uv );
				for ( int k = 0; k < 3; k++ )
					uv[k] = 200.0f + 800.0f * uv[k];
				f.uniform4fv( l, 1, uv );
			}
			if ( const GLint l = f.getUniformLocation( pWater, "waterMat[7]" ); l >= 0 ) {
				GLfloat p1[4] = {};
				gw.glGetUniformfv( pWater, l, p1 );
				p1[1] = 0.5f;
				p1[3] = 250.0f + 1500.0f * p1[3];	// the records' p10 .. p90 (290 .. 1739; notes waterdepth1 watr.tsv, 42 WATRs)
				f.uniform4fv( l, 1, p1 );
			}
			/* params3 / params4 as every one of those records holds them: alpha and colour shallow range 1, deep range
			 * 0.72 .. 0.99, shallow alpha 0, deep alpha 1 (the seeded ranges, inverted or crossed, put the alpha and
			 * colour steps where the records never do) */
			if ( const GLint l = f.getUniformLocation( pWater, "waterMat[9]" ); l >= 0 ) {
				GLfloat p3[4] = {};
				gw.glGetUniformfv( pWater, l, p3 );
				const GLfloat v[4] = { 1.0f, 0.72f + 0.27f * p3[1], 0.0f, 1.0f };
				f.uniform4fv( l, 1, v );
			}
			if ( const GLint l = f.getUniformLocation( pWater, "waterMat[10]" ); l >= 0 ) {
				GLfloat p4[4] = {};
				gw.glGetUniformfv( pWater, l, p4 );
				p4[0] = 1.0f;
				p4[1] = 0.72f + 0.27f * p4[1];
				f.uniform4fv( l, 1, p4 );
			}
			/* params2 in the records' ranges (notes vkwater3 watr_normals.tsv, the same 42 WATRs): surface effect falloff
			 * 0.9 .. 1, normal magnitude 0.15 .. 0.84, shallow normal falloff 1 on every record, deep 0.75 .. 0.999. The
			 * seeded ones faded shallow water to the scene (base seed 2 draw 5) or kept no normal over it (m seed 2 draw 3,
			 * shallow falloff under 1; d seed 2 draw 2, the pair inverted): no texture reached the colour, red 2 blind */
			if ( const GLint l = f.getUniformLocation( pWater, "waterMat[8]" ); l >= 0 ) {
				GLfloat p2[4] = {};
				gw.glGetUniformfv( pWater, l, p2 );
				p2[0] = 0.9f + 0.1f * p2[0];
				p2[1] = 0.15f + 0.7f * p2[1];
				p2[2] = 1.0f;
				p2[3] = 0.75f + 0.249f * p2[3];
				f.uniform4fv( l, 1, p2 );
			}
			if ( seed == 2 ) {
				/* the air's volume laid out as the app packs it (seeded 0..1 texel counts and slice counts put clamp( x,
				 * lo, hi ) at lo > hi, where GL and Vulkan order min / max differently): the near grid alone over the 16 x
				 * 16 x 8 volume, 64 u to 20 000 u in its 8 slices, the frame's rect, view units = game units, no far grid */
				const float dims[4] = { 16.0f, 16.0f, 8.0f, 8.0f }, pack[4] = { 1.0f, 1.0f, 0.0f, 0.0f };
				const float slices[4] = { 64.0f, std::log( 20000.0f / 64.0f ), 8.0f, 1.0f }, off[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				const float rect[4] = { 0.0f, 0.0f, 1.0f / float( W ), 1.0f / float( H ) };
				set4( "volPackDims", dims );
				set4( "volPack", pack );
				set4( "volSlices", slices );
				set4( "volFar", off );
				set4( "volRect", rect );
				if ( const GLint l = f.getUniformLocation( pWater, "fogDistScale" ); l >= 0 ) {
					const GLfloat s1 = 1.0f;
					f.uniform1fv( l, 1, &s1 );
				}
			}
		}
		/* piece 2: a probe twin (fo4_waterlookp / dp) seeded 0 / 1 drew probe 1, the normal, which reads no texture (red 2
		 * blind). Its look under probe 0 on seeds 1 and 3 (the reds judged there), probe 25 (the light it writes) on seed 2 */
		if ( f.getUniformLocation( pWater, "waterProbe" ) >= 0 ) {
			f.useProgram( pWater );
			const GLint pr = seed == 2 ? 25 : 0;
			f.uniform1iv( f.getUniformLocation( pWater, "waterProbe" ), 1, &pr );
			std::printf( "WATERDRAW seed %d: waterProbe %d\n", seed, int( pr ) );
		}
		// diagnostic only (never in a gate run): WATERDRAW_SETI=name=v[,name=v] sets int uniforms on every seed
		// diagnostic only: WATERDRAW_SETF=<seed>:name.c=v[,...] sets one component of a vec4 (c in xyzw) on that seed
		if ( const char * sf = std::getenv( "WATERDRAW_SETF" ) ) {
			f.useProgram( pWater );
			std::string all( sf );
			for ( size_t a = 0; a < all.size(); ) {
				size_t e = all.find( ',', a );
				if ( e == std::string::npos )
					e = all.size();
				const std::string kv = all.substr( a, e - a );
				const size_t c = kv.find( ':' ), d = kv.find( '.', c == std::string::npos ? 0 : c ), q = kv.find( '=' );
				if ( c != std::string::npos && d != std::string::npos && q != std::string::npos && std::atoi( kv.c_str() ) == seed ) {
					const std::string nm = kv.substr( c + 1, d - c - 1 );
					const int comp = int( std::string( "xyzw" ).find( kv[d + 1] ) );
					const GLint l = f.getUniformLocation( pWater, nm.c_str() );
					GLfloat v[16] = {};
					if ( l >= 0 && comp >= 0 && comp < 4 ) {
						gw.glGetUniformfv( pWater, l, v );
						v[comp] = float( std::atof( kv.c_str() + q + 1 ) );
						f.uniform4fv( l, 1, v );
					}
					std::printf( "WATERDRAW diagnostic: %s.%c = %g%s\n", nm.c_str(), kv[d + 1], double( v[comp < 0 ? 0 : comp] ),
						l >= 0 ? "" : " (no location)" );
				}
				a = e + 1;
			}
		}
		if ( const char * si = std::getenv( "WATERDRAW_SETI" ) ) {
			f.useProgram( pWater );
			std::string all( si );
			for ( size_t a = 0; a < all.size(); ) {
				size_t e = all.find( ',', a );
				if ( e == std::string::npos )
					e = all.size();
				const std::string kv = all.substr( a, e - a );
				const size_t q = kv.find( '=' );
				if ( q != std::string::npos ) {
					const GLint v = GLint( std::atoi( kv.c_str() + q + 1 ) );
					const GLint l = f.getUniformLocation( pWater, kv.substr( 0, q ).c_str() );
					if ( l >= 0 )
						f.uniform1iv( l, 1, &v );
					std::printf( "WATERDRAW diagnostic: %s = %d%s\n", kv.substr( 0, q ).c_str(), int( v ), l >= 0 ? "" : " (no location)" );
				}
				a = e + 1;
			}
		}
		if ( seed == 3 ) {
			/* the info zero (integer inputs) names no body: wwOcBody finds no set and the ds adds nothing. One sea body
			 * over the whole lookup: every index tile uniform with word body 1 | open-water shore and depth codes, body
			 * 1's row a sea (class 0, no flags) of a large area */
			const std::uint32_t word = 1u | ( 255u << 16 ) | ( 255u << 24 );
			const float sea[4] = { 0.0f, 0.0f, 0.0f, 1.0e8f };
			for ( const Smp & s : smp ) {
				if ( s.name == "waterInfoIndex" ) {
					std::vector<std::uint32_t> d( size_t( s.w ) * size_t( s.h ) * 2, 0u );
					for ( size_t i = 1; i < d.size(); i += 2 )
						d[i] = word;
					glBindTexture( GL_TEXTURE_2D, s.tex );
					glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, s.w, s.h, 0x8228, GL_UNSIGNED_INT, d.data() );
				} else if ( s.name == "waterInfoBodies" ) {
					glBindTexture( GL_TEXTURE_2D, s.tex );
					glTexSubImage2D( GL_TEXTURE_2D, 0, 1, 0, 1, 1, GL_RGBA, GL_FLOAT, sea );
				}
			}
			glBindTexture( GL_TEXTURE_2D, 0 );
			if ( glGetError() != GL_NO_ERROR )
				return fatal( "seed 3's sea body" );
			/* seed 3, the tessellated twin's own: the seeded 0.05..1 leaves the grid (150..2500 u away) past the fade: no
			 * split, no displacement, a blind ds (red 32 EXACT on seed 1). The ocean on, full out to 3000 u, none past 6000, an 8 px edge, a 4096 split cap: the hs splits (fractional
			 * odd), the ds moves the vertices */
			f.useProgram( pWater );
			const GLint one = 1;
			const float disp[4] = { 3000.0f, 6000.0f, 8.0f, 4096.0f };
			for ( const char * on : { "waterOceanOn", "waterInfoOn" } )
				if ( f.getUniformLocation( pWater, on ) >= 0 )
					f.uniform1iv( f.getUniformLocation( pWater, on ), 1, &one );
			if ( f.getUniformLocation( pWater, "waterOceanDisp" ) >= 0 )
				f.uniform4fv( f.getUniformLocation( pWater, "waterOceanDisp" ), 1, disp );
			/* a sea the size of the app's: the seeded 0.05..1 made 1 / L tiles of a metre and a seam fade over the
			 * grid, a chop that threw seed 1's grid out of view on both sides. A unit wind of 8 m/s, tiles of 50, 12
			 * and 3 m, gains of 1, info texels of 64 u, the seam off */
			const float wind[4] = { 1.0f, 0.0f, 8.0f, 0.1f }, grid[4] = { -4096.0f, -4096.0f, 64.0f, 64.0f };
			const float lay[4] = { 8.0f, 8.0f, 16.0f, 64.0f }, seam[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			const float invL[8] = { 0.02f, 0.08f, 0.33f, 1.0f, 0.02f, 0.08f, 0.33f, 1.0f };
			const float set[8] = { 1.0f, 1.0f, 0.2f, 0.01f, 1.0f, 1.0f, 0.2f, 0.01f };
			const float setB[8] = { 1000.0f, 1.0f, 64.0f, 0.0f, 1000.0f, 1.0f, 64.0f, 0.0f };
			const struct { const char * nm; const float * v; int n; } sane[] = { { "waterOceanWind", wind, 1 },
				{ "waterInfoGrid", grid, 1 }, { "waterInfoLayout", lay, 1 }, { "waterSeam", seam, 1 },
				{ "waterOceanInvL", invL, 2 }, { "waterOceanSet", set, 2 }, { "waterOceanSetB", setB, 2 } };
			for ( const auto & s : sane )
				if ( f.getUniformLocation( pWater, s.nm ) >= 0 )
					f.uniform4fv( f.getUniformLocation( pWater, s.nm ), s.n, s.v );
			// the info on: the seeded far mask discards every sample the random index calls dry (seed 1 covered 0)
			const GLint zero = 0;
			if ( f.getUniformLocation( pWater, "waterFarMask" ) >= 0 )
				f.uniform1iv( f.getUniformLocation( pWater, "waterFarMask" ), 1, &zero );
			// diagnostic only (never in a gate run): WATERDRAW_OCRED=<n> sets the shaders' own waterOceanRed on seed 3,
			// WATERDRAW_OCPX=<px> the split's target edge
			if ( const char * oc = std::getenv( "WATERDRAW_OCRED" ) ) {
				const GLint v = GLint( std::atoi( oc ) );
				if ( f.getUniformLocation( pWater, "waterOceanRed" ) >= 0 )
					f.uniform1iv( f.getUniformLocation( pWater, "waterOceanRed" ), 1, &v );
				std::printf( "WATERDRAW diagnostic: waterOceanRed %d\n", int( v ) );
			}
			if ( const char * px = std::getenv( "WATERDRAW_OCPX" ) ) {
				const float d2[4] = { disp[0], disp[1], float( std::atof( px ) ), disp[3] };
				f.uniform4fv( f.getUniformLocation( pWater, "waterOceanDisp" ), 1, d2 );
				std::printf( "WATERDRAW diagnostic: target edge %g px\n", double( d2[2] ) );
			}
		}
		blocks( rng );
		const bool scissor = seed == 2;
		const long long idxOff = 12;  // past the 6 leading dummy indices: an element offset on both seeds
		const int count = triIdx;
		std::printf( "WATERDRAW seed %d: %d loose uniforms seeded%s\n", seed, nu, scissor ? ", a scissor" : "" );

		before( fbRef );
		const std::vector<std::uint16_t> bgC = readColor( refCol );
		const std::vector<float> bgD = readDepth( refDs, false );
		before( fbRef );
		drawState( fbRef, scissor );
		glDrawElements( drawMode, count, GL_UNSIGNED_SHORT, reinterpret_cast<const void *>( std::uintptr_t( idxOff ) ) );
		stateEnd();
		const GLenum ge = glGetError();
		if ( ge != GL_NO_ERROR )
			return fatal( "the GL reference draw: GL error " + std::to_string( ge ) );
		const std::vector<std::uint16_t> glC = readColor( refCol );
		const std::vector<float> glD = readDepth( refDs, false );

		for ( int red : { 0, 1, 2, 4, 8, 16, 32 } ) {
			/* 16 / 32: the tessellated stages' reds. Red 32 on seed 3 only (the sea displaced; seed 1's ds a pass-through,
			 * red 32 EXACT there). Piece 2: every profile proven, seed 1 all but 32, seed 2 (the air) 1 and 2, seed 3 2 and 32 */
			const bool judged = seed == 1 ? red != 32 : seed == 2 ? red <= 2 : ( red == 0 || red == 2 || red == 32 );
			if ( !judged || ( red >= 16 && !tab.tess ) )
				continue;
			before( fbFrame );
			drawState( fbFrame, scissor );
			WwVkPresent::WaterRun r;
			r.variant = variant;
			WwVkPresent::waterStateOf( &gw, pWater, r );
			r.w = W;
			r.h = H;
			r.color = fc;
			r.idxOff = idxOff;
			r.count = count;
			r.baseVertex = 0;
			r.key = reinterpret_cast<std::uintptr_t>( twinKeys );
			r.red = red;
			const bool ran = WwVkPresent::waterRun( r, why );
			stateEnd();
			runs++;
			if ( !ran ) {
				std::printf( "WATERDRAW seed %d red %d: REFUSED (%s)\n", seed, red, why.c_str() );
				gateOk = false;
				continue;
			}
			const std::vector<std::uint16_t> vkC = readColor( fc );
			const std::vector<float> vkD = readDepth( fds, true );
			const Verdict v = judge( bgC, bgD, vkC, vkD, glC, glD );
			const char * word = v.word();
			const bool fail = std::strcmp( word, "FAIL" ) == 0;
			std::printf( "WATERDRAW seed %d red %d: %s covered %zu (both %zu, one side only %zu), depth max %ld D24 units (%zu past 1), "
				"colour %zu halves differ, %zu samples past 1e-2 (max rel %.3g)%s\n", seed, red, word, v.covered, v.both, v.onlyOne,
				v.maxDepthU, v.depthPast, v.halvesDiff, v.colPast, v.maxRel, red ? ( fail ? " -> red FAILS as it must" : " -> red PASSES: the gate is blind" ) : "" );
			if ( !red && v.colPast )
				where( vkC, glC, W, H, S );
			if ( red ? !fail : fail )
				gateOk = false;
		}
		/* lane VKWATER4 (G5): the ocean's arrays shared in place (seed 3, the sea on). waterOceanB / C / D as shared targets
		 * (vkwater.inc's farSharedOf branch: no waterTwin copy) EXACT against the same draw through the copy route; ocean
		 * copies 3 there, 0 shared. Red prev: the other shared set (other contents, the frame before's) FAILS against this
		 * draw and is EXACT against the copy route drawn from those other contents */
		if ( seed == 3 ) {
			auto vkDraw = [&]( std::vector<std::uint16_t> & C, std::vector<float> & D, WwVkPresent::OceanWaterStat & st, const char * what ) {
				WwVkPresent::takeOceanWaterStat();
				before( fbFrame );
				drawState( fbFrame, scissor );
				WwVkPresent::WaterRun r;
				r.variant = variant;
				WwVkPresent::waterStateOf( &gw, pWater, r );
				r.w = W;
				r.h = H;
				r.color = fc;
				r.idxOff = idxOff;
				r.count = count;
				r.baseVertex = 0;
				r.key = reinterpret_cast<std::uintptr_t>( twinKeys );
				const bool ran = WwVkPresent::waterRun( r, why );
				stateEnd();
				runs++;
				st = WwVkPresent::takeOceanWaterStat();
				if ( ran ) {
					C = readColor( fc );
					D = readDepth( fds, true );
				} else
					std::printf( "WATERDRAW shared %s: REFUSED (%s)\n", what, why.c_str() );
				return ran;
			};
			std::vector<Smp *> oc;
			for ( Smp & s : smp )
				if ( s.name.compare( 0, 10, "waterOcean" ) == 0 && s.kind == 2 )
					oc.push_back( &s );
			GLuint orig[3] = {}, prev[3] = {}, sh[2][3] = {};
			bool ok6 = oc.size() == 3;
			std::mt19937 rp( 20261115u );
			for ( size_t k = 0; ok6 && k < 3; k++ ) {
				Smp & s = *oc[k];
				orig[k] = s.tex;
				Smp p = s;
				p.tex = 0;
				ok6 = fill( p, rp );
				prev[k] = p.tex;
				for ( int set = 0; ok6 && set < 2; set++ ) {
					unsigned t = 0;
					ok6 = WwVkPresent::sharedTarget( 0x57440000u + std::uintptr_t( set * 3 ) + k, 2, s.w, s.h, s.d, s.fmt, t, why, s.levels );
					if ( !ok6 )
						break;
					sh[set][k] = t;
					const unsigned tg = targetOf( s.kind );
					glBindTexture( tg, t );
					glTexParameteri( tg, GL_TEXTURE_MIN_FILTER, s.levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR );
					glTexParameteri( tg, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
					glTexParameteri( tg, GL_TEXTURE_WRAP_S, s.levels > 1 ? GL_REPEAT : 0x812F );
					glTexParameteri( tg, GL_TEXTURE_WRAP_T, s.levels > 1 ? GL_REPEAT : 0x812F );
					glTexParameteri( tg, 0x8072, 0x812F );
					glTexParameteri( tg, fMaxLevel, s.levels - 1 );
					glBindTexture( tg, 0 );
					for ( int l = 0; l < s.levels; l++ )
						f.copyImageSubData( set ? prev[k] : orig[k], tg, l, 0, 0, 0, t, tg, l, 0, 0, 0, std::max( s.w >> l, 1 ), std::max( s.h >> l, 1 ), s.d );
				}
			}
			ok6 = ok6 && glGetError() == GL_NO_ERROR;
			if ( !ok6 ) {
				std::printf( "WATERDRAW shared: could not make the shared ocean arrays (%zu found; %s)\n", oc.size(), why.c_str() );
				gateOk = false;
			} else {
				std::vector<std::uint16_t> c0, c1, cp, cr;
				std::vector<float> d0, d1, dp, dr;
				WwVkPresent::OceanWaterStat s0, s1, sp, sr;
				auto use = [&]( const GLuint * t ) {
					for ( size_t k = 0; k < 3; k++ )
						oc[k]->tex = t[k];
				};
				bool ran = vkDraw( c0, d0, s0, "copy route" );
				use( sh[0] );
				ran = vkDraw( c1, d1, s1, "in place" ) && ran;
				use( prev );
				ran = vkDraw( cp, dp, sp, "copy route, the other contents" ) && ran;
				use( sh[1] );
				ran = vkDraw( cr, dr, sr, "RED prev" ) && ran;
				use( orig );
				if ( !ran )
					gateOk = false;
				else {
					const Verdict v = judge( bgC, bgD, c1, d1, c0, d0 ), vr = judge( bgC, bgD, cr, dr, c0, d0 ), vp = judge( bgC, bgD, cr, dr, cp, dp );
					std::printf( "WATERDRAW shared in place: %s against the copy route, covered %zu (one side only %zu), %zu halves and %zu depths "
						"differ; ocean copies %d / shared %d in place, copy route %d / %d\n", v.word(), v.covered, v.onlyOne, v.halvesDiff,
						v.depthDiff, s1.copies, s1.shared, s0.copies, s0.shared );
					std::printf( "WATERDRAW shared RED prev: %s against this draw (%zu samples past 1e-2) -> %s; against the copy route of "
						"the other contents %s (%zu halves, %zu depths differ)\n", vr.word(), vr.colPast,
						std::strcmp( vr.word(), "FAIL" ) == 0 ? "red FAILS as it must" : "red PASSES: the gate is blind", vp.word(),
						vp.halvesDiff, vp.depthDiff );
					const bool good = std::strcmp( v.word(), "EXACT" ) == 0 && s1.copies == 0 && s1.shared == 3 && s0.copies == 3
						&& s0.shared == 0 && std::strcmp( vr.word(), "FAIL" ) == 0 && std::strcmp( vp.word(), "EXACT" ) == 0;
					if ( !good )
						gateOk = false;
				}
			}
			for ( int set = 0; set < 2; set++ )
				for ( size_t k = 0; k < 3; k++ )
					if ( sh[set][k] )
						WwVkPresent::sharedTargetDrop( 0x57440000u + std::uintptr_t( set * 3 ) + k );
			for ( GLuint & t : prev )
				if ( t )
					glDeleteTextures( 1, &t );
		}
		// diagnostic (WATERDRAW_BISECT=<seed>): each seeded loose scalar set to 0 alone, both sides redrawn; the ones that
		// move the colour misses are printed. Never part of the verdict.
		const char * bis = std::getenv( "WATERDRAW_BISECT" );
		if ( bis && *bis == char( '0' + seed ) ) {
			GLint n = 0;
			f.getProgramiv( pWater, fActiveUnis, &n );
			size_t base = 0;
			for ( GLint i = -1; i < n; i++ ) {
				char nm[256] = {};
				GLint sz = 0;
				GLenum ty = 0;
				GLint l = -1;
				GLint iv = 0;
				GLfloat fv = 0.0f;
				const bool isInt = false;
				bool asInt = isInt;
				if ( i >= 0 ) {
					f.getActiveUniform( pWater, GLuint( i ), 256, nullptr, &sz, &ty, nm );
					const GLuint idx = GLuint( i );
					GLint bi = -1;
					f.getActiveUniformsiv( pWater, 1, &idx, fBlockIndex, &bi );
					if ( bi >= 0 || sz != 1 || ( ty != 0x1404 && ty != 0x8B56 && ty != 0x1406 ) )
						continue;
					asInt = ty != 0x1406;
					l = f.getUniformLocation( pWater, nm );
					f.useProgram( pWater );
					if ( asInt ) {
						gw.glGetUniformiv( pWater, l, &iv );
						if ( !iv )
							continue;
						const GLint z = 0;
						f.uniform1iv( l, 1, &z );
					} else {
						gw.glGetUniformfv( pWater, l, &fv );
						const GLfloat z = 0.0f;
						f.uniform1fv( l, 1, &z );
					}
				}
				bool ran = false;
				const Verdict pv = pairRun( bgC, bgD, idxOff, count, ran );
				const size_t past = ran ? pv.colPast : size_t( -1 );
				if ( i < 0 )
					base = past;
				else if ( past != base )
					std::printf( "WATERDRAW bisect: %s (%s %g) = 0 -> %zu samples past (base %zu)\n", nm, asInt ? "int" : "float",
						asInt ? double( iv ) : double( fv ), past, base );
				if ( i >= 0 ) {
					f.useProgram( pWater );
					if ( asInt )
						f.uniform1iv( l, 1, &iv );
					else
						f.uniform1fv( l, 1, &fv );
				}
			}
			std::printf( "WATERDRAW bisect done (base %zu)\n", base );
		}
		// the app's anisotropy (16 on the noise / moments), seed 1 once more: logged as the known driver difference, never judged
		if ( seed == 1 ) {
			auto setAniso = [&]( bool app ) {
				for ( const Smp & s : smp )
					if ( s.appAniso > 1.0f ) {
						glBindTexture( targetOf( s.kind ), s.tex );
						glTexParameterf( targetOf( s.kind ), 0x84FE, app ? s.appAniso : 1.0f );
						glBindTexture( targetOf( s.kind ), 0 );
					}
			};
			setAniso( true );
			bool ran = false;
			const Verdict av = pairRun( bgC, bgD, idxOff, count, ran );
			setAniso( false );
			runs++;
			if ( ran )
				std::printf( "WATERDRAW seed 1 app anisotropy (known driver difference, not judged): covered %zu (one side only %zu), depth max %ld D24 "
					"units, %zu samples past 1e-2 (max rel %.3g)\n", av.covered, av.onlyOne, av.maxDepthU, av.colPast, av.maxRel );
			else
				std::printf( "WATERDRAW seed 1 app anisotropy: REFUSED (%s)\n", why.c_str() );
		}
	}
	/* lane VKWATER4: the SSR water ray pass (cellssr.cpp step 1b), m twin only (the app's ray twin), on the last seed's
	 * data: waterSslr 1, an RGBA32F 1-sample shared target of its own size (200x120, not the tone frame's) over a D24S8
	 * renderbuffer holding an occluder (the slab, left half), depth LEQUAL with the mask off, colour cleared to 0. GL draws
	 * its own copy; the twin draws against waterRayDepth's copy of the renderbuffer. Same bar per pixel (covered = colour
	 * not the clear on either side; no depth: the pass writes none). Reds 1 / 2 / 4 / 8 must FAIL. Run twice: D24S8, then
	 * D32F_S8 ("ray d32fs8" lines). */
	if ( variant == WwVkPresent::kWaterLookM ) {
		const int WR = 200, HR = 120;
		auto rayFbo = [&]( GLuint col, GLuint rb ) {
			GLuint b = 0;
			f.genFramebuffers( 1, &b );
			f.bindFramebuffer( fFb, b );
			f.framebufferTexture2D( fFb, fCol0, GL_TEXTURE_2D, col, 0 );
			ex.framebufferRenderbuffer( fFb, fDsAtt, 0x8D41, rb );
			const GLenum buf = fCol0;
			f.drawBuffers( 1, &buf );
			return f.checkFramebufferStatus( fFb ) == fComplete ? b : 0u;
		};
		auto raySpec = [&]( GLuint rb, GLenum fmt ) {
			ex.bindRenderbuffer( 0x8D41, rb );
			ex.renderbufferStorage( 0x8D41, fmt, WR, HR );
			ex.bindRenderbuffer( 0x8D41, 0 );
		};
		auto rayRb = [&]() {
			GLuint rb = 0;
			ex.genRenderbuffers( 1, &rb );
			raySpec( rb, fD24s8 );
			return rb;
		};
		static char rayKeys[2];
		unsigned rayTex = 0;
		const GLuint refTex = tex2d( WR, HR, fRgba32f, GL_RGBA, GL_FLOAT, nullptr, false ), rbRef = rayRb(), rbVk = rayRb();
		const bool shared = WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &rayKeys[0] ), 0, WR, HR, 1, fRgba32f, rayTex, why );
		const GLuint fbRayRef = rayFbo( refTex, rbRef ), fbRayVk = shared && rayTex ? rayFbo( rayTex, rbVk ) : 0u;
		if ( !fbRayRef || !fbRayVk )
			return fatal( "the ray targets: " + ( shared ? std::string( "a framebuffer incomplete" ) : why ) );
		const GLint sslr = f.getUniformLocation( pWater, "waterSslr" );
		if ( sslr < 0 )
			return fatal( "the m twin's GL program has no waterSslr" );
		auto rayBefore = [&]( GLuint fb ) {
			f.bindFramebuffer( fFb, fb );
			glViewport( 0, 0, WR, HR );
			drawState0();
			glDepthMask( GL_TRUE );
			glStencilMask( 0xFF );
			glClearDepth( 1.0 );
			glClearStencil( 0 );
			glClear( GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
			glEnable( GL_SCISSOR_TEST );
			glScissor( 0, 0, WR / 2, HR );
			glEnable( GL_DEPTH_TEST );
			glDepthFunc( GL_ALWAYS );
			glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
			f.useProgram( pSlab );
			f.uniform1fv( f.getUniformLocation( pSlab, "zd" ), 1, &zd );
			glDrawArrays( GL_TRIANGLES, 0, 3 );
			drawState0();
			glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
			glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
			glClear( GL_COLOR_BUFFER_BIT );
			glDepthFunc( GL_LESS );
		};
		auto rayState = [&]( GLuint fb ) {
			drawState( fb, false );
			glViewport( 0, 0, WR, HR );
			glDepthMask( GL_FALSE );
			const GLint one = 1;
			f.uniform1iv( sslr, 1, &one );
		};
		auto rayRead = [&]( GLuint fb ) {
			std::vector<float> out( size_t( WR ) * HR * 4 );
			f.bindFramebuffer( fFb, fb );
			glReadPixels( 0, 0, WR, HR, GL_RGBA, GL_FLOAT, out.data() );
			return out;
		};
		const long long idxOff = 12;
		const int count = triIdx;
		// WW_WATERDRAW_RAYVIEW_RED=1: every ray run with red 64 (the depth twin's view keyed on its VkImage handle only, the
		// old key): a remade twin at the freed handle value keeps a view of the freed image, and the d32fs8 arm draws nothing
		const char * rvEnv = std::getenv( "WW_WATERDRAW_RAYVIEW_RED" );
		const int rayViewRed = rvEnv && std::atoi( rvEnv ) == 1 ? 64 : 0;
		// WW_WATERDRAW_RAYREUSE=1 (the flake's trigger on every ray run): the renderbuffer flips D24S8 <-> D32F_S8 every
		// run, and before each run waterRayForceReuse remakes the depth twin at the new format until it comes back at the
		// freed handle value. 20 runs (reds 0/1/2/4/8 four times, red 0 twice per format), the twin primed so run 1 is forced
		const char * reEnv = std::getenv( "WW_WATERDRAW_RAYREUSE" );
		const bool forceReuse = reEnv && std::atoi( reEnv ) == 1;
		const GLenum kDsFmt[2] = { GLenum( fD24s8 ), GLenum( 0x8CAD ) };
		auto rayTargets = [&]( GLuint rb, GLuint fb, GLenum dsFmt ) {
			raySpec( rb, dsFmt );
			f.bindFramebuffer( fFb, fb );
			return f.checkFramebufferStatus( fFb ) == fComplete;
		};
		auto rayRef = [&]() {
			rayBefore( fbRayRef );
			rayState( fbRayRef );
			glDrawElements( drawMode, count, GL_UNSIGNED_SHORT, reinterpret_cast<const void *>( std::uintptr_t( idxOff ) ) );
			stateEnd();
			return rayRead( fbRayRef );
		};
		// one ray run: false = the harness cannot go on (its fatal is printed by the caller)
		auto rayOne = [&]( const char * lab, int red, GLenum dsFmt, const std::vector<float> & glR ) {
			{
				rayBefore( fbRayVk );
				std::string forced;
				if ( forceReuse ) {
					const int tries = WwVkPresent::waterRayForceReuse( reinterpret_cast<std::uintptr_t>( &rayKeys[1] ), dsFmt, why );
					forced = tries > 0 ? "; forced reuse: hit after " + std::to_string( tries ) : tries == 0 ? "; forced reuse: miss"
						: tries == -1 ? "; forced reuse: no twin" : "; forced reuse: remake failed (" + why + ")";
					forcedHits += tries > 0;
					forcedMisses += tries == 0;
					forcedFails += tries < 0;
				}
				unsigned dTwin = 0;
				if ( !WwVkPresent::waterRayDepth( reinterpret_cast<std::uintptr_t>( &rayKeys[1] ), rbVk, dsFmt, WR, HR, dTwin, why ) ) {
					std::printf( "WATERDRAW %s red %d: REFUSED (the depth twin: %s)%s\n", lab, red, why.c_str(), forced.c_str() );
					gateOk = false;
					return;
				}
				if ( red == 0 && !forceReuse ) {  // which format ran: the renderbuffer's and its twin's (GL_RENDERBUFFER / TEXTURE_INTERNAL_FORMAT)
					GLint rbFmt = 0, twFmt = 0;
					ex.bindRenderbuffer( 0x8D41, rbVk );
					ex.getRenderbufferParameteriv( 0x8D41, 0x8D44, &rbFmt );
					ex.bindRenderbuffer( 0x8D41, 0 );
					glBindTexture( GL_TEXTURE_2D, dTwin );
					gw.glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, 0x1003, &twFmt );
					glBindTexture( GL_TEXTURE_2D, 0 );
					std::printf( "WATERDRAW %s formats: renderbuffer 0x%X, depth twin 0x%X%s\n", lab, unsigned( rbFmt ), unsigned( twFmt ),
						rayViewRed ? " (red 64 on: the view keyed on its handle only)" : "" );
				}
				rayState( fbRayVk );
				WwVkPresent::WaterRun r;
				r.variant = variant;
				WwVkPresent::waterStateOf( &gw, pWater, r );
				r.w = WR;
				r.h = HR;
				r.color = rayTex;
				r.rayDepth = dTwin;
				r.idxOff = idxOff;
				r.count = count;
				r.key = reinterpret_cast<std::uintptr_t>( twinKeys );
				r.red = red | rayViewRed;
				const auto rt0 = std::chrono::steady_clock::now();
				const bool ran = WwVkPresent::waterRun( r, why );
				const double runMs = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - rt0 ).count();
				stateEnd();
				runs++;
				if ( !ran ) {
					// red 128 (a faked fence timeout) must refuse with the wait named, and the run after it must draw
					const bool timeoutOk = ( red & 128 ) && why.find( "fence wait returned VkResult 2 " ) != std::string::npos;
					std::printf( "WATERDRAW %s red %d: REFUSED (%s)%s%s\n", lab, red, why.c_str(), forced.c_str(),
						timeoutOk ? " -> red refuses as it must" : "" );
					if ( !timeoutOk )
						gateOk = false;
					return;
				}
				const std::vector<float> vkR = rayRead( fbRayVk );
				size_t covered = 0, both = 0, onlyOne = 0, colPast = 0, diff = 0;
				double maxRel = 0.0;
				for ( size_t p = 0; p < size_t( WR ) * HR; p++ ) {
					const float * a = &vkR[p * 4], * b = &glR[p * 4];
					const bool ca = a[0] != 0.0f || a[1] != 0.0f || a[2] != 0.0f || a[3] != 0.0f,
						cb = b[0] != 0.0f || b[1] != 0.0f || b[2] != 0.0f || b[3] != 0.0f;
					if ( !ca && !cb )
						continue;
					covered++;
					if ( ca != cb ) {
						onlyOne++;
						continue;
					}
					both++;
					bool past = false, any = false;
					for ( int c = 0; c < 4; c++ ) {
						if ( std::isnan( a[c] ) && std::isnan( b[c] ) )
							continue;
						const double d = std::fabs( double( a[c] ) - double( b[c] ) ), m = std::max( 1.0, std::fabs( double( b[c] ) ) );
						if ( d != 0.0 || std::isnan( d ) )
							any = true;
						if ( !( d <= 1e-2 * m ) )
							past = true;
						if ( d / m > maxRel )
							maxRel = d / m;
					}
					diff += any;
					colPast += past;
				}
				const bool ok = covered >= 1000 && onlyOne * 2000 <= covered && colPast * 1000 <= both;
				const char * word = covered < 1000 ? "FAIL" : ( diff == 0 && onlyOne == 0 ) ? "EXACT" : ok ? "BOUND" : "FAIL";
				const bool fail = !ok;
				std::printf( "WATERDRAW %s red %d: %s covered %zu (both %zu, one side only %zu), colour %zu pixels differ, %zu past 1e-2 "
					"(max rel %.3g)%s; run %.1f ms%s\n", lab, red, word, covered, both, onlyOne, diff, colPast, maxRel,
					red ? ( fail ? " -> red FAILS as it must" : " -> red PASSES: the gate is blind" ) : "", runMs, forced.c_str() );
				if ( red ? !fail : fail )
					gateOk = false;
			}
		};
		if ( !forceReuse ) {
			// the D24S8 arm, then the D32F_S8 one (the app's other pre-pass format): both renderbuffers respecified at the
			// same names and size, so waterRayDepth remakes its twin at the same key
			for ( const GLenum dsFmt : kDsFmt ) {
				if ( dsFmt != kDsFmt[0] && ( !rayTargets( rbRef, fbRayRef, dsFmt ) || !rayTargets( rbVk, fbRayVk, dsFmt ) ) )
					return fatal( "the D32F_S8 ray targets: a framebuffer incomplete" );
				const std::vector<float> glR = rayRef();
				for ( int red : { 0, 1, 2, 4, 8 } )
					rayOne( dsFmt == kDsFmt[0] ? "ray" : "ray d32fs8", red, dsFmt, glR );
				if ( dsFmt != kDsFmt[0] ) {  // lane VKWATER4: the fence-timeout red, then a clean run (the wait it left must be taken)
					rayOne( "ray d32fs8", 128, dsFmt, glR );
					rayOne( "ray d32fs8 after the timeout", 0, dsFmt, glR );
				}
			}
		} else {
			std::vector<float> glR[2];
			for ( int k = 0; k < 2; k++ ) {
				if ( !rayTargets( rbRef, fbRayRef, kDsFmt[k] ) )
					return fatal( "the ray reference target: a framebuffer incomplete" );
				glR[k] = rayRef();
			}
			unsigned primed = 0;
			if ( !rayTargets( rbVk, fbRayVk, kDsFmt[1] )
				|| !WwVkPresent::waterRayDepth( reinterpret_cast<std::uintptr_t>( &rayKeys[1] ), rbVk, kDsFmt[1], WR, HR, primed, why ) )
				return fatal( "the forced reuse's primed depth twin: " + why );
			int n = 0;
			for ( int rep = 0; rep < 4; rep++ )
				for ( int red : { 0, 1, 2, 4, 8 } ) {
					const int k = n++ % 2;
					if ( !rayTargets( rbVk, fbRayVk, kDsFmt[k] ) )
						return fatal( "the ray target: a framebuffer incomplete" );
					rayOne( k ? "ray d32fs8" : "ray", red, kDsFmt[k], glR[k] );
				}
		}
		const GLint zero = 0;
		f.useProgram( pWater );
		f.uniform1iv( sslr, 1, &zero );
		f.useProgram( 0 );
		f.bindFramebuffer( fFb, 0 );
	}
	const WwVkPresent::FxStat st = WwVkPresent::takeWaterDrawStat();
	std::printf( "WATERDRAW %d runs: %d on Vulkan, %.2f ms CPU each (max %.2f; the harness's hand-off wait included), %d fails\n", runs,
		st.runs, st.runs ? st.cpuMs / st.runs : 0.0, st.maxMs, st.fails );
	// lane VKWATER4: ray runs whose depth twin was remade at its old VkImage handle value (the d32fs8 flake's trigger)
	std::printf( "WATERDRAW ray handle reused: %d runs; fence waits not VK_SUCCESS: %d; forced reuse hits %d, misses %d, remake fails %d\n",
		st.reusedHandles, st.fenceMisses, forcedHits, forcedMisses, forcedFails );
	gateOk = gateOk && unslottedOk;
	std::printf( gateOk ? "WATERDRAW GATE OK\n" : "WATERDRAW GATE FAILED\n" );
	return gateOk ? 0 : 1;
}

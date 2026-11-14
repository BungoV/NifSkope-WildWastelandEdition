// Lane VKFAR2 (2026-11-13, row G3): the far LOD rings' instanced draw on the present device (vkwater.inc farLodRun: the
// mesh pipeline over GL's own vertex / instance / element buffers, the farlod_view_vs / farlod_view_ps twins, the loose
// uniforms through the FxField tables, every input through waterTwin or the zero views) against GL's own program
// (fo4_default.vert + fo4_cellcsm.frag as the app links it, written by farloddraw_prep.py), per sample, into the same 4x
// MSAA RGBA16F + D24S8 frame contents. fxview.cpp's helpers are reused (its main renamed); GL's side is read by the
// shipped farLodStateOf. The layout is cellfarinst.cpp's: one interleaved vertex buffer (stride 36: position 3f, colour
// RGBA8, normal / tangent / bitangent INT_2_10_10_10_REV, uv 2f), instance rows (stride 64, divisor 1) and a ladder
// (2f, divisor 1), ushort elements, one instanced command per mesh with firstIndex / baseVertex / baseInstance.
// Scene: two meshes (a wavy strip, a third of its rows wound clockwise, and a hill) in one buffer, four instances (one
// with the uv flip, two on the ladder's dither either way, one turned and scaled), a depth slab over the left half before
// the rings (LEQUAL, depth written), GL's polygon offset (1, 2) on, every sampler on its own unit: seeded data of the
// app's format, the integer images and the colour cube left unbound (GL reads 0; the twin's zero views).
// Seeds: 1 the terrain rows (no stream); 2 the same through a scissor with the rows' vertex-colour AO (farVcAo.z 3);
// 3 the objects' rows: farStreamOn with the RG8 AO / sky stream (lodchsky's per-vertex sky byte), extra.y its int base,
// farBakeK (1, 1, 0), lit by the ambient alone (cellOn, cellFarOn: the air and screen AO hid the stream from red 32).
// The mipped maps (BaseMap, NormalMap, LandLodDiff / Detail) hold one smooth repeating field at every level; seed 1 is
// drawn once more with noise in every level and logged, not judged (NV picks a steep footprint's mip LOD differently GL
// vs Vulkan: the noise parts the hill's far slope, with the mips dropped it is 0). The seeded uniforms zero the probes,
// debug views and the multiply decal; the shadow block is seeded per member by type (an int seeded as a float bit
// pattern turns csmProbe's grey view on).
// Bar (set before the first run, never widened; the water draw's, notes/vkwater3/README.md):
//   covered = samples whose depth or colour differs from the frame before the rings on either side; must be >= 1000
//   (an empty layer FAILS); coverage mismatch <= 0.05 % of covered; depth within 1 D24 unit on every sample covered on
//   both sides; colour |vk - gl| <= 1e-2 * max(1, |gl|) per half on >= 99.9 % of the samples covered on both sides.
// Reds (each must FAIL, never refuse; seed 1): red 1 the ps loose block one vec4 off, red 2 plain samplers, red 4 cull
// flipped, red 8 depth ALWAYS, red 16 the polygon offset dropped; seed 3: red 32 the objects' stream copied 64 bytes late. Last line `FARLODDRAW GATE OK` or `FARLODDRAW GATE FAILED`.
#define main fxview_main
#include "fxview.cpp"
#undef main

#include <random>
#include <set>

namespace fld
{
using namespace fv;

//! GL's entry points under GL's own names (twinStateOf / waterCensusOf / fxPack call fn->glX as on Qt's functions)
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
	void( WINAPI * glGetFloatv )( GLenum, GLfloat * ) = nullptr;
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
	void( WINAPI * vertexAttribDivisor )( GLuint, GLuint ) = nullptr;
	void( WINAPI * drawInst )( GLenum, GLsizei, GLenum, const void *, GLsizei, GLint, GLuint ) = nullptr;
	void( WINAPI * texStorage2D )( GLenum, GLsizei, GLenum, GLsizei, GLsizei ) = nullptr;
	void( WINAPI * texStorage3D )( GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei ) = nullptr;
	void( WINAPI * texBuffer )( GLenum, GLenum, GLuint ) = nullptr;
	void( WINAPI * bufferSubData )( GLenum, std::ptrdiff_t, std::ptrdiff_t, const void * ) = nullptr;
	void( WINAPI * uniform2uiv )( GLint, GLsizei, const GLuint * ) = nullptr;
	void( WINAPI * uniform3uiv )( GLint, GLsizei, const GLuint * ) = nullptr;
	void( WINAPI * uniform4uiv )( GLint, GLsizei, const GLuint * ) = nullptr;
	void( WINAPI * uniformMatrix2fv )( GLint, GLsizei, GLboolean, const GLfloat * ) = nullptr;
};
Ex ex;

template <class P> bool ldx( P & p, const char * n )
{
	p = reinterpret_cast<P>( glProc( n ) );
	if ( !p )
		std::printf( "FARLODDRAW missing %s\n", n );
	return p != nullptr;
}

bool loadMore()
{
	bool ok = true;
#define FD_L( n ) ok = ldx( gw.n, #n ) && ok
	FD_L( glGetProgramiv ); FD_L( glGetActiveUniform ); FD_L( glGetActiveUniformsiv ); FD_L( glGetUniformLocation );
	FD_L( glGetUniformiv ); FD_L( glGetUniformfv ); FD_L( glActiveTexture ); FD_L( glGetIntegerv ); FD_L( glGetBooleanv ); FD_L( glGetFloatv );
	FD_L( glIsEnabled ); FD_L( glGetTexLevelParameteriv ); FD_L( glGetTexParameteriv ); FD_L( glGetTexParameterfv );
	FD_L( glGetAttachedShaders ); FD_L( glGetShaderiv ); FD_L( glBindBuffer ); FD_L( glGetBufferParameteriv );
	FD_L( glGetVertexAttribiv ); FD_L( glGetVertexAttribfv ); FD_L( glGetVertexAttribPointerv ); FD_L( glGetIntegeri_v );
	FD_L( glGetInteger64i_v ); FD_L( glGetUniformBlockIndex ); FD_L( glGetActiveUniformBlockiv );
#undef FD_L
#define FD_X( m, n ) ok = ldx( ex.m, n ) && ok
	FD_X( genVertexArrays, "glGenVertexArrays" ); FD_X( bindVertexArray, "glBindVertexArray" );
	FD_X( vertexAttribPointer, "glVertexAttribPointer" ); FD_X( enableVertexAttribArray, "glEnableVertexAttribArray" );
	FD_X( vertexAttribDivisor, "glVertexAttribDivisor" ); FD_X( drawInst, "glDrawElementsInstancedBaseVertexBaseInstance" );
	FD_X( texStorage2D, "glTexStorage2D" ); FD_X( texStorage3D, "glTexStorage3D" ); FD_X( texBuffer, "glTexBuffer" );
	FD_X( bufferSubData, "glBufferSubData" ); FD_X( uniform2uiv, "glUniform2uiv" ); FD_X( uniform3uiv, "glUniform3uiv" );
	FD_X( uniform4uiv, "glUniform4uiv" ); FD_X( uniformMatrix2fv, "glUniformMatrix2fv" );
#undef FD_X
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
	int kind = 0;       // 0 2D, 1 3D, 2 2D array, 3 cube, 4 cube-map array, 5 texel buffer
	GLenum fmt = 0x881A;
	int w = 64, h = 64, d = 1, levels = 1, unit = 0, bufBytes = 16 * 256;
	bool intF = false, depth = false, compare = false, unbound = false;
	GLuint tex = 0, buf = 0;
	float phase[4] = {};  // a mipped map's smooth field (fill)
};

bool g_mipNoise = false;  // the logged arm: noise in every mip level (judged runs: the smooth field)

bool smpOf( Smp & s, int W, int H )
{
	switch ( s.glType ) {
	case 0x8B5E: s.kind = 0; break;                          // sampler2D
	case 0x8B5F: s.kind = 1; break;                          // sampler3D
	case 0x8B60: s.kind = 3; s.unbound = true; break;        // samplerCube: the app binds none on the rings (GL reads 0)
	case 0x8DC1: s.kind = 2; break;                          // sampler2DArray
	case 0x8DC4: s.kind = 2; s.compare = true; break;        // sampler2DArrayShadow
	case 0x900D: s.kind = 4; s.compare = true; break;        // samplerCubeMapArrayShadow
	case 0x8DC2: s.kind = 5; s.fmt = 0x8814; break;          // samplerBuffer
	case 0x8DD0: s.kind = 5; s.fmt = 0x8235; s.intF = true; break;  // isamplerBuffer: R32I (cellclustergl.cpp)
	case 0x8DD8: s.kind = 5; s.fmt = 0x8D70; s.intF = true; break;  // usamplerBuffer
	case 0x8DD2: s.kind = 0; s.unbound = true; s.intF = true; break;  // usampler2D: unbound (the twin's zero R32UI)
	case 0x8DCA: s.kind = 0; s.unbound = true; s.intF = true; break;  // isampler2D
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
	if ( s.kind == 2 )
		s.d = 4;
	if ( n == "farStream" ) {
		s.fmt = 0x822B;  // RG8: the objects' AO / sky bytes (cellfarinst.cpp)
		s.bufBytes = 8192;
	} else if ( n == "BaseMap" || n == "NormalMap" || n == "LandLodDiff" || n == "LandLodDetail" ) {
		s.w = s.h = 64;  // mipped, repeat: the uv runs past 0..1 (red 2's plain sampler clamps)
		s.levels = std::getenv( "FLD_NOMIP" ) ? 1 : 7;
	} else if ( n == "waterInfoBodies" || n == "cellCtGbuf" ) {
		s.fmt = 0x8814;  // RGBA32F
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
	}
	return true;
}

unsigned targetOf( int kind )
{
	static const unsigned t[6] = { 0x0DE1, 0x806F, 0x8C1A, 0x8513, 0x9009, 0x8C2A };
	return t[kind];
}

// the texture (made once, its contents per seed): float data in [0, 1); texel buffers zero except the RG8 stream (bytes)
bool fill( Smp & s, std::mt19937 & rng )
{
	if ( s.unbound )
		return true;
	std::uniform_real_distribution<float> u01( 0.0f, 1.0f );
	const unsigned tg = targetOf( s.kind );
	if ( s.kind == 5 ) {
		if ( !s.tex ) {
			glGenTextures( 1, &s.tex );
			f.genBuffers( 1, &s.buf );
			f.bindBuffer( 0x8C2A, s.buf );
			f.bufferData( 0x8C2A, s.bufBytes, nullptr, fStatic );
			glBindTexture( tg, s.tex );
			ex.texBuffer( tg, s.fmt, s.buf );
		}
		std::vector<std::uint8_t> z( size_t( s.bufBytes ), 0 );
		if ( s.fmt == 0x822B )	// AO / sky ramps by texel (per-vertex noise averaged out over a triangle: red 32 hid)
			for ( size_t i = 0; i < z.size(); i++ ) {
				rng();
				const size_t t = i / 2;
				z[i] = std::uint8_t( i & 1 ? ( t * 7 + 128 ) & 0xFF : ( t * 5 ) & 0xFF );
			}
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
		glTexParameteri( tg, GL_TEXTURE_MIN_FILTER, mip ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR );
		glTexParameteri( tg, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( tg, GL_TEXTURE_WRAP_S, mip ? GL_REPEAT : 0x812F );
		glTexParameteri( tg, GL_TEXTURE_WRAP_T, mip ? GL_REPEAT : 0x812F );
		glTexParameteri( tg, 0x8072, 0x812F );
		glTexParameteri( tg, fMaxLevel, s.levels - 1 );
		if ( s.compare ) {
			glTexParameteri( tg, 0x884C, 0x884E );  // COMPARE_REF_TO_TEXTURE
			glTexParameteri( tg, 0x884D, GL_LEQUAL );
		}
	}
	for ( int l = 0; l < s.levels; l++ ) {
		const int lw = std::max( s.w >> l, 1 ), lh = std::max( s.h >> l, 1 );
		const int ld = s.kind == 1 ? std::max( s.d >> l, 1 ) : s.kind == 0 ? 1 : s.d;
		const size_t n = size_t( lw ) * size_t( lh ) * size_t( ld );
		GLenum pf = GL_RGBA;
		int comps = 4;
		if ( s.fmt == 0x81A5 ) {
			pf = GL_DEPTH_COMPONENT;
			comps = 1;
		} else if ( s.fmt == 0x822E ) {
			pf = fRed;
			comps = 1;
		}
		std::vector<float> data( n * size_t( comps ) );
		static const char * texOnly = std::getenv( "FLD_TEX" );  // probing: only textures named in it keep noise
		const bool noise = !texOnly || std::strstr( texOnly, s.name.c_str() );
		if ( texOnly && l == 0 )
			std::printf( "FLDTEX %s kind %d fmt 0x%X %dx%dx%d levels %d%s\n", s.name.c_str(), s.kind, s.fmt, s.w, s.h, s.d,
				s.levels, s.compare ? " compare" : "" );
		if ( s.levels > 1 && !g_mipNoise ) {
			/* a mipped map: one smooth repeating field sampled at every level's texel centres (seeded phases), so the
			 * level a pixel reads changes little. NV picks a steep footprint's mip LOD differently GL vs Vulkan (noise in
			 * every level parted 7 % of the hill's samples, with no mips 0: notes/vkfar2/README.md); a wrong uv, wrap or
			 * missing level still parts the field by up to 0.7 */
			if ( l == 0 )
				for ( float & ph : s.phase )
					ph = u01( rng );
			for ( int z = 0; z < ld; z++ )
				for ( int j = 0; j < lh; j++ )
					for ( int i = 0; i < lw; i++ )
						for ( int c = 0; c < comps; c++ ) {
							const float u = ( float( i ) + 0.5f ) / float( lw ), v = ( float( j ) + 0.5f ) / float( lh );
							const float k = 6.2831853f;
							data[( ( size_t( z ) * size_t( lh ) + size_t( j ) ) * size_t( lw ) + size_t( i ) ) * size_t( comps ) + size_t( c )] =
								0.5f + 0.35f * std::sin( k * ( u + s.phase[c] + 0.13f * float( z ) ) ) * std::cos( k * ( v + s.phase[( c + 1 ) & 3] ) );
						}
		} else {
			for ( auto & x : data )
				x = noise ? u01( rng ) : 0.5f;
		}
		if ( s.kind == 0 )
			glTexSubImage2D( tg, l, 0, 0, lw, lh, pf, GL_FLOAT, data.data() );
		else
			f.texSubImage3D( tg, l, 0, 0, 0, lw, lh, ld, pf, GL_FLOAT, data.data() );
	}
	const GLenum e = glGetError();
	if ( e != GL_NO_ERROR )
		std::printf( "FARLODDRAW texture %s: GL error 0x%X\n", s.name.c_str(), e );
	return e == GL_NO_ERROR;
}

// every active loose uniform seeded by its GL type (block members and samplers skipped); the placement ones fixed, the
// switches a non-far shape or a discard reads zero, names with "Red" / "red" zero. The far ones are set after.
// FLD_ZERO=a,b,...: names holding any of these substrings zero too (finding a constant layer)
bool extraZero( const std::string & base )
{
	static const std::string list = getenv( "FLD_ZERO" ) ? getenv( "FLD_ZERO" ) : "";
	size_t s = 0;
	while ( s < list.size() ) {
		size_t e = list.find( ',', s );
		if ( e == std::string::npos )
			e = list.size();
		if ( e > s && base.find( list.substr( s, e - s ) ) != std::string::npos )
			return true;
		s = e + 1;
	}
	return false;
}

/* the switch profile (exact names): those loose ints 1, every other loose int 0. Random switches turn the program's
 * views on (a constant layer hides the samplers), so each seed draws a far ring's real set: the terrain rows lit, fogged,
 * shadowed, with the screen inputs (seed 1), plus the extra lights / reflections / gobo (seed 2), the objects' maps
 * (seed 3). FLD_ON=a,b,... replaces it (probing one path). */
int g_profileSeed = 1;
const char * switchList()
{
	if ( getenv( "FLD_ON" ) )
		return getenv( "FLD_ON" );
	static const std::string base = "cellOn,fogOn,volOn,cellFarOn,cellShadowOn,cellShadowFadeOn,ussOn,ussCsmOn,ussTerrOn,"
		"ussContactOn,ussCloudOn,cellAoOn,cellHasDir,cellClusterOn,landShoreOn,landWetOn,landSwashOn,waterInfoOn";
	static const std::string s1 = base + ",cellSsgiOn,cellSsrOn,cellIsOn";
	static const std::string s2 = base + ",cellReflOn,cellReflCount,cellLightCount,cellGoboOn,cellCtOn,cellFarPlacedOn,cellHasDalc,cellSpecOcc";
	// seed 3: the objects' baked AO / sky stream under the ambient alone (seeds 1-2 judge the air, sun, shadows and
	// screen inputs): with those on, the stream's share stayed under the bar and red 32 passed (zp_stream*.log)
	static const std::string s3 = "cellOn,cellFarOn";
	return ( g_profileSeed == 2 ? s2 : g_profileSeed == 3 ? s3 : s1 ).c_str();
}

bool onSwitch( const std::string & base )
{
	const std::string list = std::string( "," ) + switchList() + ",";
	return list.find( "," + base + "," ) != std::string::npos;
}

int seedUnis( GLuint p, std::mt19937 & rng )
{
	std::uniform_real_distribution<float> u01( 0.05f, 1.0f );
	f.useProgram( p );
	GLint n = 0;
	f.getProgramiv( p, fActiveUnis, &n );
	int set = 0;
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
			|| base == "wwGrassWind2" || base == "wwTreeWind2" || base == "farVcAo" || base == "lodViewCode" || base == "farSplat"
			|| base == "vertexColorOverride" || base == "alphaFlags" || base == "wwDecalPom" || base == "wwDecalAngle"
			|| base == "wwGrassFade" || base == "lodChannelView" || base == "landSplatDbg"
			|| base == "wwMulDecal"	// a multiply decal writes its albedo factor alone (1 or 0.5 grey with no coverage)
			|| base.find( "Red" ) != std::string::npos || base.find( "red" ) != std::string::npos
			// the program's probes and debug views (a grey constant layer: csmProbe, ussView, wwDebugView, cellProbe...)
			|| base.find( "Probe" ) != std::string::npos || base.find( "View" ) != std::string::npos
			|| base.find( "Debug" ) != std::string::npos || base.find( "Dbg" ) != std::string::npos || base == "volFogBlend"
			|| extraZero( base );
		for ( size_t k = 0; k < total; k++ ) {
			fv[k] = zero ? 0.0f : u01( rng );
			iv[k] = zero ? 0 : GLint( rng() & 1u );
			if ( switchList() )	// a switch profile: these ints 1, every other int 0
				iv[k] = onSwitch( base ) ? 1 : 0;
		}
		if ( getenv( "FLD_LIST" ) && !zero )
			std::printf( "FLDUNI %s %c %d\n", base.c_str(), kind, kind == 'i' ? iv[0] : -1 );
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
	std::vector<char> map;	// FLD_MAP probing only
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
		if ( past && getenv( "FLD_DUMP" ) && v.colPast < 24 )	// probing: where the colours part
			std::printf( "FLDDUMP s %zu gl %.4f %.4f %.4f vk %.4f %.4f %.4f\n", s, h2f( glC[s * 4] ), h2f( glC[s * 4 + 1] ),
				h2f( glC[s * 4 + 2] ), h2f( vkC[s * 4] ), h2f( vkC[s * 4 + 1] ), h2f( vkC[s * 4 + 2] ) );
		v.colPast += past;
		if ( getenv( "FLD_MAP" ) && n == size_t( 256 * 160 * 4 ) ) {	// probing: a 64 x 40 map, 4 x 4 pixels a cell
			if ( v.map.empty() )
				v.map.assign( 64 * 40, ' ' );
			const size_t px = s / 4, x = px % 256, y = px / 256;
			char & m = v.map[( y / 4 ) * 64 + x / 4];
			if ( past )
				m = '#';
			else if ( m == ' ' )
				m = '.';
		}
	}
	for ( int r = 39; r >= 0 && !v.map.empty(); r-- )
		std::printf( "FLDMAP %2d %.*s\n", r, 64, &v.map[size_t( r ) * 64] );
	return v;
}

// INT_2_10_10_10_REV, normalized: x bits 0..9, y 10..19, z 20..29, w 30..31
std::uint32_t pack1010102( float x, float y, float z, float w )
{
	auto q = []( float v, float m ) { return std::uint32_t( std::lround( std::clamp( v, -1.0f, 1.0f ) * m ) ); };
	return ( q( x, 511.0f ) & 0x3FFu ) | ( ( q( y, 511.0f ) & 0x3FFu ) << 10 ) | ( ( q( z, 511.0f ) & 0x3FFu ) << 20 ) | ( ( q( w, 1.0f ) & 3u ) << 30 );
}
}  // namespace fld

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace fld;
	gTag = "FARLODDRAW";
	if ( argc < 2 ) {
		std::printf( "usage: farloddraw <prep dir>\nFARLODDRAW GATE FAILED\n" );
		return 2;
	}
	const std::string dir = argv[1];
	auto fatal = [&]( const std::string & why ) {
		std::printf( "FARLODDRAW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	const int S = 4, W = 256, H = 160, WS = W * S;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwfarloddraw";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwfarloddraw", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, 320, 180 };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwfarloddraw", "far LOD draw harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
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
	std::printf( "FARLODDRAW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	const GLuint pUnpack = program( kTriVs, kUnpackFs, "unpack" ), pDepth = program( kTriVs, kDepthFs, "depth" ),
		pSlab = program( kTriVs, kSlabFs, "slab" );
	const GLuint pFar = program( readFile( dir + "/prog.vert.glsl" ), readFile( dir + "/prog.frag.glsl" ), "fo4_cellcsm" );
	if ( !pUnpack || !pDepth || !pSlab || !pFar )
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

	// ---- the program's samplers: one unit each (the unbound ones too: no two types on a unit)
	std::vector<Smp> smp;
	{
		GLint n = 0;
		f.getProgramiv( pFar, fActiveUnis, &n );
		for ( GLint i = 0; i < n; i++ ) {
			char nm[256] = {};
			GLint sz = 0;
			GLenum ty = 0;
			f.getActiveUniform( pFar, GLuint( i ), 256, nullptr, &sz, &ty, nm );
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
		std::string miss;
		int nMiss = 0;
		for ( const WwVkPresent::FxField & fd : WwVkPresent::kT_farlodPs )
			if ( f.getUniformLocation( pFar, fd.name ) < 0 ) {
				nMiss++;
				miss += std::string( " " ) + fd.name;
			}
		for ( const WwVkPresent::FxField & fd : WwVkPresent::kT_waterVs )
			if ( f.getUniformLocation( pFar, fd.name ) < 0 ) {
				nMiss++;
				miss += std::string( " vs:" ) + fd.name;
			}
		std::printf( "FARLODDRAW %d twin fields GL has no location for:%s\n", nMiss, miss.c_str() );
	}
	std::printf( "FARLODDRAW %zu samplers active in GL's program:", smp.size() );
	for ( const Smp & s : smp )
		std::printf( " %s%s", s.name.c_str(), s.unbound ? "(unbound)" : "" );
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
		const GLuint bi = f.getUniformBlockIndex( pFar, blockName[k] );
		if ( bi != 0xFFFFFFFFu ) {
			f.uniformBlockBinding( pFar, bi, GLuint( k ) );
			f.getActiveUniformBlockiv( pFar, bi, fBlockSize, &blockSize[k] );
		}
		std::vector<std::uint8_t> z( size_t( std::max( blockSize[k], 16 ) ), 0 );
		f.bindBuffer( fUbo, ubo[k] );
		f.bufferData( fUbo, std::ptrdiff_t( z.size() ), z.data(), fStatic );
	}
	std::printf( "FARLODDRAW blocks: globalUniforms %d, wwShadowFrame %d, skinningUniforms %d bytes\n", blockSize[0], blockSize[1], blockSize[2] );
	/* wwShadowFrame's members by type: floats seeded, ints 0 / 1 and the views / probes / reds 0. Seeding the block as floats
	 * read every int as a huge number: ussOn and ussView on, the shadow ladder's grey over the whole layer (red 2 blind) */
	struct ShMember
	{
		std::string name;
		GLint off = 0, type = 0, size = 1, stride = 0;
	};
	std::vector<ShMember> shMembers;
	{
		const GLuint sbi = f.getUniformBlockIndex( pFar, "wwShadowFrame" );
		GLint n = 0;
		f.getProgramiv( pFar, fActiveUnis, &n );
		for ( GLint i = 0; i < n && sbi != 0xFFFFFFFFu; i++ ) {
			const GLuint idx = GLuint( i );
			GLint bi = -1;
			f.getActiveUniformsiv( pFar, 1, &idx, fBlockIndex, &bi );
			if ( bi != GLint( sbi ) )
				continue;
			ShMember m;
			char nm[256] = {};
			GLenum ty = 0;
			f.getActiveUniform( pFar, idx, 256, nullptr, &m.size, &ty, nm );
			m.name = nm;
			m.type = GLint( ty );
			f.getActiveUniformsiv( pFar, 1, &idx, 0x8A3B, &m.off );     // GL_UNIFORM_OFFSET
			f.getActiveUniformsiv( pFar, 1, &idx, 0x8A3C, &m.stride );  // GL_UNIFORM_ARRAY_STRIDE
			shMembers.push_back( m );
		}
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
			default: continue;  // any other type stays zero
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

	// ---- the meshes, cellfarinst.cpp's interleaved layout (stride 36)
	struct Vtx
	{
		float p[3];
		std::uint8_t c[4];
		std::uint32_t n, t, b;
		float uv[2];
	};
	static_assert( sizeof( Vtx ) == 36, "cellfarinst.cpp's stride" );
	std::vector<Vtx> vtx;
	std::vector<std::uint16_t> idx( 6, 0 );  // 6 leading dummies: every command starts past them
	auto grid = [&]( int GX, int GZ, float x0, float x1, float z0, float z1, auto height, bool cwRows ) {
		const int base = int( vtx.size() );
		for ( int j = 0; j < GZ; j++ )
			for ( int i = 0; i < GX; i++ ) {
				const float x = x0 + ( x1 - x0 ) * float( i ) / float( GX - 1 ), z = z0 + ( z1 - z0 ) * float( j ) / float( GZ - 1 );
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
				const std::uint16_t a = std::uint16_t( j * GX + i ), b = std::uint16_t( a + 1 ), c = std::uint16_t( a + GX ),
					d = std::uint16_t( c + 1 );
				if ( cwRows && j % 3 == 1 )
					idx.insert( idx.end(), { a, c, b, b, c, d } );  // clockwise seen from above: culled
				else
					idx.insert( idx.end(), { a, b, c, b, d, c } );
			}
		WwVkPresent::WaterCmd c;
		c.count = std::uint32_t( idx.size() ) - first;
		c.firstIndex = first;
		c.baseVertex = base;  // the ushort elements are the mesh's own
		return c;
	};
	auto strip = []( float x, float z ) { return 25.0f * std::sin( x * 0.011f ) * std::cos( z * 0.013f ); };
	auto hill = []( float x, float z ) { return 80.0f * std::exp( -( x * x + z * z ) / 9000.0f ) + 6.0f * std::sin( x * 0.05f ); };
	std::vector<WwVkPresent::WaterCmd> cmds;
	cmds.push_back( grid( 32, 40, -400.0f, 400.0f, 0.0f, -1500.0f, strip, true ) );
	cmds.push_back( grid( 16, 16, -160.0f, 160.0f, 160.0f, -160.0f, hill, false ) );
	cmds[0].instanceCount = 2, cmds[0].baseInstance = 0;
	cmds[1].instanceCount = 2, cmds[1].baseInstance = 2;
	const int nVtx = int( vtx.size() );

	// the instances: rows (a 3x4 affine, view space) + extra (x the uv flip's reference, y z the colour / stream base, w e)
	struct Inst
	{
		float row[3][4];
		float extra[4];
	};
	auto inst = [&]( float yawDeg, float scale, float tx, float ty, float tz, float ex0, float ey, float ez, float ew ) {
		const float a = yawDeg * 0.01745329f, c = std::cos( a ) * scale, s = std::sin( a ) * scale;
		Inst k { { { c, 0.0f, s, tx }, { 0.0f, scale, 0.0f, ty }, { -s, 0.0f, c, tz } }, { ex0, ey, ez, ew } };
		return k;
	};
	const Inst kInst[4] = { inst( 0.0f, 1.0f, -430.0f, -130.0f, -250.0f, 0.0f, 0.9f, 0.8f, 0.95f ),
		inst( 0.0f, 1.0f, 430.0f, -110.0f, -250.0f, 2.0f, 0.7f, 1.0f, 2.85f ),  // e > 1.5: the uv flip, e - 2
		inst( 0.0f, 1.0f, -250.0f, -70.0f, -700.0f, 0.0f, 1.0f, 1.0f, 1.0f ),
		inst( 30.0f, 1.5f, 300.0f, -90.0f, -1300.0f, 0.0f, 0.8f, 0.9f, 1.0f ) };
	const float kLadder[4][2] = { { 0.0f, 0.0f }, { 0.0f, 0.0f }, { 1.5f, 0.0f }, { -1.5f, 0.0f } };  // 2 and 3 on the dither
	GLuint vao = 0, vb = 0, ib = 0, lb = 0, eb = 0;
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
	f.genBuffers( 1, &ib );
	f.bindBuffer( 0x8892, ib );
	f.bufferData( 0x8892, std::ptrdiff_t( sizeof( kInst ) ), kInst, 0x88E8 );  // DYNAMIC_DRAW: seed 3 rewrites extra.y
	for ( int a = 0; a < 4; a++ ) {
		ex.vertexAttribPointer( GLuint( 10 + a ), 4, GL_FLOAT, GL_FALSE, 64, reinterpret_cast<const void *>( std::uintptr_t( 16 * a ) ) );
		ex.enableVertexAttribArray( GLuint( 10 + a ) );
		ex.vertexAttribDivisor( GLuint( 10 + a ), 1 );
	}
	f.genBuffers( 1, &lb );
	f.bindBuffer( 0x8892, lb );
	f.bufferData( 0x8892, std::ptrdiff_t( sizeof( kLadder ) ), kLadder, fStatic );
	ex.vertexAttribPointer( 9, 2, GL_FLOAT, GL_FALSE, 8, nullptr );
	ex.enableVertexAttribArray( 9 );
	ex.vertexAttribDivisor( 9, 1 );
	f.genBuffers( 1, &eb );
	f.bindBuffer( 0x8893, eb );
	f.bufferData( 0x8893, std::ptrdiff_t( idx.size() * 2 ), idx.data(), fStatic );
	ex.bindVertexArray( 0 );
	const float zm = -900.0f, zd = ( ( proj[10] * zm + proj[14] ) / -zm ) * 0.5f + 0.5f;
	std::printf( "FARLODDRAW job: %dx%d x%d, %d vertices, %zu elements, 2 commands x 2 instances, slab depth %.6f\n", W, H, S, nVtx,
		idx.size(), zd );

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
	// cellfarinst.cpp's state: opaque, depth LEQUAL written, back faces culled, the rings' polygon offset
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
		f.useProgram( pFar );
		for ( int k = 0; k < 3; k++ )
			f.bindBufferBase( fUbo, GLuint( k ), ubo[k] );
		for ( const Smp & s : smp ) {
			bindUnit( s.unit, targetOf( s.kind ), s.tex );
			f.uniform1i( f.getUniformLocation( pFar, s.name.c_str() ), s.unit );
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
	auto glDraw = [&]() {
		for ( const WwVkPresent::WaterCmd & c : cmds )
			ex.drawInst( GL_TRIANGLES, GLsizei( c.count ), GL_UNSIGNED_SHORT, reinterpret_cast<const void *>( std::uintptr_t( c.firstIndex ) * 2u ),
				GLsizei( c.instanceCount ), c.baseVertex, c.baseInstance );
	};
	auto setI = [&]( const char * n, GLint v ) {
		const GLint l = f.getUniformLocation( pFar, n );
		if ( l >= 0 )
			f.uniform1iv( l, 1, &v );
	};
	auto set4 = [&]( const char * n, float a, float b, float c, float d ) {
		const GLint l = f.getUniformLocation( pFar, n );
		const float v[4] = { a, b, c, d };
		if ( l >= 0 )
			f.uniform4fv( l, 1, v );
	};

	static char twinKeys[160];
	bool gateOk = true;
	int runs = 0;
	for ( int arm = 1; arm <= 4; arm++ ) {
		// arm 4: seed 1 again with noise in every mip level, logged, not judged (NV's steep-footprint LOD, fill())
		const bool logged = arm == 4;
		const int seed = logged ? 1 : arm;
		g_mipNoise = logged;
		std::mt19937 rng( 20261113u + std::uint32_t( seed ) * 7919u
			+ std::uint32_t( getenv( "FLD_SEED" ) ? std::atoi( getenv( "FLD_SEED" ) ) : 0 ) );	// FLD_SEED: other draws
		for ( Smp & s : smp )
			if ( !fill( s, rng ) )
				return fatal( "texture " + s.name );
		g_profileSeed = seed;
		const int nu = seedUnis( pFar, rng );
		blocks( rng );
		// cellfarinst.cpp's draw: farInst on; the rows' stream (seed 3) or vertex-colour AO (seed 2)
		const bool stream = seed == 3, scissor = seed == 2;
		f.useProgram( pFar );
		setI( "farInst", 1 );
		setI( "farStreamOn", stream ? 1 : 0 );
		set4( "farVcAo", seed == 2 ? 0.7f : 0.0f, seed == 2 ? 0.5f : 0.0f, seed == 2 ? 3.0f : 0.0f, 0.0f );
		if ( stream ) {	// the stream at full weight, its AO through cellAoShared on the lit colour (a seeded farBakeK hid red 32)
			const GLint l = f.getUniformLocation( pFar, "farBakeK" );
			const float k[3] = { 1.0f, 1.0f, 0.0f };
			if ( l >= 0 )
				f.uniform3fv( l, 1, k );
		}
		{	// the fog's distance in game units: a seeded 0..1 scale on the rings' few hundred units fogs them solid
			const GLint l = f.getUniformLocation( pFar, "fogDistScale" );
			const float v = 1.0f / 1500.0f;
			if ( l >= 0 )
				f.uniform1fv( l, 1, &v );
		}
		Inst rows[4];
		std::memcpy( rows, kInst, sizeof( rows ) );
		if ( stream )
			for ( int k = 0; k < 4; k++ ) {
				const std::int32_t base = 64 * k;  // extra.y: the stream's int base (bits), gl_VertexID added
				std::memcpy( &rows[k].extra[1], &base, 4 );
			}
		f.bindBuffer( 0x8892, ib );
		ex.bufferSubData( 0x8892, 0, std::ptrdiff_t( sizeof( rows ) ), rows );
		std::printf( "FARLODDRAW seed %d: %d loose uniforms seeded%s%s\n", seed, nu, scissor ? ", a scissor, the rows' vertex-colour AO" : "",
			stream ? ", the objects' RG8 AO / sky stream" : ", the terrain rows (no stream)" );

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
		{	// the drawn colour must vary (a constant layer hides the samplers: red 2 would pass)
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
			std::printf( "FARLODDRAW seed %d GL colour over the rings: r %.4g..%.4g g %.4g..%.4g b %.4g..%.4g a %.4g..%.4g, %zu distinct\n", seed,
				lo[0], hi[0], lo[1], hi[1], lo[2], hi[2], lo[3], hi[3], distinct.size() );
			if ( distinct.size() < 1000 && !logged ) {
				std::printf( "FARLODDRAW seed %d: under 1000 distinct colours over the rings: a near-constant layer is blind\n", seed );
				gateOk = false;
			}
		}

		for ( int red : { 0, 1, 2, 4, 8, 16, 32 } ) {
			if ( red && ( logged || ( red == 32 ? seed != 3 : seed != 1 ) ) )	// red 32 (the stream shifted) on the objects' seed
				continue;
			before( fbFrame );
			drawState( fbFrame, scissor );
			WwVkPresent::WaterRun r;
			WwVkPresent::farLodStateOf( &gw, pFar, r );
			r.w = W;
			r.h = H;
			r.color = fc;
			r.cmds = cmds;
			r.key = reinterpret_cast<std::uintptr_t>( twinKeys );
			r.red = red;
			const bool ran = WwVkPresent::farLodRun( r, why );
			stateEnd();
			runs++;
			if ( !ran ) {
				std::printf( "FARLODDRAW seed %d red %d: REFUSED (%s)\n", seed, red, why.c_str() );
				gateOk = false;
				continue;
			}
			const std::vector<std::uint16_t> vkC = readColor( fc );
			const std::vector<float> vkD = readDepth( fds, true );
			const Verdict v = judge( bgC, bgD, vkC, vkD, glC, glD );
			if ( logged ) {
				std::printf( "FARLODDRAW logged (not judged): seed 1 with noise in every mip level: covered %zu, depth max %ld D24 units, "
					"%zu samples past 1e-2 (max rel %.3g): NV's mip LOD on the hill's steep footprints, GL vs Vulkan\n",
					v.covered, v.maxDepthU, v.colPast, v.maxRel );
				continue;
			}
			const char * word = v.word();
			const bool fail = std::strcmp( word, "FAIL" ) == 0;
			std::printf( "FARLODDRAW seed %d red %d: %s covered %zu (both %zu, one side only %zu), depth max %ld D24 units (%zu past 1), "
				"colour %zu halves differ, %zu samples past 1e-2 (max rel %.3g)%s\n", seed, red, word, v.covered, v.both, v.onlyOne,
				v.maxDepthU, v.depthPast, v.halvesDiff, v.colPast, v.maxRel, red ? ( fail ? " -> red FAILS as it must" : " -> red PASSES: the gate is blind" ) : "" );
			if ( red ? !fail : fail )
				gateOk = false;
		}
	}
	const WwVkPresent::FxStat st = WwVkPresent::takeFarLodDrawStat();
	std::printf( "FARLODDRAW %d runs: %d on Vulkan, %.2f ms CPU each (max %.2f; the harness's hand-off wait included), %d fails\n", runs,
		st.runs, st.runs ? st.cpuMs / st.runs : 0.0, st.maxMs, st.fails );
	std::printf( gateOk ? "FARLODDRAW GATE OK\n" : "FARLODDRAW GATE FAILED\n" );
	return gateOk ? 0 : 1;
}

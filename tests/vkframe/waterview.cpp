// Lane VKWATER1 (2026-11-13, plan row G5): the water's census and verdict (vkwater.h, vkwater.inc: the shipped code, built
// in here with WW_VKPRESENT_NO_QT) against GL programs that declare the water twins' inputs, bound the way the app binds
// them, and against the present device.
// Usage: waterview <tree root> <slots.txt>   (slots.txt: waterview_prep.py over the twins' spirv-cross reflections)
// Every input gets a GL texture of the app's format (glscene.cpp, cellwater.cpp, oceanfft.cpp) on its own unit, in programs
// of at most 16 samplers; two GLSL-only inputs of cell_lights.glsl (cellLights samplerBuffer, cellShadow
// samplerCubeArrayShadow) join them, and a tessellated program stands for fo4_waterlookd / dp.
// Checks (each one FAIL fails the gate):
//  census   every input read back: name, kind, unit, texture, format, size, levels, anisotropy; nothing missing or extra;
//           an empty census or an input of slots.txt no census covers is a FAIL
//  verdict  each input's class equals the fixture's (independent table below); the line names every blocked input
//  share    every input the verdict lets move (2D / 3D / array) is taken by sharedTarget; every one it refuses for a
//           format (BC, integer, other) is refused by sharedTarget too
//  device   the caps equal the features vkpresent.cpp enables (textureCompressionBC, imageCubeArray, samplerAnisotropy,
//           tessellationShader)
//  tess     the tessellated program reads tess, the others do not; its verdict names the tessellated twin
// Reds (each must FAIL): bc (caps.bc on), int (the integer rows lost), drop (one census input lost), unit (two inputs'
// textures swapped), tess (caps.tess on), empty (every census empty). The fixture's classes are fixed (lane VKWATER2: the
// integer lookups move); a red only mutates the caps or the census, never the expectation. Last line `WATERVIEW GATE OK` or `WATERVIEW GATE FAILED`.
#define WW_VKPRESENT_NO_QT
#include "../../src/features/vkframe/vkpresent.cpp"

#include <GL/gl.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace wtv
{
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

//! GL entry points under GL's own names: waterCensusOf calls fn->glX( ... ) on this as on Qt's functions
struct GlFn
{
	void( WINAPI * glGetProgramiv )( GLuint, GLenum, GLint * ) = nullptr;
	void( WINAPI * glGetActiveUniform )( GLuint, GLuint, GLsizei, GLsizei *, GLint *, GLenum *, char * ) = nullptr;
	void( WINAPI * glGetActiveUniformsiv )( GLuint, GLsizei, const GLuint *, GLenum, GLint * ) = nullptr;
	GLint( WINAPI * glGetUniformLocation )( GLuint, const char * ) = nullptr;
	void( WINAPI * glGetUniformiv )( GLuint, GLint, GLint * ) = nullptr;
	void( WINAPI * glActiveTexture )( GLenum ) = nullptr;
	void( WINAPI * glGetIntegerv )( GLenum, GLint * ) = nullptr;
	void( WINAPI * glGetTexLevelParameteriv )( GLenum, GLint, GLenum, GLint * ) = nullptr;
	void( WINAPI * glGetTexParameteriv )( GLenum, GLenum, GLint * ) = nullptr;
	void( WINAPI * glGetTexParameterfv )( GLenum, GLenum, GLfloat * ) = nullptr;
	void( WINAPI * glGetAttachedShaders )( GLuint, GLsizei, GLsizei *, GLuint * ) = nullptr;
	void( WINAPI * glGetShaderiv )( GLuint, GLenum, GLint * ) = nullptr;
	// setup only
	GLuint( WINAPI * glCreateShader )( GLenum ) = nullptr;
	void( WINAPI * glShaderSource )( GLuint, GLsizei, const char * const *, const GLint * ) = nullptr;
	void( WINAPI * glCompileShader )( GLuint ) = nullptr;
	void( WINAPI * glGetShaderInfoLog )( GLuint, GLsizei, GLsizei *, char * ) = nullptr;
	GLuint( WINAPI * glCreateProgram )() = nullptr;
	void( WINAPI * glAttachShader )( GLuint, GLuint ) = nullptr;
	void( WINAPI * glLinkProgram )( GLuint ) = nullptr;
	void( WINAPI * glGetProgramInfoLog )( GLuint, GLsizei, GLsizei *, char * ) = nullptr;
	void( WINAPI * glUseProgram )( GLuint ) = nullptr;
	void( WINAPI * glTexStorage2D )( GLenum, GLsizei, GLenum, GLsizei, GLsizei ) = nullptr;
	void( WINAPI * glTexStorage3D )( GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei ) = nullptr;
	void( WINAPI * glTexBuffer )( GLenum, GLenum, GLuint ) = nullptr;
	void( WINAPI * glGenBuffers )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * glBindBuffer )( GLenum, GLuint ) = nullptr;
	void( WINAPI * glBufferData )( GLenum, std::ptrdiff_t, const void *, GLenum ) = nullptr;
};
GlFn g;

template <class T> bool ld( T & p, const char * n )
{
	p = reinterpret_cast<T>( glProc( n ) );
	if ( !p )
		std::printf( "missing %s\n", n );
	return p != nullptr;
}

bool loadAll()
{
	bool ok = true;
#define WTV_L( n ) ok = ld( g.n, #n ) && ok
	WTV_L( glGetProgramiv ); WTV_L( glGetActiveUniform ); WTV_L( glGetActiveUniformsiv ); WTV_L( glGetUniformLocation );
	WTV_L( glGetUniformiv ); WTV_L( glActiveTexture ); WTV_L( glGetIntegerv ); WTV_L( glGetTexLevelParameteriv );
	WTV_L( glGetTexParameteriv ); WTV_L( glGetTexParameterfv ); WTV_L( glGetAttachedShaders ); WTV_L( glGetShaderiv );
	WTV_L( glCreateShader ); WTV_L( glShaderSource ); WTV_L( glCompileShader ); WTV_L( glGetShaderInfoLog );
	WTV_L( glCreateProgram ); WTV_L( glAttachShader ); WTV_L( glLinkProgram ); WTV_L( glGetProgramInfoLog );
	WTV_L( glUseProgram ); WTV_L( glTexStorage2D ); WTV_L( glTexStorage3D ); WTV_L( glTexBuffer ); WTV_L( glGenBuffers );
	WTV_L( glBindBuffer ); WTV_L( glBufferData );
#undef WTV_L
	return ok;
}

//! one input as the fixture makes it, and the class the verdict must give it (the fixture's own table, not vkwater.h's)
struct In
{
	std::string name, glslType, tags;
	int kind = 0;
	unsigned fmt = 0x881A;  // GL_RGBA16F
	int w = 16, h = 16, d = 1, levels = 1;
	float aniso = 1.0f;
	int expect = WwVkPresent::kWaterOk;
	unsigned tex = 0;
	int unit = -1, prog = -1;
};

int kindOfGlsl( const std::string & t )
{
	if ( t.find( "CubeArray" ) != std::string::npos )
		return 4;
	if ( t.find( "Buffer" ) != std::string::npos )
		return 5;
	if ( t.find( "2DArray" ) != std::string::npos )
		return 2;
	if ( t.find( "3D" ) != std::string::npos )
		return 1;
	return 0;
}

void fixtureOf( In & x )
{
	using namespace WwVkPresent;
	const std::string & n = x.name;
	x.kind = kindOfGlsl( x.glslType );
	if ( x.kind == 1 || x.kind == 2 )
		x.d = 4;
	if ( n == "waterNoise0" || n == "waterNoise1" || n == "waterNoise2" ) {
		x.fmt = 0x8DBD;  // BC5 (RGTC2), as the DDS noise loads; trilinear repeat, aniso 16
		x.w = x.h = 64;
		x.levels = 7;
		x.aniso = 16.0f;
		x.expect = kWaterAniso;  // vkwater.h names anisotropy before the format; fixtureNoisePlain keeps one plain BC
	} else if ( n == "waterInfoIndex" || n == "waterFlowIndex" ) {
		x.fmt = 0x823C;  // RG32UI (lane VKWATER2: vkFormatOf's R32G32_UINT row)
		x.expect = kWaterOk;
	} else if ( n == "waterInfoAtlas" || n == "waterFlowAtlas" || n == "waterSkyIndex" ) {
		x.fmt = 0x8236;  // R32UI
		x.expect = kWaterOk;
	} else if ( n == "waterSkyAtlas" ) {
		x.fmt = 0x8232;  // R8UI
		x.expect = kWaterOk;
	} else if ( n == "waterInfoBodies" ) {
		x.fmt = 0x8814;  // RGBA32F
	} else if ( n == "waterMoment0" || n == "waterMoment1" || n == "waterMoment2" ) {
		x.fmt = 0x8230;  // RG32F, mipped, the noise's sampler
		x.w = x.h = 64;
		x.levels = 7;
		x.aniso = 16.0f;
		x.expect = kWaterAniso;
	} else if ( n == "waterOceanB" ) {
		x.fmt = 0x881A;
	} else if ( n == "waterOceanC" ) {
		x.fmt = 0x8814;
	} else if ( n == "waterDepth" ) {
		x.fmt = 0x8CAD;  // DEPTH32F_STENCIL8 (glscene.cpp's float depth frame; a D24S8 frame shares)
		x.expect = kWaterFormat;
	} else if ( n == "csmMap" ) {
		x.fmt = 0x81A5;  // D16 array
	} else if ( n == "cellShadow" ) {
		x.fmt = 0x81A5;
		x.d = 6;
		x.expect = kWaterCubeArray;
	} else if ( n == "cellLights" ) {
		x.fmt = 0x8814;
		x.expect = kWaterTexelBuffer;
	} else if ( n == "ussTerrMap" || n == "ussContactMap" ) {
		x.fmt = 0x822E;  // R32F
	}
}

// the anisotropic BC noise: vkwater.h checks anisotropy before the format, so a BC input with aniso 16 reads "anisotropic
// sampling"; a BC input without it must read "BC formats". The fixture keeps one noise layer plain so both classes run.
void fixtureNoisePlain( In & x )
{
	if ( x.name == "waterNoise2" ) {
		x.aniso = 1.0f;
		x.expect = WwVkPresent::kWaterBc;
	}
}

unsigned targetOf( int kind )
{
	static const unsigned t[6] = { 0x0DE1, 0x806F, 0x8C1A, 0x8513, 0x9009, 0x8C2A };
	return t[kind];
}

bool makeTex( In & x )
{
	glGenTextures( 1, &x.tex );
	const unsigned tg = targetOf( x.kind );
	glBindTexture( tg, x.tex );
	if ( x.kind == 5 ) {
		GLuint b = 0;
		g.glGenBuffers( 1, &b );
		g.glBindBuffer( 0x8C2A, b );
		g.glBufferData( 0x8C2A, 16 * 64, nullptr, 0x88E4 );
		g.glTexBuffer( 0x8C2A, x.fmt, b );
		x.w = 64;
		x.h = 1;
	} else if ( x.kind == 0 ) {
		g.glTexStorage2D( tg, x.levels, x.fmt, x.w, x.h );
	} else {
		g.glTexStorage3D( tg, x.levels, x.fmt, x.w, x.h, x.d );
	}
	if ( x.kind != 5 ) {
		const bool intF = WwVkPresent::waterIntFormat( x.fmt );
		const bool mip = x.levels > 1;
		glTexParameteri( tg, GL_TEXTURE_MIN_FILTER, intF ? GL_NEAREST : mip ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR );
		glTexParameteri( tg, GL_TEXTURE_MAG_FILTER, intF ? GL_NEAREST : GL_LINEAR );
		glTexParameteri( tg, GL_TEXTURE_WRAP_S, mip ? GL_REPEAT : 0x812F );
		glTexParameteri( tg, GL_TEXTURE_WRAP_T, mip ? GL_REPEAT : 0x812F );
		if ( x.aniso > 1.0f )
			glTexParameterf( tg, 0x84FE, x.aniso );
		if ( x.kind == 4 )
			glTexParameteri( tg, 0x884C, 0x884E );  // COMPARE_MODE = COMPARE_REF_TO_TEXTURE
	}
	return glGetError() == GL_NO_ERROR;
}

std::string sampleOf( const In & x )
{
	const std::string & n = x.name;
	if ( x.glslType[0] == 'u' || x.glslType[0] == 'i' )
		return "vec4( texelFetch( " + n + ", ivec2( 0 ), 0 ) )";
	switch ( x.kind ) {
	case 1: return "texture( " + n + ", vec3( 0.5 ) )";
	case 2: return "texture( " + n + ", vec3( 0.5, 0.5, 0.0 ) )";
	case 4: return "vec4( texture( " + n + ", vec4( 1.0, 0.0, 0.0, 0.0 ), 0.5 ) )";
	case 5: return "texelFetch( " + n + ", 0 )";
	default: return "texture( " + n + ", vec2( 0.5 ) )";
	}
}

GLuint shader( GLenum type, const std::string & src, std::string & log )
{
	const GLuint s = g.glCreateShader( type );
	const char * p = src.c_str();
	g.glShaderSource( s, 1, &p, nullptr );
	g.glCompileShader( s );
	GLint ok = 0;
	g.glGetShaderiv( s, 0x8B81, &ok );
	if ( !ok ) {
		char b[2048] = {};
		g.glGetShaderInfoLog( s, sizeof( b ), nullptr, b );
		log += b;
		return 0;
	}
	return s;
}

GLuint program( const std::vector<std::pair<GLenum, std::string>> & stages, std::string & log )
{
	const GLuint p = g.glCreateProgram();
	for ( auto & st : stages ) {
		const GLuint s = shader( st.first, st.second, log );
		if ( !s )
			return 0;
		g.glAttachShader( p, s );
	}
	g.glLinkProgram( p );
	GLint ok = 0;
	g.glGetProgramiv( p, 0x8B82, &ok );
	if ( !ok ) {
		char b[2048] = {};
		g.glGetProgramInfoLog( p, sizeof( b ), nullptr, b );
		log += b;
		return 0;
	}
	return p;
}

const char * kVs = "#version 450\nvoid main() { gl_Position = vec4( 0.0 ); }\n";

//! a mutation of the gate's inputs (the reds)
struct Mut
{
	std::string name;
	bool bc = false, intF = false, tess = false, drop = false, swap = false, empty = false;
};

struct World
{
	std::vector<In> ins;
	std::vector<GLuint> progs;
	GLuint tessProg = 0;
	std::vector<WwVkPresent::WaterCensus> cs;  // one a program, from the shipped census
	WwVkPresent::WaterCensus tessC;
	std::string src;                           // vkpresent.cpp's text
};

bool has( const std::string & s, const std::string & t )
{
	return s.find( t ) != std::string::npos;
}

// all checks over (possibly mutated) censuses; prints when loud; returns the failures
int evaluate( World & wd, const Mut & m, bool loud )
{
	using namespace WwVkPresent;
	int fails = 0;
	auto bad = [&]( const std::string & what ) {
		fails++;
		if ( loud )
			std::printf( "WATERVIEW FAIL %s\n", what.c_str() );
	};
	WaterCaps caps = waterCaps();
	if ( m.bc )
		caps.bc = true;
	if ( m.intF )
		caps.intFormats = false;  // lane VKWATER2: the integer rows lost
	if ( m.tess )
		caps.tess = true;
	std::vector<WaterCensus> cs = wd.cs;
	if ( m.empty )
		for ( auto & c : cs )
			c.inputs.clear();
	if ( m.drop && !cs.empty() && !cs[0].inputs.empty() )
		cs[0].inputs.erase( cs[0].inputs.begin() + 1 );
	if ( m.swap && !cs.empty() && cs[0].inputs.size() > 2 ) {
		std::swap( cs[0].inputs[0].tex, cs[0].inputs[2].tex );
		std::swap( cs[0].inputs[0].glFmt, cs[0].inputs[2].glFmt );
	}

	// device: the caps against the features vkpresent.cpp enables
	const struct { const char * tok; bool cap; } dev[] = { { "en.textureCompressionBC", caps.bc },
		{ "en.imageCubeArray", caps.cubeArray }, { "en.samplerAnisotropy", caps.aniso }, { "en.tessellationShader", caps.tess } };
	for ( auto & d : dev )
		if ( has( wd.src, d.tok ) != d.cap )
			bad( std::string( "device " ) + d.tok + ( d.cap ? " claimed, not enabled" : " enabled, not claimed" ) );

	// census + coverage
	std::map<std::string, int> seen;
	std::map<std::string, std::string> byClass;
	for ( std::size_t p = 0; p < cs.size(); p++ ) {
		const WaterCensus & c = cs[p];
		int expectN = 0;
		for ( auto & x : wd.ins )
			if ( x.prog == int( p ) )
				expectN++;
		if ( c.inputs.empty() )
			bad( "census program " + std::to_string( p ) + " is empty" );
		if ( int( c.inputs.size() ) != expectN || c.samplersSeen != expectN )
			bad( "census program " + std::to_string( p ) + " has " + std::to_string( c.inputs.size() ) + " inputs ("
				+ std::to_string( c.samplersSeen ) + " samplers seen), bound " + std::to_string( expectN ) );
		if ( c.tess )
			bad( "census program " + std::to_string( p ) + " reads tess" );
		std::vector<int> blocks;
		const std::string verdict = waterVerdict( c, caps, &blocks );
		if ( !has( verdict, "the shaded draw" ) )
			bad( "verdict of program " + std::to_string( p ) + " does not name the shaded draw" );
		for ( std::size_t i = 0; i < c.inputs.size(); i++ ) {
			const WaterSlot & s = c.inputs[i];
			const In * x = nullptr;
			for ( auto & y : wd.ins )
				if ( y.name == s.name && y.prog == int( p ) )
					x = &y;
			if ( !x ) {
				bad( "census input " + s.name + " was not bound in program " + std::to_string( p ) );
				continue;
			}
			seen[s.name]++;
			const int ew = x->kind == 5 ? 64 : x->w, eh = x->kind == 5 ? 1 : x->h;
			const int ed = ( x->kind == 1 || x->kind == 2 || x->kind == 4 ) ? x->d : 1;
			if ( s.kind != x->kind || s.unit != x->unit || s.tex != x->tex || s.glFmt != x->fmt || s.w != ew || s.h != eh
				|| s.d != ed || ( x->kind != 5 && s.levels != x->levels ) || ( x->kind != 5 && s.aniso != x->aniso )
				|| s.samplerObj ) {
				char b[400];
				std::snprintf( b, sizeof( b ),
					"census %s: kind %d/%d unit %d/%d tex %u/%u fmt 0x%X/0x%X size %dx%dx%d/%dx%dx%d levels %d/%d aniso %g/%g",
					s.name.c_str(), s.kind, x->kind, s.unit, x->unit, s.tex, x->tex, s.glFmt, x->fmt, s.w, s.h, s.d, ew, eh, ed,
					s.levels, x->levels, double( s.aniso ), double( x->aniso ) );
				bad( b );
			}
			const int b = blocks[i];
			const int want = x->expect;
			if ( b != want )
				bad( "verdict " + s.name + " reads " + waterBlockName( b ) + ", the fixture " + waterBlockName( want ) );
			if ( b != kWaterOk && !has( verdict, " " + s.name ) )
				bad( "verdict of program " + std::to_string( p ) + " does not name " + s.name );
			if ( b != kWaterOk )
				byClass[waterBlockName( b )] += " " + s.name;
			// share: what the verdict lets move must be taken; a format refusal must be refused by sharedTarget too
			if ( s.kind >= 0 && s.kind <= 2 && s.tex && ( b == kWaterOk || b == kWaterBc || b == kWaterIntFormat
				|| b == kWaterFormat ) ) {
				static unsigned keys[256];
				const std::uintptr_t key = reinterpret_cast<std::uintptr_t>( &keys[( p * 32 + i ) & 255] );
				unsigned t = 0;
				std::string why;
				const bool took = sharedTarget( key, s.kind, s.w, s.h, s.d, s.glFmt, t, why, s.levels );
				if ( took )
					sharedTargetDrop( key );
				if ( b == kWaterOk && !took )
					bad( "share " + s.name + " moves by the verdict, sharedTarget refuses (" + why + ")" );
				if ( b != kWaterOk && took )
					bad( "share " + s.name + " refused by the verdict (" + waterBlockName( b ) + "), sharedTarget takes it" );
			}
		}
	}
	for ( auto & x : wd.ins )
		if ( seen[x.name] != 1 )
			bad( "coverage " + x.name + " in " + std::to_string( seen[x.name] ) + " censuses" );

	// tess
	if ( !wd.tessC.tess )
		bad( "tess program reads no tessellation stage" );
	if ( !has( waterVerdict( wd.tessC, caps ), "the tessellated twin" ) )
		bad( "tess verdict does not name the tessellated twin" );

	if ( loud )
		for ( auto & kv : byClass )
			std::printf( "WATERVIEW BLOCKED %s:%s\n", kv.first.c_str(), kv.second.c_str() );
	return fails;
}
}  // namespace wtv

int main( int argc, char ** argv )
{
	using namespace wtv;
	using namespace WwVkPresent;
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	if ( argc < 3 ) {
		std::printf( "usage: waterview <tree root> <slots.txt>\nWATERVIEW GATE FAILED\n" );
		return 2;
	}
	World wd;
	{
		std::ifstream f( std::string( argv[1] ) + "/src/features/vkframe/vkpresent.cpp", std::ios::binary );
		std::stringstream ss;
		ss << f.rdbuf();
		wd.src = ss.str();
	}
	{
		std::ifstream f( argv[2] );
		std::string line;
		while ( std::getline( f, line ) ) {
			std::istringstream ls( line );
			In x;
			std::string t, bind;
			if ( !( ls >> x.name >> t >> bind >> x.tags ) )
				continue;
			x.glslType = ( t[0] == 'u' || t[0] == 'i' ) ? std::string( 1, t[0] ) + "sampler" + t.substr( 8 ) : "sampler" + t.substr( 7 );
			wd.ins.push_back( x );
		}
	}
	if ( wd.src.empty() || wd.ins.size() < 8 ) {
		std::printf( "WATERVIEW GATE FAILED (vkpresent.cpp %zu bytes, %zu inputs in slots.txt)\n", wd.src.size(), wd.ins.size() );
		return 2;
	}
	const std::size_t fromTwins = wd.ins.size();
	for ( const char * e : { "cellLights samplerBuffer", "cellShadow samplerCubeArrayShadow" } ) {
		In x;
		std::istringstream ls( e );
		ls >> x.name >> x.glslType;
		x.tags = "glsl";
		wd.ins.push_back( x );
	}
	for ( auto & x : wd.ins ) {
		fixtureOf( x );
		fixtureNoisePlain( x );
	}

	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwwaterview";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwwaterview", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	const int W = 128, H = 128, S = 4;
	RECT rc { 0, 0, W, H };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwwaterview", "water view harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
		rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	const int attr[] = { 0x2001, 1, 0x2010, 1, 0x2011, 1, 0x2003, 0x2027, 0x2013, 0x202B, 0x2015, 8, 0x2017, 8, 0x2019, 8,
		0x201B, 8, 0x2022, 24, 0x2023, 8, 0x2041, 1, 0x2042, 4, 0 };
	int fmt = 0;
	UINT nf = 0;
	if ( !choose || !choose( dc, attr, nullptr, 1, &fmt, &nf ) || !nf ) {
		std::printf( "WATERVIEW GATE FAILED (no 4x MSAA pixel format)\n" );
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
	if ( !loadAll() ) {
		std::printf( "WATERVIEW GATE FAILED (GL entry points)\n" );
		return 2;
	}
	std::printf( "GL on %s; %zu inputs from the twins + %zu GLSL-only\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ),
		fromTwins, wd.ins.size() - fromTwins );
	std::string why;
	if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) ) {
		std::printf( "WATERVIEW GATE FAILED (the view: %s)\n", why.c_str() );
		return 2;
	}

	// textures, then programs of <= 16 samplers on scattered units (3 i + 5 mod 32: no unit equals its index)
	for ( auto & x : wd.ins )
		if ( !makeTex( x ) ) {
			std::printf( "WATERVIEW GATE FAILED (texture %s fmt 0x%X)\n", x.name.c_str(), x.fmt );
			return 2;
		}
	const int per = 16;
	for ( std::size_t b = 0; b < wd.ins.size(); b += per ) {
		std::string fs = "#version 450\nout vec4 wwOut;\n", body;
		const int p = int( wd.progs.size() );
		for ( std::size_t i = b; i < wd.ins.size() && i < b + per; i++ ) {
			In & x = wd.ins[i];
			x.prog = p;
			x.unit = int( ( 3 * ( i - b ) + 5 ) % 32 );
			fs += "layout( binding = " + std::to_string( x.unit ) + " ) uniform " + x.glslType + " " + x.name + ";\n";
			body += "\ta += " + sampleOf( x ) + ";\n";
		}
		fs += "void main()\n{\n\tvec4 a = vec4( 0.0 );\n" + body + "\twwOut = a;\n}\n";
		std::string log;
		const GLuint pr = program( { { 0x8B31, kVs }, { 0x8B30, fs } }, log );
		if ( !pr ) {
			std::printf( "WATERVIEW GATE FAILED (program %d: %s)\n%s", p, log.c_str(), fs.c_str() );
			return 2;
		}
		wd.progs.push_back( pr );
	}
	{
		std::string log;
		wd.tessProg = program( { { 0x8B31, kVs },
			{ 0x8E88, "#version 450\nlayout( vertices = 3 ) out;\nvoid main() { gl_TessLevelOuter[0] = 1.0; gl_TessLevelOuter[1] = 1.0;"
					  " gl_TessLevelOuter[2] = 1.0; gl_TessLevelInner[0] = 1.0; }\n" },
			{ 0x8E87, "#version 450\nlayout( triangles ) in;\nvoid main() { gl_Position = vec4( gl_TessCoord, 1.0 ); }\n" },
			{ 0x8B30, "#version 450\nout vec4 wwOut;\nvoid main() { wwOut = vec4( 1.0 ); }\n" } }, log );
		if ( !wd.tessProg ) {
			std::printf( "WATERVIEW GATE FAILED (tess program: %s)\n", log.c_str() );
			return 2;
		}
	}

	// the census, as the app takes it: program in use, every unit bound
	double ms = 0.0;
	for ( std::size_t p = 0; p < wd.progs.size(); p++ ) {
		g.glUseProgram( wd.progs[p] );
		for ( auto & x : wd.ins )
			if ( x.prog == int( p ) ) {
				g.glActiveTexture( 0x84C0 + unsigned( x.unit ) );
				glBindTexture( targetOf( x.kind ), x.tex );
			}
		g.glActiveTexture( 0x84C0 );
		WaterCensus c;
		const auto t0 = std::chrono::steady_clock::now();
		waterCensusOf( &g, wd.progs[p], c );
		ms += std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t0 ).count();
		wd.cs.push_back( c );
		GLint act = 0;
		g.glGetIntegerv( 0x84E0, &act );
		if ( act != 0x84C0 ) {
			std::printf( "WATERVIEW FAIL census left texture unit 0x%X active\nWATERVIEW GATE FAILED\n", act );
			return 1;
		}
	}
	g.glUseProgram( wd.tessProg );
	waterCensusOf( &g, wd.tessProg, wd.tessC );
	std::printf( "census: %zu programs, %.3f ms each\n", wd.progs.size(), ms / double( wd.progs.size() ) );
	{
		WaterCensus all;
		for ( auto & c : wd.cs )
			all.inputs.insert( all.inputs.end(), c.inputs.begin(), c.inputs.end() );
		all.loose = 245;
		std::printf( "verdict: %s\n", waterVerdict( all, waterCaps() ).c_str() );
	}

	const int green = evaluate( wd, Mut(), true );
	std::printf( "WATERVIEW GREEN %s (%d failures)\n", green ? "FAIL" : "OK", green );
	bool redsOk = true;
	Mut reds[6];
	reds[0].name = "bc";    reds[0].bc = true;
	reds[1].name = "int";   reds[1].intF = true;
	reds[2].name = "drop";  reds[2].drop = true;
	reds[3].name = "unit";  reds[3].swap = true;
	reds[4].name = "tess";  reds[4].tess = true;
	reds[5].name = "empty"; reds[5].empty = true;
	for ( auto & r : reds ) {
		const int f = evaluate( wd, r, false );
		std::printf( "WATERVIEW RED %s %s (%d failures)\n", r.name.c_str(), f ? "FAIL" : "PASSED (must fail)", f );
		redsOk = redsOk && f > 0;
	}
	for ( auto & x : wd.ins )
		glDeleteTextures( 1, &x.tex );
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	ReleaseDC( wnd, dc );
	DestroyWindow( wnd );
	const bool ok = green == 0 && redsOk;
	std::printf( "WATERVIEW GATE %s\n", ok ? "OK" : "FAILED" );
	return ok ? 0 : 1;
}

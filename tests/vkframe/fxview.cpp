// Lane VKAIR1 (2026-11-13, rows L5 and L6): the air over the sky (volSkyRun) and the clouds' march + resolve (cloudRun)
// on the present device (vkpresent.cpp, built in here with WW_VKPRESENT_NO_QT: the shipped code) against GL's own programs
// (the tree's .frag with the loader's defines, written by fxview_prep.py) on the same inputs.
// Usage: fxview volsky <prep dir> | fxview cloud <prep dir>
// The Vulkan side takes what the app's call sites take: fxPack over the GL program's own uniform values, fxTexOf over the
// units GL samples, shared targets where the app shares them (the air's packed volume; the clouds' march and history).
// volsky: a 4x MSAA RGBA16F + D24S8 frame cleared to a sky colour, the pass drawn with cellvolair.cpp's state (stencil
//   ALWAYS ref 1 REPLACE mask 0x03, no depth test, blend ONE / ONE_MINUS_SRC_ALPHA (alpha ZERO, ONE) over the sky), then a
//   strip drawn where stencil == 1; per sample. Bar (set before the first run, never widened): every half within 2 ulp,
//   <= 0.1 % of halves past 1 ulp.
// cloud: GL's march at the history size seeds hist[1]; the march at the stride-4 size and the resolve into hist[0] are the
//   reference. Bars: the march colour, the march depth and hist[0] each under frag_gate's MARCH_RULE (values with rel >
//   1e-3 <= 1e-4 of all); the resolve on GL's own march (red bit 4: the march targets filled from GL's) EXACT.
// Reds (each must FAIL): volsky stencil ref 2, no blend over the sky, volAirMed zeroed; cloud shape / detail swapped,
// the resolve's march / history swapped. Last line `VOLSKYVIEW GATE OK|FAILED` or `CLOUDVIEW GATE OK|FAILED`.
#define WW_VKPRESENT_NO_QT
#include "../../src/features/vkframe/vkpresent.cpp"

#include <GL/gl.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace fv
{
const char * gTag = "FXVIEW";
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

constexpr GLenum fT2DMS = 0x9100, fT3D = 0x806F, fT2DA = 0x8C1A, fFb = 0x8D40, fCol0 = 0x8CE0, fDsAtt = 0x821A;
constexpr GLenum fD24s8 = 0x88F0, fRgba16f = 0x881A, fRgba32f = 0x8814, fRg32f = 0x8230, fR32f = 0x822E, fR8 = 0x8229;
constexpr GLenum fRgba8 = 0x8058, fRed = 0x1903, fRg = 0x8227, fClamp = 0x812F, fWrapR = 0x8072, fTex0 = 0x84C0;
constexpr GLenum fFrag = 0x8B30, fVert = 0x8B31, fCompiled = 0x8B81, fLinked = 0x8B82, fActiveUnis = 0x8B86;
constexpr GLenum fComplete = 0x8CD5, fMaxLevel = 0x813D, fHalf = 0x140B, fUbo = 0x8A11, fBlockSize = 0x8A40, fStatic = 0x88E4;
constexpr GLenum fBlocks = 0x8A36, fBlockIndex = 0x8A3A, fMipLinear = 0x2703;

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
	void( WINAPI * getActiveUniform )( GLuint, GLuint, GLsizei, GLsizei *, GLint *, GLenum *, char * ) = nullptr;
	void( WINAPI * getActiveUniformsiv )( GLuint, GLsizei, const GLuint *, GLenum, GLint * ) = nullptr;
	GLint( WINAPI * getUniformLocation )( GLuint, const char * ) = nullptr;
	void( WINAPI * uniform1i )( GLint, GLint ) = nullptr;
	void( WINAPI * uniform1iv )( GLint, GLsizei, const GLint * ) = nullptr;
	void( WINAPI * uniform2iv )( GLint, GLsizei, const GLint * ) = nullptr;
	void( WINAPI * uniform3iv )( GLint, GLsizei, const GLint * ) = nullptr;
	void( WINAPI * uniform4iv )( GLint, GLsizei, const GLint * ) = nullptr;
	void( WINAPI * uniform1uiv )( GLint, GLsizei, const GLuint * ) = nullptr;
	void( WINAPI * uniform1fv )( GLint, GLsizei, const GLfloat * ) = nullptr;
	void( WINAPI * uniform2fv )( GLint, GLsizei, const GLfloat * ) = nullptr;
	void( WINAPI * uniform3fv )( GLint, GLsizei, const GLfloat * ) = nullptr;
	void( WINAPI * uniform4fv )( GLint, GLsizei, const GLfloat * ) = nullptr;
	void( WINAPI * uniformMatrix3fv )( GLint, GLsizei, GLboolean, const GLfloat * ) = nullptr;
	void( WINAPI * uniformMatrix4fv )( GLint, GLsizei, GLboolean, const GLfloat * ) = nullptr;
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
	void( WINAPI * texImage3D )( GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void * ) = nullptr;
	void( WINAPI * texSubImage3D )( GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void * ) = nullptr;
	void( WINAPI * generateMipmap )( GLenum ) = nullptr;
	void( WINAPI * genFramebuffers )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindFramebuffer )( GLenum, GLuint ) = nullptr;
	void( WINAPI * framebufferTexture2D )( GLenum, GLenum, GLenum, GLuint, GLint ) = nullptr;
	GLenum( WINAPI * checkFramebufferStatus )( GLenum ) = nullptr;
	void( WINAPI * drawBuffers )( GLsizei, const GLenum * ) = nullptr;
	void( WINAPI * blendFuncSeparate )( GLenum, GLenum, GLenum, GLenum ) = nullptr;
	void( WINAPI * copyImageSubData )( GLuint, GLenum, GLint, GLint, GLint, GLint, GLuint, GLenum, GLint, GLint, GLint, GLint,
		GLsizei, GLsizei, GLsizei ) = nullptr;
};
Fns f;

// the GL functions fxPack / fxTexOf call, named as GL names them (the app hands Qt's 4.x core functions)
struct GlA
{
	GLint( WINAPI * glGetUniformLocation )( GLuint, const char * ) = nullptr;
	void( WINAPI * glGetUniformiv )( GLuint, GLint, GLint * ) = nullptr;
	void( WINAPI * glGetUniformfv )( GLuint, GLint, GLfloat * ) = nullptr;
	void( WINAPI * glGetIntegerv )( GLenum, GLint * ) = nullptr;
	void( WINAPI * glActiveTexture )( GLenum ) = nullptr;
	void( WINAPI * glGetTexLevelParameteriv )( GLenum, GLint, GLenum, GLint * ) = nullptr;
	void( WINAPI * glGetTexParameteriv )( GLenum, GLenum, GLint * ) = nullptr;
};
GlA ga;

template <class P> bool load( P & p, const char * n )
{
	p = reinterpret_cast<P>( glProc( n ) );
	return p != nullptr;
}

bool loadAll()
{
	bool ok = load( f.createShader, "glCreateShader" ) && load( f.shaderSource, "glShaderSource" )
		&& load( f.compileShader, "glCompileShader" ) && load( f.getShaderiv, "glGetShaderiv" )
		&& load( f.getShaderInfoLog, "glGetShaderInfoLog" ) && load( f.createProgram, "glCreateProgram" )
		&& load( f.attachShader, "glAttachShader" ) && load( f.linkProgram, "glLinkProgram" )
		&& load( f.getProgramiv, "glGetProgramiv" ) && load( f.getProgramInfoLog, "glGetProgramInfoLog" )
		&& load( f.useProgram, "glUseProgram" ) && load( f.getActiveUniform, "glGetActiveUniform" )
		&& load( f.getActiveUniformsiv, "glGetActiveUniformsiv" ) && load( f.getUniformLocation, "glGetUniformLocation" )
		&& load( f.uniform1i, "glUniform1i" ) && load( f.uniform1iv, "glUniform1iv" ) && load( f.uniform2iv, "glUniform2iv" )
		&& load( f.uniform3iv, "glUniform3iv" ) && load( f.uniform4iv, "glUniform4iv" ) && load( f.uniform1uiv, "glUniform1uiv" )
		&& load( f.uniform1fv, "glUniform1fv" ) && load( f.uniform2fv, "glUniform2fv" ) && load( f.uniform3fv, "glUniform3fv" )
		&& load( f.uniform4fv, "glUniform4fv" ) && load( f.uniformMatrix3fv, "glUniformMatrix3fv" )
		&& load( f.uniformMatrix4fv, "glUniformMatrix4fv" ) && load( f.getUniformfv, "glGetUniformfv" )
		&& load( f.getUniformiv, "glGetUniformiv" ) && load( f.getUniformBlockIndex, "glGetUniformBlockIndex" )
		&& load( f.uniformBlockBinding, "glUniformBlockBinding" )
		&& load( f.getActiveUniformBlockiv, "glGetActiveUniformBlockiv" ) && load( f.genBuffers, "glGenBuffers" )
		&& load( f.bindBuffer, "glBindBuffer" ) && load( f.bufferData, "glBufferData" )
		&& load( f.bindBufferBase, "glBindBufferBase" ) && load( f.activeTexture, "glActiveTexture" )
		&& load( f.texImage2DMultisample, "glTexImage2DMultisample" ) && load( f.texImage3D, "glTexImage3D" )
		&& load( f.texSubImage3D, "glTexSubImage3D" ) && load( f.generateMipmap, "glGenerateMipmap" )
		&& load( f.genFramebuffers, "glGenFramebuffers" ) && load( f.bindFramebuffer, "glBindFramebuffer" )
		&& load( f.framebufferTexture2D, "glFramebufferTexture2D" )
		&& load( f.checkFramebufferStatus, "glCheckFramebufferStatus" ) && load( f.drawBuffers, "glDrawBuffers" )
		&& load( f.blendFuncSeparate, "glBlendFuncSeparate" ) && load( f.copyImageSubData, "glCopyImageSubData" );
	ga.glGetUniformLocation = f.getUniformLocation;
	ga.glGetUniformiv = f.getUniformiv;
	ga.glGetUniformfv = f.getUniformfv;
	ga.glGetIntegerv = &::glGetIntegerv;
	ga.glActiveTexture = f.activeTexture;
	ga.glGetTexLevelParameteriv = &::glGetTexLevelParameteriv;
	ga.glGetTexParameteriv = &::glGetTexParameteriv;
	return ok;
}

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
	f.getShaderiv( s, fCompiled, &ok );
	if ( !ok ) {
		std::vector<char> log( 65536 );
		f.getShaderInfoLog( s, GLsizei( log.size() ), nullptr, log.data() );
		std::printf( "%s compile failed (%s): %.1500s\n", gTag, tag, log.data() );
		return 0;
	}
	return s;
}

GLuint program( const std::string & vs, const std::string & fs, const char * tag )
{
	if ( fs.empty() ) {
		std::printf( "%s no source (%s)\n", gTag, tag );
		return 0;
	}
	const GLuint a = shader( fVert, vs, tag ), b = shader( fFrag, fs, tag );
	if ( !a || !b )
		return 0;
	const GLuint p = f.createProgram();
	f.attachShader( p, a );
	f.attachShader( p, b );
	f.linkProgram( p );
	GLint ok = 0;
	f.getProgramiv( p, fLinked, &ok );
	if ( !ok ) {
		std::vector<char> log( 65536 );
		f.getProgramInfoLog( p, GLsizei( log.size() ), nullptr, log.data() );
		std::printf( "%s link failed (%s): %.1500s\n", gTag, tag, log.data() );
		return 0;
	}
	return p;
}

void u1i( GLuint p, const char * n, int v )
{
	f.useProgram( p );
	const GLint l = f.getUniformLocation( p, n );
	if ( l >= 0 )
		f.uniform1i( l, v );
}

// ---- the job files ----------------------------------------------------------------------------------------------
using UniMap = std::map<std::string, std::vector<double>>;
UniMap readUni( const std::string & path )
{
	UniMap m;
	std::ifstream in( path );
	std::string line;
	while ( std::getline( in, line ) ) {
		std::istringstream ls( line );
		std::string name;
		if ( !( ls >> name ) )
			continue;
		std::vector<double> v;
		double x;
		while ( ls >> x )
			v.push_back( x );
		m[name] = v;
	}
	return m;
}

// every active loose uniform the map names, by its GL type (block members and samplers skipped); returns the count set
int setUnis( GLuint p, const UniMap & m )
{
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
		const auto it = m.find( base );
		if ( it == m.end() )
			continue;
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
		case 0x8B5B: kind = '3'; comp = 9; break;
		case 0x8B5C: kind = '4'; comp = 16; break;
		default: continue;  // a sampler: the caller sets its unit
		}
		const std::vector<double> & v = it->second;
		const GLsizei count = GLsizei( std::min<size_t>( size_t( sz ), v.size() / size_t( comp ) ) );
		if ( count < 1 )
			continue;
		const GLint l = f.getUniformLocation( p, nm );
		if ( kind == 'i' || kind == 'u' ) {
			std::vector<GLint> a( v.size() );
			for ( size_t k = 0; k < v.size(); k++ )
				a[k] = GLint( std::lround( v[k] ) );
			if ( kind == 'u' )
				f.uniform1uiv( l, count, reinterpret_cast<const GLuint *>( a.data() ) );
			else
				( comp == 1 ? f.uniform1iv : comp == 2 ? f.uniform2iv : comp == 3 ? f.uniform3iv : f.uniform4iv )( l, count, a.data() );
		} else {
			std::vector<GLfloat> a( v.begin(), v.end() );
			if ( kind == '3' )
				f.uniformMatrix3fv( l, count, GL_FALSE, a.data() );
			else if ( kind == '4' )
				f.uniformMatrix4fv( l, count, GL_FALSE, a.data() );
			else
				( comp == 1 ? f.uniform1fv : comp == 2 ? f.uniform2fv : comp == 3 ? f.uniform3fv : f.uniform4fv )( l, count, a.data() );
		}
		set++;
	}
	return set;
}

struct TexIn
{
	std::string name, kind, fmt, filt, wrap, file;
	int w = 0, h = 0, d = 1, mips = 0;
	GLuint gl = 0;
	GLenum target = GL_TEXTURE_2D;
	int type = 0;  // sharedTarget's: 0 2D, 1 3D, 2 2D array
};

bool fmtOf( const std::string & fmt, GLenum & ifmt, GLenum & pf, GLenum & ty, int & bpp )
{
	if ( fmt == "rgba32f" || fmt == "rgba16f" ) {  // rgba16f: floats already rounded to halves by the prep
		ifmt = fmt == "rgba32f" ? fRgba32f : fRgba16f;
		pf = GL_RGBA;
		ty = GL_FLOAT;
		bpp = 16;
	} else if ( fmt == "r32f" ) {
		ifmt = fR32f;
		pf = fRed;
		ty = GL_FLOAT;
		bpp = 4;
	} else if ( fmt == "rg32f" ) {
		ifmt = fRg32f;
		pf = fRg;
		ty = GL_FLOAT;
		bpp = 8;
	} else if ( fmt == "r8" ) {
		ifmt = fR8;
		pf = fRed;
		ty = GL_UNSIGNED_BYTE;
		bpp = 1;
	} else if ( fmt == "rgba8" ) {
		ifmt = fRgba8;
		pf = GL_RGBA;
		ty = GL_UNSIGNED_BYTE;
		bpp = 4;
	} else {
		return false;
	}
	return true;
}

std::vector<TexIn> readTex( const std::string & dir )
{
	std::vector<TexIn> out;
	std::ifstream in( dir + "/tex.txt" );
	std::string line;
	while ( std::getline( in, line ) ) {
		std::istringstream ls( line );
		TexIn t;
		if ( !( ls >> t.name >> t.kind >> t.fmt >> t.w >> t.h >> t.d >> t.filt >> t.wrap >> t.mips >> t.file ) )
			continue;
		t.target = t.kind == "3d" ? fT3D : t.kind == "2darray" ? fT2DA : GLenum( GL_TEXTURE_2D );
		t.type = t.kind == "3d" ? 1 : t.kind == "2darray" ? 2 : 0;
		out.push_back( t );
	}
	return out;
}

void texParams( GLenum target, bool linear, bool repeat, bool mips )
{
	glTexParameteri( target, GL_TEXTURE_MIN_FILTER, mips ? GLint( fMipLinear ) : linear ? GL_LINEAR : GL_NEAREST );
	glTexParameteri( target, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST );
	const GLint wr = repeat ? GL_REPEAT : GLint( fClamp );
	glTexParameteri( target, GL_TEXTURE_WRAP_S, wr );
	glTexParameteri( target, GL_TEXTURE_WRAP_T, wr );
	glTexParameteri( target, fWrapR, wr );
	if ( !mips )
		glTexParameteri( target, fMaxLevel, 0 );
}

// the texture's data into a new GL name, or (into != 0) into that existing level-0 storage (a shared target)
bool upload( const std::string & dir, TexIn & t, GLuint into )
{
	GLenum ifmt, pf, ty;
	int bpp = 0;
	if ( !fmtOf( t.fmt, ifmt, pf, ty, bpp ) ) {
		std::printf( "%s texture %s: format %s\n", gTag, t.name.c_str(), t.fmt.c_str() );
		return false;
	}
	const std::string data = readFile( dir + "/" + t.file );
	if ( data.size() != size_t( t.w ) * t.h * t.d * bpp ) {
		std::printf( "%s texture %s: %zu bytes, want %d\n", gTag, t.name.c_str(), data.size(), t.w * t.h * t.d * bpp );
		return false;
	}
	GLuint x = into;
	if ( !x )
		glGenTextures( 1, &x );
	glBindTexture( t.target, x );
	if ( into ) {
		f.texSubImage3D( t.target, 0, 0, 0, 0, t.w, t.h, t.d, pf, ty, data.data() );
	} else if ( t.target == GL_TEXTURE_2D ) {
		glTexImage2D( GL_TEXTURE_2D, 0, GLint( ifmt ), t.w, t.h, 0, pf, ty, data.data() );
	} else {
		f.texImage3D( t.target, 0, GLint( ifmt ), t.w, t.h, t.d, 0, pf, ty, data.data() );
	}
	texParams( t.target, t.filt == "linear", t.wrap == "repeat", t.mips != 0 );
	if ( t.mips )
		f.generateMipmap( t.target );
	t.gl = x;
	return glGetError() == GL_NO_ERROR;
}

GLuint tex2d( int w, int hgt, GLenum ifmt, GLenum fmt, GLenum typ, const void * data, bool linear )
{
	GLuint t = 0;
	glGenTextures( 1, &t );
	glBindTexture( GL_TEXTURE_2D, t );
	glTexImage2D( GL_TEXTURE_2D, 0, GLint( ifmt ), w, hgt, 0, fmt, typ, data );
	texParams( GL_TEXTURE_2D, linear, false, false );
	return t;
}

GLuint msTex( int w, int hgt, GLenum ifmt, int s )
{
	GLuint t = 0;
	glGenTextures( 1, &t );
	glBindTexture( fT2DMS, t );
	f.texImage2DMultisample( fT2DMS, s, ifmt, w, hgt, GL_TRUE );
	glBindTexture( fT2DMS, 0 );
	return t;
}

GLuint fbo( GLenum colType, GLuint col, GLuint col1, GLuint ds )
{
	GLuint b = 0;
	f.genFramebuffers( 1, &b );
	f.bindFramebuffer( fFb, b );
	f.framebufferTexture2D( fFb, fCol0, colType, col, 0 );
	if ( col1 )
		f.framebufferTexture2D( fFb, fCol0 + 1, colType, col1, 0 );
	if ( ds )
		f.framebufferTexture2D( fFb, fDsAtt, fT2DMS, ds, 0 );
	const GLenum bufs[2] = { fCol0, fCol0 + 1 };
	f.drawBuffers( col1 ? 2 : 1, bufs );
	if ( f.checkFramebufferStatus( fFb ) != fComplete ) {
		std::printf( "%s framebuffer incomplete\n", gTag );
		return 0;
	}
	return b;
}

void bindUnit( int unit, GLenum type, GLuint t )
{
	f.activeTexture( fTex0 + GLenum( unit ) );
	glBindTexture( type, t );
	f.activeTexture( fTex0 );
}

const char * kTriVs = "#version 410 core\n"
	"void main() { vec2 p = vec2( float( ( gl_VertexID << 1 ) & 2 ), float( gl_VertexID & 2 ) ); "
	"gl_Position = vec4( p * 2.0 - 1.0, 0.0, 1.0 ); }\n";
const char * kObjFs = "#version 410 core\nout vec4 o; void main() { o = vec4( 0.25, 0.5, 0.75, 1.0 ); }\n";
const char * kUnpackFs = "#version 410 core\n"
	"uniform sampler2DMS src; uniform int S; out vec4 o;\n"
	"void main() { ivec2 q = ivec2( gl_FragCoord.xy ); o = texelFetch( src, ivec2( q.x / S, q.y ), q.x % S ); }\n";

void drawState0()
{
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_STENCIL_TEST );
	glDisable( GL_BLEND );
	glDisable( GL_SCISSOR_TEST );
	glDisable( GL_CULL_FACE );
	glDepthMask( GL_FALSE );
	glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
}

// ---- compares ---------------------------------------------------------------------------------------------------
struct Halves
{
	size_t total = 0, diff = 0, past1 = 0, past2 = 0;
	int maxu = 0;
	bool ok() const { return total > 0 && maxu <= 2 && past1 * 1000 <= total; }
	const char * word() const { return total == 0 ? "FAIL" : diff == 0 ? "EXACT" : ok() ? "BOUND" : "FAIL"; }
};

int ordered( std::uint16_t hv )
{
	return ( hv & 0x8000 ) ? -int( hv & 0x7FFF ) : int( hv );
}

Halves compareHalves( const std::vector<std::uint16_t> & a, const std::vector<std::uint16_t> & b )
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
		v.past2 += d > 2;
		v.maxu = std::max( v.maxu, d );
	}
	return v;
}

float h2f( std::uint16_t h )
{
	const std::uint32_t s = std::uint32_t( h & 0x8000 ) << 16, e = ( h >> 10 ) & 0x1F, m = h & 0x3FF;
	std::uint32_t bits;
	if ( e == 0 ) {
		if ( m == 0 ) {
			bits = s;
		} else {  // subnormal
			int ee = -1;
			std::uint32_t mm = m;
			do {
				ee++;
				mm <<= 1;
			} while ( !( mm & 0x400 ) );
			bits = s | ( std::uint32_t( 127 - 15 - ee ) << 23 ) | ( ( mm & 0x3FF ) << 13 );
		}
	} else if ( e == 31 ) {
		bits = s | 0x7F800000u | ( m << 13 );
	} else {
		bits = s | ( ( e + 112 ) << 23 ) | ( m << 13 );
	}
	float r;
	std::memcpy( &r, &bits, 4 );
	return r;
}

// frag_gate.py's MARCH_RULE: rel = |a - b| / max(|a|, |b|) per value (bit-equal = 0, NaN vs number = 1); values with
// rel > 1e-3 must be <= 1e-4 of all
struct Rel
{
	size_t n = 0, differ = 0, past = 0;
	double maxRel = 0.0;
	bool ok() const { return n > 0 && double( past ) <= 1e-4 * double( n ); }
	const char * word() const { return n == 0 ? "FAIL" : differ == 0 ? "EXACT" : ok() ? "BOUND" : "FAIL"; }
};

Rel compareRel( const std::vector<float> & a, const std::vector<float> & b )
{
	Rel r;
	if ( a.size() != b.size() || a.empty() )
		return r;
	r.n = a.size();
	for ( size_t i = 0; i < a.size(); i++ ) {
		std::uint32_t ia, ib;
		std::memcpy( &ia, &a[i], 4 );
		std::memcpy( &ib, &b[i], 4 );
		if ( ia == ib )
			continue;
		const bool na = std::isnan( a[i] ), nb = std::isnan( b[i] );
		if ( na && nb )
			continue;
		r.differ++;
		double rel = 1.0;
		if ( !na && !nb ) {
			const double m = std::max( std::max( std::fabs( double( a[i] ) ), std::fabs( double( b[i] ) ) ), 1e-30 );
			rel = std::fabs( double( a[i] ) - double( b[i] ) ) / m;
			if ( !std::isfinite( rel ) )
				rel = 1.0;
		}
		r.past += rel > 1e-3;
		r.maxRel = std::max( r.maxRel, rel );
	}
	return r;
}

std::vector<float> readRgba16f( GLuint t, int w, int h )
{
	std::vector<std::uint16_t> hv( size_t( w ) * h * 4 );
	glBindTexture( GL_TEXTURE_2D, t );
	glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, fHalf, hv.data() );
	std::vector<float> out( hv.size() );
	for ( size_t i = 0; i < hv.size(); i++ )
		out[i] = h2f( hv[i] );
	return out;
}

std::vector<float> readRg32f( GLuint t, int w, int h )
{
	std::vector<float> out( size_t( w ) * h * 2 );
	glBindTexture( GL_TEXTURE_2D, t );
	glGetTexImage( GL_TEXTURE_2D, 0, fRg, GL_FLOAT, out.data() );
	return out;
}

bool readJob( const std::string & path, std::vector<int> & v, size_t n )
{
	std::ifstream in( path );
	int x;
	while ( in >> x )
		v.push_back( x );
	return v.size() >= n;
}
}  // namespace fv

int main( int argc, char ** argv )
{
	using namespace fv;
	const std::string mode = argc > 1 ? argv[1] : "";
	const bool cloud = mode == "cloud";
	gTag = cloud ? "CLOUDVIEW" : "VOLSKYVIEW";
	if ( argc < 3 || ( mode != "volsky" && mode != "cloud" ) ) {
		std::printf( "usage: fxview volsky|cloud <prep dir>\nFXVIEW GATE FAILED\n" );
		return 2;
	}
	const std::string dir = argv[2];
	auto fatal = [&]( const std::string & why ) {
		std::printf( "%s GATE FAILED (%s)\n", gTag, why.c_str() );
		return 2;
	};
	const int S = 4;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwfxview";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwfxview", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, 320, 180 };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwfxview", "fx view harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
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
	if ( !loadAll() )
		return fatal( "GL entry points" );
	std::printf( "%s GL on %s\n", gTag, reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	const GLuint pObj = program( kTriVs, kObjFs, "object" ), pUnpack = program( kTriVs, kUnpackFs, "unpack" );
	if ( !pObj || !pUnpack )
		return fatal( "GL helper programs" );
	std::string why;
	bool ok = true;

	if ( !cloud ) {
		// ---------------------------------------------------------------- L5: the air over the sky
		const char * jobs[2] = { "volsky_leak", "volsky_leak_near" };
		std::vector<int> j0;
		if ( !readJob( dir + "/" + jobs[0] + "/job.txt", j0, 3 ) )
			return fatal( "job.txt" );
		const int W = j0[0], H = j0[1], WS = W * S;
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
		if ( !fbFrame || !fbRef || !fbUnp )
			return fatal( "GL targets" );
		auto readSamples = [&]( GLuint msColor ) {
			std::vector<std::uint16_t> out( size_t( WS ) * H * 4 );
			f.bindFramebuffer( fFb, fbUnp );
			glViewport( 0, 0, WS, H );
			drawState0();
			f.useProgram( pUnpack );
			bindUnit( 15, fT2DMS, msColor );
			u1i( pUnpack, "src", 15 );
			u1i( pUnpack, "S", S );
			glDrawArrays( GL_TRIANGLES, 0, 3 );
			glReadPixels( 0, 0, WS, H, GL_RGBA, fHalf, out.data() );
			return out;
		};
		auto clearFrame = [&]( GLuint fb ) {
			f.bindFramebuffer( fFb, fb );
			glViewport( 0, 0, W, H );
			drawState0();
			glDepthMask( GL_TRUE );
			glStencilMask( 0xFF );
			glClearColor( 0.3f, 0.45f, 0.7f, 0.5f );  // the sky the air lays over (alpha kept: ZERO, ONE)
			glClearDepth( 1.0 );
			glClearStencil( 0 );
			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
		};
		// what reads the air's stencil: a strip drawn where stencil == 1 (the cell's pixels)
		auto strip = [&]( GLuint fb ) {
			f.bindFramebuffer( fFb, fb );
			glViewport( 0, 0, W, H );
			drawState0();
			glEnable( GL_SCISSOR_TEST );
			glScissor( 0, 0, W / 8, H );
			glEnable( GL_STENCIL_TEST );
			glStencilMask( 0 );
			glStencilFunc( GL_EQUAL, 1, 0xFF );
			glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
			f.useProgram( pObj );
			glDrawArrays( GL_TRIANGLES, 0, 3 );
			drawState0();
		};
		static char twinKeys[7], volKey;
		GLuint volShared = 0;
		GLuint ubo = 0;
		f.genBuffers( 1, &ubo );
		std::vector<std::uint16_t> refs[2];
		struct Prep
		{
			GLuint prog = 0;
			UniMap uni;
			std::vector<TexIn> tex;
			std::string global;
			int over = 0;
		} P[2];
		const char * slot[7] = { "atApTex", "volAirTerr", "volAirRainMap", "volTex", "volTile", "atSkyTransTex", "volAirNoise" };
		const int slotType[7] = { 1, 2, 0, 1, 0, 0, 1 };
		for ( int ji = 0; ji < 2; ji++ ) {
			const std::string jd = dir + "/" + jobs[ji];
			std::vector<int> jv;
			if ( !readJob( jd + "/job.txt", jv, 3 ) || jv[0] != W || jv[1] != H )
				return fatal( std::string( "job.txt of " ) + jobs[ji] );
			P[ji].over = jv[2];
			P[ji].prog = program( kTriVs, readFile( jd + "/prog.frag.glsl" ), jobs[ji] );
			if ( !P[ji].prog )
				return fatal( "GL program" );
			GLint nb = 0;
			f.getProgramiv( P[ji].prog, fBlocks, &nb );
			const GLuint bi = f.getUniformBlockIndex( P[ji].prog, "globalUniforms" );
			GLint size = 0;
			if ( bi != 0xFFFFFFFFu )
				f.getActiveUniformBlockiv( P[ji].prog, bi, fBlockSize, &size );
			if ( nb != 1 || size != WwVkPresent::kFxGlobalBytes )
				return fatal( "the program's blocks: " + std::to_string( nb ) + ", globalUniforms " + std::to_string( size ) + " bytes" );
			f.uniformBlockBinding( P[ji].prog, bi, 0 );
			P[ji].uni = readUni( jd + "/uni.txt" );
			P[ji].global = readFile( jd + "/global.bin" );
			if ( P[ji].global.size() != size_t( WwVkPresent::kFxGlobalBytes ) )
				return fatal( "global.bin" );
			P[ji].tex = readTex( jd );
			if ( P[ji].tex.size() != 7 )
				return fatal( "tex.txt" );
			for ( int k = 0; k < 7; k++ ) {
				TexIn & t = P[ji].tex[size_t( k )];
				if ( t.name != slot[k] || t.type != slotType[k] )
					return fatal( "tex.txt order at " + t.name );
				GLuint into = 0;
				if ( ji == 0 && k == 3 ) {  // the packed volume: a shared target, read by Vulkan with no copy (as the app's)
					if ( !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &volKey ), 1, t.w, t.h, t.d, fRgba16f, volShared, why ) )
						return fatal( "the shared volume: " + why );
					into = volShared;
				}
				if ( !upload( jd, t, into ) )
					return fatal( "upload " + t.name );
			}
			const int nset = setUnis( P[ji].prog, P[ji].uni );
			std::printf( "%s job %s: %dx%d x%d, overSky %d, %d uniforms set\n", gTag, jobs[ji], W, H, S, P[ji].over, nset );
		}
		auto bindJob = [&]( int ji ) {
			f.useProgram( P[ji].prog );
			setUnis( P[ji].prog, P[ji].uni );
			for ( int k = 0; k < 7; k++ ) {
				bindUnit( k, P[ji].tex[size_t( k )].target, P[ji].tex[size_t( k )].gl );
				u1i( P[ji].prog, slot[k], k );
			}
			f.bindBuffer( fUbo, ubo );
			f.bufferData( fUbo, WwVkPresent::kFxGlobalBytes, P[ji].global.data(), fStatic );
			f.bindBufferBase( fUbo, 0, ubo );
			f.useProgram( P[ji].prog );
		};
		// cellvolair.cpp's draw site, into GL's own frame
		auto runGl = [&]( int ji, std::vector<std::uint16_t> & out ) {
			clearFrame( fbRef );
			bindJob( ji );
			f.bindFramebuffer( fFb, fbRef );
			glViewport( 0, 0, W, H );
			drawState0();
			glEnable( GL_STENCIL_TEST );
			glStencilMask( 0x03 );
			glStencilFunc( GL_ALWAYS, 1, 0x03 );
			glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
			if ( P[ji].over ) {
				glEnable( GL_BLEND );
				f.blendFuncSeparate( GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE );
			}
			f.useProgram( P[ji].prog );
			glDrawArrays( GL_TRIANGLES, 0, 3 );
			drawState0();
			strip( fbRef );
			out = readSamples( refCol );
			return glGetError() == GL_NO_ERROR;
		};
		enum Red { kNone, kStencil, kNoBlend, kField };
		auto runVk = [&]( int ji, Red red, std::vector<std::uint16_t> & out ) -> bool {
			clearFrame( fbFrame );
			bindJob( ji );
			f.bindFramebuffer( fFb, fbFrame );
			glViewport( 0, 0, W, H );
			WwVkPresent::VolSkyRun t;
			t.w = W;
			t.h = H;
			glGetIntegerv( GL_VIEWPORT, t.vp );
			t.color = fc;
			t.overSky = P[ji].over != 0;
			for ( int k = 0; k < 7; k++ ) {
				if ( !WwVkPresent::fxTexOf( &ga, P[ji].prog, slot[k], slotType[k], t.tex[k], why ) ) {
					std::printf( "%s %s: fxTexOf refused: %s\n", gTag, jobs[ji], why.c_str() );
					return false;
				}
				t.tex[k].twin = reinterpret_cast<std::uintptr_t>( &twinKeys[k] );
				if ( k == 3 && WwVkPresent::sharedTargetIs( reinterpret_cast<std::uintptr_t>( &volKey ), t.tex[k].tex ) )
					t.tex[k].shared = reinterpret_cast<std::uintptr_t>( &volKey );
			}
			std::memcpy( t.global, P[ji].global.data(), size_t( WwVkPresent::kFxGlobalBytes ) );
			WwVkPresent::fxPack( &ga, P[ji].prog, WwVkPresent::kT_volsky, int( std::size( WwVkPresent::kT_volsky ) ), t.loose );
			if ( red == kField ) {
				for ( const WwVkPresent::FxField & fd : WwVkPresent::kT_volsky )
					if ( std::strcmp( fd.name, "volAirMed" ) == 0 )
						std::memset( t.loose + fd.offset, 0, 16 );
			}
			t.red = red == kStencil ? 1 : red == kNoBlend ? 2 : 0;
			f.bindFramebuffer( fFb, fbFrame );
			if ( !WwVkPresent::volSkyRun( t, why ) ) {
				std::printf( "%s %s: volSkyRun refused: %s\n", gTag, jobs[ji], why.c_str() );
				return false;
			}
			strip( fbFrame );
			out = readSamples( fc );
			return glGetError() == GL_NO_ERROR;
		};
		for ( int ji = 0; ji < 2; ji++ ) {
			std::vector<std::uint16_t> vk;
			if ( !runGl( ji, refs[ji] ) || !runVk( ji, kNone, vk ) ) {
				std::printf( "%s %s FAIL (a run failed)\n", gTag, jobs[ji] );
				ok = false;
				continue;
			}
			const Halves v = compareHalves( vk, refs[ji] );
			std::printf( "%s %s %s (%zu of %zu halves differ, %zu past 1 ulp, %zu past 2, max %d ulp; volume %s)\n", gTag, jobs[ji],
				v.word(), v.diff, v.total, v.past1, v.past2, v.maxu, ji == 0 ? "shared" : "copied into its twin" );
			ok = ok && v.ok();
		}
		const struct
		{
			const char * name;
			Red red;
			int job;
		} reds[3] = { { "stencil (ref 2: the strip after it finds no cell pixel)", kStencil, 0 },
			{ "blend (none over the sky)", kNoBlend, 0 }, { "field (volAirMed zeroed in the loose block)", kField, 0 } };
		for ( const auto & r : reds ) {
			std::vector<std::uint16_t> vk;
			const bool ran = runVk( r.job, r.red, vk );
			const Halves v = compareHalves( vk, refs[r.job] );
			const bool redFails = !ran || !v.ok();
			std::printf( "%s RED %s %s (%zu halves differ, %zu past 2 ulp, max %d; must FAIL)\n", gTag, redFails ? "FAIL" : v.word(),
				r.name, v.diff, v.past2, v.maxu );
			ok = ok && redFails;
		}
		const WwVkPresent::FxStat st = WwVkPresent::takeVolSkyStat();
		std::printf( "%s stat: %d runs on Vulkan, %d failed (%s), %d texture copies, drawing thread %.3f ms a run (max %.3f), "
			"%d pipelines built in %.1f ms\n", gTag, st.runs, st.fails, st.failWhy.c_str(), st.copies,
			st.runs ? st.cpuMs / st.runs : 0.0, st.maxMs, st.pipelines, st.buildMs );
		f.bindFramebuffer( fFb, 0 );
		WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &volKey ) );
		for ( char & k : twinKeys )
			WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &k ) );
		WwVkPresent::toneFrameDrop();
	} else {
		// ---------------------------------------------------------------- L6: the clouds' march + resolve
		static char keys[2][6], twins[2][8];
		bool started = false;
		for ( int a = 0; a < 2; a++ ) {
			const std::string jd = dir + ( a ? "/cloud_a" : "/cloud" );
			const char * var = a ? "cloud_a" : "cloud";
			std::vector<int> jv;
			if ( !readJob( jd + "/job.txt", jv, 5 ) )
				return fatal( std::string( "job.txt of " ) + var );
			const int hw = jv[0], hh = jv[1], mw = jv[2], mh = jv[3];
			const bool accum = jv[4] != 0;
			if ( !started ) {
				unsigned fc = 0, fds = 0;
				if ( !WwVkPresent::viewBegin( hw, hh, S, glProc, why ) )
					return fatal( "the view: " + why );
				if ( !WwVkPresent::toneFrame( hw, hh, S, fc, fds, why ) )
					return fatal( "toneFrame: " + why );
				started = true;
			}
			const GLuint pM = program( kTriVs, readFile( jd + "/march.frag.glsl" ), "march" );
			const GLuint pR = program( kTriVs, readFile( jd + "/resolve.frag.glsl" ), "resolve" );
			if ( !pM || !pR )
				return fatal( "GL programs" );
			for ( GLuint p : { pM, pR } ) {
				GLint nb = 0;
				f.getProgramiv( p, fBlocks, &nb );
				if ( nb != 0 )
					return fatal( "a cloud program with a uniform block" );
			}
			const UniMap s4 = readUni( jd + "/s4.uni" ), prev = readUni( jd + "/prev.uni" ), res = readUni( jd + "/resolve.uni" );
			std::vector<TexIn> tex = readTex( jd );
			const std::vector<std::string> names = accum
				? std::vector<std::string> { "cvMap", "cvShape", "cvDetail", "cvBlue", "cvSunView", "cvMoonView", "cvScene", "cvTrans" }
				: std::vector<std::string> { "cvMap", "cvShape", "cvDetail", "cvSunView", "cvMoonView", "cvScene", "cvTrans" };
			if ( tex.size() != names.size() )
				return fatal( "tex.txt" );
			for ( size_t k = 0; k < tex.size(); k++ ) {
				if ( tex[k].name != names[k] || tex[k].type != ( k == 1 || k == 2 ? 1 : 0 ) )
					return fatal( "tex.txt order at " + tex[k].name );
				if ( !upload( jd, tex[k], 0 ) )
					return fatal( "upload " + tex[k].name );
			}
			auto bindMarch = [&]( const UniMap & u ) {
				setUnis( pM, u );
				for ( size_t k = 0; k < tex.size(); k++ ) {
					bindUnit( int( k ), tex[k].target, tex[k].gl );
					u1i( pM, names[k].c_str(), int( k ) );
				}
				f.useProgram( pM );
			};
			// GL's targets (cloudvol.cpp's formats and sampling) and the shared ones (key: colour, key + 1: depth)
			const GLuint mC = tex2d( mw, mh, fRgba16f, GL_RGBA, GL_FLOAT, nullptr, true ), mD = tex2d( mw, mh, fRg32f, fRg, GL_FLOAT, nullptr, true );
			GLuint hC[2], hD[2];
			for ( int k = 0; k < 2; k++ ) {
				hC[k] = tex2d( hw, hh, fRgba16f, GL_RGBA, GL_FLOAT, nullptr, true );
				hD[k] = tex2d( hw, hh, fRg32f, fRg, GL_FLOAT, nullptr, true );
			}
			const std::uintptr_t kM = reinterpret_cast<std::uintptr_t>( &keys[a][0] ), kH0 = reinterpret_cast<std::uintptr_t>( &keys[a][2] ),
								 kH1 = reinterpret_cast<std::uintptr_t>( &keys[a][4] );
			GLuint sMC = 0, sMD = 0, sHC[2] = {}, sHD[2] = {};
			if ( !WwVkPresent::sharedTarget( kM, 0, mw, mh, 1, fRgba16f, sMC, why ) || !WwVkPresent::sharedTarget( kM + 1, 0, mw, mh, 1, fRg32f, sMD, why )
				|| !WwVkPresent::sharedTarget( kH0, 0, hw, hh, 1, fRgba16f, sHC[0], why )
				|| !WwVkPresent::sharedTarget( kH0 + 1, 0, hw, hh, 1, fRg32f, sHD[0], why )
				|| !WwVkPresent::sharedTarget( kH1, 0, hw, hh, 1, fRgba16f, sHC[1], why )
				|| !WwVkPresent::sharedTarget( kH1 + 1, 0, hw, hh, 1, fRg32f, sHD[1], why ) )
				return fatal( "the shared march / history targets: " + why );
			for ( GLuint t : { sMC, sMD, sHC[0], sHD[0], sHC[1], sHD[1] } ) {
				glBindTexture( GL_TEXTURE_2D, t );
				texParams( GL_TEXTURE_2D, true, false, false );
			}
			const GLuint fbM = fbo( GL_TEXTURE_2D, mC, mD, 0 ), fbH0 = fbo( GL_TEXTURE_2D, hC[0], hD[0], 0 ), fbH1 = fbo( GL_TEXTURE_2D, hC[1], hD[1], 0 );
			const GLuint fbSM = fbo( GL_TEXTURE_2D, sMC, sMD, 0 ), fbSH0 = fbo( GL_TEXTURE_2D, sHC[0], sHD[0], 0 );
			if ( !fbM || !fbH0 || !fbH1 || !fbSM || !fbSH0 )
				return fatal( "GL cloud targets" );
			auto draw = [&]( GLuint fb, int w, int h, GLuint prog ) {
				f.bindFramebuffer( fFb, fb );
				glViewport( 0, 0, w, h );
				drawState0();
				f.useProgram( prog );
				glDrawArrays( GL_TRIANGLES, 0, 3 );
			};
			auto clear2 = [&]( GLuint fb ) {  // a value no pass writes: what Vulkan leaves unwritten reads as a difference
				f.bindFramebuffer( fFb, fb );
				drawState0();
				glClearColor( 7.0f, -7.0f, 7.0f, -7.0f );
				glClear( GL_COLOR_BUFFER_BIT );
			};
			const int rU[4] = { 10, 11, 12, 13 };
			auto bindResolve = [&]() {
				setUnis( pR, res );
				bindUnit( rU[0], GL_TEXTURE_2D, mC );
				bindUnit( rU[1], GL_TEXTURE_2D, mD );
				bindUnit( rU[2], GL_TEXTURE_2D, hC[1] );
				bindUnit( rU[3], GL_TEXTURE_2D, hD[1] );
				u1i( pR, "cvMarch", rU[0] );
				u1i( pR, "cvMarchDepth", rU[1] );
				u1i( pR, "cvPrev", rU[2] );
				u1i( pR, "cvPrevDepth", rU[3] );
				f.useProgram( pR );
			};
			// GL: the last frame's full march as the history, this frame's stride-4 march, the resolve
			bindMarch( prev );
			draw( fbH1, hw, hh, pM );
			bindMarch( s4 );
			draw( fbM, mw, mh, pM );
			bindResolve();
			draw( fbH0, hw, hh, pR );
			f.bindFramebuffer( fFb, 0 );
			const std::vector<float> gMC = readRgba16f( mC, mw, mh ), gMD = readRg32f( mD, mw, mh );
			const std::vector<float> gHC = readRgba16f( hC[0], hw, hh ), gHD = readRg32f( hD[0], hw, hh );
			size_t cov = 0;
			for ( size_t i = 3; i < gMC.size(); i += 4 )
				cov += gMC[i] < 0.999f;
			std::printf( "%s %s: history %dx%d, march %dx%d; INFO the march's cloud texels (T < 0.999) %.1f %%\n", gTag, var, hw, hh, mw, mh,
				100.0 * double( cov ) / double( gMC.size() / 4 ) );
			// the history Vulkan's resolve reads: GL's, copied into the shared hist[1]
			f.copyImageSubData( hC[1], GL_TEXTURE_2D, 0, 0, 0, 0, sHC[1], GL_TEXTURE_2D, 0, 0, 0, 0, hw, hh, 1 );
			f.copyImageSubData( hD[1], GL_TEXTURE_2D, 0, 0, 0, 0, sHD[1], GL_TEXTURE_2D, 0, 0, 0, 0, hw, hh, 1 );
			if ( glGetError() != GL_NO_ERROR )
				return fatal( "GL's cloud reference" );
			// the Vulkan run as cloudvol.cpp's: the march's slots and both blocks from the programs GL is about to draw with
			auto runVk = [&]( int red, bool marchFromGl ) -> bool {
				clear2( fbSH0 );
				if ( marchFromGl ) {
					f.copyImageSubData( mC, GL_TEXTURE_2D, 0, 0, 0, 0, sMC, GL_TEXTURE_2D, 0, 0, 0, 0, mw, mh, 1 );
					f.copyImageSubData( mD, GL_TEXTURE_2D, 0, 0, 0, 0, sMD, GL_TEXTURE_2D, 0, 0, 0, 0, mw, mh, 1 );
				} else {
					clear2( fbSM );
				}
				f.bindFramebuffer( fFb, 0 );
				WwVkPresent::CloudRun t;
				t.accum = accum;
				t.mw = mw;
				t.mh = mh;
				t.hw = hw;
				t.hh = hh;
				t.cur = 0;
				t.marchKey = kM;
				t.histKey[0] = kH0;
				t.histKey[1] = kH1;
				bindMarch( s4 );
				for ( size_t k = 0; k < names.size(); k++ ) {
					if ( !WwVkPresent::fxTexOf( &ga, pM, names[k].c_str(), k == 1 || k == 2 ? 1 : 0, t.tex[k], why ) ) {
						std::printf( "%s %s: fxTexOf refused: %s\n", gTag, var, why.c_str() );
						return false;
					}
					t.tex[k].twin = reinterpret_cast<std::uintptr_t>( &twins[a][k] );
					t.tex[k].still = k == 1 || k == 2 || ( accum && k == 3 );
				}
				if ( accum )
					WwVkPresent::fxPack( &ga, pM, WwVkPresent::kT_march_a, int( std::size( WwVkPresent::kT_march_a ) ), t.march );
				else
					WwVkPresent::fxPack( &ga, pM, WwVkPresent::kT_march, int( std::size( WwVkPresent::kT_march ) ), t.march );
				bindResolve();
				if ( accum )
					WwVkPresent::fxPack( &ga, pR, WwVkPresent::kT_resolve_a, int( std::size( WwVkPresent::kT_resolve_a ) ), t.resolve );
				else
					WwVkPresent::fxPack( &ga, pR, WwVkPresent::kT_resolve, int( std::size( WwVkPresent::kT_resolve ) ), t.resolve );
				t.red = red | ( marchFromGl ? 4 : 0 );
				if ( !WwVkPresent::cloudRun( t, why ) ) {
					std::printf( "%s %s: cloudRun refused: %s\n", gTag, var, why.c_str() );
					return false;
				}
				return glGetError() == GL_NO_ERROR;
			};
			auto rel = [&]( const char * what, const std::vector<float> & gl, const std::vector<float> & vk, bool exact ) {
				const Rel r = compareRel( gl, vk );
				const bool pass = exact ? ( r.n > 0 && r.differ == 0 ) : r.ok();
				std::printf( "%s %s %s %s (%zu of %zu values differ, %zu past rel 1e-3, max rel %.3g; bar %s)\n", gTag, var, what,
					exact ? ( pass ? "EXACT" : "FAIL" ) : r.word(), r.differ, r.n, r.past, r.maxRel, exact ? "EXACT" : "MARCH_RULE" );
				return pass;
			};
			// the full job: Vulkan's march and resolve
			if ( !runVk( 0, false ) ) {
				ok = false;
			} else {
				ok = rel( "march colour", gMC, readRgba16f( sMC, mw, mh ), false ) && ok;
				ok = rel( "march depth", gMD, readRg32f( sMD, mw, mh ), false ) && ok;
				ok = rel( "hist[0] colour", gHC, readRgba16f( sHC[0], hw, hh ), false ) && ok;
				ok = rel( "hist[0] depth", gHD, readRg32f( sHD[0], hw, hh ), false ) && ok;
			}
			// the resolve alone on GL's own march
			if ( !runVk( 0, true ) ) {
				ok = false;
			} else {
				ok = rel( "resolve colour (GL's march)", gHC, readRgba16f( sHC[0], hw, hh ), true ) && ok;
				ok = rel( "resolve depth (GL's march)", gHD, readRg32f( sHD[0], hw, hh ), true ) && ok;
			}
			// the reds: each must FAIL
			{
				const bool ran = runVk( 2, false );
				const Rel c = compareRel( gMC, ran ? readRgba16f( sMC, mw, mh ) : std::vector<float>() );
				const Rel h = compareRel( gHC, ran ? readRgba16f( sHC[0], hw, hh ) : std::vector<float>() );
				const bool redFails = !ran || !c.ok() || !h.ok();
				std::printf( "%s RED %s %s shape / detail swapped (march %zu past rel 1e-3, hist %zu; must FAIL)\n", gTag,
					redFails ? "FAIL" : "BOUND", var, c.past, h.past );
				ok = ok && redFails;
			}
			{
				const bool ran = runVk( 1, true );
				const Rel c = compareRel( gHC, ran ? readRgba16f( sHC[0], hw, hh ) : std::vector<float>() );
				const bool redFails = !ran || c.n == 0 || c.differ > 0;
				std::printf( "%s RED %s %s resolve's march / history swapped (%zu of %zu values differ; must FAIL)\n", gTag,
					redFails ? "FAIL" : "EXACT", var, c.differ, c.n );
				ok = ok && redFails;
			}
			for ( std::uintptr_t k : { kM, kM + 1, kH0, kH0 + 1, kH1, kH1 + 1 } )
				WwVkPresent::sharedTargetDrop( k );
			for ( char & k : twins[a] )
				WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &k ) );
		}
		const WwVkPresent::FxStat st = WwVkPresent::takeCloudStat();
		std::printf( "%s stat: %d runs on Vulkan, %d failed (%s), %d texture copies, drawing thread %.3f ms a run (max %.3f), "
			"%d pipelines built in %.1f ms\n", gTag, st.runs, st.fails, st.failWhy.c_str(), st.copies,
			st.runs ? st.cpuMs / st.runs : 0.0, st.maxMs, st.pipelines, st.buildMs );
		f.bindFramebuffer( fFb, 0 );
		WwVkPresent::toneFrameDrop();
	}
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	DestroyWindow( wnd );
	std::printf( "%s GATE %s\n", gTag, ok ? "OK" : "FAILED" );
	return ok ? 0 : 1;
}

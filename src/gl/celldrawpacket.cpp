#include "celldrawpacket.h"

#include "wwswitch.h"
#include "gl/glcontext.hpp"
#include "gl/gltex.h"

#include <QString>

#include <cstdio>
#include <cstring>
#include <unordered_map>

bool wwFrameLog();	// gl/cellcull.cpp

namespace WwDp
{
Packet * rec = nullptr;

namespace
{
struct State
{
	std::uint32_t epoch = 1;
	bool aborted = false;
	std::unordered_map<const Shape *, Packet> packets;
	long long replays = 0, records = 0, aborts = 0, checked = 0, bad = 0, badAll = 0;
	QString line;
};

State & st()
{
	static State s;
	return s;
}
}

int mode()
{
	WW_SWITCH_STATIC( int, m, wwEnvInt( "WW_DRAW_PACKET_RED" ) != 0 ? 0 : ( wwEnvInt( "WW_DRAW_PACKET_CHECK" ) != 0 ? 2 : 1 ) );
	return m;
}

void frame()
{
	State & s = st();
	if ( ++s.epoch == 0 )
		s.epoch = 1;
	if ( s.packets.size() > 65536 )	// shapes of closed documents
		s.packets.clear();
	if ( s.replays || s.records ) {
		const QString line = QStringLiteral( "draw packets: %1 replays=%2 records=%3 aborts=%4 checked=%5 mismatches=%6 (all frames %7)" )
			.arg( mode() == 2 ? QStringLiteral( "check" ) : QStringLiteral( "on" ) ).arg( s.replays ).arg( s.records )
			.arg( s.aborts ).arg( s.checked ).arg( s.bad ).arg( s.badAll );
		if ( line != s.line && wwFrameLog() ) {
			s.line = line;
			std::fprintf( stderr, "%s\n", line.toLocal8Bit().constData() );
		}
	}
	s.replays = s.records = s.aborts = s.checked = s.bad = 0;
}

void invalidate()
{
	State & s = st();
	if ( ++s.epoch == 0 )
		s.epoch = 1;
	if ( rec )
		s.aborted = true;
}

void abortRecording()
{
	if ( rec )
		st().aborted = true;
}

Packet * slot( const Shape * shape, const void * prog )
{
	if ( !mode() || !shape || !prog )
		return nullptr;
	return &st().packets[shape];
}

bool valid( const Packet & p, const void * prog )
{
	return p.ready && p.prog == prog && p.epoch == st().epoch;
}

void begin( Packet & p, const void * prog )
{
	p.ready = false;
	p.prog = prog;
	p.epoch = st().epoch;
	p.ops.clear();
	p.data.clear();
	st().aborted = false;
	rec = &p;
}

void end( bool ok, int texunit )
{
	State & s = st();
	if ( !rec )
		return;
	Packet & p = *rec;
	rec = nullptr;
	if ( ok && !s.aborted && p.epoch == s.epoch ) {
		p.texunit = texunit;
		p.ready = true;
		s.records++;
	} else {
		p.ready = false;
		s.aborts += s.aborted ? 1 : 0;
	}
	s.aborted = false;
}

void counted( bool replayed, int mismatches )
{
	State & s = st();
	if ( replayed )
		s.replays++;
	if ( mismatches >= 0 ) {
		s.checked++;
		s.bad += mismatches;
		s.badAll += mismatches;
	}
}

std::int32_t lastBound()
{
	if ( rec )
		for ( auto it = rec->ops.rbegin(); it != rec->ops.rend(); ++it )
			if ( it->kind == BindTex )
				return it->b;
	return 0;
}

void put( Kind k, std::int32_t a, std::int32_t b, const void * v, int words )
{
	if ( !rec )
		return;
	Op o;
	o.kind = k;
	o.a = a;
	o.b = b;
	o.off = std::uint32_t( rec->data.size() );
	if ( words > 0 ) {
		rec->data.resize( rec->data.size() + size_t( words ) );
		std::memcpy( rec->data.data() + o.off, v, size_t( words ) * 4 );
	}
	rec->ops.push_back( o );
}

void replay( const Packet & p, NifSkopeOpenGLContext::GLFunctions * f, TexCache * tc, void ( *live )( void * ), void * ctx )
{
	const std::uint32_t * d = p.data.data();
	for ( const Op & o : p.ops ) {
		const std::uint32_t * w = d + o.off;
		const GLfloat * x = reinterpret_cast<const GLfloat *>( w );
		const GLint * xi = reinterpret_cast<const GLint *>( w );
		switch ( o.kind ) {
		case U1i: f->glUniform1i( o.a, xi[0] ); break;
		case U1f: f->glUniform1f( o.a, x[0] ); break;
		case U2f: f->glUniform2f( o.a, x[0], x[1] ); break;
		case U3f: f->glUniform3f( o.a, x[0], x[1], x[2] ); break;
		case U4f: f->glUniform4f( o.a, x[0], x[1], x[2], x[3] ); break;
		case U1iv: f->glUniform1iv( o.a, o.b, xi ); break;
		case U1fv: f->glUniform1fv( o.a, o.b, x ); break;
		case U4fv: f->glUniform4fv( o.a, o.b, x ); break;
		case UM3: f->glUniformMatrix3fv( o.a, 1, 0, x ); break;
		case UM4: f->glUniformMatrix4fv( o.a, 1, 0, x ); break;
		case ActiveTex: f->glActiveTexture( GLenum( o.a ) ); break;
		case BindTex: f->glBindTexture( GLenum( o.a ), GLuint( o.b ) ); break;
		case TexParamNeeded:
			// the PERF5 shadow decides again, as the setup would have at this point
			tc->wwParamArm( GLuint( o.b ) );
			if ( tc->wwParamsNeeded( xi[0], xi[1], xi[2], x[3] ) ) {
				f->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, xi[0] );
				f->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, xi[1] );
				f->glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, x[3] );
				f->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
				f->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, xi[2] );
			}
			break;
		case TexParamI: f->glTexParameteri( GLenum( o.a ), GLenum( o.b ), xi[0] ); break;
		case Enable: f->glEnable( GLenum( o.a ) ); break;
		case Disable: f->glDisable( GLenum( o.a ) ); break;
		case CullFace: f->glCullFace( GLenum( o.a ) ); break;
		case BlendSep: f->glBlendFuncSeparate( GLenum( xi[0] ), GLenum( xi[1] ), GLenum( xi[2] ), GLenum( xi[3] ) ); break;
		case PolyOffset: f->glPolygonOffset( x[0], x[1] ); break;
		case DepthFunc: f->glDepthFunc( GLenum( o.a ) ); break;
		case DepthMask: f->glDepthMask( GLboolean( o.a ) ); break;
		case PolyMode: f->glPolygonMode( GLenum( o.a ), GLenum( o.b ) ); break;
		case Live: live( ctx ); break;
		}
	}
}

bool sameOps( const Packet & a, const Packet & b )
{
	if ( a.ops.size() != b.ops.size() || a.data != b.data || a.texunit != b.texunit )
		return false;
	for ( size_t i = 0; i < a.ops.size(); i++ ) {
		const Op & x = a.ops[i];
		const Op & y = b.ops[i];
		if ( x.kind != y.kind || x.a != y.a || x.b != y.b || x.off != y.off )
			return false;
	}
	return true;
}

void snapFixed( NifSkopeOpenGLContext::GLFunctions * f, FixedSnap & o )
{
	o.v.clear();
	static const GLenum caps[] = { GL_BLEND, GL_CULL_FACE, GL_DEPTH_TEST, GL_POLYGON_OFFSET_FILL, GL_TEXTURE_CUBE_MAP_SEAMLESS,
		GL_STENCIL_TEST, GL_SCISSOR_TEST, GL_DEPTH_CLAMP };
	for ( GLenum c : caps )
		o.v.push_back( f->glIsEnabled( c ) ? 1 : 0 );
	static const GLenum ints[] = { GL_CULL_FACE_MODE, GL_FRONT_FACE, GL_BLEND_SRC_RGB, GL_BLEND_DST_RGB, GL_BLEND_SRC_ALPHA,
		GL_BLEND_DST_ALPHA, GL_DEPTH_FUNC, GL_ACTIVE_TEXTURE, GL_CURRENT_PROGRAM };
	for ( GLenum e : ints ) {
		GLint v = 0;
		f->glGetIntegerv( e, &v );
		o.v.push_back( v );
	}
	GLboolean dm = GL_FALSE;
	f->glGetBooleanv( GL_DEPTH_WRITEMASK, &dm );
	o.v.push_back( dm ? 1 : 0 );
	GLfloat po[2] = {};
	f->glGetFloatv( GL_POLYGON_OFFSET_FACTOR, &po[0] );
	f->glGetFloatv( GL_POLYGON_OFFSET_UNITS, &po[1] );
	for ( GLfloat v : po ) {
		std::int32_t i;
		std::memcpy( &i, &v, 4 );
		o.v.push_back( i );
	}
	GLint pm[2] = {};
	f->glGetIntegerv( GL_POLYGON_MODE, pm );
	o.v.push_back( pm[0] );
	// the sampler state of the material units' textures (the shadow skips these sets)
	GLint active = 0;
	f->glGetIntegerv( GL_ACTIVE_TEXTURE, &active );
	for ( int u = 0; u < 12; u++ ) {
		f->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		for ( GLenum t : { GLenum( GL_TEXTURE_2D ), GLenum( GL_TEXTURE_CUBE_MAP ) } ) {
			GLint id = 0;
			f->glGetIntegerv( t == GL_TEXTURE_2D ? GL_TEXTURE_BINDING_2D : GL_TEXTURE_BINDING_CUBE_MAP, &id );
			if ( !id )
				continue;
			static const GLenum ps[] = { GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T, GL_TEXTURE_WRAP_R, GL_TEXTURE_MIN_FILTER,
				GL_TEXTURE_MAG_FILTER };
			for ( GLenum p : ps ) {
				GLint v = 0;
				f->glGetTexParameteriv( t, p, &v );
				o.v.push_back( v );
			}
			GLfloat a = 0.0f;
			f->glGetTexParameterfv( t, GL_TEXTURE_MAX_ANISOTROPY_EXT, &a );
			std::int32_t ai;
			std::memcpy( &ai, &a, 4 );
			o.v.push_back( ai );
		}
	}
	f->glActiveTexture( GLenum( active ) );
}
}

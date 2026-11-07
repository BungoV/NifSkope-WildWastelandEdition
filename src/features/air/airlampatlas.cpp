#include "features/air/airlampatlas.h"

/* lane MODULES1 route (a): the air's lamp atlas (airlampatlas.h). RESEARCH_AIR.md section 5:
 *  - one GL_DEPTH_COMPONENT16 atlas, kAtlas x kAtlas, depth = distance / radius drawn by cell_shadowdepth.prog (the light
 *    cubes' program and caster rules), six s x s tiles a lamp, s = nextPow2(0.5 x screen px) on 32 .. 256;
 *  - tiles from a buddy allocator over 32 px cells; over budget the smallest lamps step down first;
 *  - the static world drawn once a cell, kFacesPerFrame faces a frame, nearest lamps first; a resize draws the new tiles
 *    before the old are freed (hysteresis 0.35 down);
 *  - AUDIT 11 movers: a caster whose world transform or two vertices changed since the last frame (LAMPSHADOW1's
 *    lsSigOf rule) redraws every lamp whose sphere holds its old or new bound; the held tiles stay until the new are
 *    whole (or, with no room, the lamp takes the walk for the frames of its redraw). Red WW_CELL_AIR_LAMPATLAS_RED=frozen:
 *    movers frozen at the draw, as before;
 *  - the table (RGBA32F buffer, 4 texels a light): faces 0-1, 2-3, 4-5 tile origins in px, then (s, ready, 0, 0).
 * The shader: cell_volair.frag airLeakLightK (face by the major axis, 2x2 compare one texel inside the tile). */

#include "wwswitch.h"
#include "gl/cellunits.h"
#include "gl/celllights.h"
#include "features/lights/cubefacecull.h"
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "gl/renderer.h"

#include <QElapsedTimer>
#include <QOpenGLContext>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#include <unordered_map>

namespace {
int g_lampVisRow = -1;	// the menu row's mode (QSettings WW/AirLampVis), read once
}

bool wwAirLampVisPinned()
{
	return !wwEnv( "WW_CELL_AIR_LAMPVIS" ).trimmed().isEmpty();
}

int wwAirLampVisMode()
{
	const QByteArray v = wwEnv( "WW_CELL_AIR_LAMPVIS" ).trimmed();	// the pin wins, read live
	if ( v == "atlas" )
		return 1;
	if ( v == "rays" )
		return 2;
	if ( !v.isEmpty() )
		return 0;
	if ( g_lampVisRow < 0 )
		g_lampVisRow = std::clamp( QSettings().value( QStringLiteral( "WW/AirLampVis" ), 1 ).toInt(), 0, 2 );	// default Atlas (the Museum winner, 11-05)
	return g_lampVisRow;
}

int wwAirLampVisChoice()
{
	static const int item[3] = { 2, 0, 1 };	// walk, atlas, rays -> the menu's Atlas | Ray traced | Probe walk
	return item[wwAirLampVisMode()];
}

void wwAirLampVisSetChoice( int item )
{
	if ( wwAirLampVisPinned() )
		return;
	static const int mode[3] = { 1, 2, 0 };
	g_lampVisRow = mode[std::clamp( item, 0, 2 )];
	QSettings().setValue( QStringLiteral( "WW/AirLampVis" ), g_lampVisRow );
}

int wwAirLampAtlasRed()
{
	const QByteArray v = wwEnv( "WW_CELL_AIR_LAMPATLAS_RED" ).trimmed();	// read live; "twice,faces,perface"
	const bool perface = v.contains( "perface" );
	const bool faces = QByteArray( v ).replace( "perface", "" ).contains( "faces" );
	return ( v.contains( "twice" ) ? 1 : 0 ) | ( faces ? 2 : 0 ) | ( perface ? 4 : 0 ) | ( v.contains( "frozen" ) ? 8 : 0 );
}

bool wwAirAtlasAllRed()
{
	return wwEnv( "WW_AIR_ATLAS_ALL_RED" ).trimmed() == "1";	// read live
}

namespace {

constexpr int kAtlas = 8192;
constexpr int kLevels = 9;	// 8192 .. 32
constexpr int kMinS = 32, kMaxS = 256;
constexpr int kFacesPerFrame = 192;	// red "faces": the first fill's cap
constexpr qint64 kFillNs = 4000000;	// the fill's CPU budget a frame (AUDIT row 13); one face at least
constexpr int kTexelsPerLamp = 4;

int levelOf( int s )
{
	int l = 0;
	for ( int z = kAtlas; z > s; z >>= 1 )
		l++;
	return l;
}

struct Buddy
{
	std::set<std::uint32_t> freeAt[kLevels];

	static std::uint32_t key( int x, int y ) { return std::uint32_t( x ) << 16 | std::uint32_t( y ); }

	void reset()
	{
		for ( auto & f : freeAt )
			f.clear();
		freeAt[0].insert( key( 0, 0 ) );
	}
	//! a block of level l (side kAtlas >> l), or -1
	std::int64_t take( int l )
	{
		if ( !freeAt[l].empty() ) {
			const std::uint32_t k = *freeAt[l].begin();
			freeAt[l].erase( freeAt[l].begin() );
			return k;
		}
		if ( l == 0 )
			return -1;
		const std::int64_t p = take( l - 1 );
		if ( p < 0 )
			return -1;
		const int x = int( p >> 16 ), y = int( p & 0xffff ), h = kAtlas >> l;
		freeAt[l].insert( key( x + h, y ) );
		freeAt[l].insert( key( x, y + h ) );
		freeAt[l].insert( key( x + h, y + h ) );
		return key( x, y );
	}
	void give( int l, std::uint32_t k )
	{
		if ( l > 0 ) {
			const int h = kAtlas >> l, x = int( k >> 16 ), y = int( k & 0xffff );
			const int px = x & ~( 2 * h - 1 ), py = y & ~( 2 * h - 1 );
			const std::uint32_t sib[4] = { key( px, py ), key( px + h, py ), key( px, py + h ), key( px + h, py + h ) };
			int freeSibs = 0;
			for ( std::uint32_t s : sib )
				freeSibs += ( s != k && freeAt[l].count( s ) ) ? 1 : 0;
			if ( freeSibs == 3 ) {
				for ( std::uint32_t s : sib )
					freeAt[l].erase( s );
				give( l - 1, key( px, py ) );
				return;
			}
		}
		freeAt[l].insert( k );
	}
};

struct Tiles
{
	int s = 0;
	std::uint32_t at[6] = {};
	int drawn = 0;	// faces drawn (6 = ready)
};

struct Lamp
{
	Tiles cur, next;
	bool redraw = false;	// AUDIT 11: a caster in its sphere moved; next is drawn at cur's size
};

//! AUDIT 11: a caster's world rotation, translation, scale and two vertices (a CPU-skinned shape moves its vertices)
struct Sig
{
	float v[19];
	float c[3];
	float rad = 0.0f;
};

struct State
{
	GLuint tex = 0, fbo = 0, buf = 0, tbo = 0;
	bool fboOk = false;
	Buddy buddy;
	std::vector<Lamp> lamps;
	std::uint64_t key = 0;
	std::vector<float> table;
	bool tableDirty = true;
	qint64 frames = 0, facesTotal = 0;
	int lastReady = -1, lastPending = -1;
	double drawMs = 0.0, maxMs = 0.0;	// since the cell's key last changed
	std::unordered_map<const Shape *, Sig> hist;	// AUDIT 11: last frame's signature a caster
	qint64 moves = 0, redraws = 0;	// AUDIT 11: since the cell's key last changed
};

State & st()
{
	static State s;
	return s;
}

void freeTiles( State & s, Tiles & t )
{
	if ( t.s > 0 ) {
		const int l = levelOf( t.s );
		for ( std::uint32_t k : t.at )
			s.buddy.give( l, k );
	}
	t = Tiles();
}

bool takeTiles( State & s, Tiles & t, int size )
{
	const int l = levelOf( size );
	std::int64_t got[6];
	for ( int f = 0; f < 6; f++ ) {
		got[f] = s.buddy.take( l );
		if ( got[f] < 0 ) {
			for ( int g = 0; g < f; g++ )
				s.buddy.give( l, std::uint32_t( got[g] ) );
			return false;
		}
	}
	t = Tiles();
	t.s = size;
	for ( int f = 0; f < 6; f++ )
		t.at[f] = std::uint32_t( got[f] );
	return true;
}

void release( Renderer * r )
{
	State & s = st();
	auto fn = r->fn;
	if ( s.tex )
		fn->glDeleteTextures( 1, &s.tex );
	if ( s.fbo )
		fn->glDeleteFramebuffers( 1, &s.fbo );
	if ( s.tbo )
		fn->glDeleteTextures( 1, &s.tbo );
	if ( s.buf )
		fn->glDeleteBuffers( 1, &s.buf );
	s = State();
}

int nextPow2( float v )
{
	int s = kMinS;
	while ( s < kMaxS && float( s ) < v )
		s <<= 1;
	return s;
}

bool layeredSupported()
{
	static int ok = -1;
	if ( ok < 0 ) {
		const QOpenGLContext * c = QOpenGLContext::currentContext();
		ok = ( c && c->hasExtension( QByteArrayLiteral( "GL_ARB_shader_viewport_layer_array" ) ) ) ? 1 : 0;
		std::fprintf( stderr, "air lamp atlas: layered face draw %s\n", ok ? "on (GL_ARB_shader_viewport_layer_array)" : "off (no GL_ARB_shader_viewport_layer_array)" );
	}
	return ok != 0;
}

std::uint64_t cellKey( const WwCellLighting * L )
{
	std::uint64_t h = 1469598103934665603ULL ^ std::uint64_t( reinterpret_cast<std::uintptr_t>( L ) );
	for ( const WwCellLight & l : L->lights ) {
		const float v[5] = { l.pos[0], l.pos[1], l.pos[2], l.radius, l.nearClip };
		for ( float f : v ) {
			std::uint32_t b;
			std::memcpy( &b, &f, 4 );
			h = ( h ^ b ) * 1099511628211ULL;
		}
	}
	return h;
}

Sig sigOf( const WwCubeCaster & c )
{
	Sig g;
	const Transform & w = c.sh->worldTrans();
	int n = 0;
	for ( int i = 0; i < 3; i++ )
		for ( int j = 0; j < 3; j++ )
			g.v[n++] = w.rotation( i, j );
	for ( int i = 0; i < 3; i++ )
		g.v[n++] = w.translation[i];
	g.v[n++] = w.scale;
	const bool any = !c.sh->verts.isEmpty();
	for ( int i = 0; i < 3; i++ ) {
		g.v[n + i] = any ? c.sh->verts.constFirst()[i] : 0.0f;
		g.v[n + 3 + i] = any ? c.sh->verts.at( c.sh->verts.size() / 2 )[i] : 0.0f;
		g.c[i] = c.c[i];
	}
	g.rad = c.rad;
	return g;
}

bool touches( const WwCellLight & l, const float c[3], float rad )
{
	const float dx = c[0] - l.pos[0], dy = c[1] - l.pos[1], dz = c[2] - l.pos[2], reach = rad + l.radius;
	return dx * dx + dy * dy + dz * dz < reach * reach;
}

}	// namespace

int wwAirLampAtlasFrame( Scene * scene, const WwCellLighting * L, const float cam[3], float p11, int viewH, float farD )
{
	Renderer * r = scene ? scene->renderer : nullptr;
	if ( !r )
		return 0;
	State & s = st();
	if ( wwAirLampVisMode() != 1 || !L || L->lights.isEmpty() ) {
		if ( s.tex )
			release( r );
		return 0;
	}
	auto fn = r->fn;
	QElapsedTimer clock;
	clock.start();

	if ( !s.tex ) {
		GLint maxTex = 0;
		fn->glGetIntegerv( GL_MAX_TEXTURE_SIZE, &maxTex );
		if ( maxTex < kAtlas )
			return 0;
		fn->glGenTextures( 1, &s.tex );
		fn->glBindTexture( GL_TEXTURE_2D, s.tex );
		fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, kAtlas, kAtlas, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, nullptr );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
		GLint prevDraw = 0;
		fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
		fn->glGenFramebuffers( 1, &s.fbo );
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, s.fbo );
		fn->glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, s.tex, 0 );
		fn->glDrawBuffer( GL_NONE );
		s.fboOk = fn->glCheckFramebufferStatus( GL_DRAW_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE;
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
		fn->glGenBuffers( 1, &s.buf );
		fn->glGenTextures( 1, &s.tbo );
		s.buddy.reset();
		s.key = 0;
		std::fprintf( stderr, "air lamp atlas: made %d px depth16 (%d MB) fbo %s\n", kAtlas, kAtlas / 1024 * kAtlas / 1024 * 2,
			s.fboOk ? "ok" : "INCOMPLETE" );
	}
	if ( !s.fboOk )
		return 0;

	// a new cell (or its lights moved): every tile is drawn again
	const std::uint64_t key = cellKey( L );
	const int n = int( L->lights.size() );
	if ( key != s.key || int( s.lamps.size() ) != n ) {
		s.key = key;
		s.buddy.reset();
		s.lamps.assign( size_t( n ), Lamp() );
		s.tableDirty = true;
		s.lastReady = s.lastPending = -1;
		s.frames = s.facesTotal = 0;
		s.drawMs = s.maxMs = 0.0;
		s.hist.clear();
		s.moves = s.redraws = 0;
	}

	// AUDIT 11: what moved since the last frame redraws the lamps it touches (red "frozen": movers frozen at the draw)
	std::vector<WwCubeCaster> casters;
	bool haveCasters = false;
	if ( !( wwAirLampAtlasRed() & 8 ) ) {
		wwCellCubeCasters( scene, casters );
		haveCasters = true;
		for ( const WwCubeCaster & c : casters ) {
			const Sig g = sigOf( c );
			auto it = s.hist.find( c.sh );
			if ( it != s.hist.end() && std::memcmp( it->second.v, g.v, sizeof g.v ) != 0 ) {
				s.moves++;
				for ( int i = 0; i < n; i++ ) {
					Lamp & m = s.lamps[i];
					if ( !m.cur.s && !m.next.s )
						continue;
					const WwCellLight & l = L->lights.at( i );
					if ( touches( l, it->second.c, it->second.rad ) || touches( l, g.c, g.rad ) ) {
						s.redraws += m.redraw ? 0 : 1;
						m.redraw = true;
						m.next.drawn = 0;
					}
				}
			}
			s.hist[c.sh] = g;
		}
	}

	// the sizes: half the lamp's screen extent, on the ladder; away from the air the tiles go back to the pool
	std::vector<int> want( size_t( n ), 0 );
	std::vector<float> px( size_t( n ), 0.0f ), dist( size_t( n ), 0.0f );
	const bool allRed = wwAirAtlasAllRed();	// part 22: only shadow-flagged lamps take a tile
	for ( int i = 0; i < n; i++ ) {
		const WwCellLight & l = L->lights.at( i );
		const float dx = l.pos[0] - cam[0], dy = l.pos[1] - cam[1], dz = l.pos[2] - cam[2];
		dist[i] = std::sqrt( dx * dx + dy * dy + dz * dz );
		Lamp & m = s.lamps[i];
		const int have = m.cur.s;
		if ( l.shadow == 0 && !allRed )
			continue;
		if ( l.radius <= 0.0f || dist[i] - l.radius >= farD * ( have ? 1.25f : 1.0f ) )
			continue;
		px[i] = 0.5f * float( viewH ) * p11 * l.radius / std::max( dist[i], l.radius );
		int w = nextPow2( 0.5f * px[i] );
		if ( have && w < have && 0.5f * px[i] >= 0.35f * float( have ) )
			w = have;	// hysteresis: shrink only well under the size held
		want[i] = w;
	}
	// over budget: the smallest lamps step down first (85 % of the atlas, the rest for the resizes in flight)
	{
		const double cap = 0.85 * double( kAtlas ) * double( kAtlas );
		double area = 0.0;
		for ( int i = 0; i < n; i++ )
			area += 6.0 * double( want[i] ) * double( want[i] );
		std::vector<int> order;
		for ( int i = 0; i < n; i++ )
			if ( want[i] )
				order.push_back( i );
		std::sort( order.begin(), order.end(), [&]( int a, int b ) { return px[a] < px[b]; } );
		while ( area > cap ) {
			bool stepped = false;
			for ( int i : order ) {
				if ( area <= cap )
					break;
				if ( want[i] > kMinS ) {
					area -= 6.0 * 0.75 * double( want[i] ) * double( want[i] );
					want[i] >>= 1;
					stepped = true;
				}
			}
			if ( !stepped ) {
				for ( auto it = order.rbegin(); it != order.rend() && area > cap; ++it ) {	// the farthest go to the walk
					if ( want[*it] ) {
						area -= 6.0 * double( want[*it] ) * double( want[*it] );
						want[*it] = 0;
					}
				}
				break;
			}
		}
	}
	std::vector<int> pending;
	for ( int i = 0; i < n; i++ ) {
		Lamp & m = s.lamps[i];
		if ( !want[i] ) {
			if ( m.cur.s || m.next.s )
				s.tableDirty = true;
			freeTiles( s, m.next );
			freeTiles( s, m.cur );
			m.redraw = false;
			continue;
		}
		if ( m.cur.s == want[i] && !m.redraw ) {
			freeTiles( s, m.next );
			continue;
		}
		if ( m.next.s != want[i] ) {
			freeTiles( s, m.next );
			if ( !takeTiles( s, m.next, want[i] ) ) {
				if ( !( m.redraw && m.cur.s == want[i] ) )
					continue;	// no room this frame: the held tiles (or the walk) stay
				m.next = m.cur;	// AUDIT 11, no room: redraw in place, the walk until it is whole
				m.next.drawn = 0;
				m.cur = Tiles();
				s.tableDirty = true;
			}
		}
		pending.push_back( i );
	}
	std::sort( pending.begin(), pending.end(), [&]( int a, int b ) { return dist[a] < dist[b]; } );

	int faces = 0;
	if ( !pending.empty() ) {
		if ( !haveCasters )
			wwCellCubeCasters( scene, casters );
		// one instanced draw a caster for all the faces it touches (the vertex stage picks the face's viewport,
		// GL_ARB_shader_viewport_layer_array; notes/modules1/ATLASFACES_RESEARCH.md); else one draw a face
		const bool layered = !( wwAirLampAtlasRed() & 4 ) && layeredSupported();
		NifSkopeOpenGLContext::Program * prog = layered ? r->useProgram( "cell_shadowdepth6.prog" ) : nullptr;
		const bool useLayered = prog != nullptr;
		if ( !prog )
			prog = r->useProgram( "cell_shadowdepth.prog" );
		if ( prog ) {
			GLint prevRead = 0, prevDraw = 0, vp[4], sb[4], polyMode[2], depthFunc = 0, prevActive = 0;
			GLboolean depthMask = GL_TRUE, colorMask[4];
			GLfloat clearDepth = 1.0f;
			fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
			fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
			fn->glGetIntegerv( GL_VIEWPORT, vp );
			fn->glGetIntegerv( GL_SCISSOR_BOX, sb );
			fn->glGetIntegerv( GL_POLYGON_MODE, polyMode );
			fn->glGetIntegerv( GL_DEPTH_FUNC, &depthFunc );
			fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
			fn->glGetFloatv( GL_DEPTH_CLEAR_VALUE, &clearDepth );
			fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
			fn->glGetBooleanv( GL_COLOR_WRITEMASK, colorMask );
			const bool wasCull = fn->glIsEnabled( GL_CULL_FACE ), wasOffset = fn->glIsEnabled( GL_POLYGON_OFFSET_FILL );
			const bool wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );
			const bool wasBlend = fn->glIsEnabled( GL_BLEND );

			fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, s.fbo );
			fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, s.fbo );
			fn->glDrawBuffer( GL_NONE );
			fn->glReadBuffer( GL_NONE );
			fn->glEnable( GL_SCISSOR_TEST );
			fn->glDisable( GL_BLEND );
			fn->glDisable( GL_CULL_FACE );
			fn->glDisable( GL_POLYGON_OFFSET_FILL );
			fn->glEnable( GL_DEPTH_TEST );
			fn->glDepthFunc( GL_LEQUAL );
			fn->glDepthMask( GL_TRUE );
			fn->glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
			fn->glClearDepth( 1.0 );

			const Transform & vt = scene->view;
			const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
			const int lRel = prog->uniLocation( "shRelFromView" ), lClip = prog->uniLocation( "shClipFromRel" );
			const int lRn = prog->uniLocation( "shRadiusNear" );
			const int lFaceClip = useLayered ? prog->uniLocation( "shClipFace" ) : -1;
			const int lFaces = useLayered ? prog->uniLocation( "shFaces" ) : -1;
			std::vector<const WwCubeCaster *> mine;
			const bool faceCap = ( wwAirLampAtlasRed() & 2 ) != 0;
			auto full = [&]() { return faceCap ? faces >= kFacesPerFrame : faces > 0 && clock.nsecsElapsed() >= kFillNs; };
			for ( int i : pending ) {
				if ( full() )
					break;
				const WwCellLight & l = L->lights.at( i );
				Lamp & m = s.lamps[i];
				float rel[16];
				for ( int row = 0; row < 3; row++ ) {
					for ( int j = 0; j < 3; j++ )
						rel[j * 4 + row] = vt.rotation( j, row ) / sc;
					rel[12 + row] = cam[row] - l.pos[row];
				}
				rel[3] = rel[7] = rel[11] = 0.0f;
				rel[15] = 1.0f;
				if ( lRel >= 0 )
					fn->glUniformMatrix4fv( lRel, 1, GL_FALSE, rel );
				if ( lRn >= 0 )
					fn->glUniform2f( lRn, l.radius, l.nearClip );
				const float nz = 1.0f, fz = l.radius * 1.01f + 2.0f;
				const float A = ( fz + nz ) / ( fz - nz ), B = -2.0f * fz * nz / ( fz - nz );
				mine.clear();
				for ( const WwCubeCaster & c : casters ) {
					const float dx = c.c[0] - l.pos[0], dy = c.c[1] - l.pos[1], dz = c.c[2] - l.pos[2];
					const float reach = c.rad + l.radius;
					if ( dx * dx + dy * dy + dz * dz < reach * reach )
						mine.push_back( &c );
				}
				auto faceClip = [&]( int f, float * clip ) {
					for ( int c = 0; c < 3; c++ ) {
						clip[c * 4 + 0] = WwCubeFace::kU[f][c];
						clip[c * 4 + 1] = WwCubeFace::kV[f][c];
						clip[c * 4 + 2] = A * WwCubeFace::kM[f][c];
						clip[c * 4 + 3] = WwCubeFace::kM[f][c];
					}
					clip[12] = clip[13] = clip[15] = 0.0f;
					clip[14] = B;
				};
				if ( useLayered ) {	// the lamp's six faces at once: clear the tiles, one viewport + scissor a face
					float clips[6 * 16];
					const int sz = m.next.s;
					for ( int f = 0; f < 6; f++ ) {
						faceClip( f, clips + f * 16 );
						const int ox = int( m.next.at[f] >> 16 ), oy = int( m.next.at[f] & 0xffff );
						fn->glScissorIndexed( 0, ox, oy, sz, sz );	// glClear takes viewport 0's scissor
						fn->glClear( GL_DEPTH_BUFFER_BIT );
					}
					for ( int f = 0; f < 6; f++ ) {
						const int ox = int( m.next.at[f] >> 16 ), oy = int( m.next.at[f] & 0xffff );
						fn->glViewportIndexedf( GLuint( f ), float( ox ), float( oy ), float( sz ), float( sz ) );
						fn->glScissorIndexed( GLuint( f ), ox, oy, sz, sz );
					}
					if ( lFaceClip >= 0 )
						fn->glUniformMatrix4fv( lFaceClip, 6, GL_FALSE, clips );
					for ( const WwCubeCaster * c : mine ) {
						const float cr[3] = { c->c[0] - l.pos[0], c->c[1] - l.pos[1], c->c[2] - l.pos[2] };
						GLint list[6];
						int k = 0;
						for ( int f = 0; f < 6; f++ )
							if ( WwCubeFace::touches( f, cr, c->rad ) )
								list[k++] = f;
						if ( !k )
							continue;
						if ( lFaces >= 0 )
							fn->glUniform1iv( lFaces, k, list );
						prog->uni4m( "modelViewMatrix", c->sh->viewTrans().toMatrix4() );
						wwCellCubeDrawCaster( r, prog, c->sh, k );
					}
					faces += 6 - m.next.drawn;
					m.next.drawn = 6;
				}
				for ( ; m.next.drawn < 6 && !full(); m.next.drawn++, faces++ ) {
					const int f = m.next.drawn;
					const int ox = int( m.next.at[f] >> 16 ), oy = int( m.next.at[f] & 0xffff ), sz = m.next.s;
					fn->glViewport( ox, oy, sz, sz );
					fn->glScissor( ox, oy, sz, sz );
					fn->glClear( GL_DEPTH_BUFFER_BIT );
					float clip[16];
					faceClip( f, clip );
					if ( lClip >= 0 )
						fn->glUniformMatrix4fv( lClip, 1, GL_FALSE, clip );
					for ( const WwCubeCaster * c : mine ) {
						const float cr[3] = { c->c[0] - l.pos[0], c->c[1] - l.pos[1], c->c[2] - l.pos[2] };
						if ( !WwCubeFace::touches( f, cr, c->rad ) )
							continue;
						prog->uni4m( "modelViewMatrix", c->sh->viewTrans().toMatrix4() );
						wwCellCubeDrawCaster( r, prog, c->sh );
					}
				}
				if ( m.next.drawn == 6 ) {	// the new tiles are whole: the old go back
					freeTiles( s, m.cur );
					m.cur = m.next;
					m.next = Tiles();
					m.redraw = false;
					s.tableDirty = true;
				}
			}
			r->stopProgram();

			fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
			fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
			fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
			fn->glScissor( sb[0], sb[1], sb[2], sb[3] );
			fn->glPolygonMode( GL_FRONT_AND_BACK, GLenum( polyMode[0] ) );
			fn->glDepthFunc( GLenum( depthFunc ) );
			fn->glDepthMask( depthMask );
			fn->glColorMask( colorMask[0], colorMask[1], colorMask[2], colorMask[3] );
			fn->glClearDepth( clearDepth );
			wasCull ? fn->glEnable( GL_CULL_FACE ) : fn->glDisable( GL_CULL_FACE );
			wasOffset ? fn->glEnable( GL_POLYGON_OFFSET_FILL ) : fn->glDisable( GL_POLYGON_OFFSET_FILL );
			wasDepth ? fn->glEnable( GL_DEPTH_TEST ) : fn->glDisable( GL_DEPTH_TEST );
			wasScissor ? fn->glEnable( GL_SCISSOR_TEST ) : fn->glDisable( GL_SCISSOR_TEST );
			wasBlend ? fn->glEnable( GL_BLEND ) : fn->glDisable( GL_BLEND );
			fn->glActiveTexture( GLenum( prevActive ) );
		}
	}

	// the table: a ready lamp's six origins and its size
	if ( s.tableDirty ) {
		s.table.assign( size_t( n ) * kTexelsPerLamp * 4, 0.0f );
		for ( int i = 0; i < n; i++ ) {
			const Tiles & t = s.lamps[i].cur;
			if ( !t.s )
				continue;
			float * p = s.table.data() + size_t( i ) * kTexelsPerLamp * 4;
			for ( int f = 0; f < 6; f++ ) {
				p[f * 2 + 0] = float( t.at[f] >> 16 );
				p[f * 2 + 1] = float( t.at[f] & 0xffff );
			}
			p[12] = float( t.s );
			p[13] = 1.0f;
		}
		fn->glBindBuffer( GL_TEXTURE_BUFFER, s.buf );
		fn->glBufferData( GL_TEXTURE_BUFFER, GLsizeiptr( s.table.size() * sizeof( float ) ), s.table.data(), GL_DYNAMIC_DRAW );
		fn->glBindBuffer( GL_TEXTURE_BUFFER, 0 );
		s.tableDirty = false;
	}
	GLint prevActive = 0;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::AirLampAtlas ) );
	fn->glBindTexture( GL_TEXTURE_2D, s.tex );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::AirLampTable ) );
	fn->glBindTexture( GL_TEXTURE_BUFFER, s.tbo );
	fn->glTexBuffer( GL_TEXTURE_BUFFER, GL_RGBA32F, s.buf );
	fn->glActiveTexture( GLenum( prevActive ) );

	// the census: on a change of the ready / pending counts, and every 120 frames while tiles are drawn
	s.frames++;
	s.facesTotal += faces;
	const double ms = double( clock.nsecsElapsed() ) * 1e-6;
	s.drawMs += ms;
	s.maxMs = std::max( s.maxMs, ms );
	int ready = 0, tiled = 0, bySize[4] = { 0, 0, 0, 0 };	// 32, 64, 128, 256
	double used = 0.0;
	for ( const Lamp & m : s.lamps ) {
		ready += m.cur.s ? 1 : 0;
		if ( m.cur.s )
			bySize[std::clamp( int( std::log2( double( m.cur.s ) ) + 0.5 ) - 5, 0, 3 )]++;
		used += 6.0 * ( double( m.cur.s ) * m.cur.s + double( m.next.s ) * m.next.s );
	}
	for ( int w : want )
		tiled += w ? 1 : 0;
	const int pend = int( pending.size() );
	if ( ready != s.lastReady || ( pend == 0 ) != ( s.lastPending == 0 ) || ( pend && s.frames % 120 == 0 ) ) {
		std::fprintf( stderr, "air lamp atlas: lamps %d in air %d ready %d pending %d faces %lld used %.1f%% cpu %.2f ms (%lld frames, max %.2f) tiles 32/64/128/256 %d/%d/%d/%d movers %lld redraws %lld%s\n",
			n, tiled, ready, pend, (long long)s.facesTotal, 100.0 * used / ( double( kAtlas ) * kAtlas ), s.drawMs, (long long)s.frames,
			s.maxMs, bySize[0], bySize[1], bySize[2], bySize[3], (long long)s.moves, (long long)s.redraws, haveCasters ? "" : " (frozen)" );
		s.lastReady = ready;
		s.lastPending = pend;
	}
	return n;
}

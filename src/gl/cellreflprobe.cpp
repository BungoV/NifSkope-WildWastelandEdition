/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellreflprobe.h"
#include "cellunits.h"
#include "celluniform.h"	// lane CELLUNI2
#include "gl/alphabgsm.h"	// lane ALPHABGSM1: the side passes' alpha-test census
#include "gl/celldrawstate.h"
#include "wwswitch.h"	// fix TDFIX2: a live set re-reads the switches
#include "gl/celllooks.h"	// lane OPENCELLFIX2: the Reflections master

#include "gl/celllights.h"
#include "gl/cellssr.h"
#include "gl/cellwater.h"
#include "gl/glnode.h"
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "gl/lookdevstage.h"
#include "gl/renderer.h"
#include "gl/sunshadow.h"

#include <QElapsedTimer>
#include <QHash>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

/* ---- lane REFLPROBE1: the placed reflection probes (cellreflprobe.h, res/shaders/cell_refl*.{frag,glsl}) ---- */
bool rtddgiSkyBelowRedNow();	// lane SKYBELOW2: SKYBELOW1's red (src/rtddgicell.cpp), read every frame (live)

namespace
{

constexpr int kFace = 64;			// a G-buffer face and an atlas tile (RDR2's camera cube is as small)
constexpr int kPlaced = 32;			// resident placed probes
constexpr int kSlots = kPlaced + 1;	// + the camera's (slot 0)
constexpr int kCols = 8, kRows = 5;	// the atlas's tiles (40 >= 33), 512 x 320
constexpr int kLods = 4;			// 64 -> 4 texels a tile; tiles stay aligned at every mip
constexpr float kBox = 2048.0f;		// a room's split, the outdoor grid's cell
constexpr float kFadeK = 0.15f;		// the fade band, of the box's smallest side

struct RpState
{
	bool loaded = false;
	bool on = false, pinOn = false;
	int red = 0;		// 1 fixed (the material cube returned); relight reds 2 nosun, 4 nolights, 8 nogi, 16 white
	int probe = 0;		// WW_CELL_LIT_PROBE
	bool all = false;	// WW_CELL_REFLPROBE_ALL: capture and relight every probe at once
	bool greyscale = false;	// the draw being set up reads the greyscale palette
	QString last;
	quint64 liveGen = 0;	// fix TDFIX2: a live set of WW_CELL_REFLPROBE* / WW_CELL_LIT_PROBE re-reads them
};

RpState & rps()
{
	static RpState s;
	if ( !s.loaded || wwSwitchMovedFor( s.liveGen, { "WW_CELL_REFLPROBE", "WW_CELL_LIT_PROBE" } ) ) {
		s.loaded = true;
		s.liveGen = wwSwitchGen();
		s.red = 0;	// the reds are or-ed: back to none before the re-read
		const QByteArray pin = qgetenv( "WW_CELL_REFLPROBE" ).trimmed();
		s.pinOn = !pin.isEmpty();
		s.on = s.pinOn ? pin != "0" : QSettings().value( QStringLiteral( "WW/CellReflProbe" ), false ).toBool();
		for ( const QByteArray & w : qgetenv( "WW_CELL_REFLPROBE_RED" ).trimmed().split( ',' ) ) {
			const QByteArray t = w.trimmed();
			s.red |= t == "fixed" ? 1 : t == "nosun" ? 2 : t == "nolights" ? 4 : t == "nogi" ? 8 : t == "white" ? 16 : 0;
		}
		s.probe = qEnvironmentVariableIntValue( "WW_CELL_LIT_PROBE" );
		s.all = qEnvironmentVariableIntValue( "WW_CELL_REFLPROBE_ALL" ) != 0;
	}
	return s;
}

struct Probe
{
	quint64 key = 0;		// 0: empty slot
	float pos[3] = {}, bmin[3] = {}, bmax[3] = {};
	int faces = 0;			// G-buffer faces captured (6: whole)
	bool lit = false;
	quint64 litHash = 0;
};

struct Candidate
{
	quint64 key;
	float pos[3], bmin[3], bmax[3];
};

struct RpGpu
{
	GLuint alb = 0, nd = 0, depth = 0, capFbo = 0;	// the G-buffer cubes (RGBA8, RGBA16F; layer-faces 6 x slot)
	GLuint atlas = 0, relFbo = 0;					// the relit probes (R11G11B10F, octahedral tiles)
	bool failed = false;
	const void * doc = nullptr;
	quint64 sceneKey = 0;			// the statics drawn: a change recaptures every probe
	const void * roomsGi = nullptr;	// the GI the room candidates came from
	size_t roomsSize = 0;
	std::vector<Candidate> rooms;	// per room (or room part): its probe and box
	Probe slot[kSlots];
	int camFace = 0;				// the camera probe's next face
	quint64 stamp = 1;				// bumps when the arrays the surfaces read change
	QHash<GLuint, quint64> progStamp;
	std::vector<float> uPos, uMin, uMax;
	int count = 0;					// probes in the arrays
	bool ready = false;				// some probe is lit
	qint64 kb = 0;
};

QHash<const void *, RpGpu> & rpGpus()
{
	static QHash<const void *, RpGpu> g;
	return g;
}

quint64 mix64( quint64 h, quint64 v )
{
	h ^= v + 0x9e3779b97f4a7c15ULL + ( h << 6 ) + ( h >> 2 );
	return h;
}

quint64 mixF( quint64 h, float f )
{
	std::uint32_t u;
	std::memcpy( &u, &f, 4 );
	return mix64( h, u );
}

bool allocGpu( NifSkopeOpenGLContext::GLFunctions * fn, RpGpu & g )
{
	if ( g.alb || g.failed )
		return !g.failed;
	GLint prevActive = 0;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::ReflGbufAlb ) );
	auto cubeArray = [&]( GLuint & t, GLenum internal, GLenum type ) {
		fn->glGenTextures( 1, &t );
		fn->glBindTexture( GL_TEXTURE_CUBE_MAP_ARRAY, t );
		fn->glTexImage3D( GL_TEXTURE_CUBE_MAP_ARRAY, 0, GLint( internal ), kFace, kFace, 6 * kSlots, 0, GL_RGBA, type, nullptr );
		fn->glTexParameteri( GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAX_LEVEL, 0 );
		fn->glTexParameteri( GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
		fn->glBindTexture( GL_TEXTURE_CUBE_MAP_ARRAY, 0 );
	};
	cubeArray( g.alb, GL_RGBA8, GL_UNSIGNED_BYTE );
	cubeArray( g.nd, GL_RGBA16F, GL_HALF_FLOAT );
	fn->glGenRenderbuffers( 1, &g.depth );
	fn->glBindRenderbuffer( GL_RENDERBUFFER, g.depth );
	fn->glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, kFace, kFace );
	fn->glBindRenderbuffer( GL_RENDERBUFFER, 0 );
	fn->glGenFramebuffers( 1, &g.capFbo );

	fn->glGenTextures( 1, &g.atlas );
	fn->glBindTexture( GL_TEXTURE_2D, g.atlas );
	for ( int l = 0; l <= kLods; l++ )
		fn->glTexImage2D( GL_TEXTURE_2D, l, GL_R11F_G11F_B10F, ( kCols * kFace ) >> l, ( kRows * kFace ) >> l, 0, GL_RGB,
			GL_FLOAT, nullptr );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0 );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, kLods );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	fn->glGenFramebuffers( 1, &g.relFbo );
	fn->glActiveTexture( GLenum( prevActive ) );
	// G-buffer 12 B a texel x 6 faces, the atlas 4 B a texel with its mips
	g.kb = ( qint64( kSlots ) * 6 * kFace * kFace * 12 + qint64( kCols * kRows ) * kFace * kFace * 4 * 4 / 3 ) / 1024;
	return true;
}

/* the room candidates: per room of the fine grid its air cells' box (split into <= 2048-unit parts), the probe at
 * the part's air cell nearest the part's mean (an air cell, so never inside a wall) */
void buildRooms( RpGpu & g, const WwCellGi * G )
{
	g.rooms.clear();
	g.roomsGi = G;
	g.roomsSize = G ? G->rooms.size() : 0;
	if ( !G || G->rooms.empty() )
		return;
	const int dx = G->roomsDims[0], dy = G->roomsDims[1], dz = G->roomsDims[2];
	if ( dx <= 0 || dy <= 0 || dz <= 0 || size_t( dx ) * dy * dz != G->rooms.size() )
		return;
	struct Box { int lo[3] = { 1 << 30, 1 << 30, 1 << 30 }, hi[3] = { -1, -1, -1 }; };
	std::map<int, Box> box;
	auto roomsOf = [&]( size_t i, int out[2] ) {
		const int k = int( G->rooms[i] + 0.5f );
		out[0] = k / 4096 - 1;
		out[1] = k - ( k / 4096 ) * 4096 - 1;
	};
	auto each = [&]( auto && f ) {
		size_t i = 0;
		for ( int z = 0; z < dz; z++ )
			for ( int y = 0; y < dy; y++ )
				for ( int x = 0; x < dx; x++, i++ ) {
					int rm[2];
					roomsOf( i, rm );
					if ( rm[0] < 0 )
						continue;	// solid
					for ( int j = 0; j < 2; j++ )
						if ( rm[j] >= 1 && ( j == 0 || rm[1] != rm[0] ) )
							f( rm[j], x, y, z );
				}
	};
	each( [&]( int room, int x, int y, int z ) {
		Box & b = box[room];
		const int c[3] = { x, y, z };
		for ( int a = 0; a < 3; a++ ) {
			b.lo[a] = std::min( b.lo[a], c[a] );
			b.hi[a] = std::max( b.hi[a], c[a] );
		}
	} );
	const float cell = G->roomsCell;
	struct Part { double sum[3] = {}; int n = 0; float best = 1e30f; int at[3] = {}; };
	std::map<std::pair<int, int>, Part> parts;
	auto partOf = [&]( const Box & b, const int c[3], int split[3] ) {
		int p = 0;
		for ( int a = 2; a >= 0; a-- ) {
			const int span = b.hi[a] - b.lo[a] + 1;
			split[a] = std::max( 1, int( std::ceil( span * cell / kBox ) ) );
			p = p * 64 + std::min( ( c[a] - b.lo[a] ) * split[a] / span, 63 );
		}
		return p;
	};
	each( [&]( int room, int x, int y, int z ) {
		const int c[3] = { x, y, z };
		int split[3];
		Part & p = parts[{ room, partOf( box[room], c, split ) }];
		p.sum[0] += x;
		p.sum[1] += y;
		p.sum[2] += z;
		p.n++;
	} );
	each( [&]( int room, int x, int y, int z ) {
		const int c[3] = { x, y, z };
		int split[3];
		Part & p = parts[{ room, partOf( box[room], c, split ) }];
		float d = 0.0f;
		for ( int a = 0; a < 3; a++ ) {
			const float m = float( p.sum[a] / p.n ) - float( c[a] );
			d += m * m;
		}
		if ( d < p.best ) {
			p.best = d;
			p.at[0] = x;
			p.at[1] = y;
			p.at[2] = z;
		}
	} );
	for ( const auto & kv : parts ) {
		const Box & b = box[kv.first.first];
		const Part & p = kv.second;
		int split[3], pi = kv.first.second;
		int idx[3];
		partOf( b, p.at, split );
		for ( int a = 0; a < 3; a++ ) {
			idx[a] = pi % 64;
			pi /= 64;
		}
		Candidate c;
		c.key = mix64( mix64( 0x524f4f4dULL, quint64( kv.first.first ) ), quint64( kv.first.second ) );
		for ( int a = 0; a < 3; a++ ) {
			const int span = b.hi[a] - b.lo[a] + 1;
			const int lo = b.lo[a] + idx[a] * span / split[a], hi = b.lo[a] + ( idx[a] + 1 ) * span / split[a];
			c.bmin[a] = G->roomsOrigin[a] + float( lo ) * cell;
			c.bmax[a] = G->roomsOrigin[a] + float( hi ) * cell;
			c.pos[a] = G->roomsOrigin[a] + ( float( p.at[a] ) + 0.5f ) * cell;
		}
		g.rooms.push_back( c );
	}
}

struct Caster
{
	const Shape * sh;
	float c[3];
	float rad;
	Matrix4 mv;
	bool alphaTested;
	float atRef;	// lane ALPHABGSM1: the threshold the colour draw tests with (Shape::wwDrawAlpha)
};

// GL's cube faces: major axis m, s along u, t along v (as the light shadow cubes, celllights.cpp)
const float kM[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
const float kU[6][3] = { { 0, 0, -1 }, { 0, 0, 1 }, { 1, 0, 0 }, { 1, 0, 0 }, { 1, 0, 0 }, { -1, 0, 0 } };
const float kV[6][3] = { { 0, -1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }, { 0, -1, 0 }, { 0, -1, 0 } };

//! a sphere against one face's 90-degree frustum (the four side planes m +- u, m +- v)
bool inFace( int face, const float rel[3], float rad )
{
	const float m = rel[0] * kM[face][0] + rel[1] * kM[face][1] + rel[2] * kM[face][2];
	const float u = rel[0] * kU[face][0] + rel[1] * kU[face][1] + rel[2] * kU[face][2];
	const float v = rel[0] * kV[face][0] + rel[1] * kV[face][1] + rel[2] * kV[face][2];
	const float r = rad * 1.41421356f;
	return m - u >= -r && m + u >= -r && m - v >= -r && m + v >= -r;
}

}	// namespace

bool wwCellReflProbeOn()
{
	return rps().on && ( rps().pinOn || wwGroupLets( WwGroup::Reflections ) );	// lane OPENCELLFIX2: the group's master
}

void wwCellReflProbeSetOn( bool on )
{
	RpState & s = rps();
	if ( s.pinOn )
		return;
	s.on = on;
	QSettings().setValue( QStringLiteral( "WW/CellReflProbe" ), on );
}

void wwCellReflProbeNote( bool greyscalePalette )
{
	rps().greyscale = greyscalePalette;
}

void wwCellReflProbePass( Scene * scene, bool run )
{
	if ( !scene || !scene->renderer )
		return;
	RpState & s = rps();
	Renderer * r = scene->renderer;
	RpGpu & g = rpGpus()[r];
	auto echo = [&]( const QString & line ) {
		if ( line != s.last ) {
			s.last = line;
			std::fprintf( stderr, "reflprobe: %s\n", qPrintable( line ) );
		}
	};
	if ( !s.on ) {
		g.ready = false;
		echo( s.pinOn ? QStringLiteral( "off(WW_CELL_REFLPROBE=0)" ) : QStringLiteral( "off(row)" ) );
		return;
	}
	if ( !run || !wwCellLightsWanted( scene ) ) {
		g.ready = false;
		return;
	}
	if ( r->globalUniforms->projectionMatrix[3][3] == 1.0f )
		return;	// orthographic: no cell lighting either
	const WwCellLighting * L = wwCellLightsFor( scene->nifModel );
	if ( !L )
		return;
	auto fn = r->fn;
	if ( !allocGpu( fn, g ) ) {
		echo( QStringLiteral( "refused(no textures)" ) );
		return;
	}
	QElapsedTimer timer;
	timer.start();

	// the statics the captures draw: what the GI bake's soup takes (no actors, effects, blends, water)
	std::vector<Caster> casters;
	AlphaBgsmTally atTally;	// lane ALPHABGSM1: the census (gl/alphabgsm.h)
	quint64 sceneKey = mix64( 0, quint64( reinterpret_cast<std::uintptr_t>( scene->nifModel ) ) );
	for ( Node * node : scene->nodes.list() ) {
		const Shape * sh = dynamic_cast<const Shape *>( node );
		if ( !sh || !sh->isVisible() || !sh->wwCastsSunShadow() || sh->wwGpuSkinned() )
			continue;
		if ( sh->verts.isEmpty() || sh->triangles.isEmpty() || wwCellWaterIsShape( scene->nifModel, sh->id() ) )
			continue;
		const BoundSphere b = sh->bounds();
		Caster c { sh, { b.center[0], b.center[1], b.center[2] }, b.radius, sh->viewTrans().toMatrix4(), false, 0.0f };
		/* lane ALPHABGSM1: the colour draw's test (a BGSM's wins over the NIF's): a BGSM-only test (GlassWindows01)
		 * was captured solid into the cubes. Red WW_ALPHABGSM_RED=refl */
		const Shape::WwDrawAlpha da = sh->wwDrawAlpha( "refl" );
		AlphaBgsmTally::note( atTally, sh->wwAlphaTested(), da.test );
		c.alphaTested = da.test && sh->wwShaderLighting() && !sh->coords.isEmpty()
			&& sh->coords.constFirst().size() == sh->verts.size();
		c.atRef = sh->wwLeafSideThreshold( da, "refl" );	// lane TREELEAF2: keeps the colour pass's texels
		casters.push_back( c );
		sceneKey = mix64( sceneKey, quint64( sh->id() ) );
		sceneKey = mix64( sceneKey, quint64( sh->wwLeafCovRef( "refl" ) ) );	// lane TREELEAF2: a live red re-captures
		sceneKey = mix64( sceneKey, quint64( c.alphaTested ? 1 : 0 ) );	// lane ALPHABGSM1: a live red re-captures
	}
	AlphaBgsmTally::report( atTally, "reflection probes", qint64( casters.size() ) );
	if ( g.doc != scene->nifModel || g.sceneKey != sceneKey ) {
		for ( Probe & p : g.slot )
			p = Probe();
		g.doc = scene->nifModel;
		g.sceneKey = sceneKey;
		g.stamp++;
	}

	// the camera (the view's inverse, as the light shadow pass)
	const Transform & vt = scene->view;
	const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
	float cam[3];
	for ( int k = 0; k < 3; k++ ) {
		float w = 0.0f;
		for ( int j = 0; j < 3; j++ )
			w -= vt.rotation( j, k ) * vt.translation[j];
		cam[k] = w / sc;
	}

	// the wanted placed probes: the rooms where the GI has them, else the outdoor grid round the camera
	const WwCellGi * G = wwCellGiFor( scene->nifModel );
	if ( g.roomsGi != G || g.roomsSize != ( G ? G->rooms.size() : 0 ) )
		buildRooms( g, G );
	std::vector<Candidate> cand;
	const bool byRooms = L->interior && !g.rooms.empty();
	const float camZ = std::round( cam[2] / 256.0f ) * 256.0f;
	if ( byRooms ) {
		cand = g.rooms;
	} else {
		const int ix = int( std::floor( cam[0] / kBox ) ), iy = int( std::floor( cam[1] / kBox ) );
		for ( int j = -1; j <= 1; j++ )
			for ( int i = -1; i <= 1; i++ ) {
				Candidate c;
				c.key = mix64( mix64( mix64( 0x47524944ULL, quint64( std::int64_t( ix + i ) ) ), quint64( std::int64_t( iy + j ) ) ),
					quint64( std::int64_t( camZ ) ) );
				c.bmin[0] = float( ix + i ) * kBox;
				c.bmin[1] = float( iy + j ) * kBox;
				c.bmin[2] = camZ - kBox * 0.5f;
				for ( int a = 0; a < 3; a++ ) {
					c.bmax[a] = c.bmin[a] + kBox;
					c.pos[a] = c.bmin[a] + kBox * 0.5f;
				}
				c.pos[2] = camZ;
				cand.push_back( c );
			}
	}
	auto boxDist = [&]( const Candidate & c ) {
		float d2 = 0.0f;
		for ( int a = 0; a < 3; a++ ) {
			const float e = std::max( { c.bmin[a] - cam[a], 0.0f, cam[a] - c.bmax[a] } );
			d2 += e * e;
		}
		return d2;
	};
	std::sort( cand.begin(), cand.end(), [&]( const Candidate & a, const Candidate & b ) { return boxDist( a ) < boxDist( b ); } );
	if ( int( cand.size() ) > kPlaced )
		cand.resize( kPlaced );
	// keep the slots still wanted, free the rest, seat the new ones
	bool changed = false;
	for ( int k = 1; k < kSlots; k++ ) {
		Probe & p = g.slot[k];
		if ( !p.key )
			continue;
		if ( std::none_of( cand.begin(), cand.end(), [&]( const Candidate & c ) { return c.key == p.key; } ) ) {
			p = Probe();
			changed = true;
		}
	}
	for ( const Candidate & c : cand ) {
		bool held = false;
		for ( int k = 1; k < kSlots && !held; k++ )
			held = g.slot[k].key == c.key;
		if ( held )
			continue;
		for ( int k = 1; k < kSlots; k++ ) {
			if ( g.slot[k].key )
				continue;
			Probe & p = g.slot[k];
			p = Probe();
			p.key = c.key;
			std::copy( c.pos, c.pos + 3, p.pos );
			std::copy( c.bmin, c.bmin + 3, p.bmin );
			std::copy( c.bmax, c.bmax + 3, p.bmax );
			changed = true;
			break;
		}
	}
	// the camera probe (RDR2): at the camera snapped to 128 units, faces cycled one a frame
	{
		Probe & p = g.slot[0];
		float at[3];
		for ( int a = 0; a < 3; a++ )
			at[a] = std::round( cam[a] / 128.0f ) * 128.0f;
		if ( !p.key || at[0] != p.pos[0] || at[1] != p.pos[1] || at[2] != p.pos[2] ) {
			const bool had = p.key != 0;
			p.key = 1;
			const float half[3] = { L->interior ? 1024.0f : 4096.0f, L->interior ? 1024.0f : 4096.0f, L->interior ? 512.0f : 2048.0f };
			for ( int a = 0; a < 3; a++ ) {
				p.pos[a] = at[a];
				p.bmin[a] = at[a] - half[a];
				p.bmax[a] = at[a] + half[a];
			}
			if ( !had ) {
				p.faces = 0;
				p.lit = false;
			}
			changed = true;
		}
	}

	// the capture: the camera probe's next face, the nearest unfinished placed probe's next face (all: every face)
	std::vector<std::pair<int, int>> jobs;	// slot, face
	if ( s.all ) {
		for ( int k = 0; k < kSlots; k++ )
			if ( g.slot[k].key )
				for ( int f = g.slot[k].faces; f < 6; f++ )
					jobs.emplace_back( k, f );
		if ( g.slot[0].faces >= 6 )
			for ( int f = 0; f < 6; f++ )
				jobs.emplace_back( 0, f );
	} else {
		jobs.emplace_back( 0, g.slot[0].faces < 6 ? g.slot[0].faces : g.camFace );
		int best = -1;
		float bestD = 1e30f;
		for ( int k = 1; k < kSlots; k++ ) {
			const Probe & p = g.slot[k];
			if ( !p.key || p.faces >= 6 )
				continue;
			Candidate c;
			std::copy( p.bmin, p.bmin + 3, c.bmin );
			std::copy( p.bmax, p.bmax + 3, c.bmax );
			const float d = boxDist( c );
			if ( d < bestD ) {
				bestD = d;
				best = k;
			}
		}
		if ( best > 0 )
			jobs.emplace_back( best, g.slot[best].faces );
	}

	GLint prevRead = 0, prevDraw = 0, vp[4], polyMode[2], depthFunc = 0, prevActive = 0;
	GLboolean depthMask = GL_TRUE, colorMask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	fn->glGetIntegerv( GL_POLYGON_MODE, polyMode );
	fn->glGetIntegerv( GL_DEPTH_FUNC, &depthFunc );
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	fn->glGetBooleanv( GL_COLOR_WRITEMASK, colorMask );
	const bool wasCull = fn->glIsEnabled( GL_CULL_FACE ), wasDepth = fn->glIsEnabled( GL_DEPTH_TEST );
	const bool wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST ), wasBlend = fn->glIsEnabled( GL_BLEND );
	const bool wasStencil = fn->glIsEnabled( GL_STENCIL_TEST );
	auto restore = [&]() {
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
		fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
		fn->glPolygonMode( GL_FRONT_AND_BACK, GLenum( polyMode[0] ) );
		fn->glDepthFunc( GLenum( depthFunc ) );
		fn->glDepthMask( depthMask );
		fn->glColorMask( colorMask[0], colorMask[1], colorMask[2], colorMask[3] );
		wasCull ? fn->glEnable( GL_CULL_FACE ) : fn->glDisable( GL_CULL_FACE );
		wasDepth ? fn->glEnable( GL_DEPTH_TEST ) : fn->glDisable( GL_DEPTH_TEST );
		wasScissor ? fn->glEnable( GL_SCISSOR_TEST ) : fn->glDisable( GL_SCISSOR_TEST );
		wasBlend ? fn->glEnable( GL_BLEND ) : fn->glDisable( GL_BLEND );
		wasStencil ? fn->glEnable( GL_STENCIL_TEST ) : fn->glDisable( GL_STENCIL_TEST );
		fn->glActiveTexture( GLenum( prevActive ) );
	};
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glDisable( GL_CULL_FACE );	// both sides (a cube face mirrors the winding; the shader turns the normal)
	fn->glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );

	int captured = 0;
	qint64 draws = 0;
	bool complete = true;
	if ( !jobs.empty() ) {
		NifSkopeOpenGLContext::Program * prog = r->useProgram( "cell_reflcap.prog" );
		if ( !prog ) {
			restore();
			echo( QStringLiteral( "refused(cell_reflcap.prog did not link)" ) );
			return;
		}
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.capFbo );
		fn->glFramebufferRenderbuffer( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, g.depth );
		const GLenum bufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		fn->glDrawBuffers( 2, bufs );
		fn->glEnable( GL_DEPTH_TEST );
		fn->glDepthFunc( GL_LEQUAL );
		fn->glDepthMask( GL_TRUE );
		fn->glViewport( 0, 0, kFace, kFace );
		const int lRel = prog->uniLocation( "rpRelFromView" ), lClip = prog->uniLocation( "rpClipFromRel" );
		for ( const auto & job : jobs ) {
			Probe & p = g.slot[job.first];
			const int face = job.second;
			fn->glFramebufferTextureLayer( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, g.alb, 0, 6 * job.first + face );
			fn->glFramebufferTextureLayer( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, g.nd, 0, 6 * job.first + face );
			if ( fn->glCheckFramebufferStatus( GL_DRAW_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
				complete = false;
				break;
			}
			const float zero[4] = { 0, 0, 0, 0 };
			fn->glClearBufferfv( GL_COLOR, 0, zero );
			fn->glClearBufferfv( GL_COLOR, 1, zero );
			fn->glClearDepth( 1.0 );
			fn->glClear( GL_DEPTH_BUFFER_BIT );
			float rel[16];
			for ( int row = 0; row < 3; row++ ) {
				for ( int j = 0; j < 3; j++ )
					rel[j * 4 + row] = vt.rotation( j, row ) / sc;
				rel[12 + row] = cam[row] - p.pos[row];
			}
			rel[3] = rel[7] = rel[11] = 0.0f;
			rel[15] = 1.0f;
			// the reach: the box's far corner from the probe (the camera probe's whole box), at least 4096
			float reach = 0.0f;
			for ( int a = 0; a < 3; a++ ) {
				const float e = std::max( p.bmax[a] - p.pos[a], p.pos[a] - p.bmin[a] );
				reach += e * e;
			}
			reach = std::max( std::sqrt( reach ) * 2.0f, 4096.0f );
			const float n = 4.0f, f = reach;
			const float A = ( f + n ) / ( f - n ), B = -2.0f * f * n / ( f - n );
			float clip[16];
			for ( int c = 0; c < 3; c++ ) {
				clip[c * 4 + 0] = kU[face][c];
				clip[c * 4 + 1] = kV[face][c];
				clip[c * 4 + 2] = A * kM[face][c];
				clip[c * 4 + 3] = kM[face][c];
			}
			clip[12] = clip[13] = clip[15] = 0.0f;
			clip[14] = B;
			if ( lRel >= 0 )
				fn->glUniformMatrix4fv( lRel, 1, GL_FALSE, rel );
			if ( lClip >= 0 )
				fn->glUniformMatrix4fv( lClip, 1, GL_FALSE, clip );
			for ( const Caster & c : casters ) {
				const float d[3] = { c.c[0] - p.pos[0], c.c[1] - p.pos[1], c.c[2] - p.pos[2] };
				if ( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] > ( reach + c.rad ) * ( reach + c.rad ) || !inFace( face, d, c.rad ) )
					continue;
				int texunit = 0;
				BSShaderLightingProperty * bsp = c.sh->wwShaderLighting();
				const BSLightingShaderProperty * lsp = c.sh->wwLightingShader();
				const bool uv = !c.sh->coords.isEmpty() && c.sh->coords.constFirst().size() == c.sh->verts.size();
				c.sh->wwLeafCovArm( "refl" );	// lane TREELEAF2: the colour pass's coverage copy (src/gl/wwleafcov.h)
				const bool base = bsp && uv && prog->uniSampler( bsp, "BaseMap", 0, texunit, QString(), bsp->clampMode, QString() );
				c.sh->wwLeafCovDisarm();
				prog->uni1b( "rpHasBase", base );
				prog->uni1b( "rpAlphaTest", c.alphaTested && base );
				if ( bsp ) {
					prog->uni2f( "uvScale", bsp->uvScale.x, bsp->uvScale.y );
					prog->uni2f( "uvOffset", bsp->uvOffset.x, bsp->uvOffset.y );
				} else {
					prog->uni2f( "uvScale", 1.0f, 1.0f );
					prog->uni2f( "uvOffset", 0.0f, 0.0f );
				}
				prog->uni1f( "alpha", lsp ? lsp->alpha : 1.0f );
				prog->uni1f( "alphaThreshold", c.atRef );
				const bool vc = c.sh->colors.size() == c.sh->verts.size();
				const bool nm = c.sh->norms.size() == c.sh->verts.size();
				prog->uni1b( "rpHasVc", vc );
				prog->uni4m( "modelViewMatrix", c.mv );
				const float * attrv[8] = { &( c.sh->verts.constFirst()[0] ), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
				std::uint64_t mask = 3;
				if ( vc ) {
					attrv[1] = &( c.sh->colors.constFirst()[0] );
					mask |= 0x40;
				}
				if ( nm ) {
					attrv[2] = &( c.sh->norms.constFirst()[0] );
					mask |= 0x300;
				}
				if ( uv ) {
					attrv[7] = &( c.sh->coords.constFirst().constFirst()[0] );
					mask |= 0x20000000ULL;
				}
				r->drawShape( (unsigned int) ( c.sh->verts.size() ), mask, (unsigned int) ( c.sh->triangles.size() * 3 ),
					GL_TRIANGLES, GL_UNSIGNED_SHORT, attrv, c.sh->triangles.constData() );
				draws++;
			}
			if ( job.first == 0 && p.faces >= 6 ) {
				g.camFace = ( face + 1 ) % 6;
				p.lit = false;	// a recaptured face: relight the camera probe
			} else {
				p.faces = std::max( p.faces, face + 1 );
				if ( job.first == 0 )
					g.camFace = p.faces % 6;
			}
			captured++;
		}
		fn->glFramebufferTextureLayer( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0, 0, 0 );
		fn->glFramebufferTextureLayer( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, 0, 0, 0 );
		r->stopProgram();
	}
	if ( !complete ) {
		restore();
		echo( QStringLiteral( "refused(capture framebuffer incomplete)" ) );
		return;
	}

	// the relight: the camera probe whenever a face of it came in, one placed probe a frame (all: every one) whose
	// light changed; the light is read back from the program the cell setters filled
	int relit = 0;
	{
		NifSkopeOpenGLContext::Program * prog = r->useProgram( "cell_reflrelight.prog" );
		if ( !prog ) {
			restore();
			echo( QStringLiteral( "refused(cell_reflrelight.prog did not link)" ) );
			return;
		}
		wwLookdevFogUniforms( scene );
		wwSunShadowUniforms( scene );
		wwCellLightsUniforms( scene );
		// the screen's helpers have no screen here: every light walks, no half-res GI, no decal, no contact shadow
		prog->uni1b( "cellClusterOn", false );
		prog->uni1b( "cellGiHalfOn", false );
		prog->uni1b( "cellAoDecalOn", false );
		prog->uni1i( "ussContactOn", 0 );
		prog->uni1i( "cellPass", 0 );
		prog->uni1i( "cellProbe", 0 );
		prog->uni1i( "rpRed", ( s.red >> 1 ) & 15 );
		/* lane SKYBELOW2 (bungo 10-30, SKYBELOW1's rule for every probe): a G-buffer miss below the horizon is the
		 * ground (outdoors the probe's own ground albedo x the sky from above, indoors 0), never the sky ambient
		 * (res/shaders/cell_skybelow.glsl). Ships on; WW_SKYBELOW_RED=1 keeps today's main, live: it is in the hash */
		const bool skyBelowRed = rtddgiSkyBelowRedNow();
		prog->uni1i( "rpSkyBelow", skyBelowRed ? 0 : 1 );
		auto uniF = [&]( const char * name, int cnt, quint64 h ) {
			float v[4] = {};
			const int l = prog->uniLocation( name );
			if ( l >= 0 )
				fn->glGetUniformfv( prog->id, l, v );
			for ( int i = 0; i < cnt; i++ )
				h = mixF( h, v[i] );
			return h;
		};
		auto uniI = [&]( const char * name, quint64 h ) {
			GLint v = 0;
			const int l = prog->uniLocation( name );
			if ( l >= 0 )
				fn->glGetUniformiv( prog->id, l, &v );
			return mix64( h, quint64( v ) );
		};
		quint64 h = uniI( "cellOn", 0 );
		h = uniF( "cellDirTo", 3, h );
		h = uniF( "cellDirColor", 3, h );
		h = uniF( "cellDalc[0]", 4, h );
		h = uniF( "cellDalc[1]", 4, h );
		h = uniF( "cellDalc[2]", 4, h );
		h = uniI( "cellLightCount", h );
		h = uniI( "cellHasDir", h );
		h = uniI( "cellGiOn", h );
		h = uniI( "cellGiSky", h );
		h = uniF( "cellSkyVis", 1, h );
		for ( int a = 0; a < 3; a++ )
			h = mixF( h, r->globalUniforms->lightSourceAmbient[a] );
		h = mix64( h, quint64( reinterpret_cast<std::uintptr_t>( G ) ) );
		h = mix64( h, G ? quint64( G->rgba.size() ) : 0 );
		h = mix64( h, skyBelowRed ? 0x5b2u : 0x5b1u );	// lane SKYBELOW2: a set relights every probe
		std::vector<int> todo;
		if ( g.slot[0].faces >= 6 && ( !g.slot[0].lit || g.slot[0].litHash != h ) )
			todo.push_back( 0 );
		int best = -1;
		float bestD = 1e30f;
		for ( int k = 1; k < kSlots; k++ ) {
			const Probe & p = g.slot[k];
			if ( !p.key || p.faces < 6 || ( p.lit && p.litHash == h ) )
				continue;
			if ( s.all ) {
				todo.push_back( k );
				continue;
			}
			Candidate c;
			std::copy( p.bmin, p.bmin + 3, c.bmin );
			std::copy( p.bmax, p.bmax + 3, c.bmax );
			const float d = boxDist( c );
			if ( d < bestD ) {
				bestD = d;
				best = k;
			}
		}
		if ( best > 0 )
			todo.push_back( best );
		if ( !todo.empty() ) {
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::ReflGbufAlb ) );
			fn->glBindTexture( GL_TEXTURE_CUBE_MAP_ARRAY, g.alb );
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::ReflGbufNd ) );
			fn->glBindTexture( GL_TEXTURE_CUBE_MAP_ARRAY, g.nd );
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::ReflProbe ) );
			fn->glBindTexture( GL_TEXTURE_2D, 0 );	// the atlas is the target
			prog->uni1i( "rpAlb", WwCellUnit::ReflGbufAlb );
			prog->uni1i( "rpNd", WwCellUnit::ReflGbufNd );
			prog->uni4m( "rpViewFromWorld", vt.toMatrix4() );
			fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.relFbo );
			fn->glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.atlas, 0 );
			fn->glDrawBuffer( GL_COLOR_ATTACHMENT0 );
			fn->glDisable( GL_DEPTH_TEST );
			fn->glDepthMask( GL_FALSE );
			if ( fn->glCheckFramebufferStatus( GL_DRAW_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE ) {
				static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
				static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
				const float * attrs = quad;
				for ( int k : todo ) {
					Probe & p = g.slot[k];
					const int tx = ( k % kCols ) * kFace, ty = ( k / kCols ) * kFace;
					fn->glViewport( tx, ty, kFace, kFace );
					prog->uni1f( "rpLayer", float( k ) );
					prog->uni3f( "rpTile", float( tx ), float( ty ), float( kFace ) );
					prog->uni3f( "rpPos", p.pos[0], p.pos[1], p.pos[2] );
					r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
					p.lit = true;
					p.litHash = h;
					relit++;
				}
				fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::ReflProbe ) );
				fn->glBindTexture( GL_TEXTURE_2D, g.atlas );
				fn->glGenerateMipmap( GL_TEXTURE_2D );	// the box prefilter (RESEARCH.md: not GGX-convolved)
			}
			fn->glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0 );
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::ReflGbufAlb ) );
			fn->glBindTexture( GL_TEXTURE_CUBE_MAP_ARRAY, 0 );
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::ReflGbufNd ) );
			fn->glBindTexture( GL_TEXTURE_CUBE_MAP_ARRAY, 0 );
		}
		r->stopProgram();
	}
	restore();

	// the arrays the surfaces read: the lit probes, the camera's first
	std::vector<float> pos, mn, mx;
	int count = 0, placedLit = 0, resident = 0;
	for ( int k = 0; k < kSlots; k++ ) {
		const Probe & p = g.slot[k];
		if ( !p.key )
			continue;
		resident += k > 0 ? 1 : 0;
		if ( !p.lit )
			continue;
		placedLit += k > 0 ? 1 : 0;
		float side = 1e30f;
		for ( int a = 0; a < 3; a++ )
			side = std::min( side, p.bmax[a] - p.bmin[a] );
		pos.insert( pos.end(), { p.pos[0], p.pos[1], p.pos[2], float( k ) } );
		mn.insert( mn.end(), { p.bmin[0], p.bmin[1], p.bmin[2], std::max( side * kFadeK, 1.0f ) } );
		mx.insert( mx.end(), { p.bmax[0], p.bmax[1], p.bmax[2], k == 0 ? 1.0f : 0.0f } );
		count++;
	}
	if ( changed || relit || pos != g.uPos || mn != g.uMin || mx != g.uMax ) {
		g.uPos.swap( pos );
		g.uMin.swap( mn );
		g.uMax.swap( mx );
		g.count = count;
		g.stamp++;
	}
	g.ready = g.count > 0;
	echo( QStringLiteral( "on%1 %2 placed=%3 resident=%4/%5 lit=%6 camera=%7 faces/frame=%8 kb=%9" )
		.arg( ( s.red & 1 ) ? QStringLiteral( "(RED=fixed: material cube returned)" ) : QString() )
		.arg( byRooms ? QStringLiteral( "rooms" ) : QStringLiteral( "grid" ) ).arg( cand.size() ).arg( resident ).arg( kPlaced )
		.arg( placedLit ).arg( g.slot[0].lit ? QStringLiteral( "lit" ) : QStringLiteral( "capturing" ) )
		.arg( s.all ? QStringLiteral( "all" ) : QStringLiteral( "2" ) ).arg( g.kb )
		+ ( rtddgiSkyBelowRedNow() ? QStringLiteral( " below-horizon miss=sky (WW_SKYBELOW_RED)" )	// lane SKYBELOW2
			: QStringLiteral( " below-horizon miss=ground" ) ) );
	if ( qEnvironmentVariableIsSet( "WW_CELL_REFLPROBE_TIME" ) && ( captured || relit ) )
		std::fprintf( stderr, "reflprobe frame: captured=%d draws=%lld relit=%d cpu-ms=%lld\n", captured,
			(long long) draws, relit, (long long) timer.elapsed() );
}

void wwCellReflProbeDraw( Scene * scene, bool cellProgram )
{
	if ( !cellProgram || !scene || !scene->renderer )
		return;
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->getCurrentProgram();
	if ( !prog || prog->uniLocation( "cellReflOn" ) < 0 )
		return;
	RpState & s = rps();
	auto it = rpGpus().find( r );
	const bool on = s.on && !( s.red & 1 ) && it != rpGpus().end() && it->ready && it->doc == scene->nifModel
		&& !s.greyscale && !wwCellSsrInPass() && !( wwCellUniRed() ? r->fn->glIsEnabled( GL_BLEND ) : wwDrawBlend() )	// CELLUNI2
		&& ( s.probe == 0 || s.probe == 66 );
	prog->uni1b( "cellReflOn", on );
	if ( !on )
		return;
	RpGpu & g = *it;
	auto fn = r->fn;
	// the atlas on its own unit, read through the idle GreyscaleMap sampler (the palette is off for this draw)
	WwCellUni::Units<std::remove_pointer_t<decltype( fn )>> u( fn, wwCellUniDsa() );	// lane CELLUNI2: no query with DSA
	u.bind( WwCellUnit::ReflProbe, GL_TEXTURE_2D, g.atlas );
	u.done();
	prog->uni1i( "GreyscaleMap", WwCellUnit::ReflProbe );
	quint64 & ps = g.progStamp[prog->id];
	if ( ps == g.stamp )
		return;
	ps = g.stamp;
	prog->uni1i( "cellReflCount", g.count );
	if ( g.count > 0 ) {
		const int lp = prog->uniLocation( "cellReflPos[%d]", 0 ), ln = prog->uniLocation( "cellReflMin[%d]", 0 ),
			lx = prog->uniLocation( "cellReflMax[%d]", 0 );
		fn->glUniform4fv( lp, g.count, g.uPos.data() );
		fn->glUniform4fv( ln, g.count, g.uMin.data() );
		fn->glUniform4fv( lx, g.count, g.uMax.data() );
		// lane CELLUNI2: raw array writes, so the uniform cache forgets those locations
		WwCellUni::wrote( prog->wwUni, lp, g.count );
		WwCellUni::wrote( prog->wwUni, ln, g.count );
		WwCellUni::wrote( prog->wwUni, lx, g.count );
	}
	prog->uni4f( "cellReflAtlas", FloatVector4( 1.0f / float( kCols * kFace ), 1.0f / float( kRows * kFace ), float( kFace ),
		float( kCols ) ) );
	prog->uni1f( "cellReflLods", float( kLods ) );
}

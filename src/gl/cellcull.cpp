/*! Lane SUNCELL1: the cell view's per-placement culling (see cellcull.h). */
#include "cellcull.h"

#include "gl/cellumbra.h"
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "gl/renderer.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <string>

namespace
{

struct Counts
{
	std::int64_t shapes = 0, runs = 0, drawn = 0, culled = 0, calls = 0, tris = 0, trisAll = 0;
	std::int64_t occluded = 0, roomCulled = 0, casterHidden = 0;	// lane SUNCELL1: the Previs row's share
	std::int64_t tomeDrawn = 0, tomeCulled = 0, ungoverned = 0, tomeCasterHidden = 0;	// lane UMBRA1: the tome's share
	std::int64_t cDrawn[3] = { 0, 0, 0 }, cCulled[3] = { 0, 0, 0 };
	bool operator==( const Counts & o ) const
	{
		for ( int i = 0; i < 3; i++ )
			if ( cDrawn[i] != o.cDrawn[i] || cCulled[i] != o.cCulled[i] )
				return false;
		return shapes == o.shapes && runs == o.runs && drawn == o.drawn && culled == o.culled && calls == o.calls
			&& tris == o.tris && trisAll == o.trisAll && occluded == o.occluded && roomCulled == o.roomCulled
			&& casterHidden == o.casterHidden && tomeDrawn == o.tomeDrawn && tomeCulled == o.tomeCulled
			&& ungoverned == o.ungoverned && tomeCasterHidden == o.tomeCasterHidden;
	}
};

//! lane SUNCELL1: one primitive in view space
struct ViewBox
{
	Vector3 c, ax[3];
	float half[3] = { 0, 0, 0 };
	int type = 1;
};

//! lane SUNCELL1: the previs scene in this frame's view space, with the rooms the camera reaches
struct PrevisCache
{
	const void * nif = nullptr;
	std::uint64_t frame = ~std::uint64_t( 0 );
	bool valid = false;
	std::vector<ViewBox> occ, rooms;
	std::vector<char> roomSeen;
	bool roomsActive = false;
	int roomsSeen = 0, portalsPassed = 0;
};

/*! lane UMBRA1: one previs block's decoded tome and this camera's query of it */
struct UmbraBlock
{
	std::uint32_t block = 0;
	WwUmbraTome tome;
	QHash<std::uint32_t, std::vector<int>> fdByCode;	//!< the 0xFD (combined) objects by their cell code, bits 14..23
	WwUmbraResult res;
	bool usable = false;	//!< decoded, queried for this camera, and a start cell found
};

//! lane UMBRA1: what a ref form id is to the tomes (block -1 = nothing, -2 = an ambiguous low-24-bit match)
struct UmbraRef
{
	int block = -1, obj = -1;
	std::uint32_t code = 0;
	bool combined = false;
};

//! lane UMBRA1: one run's governance: the block whose tome decides it (-1 = none) and the objects it stands for
struct UmbraGov
{
	int block = -1;
	std::vector<int> objs;
};

struct UmbraDoc
{
	std::vector<UmbraBlock> blocks;
	QHash<std::uint32_t, UmbraRef> byRef, byLow;
	QHash<int, std::vector<UmbraGov>> gov;	//!< shape block -> one entry a run
	float camKey[19] = {};
	bool camValid = false;
	std::uint64_t frame = ~std::uint64_t( 0 );
	int queried = 0, noStart = 0, cells = 0, visible = 0, objects = 0, govUid = 0, govComb = 0, decoded = 0;
};

struct CullState
{
	bool loaded = false, on = false, pinned = false;
	int red = 0;
	QHash<const void *, QHash<int, std::vector<WwCullRun>>> docs;
	Counts cur, last;
	bool any = false, printed = false;
	// lane SUNCELL1: the Previs row
	bool previsOn = false, previsPinned = false;
	int previsRed = 0;
	QHash<const void *, WwPrevisScene> previs;
	PrevisCache pc;
	std::uint64_t frame = 0;
	int lastOcc = -1, lastRooms = -1, lastSeen = -1, lastPortals = -1;
	// lane UMBRA1: the tome path (it rides the Previs row)
	bool umbraOn = true, umbraDepth = false, probing = false;
	int umbraRed = 0;
	QString umbraDump;
	QHash<const void *, std::shared_ptr<UmbraDoc>> umbra;
	const UmbraDoc * umbraLast = nullptr;
	QString lastUmbraLine;
	float depthKey[19] = {};
	bool depthKeyValid = false;
};

CullState & st()
{
	static CullState s;
	if ( !s.loaded ) {
		s.loaded = true;
		const QByteArray pin = qgetenv( "WW_CELL_CULL" ).trimmed();
		if ( !pin.isEmpty() ) {
			s.pinned = true;
			s.on = pin != "0";
		} else {
			s.on = QSettings().value( QStringLiteral( "WW/CellCull" ), false ).toBool();
		}
		s.red = qgetenv( "WW_CELL_CULL_RED" ).trimmed() == "casters" ? 1 : 0;
		const QByteArray pv = qgetenv( "WW_CELL_PREVIS" ).trimmed();
		if ( !pv.isEmpty() ) {
			s.previsPinned = true;
			s.previsOn = pv != "0";
		} else {
			s.previsOn = QSettings().value( QStringLiteral( "WW/CellPrevis" ), false ).toBool();
		}
		s.previsRed = qgetenv( "WW_CELL_PREVIS_RED" ).trimmed() == "casters" ? 1 : 0;
		// lane UMBRA1
		s.umbraOn = qgetenv( "WW_CELL_UMBRA" ).trimmed() != "0";
		s.umbraRed = qgetenv( "WW_CELL_UMBRA_RED" ).trimmed() == "casters" ? 1 : 0;
		s.umbraDump = QString::fromLocal8Bit( qgetenv( "WW_CELL_UMBRA_DUMP" ) ).trimmed();
		s.umbraDepth = !s.umbraDump.isEmpty() && qgetenv( "WW_CELL_UMBRA_DEPTH" ).trimmed() == "1";
	}
	return s;
}

ViewBox toView( const Transform & view, const WwPrevisBox & b )
{
	ViewBox v;
	v.c = view * b.c;
	for ( int k = 0; k < 3; k++ ) {
		v.ax[k] = view.rotation * b.ax[k];
		const float l = v.ax[k].length();
		if ( l > 0.0f )
			v.ax[k] /= l;
		v.half[k] = b.half[k] * std::fabs( view.scale );
	}
	v.type = b.type;
	return v;
}

//! the sphere (s, r) within the box grown by g on every side (view space)
bool inBox( const ViewBox & b, const Vector3 & s, float g )
{
	const Vector3 d = s - b.c;
	for ( int k = 0; k < 3; k++ )
		if ( std::fabs( Vector3::dotproduct( d, b.ax[k] ) ) > b.half[k] + g )
			return false;
	return true;
}

/*! Does the quad (centre c, normal n0, in-plane unit axes u, w with half extents hu, hw) hide the sphere (s, r)
 *  from the camera at the view-space origin? Only when the whole sphere lies past the quad's plane AND inside the
 *  four planes through the camera and the quad's edges (conservative: a sphere that straddles is drawn). */
bool quadHides( const Vector3 & c, const Vector3 & n0, const Vector3 & u, float hu, const Vector3 & w, float hw,
	const Vector3 & s, float r )
{
	Vector3 n = n0;
	float d = Vector3::dotproduct( n, c );
	if ( d < 0.0f ) {
		n = -n;
		d = -d;
	}
	if ( d < 1.0f )
		return false;	// the camera on the occluder's plane
	if ( Vector3::dotproduct( n, s ) - d <= r )
		return false;
	const Vector3 p[4] = { c + u * hu + w * hw, c - u * hu + w * hw, c - u * hu - w * hw, c + u * hu - w * hw };
	for ( int i = 0; i < 4; i++ ) {
		Vector3 e = Vector3::crossproduct( p[i], p[( i + 1 ) & 3] );
		const float l = e.length();
		if ( l <= 0.0f )
			return false;
		e /= l;
		if ( Vector3::dotproduct( e, c ) < 0.0f )
			e = -e;
		if ( Vector3::dotproduct( e, s ) < r )
			return false;
	}
	return true;
}

//! one occluder: a plane is one quad; a box hides with any face the camera sees (never from inside it)
bool occluderHides( const ViewBox & o, const Vector3 & s, float r )
{
	if ( o.type == 3 ) {
		int m = 0;
		for ( int k = 1; k < 3; k++ )
			if ( o.half[k] < o.half[m] )
				m = k;
		const int i = ( m + 1 ) % 3, j = ( m + 2 ) % 3;
		return quadHides( o.c, o.ax[m], o.ax[i], o.half[i], o.ax[j], o.half[j], s, r );
	}
	if ( inBox( o, Vector3( 0.0f, 0.0f, 0.0f ), 0.0f ) )
		return false;
	for ( int k = 0; k < 3; k++ )
		for ( int sg = -1; sg <= 1; sg += 2 ) {
			const Vector3 nk = o.ax[k] * float( sg );
			const Vector3 fc = o.c + nk * o.half[k];
			if ( Vector3::dotproduct( nk, fc ) >= 0.0f )
				continue;	// a face turned away from the camera
			const int i = ( k + 1 ) % 3, j = ( k + 2 ) % 3;
			if ( quadHides( fc, nk, o.ax[i], o.half[i], o.ax[j], o.half[j], s, r ) )
				return true;
		}
	return false;
}

//! the camera's four side planes in view space (Gribb-Hartmann from the column-major projection), normalised
void cameraPlanes( Scene * scene, float pl[4][4] )
{
	const auto & pm = scene->renderer->globalUniforms->projectionMatrix;	// pm[column][row]
	for ( int i = 0; i < 4; i++ ) {
		const int axis = i >> 1;		// 0 = x, 1 = y
		const float sg = ( i & 1 ) ? -1.0f : 1.0f;
		float l = 0.0f;
		for ( int c = 0; c < 4; c++ ) {
			pl[i][c] = pm[c][3] + sg * pm[c][axis];
			if ( c < 3 )
				l += pl[i][c] * pl[i][c];
		}
		l = std::sqrt( l );
		if ( l > 0.0f )
			for ( int c = 0; c < 4; c++ )
				pl[i][c] /= l;
	}
}

bool insidePlanes( const float pl[4][4], const Vector3 & v, float r )
{
	for ( int i = 0; i < 4; i++ )
		if ( pl[i][0] * v[0] + pl[i][1] * v[1] + pl[i][2] * v[2] + pl[i][3] < -r )
			return false;
	return true;
}

/*! lane SUNCELL1: the previs scene in this frame's view space, and the rooms the camera reaches: the rooms that
 *  hold the camera and their linked rooms, then through every portal of a reached room that lies in the frustum,
 *  the room on its other side (and its linked rooms), until nothing new is reached. A camera in no room culls by
 *  no room. Built in the camera pass only, so a shadow pass's matrices never feed it. */
void previsBuild( CullState & s, Scene * scene, const float pl[4][4] )
{
	PrevisCache & pc = s.pc;
	const void * nif = scene->nifModel;
	if ( pc.valid && pc.nif == nif && pc.frame == s.frame )
		return;
	pc = PrevisCache();
	pc.nif = nif;
	pc.frame = s.frame;
	auto it = s.previs.constFind( nif );
	if ( it == s.previs.constEnd() )
		return;
	const WwPrevisScene & sc = *it;
	const Transform & view = scene->view;
	for ( const WwPrevisBox & b : sc.occluders )
		pc.occ.push_back( toView( view, b ) );
	QHash<std::uint32_t, int> roomAt;
	for ( const WwPrevisBox & b : sc.rooms ) {
		roomAt.insert( b.ref, int( pc.rooms.size() ) );
		pc.rooms.push_back( toView( view, b ) );
	}
	pc.roomSeen.assign( pc.rooms.size(), 0 );
	std::vector<int> todo;
	std::function<void( int )> reach = [&]( int i ) {
		if ( i < 0 || pc.roomSeen[i] )
			return;
		pc.roomSeen[i] = 1;
		pc.roomsSeen++;
		todo.push_back( i );
		for ( const std::uint32_t l : sc.rooms[i].linked )
			reach( roomAt.value( l, -1 ) );
	};
	for ( int i = 0; i < int( pc.rooms.size() ); i++ )
		if ( inBox( pc.rooms[i], Vector3( 0.0f, 0.0f, 0.0f ), 0.0f ) )
			reach( i );
	pc.roomsActive = !todo.empty();
	std::vector<char> passed( sc.portals.size(), 0 );
	while ( !todo.empty() ) {
		const std::uint32_t ref = sc.rooms[todo.back()].ref;
		todo.pop_back();
		for ( size_t p = 0; p < sc.portals.size(); p++ ) {
			const WwPrevisBox & q = sc.portals[p];
			if ( passed[p] || ( q.from != ref && q.to != ref ) )
				continue;
			const ViewBox v = toView( view, q );
			float r2 = 0.0f;
			for ( int k = 0; k < 3; k++ )
				r2 += v.half[k] * v.half[k];
			if ( !insidePlanes( pl, v.c, std::sqrt( r2 ) ) )
				continue;
			passed[p] = 1;
			pc.portalsPassed++;
			reach( roomAt.value( q.from == ref ? q.to : q.from, -1 ) );
		}
	}
	pc.valid = true;
}

//! lane SUNCELL1: 0 = drawn, 1 = behind an occluder, 2 = in a room the camera does not reach
int previsHidden( const PrevisCache & pc, const Vector3 & v, float r )
{
	if ( !pc.valid )
		return 0;
	if ( pc.roomsActive ) {
		bool inRoom = false;
		for ( const ViewBox & b : pc.rooms )
			if ( inBox( b, v, 0.0f ) ) {
				inRoom = true;
				break;
			}
		if ( inRoom ) {
			bool seen = false;
			for ( size_t i = 0; i < pc.rooms.size() && !seen; i++ )
				seen = pc.roomSeen[i] && inBox( pc.rooms[i], v, r );
			if ( !seen )
				return 2;
		}
	}
	for ( const ViewBox & o : pc.occ )
		if ( occluderHides( o, v, r ) )
			return 1;
	return 0;
}

//! the visible runs into (first, count) pairs, adjacent ones merged; returns the culled count
template <class Visible>
std::int64_t collect( const std::vector<WwCullRun> & runs, std::int64_t numTris, Visible vis,
	std::vector<std::uint32_t> & out, std::int64_t & drawnTris )
{
	out.clear();
	std::int64_t culled = 0;
	drawnTris = 0;
	for ( const WwCullRun & r : runs ) {
		if ( std::int64_t( r.first ) >= numTris )
			break;
		const std::uint32_t cnt = std::uint32_t( std::min<std::int64_t>( r.count, numTris - std::int64_t( r.first ) ) );
		if ( !vis( r ) ) {
			culled++;
			continue;
		}
		drawnTris += cnt;
		if ( !out.empty() && out[out.size() - 2] + out[out.size() - 1] == r.first )
			out[out.size() - 1] += cnt;
		else {
			out.push_back( r.first );
			out.push_back( cnt );
		}
	}
	return culled;
}

/*! lane UMBRA1: the GL world -> clip matrix, row-major, in double. The cell document's scene space is the game's
 *  world (every cell shape carries the build origin as its translation), so this is world -> clip. */
void umbraGlClip( Scene * scene, double G[4][4] )
{
	const auto & pm = scene->renderer->globalUniforms->projectionMatrix;	// pm[column][row]
	const Transform & v = scene->view;
	double P[4][4], V[4][4];
	for ( int r = 0; r < 4; r++ )
		for ( int c = 0; c < 4; c++ ) {
			P[r][c] = double( pm[c][r] );
			V[r][c] = 0.0;
		}
	for ( int r = 0; r < 3; r++ ) {
		for ( int c = 0; c < 3; c++ )
			V[r][c] = double( v.rotation( r, c ) ) * double( v.scale );
		V[r][3] = double( v.translation[r] );
	}
	V[3][3] = 1.0;
	for ( int r = 0; r < 4; r++ )
		for ( int c = 0; c < 4; c++ ) {
			double a = 0.0;
			for ( int k = 0; k < 4; k++ )
				a += P[r][k] * V[k][c];
			G[r][c] = a;
		}
}

//! lane UMBRA1: the camera as the tome's query takes it: world -> D3D clip (GL's z row becomes (z + w) / 2)
bool umbraCamera( Scene * scene, WwUmbraCamera & cam )
{
	if ( !scene || !scene->renderer )
		return false;
	const Transform & v = scene->view;
	if ( v.scale == 0.0f )
		return false;
	double G[4][4];
	umbraGlClip( scene, G );
	for ( int r = 0; r < 4; r++ )
		for ( int c = 0; c < 4; c++ )
			cam.M[r * 4 + c] = float( r == 2 ? 0.5 * ( G[2][c] + G[3][c] ) : G[r][c] );
	for ( int i = 0; i < 3; i++ ) {	// the camera = R^T (-t) / scale
		double a = 0.0;
		for ( int r = 0; r < 3; r++ )
			a += double( v.rotation( r, i ) ) * double( v.translation[r] );
		cam.pos[i] = float( -a / double( v.scale ) );
	}
	return true;
}

void umbraKey( const WwUmbraCamera & cam, float * key )
{
	std::memcpy( key, cam.M, sizeof( cam.M ) );
	std::memcpy( key + 16, cam.pos, sizeof( cam.pos ) );
}

QString umbraHex( std::uint32_t v )
{
	return QStringLiteral( "%1" ).arg( v, 8, 16, QLatin1Char( '0' ) ).toUpper();
}

//! lane UMBRA1: every block's query, once a frame and only when the camera moved (the camera pass only)
void umbraFrame( CullState & s, Scene * scene, UmbraDoc & d )
{
	if ( d.frame == s.frame )
		return;
	d.frame = s.frame;
	s.umbraLast = &d;
	WwUmbraCamera cam;
	if ( !umbraCamera( scene, cam ) )
		return;
	float key[19];
	umbraKey( cam, key );
	if ( d.camValid && std::memcmp( key, d.camKey, sizeof( key ) ) == 0 )
		return;
	std::memcpy( d.camKey, key, sizeof( key ) );
	d.camValid = true;
	d.queried = d.noStart = d.cells = d.visible = d.objects = 0;
	const std::vector<char> gates;	// every gate open: doors are not tracked
	for ( UmbraBlock & b : d.blocks ) {
		b.usable = false;
		if ( !b.tome.ok )
			continue;
		wwUmbraQuery( b.tome, cam, gates, b.res );
		d.objects += int( b.tome.numObjects );
		if ( !b.res.ok ) {
			std::fprintf( stderr, "cell umbra tome %08X: query FAILED (%s); its refs keep the occluder path\n",
				unsigned( b.block ), qPrintable( b.res.error ) );
			continue;
		}
		d.queried++;
		if ( b.res.starts.empty() )
			d.noStart++;	// the camera is outside this tome: its refs keep the occluder path
		else
			b.usable = true;
		d.cells += int( b.res.keys.size() );
		d.visible += int( b.res.visible.size() );
		if ( !s.umbraDump.isEmpty() ) {
			QDir().mkpath( s.umbraDump );
			QFile f( QDir( s.umbraDump ).filePath( QStringLiteral( "query_%1.txt" ).arg( umbraHex( b.block ) ) ) );
			if ( f.open( QIODevice::WriteOnly ) ) {
				const std::string t = wwUmbraQueryText( b.tome, cam, gates, b.res );
				f.write( t.data(), qint64( t.size() ) );
			}
		}
	}
}

UmbraRef umbraLookup( const UmbraDoc & d, std::uint32_t ref )
{
	if ( !ref )
		return UmbraRef();
	auto it = d.byRef.constFind( ref );
	if ( it != d.byRef.constEnd() )
		return it.value();
	auto lo = d.byLow.constFind( ref & 0x00FFFFFFU );
	if ( lo != d.byLow.constEnd() && lo.value().block >= 0 )
		return lo.value();
	return UmbraRef();
}

//! lane UMBRA1: one shape's runs' governance, worked out once (a cell shape's world spheres never move)
const std::vector<UmbraGov> & umbraGovFor( UmbraDoc & d, const Shape * sh, int block, const std::vector<WwCullRun> & runs )
{
	auto it = d.gov.find( block );
	if ( it != d.gov.end() && it.value().size() == runs.size() )
		return it.value();
	std::vector<UmbraGov> g( runs.size() );
	const Transform wt = sh->worldTrans();
	for ( size_t i = 0; i < runs.size(); i++ ) {
		const WwCullRun & r = runs[i];
		const UmbraRef u = umbraLookup( d, r.ref );
		if ( u.block < 0 || u.block >= int( d.blocks.size() ) || !d.blocks[size_t( u.block )].tome.ok )
			continue;
		const UmbraBlock & b = d.blocks[size_t( u.block )];
		if ( !u.combined ) {	// a reference the tome names by its own form id
			g[i].block = u.block;
			g[i].objs.push_back( u.obj );
			d.govUid++;
			continue;
		}
		// a precombined ref: the tome's combined objects of its cell that overlap the run's sphere (grown one unit)
		auto f = b.fdByCode.constFind( u.code );
		if ( f == b.fdByCode.constEnd() )
			continue;
		const Vector3 c = wt * r.center;
		const float rad = r.radius * std::fabs( wt.scale ) + 1.0f;
		for ( const int o : f.value() ) {
			const float * bx = &b.tome.bounds[6 * size_t( o )];
			float d2 = 0.0f;
			for ( int a = 0; a < 3; a++ ) {
				const float q = c[a] < bx[a] ? bx[a] - c[a] : ( c[a] > bx[3 + a] ? c[a] - bx[3 + a] : 0.0f );
				d2 += q * q;
			}
			if ( d2 <= rad * rad )
				g[i].objs.push_back( o );
		}
		if ( !g[i].objs.empty() ) {
			g[i].block = u.block;
			d.govComb++;
		}
	}
	d.gov.insert( block, std::move( g ) );
	return d.gov.find( block ).value();
}

//! lane UMBRA1: 0 = not governed (or its block has no answer for this camera), 1 = the tome sees it, 2 = it hides it
int umbraRun( const UmbraDoc & d, const std::vector<UmbraGov> & g, size_t i )
{
	if ( i >= g.size() || g[i].block < 0 )
		return 0;
	const UmbraBlock & b = d.blocks[size_t( g[i].block )];
	if ( !b.usable )
		return 0;
	for ( const int o : g[i].objs )
		if ( o >= 0 && size_t( o ) < b.res.objVisible.size() && b.res.objVisible[size_t( o )] )
			return 1;
	return 2;
}

//! lane UMBRA1: Gauss-Jordan with partial pivoting
bool invert4( const double m[4][4], double out[4][4] )
{
	double a[4][8];
	for ( int r = 0; r < 4; r++ )
		for ( int c = 0; c < 8; c++ )
			a[r][c] = c < 4 ? m[r][c] : ( c - 4 == r ? 1.0 : 0.0 );
	for ( int c = 0; c < 4; c++ ) {
		int p = c;
		for ( int r = c + 1; r < 4; r++ )
			if ( std::fabs( a[r][c] ) > std::fabs( a[p][c] ) )
				p = r;
		if ( a[p][c] == 0.0 )
			return false;
		if ( p != c )
			for ( int k = 0; k < 8; k++ )
				std::swap( a[p][k], a[c][k] );
		const double inv = 1.0 / a[c][c];
		for ( int k = 0; k < 8; k++ )
			a[c][k] *= inv;
		for ( int r = 0; r < 4; r++ ) {
			if ( r == c )
				continue;
			const double f = a[r][c];
			for ( int k = 0; k < 8; k++ )
				a[r][k] -= f * a[c][k];
		}
	}
	for ( int r = 0; r < 4; r++ )
		for ( int c = 0; c < 4; c++ )
			out[r][c] = a[r][c + 4];
	return true;
}

} // namespace

void wwCellCullBegin( const void * nif )
{
	st().docs.remove( nif );
	st().previs.remove( nif );	// lane SUNCELL1
	st().pc = PrevisCache();
	st().umbra.remove( nif );	// lane UMBRA1
	st().umbraLast = nullptr;
	st().depthKeyValid = false;
}

void wwCellUmbraSet( const void * nif, std::vector<WwUmbraBlockIn> && blocks )
{
	CullState & s = st();
	auto doc = std::make_shared<UmbraDoc>();
	for ( WwUmbraBlockIn & in : blocks ) {
		UmbraBlock b;
		b.block = in.block;
		const int bi = int( doc->blocks.size() );
		if ( !b.tome.load( in.tome ) ) {
			std::fprintf( stderr, "cell umbra tome %08X: FAILED (%s); its refs keep the occluder path\n",
				unsigned( in.block ), qPrintable( b.tome.error ) );
			doc->blocks.push_back( std::move( b ) );
			continue;
		}
		doc->decoded++;
		UmbraDoc & d = *doc;
		auto addLow = [&d]( std::uint32_t id, const UmbraRef & u ) {
			auto lo = d.byLow.find( id & 0x00FFFFFFU );
			if ( lo == d.byLow.end() )
				d.byLow.insert( id & 0x00FFFFFFU, u );
			else if ( lo.value().block != u.block || lo.value().obj != u.obj || lo.value().code != u.code
				|| lo.value().combined != u.combined )
				lo.value().block = -2;	// two different things share the low 24 bits: no fallback for either
		};
		int refIds = 0, fdIds = 0;
		for ( std::uint32_t o = 0; o < b.tome.numObjects && o < b.tome.uids.size(); o++ ) {
			const std::uint32_t uid = b.tome.uids[o];
			if ( ( uid >> 24 ) == 0xFDU ) {
				b.fdByCode[uid & 0x00FFC000U].push_back( int( o ) );
				fdIds++;
				continue;
			}
			UmbraRef u;
			u.block = bi;
			u.obj = int( o );
			if ( !d.byRef.contains( uid ) )
				d.byRef.insert( uid, u );
			addLow( uid, u );
			refIds++;
		}
		for ( const auto & c : in.combined ) {
			UmbraRef u;
			u.block = bi;
			u.code = c.second & 0x00FFC000U;
			u.combined = true;
			if ( !d.byRef.contains( c.first ) )
				d.byRef.insert( c.first, u );
			addLow( c.first, u );
		}
		std::fprintf( stderr, "cell umbra tome %08X: decoded, objects %u (%d reference ids, %d combined ids), tiles %u "
			"(%u leaf), cells %u, gates %u; %d precombined refs in its loaded cells\n", unsigned( in.block ),
			unsigned( b.tome.numObjects ), refIds, fdIds, unsigned( b.tome.numTiles ), unsigned( b.tome.numLeaf ),
			unsigned( b.tome.numCells ), unsigned( b.tome.numGates ), int( in.combined.size() ) );
		if ( !s.umbraDump.isEmpty() ) {
			QDir().mkpath( s.umbraDump );
			QFile f( QDir( s.umbraDump ).filePath( QStringLiteral( "tome_%1.txt" ).arg( umbraHex( in.block ) ) ) );
			if ( f.open( QIODevice::WriteOnly ) ) {
				const std::string t = b.tome.dump();
				f.write( t.data(), qint64( t.size() ) );
			}
		}
		doc->blocks.push_back( std::move( b ) );
	}
	s.umbra.insert( nif, doc );
	s.umbraLast = nullptr;
	s.depthKeyValid = false;
}

bool wwCellUmbraDepthWanted( Scene * scene )
{
	CullState & s = st();
	if ( !s.umbraDepth || !scene || !scene->renderer || !s.umbra.contains( scene->nifModel ) )
		return false;
	WwUmbraCamera cam;
	if ( !umbraCamera( scene, cam ) )
		return false;
	float key[19];
	umbraKey( cam, key );
	return !s.depthKeyValid || std::memcmp( key, s.depthKey, sizeof( key ) ) != 0;
}

void wwCellUmbraProbing( bool on )
{
	st().probing = on;
}

void wwCellUmbraDepthWrite( Scene * scene, const float * depth, int w, int h )
{
	CullState & s = st();
	if ( !scene || !scene->renderer || !depth || w <= 0 || h <= 0 || s.umbraDump.isEmpty() )
		return;
	WwUmbraCamera cam;
	if ( !umbraCamera( scene, cam ) )
		return;
	umbraKey( cam, s.depthKey );
	s.depthKeyValid = true;
	double G[4][4], Gi[4][4];
	umbraGlClip( scene, G );
	if ( !invert4( G, Gi ) )
		return;
	std::string out;
	int n = 0;
	char b[64];
	for ( int y = 4; y < h; y += 8 )	// every 8th pixel; row 0 = the bottom (GL)
		for ( int x = 4; x < w; x += 8 ) {
			const float dd = depth[size_t( y ) * size_t( w ) + size_t( x )];
			if ( !( dd > 0.0f && dd < 1.0f ) )
				continue;	// the background
			const double ndc[4] = { 2.0 * ( x + 0.5 ) / w - 1.0, 2.0 * ( y + 0.5 ) / h - 1.0, 2.0 * double( dd ) - 1.0, 1.0 };
			double p[4];
			for ( int r = 0; r < 4; r++ ) {
				p[r] = 0.0;
				for ( int c = 0; c < 4; c++ )
					p[r] += Gi[r][c] * ndc[c];
			}
			if ( p[3] == 0.0 )
				continue;
			std::uint32_t wv[3];
			for ( int k = 0; k < 3; k++ ) {
				const float f = float( p[k] / p[3] );
				std::memcpy( &wv[k], &f, 4 );
			}
			std::snprintf( b, sizeof( b ), "pt=%08X,%08X,%08X\n", unsigned( wv[0] ), unsigned( wv[1] ), unsigned( wv[2] ) );
			out += b;
			n++;
		}
	QDir().mkpath( s.umbraDump );
	const QString path = QDir( s.umbraDump ).filePath( QStringLiteral( "depth.txt" ) );
	QFile f( path );
	if ( f.open( QIODevice::WriteOnly ) )
		f.write( out.data(), qint64( out.size() ) );
	std::fprintf( stderr, "cell umbra depth: %d points of a %dx%d unculled depth readback written to %s\n", n, w, h,
		qPrintable( path ) );
}

void wwCellPrevisSet( const void * nif, WwPrevisScene && sc )
{
	st().previs.insert( nif, std::move( sc ) );
	st().pc = PrevisCache();
}

bool wwCellPrevisOn()
{
	return st().previsOn;
}

void wwCellPrevisSetOn( bool on )
{
	CullState & s = st();
	if ( s.previsPinned )
		return;
	s.previsOn = on;
	QSettings().setValue( QStringLiteral( "WW/CellPrevis" ), on );
}

int wwCellPrevisRed()
{
	return st().previsRed;
}

void wwCellCullShape( const void * nif, int block, std::vector<WwCullRun> && runs )
{
	if ( runs.size() < 2 )
		return;	// one run: nothing to cull inside the shape
	st().docs[nif].insert( block, std::move( runs ) );
}

const std::vector<WwCullRun> * wwCellCullRuns( const void * nif, int block )
{
	CullState & s = st();
	auto d = s.docs.constFind( nif );
	if ( d == s.docs.constEnd() )
		return nullptr;
	auto it = d->constFind( block );
	return it == d->constEnd() ? nullptr : &( *it );
}

bool wwCellCullOn()
{
	return st().on;
}

void wwCellCullSetOn( bool on )
{
	CullState & s = st();
	if ( s.pinned )
		return;
	s.on = on;
	QSettings().setValue( QStringLiteral( "WW/CellCull" ), on );
}

int wwCellCullRed()
{
	return st().red;
}

bool wwCellCullCamera( Scene * scene, const Shape * sh, int block, std::int64_t numTris,
	std::vector<std::uint32_t> & out )
{
	CullState & s = st();
	if ( !scene || !sh || !scene->renderer || numTris <= 0 || s.docs.isEmpty() || s.probing )
		return false;	// lane UMBRA1: the depth probe draws everything and counts nothing
	// lane UMBRA1: the tome's query, once a frame (also with the row off when dumping, for the gates)
	UmbraDoc * ud = nullptr;
	{
		auto it = s.umbra.find( scene->nifModel );
		if ( it != s.umbra.end() && it.value() ) {
			ud = it.value().get();
			if ( ( s.previsOn && s.umbraOn ) || !s.umbraDump.isEmpty() )
				umbraFrame( s, scene, *ud );
		}
	}
	const std::vector<WwCullRun> * runs = wwCellCullRuns( scene->nifModel, block );
	if ( !runs )
		return false;
	const bool umbra = ud && s.previsOn && s.umbraOn;	// lane UMBRA1: rides the Previs row
	const bool previs = s.previsOn && ( s.previs.contains( scene->nifModel ) || umbra );	// lane SUNCELL1
	if ( !s.on && !previs ) {	// counted all the same, so the off frame's draw calls stand beside the on frame's
		s.any = true;
		s.cur.shapes++;
		s.cur.runs += std::int64_t( runs->size() );
		s.cur.drawn += std::int64_t( runs->size() );
		s.cur.calls++;
		s.cur.tris += numTris;
		s.cur.trisAll += numTris;
		return false;
	}
	float pl[4][4];
	cameraPlanes( scene, pl );
	if ( previs )
		previsBuild( s, scene, pl );
	const Transform & vt = sh->viewTrans();
	std::int64_t drawnTris = 0, occluded = 0, roomCulled = 0, tomeDrawn = 0, tomeCulled = 0, ungoverned = 0;
	const std::vector<UmbraGov> * gov = umbra ? &umbraGovFor( *ud, sh, block, *runs ) : nullptr;
	const std::int64_t culled = collect( *runs, numTris, [&]( const WwCullRun & r ) {
		const Vector3 v = vt * r.center;
		const float rv = r.radius * std::fabs( vt.scale );
		if ( s.on && !insidePlanes( pl, v, rv ) )
			return false;
		if ( gov ) {	// lane UMBRA1: a run the tome governs is the tome's alone
			const int u = umbraRun( *ud, *gov, size_t( &r - runs->data() ) );
			if ( u == 1 ) {
				tomeDrawn++;
				return true;
			}
			if ( u == 2 ) {
				tomeCulled++;
				return false;
			}
			ungoverned++;
		}
		if ( previs ) {	// lane SUNCELL1: the occluders and the rooms
			const int h = previsHidden( s.pc, v, rv );
			occluded += h == 1 ? 1 : 0;
			roomCulled += h == 2 ? 1 : 0;
			return h == 0;
		}
		return true;
	}, out, drawnTris );
	s.any = true;
	s.cur.shapes++;
	s.cur.runs += std::int64_t( runs->size() );
	s.cur.culled += culled - occluded - roomCulled - tomeCulled;
	s.cur.occluded += occluded;
	s.cur.roomCulled += roomCulled;
	s.cur.tomeDrawn += tomeDrawn;	// lane UMBRA1
	s.cur.tomeCulled += tomeCulled;
	s.cur.ungoverned += ungoverned;
	s.cur.drawn += std::int64_t( runs->size() ) - culled;
	s.cur.calls += std::int64_t( out.size() / 2 );
	s.cur.tris += drawnTris;
	s.cur.trisAll += numTris;
	return true;
}

bool wwCellCullCaster( Scene * scene, const Shape * sh, int block, int cascade, const float * m,
	std::vector<std::uint32_t> & out )
{
	CullState & s = st();
	/* lane SUNCELL1: casters always cast -- the Previs row never reaches this pass. Its red control `casters`
	 * hides the casters the camera pass found occluded or roomed off (this frame's camera view, or none). */
	const bool previsRed = s.previsOn && s.previsRed == 1 && s.pc.valid && scene && s.pc.nif == scene->nifModel;
	/* lane UMBRA1: casters always cast -- the tome never reaches this pass either. Its red control `casters` drops
	 * the casters the camera pass's tome query hid (the last camera's answer). */
	UmbraDoc * ud = nullptr;
	if ( s.previsOn && s.umbraOn && s.umbraRed == 1 && scene && !s.probing ) {
		auto it = s.umbra.find( scene->nifModel );
		ud = it == s.umbra.end() ? nullptr : it.value().get();
	}
	const bool umbraRed = ud != nullptr;
	if ( ( !s.on && !previsRed && !umbraRed ) || !scene || !sh || !scene->renderer || !m )
		return false;
	const std::vector<WwCullRun> * runs = wwCellCullRuns( scene->nifModel, block );
	if ( !runs )
		return false;
	const std::vector<UmbraGov> * gov = umbraRed ? &umbraGovFor( *ud, sh, block, *runs ) : nullptr;
	auto tomeHides = [&]( const WwCullRun & r ) {	// lane UMBRA1: RED only
		if ( !gov || umbraRun( *ud, *gov, size_t( &r - runs->data() ) ) != 2 )
			return false;
		s.cur.tomeCasterHidden++;
		return true;
	};
	const Transform & vt = sh->viewTrans();
	std::int64_t drawnTris = 0, culled = 0;
	if ( !s.on ) {	// only the reds: the camera pass's view of each run
		std::int64_t hidden = 0;
		const Transform cam = scene->view * sh->worldTrans();
		culled = collect( *runs, std::int64_t( sh->triangles.size() ), [&]( const WwCullRun & r ) {
			const bool h = previsRed && previsHidden( s.pc, cam * r.center, r.radius * std::fabs( cam.scale ) ) != 0;
			hidden += h ? 1 : 0;
			return !h && !tomeHides( r );
		}, out, drawnTris );
		s.cur.casterHidden += hidden;
	} else if ( s.red == 1 ) {	// RED: the camera's frustum decides which casters the cascade gets
		float pl[4][4];
		cameraPlanes( scene, pl );
		culled = collect( *runs, std::int64_t( sh->triangles.size() ), [&]( const WwCullRun & r ) {
			return insidePlanes( pl, vt * r.center, r.radius * std::fabs( vt.scale ) );
		}, out, drawnTris );
	} else {
		// the cascade's own window: light-space x and y in [-1, 1] (no depth test: a caster toward the sun still casts)
		float lx = 0.0f, ly = 0.0f;
		for ( int c = 0; c < 3; c++ ) {
			lx += m[c * 4 + 0] * m[c * 4 + 0];
			ly += m[c * 4 + 1] * m[c * 4 + 1];
		}
		lx = std::sqrt( lx );
		ly = std::sqrt( ly );
		culled = collect( *runs, std::int64_t( sh->triangles.size() ), [&]( const WwCullRun & r ) {
			const Vector3 v = vt * r.center;
			const float rv = r.radius * std::fabs( vt.scale );
			const float x = m[0] * v[0] + m[4] * v[1] + m[8] * v[2] + m[12];
			const float y = m[1] * v[0] + m[5] * v[1] + m[9] * v[2] + m[13];
			if ( !( std::fabs( x ) <= 1.0f + rv * lx && std::fabs( y ) <= 1.0f + rv * ly ) )
				return false;
			if ( previsRed ) {	// lane SUNCELL1: RED only
				const Transform cam = scene->view * sh->worldTrans();
				if ( previsHidden( s.pc, cam * r.center, r.radius * std::fabs( cam.scale ) ) != 0 ) {
					s.cur.casterHidden++;
					return false;
				}
			}
			return !tomeHides( r );	// lane UMBRA1: RED only
		}, out, drawnTris );
	}
	if ( cascade >= 0 && cascade < 3 ) {
		s.any = true;
		s.cur.cDrawn[cascade] += std::int64_t( runs->size() ) - culled;
		s.cur.cCulled[cascade] += culled;
	}
	return true;
}

QString wwCellCullSummary()
{
	const Counts & c = st().last;
	return QStringLiteral( "cell cull: " ) + ( st().on ? QStringLiteral( "on" ) : QStringLiteral( "off" ) )
		+ QStringLiteral( " camera shapes=%1 runs=%2 drawn=%3 culled=%4 calls=%5 tris=%6/%7"
		" | casters c0 %8/%9 c1 %10/%11 c2 %12/%13 (drawn/culled)%14" )
		.arg( c.shapes ).arg( c.runs ).arg( c.drawn ).arg( c.culled ).arg( c.calls ).arg( c.tris ).arg( c.trisAll )
		.arg( c.cDrawn[0] ).arg( c.cCulled[0] ).arg( c.cDrawn[1] ).arg( c.cCulled[1] ).arg( c.cDrawn[2] )
		.arg( c.cCulled[2] ).arg( st().red == 1 ? QStringLiteral( " RED=casters" ) : QString() );
}

void wwCellCullFrame()
{
	CullState & s = st();
	s.frame++;	// lane SUNCELL1: the previs scene is rebuilt in the next camera pass
	if ( !s.previs.isEmpty() ) {
		const PrevisCache & pc = s.pc;
		const int occ = int( pc.occ.size() ), rooms = int( pc.rooms.size() );
		if ( s.any && ( !( s.cur == s.last ) || occ != s.lastOcc || rooms != s.lastRooms || pc.roomsSeen != s.lastSeen
			|| pc.portalsPassed != s.lastPortals || !s.printed ) ) {
			s.lastOcc = occ;
			s.lastRooms = rooms;
			s.lastSeen = pc.roomsSeen;
			s.lastPortals = pc.portalsPassed;
			std::fprintf( stderr, "cell previs cull: %s occluders=%d rooms=%d roomsSeen=%d portalsPassed=%d occluded=%lld "
				"roomCulled=%lld casterHidden=%lld%s\n", s.previsOn ? "on" : "off", occ, rooms, pc.roomsSeen,
				pc.portalsPassed, (long long)s.cur.occluded, (long long)s.cur.roomCulled, (long long)s.cur.casterHidden,
				s.previsRed == 1 ? " RED=casters" : "" );
		}
	}
	if ( s.umbraLast ) {	// lane UMBRA1: printed when it changes
		const UmbraDoc & d = *s.umbraLast;
		const QString line = QStringLiteral( "cell umbra: %1 blocks=%2 decoded=%3 queried=%4 noStart=%5 cells=%6 "
			"visible=%7/%8 governed=%9 (uid %10, combined %11) tomeDrawn=%12 tomeCulled=%13 ungoverned=%14 "
			"casterHidden=%15%16" )
			.arg( s.previsOn && s.umbraOn ? QStringLiteral( "on" ) : QStringLiteral( "off" ) ).arg( int( d.blocks.size() ) )
			.arg( d.decoded ).arg( d.queried ).arg( d.noStart ).arg( d.cells ).arg( d.visible ).arg( d.objects )
			.arg( d.govUid + d.govComb ).arg( d.govUid ).arg( d.govComb ).arg( qint64( s.cur.tomeDrawn ) )
			.arg( qint64( s.cur.tomeCulled ) ).arg( qint64( s.cur.ungoverned ) ).arg( qint64( s.cur.tomeCasterHidden ) )
			.arg( s.umbraRed == 1 ? QStringLiteral( " RED=casters" ) : QString() );
		if ( line != s.lastUmbraLine ) {
			s.lastUmbraLine = line;
			std::fprintf( stderr, "%s\n", line.toLocal8Bit().constData() );
		}
	}
	if ( !s.any ) {
		if ( !s.printed && !s.docs.isEmpty() ) {
			std::fprintf( stderr, "cell cull: %s\n", s.on ? "on, no runs drawn this frame" : "off, no runs drawn this frame" );
			s.printed = true;
		}
		return;
	}
	if ( !( s.cur == s.last ) || !s.printed ) {
		s.last = s.cur;
		std::fprintf( stderr, "%s\n", wwCellCullSummary().toLocal8Bit().constData() );
		s.printed = true;
	}
	s.cur = Counts();
	s.any = false;
}

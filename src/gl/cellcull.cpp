/*! Lane SUNCELL1: the cell view's per-placement culling (see cellcull.h). */
#include "cellcull.h"

#include "gl/glscene.h"
#include "gl/glshape.h"
#include "gl/renderer.h"

#include <QByteArray>
#include <QHash>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>

namespace
{

struct Counts
{
	std::int64_t shapes = 0, runs = 0, drawn = 0, culled = 0, calls = 0, tris = 0, trisAll = 0;
	std::int64_t occluded = 0, roomCulled = 0, casterHidden = 0;	// lane SUNCELL1: the Previs row's share
	std::int64_t cDrawn[3] = { 0, 0, 0 }, cCulled[3] = { 0, 0, 0 };
	bool operator==( const Counts & o ) const
	{
		for ( int i = 0; i < 3; i++ )
			if ( cDrawn[i] != o.cDrawn[i] || cCulled[i] != o.cCulled[i] )
				return false;
		return shapes == o.shapes && runs == o.runs && drawn == o.drawn && culled == o.culled && calls == o.calls
			&& tris == o.tris && trisAll == o.trisAll && occluded == o.occluded && roomCulled == o.roomCulled
			&& casterHidden == o.casterHidden;
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

} // namespace

void wwCellCullBegin( const void * nif )
{
	st().docs.remove( nif );
	st().previs.remove( nif );	// lane SUNCELL1
	st().pc = PrevisCache();
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
	if ( !scene || !sh || !scene->renderer || numTris <= 0 || s.docs.isEmpty() )
		return false;
	const std::vector<WwCullRun> * runs = wwCellCullRuns( scene->nifModel, block );
	if ( !runs )
		return false;
	const bool previs = s.previsOn && s.previs.contains( scene->nifModel );	// lane SUNCELL1
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
	std::int64_t drawnTris = 0, occluded = 0, roomCulled = 0;
	const std::int64_t culled = collect( *runs, numTris, [&]( const WwCullRun & r ) {
		const Vector3 v = vt * r.center;
		const float rv = r.radius * std::fabs( vt.scale );
		if ( s.on && !insidePlanes( pl, v, rv ) )
			return false;
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
	s.cur.culled += culled - occluded - roomCulled;
	s.cur.occluded += occluded;
	s.cur.roomCulled += roomCulled;
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
	if ( ( !s.on && !previsRed ) || !scene || !sh || !scene->renderer || !m )
		return false;
	const std::vector<WwCullRun> * runs = wwCellCullRuns( scene->nifModel, block );
	if ( !runs )
		return false;
	const Transform & vt = sh->viewTrans();
	std::int64_t drawnTris = 0, culled = 0;
	if ( !s.on ) {	// only the previs red: the camera pass's view of each run
		std::int64_t hidden = 0;
		const Transform cam = scene->view * sh->worldTrans();
		culled = collect( *runs, std::int64_t( sh->triangles.size() ), [&]( const WwCullRun & r ) {
			const bool h = previsHidden( s.pc, cam * r.center, r.radius * std::fabs( cam.scale ) ) != 0;
			hidden += h ? 1 : 0;
			return !h;
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
			return true;
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

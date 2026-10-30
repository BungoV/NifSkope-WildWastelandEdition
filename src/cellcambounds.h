/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLCAMBOUNDS_H
#define CELLCAMBOUNDS_H

/* lane CAMGATE1 (bungo 10-29: "No out of bounds screenshots are allowed"; 10-30, watching a live hold window that showed
 * the underside of the cells: "Stop placing the camera out of bounds anywhere").
 *
 * THE RULE, in one place. An exterior cell document's camera eye must stand
 *   - inside the loaded n x n block (a streamed document: in any cell of the worldspace that has LAND), and
 *   - at least kClear (50) units above the LAND under it (bilinear on the cell's 33 x 33 VHGT grid, 128 units apart).
 * A view that breaks it is REFUSED where every reader takes the view (GLView::viewTransform): the window, the shots, the
 * path frames and the picks keep the last in-bounds pose, or, before there is one (a fresh document, the harness's
 * overview refit), the cell's home: the open's spawn (src/cellspawn.h), else the centre cell's middle at ground + eye
 * height. The notes say `camera refused: <why>` once per refusal (kind + cell).
 * Red WW_CAMERA_BOUNDS_RED=allow: the old behaviour (every view drawn as asked).
 * The offline twin of judge() is notes/camgate1/tools/camgate2.py check_eye (the runner's gate); tests/cambounds is its
 * unit test (green + one red per rule).
 *
 * lane CAMGATE2 (FIX1030B, 10-30):
 *   - THE CAMERA SNAPS BACK. A refused view used to be frozen on screen while the free camera's own state (Pos, Rot,
 *     Dist: the fly keys, the readouts, the favorites) went on under the ground: the user flew on blind. Now a refusal
 *     of a view that came from that state writes the kept pose back into it (Keeper::pass below), so the next input
 *     starts from the kept pose and the readouts say where the view is. A path camera or a pinned harness camera is
 *     never written (the runner judges those itself from the side file). Red WW_CAMERA_BOUNDS_RED=freeze: CAMGATE1's
 *     frozen view, the state left where it was refused.
 *   - INTERIORS are judged too: no LAND, so the bounds are the box of the cell's placements (every REFR/ACHR not
 *     deleted) grown by kRoomMargin, the open's spawn (COC marker / door arrival) always inside. Red
 *     WW_CAMERA_BOUNDS_RED=interior: interiors not judged (CAMGATE1). The reds take a comma list.
 *
 * The pure part (namespace WwCamBounds) has no Qt and no NifModel, so the unit test includes it alone. */

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <vector>

namespace WwCamBounds
{
constexpr double kClear = 50.0;	//!< the eye's least height above the LAND under it (the brief's ground + 50)
constexpr double kCellUnits = 4096.0;
constexpr int kGrid = 33;	//!< VHGT samples a side, 128 units apart
constexpr double kEyeHeight = 120.4828;	//!< src/cellspawn.h cellEyeHeight (the home pose's eye above its ground)

enum class Verdict { Ok = 0, Outside, NoLand, Under, OutsideRoom };

inline const char * verdictName( Verdict v )
{
	switch ( v ) {
	case Verdict::Ok: return "ok";
	case Verdict::Outside: return "outside the loaded cells";
	case Verdict::NoLand: return "no LAND under the eye";
	case Verdict::Under: return "under the terrain";
	case Verdict::OutsideRoom: return "outside the interior's placements";
	}
	return "?";
}

//! the loaded block: (2 half + 1)^2 cells round (cx, cy); roam = a streamed document (the block follows the eye)
struct Box
{
	int cx = 0, cy = 0, half = 0;
	bool roam = false;
};

inline int cellOf( double v )
{
	return int( std::floor( v / kCellUnits ) );
}

//! bilinear height of one cell's grid H[row = y][col = x] at the world point (x, y) inside cell (cx, cy)
inline double groundIn( const float ( *H )[kGrid], int cx, int cy, double x, double y )
{
	const double fx = ( x - cx * kCellUnits ) / ( kCellUnits / ( kGrid - 1 ) );
	const double fy = ( y - cy * kCellUnits ) / ( kCellUnits / ( kGrid - 1 ) );
	int i = int( fx ), j = int( fy );
	i = i < 0 ? 0 : i > kGrid - 2 ? kGrid - 2 : i;
	j = j < 0 ? 0 : j > kGrid - 2 ? kGrid - 2 : j;
	const double ax = fx - i, ay = fy - j;
	return ( 1 - ax ) * ( 1 - ay ) * H[j][i] + ax * ( 1 - ay ) * H[j][i + 1] + ( 1 - ax ) * ay * H[j + 1][i]
		+ ax * ay * H[j + 1][i + 1];
}

/*! The verdict for an eye at (x, y, z). `land( cx, cy )` returns that cell's 33 x 33 heights or nullptr (no LAND).
 *  `ground` gets the LAND height under the eye when the eye's cell has one. */
template<class LandFn>
Verdict judge( const Box & b, LandFn && land, double x, double y, double z, double * ground = nullptr )
{
	if ( !std::isfinite( x ) || !std::isfinite( y ) || !std::isfinite( z ) )
		return Verdict::Outside;
	const int cx = cellOf( x ), cy = cellOf( y );
#ifndef CAMBOUNDS_RED_OUTSIDE
	if ( !b.roam && ( std::abs( cx - b.cx ) > b.half || std::abs( cy - b.cy ) > b.half ) )
		return Verdict::Outside;
#endif
	const float ( *H )[kGrid] = land( cx, cy );
	if ( !H )
		return Verdict::NoLand;
	const double g = groundIn( H, cx, cy, x, y );
	if ( ground )
		*ground = g;
#ifndef CAMBOUNDS_RED_UNDER
	if ( z < g + kClear )
		return Verdict::Under;
#endif
	return Verdict::Ok;
}

/* lane CAMGATE2: an interior's bounds. kRoomMargin past the outermost placements: a wall piece's origin sits in its
 * wall, so the room's inside is in the box with room to spare; every interior path the lanes shot (16, CAMGATE1's
 * census) lies inside its own cell's box + 512 (notes/fix1030b/camgate2/interior_paths.out). */
constexpr double kRoomMargin = 512.0;

struct Room
{
	double lo[3] = { 0, 0, 0 }, hi[3] = { 0, 0, 0 };
	bool valid = false;

	void add( double x, double y, double z )
	{
		if ( !std::isfinite( x ) || !std::isfinite( y ) || !std::isfinite( z ) )
			return;
		const double p[3] = { x, y, z };
		for ( int k = 0; k < 3; k++ ) {
			lo[k] = valid ? std::fmin( lo[k], p[k] ) : p[k];
			hi[k] = valid ? std::fmax( hi[k], p[k] ) : p[k];
		}
		valid = true;
	}
	void grow( double m )
	{
		for ( int k = 0; valid && k < 3; k++ ) {
			lo[k] -= m;
			hi[k] += m;
		}
	}
	bool holds( double x, double y, double z ) const
	{
		return valid && x >= lo[0] && x <= hi[0] && y >= lo[1] && y <= hi[1] && z >= lo[2] && z <= hi[2];
	}
	//! the home pose is always in the room: a spawn outside the grown box widens it to the spawn + the margin
	void include( double x, double y, double z, double m )
	{
		if ( !valid || holds( x, y, z ) || !std::isfinite( x ) || !std::isfinite( y ) || !std::isfinite( z ) )
			return;
		const double p[3] = { x, y, z };
		for ( int k = 0; k < 3; k++ ) {
			lo[k] = std::fmin( lo[k], p[k] - m );
			hi[k] = std::fmax( hi[k], p[k] + m );
		}
	}
};

//! the verdict for an eye in an interior: outside the room = refused (a room with no placements judges nothing)
inline Verdict judgeRoom( const Room & r, double x, double y, double z )
{
	if ( !std::isfinite( x ) || !std::isfinite( y ) || !std::isfinite( z ) )
		return Verdict::OutsideRoom;
#ifndef CAMBOUNDS_RED_INTERIOR
	if ( r.valid && !r.holds( x, y, z ) )
		return Verdict::OutsideRoom;
#endif
	return Verdict::Ok;
}

/* lane CAMGATE3 (FIX1030B round 2): AN INTERIOR IS A SET OF BOXES, built from what is drawn. CAMGATE2's one box of the
 * placements' ORIGINS was loose (outlier refs: 85 Fallout4.esm cells, CambridgePolymerLabs01 34.6 km across) and
 * also, measured, NOT CONSERVATIVE: a big mesh reaches past its origin + 512, so its navmesh (43 cells, worst 5312 u)
 * and the game's own RoomMarker boxes (24 cells, Vault111Cryo 3168 u above) lay outside it: views inside the real
 * interior were refused. The rule now (notes/fix1030b/camgate3, room_census.py is its offline twin):
 *   - clumps of placement origins: origins in 26-touching kClumpVoxel voxels are one clump;
 *   - one box per clump = its origins + each placement's OBND world box (fromEuler(-rot), as the cell view places it;
 *     sky / effects cards left out by the caller), + kRoomMargin;
 *   - one box per RoomMarker (XPRM box) and PortalMarker (XPRM plane), oriented as cellview.cpp cellPrimAxes, + margin;
 *   - the open's spawn (COC marker / door arrival) always inside (RoomSet::include).
 * An eye is in bounds when it is in ANY box: the void between a building and its outlier clumps is refused, the edge of
 * every mesh is not. Census over all 1276 interiors of Fallout4.esm + DLCNukaWorld.esm: every navmesh vertex within 512
 * of any placement, every room / portal corner, every COC marker and door arrival, and the 16 interior paths the lanes
 * shot are inside; the set is under half today's volume in 45 cells. Red WW_CAMERA_BOUNDS_RED=loosebox (CAMGATE2's box);
 * unit red CAMBOUNDS_RED_LOOSEBOX. */
constexpr double kClumpVoxel = 1024.0;

//! niftypes.cpp Matrix::fromEuler( x, y, z ) (= Rx( x ) Ry( y ) Rz( z )); a ref is placed with fromEuler( -rot )
inline void eulerMatrix( double x, double y, double z, double m[3][3] )
{
	const double sx = std::sin( x ), cx = std::cos( x ), sy = std::sin( y ), cy = std::cos( y ), sz = std::sin( z ),
		cz = std::cos( z );
	m[0][0] = cy * cz;
	m[0][1] = -cy * sz;
	m[0][2] = sy;
	m[1][0] = sx * sy * cz + sz * cx;
	m[1][1] = cx * cz - sx * sy * sz;
	m[1][2] = -sx * cy;
	m[2][0] = sx * sz - cx * sy * cz;
	m[2][1] = cx * sy * sz + sx * cz;
	m[2][2] = cx * cy;
}

//! cellview.cpp cellPrimAxes: Rz( -rz ) Rx( -rx ) Ry( -ry ); column k = the marker's local axis k in world
inline void primMatrix( double rx, double ry, double rz, double m[3][3] )
{
	auto rot = []( int a, double ang, double o[3][3] ) {
		const double c = std::cos( ang ), s = std::sin( ang );
		const int i = ( a + 1 ) % 3, j = ( a + 2 ) % 3;
		for ( int u = 0; u < 3; u++ )
			for ( int v = 0; v < 3; v++ )
				o[u][v] = u == v ? 1.0 : 0.0;
		o[i][i] = c;
		o[i][j] = -s;
		o[j][i] = s;
		o[j][j] = c;
	};
	auto mul = []( const double a[3][3], const double b[3][3], double o[3][3] ) {
		for ( int u = 0; u < 3; u++ )
			for ( int v = 0; v < 3; v++ )
				o[u][v] = a[u][0] * b[0][v] + a[u][1] * b[1][v] + a[u][2] * b[2][v];
	};
	double z[3][3], x[3][3], y[3][3], t[3][3];
	rot( 2, -rz, z );
	rot( 0, -rx, x );
	rot( 1, -ry, y );
	mul( z, x, t );
	mul( t, y, m );
}

//! the world AABB of the box c + m * ( +-half ): half along world axis i = sum_k |m[i][k]| half[k]
inline Room orientedBox( const double c[3], const double m[3][3], const double half[3] )
{
	Room r;
	for ( int i = 0; i < 3; i++ ) {
		const double e = std::fabs( m[i][0] ) * half[0] + std::fabs( m[i][1] ) * half[1] + std::fabs( m[i][2] ) * half[2];
		r.lo[i] = c[i] - e;
		r.hi[i] = c[i] + e;
	}
	r.valid = std::isfinite( r.lo[0] + r.lo[1] + r.lo[2] + r.hi[0] + r.hi[1] + r.hi[2] );
	return r;
}

//! a placement's world box from its base's OBND (base-local lo / hi): pos + fromEuler( -rot ) ( v * scale )
inline Room obndWorldBox( const double pos[3], const double rot[3], double scale, const double olo[3], const double ohi[3] )
{
	const double s = scale > 0.0 ? scale : 1.0;
	double m[3][3];
	eulerMatrix( -rot[0], -rot[1], -rot[2], m );
	double c[3], h[3], cw[3];
	for ( int k = 0; k < 3; k++ ) {
		c[k] = 0.5 * ( olo[k] + ohi[k] ) * s;
		h[k] = 0.5 * std::fabs( ohi[k] - olo[k] ) * s;
	}
	for ( int i = 0; i < 3; i++ )
		cw[i] = pos[i] + m[i][0] * c[0] + m[i][1] * c[1] + m[i][2] * c[2];
	return orientedBox( cw, m, h );
}

//! a RoomMarker / PortalMarker's box: XPRM half extents |h| * scale (at least 1), axes as cellview.cpp cellPrimAxes
inline Room markerBox( const double pos[3], const double rot[3], double scale, const double half[3] )
{
	double m[3][3], h[3];
	primMatrix( rot[0], rot[1], rot[2], m );
	for ( int k = 0; k < 3; k++ )
		h[k] = std::fmax( std::fabs( half[k] ) * scale, 1.0 );
	return orientedBox( pos, m, h );
}

//! one placement of an interior: its origin and, when its base has an OBND (not a sky / effects card), its world box
struct Placed
{
	double pos[3] = { 0, 0, 0 };
	Room box;	//!< invalid = origin only
};

/*! the clumps of the origins (26-touching kClumpVoxel voxels), each a list of indices into P, ordered by first member.
 *  Union-find over the occupied voxels (std::map: no hashing of negative keys to get wrong). */
inline std::vector<std::vector<int>> clumpsOf( const std::vector<Placed> & P )
{
	std::map<std::array<long long, 3>, int> vox;
	std::vector<std::array<long long, 3>> keys;
	std::vector<int> of( P.size(), -1 );
	for ( size_t i = 0; i < P.size(); i++ ) {
		std::array<long long, 3> k;
		for ( int c = 0; c < 3; c++ )
			k[c] = (long long) std::floor( P[i].pos[c] / kClumpVoxel );
		auto it = vox.find( k );
		if ( it == vox.end() ) {
			it = vox.emplace( k, int( keys.size() ) ).first;
			keys.push_back( k );
		}
		of[i] = it->second;
	}
	std::vector<int> par( keys.size() );
	for ( size_t i = 0; i < par.size(); i++ )
		par[i] = int( i );
	auto find = [&par]( int a ) {
		while ( par[a] != a ) {
			par[a] = par[par[a]];
			a = par[a];
		}
		return a;
	};
	for ( size_t a = 0; a < keys.size(); a++ )
		for ( int dx = -1; dx <= 1; dx++ )
			for ( int dy = -1; dy <= 1; dy++ )
				for ( int dz = -1; dz <= 1; dz++ ) {
					auto it = vox.find( { keys[a][0] + dx, keys[a][1] + dy, keys[a][2] + dz } );
					if ( it == vox.end() )
						continue;
					const int ra = find( int( a ) ), rb = find( it->second );
					if ( ra != rb )
						par[std::max( ra, rb )] = std::min( ra, rb );
				}
	std::map<int, int> slot;	// root -> index in out, in order of each clump's first member
	std::vector<std::vector<int>> out;
	for ( size_t i = 0; i < P.size(); i++ ) {
		if ( of[i] < 0 || !std::isfinite( P[i].pos[0] + P[i].pos[1] + P[i].pos[2] ) )
			continue;
		const int r = find( of[i] );
		auto it = slot.find( r );
		if ( it == slot.end() ) {
			it = slot.emplace( r, int( out.size() ) ).first;
			out.emplace_back();
		}
		out[it->second].push_back( int( i ) );
	}
	return out;
}

//! an interior's bounds: in bounds = inside any box (lane CAMGATE3)
struct RoomSet
{
	std::vector<Room> boxes;
	int clumps = 0;
	int mainBox = -1;	//!< the box of the biggest clump (the home when there is no spawn)

	bool valid() const { return !boxes.empty(); }
	bool holds( double x, double y, double z ) const
	{
		for ( const Room & r : boxes )
			if ( r.holds( x, y, z ) )
				return true;
		return false;
	}
	//! the spawn is always in bounds: one more box round it when no box holds it
	void include( double x, double y, double z, double m )
	{
		if ( !valid() || holds( x, y, z ) || !std::isfinite( x ) || !std::isfinite( y ) || !std::isfinite( z ) )
			return;
		Room r;
		r.add( x, y, z );
		r.grow( m );
		boxes.push_back( r );
	}
	//! the union's AABB (for the notes)
	Room bound() const
	{
		Room u;
		for ( const Room & r : boxes ) {
			u.add( r.lo[0], r.lo[1], r.lo[2] );
			u.add( r.hi[0], r.hi[1], r.hi[2] );
		}
		return u;
	}
};

/*! the set: one box per clump (origins + their placements' boxes) + one per marker box, each + kRoomMargin.
 *  Red CAMBOUNDS_RED_LOOSEBOX: CAMGATE2's one box of every origin + the margin (markers and placement boxes ignored). */
inline RoomSet roomSetOf( const std::vector<Placed> & P, const std::vector<Room> & markers )
{
	RoomSet s;
#ifdef CAMBOUNDS_RED_LOOSEBOX
	Room all;
	for ( const Placed & p : P )
		all.add( p.pos[0], p.pos[1], p.pos[2] );
	all.grow( kRoomMargin );
	if ( all.valid ) {
		s.boxes.push_back( all );
		s.clumps = 1;
		s.mainBox = 0;
	}
	(void) markers;
	return s;
#else
	const std::vector<std::vector<int>> C = clumpsOf( P );
	s.clumps = int( C.size() );
	size_t most = 0;
	for ( const std::vector<int> & members : C ) {
		Room b;
		for ( const int i : members ) {
			b.add( P[i].pos[0], P[i].pos[1], P[i].pos[2] );
			if ( P[i].box.valid ) {
				b.add( P[i].box.lo[0], P[i].box.lo[1], P[i].box.lo[2] );
				b.add( P[i].box.hi[0], P[i].box.hi[1], P[i].box.hi[2] );
			}
		}
		if ( !b.valid )
			continue;
		b.grow( kRoomMargin );
		if ( members.size() > most ) {
			most = members.size();
			s.mainBox = int( s.boxes.size() );
		}
		s.boxes.push_back( b );
	}
	for ( Room r : markers ) {
		if ( !r.valid )
			continue;
		r.grow( kRoomMargin );
		s.boxes.push_back( r );
	}
	return s;
#endif
}

//! the verdict for an eye in an interior's set (lane CAMGATE3): outside every box = refused (an empty set judges nothing)
inline Verdict judgeRoomSet( const RoomSet & s, double x, double y, double z )
{
	if ( !std::isfinite( x ) || !std::isfinite( y ) || !std::isfinite( z ) )
		return Verdict::OutsideRoom;
#ifndef CAMBOUNDS_RED_INTERIOR
	if ( s.valid() && !s.holds( x, y, z ) )
		return Verdict::OutsideRoom;
#endif
	return Verdict::Ok;
}

//! what a refused view became (the notes' words are the caller's)
enum class Kept { None = 0, Last, Home, LastPut, HomePut };

/*! lane CAMGATE2: the gate's memo and the write-back, generic over the view (V), the free camera's state (S) and the
 *  string (Str), so tests/cambounds drives the very code GLView::wwCameraBoundsGate runs. Ops:
 *    bool same( const V &, const V & )             bit-equal views
 *    Verdict judge( const V &, Str * why, Str * key )
 *    S state()                                     the free camera's state now
 *    void put( const S & )                         the write-back (plain member writes: no signal, no repaint call)
 *    V rawNow()                                    the view the state gives now (after a put)
 *    bool home( V & view, S & state, Str & words ) the cell's home pose as a view and as a state
 *    void say( const Str & why, Kept, const Str & homeWords )   once per refusal (kind + cell) */
template<class V, class S, class Str>
struct Keeper
{
	bool askedValid = false, haveGood = false, haveGoodState = false, backValid = false, askedPut = false;
	V asked{}, given{}, good{}, backTo{};
	S goodState{}, putState{};
	Str why, saidKey;

	void reset() { *this = Keeper(); }

	/*! raw: the view asked for. stateDriven: it came from the free camera's own state (not a path, not a pinned
	 *  harness camera, Z up), so a refusal may write the state back. writeBack: the fix (false = red freeze). */
	template<class Ops>
	V pass( const V & raw, bool stateDriven, bool writeBack, Ops & ops )
	{
#ifdef CAMBOUNDS_RED_FREEZE
		writeBack = false;
#endif
		if ( askedValid && ops.same( raw, asked ) ) {
			// the same refused pose asked again: the input moved the state back onto it, so put it back again
			if ( askedPut && writeBack && stateDriven )
				ops.put( putState );
			return given;
		}
		askedPut = false;
		Str w, key;
		V out = raw;
		if ( ops.judge( raw, &w, &key ) == Verdict::Ok ) {
			const bool echo = backValid && ops.same( raw, backTo );	// the pose the gate put back: not a new one
			good = raw;
			haveGood = true;
			if ( stateDriven ) {
				goodState = ops.state();
				haveGoodState = true;
			}
			why = Str();
			if ( !echo ) {
				saidKey = Str();	// the next refusal is told again
				backValid = false;
			}
		} else {
			Kept kept = Kept::None;
			Str homeWords;
			if ( writeBack && stateDriven ) {
				V hv{};
				S hs{};
				if ( haveGoodState ) {
					putState = goodState;
					kept = Kept::LastPut;
				} else if ( ops.home( hv, hs, homeWords ) ) {
					putState = hs;
					kept = Kept::HomePut;
				}
				if ( kept != Kept::None ) {
					ops.put( putState );
					const V back = ops.rawNow();
					Str w2, k2;
					if ( ops.judge( back, &w2, &k2 ) == Verdict::Ok ) {
						out = back;
						backTo = back;
						backValid = true;
						askedPut = true;
					} else {
						kept = Kept::None;	// the put-back pose is refused as well (another view mode): frozen below
					}
				}
			}
			if ( kept == Kept::None ) {
				V hv{};
				S hs{};
				if ( haveGood ) {
					out = good;
					kept = Kept::Last;
				} else if ( ops.home( hv, hs, homeWords ) ) {
					out = hv;
					kept = Kept::Home;
				}
			}
			why = w;
			if ( key != saidKey ) {
				saidKey = key;
				ops.say( w, kept, homeWords );
			}
		}
		asked = raw;
		given = out;
		askedValid = true;
		return out;
	}
};
} // namespace WwCamBounds

#ifndef CAMBOUNDS_PURE
#include <QString>
#include <memory>

class NifModel;
class EsmWorld;

/*! the open of an exterior or interior cell document (cellview.cpp liveKeep): its block and world. An interior's room
 *  (lane CAMGATE2) is read here from world->interiorRefrs(); the notes say `camera bounds: interior ...` once. */
void cellCamBoundsKeep( const NifModel * nif, const std::shared_ptr<EsmWorld> & world, int cx, int cy, int n,
	bool interior, bool streamed );
void cellCamBoundsForget( const NifModel * nif );
//! the open's spawn (cellview.cpp, beside cellSpawnPublish): the home pose a refused first view takes
void cellCamBoundsHome( const NifModel * nif, bool valid, const float eye[3], float yawDeg );
//! the rule applies to this document (an exterior cell document, or an interior with placements; reds not set)
bool cellCamBoundsActive( const NifModel * nif );
//! changes with every open of the document (the last in-bounds pose of the old one is not kept)
quint64 cellCamBoundsGen( const NifModel * nif );
/*! the verdict for an eye; `why` = the words (eye, cell, ground), `key` = what the once-a-refusal line is keyed on
 *  (the kind and the cell), so a flight under one cell logs once */
WwCamBounds::Verdict cellCamBoundsJudge( const NifModel * nif, double x, double y, double z, QString * why,
	QString * key = nullptr );
//! the home pose: the spawn, else the centre cell's middle at its ground + eye height (false: none)
bool cellCamBoundsHomeOf( const NifModel * nif, float eye[3], float & yawDeg );
//! WW_CAMERA_BOUNDS_RED=allow (live)
bool cellCamBoundsRed();
//! lane CAMGATE2: one word of the WW_CAMERA_BOUNDS_RED comma list (allow, freeze, interior; CAMGATE3: loosebox), live
bool cellCamBoundsRedHas( const char * word );
#endif

#endif // CELLCAMBOUNDS_H

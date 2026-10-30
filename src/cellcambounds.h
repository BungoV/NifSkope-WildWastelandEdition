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

#include <cmath>

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
//! lane CAMGATE2: one word of the WW_CAMERA_BOUNDS_RED comma list (allow, freeze, interior), live
bool cellCamBoundsRedHas( const char * word );
#endif

#endif // CELLCAMBOUNDS_H

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
 * height. The notes say `camera refused: <why>` once per refusal (kind + cell). Interiors have no LAND: not judged.
 * Red WW_CAMERA_BOUNDS_RED=allow: the old behaviour (every view drawn as asked).
 * The offline twin of judge() is notes/camgate1/tools/camgate2.py check_eye (the runner's gate); tests/cambounds is its
 * unit test (green + one red per rule).
 *
 * The pure part (namespace WwCamBounds) has no Qt and no NifModel, so the unit test includes it alone. */

#include <cmath>

namespace WwCamBounds
{
constexpr double kClear = 50.0;	//!< the eye's least height above the LAND under it (the brief's ground + 50)
constexpr double kCellUnits = 4096.0;
constexpr int kGrid = 33;	//!< VHGT samples a side, 128 units apart
constexpr double kEyeHeight = 120.4828;	//!< src/cellspawn.h cellEyeHeight (the home pose's eye above its ground)

enum class Verdict { Ok = 0, Outside, NoLand, Under };

inline const char * verdictName( Verdict v )
{
	switch ( v ) {
	case Verdict::Ok: return "ok";
	case Verdict::Outside: return "outside the loaded cells";
	case Verdict::NoLand: return "no LAND under the eye";
	case Verdict::Under: return "under the terrain";
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
} // namespace WwCamBounds

#ifndef CAMBOUNDS_PURE
#include <QString>
#include <memory>

class NifModel;
class EsmWorld;

//! the open of an exterior or interior cell document (cellview.cpp liveKeep): its block and world
void cellCamBoundsKeep( const NifModel * nif, const std::shared_ptr<EsmWorld> & world, int cx, int cy, int n,
	bool interior, bool streamed );
void cellCamBoundsForget( const NifModel * nif );
//! the open's spawn (cellview.cpp, beside cellSpawnPublish): the home pose a refused first view takes
void cellCamBoundsHome( const NifModel * nif, bool valid, const float eye[3], float yawDeg );
//! the rule applies to this document (an exterior cell document, red not set)
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
#endif

#endif // CELLCAMBOUNDS_H

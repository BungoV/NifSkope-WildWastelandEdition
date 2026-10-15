#ifndef CELLCABLE_H
#define CELLCABLE_H

/* lane CABLES1: CABLES in the cell view (the power lines, Goodneighbor's string-light wires, ropes).
 *
 * A Fallout 4 cable is a placed reference whose base is a BNDS (bendable spline) record. The base has no
 * model: the game builds the tube itself from the reference's XBSD (slack, thickness, half extents, the
 * detached-end byte) and the base's DNAM (tiles, slices, default colour, wind). This builds the same tube,
 * measured against the game, in the reference's own frame; the cell view places it like a static.
 *
 * THE SHAPE (measured, 2026-10-16):
 *   ends      A = -half, B = +half, in the reference's frame (its position is the cable's middle)
 *   sag       M = (A + B) / 2, lowered by |B - A| * 1.4 * slack (1.4 is the game's sag setting); no sag when
 *             A and B share x and y (a vertical cable)
 *   curve     the quadratic Bezier A, M, B, sampled at 48 points; its lowest point is 0.7 * L * slack under
 *             the chord's middle
 *   rings     one every 4 % of the curve's length (at least 8 segments), at equal distance along it; the last
 *             ring is B. Each ring has `slices` sides (3 or 4) plus a seam copy; normals are radial
 *   radius    thickness / 2
 *   uv        u = ring / segments * tiles, v = side / slices. With the base's per-length flag, tiles = the
 *             base's tiles per unit * 2 * the parabola's length summed in 0.1-unit steps over the chord
 *   colour    the base's default colour when it has no texture set, white when it has one
 *   wind      a base with wind sensibility > 0 sways (the game draws it with its grass shader). The vertex
 *             alpha is the weight: attached at both ends, the curve point's distance from the chord / the
 *             chord's length; detached, its share along the chord (0 at A, 1 at B). Renormalised to
 *             (w - min) / (max - min) * slack * sensibility / thickness, clamped to [0, 1]. No wind: alpha 1.
 *
 * WW_CELL_CABLES_RED=off is the red control: no cable is built (the references fall back to "no model"). */

#include <QString>
#include <vector>

struct EsmBendable;
struct NativeSrcShape;

//! One placed cable, as its reference stores it (XBSD).
struct WwCableSpec
{
	float slack = 0.0f;
	float thickness = 0.0f;
	float half[3] = { 0, 0, 0 };
	bool detached = false;
};

//! What one build measured: the curve's ring centres (reference frame) and the mesh's size.
struct WwCableBuilt
{
	std::vector<float> centres;   //!< 3 per ring, A first, B last
	int rings = 0;
	int slices = 0;
	float length = 0.0f;          //!< the curve's length (the 48-point polyline)
	float tiles = 0.0f;
	bool wind = false;            //!< the base sways (wind sensibility > 0)
	float sagExpected = 0.0f;     //!< 0.7 * L * slack (0 for a vertical cable)
	float sagMeasured = 0.0f;     //!< the chord middle's z minus the lowest ring centre's z, measured on the rings
};

//! Cables on (false under WW_CELL_CABLES_RED=off).
bool wwCellCablesOn();

/*! Builds the cable's tube into `out` (geometry in the reference's frame, the texture set as tex0 / tex1, the
 *  vertex colour stream filled). False when the base is not a BNDS or the spec has no length. */
bool wwCellCableShape( const EsmBendable & look, const WwCableSpec & spec, NativeSrcShape * out,
	WwCableBuilt * built = nullptr );

#endif

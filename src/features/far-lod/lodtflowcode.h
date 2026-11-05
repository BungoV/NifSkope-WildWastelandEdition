#ifndef LODTFLOWCODE_H
#define LODTFLOWCODE_H

/* lane FARLOOK1101 (FARWATER5 L3): the speed nibble of a .lodl flow word (bits 8..11) and the encoding id the header
 * carries at 0xCC. Pure (no Qt): tests/features/far-lod/farwater5 compiles it alone.
 *   0 (every bake before this): linear, code = round(8 r), r = speed / the body's reference, 15 = 1.94 x. A lodgen bake
 *     saturated 38953 of 138499 solved texels there (FARWATER4).
 *   1 (WW_LODL_FLOW_LOG=1): half-octave log, code = round(8 + 2 log2 r), 1 = 0.088 x .. 8 = 1 x .. 15 = 11.3 x;
 *     0 stays still water.
 *   2 (WW_LODL_FLOW_LOG=2, lane FARWATER7): three-quarter-octave log, code = round(8 + log2 r / 0.75), 1 = 0.026 x ..
 *     8 = 1 x .. 15 = 38.1 x. Encoding 1 clipped 4.48 % of the moving river texels above 13.45 x (p99 decode error
 *     1.85 octaves); 0.75 is the narrowest step whose clipped share is under 1 % (0.76 %, p99 0.374 = its own s / 2;
 *     notes/farwater7 scan.py over the Commonwealth's 111 river dumps). The low end is unused (0.00 % below 0.026 x).
 * Constant words write code 8, which is 1 x the reference in all three.
 * lane FARWATER6 (FARWATER5 L4): the reference is lodtFlowSpeedReference() for both writers (lodgen's river bake and
 * the marking tool's solve). */

#include <algorithm>
#include <cmath>
#include <vector>

constexpr unsigned kLodlFlowEncLinear = 0u;
constexpr unsigned kLodlFlowEncLog = 1u;
constexpr unsigned kLodlFlowEncLog34 = 2u;	//!< lane FARWATER7: 0.75 octave a code

//! the encodings a reader knows (an unknown one reads as linear, the old reader)
inline bool lodtFlowEncodingKnown( unsigned enc )
{
	return enc == kLodlFlowEncLinear || enc == kLodlFlowEncLog || enc == kLodlFlowEncLog34;
}

//! octaves a code of a log encoding (0 for linear)
inline double lodtFlowLogStep( unsigned enc )
{
	return enc == kLodlFlowEncLog ? 0.5 : enc == kLodlFlowEncLog34 ? 0.75 : 0.0;
}

//! speed / reference for a stored code under an encoding (an unknown encoding reads as linear, the old reader)
inline double lodtFlowSpeedFactor( unsigned enc, int code )
{
	if ( code <= 0 )
		return 0.0;
	if ( enc == kLodlFlowEncLog || enc == kLodlFlowEncLog34 )
		return std::exp2( double( code - 8 ) * lodtFlowLogStep( enc ) );
	return double( code ) / 8.0;
}

//! the unrounded, unclipped code of r > 0 (past 15.5 the nibble cannot carry it: lodgen's census counts those)
inline double lodtFlowSpeedCodeExact( unsigned enc, double r )
{
	if ( enc == kLodlFlowEncLog )
		return 8.0 + 2.0 * std::log2( r );
	if ( enc == kLodlFlowEncLog34 )
		return 8.0 + std::log2( r ) * ( 4.0 / 3.0 );
	return 8.0 * r;
}

//! the code for speed / reference r (r <= 0 = 0), before the moving-water floor (watermark.h waterSpeedFloor)
inline int lodtFlowSpeedCode( unsigned enc, double r )
{
	if ( !( r > 0.0 ) )
		return 0;
	const double c = std::floor( lodtFlowSpeedCodeExact( enc, r ) + 0.5 );
	return c < 0.0 ? 0 : c > 15.0 ? 15 : int( c );
}

/*! lane FARWATER6 (FARWATER5 L4): THE speed reference, code 8: the upper median of the moving speeds (> 0) of one
 *  solved piece; 0 when none moves. lodgen's river bake always used it; the marking tool used the body's mean, so a
 *  body re-marked in the tool decoded at mean / median times the baked speed (red WW_WATER_SPEED_REF_RED=mean). */
inline double lodtFlowSpeedReference( std::vector<double> sp )
{
	sp.erase( std::remove_if( sp.begin(), sp.end(), []( double s ) { return !( s > 0.0 ); } ), sp.end() );
	if ( sp.empty() )
		return 0.0;
	std::nth_element( sp.begin(), sp.begin() + std::ptrdiff_t( sp.size() / 2 ), sp.end() );
	return sp[sp.size() / 2];
}

#endif

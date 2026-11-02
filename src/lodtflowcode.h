#ifndef LODTFLOWCODE_H
#define LODTFLOWCODE_H

/* lane FARLOOK1101 (FARWATER5 L3): the speed nibble of a .lodl flow word (bits 8..11) and the encoding id the header
 * carries at 0xCC. Pure (no Qt): tests/farwater5 compiles it alone.
 *   0 (every bake before this): linear, code = round(8 r), r = speed / the body's reference, 15 = 1.94 x. A lodgen bake
 *     saturated 38953 of 138499 solved texels there (FARWATER4).
 *   1 (WW_LODL_FLOW_LOG=1): half-octave log, code = round(8 + 2 log2 r), 1 = 0.088 x .. 8 = 1 x .. 15 = 11.3 x;
 *     0 stays still water.
 * Constant words write code 8, which is 1 x the reference in both.
 * lane FARWATER6 (FARWATER5 L4): the reference is lodtFlowSpeedReference() for both writers (lodgen's river bake and
 * the marking tool's solve). */

#include <algorithm>
#include <cmath>
#include <vector>

constexpr unsigned kLodlFlowEncLinear = 0u;
constexpr unsigned kLodlFlowEncLog = 1u;

//! speed / reference for a stored code under an encoding (an unknown encoding reads as linear, the old reader)
inline double lodtFlowSpeedFactor( unsigned enc, int code )
{
	if ( code <= 0 )
		return 0.0;
	if ( enc == kLodlFlowEncLog )
		return std::exp2( double( code - 8 ) * 0.5 );
	return double( code ) / 8.0;
}

//! the code for speed / reference r (r <= 0 = 0), before the moving-water floor (watermark.h waterSpeedFloor)
inline int lodtFlowSpeedCode( unsigned enc, double r )
{
	double c;
	if ( !( r > 0.0 ) )
		return 0;
	if ( enc == kLodlFlowEncLog )
		c = std::floor( 8.0 + 2.0 * std::log2( r ) + 0.5 );
	else
		c = std::floor( 8.0 * r + 0.5 );
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

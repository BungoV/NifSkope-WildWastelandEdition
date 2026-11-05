#ifndef CONTACTLINES_H
#define CONTACTLINES_H

/*! lane PERFFIX2 (item 7): the dispatch of the contact march in Bend Studio's line layout (res/shaders/features/shadows/ww_contact.comp).
 *  Pure (no Qt, no GL), so tests/features/shadows/contactlines checks it alone.
 *
 *  Around the light's projected point (mask pixels, float) the mask splits into four sectors by the fragment pass's own
 *  major axis (|dx| > |dy| is X): 0 X+, 1 X-, 2 Y+, 3 Y-. In a sector the columns (major-axis pixel lines) are ranked by
 *  their distance from the light; 64 ranks make a band, and a band's lines run from the light through its FAR column
 *  (rank 64 b + 63) at every minor pixel centre j + 0.5, so in the band they are at most one pixel apart. One workgroup
 *  of 64 lanes per (band, line); the shader's owner rule (the first line whose position falls in a pixel) gives every
 *  pixel one lane. The light's own pixel (no column at distance > 0) is left to the mask's clear (lit, as the
 *  fragment pass's dist0 < 1). */

#include <cmath>
#include <vector>

struct WwContactBand
{
	int sector, kAnchor, jMin, waveStart;	// the shader's Band (std430 ivec4)
};

struct WwContactBands
{
	std::vector<WwContactBand> bands;
	std::vector<int> lines;	// per band: its line count (the shader needs only waveStart)
	int waves = 0;
	int first[4] = { 0, 0, 0, 0 };	// per sector: the column (major index) nearest the light
};

//! the bands and lines for a light at (lx, ly) mask pixels over an mw x mh mask
inline WwContactBands wwContactBands( double lx, double ly, int mw, int mh )
{
	WwContactBands r;
	for ( int sector = 0; sector < 4; sector++ ) {
		const bool majorX = sector < 2;
		const int s = ( sector & 1 ) == 0 ? 1 : -1;
		const double Lm = majorX ? lx : ly, Ln = majorX ? ly : lx;
		const int Mw = majorX ? mw : mh, Mn = majorX ? mh : mw;
		int fi, kLo, kHi;
		if ( s > 0 ) {
			fi = int( std::floor( Lm - 0.5 ) ) + 1;
			kLo = fi < 0 ? -fi : 0;
			kHi = Mw - 1 - fi;
		} else {
			fi = int( std::ceil( Lm - 0.5 ) ) - 1;
			kLo = fi - ( Mw - 1 ) > 0 ? fi - ( Mw - 1 ) : 0;
			kHi = fi;
		}
		r.first[sector] = fi;
		if ( kHi < kLo )
			continue;
		for ( int b = kLo / 64; b <= kHi / 64; b++ ) {
			const int kA = 64 * b + 63;
			const double Da = s * ( fi + s * kA + 0.5 - Lm );
			const int kn = 64 * b > kLo ? 64 * b : kLo, kf = 64 * b + 63 < kHi ? 64 * b + 63 : kHi;
			double jlo = 1e30, jhi = -1e30;
			for ( int k : { kn, kf } ) {
				double D = s * ( fi + s * k + 0.5 - Lm );
				D = D > 1e-3 ? D : 1e-3;
				for ( int row : { 0, Mn - 1 } ) {
					const double jf = ( row + 0.5 - Ln ) * Da / D + Ln - 0.5;
					jlo = jf < jlo ? jf : jlo;
					jhi = jf > jhi ? jf : jhi;
				}
			}
			// a pixel on the sector's diagonal (|minor| = D) is first reached by a line up to half a pixel beyond it:
			// Da x (1 + 0.5 / D) at the anchor, D the band's nearest column
			double Dn = s * ( fi + s * kn + 0.5 - Lm );
			Dn = Dn > 0.5 ? Dn : 0.5;
			const double reach = Da * ( 1.0 + 0.5 / Dn ) + 2.0;
			int jmin = int( std::floor( jlo ) ) - 1, jmax = int( std::ceil( jhi ) ) + 1;
			const int smin = int( std::floor( Ln - 0.5 - reach ) ), smax = int( std::ceil( Ln - 0.5 + reach ) );
			jmin = jmin > smin ? jmin : smin;
			jmax = jmax < smax ? jmax : smax;
			if ( jmax < jmin )
				continue;
			r.bands.push_back( { sector, kA, jmin, r.waves } );
			r.lines.push_back( jmax - jmin + 1 );
			r.waves += jmax - jmin + 1;
		}
	}
	return r;
}

#endif // CONTACTLINES_H

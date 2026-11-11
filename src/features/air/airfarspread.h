#ifndef WW_AIRFARSPREAD_H
#define WW_AIRFARSPREAD_H

/* lane AIRPERF1 A4: the far air grid's refresh of an UNCHANGED view, spread over its `refresh` frames (std only; the
 * unit: notes/modules1/airperf1/unit/farspread.cpp). The surfaces read only the summed volume, so the inject layers
 * are scratch between frames: steps 0 .. N - 2 inject the layers in equal runs, step N - 1 sums them all. Until that
 * last step the surfaces keep the last whole sum; a changed view rebuilds everything in its own frame, as before. */
namespace airfar
{

struct Share
{
	int inj0, inj1;	// this step injects layers [inj0, inj1)
	bool sum;		// this step sums every layer (the surfaces see the new grid)
};

//! step p (0 .. n - 1) of an n-frame refresh over `slices` layers; n < 2: all at once (the rule before)
inline Share spreadShare( int p, int n, int slices )
{
	if ( n < 2 )
		return { 0, slices, true };
#ifdef AIRFAR_RED_SUMFIRST
	if ( p == 0 )
		return { 0, 0, true };
	p--;
#else
	if ( p >= n - 1 )
		return { 0, 0, true };
#endif
	const long long s = slices;
	return { int( s * p / ( n - 1 ) ), int( s * ( p + 1 ) / ( n - 1 ) ), false };
}

}	// namespace airfar

#endif

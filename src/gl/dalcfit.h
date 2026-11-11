#ifndef WW_DALCFIT_H
#define WW_DALCFIT_H

/* lane SKYLIGHT1 (item 1b): the six-axis ambient's upload. cell_lights.glsl reads pow(dot(cellDalc, (N, 1)), 2.2), an
 * affine law in N over the gamma values g = DALC^(1/2.2), so it cannot hold six free values: with the plain mean of
 * the six as the constant, an up normal reads mean6 + (g_up - g_down) / 2, not g_up (the flight at CommonwealthOvercast
 * 13.8: DALC up 0.529 lux, read 0.349). The fit takes the constant from the up / down pair, so both poles a terrain
 * shows the most read their own value exactly; the side poles keep their half differences. Red
 * WW_CELL_DALC_FIT_RED=1: the mean of the six. */

namespace WwDalcFit
{

//! g in the travel order (X+ X- Y+ Y- Z+ Z-); sgn the axis sign (-0.5: n.z = +1 reads Z-, index 5); out = cellDalc
inline void encode( const float g[6], float sgn, bool red, float out[4] )
{
	float mean = 0.0f;
	for ( int a = 0; a < 6; a++ )
		mean += g[a] / 6.0f;
	out[0] = ( g[0] - g[1] ) * sgn;
	out[1] = ( g[2] - g[3] ) * sgn;
	out[2] = ( g[4] - g[5] ) * sgn;
	out[3] = red ? mean : 0.5f * ( g[4] + g[5] );
}

}	// namespace WwDalcFit

#endif

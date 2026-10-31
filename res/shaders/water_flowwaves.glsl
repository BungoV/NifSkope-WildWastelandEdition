// lane WATERLOOK1 step 5 (src/gl/cellocean.h): the wind's waves carried along the baked river flow (WATERFLOWBAKE1).
// Two reads of the spectrum, each moved downstream by the flow for its own time, restarting every period half a period
// apart; each fades to nothing at its restart, so the restarts never show (Vlachos 2010, "Water flow in Portal 2").
// Two decorrelated fields mixed with weights a, b keep only a^2 + b^2 of the slope variance (the water would breathe
// rougher and smoother each period): the mix is renormalised by the expected correlation of the two reads at the
// distance the flow has carried them apart, taken from the spectrum itself (it dips below zero near the peak waves'
// half wavelength), so still water reads exactly one sample (today's waves) and a slow current blends into it
// continuously. The flow is bilinear between the bake's texels (its own texels are
// nearest: the waves would tear along their edges).
uniform vec4 waterOceanFlow;	// seconds moved of reads A and B (each wrapped by the period), their weights (sum 1)
// per set (8 each): the slope field's correlation at d0 2^( i / 2 ), i 0..15, along the wind (4), then across it (4);
// d0 = the finest cascade's texel (src/gl/oceanspectrum.h slopeCorr)
uniform vec4 waterOceanFlowCorr[16];

vec2 wwWaterFlowSmooth( vec2 P )
{
	float tex = waterFlowGrid.z;
	vec2 g = ( P - waterFlowGrid.xy ) / tex - 0.5;
	vec2 t0 = floor( g ), f = g - t0;
	vec2 c0 = waterFlowGrid.xy + ( t0 + 0.5 ) * tex;
	return mix( mix( wwWaterFlow( c0 ), wwWaterFlow( c0 + vec2( tex, 0.0 ) ), f.x ),
		mix( wwWaterFlow( c0 + vec2( 0.0, tex ) ), wwWaterFlow( c0 + vec2( tex, tex ) ), f.x ), f.y );
}

float wwOcFlowCorrAt( int base, float x )
{
	int i = min( int( x ), 14 );
	float f = x - float( i );
	return mix( waterOceanFlowCorr[base + i / 4][i % 4], waterOceanFlowCorr[base + ( i + 1 ) / 4][( i + 1 ) % 4], f );
}

// 1 / the rms of a A + b B for unit fields correlated by how far apart the flow v (u/s) carried the two reads; set:
// the body's. Between the wind and across it the correlation is mixed by cos^2 of the flow's angle to the wind
float wwOcFlowNorm( vec2 v, int set )
{
	float a = waterOceanFlow.z, b = waterOceanFlow.w;
	int st = max( set, 0 );
	float d = length( v ) * abs( waterOceanFlow.x - waterOceanFlow.y ) * kOcMetre;	// m
	float r = d / ( 1.0 / ( 128.0 * waterOceanInvL[st].z ) );	// in finest texels
	float x = clamp( 2.0 * log2( max( r, 1.0 ) ), 0.0, 15.0 );
	float c = dot( v, waterOceanWind.xy ) / max( length( v ), 1e-9 );
	float rho = mix( wwOcFlowCorrAt( st * 8 + 4, x ), wwOcFlowCorrAt( st * 8, x ), c * c );
	if ( r < 1.0 )
		rho = 1.0 - ( 1.0 - rho ) * r * r;	// quadratic to 1 at no separation
	return inversesqrt( max( a * a + b * b + 2.0 * rho * a * b, 1e-6 ) );
}

/* lane FIX1031B: the bake's flow is the WATR NAM0 linear velocity (WATERFLOWBAKE1: "FO4 NAM0 speeds are small; scale
 * in the look", and the look never did): median 0.40, max 1.6, read as u/s the waves moved 6 mm/s (bungo's 10-31 river
 * frames: flow on = off). NAM0 is the current the game's physics pushes floating objects with, and the physics works in
 * metres (kOcMetre): 0.4 m/s is a slow river. INFERRED from the numbers and the convention, not from a capture.
 * Rivers only: a lake's or the sea's one constant NAM0 (0.3-0.5) is the game's push on floating things, not a current
 * to carry the whole body's waves (an engineering choice; they keep the reading before).
 * Red WW_CELL_WATER_OCEAN_RED=rawflow (1024): the bake's numbers as u/s everywhere. */
const float kOcFlowUnit = 1.0 / kOcMetre;	// u/s a bake unit (m/s)

// the spectrum at P (as wwWaterOcean) carried by the flow there; red noflow (256) reads it in place
WwOcean wwWaterOceanFlowed( vec2 P, vec2 dPx, vec2 dPy )
{
	vec2 v = ( waterOceanRed & 256 ) != 0 ? vec2( 0.0 ) : wwWaterFlowSmooth( P )
		* ( ( waterOceanRed & 1024 ) != 0 || abs( wwWaterInfo( P ).w - 1.0 ) > 0.5 ? 1.0 : kOcFlowUnit );
	if ( v == vec2( 0.0 ) )
		return wwWaterOcean( P, dPx, dPy );
	int set;
	WwOcean A = wwWaterOceanQ( P, P - v * waterOceanFlow.x, dPx, dPy, set );
	WwOcean B = wwWaterOceanQ( P, P - v * waterOceanFlow.y, dPx, dPy, set );
	float a = waterOceanFlow.z, b = waterOceanFlow.w, n = wwOcFlowNorm( v, set );
	WwOcean o = A;
	o.slope = n * ( a * A.slope + b * B.slope );
	o.var = ( a * a * A.var + b * b * B.var ) / max( a * a + b * b, 1e-6 );
	o.white = a * A.white + b * B.white;
	o.cau = a * A.cau + b * B.cau;
	return o;
}

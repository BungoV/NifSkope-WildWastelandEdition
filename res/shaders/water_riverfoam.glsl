// lane WATERLOOK1 step 15 (WATERFLOW1, src/gl/cellwater.h): foam streaks on baked rivers, drawn along the baked flow
// (WATERFLOWBAKE1) and carried downstream by it. Pattern: each of the four flow texels around the pixel lays a streak
// noise along its own flow (long along, narrow across) and the four are blended bilinearly (tiled directional flow,
// van Hoesel 2010); each is read twice, half a foam life apart, fading out at its restart (Vlachos 2010, "Water flow
// in Portal 2"). The texels share one noise in world space (origin snapped to 2048 u), so texels that flow alike draw
// the same streaks and a streak crosses texels without dissolving; the blend is renormalised by the reads' own
// correlation, so the foam keeps its cover anywhere. Amount: foam made by the moving water (half at the bake's median
// speed) and gathered where the surface flow converges: steady foam c = s / ( 1 / T - C ) for a source s, life T and
// convergence C = -div v, so the cover grows 1 / ( 1 - C T ) where the flow squeezes together, thins where it spreads.
// Needs water_info.glsl (wwWaterFlow, wwWaterInfo) and water_shore.glsl (wwShoreNoise, wwShoreHash) before it.
// Lane FIX1031B (LATE ITEM 8, foam): the bake's flow is m/s (the first FIX1031B agent's measure: 0.36-0.79 at Cutler Bend),
// so the streaks are now carried at v / kRfMetre u/s (they crawled at 1/70 of the river) with an 8 s life (the Vlachos
// period ~ one streak length at the river's speed), the source half at 0.4 m/s, and the source grows where the river runs
// SHALLOW (under 1.5 m) or SHOALS along its flow (the bed rising downstream: turbulence, rapids): x ( 1 + 2 shallow +
// 2 shoal ). Red WW_CELL_WATER_RIVERFOAM_RED=raw (16): the old reading (raw flow, no depth term; life from C++).
uniform bool waterRiverFoamOn;
uniform int waterRiverFoamRed;	// 1 nodiv (no gathering), 2 still (not carried), 4 iso (round blobs), 8 alias (no far fade), 16 raw
uniform vec4 waterRiverFoamK;	// foam life (s), cover at full source, half-source speed (m/s), opacity
const float kRfMetre = 0.0142875;	// m per world unit
uniform vec4 waterRiverFoamK2;	// streak width (u), length / width, edge softness (sd), gathering cap (C T)

// one read's sd, and the value noise's correlation exp( -d^2 / kRfCorr ) at a lattice offset d (fit within 0.01),
// both measured: notes/waterlook1/tools/riverfoam_gl.py, vn_corr.py
const float kRfSigma = 0.1586;
const float kRfCorr = 0.593;

// one read of the streak noise, zero mean, at s = (along / length, across / width), o the read's offset
float wwRfStreak( vec2 s, vec2 o )
{
	return 0.65 * wwShoreNoise( s + o ) + 0.35 * wwShoreNoise( 2.03 * s + o.yx ) - 0.5;
}

// the read made a unit normal (Heitz and Neyret 2018, histogram-preserving blending), so any blend of reads has one
// distribution and the cover follows D at every weight; fit: notes/waterlook1/tools/rf_gauss_fit.py
float wwRfGauss( float x )
{
	float u = x * ( 1.0 / kRfSigma ), u2 = u * u;
	return u * ( 0.91999 + u2 * ( 0.02318 + u2 * 0.002227 ) );
}

// the correlation of two reads (same offset) whose coordinates differ by d
float wwRfRho( vec2 d )
{
	float e = dot( d, d ) / kRfCorr;
	return ( 0.4225 * exp( -e ) + 0.1225 * exp( -4.1209 * e ) ) * ( 1.0 / 0.545 );
}

// (foam density 0..1, alpha 0..1, convergence x life, the flow's speed u/s) at world P; t the clock (s), foot the pixel (u)
vec4 wwWaterRiverFoam( vec2 P, float t, float foot )
{
	if ( !waterRiverFoamOn || !waterFlowOn )
		return vec4( 0.0 );
	if ( abs( wwWaterInfo( P ).w - 1.0 ) > 0.5 )
		return vec4( 0.0 );	// rivers only
	float tex = waterFlowGrid.z;
	vec2 g = ( P - waterFlowGrid.xy ) / tex - 0.5;
	vec2 t0 = floor( g ), f = g - t0;
	vec2 c0 = waterFlowGrid.xy + ( t0 + 0.5 ) * tex;
	bool raw = ( waterRiverFoamRed & 16 ) != 0;
	float uq = raw ? 1.0 : 1.0 / kRfMetre;	// bake units -> u/s
	vec2 vc[4] = vec2[4]( wwWaterFlow( c0 ), wwWaterFlow( c0 + vec2( tex, 0.0 ) ), wwWaterFlow( c0 + vec2( 0.0, tex ) ),
		wwWaterFlow( c0 + vec2( tex, tex ) ) );
	for ( int i = 0; i < 4; i++ )
		vc[i] *= uq;
	vec2 v = mix( mix( vc[0], vc[1], f.x ), mix( vc[2], vc[3], f.x ), f.y );
	float spd = length( v );
	if ( spd < 1e-4 )
		return vec4( 0.0 );
	// the bilinear field's divergence: d vx / dx + d vy / dy
	float div = ( mix( vc[1].x - vc[0].x, vc[3].x - vc[2].x, f.y ) + mix( vc[2].y - vc[0].y, vc[3].y - vc[1].y, f.x ) ) / tex;
	float T = waterRiverFoamK.x;
	float CT = ( waterRiverFoamRed & 1 ) != 0 ? 0.0 : clamp( -div * T, -9.0, waterRiverFoamK2.w );
	float sm = spd / uq;	// the source's speed in bake units (m/s; raw: u/s as before)
	float src = waterRiverFoamK.y * sm / ( sm + waterRiverFoamK.z );
	if ( !raw ) {
		float dHere = wwWaterInfo( P ).y * kRfMetre, dDown = wwWaterInfo( P + v / spd * tex ).y * kRfMetre;
		float shallow = 1.0 - smoothstep( 0.3, 1.5, dHere );
		float shoal = clamp( ( dHere - dDown ) / 0.5, 0.0, 1.0 );	// the bed rises 0.5 m within a texel downstream
		src *= 1.0 + 2.0 * shallow + 2.0 * shoal;
	}
	float D = clamp( src / ( 1.0 - CT ), 0.0, 1.0 );
	if ( D < 0.002 )
		return vec4( 0.0, 0.0, CT, spd );
	float W = waterRiverFoamK2.x;
	float vis = ( waterRiverFoamRed & 8 ) != 0 ? 1.0 : 1.0 - smoothstep( 0.35, 0.7, foot / W );
	// cover D over the threshold of a unit-sd read (logistic fit of the normal quantile)
	float z = log( ( 1.0 - D ) / D ) / 1.702;
	float a = 0.0;
	if ( vis > 0.0 ) {
		float L = ( waterRiverFoamRed & 4 ) != 0 ? W : W * waterRiverFoamK2.y;
		float ph = fract( t / T );
		vec2 wp = vec2( 1.0 - abs( 2.0 * ph - 1.0 ), abs( 2.0 * ph - 1.0 ) );
		vec2 tp = ( waterRiverFoamRed & 2 ) != 0 ? vec2( 0.0 ) : vec2( ph, fract( ph + 0.5 ) ) * T;
		vec2 wc = vec2( 1.0 ) - f;
		vec4 wq = vec4( wc.x * wc.y, f.x * wc.y, wc.x * f.y, f.x * f.y );
		float n = 0.0, var = 0.0;
		for ( int k = 0; k < 2; k++ ) {
			vec2 o = k == 0 ? vec2( 0.37, 0.71 ) : vec2( 311.37, 97.71 );
			vec2 s[4];
			for ( int i = 0; i < 4; i++ ) {
				float sp = length( vc[i] );
				vec2 dir = sp > 1e-4 ? vc[i] / sp : vec2( 1.0, 0.0 );
				vec2 Pc = c0 + vec2( float( i & 1 ), float( i >> 1 ) ) * tex;
				vec2 blk = floor( Pc * ( 1.0 / 2048.0 ) );
				vec2 r = P - 2048.0 * blk - vc[i] * tp[k];
				// each 2048 u block its own noise (a lattice offset by its hash): no repeat from block to block
				uint hb = wwShoreHash( ivec2( blk ) );
				s[i] = vec2( dot( r, dir ) / L, dot( r, vec2( -dir.y, dir.x ) ) / W )
					+ vec2( float( hb & 1023u ), float( ( hb >> 10 ) & 1023u ) ) * 2.0;
				n += wp[k] * wq[i] * wwRfGauss( wwRfStreak( s[i], o ) );
			}
			for ( int i = 0; i < 4; i++ )
				for ( int j = 0; j < 4; j++ )
					var += wp[k] * wp[k] * wq[i] * wq[j] * ( i == j ? 1.0 : wwRfRho( s[i] - s[j] ) );
		}
		n /= sqrt( max( var, 1e-8 ) );	// unit sd at every weight
		float sf = waterRiverFoamK2.z;
		a = smoothstep( z - sf, z + sf, n );
	}
	a = mix( D, a, vis ) * waterRiverFoamK.w;
	return vec4( D, a, CT, spd );
}

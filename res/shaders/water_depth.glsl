/* lane WATERDEPTH1 (src/gl/cellwater.h): the water's depth colour from the TRUE vertical bed depth.
 * Only fo4_waterlook.frag compiles this (WW_WATER_DEPTH); the default fo4_water program is the one it always was.
 *   bed depth  the baked .lodl depth (water_info.glsl), bilinear over the 4 texels around P (128 u a texel); a dry
 *              texel counts 0 (the shore), a wet texel at or under the ground (sloped / placed water) counts 0.
 *              Known only when a tap is wet and P lies within 128 u of that body's height (else: a placed water
 *              mesh at its own height, the game's term alone).
 *   who wins   the opaque scene behind the pixel (the game's term, per pixel, sees rocks and objects); the bake where
 *              the frame has no scene there, or the scene lies more than 256 u under the baked bed (the drawn
 *              bed is missing: far rings, holes in the LOD ground)
 *   along      the straight view ray to a flat bed at that depth: vert / |V.z| (the game's scene term's geometry)
 *   colour     the game's ramp (WATR shallow / deep colour, ranges, alphas, depth amount: fo4_water.frag depthFog);
 *              transmittance T = (1 - a) x exp( -k L ), a the game's opacity (the turbid share, flat in colour),
 *              k pure water's absorption (Pope & Fry 1997, 1/m at 650 / 550 / 450 nm), L the path in metres
 *   no scene   the bed is the record's shallow colour (the ramp's own zero-depth end)
 *   far        past the noise falloff (dfade 0) the game shows one reflection colour; with the depth known the
 *              body shows through it by 1 - F (Fresnel): grazing = the game's far colour
 * For later lanes (WATERCAUSTICS1): wwWaterBedDepth() is the vertical depth, wwWaterAbsorb() pure water's T. */

uniform int waterDepthRed;	// 1 flat (the bed depth read as 0), 2 noabsorb (no pure-water term), 3 nearest (no bilinear)
uniform bool waterDepthOn;	// lane WATERCOMBINE1: the Water depth colour row (fo4_waterlook carries every water look)

const float kWwUnitMetres = 0.0142875;	// metres a game unit
const vec3 kWwPureWater = vec3( 0.340, 0.0565, 0.00922 );	// Pope & Fry 1997, absorption 1/m at 650, 550, 450 nm

struct WwWaterBed
{
	bool baked;	// the bake wins at this pixel
	bool known;	// a depth is known (the scene's or the bake's)
	float vert;	// vertical depth, units
	float along;	// path along the view ray, units
	float bd;	// lane FIX1031B: the baked bed's vertical depth here (< 0 = not known)
};

// the vertical bed depth under the water surface point P (world units); < 0 = not known here
float wwWaterBedDepth( vec3 P )
{
	if ( !waterInfoOn )
		return -1.0;
	vec2 tc = ( P.xy - waterInfoGrid.xy ) / waterInfoGrid.z - 0.5;
	vec2 t0 = floor( tc ), f = tc - t0;
	if ( waterDepthRed == 3 ) {
		t0 = floor( tc + 0.5 );
		f = vec2( 0.0 );
	}
	float d[4];
	float h = 0.0;
	bool wet = false;
	for ( int k = 0; k < 4; k++ ) {
		vec2 tk = t0 + vec2( float( k & 1 ), float( k >> 1 ) );
		uint w = wwWaterInfoWord( waterInfoGrid.xy + ( tk + 0.5 ) * waterInfoGrid.z );
		uint id = w & 0xFFFFu;
		float code = float( w >> 24 ) / 255.0;
		d[k] = id != 0u ? code * code * 8192.0 : 0.0;
		if ( id != 0u && !wet ) {
			wet = true;
			h = texelFetch( waterInfoBodies, ivec2( int( id % 256u ), int( id / 256u ) ), 0 ).r;
		}
	}
	if ( !wet || abs( P.z - h ) > 128.0 )
		return -1.0;
	if ( waterDepthRed == 1 )
		return 0.0;
	float b = mix( mix( d[0], d[1], f.x ), mix( d[2], d[3], f.x ), f.y );
	return max( b + ( P.z - h ), 0.0 );
}

// pure water's transmittance over `units` of path
vec3 wwWaterAbsorb( float units )
{
	return waterDepthRed == 2 ? vec3( 1.0 ) : exp( -kWwPureWater * ( units * kWwUnitMetres ) );
}

// which depth this pixel uses; sceneVert / sceneAlong = the game's (1e9 = no scene behind)
WwWaterBed wwWaterBedAt( vec3 P, vec3 V, float sceneVert, float sceneAlong )
{
	WwWaterBed b;
	float bd = wwWaterBedDepth( P );
	b.baked = bd >= 0.0 && ( sceneVert > 1e8 || sceneVert > bd + 256.0 );
	b.known = b.baked || sceneVert < 1e8;
	b.vert = b.baked ? bd : sceneVert;
	b.along = b.baked ? bd / max( -V.z, 1e-3 ) : sceneAlong;
	b.bd = bd;
	return b;
}

/* lane FIX1031B (the teeth, LATE ITEM 6.2): how much the baked bed counts at this pixel (0 = the drawn scene). The rule
 * above is a hard step where the scene lies 256 u under the baked bed; a coarse ground mesh (far LOD, 512-4096 u
 * triangles) crossing that line cut straight-edged teeth into the clear shallows (sheet_W0sea waterline 4x). soft = a
 * ramp from 64 u to 768 u under the baked bed, so the two beds blend; not soft = the step (red old) */
float wwWaterBedBakeW( float bd, float sceneVert, bool soft )
{
	if ( bd < 0.0 )
		return 0.0;
	if ( sceneVert > 1e8 )
		return 1.0;
	return soft ? smoothstep( bd + 64.0, bd + 768.0, sceneVert ) : ( sceneVert > bd + 256.0 ? 1.0 : 0.0 );
}

// the game's depth colour over c (fogC its ramp colour, a its opacity) with pure water's absorption on top
vec3 wwWaterDepthMix( vec3 c, vec3 fogC, float a, float along )
{
	vec3 T = vec3( 1.0 - a );
	if ( along < 1e8 )
		T *= wwWaterAbsorb( along );
	return mix( fogC, c, T );
}

// past the noise falloff: the body through the game's far colour by 1 - F, where the depth is known
vec3 wwWaterDepthFar( vec3 far, vec3 body, float F, bool known )
{
	return known ? mix( body, far, F ) : far;
}

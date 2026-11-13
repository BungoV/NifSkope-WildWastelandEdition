// HLSL source of features/air/lookdev_fog.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	bool fogOn;
	float4 fogK[6];		// cb12[41..46]
	float4 fogView;		// world z above the ground = dot( fogView.xyz, posView ) + fogView.w
	float fogDistScale;	// view units -> game units (1 / the view scale)
	float4 fogSun;		// view space, unit, TO the sun; .w = intensity
	float4 fogSunColour;	// linear; .w = fDirectionalFogPower
	float fogUnit;	// lane PHYSUNITS1: the light's scale over the weather's (0 = unset = 1)
	float4 fogProbe;		// gates only: .z = 0 off, 1 alpha, 2 colour / 2, 3 hb (at d = .x, z = .y); 5 geometry echo (d / 4096, 0.5 + z / 2000);
	// 6 / 7 per fragment (lane FOG2): (alpha, hb, 0) / colour ^ 1/2.2; 8 the geometry, wide (d / 16384, 0.5 + z / 32768)
	int fogRed;		// gates only: 1 maxclamp, 2 noescape, 4 height0; 8 the air tail's source from the last slice alone (MODULES1 R7-4)
	// 16 the sky's level ray on the old branch (MODULES1 R7 item 6)
	/* MODULES1 R7 night sky d: the saturated air's night colour x the night share (a: the share, 0 = day, the red
	 * WW_NIGHTAIR_RED=1, no weather). Master Off: the weather record's far fog colour, what the game's fog ends at; On: the
	 * sky's mean with the record's night level (the ambient axes' mean; the air's albedo is 1). The whole air of a ray
	 * (L, T) holds at least that x (1 - T): the sky's cover (cell_volsky.frag) and the surfaces (wwFog) alike, so the far
	 * land and the sky behind it end at one colour. */
	float4 fogNight;
	/* FARWATER1 late item 3 (bungo b71, night: grey haze reading brown between a blue sky and blue water): 1 = the air's light
	 * at night takes the night sky's own tint (fogNight's chroma at the air's own luminance), weighted by the night share
	 * fogNight.a, so twilight blends into it. Red WW_NIGHTAIR_RED=neutral (live): 0, the neutral lobes' grey as before. */
	float fogNightHue;
	float4 atAp;		// 1 = on, maxKm, km per view unit, slices
	float4 atApProj;		// the projection the volume was built for: P00, P11, P20, P21
	float atApDouble;	// red "double": the fog laid over the whole aerial perspective
	/* lane VOLAIR1 (src/features/air/cellvolair.h): the air IN PLACE of the game's fog. volTex holds, per froxel column, the
	 * light the air sends toward the eye from the near plane to the far edge of slice k (rgb) and the transmittance
	 * over the same stretch (a); the slices are exponential from volSlices.x over ln(far / near) = volSlices.y. A
	 * surface at distance d shows colour x T(d) + L(d), linear in log-depth. Linear light. */
	bool volOn;
	float4 volRect;		// the viewport's origin, 1 / its size
	int volFarClamp;	// 1: the lookup clamped to its column's farthest (red nofarclamp, and the programs without it: 0)
	float4 volSlices;		// near, ln(far / near), slices, 1 = applied (0 under the red "off": the game's fog drawn)
	int volProbe;		// gates only: 1 the light term L alone, raw; 2 the transmittance T, grey; 3 the sun's T to the surface
	int volFogBlend;	// test only (WW_CELL_VOLAIR_FOGBLEND=1, option B): the game's fog drawn over the lit air
	float4 volFar;		// lane FARGRID1: the far grid's start (the near grid's end), ln(end / start), slices, 1 = on
	float4 volPack;		// the packed volume's shares: the near grid's width, height, the far grid's
	float4 volPackDims;	// the packed volume's texels: width, height, depth; the near grid's slices
	float4 volAirMed;		// the pass's medium (cell_volair.frag airMed): sigma0, zb, 1 / H, the top (the fog's height frame)
	float4 volAirLayer;	// its near band and ground layer (airLayer): the near share w, zn, 1 / Hn, the ground strength s
	float4 volAirGround;	// (airGround): the ground layer's anchor zg, 1 / its thickness, G2: 1 / the top's falloff height (0: a hard top)
	int volAirFlags;	// 1 the surfaces' sun through the air (FOGSUN1; red nosunt clears it), 2 the far tail (red
	float4 volAirTerrU;	// its uv at view point p: (dot( U.xyz, p ) + U.w, dot( V.xyz, p ) + V.w)
	float4 volAirTerrV;
	float4 volAirTerrFU;	// FOGTERRAIN3: the fine level's (layer 1; uv -1 everywhere: none)
	float4 volAirTerrFV;
	float volAirTerrRef;	// the world z of the fog frame's ground (airTerrRef)
	/* lane RAINFOG2 (CLOUD6e's leftover): the rain fog (cell_rainfog.glsl, src/features/weather/rainfogmath.h) in the closed-form tails
	 * past the froxel grids (wwVolAirAtUv, wwVolAirAtFar) and in the sky's tail (wwVolAirSky). Its optical depth along a
	 * view segment: the segment cut to the heights the fog reaches (above base + the fade nothing), then n sub-segments,
	 * each the cloud map's coverage at its middle (faded to the band's scalar past the map) x the profile's EXACT mean over
	 * its height range (wwAirRainP, the antiderivative). volAirRain.x 0 (no rain or snow, the red, no cloud map, the air
	 * pass's rain fog off): 0, the tails exactly as before. The loop's bound is a uniform: one copy, not unrolled. */
	float4 volAirRain;		// x sigma per game unit near the ground (0 = none), y the base excess, z 1 / falloff km, w 1 / fade km
	float4 volAirRainFar;		// x the Low band's coverage past the map, y the map's edge band (uv), z the sub-segments
	row_major float4x4 volAirRainXf;		// view -> (u, v, km below the Low base): cell_rainfog.glsl's rfFromView
	/* lane WEATHERBLEND4: the drifting air (cell_volair.frag airDrift) where the surfaces and the puffs read it: the density
	 * factor at view point p, around 1; amp 0 (drift off, red WW_CELL_FOGDRIFT_RED=nosun) = 1 */
	float4 volAirDrift;		// the air pass's volDrift: x amp, y the fade's start, z 1 / its length, w the noise's mean
	float4 volAirDriftUv[6];	// per octave three rows: the noise's uvw at view point p = dot( row, vec4( p, 1 ) )
	int volAirDriftRed;		// lane WEATHERBLEND5: 1 red onesun (the sun's column at one point), 2 red puffonly
#else
#ifndef WW_GLSL_SIZE_Texture3D_float4_
#define WW_GLSL_SIZE_Texture3D_float4_
int3 textureSize(Texture3D<float4> t, int lod) { uint w, h, d, l; t.GetDimensions(uint(lod), w, h, d, l); return int3(w, h, d); }
#endif
#ifndef WW_GLSL_MOD
#define WW_GLSL_MOD	// GLSL mod: x - y floor(x / y) (HLSL fmod truncates)
float mod(float x, float y) { return x - y * floor(x / y); }
float2 mod(float2 x, float2 y) { return x - y * floor(x / y); }
float3 mod(float3 x, float3 y) { return x - y * floor(x / y); }
float4 mod(float4 x, float4 y) { return x - y * floor(x / y); }
float2 mod(float2 x, float y) { return x - y * floor(x / y); }
float3 mod(float3 x, float y) { return x - y * floor(x / y); }
float4 mod(float4 x, float y) { return x - y * floor(x / y); }
#endif
// Lookdev weather fog (lane FOG1): the engine fog formula, a line-for-line port
// of the composite shader as transcribed in scratchpad/pbrprep1_20260924/spec_fog.md
// 2.4 (FO4CS ambient_ibl_pass.hlsl F4FXApplyEngineFog), fed the cb12[41..46]
// packing (fogK) that gl/lookdevstage.cpp builds from the WTHR (esmweather.cpp
// wwFogAt). Applied per fragment to scene geometry in LINEAR light, before the
// exposure and the view transform (studioOutput); the sky dome never takes the game's
// fog (lane VOLAIR1: it takes the air, cell_volsky.frag, wwVolAirSky below).
// The fog-sun term's colour and intensity are INFERRED (the lookdev sun: its
// direction, the Sunlight row, intensity 1); the power is fDirectionalFogPower.
// Needs: posView = the fragment's view-space position (game units x view scale).

#define WW_LOOKDEV_FOG_GLSL 1	// lane SHADOWPAR1: ww_sunshadow.glsl's air-tail reader needs fogDistScale


float3 wwAirNightHue( float3 L )
{
	if ( or( fogNightHue <= 0.0, fogNight.a <= 0.0 ) )
		return L;
	const float3 kY = float3( 0.2126, 0.7152, 0.0722 );
	float yn = dot( fogNight.rgb, kY );
	if ( yn <= 1e-12 )
		return L;
	return lerp( L, dot( max( L, (float3)( 0.0 ) ), kY ) * fogNight.rgb / yn, clamp( fogNight.a * fogNightHue, 0.0, 1.0 ) );
}

/* lane SKYA (ATMO2): the physical sky's aerial perspective (src/features/sky/atmosphere.h, Hillaire 2020 sec. 5.4). atApTex
 * holds per froxel the light the air sends toward the eye from the camera to slice k (rgb, the sky's scene units)
 * and the mean transmittance (a); slice k's centre sits at maxKm ((k + 0.5) / n)^2, so the texture's depth
 * coordinate is sqrt(d / maxKm). Off (atAp.x 0): every path below is the one before the lane. */
#include "../sky/atmo_apfog.hlsli"
Texture3D<float4> atApTex : register(t25, space0); SamplerState _atApTex_sampler : register(s25, space0);

bool wwApOn()
{
	return atAp.x > 0.5;
}

// (L, T) of the sky's air between the eye and posView (view space)
float4 wwAtmoApAt( float3 posView )
{
	float dKm = length( posView ) * atAp.z;
	float zv = max( -posView.z, 1e-6 );
	float2 ndc = float2( atApProj.x * posView.x + atApProj.z * posView.z, atApProj.y * posView.y + atApProj.w * posView.z ) / zv;
	float2 uv = clamp( ndc * 0.5 + 0.5, 0.0, 1.0 );
	float n = atAp.w;
	float s = sqrt( clamp( dKm / atAp.y, 0.0, 1.0 ) );
	float4 v;
	if ( s < 0.5 / n ) {
		// nearer than slice 0's centre: its (L, 1 - T) scaled by the distance (both linear there)
		v = atApTex.Sample( _atApTex_sampler, float3( uv, 0.5 / n ) );
		float f = s * n * 2.0;
		f *= f;
		v = float4( v.rgb * f, 1.0 - ( 1.0 - v.a ) * f );
	} else {
		v = atApTex.Sample( _atApTex_sampler, float3( uv, s ) );
	}
	return float4( max( v.rgb, (float3)( 0.0 ) ), clamp( v.a, 0.0, 1.0 ) );
}

// fog alpha, height blend and colour (before the sun term) of one fragment
float wwFogEval( float d, float z, out float hb, out float3 fogCol )
{
	if ( ( fogRed & 4 ) != 0 )
		z = 0.0;
	float ramp = d * fogK[0].x - fogK[0].z;
	float f = clamp( ramp, 0.0, 1.0 );
	float2 pair = clamp( z * fogK[5].xy - fogK[5].zw, 0.0, 1.0 );
	hb = lerp( pair.x, pair.y, f );
	float mx = fogK[2].w;
	float clampT = ( ramp > 0.75 && ( fogRed & 1 ) == 0 ) ? min( ( f - 0.75 ) * 4.0 * ( 1.0 - mx ) + mx, 1.0 ) : mx;
	float escape = ( ramp < 0.015 && ( fogRed & 2 ) == 0 ) ? f * 66.666672 : 1.0;
	float I = f > 0.0 ? min( clampT, pow( f, fogK[1].w ) ) : 0.0;
	float weight = hb * fogK[3].w + ( 1.0 - hb );
	fogCol = lerp( lerp( fogK[1].rgb, fogK[3].rgb, I ), lerp( fogK[2].rgb, fogK[4].rgb, I ), hb );
	return weight * I * escape;
}
Texture3D<float4> volTex : register(t26, space0); SamplerState _volTex_sampler : register(s26, space0);
Texture2D<float4> volTile : register(t27, space0); SamplerState _volTile_sampler : register(s27, space0);	// the air pass's stage 0: per froxel column the farthest, nearest opaque ray distance (game units)
/* FARWATER1 round 2: a draw that is never in the obscurance pass's depth (the water) sets this before wwFog: the column's
 * farthest opaque surface is a hill behind it or nothing, so the clamp cut the sea's air to the hill's (light flat-topped
 * blocks where far hills meet the sea). Red WW_ENDLESS_SEA_RED=air: clamped as before. */
static bool wwAirFree = false;

/* FARWATER1 round 2: the endless sea is curved (endlesssea.cpp: z = sea - r^2 / 2R) while the air's height frame is flat;
 * its far rows sat up to ~19700 u under the frame's ground and took a wrong, deep blue air. The point as the flat frame
 * sees it: lifted by g^2 / 2R (g its horizontal world distance), so it reads the air at its height above the ground, as
 * the far land at the same distance does. Also the sky band's far-plane point while the sea is drawn (cell_volsky.frag). */
float3 wwAirFlatPos( float3 posView )
{
	float up2 = max( dot( fogView.xyz, fogView.xyz ), 1e-12 );
	float h = dot( fogView.xyz, posView );
	float g2 = max( dot( posView, posView ) * up2 - h * h, 0.0 );	// |fogView.xyz| = view -> world, h is world
	return posView + fogView.xyz * ( g2 / ( 2.0 * 6371000.0 * 70.0 ) / up2 );
}
				// with its own alpha (0 nearer than its start distance); 0 = option A, the real light only


				// froxonly clears it), 4 the records' colour on the tail (red physcol clears it), 8 the sky
				// through the air (cell_volsky.frag; red nosky clears it), 16 the effects and particles through the
				// air (wwFogFx; red fxfog clears it), 32 the tricubic read (FOGTRICUBIC1, WW_CELL_VOLAIR_TRICUBIC=1),
				// 64 the ground layer on the terrain grid (FOGTERRAIN2; red WW_FOG_TERRAIN_RED=absolute clears it)
Texture2DArray<float4> volAirTerr : register(t28, space0); SamplerState _volAirTerr_sampler : register(s28, space0);	// FOGTERRAIN2: the pass's low-pass terrain (cell_airmed.glsl airTerr), world z

// the column between heights lo < hi of one term B(z) = min(1, exp(-(z - z0) invH)): flat under z0, exponential
// above (invH 0: flat everywhere); no cancellation of two large columns. Shared with cell_volair.frag airCol.
float wwAirColOne( float lo, float hi, float z0, float invH )
{
	if ( hi <= lo )
		return 0.0;
	if ( invH <= 0.0 )
		return hi - lo;
	float below = max( min( hi, z0 ) - lo, 0.0 );
	float a = max( lo, z0 );
	if ( hi <= a )
		return below;
	float w = hi - a;
	float u = w * invH;
	float e1 = u < 1e-3 ? 1.0 - u * ( 0.5 - u / 6.0 ) : ( 1.0 - exp( -u ) ) / u;
	return below + exp( -( a - z0 ) * invH ) * w * e1;
}

/* MODULES1 G2 (bungo 11-09, flight items 10b, 10c: "a flat pale-blue slab with a hard, stair-stepped edge", "above the
 * top there is no haze at all"): the air does not end at its top. Above it the top's own density falls off with height,
 * exp(-(z - top) / Ht), Ht = 1 / ground.z (cellvolair.cpp vaTopFall: 120 m), in every site of the law at once: the
 * froxels, the surfaces' tail, the sky's tail, the sun's and the sky light's columns. ground.z 0 (the red
 * WW_CELL_AIR_TOPFALL_RED=1): empty above the top, as before, to the bit.
 * wwAirEnd: the height where the air counts as ended (16 falloff heights, 1e-7 of the top's density); the red: the top */
float wwAirEnd( float4 med, float4 ground )
{
	return ground.z > 0.0 ? med.w + 16.0 / ground.z : med.w;
}

// the density per unit sigma0 at height z (cell_volair.frag airRho)
float wwAirRhoLaw( float z, float4 med, float4 layer, float4 ground )
{
	float k = 1.0;
	if ( z > med.w ) {
		if ( ground.z <= 0.0 )
			return 0.0;
		k = exp( -( z - med.w ) * ground.z );	// G2: the top's density, falling off
		z = med.w;
	}
	float r = med.z > 0.0 ? min( 1.0, exp( -( z - med.y ) * med.z ) ) : 1.0;
	if ( layer.x <= 0.0 && layer.w <= 0.0 )
		return k * r;
	if ( layer.x > 0.0 )
		r = ( 1.0 - layer.x ) * r + layer.x * ( layer.z > 0.0 ? min( 1.0, exp( -( z - layer.y ) * layer.z ) ) : 1.0 );
	if ( layer.w > 0.0 )
		r += layer.w * ( ground.y > 0.0 ? min( 1.0, exp( -( z - ground.x ) * ground.y ) ) : 1.0 );
	return k * r;
}

/* the column of the whole density (cellvolair.cpp VaLayer): (1 - w) far band + w near band + s ground layer, above the
 * top its falloff (G2; the red: empty); exactly the far band's column when w = s = 0 */
float wwAirColLaw( float lo, float hi, float4 med, float4 layer, float4 ground )
{
	float up = ground.z > 0.0 && hi > med.w
		? wwAirRhoLaw( med.w, med, layer, ground ) * wwAirColOne( max( lo, med.w ), hi, med.w, ground.z ) : 0.0;
	hi = min( hi, med.w );
	float c = wwAirColOne( lo, hi, med.y, med.z );
	if ( layer.x <= 0.0 && layer.w <= 0.0 )
		return c + up;
	if ( layer.x > 0.0 )
		c = ( 1.0 - layer.x ) * c + layer.x * wwAirColOne( lo, hi, layer.y, layer.z );
	if ( layer.w > 0.0 )
		c += layer.w * wwAirColOne( lo, hi, ground.x, ground.y );
	return c + up;
}

// FOGTERRAIN3: the ground at view point p (cell_airmed.glsl airTerrH): the fine level over the coarse one, faded at its edge
float wwAirTerrH( float3 p )
{
	float2 uv = float2( dot( volAirTerrU.xyz, p ) + volAirTerrU.w, dot( volAirTerrV.xyz, p ) + volAirTerrV.w );
	float2 uf = float2( dot( volAirTerrFU.xyz, p ) + volAirTerrFU.w, dot( volAirTerrFV.xyz, p ) + volAirTerrFV.w );
	float hc = volAirTerr.Sample( _volAirTerr_sampler, float3( uv, 0.0 ) ).r;
	float wf = clamp( min( min( uf.x, 1.0 - uf.x ), min( uf.y, 1.0 - uf.y ) ) * 16.0, 0.0, 1.0 );
	return wf > 0.0 ? lerp( hc, volAirTerr.Sample( _volAirTerr_sampler, float3( uf, 1.0 ) ).r, wf ) : hc;
}

// FOGTERRAIN2: the ground layer's anchor at view point p, the froxels' (cell_airmed.glsl airGroundAt); flag 64 off: absolute
float4 wwAirGroundAt( float3 p )
{
	if ( ( volAirFlags & 64 ) == 0 )
		return volAirGround;
	return float4( volAirGround.x + wwAirTerrH( p ) - volAirTerrRef, volAirGround.yzw );
}

/* FOGTERRAIN3: the anchor for a ray segment (the far tail, the sky): from view point p0 along unit view direction dirV for
 * len fog units at elevation sine s, inside the grid's window. The column grows as the layer-weighted mean of exp(h / H)
 * (weight exp(-s x / H), cut where it falls under e^-40). Six samples at the sixths of the wider exp(-s x / 3H), weighted
 * back to the layer's own weight: a ground climbing with the ray far out still counts (notes/fogterrain3 rayexp). */
float4 wwAirGroundRay( float3 p0, float3 dirV, float len, float s )
{
	if ( ( volAirFlags & 64 ) == 0 )
		return volAirGround;
	float k = 1.0 / max( fogDistScale, 1e-12 );	// view units per fog unit
	float2 u0 = float2( dot( volAirTerrU.xyz, p0 ) + volAirTerrU.w, dot( volAirTerrV.xyz, p0 ) + volAirTerrV.w );
	float2 du = float2( dot( volAirTerrU.xyz, dirV ), dot( volAirTerrV.xyz, dirV ) ) * k;
	float2 ex = float2( du.x > 0.0 ? ( 1.0 - u0.x ) / du.x : du.x < 0.0 ? -u0.x / du.x : 1e30,
		du.y > 0.0 ? ( 1.0 - u0.y ) / du.y : du.y < 0.0 ? -u0.y / du.y : 1e30 );
	float L = clamp( min( min( ex.x, ex.y ), min( len, 1e7 ) ), 0.0, 1e7 );
	float H = volAirGround.y > 0.0 ? 1.0 / volAirGround.y : 1e9;
	float Lt = min( L, 40.0 * H / max( abs( s ), 1e-9 ) ), xs = s < 0.0 ? L - Lt : 0.0;	// the weight's live span
	float b = s * Lt / H, bp = b / 3.0, E = 1.0 - exp( -bp );
	float a[6], t[6], m = -1e30;
	for ( int q = 0; q < 6; q++ ) {
		float f = ( float( q ) + 0.5 ) / 6.0;
		t[q] = abs( bp ) < 1e-3 ? f : -log( 1.0 - f * E ) / bp;
		a[q] = wwAirTerrH( p0 + dirV * ( ( xs + t[q] * Lt ) * k ) ) / H - ( b - bp ) * t[q];
		m = max( m, a[q] );
	}
	// ln of the mean of exp(-c t) over t in 0..1 for c = b and bp: the importance weight's normalisers
	float lf[2];
	for ( int i = 0; i < 2; i++ ) {
		float c = i == 0 ? b : bp;
		lf[i] = abs( c ) < 1e-3 ? -0.5 * c : c > 0.0 ? log( ( 1.0 - exp( -c ) ) / c ) : -c + log( ( 1.0 - exp( c ) ) / -c );
	}
	float sum = 0.0;
	for ( int q = 0; q < 6; q++ )
		sum += exp( a[q] - m );
	float zg = H * ( m + log( sum / 6.0 ) + lf[1] - lf[0] );
	return float4( volAirGround.x + zg - volAirTerrRef, volAirGround.yzw );
}

// the column of air between heights lo < hi per unit sigma0, the surfaces' copy of the pass's medium, its ground layer
// anchored by `ground` (wwAirGroundAt)
float wwAirCol( float lo, float hi, float4 ground )
{
	return wwAirColLaw( lo, hi, volAirMed, volAirLayer, ground );
}
Texture2D<float4> volAirRainMap : register(t29, space0); SamplerState _volAirRainMap_sampler : register(s29, space0);	// CLOUD1's cloud map (R: the Low band's coverage)

// the integral of the height profile p(h) from 0 to h (km below the base; above it the fade, flat past the fade's top)
float wwAirRainP( float h )
{
	if ( h >= 0.0 )
		return h + volAirRain.y * ( 1.0 - exp( -h * volAirRain.z ) ) / volAirRain.z;
	float hc = max( h, -1.0 / volAirRain.w );
	return ( 1.0 + volAirRain.y ) * ( hc + 0.5 * hc * hc * volAirRain.w );
}

// the rain fog's optical depth between view points pA and pB
float wwAirRainTau( float3 pA, float3 pB )
{
	if ( volAirRain.x <= 0.0 )
		return 0.0;
	float3 qA = ( mul(float4( pA, 1.0 ), volAirRainXf) ).xyz;
	float3 dq = ( mul(float4( pB, 1.0 ), volAirRainXf) ).xyz - qA;
	float hTop = -1.0 / volAirRain.w;	// the fade's top: no fog above it
	float t0 = 0.0, t1 = 1.0;
	if ( abs( dq.z ) > 1e-9 ) {
		float tc = ( hTop - qA.z ) / dq.z;
		if ( dq.z > 0.0 )
			t0 = max( t0, tc );
		else
			t1 = min( t1, tc );
	} else if ( qA.z <= hTop ) {
		return 0.0;
	}
	if ( t1 <= t0 )
		return 0.0;
	int n = int( volAirRainFar.z );
	float w = ( t1 - t0 ) / float( n );
	float dh = dq.z * w;	// every sub-segment's height range (km)
	bool level = abs( dh ) <= 1e-4;
	float Pa = level ? 0.0 : wwAirRainP( qA.z + dq.z * t0 );	// the profile's integral at the sub-segment's start
	float sum = 0.0;
	for ( int k = 0; k < n; k++ ) {
		float3 qm = qA + dq * ( t0 + ( float( k ) + 0.5 ) * w );
		float2 e = abs( qm.xy - 0.5 ) * 2.0;
		float c = lerp( volAirRainMap.SampleLevel( _volAirRainMap_sampler, qm.xy, 0.0 ).r, volAirRainFar.x, smoothstep( 1.0 - volAirRainFar.y, 1.0, max( e.x, e.y ) ) );
		float p;
		if ( level ) {
			p = qm.z >= 0.0 ? 1.0 + volAirRain.y * exp( -qm.z * volAirRain.z ) : ( 1.0 + volAirRain.y ) * max( 1.0 + qm.z * volAirRain.w, 0.0 );
		} else {
			float Pb = wwAirRainP( qA.z + dq.z * ( t0 + float( k + 1 ) * w ) );
			p = ( Pb - Pa ) / dh;
			Pa = Pb;
		}
		sum += c * p;
	}
	return volAirRain.x * length( pB - pA ) * fogDistScale * w * sum;
}

// the records' haze colour at distance d, height z, view direction dirView (cell_volair.frag airEnvColour), unit luminance
float3 wwAirRecordColour( float d, float z, float3 dirView, out bool ok );
#ifdef WW_SUNSHADOW
float wwAirTailShadow( float3 dirView, float t0, float t1, float tau );	// ww_sunshadow.glsl (lane SHADOWPAR1)
#endif

/* lane FARGRID1: with the far grid on, one volume holds both grids: the near grid in layers 0..n-1 over its share of
 * the width and height, the far grid after it over its own share. (L, T) of one grid at slice coordinate f (0 at its
 * near edge), the bilinear kept inside the grid's share (its own clamp to edge). */
float4 wwGridAt( float2 uv, float2 share, float base, float n, float f )
{
	float2 lo = 0.5 / volPackDims.xy;
	float2 st = clamp( uv * share, lo, share - lo );
	float4 v;
	if ( f < 1.0 ) {
		v = volTex.Sample( _volTex_sampler, float3( st, ( base + 0.5 ) / volPackDims.z ) );
		v = float4( v.rgb * max( f, 0.0 ), 1.0 - ( 1.0 - v.a ) * max( f, 0.0 ) );
	} else {
		v = volTex.Sample( _volTex_sampler, float3( st, ( base + min( f, n ) - 0.5 ) / volPackDims.z ) );
	}
	return float4( max( v.rgb, (float3)( 0.0 ) ), clamp( v.a, 0.0, 1.0 ) );
}

/* the closed-form tail's J: the light per unit (1 - T) of the grid's last slice, `last` its (L, T) at the end, `prev` one
 * slice nearer. MODULES1 G2: in thin air (the top's falloff) the last slice's dT falls under the volume's precision and
 * the tail went black; there the whole column's light per unit (1 - T) stands in (the red, ground.z 0: none, as before) */
/* MODULES1 R7-4 (bungo 11-10, level strips in the fog from above): the volume keeps T in 16F, steps of 4.9e-4 under 1.0,
 * and one slice's dT is 2 to 30 steps wide: dL / dT of the last slice alone stepped by 10 to 50 % from one row of rays
 * to the next (notes/modules1/r7/t/strips_model.py). The source now comes from a WINDOW of the grid's end, `prev` read
 * where the column's depth is 1/16 less (wwAirTailF), `last` the whole column:
 *   the window's dL / dT; where it holds under 64 steps of T (air thinning toward the grid's end), the whole column's;
 *   a thin column (depth <= 1/8: the window is the grid's far half) returns the window's dL alone and wwAirThinJ
 *   divides it by the LAW's depth of that half, no 16F step in it.
 * fogRed 8 (red WW_CELL_AIR_TAILJ_RED=1): the last slice's dL / dT, as before */
float3 wwAirTailJ( float4 last, float4 prev )
{
	float dT = prev.a - last.a;
	if ( ( fogRed & 8 ) != 0 ) {
		if ( dT > 1e-7 )
			return max( ( last.rgb - prev.rgb ) / dT, (float3)( 0.0 ) );
		return volAirGround.z > 0.0 && last.a < 0.9999 ? max( last.rgb, (float3)( 0.0 ) ) / ( 1.0 - last.a ) : (float3)( 0.0 );
	}
	float3 dL = max( last.rgb - prev.rgb, (float3)( 0.0 ) );
	if ( -log( max( last.a, 1e-6 ) ) <= 0.125 )
		return dL / max( prev.a, 1e-6 );
	float m = 1.0 - last.a;
	float3 Jc = m > 1e-7 ? max( last.rgb, (float3)( 0.0 ) ) / m : (float3)( 0.0 );
	return lerp( Jc, dT > 1e-7 ? dL / dT : (float3)( 0.0 ), clamp( dT * 64.0 / max( last.a, 1e-6 ) - 1.0, 0.0, 1.0 ) );
}

// R7-4: the share of the grid's reach the window covers, for a column of transmittance T: 1/16 of its depth, at most half
float wwAirTailW( float T )
{
	return min( 0.0625 / max( -log( max( T, 1e-6 ) ), 1e-9 ), 0.5 );
}

// R7-4: the slice coordinate of the window's start in a grid of n slices over ln(far / near) = lnSpan; the red: n - 1
float wwAirTailF( float T, float lnSpan, float n )
{
	if ( ( fogRed & 8 ) != 0 )
		return n - 1.0;
	return clamp( n * ( 1.0 + log( 1.0 - wwAirTailW( T ) ) / lnSpan ), 1.0, n - 1.0 );
}

/* MODULES1 10c (a camera far above the fog): a column whose whole grid span lies in the top's falloff is too thin for
 * the volume's T (16F steps by 4.9e-4 under 1.0: it read 1.0 or 0.9995 for an optical depth of 2e-6), so wwAirTailJ had
 * no source, or one 2000 times too small, and the tail dimmed the land far below with no light of its own. There the
 * grid's light over the optical depth of the same span by the medium's law (the one the froxels march, wwAirCol) is
 * the source: v = the grid's (L, T) at its end `far`, z0 the ray's height there. Under an optical depth of 0.02 only
 * (40 steps of T: above it the volume's own T is exact enough). volAirFlags 32768; red WW_CELL_AIR_THINSRC_RED=1 */
float3 wwAirThinJ( float3 J, float4 v, float z0, float far )
{
	float zc = fogView.w;
	bool thin = ( fogRed & 8 ) == 0 && -log( max( v.a, 1e-6 ) ) <= 0.125;
	if ( !thin && ( or( or( ( volAirFlags & 32768 ) == 0, volAirGround.z <= 0.0 ), min( zc, z0 ) < volAirMed.w ) ) )
		return J;
	// the law's depth of the whole reach (a level ray: the density at the camera's height), one call for both rules
	float z1 = abs( z0 - zc ) < 0.5 ? zc + 1.0 : z0;
	float k = volAirMed.x * far / abs( z1 - zc );
	float od = k * wwAirCol( min( zc, z1 ), max( zc, z1 ), volAirGround );
	if ( thin ) {
		/* MODULES1 R7-4: a thin column (the same test as wwAirTailJ's; J = its far half's dL over the T at the half's
		 * start). od, b = the law's depth of the whole reach and of its far half; the law is scaled so its whole-reach
		 * 1 - T lies within one 16F step of the stored one (zones, rain: the stored depth rules where they differ by
		 * more) */
		float zh = 0.5 * ( zc + z1 );
		float b = k * wwAirCol( min( zh, z1 ), max( zh, z1 ), volAirGround );
		float xa = 1.0 - exp( -od ), m = 1.0 - v.a;
		b *= clamp( xa, m - 4.8828125e-4, m + 4.8828125e-4 ) / max( xa, 1e-9 );
		return b > 1e-7 ? J / ( b * ( 1.0 - 0.5 * b ) ) : (float3)( 0.0 );
	}
	return od > 1e-7 && od < 0.02 ? max( v.rgb, (float3)( 0.0 ) ) / ( od * ( 1.0 - 0.5 * od ) ) : J;
}

// with the far grid: `b` its (L, T) at the end, `a` the near grid's, `whole` the two together. R7-4: the window runs over the
// two grids as one column (its start in either); the red: the far grid's last slice
float3 wwGridTailJ( float2 uv, float4 a, float4 b, float4 whole )
{
	float nN = volPackDims.w;
	bool red = ( fogRed & 8 ) != 0;
	float db = volFar.x * exp( volFar.y ) * ( 1.0 - wwAirTailW( whole.a ) );
	bool inFar = or( red, db > volFar.x );
	float f = red ? volFar.z - 1.0 : inFar ? min( log( db / volFar.x ) / volFar.y * volFar.z, volFar.z - 1.0 )
		: log( db / volSlices.x ) / volSlices.y * nN;
	float4 p = wwGridAt( uv, inFar ? volPack.zw : volPack.xy, inFar ? nN : 0.0, inFar ? volFar.z : nN, f );
	if ( red )
		return wwAirTailJ( b, p );
	return wwAirTailJ( whole, inFar ? float4( a.rgb + a.a * p.rgb, a.a * p.a ) : p );
}

// near grid -> far grid -> closed-form tail: (L, T) at distance d (game units) along posView
float4 wwVolAirAtFar( float d, float2 uv, float3 posView )
{
	float nN = volPackDims.w;
	if ( d <= volSlices.x )
		return float4( 0.0, 0.0, 0.0, 1.0 );
	float4 v = wwGridAt( uv, volPack.xy, 0.0, nN, log( d / volSlices.x ) / volSlices.y * nN );
	if ( d <= volFar.x )
		return v;
	float fF = log( d / volFar.x ) / volFar.y * volFar.z;
	float4 w = wwGridAt( uv, volPack.zw, nN, volFar.z, fF );
	float4 o = float4( v.rgb + v.a * w.rgb, v.a * w.a );
	if ( or( or( fF <= volFar.z, ( volAirFlags & 2 ) == 0 ), volAirMed.x <= 0.0 ) )
		return o;
	// the closed-form tail past the far grid (wwVolAirAt's, from the far grid's last slice)
	float far = volFar.x * exp( volFar.y );
	float3 J = wwGridTailJ( uv, v, w, o );
	float z1 = dot( fogView.xyz, posView ) + fogView.w;
	float zc = fogView.w;
	float z0 = zc + ( z1 - zc ) * ( far / d );
	J = wwAirThinJ( J, o, z0, far );	// MODULES1 10c
	float len = d - far;
	float dz = z1 - z0;
	float4 gT = wwAirGroundRay( posView * ( far / d ), posView / max( length( posView ), 1e-12 ), len, dz / max( len, 1e-6 ) );	// FOGTERRAIN3: along the tail
	float tau = abs( dz ) < 0.5 ? volAirMed.x * len * wwAirCol( z0, z0 + 1.0, gT ) : volAirMed.x * len * wwAirCol( min( z0, z1 ), max( z0, z1 ), gT ) / abs( dz );
	float Tt = exp( -( tau + wwAirRainTau( posView * ( far / d ), posView ) ) );	// lane RAINFOG2: + the rain fog
	if ( ( volAirFlags & 4 ) != 0 ) {
		float3 dirV = posView / max( length( posView ), 1e-12 );
		bool ok0, ok1;
		float3 c0 = wwAirRecordColour( far, z0, dirV, ok0 );
		float3 c1 = wwAirRecordColour( 0.5 * ( far + d ), 0.5 * ( z0 + z1 ), dirV, ok1 );
		if ( ok0 && ok1 )
			J *= c1 / max( c0, (float3)( 1e-6 ) );
	}
	return float4( o.rgb + o.a * J * ( 1.0 - Tt ), o.a * Tt );
}

/* (L, T) of the air between the eye and posView. Past the froxels' far edge (bungo 10-14, the far tail: far
 * mountains and the far LOD) the same medium continues in closed form: T x= exp(-sigma0 x the column along the
 * segment / its slope), L += T_edge x J x (1 - T_tail), J the light the last slice's air scattered (its L and T steps,
 * so the tail starts continuous with the froxels), carried to the tail's middle in the records' colour. A ray that
 * climbs meets thinner air: a peak keeps more contrast than its foot. */
/* FOGTRICUBIC1 (bungo 10-21): a cubic B-spline read of the froxels, from linear fetches placed between texels (the
 * fractional offsets carry the weights). Smooths the grid's blocks; xyOnly keeps the slice exact (the far tail's and the
 * sky's step J needs the two slices themselves). Flag 32 off: the plain trilinear read. */
float4 wwVolTex( float3 p, bool xyOnly )
{
	if ( ( volAirFlags & 32 ) == 0 )
		return volTex.Sample( _volTex_sampler, p );
	float3 n = float3( textureSize( volTex, 0 ) );
	float3 c = p * n - 0.5;
	float3 i = floor( c );
	float3 f = c - i;
	float3 w0 = ( ( -f + 3.0 ) * f - 3.0 ) * f + 1.0;	// x 6: (1 - f)^3
	float3 w1 = ( 3.0 * f - 6.0 ) * f * f + 4.0;
	float3 w2 = ( ( -3.0 * f + 3.0 ) * f + 3.0 ) * f + 1.0;
	float3 w3 = f * f * f;
	float3 g0 = w0 + w1, g1 = w2 + w3;
	float3 h0 = ( i - 0.5 + w1 / g0 ) / n, h1 = ( i + 1.5 + w3 / g1 ) / n;
	g0 /= 6.0;
	g1 /= 6.0;
	if ( xyOnly ) {
		return g0.y * ( g0.x * volTex.Sample( _volTex_sampler, float3( h0.x, h0.y, p.z ) ) + g1.x * volTex.Sample( _volTex_sampler, float3( h1.x, h0.y, p.z ) ) )
			+ g1.y * ( g0.x * volTex.Sample( _volTex_sampler, float3( h0.x, h1.y, p.z ) ) + g1.x * volTex.Sample( _volTex_sampler, float3( h1.x, h1.y, p.z ) ) );
	}
	float4 a = g0.y * ( g0.x * volTex.Sample( _volTex_sampler, float3( h0.x, h0.y, h0.z ) ) + g1.x * volTex.Sample( _volTex_sampler, float3( h1.x, h0.y, h0.z ) ) )
		+ g1.y * ( g0.x * volTex.Sample( _volTex_sampler, float3( h0.x, h1.y, h0.z ) ) + g1.x * volTex.Sample( _volTex_sampler, float3( h1.x, h1.y, h0.z ) ) );
	float4 b = g0.y * ( g0.x * volTex.Sample( _volTex_sampler, float3( h0.x, h0.y, h1.z ) ) + g1.x * volTex.Sample( _volTex_sampler, float3( h1.x, h0.y, h1.z ) ) )
		+ g1.y * ( g0.x * volTex.Sample( _volTex_sampler, float3( h0.x, h1.y, h1.z ) ) + g1.x * volTex.Sample( _volTex_sampler, float3( h1.x, h1.y, h1.z ) ) );
	return g0.z * a + g1.z * b;
}

// R7-4: the near grid's tail source (no far grid): `v` its (L, T) at the end, the window's start read at wwAirTailF
float3 wwAirTailNearJ( float2 uv, float4 v )
{
	return wwAirTailJ( v, wwVolTex( float3( uv, ( wwAirTailF( v.a, volSlices.y, volSlices.z ) - 0.5 ) / volSlices.z ), true ) );
}

/* MODULES1 10c: the lit-decal programs (WW_FX_DECALLIT) take the one-tail shape without WW_CELL_FOGLEAK too: the
 * old shape inlined a second whole tail (wwVolAirAtFar) and fo4_effectdecalcsm passed the instruction limit with the
 * leak red set. Same law there: the far grid's tail takes no sun shadow (WW_AIR_FARTAILSH false). Offline red
 * WW_AIR_TWOTAILS = the old shape (it must refuse) */
#ifdef WW_CELL_FOGLEAK
#define WW_AIR_ONETAIL
#define WW_AIR_FARTAILSH ( ( volAirFlags & 256 ) != 0 )
#elif defined( WW_FX_DECALLIT ) && !defined( WW_AIR_TWOTAILS )
#define WW_AIR_ONETAIL
#define WW_AIR_FARTAILSH false
#endif
#ifdef WW_AIR_ONETAIL
/* lane FOGLEAK1 (fix 4, the loader's WW_CELL_FOGLEAK only): the far grid's closed-form tail took no sun shadow
 * (wwVolAirAtFar), the near grid's did. Both grids' heads below end where their tail starts, and ONE tail takes the
 * sun's shadow for either (one wwAirTailShadow call site: a second inlined copy would tip the programs at the limit).
 * volAirFlags 256 off (red WW_CELL_AIR_RED=nofartailshadow): the far grid's tail unshadowed, as before */
bool wwVolAirFarHead( float d, float2 uv, float3 posView, out float4 o, out float3 J, out float far )
{
	J = (float3)( 0.0 );
	far = 0.0;
	float nN = volPackDims.w;
	if ( d <= volSlices.x ) {
		o = float4( 0.0, 0.0, 0.0, 1.0 );
		return false;
	}
	float4 v = wwGridAt( uv, volPack.xy, 0.0, nN, log( d / volSlices.x ) / volSlices.y * nN );
	if ( d <= volFar.x ) {
		o = v;
		return false;
	}
	float fF = log( d / volFar.x ) / volFar.y * volFar.z;
	float4 w = wwGridAt( uv, volPack.zw, nN, volFar.z, fF );
	o = float4( v.rgb + v.a * w.rgb, v.a * w.a );
	if ( or( or( fF <= volFar.z, ( volAirFlags & 2 ) == 0 ), volAirMed.x <= 0.0 ) )
		return false;
	far = volFar.x * exp( volFar.y );
	J = wwGridTailJ( uv, v, w, o );
	return true;
}

bool wwVolAirNearHead( float d, float2 uv, out float4 v, out float3 J, out float far )
{
	J = (float3)( 0.0 );
	far = 0.0;
	if ( d <= volSlices.x ) {
		v = float4( 0.0, 0.0, 0.0, 1.0 );
		return false;
	}
	float f = log( d / volSlices.x ) / volSlices.y * volSlices.z;
	if ( f < 1.0 ) {
		v = wwVolTex( float3( uv, 0.5 / volSlices.z ), true );
		v = float4( v.rgb * f, 1.0 - ( 1.0 - v.a ) * f );
	} else {
		v = wwVolTex( float3( uv, ( min( f, volSlices.z ) - 0.5 ) / volSlices.z ), f > volSlices.z );
	}
	v = float4( max( v.rgb, (float3)( 0.0 ) ), clamp( v.a, 0.0, 1.0 ) );
	if ( or( or( f <= volSlices.z, ( volAirFlags & 2 ) == 0 ), volAirMed.x <= 0.0 ) )
		return false;
	far = volSlices.x * exp( volSlices.y );
	J = wwAirTailNearJ( uv, v );
	return true;
}

float4 wwVolAirAtUv( float3 posView, float2 uv )	// lane VOLPART1: uv = the column (a particle's probe: its centre's)
{
	float d = length( posView ) * fogDistScale;
	if ( volFarClamp != 0 && !wwAirFree )
		d = min( d, volTile.Sample( _volTile_sampler, uv ).r );
	bool farGrid = volFar.w > 0.5;
	float4 v;
	float3 J;
	float far;
	bool tail = farGrid ? wwVolAirFarHead( d, uv, posView, v, J, far ) : wwVolAirNearHead( d, uv, v, J, far );
	if ( !tail )
		return v;
	float z1 = dot( fogView.xyz, posView ) + fogView.w;
	float zc = fogView.w;
	float z0 = zc + ( z1 - zc ) * ( far / d );
	J = wwAirThinJ( J, v, z0, far );	// MODULES1 10c
	float len = d - far;
	float dz = z1 - z0;
	float4 gT = wwAirGroundRay( posView * ( far / d ), posView / max( length( posView ), 1e-12 ), len, dz / max( len, 1e-6 ) );	// FOGTERRAIN3: along the tail
	float tau = abs( dz ) < 0.5 ? volAirMed.x * len * wwAirCol( z0, z0 + 1.0, gT ) : volAirMed.x * len * wwAirCol( min( z0, z1 ), max( z0, z1 ), gT ) / abs( dz );
	float Tt = exp( -( tau + wwAirRainTau( posView * ( far / d ), posView ) ) );	// lane RAINFOG2: + the rain fog
#ifdef WW_SUNSHADOW
	if ( or( !farGrid, WW_AIR_FARTAILSH ) )
		J *= wwAirTailShadow( posView / max( length( posView ), 1e-12 ), far, d, tau );
#endif
	if ( ( volAirFlags & 4 ) != 0 ) {
		float3 dirV = posView / max( length( posView ), 1e-12 );
		bool ok0, ok1;
		float3 c0 = wwAirRecordColour( far, z0, dirV, ok0 );
		float3 c1 = wwAirRecordColour( 0.5 * ( far + d ), 0.5 * ( z0 + z1 ), dirV, ok1 );
		if ( ok0 && ok1 )
			J *= c1 / max( c0, (float3)( 1e-6 ) );
	}
	return float4( v.rgb + v.a * J * ( 1.0 - Tt ), v.a * Tt );
}
#else
float4 wwVolAirAtUv( float3 posView, float2 uv )	// lane VOLPART1: uv = the column (a particle's probe: its centre's)
{
	float d = length( posView ) * fogDistScale;
	/* 10-16: no farther than the column's farthest opaque surface (the obscurance pass's depth). At a thin sliver's
	 * edge the interpolated position can lie far past the depth the same fragment writes (a trim at 596 shaded as 763):
	 * that read the froxels the depth skip had left empty, and an air no surface there sits behind. */
	if ( volFarClamp != 0 && !wwAirFree )
		d = min( d, volTile.Sample( _volTile_sampler, uv ).r );
	if ( volFar.w > 0.5 )
		return wwVolAirAtFar( d, uv, posView );
	if ( d <= volSlices.x )
		return float4( 0.0, 0.0, 0.0, 1.0 );
	float f = log( d / volSlices.x ) / volSlices.y * volSlices.z;
	float4 v;
	if ( f < 1.0 ) {
		v = wwVolTex( float3( uv, 0.5 / volSlices.z ), true );
		v = float4( v.rgb * f, 1.0 - ( 1.0 - v.a ) * f );
	} else {
		v = wwVolTex( float3( uv, ( min( f, volSlices.z ) - 0.5 ) / volSlices.z ), f > volSlices.z );
	}
	v = float4( max( v.rgb, (float3)( 0.0 ) ), clamp( v.a, 0.0, 1.0 ) );
	if ( or( or( f <= volSlices.z, ( volAirFlags & 2 ) == 0 ), volAirMed.x <= 0.0 ) )
		return v;
	// the far tail
	float far = volSlices.x * exp( volSlices.y );
	float3 J = wwAirTailNearJ( uv, v );
	float z1 = dot( fogView.xyz, posView ) + fogView.w;
	float zc = fogView.w;
	float z0 = zc + ( z1 - zc ) * ( far / d );
	J = wwAirThinJ( J, v, z0, far );	// MODULES1 10c
	float len = d - far;
	float dz = z1 - z0;
	float4 gT = wwAirGroundRay( posView * ( far / d ), posView / max( length( posView ), 1e-12 ), len, dz / max( len, 1e-6 ) );	// FOGTERRAIN3: along the tail
	float tau = abs( dz ) < 0.5 ? volAirMed.x * len * wwAirCol( z0, z0 + 1.0, gT ) : volAirMed.x * len * wwAirCol( min( z0, z1 ), max( z0, z1 ), gT ) / abs( dz );
	float Tt = exp( -( tau + wwAirRainTau( posView * ( far / d ), posView ) ) );	// lane RAINFOG2: + the rain fog
#ifdef WW_SUNSHADOW
	J *= wwAirTailShadow( posView / max( length( posView ), 1e-12 ), far, d, tau );
#endif
	if ( ( volAirFlags & 4 ) != 0 ) {
		float3 dirV = posView / max( length( posView ), 1e-12 );
		bool ok0, ok1;
		float3 c0 = wwAirRecordColour( far, z0, dirV, ok0 );
		float3 c1 = wwAirRecordColour( 0.5 * ( far + d ), 0.5 * ( z0 + z1 ), dirV, ok1 );
		if ( ok0 && ok1 )
			J *= c1 / max( c0, (float3)( 1e-6 ) );
	}
	return float4( v.rgb + v.a * J * ( 1.0 - Tt ), v.a * Tt );
}
#endif

/* (L, T) of the air between the eye and posView, at this fragment's column. Part 18 (volAirFlags 512): RDR2's single
 * dithered tap (Bauer 2019 slides 48, 57; notes/modules1/research/fog_temporal.md): the read moves up to half a froxel in
 * x, y and slice by a per-pixel gradient noise that turns each frame (bits 10-13), and TAA averages the blocks away */
float4 wwVolAirAt( float3 posView )
{
	float2 uv = ( gl_FragCoord.xy - volRect.xy ) * volRect.zw;
	if ( ( volAirFlags & 512 ) != 0 ) {
		float2 q = gl_FragCoord.xy + 5.588238 * float( ( volAirFlags >> 10 ) & 15 );
		float3 n = frac( 52.9829189 * frac( float3( dot( q, float2( 0.06711056, 0.00583715 ) ), dot( q + float2( 47.0, 17.0 ), float2( 0.06711056, 0.00583715 ) ),
			dot( q + float2( 13.0, 71.0 ), float2( 0.06711056, 0.00583715 ) ) ) ) ) - 0.5;
		uv += n.xy / ( volPackDims.xy * volPack.xy );
		posView *= exp( n.z * volSlices.y / volPackDims.w );
	}
	return wwVolAirAtUv( posView, uv );
}

/* lane SKYSEA1 (the horizon band): the air's column along a sky ray over the CURVED ground, as the physical sky's planet
 * has it (radius 6371 km x 70 u a metre). On flat ground a ray within a few degrees of level crossed the low air for
 * hundreds of km (column / sine), so from high up every row between the far land and eye level read the air's own colour
 * (T 0) while the row above read none: a bright band at the horizon. Over a curved ground the ray's height is
 * z0 + s x + x^2 / 2R; its lowest height a = z0 - s^2 R / 2, and with u = sqrt(z - a) the path is sqrt(R / 2) 2 du, so
 * each leg is an integral of the same density law (Unreal's height fog: opacity 1 - exp(-sigma0 x the column)) over u.
 * A downward ray that clears the ground (a above it) dips and climbs out again. volAirFlags 131072; red
 * WW_CELL_AIR_CURVE_RED=1: the flat ground. */
static const float kAirEarthU = 6371000.0 * 70.0;

float wwAirCurvedLeg( float a, float u0, float u1, float4 g )
{
	if ( u1 <= u0 )
		return 0.0;
	float h = ( u1 - u0 ) / 24.0, c = 0.0;
	for ( int i = 0; i < 24; i++ ) {
		float u = u0 + ( float( i ) + 0.5 ) * h;
		c += wwAirRhoLaw( a + u * u, volAirMed, volAirLayer, g );
	}
	return c * h;
}

// sigma0 x the column from height z0 at elevation sine s to the air's end `end` or the ground gz (hit)
float wwAirCurvedTau( float z0, float s, float gz, float end, float4 g, out bool hit )
{
	float k = sqrt( 0.5 * kAirEarthU ), a = z0 - 0.5 * s * s * kAirEarthU;
	float uz = sqrt( max( min( z0, end ) - a, 0.0 ) ), ue = sqrt( max( end - a, 0.0 ) );
	hit = s < 0.0 && a <= gz;
	float c = s >= 0.0 ? wwAirCurvedLeg( a, s * k, ue, g )
		: hit ? wwAirCurvedLeg( a, sqrt( max( gz - a, 0.0 ) ), uz, g )
		: wwAirCurvedLeg( a, 0.0, uz, g ) + wwAirCurvedLeg( a, 0.0, ue, g );
	return volAirMed.x * 2.0 * k * c;
}

/* (L, T) of the whole air along a sky ray (bungo 10-14: the far tower stood out against an unfogged sky). The sky is a
 * surface at infinite distance: the froxels to their far edge, then the far tail of wwVolAirAt run to the end of the
 * air -- climbing, to the air's top (the column over the sine of the elevation: finite); level or down, the air never
 * ends (T 0, the fog's own colour). It is the limit of wwVolAirAt as the distance grows, so far geometry and the sky
 * behind it take the same air. The tail's colour is read halfway to where the ray leaves the air. */
float4 wwVolAirSky( float3 dirView )
{
	float2 uv = ( gl_FragCoord.xy - volRect.xy ) * volRect.zw;
	float n = volSlices.z;
	float4 v;
	float far;
	float3 J;
	if ( volFar.w > 0.5 ) {
		// lane FARGRID1: the near grid's end, then the far grid's, the tail from the far grid's last slice
		float4 a = wwGridAt( uv, volPack.xy, 0.0, volPackDims.w, volPackDims.w );
		float4 b = wwGridAt( uv, volPack.zw, volPackDims.w, volFar.z, volFar.z );
		v = float4( a.rgb + a.a * b.rgb, a.a * b.a );
		if ( or( ( volAirFlags & 2 ) == 0, volAirMed.x <= 0.0 ) )
			return v;
		far = volFar.x * exp( volFar.y );
		J = wwGridTailJ( uv, a, b, v );
	} else {
	v = wwVolTex( float3( uv, ( n - 0.5 ) / n ), true );
	v = float4( max( v.rgb, (float3)( 0.0 ) ), clamp( v.a, 0.0, 1.0 ) );
	if ( or( ( volAirFlags & 2 ) == 0, volAirMed.x <= 0.0 ) )
		return v;
	far = volSlices.x * exp( volSlices.y );
	J = wwAirTailNearJ( uv, v );
	}
	float s = dot( fogView.xyz, dirView ) / max( fogDistScale, 1e-12 );	// the sine of the elevation
	/* MODULES1 R7 item 6 (bungo b61: "white horizontal line", 1 px at eye level from high up): a level ray (|sine| <=
	 * 1e-4, a sixth of a pixel row) took the branch below that knows only "above the top = no air" (T 1), while the
	 * top's falloff (G2) holds air there: with the eye above the top that one row let the sky through the grids' T
	 * (0.44 in the flight) between two opaque rows. The level ray ends like its neighbours: the sine held at +-1e-4.
	 * fogRed 16 = the red WW_CELL_AIR_LEVELRAY_RED=1. */
	if ( ( fogRed & 16 ) == 0 && abs( s ) <= 1e-4 )
		s = s >= 0.0 ? 1.0001e-4 : -1.0001e-4;
	float z0 = fogView.w + s * far;
	J = wwAirThinJ( J, v, z0, far );	// MODULES1 10c
	float top = volAirMed.w;
	float Tt, dEnd;
	/* MODULES1 M5 (bungo 11-10, horizon_band: a light-blue stripe over the far hills from high up): a downward ray ends
	 * where it meets the ground (the ground layer's anchor along it), as the drawn land under it does in the surfaces'
	 * tail: the column from the ground to the ray's height at the grid's end over the sine. It was T 0 (the air never
	 * ends) under the top and T 1 above it: rows over the hills switched from clear to the fog's colour. Bit 65536;
	 * red WW_CELL_AIR_GROUNDEND_RED=1. */
	float4 gD = volAirGround;
	bool groundEnd = ( volAirFlags & 65536 ) != 0 && s < -1e-4;
	if ( groundEnd ) {
		gD = wwAirGroundRay( dirView * ( far / max( fogDistScale, 1e-12 ) ), dirView, 1e7, s );
		groundEnd = z0 > gD.x;
	}
	bool curve = ( volAirFlags & 131072 ) != 0 && abs( s ) < 0.3;	// lane SKYSEA1: the curved ground (flat past 0.2..0.3)
	if ( groundEnd ) {
		dEnd = far + min( ( z0 - gD.x ) / -s, 1e7 );
		float tau = volAirMed.x * wwAirCol( gD.x, z0, gD ) / -s;
		if ( curve ) {
			bool hit;
			float tc = wwAirCurvedTau( z0, s, gD.x, wwAirEnd( volAirMed, gD ), gD, hit );
			tau = lerp( tc, tau, smoothstep( 0.2, 0.3, -s ) );
			if ( !hit )
				dEnd = far + min( -2.0 * s * kAirEarthU, 1e7 );
		}
		Tt = exp( -tau
			- wwAirRainTau( dirView * ( far / max( fogDistScale, 1e-12 ) ), dirView * ( dEnd / max( fogDistScale, 1e-12 ) ) ) );
	} else if ( s > 1e-4 ) {
		float end = wwAirEnd( volAirMed, volAirGround );	// G2: the climb runs through the top's falloff
		dEnd = far + min( max( end - z0, 0.0 ) / s, 1e7 );
		// FOGTERRAIN3: the ground along the climb, inside the grid's window (wwAirGroundRay)
		float4 gU = wwAirGroundRay( dirView * ( far / max( fogDistScale, 1e-12 ) ), dirView, dEnd - far, s );
		float tau = volAirMed.x * wwAirCol( z0, end, gU ) / s;
		if ( curve ) {
			bool hit;
			tau = lerp( wwAirCurvedTau( z0, s, -1e30, end, gU, hit ), tau, smoothstep( 0.2, 0.3, s ) );
		}
		Tt = exp( -tau
			- wwAirRainTau( dirView * ( far / max( fogDistScale, 1e-12 ) ), dirView * ( ( far + 1e7 ) / max( fogDistScale, 1e-12 ) ) ) );	// lane RAINFOG2: + the rain fog, to where the ray leaves it
	} else {
		/* MODULES1 item 18: a ray still above the air's top where the grid ends (a camera above the air looking down)
		 * has met no air yet: its tail is not opaque black; the sky behind it (the planet's ground through the haze
		 * layer) shows. Bit 16384; red WW_CELL_AIR_ABOVE_RED=1: every downward tail opaque, as before */
		Tt = or( ( s >= 0.0 && z0 >= top ), ( ( volAirFlags & 16384 ) != 0 && z0 >= top ) ) ? 1.0 : 0.0;
		dEnd = far + 1e7;
	}
	if ( ( volAirFlags & 4 ) != 0 ) {
		bool ok0, ok1;
		float dm = 0.5 * ( far + dEnd );
		float3 c0 = wwAirRecordColour( far, z0, dirView, ok0 );
		float3 c1 = wwAirRecordColour( dm, z0 + s * ( dm - far ), dirView, ok1 );
		if ( ok0 && ok1 )
			J *= c1 / max( c0, (float3)( 1e-6 ) );
	}
	return float4( v.rgb + v.a * J * ( 1.0 - Tt ), v.a * Tt );
}
Texture3D<float4> volAirNoise : register(t30, space0); SamplerState _volAirNoise_sampler : register(s30, space0);
float wwAirDriftAt( float3 p )
{
	float f = 1.0 - clamp( ( length( p ) * fogDistScale - volAirDrift.y ) * volAirDrift.z, 0.0, 1.0 );
	if ( or( volAirDrift.x <= 0.0, f <= 0.0 ) )
		return 1.0;
	float4 q = float4( p, 1.0 );
	float a = volAirNoise.Sample( _volAirNoise_sampler, float3( dot( volAirDriftUv[0], q ), dot( volAirDriftUv[1], q ), dot( volAirDriftUv[2], q ) ) ).r;
	float b = volAirNoise.Sample( _volAirNoise_sampler, float3( dot( volAirDriftUv[3], q ), dot( volAirDriftUv[4], q ), dot( volAirDriftUv[5], q ) )
		+ float3( 0.37, 0.61, 0.13 ) ).r;
	return 1.0 + volAirDrift.x * f * ( a - volAirDrift.w + 0.5 * f * ( b - volAirDrift.w ) ) * ( 1.0 / 1.5 );
}
// the factor on the sun column's optical depth from p (height lo, the sun's rise mu, the air's top): the drift's mean at
// 1/6, 1/2 and 5/6 of the column, at most the noise's tile (4096 units) along the sun; red onesun: half way only
float wwAirDriftSunK( float3 p, float lo, float mu, float top )
{
	if ( volAirDrift.x <= 0.0 )
		return 1.0;
	float3 s = fogSun.xyz * ( min( max( top - lo, 0.0 ) / max( mu, 0.02 ), 4096.0 ) / max( fogDistScale, 1e-12 ) );
	// a loop on a uniform count: one copy of the noise reads (three unrolled copies broke the decal program's limit)
	int n = ( volAirDriftRed & 1 ) != 0 ? 1 : 3;
	float f0 = n == 1 ? 0.5 : 1.0 / 6.0, k = 0.0;
	for ( int i = 0; i < n; i++ )
		k += wwAirDriftAt( p + s * ( f0 + float( i ) / 3.0 ) );
	return k / float( n );
}

/* FOGSUN1 (bungo 10-14): the sun reaching a surface through the air -- the air's transmittance from the surface's
 * height to its top along the sun, closed form (the air's own sun term, cell_volair.frag airTup): in heavy fog the
 * direct sun is dimmed by the medium; the sky's share the air scatters is its in-scattering. 1 where the air is off.
 * Red nosunt: the sun not dimmed. The grazing guard mu >= 0.02 is the air's own (a flat medium has no finite column at
 * the horizon). */
float2 wwVolAirSunTD( float3 posView )
{
	if ( or( or( or( or( or( !fogOn, !volOn ), volSlices.w < 0.5 ), fogSun.w <= 0.0 ), volAirMed.x <= 0.0 ), ( volAirFlags & 1 ) == 0 ) )
		return float2( 1.0, 0.0 );
	float lo = dot( fogView.xyz, posView ) + fogView.w;
	float end = wwAirEnd( volAirMed, volAirGround );	// G2
	if ( lo >= end )
		return float2( 1.0, 0.0 );
	float mu = max( dot( fogView.xyz, fogSun.xyz ) / max( length( fogView.xyz ), 1e-12 ), 0.02 );
	float4 g = wwAirGroundAt( posView );
	/* lane FIX1101A (atmo, the fog-sun fault): flag 128 (shipped) = only the derived ground layer dims the sun. The far and
	 * near bands ARE the records' fog, and the weather's Sunlight colour is already the sun that weather gives the ground;
	 * their column to the scene's top (its tallest shape) took Foggy noon at c3 to Ts 0.026 (no shadows, no shafts).
	 * Red WW_CELL_AIR_RED=suncolumn clears the flag: the whole column, as before. cell_airmed.glsl airTupSun is the twin. */
	float c = ( volAirFlags & 128 ) != 0
		? ( volAirLayer.w > 0.0 ? volAirLayer.w * wwAirColOne( lo, volAirMed.w, g.x, g.y ) : 0.0 )
		: wwAirCol( lo, end, g );
	float tau = volAirMed.x * c * wwAirDriftSunK( posView, lo, mu, volAirMed.w ), Td = exp( -tau / mu );
	/* MODULES1 G3 (bungo 11-09 items 6, 10e: the lit fog far brighter than the sunlit land beside it): what the column
	 * takes out of the direct beam is mostly scattered forward and reaches the surface as soft light. The conservative
	 * delta-Eddington total transmission (Meador & Weaver 1980 eq. 24) with a low-sun divisor fitted to a Monte Carlo
	 * slab (notes/modules1/tools/g3_sundif_mc.py: 80 cases, worst 0.030), minus the direct beam; y = that share x mu,
	 * the irradiance on a level surface per unit of the sun. volAirGround.w = 1 + the air's phase g (0: the red
	 * WW_CELL_AIR_SUNDIF_RED=1, the direct beam alone) */
	if ( volAirGround.w <= 0.0 )
		return float2( Td, 0.0 );
	float pg = volAirGround.w - 1.0, gp = pg / ( 1.0 + pg ), tp = ( 1.0 - pg * pg ) * tau, g1 = 0.75 * ( 1.0 - gp );
	float Tt = 1.0 - ( g1 * tp + ( 0.25 * ( 2.0 - 3.0 * gp * mu ) - g1 * mu ) * ( 1.0 - exp( -tp / mu ) ) ) / ( 1.0 + g1 * tp );
	Tt /= 1.0 + 0.035 * max( pg, 0.0 ) * ( 1.0 / mu - 1.0 ) * ( 1.0 - Td );
	return float2( Td, max( Tt - Td, 0.0 ) * mu );
}

float wwVolAirSunT( float3 posView )
{
	return wwVolAirSunTD( posView ).x;
}

float3 wwFogBase( float3 preFog, float3 posView, float tAp, bool apOn );

/* linear in, linear out. Lane SKYA (ATMO2): with the aerial perspective on, the surface goes through the sky's air
 * first, then through the cell fog's residual (atmo_apfog.glsl: the combined transmittance is the min of the two, no
 * haze counted twice); one fog path for both, so no program carries the fog code twice. AP off: today's math. */
float3 wwFog( float3 preFog, float3 posView )
{
	bool apOn = wwApOn();
	float4 ap = apOn ? wwAtmoApAt( posView ) : float4( 0.0, 0.0, 0.0, 1.0 );
	if ( apOn )
		preFog = preFog * ap.a + ap.rgb;
	if ( !fogOn )
		return preFog;
	if ( volOn && volSlices.w > 0.5 ) {
		float4 a = wwVolAirAt( posView );
		if ( apOn ) {
			bool dbl = atApDouble > 0.5;
			float tr = wwApFogResidualT( ap.a, a.a, dbl );
			float3 lr = dbl ? a.rgb : wwApFogResidualL( a.rgb, a.a, tr );
			if ( fogNight.a > 0.0 )	// R7 night sky d: the two airs' light together (ap.rgb x tr + lr), their T ap.a x tr
				lr = max( wwAirNightHue( lr ), fogNight.rgb * ( 1.0 - ap.a * tr ) - ap.rgb * tr );
			return preFog * tr + lr;
		}
		if ( fogNight.a > 0.0 )
			a.rgb = max( wwAirNightHue( a.rgb ), fogNight.rgb * ( 1.0 - a.a ) );
		float3 lit = preFog * a.a + a.rgb;
		if ( volFogBlend == 1 )
			return wwFogBase( lit, posView, 1.0, false );
		if ( volFogBlend == 2 ) {	// red blendnear (gates only): weighted by the air's 1 - T, which has no start distance
			float hb;
			float3 fc;
			wwFogEval( length( posView ) * fogDistScale, dot( fogView.xyz, posView ) + fogView.w, hb, fc );
			return lerp( lit, fc, 1.0 - a.a );
		}
		return lit;
	}
	return wwFogBase( preFog, posView, ap.a, apOn );
}

float3 wwAirRecordColour( float d, float z, float3 dirView, out bool ok )
{
	float hb;
	float3 fc;
	wwFogEval( d, z, hb, fc );
	float sunW = fogSun.w > 0.0 ? pow( max( dot( dirView, fogSun.xyz ), 0.0 ), fogSunColour.w ) * fogSun.w : 0.0;
	float3 c = max( lerp( fc, fogSunColour.rgb, sunW ), (float3)( 0.0 ) );
	float lc = dot( c, float3( 0.2126, 0.7152, 0.0722 ) );
	ok = lc > 1e-8;
	return ok ? c / lc : (float3)( 1.0 );
}

// the game's fog alone; with the aerial perspective on, its alpha cut to the residual: 1 - alpha' = min(1, (1 - alpha) / Tap)
float3 wwFogBase( float3 preFog, float3 posView, float tAp, bool apOn )
{
	float d = length( posView ) * fogDistScale;
	float z = dot( fogView.xyz, posView ) + fogView.w;
	float hb;
	float3 fogCol;
	float alpha = wwFogEval( d, z, hb, fogCol );
	if ( apOn )
		alpha = 1.0 - wwApFogResidualT( tAp, 1.0 - alpha, atApDouble > 0.5 );
	float3 viewDir = posView / max( length( posView ), 1e-6 );
	float sunW = pow( max( dot( viewDir, fogSun.xyz ), 0.0 ), fogSunColour.w ) * fogSun.w;
	float3 sunlit = lerp( fogCol, fogSunColour.rgb, sunW );
	float gray = dot( preFog, (float3)( 1.0 / 3.0 ) );
	float grayW = fogUnit > 0.0 ? gray / fogUnit : gray;	// lane PHYSUNITS1: the weight on the weather's scale
	float3 resolved = alpha < fogK[2].w ? lerp( sunlit, (float3)( gray ), grayW ) : sunlit;
	return lerp( preFog, resolved, alpha );
}

// lane VOLAIR1: an effect's or a particle's linear colour through the same fog as the opaque draws (wwFog), so a
// card on a far wall fades with the wall. An additive draw takes the transmittance only: its light adds over a
// surface that already holds the in-scatter. Red fxfog (flag 16 clear) and the air off: the game's fog alone, as before.
// Lane SKYA (ATMO2): with the aerial perspective on, a lit effect takes wwFog's split; an additive one the sky's
// transmittance times the fog's residual.
float3 wwFogFx( float3 lin, float3 posView, bool additive )
{
	bool apOn = wwApOn();
	if ( !fogOn && !apOn )
		return lin;
	bool volFx = volOn && volSlices.w > 0.5 && ( volAirFlags & 16 ) != 0;
	if ( !additive && ( or( volFx, apOn ) ) )
		return max( wwFog( lin, posView ), (float3)( 0.0 ) );
	float tAp = apOn ? wwAtmoApAt( posView ).a : 1.0;
	if ( !fogOn )
		return lin * tAp;
	float hb;
	float3 fc;
	float f = wwFogEval( length( posView ) * fogDistScale, dot( fogView.xyz, posView ) + fogView.w, hb, fc );
	if ( !volFx ) {
		if ( apOn )
			return lin * tAp * wwApFogResidualT( tAp, 1.0 - f, atApDouble > 0.5 );
		return additive ? lin * ( 1.0 - f ) : lerp( lin, fc, f );
	}
	float tv = wwVolAirAt( posView ).a;
	if ( apOn )
		return lin * tAp * wwApFogResidualT( tAp, tv, atApDouble > 0.5 );
	lin *= tv;
	return volFogBlend == 1 ? lin * ( 1.0 - f ) : lin;
}

// gates only: true when the fragment must write `o` raw (no exposure, no view transform)
bool wwFogProbe( float3 posView, out float3 o )
{
	o = (float3)( 0.0 );
	if ( fogOn && volOn && ( or( volProbe == 1, volProbe == 2 ) ) ) {	// lane VOLAIR1: the air's L or T at this fragment
		float4 a = wwVolAirAt( posView );
		o = volProbe == 1 ? a.rgb : (float3)( a.a );
		return true;
	}
	if ( fogOn && volOn && volProbe == 3 ) {	// FOGSUN1: the sun's transmittance through the air to this fragment, grey
		o = (float3)( wwVolAirSunT( posView ) );
		return true;
	}
	if ( or( !fogOn, fogProbe.z < 0.5 ) )
		return false;
	if ( fogProbe.z > 8.5 ) {
		// lane VOLAIR1 (the far tail's gate), 16 bits in R (high byte) and G (low): 9 = log2(d) / 20, 10 = (z + 65536)
		// / 131072 (d to ~1e6 units, z in 2-unit steps); B = 0
		float d = length( posView ) * fogDistScale;
		float z = dot( fogView.xyz, posView ) + fogView.w;
		float x = fogProbe.z < 9.5 ? log2( max( d, 1.0 ) ) / 20.0 : ( z + 65536.0 ) / 131072.0;
		float q = floor( clamp( x, 0.0, 1.0 ) * 65535.0 + 0.5 );
		o = float3( floor( q / 256.0 ) / 255.0, mod( q, 256.0 ) / 255.0, 0.0 );
		return true;
	}
	if ( fogProbe.z > 7.5 ) {
		// lane FOG2, 8: the geometry this fragment's fog read, wide. The gate drops the pixels whose top surface
		// is not the one the position probes saw (a translucent card that serves this probe and not theirs)
		float d = length( posView ) * fogDistScale;
		float z = dot( fogView.xyz, posView ) + fogView.w;
		o = float3( d / 16384.0, 0.5 + z / 32768.0, 0.0 );
		return true;
	}
	if ( fogProbe.z > 5.5 ) {
		// lane FOG2, per fragment: 6 = (alpha, height blend, 0), 7 = the fog colour ^ (1 / 2.2)
		float d = length( posView ) * fogDistScale;
		float z = dot( fogView.xyz, posView ) + fogView.w;
		float hb;
		float3 fogCol;
		float alpha = wwFogEval( d, z, hb, fogCol );
		o = fogProbe.z < 6.5 ? float3( alpha, hb, 0.0 ) : pow( max( fogCol, (float3)( 0.0 ) ), (float3)( 1.0 / 2.2 ) );
		return true;
	}
	if ( fogProbe.z > 4.5 ) {
		// the geometry the fog reads: R = d / 4096, G = 0.5 + z / 2000
		float d = length( posView ) * fogDistScale;
		float z = dot( fogView.xyz, posView ) + fogView.w;
		o = float3( d / 4096.0, 0.5 + z / 2000.0, 0.0 );
		return true;
	}
	float hb;
	float3 fogCol;
	float alpha = wwFogEval( fogProbe.x, fogProbe.y, hb, fogCol );
	if ( fogProbe.z < 1.5 )
		o = (float3)( alpha );
	else if ( fogProbe.z < 2.5 )
		o = fogCol * 0.5;
	else
		o = (float3)( hb );
	return true;
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/air/lookdev_fog.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cell_ddgiprobe_ps.hlsl, cell_fardots_ps.hlsl, cell_fogbloom_ps.hlsl, cell_precip_gpu_gs.hlsl, cell_precip_look_gs.hlsl, cell_precip_ps.hlsl, cell_volair_ps.hlsl, cell_volsky_ps.hlsl, fo4_cell_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl, fo4_cellfs_ps.hlsl, fo4_effectcell_ps.hlsl, fo4_effectcellfx_ps.hlsl, fo4_effectdecal_ps.hlsl, fo4_effectdecalcsm_ps.hlsl, fo4_fog_ps.hlsl, fo4_fogcsm_ps.hlsl, fo4_water_ps.hlsl, fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookm_ps.hlsl, fo4_waterlookp_ps.hlsl, impostor_cell_ps.hlsl, impostor_cellinst_ps.hlsl, impostor_cellinstez_ps.hlsl, lookdev_ground_cell_ps.hlsl, lookdev_ground_csm_ps.hlsl, lookdev_ground_ps.hlsl, particles_cell_ps.hlsl, pbrm_cell_ps.hlsl, pbrm_cellcsm_ps.hlsl, pbrm_csm_ps.hlsl, pbrm_default_ps.hlsl
// Not here (declarations, or no converted entry carried them): wwAirCurvedLeg, wwAirCurvedTau, wwVolAirSky
#ifdef WW_FN_ALL
#ifndef WW_LOOKDEV_FOG_GLSL
#define WW_LOOKDEV_FOG_GLSL 1
#endif
static const float kAirEarthU = 6371000.0 * 70.0;
#endif

#if defined(WW_FN_wwAirNightHue) || defined(WW_FN_ALL)
float3 wwAirNightHue(float3 L)
{
    bool cond2 = fogNightHue <= 0.0f;
    bool wwh0 = fogNight.w <= 0.0f;
    bool cond;
    if (!cond2)
    {
        cond = wwh0;
    }
    else
    {
        cond = cond2;
    }
    if (cond)
    {
        return L;
    }
    float yn = dot(fogNight.xyz, float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f));
    if (yn <= 9.9999999600419720025001879548654e-13f)
    {
        return L;
    }
    return lerp(L, (fogNight.xyz * dot(max(L, 0.0f.xxx), float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f))) / yn.xxx, clamp(fogNight.w * fogNightHue, 0.0f, 1.0f).xxx);
}
#endif

#if defined(WW_FN_wwApOn) || defined(WW_FN_ALL)
bool wwApOn()
{
    return atAp.x > 0.5f;
}
#endif

#if defined(WW_FN_wwAtmoApAt) || defined(WW_FN_ALL)
float4 wwAtmoApAt(float3 posView)
{
    float dKm = length(posView) * atAp.z;
    float zv = max(-posView.z, 9.9999999747524270787835121154785e-07f);
    float2 ndc = float2((atApProj.x * posView.x) + (atApProj.z * posView.z), (atApProj.y * posView.y) + (atApProj.w * posView.z)) / zv.xx;
    float2 uv = clamp((ndc * 0.5f) + 0.5f.xx, 0.0f.xx, 1.0f.xx);
    float n = atAp.w;
    float s = sqrt(clamp(dKm / atAp.y, 0.0f, 1.0f));
    float4 v;
    if (s < (0.5f / n))
    {
        v = atApTex.Sample(_atApTex_sampler, float3(uv, 0.5f / n));
        float f = (s * n) * 2.0f;
        f *= f;
        v = float4(v.xyz * f, 1.0f - ((1.0f - v.w) * f));
    }
    else
    {
        v = atApTex.Sample(_atApTex_sampler, float3(uv, s));
    }
    return float4(max(v.xyz, 0.0f.xxx), clamp(v.w, 0.0f, 1.0f));
}
#endif

#if defined(WW_FN_wwAtmoApAt_V2) || defined(WW_FN_ALL)
float4 wwAtmoApAt(float3 posView)
{
    float dKm = length(posView) * atAp.z;
    float zv = max(-posView.z, 9.9999999747524270787835121154785e-07f);
    float2 ndc = float2((atApProj.x * posView.x) + (atApProj.z * posView.z), (atApProj.y * posView.y) + (atApProj.w * posView.z)) / zv.xx;
    float2 uv = clamp((ndc * 0.5f) + 0.5f.xx, 0.0f.xx, 1.0f.xx);
    float n = atAp.w;
    float s = sqrt(clamp(dKm / atAp.y, 0.0f, 1.0f));
    float4 v;
    if (s < (0.5f / n))
    {
        v = atApTex.SampleLevel(_atApTex_sampler, float3(uv, 0.5f / n), 0.0f);
        float f = (s * n) * 2.0f;
        f *= f;
        v = float4(v.xyz * f, 1.0f - ((1.0f - v.w) * f));
    }
    else
    {
        v = atApTex.SampleLevel(_atApTex_sampler, float3(uv, s), 0.0f);
    }
    return float4(max(v.xyz, 0.0f.xxx), clamp(v.w, 0.0f, 1.0f));
}
#endif

#if defined(WW_FN_wwFogEval) || defined(WW_FN_ALL)
float wwFogEval(float d, inout float z, inout float hb, out float3 fogCol)
{
    if ((fogRed & 4) != 0)
    {
        z = 0.0f;
    }
    float ramp = (d * fogK[0].x) - fogK[0].z;
    float f = clamp(ramp, 0.0f, 1.0f);
    float2 pair = clamp((fogK[5].xy * z) - fogK[5].zw, 0.0f.xx, 1.0f.xx);
    hb = lerp(pair.x, pair.y, f);
    float mx = fogK[2].w;
    bool cond2 = ramp > 0.75f;
    bool wwh1 = (fogRed & 1) == 0;
    bool cond;
    if (cond2)
    {
        cond = wwh1;
    }
    else
    {
        cond = cond2;
    }
    float clampTPick;
    if (cond)
    {
        clampTPick = min((((f - 0.75f) * 4.0f) * (1.0f - mx)) + mx, 1.0f);
    }
    else
    {
        clampTPick = mx;
    }
    float clampT = clampTPick;
    bool cond4 = ramp < 0.014999999664723873138427734375f;
    bool wwh2 = (fogRed & 2) == 0;
    bool cond3;
    if (cond4)
    {
        cond3 = wwh2;
    }
    else
    {
        cond3 = cond4;
    }
    float escapePick;
    if (cond3)
    {
        escapePick = f * 66.6666717529296875f;
    }
    else
    {
        escapePick = 1.0f;
    }
    float escape = escapePick;
    float IPick;
    if (f > 0.0f)
    {
        IPick = min(clampT, pow(f, fogK[1].w));
    }
    else
    {
        IPick = 0.0f;
    }
    float I = IPick;
    float weight = (hb * fogK[3].w) + (1.0f - hb);
    fogCol = lerp(lerp(fogK[1].xyz, fogK[3].xyz, I.xxx), lerp(fogK[2].xyz, fogK[4].xyz, I.xxx), hb.xxx);
    return (weight * I) * escape;
}
#endif

#if defined(WW_FN_wwAirFlatPos) || defined(WW_FN_ALL)
float3 wwAirFlatPos(float3 posView)
{
    float up2 = max(dot(fogView.xyz, fogView.xyz), 9.9999999600419720025001879548654e-13f);
    float h = dot(fogView.xyz, posView);
    // |fogView.xyz| = view -> world, h is world
    float g2 = max((dot(posView, posView) * up2) - (h * h), 0.0f);
    return posView + (fogView.xyz * ((g2 / 891939968.0f) / up2));
}
#endif

#if defined(WW_FN_wwAirColOne) || defined(WW_FN_ALL)
float wwAirColOne(float lo, float hi, float z0, float invH)
{
    if (hi <= lo)
    {
        return 0.0f;
    }
    if (invH <= 0.0f)
    {
        return hi - lo;
    }
    float below = max(min(hi, z0) - lo, 0.0f);
    float a = max(lo, z0);
    if (hi <= a)
    {
        return below;
    }
    float w = hi - a;
    float u = w * invH;
    float cond2;
    if (u < 0.001000000047497451305389404296875f)
    {
        cond2 = 1.0f - (u * (0.5f - (u / 6.0f)));
    }
    else
    {
        cond2 = (1.0f - exp(-u)) / u;
    }
    float e1 = cond2;
    return below + ((exp((-(a - z0)) * invH) * w) * e1);
}
#endif

#if defined(WW_FN_wwAirEnd) || defined(WW_FN_ALL)
float wwAirEnd(float4 med, float4 ground)
{
    float cond2;
    if (ground.z > 0.0f)
    {
        cond2 = med.w + (16.0f / ground.z);
    }
    else
    {
        cond2 = med.w;
    }
    return cond2;
}
#endif

#if defined(WW_FN_wwAirRhoLaw) || defined(WW_FN_ALL)
float wwAirRhoLaw(inout float z, float4 med, float4 layer, float4 ground)
{
    float k = 1.0f;
    if (z > med.w)
    {
        if (ground.z <= 0.0f)
        {
            return 0.0f;
        }
        // G2: the top's density, falling off
        k = exp((-(z - med.w)) * ground.z);
        z = med.w;
    }
    float cond2;
    if (med.z > 0.0f)
    {
        cond2 = min(1.0f, exp((-(z - med.y)) * med.z));
    }
    else
    {
        cond2 = 1.0f;
    }
    float r = cond2;
    bool cond = layer.x <= 0.0f;
    bool wwh3 = layer.w <= 0.0f;
    bool clampTPick;
    if (cond)
    {
        clampTPick = wwh3;
    }
    else
    {
        clampTPick = cond;
    }
    if (clampTPick)
    {
        return k * r;
    }
    if (layer.x > 0.0f)
    {
        float cond4;
        if (layer.z > 0.0f)
        {
            cond4 = min(1.0f, exp((-(z - layer.y)) * layer.z));
        }
        else
        {
            cond4 = 1.0f;
        }
        r = ((1.0f - layer.x) * r) + (layer.x * cond4);
    }
    if (layer.w > 0.0f)
    {
        float cond3;
        if (ground.y > 0.0f)
        {
            cond3 = min(1.0f, exp((-(z - ground.x)) * ground.y));
        }
        else
        {
            cond3 = 1.0f;
        }
        r += (layer.w * cond3);
    }
    return k * r;
}
#endif

#if defined(WW_FN_wwAirColLaw) || defined(WW_FN_ALL)
float wwAirColLaw(float lo, inout float hi, float4 med, float4 layer, float4 ground)
{
    bool cond2 = ground.z > 0.0f;
    bool wwh4 = hi > med.w;
    bool cond;
    if (cond2)
    {
        cond = wwh4;
    }
    else
    {
        cond = cond2;
    }
    float clampTPick;
    if (cond)
    {
        float cond4 = med.w;
        float4 cond3 = med;
        float4 escapePick = layer;
        float4 IPick = ground;
        float wwAirRhoLawRet = wwAirRhoLaw(cond4, cond3, escapePick, IPick);
        float clampTPart4 = max(lo, med.w);
        float clampTPart3 = hi;
        float clampTPart2 = med.w;
        float clampTPart = ground.z;
        clampTPick = wwAirRhoLawRet * wwAirColOne(clampTPart4, clampTPart3, clampTPart2, clampTPart);
    }
    else
    {
        clampTPick = 0.0f;
    }
    float up = clampTPick;
    hi = min(hi, med.w);
    float cPart12 = lo;
    float cPart11 = hi;
    float cPart10 = med.y;
    float cPart9 = med.z;
    float c = wwAirColOne(cPart12, cPart11, cPart10, cPart9);
    bool cond6 = layer.x <= 0.0f;
    bool wwh5 = layer.w <= 0.0f;
    bool cond5;
    if (cond6)
    {
        cond5 = wwh5;
    }
    else
    {
        cond5 = cond6;
    }
    if (cond5)
    {
        return c + up;
    }
    if (layer.x > 0.0f)
    {
        float cPart8 = lo;
        float cPart7 = hi;
        float cPart6 = layer.y;
        float cPart5 = layer.z;
        c = ((1.0f - layer.x) * c) + (layer.x * wwAirColOne(cPart8, cPart7, cPart6, cPart5));
    }
    if (layer.w > 0.0f)
    {
        float cPart4 = lo;
        float cPart3 = hi;
        float cPart2 = ground.x;
        float cPart = ground.y;
        c += (layer.w * wwAirColOne(cPart4, cPart3, cPart2, cPart));
    }
    return c + up;
}
#endif

#if defined(WW_FN_wwAirTerrH) || defined(WW_FN_ALL)
float wwAirTerrH(float3 p)
{
    float2 uv = float2(dot(volAirTerrU.xyz, p) + volAirTerrU.w, dot(volAirTerrV.xyz, p) + volAirTerrV.w);
    float2 uf = float2(dot(volAirTerrFU.xyz, p) + volAirTerrFU.w, dot(volAirTerrFV.xyz, p) + volAirTerrFV.w);
    float hc = volAirTerr.Sample(_volAirTerr_sampler, float3(uv, 0.0f)).x;
    float wf = clamp(min(min(uf.x, 1.0f - uf.x), min(uf.y, 1.0f - uf.y)) * 16.0f, 0.0f, 1.0f);
    float cond2;
    if (wf > 0.0f)
    {
        cond2 = lerp(hc, volAirTerr.Sample(_volAirTerr_sampler, float3(uf, 1.0f)).x, wf);
    }
    else
    {
        cond2 = hc;
    }
    return cond2;
}
#endif

#if defined(WW_FN_wwAirTerrH_V2) || defined(WW_FN_ALL)
float wwAirTerrH(float3 p)
{
    float2 uv = float2(dot(volAirTerrU.xyz, p) + volAirTerrU.w, dot(volAirTerrV.xyz, p) + volAirTerrV.w);
    float2 uf = float2(dot(volAirTerrFU.xyz, p) + volAirTerrFU.w, dot(volAirTerrFV.xyz, p) + volAirTerrFV.w);
    float hc = volAirTerr.SampleLevel(_volAirTerr_sampler, float3(uv, 0.0f), 0.0f).x;
    float wf = clamp(min(min(uf.x, 1.0f - uf.x), min(uf.y, 1.0f - uf.y)) * 16.0f, 0.0f, 1.0f);
    float cond2;
    if (wf > 0.0f)
    {
        cond2 = lerp(hc, volAirTerr.SampleLevel(_volAirTerr_sampler, float3(uf, 1.0f), 0.0f).x, wf);
    }
    else
    {
        cond2 = hc;
    }
    return cond2;
}
#endif

#if defined(WW_FN_wwAirGroundAt) || defined(WW_FN_ALL)
float4 wwAirGroundAt(float3 p)
{
    if ((volAirFlags & 64) == 0)
    {
        return volAirGround;	// (airGround): the ground layer's anchor zg, 1 / its thickness, G2: 1 / the top's falloff height (0: a hard top)
    }
    float3 cond2 = p;
    return float4((volAirGround.x + wwAirTerrH(cond2)) - volAirTerrRef, volAirGround.yzw);
}
#endif

#if defined(WW_FN_wwAirGroundRay) || defined(WW_FN_ALL)
float4 wwAirGroundRay(float3 p0, float3 dirV, float len, float s)
{
    if ((volAirFlags & 64) == 0)
    {
        return volAirGround;	// (airGround): the ground layer's anchor zg, 1 / its thickness, G2: 1 / the top's falloff height (0: a hard top)
    }
    // view units per fog unit
    float k = 1.0f / max(fogDistScale, 9.9999999600419720025001879548654e-13f);
    float2 u0 = float2(dot(volAirTerrU.xyz, p0) + volAirTerrU.w, dot(volAirTerrV.xyz, p0) + volAirTerrV.w);
    float2 du = float2(dot(volAirTerrU.xyz, dirV), dot(volAirTerrV.xyz, dirV)) * k;
    float cond2;
    if (du.x > 0.0f)
    {
        cond2 = (1.0f - u0.x) / du.x;
    }
    else
    {
        float cond;
        if (du.x < 0.0f)
        {
            cond = (-u0.x) / du.x;
        }
        else
        {
            cond = 1000000015047466219876688855040.0f;
        }
        cond2 = cond;
    }
    float clampTPick;
    if (du.y > 0.0f)
    {
        clampTPick = (1.0f - u0.y) / du.y;
    }
    else
    {
        float cond4;
        if (du.y < 0.0f)
        {
            cond4 = (-u0.y) / du.y;
        }
        else
        {
            cond4 = 1000000015047466219876688855040.0f;
        }
        clampTPick = cond4;
    }
    float2 ex = float2(cond2, clampTPick);
    float L = clamp(min(min(ex.x, ex.y), min(len, 10000000.0f)), 0.0f, 10000000.0f);
    float cond3;
    if (volAirGround.y > 0.0f)
    {
        cond3 = 1.0f / volAirGround.y;
    }
    else
    {
        cond3 = 1000000000.0f;
    }
    float H = cond3;
    // the weight's live span
    float Lt = min(L, (40.0f * H) / max(abs(s), 9.999999717180685365747194737196e-10f));
    float escapePick;
    if (s < 0.0f)
    {
        escapePick = L - Lt;
    }
    else
    {
        escapePick = 0.0f;
    }
    float xs = escapePick;
    float b = (s * Lt) / H;
    float bp = b / 3.0f;
    float E = 1.0f - exp(-bp);
    float m = -1000000015047466219876688855040.0f;
    float t[6];
    float IPick;
    float a[6];
    for (int q = 0; q < 6; q++)
    {
        float f = (float(q) + 0.5f) / 6.0f;
        if (abs(bp) < 0.001000000047497451305389404296875f)
        {
            IPick = f;
        }
        else
        {
            IPick = (-log(1.0f - (f * E))) / bp;
        }
        t[q] = IPick;
        float3 wwAirRhoLawRet = p0 + (dirV * ((xs + (t[q] * Lt)) * k));
        a[q] = (wwAirTerrH(wwAirRhoLawRet) / H) - ((b - bp) * t[q]);
        m = max(m, a[q]);
    }
    float lf[2];
    float clampTPart4;
    float clampTPart3;
    for (int i = 0; i < 2; i++)
    {
        float c = (i == 0) ? b : bp;
        if (abs(c) < 0.001000000047497451305389404296875f)
        {
            clampTPart4 = (-0.5f) * c;
        }
        else
        {
            if (c > 0.0f)
            {
                clampTPart3 = log((1.0f - exp(-c)) / c);
            }
            else
            {
                clampTPart3 = (-c) + log((1.0f - exp(c)) / (-c));
            }
            clampTPart4 = clampTPart3;
        }
        lf[i] = clampTPart4;
    }
    float sum = 0.0f;
    for (int q_1 = 0; q_1 < 6; q_1++)
    {
        sum += exp(a[q_1] - m);
    }
    float zg = H * (((m + log(sum / 6.0f)) + lf[1]) - lf[0]);
    return float4((volAirGround.x + zg) - volAirTerrRef, volAirGround.yzw);
}
#endif

#if defined(WW_FN_wwAirCol) || defined(WW_FN_ALL)
float wwAirCol(float lo, float hi, float4 ground)
{
    float cond2 = lo;
    float cond = hi;
    float4 clampTPick = volAirMed;
    float4 cond4 = volAirLayer;
    float4 cond3 = ground;
    float escapePick = wwAirColLaw(cond2, cond, clampTPick, cond4, cond3);
    return escapePick;
}
#endif

#if defined(WW_FN_wwAirRainP) || defined(WW_FN_ALL)
float wwAirRainP(float h)
{
    if (h >= 0.0f)
    {
        return h + ((volAirRain.y * (1.0f - exp((-h) * volAirRain.z))) / volAirRain.z);
    }
    float hc = max(h, (-1.0f) / volAirRain.w);
    return (1.0f + volAirRain.y) * (hc + (((0.5f * hc) * hc) * volAirRain.w));
}
#endif

#if defined(WW_FN_wwAirRainTau) || defined(WW_FN_ALL)
float wwAirRainTau(float3 pA, float3 pB)
{
    if (volAirRain.x <= 0.0f)
    {
        return 0.0f;
    }
    float3 qA = mul(float4(pA, 1.0f), volAirRainXf).xyz;
    float3 dq = mul(float4(pB, 1.0f), volAirRainXf).xyz - qA;
    // the fade's top: no fog above it
    float hTop = (-1.0f) / volAirRain.w;
    float t0 = 0.0f;
    float t1 = 1.0f;
    if (abs(dq.z) > 9.999999717180685365747194737196e-10f)
    {
        float tc = (hTop - qA.z) / dq.z;
        if (dq.z > 0.0f)
        {
            t0 = max(t0, tc);
        }
        else
        {
            t1 = min(t1, tc);
        }
    }
    else
    {
        if (qA.z <= hTop)
        {
            return 0.0f;
        }
    }
    if (t1 <= t0)
    {
        return 0.0f;
    }
    int n = int(volAirRainFar.z);
    float w = (t1 - t0) / float(n);
    // every sub-segment's height range (km)
    float dh = dq.z * w;
    bool level = abs(dh) <= 9.9999997473787516355514526367188e-05f;
    float cond2;
    if (level)
    {
        cond2 = 0.0f;
    }
    else
    {
        float cond = qA.z + (dq.z * t0);
        cond2 = wwAirRainP(cond);
    }
    // the profile's integral at the sub-segment's start
    float Pa = cond2;
    float sum = 0.0f;
    float p;
    float clampTPick;
    for (int k = 0; k < n; k++)
    {
        float3 qm = qA + (dq * (t0 + ((float(k) + 0.5f) * w)));
        float2 e = abs(qm.xy - 0.5f.xx) * 2.0f;
        float c = lerp(volAirRainMap.SampleLevel(_volAirRainMap_sampler, qm.xy, 0.0f).x, volAirRainFar.x, smoothstep(1.0f - volAirRainFar.y, 1.0f, max(e.x, e.y)));
        if (level)
        {
            if (qm.z >= 0.0f)
            {
                clampTPick = 1.0f + (volAirRain.y * exp((-qm.z) * volAirRain.z));
            }
            else
            {
                clampTPick = (1.0f + volAirRain.y) * max(1.0f + (qm.z * volAirRain.w), 0.0f);
            }
            p = clampTPick;
        }
        else
        {
            float cond4 = qA.z + (dq.z * (t0 + (float(k + 1) * w)));
            float Pb = wwAirRainP(cond4);
            p = (Pb - Pa) / dh;
            Pa = Pb;
        }
        sum += (c * p);
    }
    return (((volAirRain.x * length(pB - pA)) * fogDistScale) * w) * sum;
}
#endif

#if defined(WW_FN_wwGridAt) || defined(WW_FN_ALL)
float4 wwGridAt(float2 uv, float2 share, float base, float n, float f)
{
    float2 lo = 0.5f.xx / volPackDims.xy;
    float2 st = clamp(uv * share, lo, share - lo);
    float4 v;
    if (f < 1.0f)
    {
        v = volTex.Sample(_volTex_sampler, float3(st, (base + 0.5f) / volPackDims.z));
        v = float4(v.xyz * max(f, 0.0f), 1.0f - ((1.0f - v.w) * max(f, 0.0f)));
    }
    else
    {
        v = volTex.Sample(_volTex_sampler, float3(st, ((base + min(f, n)) - 0.5f) / volPackDims.z));
    }
    return float4(max(v.xyz, 0.0f.xxx), clamp(v.w, 0.0f, 1.0f));
}
#endif

#if defined(WW_FN_wwGridAt_V2) || defined(WW_FN_ALL)
float4 wwGridAt(float2 uv, float2 share, float base, float n, float f)
{
    float2 lo = 0.5f.xx / volPackDims.xy;
    float2 st = clamp(uv * share, lo, share - lo);
    float4 v;
    if (f < 1.0f)
    {
        v = volTex.SampleLevel(_volTex_sampler, float3(st, (base + 0.5f) / volPackDims.z), 0.0f);
        v = float4(v.xyz * max(f, 0.0f), 1.0f - ((1.0f - v.w) * max(f, 0.0f)));
    }
    else
    {
        v = volTex.SampleLevel(_volTex_sampler, float3(st, ((base + min(f, n)) - 0.5f) / volPackDims.z), 0.0f);
    }
    return float4(max(v.xyz, 0.0f.xxx), clamp(v.w, 0.0f, 1.0f));
}
#endif

#if defined(WW_FN_wwAirTailJ) || defined(WW_FN_ALL)
float3 wwAirTailJ(float4 last, float4 prev)
{
    float dT = prev.w - last.w;
    if ((fogRed & 8) != 0)
    {
        if (dT > 1.0000000116860974230803549289703e-07f)
        {
            return max((last.xyz - prev.xyz) / dT.xxx, 0.0f.xxx);
        }
        bool cond2 = volAirGround.z > 0.0f;
        bool wwh6 = last.w < 0.99989998340606689453125f;
        bool cond;
        if (cond2)
        {
            cond = wwh6;
        }
        else
        {
            cond = cond2;
        }
        float3 clampTPick;
        if (cond)
        {
            clampTPick = max(last.xyz, 0.0f.xxx) / (1.0f - last.w).xxx;
        }
        else
        {
            clampTPick = 0.0f.xxx;
        }
        return clampTPick;
    }
    float3 dL = max(last.xyz - prev.xyz, 0.0f.xxx);
    if ((-log(max(last.w, 9.9999999747524270787835121154785e-07f))) <= 0.125f)
    {
        return dL / max(prev.w, 9.9999999747524270787835121154785e-07f).xxx;
    }
    float m = 1.0f - last.w;
    float3 cond4;
    if (m > 1.0000000116860974230803549289703e-07f)
    {
        cond4 = max(last.xyz, 0.0f.xxx) / m.xxx;
    }
    else
    {
        cond4 = 0.0f.xxx;
    }
    float3 Jc = cond4;
    float3 cond3;
    if (dT > 1.0000000116860974230803549289703e-07f)
    {
        cond3 = dL / dT.xxx;
    }
    else
    {
        cond3 = 0.0f.xxx;
    }
    return lerp(Jc, cond3, clamp(((dT * 64.0f) / max(last.w, 9.9999999747524270787835121154785e-07f)) - 1.0f, 0.0f, 1.0f).xxx);
}
#endif

#if defined(WW_FN_wwAirTailW) || defined(WW_FN_ALL)
float wwAirTailW(float T)
{
    return min(0.0625f / max(-log(max(T, 9.9999999747524270787835121154785e-07f)), 9.999999717180685365747194737196e-10f), 0.5f);
}
#endif

#if defined(WW_FN_wwAirTailF) || defined(WW_FN_ALL)
float wwAirTailF(float T, float lnSpan, float n)
{
    if ((fogRed & 8) != 0)
    {
        return n - 1.0f;
    }
    float cond2 = T;
    return clamp(n * (1.0f + (log(1.0f - wwAirTailW(cond2)) / lnSpan)), 1.0f, n - 1.0f);
}
#endif

#if defined(WW_FN_wwAirThinJ) || defined(WW_FN_ALL)
float3 wwAirThinJ(float3 J, float4 v, float z0, float far)
{
    float zc = fogView.w;
    bool cond2 = (fogRed & 8) == 0;
    bool wwh7 = (-log(max(v.w, 9.9999999747524270787835121154785e-07f))) <= 0.125f;
    bool cond;
    if (cond2)
    {
        cond = wwh7;
    }
    else
    {
        cond = cond2;
    }
    bool thin = cond;
    bool clampTPick = !thin;
    bool cond4;
    if (clampTPick)
    {
        bool cond3 = (volAirFlags & 32768) == 0;
        bool wwh8 = volAirGround.z <= 0.0f;
        bool escapePick;
        if (!cond3)
        {
            escapePick = wwh8;
        }
        else
        {
            escapePick = cond3;
        }
        bool wwh9 = min(zc, z0) < volAirMed.w;
        bool IPick;
        if (!escapePick)
        {
            IPick = wwh9;
        }
        else
        {
            IPick = escapePick;
        }
        cond4 = IPick;
    }
    else
    {
        cond4 = clampTPick;
    }
    if (cond4)
    {
        return J;
    }
    float wwAirRhoLawRet;
    if (abs(z0 - zc) < 0.5f)
    {
        wwAirRhoLawRet = zc + 1.0f;
    }
    else
    {
        wwAirRhoLawRet = z0;
    }
    // the law's depth of the whole reach (a level ray: the density at the camera's height), one call for both rules
    float z1 = wwAirRhoLawRet;
    float k = (volAirMed.x * far) / abs(z1 - zc);
    float clampTPart4 = min(zc, z1);
    float clampTPart3 = max(zc, z1);
    float4 clampTPart2 = volAirGround;
    float od = k * wwAirCol(clampTPart4, clampTPart3, clampTPart2);
    if (thin)
    {
        // MODULES1 R7-4: a thin column (the same test as wwAirTailJ's; J = its far half's dL over the T at the half's
        // start). od, b = the law's depth of the whole reach and of its far half; the law is scaled so its whole-reach
        // 1 - T lies within one 16F step of the stored one (zones, rain: the stored depth rules where they differ by
        // more)
        float zh = 0.5f * (zc + z1);
        float clampTPart = min(zh, z1);
        float cPart12 = max(zh, z1);
        float4 cPart11 = volAirGround;
        float b = k * wwAirCol(clampTPart, cPart12, cPart11);
        float xa = 1.0f - exp(-od);
        float m = 1.0f - v.w;
        b *= (clamp(xa, m - 0.00048828125f, m + 0.00048828125f) / max(xa, 9.999999717180685365747194737196e-10f));
        float3 cPart10;
        if (b > 1.0000000116860974230803549289703e-07f)
        {
            cPart10 = J / (b * (1.0f - (0.5f * b))).xxx;
        }
        else
        {
            cPart10 = 0.0f.xxx;
        }
        return cPart10;
    }
    float3 cPart9;
    if ((od > 1.0000000116860974230803549289703e-07f) && (od < 0.0199999995529651641845703125f))
    {
        cPart9 = max(v.xyz, 0.0f.xxx) / (od * (1.0f - (0.5f * od))).xxx;
    }
    else
    {
        cPart9 = J;
    }
    return cPart9;
}
#endif

#if defined(WW_FN_wwGridTailJ) || defined(WW_FN_ALL)
float3 wwGridTailJ(float2 uv, float4 a, float4 b, float4 whole)
{
    float nN = volPackDims.w;
    bool red = (fogRed & 8) != 0;
    float cond2 = whole.w;
    float db = (volFar.x * exp(volFar.y)) * (1.0f - wwAirTailW(cond2));
    bool wwh10 = db > volFar.x;
    bool cond;
    if (!red)
    {
        cond = wwh10;
    }
    else
    {
        cond = red;
    }
    bool inFar = cond;
    float clampTPick;
    if (red)
    {
        clampTPick = volFar.z - 1.0f;
    }
    else
    {
        float cond4;
        if (inFar)
        {
            cond4 = min((log(db / volFar.x) / volFar.y) * volFar.z, volFar.z - 1.0f);
        }
        else
        {
            cond4 = (log(db / volSlices.x) / volSlices.y) * nN;
        }
        clampTPick = cond4;
    }
    float f = clampTPick;
    float2 cond3;
    if (inFar)
    {
        cond3 = volPack.zw;
    }
    else
    {
        cond3 = volPack.xy;
    }
    float escapePick;
    if (inFar)
    {
        escapePick = volFar.z;
    }
    else
    {
        escapePick = nN;
    }
    float2 IPick = uv;
    float2 wwAirRhoLawRet = cond3;
    float clampTPart4 = inFar ? nN : 0.0f;
    float clampTPart3 = escapePick;
    float clampTPart2 = f;
    float4 p = wwGridAt(IPick, wwAirRhoLawRet, clampTPart4, clampTPart3, clampTPart2);
    if (red)
    {
        float4 clampTPart = b;
        float4 cPart12 = p;
        return wwAirTailJ(clampTPart, cPart12);
    }
    float4 cPart11;
    if (inFar)
    {
        cPart11 = float4(a.xyz + (p.xyz * a.w), a.w * p.w);
    }
    else
    {
        cPart11 = p;
    }
    float4 cPart10 = whole;
    float4 cPart9 = cPart11;
    return wwAirTailJ(cPart10, cPart9);
}
#endif

#if defined(WW_FN_wwAirRecordColour) || defined(WW_FN_ALL)
float3 wwAirRecordColour(float d, float z, float3 dirView, inout bool ok)
{
    float cond2 = d;
    float cond = z;
    float clampTPick;
    float3 cond4;
    float cond3 = wwFogEval(cond2, cond, clampTPick, cond4);
    float hb = clampTPick;
    float3 fc = cond4;
    float escapePick;
    if (fogSun.w > 0.0f)
    {
        escapePick = pow(max(dot(dirView, fogSun.xyz), 0.0f), fogSunColour.w) * fogSun.w;
    }
    else
    {
        escapePick = 0.0f;
    }
    float sunW = escapePick;
    float3 c = max(lerp(fc, fogSunColour.xyz, sunW.xxx), 0.0f.xxx);
    float lc = dot(c, float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f));
    ok = lc > 9.9999999392252902907785028219223e-09f;
    float3 IPick;
    if (ok)
    {
        IPick = c / lc.xxx;
    }
    else
    {
        IPick = 1.0f.xxx;
    }
    return IPick;
}
#endif

#if defined(WW_FN_wwVolAirAtFar) || defined(WW_FN_ALL)
float4 wwVolAirAtFar(float d, float2 uv, float3 posView)
{
    float nN = volPackDims.w;
    if (d <= volSlices.x)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    float2 cond2 = uv;
    float2 cond = volPack.xy;
    float clampTPick = 0.0f;
    float cond4 = nN;
    float cond3 = (log(d / volSlices.x) / volSlices.y) * nN;
    float4 v = wwGridAt(cond2, cond, clampTPick, cond4, cond3);
    if (d <= volFar.x)
    {
        return v;
    }
    float fF = (log(d / volFar.x) / volFar.y) * volFar.z;
    float2 escapePick = uv;
    float2 IPick = volPack.zw;
    float wwAirRhoLawRet = nN;
    float clampTPart4 = volFar.z;
    float clampTPart3 = fF;
    float4 w = wwGridAt(escapePick, IPick, wwAirRhoLawRet, clampTPart4, clampTPart3);
    float4 o = float4(v.xyz + (w.xyz * v.w), v.w * w.w);
    bool clampTPart2 = fF <= volFar.z;
    bool wwh11 = (volAirFlags & 2) == 0;
    bool clampTPart;
    if (!clampTPart2)
    {
        clampTPart = wwh11;
    }
    else
    {
        clampTPart = clampTPart2;
    }
    bool wwh12 = volAirMed.x <= 0.0f;
    bool cPart12;
    if (!clampTPart)
    {
        cPart12 = wwh12;
    }
    else
    {
        cPart12 = clampTPart;
    }
    if (cPart12)
    {
        return o;
    }
    // the closed-form tail past the far grid (wwVolAirAt's, from the far grid's last slice)
    float far = volFar.x * exp(volFar.y);
    float2 cPart11 = uv;
    float4 cPart10 = v;
    float4 cPart9 = w;
    float4 cond6 = o;
    float3 J = wwGridTailJ(cPart11, cPart10, cPart9, cond6);
    float z1 = dot(fogView.xyz, posView) + fogView.w;
    float zc = fogView.w;
    float z0 = zc + ((z1 - zc) * (far / d));
    float3 cond5 = J;
    float4 cPart8 = o;
    float cPart7 = z0;
    float cPart6 = far;
    // MODULES1 10c
    J = wwAirThinJ(cond5, cPart8, cPart7, cPart6);
    float len = d - far;
    float dz = z1 - z0;
    float3 cPart5 = posView * (far / d);
    float3 cPart4 = posView / max(length(posView), 9.9999999600419720025001879548654e-13f).xxx;
    float cPart3 = len;
    float cPart2 = dz / max(len, 9.9999999747524270787835121154785e-07f);
    // FOGTERRAIN3: along the tail
    float4 gT = wwAirGroundRay(cPart5, cPart4, cPart3, cPart2);
    float cPart;
    if (abs(dz) < 0.5f)
    {
        float cPart18 = z0;
        float cPart17 = z0 + 1.0f;
        float4 cPart16 = gT;
        cPart = (volAirMed.x * len) * wwAirCol(cPart18, cPart17, cPart16);
    }
    else
    {
        float cPart15 = min(z0, z1);
        float cPart14 = max(z0, z1);
        float4 cPart13 = gT;
        cPart = ((volAirMed.x * len) * wwAirCol(cPart15, cPart14, cPart13)) / abs(dz);
    }
    float tau = cPart;
    float3 TtPart2 = posView * (far / d);
    float3 TtPart = posView;
    // lane RAINFOG2: + the rain fog
    float Tt = exp(-(tau + wwAirRainTau(TtPart2, TtPart)));
    if ((volAirFlags & 4) != 0)
    {
        float3 dirV = posView / max(length(posView), 9.9999999600419720025001879548654e-13f).xxx;
        float wwAirRecordColourPart6 = far;
        float wwAirRecordColourPart5 = z0;
        float3 wwAirRecordColourPart4 = dirV;
        bool wwAirRecordColourPick2;
        float3 wwAirRecordColourRet2 = wwAirRecordColour(wwAirRecordColourPart6, wwAirRecordColourPart5, wwAirRecordColourPart4, wwAirRecordColourPick2);
        bool ok0 = wwAirRecordColourPick2;
        float3 c0 = wwAirRecordColourRet2;
        float wwAirRecordColourPart3 = 0.5f * (far + d);
        float wwAirRecordColourPart2 = 0.5f * (z0 + z1);
        float3 wwAirRecordColourPart = dirV;
        bool wwAirRecordColourPick;
        float3 wwAirRecordColourRet = wwAirRecordColour(wwAirRecordColourPart3, wwAirRecordColourPart2, wwAirRecordColourPart, wwAirRecordColourPick);
        bool ok1 = wwAirRecordColourPick;
        float3 c1 = wwAirRecordColourRet;
        if (ok0 && ok1)
        {
            J *= (c1 / max(c0, 9.9999999747524270787835121154785e-07f.xxx));
        }
    }
    return float4(o.xyz + ((J * o.w) * (1.0f - Tt)), o.w * Tt);
}
#endif

#if defined(WW_FN_wwVolTex) || defined(WW_FN_ALL)
float4 wwVolTex(float3 p, bool xyOnly)
{
    if ((volAirFlags & 32) == 0)
    {
        return volTex.Sample(_volTex_sampler, p);
    }
    float3 n = float3(int3(spvTextureSize(volTex, uint(0), dummy_parameter)));
    float3 c = (p * n) - 0.5f.xxx;
    float3 i = floor(c);
    float3 f = c - i;
    float3 w0 = (((((-f) + 3.0f.xxx) * f) - 3.0f.xxx) * f) + 1.0f.xxx;
    float3 w1 = ((((f * 3.0f) - 6.0f.xxx) * f) * f) + 4.0f.xxx;
    float3 w2 = (((((f * (-3.0f)) + 3.0f.xxx) * f) + 3.0f.xxx) * f) + 1.0f.xxx;
    float3 w3 = (f * f) * f;
    float3 g0 = w0 + w1;
    float3 g1 = w2 + w3;
    float3 h0 = ((i - 0.5f.xxx) + (w1 / g0)) / n;
    float3 h1 = ((i + 1.5f.xxx) + (w3 / g1)) / n;
    g0 /= 6.0f.xxx;
    g1 /= 6.0f.xxx;
    if (xyOnly)
    {
        return (((volTex.Sample(_volTex_sampler, float3(h0.x, h0.y, p.z)) * g0.x) + (volTex.Sample(_volTex_sampler, float3(h1.x, h0.y, p.z)) * g1.x)) * g0.y) + (((volTex.Sample(_volTex_sampler, float3(h0.x, h1.y, p.z)) * g0.x) + (volTex.Sample(_volTex_sampler, float3(h1.x, h1.y, p.z)) * g1.x)) * g1.y);
    }
    float4 a = (((volTex.Sample(_volTex_sampler, float3(h0.x, h0.y, h0.z)) * g0.x) + (volTex.Sample(_volTex_sampler, float3(h1.x, h0.y, h0.z)) * g1.x)) * g0.y) + (((volTex.Sample(_volTex_sampler, float3(h0.x, h1.y, h0.z)) * g0.x) + (volTex.Sample(_volTex_sampler, float3(h1.x, h1.y, h0.z)) * g1.x)) * g1.y);
    float4 b = (((volTex.Sample(_volTex_sampler, float3(h0.x, h0.y, h1.z)) * g0.x) + (volTex.Sample(_volTex_sampler, float3(h1.x, h0.y, h1.z)) * g1.x)) * g0.y) + (((volTex.Sample(_volTex_sampler, float3(h0.x, h1.y, h1.z)) * g0.x) + (volTex.Sample(_volTex_sampler, float3(h1.x, h1.y, h1.z)) * g1.x)) * g1.y);
    return (a * g0.z) + (b * g1.z);
}
#endif

#if defined(WW_FN_wwVolTex_V2) || defined(WW_FN_ALL)
float4 wwVolTex(float3 p, bool xyOnly)
{
    if ((volAirFlags & 32) == 0)
    {
        return volTex.SampleLevel(_volTex_sampler, p, 0.0f);
    }
    float3 n = float3(int3(spvTextureSize(volTex, uint(0), dummy_parameter)));
    float3 c = (p * n) - 0.5f.xxx;
    float3 i = floor(c);
    float3 f = c - i;
    float3 w0 = (((((-f) + 3.0f.xxx) * f) - 3.0f.xxx) * f) + 1.0f.xxx;
    float3 w1 = ((((f * 3.0f) - 6.0f.xxx) * f) * f) + 4.0f.xxx;
    float3 w2 = (((((f * (-3.0f)) + 3.0f.xxx) * f) + 3.0f.xxx) * f) + 1.0f.xxx;
    float3 w3 = (f * f) * f;
    float3 g0 = w0 + w1;
    float3 g1 = w2 + w3;
    float3 h0 = ((i - 0.5f.xxx) + (w1 / g0)) / n;
    float3 h1 = ((i + 1.5f.xxx) + (w3 / g1)) / n;
    g0 /= 6.0f.xxx;
    g1 /= 6.0f.xxx;
    if (xyOnly)
    {
        return (((volTex.SampleLevel(_volTex_sampler, float3(h0.x, h0.y, p.z), 0.0f) * g0.x) + (volTex.SampleLevel(_volTex_sampler, float3(h1.x, h0.y, p.z), 0.0f) * g1.x)) * g0.y) + (((volTex.SampleLevel(_volTex_sampler, float3(h0.x, h1.y, p.z), 0.0f) * g0.x) + (volTex.SampleLevel(_volTex_sampler, float3(h1.x, h1.y, p.z), 0.0f) * g1.x)) * g1.y);
    }
    float4 a = (((volTex.SampleLevel(_volTex_sampler, float3(h0.x, h0.y, h0.z), 0.0f) * g0.x) + (volTex.SampleLevel(_volTex_sampler, float3(h1.x, h0.y, h0.z), 0.0f) * g1.x)) * g0.y) + (((volTex.SampleLevel(_volTex_sampler, float3(h0.x, h1.y, h0.z), 0.0f) * g0.x) + (volTex.SampleLevel(_volTex_sampler, float3(h1.x, h1.y, h0.z), 0.0f) * g1.x)) * g1.y);
    float4 b = (((volTex.SampleLevel(_volTex_sampler, float3(h0.x, h0.y, h1.z), 0.0f) * g0.x) + (volTex.SampleLevel(_volTex_sampler, float3(h1.x, h0.y, h1.z), 0.0f) * g1.x)) * g0.y) + (((volTex.SampleLevel(_volTex_sampler, float3(h0.x, h1.y, h1.z), 0.0f) * g0.x) + (volTex.SampleLevel(_volTex_sampler, float3(h1.x, h1.y, h1.z), 0.0f) * g1.x)) * g1.y);
    return (a * g0.z) + (b * g1.z);
}
#endif

#if defined(WW_FN_wwAirTailNearJ) || defined(WW_FN_ALL)
float3 wwAirTailNearJ(float2 uv, float4 v)
{
    float cond2 = v.w;
    float cond = volSlices.y;
    float clampTPick = volSlices.z;
    float3 cond4 = float3(uv, (wwAirTailF(cond2, cond, clampTPick) - 0.5f) / volSlices.z);
    bool cond3 = true;
    float4 escapePick = v;
    float4 IPick = wwVolTex(cond4, cond3);
    return wwAirTailJ(escapePick, IPick);
}
#endif

#if defined(WW_FN_wwVolAirFarHead) || defined(WW_FN_ALL)
bool wwVolAirFarHead(float d, float2 uv, float3 posView, inout float4 o, out float3 J, out float far)
{
    J = 0.0f.xxx;
    far = 0.0f;
    float nN = volPackDims.w;
    if (d <= volSlices.x)
    {
        o = float4(0.0f, 0.0f, 0.0f, 1.0f);
        return false;
    }
    float2 cond2 = uv;
    float2 cond = volPack.xy;
    float clampTPick = 0.0f;
    float cond4 = nN;
    float cond3 = (log(d / volSlices.x) / volSlices.y) * nN;
    float4 v = wwGridAt(cond2, cond, clampTPick, cond4, cond3);
    if (d <= volFar.x)
    {
        o = v;
        return false;
    }
    float fF = (log(d / volFar.x) / volFar.y) * volFar.z;
    float2 escapePick = uv;
    float2 IPick = volPack.zw;
    float wwAirRhoLawRet = nN;
    float clampTPart4 = volFar.z;
    float clampTPart3 = fF;
    float4 w = wwGridAt(escapePick, IPick, wwAirRhoLawRet, clampTPart4, clampTPart3);
    o = float4(v.xyz + (w.xyz * v.w), v.w * w.w);
    bool clampTPart2 = fF <= volFar.z;
    bool wwh13 = (volAirFlags & 2) == 0;
    bool clampTPart;
    if (!clampTPart2)
    {
        clampTPart = wwh13;
    }
    else
    {
        clampTPart = clampTPart2;
    }
    bool wwh14 = volAirMed.x <= 0.0f;
    bool cPart12;
    if (!clampTPart)
    {
        cPart12 = wwh14;
    }
    else
    {
        cPart12 = clampTPart;
    }
    if (cPart12)
    {
        return false;
    }
    far = volFar.x * exp(volFar.y);
    float2 cPart11 = uv;
    float4 cPart10 = v;
    float4 cPart9 = w;
    float4 cond6 = o;
    J = wwGridTailJ(cPart11, cPart10, cPart9, cond6);
    return true;
}
#endif

#if defined(WW_FN_wwVolAirNearHead) || defined(WW_FN_ALL)
bool wwVolAirNearHead(float d, float2 uv, inout float4 v, out float3 J, out float far)
{
    J = 0.0f.xxx;
    far = 0.0f;
    if (d <= volSlices.x)
    {
        v = float4(0.0f, 0.0f, 0.0f, 1.0f);
        return false;
    }
    float f = (log(d / volSlices.x) / volSlices.y) * volSlices.z;
    if (f < 1.0f)
    {
        float3 cond2 = float3(uv, 0.5f / volSlices.z);
        bool cond = true;
        v = wwVolTex(cond2, cond);
        v = float4(v.xyz * f, 1.0f - ((1.0f - v.w) * f));
    }
    else
    {
        float3 clampTPick = float3(uv, (min(f, volSlices.z) - 0.5f) / volSlices.z);
        bool cond4 = f > volSlices.z;
        v = wwVolTex(clampTPick, cond4);
    }
    v = float4(max(v.xyz, 0.0f.xxx), clamp(v.w, 0.0f, 1.0f));
    bool cond3 = f <= volSlices.z;
    bool wwh15 = (volAirFlags & 2) == 0;
    bool escapePick;
    if (!cond3)
    {
        escapePick = wwh15;
    }
    else
    {
        escapePick = cond3;
    }
    bool wwh16 = volAirMed.x <= 0.0f;
    bool IPick;
    if (!escapePick)
    {
        IPick = wwh16;
    }
    else
    {
        IPick = escapePick;
    }
    if (IPick)
    {
        return false;
    }
    far = volSlices.x * exp(volSlices.y);
    float2 wwAirRhoLawRet = uv;
    float4 clampTPart4 = v;
    J = wwAirTailNearJ(wwAirRhoLawRet, clampTPart4);
    return true;
}
#endif

#if defined(WW_FN_wwVolAirAtUv) || defined(WW_FN_ALL)
float4 wwVolAirAtUv(float3 posView, float2 uv)
{
    float d = length(posView) * fogDistScale;
    if ((volFarClamp != 0) && (!wwAirFree))
    {
        d = min(d, volTile.Sample(_volTile_sampler, uv).x);
    }
    if (volFar.w > 0.5f)
    {
        float cond2 = d;
        float2 cond = uv;
        float3 clampTPick = posView;
        return wwVolAirAtFar(cond2, cond, clampTPick);
    }
    if (d <= volSlices.x)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    float f = (log(d / volSlices.x) / volSlices.y) * volSlices.z;
    float4 v;
    if (f < 1.0f)
    {
        float3 cond4 = float3(uv, 0.5f / volSlices.z);
        bool cond3 = true;
        v = wwVolTex(cond4, cond3);
        v = float4(v.xyz * f, 1.0f - ((1.0f - v.w) * f));
    }
    else
    {
        float3 escapePick = float3(uv, (min(f, volSlices.z) - 0.5f) / volSlices.z);
        bool IPick = f > volSlices.z;
        v = wwVolTex(escapePick, IPick);
    }
    v = float4(max(v.xyz, 0.0f.xxx), clamp(v.w, 0.0f, 1.0f));
    bool wwAirRhoLawRet = f <= volSlices.z;
    bool wwh17 = (volAirFlags & 2) == 0;
    bool clampTPart4;
    if (!wwAirRhoLawRet)
    {
        clampTPart4 = wwh17;
    }
    else
    {
        clampTPart4 = wwAirRhoLawRet;
    }
    bool wwh18 = volAirMed.x <= 0.0f;
    bool clampTPart3;
    if (!clampTPart4)
    {
        clampTPart3 = wwh18;
    }
    else
    {
        clampTPart3 = clampTPart4;
    }
    if (clampTPart3)
    {
        return v;
    }
    float far = volSlices.x * exp(volSlices.y);
    float2 clampTPart2 = uv;
    float4 clampTPart = v;
    float3 J = wwAirTailNearJ(clampTPart2, clampTPart);
    float z1 = dot(fogView.xyz, posView) + fogView.w;
    float zc = fogView.w;
    float z0 = zc + ((z1 - zc) * (far / d));
    float3 cPart12 = J;
    float4 cPart11 = v;
    float cPart10 = z0;
    float cPart9 = far;
    J = wwAirThinJ(cPart12, cPart11, cPart10, cPart9);
    float len = d - far;
    float dz = z1 - z0;
    float3 cond6 = posView * (far / d);
    float3 cond5 = posView / max(length(posView), 9.9999999600419720025001879548654e-13f).xxx;
    float cPart8 = len;
    float cPart7 = dz / max(len, 9.9999999747524270787835121154785e-07f);
    float4 gT = wwAirGroundRay(cond6, cond5, cPart8, cPart7);
    float cPart6;
    if (abs(dz) < 0.5f)
    {
        float cPart5 = z0;
        float cPart4 = z0 + 1.0f;
        float4 cPart3 = gT;
        cPart6 = (volAirMed.x * len) * wwAirCol(cPart5, cPart4, cPart3);
    }
    else
    {
        float cPart2 = min(z0, z1);
        float cPart = max(z0, z1);
        float4 cPart18 = gT;
        cPart6 = ((volAirMed.x * len) * wwAirCol(cPart2, cPart, cPart18)) / abs(dz);
    }
    float tau = cPart6;
    float3 cPart17 = posView * (far / d);
    float3 cPart16 = posView;
    float Tt = exp(-(tau + wwAirRainTau(cPart17, cPart16)));
    if ((volAirFlags & 4) != 0)
    {
        float3 dirV = posView / max(length(posView), 9.9999999600419720025001879548654e-13f).xxx;
        float cPart15 = far;
        float cPart14 = z0;
        float3 cPart13 = dirV;
        bool TtPart2;
        float3 TtPart = wwAirRecordColour(cPart15, cPart14, cPart13, TtPart2);
        bool ok0 = TtPart2;
        float3 c0 = TtPart;
        float wwAirRecordColourPart6 = 0.5f * (far + d);
        float wwAirRecordColourPart5 = 0.5f * (z0 + z1);
        float3 wwAirRecordColourPart4 = dirV;
        bool wwAirRecordColourPick2;
        float3 wwAirRecordColourRet2 = wwAirRecordColour(wwAirRecordColourPart6, wwAirRecordColourPart5, wwAirRecordColourPart4, wwAirRecordColourPick2);
        bool ok1 = wwAirRecordColourPick2;
        float3 c1 = wwAirRecordColourRet2;
        if (ok0 && ok1)
        {
            J *= (c1 / max(c0, 9.9999999747524270787835121154785e-07f.xxx));
        }
    }
    return float4(v.xyz + ((J * v.w) * (1.0f - Tt)), v.w * Tt);
}
#endif

#if defined(WW_FN_wwVolAirAtUv_V2) || defined(WW_FN_ALL)
float4 wwVolAirAtUv(float3 posView, float2 uv)
{
    float d = length(posView) * fogDistScale;
    if ((volFarClamp != 0) && (!wwAirFree))
    {
        d = min(d, volTile.Sample(_volTile_sampler, uv).x);
    }
    if (volFar.w > 0.5f)
    {
        float cond2 = d;
        float2 cond = uv;
        float3 clampTPick = posView;
        return wwVolAirAtFar(cond2, cond, clampTPick);
    }
    if (d <= volSlices.x)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    float f = (log(d / volSlices.x) / volSlices.y) * volSlices.z;
    float4 v;
    if (f < 1.0f)
    {
        float3 cond4 = float3(uv, 0.5f / volSlices.z);
        bool cond3 = true;
        v = wwVolTex(cond4, cond3);
        v = float4(v.xyz * f, 1.0f - ((1.0f - v.w) * f));
    }
    else
    {
        float3 escapePick = float3(uv, (min(f, volSlices.z) - 0.5f) / volSlices.z);
        bool IPick = f > volSlices.z;
        v = wwVolTex(escapePick, IPick);
    }
    v = float4(max(v.xyz, 0.0f.xxx), clamp(v.w, 0.0f, 1.0f));
    bool wwAirRhoLawRet = f <= volSlices.z;
    bool wwh19 = (volAirFlags & 2) == 0;
    bool clampTPart4;
    if (!wwAirRhoLawRet)
    {
        clampTPart4 = wwh19;
    }
    else
    {
        clampTPart4 = wwAirRhoLawRet;
    }
    bool wwh20 = volAirMed.x <= 0.0f;
    bool clampTPart3;
    if (!clampTPart4)
    {
        clampTPart3 = wwh20;
    }
    else
    {
        clampTPart3 = clampTPart4;
    }
    if (clampTPart3)
    {
        return v;
    }
    float far = volSlices.x * exp(volSlices.y);
    float2 clampTPart2 = uv;
    float4 clampTPart = v;
    float3 J = wwAirTailNearJ(clampTPart2, clampTPart);
    float z1 = dot(fogView.xyz, posView) + fogView.w;
    float zc = fogView.w;
    float z0 = zc + ((z1 - zc) * (far / d));
    float3 cPart12 = J;
    float4 cPart11 = v;
    float cPart10 = z0;
    float cPart9 = far;
    J = wwAirThinJ(cPart12, cPart11, cPart10, cPart9);
    float len = d - far;
    float dz = z1 - z0;
    float3 cond6 = posView * (far / d);
    float3 cond5 = posView / max(length(posView), 9.9999999600419720025001879548654e-13f).xxx;
    float cPart8 = len;
    float cPart7 = dz / max(len, 9.9999999747524270787835121154785e-07f);
    float4 gT = wwAirGroundRay(cond6, cond5, cPart8, cPart7);
    float cPart6;
    if (abs(dz) < 0.5f)
    {
        float cPart5 = z0;
        float cPart4 = z0 + 1.0f;
        float4 cPart3 = gT;
        cPart6 = (volAirMed.x * len) * wwAirCol(cPart5, cPart4, cPart3);
    }
    else
    {
        float cPart2 = min(z0, z1);
        float cPart = max(z0, z1);
        float4 cPart18 = gT;
        cPart6 = ((volAirMed.x * len) * wwAirCol(cPart2, cPart, cPart18)) / abs(dz);
    }
    float tau = cPart6;
    float3 cPart17 = posView * (far / d);
    float3 cPart16 = posView;
    float Tt = exp(-(tau + wwAirRainTau(cPart17, cPart16)));
    float3 cPart15 = posView / max(length(posView), 9.9999999600419720025001879548654e-13f).xxx;
    float cPart14 = far;
    float cPart13 = d;
    float TtPart2 = tau;
    J *= wwAirTailShadow(cPart15, cPart14, cPart13, TtPart2);
    if ((volAirFlags & 4) != 0)
    {
        float3 dirV = posView / max(length(posView), 9.9999999600419720025001879548654e-13f).xxx;
        float TtPart = far;
        float wwAirRecordColourPart6 = z0;
        float3 wwAirRecordColourPart5 = dirV;
        bool wwAirRecordColourPart4;
        float3 wwAirRecordColourPick2 = wwAirRecordColour(TtPart, wwAirRecordColourPart6, wwAirRecordColourPart5, wwAirRecordColourPart4);
        bool ok0 = wwAirRecordColourPart4;
        float3 c0 = wwAirRecordColourPick2;
        float wwAirRecordColourRet2 = 0.5f * (far + d);
        float wwAirRecordColourPart3 = 0.5f * (z0 + z1);
        float3 wwAirRecordColourPart2 = dirV;
        bool wwAirRecordColourPart;
        float3 wwAirRecordColourPick = wwAirRecordColour(wwAirRecordColourRet2, wwAirRecordColourPart3, wwAirRecordColourPart2, wwAirRecordColourPart);
        bool ok1 = wwAirRecordColourPart;
        float3 c1 = wwAirRecordColourPick;
        if (ok0 && ok1)
        {
            J *= (c1 / max(c0, 9.9999999747524270787835121154785e-07f.xxx));
        }
    }
    return float4(v.xyz + ((J * v.w) * (1.0f - Tt)), v.w * Tt);
}
#endif

#if defined(WW_FN_wwVolAirAtUv_V3) || defined(WW_FN_ALL)
float4 wwVolAirAtUv(float3 posView, float2 uv)
{
    float d = length(posView) * fogDistScale;
    if ((volFarClamp != 0) && (!wwAirFree))
    {
        d = min(d, volTile.SampleLevel(_volTile_sampler, uv, 0.0f).x);
    }
    if (volFar.w > 0.5f)
    {
        float cond2 = d;
        float2 cond = uv;
        float3 clampTPick = posView;
        return wwVolAirAtFar(cond2, cond, clampTPick);
    }
    if (d <= volSlices.x)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    float f = (log(d / volSlices.x) / volSlices.y) * volSlices.z;
    float4 v;
    if (f < 1.0f)
    {
        float3 cond4 = float3(uv, 0.5f / volSlices.z);
        bool cond3 = true;
        v = wwVolTex(cond4, cond3);
        v = float4(v.xyz * f, 1.0f - ((1.0f - v.w) * f));
    }
    else
    {
        float3 escapePick = float3(uv, (min(f, volSlices.z) - 0.5f) / volSlices.z);
        bool IPick = f > volSlices.z;
        v = wwVolTex(escapePick, IPick);
    }
    v = float4(max(v.xyz, 0.0f.xxx), clamp(v.w, 0.0f, 1.0f));
    bool wwAirRhoLawRet = f <= volSlices.z;
    bool wwh21 = (volAirFlags & 2) == 0;
    bool clampTPart4;
    if (!wwAirRhoLawRet)
    {
        clampTPart4 = wwh21;
    }
    else
    {
        clampTPart4 = wwAirRhoLawRet;
    }
    bool wwh22 = volAirMed.x <= 0.0f;
    bool clampTPart3;
    if (!clampTPart4)
    {
        clampTPart3 = wwh22;
    }
    else
    {
        clampTPart3 = clampTPart4;
    }
    if (clampTPart3)
    {
        return v;
    }
    float far = volSlices.x * exp(volSlices.y);
    float2 clampTPart2 = uv;
    float4 clampTPart = v;
    float3 J = wwAirTailNearJ(clampTPart2, clampTPart);
    float z1 = dot(fogView.xyz, posView) + fogView.w;
    float zc = fogView.w;
    float z0 = zc + ((z1 - zc) * (far / d));
    float3 cPart12 = J;
    float4 cPart11 = v;
    float cPart10 = z0;
    float cPart9 = far;
    J = wwAirThinJ(cPart12, cPart11, cPart10, cPart9);
    float len = d - far;
    float dz = z1 - z0;
    float3 cond6 = posView * (far / d);
    float3 cond5 = posView / max(length(posView), 9.9999999600419720025001879548654e-13f).xxx;
    float cPart8 = len;
    float cPart7 = dz / max(len, 9.9999999747524270787835121154785e-07f);
    float4 gT = wwAirGroundRay(cond6, cond5, cPart8, cPart7);
    float cPart6;
    if (abs(dz) < 0.5f)
    {
        float cPart5 = z0;
        float cPart4 = z0 + 1.0f;
        float4 cPart3 = gT;
        cPart6 = (volAirMed.x * len) * wwAirCol(cPart5, cPart4, cPart3);
    }
    else
    {
        float cPart2 = min(z0, z1);
        float cPart = max(z0, z1);
        float4 cPart18 = gT;
        cPart6 = ((volAirMed.x * len) * wwAirCol(cPart2, cPart, cPart18)) / abs(dz);
    }
    float tau = cPart6;
    float3 cPart17 = posView * (far / d);
    float3 cPart16 = posView;
    float Tt = exp(-(tau + wwAirRainTau(cPart17, cPart16)));
    if ((volAirFlags & 4) != 0)
    {
        float3 dirV = posView / max(length(posView), 9.9999999600419720025001879548654e-13f).xxx;
        float cPart15 = far;
        float cPart14 = z0;
        float3 cPart13 = dirV;
        bool TtPart2;
        float3 TtPart = wwAirRecordColour(cPart15, cPart14, cPart13, TtPart2);
        bool ok0 = TtPart2;
        float3 c0 = TtPart;
        float wwAirRecordColourPart6 = 0.5f * (far + d);
        float wwAirRecordColourPart5 = 0.5f * (z0 + z1);
        float3 wwAirRecordColourPart4 = dirV;
        bool wwAirRecordColourPick2;
        float3 wwAirRecordColourRet2 = wwAirRecordColour(wwAirRecordColourPart6, wwAirRecordColourPart5, wwAirRecordColourPart4, wwAirRecordColourPick2);
        bool ok1 = wwAirRecordColourPick2;
        float3 c1 = wwAirRecordColourRet2;
        if (ok0 && ok1)
        {
            J *= (c1 / max(c0, 9.9999999747524270787835121154785e-07f.xxx));
        }
    }
    return float4(v.xyz + ((J * v.w) * (1.0f - Tt)), v.w * Tt);
}
#endif

#if defined(WW_FN_wwVolAirAt) || defined(WW_FN_ALL)
float4 wwVolAirAt(inout float3 posView)
{
    float2 uv = (gl_FragCoord.xy - volRect.xy) * volRect.zw;
    if ((volAirFlags & 512) != 0)
    {
        float2 q = gl_FragCoord.xy + (5.588237762451171875f * float((volAirFlags >> 10) & 15)).xx;
        float3 n = frac(frac(float3(dot(q, float2(0.067110560834407806396484375f, 0.005837149918079376220703125f)), dot(q + float2(47.0f, 17.0f), float2(0.067110560834407806396484375f, 0.005837149918079376220703125f)), dot(q + float2(13.0f, 71.0f), float2(0.067110560834407806396484375f, 0.005837149918079376220703125f)))) * 52.98291778564453125f) - 0.5f.xxx;
        uv += (n.xy / (volPackDims.xy * volPack.xy));
        posView *= exp((n.z * volSlices.y) / volPackDims.w);
    }
    float3 cond2 = posView;
    float2 cond = uv;
    return wwVolAirAtUv(cond2, cond);
}
#endif

#if defined(WW_FN_wwVolAirAt_V2) || defined(WW_FN_ALL)
float4 wwVolAirAt(inout float3 posView)
{
    float2 uv = (pcFragCoord.xy - volRect.xy) * volRect.zw;
    if ((volAirFlags & 512) != 0)
    {
        float2 q = pcFragCoord.xy + (5.588237762451171875f * float((volAirFlags >> 10) & 15)).xx;
        float3 n = frac(frac(float3(dot(q, float2(0.067110560834407806396484375f, 0.005837149918079376220703125f)), dot(q + float2(47.0f, 17.0f), float2(0.067110560834407806396484375f, 0.005837149918079376220703125f)), dot(q + float2(13.0f, 71.0f), float2(0.067110560834407806396484375f, 0.005837149918079376220703125f)))) * 52.98291778564453125f) - 0.5f.xxx;
        uv += (n.xy / (volPackDims.xy * volPack.xy));
        posView *= exp((n.z * volSlices.y) / volPackDims.w);
    }
    float3 cond2 = posView;
    float2 cond = uv;
    return wwVolAirAtUv(cond2, cond);
}
#endif

#if defined(WW_FN_wwAirDriftAt) || defined(WW_FN_ALL)
float wwAirDriftAt(float3 p)
{
    float f = 1.0f - clamp(((length(p) * fogDistScale) - volAirDrift.y) * volAirDrift.z, 0.0f, 1.0f);
    bool wwh0 = (volAirDrift.x <= 0.0f);
    bool wwh1 = (f <= 0.0f);
    if (or(wwh0, wwh1))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
    {
        return 1.0f;
    }
    float4 q = float4(p, 1.0f);
    float a = volAirNoise.Sample(_volAirNoise_sampler, float3(dot(volAirDriftUv[0], q), dot(volAirDriftUv[1], q), dot(volAirDriftUv[2], q))).x;
    float b = volAirNoise.Sample(_volAirNoise_sampler, float3(dot(volAirDriftUv[3], q), dot(volAirDriftUv[4], q), dot(volAirDriftUv[5], q)) + float3(0.37000000476837158203125f, 0.61000001430511474609375f, 0.12999999523162841796875f)).x;
    return 1.0f + (((volAirDrift.x * f) * ((a - volAirDrift.w) + ((0.5f * f) * (b - volAirDrift.w)))) * 0.666666686534881591796875f);
}
#endif

#if defined(WW_FN_wwAirDriftAt_V2) || defined(WW_FN_ALL)
float wwAirDriftAt(float3 p)
{
    float f = 1.0f - clamp(((length(p) * fogDistScale) - volAirDrift.y) * volAirDrift.z, 0.0f, 1.0f);
    bool wwh2 = (volAirDrift.x <= 0.0f);
    bool wwh3 = (f <= 0.0f);
    if (or(wwh2, wwh3))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
    {
        return 1.0f;
    }
    float4 q = float4(p, 1.0f);
    float a = volAirNoise.SampleLevel(_volAirNoise_sampler, float3(dot(volAirDriftUv[0], q), dot(volAirDriftUv[1], q), dot(volAirDriftUv[2], q)), 0.0f).x;
    float b = volAirNoise.SampleLevel(_volAirNoise_sampler, float3(dot(volAirDriftUv[3], q), dot(volAirDriftUv[4], q), dot(volAirDriftUv[5], q)) + float3(0.37000000476837158203125f, 0.61000001430511474609375f, 0.12999999523162841796875f), 0.0f).x;
    return 1.0f + (((volAirDrift.x * f) * ((a - volAirDrift.w) + ((0.5f * f) * (b - volAirDrift.w)))) * 0.666666686534881591796875f);
}
#endif

#if defined(WW_FN_wwAirDriftSunK) || defined(WW_FN_ALL)
float wwAirDriftSunK(float3 p, float lo, float mu, float top)
{
    if (volAirDrift.x <= 0.0f)
    {
        return 1.0f;
    }
    float3 s = fogSun.xyz * (min(max(top - lo, 0.0f) / max(mu, 0.0199999995529651641845703125f), 4096.0f) / max(fogDistScale, 9.9999999600419720025001879548654e-13f));
    // a loop on a uniform count: one copy of the noise reads (three unrolled copies broke the decal program's limit)
    int n = ((volAirDriftRed & 1) != 0) ? 1 : 3;
    float f0 = (n == 1) ? 0.5f : 0.16666667163372039794921875f;
    float k = 0.0f;
    for (int i = 0; i < n; i++)
    {
        float3 cond2 = p + (s * (f0 + (float(i) / 3.0f)));
        k += wwAirDriftAt(cond2);
    }
    return k / float(n);
}
#endif

#if defined(WW_FN_wwVolAirSunTD) || defined(WW_FN_ALL)
float2 wwVolAirSunTD(float3 posView)
{
    bool cond2 = (!fogOn) || (!volOn);
    bool wwh23 = volSlices.w < 0.5f;
    bool cond;
    if (!cond2)
    {
        cond = wwh23;
    }
    else
    {
        cond = cond2;
    }
    bool wwh24 = fogSun.w <= 0.0f;
    bool clampTPick;
    if (!cond)
    {
        clampTPick = wwh24;
    }
    else
    {
        clampTPick = cond;
    }
    bool wwh25 = volAirMed.x <= 0.0f;
    bool cond4;
    if (!clampTPick)
    {
        cond4 = wwh25;
    }
    else
    {
        cond4 = clampTPick;
    }
    bool wwh26 = (volAirFlags & 1) == 0;
    bool cond3;
    if (!cond4)
    {
        cond3 = wwh26;
    }
    else
    {
        cond3 = cond4;
    }
    if (cond3)
    {
        return float2(1.0f, 0.0f);
    }
    float lo = dot(fogView.xyz, posView) + fogView.w;
    float4 escapePick = volAirMed;
    float4 IPick = volAirGround;
    // G2: the climb runs through the top's falloff
    // G2
    float end = wwAirEnd(escapePick, IPick);
    if (lo >= end)
    {
        return float2(1.0f, 0.0f);
    }
    float mu = max(dot(fogView.xyz, fogSun.xyz) / max(length(fogView.xyz), 9.9999999600419720025001879548654e-13f), 0.0199999995529651641845703125f);
    float3 wwAirRhoLawRet = posView;
    float4 g = wwAirGroundAt(wwAirRhoLawRet);
    float clampTPart4;
    if ((volAirFlags & 128) != 0)
    {
        float clampTPart3;
        if (volAirLayer.w > 0.0f)
        {
            float clampTPart2 = lo;
            float clampTPart = volAirMed.w;
            float cPart12 = g.x;
            float cPart11 = g.y;
            clampTPart3 = volAirLayer.w * wwAirColOne(clampTPart2, clampTPart, cPart12, cPart11);
        }
        else
        {
            clampTPart3 = 0.0f;
        }
        clampTPart4 = clampTPart3;
    }
    else
    {
        float cPart10 = lo;
        float cPart9 = end;
        float4 cond6 = g;
        clampTPart4 = wwAirCol(cPart10, cPart9, cond6);
    }
    // lane FIX1101A (atmo, the fog-sun fault): flag 128 (shipped) = only the derived ground layer dims the sun. The far and
    // near bands ARE the records' fog, and the weather's Sunlight colour is already the sun that weather gives the ground;
    // their column to the scene's top (its tallest shape) took Foggy noon at c3 to Ts 0.026 (no shadows, no shafts).
    float c = clampTPart4;
    float3 cond5 = posView;
    float cPart8 = lo;
    float cPart7 = mu;
    float cPart6 = volAirMed.w;
    float tau = (volAirMed.x * c) * wwAirDriftSunK(cond5, cPart8, cPart7, cPart6);
    float Td = exp((-tau) / mu);
    // MODULES1 G3 (bungo 11-09 items 6, 10e: the lit fog far brighter than the sunlit land beside it): what the column
    // takes out of the direct beam is mostly scattered forward and reaches the surface as soft light. The conservative
    // delta-Eddington total transmission (Meador & Weaver 1980 eq. 24) with a low-sun divisor fitted to a Monte Carlo
    // slab (notes/modules1/tools/g3_sundif_mc.py: 80 cases, worst 0.030), minus the direct beam; y = that share x mu,
    // the irradiance on a level surface per unit of the sun. volAirGround.w = 1 + the air's phase g (0: the red
    // WW_CELL_AIR_SUNDIF_RED=1, the direct beam alone)
    if (volAirGround.w <= 0.0f)
    {
        return float2(Td, 0.0f);
    }
    float pg = volAirGround.w - 1.0f;
    float gp = pg / (1.0f + pg);
    float tp = (1.0f - (pg * pg)) * tau;
    float g1 = 0.75f * (1.0f - gp);
    float Tt = 1.0f - (((g1 * tp) + (((0.25f * (2.0f - ((3.0f * gp) * mu))) - (g1 * mu)) * (1.0f - exp((-tp) / mu)))) / (1.0f + (g1 * tp)));
    Tt /= (1.0f + (((0.0350000001490116119384765625f * max(pg, 0.0f)) * ((1.0f / mu) - 1.0f)) * (1.0f - Td)));
    return float2(Td, max(Tt - Td, 0.0f) * mu);
}
#endif

#if defined(WW_FN_wwVolAirSunT) || defined(WW_FN_ALL)
float wwVolAirSunT(float3 posView)
{
    float3 cond2 = posView;
    return wwVolAirSunTD(cond2).x;
}
#endif

#if defined(WW_FN_wwFogBase) || defined(WW_FN_ALL)
float3 wwFogBase(float3 preFog, float3 posView, float tAp, bool apOn)
{
    float d = length(posView) * fogDistScale;
    float z = dot(fogView.xyz, posView) + fogView.w;
    float cond2 = d;
    float cond = z;
    float clampTPick;
    float3 cond4;
    float cond3 = wwFogEval(cond2, cond, clampTPick, cond4);
    float hb = clampTPick;
    float3 fogCol = cond4;
    float alpha = cond3;
    if (apOn)
    {
        float escapePick = tAp;
        float IPick = 1.0f - alpha;
        bool wwAirRhoLawRet = atApDouble > 0.5f;
        alpha = 1.0f - wwApFogResidualT(escapePick, IPick, wwAirRhoLawRet);
    }
    float3 viewDir = posView / max(length(posView), 9.9999999747524270787835121154785e-07f).xxx;
    float sunW = pow(max(dot(viewDir, fogSun.xyz), 0.0f), fogSunColour.w) * fogSun.w;
    float3 sunlit = lerp(fogCol, fogSunColour.xyz, sunW.xxx);
    float gray = dot(preFog, 0.3333333432674407958984375f.xxx);
    float clampTPart4;
    if (fogUnit > 0.0f)
    {
        clampTPart4 = gray / fogUnit;
    }
    else
    {
        clampTPart4 = gray;
    }
    float grayW = clampTPart4;
    float3 clampTPart3;
    if (alpha < fogK[2].w)
    {
        clampTPart3 = lerp(sunlit, gray.xxx, grayW.xxx);
    }
    else
    {
        clampTPart3 = sunlit;
    }
    float3 resolved = clampTPart3;
    return lerp(preFog, resolved, alpha.xxx);
}
#endif

#if defined(WW_FN_wwFogBase_V2) || defined(WW_FN_ALL)
float3 wwFogBase(float3 preFog, float3 posView, float tAp, bool apOn)
{
    float d = length(posView) * fogDistScale;
    float z = dot(fogView.xyz, posView) + fogView.w;
    float cond2 = d;
    float cond = z;
    float clampTPick;
    float3 cond4;
    float cond3 = wwFogEval(cond2, cond, clampTPick, cond4);
    float hb = clampTPick;
    float3 fogCol = cond4;
    float alpha_1 = cond3;
    if (apOn)
    {
        float escapePick = tAp;
        float IPick = 1.0f - alpha_1;
        bool wwAirRhoLawRet = atApDouble > 0.5f;
        alpha_1 = 1.0f - wwApFogResidualT(escapePick, IPick, wwAirRhoLawRet);
    }
    float3 viewDir = posView / max(length(posView), 9.9999999747524270787835121154785e-07f).xxx;
    float sunW = pow(max(dot(viewDir, fogSun.xyz), 0.0f), fogSunColour.w) * fogSun.w;
    float3 sunlit = lerp(fogCol, fogSunColour.xyz, sunW.xxx);
    float gray = dot(preFog, 0.3333333432674407958984375f.xxx);
    float clampTPart4;
    if (fogUnit > 0.0f)
    {
        clampTPart4 = gray / fogUnit;
    }
    else
    {
        clampTPart4 = gray;
    }
    float grayW = clampTPart4;
    float3 clampTPart3;
    if (alpha_1 < fogK[2].w)
    {
        clampTPart3 = lerp(sunlit, gray.xxx, grayW.xxx);
    }
    else
    {
        clampTPart3 = sunlit;
    }
    float3 resolved = clampTPart3;
    return lerp(preFog, resolved, alpha_1.xxx);
}
#endif

#if defined(WW_FN_wwFog) || defined(WW_FN_ALL)
float3 wwFog(inout float3 preFog, float3 posView)
{
    bool apOn = wwApOn();
    float4 cond2;
    if (apOn)
    {
        float3 cond = posView;
        cond2 = wwAtmoApAt(cond);
    }
    else
    {
        cond2 = float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    float4 ap = cond2;
    if (apOn)
    {
        preFog = (preFog * ap.w) + ap.xyz;
    }
    if (!fogOn)
    {
        return preFog;
    }
    bool wwh27 = volSlices.w > 0.5f;
    bool clampTPick;
    if (volOn)
    {
        clampTPick = wwh27;
    }
    else
    {
        clampTPick = volOn;
    }
    if (clampTPick)
    {
        float3 cond4 = posView;
        float4 cond3 = wwVolAirAt(cond4);
        float4 a = cond3;
        if (apOn)
        {
            bool dbl = atApDouble > 0.5f;
            float escapePick = ap.w;
            float IPick = a.w;
            bool wwAirRhoLawRet = dbl;
            float tr = wwApFogResidualT(escapePick, IPick, wwAirRhoLawRet);
            float3 clampTPart4;
            if (dbl)
            {
                clampTPart4 = a.xyz;
            }
            else
            {
                float3 clampTPart3 = a.xyz;
                float clampTPart2 = a.w;
                float clampTPart = tr;
                clampTPart4 = wwApFogResidualL(clampTPart3, clampTPart2, clampTPart);
            }
            float3 lr = clampTPart4;
            // R7 night sky d: the two airs' light together (ap.rgb x tr + lr), their T ap.a x tr
            if (fogNight.w > 0.0f)
            {
                float3 cPart12 = lr;
                lr = max(wwAirNightHue(cPart12), (fogNight.xyz * (1.0f - (ap.w * tr))) - (ap.xyz * tr));
            }
            return (preFog * tr) + lr;
        }
        if (fogNight.w > 0.0f)
        {
            float3 cPart11 = a.xyz;
            float cPart10 = a.w;
            float3 cPart9 = max(wwAirNightHue(cPart11), fogNight.xyz * (1.0f - cPart10));
            a.x = cPart9.x;
            a.y = cPart9.y;
            a.z = cPart9.z;
        }
        float3 lit = (preFog * a.w) + a.xyz;
        if (volFogBlend == 1)
        {
            float3 cond6 = lit;
            float3 cond5 = posView;
            float cPart8 = 1.0f;
            bool cPart7 = false;
            return wwFogBase(cond6, cond5, cPart8, cPart7);
        }
        // red blendnear (gates only): weighted by the air's 1 - T, which has no start distance
        if (volFogBlend == 2)
        {
            float cPart6 = length(posView) * fogDistScale;
            float cPart5 = dot(fogView.xyz, posView) + fogView.w;
            float cPart4;
            float3 cPart3;
            float cPart2 = wwFogEval(cPart6, cPart5, cPart4, cPart3);
            float hb = cPart4;
            float3 fc = cPart3;
            return lerp(lit, fc, (1.0f - a.w).xxx);
        }
        return lit;
    }
    float3 cPart = preFog;
    float3 cPart18 = posView;
    float cPart17 = ap.w;
    bool cPart16 = apOn;
    return wwFogBase(cPart, cPart18, cPart17, cPart16);
}
#endif

#if defined(WW_FN_wwFogFx) || defined(WW_FN_ALL)
float3 wwFogFx(inout float3 lin, float3 posView, bool additive)
{
    bool apOn = wwApOn();
    if ((!fogOn) && (!apOn))
    {
        return lin;
    }
    bool wwh28 = volSlices.w > 0.5f;
    bool cond2;
    if (volOn)
    {
        cond2 = wwh28;
    }
    else
    {
        cond2 = volOn;
    }
    bool wwh29 = (volAirFlags & 16) != 0;
    bool cond;
    if (cond2)
    {
        cond = wwh29;
    }
    else
    {
        cond = cond2;
    }
    bool volFx = cond;
    bool clampTPick = !additive;
    bool cond4;
    if (clampTPick)
    {
        cond4 = volFx || apOn;
    }
    else
    {
        cond4 = clampTPick;
    }
    if (cond4)
    {
        float3 cond3 = lin;
        float3 escapePick = posView;
        float3 IPick = wwFog(cond3, escapePick);
        return max(IPick, 0.0f.xxx);
    }
    float wwAirRhoLawRet;
    if (apOn)
    {
        float3 clampTPart4 = posView;
        wwAirRhoLawRet = wwAtmoApAt(clampTPart4).w;
    }
    else
    {
        wwAirRhoLawRet = 1.0f;
    }
    float tAp = wwAirRhoLawRet;
    if (!fogOn)
    {
        return lin * tAp;
    }
    float clampTPart3 = length(posView) * fogDistScale;
    float clampTPart2 = dot(fogView.xyz, posView) + fogView.w;
    float clampTPart;
    float3 cPart12;
    float cPart11 = wwFogEval(clampTPart3, clampTPart2, clampTPart, cPart12);
    float hb = clampTPart;
    float3 fc = cPart12;
    float f = cPart11;
    if (!volFx)
    {
        if (apOn)
        {
            float cPart10 = tAp;
            float cPart9 = 1.0f - f;
            bool cond6 = atApDouble > 0.5f;
            return (lin * tAp) * wwApFogResidualT(cPart10, cPart9, cond6);
        }
        float3 cond5;
        if (additive)
        {
            cond5 = lin * (1.0f - f);
        }
        else
        {
            cond5 = lerp(lin, fc, f.xxx);
        }
        return cond5;
    }
    float3 cPart8 = posView;
    float4 cPart7 = wwVolAirAt(cPart8);
    float tv = cPart7.w;
    if (apOn)
    {
        float cPart6 = tAp;
        float cPart5 = tv;
        bool cPart4 = atApDouble > 0.5f;
        return (lin * tAp) * wwApFogResidualT(cPart6, cPart5, cPart4);
    }
    lin *= tv;
    float3 cPart3;
    if (volFogBlend == 1)
    {
        cPart3 = lin * (1.0f - f);
    }
    else
    {
        cPart3 = lin;
    }
    return cPart3;
}
#endif

#if defined(WW_FN_wwFogProbe) || defined(WW_FN_ALL)
bool wwFogProbe(float3 posView, out float3 o)
{
    o = 0.0f.xxx;
    bool cond2 = fogOn && volOn;
    bool wwh30 = (volProbe == 1) || (volProbe == 2);
    bool cond;
    if (cond2)
    {
        cond = wwh30;
    }
    else
    {
        cond = cond2;
    }
    if (cond)
    {
        float3 clampTPick = posView;
        float4 cond4 = wwVolAirAt(clampTPick);
        float4 a = cond4;
        float3 cond3;
        if (volProbe == 1)
        {
            cond3 = a.xyz;
        }
        else
        {
            cond3 = a.w.xxx;
        }
        o = cond3;
        return true;
    }
    if ((fogOn && volOn) && (volProbe == 3))
    {
        float3 escapePick = posView;
        o = wwVolAirSunT(escapePick).xxx;
        return true;
    }
    bool IPick = !fogOn;
    bool wwh31 = fogProbe.z < 0.5f;
    bool wwAirRhoLawRet;
    if (!IPick)
    {
        wwAirRhoLawRet = wwh31;
    }
    else
    {
        wwAirRhoLawRet = IPick;
    }
    if (wwAirRhoLawRet)
    {
        return false;
    }
    if (fogProbe.z > 8.5f)
    {
        float d = length(posView) * fogDistScale;
        float z = dot(fogView.xyz, posView) + fogView.w;
        float clampTPart4;
        if (fogProbe.z < 9.5f)
        {
            clampTPart4 = log2(max(d, 1.0f)) / 20.0f;
        }
        else
        {
            clampTPart4 = (z + 65536.0f) / 131072.0f;
        }
        float x = clampTPart4;
        float q = floor((clamp(x, 0.0f, 1.0f) * 65535.0f) + 0.5f);
        o = float3(floor(q / 256.0f) / 255.0f, mod(q, 256.0f) / 255.0f, 0.0f);
        return true;
    }
    if (fogProbe.z > 7.5f)
    {
        float d_1 = length(posView) * fogDistScale;
        float z_1 = dot(fogView.xyz, posView) + fogView.w;
        o = float3(d_1 / 16384.0f, 0.5f + (z_1 / 32768.0f), 0.0f);
        return true;
    }
    if (fogProbe.z > 5.5f)
    {
        float d_2 = length(posView) * fogDistScale;
        float z_2 = dot(fogView.xyz, posView) + fogView.w;
        float clampTPart3 = d_2;
        float clampTPart2 = z_2;
        float clampTPart;
        float3 cPart12;
        float cPart11 = wwFogEval(clampTPart3, clampTPart2, clampTPart, cPart12);
        float hb = clampTPart;
        float3 fogCol = cPart12;
        float alpha_1 = cPart11;
        float3 cPart10;
        if (fogProbe.z < 6.5f)
        {
            cPart10 = float3(alpha_1, hb, 0.0f);
        }
        else
        {
            cPart10 = pow(max(fogCol, 0.0f.xxx), 0.4545454680919647216796875f.xxx);
        }
        o = cPart10;
        return true;
    }
    if (fogProbe.z > 4.5f)
    {
        float d_3 = length(posView) * fogDistScale;
        float z_3 = dot(fogView.xyz, posView) + fogView.w;
        o = float3(d_3 / 4096.0f, 0.5f + (z_3 / 2000.0f), 0.0f);
        return true;
    }
    float cPart9 = fogProbe.x;
    float cond6 = fogProbe.y;
    float cond5;
    float3 cPart8;
    float cPart7 = wwFogEval(cPart9, cond6, cond5, cPart8);
    float hb_1 = cond5;
    float3 fogCol_1 = cPart8;
    float alpha_2 = cPart7;
    if (fogProbe.z < 1.5f)
    {
        o = alpha_2.xxx;
    }
    else
    {
        if (fogProbe.z < 2.5f)
        {
            o = fogCol_1 * 0.5f;
        }
        else
        {
            o = hb_1.xxx;
        }
    }
    return true;
}
#endif

#if defined(WW_FN_wwFogProbe_V2) || defined(WW_FN_ALL)
bool wwFogProbe(float3 posView, out float3 o)
{
    o = 0.0f.xxx;
    bool cond2 = fogOn && volOn;
    bool wwh32 = (volProbe == 1) || (volProbe == 2);
    bool cond;
    if (cond2)
    {
        cond = wwh32;
    }
    else
    {
        cond = cond2;
    }
    if (cond)
    {
        float3 clampTPick = posView;
        float4 cond4 = wwVolAirAt(clampTPick);
        float4 a = cond4;
        float3 cond3;
        if (volProbe == 1)
        {
            cond3 = a.xyz;
        }
        else
        {
            cond3 = a.w.xxx;
        }
        o = cond3;
        return true;
    }
    if ((fogOn && volOn) && (volProbe == 3))
    {
        float3 escapePick = posView;
        o = wwVolAirSunT(escapePick).xxx;
        return true;
    }
    bool IPick = !fogOn;
    bool wwh33 = fogProbe.z < 0.5f;
    bool wwAirRhoLawRet;
    if (!IPick)
    {
        wwAirRhoLawRet = wwh33;
    }
    else
    {
        wwAirRhoLawRet = IPick;
    }
    if (wwAirRhoLawRet)
    {
        return false;
    }
    if (fogProbe.z > 8.5f)
    {
        float d = length(posView) * fogDistScale;
        float z = dot(fogView.xyz, posView) + fogView.w;
        float clampTPart4;
        if (fogProbe.z < 9.5f)
        {
            clampTPart4 = log2(max(d, 1.0f)) / 20.0f;
        }
        else
        {
            clampTPart4 = (z + 65536.0f) / 131072.0f;
        }
        float x = clampTPart4;
        float q = floor((clamp(x, 0.0f, 1.0f) * 65535.0f) + 0.5f);
        o = float3(floor(q / 256.0f) / 255.0f, mod(q, 256.0f) / 255.0f, 0.0f);
        return true;
    }
    if (fogProbe.z > 7.5f)
    {
        float d_1 = length(posView) * fogDistScale;
        float z_1 = dot(fogView.xyz, posView) + fogView.w;
        o = float3(d_1 / 16384.0f, 0.5f + (z_1 / 32768.0f), 0.0f);
        return true;
    }
    if (fogProbe.z > 5.5f)
    {
        float d_2 = length(posView) * fogDistScale;
        float z_2 = dot(fogView.xyz, posView) + fogView.w;
        float clampTPart3 = d_2;
        float clampTPart2 = z_2;
        float clampTPart;
        float3 cPart12;
        float cPart11 = wwFogEval(clampTPart3, clampTPart2, clampTPart, cPart12);
        float hb = clampTPart;
        float3 fogCol = cPart12;
        float alpha = cPart11;
        float3 cPart10;
        if (fogProbe.z < 6.5f)
        {
            cPart10 = float3(alpha, hb, 0.0f);
        }
        else
        {
            cPart10 = pow(max(fogCol, 0.0f.xxx), 0.4545454680919647216796875f.xxx);
        }
        o = cPart10;
        return true;
    }
    if (fogProbe.z > 4.5f)
    {
        float d_3 = length(posView) * fogDistScale;
        float z_3 = dot(fogView.xyz, posView) + fogView.w;
        o = float3(d_3 / 4096.0f, 0.5f + (z_3 / 2000.0f), 0.0f);
        return true;
    }
    float cPart9 = fogProbe.x;
    float cond6 = fogProbe.y;
    float cond5;
    float3 cPart8;
    float cPart7 = wwFogEval(cPart9, cond6, cond5, cPart8);
    float hb_1 = cond5;
    float3 fogCol_1 = cPart8;
    float alpha_1 = cPart7;
    if (fogProbe.z < 1.5f)
    {
        o = alpha_1.xxx;
    }
    else
    {
        if (fogProbe.z < 2.5f)
        {
            o = fogCol_1 * 0.5f;
        }
        else
        {
            o = hb_1.xxx;
        }
    }
    return true;
}
#endif
#endif	// WW_HLSL_SRC

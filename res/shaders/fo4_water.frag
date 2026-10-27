#version 410 core

/* lane WATER1 (src/gl/cellwater.h): the game's near-water pixel shader in the cell view, term for term.
 * World space throughout (z up), linear light; V runs from the eye to the pixel, as the game's ray does.
 *   d       = |P - eye|;  dfade = saturate( (d - 8192) / (P1.x - 8192) )            (1 near, 0 past 8192)
 *   depth   vertical = |P.z - S.z|, along the ray = |S - P|, S the opaque scene point behind the pixel;
 *           rz = saturate( 1 - vertical / P1.w ), rw = saturate( 1 - along / P1.w )
 *   alpha   x = saturate( (rw - P3.y) / (P3.x - P3.y) ); a = (1 - (3 - 2x) x^2)^0.33 (P3.w - P3.z) + P3.z
 *   shore   smoothstep( saturate( (rz - P2.x) / (1 - P2.x) ) )
 *   flatten (1 - smoothstep( saturate( (rz - P2.w) / (P2.z - P2.w) ) )) x P2.y
 *   normals n_k = ( 2 t.xy - 1, sqrt( 1 - min( |2 t.xy - 1|^2, 1 ) ) ), t the noise texel at world xy / uv scale;
 *           N = normalize( (n1 - (0,0,1)) Amp.x + (0,0,1) + (n2 Amp.y + n3 Amp.z) dfade ),
 *           N = normalize( flatten (N - up) + up )
 *   Fresnel F = P4.z + (1 - P4.z)(1 - saturate( -V.N ))^5
 *   sky     Rz = reflect( V, N ).z; mix( mix( horizon, lower, sat( Rz + 0.75 ) ), upper, sat( 1.9 Rz + 0.35 ) )
 *   silt    refr + LightSilt.w (1 - a) ( mix( DarkSilt, LightSilt, refr ) - refr )
 *   spec    sun pow( sat( reflect( V, N ).L ), Var.x ) Deep.w + sun pow( sat( N.(-0.099, -0.099, 0.99) ), Shallow.w ) P1.z
 *   colour  mix( mix( Reflection.rgb, mix( silted, sky, F Var.y ), dfade ) + spec, offset refraction, shore ), fog
 * Lane WATER2 (the game's asm, entries 02102 / 02146 / 02100):
 *   SSR     sky = mix( sky, s.rgb, s.a ), s = mix( march, blurred march, fWaterBlurRatio 0.9 ) at this pixel, before
 *           the reflection weight and the far colour read it (tech 0x21c; without SSR, tech 0x1c: the sky alone)
 *   depth   the game's depth-colour pass (02146, tech 0x18010, alpha blend 1) is drawn in the deferred composite,
 *           BEFORE the water itself; the water's refraction copy (t1) therefore holds the scene already murked:
 *           t = mix( t, mix( Deep, Shallow, smooth( sat( (rz - P4.y) / (P4.x - P4.y) ) ) ), a ), a the alpha above,
 *           rz and a taken at the pixel the texel came from (the offset tap: its own ray to the water plane)
 *   rays    waterSslr: the game's water reflection-ray pass instead of the colour: N flattened by HALF the shore
 *           factor, R = reflect( V, N ) into the view, kept when it points more than 0.2 into the view;
 *           out (R in the view, the view depth) for src/gl/cellssr.cpp's ray stage
 * The sun is the frame's light 0 (its direction and colour, the one the cell's surfaces take).
 *
 * Probes (WW_CELL_WATER_PROBE, opaque, raw; "hi/lo" = a 16-bit value's high and low byte):
 *   1/2 N x 0.5 + 0.5   3/4 V x 0.5 + 0.5   5/6 (rz, rw, dfade)   7 the scene texel behind   8 the offset texel
 *   9/10 the frame constants by column (x mod 4): 0 sun colour / 4, 1 sun direction x 0.5 + 0.5, 2 (A.a / 8, D.a / 8,
 *   under the surface), 3 (cellIsOn, cellIsLinear, fogOn)
 *   11/12 the noise taps (t0.x, t0.y, t1.x)  13/14 (t1.y, t2.x, t2.y)  15/16 the SSR read (rgb)  17/18 (a, 0, 0)
 *   19/20/21 the three bytes (high first) of 24-bit fixed ( dist / 16384, along / 4096, vertical / 4096 )
 *   22/23/24 likewise ( the offset texel's along / 4096, vertical / 4096, 1 where the depth pass covered it )
 *   25/26 (WATERHDR1) the linear light the water writes ( lin / 4 )  27 the water's pixels, magenta
 *   WATERDATA1, only in the WW_WATER_INFO_PROBE build (launch WW_CELL_WATER_INFO_PROBE=1), water_info.glsl:
 *   28 (shore steps, depth code, lookup on) as bytes  29 (body id high byte, low byte, class + 1)
 *   30 cyan where the lookup finds water, red where it finds none  31 depth / 1024 as grey
 *   WATERCOMBINE1: every water look below compiles into ONE twin, fo4_waterlook (all four defines), and each look
 *   is switched by its own uniform (waterDepthOn, waterCauOn, waterShoreOn bits, waterWavesOn), so any set of ticks
 *   draws together; fo4_water.prog (no define) keeps its machine code. Probes of the looks (fo4_waterlook only):
 *   WATERDEPTH1 (Water depth colour row), water_depth.glsl: 32 the depth used / 1024 as grey, 33 cyan where the bake
 *   wins, green where the scene does, red where no depth is known
 *   WATERWAVES1 (Water swell row), water_waves.glsl: 34 (0.5 + 4 x the swell's slope x, y; its weight)
 *   WATERCAUSTICS1 (Water caustics row), water_caustics.glsl: 35 the factor on the bed / 2 as grey,
 *   36 (focus I / 8, depth envelope, sun share); CAUSTICPATTERN1: green channel, I = the net's light factor E
 *   WATERSHORE1 (Water soft edge, Shore foam rows), water_shore.glsl: 37 (edge, foam, shore distance / 1024);
 *   WATERFOAMDETAIL1 (the foam's wave-energy law): 38 (Hs / 64 u, energy amount, water column / 64 u)
 *   (was 32 in the shore lane's own twin, which clashed with the depth probe)
 *   WATERREFL1 (Water reflections, Water sun glare rows), water_refl.glsl: 44 (F, sqrt( alpha ) / 0.5, SSR confidence)
 *   45/46 the glare alone ( lin / 16 )
 *   WATERFLOWBAKE1, same build: 41 the baked flow (was 32 in its lane: WATERDEPTH1 holds 32) ( vx / 4 x 0.5 + 0.5, vy / 4 x 0.5 + 0.5, flow lookup on )
 *   WATERLOOK1 (Ocean waves row), water_ocean.glsl: 39 (0.5 + 4 x the spectrum's slope x, y; its weight),
 *   40 (whitecap cover, sqrt( lost slope variance ) / 0.5, the caustic net's path factor / 3) */

#define WW_FOG 1
#define WW_CELLLIGHTS 1

in vec3 LightDir;
in vec3 ViewDir;
in vec2 texCoord;
in mat3 btnMatrix;
flat in vec4 A;
in vec4 C;
flat in vec4 D;
in float rawVertexAlpha;
flat in mat3 reflMatrix;

out vec4 fragColor;

#include "uniforms.glsl"
#include "lookdev_fog.glsl"
#include "cell_lights.glsl"
#include "water_info.glsl"
#if defined( WW_WATER_DEPTH ) || defined( WW_WATER_CAUSTICS )
#include "water_depth.glsl"
#endif
#ifdef WW_WATER_CLEAR	// lane WATERLOOK1 step 8 (WATERCOLOR1): the water's own colour by body class (after water_depth)
#include "water_clear.glsl"
#endif
#ifdef WW_WATER_CAUSTICS	// lane WATERCAUSTICS1: the sun's cascades at the bed, then the caustic law
#include "ww_sunshadow.glsl"
#include "water_caustics.glsl"
#endif
#ifdef WW_WATER_SHORE	// lane WATERSHORE1: fo4_waterlook.prog only (fo4_water.prog keeps its machine code)
#include "water_shore.glsl"
#endif

uniform vec4 waterMat[13];	// shallow, deep, reflection, underwater, light silt, dark silt, var, p1..p4, amplitude, uv scale
uniform vec4 waterSky[3];	// horizon, lower, upper (linear)
uniform vec4 waterScroll0;	// the noise layers' offsets (0 in the cell view)
uniform vec4 waterScroll1;
uniform sampler2D waterNoise0;
uniform sampler2D waterNoise1;
uniform sampler2D waterNoise2;
uniform sampler2D waterScene;	// the frame behind the water, the viewport's corner at texel 0
uniform sampler2D waterDepth;	// the opaque pass's depth, likewise
uniform bool waterHaveDepth;
uniform int waterRed;		// 1 norefl, 2 nofresnel, 4 nosilt, 8 nospec, 16 noshore, 32 nonormal,
				// 64 nodepthfog, 128 nofar (the noise sampler's, in cellwater.cpp), 256 nossr, 512 fogunits
uniform bool waterSsrOn;	// lane WATER2: this frame's reflections are bound (half the view, top row first)
uniform sampler2D waterSsrRaw;	// the march
uniform sampler2D waterSsrFin;	// the march blurred
uniform int waterSslr;		// 1: the reflection-ray pass (waterDepth is then the obscurance pass's linear depth)
uniform int waterProbe;
#ifdef WW_WATER_WAVES	// lane WATERWAVES1: fo4_waterlook.prog only (fo4_water.prog keeps its machine code)
#include "water_waves.glsl"
#endif
#ifdef WW_WATER_REFL	// lane WATERREFL1: fo4_waterlook.prog only (fo4_water.prog keeps its machine code)
#include "water_refl.glsl"
// lane WATERLOOK1 step 9 (WATERGLINT1): the glare as counted facets
#include "water_glitter.glsl"
#endif
#ifdef WW_WATER_OCEAN	// lane WATERLOOK1: fo4_waterlook.prog only (fo4_water.prog keeps its machine code)
#include "water_ocean.glsl"
#include "water_ripple.glsl"
#include "water_sss.glsl"
#include "water_flowwaves.glsl"
#endif
#ifdef WW_WATER_DISP	// lane WATERLOOK1 step 2: fo4_waterlookd.prog, the displaced water: the undisplaced world xy (u)
in vec2 wwOcBase;
#endif

vec3 tonemap( vec3 x )	// fo4_default.frag's
{
	float a = 0.15;
	float b = 0.50;
	float c = 0.10;
	float d = 0.20;
	float e = 0.02;
	float f = 0.30;

	vec3 z = x * x * D.a * (A.a * 4.22978723);
	z = (z * (a * z + b * c) + d * e) / (z * (a * z + b) + d * f) - e / f;
	return sqrt(z / (A.a * 0.93333333));
}

// the inverse of tonemap() above: a stored frame value back to linear light
vec3 untonemap( vec3 y )
{
	const float a = 0.15, b = 0.50, c = 0.10, d = 0.20, e = 0.02, f = 0.30;
	vec3 k = min( y * y * ( A.a * 0.93333333 ) + e / f, vec3( 0.99999 ) );
	vec3 qa = a * ( k - 1.0 );
	vec3 qb = b * ( k - c );
	vec3 qc = d * ( f * k - e );
	vec3 z = ( -qb - sqrt( max( qb * qb - 4.0 * qa * qc, vec3( 0.0 ) ) ) ) / ( 2.0 * qa );
	return max( z, vec3( 0.0 ) ) / ( D.a * A.a * 4.22978723 );
}

// the frame stores what the cell programs wrote: linear (the HDR frame), or after the curve
vec3 frameToLinear( vec3 t )
{
	if ( cellOn && cellIsOn )
		return cellIsLinear ? t : pow( max( t, vec3( 0.0 ) ), vec3( 2.2 ) );	// the imagespace: approximate (interiors)
	return untonemap( t );
}

vec3 noiseN( sampler2D s, vec2 uv, out vec2 tap )
{
	tap = texture( s, uv ).xy;
	vec2 t = tap * 2.0 - 1.0;
	return vec3( t, sqrt( 1.0 - min( dot( t, t ), 1.0 ) ) );
}

vec3 fix24( vec3 v, int b )	// byte b (0 = high) of 24-bit fixed
{
	vec3 q = floor( clamp( v, 0.0, 1.0 ) * 16777215.0 + 0.5 );
	return b == 0 ? floor( q / 65536.0 ) / 255.0 : b == 1 ? mod( floor( q / 256.0 ), 256.0 ) / 255.0 : mod( q, 256.0 ) / 255.0;
}

float smooth01( float x )
{
	return x * x * ( 3.0 - 2.0 * x );
}

// lane WATER2: the game's depth-colour pass (02146) over a scene texel, linear
vec3 depthFog( vec3 c, float along, float vert )
{
	if ( ( waterRed & 64 ) != 0 )
		return c;
	vec4 P1 = waterMat[7], P3 = waterMat[9], P4 = waterMat[10];
	float k = ( waterRed & 512 ) != 0 ? 0.0142875 : 1.0;	// the red: the distances taken in metres
#ifdef WW_WATER_CLEAR	// lane WATERLOOK1 step 8: absorption + lit in-scatter replace the WATR's ramp
	if ( waterClearOn )
		return wwWaterClear( c, along, vert );
#endif
	float rzF = clamp( 1.0 - vert * k / P1.w, 0.0, 1.0 );
	float rwF = clamp( 1.0 - along * k / P1.w, 0.0, 1.0 );
	float xf = clamp( ( rwF - P3.y ) / ( P3.x - P3.y ), 0.0, 1.0 );
	float a = pow( max( 1.0 - ( 3.0 - 2.0 * xf ) * xf * xf, 0.0 ), 0.33 ) * ( P3.w - P3.z ) + P3.z;
	float xc = smooth01( clamp( ( rzF - P4.y ) / ( P4.x - P4.y ), 0.0, 1.0 ) );
#ifdef WW_WATER_DEPTH	// lane WATERDEPTH1 (WATERCOMBINE1: its own row's uniform)
	if ( waterDepthOn )
		return wwWaterDepthMix( c, mix( waterMat[1].rgb, waterMat[0].rgb, xc ), a, along );
#endif
	return mix( c, mix( waterMat[1].rgb, waterMat[0].rgb, xc ), a );
}

vec3 hi16( vec3 v )
{
	vec3 q = floor( clamp( v, 0.0, 1.0 ) * 65535.0 + 0.5 );
	return floor( q / 256.0 ) / 255.0;
}

vec3 lo16( vec3 v )
{
	vec3 q = floor( clamp( v, 0.0, 1.0 ) * 65535.0 + 0.5 );
	return mod( q, 256.0 ) / 255.0;
}

void main()
{
	vec4 Var = waterMat[6], P1 = waterMat[7], P2 = waterMat[8], P3 = waterMat[9], P4 = waterMat[10];
	vec4 Amp = waterMat[11], UV = waterMat[12];
	vec3 posView = -ViewDir;
	vec3 P = cellWorldPos( posView );
	vec3 eye = cellWorldPos( vec3( 0.0 ) );
	vec3 ray = P - eye;
	float dist = length( ray );
	vec3 V = ray / max( dist, 1e-6 );
	bool under = eye.z < P.z;

	float dfade = clamp( ( dist - 8192.0 ) / ( P1.x - 8192.0 ), 0.0, 1.0 );

	// the opaque scene point behind this pixel
	ivec2 px = ivec2( gl_FragCoord.xy ) - viewportDimensions.xy;
	float vert = 1e9, along = 1e9;
#ifdef WW_WATER_OCEAN
	vec3 ripS = P;	// lane WATERLOOK1 step 3: the opaque point behind, world
#endif
#ifdef WW_WATER_CAUSTICS
	vec3 cauSv = vec3( 0.0 );	// lane WATERCAUSTICS1: the scene point behind, view space; its factor and probe values
	vec3 cauDbg = vec3( 1.0, 0.0, 0.0 );
	float cauC = 1.0;
#endif
	if ( waterSslr == 1 ) {
		// the obscurance pass's target: .a = the opaque scene's view depth (game units), 0 where nothing drew
		float gz = texelFetch( waterDepth, ivec2( gl_FragCoord.xy ), 0 ).a;
		if ( gz > 0.0 ) {
			vec3 S = cellWorldPos( posView * ( gz / max( -posView.z, 1e-6 ) ) );
			vert = abs( P.z - S.z );
			along = length( S - P );
#ifdef WW_WATER_OCEAN
			ripS = S;
#endif
		}
	} else if ( waterHaveDepth ) {
		float dz = texelFetch( waterDepth, px, 0 ).r;
		if ( dz < 1.0 ) {
			float sceneW = projectionMatrix[3][2] / ( projectionMatrix[2][2] + ( 2.0 * dz - 1.0 ) );
			float fragW = 1.0 / gl_FragCoord.w;
			vec3 S = cellWorldPos( posView * ( sceneW / max( fragW, 1e-6 ) ) );
			vert = abs( P.z - S.z );
			along = length( S - P );
#ifdef WW_WATER_OCEAN
			ripS = S;
#endif
#ifdef WW_WATER_CAUSTICS
			cauSv = posView * ( sceneW / max( fragW, 1e-6 ) );
#endif
		}
	}
	float rz = clamp( 1.0 - vert / P1.w, 0.0, 1.0 );
	float rw = clamp( 1.0 - along / P1.w, 0.0, 1.0 );

	float x = clamp( ( rw - P3.y ) / ( P3.x - P3.y ), 0.0, 1.0 );
	float alphaV = pow( max( 1.0 - ( 3.0 - 2.0 * x ) * x * x, 0.0 ), 0.33 ) * ( P3.w - P3.z ) + P3.z;
	float shore = ( waterRed & 16 ) != 0 ? 0.0 : smooth01( clamp( ( rz - P2.x ) / ( 1.0 - P2.x ), 0.0, 1.0 ) );
#ifdef WW_WATER_SHORE
	// lane WATERSHORE1: the baked shore distance also fades the edge, so far water (no depth term) fades as near does
	vec2 shoreField = waterShoreOn != 0 ? wwShoreField( P.xy ) : vec2( 0.0 );
	// lane WATERFOAMDETAIL1: the soft contact from the water path to the scene behind and the bake (law 1), red: the bake
	float shoreEdgeV = ( waterShoreOn & 1 ) == 0 ? 0.0
		: waterShoreEdgeLaw == 1 ? wwShoreEdgeSoft( shoreField, along, V.z ) : wwShoreEdge( shoreField );
	// lane WATERFOAMDETAIL1: the foam by the wave energy at the shore (waterShoreLaw 1), the old band as its red (0)
	float shoreFoot = max( length( dFdx( P.xy ) ), length( dFdy( P.xy ) ) );
	float shoreColFoot = fwidth( min( vert, 4096.0 ) );
	vec3 shoreFoamDbg = vec3( 0.0 );
	float shoreFoamV = ( waterShoreOn & 2 ) == 0 || under ? 0.0
		: waterShoreLaw == 1 ? wwShoreFoamWave( shoreField, P.xy, waterShoreTime, vert, shoreColFoot, shoreFoot, shoreFoamDbg )
		: wwShoreFoam( shoreField, P.xy, waterShoreTime );
	if ( ( waterRed & 16 ) == 0 )
		shore = max( shore, shoreEdgeV );
#endif
	float flatten = ( 1.0 - smooth01( clamp( ( rz - P2.w ) / ( P2.z - P2.w ), 0.0, 1.0 ) ) ) * P2.y;

	// the three noise layers
	vec2 wxy = P.xy;
	vec2 tap0, tap1, tap2;
	vec3 n1 = noiseN( waterNoise0, wxy / UV.x + waterScroll0.xy, tap0 );
	vec3 n2 = noiseN( waterNoise1, wxy / UV.y + waterScroll0.zw, tap1 );
	vec3 n3 = noiseN( waterNoise2, wxy / UV.z + waterScroll1.xy, tap2 );
	vec3 N = ( n1 - vec3( 0.0, 0.0, 1.0 ) ) * Amp.x + vec3( 0.0, 0.0, 1.0 );
	N += ( n2 * Amp.y + n3 * Amp.z ) * dfade;
	N = normalize( N );
#ifdef WW_WATER_WAVES
	// lane WATERWAVES1: the swell's slopes add to the noise layers' (slope-space blend), before the game's flatten
	float swellFoot = max( length( dFdx( P.xy ) ), length( dFdy( P.xy ) ) );
	vec4 swell = waterWavesOn != 0 ? wwWaterSwell( P.xy, waterWavesK.z, swellFoot ) : vec4( 0.0 );
	if ( swell.z > 0.0 )
		N = normalize( vec3( N.xy / max( N.z, 1e-3 ) + swell.xy, 1.0 ) );
#endif
#ifdef WW_WATER_OCEAN
	// lane WATERLOOK1: on a baked body the wind's spectrum replaces the noise layers and the swell (slope space)
#ifdef WW_WATER_DISP
	vec2 ocP = wwOcBase;	// the spectrum is defined on the undisplaced grid
#else
	vec2 ocP = P.xy;
#endif
	vec2 ocDx = dFdx( ocP ), ocDy = dFdy( ocP );
	WwOcean oc = waterOceanOn ? wwWaterOceanFlowed( ocP, ocDx, ocDy ) : WwOcean( vec2( 0.0 ), 0.0, 0.0, 0.0, 1.0 );
	if ( oc.w > 0.0 )
		N = normalize( vec3( mix( N.xy / max( N.z, 1e-3 ), oc.slope, oc.w ), 1.0 ) );
	// step 3: the incident short waves reflect off what stands in the water (red norip); uniform flow (derivatives)
	if ( waterOceanOn && ( waterOceanRed & 64 ) == 0 ) {
		vec3 rip = wwContactRippleAt( vert, P, ripS, eye, oc.w * sqrt( oc.var ) );
		N = normalize( vec3( N.xy / max( N.z, 1e-3 ) + rip.xy, 1.0 ) );
		oc.var += rip.z;
	}
#endif
	vec3 N0 = N;
	N = normalize( flatten * ( N - vec3( 0.0, 0.0, 1.0 ) ) + vec3( 0.0, 0.0, 1.0 ) );
#ifdef WW_WATER_REFL	// lane WATERREFL1: GGX alpha^2 from the slopes the filtered noise lost + the footprint
	float reflA2 = waterReflOn || waterGlintOn ? wwWaterAlpha2( wxy / UV.x + waterScroll0.xy, wxy / UV.y + waterScroll0.zw,
		wxy / UV.z + waterScroll1.xy, tap0, tap1, tap2, Amp, dfade, flatten, Var.x, N, ( waterGlintRed & 2 ) == 0 ) : 0.0;
#endif
#if defined( WW_WATER_OCEAN ) && defined( WW_WATER_REFL )
	// lane WATERLOOK1: on a baked body the roughness is the spectrum's: the slopes the mips lost, the tail, the footprint
	if ( waterOceanOn && ( waterReflOn || waterGlintOn ) ) {
		vec2 ocS = N.xy / max( N.z, 0.05 );
		vec2 ocSx = dFdx( ocS ), ocSy = dFdy( ocS );
		bool ocLean = ( waterGlintRed & 2 ) == 0;
		float ocFoot = ocLean ? min( 0.5 * ( dot( ocSx, ocSx ) + dot( ocSy, ocSy ) ), waterGlintK.z ) : 0.0;
		reflA2 = mix( reflA2, 1e-4 + ( ocLean ? oc.var * flatten * flatten : 0.0 ) + ocFoot, oc.w );
	}
#endif

	if ( waterSslr == 1 ) {
		// the game's water ray pass (02100): half the flattening, the ray into the view, the 0.2 gate
		vec3 Nr = normalize( 0.5 * flatten * ( N0 - vec3( 0.0, 0.0, 1.0 ) ) + vec3( 0.0, 0.0, 1.0 ) );
		vec3 Rw = reflect( V, Nr );
		vec3 Rv = vec3( dot( Rw, cellWorldDir( vec3( 1.0, 0.0, 0.0 ) ) ), dot( Rw, cellWorldDir( vec3( 0.0, 1.0, 0.0 ) ) ),
			dot( Rw, cellWorldDir( vec3( 0.0, 0.0, 1.0 ) ) ) );
		fragColor = vec4( -Rv.z > 0.2 && !under ? Rv : vec3( 0.0 ), max( -posView.z, 1e-3 ) );
		return;
	}
	if ( ( waterRed & 32 ) != 0 )
		N = vec3( 0.0, 0.0, 1.0 );
	vec3 Ns = under ? -N : N;	// INFERRED: from below the surface faces the eye

	float c5 = 1.0 - clamp( dot( -V, Ns ), 0.0, 1.0 );
	float F = ( waterRed & 2 ) != 0 ? P4.z : P4.z + ( 1.0 - P4.z ) * c5 * c5 * c5 * c5 * c5;

	vec3 R = reflect( V, Ns );
	vec3 sky = mix( waterSky[0].rgb, waterSky[1].rgb, clamp( R.z + 0.75, 0.0, 1.0 ) );
	sky = mix( sky, waterSky[2].rgb, clamp( R.z * 1.9 + 0.35, 0.0, 1.0 ) );
#ifdef WW_WATER_REFL
	vec3 reflSky = waterReflOn ? wwWaterCloudSky( sky, P, R ) : sky;	// lane WATERREFL1: before the game's SSR blend
	vec4 reflRaw = vec4( 0.0 ), reflFin = vec4( 0.0 );
#endif
	// lane WATER2: the game's screen-space reflection over the sky (02102 lines 146-152)
	vec4 ssr = vec4( 0.0 );
	if ( waterSsrOn && !under ) {
		vec2 suv = clamp( ( gl_FragCoord.xy - vec2( viewportDimensions.xy ) ) / vec2( viewportDimensions.zw ), 0.0, 1.0 );
		vec2 st = vec2( suv.x, 1.0 - suv.y );
		ssr = mix( textureLod( waterSsrRaw, st, 0.0 ), textureLod( waterSsrFin, st, 0.0 ), 0.9 );
#ifdef WW_WATER_REFL
		reflRaw = textureLod( waterSsrRaw, st, 0.0 );
		reflFin = textureLod( waterSsrFin, st, 0.0 );
#endif
		if ( ( waterRed & 256 ) == 0 )
			sky = mix( sky, ssr.rgb, ssr.a );
	}

	// the scene behind: in place, and offset by the normal (the game's shore tap)
	ivec2 size = textureSize( waterScene, 0 );
	vec2 uv = ( vec2( px ) + 0.5 ) / vec2( size );
	vec2 offUV = uv + vec2( -N.x, -N.y ) * ( N.z * 0.0625 );
	ivec2 opx = clamp( ivec2( offUV * vec2( size ) ), ivec2( 0 ), size - 1 );
	vec3 texIn = texelFetch( waterScene, px, 0 ).rgb;
	vec3 texOff = texelFetch( waterScene, opx, 0 ).rgb;
	vec3 refr = frameToLinear( texIn );
	vec3 refrOff = frameToLinear( texOff );
#ifdef WW_WATER_SHORE
	vec3 refrIn = refr;	// lane WATERFOAMDETAIL1: the scene at this very pixel, before any water colour
#endif
	// lane WATER2: the depth-colour pass already drawn into the copy: here at this pixel's own distances, at the
	// offset texel at that texel's (its view ray met with the water's plane, where the water stands before the scene)
	float offAlong = 0.0, offVert = 0.0, offHit = 0.0;
#ifdef WW_WATER_CAUSTICS
	// lane WATERCAUSTICS1: the sun's visibility at the bed (uniform flow: the cascades take derivatives)
	float cauSh = waterCauOn && waterCauShadow ? wwSunShadow( cauSv ) : 1.0;
	// lane CAUSTICPATTERN1: the bed's footprint per pixel (u), widening the net's lines so they never sparkle
	float cauFw = min( length( fwidth( cellWorldPos( cauSv ).xy ) ), 64.0 );
#endif
#ifdef WW_WATER_DEPTH
	WwWaterBed bed = WwWaterBed( false, false, 1e9, 1e9 );	// lane WATERDEPTH1: the bed depth this pixel uses
#ifdef WW_WATER_CLEAR	// lane WATERLOOK1 step 8: the clear colour reads the same bed, lit by this frame's sun and sky
	if ( waterClearOn )
		wwWaterClearSetup( P, V, D.rgb * D.rgb, normalize( cellWorldDir( LightDir ) ), waterSky[0].rgb, waterSky[2].rgb );
	if ( waterDepthOn || waterClearOn )
#else
	if ( waterDepthOn )
#endif
		bed = wwWaterBedAt( P, V, vert, along );
#endif
	if ( !under && waterSslr == 0 ) {
#ifdef WW_WATER_CAUSTICS
		// lane WATERCAUSTICS1: under a baked body, on the drawn bed (not the bake's), before the depth colour takes it
		float cauBd = waterCauOn ? wwWaterBedDepth( P ) : -1.0;
#ifdef WW_WATER_OCEAN
		wwCauPathGain = mix( 1.0, oc.cau, oc.w );	// lane WATERLOOK1: the net focuses at the spectrum's own curvature
#endif
		if ( cauBd >= 0.0 && vert < 1e8 && vert <= cauBd + 256.0 ) {
			vec3 cauC3 = wwWaterCaustic( cellWorldPos( cauSv ), vert, normalize( cellWorldDir( LightDir ) ), D.rgb * D.rgb,
				0.5 * ( waterSky[0].rgb + waterSky[2].rgb ), cauSh, cauFw, cauDbg );
			cauC = cauC3.g;	// CAUSTICPATTERN1: per channel (the edges' colour split); the probes read green
			refr *= cauC3;
		}
#endif
#ifdef WW_WATER_DEPTH
#ifdef WW_WATER_CLEAR	// lane WATERLOOK1 step 8: no bed drawn = the class's bed albedo, lit
		refr = bed.baked ? depthFog( waterClearOn ? wwWaterClearBed() : waterMat[0].rgb, bed.along, bed.vert )
			: depthFog( refr, along, vert );
#else
		refr = bed.baked ? depthFog( waterMat[0].rgb, bed.along, bed.vert ) : depthFog( refr, along, vert );
#endif
#else
		refr = depthFog( refr, along, vert );
#endif
		vec2 ndc = ( vec2( opx ) + 0.5 ) / vec2( size ) * 2.0 - 1.0;
		float odz = waterHaveDepth ? texelFetch( waterDepth, opx, 0 ).r : 1.0;
		mat4 iP = inverse( projectionMatrix );
		vec4 h = iP * vec4( ndc, 2.0 * min( odz, 0.999999 ) - 1.0, 1.0 );
		vec3 Sv = h.xyz / h.w;
		vec3 Sw = cellWorldPos( Sv );
		vec3 dW = normalize( Sw - eye );
		float t = abs( dW.z ) > 1e-6 ? ( P.z - eye.z ) / dW.z : -1.0;
		bool sceneAt = odz < 1.0;
		if ( t > 0.0 && ( !sceneAt || t < length( Sw - eye ) ) ) {
			vec3 Wp = eye + dW * t;
			offAlong = sceneAt ? length( Sw - Wp ) : 1e9;
			offVert = sceneAt ? abs( Wp.z - Sw.z ) : 1e9;
			offHit = 1.0;
			refrOff = depthFog( refrOff, offAlong, offVert );
		}
	}

	vec3 silted = refr;
#ifdef WW_WATER_CLEAR	// lane WATERLOOK1 step 8: the in-scatter is the water's murk; the WATR's silt is not added on top
	if ( ( waterRed & 4 ) == 0 && !waterClearOn )
#else
	if ( ( waterRed & 4 ) == 0 )
#endif
		silted = refr + waterMat[4].w * ( 1.0 - alphaV ) * ( mix( waterMat[5].rgb, waterMat[4].rgb, refr ) - refr );

	vec3 Ldir = normalize( cellWorldDir( LightDir ) );
	vec3 sun = D.rgb * D.rgb;
	vec3 spec = sun * pow( clamp( dot( R, Ldir ), 0.0, 1.0 ), Var.x ) * waterMat[1].w
		+ sun * pow( clamp( dot( N, vec3( -0.099, -0.099, 0.99 ) ), 0.0, 1.0 ), waterMat[0].w ) * P1.z;
#ifdef WW_WATER_REFL
	// lane WATERREFL1: the sun's glare from the roughness (no fixed-direction sparkle); red game keeps today's spec
	vec3 glare = vec3( 0.0 );
	float glFoot = length( cross( dFdx( P ), dFdy( P ) ) );	// lane WATERLOOK1 step 9: the footprint (u^2), uniform flow
	if ( waterGlintOn && ( waterGlintRed & 1 ) == 0 && !under ) {
		// step 9: the sun's disc in the lobe; the mean glare x the facets counted in this pixel, then the shoulder
		float glA2 = max( reflA2, 1e-6 ) + wwGlitSunA2();
		vec3 glH = normalize( Ldir - V );
		float glX = wwWaterGlare( Ns, V, Ldir, vec3( 1.0 ), glA2, waterGlintK.y, false ).x
			* wwWaterGlitter( P.xy, glFoot, glH.xy / max( glH.z, 1e-3 ) - Ns.xy / max( Ns.z, 1e-3 ), glA2, dot( glH, Ns ) );
		if ( ( waterGlintRed & 4 ) == 0 )
			glX = glX / ( 1.0 + glX / waterGlintK.x );
		glare = sun * glX;
		spec = glare;
	}
	float reflAlpha = sqrt( max( reflA2, 0.0 ) );
	float reflF = wwWaterFresnel( dot( -V, Ns ), reflAlpha, waterGlintK.y );
	vec3 reflEnv = wwWaterReflEnv( reflSky, reflRaw, reflFin, waterSsrOn && !under && ( waterRed & 256 ) == 0, reflAlpha );
#endif
	if ( ( waterRed & 8 ) != 0 )
		spec = vec3( 0.0 );

	vec3 lin;
	if ( under ) {
		// the game's underwater variant: half the sky and half the underwater colour, the scene by 1 - F
		vec3 refl = 0.5 * ( sky + waterMat[3].rgb );
		lin = mix( refl, refr, 1.0 - F );
	} else {
		float wRefl = ( waterRed & 1 ) != 0 ? 0.0 : F * Var.y;
		vec3 far = mix( waterMat[2].rgb, sky, waterMat[2].w );
#ifdef WW_WATER_DEPTH
		far = wwWaterDepthFar( far, silted, F, bed.known );	// lane WATERDEPTH1
#endif
		lin = mix( silted, sky, wRefl );
		lin = mix( far, lin, dfade ) + spec;
#ifdef WW_WATER_REFL
		if ( waterReflOn && ( waterRed & 1 ) == 0 )	// lane WATERREFL1: physical Fresnel, near law to the horizon
			lin = wwWaterReflMix( silted, reflEnv, reflF, F, Var.y, far, dfade ) + spec;
#endif
		lin = mix( lin, refrOff, shore );
#ifdef WW_WATER_SHORE
		/* lane WATERFOAMDETAIL1: at the contact the water path is 0, so no depth colour, murk or shifted tap either: the
		 * pixel becomes the scene it covers and the dry side continues without a seam */
		if ( waterShoreEdgeLaw == 1 && ( waterRed & 16 ) == 0 )
			lin = mix( lin, refrIn, shoreEdgeV );
		/* lane WATERSHORE1: the foam, a Lambert white of albedo 0.55 (fresh sea foam, Koepke 1984, Applied Optics
		 * 23(11)) lit by the sun and the water's upper sky row; opacity K1.w lets the water read through its holes */
		vec3 foamLin = 0.55 * ( sun * clamp( Ldir.z, 0.0, 1.0 ) + waterSky[2].rgb );
		lin = mix( lin, foamLin, shoreFoamV * waterShoreK1.w );
#endif
#ifdef WW_WATER_OCEAN
		// lane WATERLOOK1 step 4: sunlight through the crests, the body colour, near faces and far mean alike (red noglow)
		if ( waterOceanOn && ( waterOceanRed & 128 ) == 0 && oc.w > 0.0 )
			lin += ( 1.0 - F ) * oc.w * wwWaterCrestGlow( N0.xy / max( N0.z, 1e-3 ), oc.var, V, Ldir, sun, waterMat[0].rgb );
		/* lane WATERLOOK1: whitecaps where the chop folds the surface (its Jacobian, Tessendorf 2001 section 4.4), from
		 * 4 m/s; lit as WATERSHORE1's foam (albedo 0.55, Koepke 1984) */
		if ( oc.white > 0.0 )
			lin = mix( lin, 0.55 * ( sun * clamp( Ldir.z, 0.0, 1.0 ) + waterSky[2].rgb ), 0.8 * oc.white );
#endif
	}
	lin = max( lin, vec3( 0.0 ) );
	if ( fogOn || wwApOn() )	// lane SKYA: the sky's aerial perspective with the fog off too
		lin = max( wwFog( lin, posView ), vec3( 0.0 ) );

	vec3 s = sqrt( lin );
	vec3 outC = ( cellOn && cellIsOn ) ? ( cellIsLinear ? lin : cellImageSpace( s ) ) : tonemap( s );
	fragColor = vec4( outC, 1.0 );

	if ( waterProbe > 0 ) {
		vec3 o = vec3( 0.0 );
		if ( waterProbe == 1 || waterProbe == 2 )
			o = waterProbe == 1 ? hi16( N * 0.5 + 0.5 ) : lo16( N * 0.5 + 0.5 );
		else if ( waterProbe == 3 || waterProbe == 4 )
			o = waterProbe == 3 ? hi16( V * 0.5 + 0.5 ) : lo16( V * 0.5 + 0.5 );
		else if ( waterProbe == 5 || waterProbe == 6 )
			o = waterProbe == 5 ? hi16( vec3( rz, rw, dfade ) ) : lo16( vec3( rz, rw, dfade ) );
		else if ( waterProbe == 7 )
			o = texIn;
		else if ( waterProbe == 8 )
			o = texOff;
		else if ( waterProbe == 9 || waterProbe == 10 ) {
			int col = int( gl_FragCoord.x ) % 4;
			vec3 v = col == 0 ? sun * 0.25 : col == 1 ? Ldir * 0.5 + 0.5
				: col == 2 ? vec3( A.a / 8.0, D.a / 8.0, under ? 1.0 : 0.0 )
				: vec3( ( cellOn && cellIsOn ) ? 1.0 : 0.0, cellIsLinear ? 1.0 : 0.0, fogOn ? 1.0 : 0.0 );
			o = waterProbe == 9 ? hi16( v ) : lo16( v );
		} else if ( waterProbe == 11 || waterProbe == 12 )
			o = waterProbe == 11 ? hi16( vec3( tap0, tap1.x ) ) : lo16( vec3( tap0, tap1.x ) );
		else if ( waterProbe == 13 || waterProbe == 14 )
			o = waterProbe == 13 ? hi16( vec3( tap1.y, tap2 ) ) : lo16( vec3( tap1.y, tap2 ) );
		else if ( waterProbe == 15 || waterProbe == 16 )
			o = waterProbe == 15 ? hi16( ssr.rgb ) : lo16( ssr.rgb );
		else if ( waterProbe == 17 || waterProbe == 18 )
			o = waterProbe == 17 ? hi16( vec3( ssr.a, 0.0, 0.0 ) ) : lo16( vec3( ssr.a, 0.0, 0.0 ) );
		else if ( waterProbe >= 19 && waterProbe <= 21 )
			o = fix24( vec3( dist / 16384.0, along / 4096.0, vert / 4096.0 ), waterProbe - 19 );
		else if ( waterProbe >= 22 && waterProbe <= 24 )
			o = fix24( vec3( offAlong / 4096.0, offVert / 4096.0, offHit ), waterProbe - 22 );
		else if ( waterProbe == 25 || waterProbe == 26 )	// WATERHDR1: the linear light this pixel writes, / 4
			o = waterProbe == 25 ? hi16( lin * 0.25 ) : lo16( lin * 0.25 );
		else if ( waterProbe == 27 )	// WATERHDR1: the water's pixels, magenta
			o = vec3( 1.0, 0.0, 1.0 );
#ifdef WW_WATER_INFO_PROBE
		else if ( waterProbe >= 28 && waterProbe <= 31 ) {	// WATERDATA1: the baked water info at this pixel
			uint w = wwWaterInfoWord( P.xy );
			vec4 wi = wwWaterInfo( P.xy );
			uint id = w & 0xFFFFu;
			o = waterProbe == 28 ? vec3( float( ( w >> 16 ) & 0xFFu ), float( w >> 24 ), waterInfoOn ? 255.0 : 0.0 ) / 255.0
				: waterProbe == 29 ? vec3( float( id >> 8 ), float( id & 0xFFu ), wi.w + 1.0 ) / 255.0
				: waterProbe == 30 ? ( id != 0u ? vec3( 0.0, 1.0, 1.0 ) : vec3( 1.0, 0.0, 0.0 ) )
				: vec3( clamp( wi.y / 1024.0, 0.0, 1.0 ) );
		}
		else if ( waterProbe == 41 ) {	// WATERFLOWBAKE1: the baked flow at this pixel, u/s (32 in its lane)
			vec2 fv = wwWaterFlow( P.xy );
			o = vec3( clamp( fv * 0.125 + 0.5, 0.0, 1.0 ), waterFlowOn ? 1.0 : 0.0 );
		}
#endif
#ifdef WW_WATER_DEPTH
		else if ( waterProbe == 32 || waterProbe == 33 )	// WATERDEPTH1: 32 the depth used / 1024 grey; 33 baked cyan, scene green, none red
			o = waterProbe == 32 ? vec3( clamp( bed.vert / 1024.0, 0.0, 1.0 ) )
				: bed.baked ? vec3( 0.0, 1.0, 1.0 ) : bed.known ? vec3( 0.0, 1.0, 0.0 ) : vec3( 1.0, 0.0, 0.0 );
#endif
#ifdef WW_WATER_CAUSTICS
		else if ( waterProbe == 35 || waterProbe == 36 )	// WATERCAUSTICS1: 35 the factor / 2 grey; 36 (I / 8, env, share)
			o = waterProbe == 35 ? vec3( clamp( cauC * 0.5, 0.0, 1.0 ) ) : vec3( cauDbg.x / 8.0, cauDbg.y, cauDbg.z );
#endif
#ifdef WW_WATER_SHORE
		else if ( waterProbe == 37 )	// WATERSHORE1: (edge, foam, shore distance / 1024) at this pixel
			o = vec3( shoreEdgeV, shoreFoamV, clamp( shoreField.x / 1024.0, 0.0, 1.0 ) );
		else if ( waterProbe == 38 )	// WATERFOAMDETAIL1: (Hs / 64 u, the energy amount, the column used / 64 u)
			o = clamp( shoreFoamDbg, 0.0, 1.0 );
#endif
#ifdef WW_WATER_REFL
		else if ( waterProbe == 44 )	// WATERREFL1: (F, sqrt( alpha ) / 0.5, SSR confidence); 45/46 the glare / 16
			o = vec3( reflF, clamp( sqrt( reflAlpha ) * 2.0, 0.0, 1.0 ), clamp( mix( reflRaw.a, reflFin.a, 0.5 ), 0.0, 1.0 ) );
		else if ( waterProbe == 45 || waterProbe == 46 )
			o = waterProbe == 45 ? hi16( glare / 16.0 ) : lo16( glare / 16.0 );
#endif
#ifdef WW_WATER_WAVES
		else if ( waterProbe == 34 )	// WATERWAVES1: (0.5 + 4 x the swell's slope x, y; its weight) at this pixel
			o = vec3( clamp( 0.5 + 4.0 * swell.xy, 0.0, 1.0 ), swell.z );
#endif
#ifdef WW_WATER_OCEAN
		else if ( waterProbe == 39 || waterProbe == 40 )	// WATERLOOK1: the spectrum (header)
			o = waterProbe == 39 ? vec3( clamp( 0.5 + 4.0 * oc.slope, 0.0, 1.0 ), oc.w )
				: vec3( oc.white, sqrt( oc.var ) / 0.5, oc.cau / 3.0 );
#endif
		fragColor = vec4( o, 1.0 );
	}
}

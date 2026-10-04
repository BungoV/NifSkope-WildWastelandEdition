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
 * The sun is the frame's light 0 (its direction and colour, the one the cell's surfaces take).
 *
 * Probes (WW_CELL_WATER_PROBE, opaque, raw; "hi/lo" = a 16-bit value's high and low byte):
 *   1/2 N x 0.5 + 0.5   3/4 V x 0.5 + 0.5   5/6 (rz, rw, dfade)   7 the scene texel behind   8 the offset texel
 *   9/10 the frame constants by column (x mod 4): 0 sun colour / 4, 1 sun direction x 0.5 + 0.5, 2 (A.a / 8, D.a / 8,
 *   under the surface), 3 (cellIsOn, cellIsLinear, fogOn) */

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
uniform int waterRed;		// 1 norefl, 2 nofresnel, 4 nosilt, 8 nospec, 16 noshore, 32 nonormal
uniform int waterProbe;

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

vec3 noiseN( sampler2D s, vec2 uv )
{
	vec2 t = texture( s, uv ).xy * 2.0 - 1.0;
	return vec3( t, sqrt( 1.0 - min( dot( t, t ), 1.0 ) ) );
}

float smooth01( float x )
{
	return x * x * ( 3.0 - 2.0 * x );
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
	if ( waterHaveDepth ) {
		float dz = texelFetch( waterDepth, px, 0 ).r;
		if ( dz < 1.0 ) {
			float sceneW = projectionMatrix[3][2] / ( projectionMatrix[2][2] + ( 2.0 * dz - 1.0 ) );
			float fragW = 1.0 / gl_FragCoord.w;
			vec3 S = cellWorldPos( posView * ( sceneW / max( fragW, 1e-6 ) ) );
			vert = abs( P.z - S.z );
			along = length( S - P );
		}
	}
	float rz = clamp( 1.0 - vert / P1.w, 0.0, 1.0 );
	float rw = clamp( 1.0 - along / P1.w, 0.0, 1.0 );

	float x = clamp( ( rw - P3.y ) / ( P3.x - P3.y ), 0.0, 1.0 );
	float alphaV = pow( max( 1.0 - ( 3.0 - 2.0 * x ) * x * x, 0.0 ), 0.33 ) * ( P3.w - P3.z ) + P3.z;
	float shore = ( waterRed & 16 ) != 0 ? 0.0 : smooth01( clamp( ( rz - P2.x ) / ( 1.0 - P2.x ), 0.0, 1.0 ) );
	float flatten = ( 1.0 - smooth01( clamp( ( rz - P2.w ) / ( P2.z - P2.w ), 0.0, 1.0 ) ) ) * P2.y;

	// the three noise layers
	vec2 wxy = P.xy;
	vec3 n1 = noiseN( waterNoise0, wxy / UV.x + waterScroll0.xy );
	vec3 n2 = noiseN( waterNoise1, wxy / UV.y + waterScroll0.zw );
	vec3 n3 = noiseN( waterNoise2, wxy / UV.z + waterScroll1.xy );
	vec3 N = ( n1 - vec3( 0.0, 0.0, 1.0 ) ) * Amp.x + vec3( 0.0, 0.0, 1.0 );
	N += ( n2 * Amp.y + n3 * Amp.z ) * dfade;
	N = normalize( N );
	N = normalize( flatten * ( N - vec3( 0.0, 0.0, 1.0 ) ) + vec3( 0.0, 0.0, 1.0 ) );
	if ( ( waterRed & 32 ) != 0 )
		N = vec3( 0.0, 0.0, 1.0 );
	vec3 Ns = under ? -N : N;	// INFERRED: from below the surface faces the eye

	float c5 = 1.0 - clamp( dot( -V, Ns ), 0.0, 1.0 );
	float F = ( waterRed & 2 ) != 0 ? P4.z : P4.z + ( 1.0 - P4.z ) * c5 * c5 * c5 * c5 * c5;

	vec3 R = reflect( V, Ns );
	vec3 sky = mix( waterSky[0].rgb, waterSky[1].rgb, clamp( R.z + 0.75, 0.0, 1.0 ) );
	sky = mix( sky, waterSky[2].rgb, clamp( R.z * 1.9 + 0.35, 0.0, 1.0 ) );

	// the scene behind: in place, and offset by the normal (the game's shore tap)
	ivec2 size = textureSize( waterScene, 0 );
	vec2 uv = ( vec2( px ) + 0.5 ) / vec2( size );
	vec2 offUV = uv + vec2( -N.x, -N.y ) * ( N.z * 0.0625 );
	ivec2 opx = clamp( ivec2( offUV * vec2( size ) ), ivec2( 0 ), size - 1 );
	vec3 texIn = texelFetch( waterScene, px, 0 ).rgb;
	vec3 texOff = texelFetch( waterScene, opx, 0 ).rgb;
	vec3 refr = frameToLinear( texIn );
	vec3 refrOff = frameToLinear( texOff );

	vec3 silted = refr;
	if ( ( waterRed & 4 ) == 0 )
		silted = refr + waterMat[4].w * ( 1.0 - alphaV ) * ( mix( waterMat[5].rgb, waterMat[4].rgb, refr ) - refr );

	vec3 Ldir = normalize( cellWorldDir( LightDir ) );
	vec3 sun = D.rgb * D.rgb;
	vec3 spec = sun * pow( clamp( dot( R, Ldir ), 0.0, 1.0 ), Var.x ) * waterMat[1].w
		+ sun * pow( clamp( dot( N, vec3( -0.099, -0.099, 0.99 ) ), 0.0, 1.0 ), waterMat[0].w ) * P1.z;
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
		lin = mix( silted, sky, wRefl );
		lin = mix( far, lin, dfade ) + spec;
		lin = mix( lin, refrOff, shore );
	}
	lin = max( lin, vec3( 0.0 ) );
	if ( fogOn )
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
		}
		fragColor = vec4( o, 1.0 );
	}
}

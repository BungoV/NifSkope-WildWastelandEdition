#version 410 core

/* lane VOLFOG1: the lit medium, froxel by froxel (src/gl/cellvolfog.h, docs/cloud/VOLFOG1_DESIGN.md).
 * One draw writes one layer k of a (columns x rows x slices) volume; the slices are exponential from
 * volSlices.x (near) over ln(far / near) = volSlices.y in volSlices.z steps (game units along the view ray).
 *   stage 1  inject: four equal steps across slice k; each step adds  S(mid) x max(alpha(end) - alpha(start), 0)
 *            where alpha is the game's own fog alpha along this ray (wwFogEval: the density comes from the fog
 *            records) and S is the light the medium scatters toward the eye, in display units (pi x radiance):
 *              the directional light (the weather's sun outdoors, x its cascade shadow; the lighting template's
 *              indoors) x pi x phase, the cell's shaft-emitting lights (a shadow flag and a GDRY, as the game:
 *              colour x radial x cone x their shadow cubes x the GDRY's intensity, no N.L) x pi x phase, and the probe GI read as L0 + L1:  1/4 x max(0, (2/3) sum E + 3 g (E+ - E-).d)
 *              with E = the six-axis cube / pi (the surfaces' GI unit), from the air grid where one was built.
 *            The phase is the god-ray record's (GDRY): air (Rayleigh), forward and back Henyey-Greenstein terms,
 *            their colours normalised to a luminance of 1. rgb = the slice's in-scatter, a = alpha at its far edge.
 *   stage 2  integrate: layer k = the sum of the injected layers 0..k (rgb), a = alpha at slice k's far edge.
 * ( S - S e^{-sigma_t D} ) / sigma_t x T_before (the cloud design's exact slice integral) is S x (alpha1 - alpha0)
 * when T = 1 - alpha: the same formula with sigma_t read from the fog records. */

#define WW_CELL_FX 1
#include "lookdev_fog.glsl"
#include "ww_sunshadow.glsl"
#include "cell_lights.glsl"

uniform int volStage;			// 1 inject, 2 integrate
uniform int volLayer;
uniform ivec3 volDims;			// columns, rows, slices
uniform vec2 volProj;			// the projection's [0][0], [1][1]
uniform sampler3D volInject;	// stage 2 reads stage 1's volume
uniform bool volCsm;			// the sun's cascades are drawn this frame
uniform int volRed;				// 2 gioff, 4 flat, 8 noshadow, 16 wrongsrc (1 off is the CPU's: computed, not applied)
uniform vec4 volPhase[3];		// rgb: the weight of the air (Rayleigh), forward and back terms; w: the HG g (0 for air)
uniform vec4 volGi;				// rgb: the GI's tint (the three weights summed), w: the GI's L1 g
uniform float volK;				// the record's intensity (GDRY Intensity, 1 without a record)
uniform bool volAirOn;			// the air grid (probegi's voxels away from surfaces), else the surface grid
uniform sampler3D volAir;
uniform vec3 volAirOrigin;
uniform float volAirVoxel;
uniform vec3 volAirDims;
uniform vec3 volFlat[6];		// red flat: one cube for every froxel (the grid's mean)
uniform samplerBuffer volEmit;	// the placed lights that draw shafts (a shadow flag AND a WGDR): (light index, its GDRY intensity)
uniform int volEmitCount;
uniform int volTerms;			// gates only: 1 the directional light, 2 the placed lights, 4 the GI (7 = all)

out vec4 fragColor;

const float PI = 3.14159265;

float volHg( float c, float g )
{
	float d = 1.0 + g * g - 2.0 * g * c;
	return ( 1.0 - g * g ) / ( 4.0 * PI * d * sqrt( d ) );
}

// pi x the phase (display units): c = the cosine between the light's travel and the scattered ray's (to the eye)
vec3 volPhasePi( float c )
{
	return PI * ( volPhase[0].rgb * ( 3.0 / ( 16.0 * PI ) ) * ( 1.0 + c * c )
		+ volPhase[1].rgb * volHg( c, volPhase[1].w ) + volPhase[2].rgb * volHg( c, volPhase[2].w ) );
}

// the six-axis cube at P, axis a = +X -X +Y -Y +Z -Z, in the surfaces' GI unit (cellGiE)
vec3 volCube( vec3 P, int a )
{
	if ( ( volRed & 4 ) != 0 )
		return volFlat[a];
	if ( volAirOn ) {
		vec3 g = ( P - volAirOrigin ) / volAirVoxel;
		vec2 xy = g.xy / volAirDims.xy;
		float z = clamp( g.z, 0.5, volAirDims.z - 0.5 );
		vec4 s = texture( volAir, vec3( xy, ( z + float( a ) * volAirDims.z ) / ( 6.0 * volAirDims.z ) ) );
		return s.a > 0.01 ? max( s.rgb / s.a, vec3( 0.0 ) ) : vec3( 0.0 );
	}
	const vec3 axes[6] = vec3[6]( vec3( 1, 0, 0 ), vec3( -1, 0, 0 ), vec3( 0, 1, 0 ), vec3( 0, -1, 0 ), vec3( 0, 0, 1 ), vec3( 0, 0, -1 ) );
	return cellGiE( P, axes[a] );
}

// the light the medium at posView scatters toward the eye (display units, before the record's intensity)
vec3 volScatter( vec3 posView, vec3 dirView )
{
	vec3 S = vec3( 0.0 );
	// outdoors: the weather's directional light (the lookdev sun; fogSun.w is 0 indoors), its cascade shadow
	if ( fogSun.w > 0.0 && ( volTerms & 1 ) != 0 ) {
		float vis = ( volCsm && ( volRed & 8 ) == 0 ) ? wwSunShadow( posView ) : 1.0;
		S += fogSunColour.rgb * fogSun.w * vis * volPhasePi( dot( dirView, fogSun.xyz ) );
	}
	if ( !cellOn )
		return S;
	vec3 P = cellWorldPos( posView );
	vec3 dW = cellWorldDir( dirView );
	if ( cellHasDir && cellInterior && ( volTerms & 1 ) != 0 )
		S += cellDirColor * volPhasePi( dot( normalize( cellDirTo ), dW ) );
	for ( int n = 0; n < volEmitCount && ( volTerms & 2 ) != 0; n++ ) {
		vec2 ek = texelFetch( volEmit, n ).rg;
		int i = int( ek.x + 0.5 );
		float k = ek.y;
		if ( k <= 0.0 || i >= cellLightCount )
			continue;
		vec4 t0 = texelFetch( cellLights, i * CELL_TPL );
		vec3 Lv = t0.xyz - P;
		float d = length( Lv );
		if ( d >= t0.w || d < 0.001 )
			continue;
		vec3 L = Lv / d, Lo;
		bool noSpec;
		vec3 e = cellLightE( i, P, L, Lo, noSpec );	// N = L: no N.L, the shadow lifted toward the light
		S += e * k * volPhasePi( dot( L, dW ) );
	}
	return S;
}

// the probe GI the medium at posView scatters toward the eye, read as L0 + L1 from the six-axis cube: once a froxel
// (at the slice's middle, as the cloud twin's froxel S), not per sub-step -- six room-blended reads are the cost
vec3 volGiScatter( vec3 posView, vec3 dirView )
{
	if ( !cellOn || !cellGiOn || ( volRed & 2 ) != 0 || ( volTerms & 4 ) == 0 )
		return vec3( 0.0 );
	vec3 P = cellWorldPos( posView );
	vec3 dW = cellWorldDir( dirView );
	vec3 E[6];
	for ( int a = 0; a < 6; a++ )
		E[a] = volCube( P, a ) * ( 1.0 / PI );
	vec3 fl = ( 2.0 / 3.0 ) * ( E[0] + E[1] + E[2] + E[3] + E[4] + E[5] );
	vec3 l1 = ( E[0] - E[1] ) * dW.x + ( E[2] - E[3] ) * dW.y + ( E[4] - E[5] ) * dW.z;
	return 0.25 * max( fl + 3.0 * volGi.w * l1, vec3( 0.0 ) ) * volGi.rgb;
}

float volAlpha( vec3 dirView, float t )
{
	if ( ( volRed & 16 ) != 0 )	// red wrongsrc: a fixed medium (0.02 / m) in place of the fog records
		return 1.0 - exp( -t * 0.02 / 70.0 );
	vec3 posView = dirView * ( t / fogDistScale );
	float hb;
	vec3 fogCol;
	return wwFogEval( t, dot( fogView.xyz, posView ) + fogView.w, hb, fogCol );
}

void main()
{
	ivec2 col = ivec2( gl_FragCoord.xy );
	if ( volStage == 2 ) {
		vec3 s = vec3( 0.0 );
		for ( int j = 0; j <= volLayer; j++ )
			s += texelFetch( volInject, ivec3( col, j ), 0 ).rgb;
		fragColor = vec4( s, texelFetch( volInject, ivec3( col, volLayer ), 0 ).a );
		return;
	}
	vec2 ndc = ( vec2( col ) + 0.5 ) / vec2( volDims.xy ) * 2.0 - 1.0;
	vec3 dirView = normalize( vec3( ndc.x / volProj.x, ndc.y / volProj.y, -1.0 ) );
	float t0 = volSlices.x * exp( volSlices.y * float( volLayer ) / volSlices.z );
	float t1 = volSlices.x * exp( volSlices.y * float( volLayer + 1 ) / volSlices.z );
	vec3 acc = vec3( 0.0 );
	float a0 = volAlpha( dirView, t0 ), daSum = 0.0;
	for ( int q = 0; q < 4; q++ ) {
		float ta = mix( t0, t1, float( q ) * 0.25 ), tb = mix( t0, t1, float( q + 1 ) * 0.25 );
		float a1 = volAlpha( dirView, tb );
		float da = max( a1 - a0, 0.0 );
		a0 = a1;
		daSum += da;
		if ( da > 0.0 )
			acc += da * volScatter( dirView * ( 0.5 * ( ta + tb ) / fogDistScale ), dirView );
	}
	if ( daSum > 0.0 )
		acc += daSum * volGiScatter( dirView * ( 0.5 * ( t0 + t1 ) / fogDistScale ), dirView );
	// the uniforms the CPU's setters look for must stay live
	if ( !fogOn )
		acc = vec3( 0.0 );
	fragColor = vec4( acc * volK, a0 );
}

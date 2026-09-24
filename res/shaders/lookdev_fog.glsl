// Lookdev weather fog (lane FOG1): the engine fog formula, a line-for-line port
// of the composite shader as transcribed in scratchpad/pbrprep1_20260924/spec_fog.md
// 2.4 (FO4CS ambient_ibl_pass.hlsl F4FXApplyEngineFog), fed the cb12[41..46]
// packing (fogK) that gl/lookdevstage.cpp builds from the WTHR (esmweather.cpp
// wwFogAt). Applied per fragment to scene geometry in LINEAR light, before the
// exposure and the view transform (studioOutput); the sky dome is never fogged.
// The fog-sun term's colour and intensity are INFERRED (the lookdev sun: its
// direction, the Sunlight row, intensity 1); the power is fDirectionalFogPower.
// Needs: posView = the fragment's view-space position (game units x view scale).

uniform bool fogOn;
uniform vec4 fogK[6];		// cb12[41..46]
uniform vec4 fogView;		// world z above the ground = dot( fogView.xyz, posView ) + fogView.w
uniform float fogDistScale;	// view units -> game units (1 / the view scale)
uniform vec4 fogSun;		// view space, unit, TO the sun; .w = intensity
uniform vec4 fogSunColour;	// linear; .w = fDirectionalFogPower
uniform vec4 fogProbe;		// gates only: .z = 0 off, 1 alpha, 2 colour / 2, 3 hb (at d = .x, z = .y); 5 geometry echo (d / 4096, 0.5 + z / 2000)
uniform int fogRed;		// gates only: 1 maxclamp, 2 noescape, 4 height0

// fog alpha, height blend and colour (before the sun term) of one fragment
float wwFogEval( float d, float z, out float hb, out vec3 fogCol )
{
	if ( ( fogRed & 4 ) != 0 )
		z = 0.0;
	float ramp = d * fogK[0].x - fogK[0].z;
	float f = clamp( ramp, 0.0, 1.0 );
	vec2 pair = clamp( z * fogK[5].xy - fogK[5].zw, 0.0, 1.0 );
	hb = mix( pair.x, pair.y, f );
	float mx = fogK[2].w;
	float clampT = ( ramp > 0.75 && ( fogRed & 1 ) == 0 ) ? min( ( f - 0.75 ) * 4.0 * ( 1.0 - mx ) + mx, 1.0 ) : mx;
	float escape = ( ramp < 0.015 && ( fogRed & 2 ) == 0 ) ? f * 66.666672 : 1.0;
	float I = f > 0.0 ? min( clampT, pow( f, fogK[1].w ) ) : 0.0;
	float weight = hb * fogK[3].w + ( 1.0 - hb );
	fogCol = mix( mix( fogK[1].rgb, fogK[3].rgb, I ), mix( fogK[2].rgb, fogK[4].rgb, I ), hb );
	return weight * I * escape;
}

// linear in, linear out
vec3 wwFog( vec3 preFog, vec3 posView )
{
	if ( !fogOn )
		return preFog;
	float d = length( posView ) * fogDistScale;
	float z = dot( fogView.xyz, posView ) + fogView.w;
	float hb;
	vec3 fogCol;
	float alpha = wwFogEval( d, z, hb, fogCol );
	vec3 viewDir = posView / max( length( posView ), 1e-6 );
	float sunW = pow( max( dot( viewDir, fogSun.xyz ), 0.0 ), fogSunColour.w ) * fogSun.w;
	vec3 sunlit = mix( fogCol, fogSunColour.rgb, sunW );
	float gray = dot( preFog, vec3( 1.0 / 3.0 ) );
	vec3 resolved = alpha < fogK[2].w ? mix( sunlit, vec3( gray ), gray ) : sunlit;
	return mix( preFog, resolved, alpha );
}

// gates only: true when the fragment must write `o` raw (no exposure, no view transform)
bool wwFogProbe( vec3 posView, out vec3 o )
{
	o = vec3( 0.0 );
	if ( !fogOn || fogProbe.z < 0.5 )
		return false;
	if ( fogProbe.z > 4.5 ) {
		// the geometry the fog reads: R = d / 4096, G = 0.5 + z / 2000
		float d = length( posView ) * fogDistScale;
		float z = dot( fogView.xyz, posView ) + fogView.w;
		o = vec3( d / 4096.0, 0.5 + z / 2000.0, 0.0 );
		return true;
	}
	float hb;
	vec3 fogCol;
	float alpha = wwFogEval( fogProbe.x, fogProbe.y, hb, fogCol );
	if ( fogProbe.z < 1.5 )
		o = vec3( alpha );
	else if ( fogProbe.z < 2.5 )
		o = fogCol * 0.5;
	else
		o = vec3( hb );
	return true;
}

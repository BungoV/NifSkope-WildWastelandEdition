#version 410 core

// lane ATMO1: the physical sky's pixels. A world ray per pixel, the sky-view LUT of the sun (+ the moon's own LUT),
// the sun disc through the transmittance LUT (limb darkening on High and up). Scene units: displayed = pi * k * L,
// k = the weather's Day sunlight luminance / the 45-degree transmittance (src/gl/atmosphere.cpp).

#include "uniforms.glsl"
#include "atmo_common.glsl"

uniform sampler2D atTransTex;
uniform sampler2D atSunView;
uniform sampler2D atMoonView;
uniform vec2 atViewSize;
uniform vec4 atRect;	// viewport x, y, 1/w, 1/h
uniform float atCamR;
uniform vec3 atSunW;	// world, TO the sun (may be below the horizon)
uniform vec3 atMoonW;
uniform float atSunK;	// pi * k for the sun
uniform float atMoonK;	// pi * k for the moon; 0 = no moon term
uniform float atLimb;	// 1 = limb darkening (High+)
uniform float atDiscCap;	// the disc's display cap (scene units)
uniform float atDiscHue;	// lane SKYDISC1: 1 = one scale for the whole disc (its centre to the cap): hue and limb kept
uniform float sceneExposure;
uniform int viewTransform;
uniform float atTintOn;	// lane SKYA (ATMOCHECK1): 1 = the Sky Color row's Weather Tint
uniform vec3 atTint[17];	// the weather dome's linear color at 90 (i / 16)^2 degrees (gl/atmomath.h tintTable)
uniform sampler2D atMsTex;	// lane SKY2B (SKYBLUR1): the per-pixel sky marches each pixel (red WW_ATMO_RED=perpixel)
uniform vec2 atMsSize;
uniform float atMsOn;
uniform float atGround;
uniform float atPerPixel;	// 1 = march per pixel instead of the sky-view LUT (the LUT's resolution measured against it)

out vec4 fragColor;

#include "lookdev_output.glsl"

const float kSunCos = 0.99998918;	// cos 0.2665 deg
const float kSunSolid = 6.7967e-5;	// 2 pi (1 - cos 0.2665 deg) sr

vec3 atLookup( sampler2D lut, vec3 d, vec3 l )
{
	vec2 a = d.xy, b = l.xy;
	float la = length( a ), lb = length( b );
	float cosAz = ( la > 1e-5 && lb > 1e-5 ) ? dot( a / la, b / lb ) : 1.0;
	return texture( lut, atSkyDirToUv( atCamR, d.z, cosAz, atViewSize ) ).rgb;
}

// the GLSL twin of gl/atmomath.h weatherTint: the physical luminance in the weather's color at this elevation
vec3 atWeatherTint( vec3 c, float elevDeg )
{
	float t = sqrt( clamp( elevDeg, 0.0, 90.0 ) / 90.0 ) * 16.0;
	int i = min( int( t ), 15 );
	vec3 w = mix( atTint[i], atTint[i + 1], t - float( i ) );
	const vec3 y = vec3( 0.2126, 0.7152, 0.0722 );
	float lw = dot( w, y );
	return lw > 1e-8 ? w * ( dot( c, y ) / lw ) : c;
}

void main()
{
	vec2 ndc = ( gl_FragCoord.xy - atRect.xy ) * atRect.zw * 2.0 - 1.0;
	vec3 vr = normalize( vec3( ( ndc.x + projectionMatrix[2][0] ) / projectionMatrix[0][0],
		( ndc.y + projectionMatrix[2][1] ) / projectionMatrix[1][1], -1.0 ) );
	vec3 d = normalize( vr * viewMatrix );	// view to world: the transpose of the rotation
	vec3 c;
	if ( atPerPixel > 0.5 ) {
		c = atSunK * atSkyMarch( atTransTex, atMsTex, atMsSize, atMsOn, atGround, atCamR, d, atSunW );
		if ( atMoonK > 0.0 )
			c += atMoonK * atSkyMarch( atTransTex, atMsTex, atMsSize, atMsOn, atGround, atCamR, d, atMoonW );
	} else {
		c = atSunK * atLookup( atSunView, d, atSunW );
		if ( atMoonK > 0.0 )
			c += atMoonK * atLookup( atMoonView, d, atMoonW );
	}
	if ( atTintOn > 0.5 )	// the disc below keeps the physical color
		c = atWeatherTint( c, degrees( asin( clamp( d.z, -1.0, 1.0 ) ) ) );
	float cs = dot( d, atSunW );
	if ( cs > kSunCos ) {
		float x = clamp( ( 1.0 - cs ) / ( 1.0 - kSunCos ), 0.0, 1.0 );	// ~ (theta / alpha)^2
		vec3 limb = vec3( 1.0 );
		if ( atLimb > 0.5 ) {
			vec3 a = vec3( 0.397, 0.503, 0.652 );
			limb = pow( vec3( sqrt( max( 0.0, 1.0 - x ) ) ), a ) * ( a + 2.0 ) * 0.5;
		}
		vec3 disc = atSunK * atTrans( atTransTex, atCamR, d.z ) * limb / kSunSolid;
		if ( atDiscHue > 0.5 ) {
			vec3 a = vec3( 0.397, 0.503, 0.652 );
			vec3 centre = atSunK * atTrans( atTransTex, atCamR, atSunW.z ) * ( a + 2.0 ) * 0.5 / kSunSolid;
			float m = max( centre.r, max( centre.g, centre.b ) );
			c += disc * ( m > atDiscCap ? atDiscCap / m : 1.0 );
		} else {
			c += min( disc, vec3( atDiscCap ) );
		}
	}
	fragColor = vec4( studioOutput( c ), 1.0 );
}

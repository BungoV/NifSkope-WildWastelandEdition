#version 410 core

/* lane SKYDISC1: the moon as a lit globe of 0.26 deg half-angle (gl/skydisc.h). Per pixel: the world ray against the
 * sphere, the surface point's selenographic longitude/latitude (NASA SVS CGI Moon Kit albedo + relief, loose files
 * Textures/Sky/Moon), Lommel-Seeliger mu0 / (mu0 + mu) from the real sun direction (so the phase is the light's), a
 * faint earthshine, through the air toward it (atTrans). Over it the sky in front, the same LUTs as atmo_sky.frag:
 * the moon hides the stars, not the air. Premultiplied coverage; nothing outside the disc (no glow, no outline). */

#include "uniforms.glsl"
#include "atmo_common.glsl"

uniform sampler2D atAlbedo;	// Moon_d.dds (BC1, sRGB values sampled raw)
uniform sampler2D atRelief;	// Moon_n.dds (BC5: R = east slope, G = north slope)
uniform sampler2D atTransTex;
uniform sampler2D atSunView;
uniform sampler2D atMoonView;
uniform vec2 atViewSize;
uniform vec4 atRect;	// viewport x, y, 1/w, 1/h
uniform float atCamR;
uniform vec3 atSunW;	// the sky's sun (its LUT)
uniform vec3 atMoonW;
uniform float atSunK;
uniform float atMoonK;
uniform float atTintOn;
uniform vec3 atTint[17];
uniform vec3 moonD;	// TO the moon's centre (world, unit)
uniform vec3 moonS;	// TO the sun from the moon (world, unit): the phase
uniform vec3 bodyX;	// the moon's axes: x = longitude 0, y = 90 east, z = the spin pole
uniform vec3 bodyY;
uniform vec3 bodyZ;
uniform float sinR;	// sin of the half-angle
uniform float pxAng;	// one pixel's angle (radians)
uniform float lodD;	// the albedo's mip for this size on screen (one level for the whole disc: no seam)
uniform float lodN;
uniform float albedoInv;	// 1 / the stored albedo's linear mean
uniform float sunTerm;	// scene units: R x KS(alpha) / the LS disc mean at alpha
uniform float earthTerm;	// scene units: the earthshine
uniform float alpha;	// the game clock's fade x the row's leak multiplier
uniform float sceneExposure;
uniform int viewTransform;

in vec2 uv;

out vec4 fragColor;

#include "lookdev_output.glsl"

const float kTau = 6.28318531;

vec3 atLookup( sampler2D lut, vec3 d, vec3 l )	// atmo_sky.frag's twin
{
	vec2 a = d.xy, b = l.xy;
	float la = length( a ), lb = length( b );
	float cosAz = ( la > 1e-5 && lb > 1e-5 ) ? dot( a / la, b / lb ) : 1.0;
	return texture( lut, atSkyDirToUv( atCamR, d.z, cosAz, atViewSize ) ).rgb;
}

vec3 atWeatherTint( vec3 c, float elevDeg )	// atmo_sky.frag's twin (gl/atmomath.h weatherTint)
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
	vec3 d = normalize( vr * viewMatrix );
	float b = dot( d, moonD );
	float q = length( cross( d, moonD ) );	// sine of the angle off the centre
	float cov = clamp( ( sinR - q ) / pxAng + 0.5, 0.0, 1.0 ) * clamp( d.z / pxAng + 0.5, 0.0, 1.0 );
	cov *= alpha;
	if ( b <= 0.0 || cov <= 0.0 )
		discard;
	float h = sqrt( max( 0.0, sinR * sinR - q * q ) );
	vec3 n = normalize( d * ( b - h ) - moonD );	// the surface normal (the limb point past the edge)
	float lon = atan( dot( n, bodyY ), dot( n, bodyX ) );
	float lat = asin( clamp( dot( n, bodyZ ), -1.0, 1.0 ) );
	vec2 tuv = vec2( lon / kTau + 0.5, 0.5 - lat / kPi );
	vec3 alb = srgbToLinear( textureLod( atAlbedo, tuv, lodD ).rgb ) * albedoInv;
	vec2 sl = textureLod( atRelief, tuv, lodN ).rg * 2.0 - 1.0;
	vec3 east = cross( bodyZ, n );
	float le = length( east );
	east = le > 1e-4 ? east / le : bodyX;
	vec3 north = cross( n, east );
	vec3 nm = normalize( east * sl.x + north * sl.y + n * sqrt( max( 0.0, 1.0 - dot( sl, sl ) ) ) );
	float lit = clamp( dot( n, moonS ) / 0.01 + 0.5, 0.0, 1.0 );	// the sphere's terminator, not the slopes'
	float mu0 = max( 0.0, dot( nm, moonS ) ) * lit;
	float mu = max( 1e-3, dot( nm, -d ) );
	vec3 m = alb * ( sunTerm * mu0 / ( mu0 + mu ) + earthTerm );
	m *= atTrans( atTransTex, atCamR, d.z );
	vec3 sky = atSunK * atLookup( atSunView, d, atSunW );
	if ( atMoonK > 0.0 )
		sky += atMoonK * atLookup( atMoonView, d, atMoonW );
	if ( atTintOn > 0.5 )
		sky = atWeatherTint( sky, degrees( asin( clamp( d.z, -1.0, 1.0 ) ) ) );
	fragColor = vec4( studioOutput( sky + m ) * cov, cov );
}

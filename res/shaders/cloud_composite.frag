#version 410 core

/* lane CLOUD2: the volume clouds over the sky, as the 2D layers composite (SRC_ALPHA in display space): the history
 * upsampled bilinear, its light un-premultiplied, through the view transform. */

uniform sampler2D cvHist;	// rgb = light (premultiplied by the opacity), a = transmittance
uniform vec4 cvRect;	// viewport x, y, 1 / w, 1 / h
uniform float sceneExposure;
uniform int viewTransform;
// lane CLOUD2B: cvFront = 1 draws over the scene after it (the sky had its pass first): only where the clouds' mean depth
// (cvHistDepth, km) is in front of the scene's (cvScene window depth; cvSceneZ as cloud_march.frag), never over the sky
uniform int cvFront;
uniform sampler2D cvHistDepth;
uniform sampler2D cvScene;
uniform vec4 cvSceneZ;
uniform vec4 cvProj;	// P00, P11, P20, P21
// lane CLOUD2C: cvFront = 2 is that pass INSIDE the cell's linear frame, before the tone map: the light as it is (no
// exposure, no view transform), premultiplied (the caller blends ONE, ONE_MINUS_SRC_ALPHA), so the tone map treats the
// clouds in front of the terrain as it treats the terrain
/* lane CLOUD6f (RDR2 slide 40: a lightning strike is a point light inside the cloud, added to the scattering): during a
 * storm's flash the light of a point source inside the Low band reaches the eye from the cloud at its mean depth
 * (cvHistDepth): (1 - T) x cvLtCol x max(phi + 3 g D grad(phi).d, 0), phi the diffusion Green's function with images
 * across the band's extrapolated boundaries. No density read; cvLtOn 0 (no storm, between flashes, the switch off)
 * skips it, so the output is today's. Twin: src/gl/cloudlightning.h fieldAt. */
uniform int cvLtOn;
uniform vec4 cvLtSrc;	// the strike, km: world x, y, height; w = 1 / delta (1 / km)
uniform vec4 cvLtPrm;	// r0^2 (km^2), the image planes' doubled heights 2 (base - z_b), 2 (top + z_b), 3 g D (km)
uniform vec3 cvLtCol;	// the flash's light: colour x level x power x w / (4 pi) / (4 pi D) x the Low band's tint
uniform mat3 cvCam;	// world columns: right, up, back (cloud_march.frag)
uniform vec3 cvCamKm;	// world x, y km, height km

out vec4 fragColor;

#include "lookdev_output.glsl"

float cvLtField( vec3 p, vec3 d )
{
	float phi = 0.0, gd = 0.0;
	for ( int i = 0; i < 3; i++ ) {
		float zs = i == 0 ? cvLtSrc.z : ( i == 1 ? cvLtPrm.y : cvLtPrm.z ) - cvLtSrc.z;
		vec3 a = p - vec3( cvLtSrc.xy, zs );
		float r = sqrt( dot( a, a ) + cvLtPrm.x );
		float e = exp( -r * cvLtSrc.w ) / r;
		float s = i == 0 ? 1.0 : -1.0;
		phi += s * e;
		gd += s * ( -e * ( cvLtSrc.w + 1.0 / r ) / r ) * dot( a, d );
	}
	return max( phi + cvLtPrm.w * gd, 0.0 );
}

// the flash's light at this pixel (premultiplied, as the history's), a = the clouds' opacity
vec3 cvLtLight( vec2 uv, float a )
{
	// the nearest cloudy depth of the bilinear footprint (an edge texel of no cloud holds 1e4)
	vec4 q = textureGather( cvHistDepth, uv, 0 );
	float dm = min( min( q.x, q.y ), min( q.z, q.w ) );
	if ( dm >= 1e3 )
		return vec3( 0.0 );
	vec2 ndc = uv * 2.0 - 1.0;
	vec3 d = normalize( cvCam * vec3( ( ndc.x + cvProj.z ) / cvProj.x, ( ndc.y + cvProj.w ) / cvProj.y, -1.0 ) );
	const float kRbKm = 6360.0;
	vec3 e = vec3( d.xy * dm, kRbKm + cvCamKm.z + d.z * dm );
	vec3 p = vec3( cvCamKm.xy + d.xy * dm, length( e ) - kRbKm );
	return a * cvLtCol * cvLtField( p, d );
}

void main()
{
	vec2 uv = ( gl_FragCoord.xy - cvRect.xy ) * cvRect.zw;
	if ( cvFront >= 1 ) {
		float z = texture( cvScene, uv ).r;
		if ( z >= 1.0 )
			discard;
		vec2 ndc = uv * 2.0 - 1.0;
		vec3 v = vec3( ( ndc.x + cvProj.z ) / cvProj.x, ( ndc.y + cvProj.w ) / cvProj.y, -1.0 );
		float tScene = length( v ) * cvSceneZ.y / ( 2.0 * z - 1.0 + cvSceneZ.x ) * cvSceneZ.z;
		ivec2 hs = textureSize( cvHistDepth, 0 );
		float dm = texelFetch( cvHistDepth, clamp( ivec2( uv * vec2( hs ) ), ivec2( 0 ), hs - 1 ), 0 ).r;
		if ( cvSceneZ.w > 0.5 && dm >= tScene )
			discard;
	}
	vec4 h = texture( cvHist, uv );
	float a = clamp( 1.0 - h.a, 0.0, 1.0 );
	if ( a <= 1e-4 )
		discard;
	if ( cvLtOn != 0 )
		h.rgb += cvLtLight( uv, a );
	fragColor = cvFront == 2 ? vec4( max( h.rgb, vec3( 0.0 ) ), a ) : vec4( studioOutput( h.rgb / a ), a );
}

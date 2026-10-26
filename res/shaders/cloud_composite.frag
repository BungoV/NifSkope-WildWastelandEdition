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

out vec4 fragColor;

#include "lookdev_output.glsl"

void main()
{
	vec2 uv = ( gl_FragCoord.xy - cvRect.xy ) * cvRect.zw;
	if ( cvFront == 1 ) {
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
	fragColor = vec4( studioOutput( h.rgb / a ), a );
}

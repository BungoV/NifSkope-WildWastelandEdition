#version 410 core

/* lane CLOUD2: the volume clouds over the sky, as the 2D layers composite (SRC_ALPHA in display space): the history
 * upsampled bilinear, its light un-premultiplied, through the view transform. */

uniform sampler2D cvHist;	// rgb = light (premultiplied by the opacity), a = transmittance
uniform vec4 cvRect;	// viewport x, y, 1 / w, 1 / h
uniform float sceneExposure;
uniform int viewTransform;

out vec4 fragColor;

#include "lookdev_output.glsl"

void main()
{
	vec4 h = texture( cvHist, ( gl_FragCoord.xy - cvRect.xy ) * cvRect.zw );
	float a = clamp( 1.0 - h.a, 0.0, 1.0 );
	if ( a <= 1e-4 )
		discard;
	fragColor = vec4( studioOutput( h.rgb / a ), a );
}

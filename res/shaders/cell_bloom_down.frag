#version 410 core

// FO4CS's BloomDownCS as a fragment pass, one mip of the pyramid (src/gl/cellbloom.h)

#include "cell_bloom.glsl"

uniform sampler2D bloomSrc;		// the resolved source (level 0) or the previous mip, linear, clamped
uniform vec2 bloomDst;			// this mip's size
uniform vec4 bloomParams;		// threshold, soft knee, prefilter (level 0), scale (1)
uniform int bloomRed;			// 1: level 0 a plain box, no Karis average

out vec4 fragColor;

// F4FX_BloomThresholdWeight
float bloomThresholdWeight( float luminance, float threshold, float softKnee )
{
	luminance = max( luminance, 0.0 );
	if ( luminance <= 0.0001 )
		return 0.0;
	threshold = max( threshold, 0.0 );
	float knee = threshold * max( softKnee, 0.0 );
	float hard = luminance - threshold;
	float contribution = max( hard, 0.0 );
	if ( knee > 0.0001 ) {
		float soft = clamp( hard + knee, 0.0, 2.0 * knee );
		contribution = max( contribution, ( soft * soft ) / ( 4.0 * knee ) );
	}
	return clamp( contribution / luminance, 0.0, 1.0 );
}

void main()
{
	vec2 uv = ( floor( gl_FragCoord.xy ) + 0.5 ) / bloomDst;
	vec2 texel = 1.0 / vec2( max( textureSize( bloomSrc, 0 ), ivec2( 1 ) ) );
	float prefilter = clamp( bloomParams.z, 0.0, 1.0 );
	float karis = bloomRed == 1 ? 0.0 : prefilter;
	vec3 taps[4];
	taps[0] = bloomSanitize( textureLod( bloomSrc, uv + texel * vec2( -0.5, -0.5 ), 0.0 ).rgb );
	taps[1] = bloomSanitize( textureLod( bloomSrc, uv + texel * vec2( 0.5, -0.5 ), 0.0 ).rgb );
	taps[2] = bloomSanitize( textureLod( bloomSrc, uv + texel * vec2( -0.5, 0.5 ), 0.0 ).rgb );
	taps[3] = bloomSanitize( textureLod( bloomSrc, uv + texel * vec2( 0.5, 0.5 ), 0.0 ).rgb );
	vec3 color = vec3( 0.0 );
	float weightSum = 0.0;
	for ( int t = 0; t < 4; t++ ) {
		float w = mix( 1.0, 1.0 / ( 1.0 + bloomLuma( taps[t] ) ), karis );
		color += taps[t] * w;
		weightSum += w;
	}
	color /= max( weightSum, 0.0001 );
	float extract = mix( 1.0, bloomThresholdWeight( bloomLuma( color ), bloomParams.x, bloomParams.y ), prefilter );
	fragColor = vec4( color * extract * max( bloomParams.w, 0.0 ), 1.0 );
}

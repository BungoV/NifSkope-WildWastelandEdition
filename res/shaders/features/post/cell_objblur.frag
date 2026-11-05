#version 410 core

/* lane POSTFX1: per-object motion blur, pass 4 of 4: a gather over the neighbour max as in McGuire et al. 2012 ("A
 * Reconstruction Filter for Plausible Motion Blur", I3D: tile max, neighbour max, a depth-compared gather), with
 * the sample jitter of Jimenez 2014 (interleaved gradient noise). colTex the frame, blurTex the pixel vectors
 * (pixels over the shutter, cell_objblur_vec.frag), neighTex the neighbour max (one texel per tile), depthTex the
 * frame's depth (window depth, as game_dof.frag reads it: distance = far near / ((1 - d)(far - near) + near)).
 * Weights (a lane ruling, written in notes/postfx1/RESULTS.md): every sample stands for an equal share of the
 * shutter, so the result is the time each surface covers the pixel (a box along the vector, the gate's bar):
 *   Y in front of X: counts while Y's own blur reaches X (pixel coverage over Y's radius);
 *   X in front of Y: counts while X's blur uncovers Y (pixel coverage over X's radius);
 *   the same surface: counts fully when both are still, else while the faster one's blur reaches.
 * The paper's cone weights blurred the bar's edge 0.54 of its travel; these give the box. A pixel whose
 * neighbourhood holds no vector over half a pixel is copied untouched, so a still world is the input bit for bit. */

uniform sampler2D colTex;
uniform sampler2D blurTex;
uniform sampler2D neighTex;
uniform sampler2D depthTex;
uniform int tile;
uniform int samples;
uniform float zNear;
uniform float zFar;
uniform float noiseSeed;

out vec4 fragColor;

float dist( ivec2 p )
{
	float x = 1.0 - texelFetch( depthTex, p, 0 ).r;
	return zFar * zNear / ( x * ( zFar - zNear ) + zNear );
}

float reach( float d, float r )	// the share of a pixel d away (one pixel wide) that lies inside radius r
{
	return clamp( r + 0.5 - d, 0.0, 1.0 );
}

float front( float za, float zb, float k )	// 1 when a is in front of (or level with) b, fading out over k behind it
{
	return clamp( 1.0 - ( za - zb ) / k, 0.0, 1.0 );
}

void main()
{
	ivec2 X = ivec2( gl_FragCoord.xy );
	ivec2 sz = textureSize( colTex, 0 );
	vec4 cX = texelFetch( colTex, X, 0 );
	vec2 vn = texelFetch( neighTex, X / tile, 0 ).xy * 0.5;	// radius: half the shutter's travel each side
	float ln = length( vn );
	if ( ln < 0.5 ) {
		fragColor = cX;
		return;
	}
	vec2 vs = vn * ( ( ln + 0.5 ) / ln );	// the span reaches the half pixel the last covered pixel overhangs
	float rx = length( texelFetch( blurTex, X, 0 ).xy ) * 0.5;
	float zx = dist( X );
	float k = max( 0.02 * zx, 1.0 );
	float wsum = 1.0;
	vec4 sum = cX;
	float j = fract( 52.9829189 * fract( dot( gl_FragCoord.xy, vec2( 0.06711056, 0.00583715 ) ) + noiseSeed ) ) - 0.5;
	int n = max( samples, 2 );
	for ( int i = 0; i < n; i++ ) {
		float t = mix( -1.0, 1.0, ( float( i ) + j + 1.0 ) / float( n + 1 ) );
		ivec2 Y = clamp( ivec2( floor( vec2( X ) + 0.5 + vs * t ) ), ivec2( 0 ), sz - 1 );
		float d = length( vec2( Y - X ) );
		float ry = length( texelFetch( blurTex, Y, 0 ).xy ) * 0.5;
		float zy = dist( Y );
		float yf = front( zy, zx, k );
		float xf = front( zx, zy, k );
		float same = yf * xf;
		float rm = max( rx, ry );
		float a = same * ( rm < 0.5 ? 1.0 : reach( d, rm ) )
			+ ( yf - same ) * reach( d, ry )
			+ ( xf - same ) * reach( d, rx );
		wsum += a;
		sum += a * texelFetch( colTex, Y, 0 );
	}
	fragColor = sum / wsum;
}

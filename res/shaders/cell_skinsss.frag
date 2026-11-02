#version 410 core

// lane SKINSSS1: screen-space skin subsurface scattering in the linear frame (row "Skin subsurface", WW_CELL_SKINSSS).
// FO4CS's separable SSS (res/Effects/Subsurface): extract, blur x, blur y + composite. src/gl/wwskinsss.h is the
// C++ twin (tests/skinsss); src/gl/cellskinsss.cpp runs the passes.
// sssPass 0: the extract (srcTex = the frame, skinTex = the opaque pass's skin tag) -> ( saturated light, coverage )
// sssPass 1: the blur along x of the extract; 2: the blur along y, then the composite into the frame (litTex)

uniform sampler2D srcTex;
uniform sampler2D litTex;
uniform sampler2D depthTex;	// the frame's window depth (the game's device depth law)
uniform sampler2D skinTex;	// R8: 1 on skin
uniform int sssPass;
uniform int sssRadius;
uniform vec3 sssKernel[25];	// the 5-Gaussian profile per |offset| (wwskinsss.h kernel)
uniform vec4 sssParams;		// x amount, y saturation, z depth threshold, w depth falloff
uniform bool sssRed;		// WW_CELL_SKINSSS_RED=1: the passes run, the composite keeps the frame

out vec4 fragColor;

vec3 sssSaturate( vec3 c, float s )
{
	float l = dot( c, vec3( 0.2126, 0.7152, 0.0722 ) );
	return max( mix( vec3( l ), c, s ), vec3( 0.0 ) );
}

float sssDepthWeight( float dc, float ds )
{
	return clamp( 1.0 - abs( ds - dc ) / sssParams.z * max( sssParams.w, 0.0 ), 0.0, 1.0 );
}

void main()
{
	ivec2 p = ivec2( gl_FragCoord.xy );
	ivec2 size = textureSize( srcTex, 0 );
	if ( sssPass == 0 ) {
		float w = clamp( texelFetch( skinTex, p, 0 ).r, 0.0, 1.0 ) * clamp( sssParams.x, 0.0, 1.0 );
		fragColor = w > 0.0 ? vec4( sssSaturate( texelFetch( srcTex, p, 0 ).rgb, sssParams.y ), w ) : vec4( 0.0 );
		return;
	}
	vec4 c = texelFetch( srcTex, p, 0 );
	if ( c.a <= 0.0 ) {
		if ( sssPass == 2 )
			discard;	// not skin: the frame keeps every sample
		fragColor = vec4( 0.0 );
		return;
	}
	ivec2 dir = sssPass == 1 ? ivec2( 1, 0 ) : ivec2( 0, 1 );
	float dc = texelFetch( depthTex, p, 0 ).r;
	vec3 sum = vec3( 0.0 ), ws = vec3( 0.0 );
	for ( int t = -sssRadius; t <= sssRadius; t++ ) {
		ivec2 q = clamp( p + dir * t, ivec2( 0 ), size - 1 );
		vec4 s = texelFetch( srcTex, q, 0 );
		vec3 k = sssKernel[abs( t )] * ( sssDepthWeight( dc, texelFetch( depthTex, q, 0 ).r ) * s.a );
		sum += s.rgb * k;
		ws += k;
	}
	vec3 o = vec3( ws.r > 1e-5 ? sum.r / ws.r : c.r, ws.g > 1e-5 ? sum.g / ws.g : c.g, ws.b > 1e-5 ? sum.b / ws.b : c.b );
	if ( sssPass == 1 ) {
		fragColor = vec4( o, c.a );
		return;
	}
	vec4 lit = texelFetch( litTex, p, 0 );
	fragColor = vec4( sssRed ? lit.rgb : lit.rgb + ( o - lit.rgb ) * clamp( c.a, 0.0, 1.0 ), lit.a );
}

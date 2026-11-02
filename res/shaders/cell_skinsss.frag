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
#ifdef WW_SSS_HALF
// lane SKINSSS2 (cell_skinssshalf.prog only): the half-resolution passes (src/gl/wwskinssshalf.h): 0 extract 2x2 ->
// half (+ the half depth, each block's first texel, on attachment 1), 1 / 2 blur x / y on the half image, 3 the
// composite at full resolution through a depth-aware bilinear, written as ( scattered, coverage ) under the blend
uniform sampler2D hdepthTex;	// the half depth (on litTex's unit)
layout ( location = 1 ) out vec4 sssHalfDepth;
#endif

vec3 sssSaturate( vec3 c, float s )
{
	float l = dot( c, vec3( 0.2126, 0.7152, 0.0722 ) );
	return max( mix( vec3( l ), c, s ), vec3( 0.0 ) );
}

float sssDepthWeight( float dc, float ds )
{
	return clamp( 1.0 - abs( ds - dc ) / sssParams.z * max( sssParams.w, 0.0 ), 0.0, 1.0 );
}

#ifdef WW_SSS_HALF
// lane SKINSSS2: the extract of the 2x2 block of half texel q (the twin's extractHalf) by gathers (w = texel 2q, z = +x,
// x = +y, y = +x+y; clamp-to-edge = the twin's clamp): ( saturated light, coverage ); dref = the block's reference depth
vec4 sssBlock( ivec2 q, out float dref )
{
	vec2 fuv = vec2( 2 * q + 1 ) / vec2( textureSize( depthTex, 0 ) );
	vec4 dg = textureGather( depthTex, fuv, 0 );
	vec4 tg = clamp( textureGather( skinTex, fuv, 0 ), 0.0, 1.0 );
	dref = dg.w;
	vec4 w = tg * vec4( sssDepthWeight( dg.w, dg.x ), sssDepthWeight( dg.w, dg.y ), sssDepthWeight( dg.w, dg.z ), 1.0 );
	float ws = dot( w, vec4( 1.0 ) );
	if ( ws <= 0.0 )
		return vec4( 0.0 );
	ivec2 fmax = textureSize( srcTex, 0 ) - 1;
	vec3 sum = vec3( 0.0 );
	if ( w.w > 0.0 ) sum += sssSaturate( texelFetch( srcTex, min( 2 * q, fmax ), 0 ).rgb, sssParams.y ) * w.w;
	if ( w.z > 0.0 ) sum += sssSaturate( texelFetch( srcTex, min( 2 * q + ivec2( 1, 0 ), fmax ), 0 ).rgb, sssParams.y ) * w.z;
	if ( w.x > 0.0 ) sum += sssSaturate( texelFetch( srcTex, min( 2 * q + ivec2( 0, 1 ), fmax ), 0 ).rgb, sssParams.y ) * w.x;
	if ( w.y > 0.0 ) sum += sssSaturate( texelFetch( srcTex, min( 2 * q + ivec2( 1, 1 ), fmax ), 0 ).rgb, sssParams.y ) * w.y;
	return vec4( sum / ws, ws * 0.25 * clamp( sssParams.x, 0.0, 1.0 ) );
}

// lane SKINSSS2: the half-resolution passes; depthTex and skinTex stay full size (half texel q reads 2q)
void sssHalfMain( ivec2 p )
{
	if ( sssPass == 0 ) {	// (fusing it into blur x re-extracts 7 blocks a texel: 0.055 ms against 0.011 + 0.012)
		float dc;
		fragColor = sssBlock( p, dc );
		sssHalfDepth = vec4( dc );
		return;
	}
	if ( sssPass == 3 ) {
		// written as ( scattered, coverage ) under the blend SRC_ALPHA / ONE_MINUS_SRC_ALPHA: lit + ( o - lit ) x a
		float a = clamp( texelFetch( skinTex, p, 0 ).r, 0.0, 1.0 ) * clamp( sssParams.x, 0.0, 1.0 );
		if ( a <= 0.0 || sssRed )
			discard;	// not skin (or the red): the frame keeps every sample
		ivec2 hsize = textureSize( srcTex, 0 );
		vec2 huv = ( vec2( p ) + 0.5 ) * 0.5 / vec2( hsize );
		vec2 fr = fract( ( vec2( p ) + 0.5 ) * 0.5 - 0.5 );
		float dc = texelFetch( depthTex, p, 0 ).r;
		vec4 hd = textureGather( hdepthTex, huv, 0 );
		vec4 ha = textureGather( srcTex, huv, 3 );
		vec4 w = vec4( sssDepthWeight( dc, hd.x ), sssDepthWeight( dc, hd.y ), sssDepthWeight( dc, hd.z ), sssDepthWeight( dc, hd.w ) )
			* step( vec4( 1e-30 ), ha );
		vec3 o;
		if ( all( greaterThanEqual( w, vec4( 0.98 ) ) ) ) {
			o = texture( srcTex, huv ).rgb;	// every tap at this depth (2 % of the threshold) and covered: the plain bilinear
		} else {
			w *= vec4( ( 1.0 - fr.x ) * fr.y, fr.x * fr.y, fr.x * ( 1.0 - fr.y ), ( 1.0 - fr.x ) * ( 1.0 - fr.y ) );
			float ws = dot( w, vec4( 1.0 ) );
			if ( ws <= 1e-5 )
				discard;	// no half texel at this depth: the frame keeps its light
			ivec2 q0 = ivec2( floor( ( vec2( p ) + 0.5 ) * 0.5 - 0.5 ) );
			ivec2 hmax = hsize - 1;
			o = ( texelFetch( srcTex, clamp( q0 + ivec2( 0, 1 ), ivec2( 0 ), hmax ), 0 ).rgb * w.x
				+ texelFetch( srcTex, clamp( q0 + ivec2( 1, 1 ), ivec2( 0 ), hmax ), 0 ).rgb * w.y
				+ texelFetch( srcTex, clamp( q0 + ivec2( 1, 0 ), ivec2( 0 ), hmax ), 0 ).rgb * w.z
				+ texelFetch( srcTex, clamp( q0, ivec2( 0 ), hmax ), 0 ).rgb * w.w ) / ws;
		}
		fragColor = vec4( o, a );
		return;
	}
	ivec2 size = textureSize( srcTex, 0 );
	vec4 c = texelFetch( srcTex, p, 0 );
	if ( c.a <= 0.0 ) {
		fragColor = vec4( 0.0 );
		return;
	}
	ivec2 dir = sssPass == 1 ? ivec2( 1, 0 ) : ivec2( 0, 1 );
	float dc = texelFetch( hdepthTex, p, 0 ).r;
	vec3 sum = vec3( 0.0 ), ws = vec3( 0.0 );
	for ( int t = -sssRadius; t <= sssRadius; t++ ) {
		ivec2 q = clamp( p + dir * t, ivec2( 0 ), size - 1 );
		vec4 s = texelFetch( srcTex, q, 0 );
		vec3 k = sssKernel[abs( t )] * ( sssDepthWeight( dc, texelFetch( hdepthTex, q, 0 ).r ) * s.a );
		sum += s.rgb * k;
		ws += k;
	}
	fragColor = vec4( ws.r > 1e-5 ? sum.r / ws.r : c.r, ws.g > 1e-5 ? sum.g / ws.g : c.g, ws.b > 1e-5 ? sum.b / ws.b : c.b, c.a );
}
#endif

void main()
{
	ivec2 p = ivec2( gl_FragCoord.xy );
#ifdef WW_SSS_HALF
	sssHalfMain( p );
	return;
#endif
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

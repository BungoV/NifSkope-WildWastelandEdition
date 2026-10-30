#version 410 core

/* lane CLOUD2: the volume clouds' temporal resolve (Schneider & Vos 2015: 1/16 of the pixels marched a frame, the rest
 * reprojected). A history pixel whose 4x4 slot is this frame's phase takes the fresh march; the others follow the
 * cloud's depth into the previous history. Off screen or no history: the march, bilinear. */

uniform sampler2D cvMarch;	// rgb = light, a = transmittance, at history / stride
uniform sampler2D cvMarchDepth;	// km
uniform sampler2D cvPrev;	// last frame's history
uniform sampler2D cvPrevDepth;
uniform vec2 cvHistSize;
uniform float cvStride;
uniform vec2 cvPhase;
uniform mat3 cvCam;	// this frame: world columns right, up, back
uniform vec4 cvProj;	// P00, P11, P20, P21
uniform vec3 cvCamKm;
uniform mat3 cvPrevCam;	// last frame's
uniform vec4 cvPrevProj;
uniform vec3 cvPrevCamKm;
uniform float cvHistOn;	// 0 = no history yet (first frame, resize, a jump)
uniform float cvReproj;	// 0 = the red noreproj: the same pixel of the history, the camera's motion ignored

layout ( location = 0 ) out vec4 cvOut;
layout ( location = 1 ) out vec4 cvDepth;

vec4 cvCatmull( float t )
{
	float t2 = t * t, t3 = t2 * t;
	return 0.5 * vec4( -t3 + 2.0 * t2 - t, 3.0 * t3 - 5.0 * t2 + 2.0, -3.0 * t3 + 4.0 * t2 + t, t3 - t2 );
}

/* lane CLOUD6g (CLOUDBAND1 leftover): Move Clamp. A pixel not marched this frame follows the camera into the previous
 * history, and under camera motion its Catmull-Rom read lands among neighbours that were not refreshed since the cloud
 * changed (the 16 slots refresh in SCREEN space, the content moves across them): a clearing seen from a turning camera
 * stayed 28.9 levels off 16 frames later (0.0 with a still camera). A pixel the reprojection MOVED takes its history
 * clamped to the min / max box of this frame's 3x3 march texels around it, i.e. the current frame's data bounds what
 * a stale neighbour can bring in. A still camera reads its history unclamped (pv == uv), as before.
 * 0 = WW_CLOUDVOL_MOVECLAMP=0 / WW_CLOUDVOL_RED=staleread, the old read (an unset uniform is 0: other checkers' arms). */
uniform float cvMoveClamp;

vec4 cvMoveBox( vec2 q, vec2 uv, vec2 pv, vec4 c )
{
	if ( cvMoveClamp < 0.5 || all( lessThan( abs( pv - uv ) * cvHistSize, vec2( 0.01 ) ) ) )
		return c;
	ivec2 m = ivec2( floor( q / cvStride ) ), mhi = textureSize( cvMarch, 0 ) - 1;
	vec4 lo = vec4( 1e30 ), up = vec4( -1e30 );
	for ( int j = -1; j <= 1; j++ )
		for ( int i = -1; i <= 1; i++ ) {
			vec4 n = texelFetch( cvMarch, clamp( m + ivec2( i, j ), ivec2( 0 ), mhi ), 0 );
			lo = min( lo, n );
			up = max( up, n );
		}
	return clamp( c, lo, up );
}

#ifndef WW_CLOUD_ACCUM
void main()
{
	vec2 q = floor( gl_FragCoord.xy );
	vec2 slot = mod( q, cvStride );
	vec2 uv = ( q + 0.5 ) / cvHistSize;
	if ( all( equal( slot, cvPhase ) ) ) {
		ivec2 m = ivec2( floor( q / cvStride ) );
		cvOut = texelFetch( cvMarch, m, 0 );
		cvDepth = texelFetch( cvMarchDepth, m, 0 );
		return;
	}
	vec4 coarse = texture( cvMarch, uv );
	float dk = texture( cvMarchDepth, uv ).r;
	if ( cvHistOn < 0.5 ) {
		cvOut = coarse;
		cvDepth = vec4( dk );
		return;
	}
	vec2 pv = uv;
	if ( cvReproj > 0.5 ) {
		vec2 ndc = uv * 2.0 - 1.0;
		vec3 d = normalize( cvCam * vec3( ( ndc.x + cvProj.z ) / cvProj.x, ( ndc.y + cvProj.w ) / cvProj.y, -1.0 ) );
		// the cloud this pixel saw last frame: the previous depth along the ray (a sky pixel: far away)
		float dp = texture( cvPrevDepth, uv ).r;
		float dist = min( dp < 1e3 ? dp : dk, 400.0 );
		vec3 v = transpose( cvPrevCam ) * ( cvCamKm + d * dist - cvPrevCamKm );
		if ( v.z >= -1e-4 ) {
			cvOut = coarse;
			cvDepth = vec4( dk );
			return;
		}
		vec2 vr = v.xy / -v.z;
		vec2 pn = vec2( vr.x * cvPrevProj.x - cvPrevProj.z, vr.y * cvPrevProj.y - cvPrevProj.w );
		pv = pn * 0.5 + 0.5;
		if ( any( lessThan( pv, vec2( 0.0 ) ) ) || any( greaterThan( pv, vec2( 1.0 ) ) ) ) {
			cvOut = coarse;
			cvDepth = vec4( dk );
			return;
		}
	}
	// Catmull-Rom: a bilinear read each frame blurs the history over the up to 16 resamples before a refresh, and the
	// nearest texel drifts; the depth stays nearest (it is a distance, not a picture)
	vec2 xy = pv * cvHistSize - 0.5, f = fract( xy );
	ivec2 i0 = ivec2( floor( xy ) ) - 1, hi = ivec2( cvHistSize ) - 1;
	vec4 wx = cvCatmull( f.x ), wy = cvCatmull( f.y );
	vec4 c = vec4( 0.0 );
	for ( int j = 0; j < 4; j++ )
		for ( int i = 0; i < 4; i++ )
			c += wx[i] * wy[j] * texelFetch( cvPrev, clamp( i0 + ivec2( i, j ), ivec2( 0 ), hi ), 0 );
	cvOut = cvMoveBox( q, uv, pv, vec4( max( c.rgb, vec3( 0.0 ) ), clamp( c.a, 0.0, 1.0 ) ) );	// lane CLOUD6g
	cvDepth = texelFetch( cvPrevDepth, clamp( ivec2( floor( pv * cvHistSize ) ), ivec2( 0 ), hi ), 0 );
}
#else
/* lane CLOUDBAND1 (bungo 10-30 "the clouds have banding": a diagonal line pattern): Cloud Accumulation. Off keeps ONE march
 * a history pixel, so the march's step offset pattern stays in the picture (IGN, a lattice the 4x4 phase order and the
 * upsample turn into diagonal lines; cloudband_check gate D). Here the march's offsets are blue noise (a new slice each
 * 16-frame refresh), and a pixel's fresh march is blended into its history (Bauer 2019, RDR2: history clamped to the 3x3
 * colour box of the fresh samples, then accumulated): weight cvAccAlpha (cloudvolmath.h accumAlpha: 1, 1/2, 1/3 over
 * the first refresh cycles of a history, then 1/4, about the last 4 marches of the pixel).
 * The clamp to the box of the fresh march and its 8 neighbours (in march texels) drops a history that no longer matches
 * (a camera cut the reprojection missed, a weather change): no ghost trail. Pixels not marched this frame reproject as Off.
 * Running it twice reads only the other history, so twice = once (the CLOUD2C redo). */
uniform float cvAccAlpha;	// the fresh march's weight; 1 = the red noaccum
uniform float cvAccClamp;	// 1 = clamp the history to the fresh neighbourhood's box; 0 = the red noclamp
/* lane CLOUD6g: Change Clamp. The variance clip alone let a partly changed field through (a thinning 0.6 -> 0.05 kept
 * 12.7 levels 16 frames later with a still camera, today's Off 3.1): the box of a thin field still spans old texture.
 * The history at the 9 fresh march texels' own places is compared with the fresh marches there: when their mean
 * transmittance differs by more than te = cvAccReset + cvAccResetSd x min( deviation of the 9 fresh, of the 9 history
 * values ) / 3 (the noise of a 9-mean grows with the field's spread; the smaller of the two keeps a thinning or a new
 * cloud, where one side is flat, at the low threshold) the field changed under this pixel, and its weight rises to 1
 * (smoothstep te .. 2 te) = the pixel's accumulation restarts from this march.
 * cvAccReset / cvAccResetSd = cloudvolmath.h kChangeReset / kChangeResetSd; cvAccReset 0 = WW_CLOUDVOL_CHANGECLAMP=0 /
 * WW_CLOUDVOL_RED=looseclamp (CLOUDBAND1's clamp alone). The red noclamp turns it off too (it lives inside the clamp). */
uniform float cvAccReset;
uniform float cvAccResetSd;

//! where this pixel's cloud was in the previous history (Off's reprojection); false = no history there
bool cvReprojUv( vec2 uv, float dk, out vec2 pv )
{
	pv = uv;
	if ( cvReproj < 0.5 )
		return true;
	vec2 ndc = uv * 2.0 - 1.0;
	vec3 d = normalize( cvCam * vec3( ( ndc.x + cvProj.z ) / cvProj.x, ( ndc.y + cvProj.w ) / cvProj.y, -1.0 ) );
	float dp = texture( cvPrevDepth, uv ).r;
	float dist = min( dp < 1e3 ? dp : dk, 400.0 );
	vec3 v = transpose( cvPrevCam ) * ( cvCamKm + d * dist - cvPrevCamKm );
	if ( v.z >= -1e-4 )
		return false;
	vec2 vr = v.xy / -v.z;
	vec2 pn = vec2( vr.x * cvPrevProj.x - cvPrevProj.z, vr.y * cvPrevProj.y - cvPrevProj.w );
	pv = pn * 0.5 + 0.5;
	return all( greaterThanEqual( pv, vec2( 0.0 ) ) ) && all( lessThanEqual( pv, vec2( 1.0 ) ) );
}

void main()
{
	vec2 q = floor( gl_FragCoord.xy );
	vec2 slot = mod( q, cvStride );
	vec2 uv = ( q + 0.5 ) / cvHistSize;
	bool fresh = all( equal( slot, cvPhase ) );
	ivec2 m = ivec2( floor( q / cvStride ) );
	vec4 coarse = fresh ? texelFetch( cvMarch, m, 0 ) : texture( cvMarch, uv );
	float dk = fresh ? texelFetch( cvMarchDepth, m, 0 ).r : texture( cvMarchDepth, uv ).r;
	vec2 pv;
	if ( cvHistOn < 0.5 || !cvReprojUv( uv, dk, pv ) ) {
		cvOut = coarse;
		cvDepth = vec4( dk );
		return;
	}
	// the history at pv: Catmull-Rom colour, nearest depth (as Off)
	vec2 xy = pv * cvHistSize - 0.5, f = fract( xy );
	ivec2 i0 = ivec2( floor( xy ) ) - 1, hi = ivec2( cvHistSize ) - 1;
	vec4 wx = cvCatmull( f.x ), wy = cvCatmull( f.y );
	vec4 c = vec4( 0.0 );
	for ( int j = 0; j < 4; j++ )
		for ( int i = 0; i < 4; i++ )
			c += wx[i] * wy[j] * texelFetch( cvPrev, clamp( i0 + ivec2( i, j ), ivec2( 0 ), hi ), 0 );
	c = vec4( max( c.rgb, vec3( 0.0 ) ), clamp( c.a, 0.0, 1.0 ) );
	if ( !fresh ) {
		cvOut = cvMoveBox( q, uv, pv, c );	// lane CLOUD6g
		cvDepth = texelFetch( cvPrevDepth, clamp( ivec2( floor( pv * cvHistSize ) ), ivec2( 0 ), hi ), 0 );
		return;
	}
	float a = cvAccAlpha;
	if ( cvAccClamp > 0.5 ) {
		// the 3x3 box of the fresh marches, tightened to their mean +- one deviation (variance clipping, Salvi 2016: the
		// march texels sit 4 history px apart, so the bare min / max box held most of a textured old field), never
		// tighter than the fresh march itself
		ivec2 mhi = textureSize( cvMarch, 0 ) - 1;
		vec4 lo = coarse, up = coarse, s1 = vec4( 0.0 ), s2 = vec4( 0.0 );
		for ( int j = -1; j <= 1; j++ )
			for ( int i = -1; i <= 1; i++ ) {
				vec4 n = texelFetch( cvMarch, clamp( m + ivec2( i, j ), ivec2( 0 ), mhi ), 0 );
				lo = min( lo, n );
				up = max( up, n );
				s1 += n;
				s2 += n * n;
			}
		vec4 mu = s1 / 9.0, sd = sqrt( max( s2 / 9.0 - mu * mu, vec4( 0.0 ) ) );
		lo = min( max( lo, mu - sd ), coarse );
		up = max( min( up, mu + sd ), coarse );
		c = clamp( c, lo, up );
		if ( cvAccReset > 0.0 ) {	// lane CLOUD6g: Change Clamp
			ivec2 hp = ivec2( floor( pv * cvHistSize ) );
			float h1 = 0.0, h2 = 0.0;
			for ( int j = -1; j <= 1; j++ )
				for ( int i = -1; i <= 1; i++ ) {
					float ha = texelFetch( cvPrev, clamp( hp + int( cvStride ) * ivec2( i, j ), ivec2( 0 ), hi ), 0 ).a;
					h1 += ha;
					h2 += ha * ha;
				}
			float hm = h1 / 9.0, hsd = sqrt( max( h2 / 9.0 - hm * hm, 0.0 ) );
			float te = cvAccReset + cvAccResetSd * min( sd.a, hsd ) / 3.0;
			a = max( a, smoothstep( te, 2.0 * te, abs( hm - mu.a ) ) );
		}
	}
	cvOut = mix( c, coarse, a );
	cvDepth = vec4( dk );	// the fresh march's mean distance (a mix with the 1e4 of no cloud means nothing)
}
#endif

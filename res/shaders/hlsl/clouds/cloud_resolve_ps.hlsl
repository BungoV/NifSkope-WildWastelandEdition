// HLSL source of features/clouds/cloud_resolve.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "CloudsBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#ifndef WW_GLSL_SIZE_Texture2D_float4_
#define WW_GLSL_SIZE_Texture2D_float4_
int2 textureSize(Texture2D<float4> t, int lod) { uint w, h, l; t.GetDimensions(uint(lod), w, h, l); return int2(w, h); }
#endif
#ifndef WW_GLSL_MOD
#define WW_GLSL_MOD	// GLSL mod: x - y floor(x / y) (HLSL fmod truncates)
float mod(float x, float y) { return x - y * floor(x / y); }
float2 mod(float2 x, float2 y) { return x - y * floor(x / y); }
float3 mod(float3 x, float3 y) { return x - y * floor(x / y); }
float4 mod(float4 x, float4 y) { return x - y * floor(x / y); }
float2 mod(float2 x, float y) { return x - y * floor(x / y); }
float3 mod(float3 x, float y) { return x - y * floor(x / y); }
float4 mod(float4 x, float y) { return x - y * floor(x / y); }
#endif
static float4 gl_FragCoord;
/* lane CLOUD2: the volume clouds' temporal resolve (Schneider & Vos 2015: 1/16 of the pixels marched a frame, the rest
 * reprojected). A history pixel whose 4x4 slot is this frame's phase takes the fresh march; the others follow the
 * cloud's depth into the previous history. Off screen or no history: the march, bilinear. */

Texture2D<float4> cvMarch : register(t0, space0); SamplerState _cvMarch_sampler : register(s0, space0);	// rgb = light, a = transmittance, at history / stride
Texture2D<float4> cvMarchDepth : register(t1, space0); SamplerState _cvMarchDepth_sampler : register(s1, space0);	// km
Texture2D<float4> cvPrev : register(t2, space0); SamplerState _cvPrev_sampler : register(s2, space0);	// last frame's history
Texture2D<float4> cvPrevDepth : register(t3, space0); SamplerState _cvPrevDepth_sampler : register(s3, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	float2 cvHistSize;
	float cvStride;
	float2 cvPhase;
	row_major float3x3 cvCam;	// this frame: world columns right, up, back
	float4 cvProj;	// P00, P11, P20, P21
	float3 cvCamKm;
	row_major float3x3 cvPrevCam;	// last frame's
	float4 cvPrevProj;
	float3 cvPrevCamKm;
	float cvHistOn;	// 0 = no history yet (first frame, resize, a jump)
	float cvReproj;	// 0 = the red noreproj: the same pixel of the history, the camera's motion ignored
	/* lane CLOUD6g (CLOUDBAND1 leftover): Move Clamp. A pixel not marched this frame follows the camera into the previous
	 * history, and under camera motion its Catmull-Rom read lands among neighbours that were not refreshed since the cloud
	 * changed (the 16 slots refresh in SCREEN space, the content moves across them): a clearing seen from a turning camera
	 * stayed 28.9 levels off 16 frames later (0.0 with a still camera). A pixel the reprojection MOVED takes its history
	 * clamped to the min / max box of this frame's 3x3 march texels around it, i.e. the current frame's data bounds what
	 * a stale neighbour can bring in. A still camera reads its history unclamped (pv == uv), as before.
	 * 0 = WW_CLOUDVOL_MOVECLAMP=0 / WW_CLOUDVOL_RED=staleread, the old read (an unset uniform is 0: other checkers' arms). */
	float cvMoveClamp;
#if !(!defined(WW_CLOUD_ACCUM))
	/* lane CLOUDBAND1 (bungo 10-30 "the clouds have banding": a diagonal line pattern): Cloud Accumulation. Off keeps ONE march
	 * a history pixel, so the march's step offset pattern stays in the picture (IGN, a lattice the 4x4 phase order and the
	 * upsample turn into diagonal lines; cloudband_check gate D). Here the march's offsets are blue noise (a new slice each
	 * 16-frame refresh), and a pixel's fresh march is blended into its history (Bauer 2019, RDR2: history clamped to the 3x3
	 * colour box of the fresh samples, then accumulated): weight cvAccAlpha (cloudvolmath.h accumAlpha: 1, 1/2, 1/3 over
	 * the first refresh cycles of a history, then 1/4, about the last 4 marches of the pixel).
	 * The clamp to the box of the fresh march and its 8 neighbours (in march texels) drops a history that no longer matches
	 * (a camera cut the reprojection missed, a weather change): no ghost trail. Pixels not marched this frame reproject as Off.
	 * Running it twice reads only the other history, so twice = once (the CLOUD2C redo). */
	float cvAccAlpha;	// the fresh march's weight; 1 = the red noaccum
	float cvAccClamp;	// 1 = clamp the history to the fresh neighbourhood's box; 0 = the red noclamp
	/* lane CLOUD6g: Change Clamp. The variance clip alone let a partly changed field through (a thinning 0.6 -> 0.05 kept
	 * 12.7 levels 16 frames later with a still camera, today's Off 3.1): the box of a thin field still spans old texture.
	 * The history at the 9 fresh march texels' own places is compared with the fresh marches there: when their mean
	 * transmittance differs by more than te = cvAccReset + cvAccResetSd x min( deviation of the 9 fresh, of the 9 history
	 * values ) / 3 (the noise of a 9-mean grows with the field's spread; the smaller of the two keeps a thinning or a new
	 * cloud, where one side is flat, at the low threshold) the field changed under this pixel, and its weight rises to 1
	 * (smoothstep te .. 2 te) = the pixel's accumulation restarts from this march.
	 * cvAccReset / cvAccResetSd = cloudvolmath.h kChangeReset / kChangeResetSd; cvAccReset 0 = WW_CLOUDVOL_CHANGECLAMP=0 /
	 * WW_CLOUDVOL_RED=looseclamp (CLOUDBAND1's clamp alone). The red noclamp turns it off too (it lives inside the clamp). */
	float cvAccReset;
	float cvAccResetSd;
	/* lane CLOUD6h (CLOUD6g leftovers): History Age. The history depth target's second channel (RG32F) holds each trace's
	 * age: 0 where this frame's march wrote it, the age at its reprojected place + 1 where it followed the camera. A still
	 * camera refreshes every pixel within 16 frames (ages <= 15); under motion the content slides across the screen-space
	 * slots and some traces go many cycles without a fresh march. A trace older than cvHistAge frames gets Change Clamp's
	 * test (the history at the 9 places the march texels looked vs those 9 marches, threshold x cvAgeK): when they disagree,
	 * the march stands in (the bilinear march read where its samples sit: m x stride + phase, not the texel centre).
	 * Moving thinning 9.21 -> 7.43 levels (Medium; the march itself reads 7.63 there: the floor without a fresh sample).
	 * cvHistAge 0 = WW_CLOUDVOL_HISTAGE=0 / WW_CLOUDVOL_RED=noage (CLOUD6g's resolve). Acts with Change Clamp only.
	 * Lit Clamp: Change Clamp also compares the light (luminance of the 9 history vs the 9 march values, relative to their
	 * mean, floor cvLitFloor; threshold cvLitReset + cvAccResetSd x min( the two deviations ) / 3 / mean): a sun move left
	 * the transmittance alike and read 6.59 levels with a still camera (the variance clip alone; the Off path 3.88), ~4.1
	 * with the term. cvLitReset 0 = WW_CLOUDVOL_LITCLAMP=0 / WW_CLOUDVOL_RED=litblind. Gate tests/features/clouds/cloudage_check.py. */
	float cvHistAge;
	float cvAgeK;
	float cvLitReset;
	float cvLitFloor;
#endif
};


static float4 cvOut;
static float4 cvDepth;

float4 cvCatmull( float t )
{
	float t2 = t * t, t3 = t2 * t;
	return 0.5 * float4( -t3 + 2.0 * t2 - t, 3.0 * t3 - 5.0 * t2 + 2.0, -3.0 * t3 + 4.0 * t2 + t, t3 - t2 );
}

float4 cvMoveBox( float2 q, float2 uv, float2 pv, float4 c )
{
	if ( or( cvMoveClamp < 0.5, all( ((abs( pv - uv ) * cvHistSize) < (float2)( 0.01 )) ) ) )
		return c;
	int2 m = int2( floor( q / cvStride ) ), mhi = textureSize( cvMarch, 0 ) - 1;
	float4 lo = (float4)( 1e30 ), up = (float4)( -1e30 );
	for ( int j = -1; j <= 1; j++ )
		for ( int i = -1; i <= 1; i++ ) {
			float4 n = cvMarch.Load( int3(clamp( m + int2( i, j ), (int2)( 0 ), mhi ), 0) );
			lo = min( lo, n );
			up = max( up, n );
		}
	return clamp( c, lo, up );
}

#ifndef WW_CLOUD_ACCUM
void frag_main()
{
	float2 q = floor( gl_FragCoord.xy );
	float2 slot = mod( q, cvStride );
	float2 uv = ( q + 0.5 ) / cvHistSize;
	if ( all( (slot == cvPhase) ) ) {
		int2 m = int2( floor( q / cvStride ) );
		cvOut = cvMarch.Load( int3(m, 0) );
		cvDepth = cvMarchDepth.Load( int3(m, 0) );
		return;
	}
	float4 coarse = cvMarch.Sample( _cvMarch_sampler, uv );
	float dk = cvMarchDepth.Sample( _cvMarchDepth_sampler, uv ).r;
	if ( cvHistOn < 0.5 ) {
		cvOut = coarse;
		cvDepth = (float4)( dk );
		return;
	}
	float2 pv = uv;
	if ( cvReproj > 0.5 ) {
		float2 ndc = uv * 2.0 - 1.0;
		float3 d = normalize( mul(float3( ( ndc.x + cvProj.z ) / cvProj.x, ( ndc.y + cvProj.w ) / cvProj.y, -1.0 ), cvCam) );
		// the cloud this pixel saw last frame: the previous depth along the ray (a sky pixel: far away)
		float dp = cvPrevDepth.Sample( _cvPrevDepth_sampler, uv ).r;
		float dist = min( dp < 1e3 ? dp : dk, 400.0 );
		float3 v = mul(( cvCamKm + d * dist - cvPrevCamKm ), transpose( cvPrevCam ));
		if ( v.z >= -1e-4 ) {
			cvOut = coarse;
			cvDepth = (float4)( dk );
			return;
		}
		float2 vr = v.xy / -v.z;
		float2 pn = float2( vr.x * cvPrevProj.x - cvPrevProj.z, vr.y * cvPrevProj.y - cvPrevProj.w );
		pv = pn * 0.5 + 0.5;
		if ( or( any( (pv < (float2)( 0.0 )) ), any( (pv > (float2)( 1.0 )) ) ) ) {
			cvOut = coarse;
			cvDepth = (float4)( dk );
			return;
		}
	}
	// Catmull-Rom: a bilinear read each frame blurs the history over the up to 16 resamples before a refresh, and the
	// nearest texel drifts; the depth stays nearest (it is a distance, not a picture)
	float2 xy = pv * cvHistSize - 0.5, f = frac( xy );
	int2 i0 = int2( floor( xy ) ) - 1, hi = int2( cvHistSize ) - 1;
	float4 wx = cvCatmull( f.x ), wy = cvCatmull( f.y );
	float4 c = (float4)( 0.0 );
	for ( int j = 0; j < 4; j++ )
		for ( int i = 0; i < 4; i++ )
			c += wx[i] * wy[j] * cvPrev.Load( int3(clamp( i0 + int2( i, j ), (int2)( 0 ), hi ), 0) );
	cvOut = cvMoveBox( q, uv, pv, float4( max( c.rgb, (float3)( 0.0 ) ), clamp( c.a, 0.0, 1.0 ) ) );	// lane CLOUD6g
	cvDepth = cvPrevDepth.Load( int3(clamp( int2( floor( pv * cvHistSize ) ), (int2)( 0 ), hi ), 0) );
}
#else


float cvLum( float3 c )
{
	return dot( c, float3( 0.2126, 0.7152, 0.0722 ) );
}

//! where this pixel's cloud was in the previous history (Off's reprojection); false = no history there
bool cvReprojUv( float2 uv, float dk, out float2 pv )
{
	pv = uv;
	if ( cvReproj < 0.5 )
		return true;
	float2 ndc = uv * 2.0 - 1.0;
	float3 d = normalize( mul(float3( ( ndc.x + cvProj.z ) / cvProj.x, ( ndc.y + cvProj.w ) / cvProj.y, -1.0 ), cvCam) );
	float dp = cvPrevDepth.Sample( _cvPrevDepth_sampler, uv ).r;
	float dist = min( dp < 1e3 ? dp : dk, 400.0 );
	float3 v = mul(( cvCamKm + d * dist - cvPrevCamKm ), transpose( cvPrevCam ));
	if ( v.z >= -1e-4 )
		return false;
	float2 vr = v.xy / -v.z;
	float2 pn = float2( vr.x * cvPrevProj.x - cvPrevProj.z, vr.y * cvPrevProj.y - cvPrevProj.w );
	pv = pn * 0.5 + 0.5;
	return all( (pv >= (float2)( 0.0 )) ) && all( (pv <= (float2)( 1.0 )) );
}

void frag_main()
{
	float2 q = floor( gl_FragCoord.xy );
	float2 slot = mod( q, cvStride );
	float2 uv = ( q + 0.5 ) / cvHistSize;
	bool fresh = all( (slot == cvPhase) );
	int2 m = int2( floor( q / cvStride ) );
	// lane CLOUD6h: with History Age, the march is read where its samples sit (texel m looked at m x stride + phase)
	float2 muv = cvHistAge > 0.0 ? ( ( q - cvPhase ) / cvStride + 0.5 ) / float2( textureSize( cvMarch, 0 ) ) : uv;
	float4 coarse = fresh ? cvMarch.Load( int3(m, 0) ) : cvMarch.Sample( _cvMarch_sampler, muv );
	float dk = fresh ? cvMarchDepth.Load( int3(m, 0) ).r : cvMarchDepth.Sample( _cvMarchDepth_sampler, uv ).r;
	float2 pv;
	if ( or( cvHistOn < 0.5, !cvReprojUv( uv, dk, pv ) ) ) {
		cvOut = coarse;
		cvDepth = float4( dk, 0.0, 0.0, 0.0 );	// lane CLOUD6h: .g = the age (0: this frame's march)
		return;
	}
	// the history at pv: Catmull-Rom colour, nearest depth (as Off)
	float2 xy = pv * cvHistSize - 0.5, f = frac( xy );
	int2 i0 = int2( floor( xy ) ) - 1, hi = int2( cvHistSize ) - 1;
	float4 wx = cvCatmull( f.x ), wy = cvCatmull( f.y );
	float4 c = (float4)( 0.0 );
	for ( int j = 0; j < 4; j++ )
		for ( int i = 0; i < 4; i++ )
			c += wx[i] * wy[j] * cvPrev.Load( int3(clamp( i0 + int2( i, j ), (int2)( 0 ), hi ), 0) );
	c = float4( max( c.rgb, (float3)( 0.0 ) ), clamp( c.a, 0.0, 1.0 ) );
	int2 hp0 = clamp( int2( floor( pv * cvHistSize ) ), (int2)( 0 ), hi );
	float2 hd = cvPrevDepth.Load( int3(hp0, 0) ).rg;	// lane CLOUD6h: the nearest trace's depth and age
	float age = hd.g + 1.0;
	if ( !fresh ) {
		cvOut = cvMoveBox( q, uv, pv, c );	// lane CLOUD6g
		if ( cvHistAge > 0.0 && age > cvHistAge && cvAccReset > 0.0 ) {	// lane CLOUD6h: an old trace
			// the 9 places the march texels looked this frame (m x stride + phase), moved as this pixel moved
			int2 mhi = textureSize( cvMarch, 0 ) - 1, hp = hp0 + int2( float2( m ) * cvStride + cvPhase - q );
			float s1 = 0.0, s2 = 0.0, h1 = 0.0, h2 = 0.0;
			for ( int j = -1; j <= 1; j++ )
				for ( int i = -1; i <= 1; i++ ) {
					float n = cvMarch.Load( int3(clamp( m + int2( i, j ), (int2)( 0 ), mhi ), 0) ).a;
					s1 += n;
					s2 += n * n;
					float ha = cvPrev.Load( int3(clamp( hp + int( cvStride ) * int2( i, j ), (int2)( 0 ), hi ), 0) ).a;
					h1 += ha;
					h2 += ha * ha;
				}
			float mu = s1 / 9.0, sd = sqrt( max( s2 / 9.0 - mu * mu, 0.0 ) );
			float hm = h1 / 9.0, hsd = sqrt( max( h2 / 9.0 - hm * hm, 0.0 ) );
			float te = cvAgeK * ( cvAccReset + cvAccResetSd * min( sd, hsd ) / 3.0 );
			float w = smoothstep( te, 2.0 * te, abs( hm - mu ) );
			cvOut = lerp( cvOut, coarse, w );
			age = lerp( age, 0.0, w );
		}
		cvDepth = float4( hd.r, age, 0.0, 0.0 );
		return;
	}
	float a = cvAccAlpha;
	if ( cvAccClamp > 0.5 ) {
		// the 3x3 box of the fresh marches, tightened to their mean +- one deviation (variance clipping, Salvi 2016: the
		// march texels sit 4 history px apart, so the bare min / max box held most of a textured old field), never
		// tighter than the fresh march itself
		int2 mhi = textureSize( cvMarch, 0 ) - 1;
		float4 lo = coarse, up = coarse, s1 = (float4)( 0.0 ), s2 = (float4)( 0.0 );
		for ( int j = -1; j <= 1; j++ )
			for ( int i = -1; i <= 1; i++ ) {
				float4 n = cvMarch.Load( int3(clamp( m + int2( i, j ), (int2)( 0 ), mhi ), 0) );
				lo = min( lo, n );
				up = max( up, n );
				s1 += n;
				s2 += n * n;
			}
		float4 mu = s1 / 9.0, sd = sqrt( max( s2 / 9.0 - mu * mu, (float4)( 0.0 ) ) );
		lo = min( max( lo, mu - sd ), coarse );
		up = max( min( up, mu + sd ), coarse );
		c = clamp( c, lo, up );
		if ( cvAccReset > 0.0 ) {	// lane CLOUD6g: Change Clamp
			int2 hp = int2( floor( pv * cvHistSize ) );
			float h1 = 0.0, h2 = 0.0, l1 = 0.0, l2 = 0.0, f1 = 0.0, f2 = 0.0;
			for ( int j = -1; j <= 1; j++ )
				for ( int i = -1; i <= 1; i++ ) {
					float4 hv = cvPrev.Load( int3(clamp( hp + int( cvStride ) * int2( i, j ), (int2)( 0 ), hi ), 0) );
					h1 += hv.a;
					h2 += hv.a * hv.a;
					if ( cvLitReset > 0.0 ) {	// lane CLOUD6h: Lit Clamp, the light at the same 9 places
						float hl = cvLum( hv.rgb ), fl = cvLum( cvMarch.Load( int3(clamp( m + int2( i, j ), (int2)( 0 ), mhi ), 0) ).rgb );
						l1 += hl;
						l2 += hl * hl;
						f1 += fl;
						f2 += fl * fl;
					}
				}
			float hm = h1 / 9.0, hsd = sqrt( max( h2 / 9.0 - hm * hm, 0.0 ) );
			float te = cvAccReset + cvAccResetSd * min( sd.a, hsd ) / 3.0;
			a = max( a, smoothstep( te, 2.0 * te, abs( hm - mu.a ) ) );
			if ( cvLitReset > 0.0 ) {
				float lm = l1 / 9.0, lsd = sqrt( max( l2 / 9.0 - lm * lm, 0.0 ) );
				float fm = f1 / 9.0, fsd = sqrt( max( f2 / 9.0 - fm * fm, 0.0 ) );
				float lref = max( 0.5 * ( lm + fm ), cvLitFloor );
				float tl = cvLitReset + cvAccResetSd * min( fsd, lsd ) / ( 3.0 * lref );
				a = max( a, smoothstep( tl, 2.0 * tl, abs( lm - fm ) / lref ) );
			}
		}
	}
	cvOut = lerp( c, coarse, a );
	cvDepth = float4( dk, 0.0, 0.0, 0.0 );	// the fresh march's mean distance (a mix with the 1e4 of no cloud means nothing); age 0
}
#endif

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 cvOut : SV_Target0;
    float4 cvDepth : SV_Target1;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.cvOut = cvOut;
    stage_output.cvDepth = cvDepth;
    return stage_output;
}

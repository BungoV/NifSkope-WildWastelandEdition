// HLSL source of features/shadows/ww_contact.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "ShadowsBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
// Copyright 2023 Sony Interactive Entertainment.
// Licensed under the Apache License, Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0).
//
// lane TERRSHADOW1: contact shadows, a per-pixel OpenGL port of Bend Studio's screen-space shadows (the
// light's projected coordinate, the march along the screen line to it, the perspective-correct ray depth
// lin0 * dist0 / dist, the minor-axis bilinear neighbour with its edge test, the thickness window and the
// four-lane contrast resolve). Bend's wavefront/LDS layout is a compute shader; GL 4.1 has none, so every
// pixel marches its own line, in linear view depth (game units, the shared opaque pass's .a).

Texture2D<float4> gbuf : register(t0, space0); SamplerState _gbuf_sampler : register(s0, space0);		// RGBA32F, bottom row first: view normal, linear depth (game units; 1e6 = none)
cbuffer LooseUniforms : register(b31, space0)
{
	int2 fullSize;
	float maskScale;	// full pixels per mask pixel (1 or 2)
	float4 lightCoord;	// the light's projected point in mask pixels (xy), unused z, w = sign of clip w
	float4 sssParams;		// thickness (fraction of depth), bilinear threshold, contrast, samples
	float sssFloor;		// the thickness floor, game units
	int sssPar;		// lane SHADOWPAR1: FO4CS's evidence, completion and edge fades, plane and normal rejects
	float3 sssLightView;	// the direction TO the light, view space (the normal reject)
	int sssExit;		// lane SHADOWPAR1FIX: stop once every lane is at the contrast floor (the output cannot change)
	// lane FIX1101A (LATE ITEM 14): the march's world reach inside the cascades' reach. x the reach (game units; 0 = none:
	// the far tier, the red), y + this share of the receiver's view depth, z the cascades' reach D (the reach opens up as
	// their fade weight 1 - s^4, s = (|P| / D)^2, drops), w unused. sssTan: the projection's tan(half fov) x, y.
	float4 sssReach;
	float2 sssTan;
};


static float fragMask;

// lane SHADOWPAR1: FO4CS's receiver plane (bend_sss_gpu.hlsli) -- the depth 4 pixels either side along the line
static const float kPlaneBias = 12.0;	// fReceiverPlaneBias, game units, ramped over the first 32 samples
static const float kNormalReject = 0.1;	// a caster facing the light edge-on (|N.L| <= 0.1) is no evidence
static const float kEvidence = 4.0;	// fContactEvidenceSamples
static const float kEdgeBand = 16.0;	// fEdgeFadeBandPx

float3 normalAt( float2 maskPx )
{
	int2 q = clamp( int2( floor( maskPx * maskScale ) ), (int2)( 0 ), fullSize - 1 );
	return gbuf.Load( int3(q, 0) ).rgb;
}

//! lane FIX1101A: a mask pixel at linear depth z -> view position (game units), for the reach
float3 sssViewPos( float2 maskPx, float z )
{
	float2 ndc = maskPx * maskScale / float2( fullSize ) * 2.0 - 1.0;
	return float3( ndc.x * sssTan.x * z, ndc.y * sssTan.y * z, z );
}

float linAt( float2 maskPx, out bool valid )
{
	int2 q = int2( floor( maskPx * maskScale ) );
	if ( or( any( (q < (int2)( 0 )) ), any( (q >= fullSize) ) ) ) {
		valid = false;
		return 1e6;
	}
	float l = gbuf.Load( int3(q, 0) ).a;
	valid = l < 9.0e5;
	return l;
}

void frag_main()
{
	float2 p = gl_FragCoord.xy;
	bool v0;
	float lin0 = linAt( p, v0 );
	fragMask = 1.0;
	if ( or( !v0, lin0 <= 0.0 ) )
		return;
	float2 delta = p - lightCoord.xy;
	bool majorX = abs( delta.x ) > abs( delta.y );
	float dist0 = majorX ? abs( delta.x ) : abs( delta.y );
	if ( dist0 < 1.0 )
		return;
	// light in front (w > 0): the march heads for its vanishing point; behind: away from it
	float direction = lightCoord.w > 0.0 ? -1.0 : 1.0;
	float2 stepXY = delta / dist0 * direction;	// one mask pixel along the major axis
	int n = int( sssParams.w );
	float contrast = sssParams.z;
	float4 lanes = (float4)( 1.0 );
	bool par = sssPar != 0;
	// the receiver plane: 1/z is affine along the line, its slope from the depths 4 pixels either side
	bool va4, vb4;
	float lf = linAt( p + stepXY * 4.0, va4 ), lr = linAt( p - stepXY * 4.0, vb4 );
	bool plane = par && va4 && vb4 && abs( lf - lr ) < 0.25 * lin0;
	float invSlope = plane ? ( 1.0 / lf - 1.0 / lr ) / 8.0 : 0.0;
	float hard = 1.0, support = 0.0;
	int exitK = n;
	/* lane FIX1101A (LATE ITEM 14): inside the cascades' reach the march stops past sssReach.x + sssReach.y x depth game
	 * units from the receiver: a thin caster further off (a power line 85 u in front of a wall) hit only a sample or two
	 * of the 4 lanes, so its shadow came out as a row of dashes beside the cascades' own soft line (bungo's "banding");
	 * the cascade map holds such casters. Past the reach it opens up as the cascades fade out. */
	float reachEff = 1e30;
	float3 pv0 = (float3)( 0.0 );
	if ( sssReach.x > 0.0 ) {
		pv0 = sssViewPos( p, lin0 );
		float s = clamp( dot( pv0, pv0 ) / max( sssReach.z * sssReach.z, 1.0 ), 0.0, 1.0 );
		float s2 = s * s;
		float wgt = 1.0 - s2 * s2;
		if ( wgt > 0.0 )
			reachEff = ( sssReach.x + sssReach.y * lin0 ) / wgt;
	}
	for ( int k = 1; k <= 384; k++ ) {
		if ( k > n )
			break;
		float dist = dist0 + float( k ) * direction;
		if ( dist < 1.0 )
			break;
		float2 q = p + stepXY * float( k );
		if ( sssReach.x > 0.0 && distance( sssViewPos( q, lin0 * dist0 / dist ), pv0 ) > reachEff )
			break;
		if ( par && exitK == n && ( or( any( ((q * maskScale) < (float2)( 0.0 )) ), any( ((q * maskScale) >= float2( fullSize )) ) ) ) )
			exitK = k;
		// Bend's minor-axis bilinear pair: the nearer neighbour across the line, point-filtered at an edge
		float minor = majorX ? q.y : q.x;
		float f = frac( minor ) - 0.5;
		float2 off = majorX ? float2( 0.0, f > 0.0 ? 1.0 : -1.0 ) : float2( f > 0.0 ? 1.0 : -1.0, 0.0 );
		bool va, vb;
		float la = linAt( q, va );
		float lb = linAt( q + off, vb );
		if ( !va )
			continue;	// sky or off the screen: empty, never an occluder
		bool edge = or( !vb, abs( la - lb ) > la * sssParams.y );
		float ls = edge ? la : lerp( la, lb, abs( f ) );
		float rayLin = lin0 * dist0 / dist;	// the ray's depth at this pixel (1/z is affine along the line)
		float thick = max( sssFloor, sssParams.x * rayLin );
		float d = abs( ( rayLin - ls ) / thick - 1.0 );
		if ( par ) {
			// the receiver's own plane is no occluder (within the bias, ramped in over 32 samples)
			if ( plane ) {
				float planeLin = 1.0 / max( 1.0 / lin0 + invSlope * float( k ), 1e-9 );
				if ( ls > planeLin - kPlaneBias * clamp( float( k ) / 32.0, 0.125, 1.0 ) )
					d = 1e10;
			}
			// a caster edge-on to the light
			if ( d < 1.0 && abs( dot( normalAt( q ), sssLightView ) ) <= kNormalReject )
				d = 1e10;
			// the completion fade: the last 16 samples weigh less
			float c = float( n - k ) / 16.0;
			float fadeOut = k > n - 16 ? clamp( 1.0 - c, 0.0, 1.0 ) * 0.75 : 0.0;
			if ( k <= 4 ) {
				hard = min( hard, d );
				support += clamp( ( 1.0 - d ) * contrast, 0.0, 1.0 );
			}
			d += fadeOut;
		}
		int lane = k & 3;
		lanes[lane] = min( lanes[lane], d );
		// every lane resolves to 0 from here; past the hard samples and the 16-pixel edge band nothing else can move
		if ( sssExit != 0 && k >= 4 && float( k ) * maskScale >= kEdgeBand && all( (lanes <= (float4)( 1.0 - 1.0 / contrast )) ) )
			break;
	}
	lanes = clamp( lanes * contrast + ( 1.0 - contrast ), 0.0, 1.0 );
	fragMask = dot( lanes, (float4)( 0.25 ) );
	if ( par ) {
		// the hard samples count only with enough evidence; the ray that leaves the screen early fades out
		float h = lerp( 1.0, clamp( hard * contrast + ( 1.0 - contrast ), 0.0, 1.0 ), clamp( support / kEvidence, 0.0, 1.0 ) );
		fragMask = min( h, fragMask );
		fragMask = lerp( 1.0, fragMask, smoothstep( 0.0, kEdgeBand, float( exitK ) * maskScale ) );
	}
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float fragMask : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragMask = fragMask;
    return stage_output;
}

// HLSL source of features/reflections/cell_ssr.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "ReflectionsBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
/* The game's screen-space reflections for the cell view (src/features/reflections/cellssr.cpp). Targets are half size, top row
 * first; gbuf and scene (opaque lit color without reflection, obscurance and fog; alpha = reflection flag) are
 * full size, bottom row first. Stages: 1 the reflected ray run back to the near plane, 2 a hierarchical march over
 * depth mips 1..4 (max 32 steps; alpha = confidence squared), 3 and 4 a 5-tap blur across and down.
 * Deviation from the game: float targets (the game stores the march and blurs as 8-bit sRGB).
 * WW_CELL_SSR_PREV=1 (ships off): a stage 2 hit samples the previous frame's linear color, reprojected. */

cbuffer LooseUniforms : register(b31, space0)
{
	int ssrStage;
	int2 fullSize;
	int2 halfSize;
	int2 mipSize[5];	// [0] = fullSize
	float2 proj;			// the projection's P00, P11
	row_major float3x3 viewToWorld;
	float4 ssrK;			// the game's four: color scale 1, angle gate 0.2, normal z scale 2, confidence scale 1
	float2 ssrClip;		// the game camera's near (15) and far (the cell's clip distance), game units
	int ssrRed;			// WW_CELL_SSR_RED: 2 nogap (no 50-unit refusal), 4 nofade (confidence 1 on a hit)
	bool haveWaterRay;
	bool opaqueRays;	// false in an exterior: the water's rays alone
	bool ssrPrev;		// lane PERF3: the hit's color from prevScene
	row_major float3x3 ssrPrevRot;	// this view -> the previous view (game units): ssrPrevRot * P + ssrPrevT
	float3 ssrPrevT;
	float2 ssrPrevProj;	// the previous projection's P00, P11
};

Texture2D<float4> gbuf : register(t0, space0); SamplerState _gbuf_sampler : register(s0, space0);
Texture2D<float4> zMip1 : register(t1, space0); SamplerState _zMip1_sampler : register(s1, space0);
Texture2D<float4> zMip2 : register(t2, space0); SamplerState _zMip2_sampler : register(s2, space0);
Texture2D<float4> zMip3 : register(t3, space0); SamplerState _zMip3_sampler : register(s3, space0);
Texture2D<float4> zMip4 : register(t4, space0); SamplerState _zMip4_sampler : register(s4, space0);
Texture2D<float4> scene : register(t5, space0); SamplerState _scene_sampler : register(s5, space0);
Texture2D<float4> src : register(t6, space0); SamplerState _src_sampler : register(s6, space0);
Texture2D<float4> waterRay : register(t7, space0); SamplerState _waterRay_sampler : register(s7, space0);	// full size, bottom row first: the water's ray in the view, its view depth
Texture2D<float4> prevScene : register(t8, space0); SamplerState _prevScene_sampler : register(s8, space0);	// the previous frame's linear color (full size, bottom row first, bilinear)

static float4 fragColor;

#ifdef WW_SSR_WATER_LOOK	// lane WATERREFL1 (cell_ssrwater.prog): a water ray (stage 1 alpha 2) marches 64 steps, colour unclamped
#define WW_SSR_STEPS ( waterLook ? 64 : 32 )
#define WW_SSR_HITMAX ( waterLook ? 64.0 : 1.0 )
// lane FIX1101A: a water ray takes a surface as thick as 6 % of its depth (at least the game's 50 u): a far bank's
// rays cross ~100 u of depth a texel near the horizon, so the game's 50 u refused most of them; red 8 = 50 u
#define WW_SSR_THICK( d ) ( waterLook && ( ssrRed & 8 ) == 0 ? max( 50.0, 0.06 * ( d ) ) : 50.0 )
#else
#define WW_SSR_STEPS 32
#define WW_SSR_HITMAX 1.0
#define WW_SSR_THICK( d ) 50.0
#endif

// the game's load: outside the mip reads 0
float zLoad( int m, float2 cell )
{
	int2 t = int2( cell );
	if ( or( or( or( t.x < 0, t.y < 0 ), t.x >= mipSize[m].x ), t.y >= mipSize[m].y ) )
		return 0.0;
	if ( m == 1 )
		return zMip1.Load( int3(t, 0) ).r;
	if ( m == 2 )
		return zMip2.Load( int3(t, 0) ).r;
	if ( m == 3 )
		return zMip3.Load( int3(t, 0) ).r;
	return zMip4.Load( int3(t, 0) ).r;
}

static const float kDither[16] = { 0.0, 0.5, 0.125, 0.625, 0.75, 0.22, 0.875, 0.375,
                                     0.1875, 0.6875, 0.0625, 0.5625, 0.9375, 0.4375, 0.8125, 0.3125 };

bool rayOut( float3 p )
{
	return or( or( or( or( p.x <= 0.0, p.x >= 1.0 ), p.y <= 0.0 ), p.y >= 1.0 ), 1.0 / p.z >= ssrClip.y );
}

void frag_main()
{
	int2 q = int2( gl_FragCoord.xy );
	float2 uv = ( float2( q ) + 0.5 ) / float2( halfSize );
	fragColor = (float4)( 0.0 );

	if ( ssrStage == 0 ) {	// lane PERF3: the flag (the stencil test passes flagged pixels only)
		fragColor = float4( 0.0, 0.0, 0.0, 1.0 );
		return;
	}

	if ( ssrStage == 5 ) {	// lane PERFFIX1: the bounce's donor (full size, bottom row first): the previous frame's colour
		int2 t = int2( gl_FragCoord.xy );	// where this pixel's surface was in it (the background: its direction)
		float z = gbuf.Load( int3(t, 0) ).a;
		float2 u = ( float2( t ) + 0.5 ) / float2( fullSize );
		float3 P = float3( ( 2.0 * u.x - 1.0 ) * z / proj.x, ( 2.0 * u.y - 1.0 ) * z / proj.y, -z );
		float3 Pp = mul(P, ssrPrevRot) + ssrPrevT;
		float zp = max( -Pp.z, ssrClip.x );
		float2 up = float2( 0.5 + 0.5 * ssrPrevProj.x * Pp.x / zp, 0.5 + 0.5 * ssrPrevProj.y * Pp.y / zp );
		fragColor = float4( prevScene.SampleLevel( _prevScene_sampler, clamp( up, (float2)( 0.0 ), (float2)( 1.0 ) ), 0.0 ).rgb, 0.0 );
		return;
	}

	if ( ssrStage == 1 ) {
		int2 t = min( ( ( 2 * q + 1 ) * fullSize ) / ( 2 * halfSize ), fullSize - 1 );
		t.y = fullSize.y - 1 - t.y;
		float3 P, R;
		float z;
#ifdef WW_SSR_WATER_LOOK
		float kind = 1.0;
#endif
		float4 wr = haveWaterRay ? waterRay.Load( int3(t, 0) ) : (float4)( 0.0 );
		if ( wr.a > 0.0 ) {
			// the water lies in front here; its pass already gated and wrote the ray
			if ( dot( wr.xyz, wr.xyz ) == 0.0 )
				return;
			z = wr.a;
			P = float3( ( 2.0 * uv.x - 1.0 ) * z / proj.x, ( 1.0 - 2.0 * uv.y ) * z / proj.y, -z );
			R = wr.xyz;
#ifdef WW_SSR_WATER_LOOK
			kind = 2.0;
#endif
		} else {
			if ( or( !opaqueRays, scene.Load( int3(t, 0) ).a * ssrK.w - 0.01 < 0.0 ) )
				return;
			float4 g = gbuf.Load( int3(t, 0) );
			z = g.a;
			P = float3( ( 2.0 * uv.x - 1.0 ) * z / proj.x, ( 1.0 - 2.0 * uv.y ) * z / proj.y, -z );
			float3 V = -normalize( P );
			if ( dot( g.xyz, V ) < 0.0 )
				return;
			float3 Nw = mul(g.xyz, viewToWorld);
			Nw.z *= ssrK.z;
			float3 N = mul(viewToWorld, normalize( Nw ));	// back into the view
			R = reflect( -V, N );
			if ( !( -R.z > ssrK.y ) )
				return;
		}
		float3 Q = P + 1000.0 * R;
		float zq = -Q.z;
		float2 uvq = float2( 0.5 + 0.5 * proj.x * Q.x / zq, 0.5 - 0.5 * proj.y * Q.y / zq );
		// the screen line is straight in (u, v, 1 / depth): run it back to depth = near
		float s = ( 1.0 / ssrClip.x - 1.0 / z ) / ( 1.0 / zq - 1.0 / z );
#ifdef WW_SSR_WATER_LOOK
		fragColor = float4( uv + ( uvq - uv ) * s, z, kind );
		return;
#endif
		fragColor = float4( uv + ( uvq - uv ) * s, z, 1.0 );
		return;
	}

	if ( ssrStage == 2 ) {
		float4 ray = src.Load( int3(q, 0) );
		float z0 = ray.z;
		if ( !( ssrClip.x < z0 ) )
			return;
		float dj = kDither[( q.y & 3 ) + 4 * ( q.x & 3 )];
#ifdef WW_SSR_WATER_LOOK
		bool waterLook = ray.w > 1.5;
#endif
		float3 A = float3( ray.xy, 1.0 / ssrClip.x + ( dj - 0.5 ) * 0.004 );
		float3 D = float3( uv, 1.0 / z0 ) - A;
		float2 S = uv + D.xy * dj * 0.002;
		float2 G = floor( float2( halfSize ) / 8.0 );
		float2 cell = floor( G * S );
		int level = 4;
		float2 s01 = float2( (D.xy >= (float2)( 0.0 )) );
		float2 sgn = 2.0 * s01 - 1.0;
		float2 stepv;
		float3 p;
		// one cell on: the nearer of the cell's two far edges
		float2 tc = ( ( cell + s01 ) / G - A.xy ) / D.xy;
		bool xfirst = tc.y >= tc.x;
		stepv = sgn * float2( xfirst ? 1.0 : 0.0, xfirst ? 0.0 : 1.0 );
		cell += stepv;
		p = A + min( tc.x, tc.y ) * D;
		float zfirst = 1.0 / p.z;
		bool alive = true, refused = false;
		int count = 0;
		float d = 0.0;
		for ( int n = 1; n <= WW_SSR_STEPS; n++ ) {
			alive = alive && level >= 1 && !rayOut( p );
			if ( !alive )
				break;
			d = zLoad( level, cell );
			float rz = 1.0 / p.z;
			bool isL1 = level == 1 && ( ssrRed & 2 ) == 0;
			bool ref = or( refused, ( isL1 && rz - d >= WW_SSR_THICK( d ) ) );
			if ( d < rz && !ref ) {
				level -= 1;
				float2 h = float2( ((p.xy - ( cell + 0.5 ) / G) >= (float2)( 0.0 )) );
				cell = 2.0 * cell + h;
				G *= 2.0;
				refused = false;
			} else {
				tc = ( ( cell + s01 ) / G - A.xy ) / D.xy;
				xfirst = tc.y >= tc.x;
				stepv = sgn * float2( xfirst ? 1.0 : 0.0, xfirst ? 0.0 : 1.0 );
				cell += stepv;
				p = A + min( tc.x, tc.y ) * D;
				float rz2 = 1.0 / p.z;
				refused = or( ref, ( isL1 && rz2 - d >= WW_SSR_THICK( d ) ) );
				if ( !refused ) {
					if ( d < rz2 ) {
						p = A + ( ( 1.0 / d - A.z ) / D.z ) * D;
						level -= 1;
						G *= 2.0;
						cell = floor( G * p.xy );
					} else if ( level != 4 ) {
						G = floor( G / 2.0 );
						cell = cell / 2.0 + float2( stepv.x >= 0.0 ? 0.0 : -0.5, stepv.y >= 0.0 ? 0.0 : -0.5 );
						level += 1;
					}
				}
			}
			count = n;
		}
		if ( or( or( rayOut( p ), count == WW_SSR_STEPS ), refused ) )
			return;
		float c = 1.0 - pow( min( 2.0 * length( p.xy - 0.5 ), 1.0 ), 2.0 );
		c *= max( 1.0 - 2.0 * length( S - p.xy ), 0.0 );
		c *= clamp( 1.0 - 25.0 * ( d - zfirst ) / ( ssrClip.y - ssrClip.x ), 0.0, 1.0 );
		if ( ( ssrRed & 4 ) != 0 )
			c = 1.0;
#ifdef WW_SSR_WATER_LOOK
		if ( waterLook ) {	// the far bank half a screen away stays: the screen edges fade (10% a side), nothing else
			float2 e = min( p.xy, 1.0 - p.xy );
			c = smoothstep( 0.0, 0.1, e.x ) * smoothstep( 0.0, 0.1, e.y );
			// lane FIX1101A: a ray near the end of its steps fades to the miss (no seam where the budget ends); red 8
			if ( ( ssrRed & 8 ) == 0 )
				c *= clamp( float( WW_SSR_STEPS - count ) / 8.0, 0.0, 1.0 );
		}
#endif
		if ( ssrPrev ) {	// lane PERF3: the hit in the previous frame's view; off its screen or behind it: no hit
			float zh = 1.0 / p.z;
			float3 H = float3( ( 2.0 * p.x - 1.0 ) * zh / proj.x, ( 1.0 - 2.0 * p.y ) * zh / proj.y, -zh );
			float3 Hp = mul(H, ssrPrevRot) + ssrPrevT;
			float zp = -Hp.z;
			if ( !( zp > ssrClip.x ) )
				return;
			float2 up = float2( 0.5 + 0.5 * ssrPrevProj.x * Hp.x / zp, 0.5 - 0.5 * ssrPrevProj.y * Hp.y / zp );
			if ( or( or( or( up.x <= 0.0, up.x >= 1.0 ), up.y <= 0.0 ), up.y >= 1.0 ) )
				return;
			fragColor = float4( clamp( prevScene.SampleLevel( _prevScene_sampler, float2( up.x, 1.0 - up.y ), 0.0 ).rgb, 0.0, WW_SSR_HITMAX ), c * c );
			return;
		}
		fragColor = float4( clamp( scene.SampleLevel( _scene_sampler, float2( p.x, 1.0 - p.y ), 0.0 ).rgb, 0.0, WW_SSR_HITMAX ), c * c );
		return;
	}

	// stages 3, 4: the blur (src bilinear)
	const float off[5] = { -3.294215, -1.407333, 0.0, 1.407333, 3.294215 };
	const float wgt[5] = { 0.093913, 0.304005, 0.204164, 0.304005, 0.093913 };
	float2 dir = ssrStage == 3 ? float2( 1.0 / float( halfSize.x ), 0.0 ) : float2( 0.0, 1.0 / float( halfSize.y ) );
	float4 tap[5];
	float lost = 0.0, present = 0.0, a = 0.0;
	for ( int i = 0; i < 5; i++ ) {
		tap[i] = src.SampleLevel( _src_sampler, uv + dir * off[i], 0.0 );
		a += wgt[i] * tap[i].a;
		if ( tap[i].a < 0.01 )
			lost += wgt[i];
		else
			present += 1.0;
	}
	float3 rgb = (float3)( 0.0 );
	if ( present > 0.0 ) {
		float e = lost / present;
		for ( int i = 0; i < 5; i++ )
			if ( !( tap[i].a < 0.01 ) )
				rgb += ( wgt[i] + e ) * tap[i].rgb;
	}
	fragColor = float4( rgb, a );
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}

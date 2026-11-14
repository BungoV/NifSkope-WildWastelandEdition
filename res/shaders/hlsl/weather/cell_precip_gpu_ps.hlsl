// HLSL source of features/weather/cell_precip_gpu.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "WeatherBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
/* lane PERFFIX3 D (2026-11-01): a precipitation particle in the cell view's linear frame, lit per PARTICLE
 * (cell_precip_gpu.geom: gA, gB). Per fragment only the texture, the opacity and the near-eye fade remain:
 * colour = albedo x gA + gB (= wwFogFx( albedo x lit ), the fog being affine in the colour). Alpha blended, no depth
 * write (the scene's depth tests it). */

Texture2D<float4> pcTex : register(t0, space0); SamplerState _pcTex_sampler : register(s0, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	bool pcHasTex;
	int pcFar;
	float4 pcAtlas;		// cell uv size, the deepest mip a near quad reads, the one-texel-a-cell mip
	float2 pcNear;		// view units from the eye: gone at x, whole at y (a flake never covers the lens)
#if defined(PC_LOOK)
	/* lane PRECIPLOOK1 (cell_precip_look.prog). pcLook 1 = a raindrop: no texture; the opacity is the time-averaged
	 * coverage of the drop's disc (radius hd) swept over the streak (length tl) during the shutter, which conserves the
	 * disc's area x time (Garg & Nayar 2006 streak model); its light flickers along the streak with the drop's n = 2
	 * oscillation (gDrop.z depth, gDrop.w cycles). The water lens itself (Fresnel + refraction) is the phase table the
	 * geometry stage lit it with (cell_volpart.glsl pcDropPi), so the albedo is 1: no grey drop.
	 * pcLook 2 = a snow flake with its normal map: the thin share per texel m = clamp( pcTrans (1 - a) / 0.4 ), the
	 * thick share turned by the normal toward the key light (wrap( N.L, w ) / wrap( L.z, w ), w = pcTrans), and a small ice
	 * glint at N.H (the flake's facets: Blinn 64, F0 0.02). pcClump (t, k): the merged clump atlas's density threshold
	 * (alpha = clamp( (d - t) / k )): low t = loose dry, high t = compact wet; pcAlbedo the material's colour. */
	int pcLook;
	bool pcHasN;
	float2 pcClump;
	float pcAlbedo;
	float pcTrans;
#endif
};


static float2 gUv;
static float2 gQ;
static float3 gView;
static float4 gCol;
static float4 gBox;
static float3 gA;
static float3 gB;
#ifdef PC_LOOK

Texture2D<float4> pcTexN : register(t1, space0); SamplerState _pcTexN_sampler : register(s1, space0);
static float4 gDrop;
static float gOsc;
static float3 gThin;
static float3 gThick;
static float4 gDomL;
static float3 gDomE;
static float2 gAx;

float pcWrap( float x, float w )
{
	return max( ( x + w ) / ( 1.0 + w ), 0.0 );
}
#endif

static float4 fragColor;

void frag_main()
{
	float4 t;
#ifdef PC_LOOK
	if ( pcLook == 1 ) {
		// the swept disc: along the streak s, across u (view units), the quad's half sizes hd + tl / 2 and hd
		float hd = gDrop.x, tl = gDrop.y;
		float s = gQ.x * ( hd + 0.5 * tl ), u = gQ.y * hd;
		float c2 = hd * hd - u * u;
		float cov = 0.0;
		if ( c2 > 0.0 ) {
			float c = sqrt( c2 );
			if ( tl > 1e-4 * hd )
				cov = clamp( ( min( s + c, 0.5 * tl ) - max( s - c, -0.5 * tl ) ) / tl, 0.0, 1.0 );
			else
				cov = abs( s ) < c ? 1.0 : 0.0;
		}
		float a = clamp( cov * gCol.a, 0.0, 1.0 ) * smoothstep( pcNear.x, pcNear.y, -gView.z );
		if ( a < 1.0 / 512.0 )
			discard;
		float tf = tl > 1e-4 * hd ? clamp( s / tl + 0.5, 0.0, 1.0 ) : 0.5;
		float fl = 1.0 + gDrop.z * sin( 6.2831853 * gDrop.w * tf + gOsc );
		fragColor = float4( max( gCol.rgb * gA * fl + gB, (float3)( 0.0 ) ), a );
		return;
	}
#endif
	if ( pcFar != 0 ) {
		// a far dot: the cell's mean (gCol) over a soft round footprint whose mean over the quad is 1
		t = float4( 1.0, 1.0, 1.0, exp( -2.5 * dot( gQ, gQ ) ) / 0.2985 );
	} else if ( pcHasTex ) {
		float lod = min( float2(pcTex.CalculateLevelOfDetail( _pcTex_sampler, gUv ), pcTex.CalculateLevelOfDetailUnclamped(_pcTex_sampler, gUv)).x, pcAtlas.z );
		t = pcTex.SampleLevel( _pcTex_sampler, clamp( gUv, gBox.xy, gBox.zw ), lod );
#ifdef PC_LOOK
		t.a = clamp( ( t.a - pcClump.x ) / pcClump.y, 0.0, 1.0 );
#endif
	} else {
		t = float4( 1.0, 1.0, 1.0, clamp( 1.0 - dot( gQ, gQ ), 0.0, 1.0 ) );
	}
	float a = clamp( t.a * gCol.a, 0.0, 1.0 ) * smoothstep( pcNear.x, pcNear.y, -gView.z );
	if ( a < 1.0 / 512.0 )
		discard;
	float3 alb = t.rgb * gCol.rgb;	// linear: the sRGB texture decodes on read
#ifdef PC_LOOK
	alb *= pcAlbedo;
	if ( pcLook == 2 && pcHasN && pcFar == 0 ) {
		float lod = min( float2(pcTexN.CalculateLevelOfDetail( _pcTexN_sampler, gUv ), pcTexN.CalculateLevelOfDetailUnclamped(_pcTexN_sampler, gUv)).x, pcAtlas.z );
		float3 nt = pcTexN.SampleLevel( _pcTexN_sampler, clamp( gUv, gBox.xy, gBox.zw ), lod ).rgb;
		float3 nm = float3( 2.0 * nt.r - 1.0, 1.0 - 2.0 * nt.g, 2.0 * nt.b - 1.0 );	// DirectX green: y up in the image
		float2 ay = float2( -gAx.y, gAx.x );	// the image's down is the quad's +v
		float3 N = normalize( float3( gAx * nm.x - ay * nm.y, max( nm.z, 0.0 ) ) );
		float3 L = gDomL.xyz;
		float m = clamp( pcTrans * ( 1.0 - t.a ) / 0.4, 0.0, 1.0 );
		float r = min( pcWrap( dot( N, L ), pcTrans ) / max( pcWrap( L.z, pcTrans ), 0.05 ), 4.0 );
		float3 thickF = gThick * lerp( 1.0, r, gDomL.w );
		float3 V = normalize( -gView );
		float3 H = normalize( L + V );
		float3 spark = gDomE * ( 0.02 * ( 72.0 / 8.0 ) * pow( max( dot( N, H ), 0.0 ), 64.0 ) * max( dot( N, L ), 0.0 ) );
		fragColor = float4( max( alb * ( m * gThin + ( 1.0 - m ) * thickF ) + spark + gB, (float3)( 0.0 ) ), a );
		return;
	}
#endif
	fragColor = float4( max( alb * gA + gB, (float3)( 0.0 ) ), a );
}

struct SPIRV_Cross_Input
{
    float2 gUv : TEXCOORD0;
    float2 gQ : TEXCOORD1;
    float3 gView : TEXCOORD2;
    nointerpolation float4 gCol : TEXCOORD3;
    nointerpolation float4 gBox : TEXCOORD4;
    nointerpolation float3 gA : TEXCOORD5;
    nointerpolation float3 gB : TEXCOORD6;
#if defined(PC_LOOK)
    nointerpolation float4 gDrop : TEXCOORD7;
    nointerpolation float gOsc : TEXCOORD8;
    nointerpolation float3 gThin : TEXCOORD9;
    nointerpolation float3 gThick : TEXCOORD10;
    nointerpolation float4 gDomL : TEXCOORD11;
    nointerpolation float3 gDomE : TEXCOORD12;
    nointerpolation float2 gAx : TEXCOORD13;
#endif
};

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gUv = stage_input.gUv;
    gQ = stage_input.gQ;
    gView = stage_input.gView;
    gCol = stage_input.gCol;
    gBox = stage_input.gBox;
    gA = stage_input.gA;
    gB = stage_input.gB;
#if defined(PC_LOOK)
    gDrop = stage_input.gDrop;
    gOsc = stage_input.gOsc;
    gThin = stage_input.gThin;
    gThick = stage_input.gThick;
    gDomL = stage_input.gDomL;
    gDomE = stage_input.gDomE;
    gAx = stage_input.gAx;
#endif
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}

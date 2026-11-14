// HLSL source of features/post/taa_objvel.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "PostBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
/* lane TAAINPUTS1: the object vector, previous minus current in texture units (game_taa_mv.frag's convention),
 * and the own-motion flag in the mask's B. The alpha test and the grass fade drop what the frame dropped. */

Texture2D<float4> BaseMap : register(t0, space0); SamplerState _BaseMap_sampler : register(s0, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	float2 uvScale;
	float2 uvOffset;
	int alphaTest;
	float alpha;
	float alphaThreshold;
	float3 wwGrassFade;
	/* lane TREELEAF3 (src/gl/wwfxcov.h, its GLSL mirror): 0 = today's test, base.a x alpha; 4 = an effect shape's composed
	 * alpha as fo4_effectshader.frag tests it (alpha = baseColor.a^2), + 1 x the vertex alpha, + 2 the palette's alpha
	 * at (base.a, the rest); lane TREELEAF4 (FXCOVRESID1): + 8 x the falloff (the normal-map normal), + 16 x the cell's
	 * soft fade (the opaque pass's depth): fo4_effectshader.frag's own lines, in its order */
	int fxMode;
	float4 falloffParams;		// start angle, stop angle, start opacity, stop opacity (the colour pass's uniform)
	bool fxOrtho;			// the colour pass's projectionMatrix[3][3] == 1 (ViewDir = +z)
	int2 fxDepthOrigin;	// the colour pass's viewportDimensions.xy
	float2 fxProj;			// the colour pass's projectionMatrix[2][2], [3][2]
	float fxDistScale;		// view units -> game units (celllights.cpp)
	float fxSoftDepth;		// game units
	/* lane TREELEAF4 (TAABLENDMOVER1): a BLENDED mover draws twice. covMode 1: the object vector only where its composed
	 * alpha >= covThreshold; covMode 2: the rest of its triangles, into the mask's R (reactive) -- the offline gate
	 * (tests/treeleaf4/blendmover_gate.py, the real taa2.frag) measured the background behind a moving card 15-65 % closer
	 * to the reference than the whole-card vector, the card itself within 2 %. 0 = today's (every covered pixel). */
	int covMode;
	float covThreshold;
};

Texture2D<float4> GreyscaleMap : register(t1, space0); SamplerState _GreyscaleMap_sampler : register(s1, space0);
Texture2D<float4> NormalMap : register(t2, space0); SamplerState _NormalMap_sampler : register(s2, space0);
Texture2D<float4> fxDepth : register(t3, space0); SamplerState _fxDepth_sampler : register(s3, space0);		// Scene::fxDepthTexId: the opaque pass's depth, the viewport's corner at texel 0

static float4 cCur;
static float4 cPrev;
static float3 viewPos;
static float2 texCoord;
static float vAlpha;
static float3x3 btnMatrix;

static float2 vel;
static float4 flag;

// fo4_effectshader.frag's falloff (its normal, V and two lines, unchanged); the back-face flip leaves |N.V| as it is
float fxFalloff()
{
	float3 normal = normalize( NormalMap.Sample( _NormalMap_sampler, texCoord * uvScale + uvOffset ).rgb * 2.0 - 1.0 );
	normal.b = sqrt( max( 1.0 - dot( normal.rg, normal.rg ), 0.0 ) );
	float3x3 btn = float3x3( normalize( btnMatrix[0] ), normalize( btnMatrix[1] ), normalize( btnMatrix[2] ) );
	normal = normalize( mul(normal, btn) );
	float3 V = normalize( fxOrtho ? float3( 0.0, 0.0, 1.0 ) : -viewPos );
	float falloff = smoothstep( falloffParams.x, falloffParams.y, abs( dot( normal, V ) ) );
	return lerp( max( falloffParams.z, 0.0 ), min( falloffParams.w, 1.0 ), falloff );
}

// fo4_effectshader.frag cellFxFade(), line for line
float fxSoftFade()
{
	float d = fxDepth.Load( int3(int2( gl_FragCoord.xy ) - fxDepthOrigin, 0) ).r;
	float sceneW = fxProj.y / ( fxProj.x + ( 2.0 * d - 1.0 ) );
	float k = fxDistScale / fxSoftDepth;
	float fragW = 1.0 / gl_FragCoord.w;
	float soft = clamp( ( sceneW - fragW ) * k, 0.0, 1.0 );
	float t = clamp( ( fragW * k - 0.075 ) * 2.352941, 0.0, 1.0 );
	return soft * t * t * ( 3.0 - 2.0 * t );
}

void frag_main()
{
	if ( wwGrassFade.z > 0.5 && length( viewPos ) - wwGrassFade.x >= max( wwGrassFade.y, 1e-3 ) )
		discard;
	if ( or( alphaTest != 0, covMode != 0 ) ) {
		float a = BaseMap.Sample( _BaseMap_sampler, texCoord * uvScale + uvOffset ).a;
		if ( fxMode != 0 ) {
			float k = ( fxMode & 1 ) != 0 ? vAlpha : 1.0;
			float f = ( fxMode & 8 ) != 0 ? fxFalloff() : 1.0;
			float s = ( fxMode & 16 ) != 0 ? fxSoftFade() : 1.0;
			a = ( fxMode & 2 ) != 0 ? GreyscaleMap.SampleLevel( _GreyscaleMap_sampler, float2( a, ( ( 1.0 * f ) * k ) * alpha * s ), 0.0 ).a
				: ( ( a * f ) * k ) * alpha * s;
		} else {
			a *= alpha;
		}
		// the reactive draw covers the texels the colour pass drops too (the card shows nothing there either)
		if ( alphaTest != 0 && covMode != 2 && a < alphaThreshold )
			discard;
		if ( covMode == 1 && a < covThreshold )
			discard;
		if ( covMode == 2 && !( a < covThreshold ) )
			discard;
	}
	if ( or( cCur.w <= 0.0, cPrev.w <= 0.0 ) ) {
		vel = (float2)( 0.0 );	// behind the eye last frame: no usable history
	} else {
		vel = ( cPrev.xy / cPrev.w - cCur.xy / cCur.w ) * 0.5;
	}
	flag = float4( covMode == 2 ? 1.0 : 0.0, 0.0, 1.0, 0.0 );
}

struct SPIRV_Cross_Input
{
    float4 cCur : TEXCOORD0;
    float4 cPrev : TEXCOORD1;
    float3 viewPos : TEXCOORD2;
    float2 texCoord : TEXCOORD3;
    float vAlpha : TEXCOORD4;
    float3x3 btnMatrix : TEXCOORD5;
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float2 vel : SV_Target0;
    float4 flag : SV_Target1;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    cCur = stage_input.cCur;
    cPrev = stage_input.cPrev;
    viewPos = stage_input.viewPos;
    texCoord = stage_input.texCoord;
    vAlpha = stage_input.vAlpha;
    btnMatrix = stage_input.btnMatrix;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.vel = vel;
    stage_output.flag = flag;
    return stage_output;
}

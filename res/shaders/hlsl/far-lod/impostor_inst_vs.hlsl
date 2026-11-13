// HLSL source of features/far-lod/impostor_inst.vert (vertex shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "FarLodBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
// inlined (no readable .hlsli yet): uniforms.glsl
static float4 gl_Position;
static int gl_InstanceID;
/* The octahedral impostor card, vertex stage, INSTANCED (lane FARCARD1, 2026-10-22).
 *
 * impostor_oct.vert with its per-card uniforms read from one buffer instead: the shipped-game impostor draw
 * (one instanced draw per card set, per-instance data in a buffer). The CPU still picks the frames and builds
 * the billboard (src/features/far-lod/impostordraw.cpp, the same function the per-card path uses), so every number here is
 * the float the old uniform held; the expressions below are impostor_oct.vert's, character for character.
 *
 * cardData, 6 texels a card (cardBase + gl_InstanceID):
 *   0 centre.xyz, depthSpan      1 half.xy, swayPhase, scale      2 right.xyz, frameCount
 *   3 up.xyz, frame 0            4 fwd.xyz, frame 1               5 weight 0..2, frame 2
 * cardFrames, 4 texels a frame of the set (frameTableBase + frame * 4):
 *   0 rect (u0 v0 du dv)   1 right.xyz, offset right   2 up.xyz, offset up   3 fwd.xyz, 0
 *
 * WW_CARD_GPUPICK (lane FARCARDPICK1, the twins impostor_instgp.vert / impostor_instezgp.vert): the frames and the
 * billboard are picked HERE (impostor_pick.glsl, the CPU's arithmetic), and cardData is static:
 *   2 texels a placement slot: 0 centre.xyz, depthSpan   1 half.xy, sway offset, scale
 *   then this pass's visible slots, 4 a texel, from cardIndexBase: slot = list[cardBase + gl_InstanceID].
 */

// uniforms.glsl inlined here: its .hlsli has no readable half yet (lane HLSLSRC1)
cbuffer globalUniforms : register(b0, space0)
{
	row_major float3x3	viewMatrix : packoffset(c0);					// rotation part of view transform
	row_major float3x3	envMapRotation : packoffset(c3);				// view space to environment map
	row_major float4x4	projectionMatrix : packoffset(c6);
	float4	lightSourcePosition[3] : packoffset(c10);
	float4	lightSourceDiffuse[3] : packoffset(c13);
	float4	lightSourceAmbient : packoffset(c16);
	float	toneMapScale : packoffset(c17);				// 1.0 = full tone mapping
	float	brightnessScale : packoffset(c17.y);
	float	glowScale : packoffset(c17.z);
	float	glowScaleSRGB : packoffset(c17.w);
	int4	viewportDimensions : packoffset(c18);			// X, Y, width, height
	uint	doSkinning : packoffset(c19);
	int	sceneOptions : packoffset(c19.y);
	int	cubeBgndMipLevel : packoffset(c19.z);
	int	sfParallaxMaxSteps : packoffset(c19.w);
	float	sfParallaxScale : packoffset(c20);
	float	sfParallaxOffset : packoffset(c20.y);
	float	unusedUniform1 : packoffset(c20.z);
	float	unusedUniform2 : packoffset(c20.w);
};
// end of uniforms.glsl
#ifdef WW_CARD_GPUPICK
#include "impostor_pick.hlsli"
#endif

cbuffer LooseUniforms : register(b31, space0)
{
	row_major float4x4 modelViewMatrix;
	int cardBase;          // this draw's first card in cardData
	int frameTableBase;    // this set's first texel in cardFrames
	float3 cardEyeModel;      // the camera's position in model space
	bool cardOrtho;         // true: no eye point, the ray is cardFwd
#if defined(WW_CARD_EARLYZ)
	bool useDepthOffset;    // lane PERFFIX4: the fragment's pixel depth offset (impostor_oct.frag), read here too
#endif
#if defined(WW_CARD_GPUPICK)
	int cardIndexBase;     // the first texel of this pass's slot list
	float cardSwayBase;      // Options::swayPhase; a slot adds its own offset
	int pickN;             // the set's grid side (a grid set)
	int pickV;             // the set's views (a ring set)
	bool pickRing;
	bool pickAsBaked;       // the set's convention: the legacy 180
	bool pickShuffle;       // the red control: the quarter turn
	float pickSharpen;
	int pickFrameCount;    // after the ring's cap of 2
#endif
};


Buffer<float4> cardData : register(t0, space0);
Buffer<float4> cardFrames : register(t1, space0);
#ifdef WW_CARD_EARLYZ

#endif
#ifdef WW_CARD_GPUPICK

#endif

static float3 vertexPosition;   // the unit quad, -1..1 in xy
static float2 multiTexCoord0;   // 0..1 across the quad

static float3 cardPosModel;
static float2 cardQuadUv;
static float3 cardViewRay;

// what impostor_oct.frag reads as uniforms on the per-card path
static float4 frameRect[3];
static float3 frameWeightV;
static float3 frameRight[3];
static float3 frameUp[3];
static float3 frameFwd[3];
static int frameCount;
static float2 frameOffset[3];
static float3 cardCenter;
static float2 cardHalf;
static float cardDepthSpan;
static float swayPhase;

void vert_main()
{
#ifdef WW_CARD_GPUPICK
	int ci = cardBase + gl_InstanceID;
	int slot = int( cardData.Load( cardIndexBase + ( ci >> 2 ) )[ci & 3] );
	float4 t0 = cardData.Load( slot * 2 );
	float4 t1 = cardData.Load( slot * 2 + 1 );
	int3 pIdx;
	float3 pW, inRight, inUp, inFwd;
	wwCardPick( cardEyeModel, t0.xyz, pickN, pickV, pickRing, pickAsBaked, pickShuffle, pickSharpen, pickFrameCount,
				pIdx, pW, inRight, inUp, inFwd );

	float3 inCenter = t0.xyz;
	float2 inHalf = t1.xy;
	float scale = t1.w;

	cardCenter = inCenter;
	cardHalf = inHalf;
	cardDepthSpan = t0.w;
	swayPhase = cardSwayBase + t1.z;
	frameCount = pickFrameCount;
	frameWeightV = pW;
	float frames[3] = { float( pIdx.x ), float( pIdx.y ), float( pIdx.z ) };
#else
	int base = ( cardBase + gl_InstanceID ) * 6;
	float4 t0 = cardData.Load( base );
	float4 t1 = cardData.Load( base + 1 );
	float4 t2 = cardData.Load( base + 2 );
	float4 t3 = cardData.Load( base + 3 );
	float4 t4 = cardData.Load( base + 4 );
	float4 t5 = cardData.Load( base + 5 );

	float3 inCenter = t0.xyz;
	float2 inHalf = t1.xy;
	float3 inRight = t2.xyz;
	float3 inUp = t3.xyz;
	float3 inFwd = t4.xyz;
	float scale = t1.w;

	cardCenter = inCenter;
	cardHalf = inHalf;
	cardDepthSpan = t0.w;
	swayPhase = t1.z;
	frameCount = int( t2.w );
	frameWeightV = t5.xyz;
	float frames[3] = { t3.w, t4.w, t5.w };
#endif
	for ( int k = 0; k < 3; k++ ) {
		int f = frameTableBase + int( frames[k] ) * 4;
		float4 f1 = cardFrames.Load( f + 1 );
		float4 f2 = cardFrames.Load( f + 2 );
		frameRect[k] = cardFrames.Load( f );
		frameRight[k] = f1.xyz;
		frameUp[k] = f2.xyz;
		frameFwd[k] = cardFrames.Load( f + 3 ).xyz;
		// the per-card path multiplied on the CPU: one float multiply either way
		frameOffset[k] = float2( f1.w * scale, f2.w * scale );
	}

	// impostor_oct.vert from here, with its uniforms as the locals above
	float3 p = inCenter
		+ inRight * ( vertexPosition.x * inHalf.x )
		+ inUp    * ( vertexPosition.y * inHalf.y );

	cardPosModel = p;
	cardQuadUv   = multiTexCoord0;

	bool orthoCam = or( cardOrtho, abs( projectionMatrix[2][3] ) < 1.0e-6 );
	cardViewRay = orthoCam ? -inFwd : normalize( p - cardEyeModel );

	gl_Position = mul(( mul(float4( p, 1.0 ), modelViewMatrix) ), projectionMatrix);
#ifdef WW_CARD_EARLYZ
	/* lane PERFFIX4: the fragment writes cardPosModel + ray * (height - 0.5) * span, up to half the span TOWARD the
	 * eye. Pull the vertex that far along its own eye ray (+5 % for the angle between a vertex ray and a pixel ray,
	 * + 4 units for rounding): the same screen position, and every depth the fragment writes lies behind the quad,
	 * so impostor_cellinstez.frag may declare depth_greater and the hardware tests depth before the march. */
	if ( !orthoCam && useDepthOffset ) {
		float4 e = mul(float4( p, 1.0 ), modelViewMatrix);
		float dist = length( e.xyz );
		float s = length( (float3)( modelViewMatrix[0] ) );
		float pull = min( s * cardDepthSpan * 0.5 * 1.05 + 4.0, dist * 0.5 );
		gl_Position = mul(float4( e.xyz * ( 1.0 - pull / dist ), 1.0 ), projectionMatrix);
	}
#endif
}

struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
    float2 multiTexCoord0 : TEXCOORD7;
    uint gl_InstanceID : SV_InstanceID;
};

struct SPIRV_Cross_Output
{
    WW_LOC(18) float3 cardPosModel : TEXCOORD18;
    WW_LOC(19) float2 cardQuadUv : TEXCOORD19;
    WW_LOC(20) float3 cardViewRay : TEXCOORD20;
    WW_LOC(6) nointerpolation float4 frameRect[3] : TEXCOORD6;
    WW_LOC(22) nointerpolation float3 frameWeightV : TEXCOORD22;
    WW_LOC(9) nointerpolation float3 frameRight[3] : TEXCOORD9;
    WW_LOC(12) nointerpolation float3 frameUp[3] : TEXCOORD12;
    WW_LOC(0) nointerpolation float3 frameFwd[3] : TEXCOORD0;
    WW_LOC(21) nointerpolation int frameCount : TEXCOORD21;
    WW_LOC(3) nointerpolation float2 frameOffset[3] : TEXCOORD3;
    WW_LOC(15) nointerpolation float3 cardCenter : TEXCOORD15;
    WW_LOC(17) nointerpolation float2 cardHalf : TEXCOORD17;
    WW_LOC(16) nointerpolation float cardDepthSpan : TEXCOORD16;
    WW_LOC(23) nointerpolation float swayPhase : TEXCOORD23;
    float4 gl_Position : SV_Position;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    multiTexCoord0 = stage_input.multiTexCoord0;
    gl_InstanceID = int(stage_input.gl_InstanceID);
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.cardPosModel = cardPosModel;
    stage_output.cardQuadUv = cardQuadUv;
    stage_output.cardViewRay = cardViewRay;
    stage_output.frameRect = frameRect;
    stage_output.frameWeightV = frameWeightV;
    stage_output.frameRight = frameRight;
    stage_output.frameUp = frameUp;
    stage_output.frameFwd = frameFwd;
    stage_output.frameCount = frameCount;
    stage_output.frameOffset = frameOffset;
    stage_output.cardCenter = cardCenter;
    stage_output.cardHalf = cardHalf;
    stage_output.cardDepthSpan = cardDepthSpan;
    stage_output.swayPhase = swayPhase;
    return stage_output;
}

// HLSL source of fo4_default.vert (vertex shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
static float4 gl_Position;
static int gl_VertexID;
static int gl_BaseInstanceARB;	// the draw's base instance: stage input BaseInstance
static float3 LightDir;
static float3 ViewDir;

static float2 texCoord;

static float3x3 btnMatrix;

static float4 A;
static float4 C;
static float4 D;
// the vertex colour's OWN alpha, before vertexColorOverride forces it to 1 on a tree-
// animation shape: the wind weight W the impostor bake reads (channel 11 G, CARDFIX1 step 6)
static float rawVertexAlpha;

static float3x3 reflMatrix;

// RESUME 15: the depth pre-pass and the main pass draw a shape through different programs (and passes) over one depth
// at EQUAL: the same inputs must give the same bits whichever program computes them
// invariant gl_Position; (GLSL only: HLSL has no invariant qualifier)

#include "uniforms.hlsli"

#define WW_LOOSE_TYPES 1
#if defined(WW_BINDLESS)
#include "../near/ww_bindless.hlsli"
#endif
#include "ww_treewind.hlsli"
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#if defined(WW_BINDLESS)
#include "../near/ww_bindless.hlsli"
#endif
#include "ww_treewind.hlsli"
#undef WW_LOOSE_UNIFORMS
#if !(defined(WW_BINDLESS))
	row_major float3x3 normalMatrix;			// in row-major order
	row_major float4x4 modelViewMatrix;
#endif
	float4 vertexColorOverride;	// components greater than zero replace the vertex color
	// lane FARLOD1: the far field's wind. 0 (the default, and whenever the Far LOD row is off) = no sway;
	// a tree-animation shape's vertex alpha is the weight, as in the game
	float farSwayAmp;
	float farSwayTime;
	// the loaded block in view space: xyz its centre, w its half edge -- the cell's own trees stand inside it, unswayed
	float4 farSwayBlock;
	bool lodTreeAnim;
	/* lane GRASSMB1: the game's grass wind, op for op from its grass vertex shader (Shaders011 entry 02183, the deferred
	 * prepass's grass technique; src/gl/cellgrass.h). wwGrassWind = cb2[11] = (angle, 0, previous phase, phase),
	 * wwGrassWind2 = cb2[12] = (min speed * 300, max speed * 300, frequency) with .w = 1 on a grass shape, 0 on
	 * every other (where nothing below runs). The blade's (h0 + h1) * 0.0078125 rides in the bitangent's length
	 * (1024 + it). The game's world position  is the bucket's model position plus its translation, so the offset
	 * adds here in model space. */
	float4 wwGrassWind;
	float4 wwGrassWind2;
	// lane FARDRAW1: an instanced far ring (gl/cellfarinst.h): rows 0-2 of the placement's (scale*R | t), then
	// (u mirror sum, r, g, b + 2 when mirrored). false (the default) = every other shape, untouched
	bool farInst;
	float lodViewCode;	// lane LODLIVE1: the LOD coloration's band + 1 of a shape (0 = untinted), renderer.cpp
	/* lane FIX1101A lodch: the ring's per-vertex bake channels (gl/cellfarinst.cpp; src/cellmesh.h CellMeshInst::vstream).
	 * On, the row's .y holds the int bits of its first texel minus the sub's base vertex: + gl_VertexID = this vertex's
	 * RG8 texel, R the AO (into the colour, as the flat copy has it), G the ambient's sky factor (wwFarSkyVis). The
	 * sampler's unit is set at link (gl/glcontext.cpp). Off (every other draw): wwFarSkyVis 1, nothing read. */
	bool farStreamOn;
	float3 farBakeK;
	/* MODULES1 10b-2: the far draws whose baked AO is a grey in the vertex colour. x the AO's maximum strength, y the sky
	 * occlusion's, z the draw: 0 none, 1 the flat far copy (the grey is the AO: wwFarAo), 2 the far ground (the grey is
	 * its mask sheet's B, a sky AO: wwFarSkyVis), 3 instanced rows without the stream (mesh grey x row grey is the AO).
	 * w 1 = red WW_LODL_AO_RED=albedo (the grey stays in the colour). The colour is then white. */
	float4 farVcAo;
#if defined(WW_FAR_SPLAT)
	/* lane FARLOOK1101 (FARSHIMMER2): a far row whose projected box is under S px on an axis draws widened to S about its
	 * projected centre at alpha 1 / (kx * ky) (exact ink: the area grows by kx * ky). farSplat = (S px, pass: 1 opaque =
	 * the rows at or over S, 2 the blended splat pass = the rows under it); the box = the row's mesh box (model space) */
	float2 farSplat;
#endif
};
#ifdef WW_BINDLESS
// lane BINDLESS2: the draw's matrices from its row of the per-draw buffer (the same values the uniforms carry)
#include "../near/ww_bindless.hlsli"
#define modelViewMatrix ( wwDraws[wwDrawIdx].mv )
#define normalMatrix float3x3( wwDraws[wwDrawIdx].nm[0].xyz, wwDraws[wwDrawIdx].nm[1].xyz, wwDraws[wwDrawIdx].nm[2].xyz )
#else

#endif



static float3 vertexPosition;
static float4 vertexColor;
static float3 normalVector;
static float3 tangentVector;
static float3 bitangentVector;
static float2 multiTexCoord0;
static float4 farInstRow0;
static float4 farInstRow1;
static float4 farInstRow2;
static float4 farInstExtra;
// lane PERFLOAD1 P7 round 2: the far row's tree-ladder dither (x; 0 = whole), unset (0) on every other draw
static float2 farLadderIn;
static float2 farLadder;
Buffer<float4> farStream : register(t50, space0);
static float wwFarSkyVis;
/* MODULES1 round 10 (bungo 11-11: "Far baked AO needs to behave through regular AO rules"): the stream's AO leaves the
 * vertex colour and goes to the fragment's one AO rule (cell_ao.glsl cellAoShared) as wwFarAo. farBakeK: x the AO's
 * and y the sky occlusion's maximum (live keys WW_LODL_AO_MAX / WW_LODL_SKY_MAX, 0..1), z 1 = the red
 * WW_LODL_AO_RED=albedo (the AO multiplied into the vertex colour, as before). Set with farStreamOn. */
static float wwFarAo;
#ifdef WW_FAR_SPLAT

static float4 farInstBoxC;
static float4 farInstBoxE;
static float farSplatA;
#endif

#include "bonetransform.hlsli"
#include "ww_treewind.hlsli"

void vert_main()
{
	float4	v = float4( vertexPosition, 1.0 );
	float3	n = normalVector;
	float3	t = tangentVector;
	float3	b = bitangentVector;

	if ( boneWeights[0].x > 0.0 && doSkinning )
		boneTransform( v, n, t, b );

	float4 vc = vertexColor;
	float2 uv0 = multiTexCoord0;
	float2 farXY = vertexPosition.xy;
	wwFarSkyVis = 1.0;
	wwFarAo = 1.0;
	if ( farInst ) {
		float3x3 Mt = float3x3( farInstRow0.xyz, farInstRow1.xyz, farInstRow2.xyz );	// n * Mt = M n
		v.xyz = float3( dot( farInstRow0, v ), dot( farInstRow1, v ), dot( farInstRow2, v ) );
		n = mul(Mt, n);
		t = mul(Mt, t);
		b = mul(Mt, b);
		farXY = v.xy;	// the sway phase by the placed position, as the flat copy had it
		float e = farInstExtra.w;
		if ( e > 1.5 ) {
			uv0.x = farInstExtra.x - uv0.x;
			e -= 2.0;
		}
		if ( farStreamOn ) {
			float2 ch = farStream.Load( asint( farInstExtra.y ) + gl_VertexID ).rg;
			float ao = 1.0 - ( 1.0 - ch.r ) * farBakeK.x;
			wwFarAo = farBakeK.z > 0.5 ? 1.0 : ao;
			vc.rgb *= ( farBakeK.z > 0.5 ? ao : 1.0 ) * e;
			wwFarSkyVis = 1.0 - ( 1.0 - ch.g ) * farBakeK.y;
		} else if ( farVcAo.z > 2.5 && farVcAo.w < 0.5 ) {
			wwFarAo = 1.0 - ( 1.0 - clamp( vc.r * farInstExtra.y, 0.0, 1.0 ) ) * farVcAo.x;
			vc.rgb = (float3)( 1.0 );
		} else
		vc.rgb *= float3( farInstExtra.yz, e );
	} else if ( farVcAo.z > 0.5 && farVcAo.w < 0.5 ) {
		if ( farVcAo.z < 1.5 )
			wwFarAo = 1.0 - ( 1.0 - vc.r ) * farVcAo.x;
		else
			wwFarSkyVis = 1.0 - ( 1.0 - vc.r ) * farVcAo.y;
		vc.rgb = (float3)( 1.0 );
	}
	if ( wwGrassWind2.w > 0.5 ) {
		float L = length( b );
		float off = L > 2.0 ? L - 1024.0 : 0.0;
		b = b * rsqrt( dot( b, b ) );				// dp3, rsq, mul
		n = n * rsqrt( dot( n, n ) );
		t = t * rsqrt( dot( t, t ) );
		float p = ( wwGrassWind.w - off ) * wwGrassWind2.z;	// mad -(v5.x + v5.y), 0.0078125, cb2[11].w; mul cb2[12].z
		float half_range = ( wwGrassWind2.y - wwGrassWind2.x ) * 0.5;
		float sp = sin( p ), cp = cos( p );				// sincos
		float w = sin( sp * 3.14159274 ) + sin( sp * 6.28318548 );
		w = w * 0.3 + cos( cp * 3.14159274 ) * 0.2;
		w = w + 1.0;
		w = w * half_range + wwGrassWind2.x;
		w = w * ( vertexColor.a * vertexColor.a * 0.5 );
		float3 d = float3( cos( wwGrassWind.x ), sin( wwGrassWind.x ), 0.0 );	// sincos cb2[11].x
		if ( wwGrassWind2.w < 1.5 ) {	// MODULES1 M4: a swaying cable (w 2) keeps its radial frame
			n = n + d * w;
			n = n * rsqrt( dot( n, n ) );
			t = t + d * w;
			t = t * rsqrt( dot( t, t ) );
			b = b + d * w;
			b = b * rsqrt( dot( b, b ) );
		}
		v.xyz = d * w + v.xyz;
		vc.rgb = exp2( log2( vertexColor.rgb ) * 2.2 );		// log, mul 2.2, exp: o6.rgb = (v4.rgb * v5.w)^2.2
	}
	if ( wwTreeWind2.w > 0.5 )	// lane WINDTREE1: a loaded cell's Tree_Anim shape in the weather's wind
		v.xyz = wwTreeWindApply( v.xyz, normalVector, length( bitangentVector ), modelViewMatrix );

	v = mul(v, modelViewMatrix);
	float3 farRel = mul(( v.xyz - farSwayBlock.xyz ), transpose( viewMatrix ));	// world-axis offset from the block's centre
	if ( farSwayAmp > 0.0 && lodTreeAnim && max( abs( farRel.x ), abs( farRel.y ) ) > farSwayBlock.w ) {
		// world-horizontal offset, phase by the vertex's own place so a forest does not move as one
		float ph = dot( farXY, float2( 0.0021, 0.0017 ) );
		float3 w = float3( sin( farSwayTime * 1.3 + ph ), cos( farSwayTime * 0.9 + ph * 1.7 ) * 0.6, 0.0 );
		v.xyz += mul(( w * ( farSwayAmp * vertexColor.a ) ), viewMatrix);
	}
	gl_Position = mul(v, projectionMatrix);
	farLadder = farInst ? float2( farLadderIn.x, farLadderIn.y > 0.5 ? farLadderIn.y : lodViewCode ) : float2( 0.0, lodViewCode );
#ifdef WW_FAR_SPLAT
	farSplatA = 1.0;
	if ( farInst && farSplat.x > 0.0 ) {
		float4x4 PMV = mul(modelViewMatrix, projectionMatrix);
		float2 lo = (float2)( 1.0e30 ), hi = (float2)( -1.0e30 );
		bool front = dot( farInstBoxE.xyz, (float3)( 1.0 ) ) > 0.0;
		for ( int k = 0; k < 8; k++ ) {
			float3 s = float3( ( k & 4 ) != 0 ? 1.0 : -1.0, ( k & 2 ) != 0 ? 1.0 : -1.0, ( k & 1 ) != 0 ? 1.0 : -1.0 );
			float4 m = float4( farInstBoxC.xyz + s * farInstBoxE.xyz, 1.0 );
			float4 p = mul(float4( dot( farInstRow0, m ), dot( farInstRow1, m ), dot( farInstRow2, m ), 1.0 ), PMV);
			front = front && p.w > 1.0e-3;
			float2 q = p.xy / max( p.w, 1.0e-3 );
			lo = min( lo, q );
			hi = max( hi, q );
		}
		float2 px = ( hi - lo ) * 0.5 * float2( viewportDimensions.zw );
		float2 kk = front ? max( (float2)( 1.0 ), (float2)( farSplat.x ) / max( px, (float2)( 1.0e-4 ) ) ) : (float2)( 1.0 );
		bool splat = kk.x * kk.y > 1.0001;
		if ( splat != ( farSplat.y > 1.5 ) ) {
			gl_Position = float4( 0.0, 0.0, 2.0, 1.0 );	// the other pass draws this row: outside the clip volume
		} else if ( splat ) {
			float2 c = ( lo + hi ) * 0.5;
			gl_Position.xy = ( c + ( gl_Position.xy / gl_Position.w - c ) * kk ) * gl_Position.w;
			farSplatA = 1.0 / ( kk.x * kk.y );
		}
	}
#endif
	texCoord = uv0;

	btnMatrix[2] = normalize( mul(normalMatrix, n) );
	btnMatrix[1] = normalize( mul(normalMatrix, t) );
	btnMatrix[0] = normalize( mul(normalMatrix, b) );

	reflMatrix = envMapRotation;
	reflMatrix[0][2] *= -1.0;
	reflMatrix[1][2] *= -1.0;
	reflMatrix[2][2] *= -1.0;

	if ( projectionMatrix[3][3] == 1.0 )
		ViewDir = float3(0.0, 0.0, 1.0);	// orthographic view
	else
		ViewDir = -v.xyz;
	LightDir = lightSourcePosition[0].xyz;

	A = float4( sqrt(lightSourceAmbient.rgb) * 0.375, toneMapScale );
	C = select((vertexColorOverride > (float4)( 0.0 )), vertexColorOverride, vc);
	rawVertexAlpha = vertexColor.a;
	D = float4( sqrt(lightSourceDiffuse[0].rgb), brightnessScale );
}

struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
    float4 vertexColor : TEXCOORD1;
    float3 normalVector : TEXCOORD2;
    float3 tangentVector : TEXCOORD3;
    float3 bitangentVector : TEXCOORD4;
    float4 boneWeights[2] : TEXCOORD5;
    float2 multiTexCoord0 : TEXCOORD7;
    float2 farLadderIn : TEXCOORD9;
    float4 farInstRow0 : TEXCOORD10;
    float4 farInstRow1 : TEXCOORD11;
    float4 farInstRow2 : TEXCOORD12;
    float4 farInstExtra : TEXCOORD13;
    uint gl_VertexID : SV_VertexID;
#if defined(WW_FAR_SPLAT)
    float4 farInstBoxC : TEXCOORD14;
    float4 farInstBoxE : TEXCOORD15;
#endif
#if defined(WW_BINDLESS) && defined(WW_BL_VERT) && defined(GL_ARB_shader_draw_parameters)
#ifdef __spirv__
    [[vk::builtin("BaseInstance")]] uint gl_BaseInstanceARB : BASEINSTANCE;
#else
    uint gl_BaseInstanceARB : SV_StartInstanceLocation;
#endif
#endif
};

struct SPIRV_Cross_Output
{
    WW_LOC(3) float3 LightDir : TEXCOORD3;
    WW_LOC(4) float3 ViewDir : TEXCOORD4;
    WW_LOC(8) float2 texCoord : TEXCOORD8;
    WW_LOC(5) float3x3 btnMatrix : TEXCOORD5;
    WW_LOC(0) nointerpolation float4 A : TEXCOORD0;
    WW_LOC(1) float4 C : TEXCOORD1;
    WW_LOC(2) nointerpolation float4 D : TEXCOORD2;
    WW_LOC(12) float rawVertexAlpha : TEXCOORD12;
    WW_LOC(9) nointerpolation float3x3 reflMatrix : TEXCOORD9;
    WW_LOC(13) nointerpolation float2 farLadder : TEXCOORD13;
    WW_LOC(15) float wwFarSkyVis : TEXCOORD15;
    WW_LOC(14) float wwFarAo : TEXCOORD14;
#if defined(WW_FAR_SPLAT)
    WW_LOC(16) nointerpolation float farSplatA : TEXCOORD16;
#endif
#if defined(WW_BINDLESS) && defined(WW_BL_VERT)
    WW_LOC(16) nointerpolation uint wwBlRow : TEXCOORD16;
#endif
    precise float4 gl_Position : SV_Position;
};

#ifdef vert_main	// a wrapper renamed this file's vert_main and defines its own after the include
#undef vert_main
void vert_main();
#endif

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vertexColor = stage_input.vertexColor;
    normalVector = stage_input.normalVector;
    tangentVector = stage_input.tangentVector;
    bitangentVector = stage_input.bitangentVector;
    multiTexCoord0 = stage_input.multiTexCoord0;
    farInstRow0 = stage_input.farInstRow0;
    farInstRow1 = stage_input.farInstRow1;
    farInstRow2 = stage_input.farInstRow2;
    farInstExtra = stage_input.farInstExtra;
    farLadderIn = stage_input.farLadderIn;
#if defined(WW_FAR_SPLAT)
    farInstBoxC = stage_input.farInstBoxC;
    farInstBoxE = stage_input.farInstBoxE;
#endif
    boneWeights = stage_input.boneWeights;
    gl_VertexID = int(stage_input.gl_VertexID);
#if defined(WW_BINDLESS) && defined(WW_BL_VERT) && defined(GL_ARB_shader_draw_parameters)
    gl_BaseInstanceARB = int(stage_input.gl_BaseInstanceARB);
#endif
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.LightDir = LightDir;
    stage_output.ViewDir = ViewDir;
    stage_output.texCoord = texCoord;
    stage_output.btnMatrix = btnMatrix;
    stage_output.A = A;
    stage_output.C = C;
    stage_output.D = D;
    stage_output.rawVertexAlpha = rawVertexAlpha;
    stage_output.reflMatrix = reflMatrix;
    stage_output.farLadder = farLadder;
    stage_output.wwFarSkyVis = wwFarSkyVis;
    stage_output.wwFarAo = wwFarAo;
#if defined(WW_FAR_SPLAT)
    stage_output.farSplatA = farSplatA;
#endif
#if defined(WW_BINDLESS) && defined(WW_BL_VERT)
    stage_output.wwBlRow = wwBlRow;
#endif
    return stage_output;
}

// HLSL source of features/far-lod/impostor_oct.vert (vertex shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "FarLodBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
// inlined (no readable .hlsli yet): uniforms.glsl
static float4 gl_Position;
/* The octahedral impostor card, vertex stage. Lane IMPOSTORSHOW, 2026-09-19.
 *
 * The quad is ONE camera-facing rectangle per card, `card.half` x 2 in size
 * and centred on `card.center` in the base's model space -- spec 239: "the
 * recorded halfW/halfH span the full frame, gutter included -- the quad is the
 * frame", and spec 259: "halfW/halfH then carry the frame's aspect exactly --
 * a card quad is its frame, undistorted". So the quad is not fitted to the
 * silhouette and is not squared off: it IS the frame's rectangle.
 *
 * Everything else -- which frames, what weights, where the surface really is
 * -- is per PIXEL and lives in the fragment stage, because the three frames
 * disagree about where the surface is and that disagreement is what the height
 * channel exists to settle. */

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

cbuffer LooseUniforms : register(b31, space0)
{
	row_major float4x4 modelViewMatrix;
	// The card, from its .lodm (docs/LODGEN_LODM_FORMAT.md `card` block).
	float3 cardCenter;      // model space
	float2 cardHalf;        // halfW, halfH -- the frame's own extents
	float3 cardRight;       // the billboard's axes in model space, built
	float3 cardUp;          // CPU-side from the camera direction so every
	float3 cardFwd;         // fragment of one card agrees on them
	float3 cardEyeModel;    // the camera's position in model space
	bool cardOrtho;       // true: no eye point, the ray is cardFwd
};


static float3 vertexPosition;   // the unit quad, -1..1 in xy
static float2 multiTexCoord0;   // 0..1 across the quad

static float3 cardPosModel;   // this fragment's point on the card plane, model space
static float2 cardQuadUv;     // 0..1 across the quad, for the debug channels
static float3 cardViewRay;    // model-space direction from the camera to this point

void vert_main()
{
	/* The quad's own plane. `vertexPosition.xy` is the unit square, so the
	 * card is exactly 2*halfW by 2*halfH and undistorted. */
	float3 p = cardCenter
		+ cardRight * ( vertexPosition.x * cardHalf.x )
		+ cardUp    * ( vertexPosition.y * cardHalf.y );

	cardPosModel = p;
	cardQuadUv   = multiTexCoord0;

	/* The view ray, in the same model space the card lives in, pointing FROM
	 * the camera TOWARDS this point. `cardFwd` points at the camera (it is the
	 * direction the frames were chosen for), so under an orthographic
	 * projection -- which has no eye point at all, and which is the projection
	 * the BAKE used -- every ray is its negative. Under a perspective camera
	 * the rays fan out across the card's own width, and a card wide enough to
	 * matter (a 25 m tree at the loaded-cell edge, spec 219) shows it: the
	 * parallax below has to follow the real ray or the near edge of the card
	 * parallaxes the wrong way. */
	/* ...and the projection says which of the two it is. `cardOrtho` arrives
	 * false from every caller (src/features/far-lod/impostordraw.cpp), which was fine while
	 * nothing drew a card under an orthographic camera and wrong the moment
	 * something did: an orthographic projection matrix has a w-row of
	 * (0,0,0,1) and a perspective one has -1 in it, and this shader already
	 * holds the matrix for `gl_Position`. At a bake direction under an
	 * orthographic camera the parallax step below is a mathematical no-op --
	 * it moves the sample along `ray`, and `frameUvOf` projects onto axes
	 * perpendicular to `frameFwd` -- so with the fan the card stopped being a
	 * photograph of the mesh at the one set of directions where it must be
	 * one. blast_n4 at its own sixteen bake directions, repaired sheets:
	 * parallax off 0.8823, parallax on with the fan 0.7545. */
	bool orthoCam = or( cardOrtho, abs( projectionMatrix[2][3] ) < 1.0e-6 );
	cardViewRay = orthoCam ? -cardFwd : normalize( p - cardEyeModel );

	gl_Position = mul(( mul(float4( p, 1.0 ), modelViewMatrix) ), projectionMatrix);
}

struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
    float2 multiTexCoord0 : TEXCOORD7;
};

struct SPIRV_Cross_Output
{
    float3 cardPosModel : TEXCOORD0;
    float2 cardQuadUv : TEXCOORD1;
    float3 cardViewRay : TEXCOORD2;
    float4 gl_Position : SV_Position;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    multiTexCoord0 = stage_input.multiTexCoord0;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.cardPosModel = cardPosModel;
    stage_output.cardQuadUv = cardQuadUv;
    stage_output.cardViewRay = cardViewRay;
    return stage_output;
}

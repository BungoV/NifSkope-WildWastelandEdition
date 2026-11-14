// HLSL source of features/particles/particles.geom (geometry shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: gs_6_0
#include "ParticlesBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
// inlined (no readable .hlsli yet): uniforms.glsl
struct WW_GL_PerVertex { float4 gl_Position; };
static WW_GL_PerVertex gl_in[1];
static int gl_PrimitiveIDIn;
static float4 gl_Position;
// GLSL: layout ( points ) in; (main's signature)
// GLSL: layout ( triangle_strip , max_vertices = 4 ) out; (main's signature)

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
	float2 particleScale;
	// sub-texture cell for atlas sheets: xy = uv offset, zw = uv scale (default 0,0,1,1)
	float4 particleUV;
	// lane VOLPART1, gates only: > 0 = the probe row's width in pixels; each particle then draws its own 8 pixels
	// (8 x its index .. + 8, one row) at its centre instead of its sprite (src/gl/glparticles.cpp, WW_CELL_VOLPART_DUMP)
	int volPartDump;
};


static float3 vsLightDir[1];
static float3 vsViewDir[1];

static float4 vsColor[1];
static float vsParticleSize[1];
static float2 vsUVOffset[1];
static float vsAngle[1];

static float3 LightDir;
static float3 ViewDir;

static float2 texCoords[9];

static float4 A;
static float4 C;
static float4 D;

static float3 N;

// lane VOLPART1 (particles_cell.frag reads them; the viewer's particles.frag does not): the sprite's own point
// (view space), its own coordinates (-1..1 before its rotation), and cos, sin of its rotation, its half-size
// (view units), the particle's index
static float3 pPos;
static float2 pQ;
static float4 pSprite;


struct SPIRV_Cross_Output
{
    WW_LOC(3) float3 LightDir : TEXCOORD3;
    WW_LOC(4) float3 ViewDir : TEXCOORD4;
    WW_LOC(6) float2 texCoords[9] : TEXCOORD6;
    WW_LOC(0) nointerpolation float4 A : TEXCOORD0;
    WW_LOC(1) float4 C : TEXCOORD1;
    WW_LOC(2) nointerpolation float4 D : TEXCOORD2;
    WW_LOC(5) float3 N : TEXCOORD5;
    WW_LOC(15) float3 pPos : TEXCOORD15;
    WW_LOC(16) float2 pQ : TEXCOORD16;
    WW_LOC(17) nointerpolation float4 pSprite : TEXCOORD17;
    float4 gl_Position : SV_Position;
};

#define WW_GS_STREAM TriangleStream<SPIRV_Cross_Output>
void EmitVertex(inout WW_GS_STREAM geometry_stream)
{
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.LightDir = LightDir;
    stage_output.ViewDir = ViewDir;
    stage_output.texCoords = texCoords;
    stage_output.A = A;
    stage_output.C = C;
    stage_output.D = D;
    stage_output.N = N;
    stage_output.pPos = pPos;
    stage_output.pQ = pQ;
    stage_output.pSprite = pSprite;
    geometry_stream.Append(stage_output);
}

void EndPrimitive(inout WW_GS_STREAM geometry_stream)
{
    geometry_stream.RestartStrip();
}

void geom_main(inout WW_GS_STREAM geometry_stream)
{
	LightDir = vsLightDir[0];
	ViewDir = vsViewDir[0];

	A = float4( (float3)(1.0), toneMapScale );
	C = vsColor[0];
	D = float4( (float3)(0.0), sqrt(brightnessScale) );

	N = float3( 0.0, 0.0, 1.0 );

	float	sx = vsParticleSize[0] * particleScale.x;
	float	sy = vsParticleSize[0] * particleScale.y;

	// per-particle sprite rotation about the view axis (NiPSysRotationModifier)
	float	ca = cos( vsAngle[0] );
	float	sa = sin( vsAngle[0] );
	pSprite = float4( ca, sa, sx, float( gl_PrimitiveIDIn ) );
	// rotated corner offsets: (u,v) in [-1,1] scaled by the sprite half-size
	float4	c11 = float4(  ( ca - sa ) * sx, (  sa + ca ) * sy, 0.0, 0.0 );
	float4	c01 = float4(  ( -ca - sa ) * sx, ( -sa + ca ) * sy, 0.0, 0.0 );
	float4	c10 = float4(  ( ca + sa ) * sx, (  sa - ca ) * sy, 0.0, 0.0 );
	float4	c00 = float4(  ( -ca + sa ) * sx, ( -sa - ca ) * sy, 0.0, 0.0 );

	float2	uv;
	float2	uvBase = particleUV.xy + vsUVOffset[0];

	if ( volPartDump > 0 ) {
		// the probe: the centre's texel and point on all four corners, the quad over this particle's 8 pixels
		uv = uvBase + float2( 0.5, 0.5 ) * particleUV.zw;
		float x0 = float( gl_PrimitiveIDIn * 8 ) / float( volPartDump ) * 2.0 - 1.0;
		float x1 = float( gl_PrimitiveIDIn * 8 + 8 ) / float( volPartDump ) * 2.0 - 1.0;
		for ( int k = 0; k < 4; k++ ) {
			for ( int i = 0; i < 9; i++ )
				texCoords[i] = uv;
			pPos = gl_in[0].gl_Position.xyz;
			pQ = (float2)( 0.0 );
			gl_Position = float4( ( k & 1 ) != 0 ? x1 : x0, ( k & 2 ) != 0 ? 1.0 : -1.0, 0.0, 1.0 );
			EmitVertex( geometry_stream );
		}
		EndPrimitive( geometry_stream );
		return;
	}

	uv = uvBase + float2( 1.0, 1.0 ) * particleUV.zw;
	for ( int i = 0; i < 9; i++ )
		texCoords[i] = uv;
	pPos = ( gl_in[0].gl_Position + c11 ).xyz;
	pQ = float2( 1.0, 1.0 );
	gl_Position = mul(( gl_in[0].gl_Position + c11 ), projectionMatrix);
	EmitVertex( geometry_stream );
	uv = uvBase + float2( 0.0, 1.0 ) * particleUV.zw;
	for ( int i = 0; i < 9; i++ )
		texCoords[i] = uv;
	pPos = ( gl_in[0].gl_Position + c01 ).xyz;
	pQ = float2( -1.0, 1.0 );
	gl_Position = mul(( gl_in[0].gl_Position + c01 ), projectionMatrix);
	EmitVertex( geometry_stream );
	uv = uvBase + float2( 1.0, 0.0 ) * particleUV.zw;
	for ( int i = 0; i < 9; i++ )
		texCoords[i] = uv;
	pPos = ( gl_in[0].gl_Position + c10 ).xyz;
	pQ = float2( 1.0, -1.0 );
	gl_Position = mul(( gl_in[0].gl_Position + c10 ), projectionMatrix);
	EmitVertex( geometry_stream );
	uv = uvBase + float2( 0.0, 0.0 ) * particleUV.zw;
	for ( int i = 0; i < 9; i++ )
		texCoords[i] = uv;
	pPos = ( gl_in[0].gl_Position + c00 ).xyz;
	pQ = float2( -1.0, -1.0 );
	gl_Position = mul(( gl_in[0].gl_Position + c00 ), projectionMatrix);
	EmitVertex( geometry_stream );

	EndPrimitive( geometry_stream );
}

struct SPIRV_Cross_Input
{
    WW_LOC(2) float3 vsLightDir : TEXCOORD2;
    WW_LOC(5) float3 vsViewDir : TEXCOORD5;
    WW_LOC(0) float4 vsColor : TEXCOORD0;
    WW_LOC(3) float vsParticleSize : TEXCOORD3;
    WW_LOC(4) float2 vsUVOffset : TEXCOORD4;
    WW_LOC(1) float vsAngle : TEXCOORD1;
    float4 gl_PositionIn : SV_Position;
};

[maxvertexcount(4)]
void main(point SPIRV_Cross_Input stage_input[1], uint gl_PrimitiveID_in : SV_PrimitiveID, inout WW_GS_STREAM geometry_stream)
{
    for (int i = 0; i < 1; i++)
        gl_in[i].gl_Position = stage_input[i].gl_PositionIn;
    for (int i = 0; i < 1; i++)
        vsLightDir[i] = stage_input[i].vsLightDir;
    for (int i = 0; i < 1; i++)
        vsViewDir[i] = stage_input[i].vsViewDir;
    for (int i = 0; i < 1; i++)
        vsColor[i] = stage_input[i].vsColor;
    for (int i = 0; i < 1; i++)
        vsParticleSize[i] = stage_input[i].vsParticleSize;
    for (int i = 0; i < 1; i++)
        vsUVOffset[i] = stage_input[i].vsUVOffset;
    for (int i = 0; i < 1; i++)
        vsAngle[i] = stage_input[i].vsAngle;
    gl_PrimitiveIDIn = int(gl_PrimitiveID_in);
    geom_main(geometry_stream);
}

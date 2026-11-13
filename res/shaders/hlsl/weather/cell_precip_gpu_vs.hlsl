// HLSL source of features/weather/cell_precip_gpu.vert (vertex shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "WeatherBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
static float4 gl_Position;
static int gl_VertexID;
/* lane PERFFIX3 D (2026-11-01): one precipitation particle a point, computed here from gl_VertexID (cell_precip_field.glsl,
 * the GPU twin of src/features/weather/precipmath.h particleAt): no CPU field, no upload. Then the frustum cull: a particle whose drawn
 * quad (cell_precip_gpu.geom: flat-on, so every corner sits at the centre's depth) cannot touch the view is flagged and
 * the geometry shader emits nothing and lights nothing.
 * Red WW_CELL_PRECIP_RED=cpufield: the old program (cell_precip.prog, the CPU field). */

#define WW_LOOSE_TYPES 1
#include "cell_precip_field.hlsli"
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#include "cell_precip_field.hlsli"
#undef WW_LOOSE_UNIFORMS
	float4 pcRow[3];		// world -> view rows (scale in); only the rotation is used: the eye is the view origin
	row_major float4x4 pcProj;
	float pcFocal;		// pixels per view unit at depth 1
	float pcMinPx;		// the least half size drawn, pixels
	float pcShutter;	// seconds
	float pcThin;		// how many real particles one stands for (far tier), the opacity factor
};
#include "cell_precip_field.hlsli"



static float3 vsView;
static float4 vsColor;
static float2 vsCell;
static float vsAngle;
static float vsHalf;
static float3 vsVel;
static float vsCull;
#ifdef PC_TEST
static float3 vsRel;
#endif

void vert_main()
{
	float3 rel, vel;
	float hs, angle;
	float2 cellUv;
	bool keep = pcParticle( uint( gl_VertexID ), rel, vel, hs, angle, cellUv );
	vsView = float3( dot( pcRow[0].xyz, rel ), dot( pcRow[1].xyz, rel ), dot( pcRow[2].xyz, rel ) );
	vsVel = float3( dot( pcRow[0].xyz, vel ), dot( pcRow[1].xyz, vel ), dot( pcRow[2].xyz, vel ) );
	vsHalf = hs * length( pcRow[0].xyz );
	vsColor = float4( 1.0, 1.0, 1.0, pcThin );
	vsCell = cellUv;
	vsAngle = angle;
#ifdef PC_TEST
	vsRel = rel;
#endif
	// the cull: the quad's corners lie within R = 2 hd + travel of the centre, in the centre's depth plane
	float depth = -vsView.z;
	bool cull = or( !keep, depth < 1.0 );
	if ( !cull ) {
		float hd = max( vsHalf, pcMinPx * depth / pcFocal );
		float R = 2.0 * hd + length( vsVel.xy ) * pcShutter;
#ifdef PC_CULL_RED
		R = 0.0;	// tests/features/weather/precipgpu red: the quad's extent ignored (edge particles lost)
#endif
		float4 c = mul(float4( vsView, 1.0 ), pcProj);
		float rx = abs( pcProj[0][0] ) * R, ry = abs( pcProj[1][1] ) * R;
		cull = or( or( or( c.x - rx > c.w, c.x + rx < -c.w ), c.y - ry > c.w ), c.y + ry < -c.w );
	}
	vsCull = cull ? 1.0 : 0.0;
	gl_Position = float4( vsView, 1.0 );
}

struct SPIRV_Cross_Input
{
    uint gl_VertexID : SV_VertexID;
};

struct SPIRV_Cross_Output
{
    float3 vsView : TEXCOORD0;
    float4 vsColor : TEXCOORD1;
    float2 vsCell : TEXCOORD2;
    float vsAngle : TEXCOORD3;
    float vsHalf : TEXCOORD4;
    float3 vsVel : TEXCOORD5;
    float vsCull : TEXCOORD6;
#if defined(PC_TEST)
    float3 vsRel : TEXCOORD7;
#endif
    float4 gl_Position : SV_Position;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_VertexID = int(stage_input.gl_VertexID);
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.vsView = vsView;
    stage_output.vsColor = vsColor;
    stage_output.vsCell = vsCell;
    stage_output.vsAngle = vsAngle;
    stage_output.vsHalf = vsHalf;
    stage_output.vsVel = vsVel;
    stage_output.vsCull = vsCull;
#if defined(PC_TEST)
    stage_output.vsRel = vsRel;
#endif
    return stage_output;
}

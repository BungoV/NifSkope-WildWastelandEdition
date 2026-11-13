// HLSL source of features/weather/cell_precip.geom (geometry shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: gs_6_0
#include "WeatherBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
static float4 gl_Position;
/* lane WEATHERLOOK1 phase B: a precipitation particle's quad, flat-on to the camera (bungo 10-28: every crystal
 * flat-on), turned by its slow spin about the view axis. Coverage is kept whatever the drawn size:
 *   stretch  the particle's travel in the view plane over the shutter (pcShutter, 1/120 s) lengthens the quad along
 *            it (airpart_data.md "draw-time stretch"); the swept area is 2h (2h + travel)
 *   small    a quad under pcMinPx pixels of half size is drawn at pcMinPx
 *   opacity  x 4 h^2 / ((2h' + travel) 2h'), h the true half size, h' the drawn one (the box average of the flake)
 * pcFar = 1: a far dot (precipmath.h far tier): its colour is the cell's mean, read at the 1-texel-a-cell mip */

// GLSL: layout ( points ) in; (main's signature)
// GLSL: layout ( triangle_strip , max_vertices = 4 ) out; (main's signature)
cbuffer LooseUniforms : register(b31, space0)
{
	row_major float4x4 pcProj;
	float4 pcAtlas;		// cell size in uv (x, y), the deepest mip a near quad reads, the mip with one texel a cell
	float2 pcInset;		// half a texel of that deepest mip, in uv
	float pcShutter;	// seconds
	float pcFocal;		// pixels per view unit at depth 1
	float pcMinPx;		// the least half size drawn, pixels
	int pcFar;
	bool pcHasTex;
};

Texture2D<float4> pcTex : register(t0, space0); SamplerState _pcTex_sampler : register(s0, space0);

static float3 vsView[1];
static float4 vsColor[1];
static float2 vsCell[1];
static float vsAngle[1];
static float vsHalf[1];
static float3 vsVel[1];

static float2 gUv;
static float2 gQ;
static float3 gView;
static float4 gCol;
static float4 gBox;	// the cell's uv box, inset half a texel of the deepest mip read (no neighbour in a bilinear read)


struct SPIRV_Cross_Output
{
    float2 gUv : TEXCOORD0;
    float2 gQ : TEXCOORD1;
    float3 gView : TEXCOORD2;
    nointerpolation float4 gCol : TEXCOORD3;
    nointerpolation float4 gBox : TEXCOORD4;
    float4 gl_Position : SV_Position;
};

#define WW_GS_STREAM TriangleStream<SPIRV_Cross_Output>
void EmitVertex(inout WW_GS_STREAM geometry_stream)
{
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.gUv = gUv;
    stage_output.gQ = gQ;
    stage_output.gView = gView;
    stage_output.gCol = gCol;
    stage_output.gBox = gBox;
    geometry_stream.Append(stage_output);
}

void EndPrimitive(inout WW_GS_STREAM geometry_stream)
{
    geometry_stream.RestartStrip();
}

void geom_main(inout WW_GS_STREAM geometry_stream)
{
	float3 c = vsView[0];
	float depth = -c.z;
	if ( depth < 1.0 )
		return;
	float h = vsHalf[0];
	float k = pcFocal / depth;	// pixels per view unit here
	float2 tr = vsVel[0].xy * pcShutter;
	float tl = length( tr );
	float hd = max( h, pcMinPx / k );
	float4 col = vsColor[0];
	col.a *= 4.0 * h * h / ( ( 2.0 * hd + tl ) * 2.0 * hd );
	float2 ax, ay;
	float hx = hd, hy = hd;
	if ( tl > 0.2 * hd ) {
		ax = tr / tl;
		hx = hd + 0.5 * tl;
	} else {
		ax = float2( cos( vsAngle[0] ), sin( vsAngle[0] ) );
	}
	ay = float2( -ax.y, ax.x );
	float2 cell = pcAtlas.xy;
	gBox = float4( vsCell[0] + pcInset, vsCell[0] + cell - pcInset );
	if ( pcFar != 0 ) {
		// the cell's mean: 16 taps at the one-texel-a-cell mip (all one texel there; a capped chain averages its 4 x 4)
		float4 m = (float4)( 0.0 );
		if ( pcHasTex ) {
			for ( int i = 0; i < 4; i++ )
				for ( int j = 0; j < 4; j++ )
					m += pcTex.SampleLevel( _pcTex_sampler, vsCell[0] + cell * ( float2( float( i ), float( j ) ) + 0.5 ) * 0.25, pcAtlas.w );
			m *= 1.0 / 16.0;
		} else {
			m = float4( 1.0, 1.0, 1.0, 0.3 );
		}
		col = float4( col.rgb * m.rgb, col.a * m.a );
	}
	gCol = col;
	for ( int v = 0; v < 4; v++ ) {
		float2 q = float2( ( v & 1 ) != 0 ? 1.0 : -1.0, ( v & 2 ) != 0 ? 1.0 : -1.0 );
		float3 p = c + float3( ax * ( q.x * hx ) + ay * ( q.y * hy ), 0.0 );
		gQ = q;
		gUv = vsCell[0] + cell * ( q * 0.5 + 0.5 );
		gView = p;
		gl_Position = mul(float4( p, 1.0 ), pcProj);
		EmitVertex( geometry_stream );
	}
	EndPrimitive( geometry_stream );
}

struct SPIRV_Cross_Input
{
    float3 vsView : TEXCOORD0;
    float4 vsColor : TEXCOORD1;
    float2 vsCell : TEXCOORD2;
    float vsAngle : TEXCOORD3;
    float vsHalf : TEXCOORD4;
    float3 vsVel : TEXCOORD5;
};

[maxvertexcount(4)]
void main(point SPIRV_Cross_Input stage_input[1], inout WW_GS_STREAM geometry_stream)
{
    for (int i = 0; i < 1; i++)
        vsView[i] = stage_input[i].vsView;
    for (int i = 0; i < 1; i++)
        vsColor[i] = stage_input[i].vsColor;
    for (int i = 0; i < 1; i++)
        vsCell[i] = stage_input[i].vsCell;
    for (int i = 0; i < 1; i++)
        vsAngle[i] = stage_input[i].vsAngle;
    for (int i = 0; i < 1; i++)
        vsHalf[i] = stage_input[i].vsHalf;
    for (int i = 0; i < 1; i++)
        vsVel[i] = stage_input[i].vsVel;
    geom_main(geometry_stream);
}

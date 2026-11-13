// HLSL source of features/weather/cell_precip_gpu.geom (geometry shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_AIRZONE, WW_CELL_FOGLEAK, WW_CELL_RESTIR, WW_CELL_RESTIRGI, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES, WW_FRAME_UBO (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: gs_6_0
#include "WeatherBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
// inlined (no readable .hlsli yet): uniforms.glsl
static float4 gl_Position;
/* lane PERFFIX3 D (2026-11-01): a precipitation particle's quad (as cell_precip.geom: flat-on, its slow spin, the motion
 * stretch, the least drawn size, coverage kept), for the particles cell_precip_gpu.vert did not cull, LIT ONCE HERE at
 * the particle's centre instead of at every fragment: the volumetric particles' law (cell_volpart.glsl vpLight: the
 * sun x its cascades, every placed light x the air to it, the GI, the ambient) and then the air (wwFogFx).
 * The fog is affine in the colour on the default paths (vanilla fog: mix( lin, fc, f ); the air: lin T + L; the aerial
 * perspective: lin T x residual), so the fragment's  wwFogFx( alb x lit )  is  alb x A + B  with B = wwFogFx( 0 ) and
 * A = wwFogFx( lit ) - B, both per particle (the air's volFogBlend 1 red and its grey rule are the exception).
 * The lighting files read the fragment's window position and its derivatives; here they read the particle centre's
 * (pcFragCoord) and the flat-on quad's own (one pixel = depth / focal view units in x and y). */

#define WW_CELLLIGHTS 1
static float4 pcFragCoord;
static float3 pcDPx, pcDPy;
#define gl_FragCoord pcFragCoord
#define ddx( p ) pcDPx
#define ddy( p ) pcDPy
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
#define WW_LOOSE_TYPES 1
#include "../air/lookdev_fog.hlsli"
#include "../lights/cell_lights.hlsli"
#include "../lights/cell_cluster.hlsli"
#if defined(WW_DEBUG_VIEWS)
#include "../overlays/debug_view.hlsli"
#endif
#if defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH))
#include "../ao/cell_gihalfat.hlsli"
#endif
#if defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#endif
#if !(defined(WW_CELL_PRT)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#include "../ao/cell_gihalfat.hlsli"
#endif
#include "../air/cell_volpart.hlsli"
#include "../lights/cell_fxlaw.hlsli"
#if !defined(WW_SUNSHADOW)
#include "../shadows/ww_sunshadow.hlsli"
#endif
#ifndef WW_AIR_GIVOL
#define WW_AIR_GIVOL 1	// as below, before this file
#endif
#include "../air/cell_airmed.hlsli"
#if ((defined( WW_CELL_AIRZONE ) && !defined( WW_FX_DECALLIT )))
#include "../air/cell_airzone.hlsli"
#endif
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#include "../air/lookdev_fog.hlsli"
#include "../lights/cell_lights.hlsli"
#include "../lights/cell_cluster.hlsli"
#if defined(WW_DEBUG_VIEWS)
#include "../overlays/debug_view.hlsli"
#endif
#if defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH))
#include "../ao/cell_gihalfat.hlsli"
#endif
#if defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#endif
#if !(defined(WW_CELL_PRT)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#include "../ao/cell_gihalfat.hlsli"
#endif
#include "../air/cell_volpart.hlsli"
#include "../lights/cell_fxlaw.hlsli"
#if !defined(WW_SUNSHADOW)
#include "../shadows/ww_sunshadow.hlsli"
#endif
#ifndef WW_AIR_GIVOL
#define WW_AIR_GIVOL 1	// as below, before this file
#endif
#include "../air/cell_airmed.hlsli"
#if ((defined( WW_CELL_AIRZONE ) && !defined( WW_FX_DECALLIT )))
#include "../air/cell_airzone.hlsli"
#endif
#undef WW_LOOSE_UNIFORMS
	float lightingInfluence;	// cell_volpart.glsl's vpLit reads them (not used here)
	int fxLitMode;
	row_major float4x4 pcProj;
	float4 pcAtlas;		// cell size in uv (x, y), the deepest mip a near quad reads, the mip with one texel a cell
	float2 pcInset;		// half a texel of that deepest mip, in uv
	float pcShutter;	// seconds
	float pcFocal;		// pixels per view unit at depth 1
	float pcMinPx;		// the least half size drawn, pixels
	float4 pcViewport;	// the frame's viewport (x, y, w, h): the centre's window position
	int pcFar;
	bool pcHasTex;
	float pcTrans;		// the thin (scattering) share
#if defined(PC_LOOK)
	/* lane PRECIPLOOK1 (cell_precip_look.prog; pcLook in cell_volpart.glsl). Drop (1): the quad holds the drop's disc swept
	 * over the shutter; the fragment draws its time-averaged coverage (cell_precip_gpu.frag). The drop's shape oscillates
	 * (Garg & Nayar 2006: r = r0 (1 + A20 sin(w2 t) P20 + A31 sin(w3 t) cos(phi) P31), w_n^2 = n (n-1) (n+2) sigma / (rho r0^3)),
	 * so its brightness flickers along the streak: depth from the offline streak render (notes/preciplook1/tools/
	 * droprender.py, std / mean along a 2 mm streak 0.36 at A20 0.2, 0.19 at 4 mm, 0.04 for a sphere), cycles = w2 x shutter.
	 * Snow (2): thin and thick light apart, the key light's direction, for the normal map's split per fragment. */
	float pcMmPerView;	// mm of the particle's true size per view unit (14.28 / (view scale x draw scale))
#endif
};
#include "../air/lookdev_fog.hlsli"
#include "../lights/cell_lights.hlsli"

#include "../air/cell_volpart.hlsli"

// GLSL: layout ( points ) in; (main's signature)
// GLSL: layout ( triangle_strip , max_vertices = 4 ) out; (main's signature)
Texture2D<float4> pcTex : register(t46, space0); SamplerState _pcTex_sampler : register(s46, space0);

static float3 vsView[1];
static float4 vsColor[1];
static float2 vsCell[1];
static float vsAngle[1];
static float vsHalf[1];
static float3 vsVel[1];
static float vsCull[1];

static float2 gUv;
static float2 gQ;
static float3 gView;
static float4 gCol;
static float4 gBox;	// the cell's uv box, inset half a texel of the deepest mip read (no neighbour in a bilinear read)
static float3 gA;	// the fogged light per unit albedo
static float3 gB;	// the fog's own light
#ifdef PC_LOOK

static float4 gDrop;	// drop: the disc's radius, the streak's length (view units), the flicker depth, cycles over the streak
static float gOsc;	// drop: the flicker's phase
static float3 gThin;	// snow: the fogged thin light per unit albedo
static float3 gThick;	// snow: the fogged thick light per unit albedo
static float4 gDomL;	// snow: the key light's direction (view, toward the light) and its directional share
static float3 gDomE;	// snow: the key light's fogged colour
static float2 gAx;	// the quad's u axis on screen (the normal map's tangent)
#endif


struct SPIRV_Cross_Output
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
    stage_output.gA = gA;
    stage_output.gB = gB;
#if defined(PC_LOOK)
    stage_output.gDrop = gDrop;
    stage_output.gOsc = gOsc;
    stage_output.gThin = gThin;
    stage_output.gThick = gThick;
    stage_output.gDomL = gDomL;
    stage_output.gDomE = gDomE;
    stage_output.gAx = gAx;
#endif
    geometry_stream.Append(stage_output);
}

void EndPrimitive(inout WW_GS_STREAM geometry_stream)
{
    geometry_stream.RestartStrip();
}

void geom_main(inout WW_GS_STREAM geometry_stream)
{
	if ( vsCull[0] > 0.5 )
		return;
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
#ifdef PC_LOOK
	float4 pDrop = (float4)( 0.0 );
	float pOsc = 0.0;
	if ( pcLook == 1 ) {
		// the coverage is the fragment's (swept disc): here only the least-size spread (h^2 / hd^2)
		col = float4( vsColor[0].rgb, vsColor[0].a * h * h / ( hd * hd ) );
		float D = 2.0 * h * pcMmPerView;	// mm
		float r0 = 0.0005 * D;	// m
		float w2 = sqrt( 8.0 * 0.0728 / ( 1000.0 * r0 * r0 * r0 ) );	// rad/s, n = 2
		float kf = 0.51 * clamp( ( D - 0.5 ) / 1.5, 0.0, 1.0 ) * lerp( 1.0, 0.53, clamp( ( D - 2.0 ) / 2.0, 0.0, 1.0 ) );
		pDrop = float4( hd, tl, kf, w2 * pcShutter / 6.2831853 );
		pOsc = vsAngle[0] * 7.0;
	}
	if ( tl > ( pcLook == 1 ? 1e-4 * hd : 0.2 * hd ) ) {
#else
	if ( tl > 0.2 * hd ) {
#endif
		ax = tr / tl;
		hx = hd + 0.5 * tl;
	} else {
		ax = float2( cos( vsAngle[0] ), sin( vsAngle[0] ) );
	}
	ay = float2( -ax.y, ax.x );
	float2 cell = pcAtlas.xy;
	gBox = float4( vsCell[0] + pcInset, vsCell[0] + cell - pcInset );
#ifdef PC_LOOK
	if ( pcFar != 0 && pcLook != 1 ) {
#else
	if ( pcFar != 0 ) {
#endif
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

	// the light, once, at the centre
	float4 cc = mul(float4( c, 1.0 ), pcProj);
	pcFragCoord = float4( ( cc.xy / cc.w * 0.5 + 0.5 ) * pcViewport.zw + pcViewport.xy, cc.z / cc.w * 0.5 + 0.5, 1.0 / cc.w );
	pcDPx = float3( 1.0 / k, 0.0, 0.0 );
	pcDPy = float3( 0.0, 1.0 / k, 0.0 );
	float3 lit = (float3)( 1.0 );
#ifdef PC_LOOK
	float3 thinS = (float3)( 1.0 ), thickS = (float3)( 1.0 ), domE = (float3)( 0.0 );
	float4 domL = float4( 0.0, 0.0, 1.0, 0.0 );
#endif
	if ( or( cellInterior, cellHasDir ) ) {
		float3 thin, thick;
		vpLight( c, cellWorldPos( c ), cellWorldDir( c ), dot( fogView.xyz, c ) + fogView.w,
			cellWorldDir( float3( 0.0, 0.0, 1.0 ) ), pcTrans, thin, thick );
		lit = pcTrans * thin + ( 1.0 - pcTrans ) * thick;
#ifdef PC_LOOK
		thinS = thin;
		thickS = thick;
		float3 dV = cellRow[0].xyz * vpDom.x + cellRow[1].xyz * vpDom.y + cellRow[2].xyz * vpDom.z;	// world -> view (rows transposed)
		float dl = length( dV );
		domL = float4( dl > 0.0 ? dV / dl : float3( 0.0, 0.0, 1.0 ), vpDom.w > 0.0 ? clamp( length( vpDom.xyz ) / vpDom.w, 0.0, 1.0 ) : 0.0 );
		domE = vpDomC * domL.w;
#endif
	}
	float3 fogB = wwFogFx( (float3)( 0.0 ), c, false );
	float3 fogA = wwFogFx( lit, c, false ) - fogB;
#ifdef PC_LOOK
	float3 pThin = pcLook == 2 ? wwFogFx( thinS, c, false ) - fogB : (float3)( 0.0 );
	float3 pThick = pcLook == 2 ? wwFogFx( thickS, c, false ) - fogB : (float3)( 0.0 );
	float3 pDomE = pcLook == 2 ? wwFogFx( domE, c, false ) - fogB : (float3)( 0.0 );
#endif

	for ( int v = 0; v < 4; v++ ) {
		float2 q = float2( ( v & 1 ) != 0 ? 1.0 : -1.0, ( v & 2 ) != 0 ? 1.0 : -1.0 );
		float3 p = c + float3( ax * ( q.x * hx ) + ay * ( q.y * hy ), 0.0 );
		gQ = q;
		gUv = vsCell[0] + cell * ( q * 0.5 + 0.5 );
		gView = p;
		gA = fogA;
		gB = fogB;
#ifdef PC_LOOK
		gDrop = pDrop;
		gOsc = pOsc;
		gThin = pThin;
		gThick = pThick;
		gDomL = domL;
		gDomE = pDomE;
		gAx = ax;
#endif
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
    float vsCull : TEXCOORD6;
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
    for (int i = 0; i < 1; i++)
        vsCull[i] = stage_input[i].vsCull;
    geom_main(geometry_stream);
}

#version 410 core

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
vec4 pcFragCoord;
vec3 pcDPx, pcDPy;
#define gl_FragCoord pcFragCoord
#define dFdx( p ) pcDPx
#define dFdy( p ) pcDPy
#include "uniforms.glsl"
#include "lookdev_fog.glsl"
#include "cell_lights.glsl"
uniform float lightingInfluence;	// cell_volpart.glsl's vpLit reads them (not used here)
uniform int fxLitMode;
#include "cell_volpart.glsl"

layout ( points ) in;
layout ( triangle_strip, max_vertices = 4 ) out;

uniform mat4 pcProj;
uniform vec4 pcAtlas;		// cell size in uv (x, y), the deepest mip a near quad reads, the mip with one texel a cell
uniform vec2 pcInset;		// half a texel of that deepest mip, in uv
uniform float pcShutter;	// seconds
uniform float pcFocal;		// pixels per view unit at depth 1
uniform float pcMinPx;		// the least half size drawn, pixels
uniform vec4 pcViewport;	// the frame's viewport (x, y, w, h): the centre's window position
uniform int pcFar;
uniform bool pcHasTex;
uniform sampler2D pcTex;
uniform float pcTrans;		// the thin (scattering) share

in vec3 vsView[];
in vec4 vsColor[];
in vec2 vsCell[];
in float vsAngle[];
in float vsHalf[];
in vec3 vsVel[];
in float vsCull[];

out vec2 gUv;
out vec2 gQ;
out vec3 gView;
flat out vec4 gCol;
flat out vec4 gBox;	// the cell's uv box, inset half a texel of the deepest mip read (no neighbour in a bilinear read)
flat out vec3 gA;	// the fogged light per unit albedo
flat out vec3 gB;	// the fog's own light
#ifdef PC_LOOK
/* lane PRECIPLOOK1 (cell_precip_look.prog; pcLook in cell_volpart.glsl). Drop (1): the quad holds the drop's disc swept
 * over the shutter; the fragment draws its time-averaged coverage (cell_precip_gpu.frag). The drop's shape oscillates
 * (Garg & Nayar 2006: r = r0 (1 + A20 sin(w2 t) P20 + A31 sin(w3 t) cos(phi) P31), w_n^2 = n (n-1) (n+2) sigma / (rho r0^3)),
 * so its brightness flickers along the streak: depth from the offline streak render (notes/preciplook1/tools/
 * droprender.py, std / mean along a 2 mm streak 0.36 at A20 0.2, 0.19 at 4 mm, 0.04 for a sphere), cycles = w2 x shutter.
 * Snow (2): thin and thick light apart, the key light's direction, for the normal map's split per fragment. */
uniform float pcMmPerView;	// mm of the particle's true size per view unit (14.28 / (view scale x draw scale))
flat out vec4 gDrop;	// drop: the disc's radius, the streak's length (view units), the flicker depth, cycles over the streak
flat out float gOsc;	// drop: the flicker's phase
flat out vec3 gThin;	// snow: the fogged thin light per unit albedo
flat out vec3 gThick;	// snow: the fogged thick light per unit albedo
flat out vec4 gDomL;	// snow: the key light's direction (view, toward the light) and its directional share
flat out vec3 gDomE;	// snow: the key light's fogged colour
flat out vec2 gAx;	// the quad's u axis on screen (the normal map's tangent)
#endif

void main()
{
	if ( vsCull[0] > 0.5 )
		return;
	vec3 c = vsView[0];
	float depth = -c.z;
	if ( depth < 1.0 )
		return;
	float h = vsHalf[0];
	float k = pcFocal / depth;	// pixels per view unit here
	vec2 tr = vsVel[0].xy * pcShutter;
	float tl = length( tr );
	float hd = max( h, pcMinPx / k );
	vec4 col = vsColor[0];
	col.a *= 4.0 * h * h / ( ( 2.0 * hd + tl ) * 2.0 * hd );
	vec2 ax, ay;
	float hx = hd, hy = hd;
#ifdef PC_LOOK
	vec4 pDrop = vec4( 0.0 );
	float pOsc = 0.0;
	if ( pcLook == 1 ) {
		// the coverage is the fragment's (swept disc): here only the least-size spread (h^2 / hd^2)
		col = vec4( vsColor[0].rgb, vsColor[0].a * h * h / ( hd * hd ) );
		float D = 2.0 * h * pcMmPerView;	// mm
		float r0 = 0.0005 * D;	// m
		float w2 = sqrt( 8.0 * 0.0728 / ( 1000.0 * r0 * r0 * r0 ) );	// rad/s, n = 2
		float kf = 0.51 * clamp( ( D - 0.5 ) / 1.5, 0.0, 1.0 ) * mix( 1.0, 0.53, clamp( ( D - 2.0 ) / 2.0, 0.0, 1.0 ) );
		pDrop = vec4( hd, tl, kf, w2 * pcShutter / 6.2831853 );
		pOsc = vsAngle[0] * 7.0;
	}
	if ( tl > ( pcLook == 1 ? 1e-4 * hd : 0.2 * hd ) ) {
#else
	if ( tl > 0.2 * hd ) {
#endif
		ax = tr / tl;
		hx = hd + 0.5 * tl;
	} else {
		ax = vec2( cos( vsAngle[0] ), sin( vsAngle[0] ) );
	}
	ay = vec2( -ax.y, ax.x );
	vec2 cell = pcAtlas.xy;
	gBox = vec4( vsCell[0] + pcInset, vsCell[0] + cell - pcInset );
#ifdef PC_LOOK
	if ( pcFar != 0 && pcLook != 1 ) {
#else
	if ( pcFar != 0 ) {
#endif
		// the cell's mean: 16 taps at the one-texel-a-cell mip (all one texel there; a capped chain averages its 4 x 4)
		vec4 m = vec4( 0.0 );
		if ( pcHasTex ) {
			for ( int i = 0; i < 4; i++ )
				for ( int j = 0; j < 4; j++ )
					m += textureLod( pcTex, vsCell[0] + cell * ( vec2( float( i ), float( j ) ) + 0.5 ) * 0.25, pcAtlas.w );
			m *= 1.0 / 16.0;
		} else {
			m = vec4( 1.0, 1.0, 1.0, 0.3 );
		}
		col = vec4( col.rgb * m.rgb, col.a * m.a );
	}
	gCol = col;

	// the light, once, at the centre
	vec4 cc = pcProj * vec4( c, 1.0 );
	pcFragCoord = vec4( ( cc.xy / cc.w * 0.5 + 0.5 ) * pcViewport.zw + pcViewport.xy, cc.z / cc.w * 0.5 + 0.5, 1.0 / cc.w );
	pcDPx = vec3( 1.0 / k, 0.0, 0.0 );
	pcDPy = vec3( 0.0, 1.0 / k, 0.0 );
	vec3 lit = vec3( 1.0 );
#ifdef PC_LOOK
	vec3 thinS = vec3( 1.0 ), thickS = vec3( 1.0 ), domE = vec3( 0.0 );
	vec4 domL = vec4( 0.0, 0.0, 1.0, 0.0 );
#endif
	if ( cellInterior || cellHasDir ) {
		vec3 thin, thick;
		vpLight( c, cellWorldPos( c ), cellWorldDir( c ), dot( fogView.xyz, c ) + fogView.w,
			cellWorldDir( vec3( 0.0, 0.0, 1.0 ) ), pcTrans, thin, thick );
		lit = pcTrans * thin + ( 1.0 - pcTrans ) * thick;
#ifdef PC_LOOK
		thinS = thin;
		thickS = thick;
		vec3 dV = cellRow[0].xyz * vpDom.x + cellRow[1].xyz * vpDom.y + cellRow[2].xyz * vpDom.z;	// world -> view (rows transposed)
		float dl = length( dV );
		domL = vec4( dl > 0.0 ? dV / dl : vec3( 0.0, 0.0, 1.0 ), vpDom.w > 0.0 ? clamp( length( vpDom.xyz ) / vpDom.w, 0.0, 1.0 ) : 0.0 );
		domE = vpDomC * domL.w;
#endif
	}
	vec3 fogB = wwFogFx( vec3( 0.0 ), c, false );
	vec3 fogA = wwFogFx( lit, c, false ) - fogB;
#ifdef PC_LOOK
	vec3 pThin = pcLook == 2 ? wwFogFx( thinS, c, false ) - fogB : vec3( 0.0 );
	vec3 pThick = pcLook == 2 ? wwFogFx( thickS, c, false ) - fogB : vec3( 0.0 );
	vec3 pDomE = pcLook == 2 ? wwFogFx( domE, c, false ) - fogB : vec3( 0.0 );
#endif

	for ( int v = 0; v < 4; v++ ) {
		vec2 q = vec2( ( v & 1 ) != 0 ? 1.0 : -1.0, ( v & 2 ) != 0 ? 1.0 : -1.0 );
		vec3 p = c + vec3( ax * ( q.x * hx ) + ay * ( q.y * hy ), 0.0 );
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
		gl_Position = pcProj * vec4( p, 1.0 );
		EmitVertex();
	}
	EndPrimitive();
}

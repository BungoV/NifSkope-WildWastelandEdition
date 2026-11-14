// HLSL source of features/overlays/debug_view.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	int wwDebugView;	// the view's id, 0 off
	int wwDvOwner;		// who drew the shape (renderer.cpp): 0 near object, 1 near land, 2 far terrain, 3 far objects, 6 far water
#else
// lane DEBUGVIEW1: the live debug views (src/features/overlays/debugview.h, WW_DEBUG_VIEW / Show > Debug view).
// Included by cell_lights.glsl only under the loader's WW_DEBUG_VIEWS (added while a view is on), so a program
// without a view on carries none of it. The view replaces the pixel with one pass or term; terms with no source
// in a program read black. Ids: src/features/overlays/debugview.cpp kViews (same order).
#define WW_DV 1
#define WW_DV_ALBEDO 1
#define WW_DV_NORMALS 2
#define WW_DV_GLOSS 3
#define WW_DV_AO 4
#define WW_DV_SUN 5
#define WW_DV_SHADOW 6
#define WW_DV_AMBIENT 7
#define WW_DV_GI 8
#define WW_DV_EMISSIVE 9
#define WW_DV_FOG 10
#define WW_DV_AIR 11
#define WW_DV_CLOUDS 12
#define WW_DV_CLOUDSHADOW 13
#define WW_DV_WATER 14
#define WW_DV_WATERREFL 15
#define WW_DV_WATERHEIGHT 16
#define WW_DV_FAROWNER 17
#define WW_DV_EXPOSURE 18
#define WW_DV_DEPTH 19
#define WW_DV_OVERDRAW 20
#define WW_DV_SPECULAR 21
#define WW_DV_REFLECTION 22
#define WW_DV_WATERSKY 23



// the lit terms as cellLit sums them (cell_lights.glsl), linear light before the AO and the fog; black where unset
static float3 wwDvSun = (float3)( 0.0 );
static float3 wwDvAmb = (float3)( 0.0 );
static float3 wwDvGi = (float3)( 0.0 );
static float3 wwDvSpec = (float3)( 0.0 );
static float3 wwDvRefl = (float3)( 0.0 );
static float2 wwDvCloud = (float2)( 0.0 );	// SEAMCLOUD1: the cloudshadow view's height share and far share (ww_sunshadow.glsl ussCloudDvInfo)

float wwDvLum( float3 c )
{
	return dot( max( c, (float3)( 0.0 ) ), float3( 0.2126, 0.7152, 0.0722 ) );
}

// blue - cyan - green - yellow - red over 0..1
float3 wwDvRamp( float t )
{
	t = clamp( t, 0.0, 1.0 );
	return clamp( float3( 1.5 - abs( 4.0 * t - 3.0 ), 1.5 - abs( 4.0 * t - 2.0 ), 1.5 - abs( 4.0 * t - 1.0 ) ), 0.0, 1.0 );
}

// the absolute false colour: log10 of the luminance over -2..5 (0.01 .. 100000 scene units), black at none
float3 wwDvAbs( float3 c )
{
	float L = wwDvLum( c );
	return L <= 1.0e-7 ? (float3)( 0.0 ) : wwDvRamp( ( log( L ) * 0.43429448 + 2.0 ) / 7.0 );
}

// the distance from the eye on a log ramp: 2 u blue .. 4 M u red
float3 wwDvDepth( float dist )
{
	return wwDvRamp( log2( max( dist, 1.0 ) ) / 22.0 );
}

float3 wwDvOwnerColour( int o )
{
	return o == 1 ? float3( 0.1, 0.8, 0.1 ) : o == 2 ? float3( 1.0, 0.5, 0.0 ) : o == 3 ? float3( 0.15, 0.35, 1.0 )
		: o == 4 ? float3( 1.0, 1.0, 0.0 ) : o == 5 ? float3( 0.0, 1.0, 1.0 ) : o == 6 ? float3( 0.6, 0.2, 1.0 )
		: o == 7 ? float3( 0.0, 0.5, 0.5 ) : (float3)( 0.5 );
}

/* One surface's view. alb linear albedo, Nw world normal, gloss / specM the material's, aoBaked / aoScreen the two
 * obscurances, sunShadow the geometry's shadow (1 lit), cloudT the clouds' toward the sun, emitLin the applied glow,
 * fogT / fogIn the fog's transmittance and in-scatter, litPreFog the lit colour before the fog, hdr after it (the
 * exposure's input), dist from the eye in game units, owner the code above */
float3 wwDvSurface( float3 alb, float3 Nw, float gloss, float specM, float aoBaked, float aoScreen, float sunShadow,
                  float cloudT, float3 emitLin, float3 fogT, float3 fogIn, float3 litPreFog, float3 hdr, float dist, int owner )
{
	int v = wwDebugView;
	if ( v == WW_DV_ALBEDO )
		return sqrt( clamp( alb, 0.0, 1.0 ) );
	if ( v == WW_DV_NORMALS )
		return normalize( Nw ) * 0.5 + 0.5;
	if ( v == WW_DV_GLOSS )
		return float3( clamp( gloss, 0.0, 1.0 ), clamp( specM, 0.0, 1.0 ), 0.0 );
	if ( v == WW_DV_AO )
		return float3( clamp( aoBaked, 0.0, 1.0 ), clamp( aoScreen, 0.0, 1.0 ), clamp( aoBaked * aoScreen, 0.0, 1.0 ) );
	if ( v == WW_DV_SUN )
		return wwDvAbs( wwDvSun );
	if ( v == WW_DV_SHADOW )
		return (float3)( clamp( sunShadow, 0.0, 1.0 ) );
	if ( v == WW_DV_AMBIENT )
		return wwDvAbs( wwDvAmb );
	if ( v == WW_DV_GI )
		return wwDvAbs( wwDvGi );
	if ( v == WW_DV_EMISSIVE )
		return wwDvAbs( emitLin );
	if ( v == WW_DV_FOG ) {
		float s = wwDvLum( fogIn );
		return float3( 1.0 - clamp( wwDvLum( fogT ), 0.0, 1.0 ), s / max( s + wwDvLum( litPreFog * fogT ), 1.0e-7 ), 0.0 );
	}
	if ( v == WW_DV_AIR )	// FARWATER1 round 2: the air's transmittance per channel (white clear, black opaque)
		return clamp( fogT, 0.0, 1.0 );
	if ( v == WW_DV_CLOUDSHADOW ) {	// SEAMCLOUD1: grey = T; red = lit by height (above the cloud's start), blue = the far value
		float t = clamp( cloudT, 0.0, 1.0 );
		return float3( max( t, 0.6 * wwDvCloud.x ), t, max( t, 0.6 * wwDvCloud.y ) );
	}
	if ( v == WW_DV_FAROWNER )
		return wwDvOwnerColour( owner ) * ( 0.55 + 0.45 * abs( normalize( Nw ).z ) );
	if ( v == WW_DV_EXPOSURE )
		return wwDvAbs( hdr );
	if ( v == WW_DV_DEPTH )
		return wwDvDepth( dist );
	if ( v == WW_DV_SPECULAR )
		return wwDvAbs( wwDvSpec );
	if ( v == WW_DV_REFLECTION )
		return wwDvAbs( wwDvRefl );
	return (float3)( 0.0 );	// clouds and overdraw (owed: the log says why) and the water's own views
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/overlays/debug_view.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Not here (declarations, or no converted entry carried them): wwDvLum, wwDvRamp, wwDvAbs, wwDvDepth, wwDvOwnerColour, wwDvSurface
#ifdef WW_FN_ALL
#ifndef WW_DV
#define WW_DV 1
#endif
#ifndef WW_DV_ALBEDO
#define WW_DV_ALBEDO 1
#endif
#ifndef WW_DV_NORMALS
#define WW_DV_NORMALS 2
#endif
#ifndef WW_DV_GLOSS
#define WW_DV_GLOSS 3
#endif
#ifndef WW_DV_AO
#define WW_DV_AO 4
#endif
#ifndef WW_DV_SUN
#define WW_DV_SUN 5
#endif
#ifndef WW_DV_SHADOW
#define WW_DV_SHADOW 6
#endif
#ifndef WW_DV_AMBIENT
#define WW_DV_AMBIENT 7
#endif
#ifndef WW_DV_GI
#define WW_DV_GI 8
#endif
#ifndef WW_DV_EMISSIVE
#define WW_DV_EMISSIVE 9
#endif
#ifndef WW_DV_FOG
#define WW_DV_FOG 10
#endif
#ifndef WW_DV_AIR
#define WW_DV_AIR 11
#endif
#ifndef WW_DV_CLOUDS
#define WW_DV_CLOUDS 12
#endif
#ifndef WW_DV_CLOUDSHADOW
#define WW_DV_CLOUDSHADOW 13
#endif
#ifndef WW_DV_WATER
#define WW_DV_WATER 14
#endif
#ifndef WW_DV_WATERREFL
#define WW_DV_WATERREFL 15
#endif
#ifndef WW_DV_WATERHEIGHT
#define WW_DV_WATERHEIGHT 16
#endif
#ifndef WW_DV_FAROWNER
#define WW_DV_FAROWNER 17
#endif
#ifndef WW_DV_EXPOSURE
#define WW_DV_EXPOSURE 18
#endif
#ifndef WW_DV_DEPTH
#define WW_DV_DEPTH 19
#endif
#ifndef WW_DV_OVERDRAW
#define WW_DV_OVERDRAW 20
#endif
#ifndef WW_DV_SPECULAR
#define WW_DV_SPECULAR 21
#endif
#ifndef WW_DV_REFLECTION
#define WW_DV_REFLECTION 22
#endif
#ifndef WW_DV_WATERSKY
#define WW_DV_WATERSKY 23
#endif
#endif
#endif	// WW_HLSL_SRC

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

uniform int wwDebugView;	// the view's id, 0 off
uniform int wwDvOwner;		// who drew the shape (renderer.cpp): 0 near object, 1 near land, 2 far terrain, 3 far objects, 6 far water

// the lit terms as cellLit sums them (cell_lights.glsl), linear light before the AO and the fog; black where unset
vec3 wwDvSun = vec3( 0.0 );
vec3 wwDvAmb = vec3( 0.0 );
vec3 wwDvGi = vec3( 0.0 );
vec3 wwDvSpec = vec3( 0.0 );
vec3 wwDvRefl = vec3( 0.0 );

float wwDvLum( vec3 c )
{
	return dot( max( c, vec3( 0.0 ) ), vec3( 0.2126, 0.7152, 0.0722 ) );
}

// blue - cyan - green - yellow - red over 0..1
vec3 wwDvRamp( float t )
{
	t = clamp( t, 0.0, 1.0 );
	return clamp( vec3( 1.5 - abs( 4.0 * t - 3.0 ), 1.5 - abs( 4.0 * t - 2.0 ), 1.5 - abs( 4.0 * t - 1.0 ) ), 0.0, 1.0 );
}

// the absolute false colour: log10 of the luminance over -2..5 (0.01 .. 100000 scene units), black at none
vec3 wwDvAbs( vec3 c )
{
	float L = wwDvLum( c );
	return L <= 1.0e-7 ? vec3( 0.0 ) : wwDvRamp( ( log( L ) * 0.43429448 + 2.0 ) / 7.0 );
}

// the distance from the eye on a log ramp: 2 u blue .. 4 M u red
vec3 wwDvDepth( float dist )
{
	return wwDvRamp( log2( max( dist, 1.0 ) ) / 22.0 );
}

vec3 wwDvOwnerColour( int o )
{
	return o == 1 ? vec3( 0.1, 0.8, 0.1 ) : o == 2 ? vec3( 1.0, 0.5, 0.0 ) : o == 3 ? vec3( 0.15, 0.35, 1.0 )
		: o == 4 ? vec3( 1.0, 1.0, 0.0 ) : o == 5 ? vec3( 0.0, 1.0, 1.0 ) : o == 6 ? vec3( 0.6, 0.2, 1.0 )
		: o == 7 ? vec3( 0.0, 0.5, 0.5 ) : vec3( 0.5 );
}

/* One surface's view. alb linear albedo, Nw world normal, gloss / specM the material's, aoBaked / aoScreen the two
 * obscurances, sunShadow the geometry's shadow (1 lit), cloudT the clouds' toward the sun, emitLin the applied glow,
 * fogT / fogIn the fog's transmittance and in-scatter, litPreFog the lit colour before the fog, hdr after it (the
 * exposure's input), dist from the eye in game units, owner the code above */
vec3 wwDvSurface( vec3 alb, vec3 Nw, float gloss, float specM, float aoBaked, float aoScreen, float sunShadow,
                  float cloudT, vec3 emitLin, vec3 fogT, vec3 fogIn, vec3 litPreFog, vec3 hdr, float dist, int owner )
{
	int v = wwDebugView;
	if ( v == WW_DV_ALBEDO )
		return sqrt( clamp( alb, 0.0, 1.0 ) );
	if ( v == WW_DV_NORMALS )
		return normalize( Nw ) * 0.5 + 0.5;
	if ( v == WW_DV_GLOSS )
		return vec3( clamp( gloss, 0.0, 1.0 ), clamp( specM, 0.0, 1.0 ), 0.0 );
	if ( v == WW_DV_AO )
		return vec3( clamp( aoBaked, 0.0, 1.0 ), clamp( aoScreen, 0.0, 1.0 ), clamp( aoBaked * aoScreen, 0.0, 1.0 ) );
	if ( v == WW_DV_SUN )
		return wwDvAbs( wwDvSun );
	if ( v == WW_DV_SHADOW )
		return vec3( clamp( sunShadow, 0.0, 1.0 ) );
	if ( v == WW_DV_AMBIENT )
		return wwDvAbs( wwDvAmb );
	if ( v == WW_DV_GI )
		return wwDvAbs( wwDvGi );
	if ( v == WW_DV_EMISSIVE )
		return wwDvAbs( emitLin );
	if ( v == WW_DV_FOG ) {
		float s = wwDvLum( fogIn );
		return vec3( 1.0 - clamp( wwDvLum( fogT ), 0.0, 1.0 ), s / max( s + wwDvLum( litPreFog * fogT ), 1.0e-7 ), 0.0 );
	}
	if ( v == WW_DV_CLOUDSHADOW )
		return vec3( clamp( cloudT, 0.0, 1.0 ) );
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
	return vec3( 0.0 );	// air, clouds and overdraw (owed: the log says why) and the water's own views
}

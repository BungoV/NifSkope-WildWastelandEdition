/* Cascaded sun shadows, the receiver (lane CSM1; spec_cascaded_shadows.md 2.4 / 2.5).
 * Included only by programs that define WW_SUNSHADOW (pbrm_csm.frag,
 * lookdev_ground_csm.frag), so the default programs never carry a line of it.
 * Uploaded by gl/sunshadow.cpp wwSunShadowUniforms; the fit is echoed there. */

uniform sampler2DArrayShadow csmMap;
uniform mat4 csmMat[3];		// view space -> (u, v, depth) of cascade i
uniform vec3 csmInvRange;	// 1 / (far_i - 150): a world-unit offset in depth units
uniform vec4 csmParams;		// 1 / view scale, shadow distance D, map size, blend band B
uniform vec4 csmSplitOffset;	// the two pair boundaries (800, 3000), receiver offsets A / B (0.275, 1.0)
uniform int csmProbe;		// 0 off, 1 hard tap, 2 cascade colour, 3 filtered + blend, 4 final factor, 5 selection
uniform int csmRed;		// 1 noblend, 2 nofade, 4 diffonly, 8 factorhalf (the OFF leak)
uniform float csmForce;		// WW_CSM_FORCE: >= 0 replaces the factor (the factor-site wiring test); -1 off

// lane TERRSHADOW1: the unified set (gl/shadowset.h). ussOn = 0 runs the CSM1 code below unchanged.
uniform int ussOn;
uniform int ussCsmOn;			// the cascade maps were built this frame
uniform vec4 ussCsmFilter;		// x Vogel filter on, y radius (texels), z taps, w receiver-plane depth bias on
uniform vec3 ussCsmBias;		// the receiver offset of each cascade, world units along the light
uniform int ussTerrOn;
uniform sampler2D ussTerrMap;		// RG16: x the height above which a point is lit, y below which it is shadowed
uniform mat4 ussWorldFromView;
uniform vec4 ussTerrXform;		// u = x X.x + X.y, v = y X.z + X.w
uniform vec4 ussTerrRange;		// zmin, z span, the receiver's bias (world up), the edge fade (uv)
uniform int ussContactOn;
uniform sampler2D ussContactMap;	// R8 contact visibility, screen space
uniform vec2 ussContactInvSize;
uniform int ussView;			// WW_SHADOW_VIEW: 1 cascades, 2 terrain, 3 contact, 4 composed, 5 weights
uniform int ussRed;			// 1 terrseam, 2 mulcompose

// spec 2.5: the 16 Poisson taps on [0,1]^2, used as (p - 0.5) x 6 texels
const vec2 csmPoisson[16] = vec2[16](
	vec2( 0.493393, 0.394269 ), vec2( 0.798547, 0.885922 ), vec2( 0.247322, 0.926450 ), vec2( 0.051454, 0.140782 ),
	vec2( 0.831843, 0.009552 ), vec2( 0.428632, 0.017151 ), vec2( 0.015656, 0.749779 ), vec2( 0.758385, 0.496170 ),
	vec2( 0.223487, 0.562151 ), vec2( 0.011628, 0.406995 ), vec2( 0.241462, 0.304636 ), vec2( 0.430311, 0.727226 ),
	vec2( 0.981811, 0.278359 ), vec2( 0.407056, 0.500534 ), vec2( 0.123478, 0.463546 ), vec2( 0.809534, 0.682272 ) );

vec3 csmUvd( int i, vec3 posView )
{
	return ( csmMat[i] * vec4( posView, 1.0 ) ).xyz;
}

//! one bilinear comparison tap (GL_LINEAR + COMPARE_REF_TO_TEXTURE): 1 lit, 0 shadowed
float csmHard( int i, vec3 posView, float offsetWorld )
{
	vec3 q = csmUvd( i, posView );
	return texture( csmMap, vec4( q.xy, float( i ), q.z - offsetWorld * csmInvRange[i] ) );
}

//! the 16-tap Poisson filter, radius 3 texels
float csmFiltered( int i, vec3 posView, float offsetWorld )
{
	vec3 q = csmUvd( i, posView );
	float zRef = q.z - offsetWorld * csmInvRange[i];
	float k = 6.0 / csmParams.z;
	float sum = 0.0;
	for ( int t = 0; t < 16; t++ )
		sum += texture( csmMap, vec4( q.xy + ( csmPoisson[t] - 0.5 ) * k, float( i ), zRef ) );
	return sum / 16.0;
}

//! the cascade pair and the blend weight at this view depth (spec 2.4)
void csmSelect( vec3 posView, out int a, out int b, out float t )
{
	float dv = -posView.z * csmParams.x;
	float bound;
	if ( dv < csmSplitOffset.y ) {
		a = 0; b = 1; bound = csmSplitOffset.x;
	} else {
		a = 1; b = 2; bound = csmSplitOffset.y;
	}
	float x = clamp( ( dv - bound ) / csmParams.w, 0.0, 1.0 );
	t = ( csmRed & 1 ) != 0 ? step( 0.5, x ) : smoothstep( 0.0, 1.0, x );
}

//! the filtered factor with the seam blend, before the distance fade
float csmBlended( vec3 posView )
{
	int a, b;
	float t;
	csmSelect( posView, a, b, t );
	float sA = t < 1.0 ? csmFiltered( a, posView, csmSplitOffset.z ) : 1.0;
	float sB = t > 0.0 ? csmFiltered( b, posView, csmSplitOffset.w ) : 1.0;
	return mix( sA, sB, t );
}

// ---- lane TERRSHADOW1: the composition ----

float ussIgn( vec2 p )
{
	return fract( 52.9829189 * fract( dot( p, vec2( 0.06711056, 0.00583715 ) ) ) );
}

//! (A) the Vogel-disk filter, rotated per pixel, with the receiver-plane depth bias
float ussVogel( int i, vec3 posView, float offsetWorld, vec3 dPx, vec3 dPy )
{
	vec3 q = csmUvd( i, posView );
	float zRef = q.z - offsetWorld * csmInvRange[i];
	vec2 rpdb = vec2( 0.0 );
	if ( ussCsmFilter.w > 0.5 ) {
		vec3 qx = mat3( csmMat[i] ) * dPx, qy = mat3( csmMat[i] ) * dPy;
		float det = qx.x * qy.y - qx.y * qy.x;
		if ( abs( det ) > 1e-24 )
			rpdb = vec2( qx.z * qy.y - qx.y * qy.z, qx.x * qy.z - qy.x * qx.z ) / det;
	}
	float R = ussCsmFilter.y * mix( 0.8, 1.2, clamp( float( i ) * 0.5, 0.0, 1.0 ) );
	int n = max( int( ussCsmFilter.z ), 1 );
	float rot = 6.2831853 * ussIgn( gl_FragCoord.xy );
	float k = 1.0 / csmParams.z;
	float lim = 2000.0 * csmInvRange[i];
	float sum = 0.0;
	for ( int t = 0; t < 48; t++ ) {
		if ( t >= n )
			break;
		float r = sqrt( ( float( t ) + 0.5 ) / float( n ) ) * R;
		float th = float( t ) * 2.39996323 + rot;
		vec2 o = vec2( cos( th ), sin( th ) ) * ( r * k );
		sum += texture( csmMap, vec4( q.xy + o, float( i ), zRef + clamp( dot( rpdb, o ), -lim, lim ) ) );
	}
	return sum / float( n );
}

//! (A) the cascades and their distance-fade weight w (1 inside, 0 past the reach)
float ussCascades( vec3 posView, vec3 dPx, vec3 dPy, out float w )
{
	w = 0.0;
	if ( ussCsmOn == 0 )
		return 1.0;
	float dv = -posView.z * csmParams.x;
	if ( dv > csmParams.y + csmParams.w )
		return 1.0;
	int a, b;
	float t;
	csmSelect( posView, a, b, t );
	float sA = 1.0, sB = 1.0;
	if ( ussCsmFilter.x > 0.5 ) {
		if ( t < 1.0 )
			sA = ussVogel( a, posView, ussCsmBias[a], dPx, dPy );
		if ( t > 0.0 )
			sB = ussVogel( b, posView, ussCsmBias[b], dPx, dPy );
	} else {
		if ( t < 1.0 )
			sA = csmFiltered( a, posView, csmSplitOffset.z );
		if ( t > 0.0 )
			sB = csmFiltered( b, posView, csmSplitOffset.w );
	}
	float s = clamp( dot( posView, posView ) * csmParams.x * csmParams.x / ( csmParams.y * csmParams.y ), 0.0, 1.0 );
	float s2 = s * s;
	w = ( csmRed & 2 ) != 0 ? 1.0 : 1.0 - s2 * s2;
	return mix( sA, sB, t );
}

//! (C) the terrain's shadow-height map; inMap = 1 inside it
float ussTerrain( vec3 posView, out float inMap )
{
	inMap = 0.0;
	if ( ussTerrOn == 0 )
		return 1.0;
	vec3 wp = ( ussWorldFromView * vec4( posView, 1.0 ) ).xyz;
	vec2 uv = vec2( wp.x * ussTerrXform.x + ussTerrXform.y, wp.y * ussTerrXform.z + ussTerrXform.w );
	if ( any( lessThan( uv, vec2( 0.0 ) ) ) || any( greaterThan( uv, vec2( 1.0 ) ) ) )
		return 1.0;
	inMap = 1.0;
	// 0 = no shadow height (lit); a missing .y (the upper limb) falls to the range's floor
	vec2 rg = texture( ussTerrMap, uv ).rg;
	float lo = ussTerrRange.x + rg.x * ussTerrRange.y;
	float hi = ussTerrRange.x + rg.y * ussTerrRange.y;
	float z = wp.z + ussTerrRange.z;
	float vis = rg.x <= 0.0 ? 1.0 : ( lo - hi > 1e-3 ? clamp( ( z - hi ) / ( lo - hi ), 0.0, 1.0 ) : ( z >= lo ? 1.0 : 0.0 ) );
	vec2 e = min( uv, 1.0 - uv ) / max( ussTerrRange.w, 1e-6 );
	return mix( 1.0, vis, clamp( min( e.x, e.y ), 0.0, 1.0 ) );
}

//! (B) the contact mask at this pixel
float ussContact()
{
	if ( ussContactOn == 0 )
		return 1.0;
	return texture( ussContactMap, gl_FragCoord.xy * ussContactInvSize ).r;
}

//! min( mix( 1, cascades, fade ), terrain, contact ): each producer fails to lit; min never darkens twice
float ussCompose( vec3 posView )
{
	vec3 dPx = dFdx( posView ), dPy = dFdy( posView );
	float w, inMap;
	float casc = ussCascades( posView, dPx, dPy, w );
	float terr = ussTerrain( posView, inMap );
	float con = ussContact();
	if ( ( ussRed & 1 ) != 0 && w > 0.0 )
		terr = 1.0;	// red terrseam: the terrain only past the cascades' reach
	float c = mix( 1.0, casc, w );
	if ( ( ussRed & 2 ) != 0 )
		return c * terr * con;	// red mulcompose
	return min( min( c, terr ), con );
}

//! the sun's shadow factor: 1 lit, 0 fully shadowed
float wwSunShadow( vec3 posView )
{
	if ( ( csmRed & 8 ) != 0 )
		return 0.5;
	if ( csmForce >= 0.0 )
		return csmForce;
	if ( ussOn != 0 )
		return ussCompose( posView );
	float dv = -posView.z * csmParams.x;
	if ( dv > csmParams.y + csmParams.w )
		return 1.0;
	float sh = csmBlended( posView );
	if ( ( csmRed & 2 ) == 0 ) {
		float s = clamp( dot( posView, posView ) * csmParams.x * csmParams.x / ( csmParams.y * csmParams.y ), 0.0, 1.0 );
		float s2 = s * s;
		sh = 1.0 - ( 1.0 - sh ) * ( 1.0 - s2 * s2 );
	}
	return sh;
}

//! the probe views (WW_CSM_PROBE); false = draw normally. `shaded` is the display colour.
bool wwSunShadowProbe( vec3 posView, vec3 shaded, out vec3 outc )
{
	outc = shaded;
	if ( ussOn != 0 && ussView > 0 ) {
		// lane TERRSHADOW1: the debug ladder (raw grey, or the weights as numbers)
		vec3 dPx = dFdx( posView ), dPy = dFdy( posView );
		float w, inMap;
		float casc = ussCascades( posView, dPx, dPy, w );
		float terr = ussTerrain( posView, inMap );
		float con = ussContact();
		if ( ussView == 1 )
			outc = vec3( mix( 1.0, casc, w ) );
		else if ( ussView == 2 )
			outc = vec3( terr );
		else if ( ussView == 3 )
			outc = vec3( con );
		else if ( ussView == 4 )
			outc = vec3( ussCompose( posView ) );
		else
			outc = vec3( w, inMap, con );
		return true;
	}
	if ( csmProbe <= 0 )
		return false;
	float dv = -posView.z * csmParams.x;
	int a, b;
	float t;
	csmSelect( posView, a, b, t );
	if ( csmProbe == 1 ) {
		int i = t < 0.5 ? a : b;
		outc = vec3( dv > csmParams.y + csmParams.w ? 1.0 : csmHard( i, posView, i == a ? csmSplitOffset.z : csmSplitOffset.w ) );
	} else if ( csmProbe == 2 ) {
		const vec3 tint[3] = vec3[3]( vec3( 1.0, 0.25, 0.25 ), vec3( 0.25, 1.0, 0.25 ), vec3( 0.3, 0.45, 1.0 ) );
		vec3 c = dv > csmParams.y + csmParams.w ? vec3( 1.0 ) : mix( tint[a], tint[b], t );
		outc = shaded * c;
	} else if ( csmProbe == 3 ) {
		outc = vec3( dv > csmParams.y + csmParams.w ? 1.0 : csmBlended( posView ) );
	} else if ( csmProbe == 5 ) {
		// the selection as numbers: R = cascade A / 2, G = cascade B / 2, B = the blend weight;
		// past D + B all three are 1
		outc = dv > csmParams.y + csmParams.w ? vec3( 1.0 ) : vec3( float( a ) * 0.5, float( b ) * 0.5, t );
	} else {
		outc = vec3( wwSunShadow( posView ) );
	}
	return true;
}

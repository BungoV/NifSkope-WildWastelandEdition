/* Cascaded sun shadows, the receiver (lane CSM1; spec_cascaded_shadows.md 2.4 / 2.5).
 * Included only by programs that define WW_SUNSHADOW (pbrm_csm.frag,
 * lookdev_ground_csm.frag), so the default programs never carry a line of it.
 * Uploaded by gl/sunshadow.cpp wwSunShadowUniforms; the fit is echoed there. */

uniform sampler2DArrayShadow csmMap;
uniform mat4 csmMat[3];		// view space -> (u, v, depth) of cascade i
uniform vec3 csmInvRange;	// 1 / (far_i - 150): a world-unit offset in depth units
uniform vec4 csmParams;		// 1 / view scale, shadow distance D, map size, blend band B
uniform vec4 csmSplitOffset;	// the two pair boundaries (800, 3000), receiver offsets A / B (0.275, 1.0)
uniform int csmProbe;		// 0 off, 1 hard tap, 2 cascade colour, 3 filtered + blend, 4 final factor
uniform int csmRed;		// 1 noblend, 2 nofade, 4 diffonly, 8 factorhalf (the OFF leak)

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

//! the sun's shadow factor: 1 lit, 0 fully shadowed
float wwSunShadow( vec3 posView )
{
	if ( ( csmRed & 8 ) != 0 )
		return 0.5;
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
	} else {
		outc = vec3( wwSunShadow( posView ) );
	}
	return true;
}

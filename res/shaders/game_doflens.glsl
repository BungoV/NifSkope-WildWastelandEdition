/* lane DOFLENS1: the thin-lens depth of field (src/gl/cellpost.cpp, row WW_CELL_DOF_LENS). Shared by the prep pass
 * and the composites (game_doflens.frag, game_doflensmblur.frag).
 * lz = (near, far - near, far * near, sky sharp): the view distance from the GL window depth as game_dof.frag reads it.
 * lensP = (focus distance S1, <= 0 = the depth at the frame's centre; aperture A in sensor px; focal length f; -),
 * S1 and f in game units. lensQ = (max circle of confusion in px, near on, far on, -).
 * The circle of confusion (diameter, px) of a point at distance z: c = A f (z - S1) / (z (S1 - f)), > 0 behind the
 * focus (the far layer), < 0 in front (the near layer). The composite needs LENS_FRAME, LENS_FAR, LENS_DEPTH. */

uniform vec4 lz;
uniform vec4 lensP;
uniform vec4 lensQ;

float lensSat( float v )
{
	return v >= 0.0 ? min( v, 1.0 ) : 0.0;
}

// the view distance of a window depth; < 0 = the sky (no geometry)
float lensDist( float d )
{
	float x = 1.0 - d;
	return 0.00001 < x ? lz.z / ( x * lz.y + lz.x ) : -1.0;
}

// the focus distance: the record's, or the distance at the frame's centre (autofocus)
float lensFocus( sampler2D depthTex )
{
	float f = max( lensP.z, 0.001 );
	float s1 = lensP.x;
	if ( !( s1 > 0.0 ) ) {
		ivec2 c = textureSize( depthTex, 0 ) / 2;
		float z = lensDist( texelFetch( depthTex, c, 0 ).r );
		s1 = z > 0.0 ? z : 1e6;
	}
	return max( s1, f * 1.01 );
}

// the signed circle of confusion (px) at view distance z (< 0 = the sky), focus s1
float lensCoc( float z, float s1 )
{
	float f = max( lensP.z, 0.001 );
	float c;
	if ( z < 0.0 )
		c = lz.w != 0.0 ? 0.0 : lensP.y * f / ( s1 - f );	// the sky: sharp, or the lens's limit at infinity
	else
		c = lensP.y * f * ( z - s1 ) / ( z * ( s1 - f ) );
	if ( lensQ.z == 0.0 )
		c = min( c, 0.0 );
	if ( lensQ.y == 0.0 )
		c = max( c, 0.0 );
	return clamp( c, -lensQ.x, lensQ.x );
}

#ifdef LENS_FRAME
uniform sampler2D lensNear;	// the near layer, premultiplied (rgb * a, a)

float lensS1 = 0.0;	// set once in main: lensS1 = lensFocus( LENS_DEPTH )

// the lens composite at one full-size texel: the sharp frame, the far layer where the circle passes 1 px, the near
// layer over both; an in-focus texel with no near neighbour is the frame bit for bit
vec4 lensTexel( ivec2 p )
{
	vec4 a = texelFetch( LENS_FRAME, p, 0 );
	float cf = lensCoc( lensDist( texelFetch( LENS_DEPTH, p, 0 ).r ), lensS1 );
	vec2 uq = ( vec2( p ) + 0.5 ) / ( 4.0 * vec2( textureSize( LENS_FAR, 0 ) ) );
	vec3 m = a.rgb;
	if ( cf > 1.0 )
		m = mix( a.rgb, texture( LENS_FAR, uq ).rgb, lensSat( ( cf - 1.0 ) / 3.0 ) );
	if ( lensQ.y != 0.0 ) {
		vec4 n = texture( lensNear, uq );
		if ( n.a > 0.0 )
			m = m * ( 1.0 - n.a ) + n.rgb;
	}
	return vec4( m, a.a );
}
#endif

#version 410 core

/* lane AO1: the game's screen-space ambient obscurance for the cell view (src/gl/celllights.h,
 * docs/PRTP_PLAN.md "ambient obscurance"), read from the game's shader. Every buffer here is half the view and stored top row
 * first (the game's pixel order), except gbuf: the full-size opaque pass, bottom row first, holding the view
 * normal (GL view axes) and the linear depth in game units.
 *   stage 1  a depth mip: the min of 2x2 of the level above (mip 1 reads gbuf)
 *   stage 2  the raw obscurance: 5 taps over 2 turns, the radius in pixels = radius x 100 / depth, the mip by
 *            the tap's reach; a still view's value = the time average of the game's history over aoAngles
 *            of its per-frame angles
 *   stage 3  the bilateral blur across, stage 4 down: 7 taps 2 pixels apart, cut by the depth key */

uniform int aoStage;
uniform sampler2D gbuf;
uniform sampler2D zMip1;
uniform sampler2D zMip2;
uniform sampler2D zMip3;
uniform sampler2D zMip4;
uniform sampler2D src;
uniform ivec2 fullSize;
uniform ivec2 halfSize;
uniform ivec2 mipSize[5];	// [0] = fullSize
uniform int mipLevel;
uniform vec2 proj;			// the projection's P00, P11
uniform vec3 aoParams;		// radius, bias, intensity (game units)
uniform int aoAngles;
uniform bool aoNoBlur;		// WW_CELL_AO_RED=noblur: stages 3 and 4 copy
uniform bool aoNoReset;		// WW_CELL_AO_RED=noreset: the plain mean over the angles

out vec4 fragColor;

float zFull( ivec2 t )
{
	t = clamp( t, ivec2( 0 ), fullSize - 1 );
	return texelFetch( gbuf, ivec2( t.x, fullSize.y - 1 - t.y ), 0 ).a;
}

float zAt( int m, ivec2 t )
{
	if ( m == 0 )
		return zFull( t );
	t = clamp( t, ivec2( 0 ), mipSize[m] - 1 );
	if ( m == 1 )
		return texelFetch( zMip1, t, 0 ).r;
	if ( m == 2 )
		return texelFetch( zMip2, t, 0 ).r;
	if ( m == 3 )
		return texelFetch( zMip3, t, 0 ).r;
	return texelFetch( zMip4, t, 0 ).r;
}

// the texel of mip m a point sample at the centre of half-size pixel q reads ((q + 0.5) / halfSize x mip size)
ivec2 texelOf( int m, ivec2 q )
{
	ivec2 n = 2 * q + 1;
	ivec2 t = ( max( n, ivec2( 0 ) ) * mipSize[m] ) / ( 2 * halfSize );
	return ivec2( n.x < 0 ? -1 : t.x, n.y < 0 ? -1 : t.y );
}

void main()
{
	ivec2 P = ivec2( gl_FragCoord.xy );
	if ( aoStage == 1 ) {
		ivec2 p = P * 2;
		float z;
		if ( mipLevel == 1 ) {
			z = min( min( zFull( p ), zFull( p + ivec2( 1, 0 ) ) ), min( zFull( p + ivec2( 0, 1 ) ), zFull( p + ivec2( 1, 1 ) ) ) );
		} else {
			ivec2 s = mipSize[mipLevel - 1] - 1;
			z = min( min( texelFetch( src, min( p, s ), 0 ).r, texelFetch( src, min( p + ivec2( 1, 0 ), s ), 0 ).r ),
			         min( texelFetch( src, min( p + ivec2( 0, 1 ), s ), 0 ).r, texelFetch( src, min( p + ivec2( 1, 1 ), s ), 0 ).r ) );
		}
		fragColor = vec4( z, 0.0, 0.0, 1.0 );
		return;
	}
	if ( aoStage == 2 ) {
		vec2 hs = vec2( halfSize );
		vec4 projInfo = vec4( -2.0 / ( hs.x * proj.x ), -2.0 / ( hs.y * proj.y ), 1.0 / proj.x, 1.0 / proj.y );
		float z = zAt( 0, texelOf( 0, P ) );
		vec3 C = vec3( ( vec2( P ) + 0.5 ) * projInfo.xy + projInfo.zw, 1.0 ) * z;
		ivec2 nt = min( 2 * P, fullSize - 1 );
		vec3 ng = texelFetch( gbuf, ivec2( nt.x, fullSize.y - 1 - nt.y ), 0 ).xyz;
		vec3 n = vec3( -ng.x, ng.y, -ng.z );	// the game's axes: x left, y up, z forward
		float d = clamp( z * 1.42857141e-4, 0.0, 1.0 );	// z / 7000
		uint hx = uint( P.x ), hy = uint( P.y );
		float hashAngle = float( ( ( hx * 3u ) ^ ( hx * hy + hy ) ) * 10u );
		vec2 ndc = ( vec2( P ) + 0.5 ) / hs * 2.0 - 1.0;
		float biasP = aoParams.y + 10.0 * max( d - 0.3, 0.0 ) + 5.0 * dot( ndc, ndc );
		float r = aoParams.x;
		float r2 = r * r;
		float r6 = r2 * r;
		r6 *= r6;
		float ssDisk = r * 100.0 / z;
		// the game turns the pattern by a random angle in [0, pi) each frame (only up to depth 3500) and keeps
		// 0.99 of the history: a still view converges on the mean over that angle
		int K = d <= 0.5 ? max( aoAngles, 1 ) : 1;
		float A = 0.0, topSum = 0.0;
		int topN = 0;
		for ( int k = 0; k < K; k++ ) {
			float rnd = d <= 0.5 ? ( float( k ) + 0.5 ) * 3.14159265 / float( K ) : 0.0;
			precise float rot = rnd + hashAngle;
			float sum = 0.0;
			for ( int i = 0; i < 5; i++ ) {
				precise float reach = ssDisk * ( float( i ) + 0.5 );
				precise float ssR = reach * 0.2;
				precise float ang = fma( float( i ) + 0.5, 2.512, rot );
				// the angle can be ~5e6 radians: reduced exactly (doubles), as the math reads
				double ad = double( ang );
				float a = float( ad - 6.283185307179586LF * floor( ad * 0.15915494309189535LF ) );
				int m = clamp( int( floor( log2( ssR ) ) ) - 3, 0, 4 );
				ivec2 Q = P + ivec2( ssR * vec2( cos( a ), sin( a ) ) );
				float qz = zAt( m, texelOf( m, Q ) );
				vec3 v = vec3( ( vec2( Q ) + 0.5 ) * projInfo.xy + projInfo.zw, 1.0 ) * qz - C;
				float vv = dot( v, v );
				float f = max( r2 - vv, 0.0 );
				sum += f * f * f * max( ( dot( v, n ) - biasP ) / ( vv + 0.01 ), 0.0 );
			}
			float Ak = max( 1.0 - sum * aoParams.z / r6, 0.0 );
			A += Ak;
			if ( Ak >= 0.95 ) {
				topSum += Ak;
				topN++;
			}
		}
		A /= float( K );
		// the game's history starts over from the frame's value when that is 0.95 or more and the history is
		// under 0.7. Where the mean is under 0.7 and some angle reaches 0.95, the history saws between the two:
		// the fall from the restart to 0.7 (t1 frames at 0.99 a frame), then the wait for the next such angle
		// (chance p a frame). Its time average:
		if ( !aoNoReset && topN > 0 && A < 0.7 ) {
			const float lam = 0.01005034;	// -ln 0.99
			float p = float( topN ) / float( K ), top = topSum / float( topN );
			float t1 = log( ( top - A ) / ( 0.7 - A ) ) / lam;
			A += ( ( top - 0.7 ) / lam + ( 0.7 - A ) / ( p + lam ) ) / ( t1 + 1.0 / p );
		}
		// the blur's depth key, packed as the game packs it (256 d / 257)
		float hi = floor( d * 256.0 );
		float key = hi * 0.00390625 * 0.996108949 + ( d * 256.0 - hi ) * 0.00389105058;
		fragColor = vec4( A, key, 0.0, 1.0 );
		return;
	}
	// stages 3 and 4: the bilateral blur
	vec2 c = texelFetch( src, P, 0 ).rg;
	if ( aoNoBlur ) {
		fragColor = vec4( c, 0.0, 1.0 );
		return;
	}
	if ( c.y == 1.0 ) {
		fragColor = vec4( 0.0, c.y, 0.0, 1.0 );
		return;
	}
	ivec2 dir = aoStage == 3 ? ivec2( 1, 0 ) : ivec2( 0, 1 );
	const float wt[4] = float[4]( 0.15317, 0.444893, 0.422649, 0.392902 );
	float sum = c.x * wt[0];
	float ws = wt[0];
	for ( int j = 1; j <= 3; j++ ) {
		for ( int s = -1; s <= 1; s += 2 ) {
			ivec2 q = P + dir * ( 2 * j * s );
			vec2 t = vec2( 0.0 );	// past the edge the game's load reads 0 (depth key 0)
			if ( q.x >= 0 && q.y >= 0 && q.x < halfSize.x && q.y < halfSize.y )
				t = texelFetch( src, q, 0 ).rg;
			float w = max( 1.0 - abs( t.y - c.y ) * 2000.0, 0.0 ) * wt[j];
			sum += t.x * w;
			ws += w;
		}
	}
	fragColor = vec4( sum / ( ws + 0.0001 ), c.y, 0.0, 1.0 );
}

// lane WATERLOOK1 (src/gl/cellocean.h): the wind's wave spectrum on every baked water body, normals only.
// JONSWAP (Hasselmann et al. 1973) with Donelan-Banner spreading, plus the sea's swell or a lake's long ripples, as
// three FFT cascades a set (Tessendorf 2001), sea and fresh water; built by src/gl/cellocean.cpp each frame into
// waterOceanB / waterOceanC. Tiles are hidden by the triangle-grid random-offset blend (Heitz and Neyret 2018,
// Deliot and Heitz 2019), the slope variance the mips lose goes to the glare's roughness (LEAN, Olano and Baker 2010),
// and the unresolved tail below the finest cascade (Cox and Munk 1954 minus what the cascades hold) too.
uniform bool waterOceanOn;		// the Ocean waves row
uniform int waterOceanRed;		// 2 tile (plain repeats), 4 nopatch (no wind patches), 8 novar (no lost variance), 16 nochop
uniform sampler2DArray waterOceanB;	// 6 layers ( sea 0-2, fresh 3-5 ): ( sx, sy, Dxx, Dyy ), mipmapped
uniform sampler2DArray waterOceanC;	// ( sx^2, sy^2, sx sy, Dxy ), mipmapped
uniform vec4 waterOceanInvL[2];	// per set: 1 / tile size of cascades 0..2 (1/m), the chop lambda
uniform vec4 waterOceanSet[2];	// per set: Hs of its tiles (m), their wind sea's Hs (m), the swell / ripple Hs (m), tail mss
uniform vec4 waterOceanSetB[2];	// per set: the tiles' fetch (m), resolved curvature rms (1/m), patch cell (u), 0
uniform vec4 waterOceanWind;	// the waves' travel direction x, y (unit), the wind (m/s), the caustic net's curvature (1/m)
uniform vec4 waterOceanPatch;	// the wind patches' drift, sea x, y, fresh x, y (u, wrapped by their period)

const float kOcMetre = 0.0142875;	// m a game unit
const float kOcGrid = 2.3255814;	// triangle grid cells a tile (1 / 0.43)
const float kOcPatchK = 1.602095;	// E[ m^2 ] = 1 (notes/waterlook1/tools/patch_k.py)

struct WwOcean
{
	vec2 slope;	// the normal's xy / z (-dh/dx, -dh/dy)
	float var;	// slope variance (x + y) the pixel cannot show: the mips' loss and the tail
	float w;	// how much of the pixel is a baked body (0: today's water)
	float white;	// whitecap cover
	float cau;	// the caustic net's path factor (curvature / the net's)
};

uint wwOcMix( uint x )
{
	x ^= x >> 16;
	x *= 0x7feb352du;
	x ^= x >> 15;
	x *= 0x846ca68bu;
	x ^= x >> 16;
	return x;
}

float wwOcHash01( ivec2 c, uint seed )
{
	uvec2 u = uvec2( c & 1023 );
	return float( wwOcMix( u.x * 0x8da6b343u ^ u.y * 0xd8163841u ^ seed ) ) * ( 1.0 / 4294967296.0 );
}

float wwOcValue( vec2 p, uint seed )
{
	vec2 i = floor( p );
	vec2 f = p - i;
	f = f * f * ( 3.0 - 2.0 * f );
	ivec2 c = ivec2( i );
	float a = wwOcHash01( c, seed ), b = wwOcHash01( c + ivec2( 1, 0 ), seed );
	float d = wwOcHash01( c + ivec2( 0, 1 ), seed ), e = wwOcHash01( c + ivec2( 1, 1 ), seed );
	return mix( mix( a, b, f.x ), mix( d, e, f.x ), f.y );
}

// the wind patch factor (mean square 1): glassy where the gusts do not reach, cat's-paws where they do
float wwOcPatch( vec2 P, int set )
{
	vec2 p = ( P + ( set == 0 ? waterOceanPatch.xy : waterOceanPatch.zw ) ) / waterOceanSetB[set].z;
	float n = 0.65 * wwOcValue( p, 0x2545f491u ) + 0.35 * wwOcValue( 2.0 * p + vec2( 17.0, 31.0 ), 0x9e3779b9u );
	return kOcPatchK * smoothstep( 0.30, 0.70, n );
}

vec2 wwOcOffset( ivec2 v, uint seed )
{
	uint h = wwOcMix( uint( v.x ) * 0x8da6b343u ^ uint( v.y ) * 0xd8163841u ^ seed );
	uint h2 = wwOcMix( h ^ 0x68e31da4u );
	return vec2( float( h ), float( h2 ) ) * ( 1.0 / 4294967296.0 );
}

/* one cascade at tile uv: the blend of three randomly shifted copies on a triangle grid, weights normalised to keep
 * the variance (S = sum w S_i / sqrt( sum w^2 )); out B ( sx, sy, Dxx, Dyy ), Dxy, the lost slope variance */
vec4 wwOcCascade( vec2 uv, vec2 gx, vec2 gy, float layer, uint seed, out float dxy, out float lost )
{
	if ( ( waterOceanRed & 2 ) != 0 ) {
		vec4 B = textureGrad( waterOceanB, vec3( uv, layer ), gx, gy );
		vec4 C = textureGrad( waterOceanC, vec3( uv, layer ), gx, gy );
		dxy = C.w;
		lost = max( C.x - B.x * B.x, 0.0 ) + max( C.y - B.y * B.y, 0.0 );
		return B;
	}
	vec2 s = vec2( uv.x, -0.57735027 * uv.x + 1.15470054 * uv.y ) * kOcGrid;
	ivec2 b = ivec2( floor( s ) );
	vec2 f = s - vec2( b );
	float z = 1.0 - f.x - f.y;
	vec3 w;
	ivec2 v0, v1, v2;
	if ( z > 0.0 ) {
		w = vec3( z, f.y, f.x );
		v0 = b;
		v1 = b + ivec2( 0, 1 );
		v2 = b + ivec2( 1, 0 );
	} else {
		w = vec3( -z, 1.0 - f.y, 1.0 - f.x );
		v0 = b + ivec2( 1, 1 );
		v1 = b + ivec2( 1, 0 );
		v2 = b + ivec2( 0, 1 );
	}
	vec4 B0 = textureGrad( waterOceanB, vec3( uv + wwOcOffset( v0, seed ), layer ), gx, gy );
	vec4 B1 = textureGrad( waterOceanB, vec3( uv + wwOcOffset( v1, seed ), layer ), gx, gy );
	vec4 B2 = textureGrad( waterOceanB, vec3( uv + wwOcOffset( v2, seed ), layer ), gx, gy );
	vec4 C0 = textureGrad( waterOceanC, vec3( uv + wwOcOffset( v0, seed ), layer ), gx, gy );
	vec4 C1 = textureGrad( waterOceanC, vec3( uv + wwOcOffset( v1, seed ), layer ), gx, gy );
	vec4 C2 = textureGrad( waterOceanC, vec3( uv + wwOcOffset( v2, seed ), layer ), gx, gy );
	float n2 = dot( w, w );
	float rn = inversesqrt( max( n2, 1e-6 ) );
	dxy = ( w.x * C0.w + w.y * C1.w + w.z * C2.w ) * rn;
	vec3 l = vec3( max( C0.x - B0.x * B0.x, 0.0 ) + max( C0.y - B0.y * B0.y, 0.0 ),
		max( C1.x - B1.x * B1.x, 0.0 ) + max( C1.y - B1.y * B1.y, 0.0 ),
		max( C2.x - B2.x * B2.x, 0.0 ) + max( C2.y - B2.y * B2.y, 0.0 ) );
	lost = dot( w * w, l ) / max( n2, 1e-6 );
	return ( w.x * B0 + w.y * B1 + w.z * B2 ) * rn;
}

// the wind sea's Hs at fetch F (m): fetch-limited, capped at fully developed (the foam's law, WATERFOAMDETAIL1)
float wwOcHw( float U, float F )
{
	return min( 0.0016 * U * sqrt( F / 9.81 ), 0.21 * U * U / 9.81 );
}

/* the spectrum at world xy P (u); dPx, dPy its screen derivatives. The body from the 4 texels around P as the swell
 * reads it (water_waves.glsl): the sea takes the sea set, rivers and lakes the fresh one, each scaled by its own fetch
 * (sqrt of its baked area; a river's is its width, 40 m); tiny bodies, placed and interior water get w = 0. */
WwOcean wwWaterOcean( vec2 P, vec2 dPx, vec2 dPy )
{
	WwOcean o = WwOcean( vec2( 0.0 ), 0.0, 0.0, 0.0, 1.0 );
	float tex = waterInfoGrid.z;
	vec2 g = ( P - waterInfoGrid.xy ) / tex - 0.5;
	vec2 t0 = floor( g );
	vec2 f = g - t0;
	vec2 c0 = waterInfoGrid.xy + ( t0 + 0.5 ) * tex;
	uvec4 wd = uvec4( wwWaterInfoWord( c0 ), wwWaterInfoWord( c0 + vec2( tex, 0.0 ) ),
		wwWaterInfoWord( c0 + vec2( 0.0, tex ) ), wwWaterInfoWord( c0 + vec2( tex, tex ) ) );
	vec4 bw = vec4( ( 1.0 - f.x ) * ( 1.0 - f.y ), f.x * ( 1.0 - f.y ), ( 1.0 - f.x ) * f.y, f.x * f.y );
	uvec4 id = wd & 0xFFFFu;
	float U = waterOceanWind.z;
	// each wet corner's set and gain; the set of the largest weight wins, its corners' gains blend
	vec4 cset = vec4( -1.0 ), cgain = vec4( 0.0 );
	float best = -1.0;
	int set = -1;
	for ( int k = 0; k < 4; k++ ) {
		if ( id[k] == 0u )
			continue;
		vec4 row = texelFetch( waterInfoBodies, ivec2( int( id[k] % 256u ), int( id[k] / 256u ) ), 0 );
		int cls = clamp( int( row.g + 0.5 ), 0, 2 );
		if ( ( int( row.b + 0.5 ) & 16 ) != 0 )
			continue;	// tiny: today's water
		int s = cls == 0 ? 0 : 1;
		float F = sqrt( max( row.a, 1.0 ) ) * tex * kOcMetre;
		if ( cls == 1 )
			F = min( F, 40.0 );
		vec4 S = waterOceanSet[s];
		float hw = S.y * wwOcHw( U, F ) / max( wwOcHw( U, waterOceanSetB[s].x ), 1e-6 );
		cset[k] = float( s );
		cgain[k] = clamp( sqrt( hw * hw + S.z * S.z ) / max( S.x, 1e-6 ), 0.15, 3.0 );
		if ( bw[k] > best ) {
			best = bw[k];
			set = s;
		}
	}
	if ( set < 0 )
		return o;
	vec4 same = vec4( equal( cset, vec4( float( set ) ) ) );
	float ws = dot( bw, same );
	o.w = ws;
	float a = dot( bw * same, cgain ) / max( ws, 1e-6 );

	// the tiles run along the wind: rotate world metres into them, and the slopes back
	vec2 d = waterOceanWind.xy, e = vec2( -d.y, d.x );
	vec2 Pm = P * kOcMetre, gxm = dPx * kOcMetre, gym = dPy * kOcMetre;
	vec2 q = vec2( dot( Pm, d ), dot( Pm, e ) ), qx = vec2( dot( gxm, d ), dot( gxm, e ) ), qy = vec2( dot( gym, d ), dot( gym, e ) );
	float m = ( waterOceanRed & 4 ) != 0 ? 1.0 : wwOcPatch( P, set );
	float pw = ( waterOceanRed & 4 ) != 0 ? 0.0 : 1.0 - 0.7 * clamp( ( U - 3.0 ) / 7.0, 0.0, 1.0 );
	vec3 gc = vec3( 1.0, mix( 1.0, m, 0.5 * pw ), mix( 1.0, m, pw ) );
	vec4 iL = waterOceanInvL[set];
	vec4 B = vec4( 0.0 );
	float dxy = 0.0, var = 0.0;
	for ( int c = 0; c < 3; c++ ) {
		float il = iL[c], dc, lc;
		vec4 Bc = wwOcCascade( q * il, qx * il, qy * il, float( set * 3 + c ), 0x1b873593u * uint( set * 3 + c + 1 ), dc, lc );
		B += gc[c] * Bc;
		dxy += gc[c] * dc;
		var += gc[c] * gc[c] * lc;
	}
	B *= a;
	dxy *= a;
	var = ( waterOceanRed & 8 ) != 0 ? 0.0 : a * a * ( var + waterOceanSet[set].w * mix( 1.0, m * m, pw ) );
	float lam = ( waterOceanRed & 16 ) != 0 ? 0.0 : iL.w;
	float jx = 1.0 + lam * B.z, jy = 1.0 + lam * B.w;
	vec2 st = vec2( B.x / max( jx, 0.2 ), B.y / max( jy, 0.2 ) );
	o.slope = -( st.x * d + st.y * e );
	o.var = var;
	float J = jx * jy - lam * lam * dxy * dxy;
	o.white = ( 1.0 - smoothstep( 0.15, 0.55, J ) ) * smoothstep( 4.0, 8.0, U ) * ws;
	o.cau = clamp( a * waterOceanSetB[set].y * gc.z / max( waterOceanWind.w, 1e-3 ), 0.0, 3.0 );
	return o;
}

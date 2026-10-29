// Lane RTXGI1 M2 (2026-10-29): the ray-traced GI volume's read at a surface (src/rtddgicell.h, notes/rtxgi1/GUIDE.md 3).
// MIRROR rtddgimath.h sampleVolume, line for line: change both or neither. Included by cell_lights.glsl only under
// WW_CELL_RTDDGI (the loader's define while the Ray-traced GI row / WW_CELL_RTDDGI is on), so the off programs do not
// hold a line of it. tests/rtddgi_math includes this same file behind a small GLSL shim and checks it against the
// header on random volumes: so the text keeps to a subset C++ also reads (no swizzles, no out parameters, no vec2, int
// helpers of its own, scalar maths written out).
//
// cellDdgiAtlas, RGBA32F, one texture buffer, the GPU's buffers as they come back (src/rtddgi/rtddgi_vk.inc):
//   texels [0, 64 nP)            irradiance: probe p's 8 x 8 tile at 64 p (encoded rgb, a = sky visibility)
//   texels [64 nP, 192 nP)       distance: probe p's 16 x 16 (mean, mean^2) pairs at 64 nP + 128 p, two a texel
//   texels [192 nP, 193 nP)      probes: offset xyz, w = the state when usable (Vigilant, blended), else 0 (Off)

uniform bool cellDdgiOn;			// a volume is up for this document
uniform samplerBuffer cellDdgiAtlas;
uniform vec4 cellDdgiOrigin;		// the first probe (world), w = D, the smallest spacing
uniform vec4 cellDdgiStep;			// the spacing per axis, w = B, the self-shadow bias
uniform ivec4 cellDdgiDims;			// probes per axis, w = nP
uniform int cellDdgiRed;			// rtddgimath.h kSample*: 1 trilinear (WW_CELL_RTDDGI_RED), 2 nocheb, 4 nobias

struct CellDdgiOut
{
	vec3 E;		// irradiance (decoded x 2 pi)
	float vis;	// sky visibility
	float w;	// the weights' sum: 0 = no usable probe
	float fade;	// 1 a cell inside the probe box, 0 at its faces and past them
};

int cellDdgiIMin( int a, int b ) { return a < b ? a : b; }
int cellDdgiIMax( int a, int b ) { return a > b ? a : b; }
float cellDdgiSignNotZero( float v ) { return v >= 0.0 ? 1.0 : -1.0; }

// MIRROR rtddgimath.h sampleTile (octEncode inside): probe p's tile at unit direction d, S = 8 irradiance, 16 distance
vec4 cellDdgiTile( int p, vec3 d, int S )
{
	float l1 = abs( d.x ) + abs( d.y ) + abs( d.z );
	float u = d.x / l1;
	float v = d.y / l1;
	if ( d.z < 0.0 ) {
		float ou = u;
		u = ( 1.0 - abs( v ) ) * cellDdgiSignNotZero( ou );
		v = ( 1.0 - abs( ou ) ) * cellDdgiSignNotZero( v );
	}
	int n = S - 2;
	float fx = 1.0 + ( u * 0.5 + 0.5 ) * float( n ) - 0.5;
	float fy = 1.0 + ( v * 0.5 + 0.5 ) * float( n ) - 0.5;
	int x0 = cellDdgiIMin( cellDdgiIMax( int( floor( fx ) ), 0 ), S - 2 );
	int y0 = cellDdgiIMin( cellDdgiIMax( int( floor( fy ) ), 0 ), S - 2 );
	float ax = clamp( fx - float( x0 ), 0.0, 1.0 );
	float ay = clamp( fy - float( y0 ), 0.0, 1.0 );
	vec4 t[4];
	for ( int k = 0; k < 4; k++ ) {
		int x = x0 + k % 2;
		int y = y0 + k / 2;
		if ( S == 8 ) {
			t[k] = texelFetch( cellDdgiAtlas, 64 * p + y * 8 + x );
		} else {
			int i = y * 16 + x;
			vec4 q = texelFetch( cellDdgiAtlas, 64 * cellDdgiDims.w + 128 * p + i / 2 );
			t[k] = i % 2 == 0 ? vec4( q.x, q.y, 0.0, 0.0 ) : vec4( q.z, q.w, 0.0, 0.0 );
		}
	}
	// ( a (1 - ax) + b ax ) (1 - ay) + ( e (1 - ax) + f ax ) ay, as the header writes it
	return ( t[0] * ( 1.0 - ax ) + t[1] * ax ) * ( 1.0 - ay ) + ( t[2] * ( 1.0 - ax ) + t[3] * ax ) * ay;
}

// MIRROR rtddgimath.h chebyshevWeight
float cellDdgiChebyshev( float r, float mean, float mean2 )
{
	if ( r <= mean )
		return 1.0;
	float var = abs( mean * mean - mean2 );
	float dd = r - mean;
	float c = var / max( var + dd * dd, 1e-12 );
	c = c * c * c;
	return max( 0.05, max( c, 0.0 ) );
}

// MIRROR rtddgimath.h sampleVolume: P surface point (world), n its unit normal, o unit direction to the viewer, d the
// direction the tiles are read at (n for the irradiance; the reflection lobe for the specular's sky visibility)
CellDdgiOut cellDdgiRead( vec3 P, vec3 n, vec3 o, vec3 d )
{
	CellDdgiOut r;
	r.E = vec3( 0.0 );
	r.vis = 0.0;
	r.w = 0.0;
	r.fade = 0.0;
	vec3 Pb = P;
	if ( ( cellDdgiRed & 4 ) == 0 )
		Pb = Pb + ( n * 0.2 + o * 0.8 ) * ( 0.75 * cellDdgiOrigin.w * cellDdgiStep.w );
	float gx = ( Pb.x - cellDdgiOrigin.x ) / cellDdgiStep.x;
	float gy = ( Pb.y - cellDdgiOrigin.y ) / cellDdgiStep.y;
	float gz = ( Pb.z - cellDdgiOrigin.z ) / cellDdgiStep.z;
	int bx = cellDdgiIMin( cellDdgiIMax( int( floor( gx ) ), 0 ), cellDdgiIMax( cellDdgiDims.x - 2, 0 ) );
	int by = cellDdgiIMin( cellDdgiIMax( int( floor( gy ) ), 0 ), cellDdgiIMax( cellDdgiDims.y - 2, 0 ) );
	int bz = cellDdgiIMin( cellDdgiIMax( int( floor( gz ) ), 0 ), cellDdgiIMax( cellDdgiDims.z - 2, 0 ) );
	float ax = clamp( gx - float( bx ), 0.0, 1.0 );
	float ay = clamp( gy - float( by ), 0.0, 1.0 );
	float az = clamp( gz - float( bz ), 0.0, 1.0 );
	float fade = 1.0;
	fade = min( fade, clamp( min( gx, float( cellDdgiDims.x - 1 ) - gx ), 0.0, 1.0 ) );
	fade = min( fade, clamp( min( gy, float( cellDdgiDims.y - 1 ) - gy ), 0.0, 1.0 ) );
	fade = min( fade, clamp( min( gz, float( cellDdgiDims.z - 1 ) - gz ), 0.0, 1.0 ) );
	r.fade = fade;
	vec3 sum = vec3( 0.0 );
	float vsum = 0.0;
	float wsum = 0.0;
	for ( int c = 0; c < cellGiLoop8; c++ ) {	// cell_lights.glsl's uniform bound: one body, never unrolled
		int ox = c % 2;
		int oy = ( c / 2 ) % 2;
		int oz = c / 4;
		int px = cellDdgiIMin( bx + ox, cellDdgiDims.x - 1 );
		int py = cellDdgiIMin( by + oy, cellDdgiDims.y - 1 );
		int pz = cellDdgiIMin( bz + oz, cellDdgiDims.z - 1 );
		int p = px + cellDdgiDims.x * ( py + cellDdgiDims.y * pz );
		vec4 pr = texelFetch( cellDdgiAtlas, 192 * cellDdgiDims.w + p );
		if ( int( pr.w + 0.5 ) != 2 )
			continue;	// rtddgimath.h probeUsable: the CPU writes 0 for a probe that is not
		float tri = ( ox == 1 ? ax : 1.0 - ax ) * ( oy == 1 ? ay : 1.0 - ay ) * ( oz == 1 ? az : 1.0 - az );
		vec3 pp = vec3( cellDdgiOrigin.x + float( px ) * cellDdgiStep.x + pr.x, cellDdgiOrigin.y + float( py ) * cellDdgiStep.y + pr.y,
			cellDdgiOrigin.z + float( pz ) * cellDdgiStep.z + pr.z );
		float w = 1.0;
		if ( ( cellDdgiRed & 1 ) == 0 ) {
			float cb = ( dot( normalize( pp - P ), n ) + 1.0 ) * 0.5;	// MIRROR rtddgimath.h backfaceWeight
			w = cb * cb + 0.2;
			vec3 bp = pp - Pb;
			float rr = length( bp );
			if ( ( cellDdgiRed & 2 ) == 0 && rr > 0.0 ) {
				vec4 m = cellDdgiTile( p, bp * ( -1.0 / rr ), 16 );
				w *= cellDdgiChebyshev( rr, m.x, m.y );
			}
			w = max( w, 1e-6 );
			w = w < 0.2 ? w * w * w / ( 0.2 * 0.2 ) : w;	// MIRROR rtddgimath.h crushWeight
		}
		w *= tri;
		vec4 e = cellDdgiTile( p, d, 8 );
		sum = sum + vec3( pow( max( e.x, 0.0 ), 2.5 ), pow( max( e.y, 0.0 ), 2.5 ), pow( max( e.z, 0.0 ), 2.5 ) ) * w;
		vsum += w * e.w;
		wsum += w;
	}
	if ( wsum <= 0.0 )
		return r;
	vec3 m = sum * ( 1.0 / wsum );
	r.E = m * m * 6.28318530717958647692;
	r.vis = vsum / wsum;
	r.w = wsum;
	return r;
}

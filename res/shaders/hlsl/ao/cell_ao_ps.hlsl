// HLSL source of features/ao/cell_ao.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "AoBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
/* The game's screen-space ambient obscurance for the cell view (src/gl/celllights.h). Buffers are half size, top
 * row first, except gbuf (full size, bottom row first: view normal and linear depth in game units).
 * Stage 5 (MODULES1) writes gbuf from the depth pre-pass's resolve (src/features/ao/ao.cpp wwCellAoPass).
 * Stages: 1 depth mip (min of 2x2), 2 raw obscurance (5 taps over 2 turns, averaged over aoAngles angles),
 * 3 and 4 the bilateral blur across and down (7 taps 2 pixels apart, cut by the depth key). */

cbuffer LooseUniforms : register(b31, space0)
{
	int aoStage;
	int2 fullSize;
	int2 halfSize;
	int2 mipSize[5];	// [0] = fullSize
	int mipLevel;
	float2 proj;			// the projection's P00, P11
	float3 aoParams;		// radius, bias, intensity (game units)
	int aoAngles;
	bool aoNoBlur;		// WW_CELL_AO_RED=noblur: stages 3 and 4 copy
	bool aoNoReset;		// WW_CELL_AO_RED=noreset: the plain mean over the angles
	row_major float4x4 aoInvProj;		// MODULES1: stage 5, clip -> view
	float aoUnit;		// MODULES1: stage 5, view units -> game units (cell_ao.glsl cellAoPassOut)
	bool aoZLin;		// RESUME 15: stage 5 reads zlin (else the window depth zres, part 14 / red zwin)
};

Texture2D<float4> gbuf : register(t0, space0); SamplerState _gbuf_sampler : register(s0, space0);
Texture2D<float4> zMip1 : register(t1, space0); SamplerState _zMip1_sampler : register(s1, space0);
Texture2D<float4> zMip2 : register(t2, space0); SamplerState _zMip2_sampler : register(s2, space0);
Texture2D<float4> zMip3 : register(t3, space0); SamplerState _zMip3_sampler : register(s3, space0);
Texture2D<float4> zMip4 : register(t4, space0); SamplerState _zMip4_sampler : register(s4, space0);
Texture2D<float4> src : register(t5, space0); SamplerState _src_sampler : register(s5, space0);
Texture2D<float4> zres : register(t6, space0); SamplerState _zres_sampler : register(s6, space0);		// MODULES1: stage 5, the depth pre-pass's resolve (window depth, bottom row first)
Texture2D<float4> zlin : register(t7, space0); SamplerState _zlin_sampler : register(s7, space0);		// RESUME 15: stage 5, the pre-pass's linear view depth (game units, 0 = nothing drawn)

static float4 fragColor;

float zFull( int2 t )
{
	t = clamp( t, (int2)( 0 ), fullSize - 1 );
	return gbuf.Load( int3(int2( t.x, fullSize.y - 1 - t.y ), 0) ).a;
}

float zAt( int m, int2 t )
{
	if ( m == 0 )
		return zFull( t );
	t = clamp( t, (int2)( 0 ), mipSize[m] - 1 );
	if ( m == 1 )
		return zMip1.Load( int3(t, 0) ).r;
	if ( m == 2 )
		return zMip2.Load( int3(t, 0) ).r;
	if ( m == 3 )
		return zMip3.Load( int3(t, 0) ).r;
	return zMip4.Load( int3(t, 0) ).r;
}

// the texel of mip m a point sample at the center of half-size pixel q reads ((q + 0.5) / halfSize x mip size)
int2 texelOf( int m, int2 q )
{
	int2 n = 2 * q + 1;
	int2 t = ( max( n, (int2)( 0 ) ) * mipSize[m] ) / ( 2 * halfSize );
	return int2( n.x < 0 ? -1 : t.x, n.y < 0 ? -1 : t.y );
}

// MODULES1: the view position at full-size pixel t from the pre-pass's depth; w = 0 where nothing was drawn (the sky)
float4 zPos( int2 t )
{
	t = clamp( t, (int2)( 0 ), fullSize - 1 );
	float2 ndc = ( float2( t ) + 0.5 ) / float2( fullSize ) * 2.0 - 1.0;
	if ( aoZLin ) {	// RESUME 15: the pixel's eye ray scaled to the stored view depth (McGuire et al. 2012, camera-space z)
		float zl = zlin.Load( int3(t, 0) ).r;
		float4 r = mul(float4( ndc, 0.0, 1.0 ), aoInvProj);
		float3 q = r.xyz / r.w;
		return float4( q * ( -zl / ( aoUnit * q.z ) ), zl > 0.0 ? 1.0 : 0.0 );
	}
	float d = zres.Load( int3(t, 0) ).r;
	float4 v = mul(float4( ndc, d * 2.0 - 1.0, 1.0 ), aoInvProj);
	return float4( v.xyz / v.w, d < 1.0 ? 1.0 : 0.0 );
}

void frag_main()
{
	int2 P = int2( gl_FragCoord.xy );
	if ( aoStage == 5 ) {
		/* MODULES1: the opaque pass's view normal and linear depth, rebuilt from the depth pre-pass. Each axis takes
		 * the side of its 3-tap cross with the smaller depth step (McGuire et al. 2012, SAO), so a silhouette's far
		 * side never bends the normal; a side on the sky is never taken. Sky: the clear (0, 0, 0, 1e6). */
		float4 c = zPos( P );
		if ( c.w < 0.5 ) {
			fragColor = float4( 0.0, 0.0, 0.0, 1.0e6 );
			return;
		}
		float4 l = zPos( P - int2( 1, 0 ) ), r = zPos( P + int2( 1, 0 ) );
		float4 b = zPos( P - int2( 0, 1 ) ), u = zPos( P + int2( 0, 1 ) );
		float3 dl = c.xyz - l.xyz, dr = r.xyz - c.xyz, db = c.xyz - b.xyz, du = u.xyz - c.xyz;
		float el = l.w > 0.5 ? abs( dl.z ) : 1.0e30, er = r.w > 0.5 ? abs( dr.z ) : 1.0e30;
		float eb = b.w > 0.5 ? abs( db.z ) : 1.0e30, eu = u.w > 0.5 ? abs( du.z ) : 1.0e30;
		float3 dx = min( el, er ) >= 1.0e30 ? float3( 1.0, 0.0, 0.0 ) : el < er ? dl : dr;
		float3 dy = min( eb, eu ) >= 1.0e30 ? float3( 0.0, 1.0, 0.0 ) : eb < eu ? db : du;
		float3 n = cross( dx, dy );
		n = dot( n, n ) > 0.0 ? normalize( n ) : float3( 0.0, 0.0, 1.0 );
		if ( dot( n, c.xyz ) > 0.0 )
			n = -n;	// toward the eye, as the geometry pass flips a back face
		fragColor = float4( n, -c.z * aoUnit );
		return;
	}
	if ( aoStage == 1 ) {
		int2 p = P * 2;
		float z;
		if ( mipLevel == 1 ) {
			z = min( min( zFull( p ), zFull( p + int2( 1, 0 ) ) ), min( zFull( p + int2( 0, 1 ) ), zFull( p + int2( 1, 1 ) ) ) );
		} else {
			int2 s = mipSize[mipLevel - 1] - 1;
			z = min( min( src.Load( int3(min( p, s ), 0) ).r, src.Load( int3(min( p + int2( 1, 0 ), s ), 0) ).r ),
			         min( src.Load( int3(min( p + int2( 0, 1 ), s ), 0) ).r, src.Load( int3(min( p + int2( 1, 1 ), s ), 0) ).r ) );
		}
		fragColor = float4( z, 0.0, 0.0, 1.0 );
		return;
	}
	if ( aoStage == 2 ) {
		float2 hs = float2( halfSize );
		float4 projInfo = float4( -2.0 / ( hs.x * proj.x ), -2.0 / ( hs.y * proj.y ), 1.0 / proj.x, 1.0 / proj.y );
		float z = zAt( 0, texelOf( 0, P ) );
		float3 C = float3( ( float2( P ) + 0.5 ) * projInfo.xy + projInfo.zw, 1.0 ) * z;
		int2 nt = min( 2 * P, fullSize - 1 );
		float3 ng = gbuf.Load( int3(int2( nt.x, fullSize.y - 1 - nt.y ), 0) ).xyz;
		float3 n = float3( -ng.x, ng.y, -ng.z );	// the game's axes: x left, y up, z forward
		float d = clamp( z * 1.42857141e-4, 0.0, 1.0 );	// z / 7000
		uint hx = uint( P.x ), hy = uint( P.y );
		float hashAngle = float( ( ( hx * 3u ) ^ ( hx * hy + hy ) ) * 10u );
		float2 ndc = ( float2( P ) + 0.5 ) / hs * 2.0 - 1.0;
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
				precise float ang = mad( float( i ) + 0.5, 2.512, rot );
				// the angle can be ~5e6 radians: reduced exactly (doubles), as the math reads
				double ad = double( ang );
				float a = float( ad - 6.283185307179586L * floor( ad * 0.15915494309189535L ) );
				int m = clamp( int( floor( log2( ssR ) ) ) - 3, 0, 4 );
				int2 Q = P + int2( ssR * float2( cos( a ), sin( a ) ) );
				float qz = zAt( m, texelOf( m, Q ) );
				float3 v = float3( ( float2( Q ) + 0.5 ) * projInfo.xy + projInfo.zw, 1.0 ) * qz - C;
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
		// the game restarts its history from a frame value >= 0.95 when the history is under 0.7, so it saws
		// between the two; this is that sawtooth's time average (fall at 0.99 a frame, restart chance p a frame)
		if ( !aoNoReset && topN > 0 && A < 0.7 ) {
			const float lam = 0.01005034;	// -ln 0.99
			float p = float( topN ) / float( K ), top = topSum / float( topN );
			float t1 = log( ( top - A ) / ( 0.7 - A ) ) / lam;
			A += ( ( top - 0.7 ) / lam + ( 0.7 - A ) / ( p + lam ) ) / ( t1 + 1.0 / p );
		}
		// the blur's depth key, packed as the game packs it (256 d / 257)
		float hi = floor( d * 256.0 );
		float key = hi * 0.00390625 * 0.996108949 + ( d * 256.0 - hi ) * 0.00389105058;
		fragColor = float4( A, key, 0.0, 1.0 );
		return;
	}
	// stages 3 and 4: the bilateral blur
	float2 c = src.Load( int3(P, 0) ).rg;
	if ( aoNoBlur ) {
		fragColor = float4( c, 0.0, 1.0 );
		return;
	}
	if ( c.y == 1.0 ) {
		fragColor = float4( 0.0, c.y, 0.0, 1.0 );
		return;
	}
	int2 dir = aoStage == 3 ? int2( 1, 0 ) : int2( 0, 1 );
	const float wt[4] = { 0.15317, 0.444893, 0.422649, 0.392902 };
	float sum = c.x * wt[0];
	float ws = wt[0];
	for ( int j = 1; j <= 3; j++ ) {
		for ( int s = -1; s <= 1; s += 2 ) {
			int2 q = P + dir * ( 2 * j * s );
			float2 t = (float2)( 0.0 );	// past the edge the game's load reads 0 (depth key 0)
			if ( q.x >= 0 && q.y >= 0 && q.x < halfSize.x && q.y < halfSize.y )
				t = src.Load( int3(q, 0) ).rg;
			float w = max( 1.0 - abs( t.y - c.y ) * 2000.0, 0.0 ) * wt[j];
			sum += t.x * w;
			ws += w;
		}
	}
	fragColor = float4( sum / ( ws + 0.0001 ), c.y, 0.0, 1.0 );
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}

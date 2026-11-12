// The game's depth of field and motion blur, and the thin lens (lane VKFRAME1 P7, 2026-11-12): the math of game_dof_down /
// game_dof_blur / game_dof / game_mblur / game_dofmblur and game_doflens*.frag (src/features/post/cellpost.cpp), shared by
// NifSkope's Vulkan kernels (PostFxBindingsVk.hlsli) and Fusion Shaders. Bindings-free: textures, samplers and values come in
// as arguments. Every expression keeps the GL twin's order (a reorder costs an ulp).
#ifndef DOF_COMMON_HLSLI
#define DOF_COMMON_HLSLI

float F4FX_PostSat( float v )	// D3D _sat: NaN -> 0
{
	return v >= 0.0f ? min( v, 1.0f ) : 0.0f;
}

float3 F4FX_ZeroNonFinite( float3 c )	// a channel whose exponent is all ones (Inf, NaN) counts as 0; by bits, not isnan
{
	const uint3 e = asuint( c ) & 0x7f800000u;
	return float3( e.x == 0x7f800000u ? 0.0f : c.x, e.y == 0x7f800000u ? 0.0f : c.y, e.z == 0x7f800000u ? 0.0f : c.z );
}

// the game's blur weights, row radius - 1, tap k at offset k - 7 (cellpost.cpp kBlurWeights, the same bits)
static const float F4FX_BlurWeights[7][15] = {
	{ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.106506713f, 0.786985755f, 0.106506713f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
	{ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0544885658f, 0.244201481f, 0.402619928f, 0.244201481f, 0.0544885658f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
	{ 0.0f, 0.0f, 0.0f, 0.0f, 0.0366327688f, 0.111280687f, 0.216745228f, 0.270682156f, 0.216745228f, 0.111280687f, 0.0366327688f, 0.0f, 0.0f, 0.0f, 0.0f },
	{ 0.0f, 0.0f, 0.0f, 0.0276304893f, 0.0662821382f, 0.1238316f, 0.180173829f, 0.204163671f, 0.180173829f, 0.1238316f, 0.0662821382f, 0.0276304893f, 0.0f, 0.0f, 0.0f },
	{ 0.0f, 0.0f, 0.0221905038f, 0.0455889553f, 0.0798113719f, 0.119064637f, 0.151360765f, 0.163967222f, 0.151360765f, 0.119064637f, 0.0798113719f, 0.0455889553f, 0.0221905038f, 0.0f, 0.0f },
	{ 0.0f, 0.018543981f, 0.0341669098f, 0.0563317239f, 0.0831085816f, 0.109719239f, 0.129617959f, 0.137022808f, 0.129617959f, 0.109719239f, 0.0831085816f, 0.0563317239f, 0.0341669098f, 0.018543981f, 0.0f },
	{ 0.0159283616f, 0.0270778015f, 0.0424232148f, 0.0612547807f, 0.0815124661f, 0.0999668092f, 0.112988554f, 0.117695801f, 0.112988554f, 0.0999668092f, 0.0815124661f, 0.0612547807f, 0.0424232148f, 0.0270778015f, 0.0159283616f },
};

// game_dof_down at target uv v1: four linear taps, uv = (tap.xy * c7.xy + v1) * c4.xy, weighted by tap.z
float3 F4FX_DofDown( Texture2D<float4> frame, SamplerState lin, float2 v1, float4 c4, float4 c7, float4 taps[4] )
{
	float3 acc = float3( 0.0f, 0.0f, 0.0f );
	for ( int i = 0; i < 4; i++ ) {
		const float2 uv = ( taps[i].xy * c7.xy + v1 ) * c4.xy;
		acc = F4FX_ZeroNonFinite( frame.SampleLevel( lin, uv, 0.0f ).rgb ) * taps[i].z + acc;
	}
	return acc;
}

// game_dof's blend factor of a window depth d: c0 = (range, range, distance, -), c1 = (strength, near on, far on, sky
// sharp), c2 = (-, near, far - near, far * near)
float F4FX_DofFactor( float d, float4 c0, float4 c1, float4 c2, bool farOnly, bool dofRed )
{
	const float x = 1.0f - d;
	float y = x * c2.z + c2.y;
	bool notSky = 0.00001f < x;
	y = c2.w / y;
	float k;
	if ( farOnly ) {
		k = F4FX_PostSat( ( y - c0.z ) / c0.y );
		k = k * c1.x;
		notSky = notSky || !( 0.0f != c1.w );
	} else {
		float nearT = ( c0.z - y ) / c0.x;
		const bool nearOn = ( y < c0.z ) && ( 0.0f != c1.y );
		nearT = nearOn ? nearT : 0.0f;
		float farT = y - c0.z;
		const bool farOn = ( c0.z < y ) && ( 0.0f != c1.z );
		notSky = notSky || !( 0.0f != c1.w );
		farT = farT / c0.y;
		k = F4FX_PostSat( farOn ? farT : nearT );
		k = k * c1.x;
	}
	if ( dofRed )
		k = c1.x;
	return notSky ? k : 0.0f;
}

float4 F4FX_DofBlend( float k, float4 a, float4 b )
{
	return k * ( b - a ) + a;
}

// the game's motion blur: texture space (v down) <-> this view's
float2 F4FX_MbV( float2 d )
{
	return float2( d.x, 1.0f - d.y );
}

float2 F4FX_MbVector( Texture2D<float4> mv, SamplerState lin, float2 d )
{
	const float2 m = mv.SampleLevel( lin, F4FX_MbV( d ), 0.0f ).xy;
	return float2( m.x, -m.y );
}

float F4FX_MbTapWeight( float2 m, float L, float threshold )	// dp2, sqrt, add, mad_sat
{
	const float l = sqrt( dot( m, m ) );
	return F4FX_PostSat( -abs( L - l ) * threshold + 1.0f );
}

// the thin lens: lz = (near, far - near, far * near, sky sharp), lensP = (focus S1, aperture px, focal length, -),
// lensQ = (max circle px, near on, far on, near reach from the tile max)
float F4FX_LensDist( float d, float4 lz )	// the view distance of a window depth; < 0 = the sky
{
	const float x = 1.0f - d;
	return 0.00001f < x ? lz.z / ( x * lz.y + lz.x ) : -1.0f;
}

float F4FX_LensFocus( Texture2D<float4> depth, float4 lz, float4 lensP )	// the record's focus, or the centre's (autofocus)
{
	const float f = max( lensP.z, 0.001f );
	float s1 = lensP.x;
	if ( !( s1 > 0.0f ) ) {
		uint w, h;
		depth.GetDimensions( w, h );
		const float z = F4FX_LensDist( depth.Load( int3( int( w ) / 2, int( h ) / 2, 0 ) ).r, lz );
		s1 = z > 0.0f ? z : 1e6f;
	}
	return max( s1, f * 1.01f );
}

float F4FX_LensCoc( float z, float s1, float4 lz, float4 lensP, float4 lensQ )	// signed circle of confusion, px
{
	const float f = max( lensP.z, 0.001f );
	float c;
	if ( z < 0.0f )
		c = lz.w != 0.0f ? 0.0f : lensP.y * f / ( s1 - f );
	else
		c = lensP.y * f * ( z - s1 ) / ( z * ( s1 - f ) );
	if ( lensQ.z == 0.0f )
		c = min( c, 0.0f );
	if ( lensQ.y == 0.0f )
		c = max( c, 0.0f );
	return clamp( c, -lensQ.x, lensQ.x );
}

// the lens composite at full-size texel p: the frame, the far layer past 1 px, the near layer over both
float4 F4FX_LensTexel( int2 p, Texture2D<float4> frame, Texture2D<float4> farLayer, Texture2D<float4> depth,
	Texture2D<float4> nearLayer, SamplerState lin, float s1, float4 lz, float4 lensP, float4 lensQ )
{
	const float4 a = frame.Load( int3( p, 0 ) );
	const float cf = F4FX_LensCoc( F4FX_LensDist( depth.Load( int3( p, 0 ) ).r, lz ), s1, lz, lensP, lensQ );
	uint fw, fh;
	farLayer.GetDimensions( fw, fh );
	const float2 uq = ( float2( p ) + 0.5f ) / ( 4.0f * float2( fw, fh ) );
	float3 m = a.rgb;
	if ( cf > 1.0f )
		m = F4FX_DofBlend( F4FX_PostSat( ( cf - 1.0f ) / 3.0f ), float4( a.rgb, 0.0f ), float4( farLayer.SampleLevel( lin, uq, 0.0f ).rgb, 0.0f ) ).rgb;	// GL mix's order
	if ( lensQ.y != 0.0f ) {
		const float4 n = nearLayer.SampleLevel( lin, uq, 0.0f );
		if ( n.a > 0.0f )
			m = m - m * n.a + n.rgb;	// GL's expansion of m * ( 1 - a ) + n (an infinite frame texel reads NaN, as GL's)
	}
	return float4( m, a.a );
}

#endif

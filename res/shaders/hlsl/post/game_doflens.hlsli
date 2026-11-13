// HLSL source of features/post/game_doflens.glsl (include-only GLSL), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	float4 lz;
	float4 lensP;
	float4 lensQ;
#else
#ifndef WW_GLSL_SIZE_Texture2D_float4_
#define WW_GLSL_SIZE_Texture2D_float4_
int2 textureSize(Texture2D<float4> t, int lod) { uint w, h, l; t.GetDimensions(uint(lod), w, h, l); return int2(w, h); }
#endif
/* lane DOFLENS1: the thin-lens depth of field (src/features/post/cellpost.cpp, row WW_CELL_DOF_LENS). Shared by the prep pass
 * and the composites (game_doflens.frag, game_doflensmblur.frag).
 * lz = (near, far - near, far * near, sky sharp): the view distance from the GL window depth as game_dof.frag reads it.
 * lensP = (focus distance S1, <= 0 = the depth at the frame's centre; aperture A in sensor px; focal length f; -),
 * S1 and f in game units. lensQ = (max circle of confusion in px, near on, far on, near reach from the tile max (gather only)).
 * The circle of confusion (diameter, px) of a point at distance z: c = A f (z - S1) / (z (S1 - f)), > 0 behind the
 * focus (the far layer), < 0 in front (the near layer). The composite needs LENS_FRAME, LENS_FAR, LENS_DEPTH. */



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
float lensFocus( Texture2D<float4> depthTex, SamplerState _depthTex_sampler )
{
	float f = max( lensP.z, 0.001 );
	float s1 = lensP.x;
	if ( !( s1 > 0.0 ) ) {
		int2 c = textureSize( depthTex, 0 ) / 2;
		float z = lensDist( depthTex.Load( int3(c, 0) ).r );
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
Texture2D<float4> lensNear : register(t0, space0); SamplerState _lensNear_sampler : register(s0, space0);	// the near layer, premultiplied (rgb * a, a)

static float lensS1 = 0.0;	// set once in main: lensS1 = lensFocus( LENS_DEPTH )

// the lens composite at one full-size texel: the sharp frame, the far layer where the circle passes 1 px, the near
// layer over both; an in-focus texel with no near neighbour is the frame bit for bit
float4 lensTexel( int2 p )
{
	float4 a = LENS_FRAME.Load( int3(p, 0) );
	float cf = lensCoc( lensDist( LENS_DEPTH.Load( int3(p, 0) ).r ), lensS1 );
	float2 uq = ( float2( p ) + 0.5 ) / ( 4.0 * float2( textureSize( LENS_FAR, 0 ) ) );
	float3 m = a.rgb;
	if ( cf > 1.0 )
		m = lerp( a.rgb, LENS_FAR.Sample( _LENS_FAR_sampler, uq ).rgb, lensSat( ( cf - 1.0 ) / 3.0 ) );
	if ( lensQ.y != 0.0 ) {
		float4 n = lensNear.Sample( _lensNear_sampler, uq );
		if ( n.a > 0.0 )
			m = m * ( 1.0 - n.a ) + n.rgb;
	}
	return float4( m, a.a );
}
#endif
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/post/game_doflens.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: game_doflens_prep_ps.hlsl, game_doflens_ps.hlsl, game_doflensmblur_ps.hlsl
// Not here (declarations, or no converted entry carried them): lensTexel

#if defined(WW_FN_lensSat) || defined(WW_FN_ALL)
float lensSat(float v)
{
    float lensSatRet;
    if (v >= 0.0f)
    {
        lensSatRet = min(v, 1.0f);
    }
    else
    {
        lensSatRet = 0.0f;
    }
    return lensSatRet;
}
#endif

#if defined(WW_FN_lensDist) || defined(WW_FN_ALL)
float lensDist(float d)
{
    float x = 1.0f - d;
    float lensSatRet;
    if (9.9999997473787516355514526367188e-06f < x)
    {
        lensSatRet = lz.z / ((x * lz.y) + lz.x);
    }
    else
    {
        lensSatRet = -1.0f;
    }
    return lensSatRet;
}
#endif

#if defined(WW_FN_lensFocus) || defined(WW_FN_ALL)
float lensFocus(Texture2D<float4> depthTex, SamplerState _depthTex_sampler)
{
    float f = max(lensP.z, 0.001000000047497451305389404296875f);
    float s1 = lensP.x;
    if (!(s1 > 0.0f))
    {
        int2 c = int2(spvTextureSize(depthTex, uint(0), dummy_parameter)) / int2(2, 2);
        float lensSatRet = depthTex.Load(int3(c, 0)).x;
        float z = lensDist(lensSatRet);
        s1 = (z > 0.0f) ? z : 1000000.0f;
    }
    return max(s1, f * 1.0099999904632568359375f);
}
#endif

#if defined(WW_FN_lensCoc) || defined(WW_FN_ALL)
float lensCoc(float z, float s1)
{
    float f = max(lensP.z, 0.001000000047497451305389404296875f);
    float c;
    if (z < 0.0f)
    {
        float lensSatRet;
        if (lz.w != 0.0f)
        {
            lensSatRet = 0.0f;
        }
        else
        {
            lensSatRet = (lensP.y * f) / (s1 - f);
        }
        // the sky: sharp, or the lens's limit at infinity
        c = lensSatRet;
    }
    else
    {
        c = ((lensP.y * f) * (z - s1)) / (z * (s1 - f));
    }
    if (lensQ.z == 0.0f)
    {
        c = min(c, 0.0f);
    }
    if (lensQ.y == 0.0f)
    {
        c = max(c, 0.0f);
    }
    return clamp(c, -lensQ.x, lensQ.x);
}
#endif
#endif	// WW_HLSL_SRC

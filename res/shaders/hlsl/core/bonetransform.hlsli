// HLSL source of bonetransform.glsl (include-only GLSL), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#ifndef WW_GLSL_MATDIAG3x4
#define WW_GLSL_MATDIAG3x4
float3x4 wwMat3x4Diag(float s) { return float3x4(s, 0.0, 0.0, 0.0, 0.0, s, 0.0, 0.0, 0.0, 0.0, s, 0.0); }
#endif
#ifndef WW_REG_skinningUniforms
#define WW_REG_skinningUniforms b1
#endif
cbuffer skinningUniforms : register(WW_REG_skinningUniforms, space0)
{
	row_major float3x4	boneTransforms[256] : packoffset(c0);		// bone transforms in row-major order
};

static float4 boneWeights[2];

#ifdef BT_POSITION_ONLY
void boneTransform( inout float4 v )
#elif defined( BT_NO_TANGENTS )
void boneTransform( inout float4 v, inout float3 n )
#else
void boneTransform( inout float4 v, inout float3 n, inout float3 t, inout float3 b )
#endif
{
#if defined( BT_POSITION_ONLY ) || defined( BT_NO_TANGENTS )
	float3	vTmp = (float3)( 0.0 );
#ifndef BT_POSITION_ONLY
	float3	nTmp = (float3)( 0.0 );
#endif
#else
	float3x4	mTmp = wwMat3x4Diag( 0.0 );
#endif
	float	wSum = 0.0;
	for ( int i = 0; i < 8; i++ ) {
		float	bw = boneWeights[i >> 2][i & 3];
		if ( !( bw > 0.0 ) )
			break;
		int	bone = int( bw ) & 0xFF;
		float	w = frac( bw );
#if defined( BT_POSITION_ONLY ) || defined( BT_NO_TANGENTS )
		float3x4	m = boneTransforms[bone];
		vTmp += mul(m, v) * w;
#ifndef BT_POSITION_ONLY
		nTmp += mul((float3x3)( m ), n) * w;
#endif
#else
		mTmp += boneTransforms[bone] * w;
#endif
		wSum += w;
	}
	if ( wSum > 0.0 ) {
#if defined( BT_POSITION_ONLY ) || defined( BT_NO_TANGENTS )
		v = float4( vTmp / wSum, 1.0 );
#ifndef BT_POSITION_ONLY
		n = nTmp;
#endif
#else
		v = float4( mul(mTmp, v) / wSum, 1.0 );
		float3x3	r = (float3x3)( mTmp );
		n = mul(r, n);
		t = mul(r, t);
		b = mul(r, b);
#endif
	}
}
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/bonetransform.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: default_vs.hlsl, f76_default_vs.hlsl, fo4_default_vs.hlsl, fo4_defaultfs_vs.hlsl, ob_default_vs.hlsl, selection_vs.hlsl, sk_default_vs.hlsl, sk_effectshader_vs.hlsl, sk_msn_vs.hlsl, stf_default_vs.hlsl, wireframe_vs.hlsl

#if defined(WW_FN_boneTransform) || defined(WW_FN_ALL)
void boneTransform(inout float4 v, inout float3 n, inout float3 t, inout float3 b)
{
    float3x4 mTmp = float3x4(0.0f.xxxx, 0.0f.xxxx, 0.0f.xxxx);
    float wSum = 0.0f;
    for (int i = 0; i < 8; i++)
    {
        float bw = boneWeights[i >> 2][i & 3];
        if (!(bw > 0.0f))
        {
            break;
        }
        int bone = int(bw) & 255;
        float w = frac(bw);
        float3x4 mTmpPart = boneTransforms[bone] * w;
        mTmp = float3x4(mTmp[0] + mTmpPart[0], mTmp[1] + mTmpPart[1], mTmp[2] + mTmpPart[2]);
        wSum += w;
    }
    if (wSum > 0.0f)
    {
        v = float4(mul(mTmp, v) / wSum.xxx, 1.0f);
        float3x3 r = float3x3(mTmp[0].xyz, mTmp[1].xyz, mTmp[2].xyz);
        n = mul(r, n);
        t = mul(r, t);
        b = mul(r, b);
    }
}
#endif

#if defined(WW_FN_boneTransform_V2) || defined(WW_FN_ALL)
void boneTransform(inout float4 v, inout float3 n)
{
    float3 vTmp = 0.0f.xxx;
    float3 nTmp = 0.0f.xxx;
    float wSum = 0.0f;
    for (int i = 0; i < 8; i++)
    {
        float bw = boneWeights[i >> 2][i & 3];
        if (!(bw > 0.0f))
        {
            break;
        }
        int bone = int(bw) & 255;
        float w = frac(bw);
        float3x4 m = boneTransforms[bone];
        vTmp += (mul(m, v) * w);
        nTmp += (mul(float3x3(m[0].xyz, m[1].xyz, m[2].xyz), n) * w);
        wSum += w;
    }
    if (wSum > 0.0f)
    {
        v = float4(vTmp / wSum.xxx, 1.0f);
        n = nTmp;
    }
}
#endif

#if defined(WW_FN_boneTransform_V3) || defined(WW_FN_ALL)
void boneTransform(inout float4 v)
{
    float3 vTmp = 0.0f.xxx;
    float wSum = 0.0f;
    for (int i = 0; i < 8; i++)
    {
        float bw = boneWeights[i >> 2][i & 3];
        if (!(bw > 0.0f))
        {
            break;
        }
        int bone = int(bw) & 255;
        float w = frac(bw);
        float3x4 m = boneTransforms[bone];
        vTmp += (mul(m, v) * w);
        wSum += w;
    }
    if (wSum > 0.0f)
    {
        v = float4(vTmp / wSum.xxx, 1.0f);
    }
}
#endif
#endif	// WW_HLSL_SRC

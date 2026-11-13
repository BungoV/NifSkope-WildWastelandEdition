// HLSL source of ww_treewind.glsl (include-only GLSL), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	float4 wwTreeWind;
	float4 wwTreeWind2;
	float4 wwTreeWind3;
	float4 wwTreeWind4;
	float4 wwTreeWind5;
#else
/* lane WINDTREE1: the game's tree wind on a loaded cell's Tree_Anim shapes (Todd's treat), op for op as
 * src/gl/celltreewind.h (wwTreeWindVertex). Set per draw by src/gl/cellgrass.cpp (wwCellGrassUniforms):
 * wwTreeWind  = (wind angle, 0, previous phase, phase)
 * wwTreeWind2 = (min speed * 300, max speed * 300, wind frequency, 1 on a tree-wind shape / 0 on every other)
 * wwTreeWind3 = (the reference's position in model space, its world x + y + z)
 * wwTreeWind4 = (2 x bound radius x scale, leaf amplitude x scale, leaf frequency, 0)
 * wwTreeWind5 = (the reference's rotation x (1, 1, 1), 0): the object normal's sum from the world normal
 * The weld carries the vertex alpha (the sway weight) in the bitangent's length: 2 + alpha. Normals are not bent. */


float3 wwTreeWindApply( float3 p, float3 nrm, float bitLen, float4x4 mv )
{
	float a = bitLen > 1.5 ? clamp( bitLen - 2.0, 0.0, 1.0 ) : 0.0;
	float3 n = nrm * rsqrt( max( dot( nrm, nrm ), 1.0e-30 ) );
	float s = dot( n, wwTreeWind5.xyz );
	float base = 3.0 * s + wwTreeWind3.w;
	float F = wwTreeWind4.z * 20.0;
	float dist = length( ( mul(float4( wwTreeWind3.xyz, 1.0 ), mv) ).xyz ) / length( mv[0].xyz );
	float A = wwTreeWind4.y * clamp( ( 4600.0 - dist ) * 0.001, 0.0, 1.0 ) * 0.035;
	float halfR = ( wwTreeWind2.y - wwTreeWind2.x ) * 0.5;
	float sp = wwTreeWind2.x + halfR * sin( wwTreeWind.w * ( ( F * wwTreeWind2.z ) * 5.0 ) + base );
	float3 off = p + ( n * ( sp * A ) ) * a - wwTreeWind3.xyz;
	float h = ( off.x + off.y + off.z ) / wwTreeWind4.x;
	float c = sin( ( wwTreeWind.w * wwTreeWind2.z ) * F + ( h * -1.5 + wwTreeWind3.w ) );
	float g = wwTreeWind2.x + halfR * ( 0.3 * sin( 3.14159274 * c ) + sin( 6.28318548 * c ) + 2.0 );
	float b = ( h * g / 3.0 ) * a;
	float3 q = off + float3( cos( wwTreeWind.x ) * b, sin( wwTreeWind.x ) * b, 0.0 );
	float ql = length( q );
	return ( ql > 0.0 ? q * ( length( off ) / ql ) : q ) + wwTreeWind3.xyz;
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/ww_treewind.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_default_vs.hlsl, fo4_defaultfs_vs.hlsl

#if defined(WW_FN_wwTreeWindApply) || defined(WW_FN_ALL)
float3 wwTreeWindApply(float3 p, float3 nrm, float bitLen, float4x4 mv)
{
    float aPick;
    if (bitLen > 1.5f)
    {
        aPick = clamp(bitLen - 2.0f, 0.0f, 1.0f);
    }
    else
    {
        aPick = 0.0f;
    }
    float a = aPick;
    float3 n = nrm * rsqrt(max(dot(nrm, nrm), 1.0000000031710768509710513471353e-30f));
    float s = dot(n, wwTreeWind5.xyz);
    float base = (3.0f * s) + wwTreeWind3.w;
    float F = wwTreeWind4.z * 20.0f;
    float dist = length(mul(float4(wwTreeWind3.xyz, 1.0f), mv).xyz) / length(mv[0].xyz);
    float A_1 = (wwTreeWind4.y * clamp((4600.0f - dist) * 0.001000000047497451305389404296875f, 0.0f, 1.0f)) * 0.0350000001490116119384765625f;
    float halfR = (wwTreeWind2.y - wwTreeWind2.x) * 0.5f;
    float sp = wwTreeWind2.x + (halfR * sin((wwTreeWind.w * ((F * wwTreeWind2.z) * 5.0f)) + base));
    float3 off = (p + ((n * (sp * A_1)) * a)) - wwTreeWind3.xyz;
    float h = ((off.x + off.y) + off.z) / wwTreeWind4.x;
    float c = sin(((wwTreeWind.w * wwTreeWind2.z) * F) + ((h * (-1.5f)) + wwTreeWind3.w));
    float g = wwTreeWind2.x + (halfR * (((0.300000011920928955078125f * sin(3.1415927410125732421875f * c)) + sin(6.283185482025146484375f * c)) + 2.0f));
    float b = ((h * g) / 3.0f) * a;
    float3 q = off + float3(cos(wwTreeWind.x) * b, sin(wwTreeWind.x) * b, 0.0f);
    float ql = length(q);
    float3 wwTreeWindApplyRet;
    if (ql > 0.0f)
    {
        wwTreeWindApplyRet = q * (length(off) / ql);
    }
    else
    {
        wwTreeWindApplyRet = q;
    }
    return wwTreeWindApplyRet + wwTreeWind3.xyz;
}
#endif
#endif	// WW_HLSL_SRC

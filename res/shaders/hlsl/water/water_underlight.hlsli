// HLSL source of features/water/water_underlight.glsl (include-only GLSL), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	// pass. The water between the eye and what it sees takes the Water clear colour's coefficients for the body's class
	// (absorption + backscatter per channel, water_clear.glsl), in place of the WATR's linear fog:
	//   transmission  exp( -k L ) along the view (L to the surface where the ray leaves the water)
	//   in-scatter    single scattering of the sun and the sky, each dimmed with depth at its own rate K = k / mu: at a
	//                 point s along the ray the depth is z = zc - dir.z s, so per light
	//                 sigma E exp( -K zc ) ( 1 - exp( -( k - K dir.z ) L ) ) / ( k - K dir.z )   (sigma = 0.75 bb)
	//   the scene     was lit in air: dimmed by the light's way down to it (as the clear colour's bed)
	// and a meniscus: where the screen crosses the surface (the near plane straddles it) a dark band a few pixels wide.
	// Needs water_depth.glsl (kWwPureWater, kWwUnitMetres) and water_clear.glsl (wwClearK) before it.
	int uwLight;		// 1 = the Underwater light row is on
	int uwLightRed;		// 2 nomeniscus, 4 nocolor (the WATR fog law stays); 1 nosnell is the water program's
	float4 uwSun;		// the sun's linear colour (0 below the horizon), its height (cos from the zenith)
	float3 uwSky;		// the water's diffuse sky light, linear: half the horizon + half the upper row
	int uwClass;		// the body's class: 0 sea, 1 river, 2 lake (placed water)
	float2 uwMenK;		// the meniscus: half width (px), darkening at its middle
#else
// lane WATERLOOK1 step 16 (WATERUNDER2, src/features/water/cellunderwater.h): the Underwater light row in the camera-underwater


// ( 1 - exp( -g L ) ) / g, per channel, safe at g -> 0
float3 wwUlInt( float3 g, float L )
{
	float3 gl = g * L;
	return select((abs( gl ) < (float3)( 1e-4 )), L * ( 1.0 - 0.5 * gl ), ( 1.0 - exp( -gl ) ) / g);
}

// the meniscus' darkening at a pixel whose near-plane point sits dz above the surface; w its half width (u)
float wwUnderMeniscus( float dz, float w )
{
	if ( or( uwLight == 0, ( uwLightRed & 2 ) != 0 ) )
		return 0.0;
	return uwMenK.y * ( 1.0 - smoothstep( 0.0, max( w, 1e-6 ), abs( dz ) ) );
}

// frame = frame * mul + add for the camera under water (pixels whose near point is above the surface: untouched)
void wwUnderLight( float3 eye, float3 Pn, float3 Pw, bool farPlane, float H, out float3 add, out float3 mul )
{
	add = (float3)( 0.0 );
	mul = (float3)( 1.0 );
	if ( Pn.z >= H )
		return;
	float3 d = Pw - eye;
	float L = length( d );
	float3 dir = d / max( L, 1e-6 );
	if ( farPlane )
		L = 1.0e9;
	if ( dir.z > 1e-6 )
		L = min( L, ( H - eye.z ) / dir.z );
	float3 bb;
	float3 k = wwClearK( uwClass, bb );
	float s = clamp( uwSun.w, 0.0, 1.0 );
	float muS = sqrt( 1.0 - ( 1.0 - s * s ) / ( 1.333 * 1.333 ) );	// the sun's cosine under the surface
	const float muD = 0.8;
	float3 Es = uwSun.rgb * s * 0.98, Ed = uwSky * 0.94;	// through the surface (Fresnel ~2% / 6%), as the clear colour
	float3 Ks = k / muS, Kd = k / muD;
	float Lm = min( L, 1.0e7 ) * kWwUnitMetres;
	float zc = max( H - eye.z, 0.0 ) * kWwUnitMetres;	// the eye's depth (m)
	float zv = max( H - ( eye.z + dir.z * min( L, 1.0e7 ) ), 0.0 ) * kWwUnitMetres;	// the seen point's depth (m)
	float3 sigma = 0.75 * bb;
	add = sigma * ( Es * exp( -Ks * zc ) * wwUlInt( k - Ks * dir.z, Lm ) + Ed * exp( -Kd * zc ) * wwUlInt( k - Kd * dir.z, Lm ) );
	// the scene's light on its way down (a small diffuse floor keeps lamp-lit points at night from going black)
	float3 Td = ( Es * exp( -Ks * zv ) + ( Ed + 1e-4 ) * exp( -Kd * zv ) ) / ( Es + Ed + 1e-4 );
	mul = Td * exp( -k * Lm );
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/water/water_underlight.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cell_underwater_ms_ps.hlsl, cell_underwater_ps.hlsl

#if defined(WW_FN_wwUlInt) || defined(WW_FN_ALL)
float3 wwUlInt(float3 g, float L)
{
    float3 gl = g * L;
    float3 wwUlIntRet3 = (1.0f.xxx - exp(-gl)) / g;
    float3 wwUlIntRet2 = (1.0f.xxx - (gl * 0.5f)) * L;
    float3 wwUlIntPart = abs(gl);
    bool3 wwUlIntRet = bool3(wwUlIntPart.x < 9.9999997473787516355514526367188e-05f.xxx.x, wwUlIntPart.y < 9.9999997473787516355514526367188e-05f.xxx.y, wwUlIntPart.z < 9.9999997473787516355514526367188e-05f.xxx.z);
    return float3(wwUlIntRet.x ? wwUlIntRet2.x : wwUlIntRet3.x, wwUlIntRet.y ? wwUlIntRet2.y : wwUlIntRet3.y, wwUlIntRet.z ? wwUlIntRet2.z : wwUlIntRet3.z);
}
#endif

#if defined(WW_FN_wwUnderMeniscus) || defined(WW_FN_ALL)
float wwUnderMeniscus(float dz, float w)
{
    bool wwUlIntRet3 = uwLight == 0;
    bool wwUlIntRet2;
    if (!wwUlIntRet3)
    {
        wwUlIntRet2 = (uwLightRed & 2) != 0;
    }
    else
    {
        wwUlIntRet2 = wwUlIntRet3;
    }
    if (wwUlIntRet2)
    {
        return 0.0f;
    }
    return uwMenK.y * (1.0f - smoothstep(0.0f, max(w, 9.9999999747524270787835121154785e-07f), abs(dz)));
}
#endif

#if defined(WW_FN_wwUnderLight) || defined(WW_FN_ALL)
void wwUnderLight(float3 eye, float3 Pn, float3 Pw, bool farPlane, float H, out float3 add, out float3 mul)
{
    add = 0.0f.xxx;
    mul = 1.0f.xxx;
    if (Pn.z >= H)
    {
        return;
    }
    float3 d = Pw - eye;
    float L = length(d);
    float3 dir = d / max(L, 9.9999999747524270787835121154785e-07f).xxx;
    if (farPlane)
    {
        L = 1000000000.0f;
    }
    if (dir.z > 9.9999999747524270787835121154785e-07f)
    {
        L = min(L, (H - eye.z) / dir.z);
    }
    int wwUlIntRet3 = uwClass;
    float3 wwUlIntRet2;
    float3 wwUlIntPart = wwClearK(wwUlIntRet3, wwUlIntRet2);
    float3 bb = wwUlIntRet2;
    float3 k = wwUlIntPart;
    float s = clamp(uwSun.w, 0.0f, 1.0f);
    // the sun's cosine under the surface
    float muS = sqrt(1.0f - ((1.0f - (s * s)) / 1.77688896656036376953125f));
    // through the surface (Fresnel ~2% / 6%), as the clear colour
    float3 Es = (uwSun.xyz * s) * 0.980000019073486328125f;
    float3 Ed = uwSky * 0.939999997615814208984375f;
    float3 Ks = k / muS.xxx;
    float3 Kd = k / 0.800000011920928955078125f.xxx;
    float Lm = min(L, 10000000.0f) * 0.01428749971091747283935546875f;
    // the eye's depth (m)
    float zc = max(H - eye.z, 0.0f) * 0.01428749971091747283935546875f;
    // the seen point's depth (m)
    float zv = max(H - (eye.z + (dir.z * min(L, 10000000.0f))), 0.0f) * 0.01428749971091747283935546875f;
    float3 sigma = bb * 0.75f;
    float3 wwUlIntRet = k - (Ks * dir.z);
    float addPart3 = Lm;
    float3 addPart2 = k - (Kd * dir.z);
    float addPart = Lm;
    add = sigma * (((Es * exp((-Ks) * zc)) * wwUlInt(wwUlIntRet, addPart3)) + ((Ed * exp((-Kd) * zc)) * wwUlInt(addPart2, addPart)));
    // the scene's light on its way down (a small diffuse floor keeps lamp-lit points at night from going black)
    float3 Td = ((Es * exp((-Ks) * zv)) + ((Ed + 9.9999997473787516355514526367188e-05f.xxx) * exp((-Kd) * zv))) / ((Es + Ed) + 9.9999997473787516355514526367188e-05f.xxx);
    mul = Td * exp((-k) * Lm);
}
#endif
#endif	// WW_HLSL_SRC

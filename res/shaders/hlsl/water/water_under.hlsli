// HLSL source of features/water/water_under.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
// lane WATERUNDER1: the camera-underwater look (src/features/water/cellunderwater.h). Pure: the offline gate
// (notes/waterunder1/tools/uw_readback.py) draws cell_underwater.frag, which includes this, through the real driver.
//   uwFog  = ( WATR uwFog amount, near, far, the surface height H, world z )
//   fogCol = the fog's colour, linear: half the water sky's horizon + half the WATR underwater colour (the game's own
//            underwater surface variant, fo4_water.frag), so the fog meets the surface seen from below without a seam
//   tint   = the WATR underwater colour, linear: the filter the light takes over `far` units of water (the probe
//            bake's glass rule, probebake.cpp waterFog), so the scene and the fog darken with depth below H
//   red    = 1 notint (no depth darkening), 2 nofog (no view fog)
//   fwd    = the camera's forward, world: the fog ramps over the view depth L . fwd, the game's (its image-space depth
//            of field shader, lane WATERUNDER3); vec3( 0 ) = over the ray length (WATERUNDER1, red radial)
// Out: the frame becomes scene * mul + add (dual-source blend, one pass).

void wwUnderwater( float3 eye, float3 Pn, float3 Pw, bool farPlane, float4 uwFog, float3 fogCol, float3 tint, int red, float3 fwd,
	out float3 add, out float3 mul )
{
	add = (float3)( 0.0 );
	mul = (float3)( 1.0 );
	float H = uwFog.w;
	if ( Pn.z >= H )
		return;	// this pixel's ray starts above the surface: the waterline splits the screen per pixel
	float3 d = Pw - eye;
	float L = length( d );
	float3 dir = d / max( L, 1e-6 );
	if ( farPlane )
		L = 1.0e9;	// nothing drew there: the fog's full amount
	if ( dir.z > 1e-6 )
		L = min( L, ( H - eye.z ) / dir.z );	// the ray leaves the water at the surface (its underside is fogged too)
	float far = max( uwFog.z, 1e-3 );
	float span = max( uwFog.z - uwFog.y, 1e-3 );
	float Lf = dot( fwd, fwd ) > 0.0 ? L * max( dot( dir, fwd ), 0.0 ) : L;
	float a = clamp( uwFog.x * clamp( ( Lf - uwFog.y ) / span, 0.0, 1.0 ), 0.0, 1.0 );
	float dv = clamp( H - ( eye.z + dir.z * L ), 0.0, far );	// the light's path down to the point
	float dc = clamp( H - eye.z, 0.0, far );	// and down to the eye: the fog's own light
	float3 Tv = (float3)( 1.0 ) - clamp( uwFog.x, 0.0, 1.0 ) * ( dv / far ) * ( (float3)( 1.0 ) - tint );
	float3 Tc = (float3)( 1.0 ) - clamp( uwFog.x, 0.0, 1.0 ) * ( dc / far ) * ( (float3)( 1.0 ) - tint );
	if ( ( red & 1 ) != 0 ) {
		Tv = (float3)( 1.0 );
		Tc = (float3)( 1.0 );
	}
	if ( ( red & 2 ) != 0 )
		a = 0.0;
	add = fogCol * Tc * a;
	mul = Tv * ( 1.0 - a );
}
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/water/water_under.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cell_underwater_ms_ps.hlsl, cell_underwater_ps.hlsl

#if defined(WW_FN_wwUnderwater) || defined(WW_FN_ALL)
void wwUnderwater(float3 eye, float3 Pn, float3 Pw, bool farPlane, float4 uwFog_1, float3 fogCol, float3 tint, int red, float3 fwd, out float3 add, out float3 mul)
{
    add = 0.0f.xxx;
    mul = 1.0f.xxx;
    float H = uwFog_1.w;
    if (Pn.z >= H)
    {
        return;
    }
    float3 d = Pw - eye;
    float L = length(d);
    float3 dir = d / max(L, 9.9999999747524270787835121154785e-07f).xxx;
    if (farPlane)
    {
        // nothing drew there: the fog's full amount
        L = 1000000000.0f;
    }
    if (dir.z > 9.9999999747524270787835121154785e-07f)
    {
        // the ray leaves the water at the surface (its underside is fogged too)
        L = min(L, (H - eye.z) / dir.z);
    }
    float far = max(uwFog_1.z, 0.001000000047497451305389404296875f);
    float span = max(uwFog_1.z - uwFog_1.y, 0.001000000047497451305389404296875f);
    float LfPick;
    if (dot(fwd, fwd) > 0.0f)
    {
        LfPick = L * max(dot(dir, fwd), 0.0f);
    }
    else
    {
        LfPick = L;
    }
    float Lf = LfPick;
    float a = clamp(uwFog_1.x * clamp((Lf - uwFog_1.y) / span, 0.0f, 1.0f), 0.0f, 1.0f);
    // the light's path down to the point
    float dv = clamp(H - (eye.z + (dir.z * L)), 0.0f, far);
    // and down to the eye: the fog's own light
    float dc = clamp(H - eye.z, 0.0f, far);
    float3 Tv = 1.0f.xxx - ((1.0f.xxx - tint) * (clamp(uwFog_1.x, 0.0f, 1.0f) * (dv / far)));
    float3 Tc = 1.0f.xxx - ((1.0f.xxx - tint) * (clamp(uwFog_1.x, 0.0f, 1.0f) * (dc / far)));
    if ((red & 1) != 0)
    {
        Tv = 1.0f.xxx;
        Tc = 1.0f.xxx;
    }
    if ((red & 2) != 0)
    {
        a = 0.0f;
    }
    add = (fogCol * Tc) * a;
    mul = Tv * (1.0f - a);
}
#endif
#endif	// WW_HLSL_SRC

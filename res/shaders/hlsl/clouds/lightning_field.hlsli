// HLSL source of features/clouds/lightning_field.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
// lane LIGHTNING2 (LIGHTNINGSKY1): a storm flash's light at p (world km x, y, height) seen along d (from the eye), per
// unit of the colour the caller hands in: max(phi + 3 g D grad(phi).d, 0), phi = the diffusion Green's function of the
// strike minus its images across the Low band's extrapolated base and top. The same field as cloud_composite.frag's
// cvLtField; twin: src/features/clouds/cloudlightning.h fieldAt (which also carries the norm; the caller's colour carries it here).
// src = (x, y, height km, 1 / delta), prm = (r0^2 km^2, 2 (base - z_b), 2 (top + z_b), 3 g D km).
float ldLtField( float4 src, float4 prm, float3 p, float3 d )
{
	float phi = 0.0, gd = 0.0;
	for ( int i = 0; i < 3; i++ ) {
		float zs = i == 0 ? src.z : ( i == 1 ? prm.y : prm.z ) - src.z;
		float3 a = p - float3( src.xy, zs );
		float r = sqrt( dot( a, a ) + prm.x );
		float e = exp( -r * src.w ) / r;
		float s = i == 0 ? 1.0 : -1.0;
		phi += s * e;
		gd += s * ( -e * ( src.w + 1.0 / r ) / r ) * dot( a, d );
	}
	return max( phi + prm.w * gd, 0.0 );
}
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/clouds/lightning_field.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Not here (declarations, or no converted entry carried them): ldLtField
#endif	// WW_HLSL_SRC

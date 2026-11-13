// HLSL source of features/far-ground/terrainpreview_decal_vs.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	row_major float4x4 uViewProj;
#else
static int vRec;
static const int kIdx[36] = { 0,2,1, 1,2,3, 4,5,6, 5,7,6, 0,1,4, 1,5,4, 2,6,3, 3,6,7, 0,4,2, 2,4,6, 1,3,5, 3,7,5 };
void main()
{
	Rec r = recs[gl_InstanceID];
	int c = kIdx[gl_VertexID];
	float3 t = float3( float( c & 1 ), float( ( c >> 1 ) & 1 ), float( ( c >> 2 ) & 1 ) );
	float3 l = float3( lerp( r.box0.x, r.box0.z, t.x ), lerp( r.box0.y, r.box0.w, t.y ), lerp( r.box1.x, r.box1.y, t.z ) );
	float3 w = r.posScale.xyz + qrot( r.quat, l * r.posScale.w );
	vRec = gl_InstanceID;
	gl_Position = mul(float4( w, 1.0 ), uViewProj);
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/far-ground/terrainpreview_decal_vs.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Not here (declarations, or no converted entry carried them): main
#endif	// WW_HLSL_SRC

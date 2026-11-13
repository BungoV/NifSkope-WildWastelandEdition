// HLSL source of features/near/ww_bindless.glsl (include-only GLSL), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	uint wwDrawBase;
#if defined(WW_BL_VERT) && defined(GL_ARB_shader_draw_parameters)
	// lane FARLOOK1101 (FARINSTBL1): an instanced far ring's base instance is a placement row; it reads wwDrawBase alone
	bool wwBlUniformRow;
#endif
#else
// lane BINDLESS2: the per-draw row (src/features/near/cellbindless.h WwBl::Row, std430, 176 bytes): the draw's matrices and the
// material's seven 2D maps as resident texture handles. wwDrawIdx is the draw's row in the ring at binding 12.
// lane BINDLESSMDI1: the row is wwDrawBase + the draw's base instance (0 for a plain draw; a multi-draw's command
// carries its own), handed to the fragment stage flat.
struct WwDraw
{
	row_major float4x4 mv;
	float4 nm[3];
	uint2 tex[8];
};
StructuredBuffer<WwDraw> wwDraws : register(t49, space0);
#ifdef WW_BL_VERT
#ifdef GL_ARB_shader_draw_parameters

#define wwDrawIdx ( wwDrawBase + ( wwBlUniformRow ? 0u : uint( gl_BaseInstanceARB ) ) )
#else
#define wwDrawIdx wwDrawBase
#endif
static uint wwBlRow;
#else
static uint wwBlRow;
#define wwDrawIdx wwBlRow
#endif
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/near/ww_bindless.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// This GLSL holds declarations and macros only; the declarations live in the per-module shim.
#endif	// WW_HLSL_SRC

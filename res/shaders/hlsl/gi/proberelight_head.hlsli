// HLSL source of features/gi/proberelight_head.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	int uN;
	int uRed;          // 1 doorblanket, 2 glassopaque, 3 dooropen
	int uHasDir;
	float3 uDirColor;
	int uSunOn;
	float3 uSun;
	int uSkyOn;
	int uRedGrow;
	int uNVox;
	int uFirst;
#else
#define WW_NUMTHREADS numthreads(64, 1, 1)	// the entry: [WW_NUMTHREADS] void main(...)
RWStructuredBuffer<float> surf : register(u0, space0);          // 16 a surfel: n, dirK, alb, sunK, le, -, B1, -
StructuredBuffer<int> pairStart : register(t0, space0);
StructuredBuffer<float> pairs : register(t1, space0);   // 8: light, d, nl, dl, doorRec, -, -, -
StructuredBuffer<float> doorRec : register(t2, space0); // 12: door0, door1, -, -, T[8]
StructuredBuffer<float> lights : register(t3, space0); // 16: pos, radius, color, bias, scale, exp, cone, cosOuter, spot, k, s|x0, kind
globallycoherent RWStructuredBuffer<uint> ctl : register(u1, space0);       // 0 max change, 1 max B, 2.. door closed
StructuredBuffer<int> linkStart : register(t4, space0);
StructuredBuffer<float> links : register(t5, space0);   // 12: surf, omega, tint, cos[6], doorRec
StructuredBuffer<float> probeK : register(t6, space0); // 20: kUnl, -, sky[18]
RWStructuredBuffer<float> pe : register(u2, space0);                 // 18 a probe
RWStructuredBuffer<float> bsrc : register(u3, space0);           // 4 a surfel
RWStructuredBuffer<float> bdst : register(u4, space0);
StructuredBuffer<int> feedStart : register(t7, space0);
StructuredBuffer<float> feed : register(t8, space0);    // 4: probe, w, doorRec, -
StructuredBuffer<int> slots : register(t9, space0);   // 2: voxel, which
RWStructuredBuffer<float> grid : register(u5, space0);           // 2 x nVox x 24

float doorF( int rec, int c, bool vis )
{
	if ( or( rec < 0, uRed == 3 ) )
		return 1.0;
	float f = 1.0;
	for ( int s = 0; s < 2; s++ ) {
		int d = asint( doorRec[rec * 12 + s] );
		if ( or( d < 0, ctl[2 + d] == 0u ) )
			continue;
		float cl = asfloat( ctl[2 + d] );   // lane DOORLIVE1: the slot's shut fraction (0u = open)
		int o = rec * 12 + 4 + s * 4;
		float tt;
		if ( uRed == 1 )
			tt = 0.0;
		else if ( uRed == 2 )
			tt = doorRec[o + 3];
		else if ( vis )
			tt = ( or( or( doorRec[o] > 0.0, doorRec[o + 1] > 0.0 ), doorRec[o + 2] > 0.0 ) ) ? 1.0 : 0.0;
		else
			tt = doorRec[o + c];
		if ( cl >= 1.0 ) {
			if ( uRed == 1 )
				return 0.0;
			f *= tt;
		} else {
			f *= 1.0 - cl * ( 1.0 - tt );
		}
	}
	return f;
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/gi/proberelight_head.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Not here (declarations, or no converted entry carried them): doorF
#endif	// WW_HLSL_SRC

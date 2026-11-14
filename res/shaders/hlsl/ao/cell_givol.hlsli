// HLSL source of features/ao/cell_givol.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
/* lane PERF1: the GI volumes' shapes (src/features/gi/proberooms.h GiVolumeShape), 3 RGBA32F texels each: (center xyz, type),
 * (size xyz, room), (rotation xyz degrees, 0). Types: 0 box (size = edges), 1 cylinder (x, y diameters, height on z),
 * 2 wedge (full height at -y, none at +y), 3 capsule (diameter on x, y; end to end on z), 4 sphere (x, y, z diameters). */

Buffer<float4> volShapes : register(t0, space0);

// shape -> world: Rz Ry Rx
float3x3 volRot( float3 deg )
{
	float3 r = radians( deg );
	float3 c = cos( r ), s = sin( r );
	float3x3 X = float3x3( 1.0, 0.0, 0.0, 0.0, c.x, s.x, 0.0, -s.x, c.x );
	float3x3 Y = float3x3( c.y, 0.0, -s.y, 0.0, 1.0, 0.0, s.y, 0.0, c.y );
	float3x3 Z = float3x3( c.z, s.z, 0.0, -s.z, c.z, 0.0, 0.0, 0.0, 1.0 );
	return mul(X, mul(Y, Z));
}

// l: the point in the shape's own frame (centered, unrotated)
bool volInside( float3 l, int type, float3 s )
{
	float3 h = max( 0.5 * s, (float3)( 1.0e-3 ) );
	if ( type == 4 )
		return dot( l / h, l / h ) <= 1.0;
	if ( type == 3 ) {
		float r = min( h.x, h.y );
		float hz = max( h.z - r, 0.0 );
		float3 d = float3( l.xy, l.z - clamp( l.z, -hz, hz ) );
		return dot( d, d ) <= r * r;
	}
	if ( any( (abs( l ) > h) ) )
		return false;
	if ( type == 1 )
		return dot( l.xy / h.xy, l.xy / h.xy ) <= 1.0;
	if ( type == 2 )
		return l.z + h.z <= 2.0 * h.z * ( 0.5 - 0.5 * l.y / h.y );
	return true;
}
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/ao/cell_givol.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cell_givol_ps.hlsl, cell_givol_vs.hlsl
// Not here (declarations, or no converted entry carried them): volInside

#if defined(WW_FN_volRot) || defined(WW_FN_ALL)
float3x3 volRot(float3 deg)
{
    float3 r = radians(deg);
    float3 c = cos(r);
    float3 s = sin(r);
    float3x3 X = float3x3(float3(1.0f, 0.0f, 0.0f), float3(0.0f, c.x, s.x), float3(0.0f, -s.x, c.x));
    float3x3 Y = float3x3(float3(c.y, 0.0f, -s.y), float3(0.0f, 1.0f, 0.0f), float3(s.y, 0.0f, c.y));
    float3x3 Z = float3x3(float3(c.z, s.z, 0.0f), float3(-s.z, c.z, 0.0f), float3(0.0f, 0.0f, 1.0f));
    return mul(X, mul(Y, Z));
}
#endif
#endif	// WW_HLSL_SRC

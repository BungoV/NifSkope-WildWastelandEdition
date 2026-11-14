// FARSKYMAP1 (deck D6, s57-58): the far sky-visibility map, written in HLSL (no GLSL source). One texel per
// 4096-unit cell, six directions, baked from probefar's per-cell probes (src/features/far-lod/farskymap.h is the
// C++ twin and the writer). Laid out by world position: every LOD chunk size reads the same map, and the
// hardware mip from the screen footprint gives far chunks a smaller mip.
// The entry includes this file three times, like the other readable halves: types, loose uniforms, body.
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)

#elif defined(WW_LOOSE_UNIFORMS)
	// probe GI drawn and a map loaded (src/gl/celllights.cpp); false = the per-vertex byte as is
	bool farSkyOn;
	// the map rect in scene units: xy = its corner (cell x0, y0 times 4096 minus the scene shift), zw = 1 / its size
	float4 farSkyRect;
	// the vertex stage's far-shape uniforms, read here by name (src/features/far-lod/cellfarinst.cpp and
	// src/gl/renderer.cpp set them per draw, 0 on every other shape): the map reaches far shapes only
	bool farInst;
	float4 farVcAo;
#else

// A far shape (past the last cascade): a far placement (farInst) or a far draw with its baked type (farVcAo.z).
// Near shapes keep the byte.
bool farSkyShapeFar( bool inst, float vcAoType )
{
#ifdef FARSKY_RED_NEAR
	return true;	// red (tests/features/far-lod/farskymap): the map on near shapes too
#endif
	return inst || vcAoType > 0.5;
}

float2 farSkyMapUv( float3 p )
{
	return ( p.xy - farSkyRect.xy ) * farSkyRect.zw;
}

// The six directions weighted by n squared; texels outside the map or never baked have valid 0 and read 1.
// a = layer 0, b = layer 1 (both premultiplied); the visibility comes back with valid in .y
float2 farSkyDir6( float4 a, float4 b, float3 n )
{
	float valid = b.z;
	if ( valid <= 0.0 )
		return float2( 1.0, 0.0 );
	float3 w = n * n;
	float sx = n.x >= 0.0 ? a.x : a.y;
	float sy = n.y >= 0.0 ? a.z : a.w;
	float sz = n.z >= 0.0 ? b.x : b.y;
#ifdef FARSKY_RED
	sz = n.z >= 0.0 ? b.y : b.x;	// red (tests/features/far-lod/farskymap): up and down swapped
#endif
	float v = ( w.x * sx + w.y * sy + w.z * sz ) / valid / max( w.x + w.y + w.z, 1e-8 );
	return float2( v, valid );
}

// base = the per-vertex byte's visibility (wwFarSkyVis). Off: base untouched. On: base x lerp(1, map, valid).
float farSkyCombine( float base, bool on, float2 vv )
{
	if ( !on )
		return base;
	return base * ( 1.0 + ( clamp( vv.x, 0.0, 1.0 ) - 1.0 ) * vv.y );
}

#ifndef FARSKY_MATH_ONLY	// the GPU match kernel (tests/features/far-lod/farskymap) takes the math alone
// layer 0 = (+X, -X, +Y, -Y) x valid, layer 1 = (+Z, -Z, valid, 0): premultiplied, so bilinear reads and mips are
// valid-weighted
Texture2DArray<float4> farSkyMap : register(t63, space0); SamplerState _farSkyMap_sampler : register(s63, space0);

// Call in uniform control flow (the hardware mip needs the pixel quad).
float farSkyVisAt( float base, float3 p, float3 n )
{
	if ( !farSkyOn || !farSkyShapeFar( farInst, farVcAo.z ) )
		return base;
	float2 uv = farSkyMapUv( p );
	float4 a = farSkyMap.Sample( _farSkyMap_sampler, float3( uv, 0.0 ) );
	float4 b = farSkyMap.Sample( _farSkyMap_sampler, float3( uv, 1.0 ) );
	if ( any( uv < 0.0 ) || any( uv > 1.0 ) )
		b = (float4)( 0.0 );	// past the map: nothing baked there
	return farSkyCombine( base, true, farSkyDir6( a, b, n ) );
}
#endif	// FARSKY_MATH_ONLY
#endif	// WW_LOOSE_UNIFORMS
#endif	// WW_HLSL_SRC

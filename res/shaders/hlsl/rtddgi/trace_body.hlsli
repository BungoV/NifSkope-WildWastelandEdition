// The probe trace's helpers ReSTIR GI shades its bounce with (lane HLSLRESTIR1, 2026-11-12; the twin of
// src/features/gi/rtddgi/trace_body.glsl). DDGI_Nearest, DDGI_IrradianceAt and DDGI_Direct live in DDGITrace.hlsli (proven
// against trace.comp, which includes trace_body.glsl); this file adds the two trace_body.glsl helpers trace.comp inlines.
// Shared, no bindings (the shim declares the volume's buffers and DDGITLAS; DDGI_USE_TLAS).
#ifndef TRACE_BODY_HLSLI
#define TRACE_BODY_HLSLI

#include "DDGITrace.hlsli"

// trace_body.glsl ddgiShadow: is anything hit in (1e-4, tmax]? (holes let light through; red hwhit: the hardware's hit)
bool DDGI_Shadow( float3 o, float3 d, float tmax )
{
	return DDGI_AnyHit( o, d, tmax, ( uint( DDGI_P.flags.w ) & DDGI_RED_HWHIT ) != 0u, false );
}

// trace_body.glsl ddgiHitRadiance:
// MIRROR rtddgimath.h hitRadiance (RTEMIS1): albedo x irradiance / pi + the surface's own emission Le (placed lights kept)
float3 DDGI_HitRadiance( float3 alb, float3 E, float3 Le ) { return alb * E / DDGI_PI + Le; }

#endif

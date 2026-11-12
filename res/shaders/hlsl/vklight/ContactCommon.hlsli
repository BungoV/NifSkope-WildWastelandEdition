// Copyright 2023 Sony Interactive Entertainment.
// Licensed under the Apache License, Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0).
//
// lane VKLIGHT1 (2026-11-12): the contact march's math (res/shaders/features/shadows/ww_contact.comp, lane PERFFIX2's
// line layout of Bend Studio's screen-space shadows), bindings-free (F4FX_ names) so Fusion Shaders can call it from its
// own shim. Expressions keep the GL twin's order and its `precise` lines: the owner rule needs every lane to get the
// same bits for one line.
#ifndef CONTACT_COMMON_HLSLI
#define CONTACT_COMMON_HLSLI

// a mask pixel and its linear depth to view space (sssViewPos)
float3 F4FX_ContactViewPos( float2 maskPx, float z, float maskScale, float2 fullSize, float2 tanXY )
{
	float2 ndc = maskPx * maskScale / fullSize * 2.0f - 1.0f;
	return float3( ndc.x * ( tanXY.x * z ), ndc.y * ( tanXY.y * z ), z );	// NVIDIA's GL groups tan x z first (measured)
}

// a line's minor coordinate at position u (lineY): j + 0.5 - offset x u / Da, unfused
float F4FX_ContactLineY( int j, float lineOff, int u, float Da )
{
	precise float r = float( u ) / Da;
	precise float y = ( float( j ) + 0.5f ) - lineOff * r;
	return y;
}

// one sample's distance from the occluder test (Bend's "under 5 instructions"); S, R from shared memory
float F4FX_ContactSample( float C, float S, float R, float Kc )
{
	return abs( ( C - S ) * min( R, Kc ) - 1.0f );
}

#endif

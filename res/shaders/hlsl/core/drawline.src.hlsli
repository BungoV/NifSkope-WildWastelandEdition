// HLSL source of drawline.glsl, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
#pragma once
#include "GlslCompat.hlsli"
uniform float lineWidth;

void drawLine( float4 p0, float4 p1, float4 c0, float4 c1 , inout WW_GS_STREAM geometry_stream)
{
	float	p0zw = p0.z + p0.w;
	float	p1zw = p1.z + p1.w;
	if ( !( p0zw > 0.0 && p1zw > 0.0 ) ) {
		if ( p0zw > 0.000001 ) {
			float	t = p0zw / ( p0zw - p1zw );
			p1 = lerp( p0, p1, t );
			c1 = lerp( c0, c1, t );
		} else if ( p1zw > 0.000001 ) {
			float	t = p1zw / ( p1zw - p0zw );
			p0 = lerp( p1, p0, t );
			c0 = lerp( c1, c0, t );
		} else {
			return;
		}
	}

	float3	p0_ndc = p0.xyz / p0.w;
	float3	p1_ndc = p1.xyz / p1.w;

	float2	vpScale = (float2)(viewportDimensions.zw) * 0.5;
	float2	vpOffs = (float2)(viewportDimensions.xy) + vpScale;

	float2	p0_ss = p0_ndc.xy * vpScale;
	float2	p1_ss = p1_ndc.xy * vpScale;

	float2	d = normalize( p1_ss - p0_ss ) * max( lineWidth * 0.5, 0.5 );
	float2	n = float2( -d.y, d.x ) / vpScale;

	C = c0;
	gl_Position = float4( p0_ndc.xy + n, p0_ndc.z, 1.0 );
	EmitVertex(geometry_stream);
	C = c0;
	gl_Position = float4( p0_ndc.xy - n, p0_ndc.z, 1.0 );
	EmitVertex(geometry_stream);
	C = c1;
	gl_Position = float4( p1_ndc.xy + n, p1_ndc.z, 1.0 );
	EmitVertex(geometry_stream);
	C = c1;
	gl_Position = float4( p1_ndc.xy - n, p1_ndc.z, 1.0 );
	EmitVertex(geometry_stream);

	geometry_stream.RestartStrip();
}

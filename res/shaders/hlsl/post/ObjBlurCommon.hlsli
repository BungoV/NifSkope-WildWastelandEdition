// Per-object motion blur (lane VKFRAME1 P7, 2026-11-12): the math of cell_objblur_vec / cell_objblur_tile / cell_objblur.frag
// (src/features/post/cellpost.h, McGuire et al. 2012 with Jimenez 2014's sample jitter), shared by NifSkope's Vulkan kernels
// (PostFxBindingsVk.hlsli) and Fusion Shaders. Bindings-free; every expression keeps the GL twin's order.
#ifndef OBJBLUR_COMMON_HLSLI
#define OBJBLUR_COMMON_HLSLI

// pass 1: the pixel's blur vector in pixels (current minus previous, times the shutter, at most maxPx), the camera taken out.
// reproj as its 4 GL columns; ji = (jitter ndc xy, 1 / size xy); vel / own = the velocity and its object mask (.b)
float2 F4FX_ObjBlurVector( float2 fc, float2 vel, float own, float d, float4 r0, float4 r1, float4 r2, float4 r3, float4 ji,
	float shutter, float maxPx, bool keepCamera )
{
	const float2 uv = fc * ji.zw;
	float2 v;
	if ( keepCamera ) {
		v = vel;
	} else {
		const float3 ndc = float3( uv * 2.0f - 1.0f - ji.xy, d * 2.0f - 1.0f );
		const float4 q = r0 * ndc.x + r1 * ndc.y + r2 * ndc.z + r3;
		const float2 cam = ( q.xy / q.w - ndc.xy ) * 0.5f;
		v = own > 0.5f ? vel - cam : float2( 0.0f, 0.0f );
	}
	v = -v / ji.zw * shutter;
	const float l = length( v );
	if ( !( l < 1e6f ) )
		v = float2( 0.0f, 0.0f );
	else if ( l > maxPx )
		v *= maxPx / l;
	return v;
}

// passes 2 / 3: keep the longer of two vectors (the first wins a tie)
void F4FX_ObjBlurLonger( float2 v, inout float2 best, inout float bl )
{
	const float l = dot( v, v );
	if ( l > bl ) {
		bl = l;
		best = v;
	}
}

float F4FX_ObjBlurDist( float d, float zNear, float zFar )	// the view distance of a window depth
{
	const float x = 1.0f - d;
	return zFar * zNear / ( x * ( zFar - zNear ) + zNear );
}

float F4FX_ObjBlurReach( float d, float r )	// the share of a pixel d away that lies inside radius r
{
	return clamp( r + 0.5f - d, 0.0f, 1.0f );
}

float F4FX_ObjBlurFront( float za, float zb, float k )	// 1 when a is in front of (or level with) b, fading over k behind
{
	return clamp( 1.0f - ( za - zb ) / k, 0.0f, 1.0f );
}

// pass 4's sample weight: X the centre (radius rx, distance zx), Y a sample d px away (ry, zy)
float F4FX_ObjBlurWeight( float d, float rx, float ry, float zx, float zy, float k )
{
	const float yf = F4FX_ObjBlurFront( zy, zx, k );
	const float xf = F4FX_ObjBlurFront( zx, zy, k );
	const float same = yf * xf;
	const float rm = max( rx, ry );
	return same * ( rm < 0.5f ? 1.0f : F4FX_ObjBlurReach( d, rm ) )
		+ ( yf - same ) * F4FX_ObjBlurReach( d, ry )
		+ ( xf - same ) * F4FX_ObjBlurReach( d, rx );
}

// pass 4's jitter (interleaved gradient noise at the fragment centre fc), in [-0.5, 0.5). GL's dot is y * b fused onto x * a:
// dot() here moved the samples off GL's on 2105 halves by > 1 ulp (the 53x amplifies a last bit), this form on 3
float F4FX_ObjBlurJitter( float2 fc, float seed )
{
	return frac( 52.9829189f * frac( mad( fc.y, 0.00583715f, fc.x * 0.06711056f ) + seed ) ) - 0.5f;
}

#endif

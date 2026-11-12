// NifSkope entry of ReSTIR GI (lane HLSLRESTIR1, 2026-11-12): both shims + DDGIRestirGi.hlsli. -DRESTIRGI_FILTER builds the
// filter kernel (modes 2..: restirgi_filt.spv.h). Gate: tests/hlslmatch restirgi / restirgi_filt (src/features/gi/rtddgi/
// restirgi.comp and this kernel on the same buffers + TLAS, byte for byte).
// hlsl profile: cs_6_5
#define DDGI_USE_TLAS
#include "DDGIBindingsVk.hlsli"
#include "DDGIRestirGiBindingsVk.hlsli"
#include "DDGIRestirGi.hlsli"

[numthreads( 64, 1, 1 )]
void main( uint3 tid : SV_DispatchThreadID, uint3 gid : SV_GroupID, uint gi : SV_GroupIndex )
{
	RestirGi_CS( tid, gid, gi );
}

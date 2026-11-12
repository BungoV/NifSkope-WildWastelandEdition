// NifSkope entry of the DDGI relocation / classification (lane VKCOMPUTE1, 2026-11-12): the shim + DDGIReloc.hlsli.
// Gate: tests/hlslmatch reloc (src/features/gi/rtddgi/reloc.comp and this kernel on the same buffers, byte for byte).
#include "DDGIBindingsVk.hlsli"
#include "DDGIReloc.hlsli"

[numthreads( 64, 1, 1 )]
void main( uint3 tid : SV_DispatchThreadID )
{
	DDGI_RelocCS( DDGI_LinearId( tid ) );
}

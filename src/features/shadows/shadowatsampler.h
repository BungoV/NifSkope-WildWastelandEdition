#ifndef SHADOWATSAMPLER_H
#define SHADOWATSAMPLER_H

/* Lane VKSHADOW9 (ruling R1, 2026-11-16): the sun shadow depth pass's alpha test (lookdev_csmdepth_at / _atq) samples its
 * base map with anisotropy 1 on both APIs. NV GL's anisotropic footprint is not Vulkan's (VKSHADOW8: 133-213 of 99k shadow
 * texels flipped at every Vulkan level against GL's 4, EXACT with both at 1). GL binds this sampler object on the alpha
 * casters' unit and unbinds it after each alpha loop (material maps keep their anisotropy everywhere else); the Vulkan twin
 * (vkpresent.cpp csmAtTex) forces anisotropy 1 in its own sampler key. tests/vkframe/shadowview_alpha.inc runs both. */

namespace ShadowAt
{
#ifdef _WIN32
#define SHADOWAT_API __stdcall
#else
#define SHADOWAT_API
#endif
using Proc = void * (*)( const char * name );

//! the last material bind's wrap S, wrap T and min filter (BSShaderLightingProperty::bind); 0 in [2]: no bind since cleared
inline int bindState[3] = { 0, 0, 0 };

//! a GL sampler object: wrap S / T and min filter as given, mag LINEAR, anisotropy 1, the rest GL's defaults. Kept for the
//! process (one a state, a handful). 0: no sampler objects (GL then samples with the texture's own state)
inline unsigned sampler( Proc gp, int wrapS, int wrapT, int minFilter )
{
	using Gen = void( SHADOWAT_API * )( int, unsigned * );
	using Pi = void( SHADOWAT_API * )( unsigned, unsigned, int );
	using Pf = void( SHADOWAT_API * )( unsigned, unsigned, float );
	static Gen gen = nullptr;
	static Pi pi = nullptr;
	static Pf pf = nullptr;
	struct E
	{
		int s, t, m;
		unsigned name;
	};
	static E e[16];
	static int n = 0;
	for ( int i = 0; i < n; i++ )
		if ( e[i].s == wrapS && e[i].t == wrapT && e[i].m == minFilter )
			return e[i].name;
	if ( !gen && gp ) {
		gen = reinterpret_cast<Gen>( gp( "glGenSamplers" ) );
		pi = reinterpret_cast<Pi>( gp( "glSamplerParameteri" ) );
		pf = reinterpret_cast<Pf>( gp( "glSamplerParameterf" ) );
	}
	if ( !gen || !pi || !pf || n >= 16 )
		return 0;
	unsigned s = 0;
	gen( 1, &s );
	if ( !s )
		return 0;
	pi( s, 0x2802, wrapS );       // GL_TEXTURE_WRAP_S
	pi( s, 0x2803, wrapT );       // GL_TEXTURE_WRAP_T
	pi( s, 0x2801, minFilter );   // GL_TEXTURE_MIN_FILTER
	pi( s, 0x2800, 0x2601 );      // GL_TEXTURE_MAG_FILTER, GL_LINEAR
	pf( s, 0x84FE, 1.0f );        // GL_TEXTURE_MAX_ANISOTROPY
	e[n++] = { wrapS, wrapT, minFilter, s };
	return s;
}
}  // namespace ShadowAt

#endif

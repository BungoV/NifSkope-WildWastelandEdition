/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2015, NIF File Format Library and Tools
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the NIF File Format Library and Tools project may not be
   used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

***** END LICENSE BLOCK *****/

#ifndef PROBEMASK_H
#define PROBEMASK_H

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

/* the hole test's UV sum must round the same in every file that includes this (the BVH's files turn
 * contraction off; the soup's other includers do not): its own push, popped at the end */
#if defined( __GNUC__ ) && !defined( __clang__ )
#pragma GCC push_options
#pragma GCC optimize( "fp-contract=off" )
#endif

namespace probebvh {

// ------------------------------------------------------------------ alpha-test masks
/* Lane ALPHATEST1 (2026-10-03): an alpha-tested surface (BGSM/BGEM bAlphaTest, or the NiAlphaProperty test
 * bit) is a hole wherever its map's alpha is under the material's threshold -- the Concord storefront glass
 * (GlassWindows01.BGSM, threshold 128) is 66-72% holes. Such a triangle carries its UVs and a mask; a ray
 * that meets it on a hole texel passes on as if it were not there. Nearest texel of the map at its largest
 * mip of at most 1024 texels across, wrapped; hole = alpha (0..255) < threshold. Triangles past the end of
 * `triOf`, or with -1, have no mask: a soup with no masked triangle traces exactly as before.
 *
 * Lane ALPHATEST2 (2026-10-04): the tested alpha is the renderer's (res/shaders/fo4_default.frag and the legacy
 * branch of pbrm_default.frag: a = vertex alpha x map alpha x material alpha), so each corner carries a scale
 * `as` = its vertex alpha (0..1; 1 where the shape draws no vertex colour or the alpha is a tree's wind weight)
 * x the material's fAlpha, interpolated over the triangle like the vertex colour: hole = map alpha x scale <
 * threshold. A triangle whose three scales are 1 tests exactly the ALPHATEST1 way (integer compare). */
struct AlphaMask
{
	struct Map
	{
		int w = 0, h = 0;
		std::vector<unsigned char> a;   // w * h alpha bytes, row 0 = the map's top (v = 0)
	};
	struct Tri
	{
		int map = -1;
		int model = -1;                 // into `models` (census and the gate's twin only)
		unsigned char thr = 128;
		float uv[6] = { 0, 0, 0, 0, 0, 0 };
		float as[3] = { 1.0f, 1.0f, 1.0f };   // lane ALPHATEST2: vertex alpha x material alpha per corner
		bool scaled() const { return as[0] != 1.0f || as[1] != 1.0f || as[2] != 1.0f; }
	};
	std::vector<Map> maps;
	std::vector<std::string> mapNames, models;
	std::vector<int> triOf;             // soup triangle -> into `tris`, -1 = none
	std::vector<Tri> tris;

	bool empty() const { return tris.empty(); }
	//! the texel's alpha (0..255) at (u, v) of map `m`
	int alphaAt( int m, double u, double v ) const
	{
		const Map & mp = maps[size_t( m )];
		u -= std::floor( u );
		v -= std::floor( v );
		int x = int( u * mp.w ), y = int( v * mp.h );
		x = std::min( std::max( x, 0 ), mp.w - 1 );
		y = std::min( std::max( y, 0 ), mp.h - 1 );
		return mp.a[size_t( y ) * size_t( mp.w ) + size_t( x )];
	}
	//! The hit at barycentrics (b1, b2) of soup triangle `tri` lands on a hole.
	bool hole( int tri, double b1, double b2 ) const
	{
		if ( tri < 0 || size_t( tri ) >= triOf.size() || triOf[size_t( tri )] < 0 )
			return false;
		const Tri & t = tris[size_t( triOf[size_t( tri )] )];
		const double b0 = 1.0 - b1 - b2;
		const double u = b0 * t.uv[0] + b1 * t.uv[2] + b2 * t.uv[4];
		const double v = b0 * t.uv[1] + b1 * t.uv[3] + b2 * t.uv[5];
		if ( !t.scaled() )
			return alphaAt( t.map, u, v ) < int( t.thr );
		const double s = b0 * double( t.as[0] ) + b1 * double( t.as[1] ) + b2 * double( t.as[2] );
		return double( alphaAt( t.map, u, v ) ) * s < double( t.thr );
	}
	//! lane ALPHATEST2: the test at (u, v) of map `m` with scale `s` (the soup builder's albedo pick)
	bool holeAt( int m, double u, double v, unsigned char thr, double s ) const
	{
		if ( s == 1.0 )
			return alphaAt( m, u, v ) < int( thr );
		return double( alphaAt( m, u, v ) ) * s < double( thr );
	}
	//! lane ALPHATEST2: any triangle carries a scale (the soup then writes AMK2, else AMK1 byte for byte)
	bool anyScaled() const
	{
		for ( const Tri & t : tris )
			if ( t.scaled() )
				return true;
		return false;
	}
};

} // namespace probebvh

#if defined( __GNUC__ ) && !defined( __clang__ )
#pragma GCC pop_options
#endif

#endif // PROBEMASK_H

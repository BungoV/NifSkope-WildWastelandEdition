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


#ifndef PROBEEMIT_H
#define PROBEEMIT_H

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// ------------------------------------------------------------------ emissive surfaces in the bake
/* Lane EMISSIVEGI1 (2026-10-03; docs/cloud/EMISSIVEGI1_DESIGN.md): a bake ray that hits a glowing surface takes
 * that surface's own emitted light on top of its lit-albedo term (GIBS, SEED SIGGRAPH 2021; no new lights).
 * The game's shader adds e = glowColor x glowMult x glowMap.rgb in sqrt-of-linear space, so the light it shows
 * unlit is e^2: each hit squares its own texel (the mean of e^2 over the hits, never the square of a mean).
 * Nearest texel of the glow map at its largest mip of at most 1024 texels across, wrapped (the alpha masks'
 * rule). The renderer's gates: Own-Emit off = nothing; the glow-map flag off = the whole surface glows; the
 * flag on with no map = black. Only triangles that can glow are listed: a soup with none bakes as before. */
struct ProbeEmit
{
	struct Map
	{
		int w = 0, h = 0;
		std::vector<unsigned char> rgb;   // w * h * 3 bytes as stored (not sRGB-decoded), row 0 = v 0
	};
	struct Emitter
	{
		float e[3] = { 0, 0, 0 };   // glowColor x glowMult (sqrt-of-linear)
		int map = -1;               // into `maps`; -1 = the whole surface glows
	};
	struct Tri
	{
		int emitter = -1;
		float uv[6] = { 0, 0, 0, 0, 0, 0 };
	};
	std::vector<Map> maps;
	std::vector<std::string> mapNames;
	std::vector<Emitter> emitters;
	std::vector<int> triOf;   // soup triangle -> into `tris`, -1 = none
	std::vector<Tri> tris;

	bool empty() const { return tris.empty(); }
	bool emits( int tri ) const { return tri >= 0 && size_t( tri ) < triOf.size() && triOf[size_t( tri )] >= 0; }
	//! Linear Le at barycentrics (b1, b2) of soup triangle `tri` (adds nothing for a triangle that does not glow).
	void le( int tri, double b1, double b2, double out[3] ) const
	{
		out[0] = out[1] = out[2] = 0.0;
		if ( !emits( tri ) )
			return;
		const Tri & t = tris[size_t( triOf[size_t( tri )] )];
		const Emitter & em = emitters[size_t( t.emitter )];
		double g[3] = { 1.0, 1.0, 1.0 };
		if ( em.map >= 0 ) {
			const Map & mp = maps[size_t( em.map )];
			const double b0 = 1.0 - b1 - b2;
			double u = b0 * t.uv[0] + b1 * t.uv[2] + b2 * t.uv[4];
			double v = b0 * t.uv[1] + b1 * t.uv[3] + b2 * t.uv[5];
			u -= std::floor( u );
			v -= std::floor( v );
			const int x = std::min( std::max( int( u * mp.w ), 0 ), mp.w - 1 );
			const int y = std::min( std::max( int( v * mp.h ), 0 ), mp.h - 1 );
			const unsigned char * p = &mp.rgb[( size_t( y ) * size_t( mp.w ) + size_t( x ) ) * 3];
			for ( int k = 0; k < 3; k++ )
				g[k] = p[k] / 255.0;
		}
		for ( int k = 0; k < 3; k++ ) {
			const double e = double( em.e[k] ) * g[k];
			out[k] = e * e;
		}
	}
	void markLast( long long triIndex, int emitter, const float uv[6] )
	{
		if ( triOf.size() <= size_t( triIndex ) )
			triOf.resize( size_t( triIndex ) + 1, -1 );
		Tri t;
		t.emitter = emitter;
		for ( int k = 0; k < 6; k++ )
			t.uv[k] = uv[k];
		triOf[size_t( triIndex )] = int( tris.size() );
		tris.push_back( t );
	}
};

#endif // PROBEEMIT_H

#ifndef WW_CLOUDNOISE_H
#define WW_CLOUDNOISE_H

/* lane CLOUD2: the volume clouds' two tiling noise volumes (Schneider & Vos, "The Real-time Volumetric Cloudscapes
 * of Horizon Zero Dawn", SIGGRAPH 2015): shape 128^3 = Perlin-Worley, detail 32^3 = Worley fBm, each packed to one
 * channel. Pure C++ (no GL, no Qt): the exe builds them once on first use; tests/cloudvol/cloudvol_dump.cpp writes the
 * same bytes for the offline gates.
 *
 * The recipe (cell counts, octave weights, the Perlin-Worley remap and the packing) is adapted from
 * TileableVolumeNoise, https://github.com/sebh/TileableVolumeNoise, whose licence follows:
 *
 * MIT License
 *
 * Copyright(c) 2017 Sebastien Hillaire
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * This port uses an integer hash in place of the original's glm noise, so the bytes differ from the original's. */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <thread>
#include <vector>

namespace WwCloudNoise
{

constexpr int kShapeN = 128, kDetailN = 32;

inline std::uint32_t hash3( int x, int y, int z, std::uint32_t seed )
{
	std::uint32_t h = std::uint32_t( x ) * 0x8da6b343u ^ std::uint32_t( y ) * 0xd8163841u ^ std::uint32_t( z ) * 0xcb1ab31fu
		^ ( seed + 1u ) * 0x9e3779b9u;
	h ^= h >> 16;
	h *= 0x7feb352du;
	h ^= h >> 15;
	h *= 0x846ca68bu;
	h ^= h >> 16;
	return h;
}

inline std::uint32_t mix32( std::uint32_t h )
{
	h ^= h >> 16;
	h *= 0x85ebca6bu;
	h ^= h >> 13;
	h *= 0xc2b2ae35u;
	h ^= h >> 16;
	return h;
}

inline float unit( std::uint32_t h )
{
	return float( h >> 8 ) * ( 1.0f / 16777216.0f );
}

inline int wrap( int i, int n )
{
	const int r = i % n;
	return r < 0 ? r + n : r;
}

//! squared distance to the nearest feature point (one a cell, n cells per period 1), clamped to [0, 1]
inline float worley( float x, float y, float z, int n, std::uint32_t seed )
{
	const float px = x * float( n ), py = y * float( n ), pz = z * float( n );
	const int ix = int( std::floor( px ) ), iy = int( std::floor( py ) ), iz = int( std::floor( pz ) );
	float d = 1e10f;
	for ( int dz = -1; dz <= 1; dz++ )
		for ( int dy = -1; dy <= 1; dy++ )
			for ( int dx = -1; dx <= 1; dx++ ) {
				const int cx = ix + dx, cy = iy + dy, cz = iz + dz;
				const std::uint32_t h = hash3( wrap( cx, n ), wrap( cy, n ), wrap( cz, n ), seed );
				const std::uint32_t h2 = mix32( h ), h3 = mix32( h2 );
				const float fx = float( cx ) + unit( h ) - px, fy = float( cy ) + unit( h2 ) - py, fz = float( cz ) + unit( h3 ) - pz;
				d = std::min( d, fx * fx + fy * fy + fz * fz );
			}
	return std::clamp( d, 0.0f, 1.0f );
}

inline float fade( float t )
{
	return t * t * t * ( t * ( t * 6.0f - 15.0f ) + 10.0f );
}

inline float grad( std::uint32_t h, float x, float y, float z )
{
	switch ( h % 12u ) {	// the 12 cube-edge directions (Perlin 2002)
	case 0: return x + y;
	case 1: return -x + y;
	case 2: return x - y;
	case 3: return -x - y;
	case 4: return x + z;
	case 5: return -x + z;
	case 6: return x - z;
	case 7: return -x - z;
	case 8: return y + z;
	case 9: return -y + z;
	case 10: return y - z;
	default: return -y - z;
	}
}

//! gradient noise with period n lattice cells over [0, 1), about -1..1
inline float perlin( float x, float y, float z, int n, std::uint32_t seed )
{
	const float px = x * float( n ), py = y * float( n ), pz = z * float( n );
	const int ix = int( std::floor( px ) ), iy = int( std::floor( py ) ), iz = int( std::floor( pz ) );
	const float fx = px - float( ix ), fy = py - float( iy ), fz = pz - float( iz );
	const float u = fade( fx ), v = fade( fy ), w = fade( fz );
	float c[8];
	for ( int k = 0; k < 8; k++ ) {
		const int ox = k & 1, oy = ( k >> 1 ) & 1, oz = ( k >> 2 ) & 1;
		const std::uint32_t h = hash3( wrap( ix + ox, n ), wrap( iy + oy, n ), wrap( iz + oz, n ), seed );
		c[k] = grad( h, fx - float( ox ), fy - float( oy ), fz - float( oz ) );
	}
	const float x0 = c[0] + ( c[1] - c[0] ) * u, x1 = c[2] + ( c[3] - c[2] ) * u;
	const float x2 = c[4] + ( c[5] - c[4] ) * u, x3 = c[6] + ( c[7] - c[6] ) * u;
	const float y0 = x0 + ( x1 - x0 ) * v, y1 = x2 + ( x3 - x2 ) * v;
	return y0 + ( y1 - y0 ) * w;
}

//! Perlin fBm, base frequency freq (cells per period), weights 1/2, 1/4, ... normalised, mapped to [0, 1]
inline float perlinFbm( float x, float y, float z, int freq, int octaves, std::uint32_t seed )
{
	float sum = 0.0f, wsum = 0.0f, w = 0.5f;
	for ( int o = 0; o < octaves; o++ ) {
		sum += perlin( x, y, z, freq, seed + std::uint32_t( o ) * 101u ) * w;
		wsum += w;
		w *= 0.5f;
		freq *= 2;
	}
	return std::clamp( sum / wsum * 0.5f + 0.5f, 0.0f, 1.0f );
}

inline float remap( float v, float lo, float hi, float nlo, float nhi )
{
	return nlo + ( v - lo ) / ( hi - lo ) * ( nhi - nlo );
}

//! one texel of the shape volume: Perlin-Worley eroded by the low Worley fBm (TileableVolumeNoise's packing)
inline float shapeAt( float x, float y, float z )
{
	const float perl = perlinFbm( x, y, z, 8, 3, 7u );
	const float w0 = 1.0f - worley( x, y, z, 8, 11u ), w1 = 1.0f - worley( x, y, z, 32, 12u ), w2 = 1.0f - worley( x, y, z, 56, 13u );
	const float wfbm = w0 * 0.625f + w1 * 0.25f + w2 * 0.125f;
	const float pw = remap( perl, 0.0f, 1.0f, wfbm, 1.0f );
	const float g0 = 1.0f - worley( x, y, z, 8, 21u ), g1 = 1.0f - worley( x, y, z, 16, 22u ), g2 = 1.0f - worley( x, y, z, 32, 23u );
	const float g3 = 1.0f - worley( x, y, z, 64, 24u );
	const float fbm0 = g0 * 0.625f + g1 * 0.25f + g2 * 0.125f;
	const float fbm1 = g1 * 0.625f + g2 * 0.25f + g3 * 0.125f;
	const float fbm2 = g2 * 0.75f + g3 * 0.25f;
	const float low = fbm0 * 0.625f + fbm1 * 0.25f + fbm2 * 0.125f;
	return std::clamp( remap( pw, -( 1.0f - low ), 1.0f, 0.0f, 1.0f ), 0.0f, 1.0f );
}

//! one texel of the detail (erosion) volume: three Worley fBm packed 0.625 / 0.25 / 0.125
inline float detailAt( float x, float y, float z )
{
	const float w0 = 1.0f - worley( x, y, z, 2, 31u ), w1 = 1.0f - worley( x, y, z, 4, 32u ), w2 = 1.0f - worley( x, y, z, 8, 33u );
	const float w3 = 1.0f - worley( x, y, z, 16, 34u );
	const float f0 = w0 * 0.625f + w1 * 0.25f + w2 * 0.125f;
	const float f1 = w1 * 0.625f + w2 * 0.25f + w3 * 0.125f;
	const float f2 = w2 * 0.75f + w3 * 0.25f;
	return std::clamp( f0 * 0.625f + f1 * 0.25f + f2 * 0.125f, 0.0f, 1.0f );
}

//! an n^3 R8 volume, x fastest, texel centres at (i + 0.5) / n; built on all cores (a slab of z a thread)
template <typename F>
std::vector<std::uint8_t> build( int n, F f )
{
	std::vector<std::uint8_t> v( static_cast<size_t>( n ) * size_t( n ) * size_t( n ) );
	const int nt = std::clamp( int( std::thread::hardware_concurrency() ), 1, 32 );
	std::vector<std::thread> ts;
	for ( int t = 0; t < nt; t++ )
		ts.emplace_back( [&, t]() {
			for ( int z = t; z < n; z += nt )
				for ( int y = 0; y < n; y++ )
					for ( int x = 0; x < n; x++ ) {
						const float s = f( ( float( x ) + 0.5f ) / float( n ), ( float( y ) + 0.5f ) / float( n ), ( float( z ) + 0.5f ) / float( n ) );
						v[( size_t( z ) * size_t( n ) + size_t( y ) ) * size_t( n ) + size_t( x )] = std::uint8_t( std::lround( s * 255.0f ) );
					}
		} );
	for ( std::thread & th : ts )
		th.join();
	return v;
}

inline std::vector<std::uint8_t> shapeVolume()
{
	return build( kShapeN, shapeAt );
}

inline std::vector<std::uint8_t> detailVolume()
{
	return build( kDetailN, detailAt );
}

} // namespace WwCloudNoise

#endif

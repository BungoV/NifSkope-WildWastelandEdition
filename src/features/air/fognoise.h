#ifndef WW_FOGNOISE_H
#define WW_FOGNOISE_H

/* MODULES1 part 19 (bungo 10-27, landed 11-06): the near air's drifting lumps read a defined texture, a 64^3 R8 .dds
 * that the weather's .recx links (groundFog "noiseTexture"); with no link the shipped default,
 * Textures\FO4CS\Fog\GroundFogNoise.dds. This header is the generator that wrote that default (lane FOGDRIFT1's
 * tileable two-octave gradient noise, no random seed), the .dds writer and the .dds reader. No Qt: the tool
 * tools/fognoise/fognoise_dds.cpp builds it too. The motion is the weather's wind (FOGWIND1), never the file's. */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace WwFogNoise
{

constexpr int kN = 64;	// the shipped texture's edge

// tileable gradient noise at (x, y, z) in lattice cells, period P cells
inline float grad( float x, float y, float z, int P )
{
	auto hash = []( int i, int j, int k ) {
		std::uint32_t h = std::uint32_t( i ) * 73856093u ^ std::uint32_t( j ) * 19349663u ^ std::uint32_t( k ) * 83492791u;
		h ^= h >> 13;
		h *= 0x5bd1e995u;
		return h ^ ( h >> 15 );
	};
	static const float g[12][3] = { { 1, 1, 0 }, { -1, 1, 0 }, { 1, -1, 0 }, { -1, -1, 0 }, { 1, 0, 1 }, { -1, 0, 1 },
		{ 1, 0, -1 }, { -1, 0, -1 }, { 0, 1, 1 }, { 0, -1, 1 }, { 0, 1, -1 }, { 0, -1, -1 } };
	const int x0 = int( std::floor( x ) ), y0 = int( std::floor( y ) ), z0 = int( std::floor( z ) );
	const float fx = x - float( x0 ), fy = y - float( y0 ), fz = z - float( z0 );
	auto fade = []( float t ) { return t * t * t * ( t * ( t * 6.0f - 15.0f ) + 10.0f ); };
	float r = 0.0f;
	for ( int c = 0; c < 8; c++ ) {
		const int dx = c & 1, dy = ( c >> 1 ) & 1, dz = ( c >> 2 ) & 1;
		const float * v = g[hash( ( x0 + dx ) % P, ( y0 + dy ) % P, ( z0 + dz ) % P ) % 12];
		const float d = v[0] * ( fx - float( dx ) ) + v[1] * ( fy - float( dy ) ) + v[2] * ( fz - float( dz ) );
		r += d * ( dx ? fade( fx ) : 1.0f - fade( fx ) ) * ( dy ? fade( fy ) : 1.0f - fade( fy ) ) * ( dz ? fade( fz ) : 1.0f - fade( fz ) );
	}
	return r;
}

//! the 64^3 texture: two octaves (4 and 8 lattice cells across), bytes over their own range (x fastest, then y, z)
inline void build( std::vector<std::uint8_t> & tex )
{
	const int n = kN;
	std::vector<float> v( size_t( n ) * n * n );
	float lo = 1e30f, hi = -1e30f;
	for ( int k = 0; k < n; k++ )
		for ( int j = 0; j < n; j++ )
			for ( int i = 0; i < n; i++ ) {
				const float x = ( float( i ) + 0.5f ) / float( n ), y = ( float( j ) + 0.5f ) / float( n ), z = ( float( k ) + 0.5f ) / float( n );
				const float f = grad( x * 4.0f, y * 4.0f, z * 4.0f, 4 ) + 0.5f * grad( x * 8.0f + 0.31f, y * 8.0f + 0.17f, z * 8.0f + 0.53f, 8 );
				v[( size_t( k ) * n + j ) * n + i] = f;
				lo = std::min( lo, f );
				hi = std::max( hi, f );
			}
	tex.resize( v.size() );
	for ( size_t i = 0; i < v.size(); i++ )
		tex[i] = std::uint8_t( int( std::lround( ( v[i] - lo ) / std::max( hi - lo, 1e-6f ) * 255.0f ) ) );
}

//! the texel mean and largest deviation from it, 0..1 (the shader's 1 + a (n - mean) / span keeps the mean density)
inline void stats( const std::vector<std::uint8_t> & tex, float & mean, float & span )
{
	double sum = 0.0;
	int bmin = 255, bmax = 0;
	for ( std::uint8_t b : tex ) {
		sum += b;
		bmin = std::min( bmin, int( b ) );
		bmax = std::max( bmax, int( b ) );
	}
	const double m = tex.empty() ? 127.5 : sum / double( tex.size() );
	mean = float( m / 255.0 );
	span = float( std::max( m - bmin, bmax - m ) / 255.0 );
}

inline void put32( std::vector<std::uint8_t> & o, std::size_t at, std::uint32_t v )
{
	for ( int k = 0; k < 4; k++ )
		o[at + k] = std::uint8_t( v >> ( 8 * k ) );
}

inline std::uint32_t get32( const std::uint8_t * p )
{
	return std::uint32_t( p[0] ) | ( std::uint32_t( p[1] ) << 8 ) | ( std::uint32_t( p[2] ) << 16 ) | ( std::uint32_t( p[3] ) << 24 );
}

//! an n^3 R8 volume as a .dds: the DX10 header, DXGI_FORMAT_R8_UNORM (61), TEXTURE3D, one mip
inline std::vector<std::uint8_t> ddsWrite( const std::vector<std::uint8_t> & tex, int n )
{
	std::vector<std::uint8_t> o( 148, 0 );
	std::memcpy( o.data(), "DDS ", 4 );
	put32( o, 4, 124 );
	put32( o, 8, 0x1u | 0x2u | 0x4u | 0x1000u | 0x8u | 0x800000u );	// CAPS HEIGHT WIDTH PIXELFORMAT PITCH DEPTH
	put32( o, 12, std::uint32_t( n ) );	// height
	put32( o, 16, std::uint32_t( n ) );	// width
	put32( o, 20, std::uint32_t( n ) );	// pitch: one byte a texel
	put32( o, 24, std::uint32_t( n ) );	// depth
	put32( o, 28, 1 );	// mips
	put32( o, 76, 32 );	// pixel format size
	put32( o, 80, 0x4u );	// FOURCC
	std::memcpy( o.data() + 84, "DX10", 4 );
	put32( o, 108, 0x1000u | 0x2u );	// TEXTURE | COMPLEX (a volume)
	put32( o, 112, 0x200000u );	// VOLUME
	put32( o, 128, 61 );	// DXGI_FORMAT_R8_UNORM
	put32( o, 132, 4 );	// D3D10_RESOURCE_DIMENSION_TEXTURE3D
	put32( o, 140, 1 );	// array size
	o.insert( o.end(), tex.begin(), tex.end() );
	return o;
}

/*! a cubic R8 volume .dds (the DX10 header with format 61, or a legacy 8-bit luminance header with the volume caps):
 *  its edge and its top mip's texels. False: not that, an edge outside 4..256, or cut short */
inline bool ddsRead( const std::uint8_t * b, std::size_t len, int & n, std::vector<std::uint8_t> & tex )
{
	if ( len < 128 || std::memcmp( b, "DDS ", 4 ) != 0 || get32( b + 4 ) != 124 )
		return false;
	const std::uint32_t h = get32( b + 12 ), w = get32( b + 16 ), d = get32( b + 24 );
	std::size_t off = 128;
	if ( std::memcmp( b + 84, "DX10", 4 ) == 0 ) {
		if ( len < 148 || get32( b + 128 ) != 61 || get32( b + 132 ) != 4 )
			return false;
		off = 148;
	} else if ( !( ( get32( b + 80 ) & 0x20000u ) && get32( b + 88 ) == 8 ) ) {	// LUMINANCE, 8 bits
		return false;
	}
	if ( w != h || w != d || w < 4 || w > 256 )
		return false;
	const std::size_t bytes = std::size_t( w ) * w * w;
	if ( off + bytes > len )
		return false;
	n = int( w );
	tex.assign( b + off, b + off + bytes );
	return true;
}

}	// namespace WwFogNoise

#endif

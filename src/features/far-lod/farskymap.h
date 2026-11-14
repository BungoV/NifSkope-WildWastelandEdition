#pragma once
// FARSKYMAP1 (deck D6, s57-58): the far sky-visibility map. One texel per 4096-unit cell, six directions each,
// baked from probefar's per-cell probes (sector_X_Y.tbk). Laid out by world position, so every LOD chunk size
// reads the same map; far chunks take a smaller mip.
// Storage: 2 layers of RGBA16F, premultiplied by the texel's validity so bilinear reads and mips weight
// correctly; the shader divides by valid.
//   layer 0 = (+X, -X, +Y, -Y) * valid, layer 1 = (+Z, -Z, valid, 0).
// Combine law (shader twin: far_skymap.hlsli): probes off -> the per-vertex byte as is (identity);
// probes on -> base * lerp(1, mapVis(N), valid).
// Pure C++, no Qt: the unit gate compiles it alone.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace FarSkyMap
{

constexpr float kCell = 4096.0f;
constexpr uint32_t kTbkMagic = 0x314B4254u;  // "TBK1"
constexpr uint32_t kFileMagic = 0x594B5346u; // "FSKY"
constexpr uint32_t kFileVersion = 1;

struct Probe { float pos[3]; float skyVis[8]; };

// Octant index: bit0 = x<0, bit1 = y<0, bit2 = z<0 (probebake / probe_bake.py).
// Each direction is the mean of the 4 octants on its side: the cosine projection (SKY1).
inline void dirsFromOctants( const float o[8], float d[6] )
{
	for ( int a = 0; a < 3; a++ ) {
		float pos = 0, neg = 0;
		for ( int i = 0; i < 8; i++ ) {
#ifdef FARSKYMAP_RED_OCTANT
			int bit = ( i >> ( 2 - a ) ) & 1; // red: axes read in the wrong bit order
#else
			int bit = ( i >> a ) & 1;
#endif
			( bit ? neg : pos ) += o[i];
		}
		d[a * 2 + 0] = pos * 0.25f;
		d[a * 2 + 1] = neg * 0.25f;
	}
}

// Reads the probes of a probe bake (.tbk v3..v5). Returns false on a bad header.
inline bool readTbkProbes( const std::string & path, std::vector<Probe> & out )
{
	FILE * f = std::fopen( path.c_str(), "rb" );
	if ( !f )
		return false;
	uint32_t h[16];
	bool ok = std::fread( h, 4, 16, f ) == 16 && h[0] == kTbkMagic && h[1] >= 3 && h[1] <= 5;
	if ( ok ) {
		uint32_t ns = h[6], np = h[7];
		ok = std::fseek( f, 64 + long( 32 ) * long( ns ), SEEK_SET ) == 0;
		unsigned char rec[144];
		for ( uint32_t i = 0; ok && i < np; i++ ) {
			if ( std::fread( rec, 1, 144, f ) != 144 ) { ok = false; break; }
			Probe p;
			std::memcpy( p.pos, rec, 12 );
			std::memcpy( p.skyVis, rec + 32, 32 ); // after pos, linkOffset, linkCount, weightScale, coverage, unlinked
			out.push_back( p );
		}
	}
	std::fclose( f );
	return ok;
}

struct Map
{
	int x0 = 0, y0 = 0, w = 0, h = 0; // cells
	int baked = 0;                    // texels with a probe
	// mips[l] holds w_l * h_l texels of 8 floats: +X -X +Y -Y (premul), +Z -Z (premul), valid, 0
	std::vector<std::vector<float>> mips;

	bool empty() const { return w <= 0 || h <= 0 || mips.empty(); }
	int mipW( int l ) const { int v = w >> l; return v < 1 ? 1 : v; }
	int mipH( int l ) const { int v = h >> l; return v < 1 ? 1 : v; }
	const float * texel( int l, int x, int y ) const { return &mips[l][( size_t( y ) * mipW( l ) + x ) * 8]; }
	size_t bytes16f() const { size_t n = 0; for ( auto & m : mips ) n += m.size() / 8 * 16; return n; }
};

inline int floorCell( float v ) { return int( std::floor( v / kCell ) ); }

// Builds mip 0 from probes laid out by world position (cell = floor(xy / 4096)), then the valid-weighted mips.
// Several probes in one cell are averaged.
inline Map build( const std::vector<Probe> & probes )
{
	Map m;
	if ( probes.empty() )
		return m;
	int ax = 1 << 30, ay = 1 << 30, bx = -( 1 << 30 ), by = -( 1 << 30 );
	for ( auto & p : probes ) {
		int cx = floorCell( p.pos[0] ), cy = floorCell( p.pos[1] );
		ax = std::min( ax, cx ); ay = std::min( ay, cy ); bx = std::max( bx, cx ); by = std::max( by, cy );
	}
	m.x0 = ax; m.y0 = ay; m.w = bx - ax + 1; m.h = by - ay + 1;
	std::vector<float> sum( size_t( m.w ) * m.h * 6, 0.0f );
	std::vector<int> cnt( size_t( m.w ) * m.h, 0 );
	for ( auto & p : probes ) {
#ifdef FARSKYMAP_RED_LAYOUT
		int tx = floorCell( p.pos[1] ) - ay, ty = floorCell( p.pos[0] ) - ax; // red: x and y swapped
		if ( tx >= m.w || ty >= m.h ) continue;
#else
		int tx = floorCell( p.pos[0] ) - ax, ty = floorCell( p.pos[1] ) - ay;
#endif
		float d[6];
		dirsFromOctants( p.skyVis, d );
		size_t i = size_t( ty ) * m.w + tx;
		for ( int k = 0; k < 6; k++ )
			sum[i * 6 + k] += d[k];
		cnt[i]++;
	}
	std::vector<float> l0( size_t( m.w ) * m.h * 8, 0.0f );
	for ( size_t i = 0; i < cnt.size(); i++ ) {
		if ( !cnt[i] )
			continue;
		m.baked++;
		for ( int k = 0; k < 6; k++ )
			l0[i * 8 + k] = sum[i * 6 + k] / float( cnt[i] );
		l0[i * 8 + 6] = 1.0f;
	}
	m.mips.push_back( std::move( l0 ) );
	// Premultiplied, so a plain 2x2 box mean is the valid-weighted mean (the GPU's linear filter does the same).
	for ( int l = 1; m.mipW( l - 1 ) > 1 || m.mipH( l - 1 ) > 1; l++ ) {
		int pw = m.mipW( l - 1 ), ph = m.mipH( l - 1 ), nw = m.mipW( l ), nh = m.mipH( l );
		std::vector<float> v( size_t( nw ) * nh * 8, 0.0f );
		for ( int y = 0; y < nh; y++ )
			for ( int x = 0; x < nw; x++ ) {
				float acc[8] = {};
				int n = 0;
				for ( int dy = 0; dy < 2; dy++ )
					for ( int dx = 0; dx < 2; dx++ ) {
						int sx = std::min( x * 2 + dx, pw - 1 ), sy = std::min( y * 2 + dy, ph - 1 );
						const float * s = &m.mips[l - 1][( size_t( sy ) * pw + sx ) * 8];
						for ( int k = 0; k < 8; k++ ) acc[k] += s[k];
						n++;
					}
				for ( int k = 0; k < 8; k++ ) v[( size_t( y ) * nw + x ) * 8 + k] = acc[k] / float( n );
			}
		m.mips.push_back( std::move( v ) );
	}
	return m;
}

// The six directions at a texel (valid-divided) weighted by the normal squared: n.x^2 picks +X or -X, ...
// Returns the map visibility; valid in [0,1] comes back in *validOut.
inline float evalTexel( const float * t, const float n[3], float * validOut )
{
	float valid = t[6];
	if ( validOut ) *validOut = valid;
	if ( valid <= 0.0f )
		return 1.0f;
	float wx = n[0] * n[0], wy = n[1] * n[1], wz = n[2] * n[2];
	float sx = n[0] >= 0 ? t[0] : t[1];
	float sy = n[1] >= 0 ? t[2] : t[3];
	float sz = n[2] >= 0 ? t[4] : t[5];
	return ( wx * sx + wy * sy + wz * sz ) / valid / std::max( wx + wy + wz, 1e-8f ); // the shader's order
}

// Combine law. on = probe GI drawn and a map loaded. Off returns base untouched (bit identity).
inline float combine( float base, bool on, float mapVis, float valid )
{
	if ( !on )
		return base;
	float v = std::min( std::max( mapVis, 0.0f ), 1.0f );
	return base * ( 1.0f + ( v - 1.0f ) * valid );
}

// File: magic, version, x0, y0, w, h, baked, levels; then each mip's floats as half-precision (u16).
inline uint16_t toHalf( float f )
{
	uint32_t x; std::memcpy( &x, &f, 4 );
	uint32_t s = ( x >> 16 ) & 0x8000u, e = ( x >> 23 ) & 0xff, m = x & 0x7fffffu;
	if ( e == 255 ) return uint16_t( s | 0x7c00u | ( m ? 0x200u : 0 ) );
	int ne = int( e ) - 127 + 15;
	if ( ne >= 31 ) return uint16_t( s | 0x7c00u );
	if ( ne <= 0 ) {
		if ( ne < -10 ) return uint16_t( s );
		m |= 0x800000u;
		uint32_t sh = uint32_t( 14 - ne );
		uint32_t r = m >> sh, rem = m & ( ( 1u << sh ) - 1 ), half = 1u << ( sh - 1 );
		if ( rem > half || ( rem == half && ( r & 1 ) ) ) r++;
		return uint16_t( s | r );
	}
	uint32_t r = ( uint32_t( ne ) << 10 ) | ( m >> 13 ), rem = m & 0x1fffu;
	if ( rem > 0x1000u || ( rem == 0x1000u && ( r & 1 ) ) ) r++;
	return uint16_t( s | r );
}
inline float fromHalf( uint16_t h )
{
	uint32_t s = uint32_t( h & 0x8000u ) << 16, e = ( h >> 10 ) & 0x1f, m = h & 0x3ffu, x;
	if ( e == 0 ) {
		if ( !m ) x = s;
		else { int k = -1; do { k++; m <<= 1; } while ( !( m & 0x400u ) ); x = s | ( uint32_t( 127 - 15 - k ) << 23 ) | ( ( m & 0x3ffu ) << 13 ); }
	} else if ( e == 31 ) x = s | 0x7f800000u | ( m << 13 );
	else x = s | ( ( e + 127 - 15 ) << 23 ) | ( m << 13 );
	float f; std::memcpy( &f, &x, 4 ); return f;
}

inline bool save( const Map & m, const std::string & path )
{
	FILE * f = std::fopen( path.c_str(), "wb" );
	if ( !f ) return false;
	int32_t hd[8] = { int32_t( kFileMagic ), int32_t( kFileVersion ), m.x0, m.y0, m.w, m.h, m.baked, int32_t( m.mips.size() ) };
	bool ok = std::fwrite( hd, 4, 8, f ) == 8;
	for ( auto & mip : m.mips ) {
		std::vector<uint16_t> hv( mip.size() );
		for ( size_t i = 0; i < mip.size(); i++ ) hv[i] = toHalf( mip[i] );
		ok = ok && std::fwrite( hv.data(), 2, hv.size(), f ) == hv.size();
	}
	std::fclose( f );
	return ok;
}

inline bool load( Map & m, const std::string & path )
{
	m = Map();
	FILE * f = std::fopen( path.c_str(), "rb" );
	if ( !f ) return false;
	int32_t hd[8];
	bool ok = std::fread( hd, 4, 8, f ) == 8 && uint32_t( hd[0] ) == kFileMagic && hd[1] == int32_t( kFileVersion )
		&& hd[4] > 0 && hd[5] > 0 && hd[4] <= 4096 && hd[5] <= 4096 && hd[7] > 0 && hd[7] <= 16;
	if ( ok ) {
		m.x0 = hd[2]; m.y0 = hd[3]; m.w = hd[4]; m.h = hd[5]; m.baked = hd[6];
		for ( int l = 0; ok && l < hd[7]; l++ ) {
			std::vector<uint16_t> hv( size_t( m.mipW( l ) ) * m.mipH( l ) * 8 );
			ok = std::fread( hv.data(), 2, hv.size(), f ) == hv.size();
			std::vector<float> v( hv.size() );
			for ( size_t i = 0; i < hv.size(); i++ ) v[i] = fromHalf( hv[i] );
			m.mips.push_back( std::move( v ) );
		}
	}
	std::fclose( f );
	if ( !ok ) m = Map();
	return ok;
}

// The map rect in scene units, for the shader: (x0*4096 - shiftX, y0*4096 - shiftY, 1/(w*4096), 1/(h*4096)).
inline void rect( const Map & m, float shiftX, float shiftY, float out[4] )
{
	out[0] = float( m.x0 ) * kCell - shiftX;
	out[1] = float( m.y0 ) * kCell - shiftY;
	out[2] = 1.0f / ( float( m.w ) * kCell );
	out[3] = 1.0f / ( float( m.h ) * kCell );
}

// One-line census for the CLI and the cell view.
inline std::string census( const Map & m )
{
	char b[256];
	std::snprintf( b, sizeof b, "%dx%d cells from (%d,%d), baked %d of %d, mips %d, %.2f MB",
		m.w, m.h, m.x0, m.y0, m.baked, m.w * m.h, int( m.mips.size() ), double( m.bytes16f() ) / ( 1024.0 * 1024.0 ) );
	return b;
}

} // namespace FarSkyMap

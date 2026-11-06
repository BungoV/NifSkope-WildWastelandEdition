#ifndef CELLPCMB_H
#define CELLPCMB_H

/* Lane PCMB1 (2026-11-06): Fallout 4's precombined meshes, read as the game reads them. Pure C++, no Qt, so
 * tests/features/near/pcmb tests it with g++ alone. Every fact below was measured on the vanilla corpus first
 * (offline census, 120,387 meshes\precombined\*_oc.nif in Fallout4 - MeshesExtra.ba2 and Fallout4 - Geometry.csg):
 *
 * 1. A precombined NIF stores NO vertices: each shape (BSMeshLODTriShape / BSTriShape) has Data Size 0 and one
 *    BSPackedCombinedSharedGeomDataExtra in its Extra Data List. The block lists objects (file hash, stream offset)
 *    and, per object, its LOD triangle counts and the instances that place it. Parsed with nif.xml's layout every
 *    block ends exactly at its size (541 of 541 sampled).
 * 2. The geometry lives in "<plugin> - Geometry.csg" in the Data folder: "bcsg", u32 objects, u32 chunks, chunks x
 *    (u32 compressed size, u32 file offset), objects x (u32 stream offset, u64 vertex desc, u32 vertices, u32
 *    indices), then zlib chunks of 65,536 bytes uncompressed. A stream offset picks chunk off >> 16, byte off & 0xFFFF.
 *    An object is vertices x stride bytes of BSVertexData, then u16 indices.
 * 3. The object's file hash is CRC-32 (poly 0xEDB88320, init 0, NO final xor) of the lower-case file name without
 *    its extension: "fallout4 - geometry" = 3723598439.
 * 4. An instance places the object in WORLD space: world = R * (v * scale) + t, R as stored (row-major). 27,632 of
 *    27,632 instances keep every vertex inside their stored bound; the transpose keeps 7,352. The shape's own NIF
 *    transform is not applied (composed on top: 0 of 27,632).
 * 5. Index offsets count indices; LOD0 is the first c0 triangles. The cell view draws LOD0.
 * 6. XCRI on the CELL: u32 meshCount, u32 words, meshCount x u32 key, words / 2 x (u32 ref, u32 key). A mesh is
 *    meshes\precombined\<cell form 8 hex>_<key 8 hex>_oc.nif. 294 vanilla pairs name a key the list does not hold:
 *    those refs have no mesh and stay drawn. */

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace WwPcmb
{

struct BlockSpan
{
	std::string type;
	std::size_t off = 0, size = 0;
};

namespace detail
{
struct Rd
{
	const std::uint8_t * d;
	std::size_t n, o;
	bool ok = true;
	bool need( std::size_t k )
	{
		if ( !ok || o + k > n || o + k < o )
			ok = false;
		return ok;
	}
	std::uint32_t u32()
	{
		if ( !need( 4 ) )
			return 0;
		std::uint32_t v;
		std::memcpy( &v, d + o, 4 );
		o += 4;
		return v;
	}
	std::uint16_t u16()
	{
		if ( !need( 2 ) )
			return 0;
		std::uint16_t v;
		std::memcpy( &v, d + o, 2 );
		o += 2;
		return v;
	}
	std::uint64_t u64()
	{
		if ( !need( 8 ) )
			return 0;
		std::uint64_t v;
		std::memcpy( &v, d + o, 8 );
		o += 8;
		return v;
	}
	float f32()
	{
		const std::uint32_t u = u32();
		float f;
		std::memcpy( &f, &u, 4 );
		return f;
	}
	void skip( std::size_t k )
	{
		if ( need( k ) )
			o += k;
	}
	void shortString()
	{
		if ( need( 1 ) )
			skip( 1 + d[o] );
	}
};

inline float half( std::uint16_t h )
{
	const std::uint32_t s = std::uint32_t( h >> 15 ) << 31, e = ( h >> 10 ) & 0x1F, m = h & 0x3FF;
	std::uint32_t u;
	if ( e == 0 ) {
		if ( m == 0 )
			u = s;
		else {
			float f = float( m ) / 1024.0f / 16384.0f;   // subnormal: m * 2^-24
			return ( h >> 15 ) ? -f : f;
		}
	} else if ( e == 31 )
		u = s | 0x7F800000u | ( m << 13 );
	else
		u = s | ( ( e + 112 ) << 23 ) | ( m << 13 );
	float f;
	std::memcpy( &f, &u, 4 );
	return f;
}
} // namespace detail

/*! The block spans of a Fallout 4 NIF (header version 20.2.0.7, BS version >= 130): type name, byte offset and size
 *  of every block, in block order. False when the header does not parse or a block runs past the end. */
inline bool nifBlockSpans( const std::uint8_t * d, std::size_t n, std::vector<BlockSpan> & out )
{
	out.clear();
	std::size_t nl = 0;
	while ( nl < n && nl < 64 && d[nl] != '\n' )
		nl++;
	if ( nl >= n || d[nl] != '\n' )
		return false;
	detail::Rd r{ d, n, nl + 1 };
	r.u32();   // version
	r.skip( 1 );   // endian
	r.u32();   // user version
	const std::uint32_t nb = r.u32();
	const std::uint32_t bsv = r.u32();
	r.shortString();
	r.shortString();
	r.shortString();
	if ( bsv >= 130 )
		r.shortString();
	const std::uint16_t nt = r.u16();
	std::vector<std::string> types( nt );
	for ( std::uint16_t k = 0; k < nt && r.ok; k++ ) {
		const std::uint32_t len = r.u32();
		if ( !r.need( len ) )
			break;
		types[k].assign( reinterpret_cast<const char *>( d + r.o ), len );
		r.o += len;
	}
	if ( !r.ok || nb > n )
		return false;
	std::vector<std::uint16_t> ti( nb );
	std::vector<std::uint32_t> sz( nb );
	for ( std::uint32_t k = 0; k < nb; k++ )
		ti[k] = r.u16();
	for ( std::uint32_t k = 0; k < nb; k++ )
		sz[k] = r.u32();
	const std::uint32_t ns = r.u32();
	r.u32();   // max string length
	for ( std::uint32_t k = 0; k < ns && r.ok; k++ )
		r.skip( r.u32() );
	r.skip( std::size_t( r.u32() ) * 4 );   // groups
	if ( !r.ok )
		return false;
	std::size_t o = r.o;
	out.reserve( nb );
	for ( std::uint32_t k = 0; k < nb; k++ ) {
		const std::uint16_t t = ti[k] & 0x7FFF;
		if ( t >= nt || o + sz[k] > n )
			return false;
		out.push_back( { types[t], o, sz[k] } );
		o += sz[k];
	}
	return true;
}

//! One placement of a packed object (BSPackedGeomDataCombined, 72 bytes).
struct Instance
{
	float g2p = 0;
	float R[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };   // row-major as stored
	float t[3] = { 0, 0, 0 };
	float scale = 1;
	float centre[3] = { 0, 0, 0 };
	float radius = 0;
};

struct Object
{
	std::uint32_t fileHash = 0, dataOffset = 0;
	std::uint32_t nv = 0, lods = 0;
	std::uint32_t triCount[3] = { 0, 0, 0 }, triOffset[3] = { 0, 0, 0 };
	std::uint64_t vdesc = 0;
	std::vector<Instance> inst;
};

struct Packed
{
	std::uint64_t vdesc = 0;
	std::uint32_t numVerts = 0, numTris = 0;
	std::vector<Object> objs;
};

/*! A BSPackedCombinedSharedGeomDataExtra block at `off` of `size` bytes (the span from nifBlockSpans). True only when
 *  the nif.xml layout ends EXACTLY at the block's size. */
inline bool parsePackedShared( const std::uint8_t * d, std::size_t n, std::size_t off, std::size_t size, Packed & p )
{
	p = Packed();
	if ( off + size > n || off + size < off )
		return false;
	detail::Rd r{ d, off + size, off };
	r.u32();   // name
	p.vdesc = r.u64();
	p.numVerts = r.u32();
	p.numTris = r.u32();
	r.u32();   // unknown 1
	r.u32();   // unknown 2
	const std::uint32_t nd = r.u32();
	if ( !r.ok || std::size_t( nd ) * 44 > size )
		return false;
	p.objs.resize( nd );
	for ( Object & ob : p.objs ) {
		ob.fileHash = r.u32();
		ob.dataOffset = r.u32();
	}
	for ( Object & ob : p.objs ) {
		ob.nv = r.u32();
		ob.lods = r.u32();
		for ( int l = 0; l < 3; l++ ) {
			ob.triCount[l] = r.u32();
			ob.triOffset[l] = r.u32();
		}
		const std::uint32_t nc = r.u32();
		if ( !r.ok || std::size_t( nc ) * 72 > size )
			return false;
		ob.inst.resize( nc );
		for ( Instance & in : ob.inst ) {
			in.g2p = r.f32();
			for ( float & v : in.R )
				v = r.f32();
			for ( float & v : in.t )
				v = r.f32();
			in.scale = r.f32();
			for ( float & v : in.centre )
				v = r.f32();
			in.radius = r.f32();
		}
		ob.vdesc = r.u64();
	}
	return r.ok && r.o == off + size;
}

//! The packed object's file hash for a CSG file name ("Fallout4 - Geometry.csg" or its base name, any case).
inline std::uint32_t csgNameHash( const std::string & fileName )
{
	std::string s = fileName;
	const std::size_t slash = s.find_last_of( "/\\" );
	if ( slash != std::string::npos )
		s.erase( 0, slash + 1 );
	const std::size_t dot = s.find_last_of( '.' );
	if ( dot != std::string::npos )
		s.erase( dot );
	std::uint32_t c = 0;
	for ( char ch : s ) {
		const unsigned char b = ( ch >= 'A' && ch <= 'Z' ) ? ( unsigned char )( ch - 'A' + 'a' ) : ( unsigned char ) ch;
		c ^= b;
		for ( int k = 0; k < 8; k++ )
			c = ( c >> 1 ) ^ ( ( c & 1 ) ? 0xEDB88320u : 0u );
	}
	return c;
}

struct CsgChunk
{
	std::uint32_t compSize = 0, fileOffset = 0;
};

struct CsgObject
{
	std::uint32_t streamOffset = 0;
	std::uint64_t vdesc = 0;
	std::uint32_t nv = 0, numIndices = 0;
};

struct CsgDir
{
	std::vector<CsgChunk> chunks;
	std::vector<CsgObject> objs;
	std::size_t headerBytes = 0;   //!< bytes the header and both tables take
};

//! The bytes the CSG header needs before parseCsgHeader can read it: call with the first 12 bytes.
inline std::size_t csgHeaderBytes( const std::uint8_t * d, std::size_t n )
{
	if ( n < 12 || std::memcmp( d, "bcsg", 4 ) != 0 )
		return 0;
	std::uint32_t no, nc;
	std::memcpy( &no, d + 4, 4 );
	std::memcpy( &nc, d + 8, 4 );
	return 12 + std::size_t( nc ) * 8 + std::size_t( no ) * 20;
}

/*! The CSG header and both tables. `fileSize`: the whole file's size; the chunks must be contiguous from the end of
 *  the tables and end at the file's end. */
inline bool parseCsgHeader( const std::uint8_t * d, std::size_t n, std::uint64_t fileSize, CsgDir & dir )
{
	dir = CsgDir();
	const std::size_t need = csgHeaderBytes( d, n );
	if ( !need || need > n )
		return false;
	std::uint32_t no, nc;
	std::memcpy( &no, d + 4, 4 );
	std::memcpy( &nc, d + 8, 4 );
	detail::Rd r{ d, n, 12 };
	dir.chunks.resize( nc );
	for ( CsgChunk & c : dir.chunks ) {
		c.compSize = r.u32();
		c.fileOffset = r.u32();
	}
	dir.objs.resize( no );
	for ( CsgObject & o : dir.objs ) {
		o.streamOffset = r.u32();
		o.vdesc = r.u64();
		o.nv = r.u32();
		o.numIndices = r.u32();
	}
	dir.headerBytes = r.o;
	std::uint64_t at = r.o;
	for ( const CsgChunk & c : dir.chunks ) {
		if ( c.fileOffset != at )
			return false;
		at += c.compSize;
	}
	return r.ok && at == fileSize;
}

//! Bytes one object takes in the uncompressed stream: vertices then u16 indices (all LODs).
inline std::size_t objectBytes( std::uint64_t vdesc, std::uint32_t nv, std::uint32_t numIndices )
{
	return std::size_t( nv ) * std::size_t( ( vdesc & 0xF ) * 4 ) + std::size_t( numIndices ) * 2;
}

//! World-space LOD0 geometry of one shape, every object and instance appended.
struct Geometry
{
	std::vector<float> pos, nrm, tan, uv;   // 3, 3, 3, 2 per vertex
	std::vector<std::uint8_t> rgba;          // 4 per vertex (255s when the object carries none)
	std::vector<std::uint32_t> tris;         // 3 per triangle, indices into this geometry
	bool colors = false;
	std::size_t vertexCount() const { return pos.size() / 3; }
};

/*! Append `ob`'s LOD0 for every instance. `src` holds the object's bytes from the stream (objectBytes long at least;
 *  `avail` its length). `transposeRed` places with R transposed: the unit test's red, never the game's rule. */
inline bool expandObject( const Object & ob, const std::uint8_t * src, std::size_t avail, Geometry & g,
	bool transposeRed = false )
{
	const std::size_t stride = std::size_t( ( ob.vdesc & 0xF ) * 4 );
	const std::uint32_t flags = std::uint32_t( ( ob.vdesc >> 44 ) & 0xFFFF );
	const bool full = ( flags & 0x400 ) != 0;
	const bool hasUv = ( flags & 0x2 ) != 0, hasNrm = ( flags & 0x8 ) != 0, hasTan = ( flags & 0x10 ) != 0;
	const bool hasCol = ( flags & 0x20 ) != 0;
	const std::size_t uvOff = ( ( ob.vdesc >> 8 ) & 0xF ) * 4, nrmOff = ( ( ob.vdesc >> 16 ) & 0xF ) * 4;
	const std::size_t tanOff = ( ( ob.vdesc >> 20 ) & 0xF ) * 4, colOff = ( ( ob.vdesc >> 24 ) & 0xF ) * 4;
	const std::uint32_t c0 = ob.triCount[0];
	const std::size_t idxAt = std::size_t( ob.nv ) * stride + std::size_t( ob.triOffset[0] ) * 2;
	if ( !stride || ( flags & 0x1 ) == 0 || idxAt + std::size_t( c0 ) * 6 > avail )
		return false;
	if ( ( full ? 12u : 6u ) > stride || ( hasUv && uvOff + 4 > stride ) || ( hasNrm && nrmOff + 3 > stride )
		|| ( hasTan && tanOff + 3 > stride ) || ( hasCol && colOff + 4 > stride ) )
		return false;
	for ( std::uint32_t k = 0; k < c0 * 3; k++ ) {
		std::uint16_t ix;
		std::memcpy( &ix, src + idxAt + std::size_t( k ) * 2, 2 );
		if ( ix >= ob.nv )
			return false;
	}
	g.colors = g.colors || hasCol;
	for ( const Instance & in : ob.inst ) {
		float M[9];
		for ( int i = 0; i < 3; i++ )
			for ( int j = 0; j < 3; j++ )
				M[i * 3 + j] = transposeRed ? in.R[j * 3 + i] : in.R[i * 3 + j];
		const std::uint32_t base = std::uint32_t( g.vertexCount() );
		for ( std::uint32_t v = 0; v < ob.nv; v++ ) {
			const std::uint8_t * p = src + std::size_t( v ) * stride;
			float x[3];
			if ( full )
				std::memcpy( x, p, 12 );
			else
				for ( int i = 0; i < 3; i++ ) {
					std::uint16_t h;
					std::memcpy( &h, p + i * 2, 2 );
					x[i] = detail::half( h );
				}
			float nb[3] = { 0, 0, 1 }, tb[3] = { 1, 0, 0 };
			for ( int i = 0; i < 3; i++ ) {
				if ( hasNrm )
					nb[i] = float( p[nrmOff + i] ) / 127.5f - 1.0f;
				if ( hasTan )
					tb[i] = float( p[tanOff + i] ) / 127.5f - 1.0f;
			}
			for ( int i = 0; i < 3; i++ ) {
				const float * row = M + i * 3;
				g.pos.push_back( ( row[0] * x[0] + row[1] * x[1] + row[2] * x[2] ) * in.scale + in.t[i] );
				g.nrm.push_back( row[0] * nb[0] + row[1] * nb[1] + row[2] * nb[2] );
				g.tan.push_back( row[0] * tb[0] + row[1] * tb[1] + row[2] * tb[2] );
			}
			if ( hasUv ) {
				std::uint16_t u, w;
				std::memcpy( &u, p + uvOff, 2 );
				std::memcpy( &w, p + uvOff + 2, 2 );
				g.uv.push_back( detail::half( u ) );
				g.uv.push_back( detail::half( w ) );
			} else {
				g.uv.push_back( 0 );
				g.uv.push_back( 0 );
			}
			for ( int i = 0; i < 4; i++ )
				g.rgba.push_back( hasCol ? p[colOff + i] : 255 );
		}
		for ( std::uint32_t k = 0; k < c0 * 3; k++ ) {
			std::uint16_t ix;
			std::memcpy( &ix, src + idxAt + std::size_t( k ) * 2, 2 );
			g.tris.push_back( base + ix );
		}
	}
	return true;
}

/*! Which of a cell's precombined meshes this view draws, and which XCRI refs it therefore skips.
 *  `keys`: XCRI's mesh keys; `pairKey` / `pairStale`: per XCRI pair, its mesh key and whether its ref is stale (a
 *  later plugin moved, scaled, deleted or disabled it: the previs rule's `moved`); `fileOk`: per key, the NIF exists
 *  and loads. A mesh is drawn when its file is there and none of its refs is stale -- at MESH granularity, because a
 *  mesh with one stale ref still holds that ref's old geometry: drawing it and the ref would double it. `red`
 *  (WW_CELL_PCMB_RED=refs): no mesh, every ref drawn. */
struct Plan
{
	std::vector<bool> meshDrawn;   //!< per key
	std::vector<bool> skipRef;     //!< per pair: the ref is drawn by its mesh
	int drawn = 0, staleMeshes = 0, missingFiles = 0, refsSkipped = 0, orphanPairs = 0;
};

inline Plan plan( const std::vector<std::uint32_t> & keys, const std::vector<std::uint32_t> & pairKey,
	const std::vector<bool> & pairStale, const std::vector<bool> & fileOk, bool red = false )
{
	Plan p;
	p.meshDrawn.assign( keys.size(), false );
	p.skipRef.assign( pairKey.size(), false );
	std::vector<int> pairMesh( pairKey.size(), -1 );
	std::vector<bool> stale( keys.size(), false );
	for ( std::size_t k = 0; k < pairKey.size(); k++ ) {
		for ( std::size_t m = 0; m < keys.size(); m++ )
			if ( keys[m] == pairKey[k] ) {
				pairMesh[k] = int( m );
				break;
			}
		if ( pairMesh[k] < 0 )
			p.orphanPairs++;
		else if ( k < pairStale.size() && pairStale[k] )
			stale[pairMesh[k]] = true;
	}
	if ( red )
		return p;
	for ( std::size_t m = 0; m < keys.size(); m++ ) {
		const bool ok = m < fileOk.size() && fileOk[m];
		if ( !ok )
			p.missingFiles++;
		else if ( stale[m] )
			p.staleMeshes++;
		else {
			p.meshDrawn[m] = true;
			p.drawn++;
		}
	}
	for ( std::size_t k = 0; k < pairKey.size(); k++ )
		if ( pairMesh[k] >= 0 && p.meshDrawn[pairMesh[k]] ) {
			p.skipRef[k] = true;
			p.refsSkipped++;
		}
	return p;
}

} // namespace WwPcmb

#endif // CELLPCMB_H

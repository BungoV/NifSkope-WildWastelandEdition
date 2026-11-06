// Lane PCMB1: the precombined reader (src/features/near/cellpcmb.h). g++ -std=c++17 -I<tree>/src tests/features/near/pcmb/pcmb_test.cpp
// Reds (each must fail): -DPCMB_RED_TRANSPOSE places with R transposed (the census rejected it, 7,352 of 27,632 in
// bound); -DPCMB_RED_REFS is WW_CELL_PCMB_RED=refs (no mesh drawn, no ref skipped).
#include "features/near/cellpcmb.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace WwPcmb;

#if defined( PCMB_RED_TRANSPOSE )
static const bool kTranspose = true, kRefsRed = false;
#elif defined( PCMB_RED_REFS )
static const bool kTranspose = false, kRefsRed = true;
#else
static const bool kTranspose = false, kRefsRed = false;
#endif

static int fails = 0, checks = 0;
static void check( bool ok, const char * what )
{
	checks++;
	if ( !ok ) {
		fails++;
		std::printf( "FAIL %s\n", what );
	}
}
static bool near( float a, float b, float tol = 1e-4f )
{
	return std::fabs( a - b ) <= tol;
}

struct W
{
	std::vector<std::uint8_t> b;
	void u8( std::uint8_t v ) { b.push_back( v ); }
	void u16( std::uint16_t v ) { b.insert( b.end(), reinterpret_cast<std::uint8_t *>( &v ), reinterpret_cast<std::uint8_t *>( &v ) + 2 ); }
	void u32( std::uint32_t v ) { b.insert( b.end(), reinterpret_cast<std::uint8_t *>( &v ), reinterpret_cast<std::uint8_t *>( &v ) + 4 ); }
	void u64( std::uint64_t v ) { b.insert( b.end(), reinterpret_cast<std::uint8_t *>( &v ), reinterpret_cast<std::uint8_t *>( &v ) + 8 ); }
	void f32( float v ) { b.insert( b.end(), reinterpret_cast<std::uint8_t *>( &v ), reinterpret_cast<std::uint8_t *>( &v ) + 4 ); }
	void str( const char * s, bool shortStr )
	{
		const std::size_t n = std::strlen( s );
		if ( shortStr )
			u8( std::uint8_t( n ) );
		else
			u32( std::uint32_t( n ) );
		b.insert( b.end(), s, s + n );
	}
};

// vertex | uv | normal | tangent, half positions; stride 20 bytes: pos half4 @0, uv half2 @8, normal @12, tangent @16
static const std::uint64_t kDesc = 5ull | ( 2ull << 8 ) | ( 3ull << 16 ) | ( 4ull << 20 ) | ( 0x1Bull << 44 );
static const std::uint16_t H0 = 0x0000, H1 = 0x3C00, H2 = 0x4000, HHALF = 0x3800;

static std::vector<std::uint8_t> objectStream()
{
	W w;
	const std::uint16_t P[4][3] = { { H1, H0, H0 }, { H0, H1, H0 }, { H0, H0, H1 }, { H2, H0, H0 } };
	for ( int v = 0; v < 4; v++ ) {
		for ( int i = 0; i < 3; i++ )
			w.u16( P[v][i] );
		w.u16( H0 );
		w.u16( HHALF );   // uv (0.5, 1)
		w.u16( H1 );
		w.u8( 255 ); w.u8( 128 ); w.u8( 128 ); w.u8( 0 );   // normal ~ +x
		w.u8( 128 ); w.u8( 255 ); w.u8( 128 ); w.u8( 0 );   // tangent ~ +y
	}
	const std::uint16_t I[9] = { 0, 1, 2, 0, 2, 3, 1, 2, 3 };   // LOD0: 2 triangles, LOD1: 1
	for ( std::uint16_t i : I )
		w.u16( i );
	return w.b;
}

static std::vector<std::uint8_t> packedBlock( int extraBytes )
{
	W w;
	w.u32( 0 );   // name
	w.u64( kDesc );
	w.u32( 8 );   // vertices: 4 x 2 instances
	w.u32( 4 );
	w.u32( 0 );
	w.u32( 0 );
	w.u32( 1 );   // one object
	w.u32( 3723598439u );
	w.u32( 1234 );
	w.u32( 4 );   // nv
	w.u32( 2 );   // lods
	w.u32( 2 ); w.u32( 0 );   // LOD0: 2 triangles at index 0
	w.u32( 1 ); w.u32( 6 );   // LOD1: 1 triangle at index 6 (offsets count indices)
	w.u32( 0 ); w.u32( 0 );
	w.u32( 2 );   // instances
	for ( int k = 0; k < 2; k++ ) {
		w.f32( 1 );
		const float R[9] = { 0, -1, 0, 1, 0, 0, 0, 0, 1 };   // 90 degrees about z, row-major
		for ( float r : R )
			w.f32( r );
		w.f32( 10 + 100 * k ); w.f32( 20 ); w.f32( 30 );
		w.f32( 2 );   // scale
		w.f32( 10 + 100 * k ); w.f32( 20 ); w.f32( 30 ); w.f32( 5 );
	}
	w.u64( kDesc );
	for ( int k = 0; k < extraBytes; k++ )
		w.u8( 0 );
	return w.b;
}

int main()
{
	// 1. the file hash: CRC-32 init 0, no final xor, of the lower-case base name
	check( csgNameHash( "Fallout4 - Geometry.csg" ) == 3723598439u, "hash of Fallout4 - Geometry.csg is 3723598439" );
	check( csgNameHash( "Data\\fallout4 - geometry.CSG" ) == 3723598439u, "hash ignores folder, case and extension" );

	// 2. the packed block ends exactly
	Packed p;
	const std::vector<std::uint8_t> blk = packedBlock( 0 );
	check( parsePackedShared( blk.data(), blk.size(), 0, blk.size(), p ), "packed block parses and ends exactly" );
	check( p.objs.size() == 1 && p.objs[0].inst.size() == 2 && p.objs[0].fileHash == 3723598439u
			&& p.objs[0].dataOffset == 1234 && p.objs[0].triCount[0] == 2 && p.objs[0].triOffset[1] == 6
			&& p.objs[0].vdesc == kDesc && near( p.objs[0].inst[1].t[0], 110 ),
		"packed fields read back" );
	const std::vector<std::uint8_t> blk2 = packedBlock( 1 );
	Packed p2;
	check( !parsePackedShared( blk2.data(), blk2.size(), 0, blk2.size(), p2 ), "one byte past the layout is refused" );
	check( !parsePackedShared( blk.data(), blk.size(), 0, blk.size() - 1, p2 ), "one byte short is refused" );

	// 3. expansion: world = R * (v * scale) + t, LOD0 only, per-instance index base
	const std::vector<std::uint8_t> os = objectStream();
	check( objectBytes( kDesc, 4, 9 ) == os.size(), "objectBytes = nv * stride + 2 * indices" );
	Geometry g;
	check( expandObject( p.objs[0], os.data(), os.size(), g, kTranspose ), "object expands" );
	check( g.vertexCount() == 8 && g.tris.size() == 12, "2 instances x 4 vertices, 2 x 2 LOD0 triangles (LOD1 dropped)" );
	if ( g.vertexCount() == 8 && g.tris.size() == 12 ) {
		// v0 = (1,0,0): R * (2,0,0) = (0,2,0), + (10,20,30)
		check( near( g.pos[0], 10 ) && near( g.pos[1], 22 ) && near( g.pos[2], 30 ), "v0 of instance 0 at (10,22,30)" );
		// v1 = (0,1,0): R * (0,2,0) = (-2,0,0)
		check( near( g.pos[3], 8 ) && near( g.pos[4], 20 ) && near( g.pos[5], 30 ), "v1 of instance 0 at (8,20,30)" );
		check( near( g.pos[12], 110 ) && near( g.pos[13], 22 ), "v0 of instance 1 at (110,22,30)" );
		// normal +x turns to +y, tangent +y to -x (byte decode b / 127.5 - 1)
		check( near( g.nrm[0], 0.0039f, 0.01f ) && near( g.nrm[1], 1.0f, 0.01f ), "normal rotated by R" );
		check( near( g.tan[0], -1.0f, 0.01f ) && near( g.tan[1], 0.0039f, 0.01f ), "tangent rotated by R" );
		check( near( g.uv[0], 0.5f ) && near( g.uv[1], 1.0f ), "uv decoded from halves" );
		check( g.tris[0] == 0 && g.tris[5] == 3 && g.tris[6] == 4 && g.tris[11] == 7, "second instance's indices start at 4" );
		check( g.rgba[0] == 255 && !g.colors, "no colour stream: white" );
	}
	// an index past nv is refused
	std::vector<std::uint8_t> bad = os;
	bad[bad.size() - 18] = 9;   // first LOD0 index
	Geometry gb;
	check( !expandObject( p.objs[0], bad.data(), bad.size(), gb ), "an index >= nv is refused" );

	// 4. which meshes are drawn, which refs skipped
	{
		const std::vector<std::uint32_t> keys = { 0xA, 0xB, 0xD };
		const std::vector<std::uint32_t> pk = { 0xA, 0xA, 0xB, 0xC, 0xD };   // 0xC: a pair whose key is not listed
		const std::vector<bool> stale = { false, false, true, false, false };
		const std::vector<bool> fileOk = { true, true, false };
		const Plan pl = plan( keys, pk, stale, fileOk, kRefsRed );
		check( pl.meshDrawn.size() == 3 && pl.meshDrawn[0] && !pl.meshDrawn[1] && !pl.meshDrawn[2],
			"mesh A drawn, B stale (one stale ref), D missing its file" );
		check( pl.skipRef.size() == 5 && pl.skipRef[0] && pl.skipRef[1] && !pl.skipRef[2] && !pl.skipRef[3] && !pl.skipRef[4],
			"only A's refs are skipped; the orphan pair's ref stays" );
		check( pl.drawn == 1 && pl.staleMeshes == 1 && pl.missingFiles == 1 && pl.refsSkipped == 2 && pl.orphanPairs == 1,
			"plan counts" );
	}

	// 5. the NIF block walk on a minimal header
	{
		W w;
		const char * line = "Gamebryo File Format, Version 20.2.0.7\n";
		w.b.insert( w.b.end(), line, line + std::strlen( line ) );
		w.u32( 0x14020007 ); w.u8( 1 ); w.u32( 12 ); w.u32( 2 ); w.u32( 130 );
		w.str( "a", true ); w.str( "", true ); w.str( "", true ); w.str( "", true );
		w.u16( 2 ); w.str( "BSTriShape", false ); w.str( "BSPackedCombinedSharedGeomDataExtra", false );
		w.u16( 0 ); w.u16( 1 );
		w.u32( 3 ); w.u32( 5 );
		w.u32( 1 ); w.u32( 4 ); w.str( "name", false );
		w.u32( 0 );
		const std::size_t at = w.b.size();
		for ( int k = 0; k < 8; k++ )
			w.u8( 0 );
		std::vector<BlockSpan> sp;
		check( nifBlockSpans( w.b.data(), w.b.size(), sp ) && sp.size() == 2 && sp[1].type == "BSPackedCombinedSharedGeomDataExtra"
				&& sp[0].off == at && sp[1].off == at + 3 && sp[1].size == 5,
			"block spans walk the header" );
		check( !nifBlockSpans( w.b.data(), w.b.size() - 1, sp ), "a block past the end is refused" );
	}

	// 6. the CSG header: contiguous chunks ending at the file's end
	{
		W w;
		w.b.insert( w.b.end(), { 'b', 'c', 's', 'g' } );
		w.u32( 1 ); w.u32( 2 );
		const std::uint32_t hdr = 12 + 2 * 8 + 1 * 20;
		w.u32( 100 ); w.u32( hdr );
		w.u32( 50 ); w.u32( hdr + 100 );
		w.u32( 0 ); w.u64( kDesc ); w.u32( 4 ); w.u32( 9 );
		CsgDir dir;
		check( csgHeaderBytes( w.b.data(), 12 ) == hdr, "header size from the first 12 bytes" );
		check( parseCsgHeader( w.b.data(), w.b.size(), hdr + 150, dir ) && dir.chunks.size() == 2 && dir.objs.size() == 1
				&& dir.objs[0].vdesc == kDesc && dir.objs[0].numIndices == 9,
			"CSG header and tables read" );
		check( !parseCsgHeader( w.b.data(), w.b.size(), hdr + 151, dir ), "chunks not ending at the file end are refused" );
	}

	std::printf( "%d checks, %d failed\n", checks, fails );
	std::printf( fails ? "PCMB UNIT FAIL\n" : "PCMB UNIT PASS\n" );
	return fails ? 1 : 0;
}

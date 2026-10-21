#ifndef PGI_FORMAT_H
#define PGI_FORMAT_H

/*! Precomputed Global Illumination: the `.pgi` file (lane PGI0, 2026-10-13; notes/pgi1/PLAN.md section 5).
 *
 *  ONE header for both sides: NifSkope writes the file, FO4CS reads it. Plain structs with static_assert sizes,
 *  only <cstdint>/<cstddef>/<cstring>: no Qt, no NifSkope types, no CommonLib. C++17 and later.
 *
 *  Where the files live (loose, the load order decides conflicts like meshes):
 *    Data\PrecomputedGI\<Worldspace EditorID>\<X>_<Y>.pgi        kind 1, one exterior cell
 *    Data\PrecomputedGI\<Worldspace EditorID>\Far\<X>_<Y>.pgi    kind 3, one far square (16 x 16 cells)
 *    Data\PrecomputedGI\Interiors\<Cell EditorID>.pgi            kind 2, one interior
 *
 *  Layout, little endian:
 *    Header (64 B), then chunkCount DirEntry (24 B each), then the chunk bodies (each 4-byte aligned).
 *    Header::headerCrc = CRC32 (IEEE) of the header with headerCrc = 0, followed by the whole directory.
 *    DirEntry::crc = CRC32 of the chunk's stored bytes. A reader skips a chunk id it does not know.
 *    DirEntry::group = which sector the chunk belongs to (the SECT chunk of the same group). An exterior cell
 *    has one group; an interior has one group per 4096-unit square its bake wrote (the near bake's sectors).
 *    DirEntry::flags bit 0 = the stored bytes are a zlib stream (RFC 1950) of rawSize bytes; writers of
 *    version 1 store raw, the bit is there so a later writer may deflate without a version bump.
 *
 *  Version 1 chunks (kind 1 and 2). The near bake's content, losslessly (a reader can rebuild the bake's
 *  sector file bit for bit), plus the horizon:
 *    SECT  1 Sector      per group: square X/Y, the bake's file version and content flags
 *    SURF  n Surfel      32 B: the bake's surfels; up to six a 70-unit cell, one per facing bin
 *    HORZ  HorizonHead + n HorizonRecord (32 B): per surfel, 16 azimuths of horizon elevation and openness
 *    PROB  n Probe       176 B: position, link range, sky visibility and tint per octant, rooms
 *    LINK  n Link        20 B: probe -> surfel (the surfel's cell relative to the probe's, its facing bin),
 *                              direction, weight, glass tint, door
 *    ROOM  n RoomBox     32 B: enclosed-room air boxes
 *    EMIT  n Emit        16 B: glowing surfels: index into SURF and emitted light (linear)
 *  Reserved ids for later lanes (PGI1 and lodprtp1): PLUG FEED GRID OCCU DOOR LGHT BRIK PTCH PLNK PCEL.
 *
 *  Resolving a link: the probe's cell is floor(position / surfelCell) per axis (float division, then floor);
 *  the surfel is the one of the same group whose cell is probeCell + cellDelta and whose bin == Link::side.
 */

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace pgi
{

constexpr uint32_t fourcc( char a, char b, char c, char d )
{
	return uint32_t( uint8_t( a ) ) | ( uint32_t( uint8_t( b ) ) << 8 ) | ( uint32_t( uint8_t( c ) ) << 16 )
		| ( uint32_t( uint8_t( d ) ) << 24 );
}

constexpr uint32_t kMagic = fourcc( 'P', 'G', 'I', '1' );
constexpr uint16_t kVersion = 1;

enum Kind : uint16_t
{
	KindExterior = 1,
	KindInterior = 2,
	KindFar = 3,   //!< lodprtp1's far square (PTCH / PLNK / PCEL); no writer yet
};

constexpr uint32_t kSECT = fourcc( 'S', 'E', 'C', 'T' );
constexpr uint32_t kSURF = fourcc( 'S', 'U', 'R', 'F' );
constexpr uint32_t kHORZ = fourcc( 'H', 'O', 'R', 'Z' );
constexpr uint32_t kPROB = fourcc( 'P', 'R', 'O', 'B' );
constexpr uint32_t kLINK = fourcc( 'L', 'I', 'N', 'K' );
constexpr uint32_t kROOM = fourcc( 'R', 'O', 'O', 'M' );
constexpr uint32_t kEMIT = fourcc( 'E', 'M', 'I', 'T' );

constexpr uint16_t kChunkDeflated = 1;

struct Header
{
	uint32_t magic;          //!< kMagic
	uint16_t version;        //!< kVersion; any other value is refused
	uint16_t kind;           //!< Kind
	uint32_t worldHash;      //!< FNV-1a 32 of the worldspace EditorID, lower case (0 for an interior)
	int32_t cellX, cellY;    //!< kind 1: the cell; kind 3: the square's lower-left cell; kind 2: 0
	uint32_t interiorPlugin; //!< kind 2: index into PLUG (0 when absent)
	uint32_t interiorForm;   //!< kind 2: local form id of the cell (0 = not recorded)
	float surfelCell;        //!< 70: the surfel cell edge, game units
	float voxelEdge;         //!< the GRID voxel edge (70); 0 when the file has no GRID
	uint32_t toolVersion;    //!< the writer's own version (NifSkope: 1 = lane PGI0)
	uint64_t inputHash;      //!< the refs the bake saw (0 = not recorded; PGI1 fills it)
	uint64_t optionsHash;    //!< FNV-1a 64 of the bake options
	uint32_t chunkCount;
	uint32_t headerCrc;      //!< CRC32 of this header (this field 0) + the directory
};
static_assert( sizeof( Header ) == 64, "pgi header" );
static_assert( offsetof( Header, inputHash ) == 40, "pgi header inputHash" );

struct DirEntry
{
	uint32_t id;
	uint16_t group;
	uint16_t flags;
	uint32_t offset;         //!< from the start of the file
	uint32_t storedSize;     //!< bytes on disk
	uint32_t rawSize;        //!< bytes after inflation (== storedSize when not deflated)
	uint32_t crc;            //!< CRC32 of the stored bytes
};
static_assert( sizeof( DirEntry ) == 24, "pgi dir entry" );

struct Sector
{
	int32_t x, y;            //!< the 4096-unit square (an exterior: the cell)
	uint32_t bakeVersion;    //!< the near bake's sector file version the content came from (5)
	uint32_t content;        //!< what the bake modelled: 1 sides, 2 rooms, 4 doors, 8 glass, 16 six sides
	uint32_t bakeFlags;      //!< 1 = links, 2 = sky visibility
	uint32_t recordKind;     //!< 1 = resolved albedo
	uint32_t reserved[2];
};
static_assert( sizeof( Sector ) == 32, "pgi sector" );

struct Surfel
{
	float position[3];       //!< world units (the mean of the cell's hits on this side)
	int16_t normal[3];       //!< snorm16
	uint8_t albedo[3];       //!< linear unorm8
	uint8_t bin;             //!< facing bin: 0 +X, 1 -X, 2 +Y, 3 -Y, 4 +Z, 5 -Z
	uint8_t pad[2];          //!< carried as the bake wrote them
	uint32_t samples;
	uint32_t reserved;
};
static_assert( sizeof( Surfel ) == 32, "pgi surfel" );

constexpr int kAzimuths = 16;

enum HorizonSource : uint16_t
{
	HorizonSelfPlane = 1,    //!< PGI0: only the surfel's own plane (nothing traced); openness 255
	HorizonTraced = 2,       //!< PGI1: traced against the 5x5 window and the far soup
};

struct HorizonHead
{
	uint16_t azimuths;       //!< kAzimuths; a reader refuses any other count
	uint16_t source;         //!< HorizonSource
	uint32_t count;          //!< records that follow (== the group's surfel count)
};
static_assert( sizeof( HorizonHead ) == 8, "pgi horizon head" );

/*! Azimuth k points along (cos a, sin a, 0), a = k x 22.5 degrees from world +X toward +Y.
 *  elevation: the horizon's elevation at that azimuth, -90 + v x 180 / 255 degrees (0 = straight down,
 *  255 = straight up: nothing above the surfel's side is open). The sun at elevation e is above the horizon
 *  when e > that angle. openness: cosine-weighted open fraction of the wedge above the horizon (255 = all). */
struct HorizonRecord
{
	uint8_t elevation[kAzimuths];
	uint8_t openness[kAzimuths];
};
static_assert( sizeof( HorizonRecord ) == 32, "pgi horizon record" );

struct Probe
{
	float position[3];
	uint32_t linkOffset, linkCount;    //!< into the group's LINK records
	float linkWeightScale;             //!< a link's weight = Link::weight / 65535 x this (solid angle, sr)
	float coverage, unlinkedWeight;
	float skyVis[8], octantDistance[8], octantDistanceRms[8];
	uint32_t placementClass, placementLevel, placementReserved[2];
	uint8_t skyTint[8][3];             //!< per octant, the sky seen through glass (255 = clear)
	uint32_t room[2];                  //!< the probe's room; an opening's second room (0xFFFFFFFF = none)
};
static_assert( sizeof( Probe ) == 176, "pgi probe" );
static_assert( offsetof( Probe, skyTint ) == 144, "pgi probe skyTint" );

struct Link
{
	int16_t cellDelta[3];    //!< surfel cell - probe cell
	int16_t dir[2];          //!< octahedral snorm16, probe -> surfel
	uint16_t weight;         //!< unorm16 of the probe's linkWeightScale
	uint8_t side;            //!< the surfel's facing bin
	uint8_t tint[3];         //!< unorm8 glass transmittance on the way (255 = clear)
	uint32_t door;           //!< the door ref the link passes through, 0 = none
};
static_assert( sizeof( Link ) == 20, "pgi link" );

struct RoomBox
{
	uint32_t room;
	float lo[3], hi[3];
	uint32_t reserved;
};
static_assert( sizeof( RoomBox ) == 32, "pgi room box" );

struct Emit
{
	uint32_t surfel;         //!< index into the group's SURF
	float le[3];             //!< mean emitted light, linear
};
static_assert( sizeof( Emit ) == 16, "pgi emit" );

//! Record size of a fixed-record chunk (0 = not a plain record array: HORZ has a head, unknown ids).
inline uint32_t recordSize( uint32_t id )
{
	switch ( id ) {
	case kSECT: return sizeof( Sector );
	case kSURF: return sizeof( Surfel );
	case kPROB: return sizeof( Probe );
	case kLINK: return sizeof( Link );
	case kROOM: return sizeof( RoomBox );
	case kEMIT: return sizeof( Emit );
	default: return 0;
	}
}

//! CRC32 (IEEE 802.3, reflected, init and xor 0xFFFFFFFF: zlib's crc32). Chain with `crc` = the previous result.
inline uint32_t crc32( const void * data, size_t size, uint32_t crc = 0 )
{
	struct Table
	{
		uint32_t t[256];
		Table()
		{
			for ( uint32_t i = 0; i < 256; i++ ) {
				uint32_t c = i;
				for ( int k = 0; k < 8; k++ )
					c = ( c & 1u ) ? 0xEDB88320u ^ ( c >> 1 ) : c >> 1;
				t[i] = c;
			}
		}
	};
	static const Table tab;
	const uint8_t * p = static_cast<const uint8_t *>( data );
	crc = ~crc;
	for ( size_t i = 0; i < size; i++ )
		crc = tab.t[( crc ^ p[i] ) & 0xFFu] ^ ( crc >> 8 );
	return ~crc;
}

inline uint32_t fnv1a32( const char * s, size_t n )
{
	uint32_t h = 2166136261u;
	for ( size_t i = 0; i < n; i++ ) {
		char c = s[i];
		if ( c >= 'A' && c <= 'Z' )
			c = char( c - 'A' + 'a' );
		h = ( h ^ uint8_t( c ) ) * 16777619u;
	}
	return h;
}

inline uint64_t fnv1a64( const void * data, size_t n, uint64_t h = 14695981039346656037ull )
{
	const uint8_t * p = static_cast<const uint8_t *>( data );
	for ( size_t i = 0; i < n; i++ )
		h = ( h ^ p[i] ) * 1099511628211ull;
	return h;
}

//! The header CRC: the header with headerCrc = 0, then the directory.
inline uint32_t headerCrc( const Header & h, const DirEntry * dir )
{
	Header z = h;
	z.headerCrc = 0;
	const uint32_t c = crc32( &z, sizeof z );
	return crc32( dir, size_t( h.chunkCount ) * sizeof( DirEntry ), c );
}

enum Refusal : int
{
	Ok = 0,
	RefusedShort,        //!< smaller than a header, or than its directory
	RefusedMagic,
	RefusedVersion,
	RefusedKind,
	RefusedHeaderCrc,    //!< header or directory damaged
	RefusedChunkRange,   //!< a chunk outside the file, or unaligned
	RefusedChunkCrc,     //!< a chunk's bytes damaged
	RefusedRecordSize,   //!< a known chunk whose size is not a whole number of its records
	RefusedHorizon,      //!< HORZ: azimuth count not kAzimuths, or its count and size disagree
};

inline const char * refusalName( int r )
{
	switch ( r ) {
	case Ok: return "ok";
	case RefusedShort: return "short";
	case RefusedMagic: return "magic";
	case RefusedVersion: return "version";
	case RefusedKind: return "kind";
	case RefusedHeaderCrc: return "header-crc";
	case RefusedChunkRange: return "chunk-range";
	case RefusedChunkCrc: return "chunk-crc";
	case RefusedRecordSize: return "record-size";
	case RefusedHorizon: return "horizon";
	default: return "unknown";
	}
}

/*! Check a whole file in memory: header, directory, every chunk's range and CRC, record sizes of known raw chunks,
 *  the horizon head. No allocation. `data` need not be aligned. */
inline Refusal checkFile( const void * data, size_t size )
{
	const uint8_t * b = static_cast<const uint8_t *>( data );
	if ( size < sizeof( Header ) )
		return RefusedShort;
	Header h;
	std::memcpy( &h, b, sizeof h );
	if ( h.magic != kMagic )
		return RefusedMagic;
	if ( h.version != kVersion )
		return RefusedVersion;
	if ( h.kind < KindExterior || h.kind > KindFar )
		return RefusedKind;
	const uint64_t dirEnd = uint64_t( sizeof( Header ) ) + uint64_t( h.chunkCount ) * sizeof( DirEntry );
	if ( dirEnd > size )
		return RefusedShort;
	{
		Header z = h;
		z.headerCrc = 0;
		uint32_t c = crc32( &z, sizeof z );
		c = crc32( b + sizeof( Header ), size_t( dirEnd - sizeof( Header ) ), c );
		if ( c != h.headerCrc )
			return RefusedHeaderCrc;
	}
	for ( uint32_t i = 0; i < h.chunkCount; i++ ) {
		DirEntry e;
		std::memcpy( &e, b + sizeof( Header ) + size_t( i ) * sizeof( DirEntry ), sizeof e );
		if ( e.offset < dirEnd || ( e.offset & 3u ) || uint64_t( e.offset ) + e.storedSize > size )
			return RefusedChunkRange;
		if ( !( e.flags & kChunkDeflated ) && e.storedSize != e.rawSize )
			return RefusedChunkRange;
		if ( crc32( b + e.offset, e.storedSize ) != e.crc )
			return RefusedChunkCrc;
		const uint32_t rs = recordSize( e.id );
		if ( rs && e.rawSize % rs )
			return RefusedRecordSize;
		if ( e.id == kHORZ && !( e.flags & kChunkDeflated ) ) {
			if ( e.storedSize < sizeof( HorizonHead ) )
				return RefusedHorizon;
			HorizonHead hh;
			std::memcpy( &hh, b + e.offset, sizeof hh );
			if ( hh.azimuths != kAzimuths
				|| uint64_t( sizeof( HorizonHead ) ) + uint64_t( hh.count ) * sizeof( HorizonRecord ) != e.rawSize )
				return RefusedHorizon;
		}
	}
	return Ok;
}

} // namespace pgi

#endif // PGI_FORMAT_H

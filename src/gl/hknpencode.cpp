#include "hknpencode.h"

#include <QtEndian>
#include <QHash>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <functional>

namespace {

void appendU32( QByteArray & out, quint32 value )
{
	value = qToLittleEndian( value ); out.append( reinterpret_cast<const char *>( &value ), 4 );
}

void appendU64( QByteArray & out, quint64 value )
{
	value = qToLittleEndian( value ); out.append( reinterpret_cast<const char *>( &value ), 8 );
}

void setU16( QByteArray & out, qsizetype offset, quint16 value )
{
	value = qToLittleEndian( value ); std::memcpy( out.data() + offset, &value, 2 );
}

void setU32( QByteArray & out, qsizetype offset, quint32 value )
{
	value = qToLittleEndian( value ); std::memcpy( out.data() + offset, &value, 4 );
}

void setFloat( QByteArray & out, qsizetype offset, float value )
{
	setU32( out, offset, std::bit_cast<quint32>( value ) );
}

void pad( QByteArray & out, int alignment, char fill = 0 )
{
	while ( out.size() % alignment ) out.append( fill );
}

QByteArray hkArray( quint32 count )
{
	QByteArray out; appendU64( out, 0 ); appendU32( out, count );
	appendU32( out, count | 0x80000000u ); return out;
}

struct GlobalFix { quint32 source, section, target; };
struct VirtualFix { quint32 object, section, name; };

struct Fixups
{
	QVector<QPair<quint32, quint32>> local;
	QVector<GlobalFix> global;
	QVector<VirtualFix> virtuals;

	/* Each table is padded to 16 with 0xff and carries NO sentinel entry -- the
	 * section header is what bounds it. Verified on all 37 vanilla ragdolls: the
	 * local table is exactly 8n rounded up to 16, the other two 12n rounded up,
	 * and every padding byte is 0xff.
	 *
	 * This wrote a full 0xffffffff entry instead until 07-29e. That reads the same
	 * to a parser -- 0xffffffff is the -1 one stops on -- but it is 20 bytes more
	 * than any vanilla file carries, which is what stopped a reassembled ragdoll
	 * from matching byte for byte.
	 */
	QByteArray localTable() const
	{
		QByteArray out; for ( const auto & f : local ) { appendU32( out, f.first ); appendU32( out, f.second ); }
		pad( out, 16, char( 0xff ) ); return out;
	}
	/*! \param reserveBytes vanilla's table length, when it is longer than needed.
	 *
	 *  Grow-only: an edited system needing more entries than the original reserved
	 *  keeps the computed minimum. See HknpSystem::globalFixupBytes for why this
	 *  cannot be derived from anything in the file.
	 */
	QByteArray globalTable( quint32 reserveBytes = 0 ) const
	{
		QByteArray out; for ( const GlobalFix & f : global ) {
			appendU32( out, f.source ); appendU32( out, f.section ); appendU32( out, f.target );
		}
		pad( out, 16, char( 0xff ) );
		while ( quint32( out.size() ) < reserveBytes )
			out.append( char( 0xff ) );		// the same 0xff the pad already uses
		return out;
	}
	QByteArray virtualTable() const
	{
		QByteArray out; for ( const VirtualFix & f : virtuals ) {
			appendU32( out, f.object ); appendU32( out, f.section ); appendU32( out, f.name );
		}
		pad( out, 16, char( 0xff ) ); return out;
	}
};

/*! Friction and restitution are stored as the TOP 16 BITS of the float, ROUNDED.
 *
 * Rounded, not truncated, which is measured rather than assumed: across 1,500
 * SetDressing files every discriminating value in the corpus is the rounded
 * word and none is the truncated one — restitution 0.4 is 0x3ECD 1,623 times
 * and 0x3ECC never, friction 0.3 is 0x3E9A 13 times and 0x3E99 never, 0.8 is
 * 0x3F4D 17 times and 0x3F4C never. Truncating, which is what this did until
 * 2026-08-20, wrote a word one ULP low on every body whose value has anything
 * in the discarded half — including the 0.4 restitution that is the Fallout 4
 * default.
 *
 * Ties go to even, the IEEE default. Nothing in the corpus lands on a tie, so
 * that half is convention and not measurement.
 *
 * A value read back off an existing packfile has a zero low half, so rounding
 * and truncation agree on it — which is why the assembler that reproduces 810
 * of 822 stock systems byte for byte is unaffected by this.
 */
quint16 roundFloat16( float value )
{
	const quint32 bits = std::bit_cast<quint32>( value );
	const quint32 low = bits & 0xffffu;
	quint32 high = bits >> 16;
	if ( low > 0x8000u || ( low == 0x8000u && ( high & 1u ) ) )
		high++;
	return quint16( high );
}

QByteArray bodyProperties( const HknpEncodeInput & in )
{
	QByteArray out( 0x50, 0 );
	setU16( out, 0x10, 0xff00 );
	setU16( out, 0x12, roundFloat16( in.friction ) );
	setU16( out, 0x14, roundFloat16( in.friction ) );
	setU16( out, 0x16, roundFloat16( in.restitution ) );
	out[0x18] = 1; out[0x19] = 2; setU16( out, 0x1a, 0x3d4c ); setU32( out, 0x1c, 0x7f7fffee );
	setFloat( out, 0x20, 1.0f ); setFloat( out, 0x24, 1.0f ); setU16( out, 0x38, 0x40a0 );
	return out;
}

QByteArray bodyCInfo( const HknpEncodeInput & in )
{
	QByteArray out( 0x60, 0 );
	setU32( out, 0x08, 0x7fffffffu ); setU32( out, 0x0c, in.dynamic ? 0u : 0x7fffffffu );
	setU32( out, 0x10, 0xffu );
	setU32( out, 0x14, ( in.layer & 0xffu ) | ( quint32( in.filterFlags ) << 8 ) | ( quint32( in.filterGroup ) << 16 ) );
	setFloat( out, 0x30, in.center[0] ); setFloat( out, 0x34, in.center[1] ); setFloat( out, 0x38, in.center[2] );
	setU32( out, 0x4c, 0x3f7fffffu );
	return out;
}

QByteArray dynamicMotion( const HknpEncodeInput & in )
{
	QByteArray out( 0x40, 0 );
	setFloat( out, 0x08, in.gravityFactor ); setFloat( out, 0x0c, 1.0f );
	setFloat( out, 0x10, in.maxLinVelocity ); setFloat( out, 0x14, in.maxAngVelocity );
	setFloat( out, 0x18, in.linDamping ); setFloat( out, 0x1c, in.angDamping );
	/* Corpus words, not float literals. 0.4905f and 0.0025f each land one ULP
	 * off what every vanilla motion record carries, which is three bytes of
	 * difference for nothing -- and the three words at +0x34 are the same on all
	 * 857 corpus bodies and were being left zero.
	 */
	setU32( out, 0x20, 0x3e2e147bu ); setU32( out, 0x24, 0x3efb22d2u );
	setU32( out, 0x28, 0x3b23d70bu ); setU32( out, 0x2c, 0x3b23d70bu ); setFloat( out, 0x30, 1.0f );
	setU32( out, 0x34, 0x999b67ffu ); setU32( out, 0x38, 0x06757304u ); setU32( out, 0x3c, 0x00000073u );
	return out;
}

/*! One dyn_inertia record: 0x70 bytes, NOT the 0x40 this wrote until 2026-08-16.
 *
 * The stride is measured twice over -- the decoder reads 0x70 (1568 bytes over
 * 14 motions) and hknpEncodePhysicsSystemData writes 0x70 -- so a 0x40 record
 * left the engine reading the last 48 bytes of every dynamic body's inertia
 * from whatever followed the array. It stayed invisible because a static body
 * has no inertia array at all, and static is what most compiles produce.
 *
 * The centre of mass at +0x30 and the identity orientation at +0x40 are what
 * the assembler writes for a body whose motion frame is unrotated.
 */
QByteArray dynamicInertia( const HknpEncodeInput & in )
{
	QByteArray out( 0x70, 0 ); setU32( out, 0x00, 0x00010000u );
	float mass = std::max( in.mass, 0.001f );
	Vector3 mn = in.verts.first(), mx = mn;
	for ( const Vector3 & v : in.verts ) for ( int a = 0; a < 3; a++ ) { mn[a] = std::min( mn[a], v[a] ); mx[a] = std::max( mx[a], v[a] ); }
	Vector3 d = mx - mn;
	double volume = 0.0;
	for ( const Triangle & t : in.tris ) volume += double( Vector3::dotproduct( in.verts[t[0]], Vector3::crossproduct( in.verts[t[1]], in.verts[t[2]] ) ) ) / 6.0;
	setFloat( out, 0x04, 1.0f / mass ); setFloat( out, 0x08, float( mass / std::max( std::fabs( volume ), 1.0e-8 ) ) );
	setU32( out, 0x0c, 0x5f7ffff0u ); setU32( out, 0x10, 0x5f7ffff0u );
	/* +0x20 takes INVERSE inertia, matching +0x04's inverse mass.
	 *
	 * in.inertia is the true tensor, as bhkRigidBody's Inertia Tensor field
	 * supplies it, so it is reciprocated here, and the box fallback is computed
	 * the same way round then inverted. Writing the true tensor into this slot --
	 * which is what this did until 07-28p -- stores a physically different
	 * quantity. It stayed invisible because the same wrong convention was used on
	 * the way in, and because nothing dynamic has been encoded yet.
	 */
	auto inv = []( float v ) { return ( v > 1.0e-12f ) ? 1.0f / v : 0.0f; };
	float ix = in.inertia[0] > 0.0f ? in.inertia[0] : mass * ( d[1] * d[1] + d[2] * d[2] ) / 12.0f;
	float iy = in.inertia[1] > 0.0f ? in.inertia[1] : mass * ( d[0] * d[0] + d[2] * d[2] ) / 12.0f;
	float iz = in.inertia[2] > 0.0f ? in.inertia[2] : mass * ( d[0] * d[0] + d[1] * d[1] ) / 12.0f;
	setFloat( out, 0x20, inv( ix ) ); setFloat( out, 0x24, inv( iy ) );
	setFloat( out, 0x28, inv( iz ) ); setFloat( out, 0x2c, 1.0f );
	// centre of mass, then the unrotated motion frame as a unit quaternion
	for ( int a = 0; a < 3; a++ ) setFloat( out, 0x30 + a * 4, in.center[a] );
	setFloat( out, 0x4c, 1.0f );
	return out;
}

/*! The 32-byte wrapper a shape hangs its mass properties or its material off.
 *
 * One local fixup +0x00 -> +0x10, one global at +0x10, and the ONLY thing that
 * differs between the two flavours is `key`: 0x0000f100 when it points at
 * hknpShapeMassProperties, 0x0000f601 when it points at hknpBSMaterialProperties.
 * Measured on 80 corpus objects with no exceptions.
 */
QByteArray refCountedProperties( quint32 key = 0x0000f100u )
{
	QByteArray out( 0x20, 0 ); setU32( out, 0x08, 1 ); setU32( out, 0x0c, 0x80000001u ); setU32( out, 0x18, key ); return out;
}

/*! hknpBSMaterialProperties: 0x20 of header, then ONE 0x18 ENTRY PER MATERIAL.
 *
 * The array's payload pointer is at +0x10 and needs a local fixup to +0x20 --
 * this wrote a count with no pointer until 2026-08-16, which is a null
 * dereference the moment the engine reads the material table. An entry carries
 * a 1 at +0x10 and the material CRC at +0x14.
 *
 * The STRIDE is 0x18, measured 2026-08-20 against the vanilla tables that hold
 * more than one entry: Toilet01's three CRCs sit at object +0x34, +0x4c and
 * +0x64, which is 0x18 apart, not the 0x20 a single-entry file cannot tell it
 * from. One entry is 0x38 bytes that pad to 0x40 -- which is why the
 * single-material writer was right by accident, and why nothing caught this
 * until a second entry had to go in.
 */
QByteArray materialProperties( const QVector<quint32> & materials )
{
	const qsizetype count = std::max<qsizetype>( materials.size(), 1 );
	QByteArray out( 0x20 + count * 0x18, 0 );
	setU32( out, 0x18, quint32( count ) ); setU32( out, 0x1c, 0x80000000u | quint32( count ) );
	for ( qsizetype i = 0; i < count; i++ ) {
		setU32( out, 0x20 + i * 0x18 + 0x10, 1 );
		setU32( out, 0x20 + i * 0x18 + 0x14, i < materials.size() ? materials.at( i ) : 0u );
	}
	return out;
}

/*! hknpCompressedMeshShape, 0xc0 bytes.
 *
 * \param material    the shape's material CRC, at +0x18
 * \param numKeys     maxKey + 1 -- the two hkBitFields are sized by KEY SPACE,
 *                    quadIsFlat numKeys/2 bits and triangleIsInterior numKeys,
 *                    which is what makes IndMachine's 1401/2802 come out (its
 *                    792 primitives pad to 256 per section in the key space)
 * \param bitsPerKey  shape key width, which must equal the data object's
 *
 * Everything here beyond the material was zero until 2026-08-16, and the zeros
 * were not neutral. Measured constant across all 41 corpus shapes: +0x44 and
 * +0x54 are the "does not own its memory" flag on two empty hkArrays, and +0x58
 * is 0xffffffff. The two hkBitFields at +0x68 / +0x80 are present in EVERY
 * vanilla file; the caller supplies their payloads and the fixups.
 *
 * numShapeKeyBits at +0x12 was the literal 7 regardless of the mesh. The
 * engine uses it to split a key between this shape and its child, so a wrong
 * value indexes the wrong primitive.
 */
QByteArray compressedMeshHeader( quint32 material, quint32 numKeys, quint32 bitsPerKey )
{
	QByteArray out( 0xc0, 0 ); out[0x10] = 4; out[0x11] = 2; out[0x12] = char( bitsPerKey ); out[0x13] = 2;
	setU32( out, 0x18, material ); setU32( out, 0x30, 0xffffffffu );
	setU32( out, 0x44, 0x80000000u ); setU32( out, 0x54, 0x80000000u ); setU32( out, 0x58, 0xffffffffu );
	const quint32 flatWords = ( numKeys / 2 + 31 ) / 32, interiorWords = ( numKeys + 31 ) / 32;
	setU32( out, 0x70, flatWords ); setU32( out, 0x74, 0x80000000u | flatWords ); setU32( out, 0x78, numKeys / 2 );
	setU32( out, 0x88, interiorWords ); setU32( out, 0x8c, 0x80000000u | interiorWords );
	setU32( out, 0x90, numKeys );
	return out;
}

/*! Both hkcd trees' compressed AABB nibbles, decoded 2026-08-17 from Elric
 *  reference pairs (a decompiled vanilla mesh, perturbed one vertex at a time,
 *  recompiled by Elric and diffed).
 *
 * A bound byte holds two 4-bit insets, (loInset << 4) | hiInset, one byte per
 * axis in x,y,z order. The nibble is on a SQUARE-ROOT scale:
 *
 *   nibble    = floor( sqrt( inset / parentSpan ) * 15 )
 *   dequant   = (nibble / 15)^2 * parentSpan
 *
 * and the fractions are measured in the PARENT's dequantized box, so the boxes
 * requantize hierarchically. Floor makes the dequantized inset never exceed the
 * true inset: every node's box CONTAINS its geometry by construction. Verified
 * against vanilla: 82.7%% of 38,808 per-section nibbles byte-exact, the rest
 * +-1 from Elric's float32 arithmetic; section-tree leaves 44/61 exact with the
 * same +-1 fringe. (The zeros written here before were the nibble rule's "whole
 * parent box" case applied everywhere -- conservative, never selective.)
 */
struct TreeBox { Vector3 lo, hi; };

quint8 insetNibble( float inset, float span )
{
	if ( span <= 0.0f || inset <= 0.0f )
		return 0;
	float f = std::min( inset / span, 1.0f );
	return quint8( std::clamp( int( std::sqrt( f ) * 15.0f + 1.0e-6f ), 0, 15 ) );
}

//! Compress `box` against `parent` into 3 axis bytes; `box` becomes the
//! dequantized result so children requantize against what a reader will see.
void encodeTreeBounds( quint8 * xyz, TreeBox & box, const TreeBox & parent )
{
	for ( int a = 0; a < 3; a++ ) {
		const float span = parent.hi[a] - parent.lo[a];
		const quint8 lo = insetNibble( box.lo[a] - parent.lo[a], span );
		const quint8 hi = insetNibble( parent.hi[a] - box.hi[a], span );
		xyz[a] = quint8( lo << 4 ) | hi;
		box.lo[a] = parent.lo[a] + ( lo / 15.0f ) * ( lo / 15.0f ) * span;
		box.hi[a] = parent.hi[a] - ( hi / 15.0f ) * ( hi / 15.0f ) * span;
	}
}

/*! The 4-byte node tree each section carries at its +0x00, and the reason
 *  compiled collision took the game down.
 *
 * The engine walks this tree to find candidate primitives; a section whose node
 * array is empty (which is what the compile path wrote) is a null traversal
 * inside Havok's mesh tree. Decoded from the corpus, confirmed on all 63
 * sections in it -- every one enumerates each of its primitives exactly once:
 *
 *   byte 3, bit 0 set  -> internal node. The left child is the next node; the
 *                         right child is this node + (byte3 & 0xfe), which is
 *                         2 * (leaves in the left subtree).
 *   byte 3, bit 0 clear-> leaf. The primitive index is byte3 >> 1, SECTION
 *                         relative, which is why a section holds at most 128
 *                         primitives: 127 is the largest index a byte can carry.
 *   bytes 0..2         -> the compressed bounds above.
 */
void appendMeshTree( QByteArray & out, QVector<int> & prims,
	const QVector<TreeBox> & boxes, int first, int count, const TreeBox & parent )
{
	TreeBox own = boxes.at( prims.at( first ) );
	for ( int i = first + 1; i < first + count; i++ ) {
		const TreeBox & b = boxes.at( prims.at( i ) );
		for ( int a = 0; a < 3; a++ ) { own.lo[a] = std::min( own.lo[a], b.lo[a] ); own.hi[a] = std::max( own.hi[a], b.hi[a] ); }
	}
	quint8 xyz[3];
	encodeTreeBounds( xyz, own, parent );
	out.append( char( xyz[0] ) ); out.append( char( xyz[1] ) ); out.append( char( xyz[2] ) );
	if ( count == 1 ) {
		out.append( char( prims.at( first ) * 2 ) );
		return;
	}
	/* Split on the widest axis of the true box, at the median, so the two sides
	 * hold as close to the same number of primitives as the split allows. Left
	 * takes the floor half, which keeps the right-child offset inside a byte:
	 * it is 2 * leftLeaves, and half of 128 leaves is 64.
	 */
	int axis = 0;
	for ( int a = 1; a < 3; a++ ) if ( own.hi[a] - own.lo[a] > own.hi[axis] - own.lo[axis] ) axis = a;
	auto mid = [&]( int p ) { return boxes.at( p ).lo[axis] + boxes.at( p ).hi[axis]; };
	std::sort( prims.begin() + first, prims.begin() + first + count,
		[&]( int a, int b ) { return mid( a ) < mid( b ); } );
	const int left = count / 2;
	out.append( char( ( left * 2 ) | 1 ) );
	appendMeshTree( out, prims, boxes, first, left, own );
	appendMeshTree( out, prims, boxes, first + left, count - left, own );
}

/*! The tree over SECTIONS at hknpCompressedMeshShapeData +0x10.
 *
 * FIVE bytes a node, not four -- the array's payload gap proves the stride
 * (7 nodes occupy 48 bytes, 11 occupy 64, 21 occupy 112; all pad16(5n), none
 * pad16(4n)) -- and Elric reproduces vanilla's trees byte-identically, so the
 * codec below was read off reference pairs:
 *
 *   bytes 0..2          the same compressed bounds as the mesh tree
 *   bytes 3..4 (u16 LE) data & 0x80 -> internal: left child next, right child
 *                       at self + 2*(data >> 8) (the high byte is the left
 *                       subtree's leaf count); else leaf, and the high byte is
 *                       the SECTION INDEX -- leaves sit in spatial order and
 *                       name their section, not the other way round.
 *
 * The high byte caps the left subtree at 255 leaves, so a balanced build is
 * safe to 511 sections -- the writer refuses far earlier (65,535 verts).
 */
void appendSectionTree( QByteArray & out, QVector<int> & secs,
	const QVector<TreeBox> & boxes, int first, int count, const TreeBox & parent )
{
	TreeBox own = boxes.at( secs.at( first ) );
	for ( int i = first + 1; i < first + count; i++ ) {
		const TreeBox & b = boxes.at( secs.at( i ) );
		for ( int a = 0; a < 3; a++ ) { own.lo[a] = std::min( own.lo[a], b.lo[a] ); own.hi[a] = std::max( own.hi[a], b.hi[a] ); }
	}
	quint8 xyz[3];
	encodeTreeBounds( xyz, own, parent );
	out.append( char( xyz[0] ) ); out.append( char( xyz[1] ) ); out.append( char( xyz[2] ) );
	if ( count == 1 ) {
		out.append( char( 0 ) ); out.append( char( secs.at( first ) ) );
		return;
	}
	int axis = 0;
	for ( int a = 1; a < 3; a++ ) if ( own.hi[a] - own.lo[a] > own.hi[axis] - own.lo[axis] ) axis = a;
	auto mid = [&]( int s ) { return boxes.at( s ).lo[axis] + boxes.at( s ).hi[axis]; };
	std::sort( secs.begin() + first, secs.begin() + first + count,
		[&]( int a, int b ) { return mid( a ) < mid( b ); } );
	const int left = count / 2;
	out.append( char( 0x80 ) ); out.append( char( left ) );
	appendSectionTree( out, secs, boxes, first, left, own );
	appendSectionTree( out, secs, boxes, first + left, count - left, own );
}

/*! hknpCompressedMeshShapeData +0xb8: two 0x60-byte entries, ONE distinct
 *  payload across all 41 corpus shapes, so it is emitted verbatim. The content
 *  is pairs of +/-FLT_MAX vectors -- empty-AABB initialisers.
 */
QByteArray meshDataTailConstant()
{
	// twelve 16-byte rows: + is +FLT_MAX x4, - is -FLT_MAX x4, and row 6 is zero
	static const char rows[] = "+-+-+-0+-+-+";
	QByteArray out;
	for ( const char row : QByteArray( rows ) ) {
		const quint32 word = row == '+' ? 0x7f7fffeeu : row == '-' ? 0xff7fffeeu : 0u;
		for ( int i = 0; i < 4; i++ ) appendU32( out, word );
	}
	return out;
}

struct ClassEntry { quint32 hash; const char * name; };

QByteArray classNames( QHash<QString, quint32> & offsets )
{
	static const ClassEntry entries[] = {
		{ 0x33d42383u, "hkClass" }, { 0xb0efa719u, "hkClassMember" },
		{ 0x8a3609cfu, "hkClassEnum" }, { 0xce6f8a6cu, "hkClassEnumItem" },
		// order of FIRST USE, which is what hknpBuildPackfile does and what all 41
		// corpus files carry; the data object used to be listed before the two
		// property classes, which no vanilla file does
		{ 0xb857718bu, "hknpPhysicsSystemData" }, { 0x5f60d536u, "hknpCompressedMeshShape" },
		{ 0x7c574867u, "hkRefCountedProperties" }, { 0xa3e47a9au, "hknpBSMaterialProperties" },
		{ 0xa2bdfc59u, "hknpCompressedMeshShapeData" }
	};
	QByteArray out;
	for ( const ClassEntry & e : entries ) {
		QByteArray name( e.name ); offsets.insert( QString::fromLatin1( e.name ), quint32( out.size() + 5 ) );
		appendU32( out, e.hash ); out.append( char( 0x09 ) ); out.append( name ); out.append( '\0' );
	}
	pad( out, 16, char( 0xff ) ); return out;
}

QByteArray sectionHeader( const char * name, quint32 start, quint32 localFix,
	quint32 globalFix, quint32 virtualFix, quint32 end )
{
	// the name is NUL-padded through +0x12, with 0xff in +0x13 alone -- true of
	// all 111 section headers in the corpus. Filling the whole field with 0xff
	// and terminating after the name, which this did until 07-29e, differs from
	// vanilla from the first section header onwards.
	QByteArray out( 0x40, 0 );
	QByteArray n( name ); std::memcpy( out.data(), n.constData(), std::min( n.size(), qsizetype( 0x13 ) ) );
	out[0x13] = char( 0xff );
	setU32( out, 0x14, start ); setU32( out, 0x18, localFix - start ); setU32( out, 0x1c, globalFix - start );
	setU32( out, 0x20, virtualFix - start ); setU32( out, 0x24, end - start ); setU32( out, 0x28, end - start ); setU32( out, 0x2c, end - start );
	for ( int i = 0x30; i < 0x40; i++ ) out[i] = char( 0xff );
	return out;
}

QByteArray fileHeader( quint32 physicsNameOffset )
{
	QByteArray out( 0x40, 0 ); const QByteArray magic = QByteArray::fromHex( "57e0e05710c0c010" ); std::memcpy( out.data(), magic.constData(), 8 );
	setU32( out, 0x0c, 11 ); out[0x10] = 8; out[0x11] = 1; out[0x13] = 1;
	setU32( out, 0x14, 3 ); setU32( out, 0x18, 2 ); setU32( out, 0x20, 0 ); setU32( out, 0x24, physicsNameOffset );
	QByteArray version = QByteArray::fromHex( "686b5f323031342e312e302d723100ff" ); std::memcpy( out.data() + 0x28, version.constData(), 16 );
	setU32( out, 0x3c, 21 ); return out;
}

} // namespace

QByteArray hknpEncodeCompressedMesh( const HknpEncodeInput & in, QString * error )
{
	auto fail = [error]( const QString & message ) { if ( error ) *error = message; return QByteArray(); };
	if ( in.verts.isEmpty() || in.tris.isEmpty() ) return fail( QStringLiteral( "Collision has no mesh geometry." ) );
	if ( in.verts.size() > 0xFFFF )
		return fail( QStringLiteral( "Collision meshes support at most 65,535 vertices." ) );
	for ( const Triangle & t : in.tris ) if ( t[0] >= in.verts.size() || t[1] >= in.verts.size() || t[2] >= in.verts.size() )
		return fail( QStringLiteral( "Collision contains an invalid triangle index." ) );
	if ( !in.triMaterial.isEmpty() && in.triMaterial.size() != in.tris.size() )
		return fail( QStringLiteral( "Collision material list does not match the triangle count." ) );

	/* The shape's material table: the distinct per-triangle CRCs in FIRST-USE
	 * order, which is the order the vanilla tables run in. Without a per-triangle
	 * list every triangle takes in.materialCRC and the table holds one entry,
	 * which is what this wrote until 2026-08-20.
	 *
	 * A run record names its material in one byte, so 256 is the ceiling. Refuse
	 * past it rather than fold two materials together silently.
	 */
	QVector<quint32> materials;
	QVector<quint16> triMat( in.tris.size(), 0 );
	{
		const bool perTriangle = in.triMaterial.size() == in.tris.size();
		QHash<quint32, quint16> index;
		for ( qsizetype i = 0; i < in.tris.size(); i++ ) {
			const quint32 crc = perTriangle ? in.triMaterial.at( i ) : in.materialCRC;
			const auto have = index.constFind( crc );
			if ( have != index.constEnd() ) {
				triMat[i] = *have;
				continue;
			}
			if ( materials.size() >= 256 )
				return fail( QStringLiteral( "Collision uses more than 256 distinct Havok materials." ) );
			const quint16 slot = quint16( materials.size() );
			index.insert( crc, slot ); materials.append( crc ); triMat[i] = slot;
		}
	}

	/* Adjacent triangles pair into QUADS first, the way every Elric output
	 * does (89%% of vanilla primitives are quads). A primitive (a,b,c,d)
	 * decodes to (a,b,c) and (a,c,d) -- hknpdecode's own expansion -- so two
	 * triangles sharing an edge in opposite directions merge exactly: rotate
	 * the first so the shared edge is c->a, the second contributes d. Pairing
	 * halves the primitive count before the 128-per-section cap is even near
	 * (vanilla's 208-triangle rubble fits ONE section because of it), and the
	 * greedy pass prefers the flattest partner, which is also what keeps
	 * quadIsFlat meaningful. Triangles left over stay degenerate quads (d==c).
	 */
	struct MeshPrim { quint16 v[4]; bool flat; quint16 mat; };
	QVector<MeshPrim> prims;
	{
		auto normalOf = [&]( const Triangle & t ) {
			return Vector3::crossproduct( in.verts[t[1]] - in.verts[t[0]],
										  in.verts[t[2]] - in.verts[t[0]] );
		};
		// directed edge (u,v) -> triangles that traverse it, with the edge slot
		QHash<quint64, QVector<QPair<int, int>>> byEdge;
		auto ekey = []( quint16 u, quint16 v ) { return quint64( u ) << 16 | v; };
		for ( int i = 0; i < in.tris.size(); i++ ) {
			const Triangle & t = in.tris.at( i );
			for ( int e = 0; e < 3; e++ )
				byEdge[ekey( t[e], t[( e + 1 ) % 3] )].append( { i, e } );
		}
		QVector<bool> used( in.tris.size(), false );
		for ( int i = 0; i < in.tris.size(); i++ ) {
			if ( used.at( i ) )
				continue;
			const Triangle & t1 = in.tris.at( i );
			const Vector3 n1 = normalOf( t1 );
			int bestTri = -1, bestEdge = -1, bestSlot = -1;
			float bestDot = -2.0f;
			for ( int e = 0; e < 3; e++ ) {
				const quint16 u = t1[e], w = t1[( e + 1 ) % 3];
				for ( const auto & cand : byEdge.value( ekey( w, u ) ) ) {
					if ( cand.first == i || used.at( cand.first ) )
						continue;
					// a quad carries ONE material, so two triangles that do not
					// agree on theirs cannot merge into one primitive
					if ( triMat.at( cand.first ) != triMat.at( i ) )
						continue;
					const Triangle & t2 = in.tris.at( cand.first );
					const quint16 d = t2[( cand.second + 2 ) % 3];
					if ( d == t1[( e + 2 ) % 3] )
						continue;   // the same triangle mirrored: a zero-volume quad
					const Vector3 n2 = normalOf( t2 );
					const float len = n1.length() * n2.length();
					const float dot = len > 0.0f ? Vector3::dotproduct( n1, n2 ) / len : -1.0f;
					if ( dot > bestDot ) {
						bestDot = dot; bestTri = cand.first; bestEdge = e; bestSlot = cand.second;
					}
				}
			}
			MeshPrim p;
			if ( bestTri >= 0 ) {
				/* shared edge is t1's (u -> w); the quad reads (a,b,c,d) with
				 * a = w, b = t1's third vert, c = u, d = t2's third vert, so the
				 * decode (a,b,c)+(a,c,d) reproduces both triangles exactly. */
				const quint16 u = t1[bestEdge], w = t1[( bestEdge + 1 ) % 3];
				p.v[0] = w; p.v[1] = t1[( bestEdge + 2 ) % 3]; p.v[2] = u;
				p.v[3] = in.tris.at( bestTri )[( bestSlot + 2 ) % 3];
				used[bestTri] = true;
				/* flat = the four corners are coplanar. Measured semantics
				 * (WW_CHANGES 2026-08-17): flat=1 quads sit at plane distance
				 * ~1e-18, bent ones at 0.03+. 1e-4 m keeps a bent quad from ever
				 * being flagged as one plane, which is the failure that matters.
				 */
				const Vector3 n = normalOf( Triangle( p.v[0], p.v[1], p.v[2] ) );
				const float nl = n.length();
				p.flat = nl > 0.0f && std::fabs( Vector3::dotproduct( n, in.verts[p.v[3]] - in.verts[p.v[0]] ) ) / nl < 1.0e-4f;
			} else {
				p.v[0] = t1[0]; p.v[1] = t1[1]; p.v[2] = t1[2]; p.v[3] = t1[2];
				p.flat = false;
			}
			used[i] = true;
			p.mat = triMat.at( i );
			prims.append( p );
		}
	}

	/* One hknp section holds at most 255 packed vertices and 128 primitives.
	 *
	 * The vertex bound is the u8 count field. The primitive bound is the mesh
	 * tree's: a leaf node stores its primitive index doubled in one byte, so 127
	 * is the highest index it can name. The corpus agrees -- the largest section
	 * in it holds 113 primitives, and nothing exceeds 128.
	 *
	 * Larger meshes are partitioned into sections: primitives are sorted into
	 * spatial slabs along the longest axis so consecutive prims share verts, then
	 * greedily packed until either budget would overflow. Verts shared between
	 * sections are simply duplicated (the shared-vertex table stays unused,
	 * like every Elric sample we decoded).
	 */
	struct MeshSec { QVector<int> prims; QVector<quint16> verts; QHash<quint16, quint8> vmap; };
	QVector<MeshSec> secs;
	{
		Vector3 gmn = in.verts.first(), gmx = gmn;
		for ( const Vector3 & v : in.verts )
			for ( int a = 0; a < 3; a++ ) { gmn[a] = std::min( gmn[a], v[a] ); gmx[a] = std::max( gmx[a], v[a] ); }
		const Vector3 ext = gmx - gmn;
		int axis = 0;
		if ( ext[1] > ext[axis] ) axis = 1;
		if ( ext[2] > ext[axis] ) axis = 2;
		QVector<int> order( prims.size() );
		for ( int i = 0; i < order.size(); i++ ) order[i] = i;
		std::sort( order.begin(), order.end(), [&]( int a, int b ) {
			const MeshPrim & pa = prims.at( a ), & pb = prims.at( b );
			float ca = 0.0f, cb = 0.0f;
			for ( int k = 0; k < 3; k++ ) { ca += in.verts[pa.v[k]][axis]; cb += in.verts[pb.v[k]][axis]; }
			return ca < cb;
		} );
		MeshSec cur;
		auto flush = [&]() { if ( !cur.prims.isEmpty() ) { secs.append( cur ); cur = MeshSec(); } };
		for ( int pi : order ) {
			const MeshPrim & p = prims.at( pi );
			quint16 uniq[4]; int nu = 0;
			for ( int c = 0; c < 4; c++ ) {
				bool seen = false;
				for ( int k = 0; k < nu; k++ ) seen = seen || uniq[k] == p.v[c];
				if ( !seen ) uniq[nu++] = p.v[c];
			}
			int newVerts = 0;
			for ( int k = 0; k < nu; k++ )
				if ( !cur.vmap.contains( uniq[k] ) ) newVerts++;
			if ( cur.prims.size() >= 128 || cur.vmap.size() + newVerts > 255 )
				flush();
			for ( int k = 0; k < nu; k++ ) {
				if ( !cur.vmap.contains( uniq[k] ) ) {
					cur.vmap.insert( uniq[k], quint8( cur.verts.size() ) );
					cur.verts.append( uniq[k] );
				}
			}
			cur.prims.append( pi );
		}
		flush();
	}
	/* The section tree's internal nodes store their left subtree's leaf count in
	 * one byte, so a balanced build is safe to 511 sections -- 65k triangles,
	 * beyond what the vertex refusal above admits anyway. (The one-section-only
	 * refusal that lived here died 2026-08-17, when Elric reference pairs gave
	 * up the section-tree codec -- see appendSectionTree.)
	 */
	if ( secs.size() > 511 )
		return fail( QStringLiteral( "Collision mesh needs more than 511 hknp sections. Use Decimate first." ) );

	QHash<QString, quint32> names; QByteArray cn = classNames( names ); Fixups fx; QByteArray data;
	auto write = [&data]( const QByteArray & bytes ) { quint32 at = quint32( data.size() ); data.append( bytes ); return at; };
	quint32 psd = quint32( data.size() ); fx.virtuals.append( { psd, 0, names.value( QStringLiteral( "hknpPhysicsSystemData" ) ) } );
	// the first array is empty AND has a zero capacity word in every corpus file,
	// unlike the other empty ones, which carry the don't-deallocate flag
	write( QByteArray( 16, 0 ) ); quint32 arr10 = write( hkArray( 1 ) ); quint32 arr20 = write( hkArray( in.dynamic ? 1 : 0 ) );
	quint32 arr30 = write( hkArray( in.dynamic ? 1 : 0 ) ); quint32 arr40 = write( hkArray( 1 ) ); write( hkArray( 0 ) ); quint32 arr60 = write( hkArray( 1 ) ); write( QByteArray( 16, 0 ) );
	quint32 props = write( bodyProperties( in ) ); fx.local.append( { arr10, props } );
	if ( in.dynamic ) { quint32 motion = write( dynamicMotion( in ) ); fx.local.append( { arr20, motion } ); quint32 inertia = write( dynamicInertia( in ) ); fx.local.append( { arr30, inertia } ); }
	quint32 cinfo = write( bodyCInfo( in ) ); fx.local.append( { arr40, cinfo } );
	quint32 shapeEntry = write( QByteArray( 16, 0 ) ); fx.local.append( { arr60, shapeEntry } );
	/* Shape keys: (section << primBits) | primKey, where a primKey indexes
	 * triangles two per quad. primBits covers the BUSIEST section's key range,
	 * the section field the section count, and maxKey is the last section's
	 * last triangle. Measured on the corpus: IndMachine (11 sections, max 121
	 * prims) carries bits 4+8=12 and numKeys 2802 = maxKey+1; the single-section
	 * degenerate is the old 2*prims-1 rule exactly.
	 */
	const quint32 numPrims = quint32( prims.size() );
	quint32 maxSecPrims = 0;
	for ( const MeshSec & sc : std::as_const( secs ) ) maxSecPrims = std::max( maxSecPrims, quint32( sc.prims.size() ) );
	auto bitLen = []( quint32 v ) { quint32 n = 0; while ( v ) { n++; v >>= 1; } return n; };
	const quint32 primBits = bitLen( 2 * maxSecPrims - 1 );
	const quint32 secBits = bitLen( quint32( secs.size() ) - 1 );
	const quint32 bitsPerKey = secBits + primBits;
	const quint32 maxKey = ( quint32( secs.size() - 1 ) << primBits ) | ( 2 * quint32( secs.last().prims.size() ) - 1 );
	quint32 shape = write( compressedMeshHeader( in.materialCRC, maxKey + 1, bitsPerKey ) );
	fx.virtuals.append( { shape, 0, names.value( QStringLiteral( "hknpCompressedMeshShape" ) ) } );
	fx.global.append( { cinfo, 2, shape } ); fx.global.append( { shapeEntry, 2, shape } );
	/* The two hkBitFields. quadIsFlat carries the pairing pass's verdicts, one
	 * bit per KEY-SPACE primitive slot: (section << (primBits-1)) | localPrim,
	 * the indexing measured on 2,965 vanilla quads. triangleIsInterior stays
	 * zero -- its semantics are still open, and zero keeps every edge
	 * collidable.
	 */
	quint32 flatWords = ( ( maxKey + 1 ) / 2 + 31 ) / 32, interiorWords = ( maxKey + 1 + 31 ) / 32;
	QByteArray flatBits( qsizetype( flatWords ) * 4, 0 );
	for ( qsizetype s = 0; s < secs.size(); s++ ) {
		const MeshSec & sc = secs.at( s );
		for ( qsizetype k = 0; k < sc.prims.size(); k++ ) {
			if ( !prims.at( sc.prims.at( k ) ).flat )
				continue;
			const quint32 bit = ( quint32( s ) << ( primBits - 1 ) ) | quint32( k );
			flatBits[bit / 8] = char( quint8( flatBits.at( bit / 8 ) ) | ( 1u << ( bit % 8 ) ) );
		}
	}
	quint32 flatAt = write( flatBits );
	quint32 interiorAt = write( QByteArray( qsizetype( interiorWords ) * 4, 0 ) );
	fx.local.append( { shape + 0x68, flatAt } ); fx.local.append( { shape + 0x80, interiorAt } );
	pad( data, 16 );
	// 0xf601 is the key for a material table; 0xf100 names mass properties, and
	// this pointed a mass-properties key at hknpBSMaterialProperties until 08-16
	quint32 ref = write( refCountedProperties( 0x0000f601u ) ); fx.virtuals.append( { ref, 0, names.value( QStringLiteral( "hkRefCountedProperties" ) ) } );
	fx.local.append( { ref, ref + 0x10 } ); fx.global.append( { shape + 0x20, 2, ref } ); pad( data, 16 );
	quint32 mat = write( materialProperties( materials ) ); fx.global.append( { ref + 0x10, 2, mat } );
	fx.virtuals.append( { mat, 0, names.value( QStringLiteral( "hknpBSMaterialProperties" ) ) } );
	fx.local.append( { mat + 0x10, mat + 0x20 } );   // the material array's own payload
	pad( data, 16 );

	Vector3 mn = in.verts.first(), mx = mn;
	for ( const Vector3 & v : in.verts ) for ( int a = 0; a < 3; a++ ) { mn[a] = std::min( mn[a], v[a] ); mx[a] = std::max( mx[a], v[a] ); }

	// per-section emission: each section quantizes against its OWN domain
	// (that is the point of sections — precision scales with density), quads
	// hold section-relative u8 indices, packed verts concatenate per section
	QByteArray quads, packed, sectionsBlob, treeBlob, materialRuns;
	QHash<QByteArray, quint32> runIndex;   // a section's run block -> where it landed
	QVector<quint32> treeAt;              // each section's tree, relative to treeBlob
	QVector<TreeBox> secBoxes;            // STORED-coordinate content box per section
	quint32 firstPrim = 0, totalPacked = 0;
	for ( const MeshSec & sc : std::as_const( secs ) ) {
		Vector3 smn = in.verts.at( sc.verts.first() ), smx = smn;
		for ( quint16 gv : sc.verts )
			for ( int a = 0; a < 3; a++ ) { smn[a] = std::min( smn[a], in.verts.at( gv )[a] ); smx[a] = std::max( smx[a], in.verts.at( gv )[a] ); }
		const Vector3 step( smx[0] > smn[0] ? ( smx[0] - smn[0] ) / 2047.0f : 1.0f,
			smx[1] > smn[1] ? ( smx[1] - smn[1] ) / 2047.0f : 1.0f,
			smx[2] > smn[2] ? ( smx[2] - smn[2] ) / 1023.0f : 1.0f );
		for ( int pi : sc.prims ) {
			const MeshPrim & p = prims.at( pi );
			for ( int c = 0; c < 4; c++ )
				quads.append( char( sc.vmap.value( p.v[c] ) ) );
		}
		/* The tree bounds cover the STORED geometry, so the boxes are computed
		 * from the quantized coordinates a reader will decode -- using the source
		 * floats would clip by up to a quantization step.
		 */
		QVector<Vector3> stored( sc.verts.size() );
		for ( qsizetype k = 0; k < sc.verts.size(); k++ ) {
			auto quant = []( float value, float base, float scale, int maximum ) { return std::clamp( int( std::lround( ( value - base ) / scale ) ), 0, maximum ); };
			const Vector3 & v = in.verts.at( sc.verts.at( k ) );
			quint32 x = quint32( quant( v[0], smn[0], step[0], 2047 ) ), y = quint32( quant( v[1], smn[1], step[1], 2047 ) ), z = quint32( quant( v[2], smn[2], step[2], 1023 ) );
			appendU32( packed, x | ( y << 11 ) | ( z << 22 ) );
			stored[k] = Vector3( smn[0] + x * step[0], smn[1] + y * step[1], smn[2] + z * step[2] );
		}
		/* This section's mesh tree: one leaf per primitive, 2n-1 nodes, the
		 * structure the engine walks to reach a triangle at all. The root's
		 * parent is the section domain, which is why every corpus root reads
		 * 00 00 00.
		 */
		QVector<TreeBox> primBoxes( sc.prims.size() );
		QVector<int> order( sc.prims.size() );
		for ( int i = 0; i < sc.prims.size(); i++ ) {
			const MeshPrim & p = prims.at( sc.prims.at( i ) );
			TreeBox & b = primBoxes[i];
			b.lo = b.hi = stored.at( sc.vmap.value( p.v[0] ) );
			for ( int c = 1; c < 4; c++ ) {
				const Vector3 & v = stored.at( sc.vmap.value( p.v[c] ) );
				for ( int a = 0; a < 3; a++ ) { b.lo[a] = std::min( b.lo[a], v[a] ); b.hi[a] = std::max( b.hi[a], v[a] ); }
			}
			order[i] = i;
		}
		TreeBox domain{ smn, smx };
		treeAt.append( quint32( treeBlob.size() ) );
		/* The tree REORDERS the section's primitives spatially; the quad and
		 * flat-bit tables were already emitted in sc.prims order, so the leaf
		 * indices the tree hands back must stay THAT order. appendMeshTree
		 * sorts `order` in place but its leaves store the original position.
		 */
		appendMeshTree( treeBlob, order, primBoxes, 0, int( order.size() ), domain );
		const quint32 nodes = quint32( sc.prims.size() ) * 2 - 1;
		TreeBox content = primBoxes.first();
		for ( const TreeBox & b : std::as_const( primBoxes ) )
			for ( int a = 0; a < 3; a++ ) { content.lo[a] = std::min( content.lo[a], b.lo[a] ); content.hi[a] = std::max( content.hi[a], b.hi[a] ); }
		secBoxes.append( content );

		QByteArray section( 0x60, 0 );
		setU32( section, 0x08, nodes ); setU32( section, 0x0c, 0x80000000u | nodes );
		for ( int a = 0; a < 3; a++ ) { setFloat( section, 0x10 + a * 4, smn[a] ); setFloat( section, 0x20 + a * 4, smx[a] ); setFloat( section, 0x30 + a * 4, smn[a] ); setFloat( section, 0x3c + a * 4, step[a] ); }
		setU32( section, 0x48, totalPacked );
		/* This section's material runs: consecutive primitives that agree on a
		 * material, written as [u8 material][u8 0][u8 firstPrimitive][u8 count].
		 * The start is SECTION-RELATIVE and the runs cover the section exactly --
		 * measured 2026-08-20 on 3,898 vanilla sections, every one of which
		 * starts at 0 and sums to its own primitive count. Byte 1 is zero on all
		 * 9,536 of their run records.
		 *
		 * Identical blocks are SHARED rather than repeated: replaying vanilla
		 * with first-use dedup reproduces 2,472 of 2,490 tables byte for byte
		 * (the other 18 share harder still, overlapping sub-blocks).
		 */
		QByteArray block;
		for ( qsizetype k = 0; k < sc.prims.size(); ) {
			const quint16 m = prims.at( sc.prims.at( k ) ).mat;
			qsizetype j = k;
			while ( j < sc.prims.size() && prims.at( sc.prims.at( j ) ).mat == m ) j++;
			block.append( char( quint8( m ) ) ); block.append( char( 0 ) );
			block.append( char( quint8( k ) ) ); block.append( char( quint8( j - k ) ) );
			k = j;
		}
		quint32 firstRun = runIndex.value( block, 0xffffffffu );
		if ( firstRun == 0xffffffffu ) {
			firstRun = quint32( materialRuns.size() / 4 );
			runIndex.insert( block, firstRun ); materialRuns.append( block );
		}
		// firstSharedIndex << 8 | low byte, and the low byte is numPacked in
		// every vanilla section (measured on the billboard's 7 and the whole
		// corpus); no shared vertices are written, so firstShared stays 0
		setU32( section, 0x4c, quint32( sc.verts.size() ) );
		setU32( section, 0x50, ( firstPrim << 8 ) | quint32( sc.prims.size() ) );
		/* +0x54 is (firstRunIndex << 8) | runCount -- the same packing as +0x50's
		 * (firstPrimitive << 8) | primitiveCount, decoded 2026-08-20 off
		 * Res01PlayerHouseInterior, whose ten sections read 0x0001, 0x0101,
		 * 0x0201, 0x0303, 0x0601 ... The literal 1 written here was the
		 * one-run-per-section case mistaken for a constant.
		 */
		setU32( section, 0x54, ( firstRun << 8 ) | quint32( block.size() / 4 ) );
		section[0x58] = char( sc.verts.size() );
		sectionsBlob += section;
		firstPrim += quint32( sc.prims.size() );
		totalPacked += quint32( sc.verts.size() );
	}

	/* hknpCompressedMeshShapeData. The header is 0xd0, not the 0xa0 written until
	 * 2026-08-16 -- three members past +0xa0 were simply missing, and the payload
	 * that used to start at +0xa0 was overlapping them.
	 *
	 * Payload order follows the corpus: section tree, sections, quads, packed
	 * vertices, the material-run table, then the constant tail.
	 */
	const QByteArray tail = meshDataTailConstant();
	const quint32 sectionTree = quint32( secs.size() ) * 2 - 1;
	// the tree over the sections, 5 bytes a node; its root's parent is the
	// global domain, so a single section is five zero bytes
	QByteArray sectionTreeBlob;
	{
		TreeBox global{ mn, mx };
		QVector<int> secOrder( secs.size() );
		for ( int i = 0; i < secOrder.size(); i++ ) secOrder[i] = i;
		appendSectionTree( sectionTreeBlob, secOrder, secBoxes, 0, int( secOrder.size() ), global );
	}
	/* Payload offsets, in the corpus's order. The sections and the constant tail
	 * hold hkVector4s and are 16-aligned, as they are in every vanilla file.
	 */
	auto align16 = []( quint32 at ) { return ( at + 15 ) & ~15u; };
	quint32 shapeData = quint32( data.size() );
	quint32 sectionTreeData = shapeData + 0xd0;
	quint32 treeData = sectionTreeData + quint32( sectionTreeBlob.size() );
	quint32 sections = align16( treeData + quint32( treeBlob.size() ) );
	quint32 quadData = sections + quint32( sectionsBlob.size() );
	quint32 vertData = quadData + quint32( quads.size() );
	quint32 runData = vertData + quint32( packed.size() );
	quint32 tailData = align16( runData + quint32( materialRuns.size() ) );
	QByteArray sd( 0xd0, 0 );
	setU32( sd, 0x18, sectionTree ); setU32( sd, 0x1c, 0x80000000u | sectionTree );
	// the domain corners carry w = 1; they were written with w = 0
	for ( int a = 0; a < 3; a++ ) { setFloat( sd, 0x20 + a * 4, mn[a] ); setFloat( sd, 0x30 + a * 4, mx[a] ); }
	setFloat( sd, 0x2c, 1.0f ); setFloat( sd, 0x3c, 1.0f );
	setU32( sd, 0x40, maxKey + 1 ); setU32( sd, 0x44, bitsPerKey ); setU32( sd, 0x48, maxKey );
	setU32( sd, 0x58, quint32( secs.size() ) ); setU32( sd, 0x5c, 0x80000000u | quint32( secs.size() ) );
	setU32( sd, 0x68, numPrims ); setU32( sd, 0x6c, 0x80000000u | numPrims );
	setU32( sd, 0x7c, 0x80000000u ); setU32( sd, 0x88, totalPacked ); setU32( sd, 0x8c, 0x80000000u | totalPacked ); setU32( sd, 0x9c, 0x80000000u );
	// the run array is sized in RUNS, not sections: with one material they are
	// the same number, which is what hid this while only one was ever written
	const quint32 runCount = quint32( materialRuns.size() / 4 );
	setU32( sd, 0xa8, runCount ); setU32( sd, 0xac, 0x80000000u | runCount );
	setU32( sd, 0xc0, 2 ); setU32( sd, 0xc4, 0x80000002u );
	write( sd ); fx.virtuals.append( { shapeData, 0, names.value( QStringLiteral( "hknpCompressedMeshShapeData" ) ) } ); fx.global.append( { shape + 0x60, 2, shapeData } );
	fx.local.append( { shapeData + 0x10, sectionTreeData } );
	fx.local.append( { shapeData + 0x50, sections } ); fx.local.append( { shapeData + 0x60, quadData } );
	fx.local.append( { shapeData + 0x80, vertData } ); fx.local.append( { shapeData + 0xa0, runData } );
	fx.local.append( { shapeData + 0xb8, tailData } );
	// each section points at its own mesh tree
	for ( qsizetype s = 0; s < treeAt.size(); s++ )
		fx.local.append( { sections + quint32( s ) * 0x60, treeData + treeAt.at( s ) } );
	write( sectionTreeBlob ); write( treeBlob );
	while ( quint32( data.size() ) < sections ) data.append( '\0' );
	write( sectionsBlob ); write( quads ); write( packed ); write( materialRuns );
	while ( quint32( data.size() ) < tailData ) data.append( '\0' );
	write( tail ); pad( data, 16 );

	/* Global fixups are grouped by the object they live in and, inside it, by
	 * member offset -- the ordering hknpBuildPackfile reproduces on 810 vanilla
	 * files. Sources here are absolute data offsets, so one sort by source says
	 * both. Emitted in write order, the data object's pointer landed after the
	 * material's and the file no longer matched what the assembler would write.
	 */
	std::stable_sort( fx.global.begin(), fx.global.end(),
		[]( const GlobalFix & a, const GlobalFix & b ) { return a.source < b.source; } );
	QByteArray local = fx.localTable(), global = fx.globalTable(), virtuals = fx.virtualTable();
	quint32 cnStart = 0x100, cnEnd = cnStart + quint32( cn.size() ), dataStart = cnEnd;
	quint32 localAbs = dataStart + quint32( data.size() ), globalAbs = localAbs + quint32( local.size() ), virtualAbs = globalAbs + quint32( global.size() );
	quint32 dataEnd = virtualAbs + quint32( virtuals.size() );
	QByteArray out = fileHeader( names.value( QStringLiteral( "hknpPhysicsSystemData" ) ) );
	out += sectionHeader( "__classnames__", cnStart, cnEnd, cnEnd, cnEnd, cnEnd );
	out += sectionHeader( "__types__", cnEnd, cnEnd, cnEnd, cnEnd, cnEnd );
	out += sectionHeader( "__data__", dataStart, localAbs, globalAbs, virtualAbs, dataEnd );
	out += cn; out += data; out += local; out += global; out += virtuals;
	if ( error ) error->clear();
	return out;
}

/*! Write one hknpCapsuleShape.
 *
 * The object is a fixed 432 bytes and most of it is constant: measured across the
 * 778 capsules in Fallout 4's actor skeletons, the flag word at +0x10, the four
 * hkRelArray descriptors, the 24-byte face table, the 24-byte index table and the
 * two unused plane slots are each ONE distinct value corpus-wide.
 *
 * The stored hull is NOT the capsule. It is a small core box, and the real solid
 * is that box with every support plane pushed out by convexRadius -- the same
 * offset convention hknpShapeMassProperties uses -- which is why the box is tiny.
 * The core is an OBB about the segment, padded by radius/99 on all THREE local
 * axes, so the shape's true perpendicular half-width is radius * (1 + 1/99).
 * Measured 0.01010091 to 0.01010104 of the radius across the corpus.
 *
 * That is an OBB, not an AABB. The distinction is invisible on the brahmin, whose
 * 39 capsules are all axis-aligned, and it matters: 195 of the 778 are tilted off
 * the world axes, and the AABB reading misplaces their corners by up to 17 mm,
 * against 4.8e-07 m worst error for the OBB.
 *
 * Local frame, every one of them measured 778 of 778:
 *   - bit 0 of the vertex index is the capsule axis, set = toward capA
 *   - bits 1 and 2 are the perpendiculars u and v, with u x v = -e0 (left-handed)
 *   - face planes run +u, -v, +v, -u, +e0, -e0, which is also what the constant
 *     index table implies -- derived independently and in agreement
 *   - planes are (n, d) with n.x + d = 0 on the face
 */
QByteArray hknpEncodeCapsuleShape( const HknpCapsuleInput & in )
{
	QByteArray out( 0x1b0, 0 );

	setU32( out, 0x10, 0x010001c3u );
	setFloat( out, 0x14, in.radius );
	setU32( out, 0x18, in.materialCRC );

	// hkRelArray is a u16 count then a u16 byte offset relative to the field
	setU16( out, 0x30, 8 );  setU16( out, 0x32, 0x0040 );   // vertices at +0x70
	setU16( out, 0x40, 8 );  setU16( out, 0x42, 0x00b0 );   // planes   at +0xf0
	setU16( out, 0x44, 6 );  setU16( out, 0x46, 0x012c );   // faces    at +0x170
	setU16( out, 0x48, 24 ); setU16( out, 0x4a, 0x0148 );   // indices  at +0x190

	for ( int k = 0; k < 3; k++ ) {
		setFloat( out, 0x50 + k * 4, in.capA[k] );
		setFloat( out, 0x60 + k * 4, in.capB[k] );
	}
	setFloat( out, 0x5c, 1.0f );
	setFloat( out, 0x6c, 1.0f );

	const Vector3 ab = in.capB - in.capA;
	const float length = ab.length();
	const Vector3 axis = ( length > 1.0e-12f ) ? ab / length : Vector3( 0.0f, 0.0f, 1.0f );

	const Vector3 e0 = -axis;	// bit 0 set points at capA
	Vector3 e1;
	if ( in.hasFrame ) {
		e1 = in.frameU - e0 * Vector3::dotproduct( in.frameU, e0 );
	} else {
		// no authored roll to honour, so take the world axis least aligned with
		// the capsule -- the standard choice, and the best conditioned one
		int least = 0;
		for ( int k = 1; k < 3; k++ )
			if ( std::fabs( e0[k] ) < std::fabs( e0[least] ) ) least = k;
		Vector3 pick;
		pick[least] = 1.0f;
		e1 = pick - e0 * Vector3::dotproduct( pick, e0 );
	}
	const float e1len = e1.length();
	e1 = ( e1len > 1.0e-9f ) ? e1 / e1len : Vector3( 1.0f, 0.0f, 0.0f );
	const Vector3 e2 = Vector3::crossproduct( axis, e1 );	// gives u x v = -e0

	const float padding = ( in.padding > 0.0f ) ? in.padding : in.radius / 99.0f;
	const Vector3 center = ( in.capA + in.capB ) * 0.5f;
	const float halfLength = length * 0.5f + padding;

	for ( int i = 0; i < 8; i++ ) {
		const Vector3 p = center
			+ e0 * ( ( i & 1 ) ? halfLength : -halfLength )
			+ e1 * ( ( i & 2 ) ? padding : -padding )
			+ e2 * ( ( i & 4 ) ? padding : -padding );
		for ( int k = 0; k < 3; k++ )
			setFloat( out, 0x70 + i * 16 + k * 4, p[k] );
		// w is 0.5 carrying the vertex index in the low mantissa byte
		setU32( out, 0x70 + i * 16 + 12, 0x3f000000u | quint32( i ) );
	}

	const Vector3 normal[6] = { e1, -e2, e2, -e1, e0, -e0 };
	const float extent[6] = { padding, padding, padding, padding, halfLength, halfLength };
	for ( int p = 0; p < 6; p++ ) {
		for ( int k = 0; k < 3; k++ )
			setFloat( out, 0xf0 + p * 16 + k * 4, normal[p][k] );
		setFloat( out, 0xf0 + p * 16 + 12,
			-( Vector3::dotproduct( normal[p], center ) + extent[p] ) );
	}
	// the array is sized 8; the two spare slots hold -FLT_MAX in every file seen
	setU32( out, 0xf0 + 6 * 16 + 12, 0xff7fffeeu );
	setU32( out, 0xf0 + 7 * 16 + 12, 0xff7fffeeu );

	for ( int f = 0; f < 6; f++ ) {
		setU16( out, 0x170 + f * 4, quint16( f * 4 ) );
		out[0x170 + f * 4 + 2] = 4;	// vertices in the loop
		out[0x170 + f * 4 + 3] = 4;	// flags, constant corpus-wide
	}

	static const quint8 loops[24] = {
		7, 6, 2, 3,   3, 2, 0, 1,   7, 5, 4, 6,
		1, 0, 4, 5,   1, 5, 7, 3,   2, 6, 4, 0
	};
	for ( int i = 0; i < 24; i++ )
		out[0x190 + i] = char( loops[i] );

	return out;
}

/*! Write one hknpSphereShape.
 *
 * Measured across the 23 spheres in Fallout 4's actor skeletons: always 128 bytes,
 * and only three things vary -- the radius at +0x14, the material at +0x18 and the
 * centre. Everything else is one value corpus-wide.
 *
 * A sphere carries NO plane, face or index arrays; the vertex payload starts right
 * where a polytope's plane descriptor would be, at +0x40. That is the trap the
 * decoder documents: reading +0x40/+0x44/+0x48 as relArrays on a sphere reinterprets
 * vertex floats as counts.
 *
 * The single vertex is repeated four times for SIMD, and all four carry index 0 in
 * the w mantissa rather than 0..3 -- they are the same vertex, not four corners.
 * Unlike a capsule there is no core box, so the solid is exactly a sphere of
 * convexRadius and nothing needs deriving.
 */
QByteArray hknpEncodeSphereShape( const Vector3 & centre, float radius, quint32 materialCRC )
{
	QByteArray out( 0x80, 0 );

	setU32( out, 0x10, 0x01000111u );
	setFloat( out, 0x14, radius );
	setU32( out, 0x18, materialCRC );

	setU16( out, 0x30, 4 );	// four copies of one vertex
	setU16( out, 0x32, 0x0010 );	// payload at +0x40

	for ( int i = 0; i < 4; i++ ) {
		for ( int k = 0; k < 3; k++ )
			setFloat( out, 0x40 + i * 16 + k * 4, centre[k] );
		setU32( out, 0x40 + i * 16 + 12, 0x3f000000u );	// 0.5, vertex index 0
	}
	return out;
}

QByteArray hknpEncodeConvexShape( const QVector<Vector3> & verts, float convexRadius,
	quint32 materialCRC, quint32 shapeFlags )
{
	const int nv = int( verts.size() );
	if ( nv < 1 || nv > 0xffff )
		return QByteArray();

	const qsizetype vertsAt = 0x40;
	QByteArray out( ( ( vertsAt + qsizetype( nv ) * 16 ) + 15 ) / 16 * 16, 0 );
	setU32( out, 0x10, shapeFlags );
	setFloat( out, 0x14, convexRadius );
	setU32( out, 0x18, materialCRC );
	// the vertex hkRelArray, and the only array this class has
	setU16( out, 0x30, quint16( nv ) );
	setU16( out, 0x32, quint16( vertsAt - 0x30 ) );
	for ( int i = 0; i < nv; i++ ) {
		for ( int k = 0; k < 3; k++ )
			setFloat( out, vertsAt + i * 16 + k * 4, verts.at( i )[k] );
		/* w is 0.5 carrying the slot's OWN index. Unlike a polytope there are no
		 * padding slots and no index is ever repeated -- checked on all 17.
		 */
		setU32( out, vertsAt + i * 16 + 12, 0x3f000000u | quint32( i & 0xff ) );
	}
	return out;
}

namespace {

/*! Write an hkPackedVector3: three int16 mantissas over a shared exponent.
 *
 * The inverse of hknpdecode's packedVector3. The exponent is chosen so the largest
 * component fills the int16 range, and is stored in the fourth slot as
 * (E + 96) << 7 with its low seven bits zero.
 *
 * frexp gives exactly the exponent wanted: it returns a mantissa in [0.5, 1), so
 * every component divided by 2^E lands in (-1, 1) and scales into range without a
 * separate search. The clamp still matters -- a component just under 1.0 can round
 * to 32768, which is not an int16.
 */
void setPackedVector3( QByteArray & out, qsizetype offset, const Vector3 & v )
{
	float largest = 0.0f;
	for ( int k = 0; k < 3; k++ )
		largest = std::max( largest, std::fabs( v[k] ) );

	int exponent = 0;
	if ( largest > 0.0f )
		std::frexp( largest, &exponent );
	else
		exponent = -96;	// the zero vector: any exponent works, pick the low end

	const float scale = std::ldexp( 1.0f, -exponent ) * 32768.0f;
	for ( int k = 0; k < 3; k++ ) {
		const int q = std::clamp( int( std::lround( v[k] * scale ) ), -32768, 32767 );
		setU16( out, offset + k * 2, quint16( qint16( q ) ) );
	}
	setU16( out, offset + 6, quint16( ( exponent + 96 ) << 7 ) );
}

} // namespace

/*! Write one hknpConvexPolytopeShape.
 *
 * Variable length, unlike the primitives, but the size follows entirely from the
 * counts. Measured over 76 vanilla polytopes with no exceptions:
 *
 *   vertices at +0x50            nv x 16, nv a multiple of 4
 *   planes    at +0x50 + nv*16   np x 16, np = roundup(nf, 4)
 *   faces     at planes end      nf x 4, padded to roundup(nf, 4) entries
 *   indices   at faces end       ni bytes, ni = sum of the face loop lengths
 *   total                        align16(indices end)
 *
 * Note the vertices start at +0x50, not the +0x70 a capsule uses -- a capsule's own
 * end points at +0x50/+0x60 push its hull out of the way.
 *
 * A face entry is (u16 firstIndex, u8 numIndices, u8 minHalfAngle) with firstIndex
 * the running sum of the counts, which is why the loops are written in order.
 */
QByteArray hknpEncodeConvexPolytopeShape( const HknpPolytopeInput & in )
{
	const int nf = int( in.faces.size() );
	if ( nf < 1 || in.faceAngles.size() != nf )
		return QByteArray();

	auto roundUp = []( int value, int to ) { return ( ( value + to - 1 ) / to ) * to; };

	int ni = 0, highest = -1;
	for ( const QVector<int> & loop : in.faces ) {
		if ( loop.size() < 3 || loop.size() > 255 )
			return QByteArray();
		for ( int v : loop ) {
			if ( v < 0 || v >= in.verts.size() )
				return QByteArray();
			highest = std::max( highest, v );
		}
		ni += int( loop.size() );
	}
	if ( ni > 0xffff || highest < 0 )
		return QByteArray();

	/* Real vertices are the ones the faces actually reference; the array is then
	 * padded up to a multiple of 4, and each padding slot DUPLICATES the last real
	 * vertex -- its position and its index tag both. Measured on the 18 vanilla
	 * polytopes that carry padding, every one of them.
	 *
	 * So the real count is recoverable from the loops and needs no extra input:
	 * writing each padding slot's own slot number into its tag, which is the
	 * obvious reading of "w carries the vertex index", is what made those 18 differ.
	 */
	const int realVerts = highest + 1;
	const int nv = roundUp( realVerts, 4 );
	if ( nv > in.verts.size() )
		return QByteArray();
	const int np = std::max( roundUp( nf, 4 ), int( in.planes.size() ) );
	const qsizetype vertsAt = 0x50;
	const qsizetype planesAt = vertsAt + nv * 16;
	const qsizetype facesAt = planesAt + np * 16;
	const qsizetype indicesAt = facesAt + roundUp( nf, 4 ) * 4;
	const qsizetype size = ( ( indicesAt + ni ) + 15 ) / 16 * 16;

	QByteArray out( size, 0 );
	setU32( out, 0x10, in.shapeFlags );
	setFloat( out, 0x14, in.convexRadius );
	setU32( out, 0x18, in.materialCRC );

	setU16( out, 0x30, quint16( nv ) );  setU16( out, 0x32, quint16( vertsAt - 0x30 ) );
	setU16( out, 0x40, quint16( np ) );  setU16( out, 0x42, quint16( planesAt - 0x40 ) );
	setU16( out, 0x44, quint16( nf ) );  setU16( out, 0x46, quint16( facesAt - 0x44 ) );
	setU16( out, 0x48, quint16( ni ) );  setU16( out, 0x4a, quint16( indicesAt - 0x48 ) );

	for ( int i = 0; i < nv; i++ ) {
		const int src = std::min( i, realVerts - 1 );	// padding repeats the last real one
		const Vector3 & v = in.verts.at( src );
		for ( int k = 0; k < 3; k++ )
			setFloat( out, vertsAt + i * 16 + k * 4, v[k] );
		setU32( out, vertsAt + i * 16 + 12, 0x3f000000u | quint32( src & 0xff ) );
	}

	for ( int p = 0; p < np; p++ ) {
		if ( p >= in.planes.size() )
			continue;	// spare slots have no fixed filler; leave them zero
		const Vector4 & pl = in.planes.at( p );
		for ( int k = 0; k < 4; k++ )
			setFloat( out, planesAt + p * 16 + k * 4, pl[k] );
	}

	int first = 0;
	for ( int f = 0; f < nf; f++ ) {
		const QVector<int> & loop = in.faces.at( f );
		setU16( out, facesAt + f * 4, quint16( first ) );
		out[facesAt + f * 4 + 2] = char( quint8( loop.size() ) );
		out[facesAt + f * 4 + 3] = char( in.faceAngles.at( f ) );
		for ( int k = 0; k < loop.size(); k++ )
			out[indicesAt + first + k] = char( quint8( loop.at( k ) ) );
		first += int( loop.size() );
	}
	return out;
}

/*! Write one compound shape object.
 *
 * Measured over 14 corpus compounds, no exceptions: the object is
 * `0xD0 + count * 0x80` bytes; the instance hkArray sits at +0x60 with the count at
 * +0x68 and `count | 0x80000000` at +0x6c; instances start at +0xD0 with a 0x80
 * stride, holding three rotation rows, a translation at +0x30, a scale at +0x40 and
 * a child pointer at +0x50.
 *
 * Two regions are reproduced verbatim rather than computed, because they are not
 * understood: the header from +0x70 to +0xCF (which holds an AABB and more), and
 * all four non-scale w slots of each instance, which carry per-instance payloads
 * rather than the flat 0.5 and 0.0 they print as. See HknpCompound. A first version
 * of this function wrote both as constants; it produced a compound with no bounds,
 * and a second still normalised row 1's negative zero away. The byte round trip
 * caught both -- a float comparison would have called them identical.
 *
 * Nothing is written into the pointer slots: they hold raw zero in vanilla and the
 * fixup tables do the binding. See HknpCompoundFixups.
 */
//! Defined below, next to the BVH writer it belongs with.
QByteArray compoundHeaderTail( const Vector3 & aabbMin, const Vector3 & aabbMax );

QByteArray hknpEncodeCompoundShape( const HknpCompound & compound, HknpCompoundFixups * fixups )
{
	const int count = int( compound.instances.size() );
	if ( count < 1 || count > 4096 )
		return QByteArray();

	QByteArray out( 0xD0 + count * 0x80, 0 );
	setU32( out, 0x10, compound.shapeFlags );
	setU32( out, 0x18, compound.materialCRC );
	setU32( out, 0x30, 0xffffffffu );
	setU32( out, 0x44, 0x80000000u );	// two empty hkArrays before the instances
	setU32( out, 0x54, 0x80000000u );
	setU32( out, 0x58, 0xffffffffu );
	setU32( out, 0x68, quint32( count ) );
	setU32( out, 0x6c, quint32( count ) | 0x80000000u );

	/* +0x70..+0xCF verbatim when it was decoded, and written from the AABB when
	 * it was not -- the fields in it are measured (see compoundHeaderTail), and a
	 * compound built from scratch used to get 0x60 bytes of zero, which is an
	 * empty bounding box around everything it owns.
	 */
	if ( compound.headerTail.size() == 0x60 )
		std::memcpy( out.data() + 0x70, compound.headerTail.constData(), 0x60 );
	else
		std::memcpy( out.data() + 0x70,
			compoundHeaderTail( compound.aabbMin, compound.aabbMax ).constData(), 0x60 );

	for ( int i = 0; i < count; i++ ) {
		const HknpCompound::Instance & one = compound.instances.at( i );
		const qsizetype at = 0xD0 + qsizetype( i ) * 0x80;
		for ( int rr = 0; rr < 3; rr++ )
			for ( int k = 0; k < 3; k++ )
				setFloat( out, at + rr * 16 + k * 4, one.rotRows[rr][k] );
		for ( int k = 0; k < 3; k++ ) {
			setFloat( out, at + 0x30 + k * 4, one.trans[k] );
			setFloat( out, at + 0x40 + k * 4, one.scale[k] );
		}
		// undecoded per-instance payloads, row 1's signed zero among them
		setU32( out, at + 0x0c, one.wPayload[0] );
		setU32( out, at + 0x1c, one.wPayload[1] );
		setU32( out, at + 0x2c, one.wPayload[2] );
		setU32( out, at + 0x3c, one.wPayload[3] );
		setFloat( out, at + 0x4c, 1.0f );	// 0x3f800000 on all 60, checked as bits
		setU32( out, at + 0x58, 0xffffffffu );
	}

	if ( fixups ) {
		fixups->childPointers.clear();
		for ( int i = 0; i < count; i++ )
			fixups->childPointers.append( 0xD0 + qsizetype( i ) * 0x80 + 0x50 );
	}
	return out;
}

/*! Write one hknpCompoundShapeData: the BVH a compound's children hang off.
 *
 * Decoded 2026-08-21 and checked against every compound in the sample — 86 of 86
 * fit it with no exceptions, and Elric recompiling a decompiled two-box body
 * reproduces vanilla's object BYTE FOR BYTE, which is what says the reading is
 * the writing.
 *
 * The object is 0x60 of header and then **2n records of 32 bytes**, so its size
 * is exactly `0x60 + 2n * 32` — 224 bytes for two children, 288 for three, 352
 * for four. The header carries 2n+1 at +0x18 (with the hkArray flag at +0x1c),
 * 2n at +0x20, the instance count at +0x28 and a 1 at +0x30.
 *
 * A record is `float3 min | u32 tag | float3 max | u16 a | u16 b`:
 *
 *  - **tag** is `0x3f000000 | (parentIndex + 1)` — the 0.5 in an hkVector4's w
 *    lane with the parent index folded into its low bits, the same trick a
 *    polytope plays with its vertex indices. The root's is 0x3f000000.
 *  - **a** is the LEFT child + 1 on an internal node and 0 on a leaf. It is
 *    always `i + 2` on an internal node, because the nodes are emitted
 *    depth-first and a node's left child is always the next record — so `a`
 *    carries no information beyond "this is not a leaf", and only `b` is a real
 *    link.
 *  - **b** is the RIGHT child + 1 on an internal node, and the INSTANCE INDEX on
 *    a leaf. The leaves are a permutation of 0..n-1.
 *
 * 2n-1 records are real (a full binary tree over n leaves) and the last one is
 * zero, which is what makes the count 2n.
 *
 * The split here is the median of the child centroids along the widest axis,
 * which is not necessarily Elric's — the tree only has to bound its children
 * correctly, and a different split is a different valid tree over the same
 * boxes, not a wrong one.
 */
QByteArray hknpEncodeCompoundShapeData( const QVector<QPair<Vector3, Vector3>> & childBoxes,
	QVector<int> * instanceLeaf )
{
	const int n = int( childBoxes.size() );
	if ( n < 1 || n > 4096 )
		return QByteArray();

	struct Node { Vector3 lo, hi; int parent; int right; int instance; };
	QVector<Node> nodes;
	QVector<int> order( n );
	for ( int i = 0; i < n; i++ )
		order[i] = i;

	// depth first, left child next, so only the right link needs storing
	std::function<int( int, int, int )> build = [&]( int first, int last, int parent ) -> int {
		const int self = int( nodes.size() );
		Node node; node.parent = parent; node.right = -1; node.instance = -1;
		node.lo = childBoxes.at( order.at( first ) ).first;
		node.hi = childBoxes.at( order.at( first ) ).second;
		for ( int k = first + 1; k < last; k++ ) {
			const auto & box = childBoxes.at( order.at( k ) );
			for ( int a = 0; a < 3; a++ ) {
				node.lo[a] = std::min( node.lo[a], box.first[a] );
				node.hi[a] = std::max( node.hi[a], box.second[a] );
			}
		}
		nodes.append( node );
		if ( last - first == 1 ) {
			nodes[self].instance = order.at( first );
			return self;
		}
		// widest axis of the box, split at the median centroid
		int axis = 0;
		const Vector3 extent = nodes.at( self ).hi - nodes.at( self ).lo;
		if ( extent[1] > extent[axis] ) axis = 1;
		if ( extent[2] > extent[axis] ) axis = 2;
		std::sort( order.begin() + first, order.begin() + last, [&]( int a, int b ) {
			return childBoxes.at( a ).first[axis] + childBoxes.at( a ).second[axis]
				 < childBoxes.at( b ).first[axis] + childBoxes.at( b ).second[axis];
		} );
		const int mid = first + ( last - first ) / 2;
		build( first, mid, self );
		nodes[self].right = build( mid, last, self );
		return self;
	};
	build( 0, n, -1 );

	QByteArray out( 0x60 + qsizetype( n ) * 2 * 32, 0 );
	setU32( out, 0x18, quint32( 2 * n + 1 ) ); setU32( out, 0x1c, 0x80000000u | quint32( 2 * n + 1 ) );
	setU32( out, 0x20, quint32( 2 * n ) );
	setU32( out, 0x28, quint32( n ) );
	setU32( out, 0x30, 1 );
	for ( qsizetype i = 0; i < nodes.size(); i++ ) {
		const Node & node = nodes.at( i );
		const qsizetype at = 0x60 + i * 32;
		for ( int a = 0; a < 3; a++ ) {
			setFloat( out, at + a * 4, node.lo[a] );
			setFloat( out, at + 16 + a * 4, node.hi[a] );
		}
		setU32( out, at + 12, 0x3f000000u | quint32( node.parent + 1 ) );
		if ( node.instance >= 0 ) {
			setU16( out, at + 28, 0 );
			setU16( out, at + 30, quint16( node.instance ) );
		} else {
			setU16( out, at + 28, quint16( i + 2 ) );
			setU16( out, at + 30, quint16( node.right + 1 ) );
		}
	}
	/* Which leaf each instance ended up in. The compound's instance record carries
	 * that back-pointer in the w lane of its last transform row, as `0.5` tagged
	 * with the leaf index + 1 — exact on 422 of 422 vanilla instances.
	 */
	if ( instanceLeaf ) {
		instanceLeaf->fill( -1, n );
		for ( qsizetype i = 0; i < nodes.size(); i++ )
			if ( nodes.at( i ).instance >= 0 )
				( *instanceLeaf )[nodes.at( i ).instance] = int( i );
	}
	return out;
}

/*! The 0x60 bytes a compound carries at +0x70, when there is no decoded original.
 *
 * Measured on 71 vanilla compounds: 0xffffffff at +0x70, the AABB as two
 * hkVector4s at +0x80 and +0x90, a 1 at +0xa0, and zero everywhere else. The
 * AABB equals the root BVH node's box on all 71.
 *
 * The w lane of the minimum at +0x8c is `0.5` with a small number in its low
 * bits — 1 to 5 across the corpus, growing slowly with the child count, which
 * reads as a tree depth. It is not decoded, and writing the commonest value
 * costs nothing: it is the padding lane of an AABB, not a field the traversal
 * reads.
 */
QByteArray compoundHeaderTail( const Vector3 & aabbMin, const Vector3 & aabbMax )
{
	QByteArray out( 0x60, 0 );
	setU32( out, 0x00, 0xffffffffu );
	for ( int a = 0; a < 3; a++ ) {
		setFloat( out, 0x10 + a * 4, aabbMin[a] );
		setFloat( out, 0x20 + a * 4, aabbMax[a] );
	}
	setU32( out, 0x1c, 0x3f000001u );
	setU32( out, 0x30, 1 );
	return out;
}

/*! A blank hkpRagdollConstraintData: the atom chain with every modelled field zero.
 *
 * Built from the measured constants rather than pasted in as a 416-byte blob, so
 * each value is attached to the atom it belongs to. Everything here is one value
 * across all 521 corpus objects.
 */
static QByteArray ragdollConstraintTemplate()
{
	QByteArray out( 0x1a0, 0 );
	// SET_LOCAL_TRANSFORMS at +0x20: two hkTransforms, pivotA always the origin
	setU32( out, 0x20, 0x00000002u );
	// SETUP_STABILIZATION at +0xb0
	setU32( out, 0xb0, 0x00010017u );
	setU32( out, 0xb4, 0x7f7fffeeu );
	setU32( out, 0xb8, 0x7f7fffeeu );
	setU32( out, 0xbc, 0x5f7ffff0u );
	// RAGDOLL_MOTOR at +0xc0: no field of it varies anywhere in the corpus
	setU32( out, 0xc0, 0x00000013u );
	setFloat( out, 0xd0, 1.0f );
	setFloat( out, 0xe4, 1.0f );
	setFloat( out, 0xf8, 1.0f );
	// ANG_FRICTION at +0x120
	setU32( out, 0x120, 0x00010011u );
	setU32( out, 0x124, 0x00000003u );
	// TWIST_LIMIT at +0x130, tau 0.8
	setU32( out, 0x130, 0x0001000fu );
	setU32( out, 0x134, 0x00000001u );
	setFloat( out, 0x140, 0.8f );
	// CONE_LIMIT at +0x150: min is the -100 sentinel, the bound a cone does not have
	setU32( out, 0x150, 0x00010010u );
	setU32( out, 0x154, 0x00380000u );
	setFloat( out, 0x158, -100.0f );
	setFloat( out, 0x160, 0.8f );
	// the second CONE_LIMIT is the perpendicular plane, and does have both bounds
	setU32( out, 0x170, 0x00010010u );
	setU32( out, 0x174, 0x00000101u );
	setFloat( out, 0x180, 0.8f );
	// BALL_SOCKET at +0x190
	setU32( out, 0x190, 0x00000005u );
	setU32( out, 0x194, 0x00000030u );
	setU32( out, 0x198, 0x7f7fffeeu );
	return out;
}

QByteArray hknpEncodeRagdollConstraintData( const HknpConstraint & c )
{
	QByteArray out = ( c.rawData.size() == 0x1a0 ) ? c.rawData : ragdollConstraintTemplate();

	// SET_LOCAL_TRANSFORMS: rotation basis then pivot, child side then parent side
	for ( int k = 0; k < 3; k++ ) {
		for ( int a = 0; a < 3; a++ ) {
			setFloat( out, 0x30 + k * 16 + a * 4, c.rotA[k][a] );
			setFloat( out, 0x70 + k * 16 + a * 4, c.rotB[k][a] );
		}
	}
	for ( int a = 0; a < 3; a++ ) {
		setFloat( out, 0x60 + a * 4, c.pivotA[a] );
		setFloat( out, 0xa0 + a * 4, c.pivotB[a] );
	}

	setFloat( out, 0x128, c.friction );
	if ( c.twist.present ) {
		setFloat( out, 0x138, c.twist.min );
		setFloat( out, 0x13c, c.twist.max );
	}
	if ( c.cone.present )
		setFloat( out, 0x15c, c.cone.max );	// the cone stores only its upper bound
	if ( c.plane.present ) {
		setFloat( out, 0x178, c.plane.min );
		setFloat( out, 0x17c, c.plane.max );
	}
	return out;
}

//! Blank hkpLimitedHingeConstraintData, same construction as the ragdoll template.
static QByteArray limitedHingeConstraintTemplate()
{
	QByteArray out( 0x130, 0 );
	setU32( out, 0x20, 0x00000002u );	// SET_LOCAL_TRANSFORMS
	setU32( out, 0xb0, 0x00010017u );	// SETUP_STABILIZATION
	setU32( out, 0xb4, 0x7f7fffeeu );
	setU32( out, 0xb8, 0x7f7fffeeu );
	setU32( out, 0xbc, 0x5f7ffff0u );
	setU32( out, 0xc0, 0x00000012u );	// ANG_MOTOR: nothing in it varies
	setU32( out, 0xe8, 0x00010011u );	// ANG_FRICTION
	setU32( out, 0xec, 0x00000001u );
	setU32( out, 0xf8, 0x0001000eu );	// ANG_LIMIT, tau 1.0 (ragdoll limits use 0.8)
	setFloat( out, 0x104, 1.0f );
	setU32( out, 0x108, 0x0000000cu );	// TWO_D_ANG
	setU32( out, 0x118, 0x00000005u );	// BALL_SOCKET
	setU32( out, 0x11c, 0x00000030u );
	setU32( out, 0x120, 0x7f7fffeeu );
	return out;	// +0x128..+0x12f is alignment tail, zero throughout
}

QByteArray hknpEncodeLimitedHingeConstraintData( const HknpConstraint & c )
{
	QByteArray out = ( c.rawData.size() == 0x130 ) ? c.rawData : limitedHingeConstraintTemplate();

	for ( int k = 0; k < 3; k++ ) {
		for ( int a = 0; a < 3; a++ ) {
			setFloat( out, 0x30 + k * 16 + a * 4, c.rotA[k][a] );
			setFloat( out, 0x70 + k * 16 + a * 4, c.rotB[k][a] );
		}
	}
	for ( int a = 0; a < 3; a++ ) {
		setFloat( out, 0x60 + a * 4, c.pivotA[a] );
		setFloat( out, 0xa0 + a * 4, c.pivotB[a] );
	}
	setFloat( out, 0xf0, c.friction );
	if ( c.hinge.present ) {
		setFloat( out, 0xfc, c.hinge.min );
		setFloat( out, 0x100, c.hinge.max );
	}
	return out;
}

QByteArray hknpEncodePositionConstraintMotor()
{
	QByteArray out( 0x30, 0 );
	setU32( out, 0x10, 0x00000001u );
	setU32( out, 0x18, 0xc9742400u );
	setFloat( out, 0x1c, 100.0f );
	setFloat( out, 0x20, 0.8f );
	setFloat( out, 0x24, 1.0f );
	setFloat( out, 0x28, 5.0f );
	setFloat( out, 0x2c, 0.2f );
	return out;
}

QByteArray hknpEncodeBreakableConstraintData( float threshold )
{
	QByteArray out( 0x30, 0 );
	setFloat( out, 0x20, threshold );
	return out;
}

QByteArray hknpEncodeSkeleton( const QVector<HknpBone> & bones, HknpSkeletonFixups * fixups )
{
	const int n = int( bones.size() );
	if ( n < 1 || n > 4096 )
		return QByteArray();

	const qsizetype parentsAt = 0x90;
	const qsizetype bonesAt = ( ( parentsAt + 2 * n ) + 15 ) / 16 * 16;
	const qsizetype poseAt = bonesAt + 16 * n;
	const qsizetype size = poseAt + 48 * n;

	QByteArray out( size, 0 );
	const qsizetype desc[3] = { 0x18, 0x28, 0x38 };
	for ( qsizetype d : desc ) {
		setU32( out, d + 8, quint32( n ) );
		setU32( out, d + 12, quint32( n ) | 0x80000000u );
	}
	/* Four negative zeros in the header, all 37 corpus skeletons.
	 *
	 * They sit past the array descriptors, in a region an earlier probe never
	 * scanned because it stopped at the end of what it already understood. Writing
	 * plain zero there is a different bit pattern.
	 */
	for ( qsizetype o : { 0x54, 0x64, 0x74, 0x84 } )
		setU32( out, o, 0x80000000u );

	for ( int i = 0; i < n; i++ ) {
		setU16( out, parentsAt + i * 2, quint16( qint16( bones.at( i ).parent ) ) );
		// bone record: null name pointer, then lockTranslation
		setU32( out, bonesAt + i * 16 + 8, bones.at( i ).lockTranslation ? 1u : 0u );

		const qsizetype p = poseAt + qsizetype( i ) * 48;
		const HknpBone & b = bones.at( i );
		for ( int k = 0; k < 3; k++ )
			setFloat( out, p + k * 4, b.translation[k] );
		// Havok stores the quaternion xyzw; NifSkope's Quat is wxyz
		setFloat( out, p + 16, b.rotation[1] );
		setFloat( out, p + 20, b.rotation[2] );
		setFloat( out, p + 24, b.rotation[3] );
		setFloat( out, p + 28, b.rotation[0] );
		// scale is NOT unity: 767 of 804 corpus bones carry 0.99999994
		for ( int k = 0; k < 3; k++ )
			setFloat( out, p + 32 + k * 4, b.scale[k] );
		setU32( out, p + 12, b.poseTransW );
		setU32( out, p + 44, b.poseScaleW );
	}

	if ( fixups ) {
		fixups->parents = parentsAt;
		fixups->bones = bonesAt;
		fixups->pose = poseAt;
	}
	return out;
}

QByteArray hknpEncodeShapeMassProperties( const Vector3 & centreOfMass, const Vector3 & inertiaRaw,
	float volume, float mass, quint64 majorAxis )
{
	QByteArray out( 0x30, 0 );	// +0x00..+0x0f is zero in all 76 vanilla objects
	setPackedVector3( out, 0x10, centreOfMass );
	setPackedVector3( out, 0x18, inertiaRaw );
	const quint64 axis = qToLittleEndian( majorAxis );
	std::memcpy( out.data() + 0x20, &axis, 8 );
	setFloat( out, 0x28, volume );
	setFloat( out, 0x2c, mass );
	return out;
}

/* ---------------------------------------------------------------------------
 * Packfile assembly.
 *
 * Everything above writes ONE object, with its pointer slots left at raw zero,
 * because a Havok packfile binds pointers through its fixup tables rather than
 * through the data. On their own those bytes cannot be loaded by anything. This
 * is the part that turns them into a file.
 * ------------------------------------------------------------------------- */

namespace {

/*! Class name -> its type hash, read off vanilla files rather than computed.
 *
 * The hash function Havok uses is not established here, so a name that is not in
 * this table is an error rather than something to guess at. The four reflection
 * classes lead every vanilla list and own no object.
 */
quint32 classHash( const QString & name )
{
	static const ClassEntry entries[] = {
		{ 0x33d42383u, "hkClass" }, { 0xb0efa719u, "hkClassMember" },
		{ 0x8a3609cfu, "hkClassEnum" }, { 0xce6f8a6cu, "hkClassEnumItem" },
		{ 0x7c574867u, "hkRefCountedProperties" }, { 0xfec1cedbu, "hkaSkeleton" },
		{ 0xc40485c7u, "hknpBreakableConstraintData" }, { 0x60a75f4cu, "hknpCapsuleShape" },
		{ 0x3ce9b3e3u, "hknpConvexPolytopeShape" }, { 0x4620d11cu, "hknpDynamicCompoundShape" },
		{ 0xf33dc3ccu, "hknpDynamicCompoundShapeData" }, { 0xb857718bu, "hknpPhysicsSystemData" },
		{ 0xdc8f20abu, "hknpRagdollData" }, { 0xe9191728u, "hknpShapeMassProperties" },
		{ 0x741e9012u, "hknpSphereShape" }, { 0x51ea603au, "hkpLimitedHingeConstraintData" },
		{ 0x143dd400u, "hkpPositionConstraintMotor" }, { 0xb77d2036u, "hkpRagdollConstraintData" },
		{ 0x5f60d536u, "hknpCompressedMeshShape" }, { 0xa2bdfc59u, "hknpCompressedMeshShapeData" },
		{ 0xa3e47a9au, "hknpBSMaterialProperties" },
		// identical in all 17 vanilla __classnames__ tables that hold one. Not
		// needed to REWRITE a file -- the file's own table wins via extraHashes --
		// but a newly authored packfile containing one has no other source.
		{ 0xc8f7c10du, "hknpConvexShape" }
	};
	for ( const ClassEntry & e : entries ) {
		if ( name == QLatin1String( e.name ) )
			return e.hash;
	}
	return 0;
}

} // namespace

QByteArray hknpBuildPackfile( const QVector<HknpPackObject> & objects, QString * error,
	const QHash<QString, quint32> & extraHashes, quint32 globalFixupBytes )
{
	auto fail = [error]( const QString & message ) { if ( error ) *error = message; return QByteArray(); };
	if ( objects.isEmpty() )
		return fail( QStringLiteral( "A packfile needs at least a root object." ) );

	// class names: the four reflection classes, then order of first use (37/37)
	QStringList used = { QStringLiteral( "hkClass" ), QStringLiteral( "hkClassMember" ),
		QStringLiteral( "hkClassEnum" ), QStringLiteral( "hkClassEnumItem" ) };
	for ( const HknpPackObject & o : objects ) {
		if ( !used.contains( o.className ) )
			used.append( o.className );
	}
	QByteArray cn;
	QHash<QString, quint32> nameOffset;
	for ( const QString & name : std::as_const( used ) ) {
		// the file's own table first: it cannot be wrong, and it covers classes the
		// built-in list has never sampled
		const quint32 hash = extraHashes.value( name, classHash( name ) );
		if ( !hash )
			return fail( QStringLiteral( "No class hash known for %1." ).arg( name ) );
		nameOffset.insert( name, quint32( cn.size() + 5 ) );
		appendU32( cn, hash );
		cn.append( char( 0x09 ) );
		cn.append( name.toLatin1() );
		cn.append( '\0' );
	}
	pad( cn, 16, char( 0xff ) );

	// objects land in the order given, each padded to 16 (every vanilla object is)
	QVector<quint32> at( objects.size() );
	QByteArray data;
	for ( qsizetype i = 0; i < objects.size(); i++ ) {
		at[i] = quint32( data.size() );
		data.append( objects.at( i ).bytes );
		pad( data, 16 );
	}

	Fixups fx;
	for ( qsizetype i = 0; i < objects.size(); i++ ) {
		const HknpPackObject & o = objects.at( i );
		fx.virtuals.append( { at[i], 0, nameOffset.value( o.className ) } );
		for ( const auto & l : o.local )
			fx.local.append( { at[i] + quint32( l.first ), at[i] + quint32( l.second ) } );
		for ( const HknpPackObject::Ref & g : o.global ) {
			if ( g.object < 0 || g.object >= objects.size() )
				return fail( QStringLiteral( "Packfile fixup names object %1 of %2." )
					.arg( g.object ).arg( objects.size() ) );
			fx.global.append( { at[i] + quint32( g.source ), 2, at[g.object] } );
		}
	}
	/* No sort. All three tables are written grouped by object in object order and,
	 * within an object, in member declaration order -- which is offset order only
	 * when no array payload carries fixups of its own. It does on a compressed mesh,
	 * where the section array's fixup lands between the +0x50 and +0x60 members.
	 * Sorting by source matches every ragdoll and no compressed-mesh system.
	 */

	QByteArray local = fx.localTable(), global = fx.globalTable( globalFixupBytes ), virtuals = fx.virtualTable();
	quint32 cnStart = 0x100, cnEnd = cnStart + quint32( cn.size() ), dataStart = cnEnd;
	quint32 localAbs = dataStart + quint32( data.size() ), globalAbs = localAbs + quint32( local.size() ),
		virtualAbs = globalAbs + quint32( global.size() );
	quint32 dataEnd = virtualAbs + quint32( virtuals.size() );
	QByteArray out = fileHeader( nameOffset.value( objects.first().className ) );
	out += sectionHeader( "__classnames__", cnStart, cnEnd, cnEnd, cnEnd, cnEnd );
	out += sectionHeader( "__types__", cnEnd, cnEnd, cnEnd, cnEnd, cnEnd );
	out += sectionHeader( "__data__", dataStart, localAbs, globalAbs, virtualAbs, dataEnd );
	out += cn; out += data; out += local; out += global; out += virtuals;
	if ( error )
		error->clear();
	return out;
}

/*! Write the root object of a ragdoll.
 *
 * The header is entirely zero apart from the seven array descriptors -- checked
 * word by word across all 37 vanilla roots -- so everything interesting is in
 * the payloads, which run back to back from +0x90 with each padded to 16.
 *
 * Field-level constants below are the values every one of the 857 corpus bodies
 * carries; the ones that vary are exactly what HknpBodyPhys models, which is why
 * this can be written from the model rather than from preserved bytes.
 */
namespace {

/*! Shared by both packfile roots, which are the same object with the ragdoll
 * one adding a bone-to-body array at +0x80 and a skeleton pointer at +0x78.
 */
QByteArray encodeSystemRoot( const HknpSystem & sys, HknpRagdollDataFixups * fixups, bool ragdoll )
{
	const qsizetype nb = sys.bodyPhys.size();
	const qsizetype nbone = sys.bones.size();
	const qsizetype ncon = sys.constraints.size();
	if ( nb <= 0 )
		return QByteArray();

	// dyn_motion / dyn_inertia are indexed by cinfo +0x0c, which is NOT the body
	// index -- limbs share motions and static bodies have none
	qsizetype nmotion = 0;
	for ( const HknpBodyPhys & b : sys.bodyPhys )
		nmotion = std::max( nmotion, qsizetype( b.motionIndex ) + 1 );
	/* The two arrays have their OWN counts and they need not agree: one vanilla
	 * physics system carries an inertia entry with no motion entry, so deriving
	 * both from the motion index writes an array the file does not have and shifts
	 * every offset after it. Stored counts win where the decode has them.
	 */
	const qsizetype nMotionArr = ( sys.motionCount >= 0 ) ? qsizetype( sys.motionCount ) : nmotion;
	const qsizetype nInertiaArr = ( sys.inertiaCount >= 0 ) ? qsizetype( sys.inertiaCount ) : nmotion;

	QByteArray props, motions( nmotion * 0x40, 0 ), inertias( nmotion * 0x70, 0 ), cinfos;
	for ( qsizetype i = 0; i < nb; i++ ) {
		const HknpBodyPhys & b = sys.bodyPhys.at( i );

		/* Start from the stored record where there is one and patch the two modelled
		 * fields in; the constants below are what 37 actor skeletons carry and are
		 * demonstrably not universal -- see HknpBodyPhys::propsRawData.
		 */
		QByteArray p = ( b.propsRawData.size() == 0x50 ) ? b.propsRawData : QByteArray( 0x50, 0 );
		if ( b.propsRawData.size() != 0x50 ) {
			setU16( p, 0x10, 0xff00 );
			setU32( p, 0x18, 0x3d4c0201u ); setU32( p, 0x1c, 0x7f7fffeeu );
			setFloat( p, 0x20, 1.0f ); setFloat( p, 0x24, 1.0f );
			setU32( p, 0x38, 0x000040a0u );
		}
		setU16( p, 0x12, roundFloat16( b.friction ) );
		setU16( p, 0x14, roundFloat16( b.friction ) );
		setU16( p, 0x16, roundFloat16( b.restitution ) );
		props.append( p );

		QByteArray c( 0x60, 0 );
		setU32( c, 0x08, 0x7fffffffu );
		setU32( c, 0x0c, b.motionIndex >= 0 ? quint32( b.motionIndex ) : 0x7fffffffu );
		c[0x10] = char( 0xff );
		/* +0x12 is the body's slot in the ragdoll's shape list -- the INVERSE of
		 * shapeListOrder, exact on all 37 -- not the body's own index and not a
		 * material id. It is the identity on one ragdoll only, which is the one
		 * whose shape list is already in body order.
		 */
		setU16( c, 0x12, quint16( std::max( sys.shapeListOrder.indexOf( int( i ) ), qsizetype( 0 ) ) ) );
		// the stored filter, not the display one: layer 0 is real, and the decode
		// substitutes 1 or 10 for it so a user sees something useful
		setU32( c, 0x14, b.hasStoredFilter ? b.packedFilter
			: ( ( b.layer & 0xffu ) | ( quint32( b.filterFlags ) << 8 )
				| ( quint32( b.filterGroup ) << 16 ) ) );
		setU32( c, 0x18, b.cinfoFlags );
		for ( int a = 0; a < 3; a++ )
			setFloat( c, 0x30 + a * 4, b.position[a] );
		setU32( c, 0x3c, b.positionW );
		// NifSkope's Quat is wxyz, Havok stores xyzw
		setFloat( c, 0x40, b.orientation[1] ); setFloat( c, 0x44, b.orientation[2] );
		setFloat( c, 0x48, b.orientation[3] ); setFloat( c, 0x4c, b.orientation[0] );
		cinfos.append( c );

		if ( b.motionIndex < 0 || b.motionIndex >= nmotion )
			continue;
		const qsizetype m = qsizetype( b.motionIndex ) * 0x40;
		setFloat( motions, m + 0x08, b.gravityFactor ); setFloat( motions, m + 0x0c, 1.0f );
		setFloat( motions, m + 0x10, b.maxLinVelocity ); setFloat( motions, m + 0x14, b.maxAngVelocity );
		setFloat( motions, m + 0x18, b.linDamping ); setFloat( motions, m + 0x1c, b.angDamping );
		setU32( motions, m + 0x20, 0x3e2e147bu ); setU32( motions, m + 0x24, 0x3efb22d2u );
		setU32( motions, m + 0x28, 0x3b23d70bu ); setU32( motions, m + 0x2c, 0x3b23d70bu );
		setFloat( motions, m + 0x30, 1.0f );
		// +0x34..+0x3f reads like uninitialised tail, but it is the same three
		// words on all 857 bodies, so it is written rather than zeroed
		setU32( motions, m + 0x34, 0x999b67ffu ); setU32( motions, m + 0x38, 0x06757304u );
		setU32( motions, m + 0x3c, 0x00000073u );

		const qsizetype n = qsizetype( b.motionIndex ) * 0x70;
		setU32( inertias, n + 0x00, b.inertiaTag );
		/* The STORED inverse mass when mass has not been edited, because 1/(1/x)
		 * is not x in float32 — three values in the corpus come back one ULP out,
		 * which was six systems failing a byte-exact round trip for nothing.
		 *
		 * The test is the decode's own expression, so it is exact by construction:
		 * if mass is still what invMassStored produced, nothing has touched it.
		 */
		const bool massUntouched = b.invMassStored > 1.0e-12f
			&& b.mass == 1.0f / b.invMassStored;
		setFloat( inertias, n + 0x04, massUntouched ? b.invMassStored
			: ( b.mass > 1.0e-12f ? 1.0f / b.mass : 0.0f ) );
		setFloat( inertias, n + 0x08, b.density );
		setU32( inertias, n + 0x0c, 0x5f7ffff0u ); setU32( inertias, n + 0x10, 0x5f7ffff0u );
		for ( int a = 0; a < 3; a++ ) {
			setFloat( inertias, n + 0x20 + a * 4, b.invInertia[a] );
			setFloat( inertias, n + 0x30 + a * 4, b.motionCom[a] );
		}
		setU32( inertias, n + 0x2c, b.inertiaScale );
		setU32( inertias, n + 0x3c, b.motionComW );
		setFloat( inertias, n + 0x40, b.motionOrientation[1] );
		setFloat( inertias, n + 0x44, b.motionOrientation[2] );
		setFloat( inertias, n + 0x48, b.motionOrientation[3] );
		setFloat( inertias, n + 0x4c, b.motionOrientation[0] );
	}

	QByteArray cons( ncon * 0x18, 0 );
	for ( qsizetype i = 0; i < ncon; i++ ) {
		setU32( cons, i * 0x18 + 0x08, quint32( sys.constraints.at( i ).childBody ) );
		setU32( cons, i * 0x18 + 0x0c, quint32( sys.constraints.at( i ).parentBody ) );
	}

	/* The bone-to-body map is the identity on all 37, including the three parts
	 * kits where the counts differ, so it is written rather than carried. It is
	 * per BONE, not per body -- see the class comment.
	 */
	QByteArray boneMap( nbone * 4, 0 );
	for ( qsizetype i = 0; i < nbone; i++ )
		setU32( boneMap, i * 4, quint32( i ) );

	/* The header is 0x90 on a ragdoll and 0x80 on a physics system -- the ragdoll
	 * root's extra bone-to-body descriptor at +0x80 is what makes the difference,
	 * and payloads start immediately after. Measured 6/6 and 50/50. Writing 0x90
	 * for both leaves a physics system 16 bytes too long with every offset shifted.
	 */
	QByteArray out( ragdoll ? 0x90 : 0x80, 0 );
	auto place = [&]( qsizetype desc, quint32 count, const QByteArray & payload ) {
		setU32( out, desc + 8, count );
		setU32( out, desc + 12, count | 0x80000000u );
		const qsizetype at = out.size();
		out.append( payload );
		pad( out, 16 );
		return at;
	};
	HknpRagdollDataFixups fx;
	/* An EMPTY array leaves its descriptor at raw zero -- count included -- and
	 * contributes no local fixup and no payload. Writing a count of 0 with a
	 * pointer to nothing is what a naive loop does, and it is not what a static
	 * system carries: 152 of the 185 sampled hold only three of the six.
	 */
	auto placeIf = [&]( qsizetype desc, qsizetype count, const QByteArray & payload ) -> qsizetype {
		if ( count <= 0 ) {
			// an EMPTY array still writes count 0 with the 0x80000000 "does not own
			// the memory" flag; only the pointer, the fixup and the payload are absent
			setU32( out, desc + 12, 0x80000000u );
			return -1;
		}
		const qsizetype at = place( desc, quint32( count ), payload );
		fx.local.append( { desc, at } );
		return at;
	};
	placeIf( 0x10, nb, props );
	placeIf( 0x20, nMotionArr, motions.left( nMotionArr * 0x40 ) );
	placeIf( 0x30, nInertiaArr, inertias.left( nInertiaArr * 0x70 ) );
	const qsizetype cinfoAt = placeIf( 0x40, nb, cinfos );
	const qsizetype consAt = placeIf( 0x50, ncon, cons );
	const qsizetype listAt = placeIf( 0x60, sys.shapeListOrder.size(),
		QByteArray( sys.shapeListOrder.size() * 8, 0 ) );
	if ( ragdoll )
		placeIf( 0x80, nbone, boneMap );

	for ( qsizetype i = 0; i < nb && cinfoAt >= 0; i++ )
		fx.bodyShapePointers.append( cinfoAt + i * 0x60 );
	for ( qsizetype i = 0; i < ncon && consAt >= 0; i++ )
		fx.constraintPointers.append( consAt + i * 0x18 );
	for ( qsizetype i = 0; i < sys.shapeListOrder.size() && listAt >= 0; i++ )
		fx.shapeListPointers.append( listAt + i * 8 );
	if ( fixups )
		*fixups = fx;
	return out;
}

} // namespace

QByteArray hknpEncodeRagdollData( const HknpSystem & sys, HknpRagdollDataFixups * fixups )
{
	return encodeSystemRoot( sys, fixups, true );
}

namespace {

/*! One shape as a packfile object. Returns false for a class with no encoder.
 *
 * Convex polytopes own an hkRefCountedProperties holding their mass properties;
 * those two objects are appended straight after the shape, which is where every
 * vanilla file puts them.
 */
bool encodeShapeObject( const HknpShape & shp, QVector<HknpPackObject> & objs );

bool encodeShapeObject( const HknpShape & shp, QVector<HknpPackObject> & objs )
{
	HknpPackObject so;
	/* The wrapper FIRST. Its own HknpShape carries the scaled geometry of the
	 * shape it wraps, so it looks like a convex polytope to every test below and
	 * was silently written as one -- which dropped the wrapper, the child's
	 * hkRefCountedProperties and its mass properties, three objects out of five.
	 */
	if ( shp.scaledChild && !shp.rawData.isEmpty() ) {
		// the child is emitted straight after, which is where the file keeps it,
		// and it brings its own properties chain, so this recurses
		so.className = shp.className;
		so.bytes = shp.rawData;
		so.local = shp.rawLocal;
		const qsizetype at = objs.size();
		objs.append( so );
		objs[at].global.append( { 0x30, int( objs.size() ) } );
		return encodeShapeObject( *shp.scaledChild, objs );
	}
	if ( shp.primType == 2 ) {
		HknpCapsuleInput in;
		in.capA = shp.capA; in.capB = shp.capB;
		in.radius = shp.convexRadius; in.materialCRC = shp.shapeMaterialCRC;
		in.padding = shp.corePadding;
		/* A capsule that came off a DECODE brings its core box, and the roll it was
		 * written with is recovered from it: bit 1 of the vertex index selects the
		 * +u side, so the difference of the two 4-corner centroids is u.
		 *
		 * A capsule that did not is a NEW one -- Compile writes these now -- and it
		 * has no roll to recover. hknpEncodeCapsuleShape synthesizes a
		 * deterministic frame and derives the padding, which is what its own
		 * documentation says to do; requiring the core box here instead is what
		 * made a body holding a capsule decline the convex path entirely.
		 */
		if ( shp.coreVerts.size() == 8 ) {
			Vector3 hi, lo;
			for ( int v = 0; v < 8; v++ )
				( ( v & 2 ) ? hi : lo ) += shp.coreVerts.at( v );
			in.frameU = hi - lo;
			in.hasFrame = in.frameU.length() > 1.0e-12f;
		}
		so.className = QStringLiteral( "hknpCapsuleShape" );
		so.bytes = hknpEncodeCapsuleShape( in );
	} else if ( shp.primType == 1 ) {
		so.className = QStringLiteral( "hknpSphereShape" );
		so.bytes = hknpEncodeSphereShape( shp.capA, shp.convexRadius, shp.shapeMaterialCRC );
	} else if ( shp.className == QLatin1String( "hknpConvexShape" ) && !shp.verts.isEmpty() ) {
		/* The convex base class: vertices, no faces. Matched by NAME.
		 *
		 * It is `isConvex` with an empty face list, so it falls past the polytope
		 * branch to the refusal at the bottom -- which is what made all 17 vanilla
		 * systems holding one fail to assemble. Matching on geometry instead would
		 * be ambiguous with a polytope whose faces failed to decode; the class name
		 * comes off the packfile's own virtual fixup and cannot be wrong.
		 *
		 * Derived rather than carried, because every byte is accounted for and
		 * nothing goes through arithmetic -- so unlike a capsule there is no ULP
		 * risk. The derived size equals the stored size on all 17, which means the
		 * untouched-shape substitution below fires anyway and an unedited file goes
		 * back byte for byte regardless.
		 */
		so.className = shp.className;
		so.bytes = hknpEncodeConvexShape( shp.verts, shp.convexRadius,
			shp.shapeMaterialCRC, shp.shapeFlags );
	} else if ( shp.isConvex && !shp.faces.isEmpty()
		&& shp.faceAngles.size() == shp.faces.size() ) {
		HknpPolytopeInput pin;
		pin.verts = shp.verts; pin.planes = shp.planes; pin.faces = shp.faces;
		pin.faceAngles = shp.faceAngles; pin.convexRadius = shp.convexRadius;
		pin.materialCRC = shp.shapeMaterialCRC; pin.shapeFlags = shp.shapeFlags;
		so.className = QStringLiteral( "hknpConvexPolytopeShape" );
		so.bytes = hknpEncodeConvexPolytopeShape( pin );
	} else if ( !shp.dataRawData.isEmpty() && !shp.rawData.isEmpty() ) {
		/* A compressed mesh. There is no derivation to attempt: each section
		 * quantizes against its own domain and the section partitioning is Havok's,
		 * so the shape and its data object both go back as they came. The authoring
		 * writer (hknpEncodeCompressedMesh) builds a NEW one from triangles; that is
		 * a different job from rewriting an existing file without disturbing it.
		 */
		so.className = shp.className;
		so.bytes = shp.rawData;
	} else {
		return false;
	}
	if ( so.bytes.isEmpty() )
		return false;
	/* An untouched shape goes back as it came. The derived bytes above are still
	 * built, because their SIZE is what says the layout was understood, but a
	 * capsule's core box cannot survive a float round trip and there is no reason
	 * to make it try. A caller that edited the shape clears rawData.
	 */
	if ( shp.rawData.size() == so.bytes.size() ) {
		so.bytes = shp.rawData;
		so.local = shp.rawLocal;
	}

	const qsizetype shapeIndex = objs.size();
	objs.append( so );

	/* What hangs off the shape, in member order: the hkRefCountedProperties at
	 * +0x20 with either mass properties or a material table under it, then a
	 * compressed mesh's data object at +0x60.
	 */
	if ( shp.hasMassProps || !shp.materialRawData.isEmpty() ) {
		const bool material = !shp.hasMassProps;
		objs[shapeIndex].global.append( { 0x20, int( objs.size() ) } );
		HknpPackObject rc;
		rc.className = QStringLiteral( "hkRefCountedProperties" );
		rc.bytes = refCountedProperties( material ? 0x0000f601u : 0x0000f100u );
		rc.local.append( { 0x00, 0x10 } );
		rc.global.append( { 0x10, int( objs.size() ) + 1 } );
		objs.append( rc );

		HknpPackObject mp;
		if ( material ) {
			mp.className = QStringLiteral( "hknpBSMaterialProperties" );
			mp.bytes = shp.materialRawData;
			mp.local = shp.materialLocal;
		} else {
			mp.className = QStringLiteral( "hknpShapeMassProperties" );
			mp.bytes = hknpEncodeShapeMassProperties( shp.massCom, shp.massInertiaRaw,
				shp.massVolume, shp.massMass, shp.massMajorAxis );
			if ( shp.massRawData.size() == mp.bytes.size() )
				mp.bytes = shp.massRawData;
		}
		objs.append( mp );
	}
	if ( !shp.dataRawData.isEmpty() ) {
		objs[shapeIndex].global.append( { 0x60, int( objs.size() ) } );
		HknpPackObject cd;
		cd.className = shp.dataClassName;
		cd.bytes = shp.dataRawData;
		cd.local = shp.dataLocal;
		objs.append( cd );
	}
	return true;
}

} // namespace

QByteArray hknpEncodeSystem( const HknpSystem & sys, QString * error )
{
	auto fail = [error]( const QString & message ) { if ( error ) *error = message; return QByteArray(); };
	if ( sys.bodyPhys.isEmpty() )
		return fail( QStringLiteral( "The system has no bodies." ) );
	const bool ragdoll = !sys.bones.isEmpty();

	/* One shape per body. A compound owns several, all carrying that same body, so
	 * its children have to be taken out of the running before the rest are matched
	 * up -- otherwise whichever child came first would be taken for the body's
	 * whole shape and the compound would vanish.
	 */
	QVector<const HknpCompound *> compoundFor( sys.bodyPhys.size(), nullptr );
	QSet<int> isChild;
	for ( const HknpCompound & c : sys.compounds ) {
		if ( c.bodyId >= 0 && c.bodyId < compoundFor.size() )
			compoundFor[c.bodyId] = &c;
		for ( int ci : c.children )
			isChild.insert( ci );
	}
	QVector<const HknpShape *> byBody( sys.bodyPhys.size(), nullptr );
	for ( qsizetype k = 0; k < sys.shapes.size(); k++ ) {
		const HknpShape & s = sys.shapes.at( k );
		if ( isChild.contains( int( k ) ) )
			continue;
		if ( s.bodyId >= 0 && s.bodyId < byBody.size() && !byBody.at( s.bodyId ) )
			byBody[s.bodyId] = &s;
	}

	QVector<HknpPackObject> objs;
	HknpRagdollDataFixups rfx;
	HknpPackObject root;
	root.className = ragdoll ? QStringLiteral( "hknpRagdollData" )
							 : QStringLiteral( "hknpPhysicsSystemData" );
	root.bytes = ragdoll ? hknpEncodeRagdollData( sys, &rfx )
						 : hknpEncodePhysicsSystemData( sys, &rfx );
	if ( root.bytes.isEmpty() )
		return fail( QStringLiteral( "Could not write the system root object." ) );
	root.local = rfx.local;
	objs.append( root );

	/* Object order is a walk of the root's own references: each body's shape (and
	 * whatever the shape owns) in body order, then each constraint (and its motor
	 * the first time one is wanted), then the skeleton last. That reproduces the
	 * object order of every vanilla ragdoll.
	 */
	QVector<int> shapeObj( sys.bodyPhys.size(), -1 );
	for ( qsizetype i = 0; i < byBody.size(); i++ ) {
		shapeObj[i] = int( objs.size() );
		/* A compound emits as compound, then its children, then its shape-data
		 * object -- data LAST even though its pointer at +0xC0 sits below the
		 * child pointers, the same inversion as the root's skeleton pointer.
		 */
		if ( const HknpCompound * comp = compoundFor.at( i ) ) {
			HknpCompoundFixups cfx;
			HknpPackObject co;
			co.className = comp->dynamic ? QStringLiteral( "hknpDynamicCompoundShape" )
										 : QStringLiteral( "hknpStaticCompoundShape" );
			/* The BVH is built BEFORE the compound, even though it is written after
			 * it, because each instance carries the index of its own leaf: the
			 * compound cannot be finished until the tree has decided where its
			 * children went. A compound that came off a decode brings both objects
			 * with it and skips all of this.
			 */
			QByteArray builtData;
			HknpCompound patched = *comp;
			if ( comp->dataRawData.isEmpty() ) {
				QVector<QPair<Vector3, Vector3>> boxes;
				for ( int ci : comp->children ) {
					if ( ci < 0 || ci >= sys.shapes.size() )
						return fail( QStringLiteral( "Compound on body %1 names a child that is not there." ).arg( i ) );
					const HknpShape & child = sys.shapes.at( ci );
					/* A sphere and a capsule have no vertex list — their geometry
					 * is the end points and the radius — so the box comes off those
					 * instead. Asking for verts refused every body holding one.
					 */
					Vector3 lo, hi;
					if ( child.primType == 1 || child.primType == 2 ) {
						const Vector3 a0 = child.transformed( child.capA );
						const Vector3 a1 = child.transformed( child.primType == 2 ? child.capB : child.capA );
						for ( int a = 0; a < 3; a++ ) {
							lo[a] = std::min( a0[a], a1[a] ); hi[a] = std::max( a0[a], a1[a] );
						}
					} else if ( !child.verts.isEmpty() ) {
						lo = hi = child.transformed( child.verts.first() );
						for ( const Vector3 & v : child.verts ) {
							const Vector3 p = child.transformed( v );
							for ( int a = 0; a < 3; a++ ) { lo[a] = std::min( lo[a], p[a] ); hi[a] = std::max( hi[a], p[a] ); }
						}
					} else {
						return fail( QStringLiteral( "Compound child on body %1 has no geometry to bound." ).arg( i ) );
					}
					/* Grown by the convex radius, because the SOLID is: vanilla's
					 * fridge bounds its two boxes at -0.8452 where their hulls stop
					 * at -0.8352, and the difference is its 0.01 radius exactly.
					 * For a sphere or a capsule that radius IS the shape.
					 */
					const float grow = ( child.primType == 1 || child.primType == 2 )
						? child.primRadius : child.convexRadius;
					for ( int a = 0; a < 3; a++ ) {
						lo[a] -= grow; hi[a] += grow;
					}
					boxes.append( { lo, hi } );
				}
				QVector<int> leaves;
				builtData = hknpEncodeCompoundShapeData( boxes, &leaves );
				if ( builtData.isEmpty() )
					return fail( QStringLiteral( "Could not write the compound shape data on body %1." ).arg( i ) );
				for ( qsizetype k = 0; k < patched.instances.size() && k < leaves.size(); k++ )
					patched.instances[k].wPayload[3] = 0x3f000000u | quint32( leaves.at( k ) + 1 );
			}
			co.bytes = hknpEncodeCompoundShape( patched, &cfx );
			if ( co.bytes.isEmpty() || cfx.childPointers.size() != comp->children.size() )
				return fail( QStringLiteral( "Could not write the compound on body %1." ).arg( i ) );
			co.local.append( { cfx.instanceArrayPointer, cfx.instanceArray } );
			const int at = int( objs.size() );
			objs.append( co );
			for ( qsizetype k = 0; k < comp->children.size(); k++ ) {
				const int ci = comp->children.at( k );
				if ( ci < 0 || ci >= sys.shapes.size() )
					return fail( QStringLiteral( "Compound on body %1 names no child %2." ).arg( i ).arg( k ) );
				objs[at].global.append( { cfx.childPointers.at( k ), int( objs.size() ) } );
				if ( !encodeShapeObject( sys.shapes.at( ci ), objs ) )
					return fail( QStringLiteral( "No encoder for a compound child on body %1 (%2)." )
						.arg( i ).arg( sys.shapes.at( ci ).className ) );
			}
			objs[at].global.append( { cfx.shapeDataPointer, int( objs.size() ) } );
			HknpPackObject cd;
			cd.className = comp->dataClassName.isEmpty()
				? ( comp->dynamic ? QStringLiteral( "hknpDynamicCompoundShapeData" )
								  : QStringLiteral( "hknpStaticCompoundShapeData" ) )
				: comp->dataClassName;
			/* A compound built from scratch has no stored shape data, and used to
			 * be refused here. Its BVH is written now (see
			 * hknpEncodeCompoundShapeData); a decoded compound still carries its
			 * own bytes through, so nothing that round-trips changes.
			 */
			cd.bytes = comp->dataRawData.isEmpty() ? builtData : comp->dataRawData;
			cd.local = comp->dataRawData.isEmpty() ? QVector<QPair<qsizetype, qsizetype>>() : comp->dataLocal;
			objs.append( cd );
			continue;
		}
		if ( !byBody.at( i ) )
			return fail( QStringLiteral( "Body %1 has no shape." ).arg( i ) );
		if ( !encodeShapeObject( *byBody.at( i ), objs ) )
			return fail( QStringLiteral( "No encoder for the shape on body %1 (%2)." )
				.arg( i ).arg( byBody.at( i )->className ) );
	}

	int motorObj = -1;
	QVector<int> conObj( sys.constraints.size(), -1 );
	for ( qsizetype i = 0; i < sys.constraints.size(); i++ ) {
		const HknpConstraint & c = sys.constraints.at( i );
		HknpPackObject co;
		co.className = c.kind;
		if ( c.kind == QLatin1String( "hkpRagdollConstraintData" ) )
			co.bytes = hknpEncodeRagdollConstraintData( c );
		else if ( c.kind == QLatin1String( "hkpLimitedHingeConstraintData" ) )
			co.bytes = hknpEncodeLimitedHingeConstraintData( c );
		else if ( !c.rawData.isEmpty() )
			// kinds with no encoder of their own -- hkpBallAndSocketConstraintData
			// turns up in physics systems -- still rewrite from their stored bytes
			co.bytes = c.rawData;
		else
			return fail( QStringLiteral( "No encoder for constraint kind %1." ).arg( c.kind ) );

		/* A breakable joint is the wrapper object, not the constraint: the root
		 * points at the wrapper and the wrapper points on. Only 3 exist, all with
		 * the constraint pointer at +0x18.
		 */
		if ( c.breakable ) {
			HknpPackObject wrap;
			wrap.className = QStringLiteral( "hknpBreakableConstraintData" );
			wrap.bytes = hknpEncodeBreakableConstraintData( c.breakThreshold );
			wrap.global.append( { 0x18, int( objs.size() ) + 1 } );
			conObj[i] = int( objs.size() );
			objs.append( wrap );
		} else {
			conObj[i] = int( objs.size() );
		}

		const int here = int( objs.size() );
		if ( !c.motorPointers.isEmpty() ) {
			const int motor = ( motorObj >= 0 ) ? motorObj : here + 1;
			for ( qsizetype s : c.motorPointers )
				co.global.append( { s, motor } );
		}
		objs.append( co );
		if ( !c.motorPointers.isEmpty() && motorObj < 0 ) {
			HknpPackObject mo;
			mo.className = QStringLiteral( "hkpPositionConstraintMotor" );
			mo.bytes = hknpEncodePositionConstraintMotor();
			motorObj = int( objs.size() );
			objs.append( mo );
		}
	}

	int skelObj = -1;
	if ( ragdoll ) {
		HknpSkeletonFixups sfx;
		HknpPackObject sk;
		sk.className = QStringLiteral( "hkaSkeleton" );
		sk.bytes = hknpEncodeSkeleton( sys.bones, &sfx );
		if ( sk.bytes.isEmpty() )
			return fail( QStringLiteral( "Could not write the ragdoll skeleton." ) );
		sk.local = { { sfx.parentsPointer, sfx.parents }, { sfx.bonesPointer, sfx.bones },
			{ sfx.posePointer, sfx.pose } };
		skelObj = int( objs.size() );
		objs.append( sk );
	}

	// the root's globals in member declaration order, which is what the packfile
	// writer preserves and what puts the skeleton pointer last
	QVector<HknpPackObject::Ref> refs;
	for ( qsizetype i = 0; i < rfx.bodyShapePointers.size(); i++ )
		refs.append( { rfx.bodyShapePointers.at( i ), shapeObj.at( i ) } );
	for ( qsizetype i = 0; i < rfx.constraintPointers.size(); i++ )
		refs.append( { rfx.constraintPointers.at( i ), conObj.at( i ) } );
	for ( qsizetype i = 0; i < rfx.shapeListPointers.size(); i++ ) {
		const int body = sys.shapeListOrder.value( i, -1 );
		if ( body < 0 || body >= shapeObj.size() )
			return fail( QStringLiteral( "Shape list entry %1 names no body." ).arg( i ) );
		refs.append( { rfx.shapeListPointers.at( i ), shapeObj.at( body ) } );
	}
	if ( skelObj >= 0 )
		refs.append( { rfx.skeletonPointer, skelObj } );
	objs[0].global = refs;

	return hknpBuildPackfile( objs, error, sys.classHashes, sys.globalFixupBytes );
}

QByteArray hknpEncodeRagdoll( const HknpSystem & sys, QString * error )
{
	auto fail = [error]( const QString & message ) { if ( error ) *error = message; return QByteArray(); };
	if ( sys.bones.isEmpty() )
		return fail( QStringLiteral( "Not a ragdoll: it has no skeleton." ) );
	return hknpEncodeSystem( sys, error );
}

/*! Write the root of a plain physics system.
 *
 * Same six array members at the same offsets as the ragdoll root -- that root
 * derives from this one -- and the payloads start at +0x90 either way. A STATIC
 * system carries only three of the six: dyn_motion and dyn_inertia exist only
 * when something simulates, the constraint array only when something is jointed,
 * and an absent array leaves its descriptor at raw zero rather than count 0.
 * Measured on 185 sampled files: 152 carry {0x10, 0x40, 0x60}, 24 add
 * {0x20, 0x30}, 7 add {0x30} alone and 2 add {0x50}.
 */
QByteArray hknpEncodePhysicsSystemData( const HknpSystem & sys, HknpRagdollDataFixups * fixups )
{
	return encodeSystemRoot( sys, fixups, false );
}

#include "hknpencode.h"

#include <QtEndian>
#include <QHash>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

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

	QByteArray localTable() const
	{
		QByteArray out; for ( const auto & f : local ) { appendU32( out, f.first ); appendU32( out, f.second ); }
		appendU32( out, 0xffffffffu ); appendU32( out, 0xffffffffu ); return out;
	}
	QByteArray globalTable() const
	{
		QByteArray out; for ( const GlobalFix & f : global ) {
			appendU32( out, f.source ); appendU32( out, f.section ); appendU32( out, f.target );
		}
		for ( int i = 0; i < 3; i++ ) appendU32( out, 0xffffffffu );
		return out;
	}
	QByteArray virtualTable() const
	{
		QByteArray out; for ( const VirtualFix & f : virtuals ) {
			appendU32( out, f.object ); appendU32( out, f.section ); appendU32( out, f.name );
		}
		for ( int i = 0; i < 3; i++ ) appendU32( out, 0xffffffffu );
		return out;
	}
};

quint16 truncFloat16( float value )
{
	return quint16( std::bit_cast<quint32>( value ) >> 16 );
}

QByteArray bodyProperties( const HknpEncodeInput & in )
{
	QByteArray out( 0x50, 0 );
	setU16( out, 0x10, 0xff00 );
	setU16( out, 0x12, truncFloat16( in.friction ) );
	setU16( out, 0x14, truncFloat16( in.friction ) );
	setU16( out, 0x16, truncFloat16( in.restitution ) );
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
	setFloat( out, 0x20, 0.17f ); setFloat( out, 0x24, 0.4905f );
	setFloat( out, 0x28, 0.0025f ); setFloat( out, 0x2c, 0.0025f ); setFloat( out, 0x30, 1.0f );
	return out;
}

QByteArray dynamicInertia( const HknpEncodeInput & in )
{
	QByteArray out( 0x40, 0 ); setU32( out, 0x00, 0x00010000u );
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
	return out;
}

QByteArray refCountedProperties()
{
	QByteArray out( 0x20, 0 ); setU32( out, 0x08, 1 ); setU32( out, 0x0c, 0x80000001u ); setU32( out, 0x18, 0x0000f100u ); return out;
}

QByteArray materialProperties( quint32 material )
{
	QByteArray out( 0x50, 0 ); setU32( out, 0x18, 2 ); setU32( out, 0x1c, 0x80000002u );
	setU32( out, 0x30, 1 ); setU32( out, 0x34, material ); setU32( out, 0x48, 1 ); setU32( out, 0x4c, material ); return out;
}

QByteArray compressedMeshHeader( quint32 material )
{
	QByteArray out( 0xc0, 0 ); out[0x10] = 4; out[0x11] = 2; out[0x12] = 7; out[0x13] = 2;
	setU32( out, 0x18, material ); setU32( out, 0x30, 0xffffffffu ); out[0x90] = 0x44; return out;
}

struct ClassEntry { quint32 hash; const char * name; };

QByteArray classNames( QHash<QString, quint32> & offsets )
{
	static const ClassEntry entries[] = {
		{ 0x33d42383u, "hkClass" }, { 0xb0efa719u, "hkClassMember" },
		{ 0x8a3609cfu, "hkClassEnum" }, { 0xce6f8a6cu, "hkClassEnumItem" },
		{ 0xb857718bu, "hknpPhysicsSystemData" }, { 0x5f60d536u, "hknpCompressedMeshShape" },
		{ 0xa2bdfc59u, "hknpCompressedMeshShapeData" }, { 0x7c574867u, "hkRefCountedProperties" },
		{ 0xa3e47a9au, "hknpBSMaterialProperties" }
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
	QByteArray out( 0x40, 0 ); for ( int i = 0; i < 0x14; i++ ) out[i] = char( 0xff );
	QByteArray n( name ); std::memcpy( out.data(), n.constData(), std::min( n.size(), qsizetype( 0x13 ) ) ); out[std::min( n.size(), qsizetype( 0x13 ) )] = 0;
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

	// One hknp section holds at most 255 packed vertices and 255 primitives
	// (u8 count fields — see hknpdecode's layout notes). Larger meshes are
	// partitioned into sections: triangles are sorted into spatial slabs
	// along the longest axis so consecutive tris share verts, then greedily
	// packed until either budget would overflow. Verts shared between
	// sections are simply duplicated (the shared-vertex table stays unused,
	// like every Elric sample we decoded).
	struct MeshSec { QVector<int> tris; QVector<quint16> verts; QHash<quint16, quint8> vmap; };
	QVector<MeshSec> secs;
	{
		Vector3 gmn = in.verts.first(), gmx = gmn;
		for ( const Vector3 & v : in.verts )
			for ( int a = 0; a < 3; a++ ) { gmn[a] = std::min( gmn[a], v[a] ); gmx[a] = std::max( gmx[a], v[a] ); }
		const Vector3 ext = gmx - gmn;
		int axis = 0;
		if ( ext[1] > ext[axis] ) axis = 1;
		if ( ext[2] > ext[axis] ) axis = 2;
		QVector<int> order( in.tris.size() );
		for ( int i = 0; i < order.size(); i++ ) order[i] = i;
		std::sort( order.begin(), order.end(), [&]( int a, int b ) {
			const Triangle & ta = in.tris.at( a ), & tb = in.tris.at( b );
			const float ca = in.verts[ta[0]][axis] + in.verts[ta[1]][axis] + in.verts[ta[2]][axis];
			const float cb = in.verts[tb[0]][axis] + in.verts[tb[1]][axis] + in.verts[tb[2]][axis];
			return ca < cb;
		} );
		MeshSec cur;
		auto flush = [&]() { if ( !cur.tris.isEmpty() ) { secs.append( cur ); cur = MeshSec(); } };
		for ( int ti : order ) {
			const Triangle & t = in.tris.at( ti );
			quint16 uniq[3]; int nu = 0;
			for ( int c = 0; c < 3; c++ ) {
				bool seen = false;
				for ( int p = 0; p < nu; p++ ) seen = seen || uniq[p] == t[c];
				if ( !seen ) uniq[nu++] = t[c];
			}
			int newVerts = 0;
			for ( int p = 0; p < nu; p++ )
				if ( !cur.vmap.contains( uniq[p] ) ) newVerts++;
			if ( cur.tris.size() >= 255 || cur.vmap.size() + newVerts > 255 )
				flush();
			for ( int p = 0; p < nu; p++ ) {
				if ( !cur.vmap.contains( uniq[p] ) ) {
					cur.vmap.insert( uniq[p], quint8( cur.verts.size() ) );
					cur.verts.append( uniq[p] );
				}
			}
			cur.tris.append( ti );
		}
		flush();
	}
	if ( secs.size() > 4096 )
		return fail( QStringLiteral( "Collision mesh needs more than 4096 hknp sections. Use Decimate first." ) );

	QHash<QString, quint32> names; QByteArray cn = classNames( names ); Fixups fx; QByteArray data;
	auto write = [&data]( const QByteArray & bytes ) { quint32 at = quint32( data.size() ); data.append( bytes ); return at; };
	quint32 psd = quint32( data.size() ); fx.virtuals.append( { psd, 0, names.value( QStringLiteral( "hknpPhysicsSystemData" ) ) } );
	write( hkArray( 0 ) ); quint32 arr10 = write( hkArray( 1 ) ); quint32 arr20 = write( hkArray( in.dynamic ? 1 : 0 ) );
	quint32 arr30 = write( hkArray( in.dynamic ? 1 : 0 ) ); quint32 arr40 = write( hkArray( 1 ) ); write( hkArray( 0 ) ); quint32 arr60 = write( hkArray( 1 ) ); write( QByteArray( 16, 0 ) );
	quint32 props = write( bodyProperties( in ) ); fx.local.append( { arr10, props } );
	if ( in.dynamic ) { quint32 motion = write( dynamicMotion( in ) ); fx.local.append( { arr20, motion } ); quint32 inertia = write( dynamicInertia( in ) ); fx.local.append( { arr30, inertia } ); }
	quint32 cinfo = write( bodyCInfo( in ) ); fx.local.append( { arr40, cinfo } );
	quint32 shapeEntry = write( QByteArray( 16, 0 ) ); fx.local.append( { arr60, shapeEntry } );
	quint32 shape = write( compressedMeshHeader( in.materialCRC ) );
	fx.virtuals.append( { shape, 0, names.value( QStringLiteral( "hknpCompressedMeshShape" ) ) } );
	fx.global.append( { cinfo, 2, shape } ); fx.global.append( { shapeEntry, 2, shape } );
	quint32 ref = write( refCountedProperties() ); fx.virtuals.append( { ref, 0, names.value( QStringLiteral( "hkRefCountedProperties" ) ) } );
	fx.local.append( { ref, ref + 0x10 } ); fx.global.append( { shape + 0x20, 2, ref } ); pad( data, 16 );
	quint32 mat = write( materialProperties( in.materialCRC ) ); fx.global.append( { ref + 0x10, 2, mat } );
	fx.virtuals.append( { mat, 0, names.value( QStringLiteral( "hknpBSMaterialProperties" ) ) } ); pad( data, 16 );

	Vector3 mn = in.verts.first(), mx = mn;
	for ( const Vector3 & v : in.verts ) for ( int a = 0; a < 3; a++ ) { mn[a] = std::min( mn[a], v[a] ); mx[a] = std::max( mx[a], v[a] ); }

	// per-section emission: each section quantizes against its OWN domain
	// (that is the point of sections — precision scales with density), quads
	// hold section-relative u8 indices, packed verts concatenate per section
	const bool legacySingle = ( secs.size() == 1 );
	QByteArray quads, packed, sectionsBlob;
	quint32 firstPrim = 0, totalPacked = 0;
	for ( const MeshSec & sc : std::as_const( secs ) ) {
		Vector3 smn = in.verts.at( sc.verts.first() ), smx = smn;
		for ( quint16 gv : sc.verts )
			for ( int a = 0; a < 3; a++ ) { smn[a] = std::min( smn[a], in.verts.at( gv )[a] ); smx[a] = std::max( smx[a], in.verts.at( gv )[a] ); }
		const Vector3 step( smx[0] > smn[0] ? ( smx[0] - smn[0] ) / 2047.0f : 1.0f,
			smx[1] > smn[1] ? ( smx[1] - smn[1] ) / 2047.0f : 1.0f,
			smx[2] > smn[2] ? ( smx[2] - smn[2] ) / 1023.0f : 1.0f );
		for ( int ti : sc.tris ) {
			const Triangle & t = in.tris.at( ti );
			const quint8 a = sc.vmap.value( t[0] ), b = sc.vmap.value( t[1] ), c = sc.vmap.value( t[2] );
			quads.append( char( a ) ); quads.append( char( b ) ); quads.append( char( c ) ); quads.append( char( c ) );
		}
		for ( quint16 gv : sc.verts ) {
			auto quant = []( float value, float base, float scale, int maximum ) { return std::clamp( int( std::lround( ( value - base ) / scale ) ), 0, maximum ); };
			const Vector3 & v = in.verts.at( gv );
			quint32 x = quint32( quant( v[0], smn[0], step[0], 2047 ) ), y = quint32( quant( v[1], smn[1], step[1], 2047 ) ), z = quint32( quant( v[2], smn[2], step[2], 1023 ) );
			appendU32( packed, x | ( y << 11 ) | ( z << 22 ) );
		}
		QByteArray section( 0x60, 0 );
		setU32( section, 0x0c, 0x80000000u );
		for ( int a = 0; a < 3; a++ ) { setFloat( section, 0x10 + a * 4, smn[a] ); setFloat( section, 0x20 + a * 4, smx[a] ); setFloat( section, 0x30 + a * 4, smn[a] ); setFloat( section, 0x3c + a * 4, step[a] ); }
		setU32( section, 0x48, totalPacked );
		// the decoded field is firstSharedIndex << 8 | numSharedIndices; the
		// in-game-validated single-section writer put the vert count here
		// (harmless: >> 8 is 0 for counts <= 255) — keep it byte-exact, and
		// write the semantically correct 0 for multi-section files
		setU32( section, 0x4c, legacySingle ? quint32( sc.verts.size() ) : 0u );
		setU32( section, 0x50, ( firstPrim << 8 ) | quint32( sc.tris.size() ) );
		section[0x58] = char( sc.verts.size() );
		sectionsBlob += section;
		firstPrim += quint32( sc.tris.size() );
		totalPacked += quint32( sc.verts.size() );
	}

	quint32 shapeData = quint32( data.size() ), sections = shapeData + 0xa0,
		quadData = sections + quint32( sectionsBlob.size() ), vertData = quadData + quint32( quads.size() );
	QByteArray sd( 0xa0, 0 ); setU32( sd, 0x0c, 0x80000000u ); setU32( sd, 0x1c, 0x80000000u );
	for ( int a = 0; a < 3; a++ ) { setFloat( sd, 0x20 + a * 4, mn[a] ); setFloat( sd, 0x30 + a * 4, mx[a] ); }
	setU32( sd, 0x4c, 0x80000000u );
	setU32( sd, 0x58, quint32( secs.size() ) ); setU32( sd, 0x5c, 0x80000000u | quint32( secs.size() ) );
	setU32( sd, 0x68, quint32( in.tris.size() ) ); setU32( sd, 0x6c, 0x80000000u | quint32( in.tris.size() ) );
	setU32( sd, 0x7c, 0x80000000u ); setU32( sd, 0x88, totalPacked ); setU32( sd, 0x8c, 0x80000000u | totalPacked ); setU32( sd, 0x9c, 0x80000000u );
	write( sd ); fx.virtuals.append( { shapeData, 0, names.value( QStringLiteral( "hknpCompressedMeshShapeData" ) ) } ); fx.global.append( { shape + 0x60, 2, shapeData } );
	fx.local.append( { shapeData + 0x50, sections } ); fx.local.append( { shapeData + 0x60, quadData } ); fx.local.append( { shapeData + 0x80, vertData } );
	write( sectionsBlob ); write( quads ); write( packed ); pad( data, 16 );

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

	// +0x70..+0xCF verbatim: bounds and whatever else lives there
	if ( compound.headerTail.size() == 0x60 )
		std::memcpy( out.data() + 0x70, compound.headerTail.constData(), 0x60 );

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

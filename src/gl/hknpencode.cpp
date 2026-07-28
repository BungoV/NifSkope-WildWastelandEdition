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
	QByteArray globalTable() const
	{
		QByteArray out; for ( const GlobalFix & f : global ) {
			appendU32( out, f.source ); appendU32( out, f.section ); appendU32( out, f.target );
		}
		pad( out, 16, char( 0xff ) ); return out;
	}
	QByteArray virtualTable() const
	{
		QByteArray out; for ( const VirtualFix & f : virtuals ) {
			appendU32( out, f.object ); appendU32( out, f.section ); appendU32( out, f.name );
		}
		pad( out, 16, char( 0xff ) ); return out;
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
		{ 0xa3e47a9au, "hknpBSMaterialProperties" }
	};
	for ( const ClassEntry & e : entries ) {
		if ( name == QLatin1String( e.name ) )
			return e.hash;
	}
	return 0;
}

} // namespace

QByteArray hknpBuildPackfile( const QVector<HknpPackObject> & objects, QString * error )
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
		const quint32 hash = classHash( name );
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
	// local ascends by source; virtual already does, being emitted in object
	// order; global keeps the caller's order, which is the reflection walk
	std::sort( fx.local.begin(), fx.local.end() );

	QByteArray local = fx.localTable(), global = fx.globalTable(), virtuals = fx.virtualTable();
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
QByteArray hknpEncodeRagdollData( const HknpSystem & sys, HknpRagdollDataFixups * fixups )
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

	QByteArray props, motions( nmotion * 0x40, 0 ), inertias( nmotion * 0x70, 0 ), cinfos;
	for ( qsizetype i = 0; i < nb; i++ ) {
		const HknpBodyPhys & b = sys.bodyPhys.at( i );

		QByteArray p( 0x50, 0 );
		setU16( p, 0x10, 0xff00 );
		setU16( p, 0x12, truncFloat16( b.friction ) );
		setU16( p, 0x14, truncFloat16( b.friction ) );
		setU16( p, 0x16, truncFloat16( b.restitution ) );
		setU32( p, 0x18, 0x3d4c0201u ); setU32( p, 0x1c, 0x7f7fffeeu );
		setFloat( p, 0x20, 1.0f ); setFloat( p, 0x24, 1.0f );
		setU32( p, 0x38, 0x000040a0u );
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
		setU32( c, 0x14, ( b.layer & 0xffu ) | ( quint32( b.filterFlags ) << 8 )
			| ( quint32( b.filterGroup ) << 16 ) );
		setU32( c, 0x18, 0x00010080u );
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
		setU16( inertias, n + 0x00, quint16( b.motionIndex ) ); setU16( inertias, n + 0x02, 1 );
		setFloat( inertias, n + 0x04, b.mass > 1.0e-12f ? 1.0f / b.mass : 0.0f );
		setFloat( inertias, n + 0x08, b.density );
		setU32( inertias, n + 0x0c, 0x5f7ffff0u ); setU32( inertias, n + 0x10, 0x5f7ffff0u );
		for ( int a = 0; a < 3; a++ ) {
			setFloat( inertias, n + 0x20 + a * 4, b.invInertia[a] );
			setFloat( inertias, n + 0x30 + a * 4, b.motionCom[a] );
		}
		setFloat( inertias, n + 0x2c, 1.0f );
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

	QByteArray out( 0x90, 0 );
	auto place = [&]( qsizetype desc, quint32 count, const QByteArray & payload ) {
		setU32( out, desc + 8, count );
		setU32( out, desc + 12, count | 0x80000000u );
		const qsizetype at = out.size();
		out.append( payload );
		pad( out, 16 );
		return at;
	};
	HknpRagdollDataFixups fx;
	const qsizetype propsAt = place( 0x10, quint32( nb ), props );
	const qsizetype motionAt = place( 0x20, quint32( nmotion ), motions );
	const qsizetype inertiaAt = place( 0x30, quint32( nmotion ), inertias );
	const qsizetype cinfoAt = place( 0x40, quint32( nb ), cinfos );
	const qsizetype consAt = place( 0x50, quint32( ncon ), cons );
	const qsizetype listAt = place( 0x60, quint32( sys.shapeListOrder.size() ),
		QByteArray( sys.shapeListOrder.size() * 8, 0 ) );
	const qsizetype mapAt = place( 0x80, quint32( nbone ), boneMap );

	fx.local = { { 0x10, propsAt }, { 0x20, motionAt }, { 0x30, inertiaAt },
		{ 0x40, cinfoAt }, { 0x50, consAt }, { 0x60, listAt }, { 0x80, mapAt } };
	for ( qsizetype i = 0; i < nb; i++ )
		fx.bodyShapePointers.append( cinfoAt + i * 0x60 );
	for ( qsizetype i = 0; i < ncon; i++ )
		fx.constraintPointers.append( consAt + i * 0x18 );
	for ( qsizetype i = 0; i < sys.shapeListOrder.size(); i++ )
		fx.shapeListPointers.append( listAt + i * 8 );
	if ( fixups )
		*fixups = fx;
	return out;
}

namespace {

/*! One shape as a packfile object. Returns false for a class with no encoder.
 *
 * Convex polytopes own an hkRefCountedProperties holding their mass properties;
 * those two objects are appended straight after the shape, which is where every
 * vanilla file puts them.
 */
bool encodeShapeObject( const HknpShape & shp, QVector<HknpPackObject> & objs )
{
	HknpPackObject so;
	if ( shp.primType == 2 && shp.coreVerts.size() == 8 ) {
		HknpCapsuleInput in;
		in.capA = shp.capA; in.capB = shp.capB;
		in.radius = shp.convexRadius; in.materialCRC = shp.shapeMaterialCRC;
		in.padding = shp.corePadding;
		// bit 1 of the vertex index selects the +u side, so the difference of the
		// two 4-corner centroids recovers the roll the file was written with
		Vector3 hi, lo;
		for ( int v = 0; v < 8; v++ )
			( ( v & 2 ) ? hi : lo ) += shp.coreVerts.at( v );
		in.frameU = hi - lo;
		in.hasFrame = in.frameU.length() > 1.0e-12f;
		so.className = QStringLiteral( "hknpCapsuleShape" );
		so.bytes = hknpEncodeCapsuleShape( in );
	} else if ( shp.primType == 1 ) {
		so.className = QStringLiteral( "hknpSphereShape" );
		so.bytes = hknpEncodeSphereShape( shp.capA, shp.convexRadius, shp.shapeMaterialCRC );
	} else if ( shp.isConvex && !shp.faces.isEmpty()
		&& shp.faceAngles.size() == shp.faces.size() ) {
		HknpPolytopeInput pin;
		pin.verts = shp.verts; pin.planes = shp.planes; pin.faces = shp.faces;
		pin.faceAngles = shp.faceAngles; pin.convexRadius = shp.convexRadius;
		pin.materialCRC = shp.shapeMaterialCRC; pin.shapeFlags = shp.shapeFlags;
		so.className = QStringLiteral( "hknpConvexPolytopeShape" );
		so.bytes = hknpEncodeConvexPolytopeShape( pin );
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
	if ( shp.rawData.size() == so.bytes.size() )
		so.bytes = shp.rawData;

	const qsizetype shapeIndex = objs.size();
	objs.append( so );
	if ( !shp.hasMassProps )
		return true;

	objs[shapeIndex].global.append( { 0x20, int( objs.size() ) } );
	HknpPackObject rc;
	rc.className = QStringLiteral( "hkRefCountedProperties" );
	rc.bytes = refCountedProperties();
	rc.local.append( { 0x00, 0x10 } );
	rc.global.append( { 0x10, int( objs.size() ) + 1 } );
	objs.append( rc );

	HknpPackObject mp;
	mp.className = QStringLiteral( "hknpShapeMassProperties" );
	mp.bytes = hknpEncodeShapeMassProperties( shp.massCom, shp.massInertiaRaw,
		shp.massVolume, shp.massMass, shp.massMajorAxis );
	if ( shp.massRawData.size() == mp.bytes.size() )
		mp.bytes = shp.massRawData;
	objs.append( mp );
	return true;
}

} // namespace

QByteArray hknpEncodeRagdoll( const HknpSystem & sys, QString * error )
{
	auto fail = [error]( const QString & message ) { if ( error ) *error = message; return QByteArray(); };
	if ( sys.bodyPhys.isEmpty() || sys.bones.isEmpty() )
		return fail( QStringLiteral( "Not a ragdoll: it has no bodies or no skeleton." ) );

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
	root.className = QStringLiteral( "hknpRagdollData" );
	root.bytes = hknpEncodeRagdollData( sys, &rfx );
	if ( root.bytes.isEmpty() )
		return fail( QStringLiteral( "Could not write the ragdoll root object." ) );
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
			co.bytes = hknpEncodeCompoundShape( *comp, &cfx );
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
			if ( comp->dataRawData.isEmpty() || comp->dataClassName.isEmpty() )
				return fail( QStringLiteral( "The compound on body %1 has no shape data." ).arg( i ) );
			objs[at].global.append( { cfx.shapeDataPointer, int( objs.size() ) } );
			HknpPackObject cd;
			cd.className = comp->dataClassName;
			cd.bytes = comp->dataRawData;
			cd.local = comp->dataLocal;
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

	HknpSkeletonFixups sfx;
	HknpPackObject sk;
	sk.className = QStringLiteral( "hkaSkeleton" );
	sk.bytes = hknpEncodeSkeleton( sys.bones, &sfx );
	if ( sk.bytes.isEmpty() )
		return fail( QStringLiteral( "Could not write the ragdoll skeleton." ) );
	sk.local = { { sfx.parentsPointer, sfx.parents }, { sfx.bonesPointer, sfx.bones },
		{ sfx.posePointer, sfx.pose } };
	const int skelObj = int( objs.size() );
	objs.append( sk );

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
	refs.append( { rfx.skeletonPointer, skelObj } );
	objs[0].global = refs;

	return hknpBuildPackfile( objs, error );
}

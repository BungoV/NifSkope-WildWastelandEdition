#include "hknpdecode.h"

#include <QHash>

#include <cmath>
#include <cstring>

/* Havok 2014 binary packfile (hk_2014.1.0-r1, 64-bit, little-endian):
   0x40-byte file header, then one 0x40-byte header per section
   (__classnames__, __types__, __data__). Section header: char tag[19+1],
   then 7 int32: absoluteDataStart, localFixupsOffset, globalFixupsOffset,
   virtualFixupsOffset, exportsOffset, importsOffset, endOffset (all data-
   relative; fixup tables are 0xFF-padded). Virtual fixups (dataOff,
   sectionIdx, classNameOff) locate the objects; local fixups (src, dst)
   are the intra-section pointer patches; global fixups (src, sectionIdx,
   dst) patch pointers to other OBJECTS (e.g. compound instance -> child).

   Validated against before/after Elric pairs (see TO_BE_IMPLEMENTED.md):

   - hknpConvexPolytopeShape / hknpSphereShape / hknpCapsuleShape all share
     the convex layout: convexRadius at +0x14; hkRelArrays (u16 count + u16
     offset relative to the descriptor's own address) at +0x30 vertices
     (hkVector4, w = id), +0x40 planes, +0x44 faces (u16 firstIndex,
     u8 numIndices, u8 flags), +0x48 u8 indices. A sphere is one unique
     vertex (the center) + convexRadius; a capsule additionally stores its
     exact end points at +0x50 / +0x60 (hkVector4, w = 1).

   - hknpCompressedMeshShapeData: global domain min/max hkVector4 at
     +0x20/+0x30; hkArray members (payloads via local fixups): +0x50
     sections (0x60 bytes each), +0x60 primitives (u8[4], d==c -> triangle),
     +0x70 sharedVerticesIndex (u16), +0x80 packedVertices (u32,
     x:11|y:11|z:10 quantized as offset + bits*step), +0x90 sharedVertices
     (u64, x:21|y:21|z:22 quantized in the global domain). Section: +0x30
     codec offset vec3, +0x3c steps vec3, +0x48 u32 firstPackedVertex,
     +0x4c u32 (firstSharedIndex << 8), +0x50 u32 (firstPrimitive << 8 |
     numPrimitives), +0x58 u8 numPackedVertices.

   - hknpDynamicCompoundShape: hkArray of 0x80-byte instances at +0x60.
     Instance: 3 rotation basis rows (hkVector4) at +0x00, translation at
     +0x30, scale at +0x40, child shape pointer at +0x50 (patched by a
     GLOBAL fixup). The rows/translation are byte-identical to the source
     bhkConvexTransformShape Matrix4 rows. */

namespace {

struct Reader
{
	const quint8 * d;
	qsizetype n;
	bool ok = true;

	quint8 u8( qsizetype o )
	{
		if ( o < 0 || o >= n ) {
			ok = false;
			return 0;
		}
		return d[o];
	}
	quint16 u16( qsizetype o )
	{
		if ( o < 0 || o + 2 > n ) {
			ok = false;
			return 0;
		}
		quint16 v;
		std::memcpy( &v, d + o, 2 );
		return v;
	}
	quint32 u32( qsizetype o )
	{
		if ( o < 0 || o + 4 > n ) {
			ok = false;
			return 0;
		}
		quint32 v;
		std::memcpy( &v, d + o, 4 );
		return v;
	}
	quint64 u64( qsizetype o )
	{
		if ( o < 0 || o + 8 > n ) {
			ok = false;
			return 0;
		}
		quint64 v;
		std::memcpy( &v, d + o, 8 );
		return v;
	}
	float f32( qsizetype o )
	{
		quint32 v = u32( o );
		float f;
		std::memcpy( &f, &v, 4 );
		return f;
	}
	Vector3 vec3( qsizetype o )
	{
		return Vector3( f32( o ), f32( o + 4 ), f32( o + 8 ) );
	}
};

//! Fan-triangulate a convex face loop into shape.tris
static void addFaceFan( HknpShape & shape, const QVector<int> & loop )
{
	for ( int k = 1; k + 1 < loop.size(); k++ ) {
		if ( loop[0] < shape.verts.size() && loop[k] < shape.verts.size() && loop[k+1] < shape.verts.size() )
			shape.tris.append( Triangle( quint16( loop[0] ), quint16( loop[k] ), quint16( loop[k+1] ) ) );
	}
}

//! Low-res UV sphere for previewing sphere / capsule primitives
static void synthSphere( HknpShape & shape, const Vector3 & c, float r )
{
	const int SEG = 12, RINGS = 6;
	shape.verts.clear();
	shape.tris.clear();
	for ( int ring = 0; ring <= RINGS; ring++ ) {
		float phi = float( M_PI ) * float( ring ) / float( RINGS );
		for ( int s = 0; s < SEG; s++ ) {
			float th = 2.0f * float( M_PI ) * float( s ) / float( SEG );
			shape.verts.append( c + Vector3( std::sin( phi ) * std::cos( th ),
											std::sin( phi ) * std::sin( th ),
											std::cos( phi ) ) * r );
		}
	}
	for ( int ring = 0; ring < RINGS; ring++ ) {
		for ( int s = 0; s < SEG; s++ ) {
			int a = ring * SEG + s, b = ring * SEG + ( s + 1 ) % SEG;
			int cc = a + SEG, dd = b + SEG;
			shape.tris.append( Triangle( quint16( a ), quint16( b ), quint16( cc ) ) );
			shape.tris.append( Triangle( quint16( b ), quint16( dd ), quint16( cc ) ) );
		}
	}
}

//! Capsule preview: cylinder body + hemispherical caps around segment a-b
static void synthCapsule( HknpShape & shape, const Vector3 & a, const Vector3 & b, float r )
{
	Vector3 axis = b - a;
	float len = axis.length();
	if ( len < 1.0e-6f ) {
		synthSphere( shape, a, r );
		return;
	}
	axis /= len;
	// orthonormal frame
	Vector3 up = ( std::fabs( axis[2] ) < 0.9f ) ? Vector3( 0, 0, 1 ) : Vector3( 1, 0, 0 );
	Vector3 u = Vector3::crossproduct( up, axis );
	u.normalize();
	Vector3 v = Vector3::crossproduct( axis, u );

	const int SEG = 12, CAP = 3;
	shape.verts.clear();
	shape.tris.clear();
	// rings from bottom cap through the body to the top cap
	QVector<float> ringOffs;
	QVector<float> ringRads;
	for ( int i = CAP; i >= 0; i-- ) {	// bottom hemisphere (around a)
		float phi = float( M_PI ) * 0.5f * float( i ) / float( CAP );
		ringOffs.append( -std::sin( phi ) * r );
		ringRads.append( std::cos( phi ) * r );
	}
	for ( int i = 0; i <= CAP; i++ ) {	// top hemisphere (around b)
		float phi = float( M_PI ) * 0.5f * float( i ) / float( CAP );
		ringOffs.append( len + std::sin( phi ) * r );
		ringRads.append( std::cos( phi ) * r );
	}
	for ( int ring = 0; ring < ringOffs.size(); ring++ ) {
		Vector3 c = a + axis * ringOffs[ring];
		for ( int s = 0; s < SEG; s++ ) {
			float th = 2.0f * float( M_PI ) * float( s ) / float( SEG );
			shape.verts.append( c + ( u * std::cos( th ) + v * std::sin( th ) ) * ringRads[ring] );
		}
	}
	for ( int ring = 0; ring + 1 < ringOffs.size(); ring++ ) {
		for ( int s = 0; s < SEG; s++ ) {
			int p0 = ring * SEG + s, p1 = ring * SEG + ( s + 1 ) % SEG;
			int p2 = p0 + SEG, p3 = p1 + SEG;
			shape.tris.append( Triangle( quint16( p0 ), quint16( p1 ), quint16( p2 ) ) );
			shape.tris.append( Triangle( quint16( p1 ), quint16( p3 ), quint16( p2 ) ) );
		}
	}
}

//! hknpConvexPolytopeShape and subclasses (sphere / capsule)
static void decodeConvexLike( Reader & r, qsizetype B, const QString & className, HknpShape & shape )
{
	shape.isConvex = true;
	shape.convexRadius = r.f32( B + 0x14 );
	shape.materialCRC = r.u32( B + 0x18 );

	auto relArray = [&]( qsizetype fieldOff, quint16 & count, qsizetype & payload ) {
		count = r.u16( fieldOff );
		payload = fieldOff + r.u16( fieldOff + 2 );
	};

	quint16 nv, np, nf, ni;
	qsizetype pv, pp, pf, pi;
	relArray( B + 0x30, nv, pv );
	relArray( B + 0x40, np, pp );
	relArray( B + 0x44, nf, pf );
	relArray( B + 0x48, ni, pi );
	if ( nv > 4096 || nf > 4096 || ni > 16384 )
		return;	// sanity: not the layout we expect

	QVector<Vector3> raw;
	for ( int i = 0; i < nv; i++ )
		raw.append( r.vec3( pv + i * 16 ) );

	if ( className == QLatin1String( "hknpSphereShape" ) ) {
		// one unique vertex (SIMD-padded to 4) + convexRadius
		Vector3 c = raw.isEmpty() ? Vector3() : raw.first();
		bool allSame = true;
		for ( const Vector3 & p : raw )
			allSame = allSame && ( p - c ).length() < 1.0e-5f;
		if ( allSame ) {
			shape.primType = 1;
			shape.capA = c;
			shape.primRadius = shape.convexRadius;
			synthSphere( shape, c, shape.primRadius );
			return;
		}
	} else if ( className == QLatin1String( "hknpCapsuleShape" ) ) {
		// exact end points at +0x50 / +0x60; radius = convexRadius plus the
		// small core hull margin (recovered from the hull verts)
		Vector3 a = r.vec3( B + 0x50 );
		Vector3 b = r.vec3( B + 0x60 );
		float margin = 0.0f;
		Vector3 ab = b - a;
		float abLen2 = Vector3::dotproduct( ab, ab );
		for ( const Vector3 & p : raw ) {
			float t = ( abLen2 > 1.0e-12f ) ? Vector3::dotproduct( p - a, ab ) / abLen2 : 0.0f;
			t = std::min( std::max( t, 0.0f ), 1.0f );
			margin = std::max( margin, ( p - ( a + ab * t ) ).length() );
		}
		shape.primType = 2;
		shape.capA = a;
		shape.capB = b;
		shape.primRadius = shape.convexRadius + margin;
		synthCapsule( shape, a, b, shape.primRadius );
		return;
	}

	shape.verts = raw;
	for ( int i = 0; i < np; i++ )
		shape.planes.append( Vector4( r.f32( pp + i*16 ), r.f32( pp + i*16 + 4 ),
										r.f32( pp + i*16 + 8 ), r.f32( pp + i*16 + 12 ) ) );
	for ( int f = 0; f < nf; f++ ) {
		quint16 first = r.u16( pf + f * 4 );
		quint8 num = r.u8( pf + f * 4 + 2 );
		QVector<int> loop;
		for ( int k = 0; k < num && first + k < ni; k++ )
			loop.append( r.u8( pi + first + k ) );
		if ( loop.size() >= 3 ) {
			shape.faces.append( loop );
			addFaceFan( shape, loop );
		}
	}
}

static void decodeCompressedMesh( Reader & r, qsizetype B, const QHash<qsizetype, qsizetype> & local,
									HknpShape & shape )
{
	Vector3 gmin = r.vec3( B + 0x20 );
	Vector3 gmax = r.vec3( B + 0x30 );
	quint32 nsec = r.u32( B + 0x58 );
	qsizetype secp = local.value( B + 0x50, -1 );
	qsizetype prim = local.value( B + 0x60, -1 );
	qsizetype shix = local.value( B + 0x70, -1 );
	qsizetype pack = local.value( B + 0x80, -1 );
	qsizetype shar = local.value( B + 0x90, -1 );
	if ( secp < 0 || prim < 0 || pack < 0 || nsec > 4096 )
		return;

	QHash<quint64, int> vertIndex;
	auto addVert = [&]( quint64 key, const Vector3 & v ) -> int {
		auto it = vertIndex.constFind( key );
		if ( it != vertIndex.constEnd() )
			return *it;
		int idx = int( shape.verts.size() );
		vertIndex.insert( key, idx );
		shape.verts.append( v );
		return idx;
	};

	for ( quint32 s = 0; s < nsec && r.ok; s++ ) {
		qsizetype S = secp + qsizetype( s ) * 0x60;
		Vector3 off = r.vec3( S + 0x30 );
		Vector3 stp = r.vec3( S + 0x3c );
		quint32 firstPacked = r.u32( S + 0x48 );
		quint32 firstShared = r.u32( S + 0x4c ) >> 8;
		quint32 pf = r.u32( S + 0x50 );
		quint32 firstPrim = pf >> 8, numPrim = pf & 0xFF;
		quint32 numPacked = r.u8( S + 0x58 );

		auto vert = [&]( quint8 idx ) -> int {
			if ( idx < numPacked ) {
				quint32 v = r.u32( pack + qsizetype( firstPacked + idx ) * 4 );
				Vector3 p( off[0] + float( v & 0x7FF ) * stp[0],
							off[1] + float( ( v >> 11 ) & 0x7FF ) * stp[1],
							off[2] + float( ( v >> 22 ) & 0x3FF ) * stp[2] );
				return addVert( quint64( v ) | ( quint64( s + 1 ) << 32 ), p );
			}
			if ( shix < 0 || shar < 0 )
				return -1;
			quint16 si = r.u16( shix + qsizetype( firstShared + idx - numPacked ) * 2 );
			quint64 w = r.u64( shar + qsizetype( si ) * 8 );
			Vector3 p( gmin[0] + float( w & 0x1FFFFF ) / 2097151.0f * ( gmax[0] - gmin[0] ),
						gmin[1] + float( ( w >> 21 ) & 0x1FFFFF ) / 2097151.0f * ( gmax[1] - gmin[1] ),
						gmin[2] + float( ( w >> 42 ) & 0x3FFFFF ) / 4194303.0f * ( gmax[2] - gmin[2] ) );
			return addVert( 0x8000000000000000ULL | si, p );
		};

		for ( quint32 t = 0; t < numPrim && r.ok; t++ ) {
			qsizetype P = prim + qsizetype( firstPrim + t ) * 4;
			int a = vert( r.u8( P ) ), b = vert( r.u8( P + 1 ) );
			int c = vert( r.u8( P + 2 ) );
			quint8 dRaw = r.u8( P + 3 );
			if ( a < 0 || b < 0 || c < 0 || shape.verts.size() > 65535 )
				continue;
			shape.tris.append( Triangle( quint16( a ), quint16( b ), quint16( c ) ) );
			if ( dRaw != r.u8( P + 2 ) ) {	// quad: second triangle
				int dd = vert( dRaw );
				if ( dd >= 0 )
					shape.tris.append( Triangle( quint16( a ), quint16( c ), quint16( dd ) ) );
			}
		}
	}
}

} // namespace

HknpSystem hknpDecode( const QByteArray & data )
{
	HknpSystem sys;
	Reader r{ reinterpret_cast<const quint8 *>( data.constData() ), data.size() };

	if ( data.size() < 0x100 || r.u32( 0 ) != 0x57E0E057 || r.u32( 4 ) != 0x10C0C010 ) {
		sys.error = QStringLiteral( "not a Havok binary packfile" );
		return sys;
	}
	qint32 numSections = qint32( r.u32( 20 ) );
	if ( numSections < 2 || numSections > 8 ) {
		sys.error = QStringLiteral( "unexpected section count" );
		return sys;
	}

	qsizetype cnStart = -1, cnLen = 0;
	qsizetype dataStart = -1, localOff = 0, globalOff = 0, virtOff = 0, expOff = 0;
	for ( int s = 0; s < numSections; s++ ) {
		qsizetype off = 0x40 + qsizetype( s ) * 0x40;
		char tag[20] = {};
		for ( int i = 0; i < 19; i++ )
			tag[i] = char( r.u8( off + i ) );
		qsizetype absStart = r.u32( off + 20 );
		if ( std::strcmp( tag, "__classnames__" ) == 0 ) {
			cnStart = absStart;
			cnLen = r.u32( off + 24 );
		} else if ( std::strcmp( tag, "__data__" ) == 0 ) {
			dataStart = absStart;
			localOff = r.u32( off + 24 );
			globalOff = r.u32( off + 28 );
			virtOff = r.u32( off + 32 );
			expOff = r.u32( off + 36 );
		}
	}
	if ( cnStart < 0 || dataStart < 0 || !r.ok ) {
		sys.error = QStringLiteral( "missing __classnames__ / __data__ section" );
		return sys;
	}

	// class names: [u32 crc][0x09][name\0]... — fixups reference the NAME offset
	QHash<qsizetype, QString> classNames;
	{
		qsizetype p = cnStart;
		while ( p + 5 < cnStart + cnLen && p + 5 < data.size() ) {
			if ( r.u8( p + 4 ) != 0x09 )
				break;
			qsizetype e = p + 5;
			while ( e < data.size() && r.u8( e ) != 0 )
				e++;
			classNames.insert( p + 5 - cnStart,
				QString::fromLatin1( reinterpret_cast<const char *>( r.d + p + 5 ), int( e - p - 5 ) ) );
			p = e + 1;
		}
	}

	// local fixups: intra-section pointer patches (member offset -> payload)
	QHash<qsizetype, qsizetype> local;
	for ( qsizetype p = dataStart + localOff; p + 8 <= dataStart + globalOff && p + 8 <= data.size(); p += 8 ) {
		qint32 src = qint32( r.u32( p ) ), dst = qint32( r.u32( p + 4 ) );
		if ( src != -1 )
			local.insert( dataStart + src, dataStart + dst );
	}

	// global fixups: pointers to other objects (compound instance -> child)
	QHash<qsizetype, qsizetype> global;
	for ( qsizetype p = dataStart + globalOff; p + 12 <= dataStart + virtOff && p + 12 <= data.size(); p += 12 ) {
		qint32 src = qint32( r.u32( p ) ), dst = qint32( r.u32( p + 8 ) );
		if ( src != -1 )
			global.insert( dataStart + src, dataStart + dst );
	}

	// virtual fixups: object offset -> class name
	QVector<QPair<qsizetype, QString>> objects;
	QHash<qsizetype, QString> objClass;
	for ( qsizetype p = dataStart + virtOff; p + 12 <= dataStart + expOff && p + 12 <= data.size(); p += 12 ) {
		qint32 src = qint32( r.u32( p ) );
		qint32 cno = qint32( r.u32( p + 8 ) );
		if ( src != -1 ) {
			objects.append( { dataStart + src, classNames.value( cno ) } );
			objClass.insert( dataStart + src, classNames.value( cno ) );
		}
	}
	std::sort( objects.begin(), objects.end() );

	// Bethesda stores the physical-material CRC table in
	// hknpBSMaterialProperties. Body cinfos reference this table with the u16
	// material ID at +0x12. Each table entry is 0x18 bytes and its CRC is +0x04;
	// the entry count is at +0x18. This matters for convex bodies: their shape
	// header +0x18 is not a reliable Bethesda material CRC.
	QVector<quint32> bodyMaterials;
	for ( const auto & obj : objects ) {
		if ( obj.second != QLatin1String( "hknpBSMaterialProperties" ) )
			continue;
		quint32 count = r.u32( obj.first + 0x18 );
		if ( count > 256 )
			continue;
		for ( quint32 i = 0; i < count; i++ )
			bodyMaterials.append( r.u32( obj.first + 0x34 + qsizetype( i ) * 0x18 ) );
	}

	// the CMSD holding the geometry for a hknpCompressedMeshShape is the next
	// hknpCompressedMeshShapeData object after it
	auto findCMSD = [&]( qsizetype after ) -> qsizetype {
		for ( const auto & o : objects ) {
			if ( o.first > after && o.second == QLatin1String( "hknpCompressedMeshShapeData" ) )
				return o.first;
		}
		return -1;
	};

	// body transforms: hknpPhysicsSystemData bodyCinfos (payload via local
	// fixup at PSD+0x40, 0x60 bytes each) place each top-level shape: shape
	// pointer at +0x00 (global fixup), position at +0x30, orientation
	// quaternion at +0x40. Stair helpers etc. are separate bodies placed
	// this way. Multiple bodies may share one shape (instancing).
	struct BodyT
	{
		int id = -1;
		Vector3 rows[3];
		Vector3 trans;
		bool ident;
	};
	QVector<QPair<qsizetype, BodyT>> bodies;
	for ( const auto & obj : objects ) {
		// hknpRagdollData is a ragdoll's packfile root and it DERIVES from
		// hknpPhysicsSystemData: the same array offsets hold at the object base.
		// Verified on the vanilla brahmin skeleton - +0x10 body_props, +0x20
		// dyn_motion, +0x30 dyn_inertia, +0x40 bodyCinfos and +0x60 shape entries
		// all read 39 (its bone count), and every one of the 39 cinfos resolves
		// through its global fixup to a distinct hknpCapsuleShape in exact index
		// order, body i -> capsule i.
		//
		// Matching only the physics class is why ragdolls decoded 0 bodies and
		// every capsule came back unattributed. With this, a ragdoll's per-bone
		// bodies are decoded outright - real filter/motion/friction per bone
		// rather than the positional inference lower down.
		//
		// (Its two extra arrays are not read here: +0x50 holds 38 entries on a
		// 39-bone ragdoll - one per non-root bone, i.e. the constraint bindings,
		// which is the lead for decoding the joints - and +0x80 holds 39.)
		if ( obj.second != QLatin1String( "hknpPhysicsSystemData" )
			&& obj.second != QLatin1String( "hknpRagdollData" ) )
			continue;
		qsizetype cinfos = local.value( obj.first + 0x40, -1 );
		quint32 nb = r.u32( obj.first + 0x48 );
		if ( cinfos < 0 || nb > 4096 )
			continue;
		// per-body physics: body_props array at PSD+0x10 (0x50 bytes per
		// body; friction/restitution as truncated float16 - the upper 16
		// bits of the float32)
		qsizetype bprops = local.value( obj.first + 0x10, -1 );
		auto truncF16 = [&]( qsizetype off ) -> float {
			quint32 u = quint32( r.u16( off ) ) << 16;
			float f;
			std::memcpy( &f, &u, 4 );
			return f;
		};
		// dyn_motion (PSD+0x20) / dyn_inertia (PSD+0x30) exist only when the
		// system simulates dynamically (props); statics lack both
		qsizetype dm = local.value( obj.first + 0x20, -1 );
		qsizetype di = local.value( obj.first + 0x30, -1 );
		if ( dm >= 0 && r.u32( obj.first + 0x28 ) > 0 ) {
			sys.dynamic = true;
			sys.gravityFactor = r.f32( dm + 0x08 );
			sys.maxLinVelocity = r.f32( dm + 0x10 );
			sys.maxAngVelocity = r.f32( dm + 0x14 );
			sys.linDamping = r.f32( dm + 0x18 );
			sys.angDamping = r.f32( dm + 0x1c );
		}
		if ( di >= 0 && r.u32( obj.first + 0x38 ) > 0 ) {
			float invMass = r.f32( di + 0x04 );
			sys.mass = ( invMass > 1.0e-12f ) ? 1.0f / invMass : 0.0f;
			sys.density = r.f32( di + 0x08 );
			sys.inertia = r.vec3( di + 0x20 );
		}
		for ( quint32 i = 0; i < nb; i++ ) {
			qsizetype c = cinfos + qsizetype( i ) * 0x60;
			HknpBodyPhys phys;
			// The collision filter stores the layer in the low byte. Some
			// packfiles leave it at zero; zero is only "unidentified", so use
			// the same useful default Elric authors for the body's motion state.
			quint32 packedFilter = r.u32( c + 0x14 );
			// Builds produced by NifSkope's first encoder revision placed this at
			// +0x1C. Read those files compatibly while preferring vanilla +0x14.
			quint32 oldPackedFilter = r.u32( c + 0x1c );
			if ( oldPackedFilter && ( packedFilter & 0xffU ) <= 1U )
				packedFilter = oldPackedFilter;
			phys.layer = packedFilter & 0xffU;
			phys.filterFlags = quint8( ( packedFilter >> 8 ) & 0xffU );
			phys.filterGroup = quint16( packedFilter >> 16 );
			quint16 materialId = r.u16( c + 0x12 );
			if ( materialId < bodyMaterials.size() )
				phys.materialCRC = bodyMaterials.at( materialId );
			phys.hasMotion = ( r.u32( c + 0x0c ) != 0x7fffffffu );
			if ( phys.layer == 0 )
				phys.layer = ( sys.dynamic && phys.hasMotion ) ? 10u : 1u;
			phys.com = r.vec3( c + 0x30 );
			/* Per-body dynamics.
			 *
			 * cinfo +0x0c is the body's MOTION INDEX into dyn_motion /
			 * dyn_inertia, not its own index - 0x7fffffff means static, which is
			 * what hasMotion above tests. Indexing by the body index instead is
			 * wrong in a way that looks plausible: on the brahmin it gives 4 kg
			 * toes, 5 kg ears and a 1 kg head, where the motion index tapers both
			 * limbs 5 -> 3 -> 1 -> 1 from thigh to toe and from upper arm to palm,
			 * and puts the 20 kg on Spine4.
			 *
			 * The arrays carry their OWN counts (+0x28, +0x38), which need not
			 * equal the body count - a 15-body Halloween banner has 14 dynamic
			 * entries because one body anchors it. Testing the motion index
			 * against those counts covers that and the static case together.
			 *
			 * Strides measured, not assumed: 0x70 for dyn_inertia and 0x40 for
			 * dyn_motion are the only candidates that read plausibly on the
			 * brahmin ragdoll, and the banner agrees independently
			 * (896 / 14 = 0x40, 1568 / 14 = 0x70).
			 */
			const quint32 motionIndex = r.u32( c + 0x0c );
			phys.mass = sys.mass;
			phys.density = sys.density;
			phys.inertia = sys.inertia;
			phys.gravityFactor = sys.gravityFactor;
			phys.maxLinVelocity = sys.maxLinVelocity;
			phys.maxAngVelocity = sys.maxAngVelocity;
			phys.linDamping = sys.linDamping;
			phys.angDamping = sys.angDamping;
			if ( dm >= 0 && motionIndex < r.u32( obj.first + 0x28 ) ) {
				qsizetype m = dm + qsizetype( motionIndex ) * 0x40;
				phys.gravityFactor = r.f32( m + 0x08 );
				phys.maxLinVelocity = r.f32( m + 0x10 );
				phys.maxAngVelocity = r.f32( m + 0x14 );
				phys.linDamping = r.f32( m + 0x18 );
				phys.angDamping = r.f32( m + 0x1c );
			}
			if ( di >= 0 && motionIndex < r.u32( obj.first + 0x38 ) ) {
				qsizetype n = di + qsizetype( motionIndex ) * 0x70;
				const float invMass = r.f32( n + 0x04 );
				phys.mass = ( invMass > 1.0e-12f ) ? 1.0f / invMass : 0.0f;
				phys.density = r.f32( n + 0x08 );
				phys.inertia = r.vec3( n + 0x20 );
			}
			if ( bprops >= 0 ) {
				qsizetype bp = bprops + qsizetype( i ) * 0x50;
				phys.friction = truncF16( bp + 0x12 );
				phys.restitution = truncF16( bp + 0x16 );
			}
			sys.bodyPhys.append( phys );
			qsizetype shp = global.value( c, -1 );
			if ( shp < 0 )
				continue;
			Vector3 pos = r.vec3( c + 0x30 );
			float qx = r.f32( c + 0x40 ), qy = r.f32( c + 0x44 );
			float qz = r.f32( c + 0x48 ), qw = r.f32( c + 0x4c );
			BodyT bt;
			// hkQuaternion (x,y,z,w) rotates column vectors; for our row-vector
			// convention (v' = v . rows) the rows are R's columns. Validated
			// numerically against vanilla stair helpers (bosbarricadewbstairsl01:
			// this fits the mesh bounds, the transposed variant lands far outside)
			bt.rows[0] = Vector3( 1.0f - 2.0f*(qy*qy + qz*qz), 2.0f*(qx*qy + qz*qw), 2.0f*(qx*qz - qy*qw) );
			bt.rows[1] = Vector3( 2.0f*(qx*qy - qz*qw), 1.0f - 2.0f*(qx*qx + qz*qz), 2.0f*(qy*qz + qx*qw) );
			bt.rows[2] = Vector3( 2.0f*(qx*qz + qy*qw), 2.0f*(qy*qz - qx*qw), 1.0f - 2.0f*(qx*qx + qy*qy) );
			bt.trans = pos;
			bt.ident = ( pos.length() < 1.0e-6f
						&& std::fabs( qx ) + std::fabs( qy ) + std::fabs( qz ) < 1.0e-6f );
			bt.id = int( i );
			bodies.append( { shp, bt } );
		}
	}
	// NOTE: the cinfo rotation/position is intentionally NOT composed into the
	// shapes - each body is placed by its referencing NODE's world transform
	// (bhkNPCollisionObject "Body ID" binding) at draw / decode time.
	auto bodyFor = [&]( qsizetype shapeOff ) -> const BodyT * {
		for ( const auto & bp : bodies ) {
			if ( bp.first == shapeOff )
				return &bp.second;
		}
		return nullptr;
	};
	auto applyResolvedBodyMaterial = [&]( HknpShape & shape, int bodyId ) {
		if ( shape.isConvex && bodyId >= 0 && bodyId < sys.bodyPhys.size()
			&& sys.bodyPhys.at( bodyId ).materialCRC )
			shape.materialCRC = sys.bodyPhys.at( bodyId ).materialCRC;
	};

	QSet<qsizetype> consumed;

	// decode one leaf shape object into out; returns false if not a shape
	auto decodeLeaf = [&]( qsizetype off, const QString & cls, HknpShape & out ) -> bool {
		out.className = cls;
		if ( cls == QLatin1String( "hknpConvexPolytopeShape" )
			|| cls == QLatin1String( "hknpConvexShape" )
			|| cls == QLatin1String( "hknpSphereShape" )
			|| cls == QLatin1String( "hknpCapsuleShape" ) ) {
			decodeConvexLike( r, off, cls, out );
			return !out.verts.isEmpty();
		}
		if ( cls == QLatin1String( "hknpCompressedMeshShape" ) ) {
			// the geometry lives in the CMSD referenced by the global fixup at
			// CMS+0x60 (object order in the file is not reliable)
			qsizetype cmsd = global.value( off + 0x60, -1 );
			if ( cmsd < 0 || objClass.value( cmsd ) != QLatin1String( "hknpCompressedMeshShapeData" ) )
				cmsd = findCMSD( off );
			if ( cmsd < 0 )
				return false;
			consumed.insert( cmsd );
			out.materialCRC = r.u32( off + 0x18 );
			decodeCompressedMesh( r, cmsd, local, out );
			return !out.tris.isEmpty();
		}
		if ( cls == QLatin1String( "hknpCompressedMeshShapeData" ) ) {
			decodeCompressedMesh( r, off, local, out );
			return !out.tris.isEmpty();
		}
		return false;
	};

	// pass 1: compound shapes — decode each instance with its transform
	for ( const auto & obj : objects ) {
		if ( obj.second != QLatin1String( "hknpDynamicCompoundShape" )
			&& obj.second != QLatin1String( "hknpStaticCompoundShape" ) )
			continue;
		consumed.insert( obj.first );
		qsizetype instBase = local.value( obj.first + 0x60, -1 );
		quint32 numInst = r.u32( obj.first + 0x68 );
		if ( instBase < 0 || numInst > 4096 )
			continue;
		// the compound's own bounding data object is not geometry
		for ( const auto & o : objects ) {
			if ( o.second == QLatin1String( "hknpDynamicCompoundShapeData" )
				|| o.second == QLatin1String( "hknpStaticCompoundShapeData" ) )
				consumed.insert( o.first );
		}
		const BodyT * compoundBody = bodyFor( obj.first );
		for ( quint32 i = 0; i < numInst && r.ok; i++ ) {
			qsizetype inst = instBase + qsizetype( i ) * 0x80;
			qsizetype child = global.value( inst + 0x50, -1 );
			if ( child < 0 )
				continue;
			consumed.insert( child );
			HknpShape shape;
			if ( !decodeLeaf( child, objClass.value( child ), shape ) )
				continue;
			shape.hasTransform = true;
			for ( int rr = 0; rr < 3; rr++ )
				shape.rotRows[rr] = r.vec3( inst + qsizetype( rr ) * 16 );
			shape.trans = r.vec3( inst + 0x30 );
			shape.scaleVec = r.vec3( inst + 0x40 );
			// the body's placement comes from its referencing NODE at draw
			// time (Body ID binding), not from the cinfo transform
			if ( compoundBody )
				shape.bodyId = compoundBody->id;
			applyResolvedBodyMaterial( shape, shape.bodyId );
			sys.shapes.append( shape );
		}
	}

	// pass 2: shapes referenced by bodies, placed by the body transform
	// (one output shape per body, so shared shapes instance correctly)
	for ( const auto & bp : bodies ) {
		if ( consumed.contains( bp.first ) )
			continue;	// compounds were handled in pass 1
		HknpShape shape;
		if ( decodeLeaf( bp.first, objClass.value( bp.first ), shape ) ) {
			shape.bodyId = bp.second.id;
			applyResolvedBodyMaterial( shape, shape.bodyId );
			sys.shapes.append( shape );
		}
	}
	for ( const auto & bp : bodies )
		consumed.insert( bp.first );

	// pass 3: any remaining shapes not referenced by a body or compound
	for ( const auto & obj : objects ) {
		if ( consumed.contains( obj.first ) )
			continue;
		HknpShape shape;
		if ( decodeLeaf( obj.first, obj.second, shape ) ) {
			consumed.insert( obj.first );
			sys.shapes.append( shape );
		} else if ( obj.second.startsWith( QLatin1String( "hknp" ) )
					&& obj.second.contains( QLatin1String( "Shape" ) )
					&& !obj.second.contains( QLatin1String( "MassProperties" ) )
					&& obj.second != QLatin1String( "hknpCompressedMeshShape" )
					&& obj.second != QLatin1String( "hknpCompressedMeshShapeData" ) ) {
			sys.unknownShapes.append( obj.second );
		}
	}

	/* Positional body attribution.
	 *
	 * A ragdoll packfile has no hknpPhysicsSystemData, so the cinfo scan above
	 * found no bodies and every shape is still unattributed - which is what
	 * collapsed a skeleton's per-bone collision into one anonymous list shape and
	 * made the viewport stack every capsule on one bone. The shapes come out in
	 * body order, so shape index IS the body id.
	 *
	 * Measured on the vanilla brahmin skeleton (39 bones, 39 capsules): pairing
	 * the 15 mirrored bones (LLeg1/RLeg1, LArm2/RArm2, ...) through this mapping
	 * matches their capsule radius and length to 0.5%, while the best wrong
	 * offset differs by 27% - 52x worse. Six of the pairs are bit-identical.
	 *
	 * Only fires when the packfile gave us nothing, so it cannot override real
	 * decoded ids; positionalBodies tells callers the ids are an inference.
	 *
	 * Requires unknownShapes to be EMPTY. A shape the decoder skipped is a hole in
	 * the sequence and every index after it is off by one, which would bind
	 * capsules to the wrong bones - wrong that looks right. Three vanilla
	 * skeletons are in that state (Deathclaw drops an hknpSphereShape; Robot and
	 * skeletonSentryBodyPart drop several), and they stay unattributed, exactly as
	 * before, until those classes decode.
	 */
	if ( sys.bodyPhys.isEmpty() && !sys.shapes.isEmpty() && sys.unknownShapes.isEmpty() ) {
		bool anyAttributed = false;
		for ( const HknpShape & shp : sys.shapes )
			anyAttributed = anyAttributed || shp.bodyId >= 0;
		if ( !anyAttributed ) {
			for ( qsizetype i = 0; i < sys.shapes.size(); i++ )
				sys.shapes[i].bodyId = int( i );
			sys.positionalBodies = true;
		}
	}

	sys.valid = !sys.shapes.isEmpty();
	if ( !sys.valid && sys.error.isEmpty() )
		sys.error = QStringLiteral( "no decodable shapes" );
	return sys;
}

const HknpSystem & hknpDecodeCached( const QByteArray & data )
{
	static QHash<size_t, HknpSystem> cache;
	size_t h = qHash( data );
	auto it = cache.constFind( h );
	if ( it != cache.constEnd() )
		return *it;
	if ( cache.size() > 64 )
		cache.clear();
	return *cache.insert( h, hknpDecode( data ) );
}

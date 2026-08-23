#include "hknpdecode.h"

#include <QHash>

#include <cmath>
#include <cstring>
#include <functional>

#include <QScopeGuard>

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

/*! Bounds-checked reads over the packfile blob.
 *
 * `ok` is STICKY on purpose -- a caller wants to know a read failed even if it
 * only checks later -- and that is exactly what made it dangerous: every
 * structural loop in this file was written `while ( i < count && r.ok )`, so one
 * out-of-range read anywhere silently truncated every list decoded after it, with
 * no error reported. A 12-instance compound came back with 11 children, missing
 * from the shape list, the viewport and the body attribution alike.
 *
 * So a per-item loop body opens a `Reader::Scope`, which restores `ok` on the way
 * out. A bad read then costs that one item instead of everything after it, and
 * `everFailed` still records that the file had a bad read at all.
 */
struct Reader
{
	const quint8 * d;
	qsizetype n;
	bool ok = true;
	bool everFailed = false;   //!< sticky across scopes: reported, never gates a loop
	qsizetype firstFail = -1;  //!< offset of the first out-of-range read, for the report

	void fail( qsizetype o )
	{
		ok = false;
		if ( !everFailed )
			firstFail = o;
		everFailed = true;
	}

	/*! Confines a read failure to one item. Declare it at the top of a loop body.
	 *
	 * Clears `ok` on entry as well as restoring it on exit, so code inside can test
	 * `ok` to mean "THIS item read cleanly" rather than "nothing has failed since
	 * the file was opened" -- which is what the atom-chain walk needs, since a bad
	 * read there really does invalidate the rest of that one chain.
	 */
	struct Scope
	{
		Reader & r;
		bool was;
		explicit Scope( Reader & reader ) : r( reader ), was( reader.ok ) { reader.ok = true; }
		~Scope() { r.ok = was; }
		Scope( const Scope & ) = delete;
		Scope & operator=( const Scope & ) = delete;
	};

	quint8 u8( qsizetype o )
	{
		if ( o < 0 || o >= n ) {
			fail( o );
			return 0;
		}
		return d[o];
	}
	quint16 u16( qsizetype o )
	{
		if ( o < 0 || o + 2 > n ) {
			fail( o );
			return 0;
		}
		quint16 v;
		std::memcpy( &v, d + o, 2 );
		return v;
	}
	quint32 u32( qsizetype o )
	{
		if ( o < 0 || o + 4 > n ) {
			fail( o );
			return 0;
		}
		quint32 v;
		std::memcpy( &v, d + o, 4 );
		return v;
	}
	quint64 u64( qsizetype o )
	{
		if ( o < 0 || o + 8 > n ) {
			fail( o );
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

/*! hkPackedVector3: three int16 mantissas plus a shared power-of-two exponent.
 *
 * The exponent lives in the fourth int16 as (E + 96) << 7, and its low seven bits
 * are zero in every vanilla object. Each component is then i16 / 32768 * 2^E.
 *
 * Established by decoding the workshop turret's centres of mass and checking them
 * against the same quantity computed from the hull geometry: 8 of 8 agreed to
 * within 1e-5 m. Reading the fourth slot as a half float -- the obvious guess --
 * gives the right DIRECTION and a scale wrong by 4x or 16x, which is exactly the
 * sort of near-miss that looks like success.
 */
static Vector3 packedVector3( Reader & r, qsizetype off )
{
	const int E = int( r.u16( off + 6 ) >> 7 ) - 96;
	const float scale = std::ldexp( 1.0f, E ) / 32768.0f;
	auto comp = [&]( qsizetype o ) { return float( qint16( r.u16( o ) ) ) * scale; };
	return Vector3( comp( off ), comp( off + 2 ), comp( off + 4 ) );
}

//! Fan-triangulate a convex face loop into shape.tris
static void addFaceFan( HknpShape & shape, const QVector<int> & loop )
{
	for ( int k = 1; k + 1 < loop.size(); k++ ) {
		if ( loop[0] >= shape.verts.size() || loop[k] >= shape.verts.size()
			|| loop[k+1] >= shape.verts.size() )
			continue;
		/* Collinear or coincident corners produce a triangle with no area, which
		 * nothing can hit and nothing can see. A face loop that repeats a vertex
		 * is not a decode error -- a hull may genuinely have one -- so it is
		 * dropped here rather than reported.
		 */
		const Vector3 e1 = shape.verts.at( loop[k] ) - shape.verts.at( loop[0] );
		const Vector3 e2 = shape.verts.at( loop[k+1] ) - shape.verts.at( loop[0] );
		if ( Vector3::crossproduct( e1, e2 ).length() < 1.0e-12f )
			continue;
		shape.tris.append( Triangle( quint16( loop[0] ), quint16( loop[k] ), quint16( loop[k+1] ) ) );
	}
}

//! index of vertex `s` on a preview ring, wrapping round the seam
static inline int ringVertex( int ringStart, int s, int seg )
{
	return ringStart + ( ( s % seg ) + seg ) % seg;
}

/*! Low-res UV sphere for previewing sphere / capsule primitives.
 *
 * The poles are ONE vertex each, not a ring of twelve coincident ones.
 *
 * Built the obvious way -- a full ring at every phi including 0 and pi -- a UV
 * sphere puts twelve vertices in the same place at each pole, and every triangle
 * spanning two of them has zero area. That was 24 of 144 triangles per sphere,
 * drawn, ray-tested and counted against the collision budget while covering
 * nothing at all.
 *
 * They surfaced as picker "misses": a ray aimed at such a triangle's centroid
 * hits nothing, because there is nothing there. Moller-Trumbore was right and the
 * geometry was wrong.
 */
static void synthSphere( HknpShape & shape, const Vector3 & c, float r )
{
	const int SEG = 12, RINGS = 6;
	shape.verts.clear();
	shape.tris.clear();

	shape.verts.append( c + Vector3( 0, 0, r ) );
	const int first = int( shape.verts.size() );
	for ( int ring = 1; ring < RINGS; ring++ ) {
		const float phi = float( M_PI ) * float( ring ) / float( RINGS );
		for ( int s = 0; s < SEG; s++ ) {
			const float th = 2.0f * float( M_PI ) * float( s ) / float( SEG );
			shape.verts.append( c + Vector3( std::sin( phi ) * std::cos( th ),
											 std::sin( phi ) * std::sin( th ),
											 std::cos( phi ) ) * r );
		}
	}
	const int south = int( shape.verts.size() );
	shape.verts.append( c + Vector3( 0, 0, -r ) );

	for ( int s = 0; s < SEG; s++ )
		shape.tris.append( Triangle( 0, quint16( ringVertex( first, s, SEG ) ),
			quint16( ringVertex( first, s + 1, SEG ) ) ) );
	for ( int ring = 0; ring + 2 < RINGS; ring++ ) {
		const int lo = first + ring * SEG, hi = lo + SEG;
		for ( int s = 0; s < SEG; s++ ) {
			const int p0 = ringVertex( lo, s, SEG ), p1 = ringVertex( lo, s + 1, SEG );
			const int p2 = ringVertex( hi, s, SEG ), p3 = ringVertex( hi, s + 1, SEG );
			shape.tris.append( Triangle( quint16( p0 ), quint16( p1 ), quint16( p2 ) ) );
			shape.tris.append( Triangle( quint16( p1 ), quint16( p3 ), quint16( p2 ) ) );
		}
	}
	const int last = first + ( RINGS - 2 ) * SEG;
	for ( int s = 0; s < SEG; s++ )
		shape.tris.append( Triangle( quint16( south ), quint16( ringVertex( last, s + 1, SEG ) ),
			quint16( ringVertex( last, s, SEG ) ) ) );
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
	/* The end rings are radius ZERO and collapse to a vertex each, exactly as the
	 * sphere's poles do -- the loops below therefore stop one short of the cap, and
	 * the two tips are appended on their own. See synthSphere.
	 */
	QVector<float> ringOffs;
	QVector<float> ringRads;
	for ( int i = CAP - 1; i >= 0; i-- ) {	// bottom hemisphere (around a)
		float phi = float( M_PI ) * 0.5f * float( i ) / float( CAP );
		ringOffs.append( -std::sin( phi ) * r );
		ringRads.append( std::cos( phi ) * r );
	}
	for ( int i = 0; i < CAP; i++ ) {	// top hemisphere (around b)
		float phi = float( M_PI ) * 0.5f * float( i ) / float( CAP );
		ringOffs.append( len + std::sin( phi ) * r );
		ringRads.append( std::cos( phi ) * r );
	}

	shape.verts.append( a - axis * r );
	const int first = int( shape.verts.size() );
	for ( int ring = 0; ring < ringOffs.size(); ring++ ) {
		Vector3 c = a + axis * ringOffs[ring];
		for ( int s = 0; s < SEG; s++ ) {
			float th = 2.0f * float( M_PI ) * float( s ) / float( SEG );
			shape.verts.append( c + ( u * std::cos( th ) + v * std::sin( th ) ) * ringRads[ring] );
		}
	}
	const int top = int( shape.verts.size() );
	shape.verts.append( b + axis * r );

	for ( int s = 0; s < SEG; s++ )
		shape.tris.append( Triangle( 0, quint16( ringVertex( first, s + 1, SEG ) ),
			quint16( ringVertex( first, s, SEG ) ) ) );
	for ( int ring = 0; ring + 1 < ringOffs.size(); ring++ ) {
		const int lo = first + ring * SEG, hi = lo + SEG;
		for ( int s = 0; s < SEG; s++ ) {
			const int p0 = ringVertex( lo, s, SEG ), p1 = ringVertex( lo, s + 1, SEG );
			const int p2 = ringVertex( hi, s, SEG ), p3 = ringVertex( hi, s + 1, SEG );
			shape.tris.append( Triangle( quint16( p0 ), quint16( p1 ), quint16( p2 ) ) );
			shape.tris.append( Triangle( quint16( p1 ), quint16( p3 ), quint16( p2 ) ) );
		}
	}
	const int last = first + ( int( ringOffs.size() ) - 1 ) * SEG;
	for ( int s = 0; s < SEG; s++ )
		shape.tris.append( Triangle( quint16( top ), quint16( ringVertex( last, s, SEG ) ),
			quint16( ringVertex( last, s + 1, SEG ) ) ) );
}

/*! Triangulate an hknpConvexShape's vertex cloud, so it can be seen.
 *
 *  All 17 vanilla objects are planar quads: four distinct positions, best-fit
 *  plane thickness at most 4.4e-16, and the eight-vertex ones store each corner
 *  twice. The stored order is a polygon loop when nv is 4 (6 of 6) and is NOT
 *  when nv is 8 (11 of 11), so the corners are sorted by angle about the
 *  centroid in the plane rather than taken in file order.
 *
 *  Indices refer to shape.verts, which stays exactly as stored -- its LENGTH is
 *  part of the object's size, so it must not be compacted.
 *
 *  Deliberately writes only tris, never faces: a shape with faces is claimed by
 *  the encoder's polytope branch, which would write a completely different
 *  object. And a non-planar hknpConvexShape would need a real hull; none exists
 *  in Fallout 4, so this leaves tris empty rather than guessing.
 */
static void synthConvexPolygon( HknpShape & shape )
{
	QVector<int> corner;			// first index of each distinct position
	for ( int i = 0; i < shape.verts.size(); i++ ) {
		bool seen = false;
		for ( int k : std::as_const( corner ) )
			seen = seen || ( shape.verts.at( i ) - shape.verts.at( k ) ).length() < 1.0e-6f;
		if ( !seen )
			corner.append( i );
	}
	if ( corner.size() < 3 )
		return;

	const Vector3 & p0 = shape.verts.at( corner.at( 0 ) );
	Vector3 n;
	for ( int b = 1; b < corner.size() && n.length() < 1.0e-9f; b++ ) {
		for ( int c = b + 1; c < corner.size(); c++ ) {
			const Vector3 cand = Vector3::crossproduct(
				shape.verts.at( corner.at( b ) ) - p0, shape.verts.at( corner.at( c ) ) - p0 );
			if ( cand.length() > 1.0e-9f ) { n = cand; break; }
		}
	}
	if ( n.length() < 1.0e-9f )
		return;						// collinear: no area, nothing to draw
	n.normalize();

	Vector3 centre;
	for ( int k : std::as_const( corner ) )
		centre += shape.verts.at( k );
	centre /= float( corner.size() );
	// every corner must lie in that plane, or this is a hull and not a polygon
	for ( int k : std::as_const( corner ) )
		if ( std::fabs( Vector3::dotproduct( n, shape.verts.at( k ) - centre ) ) > 1.0e-4f )
			return;

	Vector3 u = shape.verts.at( corner.at( 0 ) ) - centre;
	if ( u.length() < 1.0e-9f )
		return;
	u.normalize();
	const Vector3 v = Vector3::crossproduct( n, u );
	std::sort( corner.begin(), corner.end(), [&]( int a, int b ) {
		const Vector3 da = shape.verts.at( a ) - centre, db = shape.verts.at( b ) - centre;
		return std::atan2( Vector3::dotproduct( da, v ), Vector3::dotproduct( da, u ) )
			 < std::atan2( Vector3::dotproduct( db, v ), Vector3::dotproduct( db, u ) );
	} );
	addFaceFan( shape, corner );
}

//! hknpConvexPolytopeShape and subclasses (sphere / capsule / the convex base)
static void decodeConvexLike( Reader & r, qsizetype B, const QString & className, HknpShape & shape )
{
	shape.isConvex = true;
	shape.shapeFlags = r.u32( B + 0x10 );
	shape.convexRadius = r.f32( B + 0x14 );
	shape.materialCRC = r.u32( B + 0x18 );
	shape.shapeMaterialCRC = shape.materialCRC;	// kept before body resolution

	auto relArray = [&]( qsizetype fieldOff, quint16 & count, qsizetype & payload ) {
		count = r.u16( fieldOff );
		payload = fieldOff + r.u16( fieldOff + 2 );
	};

	/* Read the VERTEX array first, and nothing else yet.
	 *
	 * The plane / face / index relArrays at +0x40, +0x44 and +0x48 only exist on a
	 * polytope. On a sphere the vertex payload starts at +0x30 + 0x10, which IS
	 * +0x40, so those three fields are vertex bytes reinterpreted as counts -- on
	 * the Deathclaw ragdoll +0x44 reads 0x7f20 = 32544 faces. Reading them up
	 * front meant the shared sanity check tripped and the function returned before
	 * the sphere branch below could ever run, so every hknpSphereShape decoded to
	 * nothing.
	 */
	quint16 nv;
	qsizetype pv;
	relArray( B + 0x30, nv, pv );
	if ( nv > 4096 )
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
		/* Exact end points at +0x50 / +0x60, and the core box in the vertex array.
		 *
		 * The core is an OBB about the segment padded equally on all three local
		 * axes, so every corner sits at padding * sqrt(2) off the axis while the
		 * padding itself -- which is what actually fattens the capsule -- is that
		 * over sqrt(2). Taking the corner distance directly, as this did until
		 * 07-28t, over-reads the radius by 0.42%: it measures the diagonal.
		 *
		 * Deriving the padding from the geometry rather than from the measured
		 * radius/99 keeps this honest if a file ever disagrees with the corpus.
		 */
		Vector3 a = r.vec3( B + 0x50 );
		Vector3 b = r.vec3( B + 0x60 );
		Vector3 ab = b - a;
		float abLen2 = Vector3::dotproduct( ab, ab );
		/* Distance to the axis LINE, deliberately unclamped.
		 *
		 * Off the line every corner sits at padding * sqrt(2), one padding in each
		 * perpendicular direction. Clamping to the segment instead -- which is what
		 * a closest-point-on-segment helper does by default -- adds the axial
		 * overshoot, because the box is longer than the segment by one padding at
		 * each end, and every corner then reads padding * sqrt(3). That is a
		 * uniform 22% overestimate of the padding, so it is invisible in a ratio
		 * check and only shows up against the stored geometry.
		 */
		float perp = 0.0f;
		for ( const Vector3 & p : raw ) {
			const Vector3 rel = p - a;
			const float t = ( abLen2 > 1.0e-12f ) ? Vector3::dotproduct( rel, ab ) / abLen2 : 0.0f;
			perp = std::max( perp, ( rel - ab * t ).length() );
		}
		const float padding = perp * 0.70710678f;
		shape.primType = 2;
		shape.capA = a;
		shape.capB = b;
		/* The NIF-facing radius is ELRIC'S, measured off the oracle 2026-08-21.
		 *
		 * Fed a bhkCapsuleShape of radius R, Elric stores core = R * 0.99 * 0.99
		 * at +0x14 and pads the core box by core/99 -- so the solid's face
		 * half-width is exactly 0.99 * R. Three inputs, three exact hits:
		 *
		 *   R 1.0000 -> core 0.980100, pad/core 0.010101
		 *   R 0.5000 -> core 0.490050, pad/core 0.010101
		 *   R 0.155990 -> core 0.152886, ratio 1.020304 = (1 + 1/99)^2
		 *
		 * So the inverse is core / 0.9801, and that is what goes back into the NIF.
		 * It also happens to be the conservative reading: the solid runs from
		 * 0.99 R at a face to 0.99414 R at a corner, so R over-states it by 0.6%
		 * and never under-states it.
		 *
		 * This read core + padding * sqrt(2) until now -- the circumscribed
		 * half-width, argued from the geometry alone. It is 0.59% short of Elric's
		 * number, which does not sound like much until it round-trips: decompile
		 * and recompile ratcheted every capsule 1.43% FATTER each cycle, and the
		 * parking meter shipped with a compound AABB 0.6 mm past vanilla's. The
		 * geometry cannot answer this question. Only the tool that wrote the files
		 * can, and it did.
		 */
		shape.primRadius = shape.convexRadius / 0.9801f;
		shape.coreVerts = raw;
		shape.corePadding = padding;
		// the solid also runs one padding past each stored end point
		Vector3 axis = ( abLen2 > 1.0e-12f ) ? ab / std::sqrt( abLen2 ) : Vector3( 0.0f, 0.0f, 1.0f );
		synthCapsule( shape, a - axis * padding, b + axis * padding, shape.primRadius );
		return;
	} else if ( className == QLatin1String( "hknpConvexShape" ) ) {
		/* The base of the convex family: a vertex cloud, and nothing else.
		 *
		 * Like a sphere, its vertex payload starts at +0x30 + 0x10, which IS +0x40
		 * -- so the plane / face / index relArrays below read the FIRST VERTEX's
		 * floats. Across the 17 vanilla objects that gives nf = 5662..52483, the
		 * shared sanity check returns, and because that return happens BEFORE
		 * `shape.verts = raw` every hknpConvexShape decoded to 0 verts and 0 tris:
		 * invisible in the viewport, invisible to any geometry check, and
		 * unwritable -- encodeShapeObject has no branch for a convex shape with an
		 * empty face list, so all 17 systems refused to assemble.
		 *
		 * Matched by CLASS NAME, not by the descriptor. The tempting data-driven
		 * test -- "payload offset 0x10 means there are no polytope arrays" -- is
		 * byte-identical on hknpSphereShape (04 00 10 00), and a sphere whose four
		 * vertices are not all equal falls out of its own branch above and is
		 * meant to reach the polytope tail. That would change 114 objects to fix 17.
		 *
		 * Measured on all 17: the object is align16( 0x40 + nv * 16 ) bytes, nv is
		 * 4 or 8, and each vertex w tag is 0x3f000000 | its own slot index with no
		 * repeats. That last part is NOT the polytope rule -- a polytope's padding
		 * slots duplicate the last real vertex and its tag -- so the array has no
		 * padding and nv is the real count.
		 *
		 * verts is stored VERBATIM, all nv of them. The eight-vertex objects hold
		 * each corner of a quad twice; de-duplicating here would make the encoder
		 * write a 128-byte object where the file has 192, shifting every offset
		 * after it in the packfile.
		 */
		shape.verts = raw;
		synthConvexPolygon( shape );
		return;
	}

	// Polytope-only arrays, read after the primitives have returned above.
	quint16 np, nf, ni;
	qsizetype pp, pf, pi;
	relArray( B + 0x40, np, pp );
	relArray( B + 0x44, nf, pf );
	relArray( B + 0x48, ni, pi );
	if ( nf > 4096 || ni > 16384 )
		return;

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
			shape.faceAngles.append( r.u8( pf + f * 4 + 3 ) );
			addFaceFan( shape, loop );
		}
	}
}

/*! Walk a constraint object's atom chain, filling in frames, limits and friction.
 *
 * The object is a flat run of atoms from +0x20 to its end. Each atom opens with a
 * u16 type and has no length, so walking needs the sizes below. They are not
 * guesses: with this table every one of the 757 constraint objects across all 35
 * vanilla ragdolls walks to its exact end hitting only known types, and each
 * class yields exactly ONE atom sequence with no variants. A size wrong by even
 * four bytes desyncs the walk and turns every later type into garbage, so
 * consuming 757 objects exactly is a strong check rather than a plausible one.
 *
 * Corroborating the field offsets separately from the sizes: all 3068 decoded
 * angles land in [-pi, pi]; the 518 "-100" sentinels are exactly one per ragdoll
 * constraint, matching the one bound a cone does not have; and the tau factor
 * takes just two values across 1793 atoms, 0.8 on ragdoll limits and 1.0 on
 * hinges. Bytes read at a wrong offset do not fall into patterns like that.
 */
static void decodeConstraintAtoms( Reader & r, qsizetype cd, qsizetype end, HknpConstraint & jc )
{
	enum AtomType
	{
		SET_LOCAL_TRANSFORMS = 0x02, BALL_SOCKET = 0x05, TWO_D_ANG = 0x0c,
		ANG_LIMIT = 0x0e, TWIST_LIMIT = 0x0f, CONE_LIMIT = 0x10,
		ANG_FRICTION = 0x11, ANG_MOTOR = 0x12, RAGDOLL_MOTOR = 0x13,
		SETUP_STABILIZATION = 0x17
	};

	auto atomSize = []( quint16 t ) -> qsizetype {
		switch ( t ) {
		case SET_LOCAL_TRANSFORMS: return 144;   // 16 header + two hkTransforms
		case RAGDOLL_MOTOR:        return 96;
		case ANG_MOTOR:            return 40;
		case TWIST_LIMIT:
		case CONE_LIMIT:           return 32;
		case BALL_SOCKET:
		case TWO_D_ANG:
		case ANG_LIMIT:
		case ANG_FRICTION:
		case SETUP_STABILIZATION:  return 16;
		default:                   return 0;     // unknown: stop, do not guess
		}
	};

	// twist/cone put min,max,tau at +0x08; angLimit is packed tighter, at +0x04
	auto limit = [&r]( qsizetype a, qsizetype off ) {
		HknpAngLimit l;
		l.present = true;
		l.min = r.f32( a + off );
		l.max = r.f32( a + off + 4 );
		l.tau = r.f32( a + off + 8 );
		return l;
	};

	int cones = 0;
	// Objects are 16-aligned, so a clean walk can stop short of the next object;
	// that tail is zero padding, not a further atom.
	// a bad read here invalidates the REST OF THIS CHAIN -- the next atom's offset
	// comes from this one's type -- but not any other constraint's, hence the scope
	Reader::Scope chainScope( r );
	for ( qsizetype a = cd + 0x20; a + 16 <= end && r.ok; ) {
		const quint16 t = r.u16( a );
		const qsizetype sz = atomSize( t );
		if ( t == 0 || sz == 0 )
			break;

		switch ( t ) {
		case SET_LOCAL_TRANSFORMS:
			for ( int k = 0; k < 3; k++ ) {
				jc.rotA[k] = r.vec3( a + 0x10 + qsizetype( k ) * 16 );
				jc.rotB[k] = r.vec3( a + 0x50 + qsizetype( k ) * 16 );
			}
			jc.pivotA = r.vec3( a + 0x10 + 48 );
			jc.pivotB = r.vec3( a + 0x50 + 48 );
			jc.hasFrames = true;
			break;
		case TWIST_LIMIT:
			jc.twist = limit( a, 0x08 );
			break;
		case CONE_LIMIT:
			// the ragdoll writes two: the cone first, then the perpendicular plane
			( cones++ == 0 ? jc.cone : jc.plane ) = limit( a, 0x08 );
			break;
		case ANG_LIMIT:
			jc.hinge = limit( a, 0x04 );
			break;
		case ANG_FRICTION:
			jc.friction = r.f32( a + 0x08 );   // maxFrictionTorque
			break;
		case ANG_MOTOR:
		case RAGDOLL_MOTOR:
			jc.motorEnabled = r.u8( a + 0x02 ) != 0;
			break;
		default:
			break;
		}
		a += sz;
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
	// the two shared-vertex arrays' own counts, which is what bounds the lookups
	quint32 nShix = r.u32( B + 0x78 ), nShar = r.u32( B + 0x98 );
	if ( secp < 0 || prim < 0 || pack < 0 || nsec > 4096 )
		return;

	/* The material table and the run table, decoded 2026-08-20.
	 *
	 * A mesh does not carry one material: the CMSD holds an array of 4-byte runs
	 * at +0xa0 -- [u8 materialIndex][u8 0][u8 firstPrimitiveInSection][u8 count]
	 * -- and each section names its slice of it at +0x54, packed
	 * (firstRun << 8) | runCount the same way +0x50 packs its primitives. The
	 * index is into hknpBSMaterialProperties, whose entries are 0x18 apart.
	 *
	 * Measured on 2,490 vanilla meshes: every section's runs start at primitive
	 * 0 and sum to its primitive count, and byte 1 is zero on all 9,536 records.
	 */
	{
		const quint8 * mat = reinterpret_cast<const quint8 *>( shape.materialRawData.constData() );
		const qsizetype matSize = shape.materialRawData.size();
		if ( matSize >= 0x38 ) {
			quint32 count = 0;
			std::memcpy( &count, mat + 0x18, 4 );
			for ( quint32 i = 0; i < count && qsizetype( 0x20 + i * 0x18 + 0x18 ) <= matSize; i++ ) {
				quint32 crc = 0;
				std::memcpy( &crc, mat + 0x20 + i * 0x18 + 0x14, 4 );
				shape.materialTable.append( crc );
			}
		}
	}
	const qsizetype runs = local.value( B + 0xa0, -1 );
	const quint32 numRuns = r.u32( B + 0xa8 );

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

	for ( quint32 s = 0; s < nsec; s++ ) {
		Reader::Scope sectionScope( r );
		qsizetype S = secp + qsizetype( s ) * 0x60;
		Vector3 off = r.vec3( S + 0x30 );
		Vector3 stp = r.vec3( S + 0x3c );
		quint32 firstPacked = r.u32( S + 0x48 );
	/* +0x4c's high 24 bits are the section's first shared index. Its LOW BYTE is
	 * not a shared count, whatever the field name suggests -- on all 7 sections of
	 * a SetDressing billboard it equals numPacked at +0x58 exactly, while the real
	 * per-section shared range is the gap to the next section's firstShared.
	 *
	 * Reading it as a count let section 6 ask for index 174+74 of a 178-entry
	 * array, walk off the end into the packed-vertex data, and take a vertex
	 * position as a shared index -- which is the read landing 15x past the end of
	 * the blob that this file reports. Bounding against each array's OWN count is
	 * exact and needs no interpretation of the low byte at all.
	 */
		quint32 firstShared = r.u32( S + 0x4c ) >> 8;
		quint32 pf = r.u32( S + 0x50 );
		quint32 firstPrim = pf >> 8, numPrim = pf & 0xFF;
		quint32 numPacked = r.u8( S + 0x58 );

		/* This section's slice of the run table, expanded to one material per
		 * primitive. A run's start is SECTION-relative, so the walk is local;
		 * anything the runs do not name keeps the shape's own material, which is
		 * what a single-material file leaves everywhere.
		 */
		QVector<quint32> primMaterial( qsizetype( numPrim ), shape.materialCRC );
		if ( runs >= 0 && !shape.materialTable.isEmpty() ) {
			const quint32 rf = r.u32( S + 0x54 );
			const quint32 firstRun = rf >> 8, runCount = rf & 0xFF;
			for ( quint32 k = 0; k < runCount && firstRun + k < numRuns; k++ ) {
				const qsizetype R = runs + qsizetype( firstRun + k ) * 4;
				const quint32 mi = r.u8( R ), start = r.u8( R + 2 ), n = r.u8( R + 3 );
				const quint32 crc = mi < quint32( shape.materialTable.size() )
					? shape.materialTable.at( qsizetype( mi ) ) : shape.materialCRC;
				for ( quint32 p = start; p < start + n && p < numPrim; p++ )
					primMaterial[qsizetype( p )] = crc;
			}
		}

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
			const qsizetype k = qsizetype( firstShared ) + idx - qsizetype( numPacked );
			if ( k < 0 || k >= qsizetype( nShix ) )
				return -1;
			quint16 si = r.u16( shix + k * 2 );
			if ( si >= nShar )
				return -1;
			quint64 w = r.u64( shar + qsizetype( si ) * 8 );
			Vector3 p( gmin[0] + float( w & 0x1FFFFF ) / 2097151.0f * ( gmax[0] - gmin[0] ),
						gmin[1] + float( ( w >> 21 ) & 0x1FFFFF ) / 2097151.0f * ( gmax[1] - gmin[1] ),
						gmin[2] + float( ( w >> 42 ) & 0x3FFFFF ) / 4194303.0f * ( gmax[2] - gmin[2] ) );
			return addVert( 0x8000000000000000ULL | si, p );
		};

		for ( quint32 t = 0; t < numPrim; t++ ) {
			Reader::Scope primScope( r );
			qsizetype P = prim + qsizetype( firstPrim + t ) * 4;
			int a = vert( r.u8( P ) ), b = vert( r.u8( P + 1 ) );
			int c = vert( r.u8( P + 2 ) );
			quint8 dRaw = r.u8( P + 3 );
			if ( a < 0 || b < 0 || c < 0 || shape.verts.size() > 65535 )
				continue;
			shape.tris.append( Triangle( quint16( a ), quint16( b ), quint16( c ) ) );
			shape.triMaterial.append( primMaterial.at( qsizetype( t ) ) );
			if ( dRaw != r.u8( P + 2 ) ) {	// quad: second triangle
				int dd = vert( dRaw );
				if ( dd >= 0 ) {
					shape.tris.append( Triangle( quint16( a ), quint16( c ), quint16( dd ) ) );
					// both halves of a quad are ONE primitive, so one material
					shape.triMaterial.append( primMaterial.at( qsizetype( t ) ) );
				}
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
			// the SPAN, not the count: a 12-byte pad is indistinguishable from one
			// more -1 entry, so only the region length is measurable
			sys.globalFixupBytes = quint32( virtOff - globalOff );
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
			const QString cname = QString::fromLatin1(
				reinterpret_cast<const char *>( r.d + p + 5 ), int( e - p - 5 ) );
			classNames.insert( p + 5 - cnStart, cname );
			// the file's own hashes, so a rewrite never has to know one it was not
			// told -- see HknpSystem::classHashes
			sys.classHashes.insert( cname, r.u32( p ) );
			p = e + 1;
		}
	}

	// local fixups: intra-section pointer patches (member offset -> payload)
	QHash<qsizetype, qsizetype> local;
	/* Table order, not just the mapping. LOCAL fixups follow the same reflection
	 * walk the globals do -- member declaration order, with an array member's
	 * payload fixups emitted right after the member itself -- so a compressed
	 * mesh's section-array fixup sits between its +0x50 and +0x60 members rather
	 * than after them. Sorting by offset happens to match on every ragdoll and does
	 * not match here, so an object carried whole has to keep the order it had.
	 */
	QVector<QPair<qsizetype, qsizetype>> localOrder;
	for ( qsizetype p = dataStart + localOff; p + 8 <= dataStart + globalOff && p + 8 <= data.size(); p += 8 ) {
		qint32 src = qint32( r.u32( p ) ), dst = qint32( r.u32( p + 4 ) );
		if ( src != -1 )
			local.insert( dataStart + src, dataStart + dst );
			localOrder.append( { dataStart + src, dataStart + dst } );
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

	/* hkaSkeleton: the ragdoll's own copy of the bone hierarchy, one per packfile
	 * and a root object in its own right rather than a member of the ragdoll data.
	 * hkArrays at +0x18 parentIndices (hkInt16), +0x28 bones (16 bytes: a name
	 * pointer that is always null here, then lockTranslation) and +0x38
	 * referencePose (hkQsTransform, 48 bytes: translation, rotation, scale).
	 */
	for ( const auto & obj : objects ) {
		if ( obj.second != QLatin1String( "hkaSkeleton" ) )
			continue;
		const qsizetype par = local.value( obj.first + 0x18, -1 );
		const qsizetype bon = local.value( obj.first + 0x28, -1 );
		const qsizetype rp  = local.value( obj.first + 0x38, -1 );
		const quint32 n = r.u32( obj.first + 0x20 );
		if ( par < 0 || n == 0 || n > 4096 )
			continue;
		sys.skeletonRawOffset = obj.first;
		for ( quint32 i = 0; i < n; i++ ) {
			Reader::Scope boneScope( r );
			HknpBone b;
			b.parent = qint16( r.u16( par + qsizetype( i ) * 2 ) );
			if ( bon >= 0 )
				b.lockTranslation = r.u8( bon + qsizetype( i ) * 16 + 8 ) != 0;
			if ( rp >= 0 ) {
				const qsizetype e = rp + qsizetype( i ) * 48;
				b.translation = r.vec3( e );
				// Havok stores a quaternion xyzw; NifSkope's Quat is wxyz
				b.rotation = Quat( r.f32( e + 28 ), r.f32( e + 16 ),
					r.f32( e + 20 ), r.f32( e + 24 ) );
				b.scale = r.vec3( e + 32 );
				b.poseTransW = r.u32( e + 12 );
				b.poseScaleW = r.u32( e + 44 );
			}
			sys.bones.append( b );
		}
		break;
	}

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
		sys.motionCount = int( r.u32( obj.first + 0x28 ) );
		sys.inertiaCount = int( r.u32( obj.first + 0x38 ) );
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
			sys.invInertia = r.vec3( di + 0x20 );
		}
		/* Ragdoll joints: the array at +0x50, one entry per non-root body.
		 * Only hknpRagdollData carries it; a physics system leaves it empty.
		 */
		if ( qsizetype cons = local.value( obj.first + 0x50, -1 ); cons >= 0 ) {
			const quint32 nc = r.u32( obj.first + 0x58 );
			for ( quint32 i = 0; i < nc && nc <= 4096; i++ ) {
				Reader::Scope constraintScope( r );
				const qsizetype e = cons + qsizetype( i ) * 0x18;
				HknpConstraint jc;
				jc.childBody = int( r.u32( e + 0x08 ) );
				jc.parentBody = int( r.u32( e + 0x0c ) );
				qsizetype cd = global.value( e, -1 );
				jc.kind = objClass.value( cd );

				// objects sit back to back, so the next one bounds this one; the
				// last is bounded by the local fixup table that follows the data
				auto objEnd = [&]( qsizetype o ) {
					auto nx = std::upper_bound( objects.cbegin(), objects.cend(), o,
						[]( qsizetype v, const QPair<qsizetype, QString> & p ) { return v < p.first; } );
					return ( nx != objects.cend() ) ? nx->first : dataStart + localOff;
				};

				/* hknpBreakableConstraintData is a wrapper, not a constraint: it holds
				 * a pointer to the real hkp* data. Both in the corpus point at a
				 * limited hinge. Rather than hard-code the member offset from two
				 * samples, take the first pointer in the object that lands on an hkp*
				 * constraint - the wrapper has no other constraint-typed member.
				 */
				if ( cd >= 0 && jc.kind.startsWith( QLatin1String( "hknp" ) ) ) {
					for ( qsizetype o = cd; o + 8 <= objEnd( cd ); o += 8 ) {
						const qsizetype in = global.value( o, -1 );
						const QString ik = objClass.value( in );
						if ( in >= 0 && ik.startsWith( QLatin1String( "hkp" ) )
							&& ik.endsWith( QLatin1String( "ConstraintData" ) ) ) {
							jc.breakThreshold = r.f32( cd + 0x20 );
							cd = in;
							jc.kind = ik;
							jc.breakable = true;
							break;
						}
					}
				}

				// Only the hkp* constraint datas carry the atom chain; anything else
				// would decode into a plausible-looking wrong frame.
				if ( cd >= 0 && jc.kind.startsWith( QLatin1String( "hkp" ) ) ) {
					decodeConstraintAtoms( r, cd, objEnd( cd ), jc );
					jc.rawOffset = cd;
					const qsizetype len = objEnd( cd ) - cd;
					if ( len > 0 && cd + len <= data.size() )
						jc.rawData = data.mid( cd, len );
					// motor pointers, so a rewrite can re-bind them (see motorPointers)
					for ( qsizetype o = cd; o + 8 <= cd + len; o += 8 ) {
						if ( objClass.value( global.value( o, -1 ) )
							== QLatin1String( "hkpPositionConstraintMotor" ) )
							jc.motorPointers.append( o - cd );
					}
				}
				sys.constraints.append( jc );
			}
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
			phys.packedFilter = packedFilter;
			phys.hasStoredFilter = true;
			phys.layer = packedFilter & 0xffU;
			phys.filterFlags = quint8( ( packedFilter >> 8 ) & 0xffU );
			phys.filterGroup = quint16( packedFilter >> 16 );
			/* cinfo +0x12 IS NOT A MATERIAL INDEX. It is the body's slot in the
			 * root's +0x60 shape list — measured equal to it on 178 of 178
			 * multi-body systems, and the encoder has been writing it as that for
			 * 833 byte-exact packfiles.
			 *
			 * Read as an index into bodyMaterials it was worse than useless,
			 * because bodyMaterials is a CONCATENATION of every shape's own
			 * hknpBSMaterialProperties table: a slot number from one shape indexes
			 * into another shape's materials. Over the sampled corpus that put an
			 * index past the end 18 times (harmless, the override was skipped),
			 * landed on an equal value 7 times, and OVERWROTE a convex shape's own
			 * material with one belonging to a different shape 5 times.
			 *
			 * So nothing resolves a body material here any more, and
			 * applyResolvedBodyMaterial below leaves each convex shape the CRC in
			 * its own header. If a per-body material override does exist in this
			 * format, it is not this field and it has not been found.
			 */
			phys.hasMotion = ( r.u32( c + 0x0c ) != 0x7fffffffu );
			phys.cinfoFlags = r.u32( c + 0x18 );
			if ( phys.layer == 0 )
				phys.layer = ( sys.dynamic && phys.hasMotion ) ? 10u : 1u;
			phys.position = r.vec3( c + 0x30 );
			phys.positionW = r.u32( c + 0x3c );
			// Havok stores xyzw; NifSkope's Quat is wxyz
			phys.orientation = Quat( r.f32( c + 0x4c ), r.f32( c + 0x40 ),
				r.f32( c + 0x44 ), r.f32( c + 0x48 ) );
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
			if ( motionIndex != 0x7fffffffu && motionIndex < 0x40000000u )
				phys.motionIndex = int( motionIndex );
			phys.mass = sys.mass;
			phys.density = sys.density;
			phys.invInertia = sys.invInertia;
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
				phys.invMassStored = invMass;		// see HknpBodyPhys::invMassStored
				phys.density = r.f32( n + 0x08 );
				phys.invInertia = r.vec3( n + 0x20 );
				phys.inertiaTag = r.u32( n + 0x00 );
				phys.inertiaScale = r.u32( n + 0x2c );
				phys.motionCom = r.vec3( n + 0x30 );
				phys.motionComW = r.u32( n + 0x3c );
				phys.motionOrientation = Quat( r.f32( n + 0x4c ), r.f32( n + 0x40 ),
					r.f32( n + 0x44 ), r.f32( n + 0x48 ) );
			}
			if ( bprops >= 0 ) {
				qsizetype bp = bprops + qsizetype( i ) * 0x50;
				phys.friction = truncF16( bp + 0x12 );
				phys.restitution = truncF16( bp + 0x16 );
				phys.materialFlags = r.u32( bp + 0x0c );
				phys.triggerType = r.u8( bp + 0x10 );
				if ( bp + 0x50 <= data.size() )
					phys.propsRawData = data.mid( bp, 0x50 );
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

		/* The shape list at +0x60: the same shapes the cinfos name, permuted into
		 * the NIF's block order. Recorded as body indices so a writer can restore
		 * the order without carrying packfile offsets. See HknpSystem::shapeListOrder.
		 */
		{
			sys.rootRawOffset = obj.first;
			sys.rootClassName = obj.second;
			if ( qsizetype list = local.value( obj.first + 0x60, -1 ); list >= 0 ) {
				const quint32 n = r.u32( obj.first + 0x68 );
				for ( quint32 k = 0; k < n && n <= 4096; k++ ) {
					Reader::Scope slotScope( r );
					const qsizetype shp = global.value( list + qsizetype( k ) * 8, -1 );
					int body = -1;
					for ( quint32 i = 0; i < nb && body < 0; i++ ) {
						if ( global.value( cinfos + qsizetype( i ) * 0x60, -1 ) == shp )
							body = int( i );
					}
					sys.shapeListOrder.append( body );
				}
			}
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

	/* An object's bytes. Objects sit back to back, so the next one bounds this one
	 * and the last is bounded by the local fixup table that follows the data. Used
	 * wherever an object is carried whole rather than decoded.
	 */
	auto objectBytes = [&]( qsizetype off ) -> QByteArray {
		auto nx = std::upper_bound( objects.cbegin(), objects.cend(), off,
			[]( qsizetype v, const QPair<qsizetype, QString> & p ) { return v < p.first; } );
		const qsizetype end = ( nx != objects.cend() ) ? nx->first : dataStart + localOff;
		return ( end > off && end <= data.size() ) ? data.mid( off, end - off ) : QByteArray();
	};
	//! LOCAL fixups inside one object, relative to its start
	auto objectLocal = [&]( qsizetype off, qsizetype len ) {
		QVector<QPair<qsizetype, qsizetype>> out;
		for ( const auto & f : std::as_const( localOrder ) ) {
			if ( f.first >= off && f.first < off + len )
				out.append( { f.first - off, f.second - off } );
		}
		return out;   // table order, deliberately not sorted
	};

	/* Decode one leaf shape object into out; false if it is not a shape.
	 *
	 * std::function rather than auto because hknpScaledConvexShape is a wrapper and
	 * this has to call itself to decode what it wraps.
	 */
	std::function<bool( qsizetype, const QString &, HknpShape & )> decodeLeaf;
	decodeLeaf = [&]( qsizetype off, const QString & cls, HknpShape & out ) -> bool {
		out.className = cls;
		out.rawOffset = off;
		// the object as stored, so an untouched shape can be written back
		// unchanged rather than re-derived -- see HknpShape::rawData
		out.rawData = objectBytes( off );
		out.rawLocal = objectLocal( off, out.rawData.size() );
		if ( cls == QLatin1String( "hknpConvexPolytopeShape" )
			|| cls == QLatin1String( "hknpConvexShape" )
			|| cls == QLatin1String( "hknpSphereShape" )
			|| cls == QLatin1String( "hknpCapsuleShape" ) ) {
			decodeConvexLike( r, off, cls, out );
			/* Mass properties hang off hkRefCountedProperties at shape+0x20,
			 * which in turn points at the hknpShapeMassProperties at its +0x10 --
			 * the same two-hop the material properties take.
			 */
			if ( qsizetype ref = global.value( off + 0x20, -1 ); ref >= 0 ) {
				const qsizetype mp = global.value( ref + 0x10, -1 );
				if ( mp >= 0 && objClass.value( mp )
					== QLatin1String( "hknpShapeMassProperties" ) ) {
					out.hasMassProps = true;
					out.massCom = packedVector3( r, mp + 0x10 );
					out.massInertiaRaw = packedVector3( r, mp + 0x18 );
					out.massVolume = r.f32( mp + 0x28 );
					out.massMass = r.f32( mp + 0x2c );
					out.massMajorAxis = r.u64( mp + 0x20 );
					out.massPropsOffset = mp;
					if ( mp + 0x30 <= data.size() )
						out.massRawData = data.mid( mp, 0x30 );
				} else if ( mp >= 0 && objClass.value( mp )
					== QLatin1String( "hknpBSMaterialProperties" ) ) {
					out.materialRawData = objectBytes( mp );
					out.materialLocal = objectLocal( mp, out.materialRawData.size() );
				}
			}
			return !out.verts.isEmpty();
		}
		if ( cls == QLatin1String( "hknpScaledConvexShape" ) ) {
			// a wrapper: decode what it points at, then scale it. See scaledChild.
			const qsizetype inner = global.value( off + 0x30, -1 );
			if ( inner < 0 )
				return false;
			auto child = QSharedPointer<HknpShape>::create();
			if ( !decodeLeaf( inner, objClass.value( inner ), *child ) )
				return false;
			consumed.insert( inner );
			const Vector3 scale = r.vec3( off + 0x40 );
			const QByteArray wrapper = out.rawData;
			const QVector<QPair<qsizetype, qsizetype>> wrapperLocal = out.rawLocal;
			const qsizetype wrapperAt = out.rawOffset;
			out = *child;
			out.className = cls;
			out.rawData = wrapper;
			out.rawLocal = wrapperLocal;
			out.rawOffset = wrapperAt;
			out.scaledChild = child;
			for ( Vector3 & v : out.verts )
				v = Vector3( v[0] * scale[0], v[1] * scale[1], v[2] * scale[2] );
			out.shapeFlags = r.u32( off + 0x10 );
			out.convexRadius = r.f32( off + 0x14 );
			out.materialCRC = out.shapeMaterialCRC = r.u32( off + 0x18 );
			/* Everything the CHILD owns stays with the child object and is emitted
			 * with it; the wrapper owns none of it. Leaving these set would make the
			 * writer emit a second copy hanging off the wrapper.
			 */
			out.hasMassProps = false;
			out.massPropsOffset = -1;
			out.massRawData.clear();
			out.materialRawData.clear();
			out.dataRawData.clear();
			out.dataLocal.clear();
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
			out.shapeMaterialCRC = out.materialCRC;
			// carried whole: the quantization is not reconstructible, see dataRawData
			out.dataClassName = objClass.value( cmsd );
			out.dataRawData = objectBytes( cmsd );
			out.dataLocal = objectLocal( cmsd, out.dataRawData.size() );
			// a compressed mesh names its material table the same two-hop way a convex
			// shape names its mass properties
			if ( qsizetype ref = global.value( off + 0x20, -1 ); ref >= 0 ) {
				const qsizetype mp = global.value( ref + 0x10, -1 );
				if ( mp >= 0 && objClass.value( mp ) == QLatin1String( "hknpBSMaterialProperties" ) ) {
					out.materialRawData = objectBytes( mp );
					out.materialLocal = objectLocal( mp, out.materialRawData.size() );
				}
			}
			decodeCompressedMesh( r, cmsd, local, out );
			return !out.tris.isEmpty();
		}
		if ( cls == QLatin1String( "hknpCompressedMeshShapeData" ) ) {
			decodeCompressedMesh( r, off, local, out );
			return !out.tris.isEmpty();
		}
		return false;
	};

	/*! decodeLeaf, but a shape that yields no GEOMETRY is still a shape.
	 *
	 * A compressed mesh with no triangles decodes to nothing drawable and used to
	 * be dropped, which left a 12-instance compound holding 11 children and made
	 * the whole system unwritable. The bytes are captured either way, so the shape
	 * is kept: it draws nothing, exactly as before, and it writes.
	 */
	auto decodeShapeSlot = [&]( qsizetype off, const QString & cls, HknpShape & out ) -> bool {
		if ( decodeLeaf( off, cls, out ) )
			return true;
		if ( !cls.endsWith( QLatin1String( "Shape" ) ) || out.rawData.isEmpty() )
			return false;
		/* Kept for its bytes, but SAY SO. A shape that reaches here contributes no
		 * geometry: it cannot be drawn, picked, decompiled or edited, and it was
		 * indistinguishable in every report from one that decoded fine. That is how
		 * hknpConvexShape stayed hidden -- 0 verts, 0 tris, no error anywhere, and
		 * a packfile that then refused to assemble with no clue why.
		 */
		if ( !sys.geometrylessShapes.contains( cls ) )
			sys.geometrylessShapes.append( cls );
		return true;
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

		// the compound as an object, kept alongside the flattened children so a
		// writer has something to reproduce
		HknpCompound comp;
		comp.rawOffset = obj.first;
		comp.dynamic = ( obj.second == QLatin1String( "hknpDynamicCompoundShape" ) );
		comp.shapeFlags = r.u32( obj.first + 0x10 );
		comp.materialCRC = r.u32( obj.first + 0x18 );
		comp.aabbMin = r.vec3( obj.first + 0x80 );
		comp.aabbMax = r.vec3( obj.first + 0x90 );
		comp.headerTail.resize( 0x60 );
		for ( int k = 0; k < 0x60; k++ )
			comp.headerTail[k] = char( r.u8( obj.first + 0x70 + k ) );
		comp.bodyId = compoundBody ? compoundBody->id : -1;
		// the shape-data object, carried whole -- see HknpCompound::dataRawData
		if ( qsizetype cd = global.value( obj.first + 0xc0, -1 ); cd >= 0 ) {
			comp.dataClassName = objClass.value( cd );
			comp.dataRawData = objectBytes( cd );
			comp.dataLocal = objectLocal( cd, comp.dataRawData.size() );
		}
		for ( quint32 i = 0; i < numInst; i++ ) {
			Reader::Scope instanceScope( r );
			const qsizetype inst = instBase + qsizetype( i ) * 0x80;
			HknpCompound::Instance one;
			for ( int rr = 0; rr < 3; rr++ )
				one.rotRows[rr] = r.vec3( inst + qsizetype( rr ) * 16 );
			one.trans = r.vec3( inst + 0x30 );
			one.scale = r.vec3( inst + 0x40 );
			one.wPayload[0] = r.u32( inst + 0x0c );
			one.wPayload[1] = r.u32( inst + 0x1c );
			one.wPayload[2] = r.u32( inst + 0x2c );
			one.wPayload[3] = r.u32( inst + 0x3c );
			comp.instances.append( one );
		}

		/* r.ok is STICKY: one out-of-range read anywhere leaves it false and every
		 * later `&& r.ok` loop stops early without saying so. That silently dropped
		 * the last child of a 12-instance compound, which then would not assemble.
		 * A child's decode is bounded work; if it reads badly, that is its problem
		 * and not a reason to abandon the children after it.
		 */
		for ( quint32 i = 0; i < numInst; i++ ) {
			Reader::Scope childScope( r );
			qsizetype inst = instBase + qsizetype( i ) * 0x80;
			qsizetype child = global.value( inst + 0x50, -1 );
			if ( child < 0 )
				continue;
			consumed.insert( child );
			HknpShape shape;
			if ( !decodeShapeSlot( child, objClass.value( child ), shape ) )
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
			comp.children.append( int( sys.shapes.size() ) );
			sys.shapes.append( shape );
		}
		sys.compounds.append( comp );
	}

	// pass 2: shapes referenced by bodies, placed by the body transform
	// (one output shape per body, so shared shapes instance correctly)
	for ( const auto & bp : bodies ) {
		if ( consumed.contains( bp.first ) )
			continue;	// compounds were handled in pass 1
		HknpShape shape;
		if ( decodeShapeSlot( bp.first, objClass.value( bp.first ), shape ) ) {
			shape.bodyId = bp.second.id;
			applyResolvedBodyMaterial( shape, shape.bodyId );
			sys.shapes.append( shape );
		} else {
			// Say so. A body's shape that fails here used to vanish without a
			// trace - pass 3 reports its failures but pass 2 did not - which is
			// exactly how the broken sphere decode stayed invisible once bodies
			// started resolving and spheres stopped reaching pass 3.
			const QString cls = objClass.value( bp.first );
			if ( !cls.isEmpty() && !sys.unknownShapes.contains( cls ) )
				sys.unknownShapes.append( cls );
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

	sys.readTruncated = r.everFailed;
	sys.readTruncatedAt = r.firstFail;
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

#ifndef HKNPENCODE_H
#define HKNPENCODE_H

#include "data/niftypes.h"

#include <QByteArray>
#include <QString>
#include <QVector>

//! Physics and geometry accepted by the FO4 hknp compressed-mesh writer.
struct HknpEncodeInput
{
	QVector<Vector3> verts;       //!< Havok-space vertices
	QVector<Triangle> tris;
	quint32 materialCRC = 0;
	quint32 layer = 1;
	quint8 filterFlags = 0;
	quint16 filterGroup = 0;
	bool dynamic = false;
	float mass = 0.0f;
	float friction = 0.5f;
	float restitution = 0.4f;
	float gravityFactor = 1.0f;
	float maxLinVelocity = 104.375f;
	float maxAngVelocity = 31.57f;
	float linDamping = 0.1f;
	float angDamping = 0.05f;
	Vector3 center;
	Vector3 inertia;
};

//! Encode one editable collision body as an hk_2014.1.0 hknp packfile.
//! The current writer emits one compressed-mesh section (255 vertices/tris).
QByteArray hknpEncodeCompressedMesh( const HknpEncodeInput & input, QString * error = nullptr );

//! Geometry of one hknpCapsuleShape, enough to write the whole 432-byte object.
struct HknpCapsuleInput
{
	Vector3 capA, capB;         //!< segment end points, shape space
	float radius = 0.0f;        //!< convexRadius as stored
	quint32 materialCRC = 0;

	/*! Perpendicular basis vector, optional.
	 *
	 * A capsule is rotationally symmetric, so any unit vector perpendicular to
	 * the axis describes the same solid. Vanilla files nonetheless carry a
	 * definite roll, and it is NOT a function of the axis: capsules whose axes
	 * agree to 0.008 degrees were written with different rolls, while capsules
	 * whose axes agree exactly always agree on the roll too (34 of 34 groups).
	 * It is inherited from the authored primitive and cannot be recovered from
	 * (capA, capB, radius) alone.
	 *
	 * So: pass the frame read back off an existing shape to rewrite that shape
	 * byte for byte, and leave this unset for a new capsule, which then gets a
	 * deterministic synthesized frame.
	 */
	bool hasFrame = false;
	Vector3 frameU;

	/*! Core-box padding, optional; 0 derives it as radius/99.
	 *
	 * The stored hull is the segment grown by this much on all three local axes,
	 * and the solid is that box offset by radius. Across the corpus the ratio
	 * padding/radius sits at 1/99 but SCATTERS by 1.6e-5 relative -- hundreds of
	 * ULP, far more than any rounding of a fixed formula. So the padding is not a
	 * function of the stored radius; 1/99 is the authoring intent (an outer radius
	 * split 0.99 into the convex radius and 0.01 into the box) and what survives
	 * into the file has been through the exporter's own arithmetic.
	 *
	 * Pass the value read back off an existing shape to preserve its geometry
	 * exactly; leave it 0 for a new capsule.
	 */
	float padding = 0.0f;
};

//! Write one hknpCapsuleShape object: always 432 bytes, no packfile around it.
QByteArray hknpEncodeCapsuleShape( const HknpCapsuleInput & input );

/*! Write one hknpSphereShape object: always 128 bytes, no packfile around it.
 *
 * Nothing here is derived, unlike a capsule -- a sphere stores its centre and its
 * radius and everything else is constant -- so this reproduces vanilla exactly.
 */
QByteArray hknpEncodeSphereShape( const Vector3 & centre, float radius, quint32 materialCRC );

/*! Write one hknpShapeMassProperties object: always 48 bytes.
 *
 * Required by every convex polytope. `inertiaRaw` is the value as STORED, which is
 * 1.5x the physical inertia -- pass `HknpShape::massInertiaRaw`, not massInertia().
 *
 * `majorAxis` is the 8-byte frame at +0x20, carried verbatim because its packing is
 * not decoded (see HknpShape). Pass the value read off an existing shape to rewrite
 * it; there is no honest default for a new one yet.
 */
QByteArray hknpEncodeShapeMassProperties( const Vector3 & centreOfMass, const Vector3 & inertiaRaw,
	float volume, float mass, quint64 majorAxis );

#endif // HKNPENCODE_H

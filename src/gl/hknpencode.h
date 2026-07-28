#ifndef HKNPENCODE_H
#define HKNPENCODE_H

#include "data/niftypes.h"
#include "gl/hknpdecode.h"   // HknpCompound is the writer's input as well as a decode result

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

//! Geometry of one hknpConvexPolytopeShape. Variable length, unlike the primitives.
struct HknpPolytopeInput
{
	/*! Hull vertices. Padded to a multiple of 4 by the writer if needed; pass the
	 * array straight off a decode to keep vanilla's own padding, since the padding
	 * slots repeat an earlier vertex rather than holding a fixed filler.
	 */
	QVector<Vector3> verts;
	/*! Face planes (nx, ny, nz, d) with n.x + d = 0 on the face. The stored array
	 * is padded to roundup(faces, 4); pass the spare slots through from a decode,
	 * as their contents are NOT one sentinel the way a capsule's are.
	 */
	QVector<Vector4> planes;
	QVector<QVector<int>> faces;   //!< face loops, indices into verts
	//! Havok's per-face minHalfAngle, parallel to faces. It is the sharpest edge on
	//! the face quantized over 0..90 degrees; pass it through from a decode, since
	//! deriving it reproduces vanilla exactly only about two thirds of the time.
	QVector<quint8> faceAngles;
	float convexRadius = 0.0f;
	quint32 materialCRC = 0;
	quint32 shapeFlags = 0x01000143u;   //!< +0x10; 74 of 76 vanilla polytopes
};

//! Write one hknpConvexPolytopeShape object, no packfile around it. Empty on
//! invalid input (no faces, a loop under 3 vertices, or an out-of-range index).
QByteArray hknpEncodeConvexPolytopeShape( const HknpPolytopeInput & input );

/*! Where a compound's pointers have to be patched.
 *
 * Offsets are relative to the object's own start. Every one of these slots holds
 * raw zero in the bytes -- verified on all 60 instances in the corpus -- because a
 * Havok packfile binds pointers through its fixup tables, not through the data. So
 * unlike the primitives, writing a compound is not finished when the bytes are
 * written: the caller must add these entries once it knows where the children and
 * the shape-data object landed.
 */
struct HknpCompoundFixups
{
	qsizetype instanceArrayPointer = 0x60;   //!< LOCAL fixup -> instanceArray
	qsizetype instanceArray = 0xD0;          //!< where that fixup points
	qsizetype shapeDataPointer = 0xC0;       //!< GLOBAL fixup -> the CompoundShapeData
	QVector<qsizetype> childPointers;        //!< GLOBAL fixup per instance -> its shape
};

//! Write one compound shape object. Fills `fixups` with the slots the caller must
//! still patch (see HknpCompoundFixups). Empty on an empty instance list.
QByteArray hknpEncodeCompoundShape( const HknpCompound & compound,
	HknpCompoundFixups * fixups = nullptr );

/*! Write one hkpRagdollConstraintData: always 416 bytes.
 *
 * The object is a fixed atom chain -- SET_LOCAL_TRANSFORMS, SETUP_STABILIZATION,
 * RAGDOLL_MOTOR, ANG_FRICTION, TWIST_LIMIT, CONE_LIMIT, CONE_LIMIT, BALL_SOCKET --
 * identical on all 521 in the corpus, filling the 416 bytes exactly.
 *
 * If `constraint.rawData` holds the original object the write starts from it and
 * touches only the modelled fields, so editing a limit disturbs nothing else. That
 * matters because parts of the object are not modelled and not even meaningful:
 * one vanilla pivot's w slot is uninitialised padding. Without it, a fresh template
 * built from the measured constants is used.
 */
QByteArray hknpEncodeRagdollConstraintData( const HknpConstraint & constraint );

/*! Write one hkpLimitedHingeConstraintData: always 304 bytes.
 *
 * Same treatment as the ragdoll constraint. Its chain is SET_LOCAL_TRANSFORMS,
 * SETUP_STABILIZATION, ANG_MOTOR, ANG_FRICTION, ANG_LIMIT, TWO_D_ANG, BALL_SOCKET,
 * identical on all 246 in the corpus, plus 8 bytes of alignment tail. Unlike the
 * ragdoll type this one has a real pivotA -- hinges sit off the bone origin.
 */
QByteArray hknpEncodeLimitedHingeConstraintData( const HknpConstraint & constraint );

/*! Write one hkpPositionConstraintMotor: always 48 bytes, and entirely constant.
 *
 * Not one of its 12 words varies across the 37 in the corpus, so there is nothing
 * to parameterise. Present so a ragdoll writer does not have to carry a literal.
 */
QByteArray hknpEncodePositionConstraintMotor();

//! Write one hknpBreakableConstraintData: 48 bytes, everything zero but the
//! break threshold at +0x20. Only 3 exist in the corpus (two at 0, one at 0.01).
QByteArray hknpEncodeBreakableConstraintData( float threshold );

//! The three array pointers in an hkaSkeleton, all patched by LOCAL fixups.
struct HknpSkeletonFixups
{
	qsizetype parentsPointer = 0x18, parents = 0x90;
	qsizetype bonesPointer = 0x28, bones = 0;
	qsizetype posePointer = 0x38, pose = 0;
};

/*! Write one hkaSkeleton. Variable length; empty if given no bones.
 *
 * Layout verified on all 37 corpus skeletons with no exceptions: a 24-byte zero
 * header, three hkArray descriptors at +0x18/+0x28/+0x38 (pointer, then count at
 * +8 and count|0x80000000 at +12), then parent indices as hkInt16 at +0x90, the
 * bone records at align16(0x90 + 2n) at 16 bytes each, and the reference pose at
 * 48 bytes each. Total is exactly pose + 48n.
 *
 * A bone record's name pointer is null on all 804 corpus bones -- the ragdoll's
 * skeleton copy identifies bones by index and carries no strings -- so this needs
 * no string table and no fixups beyond the three array pointers.
 */
QByteArray hknpEncodeSkeleton( const QVector<HknpBone> & bones,
	HknpSkeletonFixups * fixups = nullptr );

#endif // HKNPENCODE_H

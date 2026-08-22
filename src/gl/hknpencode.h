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
	/*! Per-triangle Havok material CRC, parallel to `tris`.
	 *
	 * Empty means the whole mesh takes `materialCRC`, which is what every
	 * compile wrote before 2026-08-20. Filled, the writer builds the shape's
	 * material table from the distinct values in first-use order and emits the
	 * CMSD run table over it, so a body assembled from parts with different
	 * materials keeps them instead of taking the first leaf's for everything.
	 *
	 * A run record names its material in ONE BYTE, so at most 256 distinct
	 * values encode; beyond that the writer refuses rather than silently
	 * folding them together.
	 */
	QVector<quint32> triMaterial;
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
	/*! KEYFRAMED: the body has a motion index and an inertia record, but NO
	 * dyn_motion record. Measured over 1,200 vanilla files, the three states are
	 * distinct and the counts say which:
	 *
	 *   motion=0 inertia=0, index 0x7fffffff   727 files   a true static
	 *   motion=N inertia=N, index set          ~190        a dynamic prop
	 *   motion=0 inertia=N, index set          170         KEYFRAMED
	 *
	 * and every file in that third group is something the game ANIMATES:
	 * Animated/CarPush01, DinerDoorSingle01, CraneBridge01, FenceChainlinkGate01,
	 * and every door in Concord. Compile collapsed it to a true static, so the
	 * engine had no motion to drive and the door could not open however correct
	 * its collision was.
	 */
	bool keyframed = false;
	/*! cinfo +0x40, the body's own orientation, xyzw.
	 *
	 * Identity on every static and every dynamic prop measured, and NOT identity
	 * on an animated one: BldWoodPDoorBroke01 carries (0, 0, 0.7513, 0.66), a 97
	 * degree turn about Z, which with the position at +0x30 is the hinge's rest
	 * frame. Written as identity before, which is the same class of loss as the
	 * position: invisible while nothing moves it.
	 */
	Quat orientation = Quat( 1, 0, 0, 0 );
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

/*! Write one hknpConvexShape: the base of the convex family, vertices and nothing else.
 *
 * Size is align16( 0x40 + verts * 16 ) -- 128 bytes for 4 vertices, 192 for 8, the
 * only two counts in Fallout 4. There is no plane, face or index array: the vertex
 * payload starts at +0x40, which is exactly where a polytope's plane descriptor sits,
 * and that is the whole difference between the two classes at the byte level.
 *
 * Pass the vertex array straight off a decode. Its LENGTH is part of the object's
 * size, and the eight-vertex objects store each corner of a quad twice, so compacting
 * it changes the file.
 */
QByteArray hknpEncodeConvexShape( const QVector<Vector3> & verts, float convexRadius,
	quint32 materialCRC, quint32 shapeFlags = 0x01000001u );

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

/*! Write one hknpCompoundShapeData: the BVH a compound's children hang off.
 *
 * Decoded 2026-08-21; 86 of 86 vanilla compounds fit the model, and Elric
 * recompiling a decompiled two-box body reproduces vanilla's object byte for
 * byte. `0x60 + 2n * 32` bytes: a small header, then 2n-1 nodes depth first and
 * one zero record. A node is `float3 min | u32 0x3f000000|(parent+1) | float3
 * max | u16 leftChild+1 or 0 | u16 rightChild+1 or the instance index`.
 *
 * Pass each child's AABB in the COMPOUND's space, in instance order. The tree
 * built here splits at the median centroid along the widest axis, which need not
 * be the split Elric picked: a different tree over the same boxes is a different
 * valid answer, not a wrong one.
 */
QByteArray hknpEncodeCompoundShapeData( const QVector<QPair<Vector3, Vector3>> & childBoxes,
	QVector<int> * instanceLeaf = nullptr );

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

/*! One object on its way into a packfile: its bytes, its class, its pointers.
 *
 * Every encoder above returns bytes with its pointer slots left at raw zero,
 * because a Havok packfile binds pointers through fixup tables rather than
 * through the data. This is what carries those slots to the assembler.
 *
 * Offsets are relative to the object's own start, so an object can be built
 * before anything knows where it will land.
 */
struct HknpPackObject
{
	QString className;
	QByteArray bytes;
	//! LOCAL fixups: source and target both inside this object
	QVector<QPair<qsizetype, qsizetype>> local;
	//! GLOBAL fixups: a source inside this object, pointing at another object
	struct Ref { qsizetype source; int object; };
	QVector<Ref> global;
};

/*! Assemble objects into an hk_2014.1.0-r1 packfile.
 *
 * Object 0 is the root -- the file header names its class, and the game loads
 * that object. Objects land in the order given, each padded to 16.
 *
 * The layout rules are measured on the 37 vanilla ragdolls, not assumed:
 *
 *  - __classnames__ starts at 0x100, entries are `u32 hash | 0x09 | name | NUL`,
 *    padded to 16 with 0xff. hkClass, hkClassMember, hkClassEnum and
 *    hkClassEnumItem always lead -- which is what puts the root's own name at
 *    offset 75 in every vanilla file -- and the rest follow in ORDER OF FIRST
 *    USE walking the objects (37/37).
 *  - __types__ is empty and sits between classnames and data.
 *  - local and virtual fixups ascend by source. GLOBAL fixups do NOT: they are
 *    emitted grouped by source object, and within an object in MEMBER
 *    DECLARATION order, which is not offset order once arrays are involved --
 *    an array member contributes its payload's fixups where the member is
 *    declared, so hknpRagdollData's skeleton pointer at +0x78 is written after
 *    the fixups for the array whose payload sits at +0x7c0. Passing the fixups
 *    in declaration order is therefore the caller's job; this preserves it.
 *
 * Returns empty and sets `error` if an object names a class with no known hash.
 */
QByteArray hknpBuildPackfile( const QVector<HknpPackObject> & objects, QString * error = nullptr,
	const QHash<QString, quint32> & extraHashes = {}, quint32 globalFixupBytes = 0 );

/*! Where an hknpRagdollData's pointers have to be patched, all relative to the
 * object's start and IN THE ORDER hknpBuildPackfile wants them.
 *
 * The globals are a reflection walk: one per body cinfo, then one per
 * constraint, then one per shape-list slot, then the skeleton. Emitting them
 * sorted by offset instead puts the skeleton in the wrong place.
 */
struct HknpRagdollDataFixups
{
	QVector<QPair<qsizetype, qsizetype>> local;   //!< the seven array descriptors
	QVector<qsizetype> bodyShapePointers;         //!< cinfo[i] + 0x00, per body
	QVector<qsizetype> constraintPointers;        //!< constraint entry + 0x00
	QVector<qsizetype> shapeListPointers;         //!< the +0x60 array, NIF order
	qsizetype skeletonPointer = 0x78;             //!< written LAST of the globals
};

/*! Write one hknpRagdollData: the root object of a ragdoll.
 *
 * Header is entirely zero apart from seven hkArray descriptors -- at +0x10
 * body_props, +0x20 dyn_motion, +0x30 dyn_inertia, +0x40 cinfo, +0x50
 * constraints, +0x60 the shape list and +0x80 bone-to-body -- plus a plain
 * skeleton pointer at +0x78. Payloads run back to back from +0x90, each padded
 * to 16, and the object ends where the last one does.
 *
 * THE COUNTS ARE NOT ALL THE SAME. Four arrays are per-BODY, the constraint
 * array is bones-1 and the bone-to-body array is per-BONE. On 34 of the 37
 * vanilla ragdolls bodies == bones so the difference is invisible; the three
 * that differ are parts kits (SkeletonRef 48/11, skeletonSentryBodyPart 24/9,
 * TurretMountedSkeleton 5/4) whose spare collision bodies sit outside the
 * ragdoll hierarchy.
 */
QByteArray hknpEncodeRagdollData( const HknpSystem & system, HknpRagdollDataFixups * fixups );

/*! Write one hknpPhysicsSystemData: the root of every collision packfile that is
 * not a ragdoll, which is most of them.
 *
 * The ragdoll root DERIVES from this one, so the first six array descriptors are
 * the same members at the same offsets and the payloads start at +0x90 either
 * way. What this one lacks is the bone-to-body array at +0x80 and the skeleton
 * pointer at +0x78; what it gains is nothing. A static system carries only three
 * of the six -- body_props, cinfo and the shape list -- since dyn_motion and
 * dyn_inertia exist only when something simulates, and the constraint array only
 * when something is jointed.
 *
 * Reuses HknpRagdollDataFixups: the slots mean the same thing, and the skeleton
 * pointer is simply left at 0.
 */
QByteArray hknpEncodePhysicsSystemData( const HknpSystem & system, HknpRagdollDataFixups * fixups );

/*! Assemble any collision packfile from a decoded system: a ragdoll when it has
 * bones, a plain physics system otherwise.
 *
 * hknpEncodeRagdoll is this with the ragdoll path required.
 */
QByteArray hknpEncodeSystem( const HknpSystem & system, QString * error = nullptr );

/*! Assemble a whole ragdoll packfile from a decoded system.
 *
 * Writes the root, every shape with its mass properties, every constraint with
 * its motor, and the skeleton, then binds them. This is what makes the object
 * encoders usable: on their own they produce bytes nothing can load.
 *
 * Returns empty and sets `error` if the system holds a shape class that has no
 * encoder yet -- notably compressed meshes, which still only decode.
 */
QByteArray hknpEncodeRagdoll( const HknpSystem & system, QString * error = nullptr );

#endif // HKNPENCODE_H

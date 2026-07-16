/* Decoder for FO4 compiled collision: the "Binary Data" of a
   bhkPhysicsSystem / bhkRagdollSystem is a Havok 2014 binary packfile
   holding hknp (new physics) objects. Format notes and the Python
   reference implementation live in tools/hkparse.py and
   TO_BE_IMPLEMENTED.md ("Display compiled FO4 collision"). */

#ifndef HKNPDECODE_H
#define HKNPDECODE_H

#include "data/niftypes.h"

#include <QByteArray>
#include <QString>
#include <QVector>

//! One decoded hknp collision shape, in Havok units
//! (multiply by 69.99125 for game units)
struct HknpShape
{
	QString className;
	//! shape-local vertices (primitives get synthesized wireframe geometry)
	QVector<Vector3> verts;
	//! triangulated geometry (convex faces are fan-triangulated)
	QVector<Triangle> tris;
	//! convex only: the original face loops (indices into verts)
	QVector<QVector<int>> faces;
	//! convex only: face planes (nx, ny, nz, d)
	QVector<Vector4> planes;
	float convexRadius = 0.0f;
	bool isConvex = false;
	//! Havok material CRC (u32 at shape+0x18; same value as the legacy
	//! HavokMaterial enum). 0 = unknown.
	quint32 materialCRC = 0;

	//! primitive interpretation: 0 generic, 1 sphere, 2 capsule
	int primType = 0;
	Vector3 capA, capB;            //!< capsule end points / sphere center (capA)
	float primRadius = 0.0f;       //!< sphere / capsule radius (capsule: approx)

	//! index of the hknpBodyCinfo this shape belongs to (-1 = none). Each
	//! bhkNPCollisionObject names its body via its "Body ID" field, and the
	//! body is PLACED BY THAT NODE's world transform (the cinfo position /
	//! orientation is Elric's rest pose, not the scene placement - validated
	//! on vanilla stair helpers where the two differ).
	int bodyId = -1;

	//! instance transform from hknpDynamicCompoundShape. Verbatim NIF
	//! Matrix4 rows: v' = (v * scaleVec) . R + trans (row vectors)
	bool hasTransform = false;
	Vector3 rotRows[3] = { Vector3( 1, 0, 0 ), Vector3( 0, 1, 0 ), Vector3( 0, 0, 1 ) };
	Vector3 trans;
	Vector3 scaleVec = Vector3( 1, 1, 1 );

	//! a vertex with the instance transform applied (still Havok units)
	Vector3 transformed( const Vector3 & v ) const
	{
		if ( !hasTransform )
			return v;
		Vector3 s( v[0] * scaleVec[0], v[1] * scaleVec[1], v[2] * scaleVec[2] );
		return rotRows[0] * s[0] + rotRows[1] * s[1] + rotRows[2] * s[2] + trans;
	}
};

//! Per-body physics decoded from the packfile (indexed by body id).
//! Layouts validated against controlled Elric pairs + CK FileConvert XML
//! (PropCollision mass 10 -> inverseMass 0.1, layer 10, motionType 2);
//! body_props stride 0x50 established by signature scan on 6-body files
//! (PyNifly documents 0x110, which only ever fit single-body files).
struct HknpBodyPhys
{
	quint32 layer = 1;          //!< cinfo +0x14 packed Havok Filter (old local writer used +0x1C)
	quint8 filterFlags = 0;     //!< packed Havok Filter byte 1
	quint16 filterGroup = 0;    //!< packed Havok Filter upper 16 bits
	quint32 materialCRC = 0;    //!< body material ID resolved through hknpBSMaterialProperties
	bool hasMotion = false;     //!< cinfo +0x0C != 0x7fffffff (dynamic body)
	float friction = 0.5f;      //!< body_props +0x12 (truncated float16)
	float restitution = 0.4f;   //!< body_props +0x16 (truncated float16)
	Vector3 com;                //!< cinfo +0x30 (center of mass for dynamic)
};

//! A decoded bhkPhysicsSystem blob
struct HknpSystem
{
	QVector<HknpShape> shapes;
	//! per-body physics, indexed by body id (may be shorter than expected on
	//! malformed data - callers use value( id ) semantics)
	QVector<HknpBodyPhys> bodyPhys;
	//! dyn_motion / dyn_inertia arrays present (PSD +0x20 / +0x30): the
	//! system simulates dynamically (props); statics lack both arrays
	bool dynamic = false;
	float mass = 0.0f;          //!< 1 / (dyn_inertia +0x04); 0 for statics
	float density = 0.0f;       //!< dyn_inertia +0x08 (mass / collision volume)
	Vector3 inertia;            //!< dyn_inertia +0x20/24/28 (diagonal)
	//! dyn_motion values (engine defaults when the array is absent)
	float gravityFactor = 1.0f;         //!< dyn_motion +0x08
	float maxLinVelocity = 104.375f;    //!< dyn_motion +0x10
	float maxAngVelocity = 31.57f;      //!< dyn_motion +0x14
	float linDamping = 0.1f;            //!< dyn_motion +0x18
	float angDamping = 0.05f;           //!< dyn_motion +0x1C
	//! class names present but not decoded
	QStringList unknownShapes;
	bool valid = false;
	QString error;
};

//! Decode the Binary Data of a bhkPhysicsSystem / bhkRagdollSystem block
HknpSystem hknpDecode( const QByteArray & data );

//! The decode result for this data, from a small internal cache
const HknpSystem & hknpDecodeCached( const QByteArray & data );

#endif // HKNPDECODE_H

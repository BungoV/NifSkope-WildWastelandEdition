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

#endif // HKNPENCODE_H

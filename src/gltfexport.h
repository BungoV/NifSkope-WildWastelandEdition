/* glTF 2.0 writer for a posed, skinned, ANIMATED character: the node
   hierarchy, one or more skinned meshes and one animation sampled per frame,
   written as a .gltf (JSON) beside a .bin (the single buffer), which Blender
   opens natively (File > Import > glTF 2.0).

   Lane HKX4, 2026-09-10. The mapping contract -- axes, units, bone order,
   channels and what is lost -- is docs/GLTF_INTERCHANGE.md; this file
   implements exactly that page.

   WHY A SECOND EXPORTER. src/lib/importex/gltf.cpp (upstream, tiny_gltf) also
   writes glTF and stays the exporter for a static scene: it carries materials,
   textures embedded as PNG, LODs and Starfield. It cannot write an ANIMATION
   (glTF `animations` is never populated there), and it is reachable only
   through the GL Scene, so nothing it does can be gated while the exe cannot
   be built. This writer is deliberately the other shape: a neutral input
   struct in, bytes out, QtCore only, no NifModel, no Scene, no GL, so a
   standalone binary links it and the gates run without NifSkope.exe. The two
   agree on the two conventions a user can see -- 1 unit = 0.9144/64 m and the
   Z-up -> Y-up rotation at the root -- so a mesh exported by either lands in
   the same place in Blender.

   Everything is refused by a sentence that names the field and its value; the
   writer never writes a file it cannot describe. */

#ifndef GLTFEXPORT_H
#define GLTFEXPORT_H

#include "data/niftypes.h"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

//! One node of the exported hierarchy, in NIF space and NIF units.
struct GltfExportNode
{
	QString name;
	int parent = -1;                          //!< index into GltfExportScene::nodes; -1 = a scene root
	Vector3 translation;
	Quat rotation;                            //!< NifSkope order (w, x, y, z)
	Vector3 scale = Vector3( 1.0f, 1.0f, 1.0f );
};

//! FO4 stores exactly four influences per vertex.
struct GltfExportVertexSkin
{
	quint16 joints[4] = { 0, 0, 0, 0 };       //!< indices into GltfExportMesh::joints
	float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};

//! One shape. positions/normals are mesh-local, in NIF units; for a skinned
//! shape the NIF's own shape transform is already folded into inverseBind
//! (see the contract), so the mesh node is written with no transform, which
//! is what the glTF spec requires of a skinned mesh node anyway.
struct GltfExportMesh
{
	QString name;
	int node = -1;                            //!< the scene node this mesh hangs on
	QVector<Vector3> positions;
	QVector<Vector3> normals;                 //!< empty or positions.size()
	QVector<Vector2> texCoords;               //!< empty or positions.size()
	QVector<quint32> indices;                 //!< 3 per triangle
	QVector<GltfExportVertexSkin> skin;       //!< empty = unskinned
	QVector<int> joints;                      //!< scene-node index per skin joint
	QVector<float> inverseBind;               //!< 16 floats per joint, COLUMN-major, translation in NIF units
	QString materialName;
	QString diffuseUri;                       //!< external image URI, may be empty
	QString diffuseSourcePath;                //!< the RAW path the NIF stored, before normalising; extras only
	QStringList mergedPartitions;             //!< the segments merged into this primitive, recorded in extras
};

//! Per-frame local TRS for one node. Any of the three may be empty (that
//! channel is then not written and the node keeps its bind value).
struct GltfExportChannel
{
	int node = -1;
	QVector<Vector3> translations;
	QVector<Quat> rotations;
	QVector<Vector3> scales;
};

struct GltfExportAnimation
{
	QString name;
	int numFrames = 0;
	float frameDuration = 0.0f;               //!< seconds per frame; the clip's own rate
	QVector<GltfExportChannel> channels;

	//! Root motion (hkaDefaultAnimatedReferenceFrame). Written ONLY when
	//! applyRootMotion is true, and then only onto rootMotionNode's channels:
	//! translation += rootMotionTranslation[f], rotation = yaw(f) * rotation.
	//! When false it is omitted from the channels and recorded in extras, so
	//! the clip plays in place -- the flag is the way back (CONSTITUTION 7).
	bool applyRootMotion = false;
	int rootMotionNode = -1;
	QVector<Vector3> rootMotionTranslation;
	QVector<float> rootMotionYaw;             //!< radians about rootMotionUp
	Vector3 rootMotionUp = Vector3( 0.0f, 0.0f, 1.0f );

	//! Tracks whose bone has no node here, listed by name in extras and in
	//! the caller's report; never silently dropped.
	QStringList unmatchedTracks;
};

struct GltfExportScene
{
	QVector<GltfExportNode> nodes;
	QVector<GltfExportMesh> meshes;
	QVector<GltfExportAnimation> animations;

	//! metres per NIF unit. 0.9144 / 64 = 0.0142875 exactly (64 units = one
	//! yard), the same constant src/lib/importex/gltf.cpp uses.
	float unitScale = 0.9144f / 64.0f;
	//! Name of the synthetic root that carries the Z-up -> Y-up rotation.
	QString upAxisNodeName = QStringLiteral( "NifSkope_Y_up" );
	QString generator;
	QString copyright;
};

//! Writes <gltfPath> and <gltfPath with .bin>. False + a sentence in `error`
//! on any inconsistency; nothing is written when it refuses.
bool gltfExportWrite( const GltfExportScene & scene, const QString & gltfPath, QString & error );

//! The .bin path this writer will use for a given .gltf path.
QString gltfExportBinPath( const QString & gltfPath );

#endif // GLTFEXPORT_H

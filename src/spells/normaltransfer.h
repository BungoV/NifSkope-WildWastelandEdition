/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef NORMALTRANSFER_H
#define NORMALTRANSFER_H

#include "data/niftypes.h"

#include <QVector>

#include <array>

class NifModel;

//! @file normaltransfer.h Copy one mesh's normals onto another.
/*!
 * The spell (Mesh ▸ Transfer Normals…) and the CLI (`transfer-normals`) are the
 * same code with two front ends, so what a script does and what the dialog does
 * cannot drift apart — and so the mapping can be tested without a dialog in the
 * way, which is the only reason the CLI verb exists.
 *
 * Mappings are Blender's Data Transfer ▸ Face Corner Data list, in its order.
 */
namespace NormalTransfer
{

enum Mapping
{
	Topology = 0,                       //!< index for index
	NearestCornerBestNormal,            //!< nearby corner whose normal agrees
	NearestCornerBestFaceNormal,        //!< the same, judged by face normal
	NearestCornerOfNearestFace,         //!< closest face, then its nearest corner
	NearestFaceInterpolated,            //!< closest face, barycentric blend
	ProjectedFaceInterpolated,          //!< cast along this vertex's own normal
	MappingCount
};

//! One mesh's positions, normals and faces, in WORLD space — so a donor posed
//! differently from the target still lines up.
struct Mesh
{
	QVector<Vector3> pos, nrm;
	/*! Faces as INT triples, not Triangle.
	 *
	 *  Triangle indexes with quint16, which is right for one FO4 shape and wrong
	 *  the moment several are combined: five 20k-vertex pieces is 100k vertices,
	 *  and every index past 65535 would silently wrap into another piece's
	 *  geometry. Reading is where the format's limit belongs; this is the working
	 *  copy. */
	QVector<std::array<int, 3>> tris;
	QVector<QVector<int>> incident;     //!< vertex -> the faces it belongs to
	bool valid() const { return !pos.isEmpty(); }
};

//! Read a shape (BSTriShape or NiTriShape) into world space. An empty result
//! means the block has no normals to read, which is a fact, not an error.
Mesh read( const NifModel * nif, int block );

/*! Several sources as one.
 *
 *  Transferring from a multi-selection is not "run it once per source and keep
 *  the last": every mapping here already asks "which source vertex or face is
 *  nearest", and the answer over a set of meshes is the answer over their union.
 *  Concatenating them (with the triangle indices rebased) makes that literally
 *  true, so one selection of five armour pieces behaves as the one surface they
 *  visually are. Topology is the exception and the caller must refuse it: index
 *  N of a concatenation means nothing.
 */
Mesh combine( const QVector<Mesh> & parts );

//! The mapping itself. No model, no UI: given two meshes it is a pure function,
//! which is what makes it checkable.
QVector<Vector3> map( const Mesh & src, const Mesh & tgt, int mapping, float mix );

//! Write world-space normals back into a shape's own space. Returns how many.
int apply( NifModel * nif, int targetBlock, const QVector<Vector3> & normals );

//! The Blender-facing name of a mapping, for menus, CLI help and messages.
QString mappingName( int mapping );

} // namespace NormalTransfer

#endif // NORMALTRANSFER_H

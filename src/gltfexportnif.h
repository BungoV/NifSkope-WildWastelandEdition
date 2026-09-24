/* The bridge between a loaded NIF (and a loaded .hkx clip) and the neutral
   input of src/gltfexport.cpp.

   src/gltfexport.cpp is deliberately model-free -- a struct in, two files out,
   QtCore only, so it can be linked and gated with no NifSkope.exe (skill
   ww-standalone-writer-gate). This file is the other half: it fills that
   struct from a NifModel and from an HkxAnimClip, and it is the ONLY part of
   the export that knows what a NIF block is.

   Everything it reads was proved field by field by lane HKX4b's standalone
   driver (tests/gltfexport_dump.cpp), which reads the same NIF bytes with its
   own walker, and by tests/spells/gltf_readback.py, which reads them a third
   time in Python. The three agree on all nine shapes of
   fixtures/human_male_vanilla.nif. What this file adds over the driver is
   that it goes through the model rather than the container, so it also works
   on a NIF the user has edited in the tree and not saved.

   The contract it implements is docs/GLTF_INTERCHANGE.md.

   NOT HOOKED UP. The menu entry, the CLI switch and the two NifSkope.pro
   lines are in scratchpad/hkx4_20260910/hookup.py and have not been applied;
   until they are, nothing calls this file and it is not in the build.
   Compile evidence so far is `g++ -fsyntax-only` with the real
   Makefile.Release flags (scratchpad/hkx4_20260910/sx.sh). */

#ifndef GLTFEXPORTNIF_H
#define GLTFEXPORTNIF_H

#include "gltfexport.h"
#include "hkxanim.h"

#include <QHash>
#include <QModelIndex>
#include <QString>
#include <QStringList>

class NifModel;

//! What the export did, for the message the caller shows. Every count here is
//! written by gltfExportNifScene()/gltfExportNifClip() and is checked by
//! tests/spells/gltf_readback.py against the same file read independently.
struct GltfExportReport
{
	int nodes = 0;
	int shapes = 0;
	int skinnedShapes = 0;
	int vertices = 0;
	int triangles = 0;
	int bonesAdded = 0;              //!< skin bones with no node of their own; named in `notes`
	int tracksMatched = 0;
	int tracksUnmatched = 0;
	QStringList notes;               //!< one sentence per thing the reader had to decide
};

/*! Fills `scene` from the NIF.
 *
 * @param nif        the model.
 * @param iRoot      the node to export, or an invalid index for every root.
 * @param scene      filled: nodes in NIF space and NIF units, one mesh per
 *                   BSTriShape-family shape, skins from BSSkin::Instance.
 * @param nodeByName lower-cased node name -> index into scene.nodes, for the
 *                   clip's bone matching. First name wins, as FO4 skeletons
 *                   carry names differing only in case.
 * @param report     counts and notes.
 * @param error      a sentence naming the block and the value, on false.
 */
bool gltfExportNifScene( const NifModel * nif, const QModelIndex & iRoot,
						 GltfExportScene & scene, QHash<QString, int> & nodeByName,
						 GltfExportReport & report, QString & error );

/*! Adds one decoded clip to `scene` as a glTF animation.
 *
 * @param clip           from hkxAnimLoad().
 * @param boneNames      the hkaSkeleton's bone names, in bone-index order.
 *                       Empty = the clip's own skeleton, if it carries one.
 * @param applyRootMotion root motion is composed onto the root bone's channels
 *                       when true, and only recorded in extras when false.
 */
bool gltfExportNifClip( const HkxAnimClip & clip, const QStringList & boneNames,
						const QHash<QString, int> & nodeByName, bool applyRootMotion,
						GltfExportScene & scene, GltfExportReport & report, QString & error );

//! Scene + optional clip + write, in one call. `clip` may be null.
bool gltfExportNifWrite( const NifModel * nif, const QModelIndex & iRoot,
						 const HkxAnimClip * clip, const QStringList & boneNames,
						 bool applyRootMotion, const QString & gltfPath,
						 GltfExportReport & report, QString & error );

/*! The seam to the Animation workspace.
 *
 * The menu entry has no way of its own to know which clip the user has
 * loaded -- that list belongs to the Animation workspace (lane HKX3). It
 * registers a provider here; until it does, `gltfExportClipProvider()` is
 * null, the menu exports the character with no animation, and it SAYS so
 * rather than writing a file that quietly has none.
 *
 * The provider fills `clip` and the bone names of the skeleton the clip was
 * bound to, and returns false when nothing is selected.
 */
typedef bool ( *GltfExportClipProvider )( HkxAnimClip & clip, QStringList & boneNames );
void gltfExportSetClipProvider( GltfExportClipProvider fn );
GltfExportClipProvider gltfExportClipProvider();

#endif // GLTFEXPORTNIF_H

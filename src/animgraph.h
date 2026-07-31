/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef ANIMGRAPH_H
#define ANIMGRAPH_H

#include <QStringList>

class NifModel;

//! @file animgraph.h One animation graph per file.
/*!
 * A NIF carries ONE NiControllerManager, on its root: **0 of the 34,983 meshes
 * in the Fallout 4 corpus have two**. Merging breaks that, because every
 * ArtObject brings its own manager, object palette and multi-target controller.
 * Six X-01 Tesla VFX files onto one skeleton produce six managers, six palettes
 * and twelve sequences sharing two names — and a sequence is addressed BY NAME,
 * so the file then holds six answers to "play autoPlay" and no way to choose
 * between them. Worse, the six managers do not even sit on the root: each landed
 * on whatever node its file attached to (HEAD, LLeg_Calf_Armor2, ...), so five of
 * them are invisible to anything that looks where a manager is supposed to be.
 *
 * consolidateControllerManagers folds them back into one on the root. It is the
 * merge's business, not the loading screen's: a merged rig with six managers is
 * wrong however it is used.
 */

//! What consolidateControllerManagers folded.
struct AnimGraphResult
{
	int managersFolded  = 0;   //!< managers removed (one always survives)
	int sequencesFused   = 0;  //!< same-named sequences merged into one
	int sequencesMoved   = 0;  //!< sequences re-registered on the survivor as-is
	int paletteEntries   = 0;  //!< object-palette entries carried over
	int extraTargets     = 0;  //!< multi-target controller targets carried over
	int blocksRemoved    = 0;  //!< total blocks deleted by the fold
	QStringList sequenceNames; //!< the sequences the survivor ends up with
};

//! Fold every NiControllerManager in \a nif into one on the file root.
/*!
 * A no-op on a file with fewer than two managers, which is every file that has
 * not been merged.
 *
 * Sequences that share a name are fused rather than renamed: whatever plays
 * "autoPlay" has to drive all six limbs' effects, and renaming them apart would
 * need something to know to play six sequences instead of one.
 */
AnimGraphResult consolidateControllerManagers( NifModel * nif );

//! Drop object-palette entries and multi-target extra targets that point at
//! nothing, and compact the arrays behind them.
/*! Deleting blocks rewrites links to them as -1 but leaves the array entry, so a
 *  file that lost its skeleton ends up with a palette full of holes. Returns the
 *  number of entries dropped. */
int pruneDeadAnimLinks( NifModel * nif );

#endif // ANIMGRAPH_H

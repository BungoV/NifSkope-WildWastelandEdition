/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef FREEZEANIM_H
#define FREEZEANIM_H

#include <QModelIndex>
#include <QString>
#include <QStringList>

class NifModel;

//! @file freezeanim.h Bake a named animation sequence to a still.
/*!
 * A loading screen is one instant. The X-01 Tesla FX files are not: each one
 * carries a `NiControllerManager` with two named sequences (`autoPlay` and
 * `autoLoop`), and the arcs, the glow and the ribbons all move.
 *
 * Freezing picks a time in one of those sequences, writes what every controlled
 * block evaluates to at that instant into the field it drives, and then removes
 * the controller graph. The file that comes out is a still of that moment and
 * merges as ordinary static content.
 *
 * Freezing is a **choice, not a requirement** — 18 of the 173 vanilla loading
 * screens move and 13 animate a shader, so a limb can equally be left live. That
 * is exactly why this is a per-file operation applied before the merge rather
 * than a step inside it.
 */

namespace FreezeAnim
{

//! What a freeze did, in enough detail to tell a silent no-op from a success.
struct Result
{
	bool ok = false;
	//! Controlled blocks whose value was evaluated and written.
	int baked = 0;
	//! Controlled blocks left alone (see `unhandled` for why).
	int skipped = 0;
	//! One line per controller type that could not be baked, with the reason.
	QStringList unhandled;
	//! Blocks deleted when stripping the controller graph.
	int blocksRemoved = 0;
	//! Anything worth saying that is not an error: ambiguous node names, etc.
	QStringList notes;
};

//! Start/stop time of a named sequence. False when there is no such sequence.
bool sequenceRange( const NifModel * nif, const QString & sequence, float * start, float * stop );

//! Bake `sequence` at `time` onto the blocks it drives.
/*!
 * Runs as one undoable snapshot operation, so a freeze is Ctrl+Z away and a
 * loaded document can still be put back byte-for-byte.
 *
 * What each controlled block evaluates to is written into the block its
 * controller drives — a node's Translation/Rotation/Scale, a shader property
 * field named by the controller's Controlled Variable, a light's Dimmer, a
 * node's hidden flag. Controllers that drive a *simulation* rather than a value
 * (particle systems, procedural lightning) cannot be baked this way; they are
 * reported in `Result::unhandled` and left untouched, because turning those into
 * a still means snapshotting their generated geometry, which is the
 * loading-screen convert's job, not this one's.
 *
 * \param stripGraph  Remove the controller graph afterwards (manager, sequences,
 *                    controllers, interpolators, key data, text keys, object
 *                    palette). With false the values are baked but the file
 *                    still animates — useful to see what a time looks like
 *                    before committing to it.
 */
Result freeze( NifModel * nif, const QString & sequence, float time,
               bool stripGraph = true, QString * error = nullptr );

} // namespace FreezeAnim

#endif // FREEZEANIM_H

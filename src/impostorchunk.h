/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef IMPOSTORCHUNK_H
#define IMPOSTORCHUNK_H

#include "impostorcard.h"
#include "gl/impostordraw.h"

#include <QString>
#include <QStringList>
#include <QVector>

class Scene;
class NifModel;

/* ---------------------------------------------------------------------------
 * OCTAHEDRAL CARDS INSIDE A NATIVE LOD CHUNK -- a feature master, shipped OFF.
 *
 * The spec (329..333) gives the chunk manifest a `C` line per placed card:
 *
 *     C index cx cy cz halfW halfH N depthspan lodm [arrayLodm layer]
 *
 * and the object row above it gives that index its worldspace position and its
 * SCALE. Neither line is enough on its own: the `C` line's extents are the
 * set's own, unscaled (on Commonwealth chunk -32 16 every `C` line for
 * 000531b3 reads 135.314 x 360.837 while its references read 1.1543, 0.9118,
 * 0.7857 ...), so a reader that ignores the object row draws every tree in the
 * chunk at one size. This pairs them.
 *
 * WHY IT SHIPS OFF. Standing rule, bungo: every feature master ships OFF by
 * default with a menu row. Drawing cards where vanilla puts authored tree LOD
 * changes what a person sees in a chunk they opened to inspect, and that is a
 * choice they make, not one this viewer makes for them.
 *
 * WHAT IT DOES NOT DO. It does not remove the chunk's own tree LOD shapes. The
 * manifest names the reference by its object index and by its form ID, and
 * nothing in this lane has yet proved which drawn shape that index owns; the
 * `hideReplaced` switch below is therefore absent rather than guessed at, and
 * the picture is cards drawn OVER the authored LOD until the mapping is
 * measured. Said here, in the menu row's own text and in the changelog,
 * because a half-done replacement that looks finished is worse than one that
 * says what it is.
 * ------------------------------------------------------------------------- */

namespace ImpostorChunk
{

//! One placement, paired with the object row that gives it a world transform.
struct Placed
{
	ImpostorPlacement c;			//!< the `C` line
	float world[3] = { 0.0f, 0.0f, 0.0f };	//!< the object row's x y z
	float scale = 1.0f;				//!< the object row's scale
	QString formId;					//!< for the log, so a card names its tree
	int setIndex = -1;				//!< into `sets()`; -1 = its set did not load
};

//! THE MASTER. False unless the person turned it on, or `WW_IMPOSTOR_CHUNK` is
//! set in the environment -- which is how the gate reaches it without touching
//! anyone's settings.
bool enabled();
void setEnabled( bool on );

/*! Read a chunk's manifest and hold what it places, for the scene now open.
 *
 *  `chunkPath` is the `.bto`; the manifest is `<chunkPath>.manifest.txt`. A
 *  missing manifest is not an error -- most chunks have none -- and leaves
 *  nothing armed. Returns the number of placements paired with an object row.
 *
 *  Safe to call with the master off: nothing is drawn until `draw`, and
 *  `report()` then still says what the manifest holds, which is the answer to
 *  "why do I see no cards".
 */
int arm( NifModel * nif, const QString & chunkPath );

/*! Arm ONE set, at the origin, because the person opened its `.lodm` itself.
 *
 *  THIS ONE IS NOT BEHIND THE MASTER, and that is deliberate. A master exists
 *  so that drawing cards in place of a chunk's authored LOD is a choice; a
 *  person who opens `000531b3_oct.lodm` has already made every choice there is
 *  to make, and a viewer that answered with an empty window and a switch
 *  somewhere else would be a viewer that cannot open its own file format.
 *
 *  Returns false, with `error` set, for a `.lodm` that is not a card set --
 *  the extension is also the material format, and a material opened here says
 *  which it is rather than drawing nothing. */
bool armSingle( NifModel * nif, const QString & lodmPath, QString * error );

//! Forget everything. Called when the document changes.
void forget();

//! Draw every armed card. Returns how many actually reached the framebuffer;
//! 0 with the master off, without touching any GL state.
int draw( Scene * scene, const ImpostorDraw::Options & opt );

//! What was read, what paired, what loaded and what refused -- one line each.
QStringList report();

//! The placements, for a harness or a gate that wants to count them.
const QVector<Placed> & placements();

} // namespace ImpostorChunk

#endif // IMPOSTORCHUNK_H

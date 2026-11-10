/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef IMPOSTORCHUNK_H
#define IMPOSTORCHUNK_H

#include "impostorcard.h"
#include "features/far-lod/impostordraw.h"
#include "data/niftypes.h"	// Triangle (the card casters)

#include <QHash>
#include <QRect>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

struct LodiTreeCard;	// lodinative.h

class Scene;
class NifModel;

// OCTAHEDRAL CARDS INSIDE A NATIVE LOD CHUNK -- a feature master, shipped OFF.

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
	QString ref;					//!< the row's REFR (manifest column `ref`), hex
	/* lane PERFLOAD1 P7, the tree ladder (treeladder.h): bound radius (0 = not a handed tree), the ring step its mesh
	 * row draws (1 + MNAM slot; -1 = none: always the impostor), the REFR that row hides by */
	float ladderRadius = 0.0f;
	qint8 ladderMeshStep = -1;
	quint32 ladderRef = 0;
};

//! THE MASTER. False unless the person turned it on, or `WW_IMPOSTOR_CHUNK` is
//! set in the environment -- which is how the gate reaches it without touching
//! anyone's settings.
bool enabled();
void setEnabled( bool on );

//! Read a chunk's manifest and hold what it places, for the scene now open.
int arm( NifModel * nif, const QString & chunkPath );

//! Arm ONE set, at the origin, because the person opened its `.lodm` itself.
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

// the cell view's far tree cards A state of their own: `arm` runs after every load (the cell document too) and
// resets the chunk state, so the cards around a cell block live apart from it.
int armCellFar( NifModel * nif, const QStringList & chunkPaths, const float shift[3],
	float cx, float cy, float rMax, int hx0, int hy0, int hx1, int hy1,
	QSet<quint32> * refs, QStringList * notes, const QVector<QRect> * oursOnly = nullptr );
int drawCellFar( Scene * scene, const ImpostorDraw::Options & opt );
/*! lane PERFLOAD1 P1: the bake's card arrays (`<objectsDir>/<ws>.LodgenCards.*.lodm`, horizon ring sets left out) read
 *  layer by layer: form -> its layer into `layerOf`, each layer loaded once as a set of the far cards. After armCellFar. */
int cellFarTreeForms( const QString & objectsDir, const QString & ws, QHash<quint32, int> * layerOf, QStringList * notes );
//! the trees the object rings handed over (LodiTreeCard), placed as cards of their form's set; returns how many
int addCellFarTrees( const QVector<::LodiTreeCard> & trees );
//! the armed far cards by kind: octahedral (N x N views), horizon ring, flat (one view)
void cellFarCensus( int * octahedral, int * ring, int * flat );
void forgetCellFar();
int cellFarCount();

// the far cards as terrain-shadow casters, in the cell scene's frame (world minus the shift).
struct FarCasterPiece
{
	QVector<Vector3> verts;
	QVector<Triangle> tris;
	bool nearOut = false;	// SHAFTS1b: base not wholly inside keep: the near tier skips it, the far tiers cast it
};
// SHAFTS1b: keep (x0, y0, x1, y1) marks the pyramids whose base square is not inside it (nearOut, counted in *cut)
int cellFarCasters( QVector<FarCasterPiece> & out, float apexScale = 1.0f, const float * keep = nullptr, int * cut = nullptr );
quint64 cellFarGen();

/* the cells the stream holds loaded (cellfarhide.h); a far card placed in one does not draw nor cast.
 * The key is the card's placement cell, armCellFar's hole test. Empty = every card. */
inline qint64 cellFarCellKey( int x, int y ) { return ( qint64( x ) << 32 ) ^ qint64( quint32( y ) ); }
void cellFarHideSet( const QSet<qint64> & cells );
//! The armed cards placed in `cells` that still draw
int cellFarCardsShownIn( const QSet<qint64> & cells );

} // namespace ImpostorChunk

#endif // IMPOSTORCHUNK_H

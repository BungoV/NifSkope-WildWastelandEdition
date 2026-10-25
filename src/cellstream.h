#ifndef CELLSTREAM_H
#define CELLSTREAM_H

/* lane CELLSTREAM1: THE EXTERIOR CELL VIEW STREAMS ITS CELLS ROUND THE CAMERA.
 *
 * The open (nifCreateCellScene) builds the document, the far field and the lighting with no cell welded, and
 * hands the indexed world to the streamer here. The streamer keeps a 5x5 block of full cells centred on the
 * camera's cell: each cell is built on a worker thread (the cell builder, n = 1, stopping before the document),
 * then written into the document in slices of at most WW_CELL_STREAM_BUDGET ms (4) a frame, under its own NiNode
 * "cell x,y". A cell that leaves the block is unlinked and its blocks reset and kept for the next cell (block
 * numbers never move, so nothing else in the document is renumbered). The far LOD's ring-0 ground is one shape a
 * cell: it is shown where no cell is attached and hidden under one that is.
 *
 * The 5x5 centre moves when the camera is half a cell into the neighbour; the row that move will need is built
 * as soon as the camera enters the neighbour cell.
 *
 * Environment (harness pins, reds and dumps only):
 *   WW_CELL_STREAM=1|0             pin streaming on / off (unset: on, unless WW_CELL_OPEN asked a fixed block)
 *   WW_CELL_STREAM_BUDGET=<ms>     main-thread write slice a frame (default 4)
 *   WW_CELL_STREAM_SHIFT=<units>   how far into the neighbour cell the block moves (default 2048)
 *   WW_CELL_STREAM_PATH=<file>     camera steps: lines "ex ey ez ax ay az [fov]" (eye, look-at), each waits for idle
 *   WW_CELL_STREAM_ROAM=<cells>    how far from the open's centre the far ring 0 is built uncut (default 4)
 *   WW_CELL_STREAM_DUMP=<file>     one line a step: loaded vs expected, slice / frame ms, private bytes, coverage
 *   WW_CELL_STREAM_RED=nounload    cells that leave the block stay (the memory gate's red)
 *                     =nohandover  the far ring-0 ground stays under loaded cells (the far-LOD gate's red)
 */

#include "cellview.h"
#include "cellrefs.h"
#include "cellpick.h"
#include "data/niftypes.h"
#include "gl/celllights.h"
#include "gl/cellfxlit.h"

#include <QHash>
#include <QPersistentModelIndex>
#include <QString>
#include <QVector>

#include <array>
#include <memory>
#include <utility>
#include <vector>

class EsmWorld;
class NifModel;
class Scene;

//! What the open hands the streamer.
struct CellStreamSeed
{
	std::shared_ptr<EsmWorld> world;    //!< indexed once by the open; only the worker reads it afterwards
	CellSceneSpec spec;                 //!< resolved (the far stack, the mod pick); terrain/water/grid as asked
	QString dataRoot;
	Vector3 origin;                     //!< every shape's translation (the open's centre cell middle)
	int rootBlock = -1;
	int cx = 0, cy = 0;                 //!< the open's centre cell
	int roam = 4;                       //!< cells from the open's centre the far ring 0 covers uncut
	WwCellLighting base;                //!< the open's published lighting (no cell lights)
	std::vector<std::array<int, 3>> farHand;    //!< far ring-0 shapes: block, cell x, cell y
	int farLand = 0;                    //!< how many of them (the first) are terrain; the rest are water
};

//! The block numbers a streamed cell is written into: a freed one of the type when there is one, else new.
class CellBlockSource
{
public:
	virtual ~CellBlockSource() = default;
	virtual QModelIndex take( NifModel * nif, const QString & type ) = 0;
	//! The scene-node wiring still owed for this slice's blocks: the writer stops when it would overrun.
	virtual double wireOwedMs() const { return 0.0; }
	//! lane LIVESET3 (STREAMPSYS1): a block the writer appended itself (a copied particle branch) joins the cell
	virtual void adopt( NifModel * nif, int block ) { Q_UNUSED( nif ); Q_UNUSED( block ); }
	//! lane LIVESET3: particle models parsed once for the whole stream (null = no particles)
	virtual QHash<QString, std::shared_ptr<NifModel>> * pfxModels() { return nullptr; }
	//! lane LIVESET3: particle copies still allowed under WW_CELL_PARTICLES_MAX
	virtual int pfxRoom() const { return 0; }
};

struct CellChunkBuckets;    // cellview.cpp: the welded buckets, freed as they are written

//! One streamed cell: built on the worker, written on the main thread.
struct CellChunk
{
	std::shared_ptr<const CellStreamSeed> seed;
	int cx = 0, cy = 0;
	int facts[4] = { 0, 0, -1, -1 };    //!< the 5x5 whose enable facts apply (the fixed block's rule)
	// worker out
	bool ok = false;
	QString error;
	CellRefTable refs;
	CellPickTable picks;
	WwCellLighting lights;
	QVector<WwFxLitModel> fxModels;
	std::shared_ptr<CellChunkBuckets> buckets;
	qint64 buildMs = 0;
	// main-thread write state
	int next = 0;
	int fxBase = 0;
	bool started = false;
	QPersistentModelIndex iGround;
	std::vector<std::pair<int, int>> fxShapes;     //!< written block, local model serial
	//! lane LIVESET3 (STREAMPSYS1): placed models with particle systems (the open's filter), copied after the buckets
	struct Pfx
	{
		QString model;
		Vector3 at;
		Matrix rot;
		float scale = 1.0f;
	};
	std::vector<Pfx> pfx;
	bool pfxDone = false;
	int pfxCopies = 0;
	std::vector<std::pair<int, quint32>> xemiShapes;   //!< lane LIGHTS2 (XEMI1): written block, emittance colour
	qint64 shapes = 0, verts = 0, tris = 0;
};

//! Streaming applies to this open (WW_CELL_STREAM, WW_CELL_OPEN).
bool cellStreamWanted();
//! The open is done: start streaming round `seed` for this document.
void cellStreamStart( NifModel * nif, std::shared_ptr<CellStreamSeed> seed );
//! This document is about to be rebuilt or closed: drop its streamer (waits for its worker).
void cellStreamStop( NifModel * nif );
//! lane LIVESET3: a live set rebuilds the far LOD and the streamed cells after it: drop the streamer, its tables back
//! to the open's, and hand back its seed to start again
std::shared_ptr<CellStreamSeed> cellStreamSuspend( NifModel * nif );
//! One frame (GLView): follow the eye (world units). True when the document or the scene changed.
bool cellStreamTick( NifModel * nif, Scene * scene, const Vector3 & eye );
//! Cells queued, building or being written (a shot waits for false). Null = any document.
bool cellStreamBusy( const NifModel * nif );
//! lane CELLSTREAM1: this document streams its cells
bool cellStreamOn( const NifModel * nif );
//! lane CELLSTREAM1: seconds a file texture stays loaded unbound while streaming (WW_CELL_STREAM_TEXKEEP, 0 = kept)
int cellStreamTexKeepSec();
//! lane CELLSTREAM1: file textures the view freed after they went unused (counted in the stats)
void cellStreamCountEvicted( const NifModel * nif, int n );

//! What a dump line reports (null document = the first streamer).
struct CellStreamStats
{
	bool on = false;
	int cx = 0, cy = 0;                 //!< the block's centre cell
	QString expected, loaded;           //!< "x,y x,y ..." sorted
	bool match = false;
	int attached = 0, ready = 0, pending = 0, failed = 0, blocks = 0, pooled = 0;
	qint64 built = 0, attaches = 0, unloads = 0, inserted = 0, recycled = 0;
	double tickMaxMs = 0, sliceMaxMs = 0, unloadMaxMs = 0, drainMaxMs = 0, rebuildMaxMs = 0, buildMaxMs = 0;
	int persistent = 0, sceneNodes = 0, sceneProps = 0, sceneShapes = 0;   //!< lane CELLSTREAM1: leak watch
	qint64 texEvicted = 0;
	double buryMaxMs = 0;   //!< worst frame of freeing a left cell's scene nodes
	qint64 quietResets = 0, loudResets = 0;   //!< freed blocks reset without / with row signals
	QString persistentTop;              //!< the block types holding the most persistent indexes
	QString lastError;
};
CellStreamStats cellStreamStats( const NifModel * nif, bool resetMaxima );
/*! The far-LOD hand-over measured (gate c): straight down onto the ground on an 8-unit grid either side of
 *  every border between an attached cell and one that is not; a point no ground covers is a hole, a point both
 *  the near ground and a drawn far ring-0 tile cover is a double. Returns the summary line. */
QString cellStreamCoverage( const NifModel * nif, int * holes, int * doubles, int * samples );
//! The process's private bytes (Windows; 0 elsewhere).
qint64 cellStreamPrivateBytes();
//! lane CELLSTREAM1: the heaps' bytes in use and held free (a walk of every heap; slow, for the dump only)
void cellStreamHeapBytes( qint64 * busy, qint64 * held );
//! lane CELLSTREAM1: committed private memory by region size: [0] under 1 MB, [1] 1-64 MB, [2] 64 MB and up
void cellStreamRegionBytes( qint64 out[3] );
//! WW_CELL_STREAM_TRACE=1: one stderr line
void cellStreamTrace( const QString & line );

// cellview.cpp
//! Worker thread: the cell builder for one cell, stopping before the document.
void cellChunkBuild( CellChunk & c );
//! Main thread, resumable: writes buckets under `iCell` until done (true) or `budgetMs` is spent (false).
bool cellChunkEmit( NifModel * nif, CellChunk & c, CellBlockSource & src, const QModelIndex & iCell,
	double budgetMs, QString * error );

#endif // CELLSTREAM_H

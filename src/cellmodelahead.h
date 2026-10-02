#ifndef CELLMODELAHEAD_H
#define CELLMODELAHEAD_H

/* Lane SPEED1 (2026-10-02): THE CELL VIEW'S MODELS, READ AND PARSED AHEAD ON WORKER THREADS.
 *
 * The cell builder loads each distinct model (and material swap) once, the first time a placement needs it,
 * on the drawing thread: 3.0-4.6 s of a cell (measured, three cells). The loads do not depend on each other,
 * so before the builder's loop every model it will ask for is loaded by the generator's own fan-out
 * (lodgenParallelFor); the loop then takes each answer where it used to call the loader.
 *
 * SAFE BY CONSTRUCTION: an answer is kept under the loader's own inputs (the model as named, the swap rows as
 * given), so the loop gets exactly what its own call would have returned or, for anything not read ahead, makes
 * that call as before. What is wanted here and what the loop asks for may drift apart; the cost is a wasted or
 * a late load, never a different one. Nothing depends on which worker finishes first.
 *
 * Each worker takes its document items from a run of its own (data/nifitemcache.h); without that the workers
 * queue on the item pool's one mutex. Nothing is kept after the cell is built (the loader's per-thread model
 * cache is not used). Off under WW_CELL_SPEED_RED=slow and =nomodels. */

#include "lodgen.h"
#include "nativeemit.h"

#include <QHash>
#include <QString>

#include <vector>

class EsmWorld;

//! lodgen.cpp: the placed load (root transform left out) with nothing kept in the loader's cache.
bool lodgenNativeLoadModelPlacedOnce( void * user, const QString & model, const LodgenMaterialSubst * swap,
	std::vector<NativeSrcShape> * out );

class CellModelAhead
{
public:
	CellModelAhead( const QString & dataRoot, bool on );
	~CellModelAhead();
	//! One placement: its base's model with its material swap, as the cell builder will ask for it.
	void want( const EsmWorld & world, quint32 base, quint32 swap );
	//! Load everything wanted; returns when all of it is in.
	void load();
	//! The loader's answer for (model, swap rows) if it was read ahead: `out` and `ok` as the loader gives them.
	bool take( const QString & model, const LodgenMaterialSubst * subst, std::vector<NativeSrcShape> * out, bool * ok );

private:
	struct Job
	{
		QString model;
		LodgenMaterialSubst subst;
		std::vector<NativeSrcShape> shapes;
		bool ok = false, done = false, taken = false;
	};
	QString dataRoot_;
	bool on_;
	std::vector<Job> jobs_;
	QHash<QString, int> byKey_;
	QHash<quint32, LodgenMaterialSubst> swaps_;
	int taken_ = 0;
};

#endif

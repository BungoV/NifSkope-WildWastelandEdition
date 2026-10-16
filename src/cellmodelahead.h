#ifndef CELLMODELAHEAD_H
#define CELLMODELAHEAD_H

/* The cell view's models, loaded and parsed ahead on worker threads (lodgenParallelFor). Answers are keyed on
 * the loader's own inputs (model as named, swap rows as given), so a miss only costs a late load, never a
 * different one. Off under WW_CELL_SPEED_RED=slow and =nomodels. */

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

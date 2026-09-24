/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef NIFPARSESTRESS_H
#define NIFPARSESTRESS_H

#include <QString>
#include <QStringList>

class QTextStream;

/*! THE MODEL LAYER ON N THREADS, WITH NOTHING ELSE IN THE PICTURE.
 *
 *  Lane NIFPARSE1, 2026-09-11. BAKEPERF1 found a `-no-gui lodgen` bake at
 *  sixteen chunk threads faulting inside `NifItem::deleteChildItems()` under
 *  `BaseModel::~BaseModel()`, with every other worker in the same parser, and
 *  contained it with a mutex around the whole life of each temporary document.
 *  A whole-bake crash is a poor instrument: a chunk pass is the parser AND the
 *  plugin reader AND the texture cache AND the archive layer AND the message
 *  sink, so "the parser is not thread-safe" was a conclusion drawn from a
 *  stack, not from an experiment that could separate them.
 *
 *  This is that experiment. It reads the files ONCE on the calling thread into
 *  memory, then builds and destroys `NifModel`s from those bytes on N threads.
 *  No `EsmWorld`, no texture cache, no archive lookup, no road gatherer, no
 *  file I/O inside the threaded part at all. What survives or faults here is
 *  the model layer and only the model layer, which is the discriminator the
 *  bake cannot give.
 *
 *  IT IS NOT A CRASH DETECTOR ONLY. Every worker digests what it actually
 *  READ back out of the document -- block names, counts, array sizes, a sample
 *  of field values -- and every digest must equal the single-threaded
 *  reference digest for the same file. Silent corruption that does not happen
 *  to fault still fails. (CONSTITUTION 4: an invariant that fails on broken
 *  code, and a floor that shows it failing.)
 */
struct NifParseStressOptions
{
	QStringList paths;      //!< the .nif files to hammer; read once, up front
	int threads = 16;       //!< worker threads in the parallel pass
	int reps = 8;           //!< times each worker walks the whole file list
	/*! THE FLOOR. Without one, a harness that cannot go red is not a check.
	 *  "digest" flips one byte of one file's bytes for one worker: the digest
	 *  must come back different, which is what proves the digest reaches the
	 *  parse at all rather than hashing a constant.
	 *  "share" puts every worker on ONE shared `NifModel`, which is the thing
	 *  the model layer is not allowed to survive -- it is expected to fault or
	 *  mismatch, and it is the floor for "N threads clean" meaning anything. */
	QString sabotage;       //!< "", "digest" or "share"
	bool verbose = false;
};

/*! Run it. Returns the number of FAILURES (0 = pass); the stream gets the
 *  table, the counts and a final `PASS` / `FAIL` line. A worker that faults
 *  takes the process with it -- that is the point, and the exit code says so. */
int nifParseStressRun( const NifParseStressOptions & opts, QTextStream & out );

#endif // NIFPARSESTRESS_H

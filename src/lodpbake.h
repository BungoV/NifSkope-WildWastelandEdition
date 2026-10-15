/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef LODPBAKE_H
#define LODPBAKE_H

#include <QString>
#include <QStringList>

/*! LOD PRTP step 1 (lane L1, 2026-10-15): place the `.lodp` sector files
 *  for a whole native bake. Reads the pair `<ws>.lodo` + `<ws>.lodi` and the
 *  ground from `<ws>.lodl`, places every patch (src/lodpplace.h) and writes
 *  `lodp_X_Y.lodp` into `outDir`. CLI: `lodgen --lodp <bake dir> <out dir>`.
 *
 *  A placement draws the base's FIRST PRESENT rep (the same mesh the native
 *  viewer draws by default); Initially Disabled placements (near library
 *  only) are skipped and counted. When the `.lodo` carries the v8 chart
 *  stream it is recomputed here and must agree byte for byte; the files are
 *  then marked BOUND. A `.lodo` without the stream is still placed (a census
 *  run) and the files say so by leaving BOUND clear. */
struct LodpBakeOptions
{
	QString lodoPath, lodiPath, lodlPath;   //!< lodlPath may be empty: objects only
	QString outDir;
	bool region = false;                    //!< limit to an inclusive cell rectangle
	int x0 = 0, y0 = 0, x1 = -1, y1 = -1;
	bool writeFiles = true;                 //!< false = census only (sizes measured, nothing written)
};

bool lodpBake( const LodpBakeOptions & o, QStringList * report, QString * error );

/*! Reads every `lodp_*.lodp` in `dir` with every check on and prints one
 *  line a file plus a total. False on the first refused file. */
bool lodpVerifyDir( const QString & dir, QStringList * report, QString * error );

#endif

#ifndef PGIWRITE_H
#define PGIWRITE_H

/*! Lane PGI0 (2026-10-13; notes/pgi1/PLAN.md section 5): the `.pgi` ("Precomputed GI") writer.
 *
 *  The file is the near bake's `.tbk` v5 sector content re-packed into the chunks of src/pgi_format.h (the
 *  header FO4CS includes unchanged), plus the HORZ chunk. No new tracing: the writer reads the sector files
 *  the bake has just written and nothing else. Lossless: a reader rebuilds every sector file bit for bit
 *  (tests/spells/pgi_read.py, gate F1).
 *
 *  HORZ in this lane holds the surfel's SELF-PLANE horizon (source 1): per azimuth, the elevation below which
 *  the surfel's own plane hides the sky, and openness 255. Lane PGI1 replaces it with the traced horizon
 *  (source 2) in the same records.
 *
 *  Exterior: one file per sector, `<X>_<Y>.pgi`. Interior: one file, `<Cell EditorID>.pgi`, one group per sector. */

#include <QString>
#include <QStringList>

struct PgiWriteSpec
{
	bool interior = false;   //!< kind 2 (one file for every sector) instead of kind 1 (a file a sector)
	QString world;           //!< worldspace EditorID (header worldHash); exterior only
	QString interiorName;    //!< interior: the file's name (the cell's EditorID); empty = "interior"
	quint32 interiorForm = 0;
	QByteArray options;      //!< hashed into the header's optionsHash (the bake's options, any stable text)
	/*! Deliberate defects for the gates' refuters (empty in every real run): "shuffle" turns every link's
	 *  side to the next bin (the round trip must fail), "horz8" writes 8 azimuths (the horizon gate must fail). */
	QString red;
};

struct PgiWriteResult
{
	QStringList files;
	int sectors = 0;
	qint64 surfels = 0, probes = 0, links = 0, rooms = 0, emits = 0, bytes = 0;
	QString error;
};

//! Write `.pgi` files into `outDir` from the `.tbk` v5 sector files `tbkFiles`. False on a refused input.
bool pgiWriteFromTbk( const QStringList & tbkFiles, const PgiWriteSpec & spec, const QString & outDir, PgiWriteResult * out );
QString pgiWriteCensusText( const PgiWriteResult & r );

/*! `nifskope -no-gui pgiwrite --tbk <dir> --out <dir> [--interior <name>] [--world <EditorID>] [--red shuffle|horz8]`
 *  `nifskope -no-gui pgicheck <file.pgi>... [--red acceptall]`: the shared header's checkFile + a line per chunk. */
int pgiWriteCli( const QStringList & args );
int pgiCheckCli( const QStringList & args );

#endif // PGIWRITE_H

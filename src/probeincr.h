#ifndef PROBEINCR_H
#define PROBEINCR_H

/*! INCREMENTAL PROBE BAKE (lane BAKEINCR1, 2026-10-04).
 *
 *  An exterior probe bake writes, next to its sector `.tbk` files, one text record
 *  (`bakeincr.txt`) of everything the bake read:
 *    identity  the running exe's sha1 + the WW_* knobs that can move a bake byte
 *    records   the ESM views the bake reads over its 5x5 block, LAND one cell wider:
 *              references (form, base, position, rotation, scale, flags, enable state,
 *              swaps, primitives), their bases, static-collection parts, material swaps,
 *              cells, water, land heights/colours/splat, landscape texture sets
 *    far       the sha1 of the far-soup triangles the block's ring reads from the LOD files
 *    asset     every file the bake read through the generator's asset reader (models,
 *              materials, textures), path + sha1 of the bytes ("missing" when absent)
 *    out       each `.tbk` it wrote, sha1
 *
 *  A check (WW_CELL_BAKEINCR_CHECK, an env mode of the GUI exe that runs before any
 *  window, so assets resolve through the very same archives and stack the bake used)
 *  recomputes every line and calls the cell DIRTY when any moved. THE NEIGHBOUR RULE
 *  needs no widening pass: a cell's records digest covers its own 5x5 block, so an
 *  edit in cell C moves the digest of exactly the 25 cells whose block holds C.
 *  Lights and time of day are not in the record (relight is a runtime pass): a LIGH
 *  record's colour, radius, fade or a light-only REFR field dirties nothing.
 *
 *  It can only over-rebake: a field the bake might read is fed; a field the bake
 *  provably never reads (light-only data) is the only thing left out.
 *
 *  Red controls (gate only, read by the CHECK, so one honest baseline serves both),
 *  WW_CELL_BAKEINCR_RED=
 *    nonbr     the check compares the asked cell's own records alone (the record's
 *              `cellrecords` line, no 5x5): an edit next door leaves the cell "clean"
 *    nomodels  the check skips the `.nif` asset lines: a changed model goes unseen
 */

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

class EsmWorld;

namespace ProbeIncr
{
//! the record file each baked cell folder holds
inline const char * recordName() { return "bakeincr.txt"; }

//! lane BAKEINCR1's red control, "" in every real run
QByteArray red();

// ---- the asset recorder: lodgenReadAsset calls note() for every read while it is on
void startRecording();
void stopRecording();
bool recording();
//! one read: the data root it was asked under, the path, archive folder and extension, and what came back
void note( const QString & root, const QString & relPath, const char * folder, const char * ext, bool ok,
	const QByteArray & bytes );

struct Asset
{
	QString root, folder, ext, relPath, digest;
};
//! what was read since startRecording(), sorted by folder|path, one row per file
QVector<Asset> recorded();

//! the knobs that are part of a bake's identity, "NAME=VALUE", sorted
QStringList identityKnobs();
//! sha1 over the exe's bytes, the knobs and the world name
QString identity( const QString & world );

//! the ESM views a bake of cell (cx, cy) with a (2 half + 1)^2 block reads
QString recordsDigest( const EsmWorld & world, int cx, int cy, int half );

/*! lane FARLIGHTMAP1: the placed lights the block's relight reads (each LIGH ref's fields, its base's light data,
 *  its box, its enable chain). Light moves no .tbk byte, so only a far-light bake writes it as the `lights` line. */
QString lightsDigest( const EsmWorld & world, int cx, int cy, int half, int * count = nullptr );

//! write `<dir>/bakeincr.txt` after a successful bake of (cx, cy); the outputs are the dir's *.tbk
bool writeRecord( const QString & dir, const QString & world, int cx, int cy, int half,
	const QString & records, const QString & cellRecords, const QString & far, QString * error,
	const QString & lights = QString() );

//! the WW_CELL_BAKEINCR_CHECK env mode: returns the process exit code
int checkMain();
}

#endif

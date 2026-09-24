#ifndef LODBFILE_H
#define LODBFILE_H

#include <QString>
#include <QStringList>
#include <QVector>


/*! THE BAKE RECORD `.lodb` -- lane BAKEREC1, 2026-09-17.
 *
 * bungo, 2026-09-16 19:1x: *"Should we also save a file for each bake, or a
 * list of plugins that were used for it?"*, then *"So yeah, each bake needs to
 * know plugins used or what's different, to even attempt a partial rebake"*,
 * then *"lodb sounds good"*.
 *
 * THIS FILE IS NOT A NEW MECHANISM. `.lodb` already existed -- lane INCR1 wrote
 * it on 2026-09-12 as "the bake ledger", a 16-byte header and compact JSON at
 * `<out-dir>/<ws>.lodb`, carrying a per-chunk INPUT digest and the per-file
 * OUTPUT digests that `--incremental` reads. Lane LAYOUT1 then ruled, in
 * `docs/LODGEN_LEDGER_FORMAT.md` section 1 and in `src/lodgenlayout.h`, that its
 * new home is `FO4CSLOD/<ws>/<ws>.lodb` and that it belongs to this lane.
 *
 * So the bake RECORD and the bake LEDGER are ONE FILE. Version 2 keeps
 * everything version 1 had -- the shape, the switch digest, the load-order
 * hash, every chunk's input digest and every output's sha1 -- and adds what the
 * record needs on top: the exe that baked it, the five corpus hashes the
 * `.lodo`/`.lodi` pair carries, one line per PLUGIN with its own byte hash, one
 * line per resource, the argument vector verbatim, every census line the bake
 * printed, and an `end` line counted off the disk. Writing a second file with a
 * second per-chunk hash would have given INCR1 two rules to obey.
 *
 * THE CONTAINER IS PLAIN TEXT: UTF-8, LF, one record a line, `key<TAB>fields`.
 * A reader that meets an unknown line KIND ignores that line, so a later lane
 * can add one without breaking this one; a reader that meets an unknown
 * `version` REFUSES BY NAME, because a changed meaning cannot be guessed at.
 * The v1 binary container is recognised by its 'LODB' magic and refused by name
 * rather than mis-parsed.
 *
 * IT IS DETERMINISTIC EXCEPT WHERE IT SAYS IT IS NOT. Version 1's law -- two
 * full bakes of the same tree write byte-identical ledgers, so a whole-
 * directory comparison can include the ledger instead of excepting it -- still
 * holds for every line but five, and those five are named here and nowhere
 * else:
 *
 *   `baked`                     the bake's wall clock
 *   the `path` field of `plugin`  an absolute path, informational, never hashed
 *   the whole `resource` line     absolute paths and archive mtimes
 *   `census<TAB>stage times: ...`  a WALL CLOCK -- two bakes of one tree
 *                                 differ by tenths of a second. Found by
 *                                 gate (h) measuring, not by thinking.
 *   the `peak working set: ...`   THIS MACHINE AT THIS MOMENT, not the
 *   clause of the `census` line    inputs. The only one that is INLINE:
 *   that begins `bake census:`     the rest of that line is content, so
 *                                 exactly the clause up to the next comma
 *                                 is masked. Handed over by ARCHLOCK1.
 *
 * `lodbNormalise()` strips exactly those, and the gates compare the normalised
 * form. Anything else differing between two bakes of one tree is a defect.
 *
 * The format, the chunk-hash recipe and the refusal words are
 * `docs/LODGEN_BAKE_RECORD.md`.
 */


//! One plugin of the load order, as the record carries it.
struct LodbPlugin
{
	int     index = 0;      //!< position in the load order the bake was given
	QString name;           //!< lower-cased BASE FILE NAME -- what loadOrderHash folds
	qint64  bytes = 0;      //!< the file's byte size -- what loadOrderHash folds
	quint64 hash  = 0;      //!< FNV-1a 64 over the file's BYTES: what loadOrderHash cannot see
	QString path;           //!< the full path, INFORMATIONAL, never part of any hash
};

//! One entry of the resource stack (`--resource`, `--mo2`, the panel's list).
struct LodbResource
{
	QString kind;           //!< `folder` / `ba2` / `bsa`
	QString path;           //!< as the stack spelled it
	qint64  bytes = 0;      //!< an archive's size; 0 for a folder
	QString mtimeIso;       //!< an archive's mtime, ISO-8601 UTC; empty for a folder
};

/*! FNV-1a 64 over a file's bytes, streamed. Returns false when the file cannot
 *  be read, so a missing plugin is a refusal and never a zero that looks like
 *  an answer. The same constants as `EsmWorld::loadOrderHash` and as
 *  `tests/spells/lodgen_native_decode.py`. */
bool lodbFileFnv1a64( const QString & path, quint64 * out );

/*! Every census line the bake printed, in the order it printed them.
 *
 *  The recorder SNIFFS: `lodbNoteCensus` keeps a line only when its first word
 *  is one of the keywords `docs/LODGEN_CENSUS.md` section 6.1 registers, so a
 *  print site that is not a census line costs nothing and a new census line is
 *  picked up by registering its keyword in one place. The completeness FLOOR is
 *  in the gate, not here: `tests/spells/lodgen_bakerec.sh` leg (a) greps the
 *  bake's own log for the same keywords and requires the two sets to be equal,
 *  so a missed print site fails loudly instead of quietly shipping a short
 *  record. A block may carry several lines; each is sniffed on its own. */
void        lodbClearCensus();
void        lodbNoteCensus( const QString & block );
QStringList lodbCensusLines();
//! True when `line`'s first word is a registered census keyword. Public so the gate's floor can use it.
bool        lodbIsCensusLine( const QString & line );

/*! The exe that baked the record: `WW_EDITION_VERSION+<build rev>`, composed
 *  the same way `src/main.cpp` composes the window title (build_rev.txt beside
 *  the exe, falling back to the compiled-in NIFSKOPE_REVISION), and the exe's
 *  own byte size. Both read at the moment of writing, never cached. */
QString lodbExeStamp();
qint64  lodbExeSize();

/*! Strip the FIVE lines/fields the record declares VOLATILE (see the block
 *  comment). Used by the gates; a caller that wants the file verbatim reads the
 *  file. */
QStringList lodbNormalise( const QStringList & lines );

/*! Name what moved between the plugin list a record was baked from and the list
 *  in hand: one sentence per plugin, `ADDED` / `REMOVED` / `REORDERED` /
 *  `EDITED` / `RESIZED`, with the index and the file named. Empty when the two
 *  lists agree, which is the only answer that means "nothing moved".
 *
 *  `nowPaths` is the comma-joined list `EsmWorld::pluginList()` hands back --
 *  the same list `loadOrderHash()` walks -- so the diff and the hash can never
 *  disagree about what the load order is. */
QStringList lodbDiffPlugins( const QVector<LodbPlugin> & recorded, const QString & nowPaths );

/*! Read a record's plugin lines only, cheaply, for the refusal path. Returns
 *  false and fills `error` when there is no readable record at `path`. */
bool lodbReadPlugins( const QString & path, QVector<LodbPlugin> * out, QString * error );

/*! The record that sits beside a `.lodo`/`.lodi` pair: `<dir>/<ws>.lodb`, with
 *  `<ws>` taken from the pair's own file name. Empty when none is there. */
QString lodbPathBeside( const QString & lodoPath );

#endif // LODBFILE_H

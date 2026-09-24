#ifndef NIFXFILE_H
#define NIFXFILE_H

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>


/*! The `.nifx` sidecar, JSON generation (version 1) -- lane PBRR1.
 *
 * Contract: docs/NIFSKOPE_PBR_RENDERER.md s2.4 + FO4CS src/NifSidecar/
 * NifSidecar.h. `<nifstem>.nifx` sits BESIDE its .nif (same folder, same stem;
 * ruled 2026-09-23, no other search path). A JSON object with a required
 * `"version": 1`; every other top-level key is a section. NifSkope consumes
 * the `material` section:
 *
 *     "material": { "<geometry node name>": { "pbrm": "Materials\\x\\y.pbrm" } }
 *
 * Node keys match case-insensitively (the engine's BSFixedString rule). Limits
 * as FO4CS: 1 MiB, 64 sections, 4096 nodes. A version other than 1 is refused;
 * a file whose first non-space byte is not `{` is a later (binary) generation
 * and is refused too.
 *
 * SPAN-PRESERVING. The document keeps the file's bytes and the byte spans of
 * every top-level member and every `material` entry; an edit is a splice of
 * the affected span followed by a reparse. So unknown sections, unknown keys
 * inside an entry, key order, whitespace and number spellings survive a load
 * and save byte for byte, and an edit changes only the bytes it names.
 */
struct NifxDocument
{
	struct Span
	{
		int begin = -1;	//!< byte offset, inclusive
		int end = -1;	//!< byte offset, exclusive
		bool valid() const { return begin >= 0 && end >= begin; }
	};

	struct Member
	{
		QString key;		//!< decoded
		Span keySpan;		//!< the quoted key token
		Span valueSpan;		//!< the value token
	};

	struct MaterialEntry
	{
		QString node;			//!< decoded key, as authored
		QString pbrm;			//!< the `pbrm` string, as authored (empty = missing)
		bool valid = false;		//!< an object with a string `pbrm`
		QString problem;		//!< why not valid
		Span keySpan;
		Span valueSpan;			//!< the entry object
		Span pbrmSpan;			//!< the `pbrm` value token (invalid when missing)
	};

	QByteArray bytes;			//!< the document as read or as last edited
	bool ok = false;
	QString error;				//!< empty when ok
	int version = 0;
	QList<Member> members;		//!< top level, file order (includes "version")
	QStringList unknownSections;	//!< sections NifSkope does not consume, by name
	bool hasMaterial = false;
	Span materialSpan;			//!< the `material` object
	QList<MaterialEntry> material;	//!< file order
	QStringList warnings;

	//! The first entry whose node matches case-insensitively, or nullptr.
	const MaterialEntry * findMaterial( const QString & node ) const;
};

inline constexpr int NIFX_MAX_BYTES = 1 << 20;
inline constexpr int NIFX_MAX_SECTIONS = 64;
inline constexpr int NIFX_MAX_NODES = 4096;

NifxDocument nifxParse( const QByteArray & bytes );

//! Read and parse; a missing file is ok=false with error "absent".
NifxDocument nifxParseFile( const QString & path );

//! The document's bytes. With no edits this is exactly what was read.
inline QByteArray nifxSerialize( const NifxDocument & d ) { return d.bytes; }

/*! Set `node`'s `pbrm` link. An existing entry (case-insensitive match) has
 * only its `pbrm` value token replaced -- or the member appended inside the
 * entry when it has none; a new node is appended as the last member of
 * `material`; a missing `material` section is appended as the last top-level
 * member. The whitespace style of the neighbouring member is reused.
 */
bool nifxSetMaterial( NifxDocument & d, const QString & node, const QString & pbrm, QString * why = nullptr );

//! Remove `node`'s entry (and its separating comma). False when absent.
bool nifxRemoveMaterial( NifxDocument & d, const QString & node, QString * why = nullptr );

//! `<dir>/<stem>.nifx` for `<dir>/<stem>.nif`; empty for a non-.nif path.
QString nifxPathForNif( const QString & nifPath );

/*! The viewport's cached lookup: the `material` entry for `node` in the .nifx
 * beside `nifPath` (cached by path and modification time). Returns the raw
 * `pbrm` string, or empty with `note` saying why (no file, parse error,
 * no entry, entry invalid). `note` is empty when the NIF simply has no .nifx.
 */
QString nifxMaterialFor( const QString & nifPath, const QString & node, QString * note = nullptr );

#endif // NIFXFILE_H

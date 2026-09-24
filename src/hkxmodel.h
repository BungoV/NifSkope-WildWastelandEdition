/* The Blocks-tab model for a Havok packfile (.hkx) -- lane HKXEDIT1, 2026-09-10.
   bungo, verbatim: "Just make hkx fully editable in our nifskope".

   HkxModel is a BaseModel, the same base as NifModel and KfmModel, so the
   block list, the block details tree, the NifDelegate editors (ValueEdit,
   enum combo, flag check list) and the row views work on it unchanged --
   exactly the KfmModel idiom, which is how a second document species has
   always been bound into the window (NifSkope::load's ".kfm" branch, the
   kfmtree view). Every object of the packfile is a top-level item (a "block"),
   every serialised member of its class a child item, arrays as array items
   with one child per element, inline structs as compounds. Editing a Value
   cell goes through BaseModel::setData; HkxModel wraps it in an undo command
   on its own QUndoStack (`undoStack`, the NifModel spelling, so the window's
   Undo / Redo / dirty-title plumbing binds the same way).

   The layout store is the Hkx::File the file was read into: object order,
   class-name order, header bytes, array capacities, struct holes. Saving
   syncs every item's value back into that File and writes it through
   Hkx::File::write -- so an UNMODIFIED document saves byte-identically, and
   a modified one differs only where the edit lands (gate (b)).

   The animation workspace reads a clip from this model by DECODING ITS BYTES:
   animFile() writes the File and hands the bytes to hkxAnimLoadPackfile(),
   the same reader HkxPlayback::load uses on a disk file. No second decoder,
   no clip type exposed by the model. */
#ifndef HKXMODEL_H
#define HKXMODEL_H

#include "model/basemodel.h"
#include "hkxfile.h"
#include "hkxanim.h"

#include <QUndoStack>

class HkxModel final : public BaseModel
{
	Q_OBJECT

public:
	HkxModel( QObject * parent = nullptr );
	~HkxModel();

	void clear() override final;
	bool load( QIODevice & device, const char * fileName = nullptr ) override final;
	bool save( QIODevice & device ) const override final;
	QString getVersion() const override final { return QStringLiteral( "hk_2014.1.0-r1" ); }
	quint32 getVersionNumber() const override final { return 0x20140100; }

	QVariant data( const QModelIndex & index, int role = Qt::DisplayRole ) const override final;
	bool setData( const QModelIndex & index, const QVariant & value, int role = Qt::EditRole ) override final;
	Qt::ItemFlags flags( const QModelIndex & index ) const override final;

	//! The load refusal (a sentence naming field and value), empty after a good load.
	QString loadError() const { return lastError; }
	//! The class database this model reads with (null with the reason in loadError() when none loads).
	const Hkx::ClassDb * classDb() const { return db; }
	//! Use a specific class database (the harness; the default is Hkx::ClassDb::instance()).
	void setClassDb( const Hkx::ClassDb * d ) { db = d; }

	//! Objects (blocks) as top-level items.
	int objectCount() const { return root->childCount(); }
	QModelIndex objectIndex( int i ) const { return index( i, 0 ); }
	//! The object index of an item's block, or -1.
	int objectOf( const QModelIndex & index ) const;
	//! The class name of block i.
	QString objectClass( int i ) const;

	//! The document's bytes as they would be saved (items synced into the File first).
	QByteArray toBytes() const;
	//! The Hkx::File after a sync: the layout store plus every edited value.
	const Hkx::File & file() const;
	//! The animation reader's view of this document: a decode of toBytes().
	HkxAnimFile animFile() const;

	//! Change an array item's element count: inserts default elements or removes the tail (undoable).
	bool setArraySize( const QModelIndex & array, int newCount );
	//! Append a new block of the named class (undoable through the stack as one command).
	int addObject( const QString & className, QString * error = nullptr );

	//! true when something was edited since load / save (the undo stack is not clean)
	bool isDirty() const { return undoStack && !undoStack->isClean(); }

	QUndoStack * undoStack = nullptr;

protected:
	bool updateArraySizeImpl( NifItem * array ) override final;
	bool setHeaderString( const QString &, uint ver = 0 ) override final;
	bool evalVersionImpl( const NifItem * item ) const override final;
	QString ver2str( quint32 v ) const override final;
	quint32 str2ver( QString s ) const override final;

private:
	friend class HkxSetValueCommand;
	friend class HkxArrayResizeCommand;

	//! setData without the undo wrapper (the commands call this)
	bool setDataDirect( const QModelIndex & index, const QVariant & value );
	//! resize without the undo wrapper
	bool resizeDirect( const QModelIndex & array, int newCount );

	void buildItems();
	NifItem * buildValue( NifItem * parent, const QString & name, const Hkx::Value & v, const Hkx::MemberDef * m, int at = -1 );
	NifItem * buildElement( NifItem * parent, const QString & name, const Hkx::Value & v, const Hkx::MemberDef * arrayMember, int at = -1 );
	NifItem * buildPlainElement( NifItem * parent, const QString & name, const Hkx::Value & array, int i, int at = -1 );
	void syncFromItems() const;
	void syncValue( Hkx::Value & v, const NifItem * item ) const;
	void syncElement( Hkx::Value & v, const NifItem * item, const Hkx::MemberDef * arrayMember ) const;
	void syncPlainElement( Hkx::Value & array, int i, const NifItem * item ) const;
	static void registerEnums( const Hkx::ClassDb & db );

	const Hkx::ClassDb * db = nullptr;
	mutable Hkx::File hkfile;
	QString lastError;
	bool inCommand = false;
};

#endif // HKXMODEL_H

/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef NIFSNAPSHOT_H
#define NIFSNAPSHOT_H

#include "model/nifmodel.h"

#include <QBuffer>
#include <QByteArray>
#include <QUndoCommand>
#include <QUndoStack>

#include <functional>

//! @file nifsnapshot.h Whole-model snapshot undo for structural edits

//! Undo command that restores a serialized copy of the whole model.
//! Used for structural operations (array resizes, block insertion/removal)
//! that the value-level ChangeValueCommand cannot represent.
class NifSnapshotCommand final : public QUndoCommand
{
public:
	NifSnapshotCommand( NifModel * model, const QByteArray & before, const QByteArray & after, const QString & text )
		: QUndoCommand(), nif( model ), dataBefore( before ), dataAfter( after )
	{
		setText( text );
	}

	void redo() override
	{
		if ( firstRedo ) {
			// the operation itself already ran before the command was pushed
			firstRedo = false;
			return;
		}
		restore( dataAfter );
	}

	void undo() override
	{
		restore( dataBefore );
	}

	//! Refresh the redo snapshot after a lightweight live preview has edited the
	//! already-created result in place.  The original undo snapshot is retained.
	void setAfterSnapshot( const QByteArray & after )
	{
		dataAfter = after;
	}

private:
	void restore( const QByteArray & data )
	{
		QBuffer buf;
		buf.setData( data );
		if ( buf.open( QIODevice::ReadOnly ) )
			nif->load( buf );
	}

	NifModel * nif;
	QByteArray dataBefore, dataAfter;
	bool firstRedo = true;
};

//! Run a structural operation and push a single snapshot undo step for it
inline bool nifSnapshotOp( NifModel * nif, const QString & description, const std::function<void()> & op )
{
	if ( !nif )
		return false;

	QByteArray before;
	{
		QBuffer buf( &before );
		buf.open( QIODevice::WriteOnly );
		if ( !nif->save( buf ) )
			return false;
	}

	op();

	QByteArray after;
	{
		QBuffer buf( &after );
		buf.open( QIODevice::WriteOnly );
		if ( !nif->save( buf ) )
			return false;
	}

	if ( nif->undoStack )
		nif->undoStack->push( new NifSnapshotCommand( nif, before, after, description ) );

	return true;
}

#endif

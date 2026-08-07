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
		: QUndoCommand(), nif( model ), dataBefore( before ), dataAfter( after ), haveAfter( true )
	{
		setText( text );
	}

	/*! The same, without the redo snapshot — taken at the first undo instead.
	 *
	 *  HALF THE COST OF EVERY STRUCTURAL EDIT, and nothing given up for it.
	 *  Saving the whole model twice per operation measured 88 ms on a 512-block
	 *  file and 160 ms on 2012, and every array resize, insertion and removal in
	 *  the program pays it — while the "after" copy is only ever read by a REDO,
	 *  which needs an undo in front of it.
	 *
	 *  At that moment the model already holds exactly that state. The undo stack
	 *  is LIFO, so by the time this command is undone every command pushed after
	 *  it has been undone too, and what is in front of you IS this command's
	 *  result. So it is captured there: off the edit, onto a keystroke that was
	 *  going to rewrite the whole model anyway.
	 */
	NifSnapshotCommand( NifModel * model, const QByteArray & before, const QString & text )
		: QUndoCommand(), nif( model ), dataBefore( before )
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
		if ( !haveAfter && nif ) {
			QBuffer buf( &dataAfter );
			buf.open( QIODevice::WriteOnly );
			haveAfter = nif->save( buf );
		}
		restore( dataBefore );
	}

	//! Refresh the redo snapshot after a lightweight live preview has edited the
	//! already-created result in place.  The original undo snapshot is retained.
	void setAfterSnapshot( const QByteArray & after )
	{
		dataAfter = after;
		haveAfter = true;
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
	bool haveAfter = false;
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

	// no "after" pass: the command takes it at the first undo, when the model is
	// already holding exactly that state. See the constructor.
	if ( nif->undoStack )
		nif->undoStack->push( new NifSnapshotCommand( nif, before, description ) );

	return true;
}

#endif

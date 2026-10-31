#ifndef CELLTEXAHEAD_H
#define CELLTEXAHEAD_H

/* A cell's texture files, read ahead of the first frame by worker threads (bounded by AHEAD_BYTES); only the
 * upload needs the drawing thread. TexCache::texLoad takes the same bytes it would have read itself, so the
 * picture never depends on thread timing. Cell documents only; WW_CELL_SPEED_RED=slow or =notex turns it off. */

#include <QByteArray>
#include <QString>
#include <QStringList>

class NifModel;

namespace CellTexAhead
{

//! The cell builder has started filling this document: its materials' textures are read ahead from now.
void arm( const NifModel * nif );
//! Is this the document being read ahead? One atomic load: ask before gathering names.
bool armedFor( const NifModel * nif );
//! A shape of `nif` will bind these textures.
void want( const NifModel * nif, const QStringList & names );
//! The bytes of `filepath` as TexCache::texLoad is about to read it. False = read it yourself.
bool take( const NifModel * nif, const QString & filepath, QByteArray & data );
//! lane PERF1: the names whose bytes are read and waiting (at most `max`), for the drawing thread to upload now:
//! the workers stop at AHEAD_BYTES, and the drawing thread asks in another order than they read, so without this
//! it read ~40% of a cell's files itself, one at a time.
QStringList ready( const NifModel * nif, int max );
//! lane STREAM1: `name`'s file is loaded already (under another name): its waiting bytes are freed.
void drop( const NifModel * nif, const QString & name );
//! This document's resources are about to change or go: the workers stop first. Any thread.
void stop( const NifModel * nif );
//! lane GAPFIX1: the view freed `name`'s texture (the stream keep-timer): the next want() reads it ahead again
void forget( const NifModel * nif, const QString & name );
//! lane GAPFIX1: names read ahead again after a forget; handed-out names the drawing thread had to read itself
void counts( qint64 & again, qint64 & drawRead );

//! The end of a frame: one that asked for no texture ends the read-ahead.
void frameEnd();
//! One per GLView::paintGL: its end is the end of a frame.
struct Frame
{
	Frame() {}
	~Frame() { frameEnd(); }
};

} // namespace CellTexAhead

#endif

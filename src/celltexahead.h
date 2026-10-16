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
//! This document's resources are about to change or go: the workers stop first. Any thread.
void stop( const NifModel * nif );

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

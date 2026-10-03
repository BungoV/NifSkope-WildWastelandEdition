#ifndef CELLTEXAHEAD_H
#define CELLTEXAHEAD_H

/* Lane SPEED1 (2026-10-02): A CELL'S TEXTURE FILES, READ AHEAD OF THE FIRST FRAME.
 *
 * A cell's first frame loads several hundred textures one after another on the
 * drawing thread: find the file, take it out of its archive (inflate), hand it to
 * the graphics card. Measured, that was 9 to 23 s of a 25 to 49 s cell, on one core
 * of sixteen. Only the last step needs the drawing thread.
 *
 * So when a shape's shader property of the cell's document is resolved, the texture
 * files it will bind (BSShaderLightingProperty::fileName, the renderer's own question)
 * are queued here and worker threads read them, in that order, into memory. When
 * TexCache::texLoad comes for a file it takes the bytes if they are ready, waits if
 * a worker is on that very file, and reads the file itself otherwise -- the same
 * call on the same path, so the bytes are the same whichever thread read them and
 * the picture cannot depend on thread timing.
 *
 * Bounded: the bytes waiting never pass AHEAD_BYTES (the workers wait there; nothing
 * read is thrown away to make room, the drawing thread comes for it in its own order);
 * a frame that asked for nothing ends the read-ahead and frees what nobody took. When
 * the drawing thread has to read a file no worker has started, the workers go on from
 * that file, not from where they were. Only a cell document is armed
 * (cellmesh.cpp); every other document loads its textures as it always did.
 *
 * WW_CELL_SPEED_RED=slow or =notex turns it off (tests/spells/cell_speed.sh). */

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

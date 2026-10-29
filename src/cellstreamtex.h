#ifndef CELLSTREAMTEX_H
#define CELLSTREAMTEX_H

/* lane STREAMTEX1: THE STREAMED CELL VIEW'S FILE TEXTURES, COUNTED BY CELL (src/gl/streamtexpool.h has the policy).
 *
 * A texture a loaded cell's shader properties name stays loaded; when its last loaded cell leaves it joins a pool
 * of at most WW_CELL_STREAMTEX_MB megabytes, oldest released first out. Textures no streamed cell named (the open's
 * own blocks, the far field, the sky, terrain splats and water) keep the keep timer (WW_CELL_STREAM_TEXKEEP) they had.
 * Each crossing (the block's centre cell changes) prints one "stream tex:" census line on stderr: textures kept in
 * the pool, named by loaded cells, freed, and loaded again within 2 crossings of being freed, with their MB.
 *
 *   WW_CELL_STREAMTEX_MB=<MB>     the pool's budget (default 256; 0 = keep none past their last cell)
 *   WW_CELL_STREAMTEX_RED=off     the keep timer frees every file texture, as before this lane (census still printed)
 */

#include <QString>
#include <QStringList>
#include <QStringView>

#include <functional>

class NifModel;

//! WW_CELL_STREAMTEX_RED=off
bool cellStreamTexRed();
//! WW_CELL_STREAMTEX_MB in bytes
qint64 cellStreamTexBudgetBytes();
//! this document streams its cells (the names below are counted)
bool cellStreamTexCounting( const NifModel * nif );
//! a shader property block of `nif` names these texture files (glproperty.cpp, main thread)
void cellStreamTexNote( const NifModel * nif, int block, const QStringList & names );
//! the texture cache uploaded `name` (`bytes` of it)
void cellStreamTexLoaded( const NifModel * nif, const QString & name, qint64 bytes );
//! the view freed `name` (`counted` false: it was not loaded any more, a flush took it)
void cellStreamTexFreed( const NifModel * nif, const QString & name, bool counted = true );
//! false = a loaded cell names it or the pool owns it: the keep timer leaves it alone
bool cellStreamTexTimerMayFree( const NifModel * nif, QStringView name );
//! the pool's textures to free now; busy(name) true = bound by one of the last 3 paints, kept this time
QStringList cellStreamTexPlan( const NifModel * nif, const std::function<bool( const QString & )> & busy );

#endif

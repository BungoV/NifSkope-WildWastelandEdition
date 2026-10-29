#ifndef CELLSTREAMTEX_H
#define CELLSTREAMTEX_H

/* lane STREAMTEX1: THE STREAMED CELL VIEW'S FILE TEXTURES, COUNTED BY CELL (src/gl/streamtexpool.h keeps the counts).
 *
 * Every texture a written cell's shader properties name is counted once per loaded cell; when its last loaded cell
 * leaves it is "released". Each crossing (the block's centre cell changes) prints one "stream tex:" census line on
 * stderr: textures released and still resident, named by loaded cells, freed, and loaded again within 2 crossings of
 * being freed, with their MB (the walk dump carries the same as texKept/texRefd/texFreed/texReloaded pairs).
 *
 * lane STREAMTEX3: the counts order the PERFFIX4 pool (src/gl/texpool.h, budget WW_CELL_STREAM_TEXPOOL): over its
 * budget it frees released textures first, then the ones no streamed cell named (the open's own blocks, far field,
 * sky, terrain splats, water), and never one a loaded cell names. STREAMTEX1's own 256 MB pool of released textures
 * (WW_CELL_STREAMTEX_MB) is gone: one budget, the preset's.
 *
 *   WW_CELL_STREAMTEX_RED=off     the pool frees by last bind alone, as PERFFIX4 did (census still printed)
 *   WW_STREAM1_RED=texpool        the 5 s keep timer instead of the pool (PERFFIX4's red, unchanged)
 */

#include <QString>
#include <QStringList>
#include <QStringView>

#include <functional>

class NifModel;

//! WW_CELL_STREAMTEX_RED=off
bool cellStreamTexRed();
//! this document streams its cells (the names below are counted)
bool cellStreamTexCounting( const NifModel * nif );
//! a shader property block of `nif` names these texture files (glproperty.cpp, main thread)
void cellStreamTexNote( const NifModel * nif, int block, const QStringList & names );
//! the texture cache uploaded `name` (`bytes` of it)
void cellStreamTexLoaded( const NifModel * nif, const QString & name, qint64 bytes );
//! the view freed `name` (`counted` false: it was not loaded any more, a flush took it)
void cellStreamTexFreed( const NifModel * nif, const QString & name, bool counted = true );
//! lane STREAMTEX3: the PERFFIX4 pool's tier of a file (src/gl/texpool.h: 0 its cells left, 1 no streamed cell named
//! it, kTierKeep a loaded cell names it); empty (every file tier 0, PERFFIX4's order) with no stream or the red
std::function<int( QStringView )> cellStreamTexTiers( const NifModel * nif );

#endif

#ifndef CELLWATER_H
#define CELLWATER_H

#include <QString>

class Scene;
struct WwWaterRecord;

/*! The game's water in the cell view: water planes and placed water meshes, registered with their WATR record,
 *  draw through fo4_water.prog (the game's water pixel shader term for term) while the Cell lights row is on.
 *  WW_CELL_WATER=0 draws them plainly; WW_CELL_WATER_RED=<term>, _PROBE=<n> and _DUMP=<file> are debug aids. */

//! A new scene for this document: its table is emptied.
void wwCellWaterBegin( const void * nif );
//! The written shape `block` is a water surface of record `rec`.
void wwCellWaterShape( const void * nif, int block, const WwWaterRecord & rec );
//! True when `block` draws as water this frame (the row on, not pinned off, a registered water shape).
bool wwCellWaterWanted( Scene * scene, int block );
//! `block` is a registered water surface (row or not): it never casts a sun shadow, as in the game.
bool wwCellWaterIsShape( const void * nif, int block );
//! the Water row (WW/CellWater, default on as before); the pin WW_CELL_WATER wins
bool wwCellWaterOn();
void wwCellWaterSetOn( bool on );
//! With fo4_water.prog bound for `block`: the record, the sky, the textures. False = draw it the old way.
bool wwCellWaterUniforms( Scene * scene, int block, int firstTextureUnit );
//! the water just set up writes linear light into the cell view's HDR frame (the renderer marks its
//! pixels for the one tone map, cellhdr.h). False for its probes and its reflection-ray pass (raw data).
bool wwCellWaterWritesLinear();
//! a water surface of this scene draws as water and its WATR's SSR flag is set (cellssr.h).
bool wwCellWaterSsrWanted( Scene * scene );
//! One census line: how many shapes and records this document registered.
QString wwCellWaterEcho( const void * nif );

#endif

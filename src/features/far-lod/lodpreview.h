#ifndef LODPREVIEW_H
#define LODPREVIEW_H

/* lane PERFLOAD1 P8 (bungo 11-10, "a way to preview specific lod levels, for issue finding"): the LOD preview, live
 * (no reopen), one menu row each under the LOD group (src/cellworkspace.cpp). Not saved: every session starts on auto.
 *   level  auto | full | rep0 | rep1 | rep2 | rep3 | impostor (pin WW_CELL_LOD_FORCE). The far trees take it per frame
 *          (TreeLadder::forced, the nearest step a tree has); the far object rows take its MNAM slot at a far rebuild in
 *          place (liveFarKey: full = rep0, impostor = rep3; an object without that slot draws its nearest one).
 *   tint   each level its own colour, a row or card mid-fade the band colour (pin WW_CELL_LOD_TINT=1). Untinted = the
 *          near cells' full meshes and the terrain.
 *   ring   all | band | LOD4 | LOD8 | LOD16 | LOD32: one far ring alone, terrain and objects (pin WW_CELL_LOD_RING).
 *   grid   the near 5x5: follow | freeze (stays where it is, the camera flies free) | off (no near cell drawn, the far
 *          rings fill the hole) (pin WW_CELL_NEAR_GRID). src/cellstream.cpp.
 * Telemetry: `lod preview:` lines on each change (impostorchunk.cpp, cellfarlod.cpp, cellstream.cpp). */

#include <QString>

namespace LodPreview
{
enum Grid { Follow = 0, Freeze = 1, Off = 2 };
constexpr int kLevels = 6;	//!< TreeLadder steps: full, rep0..3, impostor
constexpr int kRings = 5;	//!< band, LOD4, LOD8, LOD16, LOD32 (cellfarlod.cpp's defs order)

//! -1 auto, else a TreeLadder step 0..5
int force();
void setForce( int step );
//! the far object rows' MNAM slot for the forced level: -1 auto, else 0..3 (built in; liveFarKey reads it)
int objectSlot();
bool tint();
void setTint( bool on );
//! -1 all, else the ring index 0..4
int ring();
void setRing( int ring );
int grid();
void setGrid( int mode );
bool pinned( const char * which );	//!< "force" | "tint" | "ring" | "grid": its WW_ pin is set
//! bumped on every change (the drawers re-upload and say their line once)
quint64 gen();
//! the eye the grid froze at (kept across a far rebuild in place, whose new stream freezes there again)
bool frozenEye( float out[3] );
void setFrozenEye( const float eye[3] );

QString levelName( int step );	//!< "auto" for -1
QString ringName( int ring );	//!< "all" for -1
QString gridName( int mode );
//! a far shape's ring from its name ("FarLOD ... r<i> ..."); -1 = not a far ring shape
int ringOf( const QString & shapeName );
//! true when the ring preview leaves this shape out of the draw
bool ringHides( const QString & shapeName );
//! the tint colour of a step (0..5), step 6 = the fade band; rgb 0..1
void tintColour( int step, float rgb[3] );
}

#endif

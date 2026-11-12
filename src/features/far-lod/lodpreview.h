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
bool pinned( const char * which );	//!< "force" | "tint" | "ring" | "grid" | "level": its WW_ pin is set
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
//! the tint colour of a step (0..5), step 6 = the fade band, 7 = an octahedral impostor; rgb 0..1
void tintColour( int step, float rgb[3] );

/* lane LODLIVE1 (bungo 11-11): the map level, one LOD level forced on every far ring past the band (terrain sheets,
 * object slot, the game's chunks): auto | lod4 | lod8 | lod16 | lod32 (pin WW_CELL_LOD_LEVEL); rebuilt in place. */
int level();	//!< -1 auto, else 0..3 = LOD4..LOD32
void setLevel( int level );
QString mapLevelName( int level );	//!< "auto" for -1

/* The LOD coloration (the tint row, like Unreal's): one band a drawn thing. The band index is also its tintColour step.
 * Shader code = band + 1 (fo4_default farLadder.y; 0 = untinted). */
enum Band { Full = 0, Lod4 = 1, Lod8 = 2, Lod16 = 3, Lod32 = 4, Card = 5, Fade = 6, Oct = 7 };
constexpr int kBands = 8;
QString bandName( int band );
//! the rule cellfarlod.cpp builds by: ring 0..4, objects (MNAM slot) or ground; level / objectSlot as level() / objectSlot()
int bandOfRing( int ring, bool objects, int level, int objectSlot );
//! a far shape or node name ("FarLOD [terrain|water] [v] r<i> ..."): its band now; -1 = not a ring shape
int bandOfFarName( const QString & name );
//! an octahedral set's stripe count by its frame size: 64 px 1, 128 px 2, 256 px 3, 512 px 4
int octStripe( int frameW );
//! counts for the `lod view:` line: cards drawn last frame (flat, octahedral, mid-fade); rows by band at their tint write
void noteCards( int flat, int oct, int fading );
void noteRows( int band, int n );
//! the line's card and row part, and the colour legend with each ring's start and screen size at that distance
QString viewCounts();
QString viewLegend( const float ringStart[kRings], int rings, double tanHalfFovY );
}

#endif

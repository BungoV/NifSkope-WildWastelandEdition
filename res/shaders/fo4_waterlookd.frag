#version 410 core

// lane WATERLOOK1 step 2 (src/gl/cellocean.h): fo4_waterlook.frag behind the split, displaced water (fo4_waterdisp.tesc /
// .tese): every water look, the spectrum read at the undisplaced point the tessellation hands over (wwOcBase).
// Chosen by name while the Ocean waves row is on (src/gl/cellwater.cpp wwCellWaterProgram); else fo4_waterlook.prog.
#define WW_WATER_DEPTH 1
#define WW_WATER_CAUSTICS 1
#define WW_WATER_SHORE 1
#define WW_WATER_WAVES 1
#define WW_WATER_REFL 1
#define WW_WATER_OCEAN 1
#define WW_WATER_SKYFAR 1
#define WW_WATER_CLEAR 1
#define WW_WATER_SKYVIS 1
#define WW_WATER_SURF 1
#define WW_WATER_CONTACT 1
#define WW_WATER_RIVERFOAM 1
#define WW_WATER_UNDER 1
#define WW_WATER_DISP 1
#ifndef WW_WATER_PROBES	// lane WATERFIX7: as fo4_waterlook.frag; the probes in fo4_waterlookdp
#define WW_WATER_NOPROBE 1
#endif
#include "fo4_water.frag"

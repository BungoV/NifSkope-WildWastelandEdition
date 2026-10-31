#version 410 core

// lane FIX1101A (src/gl/waterpreset.h): fo4_waterlook.frag for the Medium water preset: only the looks Medium draws
// (clear colour on the bed, physical reflections, soft edge, river foam; the far sky byte). The full twin compiles every
// look in, and its registers cost every look even when its rows are off (+0.07 ms at 1080p, tests/watercost). Picked by
// name while the preset is Medium and no other water look row is on (src/gl/cellwater.cpp wwCellWaterProgram).
#define WW_WATER_DEPTH 1
#define WW_WATER_SHORE 1
#define WW_WATER_REFL 1
#define WW_WATER_SKYFAR 1
#define WW_WATER_CLEAR 1
#define WW_WATER_RIVERFOAM 1
#define WW_WATER_NOPROBE 1
#include "fo4_water.frag"

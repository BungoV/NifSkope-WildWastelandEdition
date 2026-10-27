#version 410 core

// lane WATERCOMBINE1 (src/gl/cellwater.h): fo4_water.frag with every water look compiled in (depth colour, caustics,
// soft edge + shore foam, swell); each look is switched by its own uniform, so any set of the rows draws together.
// Chosen by name while any of the water look rows is on; with all off fo4_water.prog draws (its machine code kept).
#define WW_WATER_DEPTH 1
#define WW_WATER_CAUSTICS 1
#define WW_WATER_SHORE 1
#define WW_WATER_WAVES 1
#define WW_WATER_REFL 1
#define WW_WATER_OCEAN 1
#define WW_WATER_CLEAR 1
#define WW_WATER_SKYVIS 1
#define WW_WATER_SURF 1
#define WW_WATER_CONTACT 1
#include "fo4_water.frag"

#version 410 core

// lane WATERDEPTH1 (src/gl/cellwater.h): fo4_water.frag with the bed-depth colour compiled in (water_depth.glsl).
// Chosen by name only while the Water depth colour row is on, so the row off runs the program it always ran.
#define WW_WATER_DEPTH 1
#include "fo4_water.frag"

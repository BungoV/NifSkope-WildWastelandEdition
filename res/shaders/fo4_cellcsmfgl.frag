#version 410 core

// lane FARDRAW1: fo4_cellcsm.frag with the far ring 0's live ground (cell_farground.glsl, option 1). Chosen by
// name in Renderer::setupProgram for ring 0's terrain shapes only while the Far ground row is live.
#define WW_FOG 1
#define WW_CELLLIGHTS 1
#define WW_SUNSHADOW 1
#define WW_FARGROUND 1
#include "fo4_default.frag"

#version 410 core

// lane FARPAGES1: fo4_cellcsm.frag with the far ring 0's cached pages (cell_farground.glsl, option 3). Chosen by
// name in Renderer::setupProgram for ring 0's terrain shapes only while the Far ground row is on cached pages.
#define WW_FOG 1
#define WW_CELLLIGHTS 1
#define WW_SUNSHADOW 1
#define WW_FARGROUND 3
#include "fo4_default.frag"

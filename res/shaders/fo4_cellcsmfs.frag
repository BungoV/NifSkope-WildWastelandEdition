#version 410 core

// lane FARLOOK1101 (FARSHIMMER2): fo4_cellcsm.frag drawing a far row at its splat's coverage (fo4_defaultfs.vert).
// Chosen by name in Renderer::setupProgram for the instanced far rings only while WW_CELL_FARSPLAT is set.
#define WW_FOG 1
#define WW_CELLLIGHTS 1
#define WW_SUNSHADOW 1
#define WW_FAR_SPLAT 1
#include "fo4_default.frag"

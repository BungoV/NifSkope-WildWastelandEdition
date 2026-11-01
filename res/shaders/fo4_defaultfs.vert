#version 410 core

// lane FARLOOK1101 (FARSHIMMER2): fo4_default.vert with the far rows' blended splat (farSplat). Only the far splat
// twins (fo4_cellfs / fo4_cellcsmfs) use it, so every other program's binary stays as it was.
#define WW_FAR_SPLAT 1
#include "fo4_default.vert"

#version 410 core

// fo4_effectshader.frag the game's way in the cell view (unlit, linear, soft and near fades, fog, imagespace).
// Renderer::setupProgram picks it by name only while a draw is cell-lit.
#define WW_CELLLIGHTS 1
#define WW_CELL_FX 1
#include "fo4_effectshader.frag"

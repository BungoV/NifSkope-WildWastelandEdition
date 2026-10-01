#version 410 core

// lane EFX2: fo4_effectshader.frag the game's way in the cell view (unlit, linear, the soft and near
// fades, the fog, the cell's imagespace). Chosen by name in Renderer::setupProgram only while a draw
// is cell-lit, so every other effect draw runs the shader it always ran.
#define WW_CELLLIGHTS 1
#define WW_CELL_FX 1
#include "fo4_effectshader.frag"

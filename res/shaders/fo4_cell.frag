#version 410 core

// fo4_default.frag with the cell's own lights (and the Lookdev fog) compiled in.
// Renderer::setupProgram picks it by name only while a draw is cell-lit.
#define WW_FOG 1
#define WW_CELLLIGHTS 1
#include "fo4_default.frag"

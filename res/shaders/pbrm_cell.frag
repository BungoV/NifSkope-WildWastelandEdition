#version 410 core

// pbrm_default.frag with the cell's own lights and the bake's bounce compiled in (src/gl/celllights.h).
// Renderer::setupProgram picks it by name only while a draw is cell-lit.
#define WW_CELLLIGHTS 1
#include "pbrm_default.frag"

#version 410 core

// lane PRTP3: fo4_default.frag with the cell's own lights (and the Lookdev fog) compiled in.
// Chosen by name in Renderer::setupProgram only while a draw is cell-lit, so the row off runs
// the shader it always ran.
#define WW_FOG 1
#define WW_CELLLIGHTS 1
#include "fo4_default.frag"

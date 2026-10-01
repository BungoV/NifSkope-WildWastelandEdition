#version 410 core

// lane PRTPGI: pbrm_default.frag with the cell's own lights and the bake's bounce compiled in
// (src/gl/celllights.h). Chosen by name in Renderer::setupProgram only while a draw is cell-lit,
// so the row off runs the shader it always ran.
#define WW_CELLLIGHTS 1
#include "pbrm_default.frag"

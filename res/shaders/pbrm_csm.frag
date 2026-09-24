#version 410 core

// lane CSM1: pbrm_default.frag with the cascaded sun shadows compiled in. The loader
// drops the included file's own #version line. Chosen by name in
// Renderer::setupProgram only while a draw receives, so Shadows off runs the pre-CSM shader.
#define WW_SUNSHADOW 1
#include "pbrm_default.frag"

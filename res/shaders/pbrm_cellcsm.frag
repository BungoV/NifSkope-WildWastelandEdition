#version 410 core

// lane SUNCELL1: pbrm_cell.frag with the cascaded sun shadows compiled in. Chosen by name in
// Renderer::setupProgram only while a cell-lit draw receives, so Shadows off runs pbrm_cell as before.
#define WW_CELLLIGHTS 1
#define WW_SUNSHADOW 1
#include "pbrm_default.frag"

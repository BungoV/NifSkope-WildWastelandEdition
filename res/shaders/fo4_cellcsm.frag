#version 410 core

// lane SUNCELL1: fo4_cell.frag with the cascaded sun shadows compiled in. Chosen by name in
// Renderer::setupProgram only while a cell-lit draw receives, so Shadows off runs fo4_cell as before.
#define WW_FOG 1
#define WW_CELLLIGHTS 1
#define WW_SUNSHADOW 1
#include "fo4_default.frag"

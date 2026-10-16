#version 410 core

// fo4_cell.frag with the cascaded sun shadows compiled in.
// Renderer::setupProgram picks it by name only while a cell-lit draw receives shadows.
#define WW_FOG 1
#define WW_CELLLIGHTS 1
#define WW_SUNSHADOW 1
#include "fo4_default.frag"

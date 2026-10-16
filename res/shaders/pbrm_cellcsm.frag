#version 410 core

// pbrm_cell.frag with the cascaded sun shadows compiled in.
// Renderer::setupProgram picks it by name only while a cell-lit draw receives shadows.
#define WW_CELLLIGHTS 1
#define WW_SUNSHADOW 1
#include "pbrm_default.frag"

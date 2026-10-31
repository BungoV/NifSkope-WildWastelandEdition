#version 410 core

// pbrm_default.frag with the cascaded sun shadows compiled in. The loader drops the included file's #version line.
// Renderer::setupProgram picks it by name only while a draw receives shadows.
#define WW_SUNSHADOW 1
#include "pbrm_default.frag"

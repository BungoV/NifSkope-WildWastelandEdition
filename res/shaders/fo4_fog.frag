#version 410 core

// fo4_default.frag with the Lookdev weather fog compiled in. The loader drops the included file's #version line.
// Renderer::setupProgram picks it by name only while a draw fogs.
#define WW_FOG 1
#include "fo4_default.frag"

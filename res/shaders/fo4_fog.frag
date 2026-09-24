#version 410 core

// lane FOG1: fo4_default.frag with the Lookdev weather fog compiled in. The loader
// drops the included file's own #version line. Chosen by name in
// Renderer::setupProgram only while a draw fogs, so Fog off runs the pre-fog shader.
#define WW_FOG 1
#include "fo4_default.frag"

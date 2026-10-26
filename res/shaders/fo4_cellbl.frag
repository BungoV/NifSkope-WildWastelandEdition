#version 410 core
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_shader_storage_buffer_object : require
#extension GL_ARB_shading_language_420pack : require

// lane BINDLESS2: fo4_cell.frag with the material's maps read through the per-draw row's handles. Chosen by name in
// Renderer::setupProgram only while the Bindless draws row is on, so the row off runs fo4_cell as before.
#define WW_FOG 1
#define WW_CELLLIGHTS 1
#define WW_BINDLESS 1
#include "fo4_default.frag"

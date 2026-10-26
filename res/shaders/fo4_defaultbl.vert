#version 410 core
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_shader_storage_buffer_object : require
#extension GL_ARB_shading_language_420pack : require

// lane BINDLESS2: fo4_default.vert reading its matrices from the per-draw row (ww_bindless.glsl)
#define WW_BINDLESS 1
#include "fo4_default.vert"

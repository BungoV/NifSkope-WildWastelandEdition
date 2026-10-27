#version 410 core
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_shader_storage_buffer_object : require
#extension GL_ARB_shading_language_420pack : require
#extension GL_ARB_shader_draw_parameters : enable

// lane BINDLESS2: fo4_default.vert reading its matrices from the per-draw row (ww_bindless.glsl)
#define WW_BINDLESS 1
#define WW_BL_VERT 1
#define main wwBlMain
#include "fo4_default.vert"
#undef main

// lane BINDLESSMDI1: the row once per vertex, so a multi-draw's fragments read their own draw's maps
void main()
{
	wwBlRow = wwDrawIdx;
	wwBlMain();
}

#version 410 core
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_shader_storage_buffer_object : require
#extension GL_ARB_shading_language_420pack : require

// lane BINDLESS2: fo4_cellcsm.frag with the material's maps read through the per-draw row's handles (the Bindless
// draws row on); the row off runs fo4_cellcsm as before.
#define WW_FOG 1
#define WW_CELLLIGHTS 1
#define WW_SUNSHADOW 1
#define WW_BINDLESS 1
#include "fo4_default.frag"

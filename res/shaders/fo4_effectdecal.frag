#version 410 core

// fo4_effectshader.frag for a Decal-flagged effect in the cell view: the game draws it into the surface under it,
// so it takes that surface's lights, ambient, sky, obscurance and fog. Debug: WW_CELL_DECALLIT_RED=unlit.
#define WW_CELLLIGHTS 1
#define WW_CELL_FX 1
#define WW_FX_DECALLIT 1
#include "fo4_effectshader.frag"

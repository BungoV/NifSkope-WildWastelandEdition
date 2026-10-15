#version 410 core

// lane DECALLIT1: fo4_effectshader.frag for an effect with the Decal flag in the cell view: the game draws it into the
// surface it lies on (the deferred decal groups), so it takes that surface's lights, ambient, sky, obscurance and fog.
// Chosen by name in Renderer::setupProgram only for such a draw while it is cell-lit (red WW_CELL_DECALLIT_RED=unlit).
#define WW_CELLLIGHTS 1
#define WW_CELL_FX 1
#define WW_FX_DECALLIT 1
#include "fo4_effectshader.frag"

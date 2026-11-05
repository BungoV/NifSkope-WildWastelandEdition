#version 410 core

// lane SUNCELL1: particles.frag the game's way in the cell view (linear, fogged, the HDR frame or the cell's
// imagespace). Chosen by name in Particles::drawShapes only while a draw is cell-lit.
#define WW_CELLLIGHTS 1
#include "particles.frag"

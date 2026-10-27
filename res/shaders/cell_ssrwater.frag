#version 410 core

// lane WATERREFL1 (src/gl/cellssr.cpp): cell_ssr.frag for the Water reflections row. A water ray marches 64 steps,
// keeps far hits (screen-edge fade only, no travelled-distance or depth-gain fade) and their colour unclamped (HDR);
// opaque rays are the game's. Picked by name while the row is on; cell_ssr.prog keeps its machine code.
#define WW_SSR_WATER_LOOK 1
#include "cell_ssr.frag"

#version 410 core

// lane WATERUNDER4: cell_underwater.frag run per sample over the samples its per-pixel draw left (an edge pixel's
// nearer samples), each with its own depth. Selected by name in src/gl/cellunderwater.cpp after cell_underwater.prog.
#define WW_UW_SAMPLE 1
#include "cell_underwater.frag"

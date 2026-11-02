#version 410 core

// lane SKINSSS2: cell_skinsss.frag's half-resolution passes (option WW_CELL_SKINSSS_HALF=1). Its own program, so the
// full-resolution cell_skinsss.prog keeps SKINSSS1's code.
#define WW_SSS_HALF 1
#include "cell_skinsss.frag"

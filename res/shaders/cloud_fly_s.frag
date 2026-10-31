#version 410 core

// lane CLOUDSHAPE1: cloud_march.frag with the fly-through grid (CLOUD4) over the Cloud Shapes density; off draws
// cloud_fly.prog / cloud_fly_a.prog unchanged.

#define WW_CLOUD_SHAPE 1
#define WW_CLOUD_ACCUM 1
#define WW_CLOUDFLY 1
#include "cloud_march.frag"

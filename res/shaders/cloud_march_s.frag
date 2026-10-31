#version 410 core

// lane CLOUDSHAPE1: cloud_march.frag over the Cloud Shapes density (blue noise offsets as Cloud Accumulation's; IGN when
// cvBlueSlice is -1); off draws cloud_march.prog / cloud_march_a.prog unchanged.

#define WW_CLOUD_SHAPE 1
#define WW_CLOUD_ACCUM 1
#include "cloud_march.frag"

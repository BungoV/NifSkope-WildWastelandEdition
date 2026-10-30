#version 410 core

// lane CLOUDBAND1: cloud_fly.frag with Cloud Accumulation's blue noise step offsets; off draws cloud_fly.prog unchanged.

#define WW_CLOUDFLY 1
#define WW_CLOUD_ACCUM 1
#include "cloud_march.frag"

#version 410 core

// lane CLOUDBAND1: cloud_resolve.frag with Cloud Accumulation (the fresh march blended into the clamped history); off
// draws cloud_resolve.prog unchanged.

#define WW_CLOUD_ACCUM 1
#include "cloud_resolve.frag"

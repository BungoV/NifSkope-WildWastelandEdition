#version 410 core

// lane CLOUD6c: the offline gate's cost probe: cloud_march_a.frag (the default program) with the step counters on;
// cvDepth = (march samples, lit samples, density reads, refinements) per history pixel. Test only, never shipped.
// tests/spells/cloudedge_check.py builds it from a scratch copy of res/shaders with this folder laid over it.

#define WW_CLOUD_ACCUM 1
#define CV_COUNT 1
#include "cloud_march.frag"

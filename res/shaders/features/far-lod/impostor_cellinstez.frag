#version 410 core
#extension GL_ARB_conservative_depth : enable
// lane PERFFIX4: impostor_cellinst.frag with gl_FragDepth declared depth_greater (the quad stands in front of it,
// impostor_instez.vert), so the hardware's early depth test runs before the sheets are marched.
#define WW_FOG 1
#define WW_CELLLIGHTS 1
#define WW_CARDINST 1
#define WW_CARD_EARLYZ 1
#include "impostor_oct.frag"

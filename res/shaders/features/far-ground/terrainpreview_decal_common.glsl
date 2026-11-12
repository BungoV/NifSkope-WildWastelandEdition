#version 430 core
struct Rec { vec4 posScale; vec4 quat; vec4 box0; vec4 box1; vec4 atlas; };
layout(std430, binding = 0) readonly buffer Recs { Rec recs[]; };
vec3 qrot( vec4 q, vec3 v ) { return v + 2.0 * cross( q.xyz, cross( q.xyz, v ) + q.w * v ); }

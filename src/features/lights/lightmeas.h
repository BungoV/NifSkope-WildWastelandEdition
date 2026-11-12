#pragma once

class Scene;

/*! lane LIGHTMEAS1 (measure only, no light law changes here): the frame's light at the camera in real units (lux, cd/m2),
 *  every line prefixed `light meas:`, beside the references (CIE overcast, real overcast and clear-sky ranges, the full
 *  moon). Called from the exposure's measure (cellhdr.cpp isApply) with its quarter-size mip: rgb = the mean linear light
 *  of the cell pixels in the texel (the sky too under WW_SKY_LINEAR), a = their share. At most once a second, a line only
 *  when its text changed. Switch WW_LIGHT_MEAS (live, default on; 0 = off). Notes: notes/lightmeas1/. */
void wwLightMeasFrame( Scene * scene, const float * px, int qw, int qh );

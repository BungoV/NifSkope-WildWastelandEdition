#version 410 core

// lane FARLOD1: impostor_oct.frag with the cell's own lights and the Lookdev fog compiled in
// (fo4_cell.frag's pattern). Chosen by name only for the cell view's far cards.
#define WW_FOG 1
#define WW_CELLLIGHTS 1
#include "impostor_oct.frag"

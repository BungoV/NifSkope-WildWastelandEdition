#version 410 core

// lane FARCARD1: impostor_cell.frag with the per-card values as flat inputs (impostor_inst.vert), for the
// cell view's instanced far cards. The same arithmetic: only where the numbers come from moves.
#define WW_FOG 1
#define WW_CELLLIGHTS 1
#define WW_CARDINST 1
#include "impostor_oct.frag"

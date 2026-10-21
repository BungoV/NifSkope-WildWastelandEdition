/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef WW_CELLCLUSTERGL_H
#define WW_CELLCLUSTERGL_H

// The cluster light lists on the GPU (cell_cluster.glsl); the builder is src/gl/cellcluster.h.

#include <QString>

#include <cstdint>
#include <vector>

class Scene;

/*! Per lit draw, after cellLightCount is set. `texels` is the light buffer as uploaded (celllights.cpp's layout),
 *  `stamp` changes whenever it does. The lists are rebuilt only when the camera, viewport or lights moved; a draw
 *  under any other camera keeps the flat loop. Null `texels`: only bind the sampler (an unbound samplerBuffer unit
 *  beside BaseMap's sampler2D is a draw-time error). WW_CELL_CLUSTER=0 keeps every draw on the flat loop. */
void wwCellClusterUniforms( Scene * scene, const std::vector<float> * texels, int count, std::uint64_t stamp );

//! The last build's numbers, one line (also printed to stderr whenever they change).
QString wwCellClusterEcho();

#endif // WW_CELLCLUSTERGL_H

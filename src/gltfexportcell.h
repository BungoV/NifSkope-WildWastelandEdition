/* lane GLTFCELL1 (2026-10-28): a whole exterior cell block as glTF 2.0.

   `NifSkope -no-gui gltf-cell "<plugins|world|x,y|n>"|<file.wwcell> --out <abs .gltf>
       [--cells x,y ...] [--origin x,y,z] [--textures copy|none] [--terrain on|off]`

   The placements come from the cell view's own builder (cellBuild, through the CellExportSink in
   cellview.h): the same SCOL parts, MSWP swaps, markers and sky-card skips an open draws. One glTF mesh
   per shape of each model+swap, shared by every placement of it (node.mesh instances), one node per
   placement with its TRS, the LAND per cell quadrant with its base texture, and the textures copied
   out of the resource stack beside the file. Written by src/gltfexport.cpp, so units and the Z-up root
   are the character export's. */

#ifndef GLTFEXPORTCELL_H
#define GLTFEXPORTCELL_H

#include <QStringList>

int gltfCellCli( const QStringList & args );

#endif // GLTFEXPORTCELL_H

# BAKE1 pictures (2026-09-25, from mods\FO4CSLOD)

All pictures are rendered from the baked mod folder (Commonwealth.lodl terrain, VT sheets, .lodo/.lodi objects)
by the run-copy exe 27a7bb29 through shot.sh, one NifSkope at a time on the second monitor. Top views look
straight down with north up; the scale bar is one cell (4096 units). Terrain: lodl level 2 (level 3 for the
overview). Objects: ring-4 slot (WW_LODI_SLOT=0), so trees are drawn as their authored ring-4 LOD meshes, not as
cards. Every picture was checked by eye and by numbers (luminance SD 17 to 42, 8,000 to 19,500 colours).
None is grey or empty.

| file | place, cells | what is drawn |
|---|---|---|
| 00_contact_sheet.png | all | every picture below on one sheet |
| 01_sanctuary.png | Sanctuary Hills + Red Rocket, -22,19..-17,24 | the Sanctuary houses on their loop road, the bridge, BNS and vanilla trees, rocks. A straight-edged dark block of ground in the middle follows cell edges; it looks like a changed ground texture on those cells (not checked against the plugins) |
| 02_concord.png | Concord + Museum of Freedom, -17,14..-12,19 | town blocks, the museum (the large flat-roofed building in the middle), roads, dense autumn trees |
| 03_downtown_boston.png | Diamond City to Boston Common, -5,-10..2,-3 | the stadium (left), downtown blocks, the Charles at the top, bridges |
| 04_glowing_sea_edge.png | Glowing Sea edge, -18,-24..-11,-17 | the grey crater ground (south-west) meets the brown Commonwealth ground; elevated highway, sparse trees |
| 05_bns_forest.png | the densest BNS Trees area, -4,-28..3,-21 | BNS tree placements over forest-floor ground, a road across the top |
| 06_grass_block.png | the grass-densest 8x8 block, -24,-8..-17,-1 | grass-ground cells with shrubs and trees, the elevated highway on the right, a small settlement top left |
| 07_bns_tree_3d_vs_card.png | one leafy BNS tree | the full 3D model beside its 8x8 card sheet. The card albedo is unlit; the 3D view is lit |
| 08_boston_oblique.png | downtown Boston, -5,-10..2,-3, oblique | the same area as 03 in 3D: towers, the stadium, the river bank |
| 09_overview.png | the whole playable map, -64,-48..31,47 | terrain and every object at lodl level 3. The Glowing Sea is the grey south-west. Some edge cells have flat-coloured terrain at this level. The west half of the frame is empty worldspace |

work/ holds the raw shots and their logs (unlabelled).

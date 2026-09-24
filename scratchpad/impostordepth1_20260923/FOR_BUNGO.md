# IMPOSTORDEPTH1 -- for bungo

## The card SNAPPING (one angle at a time, no blend)
E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostordepth1_20260923/gifs/maple_3dmodel_vs_8x8snap_vs_8x8crispA_vs_16x16snap.gif

Four panels, same orbit, light and material as the earlier maple GIFs:
3D model | 8x8 snap | 8x8 crisp (A) | 16x16 snap.
"Snap" means each frame of the GIF shows exactly one baked photo of the tree (the nearest one),
nothing mixed. It jumps when the next photo takes over.

## Trunk close-up, before
E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostordepth1_20260923/gifs/trunk_strip_before.png
Top row the real tree, then the card three ways, at the angles where the trunk went wrong.

## With the depth search on (search 16 steps, edge fix on)
E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostordepth1_20260923/gifs/maple_after_8bitheight_3dmodel_snap_crispsearch_stipplesearch.gif
E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostordepth1_20260923/gifs/maple_after_shippedsheets_3dmodel_snap_crispsearch_stipplesearch.gif
E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostordepth1_20260923/gifs/trunk_strip_after.png

Same four panels in both GIFs. The only difference between them is the sheet that stores
each photo's depth:
- "shippedsheets" = the card files lodgen writes today. The trunk still doubles. The depth in
  those files is squeezed too hard (about 34 units out on average), so the search looks in the
  wrong place.
- "8bitheight" = the same bake with only that one sheet saved without compression. One trunk
  at every angle in the strip.

What passed the trunk test: stipple + search + edge fix, on the 8-bit sheet, at eye level.
From 20 degrees up it misses on 4 of 360 angles (the trunk a bit too thick near one angle).
Crisp + search keeps the trunk in one piece but draws it too thin, so it fails the test.

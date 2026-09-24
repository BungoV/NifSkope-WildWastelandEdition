# Impostor vs. the model it was baked from -- every baked angle

Lane IMPOSTORLOOK1, 2026-09-19. All paths under
`E:\Projects\NifskopeWildWastelandEdition\scratchpad\impostorlook1_20260919\images\`.

## Look at this one first

`C_sim_cut_maple_n4.png` -- three panels: the model, the impostor as it ships,
and the SAME baked picture off the same sheet with one thing changed.

The middle panel is the blob you have been complaining about. The right panel is
a thin tree again. Nothing was rebaked and nothing was rebuilt to get it -- the
sheet on disk already holds the thin tree. What changes between the middle and
the right panel is that the drawer currently asks each texel a yes/no question
("is anything here?") and then paints every yes at full strength, so a texel
that is 7 per cent twig paints as solidly as a texel that is all trunk. On that
tree 94 per cent of the picture is part-covered texels, so the crown fills in.

The right panel is a SIMULATION, not a render. The check that it means anything:
the same code, asked to reproduce the impostor you ALREADY have, reproduces it
at 0.9171 overlap on the blasted maple -- above the 0.88 bar that was set before
looking. On the fine-twig maple it only reaches 0.762, so treat that picture as
a picture and not as a number.

It is not a free repair: the same change takes the fine-twig maple from 37 per
cent too much paint to 26 per cent too little, because at 32 texels across, a
twig at 20 per cent coverage nearly disappears. That is why it is put in front
of you as a decision instead of applied.

`C_sim_cut_blast_n4.png` is the same three panels for the blasted maple.

## Then the full comparison you asked for

Each sheet has ONE CELL PER BAKED FRAME, in the card's own grid order, labelled
with the frame number and the azimuth/elevation it was photographed from.
In every cell, left to right:

    the original mesh  |  the impostor as the viewer draws it  |  the raw baked
                                                                  frame off the
                                                                  sheet on disk

Same camera, same scale, same background for the two grabs. The third panel is
the sheet's own texels, enlarged by a whole number with no smoothing, so you
are looking at the actual stored picture.

1. `A_maple_n4_every_bake_angle.png` -- the fine-twig maple, 16 angles.
   The mesh is a thin twiggy tree. The impostor is a cluster of BLOBS. And the
   third panel shows the baked frame is NOT blobs -- it is a recognisable thin
   tree. So the crown detail survives the bake and is destroyed by the DRAW.
2. `A_blast_n4_every_bake_angle.png` -- the blasted maple, 16 angles.
   The trunk survives; the fine side twigs do not exist on the card at all, and
   the card's trunk edge is lumpy where the mesh's is smooth.
3. `A_blast_n8_every_bake_angle.png` -- 64 angles, EVERY mesh/card cell MISSING.
4. `A_dead_n4_every_bake_angle.png` -- 16 angles, every mesh/card cell MISSING.
5. `A_rock_n4_every_bake_angle.png` -- 16 angles, every mesh/card cell MISSING.

## Why three sheets say MISSING

Nobody has ever photographed those three subjects at their own bake directions.
Everything else in the tree is at the 24 orbit views, which are different
angles, so putting one of those in the cell would be showing you a comparison
that was never made. The cells say MISSING instead, and
`shoot_missing.sh` in this folder shoots exactly the 96 missing directions
(192 grabs, three runs). It is WRITTEN AND NOT RUN -- this lane is offline and
another lane holds the build slot.

The baked-frame panel is filled in on all five sheets, including the three
MISSING ones, because reading the sheet off disk needs no program running.

## The close-ups, and the one thing nobody had ever measured

`B_crop_crown_maple_n4_x3.png`, `B_crop_trunk_blast_n4_x3.png` and
`B_crop_fork_blast_n4_x3.png` are three-times enlargements with no smoothing --
every little square is one screen pixel of the original shot. The trunk is
fatter on the card by half a texel on each side, which is 14 per cent on the
blasted maple and DOUBLE on the fine-twig one, whose trunk is a single texel
wide in the sheet. The lumps are not compression blocks; that was tested and
ruled out.

`B_pairs_maple_n4.png` and `B_pairs_blast_n8.png` walk round the tree at four
azimuths at the shots' own size.

And the flatness, which had never been measured on these cards at all: as the
model goes from its darkest to its brightest, it changes brightness by a factor
of thirty-nine. The impostor changes by a factor of 1.4 -- and it is at its
DARKEST where the model is at its brightest. The detail of why is in the report,
but the short version is that the impostor is the only thing in the renderer
that skips the tone curve every other surface goes through.

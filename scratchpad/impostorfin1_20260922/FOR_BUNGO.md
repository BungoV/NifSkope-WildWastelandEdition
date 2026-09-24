# Tree cards: what they look like now

All the pictures below are baked at the highest setting (cardRes 512) and drawn by the new build (NifSkope.exe 22:12:21). Your open NifSkope window is still running the old build, so restart it to get the new one.

## The two pictures to open first

1. **Maple: every bake angle.** `pics512/maple_n4_every_bake_angle.png`
   - Each frame is **240x512 texels**.
   - Every one of the 16 directions is shown three ways: the real tree, the card as drawn, and the raw baked frame.
2. **Blasted maple: before and after.** `pics512/blast_n4_before_after.png`
   - Each frame is **160x512 texels**.
   - It shows the real tree, then the old build, then the new build, from the same four views.

## The rest (cardRes 512)

| Tree | Frame size (texels) | Every angle | Before / after |
|---|---|---|---|
| blasted maple, 16 frames | 160x512 | `pics512/blast_n4_every_bake_angle.png` | `pics512/blast_n4_before_after.png` |
| blasted maple, 64 frames | 160x512 | `pics512/blast_n8_every_bake_angle.png` | `pics512/blast_n8_before_after.png` |
| forest maple | 240x512 | `pics512/maple_n4_every_bake_angle.png` | `pics512/maple_n4_before_after.png` |
| dead tree | 96x512 | `pics512/dead_n4_every_bake_angle.png` | `pics512/dead_n4_before_after.png` |
| cliff rock | 512x496 | `pics512/rock_n4_every_bake_angle.png` | `pics512/rock_n4_before_after.png` |

There are three more sets of pictures:
- The forest maple at cardRes 256 (frames 128x256): `pics256/`.
- The earlier small sheets, which now have every angle filled in: `pics_fixture/`.
- Pairs where the only thing that changes is the transparency cut-off: `pics512_cutonly/`.

## The transparency cut-off: now the same as vanilla's

Vanilla Fallout 4 uses one cut-off on every far-away model: **128 out of 255**. It is a hard cut, never a blend. I checked all 497 of the game's distant-LOD files and found no other value. The cards now use that cut-off by default.

What you will see:
- Thin twigs get thinner, because the maple crown loses about a quarter of its painted area.
- Solid shapes (trunks, the rock) barely change.
- If you want the old look for one run, set `WW_IMPOSTOR_ALPHA=0.0627`.

## Something to look at: the trees now look too dark

This does not come from the cut-off. It comes from the lighting change that another lane (IMPOSTORLOOK1) wrote and I switched on as the brief said.

| | Card brightness compared with the real model |
|---|---|
| Trees, before the lighting change | about the same (0.94 to 0.97) |
| Trees, after the lighting change | about half (0.48 to 0.56) |
| Rock, before | 31% too bright (1.31) |
| Rock, after | 8% too dark (0.92) |

You can see it clearly in `pics512/dead_n4_before_after.png`. Whether to keep this lighting change, fix it or undo it is your call and the director's.

## Still wrong in every version

- The blasted maple's thin side twigs, which the real model shows, are missing from its card.
- The dead tree's spreading roots are missing from its card.

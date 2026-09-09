# Lane CARDFIT3 — the impostor card frame law

bungo's rule, his three messages, verbatim:

1. *"biggest, texture aspect ratio, maximizing the size of the geometry on each
   render, so that there is still a little bit of padding, 8 pixels on
   1024x1024, 16 on 2k, and so on, you can guess the rest"*
2. *"What we essentially need, maximizing the tree's size in each row and
   column, with enough pixel padding so that there's no mip map bleeding into
   other rows and columns"*
3. *"nifskope needs to position the tree in each view, so that the tree is equal
   in size on each one, and with minimal pixels wasted, and end up with no
   mipmap bleeding to nearby frames"* and *"then the tree must be positioned
   correctly, so that when a 3d tree transitions to an imposter, the tree won't
   change position"*

**State.** Built twice: `release/NifSkope.exe` **2026-09-09 21:01:14**, newer
than every source this lane touched, stylesheet in step. Both card gates green:
`lodgen_octahedral.sh` **70 checks / 0 failures / RESULT PASS**,
`lodgen_impostor_cards.sh` **14 / 0 / PASS**. `Fallout4.exe` and any other
`NifSkope.exe` were checked absent before every build, bake, render and harness
run. **Nothing committed** (his "Not yet"): 84 uncommitted paths in the tree.
**bungo's open NifSkope window predates this and needs a restart** to pick up
either build.

Everything below is measured on the 19-tree Sanctuary library
`scratchpad/images_20260909/gen/cards_trees19` (lane IMAGES5, exe 19:35:14,
OCT=8 TILE=128, 19 baked / 0 failed). Scripts and JSON are in
`scratchpad/cardfit_20260909/`.

---

## 1. Measurement — what the cards look like today

### 1.1 Fill, per axis, per frame

`measure_fill.py` (inherited from an earlier CARDFIT launch, re-run on the
19-tree set) and `measure_bbox.py` (new — per-frame bounding boxes rather than
just fill). 19 bases × 64 views = 1,216 frames.

Best view of each base, silhouette bounding box against the whole frame:

| | min | median | max |
|---|---|---|---|
| fill-x / frame | 0.094 | 0.344 | 0.859 |
| fill-y / frame | 0.562 | 0.781 | 0.852 |
| fill-x / inner rect (frame minus gutter) | 0.125 | 0.458 | 0.982 |
| fill-y / inner rect | 0.643 | 0.917 | 1.000 |

The number bungo's rule is actually about is the **union of the 64 views'
bounding boxes**, because one scale serves every frame: that box is what the
frame has to hold, and everything outside it is wasted in every frame.

| union box / frame | min | median | max |
|---|---|---|---|
| x | 0.125 | 0.500 | 0.875 |
| y | 0.562 | 0.812 | 0.875 |

The ceiling the padding allows is `inner/frame` — 0.750 in x and 0.875 in y on
a 32×64 frame, 0.875 on both on a 128×128 one. **The long axis is close to its
ceiling (0.812 against 0.875 median). The short axis is at two thirds of
it (0.500 against 0.750), and the worst base reaches one sixth.**

Per base (`bbox_before.json` has all of it). The two extremes:

* `0003a28b` TreeHero01, frame 128×128 — union box 112 × 103 texels, 0.875 ×
  0.805. This one is already at the ceiling.
* `0003e08d` TreeBlasted05, frame 32×32 — union box **4 × 22** texels, 0.125 ×
  0.688. A needle-shaped tree in a SQUARE frame.

### 1.2 The frame is centred on the tree

Not a cause. The union box's centre against the frame's centre, over 19 bases:

| | min | median | max |
|---|---|---|---|
| \|Δ\| x, texels | 0.0 | 0.0 | 1.0 |
| \|Δ\| y, texels | 0.0 | 1.0 | 3.5 |

So the bound-sphere centre the bake looks at is within a texel of the best
single centre in x and within 3.5 texels (2.7% of a 128 frame) in y. Recentring
is worth almost nothing; §6 puts a number on it.

### 1.3 Padding and mips as shipped

`G = max(4, tileLong/16)` on all four sides of every frame, and the mip chain
ran *while a frame's shorter side spanned eight texels*. Those two numbers were
never connected to each other.

| frame | gutter G | mips shipped | gutter at the last mip |
|---|---|---|---|
| 32×32, 32×64 | 4 | 3 | 1.00 texel |
| 96×128 | 8 | 5 | 0.50 texel |
| 128×128 | 8 | 5 | 0.50 texel |

### 1.4 Cross-frame bleed — measured, with a control

`measure_bleed.py`. The mip chain is rebuilt with lodgen's own filter (2×2 box,
rounded half-up). **Construction never mixes frames** — a frame side stays even
all the way down — so the bleed is at SAMPLE time: a reader sampling on a
frame's own UV border takes half its value from the next frame. Metric = the
neighbour's ALPHA contribution at the border, per frame border, per shipped mip.
Control = the same sheet with the gutter stripped and the inner rects re-tiled
edge to edge, which must fail.

Result over the 19 bases:

| sheet | mips 0…3 | mip 4 |
|---|---|---|
| every 32-frame card (15 of 19) | 0 on all 28 borders | not shipped |
| `0003a28b` 128×128 | 0 | **26/255 across 16 of 28 borders** |
| `0004a074`, `000d9ca8`, `000d9ca9` 96×128 | 0 | **15/255 across 10 of 28 borders** |

Control (gutter stripped): 18 of 19 bases bleed at mip 0, up to 117/255. One
does not, which is a weakness of using a real tree as the control — that tree's
silhouette does not reach the inner edge on any border. Under the new law the
frames are tighter and the control fails on **19 of 19**, up to 127/255.

**So bleed exists today, on every card whose frame is 96 texels or more, at the
last mip those cards ship.**

---

## 2. Cause

Three, in order of how much they cost:

1. **The aspect ladder's floor.** The short side came off a five-rung ratio
   ladder `{1, ¾, ½, ⅜, ¼}` and was then clamped to
   `shortFloor = max(32, 2G+4 → mult of 4)`. A 1:5.5 tree asks for a rung below
   ¼ and there is none; a tree on the 32-texel rung of the size ladder has
   `tileLong == shortFloor == 32` and gets a SQUARE frame whatever its shape.
   That is `0003e08d` at 12.5% fill and it is the single biggest waste.
2. **The gutter is a fraction of the LONG side, applied to both.** `G =
   max(4, tileLong/16)` puts 4 texels each side of a 32-texel short side —
   25% of it — while the same rule on a 128 side spends 12.5%. Narrow frames pay
   double.
3. **The mip cap had nothing to do with the gutter.** Stopping at "a frame's
   shorter side spans 8 texels" ships a level whose gutter is half a texel
   whenever the gutter started at 8. Hence §1.4.

Two things that are NOT the cause, both measured and both named in the brief as
candidates:

* **Not the bounding sphere / not the frame's centre.** §1.2: the frame is on the
  tree to within one texel in x.
* **Not the `.lodm` half extents being wrong.** `0003a28b`'s `half` is
  1286.41 × 1286.41 and its frame is 128×128; the recorded extents carry the
  frame's aspect exactly and the inner rect carries the silhouette, which is
  what the union-box measurement reads back. The extents are consistent with the
  picture; the picture is just mostly air.

---

## 3. The rule, the design, and the contract fields

### 3.1 Padding, and the mips it buys

A reader sampling inside a frame's UV rect reaches half a texel past the rect
**at the rect's own border**, and that tap lands in the neighbouring frame. Mip
construction never mixes frames. So the condition for a clean level `k` is one
whole texel of gutter at that level:

```
P / 2^k >= 1      ->      k <= log2(P)      ->      M = 1 + log2(P) mips
```

`P = side / 16` reproduces both of bungo's numbers exactly: an 8×8 grid of
128-texel frames is a 1024 sheet and gets **8**; a grid of 256-texel frames is a
2048 sheet and gets **16**. His 8 on a 128 frame is therefore `M = 4` levels —
128, 64, 32, 16 — which is his "3 clean mips" reference.

Two amendments, both stated:

* **Per axis.** `padX = f(frameW)`, `padY = f(frameH)`. The gutter is then the
  same fraction of a short side as of a long one, and a narrow frame stops
  paying double. `P = side/16` on both axes makes the inner rect exactly 7/8 of
  the frame on both axes.
* **Floor 2, rounded up to even.** Floor 2 so every card ships at least two
  clean mips. Even so that `--card-half-aux`, which halves both sides of the
  three auxiliary sheets, still lands their gutter on a whole texel.

```
pad(side)   = max(2, side/16), rounded up to even
mips        = 1 + log2( min(padX, padY) )
```

| frame | padX, padY | mips | gutter at the last mip |
|---|---|---|---|
| 16×64 | 2, 4 | 2 | 1 texel |
| 32×32 | 2, 2 | 2 | 1 texel |
| 32×64 | 2, 4 | 2 | 1 texel |
| 48×64 | 4, 4 | 3 | 1 texel |
| 96×128 | 6, 8 | 3 | 1.5 texels |
| 128×128 | 8, 8 | **4** | 1 texel |
| 256×256 | 16, 16 | 5 | 1 texel |

The 128 row is bungo's anchor, arrived at from his words rather than fitted to
them. The 5th mip that bleeds today is simply not shipped.

### 3.2 The frame's aspect

The short side is the **smallest multiple of 16 texels whose inner rect is not
narrower than the silhouette pass one measured**, never below 16, never above
the long side. It grows the frame until the silhouette fits, so the loose axis
gets air and the binding one is never cropped — the same "never crop" property
the ratio ladder had, without its floor.

Quantising to 16 keeps the number of frame SHAPES small, which is what card
arrays need (an array holds only sets sharing family, grid and frame). Over the
19 trees: **7 shapes against 4** today, for a **2.5% smaller** total sheet area.

The long side is untouched — it is bungo's size ladder of 2026-09-06q
(`WW_IMPOSTOR_REF`, pure halving, three rungs, floor 32).

### 3.3 One scale, and the measurement margin

Unchanged in principle and it already matched his rule: pass one photographs all
N² views at the bound-sphere fit and takes the widest and tallest extent **from
the fixed centre** over all of them; pass two bakes every view at that one fit.
So the tree is the same size in every frame and every frame has one UV rect.

The 4% safety margin on that measurement is now **1%**. Pass one measures in
viewport pixels at the bound fit, so its error is about one viewport pixel =
0.24% of a half-extent on an 841-pixel window; 4% was seventeen times the error
and came straight off the tree's size in every frame. The dilated padding is the
real margin.

### 3.4 The contract fields, including the pivot offset

`kind: "card"` `.lodm`, `card` object:

| key | status | meaning |
|---|---|---|
| `center` | **existed, now documented for what it is** | float[3], the offset **from the object's pivot** (the NIF root, i.e. the reference's placement origin) to the card's centre, in object units. The bake points the camera at this point in every one of the N² views, so it is the projection of the frame centre in all of them. A reader places the quad at `pivot + center`. `0003a28b` = `[-12.81, -5.23, 1070.19]` |
| `half` | unchanged | float[2], the quad's half extents in object units, spanning the WHOLE frame, gutter included |
| `pad` | **NEW** | int[2], `[padX, padY]`, the gutter in texels on EACH side of every frame. The silhouette occupies the inner rect `frame − 2·pad`; `mips` is `1 + log2(min(pad))` by construction |
| `mips` | meaning changed | now the no-bleed cap, not "until a frame's short side spans 8" |
| `frame`, `oct`, `base`, `depthSpan`, `auxDiv`, `source` | unchanged | |

`kind: "cardArray"` gains `array.pad` for the same reason; an array is built
from the same PNGs and the same dilation as the per-card set, so it inherits
that set's padding and clean depth.

A set written before `pad` existed has none, and both readers fall back to
`max(4, longSide/16)` on both axes — which is what those sheets actually carry.

**Why the offset makes the transition still.** The quad is `pivot + center ±
(halfW, halfH)` and the silhouette inside it is the inner rect. The model's own
silhouette, seen from the same direction, occupies exactly that rect by
construction — the frame IS the crop of that view. So a reader that honours
`center` puts the card where the model was; a reader that treats `center` as
zero puts it lower by `|center|`, which for `0003a28b` is 1,070 units, most of
the tree's height. §4 gates exactly that, with the zeroed-offset control.

---

## 4. Before / after, and every gate

Both libraries are the same 19 Sanctuary trees, `OCT=8 TILE=128`, 19 baked /
0 failed. BEFORE = `scratchpad/images_20260909/gen/cards_trees19` (exe 19:35:14).
AFTER = `scratchpad/cardfit_20260909/cards_after` (exe 20:49:01).

### 4.1 Fill — the union of the 64 views' silhouette boxes, against the frame

| | min | median | max |
|---|---|---|---|
| x, before | 0.125 | 0.500 | 0.875 |
| **x, after** | **0.250** | **0.729** | 0.875 |
| y, before | 0.562 | 0.812 | 0.875 |
| **y, after** | **0.797** | **0.859** | 0.875 |

and the total sheet area went **DOWN 2.5%** doing it. Frame shapes 4 -> 7.
The two extremes moved like this:

| base | before | after |
|---|---|---|
| `0003e08d` TreeBlasted05 | 32x32, union 4 x 22 tx, **0.125** x 0.688 | 16x32, union 4 x 26, **0.250** x 0.812 |
| `000393cd` TreeBlasted02 | 32x64, union 11 x 54, **0.344** x 0.844 | 16x64, union 12 x 55, **0.750** x 0.859 |
| `0003a28b` TreeHero01 | 128x128, union 112 x 103, 0.875 x 0.805 | unchanged frame, union 112 x 106, 0.875 x **0.828** |

TreeHero01 was already at the ceiling and barely moves; the needle-shaped trees
are where the law was failing and where it now pays.

### 4.2 Cross-frame mip bleed — measured, at every shipped mip, with a control

| | shipped mips that bleed | control (padding stripped) |
|---|---|---|
| before | **4 sheets**: `0003a28b` 26/255 on 16 of 28 borders at mip 4; `0004a074` 15/255 on 10; `000d9ca8` 11/255; `000d9ca9` 9/255 | 18 of 19 bases bleed, up to 117/255 |
| **after** | **0**, on every border of every shipped mip of all 19 | 19 of 19 bleed, up to 127/255 |

### 4.3 The transition gate

`scratchpad/cardfit_20260909/transition_bounds.py`, run against a SECOND reader
of each model — its own declared per-shape `Bounding Sphere` fields, read through
`-no-gui dump` and merged — never against the bake that produced the number.

| base | `card.center` vs the model's own bound centre | the PIVOT (the zeroed control) | ratio |
|---|---|---|---|
| `0003a28b` TreeHero01 | 182.6 units = **14.1%** of r | 1,174.8 units = 90.9% of r | **6.4x** |
| `0004a074` TreeMapleForest2 | 72.6 = **8.0%** | 891.6 = 97.8% | **12.3x** |
| `00038599` TreeBlasted01 | 23.1 = **4.9%** | 454.6 = 96.7% | **19.7x** |

plus, on all three: `depthSpan` = 3·max(r, 1024) within 5%; the inner half
extents 85 / 93 / 95% of the model's own bound radius and never exceeding it.
**PASS, with the control failing on every one.**

**It is a discrimination test, not a one-texel test, and that is a limitation I
am naming rather than hiding.** The instrument is the file's DECLARED bounding
spheres; the renderer recomputes each shape's bound from the VERTICES
(`src/gl/glmesh.cpp`, `boundSphere = BoundSphere( verts )`), which is tighter and
differently centred, so the file's spheres can BOUND `card.center` but cannot
confirm it to a texel. The brief asked for one texel and I did not deliver it.

**Why it is not photographic, which the brief also asked for.** The render hook
cannot serve as a metric camera today, and this lane measured that:

* `WW_RENDER_DIST` had **no effect at all** — one tree photographed at
  half-heights 1,874 and 7,496 gave two BYTE-IDENTICAL PNGs, because
  `setDistance` sets `Dist` while the half-height is `Dist/Zoom` and the auto-fit
  leaves `Zoom` where it framed the bound sphere. **FIXED** in this lane (read
  back and correct, the way the impostor bake always did).
* `WW_RENDER_CENTER` **still has no effect** on the framing: two renders of one
  model with the look-at 500 units apart are byte-identical. NOT fixed.
* the scale is **not proportional** to `WW_RENDER_DIST`: a 512-unit cube written
  by `-no-gui new --cube --size 512` spans **547, 107 and 25 px** at
  `WW_RENDER_DIST` 500, 1000 and 2000. NOT fixed, cause not measured.

Until a camera can be pinned, no picture out of that hook carries a world-unit
number, so the transition is gated geometrically and PICTURED as geometry
(`pics/transition_*.png`: the card's rectangle at `pivot + center`, the model's
own bound sphere, and the control at the pivot, all drawn to scale in world
units).

`WW_RENDER_CLEAN=1` was added while chasing this: the model alone, no viewport
grid, no axis lines, no node markers. The grid alone put a measured silhouette
box at the full width of the window.

### 4.4 What it costs: more card arrays

A card array holds only sets sharing family, grid and frame, so seven frame
shapes instead of four is more binds. Measured on the FO4CS sample set's dim-16
chunk, the same 18 card sets both times:

| | groups | arrays written |
|---|---|---|
| before | 4 (1024x1024, 768x1024, 256x512, 256x256) | 16 |
| after | **7** | **28** |

Twelve more array textures for the same library, against 2.5% less sheet area
and the fill numbers of 4.1. Stated because it is the one thing the old coarse
ladder was buying, and it is now being spent.

### 4.5 The harnesses

* `tests/spells/lodgen_impostor_cards.sh` — **PASS**, 14 checks. New: the
  `_fs.DDS` is DXT5 with vanilla's own four header numbers, and its alpha block
  is present and reads opaque on an opaque card (a BC1 sheet has no alpha block
  at that offset at all).
* `tests/spells/lodgen_octahedral.sh` — **PASS, 70 checks, 0 failures**
  (`scratchpad/cardfit_20260909/oct_harness.log`). New: the frame law as
  multiples of 16; the
  padding derived per axis and checked against the sidecar's own `pad` line; the
  padding clear on all four sides; the silhouette filling the inner rect's long
  axis; at most one rung of air on the short axis; cross-frame bleed at every
  shipped mip with the padding-stripped control that must fail; the `.lodm`'s
  `pad` and `mips`.

---

## 5. The alpha fix, and `--card-half-aux`

### 5.1 `_fs.DDS` — the card quads drew as opaque squares

Lane IMAGES5 found it and named the base; this lane measured the cause and fixed
it. The crossed-quad sheet went through `lodgenWriteDds` with the writer's
DEFAULT arguments, so `bc3 = false` and `bc1Alpha = false`: **BC1, `pfflags 0x4`,
no `DDPF_ALPHAPIXELS`, no alpha in the payload at all.** The alpha IS the cut-out
on that sheet, so a reader had nothing to cut on.

Vanilla's own alpha-tested tree LOD textures, read from the corpus:

| file | fourCC | dwFlags | pfflags | caps | size |
|---|---|---|---|---|---|
| `Textures/LOD/Trees/MapleBranchesLOD_d.dds` | DXT5 | 0x000A1007 | 0x4 | 0x401008 | 256x256, 9 mips |
| `Textures/LOD/Trees/ElmBranchesLOD_d.dds` | DXT5 | 0x000A1007 | 0x4 | 0x401008 | 256x256, 9 mips |

`lodgenWriteDds( …, bc3 = true )` emits exactly those four header numbers, so the
fix is one argument and the format now matches vanilla's byte for byte. Measured
on `0003a28b_fs.DDS`: **DXT1 -> DXT5**, 1,014,600 -> 2,029,072 bytes at 3014x501,
8 mips. The gate reads all four header fields and then the alpha block itself —
a BC1 sheet has no alpha block at that offset, so the check cannot pass on the
old output.

**Measured in the FILE, which needs no camera.** BC1 without
`DDPF_ALPHAPIXELS` has no alpha to decode at all — a reader gets 255 everywhere,
and 255 everywhere IS an opaque square. Decoding the BC3 alpha blocks of the new
sheet:

| | format | alpha |
|---|---|---|
| before | DXT1, `pfflags 0x4` | **none** — no channel, no `DDPF_ALPHAPIXELS`, a reader gets 255 |
| after | DXT5 | **98.3% of the quad is cut away** at a 0.5 alpha test; only 460 of 95,004 blocks are fully opaque |

That is the quad going from a solid rectangle to a tree-shaped cut-out, measured
on the bytes. **What is still OWED is the picture**: the chunk drawn with the new
sheet, which needs a camera this lane has shown is not pinned (§4.3), and
ultimately bungo's own eyes in the game.

### 5.2 `--card-half-aux` — confirmed OFF in tonight's sample set

`scratchpad/handoff_fo4cs/samples/make_samples.sh` runs
`--arrays --impostors "$CARDS" --cover` and **never `--card-half-aux`**, so all
four sheets of every sample card set are full size. That is the 2026-09-06
ruling's own default (bungo: *"should our impostors use half the res for normal
and other map, and full resolution for diffuse? Maybe make it a toggle."* — a
toggle, off by default, because the base colour's alpha is the coverage and
therefore the silhouette). The sample set was correct; nothing said so.

Applied: both drivers now carry the toggle **by name**.
`tools/bake_impostor_cards.sh` takes `HALF_AUX=1` and records the whole run —
`oct`, `tile`, `ref`, `half_aux`, `candidates` — in `<outdir>/library.txt`, so
whoever converts the library later does not have to remember what it was baked
for; `make_samples.sh` takes the same variable and passes `--card-half-aux`
through. Off by default in both.

The new padding law makes half-aux safe where it was previously only *assumed*
safe: the padding is rounded UP TO EVEN so a halving lands the aux gutter on a
whole texel, and `auxMips = 1 + log2(min(padX,padY)/auxDiv)` comes down with it.

### 5.3 The sample set

Regenerated over the new library (`scratchpad/cardfit_20260909/cards_after`) at
all four levels, same region (cells 0..3 x 0..3), same flags, `--slot-fallback`
at 16 and 32 only. MANIFEST rows re-measured from the files on disk.

---

## 6. Per-frame positioning — measured, NOT shipped

bungo's rule says *"the tree is equal in size on each one"*, so one scale per
card is the design and that is what shipped. This is the measurement of what a
per-frame rect would be worth, for his decision.

At one scale the frame has to hold the UNION of the 64 silhouette boxes. Any one
view is smaller than that union, and the difference is air:

| the median view's box, as a fraction of the union box | min | median | max |
|---|---|---|---|
| over the 19 trees | 0.37 | **0.52** | 0.68 |

**So the median frame spends about 48% of its inner rect on air that only the
widest view of that tree needs.** The spread within one card is large — `0004a074`
runs from a 29-texel-wide view to a 68-texel-wide one on the same 96x128 frame.

**Fields it would need** (`kind: "card"`, and per layer on `cardArray`):

| key | type | meaning |
|---|---|---|
| `frameRect` | int[4] x N² | per frame, the sub-rect of the frame the silhouette actually occupies, in texels: `[x, y, w, h]`. The reader's UV rect becomes this instead of the whole frame |
| `frameHalf` | float[2] x N² | per frame, the quad's half extents in model units for THAT view — because the quad must shrink with the rect or the tree grows |
| `frameOffset` | float[2] x N² | per frame, the quad's centre offset from `center` in the view's own plane, since a tighter box is no longer centred on the card's centre |

That is 8 numbers a frame, 512 a card at OCT=8 — a few kB of JSON, which is
nothing beside the texels — but it costs the reader its simplest property: today
one quad and one UV rect serve every direction, and blending three neighbouring
frames means blending three rects of one shape. With per-frame rects the blend
weights and the quad both become per-frame.

**Not shipped, and not recommended without his word.** The saving is real (about
half the inner rect on the median frame) but it is spent on the frames a card is
LEAST often seen through, and it makes the three-frame blend that gives an
impostor its parallax noticeably harder. His call.

**The recentring question, answered and closed.** §1.2 measured the union box's
centre against the frame's centre: within ONE texel in x and 3.5 in y over 19
bases. A better single centre is worth at most 3% on one axis of one card, and
`center` must stay a single 3D point (the camera looks at it in every view), so
there is no cheap win there. That candidate is dead with numbers.

---

## 7. Mistakes

All four are in `MISTAKES.md` at the repo root, written when each was
recognised. In short:

1. **Patched source from a bash heredoc instead of a script file — four times**,
   the last costing a ten-minute harness run. The `nifskope-ww-build-verify`
   skill's first bullet forbids exactly this and was read at the start of the
   lane. The fourth occurrence put an escaped apostrophe into a Python string
   inside `lodgen_octahedral.sh`; the block died with a SyntaxError, its checks
   never ran, and the suite printed `RESULT FAIL` that read like a failing check.
   The tell was the `ok` COUNT dropping from 68 to 47, not the verdict.
2. **Measured FILL alone in an earlier CARDFIT launch.** A single ratio cannot
   separate a frame of the wrong SHAPE, a gutter of the wrong SIZE and a frame
   off CENTRE. The per-view bounding BOX and their union separate all three and
   killed the bounding-sphere hypothesis in one pass.
3. **Wrote a harness check against the wrong artefact.** "The transparent texels
   carry dilated colour" read the BAKE's PNG, which is un-dilated by design, and
   failed on correct output — while the harness already measured the property in
   the right place four checks further down.
4. **Carried a tolerance from the brief into a gate without measuring whether
   the code could meet it.** "Fills the long axis within 2%" reads 89.3% at a
   64 px frame and 94.6% at 128, and the cause is not a defect: pass one measures
   in viewport pixels and pass two downsamples into the frame. Worse, a
   long-axis floor could never have caught the defect this lane exists for. It
   was replaced by a check that DOES: at most one 16-texel rung of air on the
   short axis, which the old five-rung ladder cannot pass.

---

## 8. Skill review

**Loaded:** `nifskope-ww-build-verify` (the gated chain, the exe rename, the
`test exe -nt source` gate, `cmp` on the stylesheet — all used, twice).
`nifskope-ww-lodgen` was the right skill for the harness rules and I used its
one-instance / second-monitor / `--port` discipline throughout.

**Amended** (live tree `E:\Projects\Claude\.claude\skills\nifskope-ww-lodgen`;
the repo's own `.claude/skills` does not carry this skill, so there is nothing
to reconcile there): a new section, *"Compile a harness's embedded Python BEFORE
running the harness"*, with the six-line `re.findall` + `compile()` gate and the
rule that a suite whose `ok` count DROPPED is a broken block until proved
otherwise; and the heredoc trap sharpened to name the escaped-apostrophe case,
which is not obviously a "backslash" to anyone reading the old wording.

**Wished had existed, and why each will recur:**

* **A skill for "pin a camera on this renderer".** This lane lost its largest
  block of time to the render hook, and everything it found is reusable: the
  ortho half-height is `Dist/Zoom` and `setDistance` must be read back;
  `WW_RENDER_CENTER` does not work; the scale is not proportional to
  `WW_RENDER_DIST`; `-no-gui new --cube --size N` is the calibration object;
  `WW_RENDER_CLEAN=1` (added here) is what makes a silhouette measurable at all.
  The next lane that needs a measured picture will re-derive all of it. **I did
  not write it, deliberately**: three of those five facts are about a hook that
  is still broken, and a skill written now would be a skill about a bug. The
  right sequence is to fix `WW_RENDER_CENTER` and the scale first and write the
  skill against a hook that works — named here so it is not lost.
* **`ww-sheet-metrics`** — the union-of-views box, the fill ratios, the mip-bleed
  metric and its padding-stripped control are four scripts that will be wanted
  again for every atlas and sheet this generator writes, not only for cards.
  They are in `scratchpad/cardfit_20260909/` and parameterised by the sidecar,
  so promoting them is mostly moving files. Worth doing when a second sheet
  format needs them; one use is not yet a procedure.

**Declined:** a skill for the frame law itself. It is a contract, not a
procedure, and it lives in `docs/LODGEN_CARD_SHEETS.md` 3.1-3.2 where a reader
of the format will actually find it.

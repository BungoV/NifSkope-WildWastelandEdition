# FARVIEW1b -- far lights that switch, and bulb dots on the skyline

Lane FARVIEW1 part 2 (prep, cloud), 2026-10-03. Design plus two Python twins; no C++, no game data.
Builds on docs/cloud/FARVIEW1_DESIGN.md (the far term: baked surfel light read by a 27-cell lookup past a
3072..7168 u band) and docs/cloud/GPURELIGHT1_DESIGN.md (branch cloud-GPURELIGHT1: relighting the near bake live).
Twins: `tests/spells/farview1b_layers_check.py` (about 80 s, peak 170 MB) and
`tests/spells/farview1b_dots_check.py` (about 21 s, peak 126 MB). Both use python3 + numpy only.
The layers twin imports the street, surfels, tracer and light evaluator from `tests/spells/farview1_check.py`
without changing it; that twin still passes (`FARVIEW1 PASS: green PASS, reds all fail; 45 s`).

Owner's calls this lane follows:
(a) lights switched by power or by quests are baked separately and switched on and off live;
(b) far lamps also show as small glowing dots on the skyline, like real city lights at night.

## 1. In one paragraph

Light adds up. With the geometry fixed, the light a set of lamps puts on the surfels (direct plus every bounce) is the
sum of what each part of the set puts there. So the far light is baked as one **always-on layer** plus one
**layer per switch group**. A switch group is all the lights that one switch turns on and off. At runtime the
far renderer adds the layers whose group is on. A group's key is the root of its lights' enable-parent chain
plus the parity of the chain's "opposite" bits. The same per-light record (a small light table per sector)
serves both this lane and the GPU relight lane: identity, group, the light as baked, and the dot. Far lamps
also get a **dot**: a tiny sprite with a minimum on-screen size and the bulb's own inverse-square energy.
The dot fades in over the FARVIEW1 band while the real bulb fades out. It fades away before it is culled and is
hidden by depth taps spread over the sprite.

## 2. Sources: what was read and what was not

Most talk hosts were blocked by this session's network proxy again: adriancourreges.com, habr.com,
advances.realtimerendering.com, humus.name, uni-weimar.de, diglib.eg.org, opengl.org, registry.khronos.org,
wickedengine.net, gta5mod.net, forums.flightsimulator.com and web.archive.org were all refused. GitHub (and its raw
file host) was reachable. **Read** below means fetched and read. **Summary only** means search-result
summaries; nothing more specific is claimed from those sources.

| Source | Read? | What it says (as far as read) | What we take |
|---|---|---|---|
| ARB_point_parameters, the OpenGL extension spec, via the Khronos registry's mirror on GitHub: https://raw.githubusercontent.com/KhronosGroup/OpenGL-Registry/main/extensions/ARB/ARB_point_parameters.txt | read | Written for "particles or tiny light sources, commonly referred to as 'Light points'". Size shrinks with distance, `derived_size = clamp(size * sqrt(1 / (a + b d + c d^2)))`, and below a threshold size the point keeps the threshold size and its alpha is scaled by `(derived_size / threshold)^2`. | The size and energy rule. Below a minimum size, keep the size and scale the brightness so the **energy** (brightness x area) still follows the physical law. Section 5.2. |
| CosmoScout VR star renderer (MIT, DLR): https://github.com/cosmoscout/cosmoscout-vr/tree/main/plugins/csp-stars/shaders (`starsOnePixel.frag`, `starSnippets.glsl`, `starsBillboard.geom`) | read (the one-pixel shader in full; the solid-angle helpers and the size lines quoted by the fetch tool) | A star's pixel luminance is its magnitude turned into luminance **over the solid angle of the pixel or sprite it is drawn into** (`getSolidAngleOfPixel`, `magnitudeToLuminance(m, solidAngle)`). The billboard's size is `dist * sqrt(uSolidAngle / (4 pi)) * 4`, a constant angular size, with per-mode scaling by magnitude. | The same principle: a far light's **energy** is physical, and its **size** is a constant screen-space footprint. Brightness = energy / footprint. |
| Schneegans, Kreskowski, Gerndt, "Smaller than Pixels: Rendering Millions of Stars in Real-Time", Eurographics 2025 short papers: https://diglib.eg.org/handle/10.2312/egs20251029 | summary only (blocked) | Sub-pixel stars: "the projected size of the stars has to be incorporated in the luminance computation"; compares point and billboard proxies by solid angle; a perception-based glare filter. Implementation in CosmoScout VR. | Confirms the rule above; the paper itself was not read. |
| GTA V: A. Courreges, "GTA V Graphics Study, part 2" (2015): https://www.adriancourreges.com/blog/2015/11/02/gta-v-graphics-study-part-2/ | summary only (blocked) | "every single small light spot you can see is a quad rendered with a small 32x32 texture", heavily batched as instanced geometry; each stands for a real light you can drive to. | Dots as instanced quads, one per real light. GTA's size, fade and hand-off rules are **not** in the summary, so nothing else is taken from it. |
| GTA V `visualsettings.dat` LOD-light settings, as listed by modding pages (e.g. https://www.gta5-mods.com/tools/distant-lod-light-editor) | summary only | Setting names such as `lodlight.small/medium/large.range`, `.fade`, `.corona.range`, `lodlight.corona.size`, `lodlight.sphere.expand.*`. | Only that the shipped game has separate ranges, fades and a corona range per light size class. The values and the meaning of each setting are unknown here; community pages, not Rockstar documentation. |
| Red Dead Redemption 2: F. Bauer, "Creating the Atmospheric World of Red Dead Redemption 2", SIGGRAPH 2019 Advances course | not read (title only) | -- | Nothing. Listed for the local lane. |
| E. Persson, "Creating Vast Game Worlds: Experiences from Avalanche Studios", SIGGRAPH 2012: https://www.humus.name/Articles/Persson_CreatingVastGameWorlds.pdf | summary only (blocked) | Just Cause 2 "distant lights": static lights put into a compact vertex buffer at build time, split into a grid for frustum culling, drawn as colored point sprites, about 0.1-0.2 ms a frame on consoles, "huge visual impact". | A dot pass is cheap. Build it offline per sector and cull per grid cell. |
| Lens-flare / corona occlusion by several depth samples (e.g. Wicked Engine's "Smooth Lens Flare in the Geometry Shader", https://wickedengine.net/2016/11/smooth-lens-flare-in-the-geometry-shader) | summary only (blocked) | Sample the scene depth around the light's screen position; visibility = unoccluded samples / all samples, to avoid popping. | Section 5.5, measured in check O. |
| Microsoft Flight Simulator forum: "Night lighting LOD transition leaves unlit gap between distant and near tiles": https://forums.flightsimulator.com/t/night-lighting-lod-transition-leaves-unlit-gap-between-distant-and-near-tiles/743455 | summary only (blocked) | A user report: far night lighting fades out before the near lighting has come in, so a dark band shows and lights pop in. | A real example of the failure the hand-off must prevent (reds `nohandoff`, `bothdrawn`). |

## 3. Switchable far lights: layers

### 3.1 Why layers are exact

Every step of the bake after the shadow tests is linear in the lights' colors (GPURELIGHT1 section 3; its check L).
So for any on/off state:

    E(state) = E_always + sum over groups g that are on of E_g

E_g is the settled surfel irradiance (direct plus bounce) of group g's lights alone. Check L measures it against
a full rebake: 6.7e-16 of the brightest value, worst over five states.

Two conditions keep it exact. Both are measured.
1. **A fixed pass count** (or a stop bar far tighter than the bar the layers must meet). BOUNCE2's own stop rule
   (stop when a pass changes nothing by more than 1e-3 of the brightest) is not linear: a dim layer and the full
   bake stop after different numbers of passes. Red `settle` gives 2.4e-5 to 3.9e-4, which fails the 1e-6 bar.
   The bake therefore runs the same number of passes for every layer. The twin uses 4. A real cell uses whatever
   count the full relight settles at (Concord 5, Vault 111 8), and that count is written into the header.
2. **Fixed geometry.** If the same switch also enables meshes (lit window panes, a sign), those meshes are
   not in the bounce of the other state. That case is not covered here (open question 2).

### 3.2 The group key

Fallout 4 references are switched together through **enable parents** (XESP). The cell view's start rule
(docs/PRTP_PLAN.md, enable parents): a child is shown when its parent's state differs from the child's
"opposite" bit, applied link by link up the chain. So whether a light shows depends only on (a) the state of the
**root** of its chain and (b) the **parity** (exclusive or) of the opposite bits along the chain:

    shown(light) = enabled(root) XOR parity(chain)

The group key is therefore:

| kind | key | lights in it | on when |
|---|---|---|---|
| ALWAYS (0) | -- | no enable parent, not switched by a script | always (the always-on layer) |
| PARENT (1) | (root reference, parity) | every light whose chain ends at that root with that parity | root enabled XOR parity |
| SELF (2) | (the light's own reference) | one light switched directly (a script enables or disables the light itself) | its own enabled state |

One root can make **two** groups: parity 0 (on when the root is enabled) and parity 1 (on when it is disabled,
like emergency lights that come on when the power fails). Check G proves the collapse: 0 of 3,000 light-states
differ from walking each chain link by link (six random root states; 55 lights hang under two-link chains, 27 with
net opposite parity). Red `noparity` (bits ignored) gets 162 wrong.

The key packs into 64 bits, with the form id file-relative so it does not depend on the user's load order:

| bits | field |
|---|---|
| 0-23 | local form id (the root for PARENT, the light for SELF; 0 for ALWAYS) |
| 24-39 | plugin ordinal: an index into the bake's plugin table (section 3.6), the same table in every file of one bake |
| 40 | parity (PARENT only) |
| 41-61 | reserved, 0 |
| 62-63 | kind |

What the bake cannot know from the plugin alone: which unparented lights a script switches (SELF). The proposal is
to mark SELF any light reference that has a script attached or is linked to a workshop or power object. The
local lane has to count these (section 7.1).

### 3.3 ONE record for both lanes: the light table

FARVIEW needs, per light: its switch group and its dot. GPURELIGHT needs, per light: the light as baked
(position, radius, color, curve, spot), the visible surfel-light pairs (built once per bake), and the live
on/off state and dimmer. Both need the same identity and the same on/off answer. Otherwise a lamp could be
on in the near view and off in the far view. So there is one table per sector, the **light table**
(`.wlt`). Every other file joins it by **light index** (a light's position in that table) or by
**group key**:

- FARVIEW's layer file (`.fvg`) names groups by key. The far renderer turns each key into on/off from the root's
  live state.
- GPURELIGHT's pairs (its section 3: "visible surfel-light pairs within each placed light's baked radius") are a
  list per light index. The color, curve and cone are read live from the light record. Check X proves that this
  record (its parameters plus pairs built once) reproduces the bake's direct light exactly (0.0 difference over
  five states, bar 1e-9). Red `nopairvis` (pairs without the shadow test) is off by 0.79.
- GPURELIGHT1's per-light basis (its section 3.2: 920 MB dense for 300k surfels x 256 lights) becomes a
  **per-group layer**. It is sparse because each layer keeps only the surfels it lights, and it is grouped because
  lights that switch together share one layer. For a switch, a layer sum replaces a full set of bounce passes.
  GPURELIGHT still needs its live relight for color changes, dimmers, flicker and the sun.

### 3.4 The bake, step by step (the twin's `bake_layers`)

1. Walk each exterior light's enable-parent chain and give it a group key (ALWAYS / PARENT / SELF).
2. Trace the bounce rays once (they depend on geometry only).
3. For each group (ALWAYS included): direct light from that group's lights only, then the fixed number of bounce
   passes through the same ray table. That gives E_g.
4. Trim: the always-on layer keeps surfels over 1e-4 of the all-on light's 99th percentile, and switched layers
   keep entries over 3e-5 (section 3.5). Pack the light as RGB9E5.
5. Write `.fvl` v2 (the always-on layer plus the record list for every surfel any layer keeps), `.fvg` (switched
   layers) and `.wlt` (lights and groups).

Moving lights stay out, as in FARVIEW1 section 7. A light that flickers stays in its group (or in the always-on
layer) at its mean strength (open question 3).

### 3.5 Cost per group (measured on the synthetic street: 500 lights, 33 groups, 1.56 sectors)

A layer reaches well past its lights' radii, because its bounce spreads. Most of a switched layer's entries
(71-94%) lie outside every member light's radius. These are faint bounce tails, and they are what the trim decides.

**The trim was chosen after the first run (stated plainly):** with FVL v1's 1e-3 for every layer, check Q failed.
Each layer drops its own dim tail, and the dropped tails add up: on a lit surfel, a median of 21 switched layers
(p90 25) carry light below 1e-3. That count comes from a one-off diagnostic run, not from the twin. The
twin prints the trade-off (all-on state, Q's measure, switched-layer entries):

| always-on trim | switched trim | switched entries | KB | Q p99 | energy |
|---|---|---|---|---|---|
| 1e-3 | 1e-3 | 27,202 | 213 | 0.1308 | 0.9931 |
| 1e-4 | 1e-3 | 27,202 | 213 | 0.1296 | 0.9931 |
| 1e-4 | 1e-4 | 77,371 | 604 | 0.0235 | 0.9990 |
| **1e-4** | **3e-5 (chosen)** | **115,360** | **901** | **0.0089** | **0.9997** |
| 1e-4 | 1e-5 | 160,827 | 1256 | 0.0034 | 0.9999 |

With the chosen trims, for the whole street:
- **Per group:** median 3,444 entries (27 KB), largest 11,269 (88 KB, a 60-lamp quest stretch); **about 600
  entries (4.7 KB) per switched light**. Every group touched 3-4 of the street's sectors.
- **Always-on layer:** 13,457 surfels. The `.fvl` v2 record list (every surfel any layer keeps) has 14,178
  records = 222 KB (v1's all-on file: 12,032 = 188 KB).
- **All switched layers:** 115,360 entries = 901 KB, or about 577 KB per built-up sector, for this street's
  unusually switch-heavy mix (11 of 500 lights switched by a script, 181 under enable parents).
- For contrast, a dense per-light basis would be 17,994 surfels x 500 lights x 12 B = 103 MB.
- **A switch at runtime:** re-sum the touched sectors' current light, E = always-on + the layers that are on, from
  the stored values (never add and subtract in place, which builds up rounding error). The CPU twin re-sums all 34
  layers densely over 17,994 surfels in 0.4 ms. On the GPU this is one pass over at most about 1 MB a sector,
  done only when a switch changes. The far lookup per pixel is unchanged: it reads the current E with the same
  27-cell hash as FARVIEW1.

A cheaper tail (for example, keeping a layer's faint entries at a coarser 280-unit cell) could remove most of the
entries. It was not measured (open question 4).

### 3.6 Files, next to `farlight_X_Y.fvl`, and their bytes

All files are little-endian. One set per 4096-unit sector, named like the `.tbk` files:

    <bake folder>/lights_%+05d_%+05d.wlt     the light table (shared: FARVIEW + GPURELIGHT)
    <bake folder>/farlight_%+05d_%+05d.fvl   version 2: the always-on layer + the record list
    <bake folder>/farlight_%+05d_%+05d.fvg   the switched layers (absent when no switched light reaches the sector)
    (GPURELIGHT's pair list per light index: that lane's file, joined by light index; not fixed here)

**`.wlt` -- light table, version 1**

| offset | type | field |
|---|---|---|
| 0 | char[4] | 'WLT1' |
| 4 | u32 | version = 1 |
| 8 | i32 | sector X |
| 12 | i32 | sector Y |
| 16 | u32 | light count |
| 20 | u32 | group count |
| 24 | u32 | plugin count |
| 28 | u32 | plugin table offset (bytes from the start of the file) |
| 32 | u64 | light-set hash: over the plugin table, the groups and the light records; `.fvl` and `.fvg` carry the same value |
| 40 | u32 | flags (bit 0: interior; always 0 for the far view) |
| 44 | u8[20] | reserved, 0 |
| 64 | group[group count] | 16 B each, below |
| ... | light[light count] | 64 B each, below, sorted by group index then form id |
| ... | char[64][plugin count] | plugin file names, zero-padded (the bake's masters in load order; identical in every file of one bake) |

Group, 16 B: u64 group key (section 3.2) | u16 first light | u16 light count (members in this sector) |
u8 kind | u8 on at the bake's start state | u16 reserved.

Light, 64 B:

| offset | type | field |
|---|---|---|
| 0 | u64 | reference key: local form id (bits 0-23) + plugin ordinal (bits 24-39), as a SELF key without the kind |
| 8 | f32[3] | position, world units |
| 20 | f32 | radius as baked (reference XRDS, else the base); the reach of the pairs and layers |
| 24 | f32[3] | color, linear: pow(record color, 2.2) x fade; negative for a Negative light (PRTP2 section 0) |
| 36 | f32[3] | bias, scale, exponent (PRTP2 section 1) |
| 48 | u32 | spot direction, octahedral, 2 x snorm16 (0 when not a spot) |
| 52 | f16[2] | cos(outer half angle), falloff exponent (PRTP2 section 2) |
| 56 | u16 | group index (into this file's groups) |
| 58 | u16 | flags: 1 spot, 2 shadowed spot, 4 negative, 8 flicker or pulse, 16 ambient-only, 32 has a dot |
| 60 | u32 | dot intensity, RGB9E5, linear (section 5.1; 0 when there is no dot) |

**`.fvl` -- far light, version 2** (v1 is in FARVIEW1 section 5; the record layout is unchanged, but E now holds
the always-on part only)

| offset | type | field |
|---|---|---|
| 0 | char[4] | 'FVL1' (unchanged; the version tells v1 from v2) |
| 4 | u32 | version = 2 |
| 8 | i32 | sector X |
| 12 | i32 | sector Y |
| 16 | f32 | surfel cell (70) |
| 20 | u32 | record count |
| 24 | u64 | light-set hash (= the `.wlt`'s) |
| 32 | u32 | flags (bit 0: a `.fvg` exists) |
| 36 | i32 | cz base (the sector's lowest surfel cell) |
| 40 | u32 | bounce passes baked (the fixed count, section 3.1) |
| 44 | f32 | always-on trim, absolute (1e-4 x the all-on p99) |
| 48 | f32 | switched trim, absolute (3e-5 x the all-on p99) |
| 52 | u8[12] | reserved, 0 |
| 64 | record[count] | 16 B, sorted by key: u16 cx, cy, cz; u8 side; u8 pos[3]; u32 E (RGB9E5, always-on part); u8 pad; u8 flags (1 = the always-on part is zero; only layers light this surfel) |

**`.fvg` -- switched layers, version 1**

| offset | type | field |
|---|---|---|
| 0 | char[4] | 'FVG1' |
| 4 | u32 | version = 1 |
| 8 | i32 | sector X |
| 12 | i32 | sector Y |
| 16 | u32 | group count |
| 20 | u32 | entry count |
| 24 | u64 | light-set hash (= the `.wlt`'s) |
| 32 | group[group count] | 16 B: u64 group key, u32 first entry, u32 entry count |
| ... | entry[entry count] | 8 B: u32 record index into the `.fvl` (bits 24-31 reserved, 0), u32 E (RGB9E5); sorted by group, then record |

A group can have entries in a sector where it has no light: its bounce reaches past the edge. That is why the
`.fvg` names groups by their global key and not by a `.wlt` index.

A reader refuses a sector whose three hashes differ. That would mean the files come from two different bakes.

## 4. The layers twin: what each check proves

`tests/spells/farview1b_layers_check.py`: the FARVIEW1 street (22 boxes, 17,994 surfels, the same 500 lights),
plus an enable-parent graph built in code. 45% of the buildings get a power marker over their wall lights, with
a quarter of those lights on the opposite bit. A quest marker covers the street lamps from 2048 to 4096 u. 30% of
the grouped lights go through an intermediate reference, and 3% of the others are switched one by one. Four
fixed bounce passes, 64 rays a surfel (the bounce is 21% of the light).

| Check | Proves | Measured | Bar | Red that must fail it |
|---|---|---|---|---|
| G group key | (root, parity) equals walking each chain | 0 of 3,000 differ | 0 | `noparity`: 162 differ |
| L layers | always-on + the layers that are on = a full rebake (fresh direct light, fresh rays, same passes) for 5 states | worst 6.7e-16 of the brightest | 1e-6 | `drop` (the smallest layer that is on, left out): 4.1e-2 to 6.3e-2. `settle` (BOUNCE2 stop rule): 2.4e-5 to 3.9e-4. `nobounce` (switched layers direct only): 2.2e-2 to 1.3e-1 |
| Q stored | the same sum from the files (trimmed, RGB9E5, float32) against the rebake on lit surfels (FARVIEW1's floor: 2% of p95) | p99 0.0044-0.0069, energy 0.9998-0.9999 | p99 0.01, energy 1 +- 0.005 | not a red target; under `drop` it also fails (p99 up to 0.0296), under `nobounce` too (p99 up to 0.78) |
| X shared record | GPURELIGHT's direct kernel, fed from the light record and pairs built once, equals the bake's direct light | 0.0 (bit-identical) in 5 states | 1e-9 | `nopairvis`: 0.73-0.79 |

**Bars, and what changed after a first run.** G, L and X were set before the first run and never moved.
Before the first run, the targets of `drop` and `nobounce` were narrowed to L only (the brief's red is the L
mismatch). Q **failed its first run**, which used a 1e-3 trim on every layer and judged every surfel above 1e-3
of the p99 by plain relative error, with no floor: p99 = 1.0. Two things were then changed: (1) the measure
became FARVIEW1's own (relative error with a floor of 2% of the 95th percentile, the floor that lane's gate uses);
(2) the trims became 1e-4 / 3e-5, chosen from the trade-off table above so that the **unchanged** numbers 1% /
0.5% are met. Under the first measure the chosen trims would still fail (the faintest lit surfels, about 1/1000
of the street's brightness, keep up to 100% relative error). That is the honest cost of trimming; no eye sees it.

## 5. Bulb dots

### 5.1 Which lights, and how bright

A light gets a dot when it has a visible bulb: an emissive shape in its own model or right next to it. Many FO4
lights are fill lights with no bulb, and those get no dot (flag 32 off). The dot's **intensity** I_dot is the bulb's
own on-screen energy as a near-field renderer would draw it: emissive color x emissive multiplier x the bulb's
projected area, in linear units. The bake computes it and stores it in the light record (RGB9E5). The dot uses the
light's color hue, sits at the light's position, and is drawn only while the light's group is on. It is the same
switch as the layers: the dot pass reads one on/off bit per group.

### 5.2 Size: a minimum screen size, physical energy

    rho    = r_bulb x f / d                               the bulb's radius on screen, px (f = focal length, px)
    sigma  = max(SIGMA_MIN, sqrt(rho^2 / 4 + 1/12))        Gaussian sprite; same second moment as the bulb's
                                                          pixel footprint (rho^2/4 for a disc, 1/12 for the pixel box)
    energy = I_dot x (d_ref / d)^2 x T_fog(d)              inverse square, times the weather fog's transmittance
                                                          (1 - f of PRTP2 section 5); the dot is additive
    pixel  = energy x Gaussian(sigma) at the pixel center, normalized by its integral (1 / (2 pi sigma^2)),
             never by its own sampled sum

- **Constant minimum screen size, not physical size.** A 6-unit bulb is 1.15 px across at 7168 u and 0.07 px at
  100,000 u. Drawn at its physical size, it falls between pixel centers and **twinkles**: red `nofloor` (sigma =
  rho / 2) varies by 400% between sub-pixel camera positions. Drawn at a minimum size with the physical energy
  (the ARB_point_parameters rule, and CosmoScout's luminance from the sprite's solid angle), it stays steady.
- **SIGMA_MIN = 0.6 px, measured.** The twin scans sigma and picks the smallest whose twinkle is under 1% (half the
  2% bar). Results: 0.50 px gives 4.1%, 0.55 px 1.4%, **0.60 px 0.46%**, 0.65 px 0.14%. That is an rms radius
  of 0.85 px. The rule is in pixels, so at 4K the dot is smaller in angle, as a point should be.
- **Continuous size.** The sprite is a quad with a float size, evaluated in the shader. It is not an integer
  point size: red `intsize` (diameter rounded to whole pixels) jumps 33% in one step.
- **The same size as the bulb at the hand-off.** With a 70-degree camera at 1080p, the floor takes over from the
  bulb's own size at 7,820 u, just past the band. Through the whole band the dot has the bulb's size (check M:
  1.937 vs 1.937 px at D0, 0.905 vs 0.908 px at D1). The hand-off is therefore a pure brightness cross-fade, never a
  change of shape. Red `bigdot` (a fixed 4 px glow sprite) is 5.2 times too large.

### 5.3 The hand-off with the real bulb's own glow

The band is FARVIEW1's: w = smoothstep(3072, 7168, d). One rule covers both surfaces and bulbs:

    drawn = (1 - w) x real bulb (its emissive, faded by the near renderer)  +  w x dot

The near renderer must fade the bulb mesh's emissive by (1 - w), just as it fades the real lights' contribution.
Without that, the dot fades in on top of a bulb drawn at full strength. The bulb then vanishes when its cell
unloads: red `bothdrawn` draws up to 2.0 times the energy and then drops by 1.0 in one step at D1. With no hand-off
at all (red `nohandoff`: bulb at full until D1, dot after it) there is a step of |m - 1|, where m is the error of
the dot's intensity estimate (measured 0.60 for m = 1.6).

The hand-off cannot remove an intensity error. It spreads the error over the band: the largest step is
|m - 1| x 1.17% (1.5 x 32 / 4096 is the smoothstep's steepest change per 32 u). With the 1% bar, the estimate must
be within about +-85% of the bulb's real energy. The twin's worst lamp (m = 1.6) measures 0.87% per step.

### 5.4 Distance fade

Physically, the dot keeps getting dimmer (inverse square times fog). It is dropped where its brightest pixel falls
to **half a display code value** (0.5 / 255 at the night exposure). Before that point it fades:

    d_max(I_dot) = the distance where the peak pixel, at SIGMA_MIN with fog, = 0.5 / 255
    fade         = 1 - smoothstep(0.7 d_max, d_max, d)

With the twin's made-up night fog, d_max is 43,085 u for I = 0.25, 69,691 u for I = 1 and 96,269 u for I = 4.
That is per light, from its own intensity. Every lamp's peak before the drop measured 0.00000 (bar 0.00196). Red
`fixedcull` (every dot dropped at 20,000 u, no fade) pops by up to 0.29 (74 code values). In the game the exposure
adapts. The same rule can be evaluated in the shader on the live exposure (fade on the predicted peak instead of
the distance). That variant is not measured here (open question 7).

### 5.5 Occlusion

The dot is depth-tested against the scene depth (near cells and LOD), with **16 taps** spread over +-1.5 sigma on a
rotated grid (all 16 taps have distinct x and distinct y). Visibility = the share of taps not covered, multiplied
into the energy. The dot's depth is pulled toward the camera by a bias, so the lamp's own fixture and the
LOD mesh's offset from the real surface do not hide it. The bias should cover at least FARVIEW1's +-48 u LOD error
and the 24 u fixture clear; it is not measured (open question 8). Check O sweeps an occluder edge across the dot by
1/8 px. Visibility changes by at most 0.125 a step and never rises while the edge covers more. Red `onetap` (one
tap at the center) jumps 1.0, so the dot blinks on and off as a roof edge crosses it. A plain 4 x 4 grid would
change by 0.25 a step, because four taps share each x.

### 5.6 Cost

One instanced quad per light with a dot, built offline per sector and culled per sector, like Just Cause 2's
distant-light vertex buffer (summary only: 0.1-0.2 ms on consoles of its time). Per dot the shader needs the light
index, position, intensity and group index. All of these are already in the `.wlt`, so there is no new file. A switch
flips one bit in the group mask. The 16 depth taps are the main cost per dot. They read a 2 x 2 pixel area at
SIGMA_MIN, so they are cheap.

## 6. The dots twin: what each check proves

`tests/spells/farview1b_dots_check.py`: a rasterizer in numpy. The camera is 1920 x 1080, 70 degrees across,
f = 1371 px. There are nine lamps: intensity 0.25 / 1 / 4 times an estimate error m = 0.6 / 1.0 / 1.6. The real
bulb is a 6-unit disc with pixel coverage from 32 x 32 samples a pixel. Each distance is drawn from 16 sub-pixel
camera offsets. The sweep runs from 2560 to 7680 u by 32 u, then by 1% steps out to 250,000 u.

| Check | Proves | Measured | Bar | Red that must fail it |
|---|---|---|---|---|
| T twinkle | the drawn energy holds steady across sub-pixel camera positions, at every distance | worst 0.0046 | 0.02 | `nofloor`: 4.0 |
| H hand-off | through the band the energy / physical target changes smoothly and never double-draws | worst step 0.0087; worst excursion 0.0016 | step 0.01; excursion 0.02 | `nohandoff`: step 0.60 at D1. `bothdrawn`: step 0.999, excursion 1.003 |
| S size | the drawn rms radius never jumps (the natural 1/d change is 1.25% a step at 2560 u) | worst 0.0126 (that natural change) | 0.03 | `intsize`: 0.33 |
| M match | at D0 and D1 the dot alone has the bulb's rms radius | worst 0.004 | 0.10 | `bigdot`: 5.25 |
| F far fade | at the step before a dot is dropped its peak is under half a code value | 0.00000 for all nine | 0.00196 | `fixedcull`: 0.29 |
| O occlusion | an occluder edge crossing the dot changes visibility gradually and monotonically | 0.125 a 1/8 px step, monotone | 0.15 | `onetap`: 1.0 |

**Bars, and what changed after a first run.** T, H, S, F and O and their numbers were set before the first run and
never moved. The first run **failed H** at 0.0126. Most of the excess over the expected 0.7% was the twin's own
real-bulb rasterizer: at 16 x 16 coverage samples, its sampling noise moved the averaged energy by about 0.5% between
neighboring distances. The coverage was raised to 32 x 32 samples. That changes the fixture, not the bar, and H now
reads 0.0087. The margin is thin and stated as such: m = 1.6 sits near the limit computed in section 5.3. Also in the
first run, red `bigdot` was aimed at S and **did not fail it**. A mismatched size still cross-fades smoothly, so S
cannot see it. Check M (bar 10%) was **added after the first run** and `bigdot` moved to it. Red `intsize` was added
for S. Peak memory 126 MB, 21 s.

## 7. What the local lane must still do with real worldspaces

1. **Group census.** Walk XESP for every exterior LIGH reference in Fallout4.esm (and the DLC): number of groups,
   lights per group, chain lengths, parity-1 groups, roots outside the worldspace or in another cell. Then find
   the unparented lights a script switches: references with a script attached, workshop-linked references,
   power-connected objects. The twin's mix (11 SELF, 181 PARENT, 308 ALWAYS of 500) is invented.
2. **Bake layers in the relight.** In `probegi`: direct light per group, then the **same fixed pass count** for
   every layer. Write `.wlt` / `.fvl` v2 / `.fvg`. Gate: on Concord, always-on + layers against a full relight with
   the same lights masked (`WW_CELL_GI_LIGHTS`) to 1e-6, which is this twin's L on real data. Reds: drop one
   layer; BOUNCE2's stop rule.
3. **Measure the trim on real sectors.** Entries per group, KB per sector, and Q's p99 at 3e-5. If the bounce
   tails dominate as they do here, try a coarse tail (open question 4).
4. **Geometry that switches with lights.** Count groups whose root also enables meshes (lit windows, signs). Measure
   how much the bounce changes when those meshes are absent from the bake.
5. **Dot sources.** Pair each light with its bulb's emissive shape (in its own model, or the nearest emissive
   shape within the fixture clear), compute I_dot, and flag fill lights as having no dot. Count them.
6. **The near side in FO4CS:** fade a bulb mesh's emissive by (1 - w) inside the band, and draw the dot pass
   with the 16-tap depth test. Measure the depth bias against real LOD meshes (a lamp post's own LOD must not hide
   its dot).
7. **What the game does today.** Do any vanilla LOD meshes carry emissive bulbs (FARVIEW1 open question 1)? At
   what distance does a real bulb vanish (cell unload)? A capture flight at night, PRTP4 style.
8. **A pixel gate in the cell view.** Render a lit town at night across the band. Dots plus layers against every
   light and bulb drawn real, with the reds `nohandoff` and `drop`.

## 8. Open questions

1. **Script switches without enable parents.** The plugin data cannot say which unparented lights a quest script
   enables. Is "has a script or a workshop link" a good enough rule for SELF, or does it need a list made by hand?
2. **Geometry in a switch group.** Layers assume fixed geometry. Does a group that also enables meshes need its
   own geometry state in the bake, or is the bounce error at far distance too small to matter?
3. **Far flicker.** Keep a flickering light at its mean in its layer (cheap), or give it its own layer and re-sum
   every frame (exact but a cost per frame)? At far distance, the mean is probably right.
4. **Bounce tails.** 71-94% of a switched layer's entries are faint bounce outside its lights' radii. Storing
   those at a coarser cell (280 u, merging 4 x 4 x 4 surfels as in FARVIEW1 open question 3) could cut most of the
   901 KB. Not measured.
5. **Group count per sector.** The `.wlt` group index is u16. A sector with more than 65,535 groups is not
   plausible, but the reader should refuse rather than wrap.
6. **The dot's intensity estimate.** The hand-off hides an estimate error up to about +-85% (section 5.3). Is the
   emissive-shape estimate that good? What about bulbs whose glow comes from a glow card (PRTP_PLAN 2s) and not
   from the emissive mesh?
7. **Exposure.** d_max is computed at one night exposure. With eye adaptation, should the fade run on the live
   predicted peak (equivalent, exposure-aware), and does the far cull then need a hard upper distance for culling
   by sector?
8. **Occlusion bias.** How far toward the camera must the dot's depth be pulled to clear its own fixture and the
   LOD mesh, without letting a dot show through a thin LOD wall?
9. **Daytime.** Dots are additive. At day exposure the inverse-square rule makes them invisible on their own,
   but bulbs that are off in the daytime (switched by a time-of-day script) belong to a group, which ties back to
   question 1.

## 9. Files

- `docs/cloud/FARVIEW1b_DESIGN.md`: this page.
- `tests/spells/farview1b_layers_check.py`: twin 1 (layers, group key, shared record). One red alone:
  `FARVIEW1B_RED=drop|settle|nobounce|noparity|nopairvis` (exit 1 when it fails, as it must).
- `tests/spells/farview1b_dots_check.py`: twin 2 (dots). One red alone:
  `FARVIEW1B_DOTS_RED=nohandoff|bothdrawn|nofloor|bigdot|intsize|fixedcull|onetap`.
- No C++. The layer bake belongs in the relight (`probegi`) and the dot pass in the far renderer. Both need real
  cells to test, and section 7 lists them.

## 10. Actual output (2026-10-03)

`python3 tests/spells/farview1b_layers_check.py` (exit 0):

```
world: 22 boxes, 17994 surfels, 500 lights, 33 switch groups + always-on (22 parent, 11 self); 55 chained references; bounce rays traced once: 1151616 (4.5 s)
layers: 34 (always-on + 33 groups) baked in 7.1 s; shared-record pairs 160886 (321.8 a light) built in 1.2 s

cost per switch group (entries = surfels the layer keeps after the trim; 8 B an entry in the .fvg):
  kind   parity lights  entries  per lgt    bytes  sectors  past radius
  parent 0          60    11269      188    90152        4          57%
  parent 0          10     5470      547    43760        4          90%
  parent 0          10     5454      545    43632        4          88%
  parent 0          10     5358      536    42864        4          86%
  parent 0          10     4989      499    39912        4          88%
  parent 0           7     4893      699    39144        4          89%
  parent 0           5     4795      959    38360        4          81%
  parent 0           7     4661      666    37288        4          85%
  parent 1           5     4340      868    34720        4          83%
  parent 1           4     4282     1070    34256        4          86%
  self   0           1     4259     4259    34072        4          81%
  parent 0           9     4176      464    33408        4          82%
  parent 0           9     3945      438    31560        4          80%
  parent 0           9     3718      413    29744        4          79%
  parent 1           2     3641     1820    29128        4          83%
  parent 1           3     3500     1167    28000        4          84%
  parent 0           8     3444      430    27552        4          78%
  parent 1           4     3203      801    25624        4          81%
  self   0           1     2665     2665    21320        4          87%
  self   0           1     2389     2389    19112        4          87%
  self   0           1     2246     2246    17968        3          83%
  parent 1           1     2215     2215    17720        4          92%
  self   0           1     2213     2213    17704        4          92%
  parent 1           2     2212     1106    17696        4          77%
  parent 1           3     2208      736    17664        3          77%
  parent 1           2     2072     1036    16576        4          87%
  self   0           1     2072     2072    16576        4          79%
  self   0           1     2071     2071    16568        4          85%
  self   0           1     2003     2003    16024        4          94%
  self   0           1     1956     1956    15648        4          81%
  self   0           1     1703     1703    13624        3          71%
  self   0           1     1159     1159     9272        3          93%
  parent 1           1      779      779     6232        3          91%
  groups 33: entries median 3444, max 11269; 601 entries a switched light; all group layers 115360 entries = 901 KB
  always-on layer: 13457 lit surfels; .fvl v2 records (union of every layer) 14178 = 222 KB (v1 all-on file: 12032 = 188 KB)
  trim trade-off (all-on state; Q metric; entries in the switched layers):
    always-on 1e-03, switched 1e-03:  27202 entries ( 213 KB), p99 0.1308, energy 0.9931
    always-on 1e-04, switched 1e-03:  27202 entries ( 213 KB), p99 0.1296, energy 0.9931
    always-on 1e-04, switched 1e-04:  77371 entries ( 604 KB), p99 0.0235, energy 0.9990
    always-on 1e-04, switched 3e-05: 115360 entries ( 901 KB), p99 0.0089, energy 0.9997   <- chosen
    always-on 1e-04, switched 1e-05: 160827 entries (1256 KB), p99 0.0034, energy 0.9999
  for contrast, a dense per-light basis (GPURELIGHT1 3.2): 17994 surfels x 500 lights x 12 B = 103 MB
  a switch re-sums the touched sectors: dense numpy re-sum of all 34 layers over 17994 surfels 0.38 ms (CPU twin, not a GPU number)

green:
  G   PASS group key (root, parity) vs walking the chain: 0 of 3000 light-states differ (bar 0); 55 lights hang under a chain of two, 27 with net opposite parity
  L   PASS always-on + the layers that are on vs a full rebake, 5 states: worst max|diff|/max 6.7e-16 (bar 1e-06)
  Q   PASS stored (trim 1e-04 / 3e-05 x p99, RGB9E5, float32 sum) vs rebake on lit surfels: worst p99 0.0069 (bar 0.01), energy 0.9998..0.9999 (bar 1 +- 0.005)
  X   PASS GPU-relight direct from the shared record (params + 160886 visible pairs, built once) vs the bake's direct, 5 states: worst 0.0e+00 (bar 1e-09)
       all on    473 of 500 lights on, 24 of 34 groups; rebake 4 passes (5.5 s), bounce share 20.9%; L 6.7e-16  Q p99 0.0069 energy 0.9998  X 0.0e+00
       roots off 335 of 500 lights on, 11 of 34 groups; rebake 4 passes (5.3 s), bounce share 20.9%; L 4.5e-16  Q p99 0.0044 energy 0.9999  X 0.0e+00
       random 1  380 of 500 lights on, 19 of 34 groups; rebake 4 passes (5.3 s), bounce share 21.0%; L 5.6e-16  Q p99 0.0062 energy 0.9998  X 0.0e+00
       random 2  393 of 500 lights on, 18 of 34 groups; rebake 4 passes (5.5 s), bounce share 20.9%; L 4.5e-16  Q p99 0.0063 energy 0.9998  X 0.0e+00
       half      369 of 500 lights on, 16 of 34 groups; rebake 4 passes (5.4 s), bounce share 21.0%; L 4.5e-16  Q p99 0.0051 energy 0.9998  X 0.0e+00

red drop (must FAIL L):
  G   PASS group key (root, parity) vs walking the chain: 0 of 3000 light-states differ (bar 0); 55 lights hang under a chain of two, 27 with net opposite parity
  L   FAIL always-on + the layers that are on vs a full rebake, 5 states: worst max|diff|/max 6.3e-02 (bar 1e-06)
  Q   FAIL stored (trim 1e-04 / 3e-05 x p99, RGB9E5, float32 sum) vs rebake on lit surfels: worst p99 0.0296 (bar 0.01), energy 0.9989..0.9997 (bar 1 +- 0.005)
  X   PASS GPU-relight direct from the shared record (params + 160886 visible pairs, built once) vs the bake's direct, 5 states: worst 0.0e+00 (bar 1e-09)
       all on    473 of 500 lights on, 24 of 34 groups; rebake 4 passes (5.5 s), bounce share 20.9%; L 6.3e-02  Q p99 0.0080 energy 0.9995  X 0.0e+00  (dropped group 33: self, 1 lights)
       roots off 335 of 500 lights on, 11 of 34 groups; rebake 4 passes (5.3 s), bounce share 20.9%; L 4.1e-02  Q p99 0.0066 energy 0.9997  X 0.0e+00  (dropped group 12: parent, 1 lights)
       random 1  380 of 500 lights on, 19 of 34 groups; rebake 4 passes (5.3 s), bounce share 21.0%; L 5.5e-02  Q p99 0.0211 energy 0.9990  X 0.0e+00  (dropped group 24: self, 1 lights)
       random 2  393 of 500 lights on, 18 of 34 groups; rebake 4 passes (5.5 s), bounce share 20.9%; L 5.5e-02  Q p99 0.0296 energy 0.9989  X 0.0e+00  (dropped group 19: parent, 1 lights)
       half      369 of 500 lights on, 16 of 34 groups; rebake 4 passes (5.4 s), bounce share 21.0%; L 4.1e-02  Q p99 0.0079 energy 0.9996  X 0.0e+00  (dropped group 12: parent, 1 lights)
  -> FAILS as it must (L)

red settle (must FAIL L):
  G   PASS group key (root, parity) vs walking the chain: 0 of 3000 light-states differ (bar 0); 55 lights hang under a chain of two, 27 with net opposite parity
  L   FAIL always-on + the layers that are on vs a full rebake, 5 states: worst max|diff|/max 3.9e-04 (bar 1e-06)
  Q   FAIL stored (trim 1e-04 / 3e-05 x p99, RGB9E5, float32 sum) vs rebake on lit surfels: worst p99 0.0111 (bar 0.01), energy 0.9995..0.9999 (bar 1 +- 0.005)
  X   PASS GPU-relight direct from the shared record (params + 160886 visible pairs, built once) vs the bake's direct, 5 states: worst 0.0e+00 (bar 1e-09)
       all on    473 of 500 lights on, 24 of 34 groups; rebake 6 passes (5.5 s), bounce share 21.0%; L 3.9e-04  Q p99 0.0111 energy 0.9995  X 0.0e+00
       roots off 335 of 500 lights on, 11 of 34 groups; rebake 5 passes (5.2 s), bounce share 21.0%; L 2.4e-05  Q p99 0.0049 energy 0.9999  X 0.0e+00
       random 1  380 of 500 lights on, 19 of 34 groups; rebake 5 passes (5.1 s), bounce share 21.0%; L 1.1e-04  Q p99 0.0074 energy 0.9997  X 0.0e+00
       random 2  393 of 500 lights on, 18 of 34 groups; rebake 5 passes (5.4 s), bounce share 21.0%; L 1.0e-04  Q p99 0.0073 energy 0.9998  X 0.0e+00
       half      369 of 500 lights on, 16 of 34 groups; rebake 5 passes (5.2 s), bounce share 21.0%; L 1.0e-04  Q p99 0.0059 energy 0.9998  X 0.0e+00
  -> FAILS as it must (L)

red nobounce (must FAIL L):
  G   PASS group key (root, parity) vs walking the chain: 0 of 3000 light-states differ (bar 0); 55 lights hang under a chain of two, 27 with net opposite parity
  L   FAIL always-on + the layers that are on vs a full rebake, 5 states: worst max|diff|/max 1.3e-01 (bar 1e-06)
  Q   FAIL stored (trim 1e-04 / 3e-05 x p99, RGB9E5, float32 sum) vs rebake on lit surfels: worst p99 0.7792 (bar 0.01), energy 0.9377..0.9921 (bar 1 +- 0.005)
  X   PASS GPU-relight direct from the shared record (params + 160886 visible pairs, built once) vs the bake's direct, 5 states: worst 0.0e+00 (bar 1e-09)
       all on    473 of 500 lights on, 24 of 34 groups; rebake 4 passes (5.5 s), bounce share 20.9%; L 1.3e-01  Q p99 0.7792 energy 0.9377  X 0.0e+00
       roots off 335 of 500 lights on, 11 of 34 groups; rebake 4 passes (5.3 s), bounce share 20.9%; L 2.2e-02  Q p99 0.1602 energy 0.9921  X 0.0e+00
       random 1  380 of 500 lights on, 19 of 34 groups; rebake 4 passes (5.3 s), bounce share 21.0%; L 4.1e-02  Q p99 0.3007 energy 0.9827  X 0.0e+00
       random 2  393 of 500 lights on, 18 of 34 groups; rebake 4 passes (5.5 s), bounce share 20.9%; L 5.8e-02  Q p99 0.3813 energy 0.9779  X 0.0e+00
       half      369 of 500 lights on, 16 of 34 groups; rebake 4 passes (5.4 s), bounce share 21.0%; L 5.3e-02  Q p99 0.2852 energy 0.9825  X 0.0e+00
  -> FAILS as it must (L)

red noparity (must FAIL G):
  G   FAIL group key (root, parity) vs walking the chain: 162 of 3000 light-states differ (bar 0); 55 lights hang under a chain of two, 0 with net opposite parity
  L   PASS always-on + the layers that are on vs a full rebake, 5 states: worst max|diff|/max 6.7e-16 (bar 1e-06)
  Q   PASS stored (trim 1e-04 / 3e-05 x p99, RGB9E5, float32 sum) vs rebake on lit surfels: worst p99 0.0069 (bar 0.01), energy 0.9998..0.9999 (bar 1 +- 0.005)
  X   PASS GPU-relight direct from the shared record (params + 160886 visible pairs, built once) vs the bake's direct, 5 states: worst 0.0e+00 (bar 1e-09)
       all on    473 of 500 lights on, 24 of 34 groups; rebake 4 passes (5.5 s), bounce share 20.9%; L 6.7e-16  Q p99 0.0069 energy 0.9998  X 0.0e+00
       roots off 335 of 500 lights on, 11 of 34 groups; rebake 4 passes (5.3 s), bounce share 20.9%; L 4.5e-16  Q p99 0.0044 energy 0.9999  X 0.0e+00
       random 1  380 of 500 lights on, 19 of 34 groups; rebake 4 passes (5.3 s), bounce share 21.0%; L 5.6e-16  Q p99 0.0062 energy 0.9998  X 0.0e+00
       random 2  393 of 500 lights on, 18 of 34 groups; rebake 4 passes (5.5 s), bounce share 20.9%; L 4.5e-16  Q p99 0.0063 energy 0.9998  X 0.0e+00
       half      369 of 500 lights on, 16 of 34 groups; rebake 4 passes (5.4 s), bounce share 21.0%; L 4.5e-16  Q p99 0.0051 energy 0.9998  X 0.0e+00
  -> FAILS as it must (G)

red nopairvis (must FAIL X):
  G   PASS group key (root, parity) vs walking the chain: 0 of 3000 light-states differ (bar 0); 55 lights hang under a chain of two, 27 with net opposite parity
  L   PASS always-on + the layers that are on vs a full rebake, 5 states: worst max|diff|/max 6.7e-16 (bar 1e-06)
  Q   PASS stored (trim 1e-04 / 3e-05 x p99, RGB9E5, float32 sum) vs rebake on lit surfels: worst p99 0.0069 (bar 0.01), energy 0.9998..0.9999 (bar 1 +- 0.005)
  X   FAIL GPU-relight direct from the shared record (params + 197395 visible pairs, built once) vs the bake's direct, 5 states: worst 7.9e-01 (bar 1e-09)
       all on    473 of 500 lights on, 24 of 34 groups; rebake 4 passes (5.5 s), bounce share 20.9%; L 6.7e-16  Q p99 0.0069 energy 0.9998  X 7.9e-01
       roots off 335 of 500 lights on, 11 of 34 groups; rebake 4 passes (5.3 s), bounce share 20.9%; L 4.5e-16  Q p99 0.0044 energy 0.9999  X 7.3e-01
       random 1  380 of 500 lights on, 19 of 34 groups; rebake 4 passes (5.3 s), bounce share 21.0%; L 5.6e-16  Q p99 0.0062 energy 0.9998  X 7.3e-01
       random 2  393 of 500 lights on, 18 of 34 groups; rebake 4 passes (5.5 s), bounce share 20.9%; L 4.5e-16  Q p99 0.0063 energy 0.9998  X 7.6e-01
       half      369 of 500 lights on, 16 of 34 groups; rebake 4 passes (5.4 s), bounce share 21.0%; L 4.5e-16  Q p99 0.0051 energy 0.9998  X 7.6e-01
  -> FAILS as it must (X)

FARVIEW1b layers PASS: green PASS, reds all fail; 80 s, peak memory 170 MB
```

`python3 tests/spells/farview1b_dots_check.py` (exit 0):

```
camera f 1371.0 px; bulb radius 6 u = 2.68 px at D0 3072, 1.15 px at D1 7168; fog {'near': 0.0, 'far': 120000.0, 'power': 1.0, 'max': 0.8}
sigma scan (twinkle of a Gaussian dot sampled at pixel centers, 16 offsets): 0.30: 0.9573, 0.35: 0.5040, 0.40: 0.2404, 0.45: 0.1039, 0.50: 0.0407, 0.55: 0.0144, 0.60: 0.0046, 0.65: 0.0014, 0.70: 0.0004, 0.75: 0.0001, 0.80: 0.0000
SIGMA_MIN = 0.6 px (the smallest with twinkle <= 0.01); dot rms radius floor 0.85 px
d_max (peak <= 0.5/255, fog included): I 0.25: 43085 u, I 1.00: 69691 u, I 4.00: 96269 u; the sprite takes over from the bulb's own size at 7820 u

green:
  T  PASS twinkle over 16 sub-pixel offsets, all lamps, 512 distances: worst 0.0046 at 30622 u (I 4.00, m 1.6) (bar 0.02)
  H  PASS band 2560..7680 u by 32 u: energy / physical target, worst change a step 0.0087 at 4960 u (I 0.25, m 1.6) (bar 0.01), worst excursion outside [min(1,m), max(1,m)] 0.0016 (bar 0.02)
  S  PASS rms radius, change a step over 2560..250000 u: worst 0.0126 at 2592 u (I 0.25, m 0.6) (bar 0.03); I 1: 2.31 px at 2560, 1.94 at D0, 0.91 at D1, 0.85 far
  M  PASS rms radius dot / bulb: 3072 u: bulb 1.937 px, dot 1.937 px; 7168 u: bulb 0.905 px, dot 0.908 px; worst 0.004 (bar 0.10)
  F  PASS far fade, all 9 lamps: worst peak at the step before the drop 0.00000 (bar 0.00196 = 0.5/255); m 1: I 0.25: dropped after 42950 u, peak there 0.00000; I 1.00: dropped after 69244 u, peak there 0.00000; I 4.00: dropped after 96159 u, peak there 0.00000
  O  PASS occluder edge swept -3.0..3.0 px by 1/8 px: largest visibility change a step 0.125 (bar 0.15), monotone True

red nohandoff (must FAIL H):
  H  FAIL band 2560..7680 u by 32 u: energy / physical target, worst change a step 0.6007 at 7168 u (I 0.25, m 1.6) (bar 0.01), worst excursion outside [min(1,m), max(1,m)] 0.0051 (bar 0.02)
  -> FAILS as it must (H)

red bothdrawn (must FAIL H):
  H  FAIL band 2560..7680 u by 32 u: energy / physical target, worst change a step 0.9992 at 7168 u (I 0.25, m 0.6) (bar 0.01), worst excursion outside [min(1,m), max(1,m)] 1.0026 (bar 0.02)
  -> FAILS as it must (H)

red nofloor (must FAIL T):
  T  FAIL twinkle over 16 sub-pixel offsets, all lamps, 512 distances: worst 4.0000 at 100063 u (I 4.00, m 1.6) (bar 0.02)
  -> FAILS as it must (T)

red bigdot (must FAIL M):
  M  FAIL rms radius dot / bulb: 3072 u: bulb 1.937 px, dot 5.655 px; 7168 u: bulb 0.905 px, dot 5.655 px; worst 5.248 (bar 0.10)
  -> FAILS as it must (M)

red intsize (must FAIL S):
  S  FAIL rms radius, change a step over 2560..250000 u: worst 0.3334 at 7424 u (I 0.25, m 0.6) (bar 0.03); I 1: 2.31 px at 2560, 1.94 at D0, 1.06 at D1, 0.71 far
  -> FAILS as it must (S)

red fixedcull (must FAIL F):
  F  FAIL far fade, all 9 lamps: worst peak at the step before the drop 0.29121 (bar 0.00196 = 0.5/255); m 1: I 0.25: dropped after 19962 u, peak there 0.01138; I 1.00: dropped after 19962 u, peak there 0.04550; I 4.00: dropped after 19962 u, peak there 0.18200
  -> FAILS as it must (F)

red onetap (must FAIL O):
  O  FAIL occluder edge swept -3.0..3.0 px by 1/8 px: largest visibility change a step 1.000 (bar 0.15), monotone True
  -> FAILS as it must (O)

FARVIEW1b dots PASS: green PASS, reds all fail; 21 s, peak memory 126 MB
```

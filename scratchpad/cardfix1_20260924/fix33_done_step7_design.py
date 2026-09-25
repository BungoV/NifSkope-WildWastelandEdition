# CARDFIX1 step 7 (IMPOSTORPBRM1): DONE.md gets job 1 (the fixture + what the current exe bakes) and the
# DESIGN, written before any code. LF-only.
R = 'E:/Projects/NifskopeWWE-cardfix1/scratchpad/cardfix1_20260924/'


def patch(name, pairs):
    p = R + name
    b = open(p, 'rb').read(); assert b.count(b'\r') == 0
    s = b.decode('utf-8')
    for o, n in pairs:
        assert s.count(o) == 1, (name, o[:60], s.count(o))
        s = s.replace(o, n)
    out = s.encode('utf-8'); assert out.count(b'\r') == 0
    open(p, 'wb').write(out); print('patched', name)


DESIGN = """## Step 7 -- IMPOSTORPBRM1: cards from .pbrm models keep their PBR quantities

### Job 1 -- the fixture, and what the current exe bakes from it
- No `.pbrm` on disk belongs to a tree (census of 169 files: 161 v6; the only TintMask ones are the four X-01
  power-armour files in the `FO4CS TintMask Test` mod; none sets a non-default specular). So the fixture is
  MADE: `tests/spells/impostor_pbrm.py fixture <dir>` writes two v6 `.pbrm` files beside the two materials
  vanilla `TreeMapleForest1.nif` names (`materials/Landscape/Trees/MapleAtlas01.pbrm`, `MapleAtlas02_Tree.pbrm`,
  the same-name-sibling route) plus three uniform 64 x 64 maps. Base colour and normal are the vanilla maple
  maps. MapleAtlas01 takes the CONSTANT route (sparse slot values: colour #E6CCB3 multiplier, roughness 0.35,
  metallic 0.8, specular weight 0.75, specular colour #80C0FF, IOR 1.33, tint masks 0.5 / 0.2 constant, Add).
  MapleAtlas02_Tree takes the TEXTURE route (RMAOS map 0.702 / 0.2 / 1 / weight 0.502; specular-colour map
  RGB 200 150 100 with IOR 2.0 in its alpha over iorMax 4.25; a tint-mask map 0.6 / 0.3 / 0.2 / 0.1 whose sum
  1.2 makes Normalize bite). Nothing binary is committed; the script regenerates it.
- Baked on the current exe 309f3aa9 (N8, TILE 256, `scratchpad/cardfix1_20260924/pbrm/job1.sh`):
  **family legacy; the third sheet is `_gsaos`; no specular sheet.** Class 0 (96,860 texels): albedo
  113.8 101.3 77.9, gsaos R (gloss) 80.8, G (specular) 18.7, B (AO) 238.7, emissive 0. Class 1 (132,771):
  albedo 109.9 97.5 82.4, gloss 80.4, specular 14.2, AO 221.0, emissive 0. Those are the VANILLA material's
  numbers: the bake never opens a `.pbrm` (the sidecar says `lodm ... none` for both shapes), so the fixture's
  roughness, metallic, specular colour, IOR and tint are all absent from the card.

### Design (written before the code; the director relays before a format ships)
1. **Family.** A textured shape is pbr-sourced when it carries a pbr source `.lodm` (as today) or resolves a
   `.pbrm` through the viewport's one resolver, `io/pbrmresolve` (the direct `.pbrm` name, else the
   same-name sibling of the `.bgsm`/`.bgem`, else the diffuse stem), reading the loose root
   `WW_LODGEN_DATA_ROOT` first and then the resource stack -- the mask path's order (src/lodgen.cpp
   ~1849-1869). A shape with both keeps its `.lodm` (the explicit LOD source wins). The card is family pbr
   when EVERY textured shape is pbr-sourced. The sidecar gets one `pbrm <path> <route> tree <0|1>` line per
   shape that resolved one.
2. **Mixed models** (some shapes `.pbrm`, some not): **legacy, exactly as today**, and the sidecar names the
   `.pbrm` shapes whose data was not used. A legacy card describes the vanilla materials; turning a `.pbrm`
   into gloss/specular would lose metallic. The alternative is offered, not built: a pbr card whose legacy
   shapes are converted by the mesh-LOD mask path's own law (roughness = 1 - gloss, metallic 0, bungo's
   metallic ruling).
3. **How the bake reads it: no shader change.** For each pbrm shape the bake evaluates the `.pbrm` law in
   TEXTURE space on the CPU and writes the results as uncompressed DDS files with box-filtered mips into a
   per-bake folder that it registers as a resource root; the existing retarget hook
   (`wwTextureOverride`) points the shape's slots at them, and the existing channels photograph them raw in
   the same 4x offscreen bake. `fo4_default.frag` is untouched.
   - slot 0 (colour): sRGB( decode(map) x colour x tintMix ), alpha = the opacity law (the map's A when
     `overrideOpacity` is off, else the constant). The tint is applied HERE, so `_bc` holds the tinted colour.
   - slot 1 (normal): the `.pbrm`'s normal map as it is (a `strength` other than 1 is not applied: stated).
   - slot 7 (RMAOS): R roughness, G metallic, B AO -- the map's channel while its override is off, else the
     constant.
   - slot 2 (emissive): sRGB colour x mask, and the card's `emissiveScale` = luminance / 100 (100 nits = 1.0,
     the PBRM alignment's convention); luminance 0 = black.
   - specular: two extra channel-10 passes per view with slot 7 pointed at the specular source, only when a
     shape departs from the default specular.
4. **Specular weight, colour and IOR -> one new sheet `_s`: RGB = sqrt(F0'), A = specular weight.
   RECOMMENDED over a sheet of raw colour + IOR.** F0' = clamp(weight x colour x ((ior-1)/(ior+1))^2) is
   everything a dielectric's lobe uses in the PBRM law (the editor's preview and NifSkope's viewport;
   F90' = sat(50 F0') is derived; OpenPBR's remap ior' = (1 + sqrt F0') / (1 - sqrt F0')), so folding loses
   nothing for a dielectric, and F0 averages linearly through the 4x downsample and the mips, where an IOR
   does not. sqrt is the Fresnel amplitude: the default F0 0.04 sits at level 51 and one level is 3.9 % of
   it (a linear F0 would be 10 % a level). A = the weight, because a METAL's lobe is weight x F82(base,
   colour): the metal keeps its weight and loses only the colour as its F82 edge tint, which FO4CS does not
   read yet (PBRM-v6.md:245, dielectrics only). Precision cost: `_s` is BC7 at the aux size, the same bytes
   as `_n`; BC1 was rejected (5:6:5 endpoints = about 8 % of F0 a step at 0.04, no alpha). Written only when a
   shape departs from the default (weight 1, white, IOR 1.5); no `_s` = F0 0.04, weight 1. The `.lodm` gets a
   new texture key `specular` on family pbr cards. A reader ignores keys it does not know (LODGEN_LODM_FORMAT
   section 2) and no existing key changes meaning, so **no version bump**. Coat, fuzz, transmission, thin film,
   subsurface colour: dropped -- no LOD reader for them, sub-pixel at LOD distance, each would be another
   sheet (LODGEN_IMPOSTOR_SPEC.md "Ours, for LOD" gets this list).
5. **Tint masks against Warframe's published TennoGen rules (a finding; the editor is not changed).**
   DE's texturing guide: the mask is its own map, R/G/B/A = tints 0-3; the base is authored GREY (non-metal
   about 128, metal about 186) and the tint supplies the colour; unmasked texels stay grey; the masks are
   STACKED layers (G over R, B over G, A over everything) and authors extend a lower mask under the upper
   one on purpose. DE publishes no blend formula (multiply on a grey base is the likely reading). Ours:
   the base keeps its authored colour and is multiplied; unmasked = unchanged; Normalize / Add SUM
   overlapping masks, so Warframe-style overlapped masks mix colours instead of the upper winning; only
   "Priority RGBA" stacks, and in the opposite order (our R wins, Warframe's A wins). Warframe's energy
   (emissive) colour has no tint-mask equivalent here. Sources: warframe.com/en/steamworkshop/texturing-guide,
   basic-art-guide, operator-accessories.
6. **Per-reference tint in FO4 data: it exists, by material swap.** A placed reference can carry XMSP
   (a Material Swap, MSWP), whose substitutions each carry a Color Remapping Index (CNAM float); models carry
   the same index (MODC). The maple itself has swap materials on disk (MapleInstituteAtlasGreen/Orange,
   MaplePreWarAtlas*). A swap replaces the whole material, so its sibling `.pbrm` (its own tint colours)
   applies to that reference only; the card bake is per base model, so a swapped reference would show the
   base material's card. Carrying tint per reference would need the mask on the card (another sheet and four
   colours per reference). Reported, not built.
7. **FO4CS reader:** contract text only (LODGEN_LODM_FORMAT.md, LODGEN_IMPOSTOR_SPEC.md); FO4CS readers are
   built last.
8. **Gate `tests/spells/impostor_pbrm.sh`, bars pre-registered here.** The rung exe 309f3aa9 is the red
   (family legacy, no `_rmaos`, no `_s`). Per material class (split by the subsurface mask and the sidecar's
   tree flag): roughness, metallic, sqrt(F0') R/G/B and weight each have a median within 1.0 level of the
   independent Python law, AND at least 0.90 of the texels within 2 levels (uniform inputs, two 8-bit
   roundings, edge texels allowed 10 %). Colour: the law applied to the LEGACY card's texel (same base map,
   same view) against the pbrm card, with a median error no more than 1.25 x the FLOOR measured in the
   same run (an identity `.pbrm`: base map only, white, no tint, against the legacy card). The law is
   perturbed three ways, each of which must fail: Add in place of Normalize, IOR ignored (1.5), and the
   specular colour dropped. Kept green as the brief lists: lodgen_octahedral, impostor_draw, and the
   native_lighting control.

"""

patch('DONE.md', [
    ("# 3. Gates (numbers; red runs)\n", DESIGN + "# 3. Gates (numbers; red runs)\n"),
])

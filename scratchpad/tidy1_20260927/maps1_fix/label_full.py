"""MAPS1's label_full.py, fixed by lane TIDY1 (audit rank 9). The original in the overseer's maps1 folder is untouched.
Differences from the original, and only these:
  * view 37 (P_colour) says it is a TINT that multiplies the ground textures, not the ground colour;
  * view 36 (P_cellrange) carries the audit's caption; view 39 (P_groundcover) says why it is empty on Fallout 4;
  * an S_ sheet's title comes from WHAT THE ARRAY HOLDS (the sidecar's source paths), not from the file name:
    A512 is every tree/leaf LOD material, so view 46 said "Building" over trees;
  * the duplicate views are dropped (22 = 16 byte for byte; 28 = 14 from another angle; 66 = 65 in another colour
    code; 68/70/72/73 = 35/40/43/44), and the NUMBERS STAY THE AUDIT'S: a dropped view leaves a gap, so 37 is still
    37 and 46 is still 46;
  * sources/outputs are arguments: label_full.py <pics dir> <out dir> <arrays sidecar .txt> [name ...]
    (names limit the output to those views; the numbering is still computed over every picture).
The water and identity views are not touched (lanes WATER1 and IDENT1 own them).
"""
import os
import sys
import glob
from PIL import Image, ImageDraw, ImageFont

SRC, OUT, SIDECAR = sys.argv[1], sys.argv[2], sys.argv[3]
ONLY = set(sys.argv[4:])
os.makedirs(OUT, exist_ok=True)

N = {
 "L00_lit_ref_aodecal": "Reference: the normal picture (shading on)",
 "F00_flat_default": "Reference: flat, no shading",
 "L01_lit_default": "Reference: lit, default",
 "L02_raw12_default": "Reference: raw colour",
 "F_sky": "Buildings: sky visibility (white = open sky)",
 "F_ao": "Buildings: ambient occlusion (white = open)",
 "F_selfao": "Buildings: self-shading inside one model",
 "F_sway": "Buildings/trees: wind sway amount",
 "F_ground": "Buildings: ground contact",
 "F_seed": "Buildings: per-object random seed",
 "F_identity": "Buildings: which object each pixel belongs to",
 "F_identityraw": "Buildings: object id, raw",
 "F_placement": "Buildings: which placement each pixel belongs to",
 "F_scrappable": "Buildings: scrappable flag (NO DATA in this bake)",
 "F_mask-r": "Ground: roughness",
 "F_mask-g": "Ground: metallic",
 "F_mask-b": "Ground: sky shading (terrain only)",
 "F_mask-a": "Ground: ground cover",
 "L03_normal": "Ground: normal map, as drawn",
 "L04_emissive": "Glow / emissive (NO DATA in this bake)",
 "L05_lc8_viewnormal": "Everything: surface direction seen from the camera",
 "O_flags": "Buildings, top-down: per-object flags",
 "O_normal_world": "Buildings, top-down: LOD mesh normals",
 "O_occluders": "Buildings, top-down: occluder boxes",
 "O_placement_ao": "Buildings, top-down: shading per placement",
 "O_sky_placement": "Buildings, top-down: sky visibility per placement",
 "O_vao_stream": "Buildings, top-down: shading per vertex",
 "O_vcolour_a": "Buildings, top-down: vertex colour alpha",
 "O_vcolour_rgb": "Buildings, top-down: vertex colour",
 "P_ao": "Ground: coarse shading",
 "P_blend": "Ground: texture blend weights",
 "P_bodyid": "Water (this bake, old format): water body id - NO DATA",
 "P_cellflags": "Water (this bake, old format): land/water flags",
 "P_cellrange": "Ground: per-cell min/max height (culling table, one value per 4096-unit cell)",
 "P_colour": "Ground: vertex tint (multiplies the ground textures; not the ground colour)",
 "P_flow": "Water (this bake, old format): flow - NO DATA",
 "P_groundcover": "Ground cover plane: absent on Fallout 4 (no GCVR records; see mask A)",
 "P_height": "Ground: height",
 "P_overview": "Ground: overview",
 "P_shore": "Water (this bake, old format): shore distance - NO DATA",
 "P_waterheight": "Water (this bake): water height",
 "P_watertype": "Water (this bake): water type",
 "T_colour_sheet": "Ground, top-down: colour texture",
 "T_height_sheet": "Ground, top-down: height",
 "T_msn_stored": "Ground, top-down: normal map as stored",
 "W3_bodyid": "Water (new water bake): water body id",
 "W3_flow": "Water (new water bake): flow direction",
 "W3_shore": "Water (new water bake): distance to shore",
 "WC_v2_cellflags": "Whole Commonwealth water (old format): land/water flags",
 "WC_v2_waterheight": "Whole Commonwealth water (old format): water height",
 "WC_v2_watertype": "Whole Commonwealth water (old format): water type",
 "WC_v3_bodyid": "Whole Commonwealth water (new bake): water body id",
 "WC_v3_flow": "Whole Commonwealth water (new bake): flow",
 "WC_v3_shore": "Whole Commonwealth water (new bake): distance to shore",
}
# duplicates, dropped (the audit's pairs): the name -> the view it repeats
DROP = {
 "L06_ao_channel_lit": "L00_lit_ref_aodecal",   # WW_LODL_CHANNEL=ao IS WW_LODL_AO=1: byte for byte
 "O_sky_stream": "F_sky",                       # the same stream from a second angle
 "T_msn_worldXYZ": "T_msn_stored",              # the same normals in another colour code
 "W3_cellflags": "P_cellflags",                 # the v3 water bake repeats the v2 planes
 "W3_height": "P_height",
 "W3_waterheight": "P_waterheight",
 "W3_watertype": "P_watertype",
}
CH = {"ao_gsB": "shading", "colour_d": "colour", "emissive_g": "glow", "gloss_gsR": "gloss",
      "height_nB": "height", "normal_n_rebuiltZ": "normal map", "spec_gsG": "specular",
      "sss_gsA": "subsurface", "sway_nA": "wind sway"}
SHEET = {"A512": "512x512"}          # the S_ tag -> the mesh array class it pictures (offline_arrays.py)


def holds(cls):
    """What a mesh array class holds, from the sidecar's source column: trees, buildings, or both."""
    srcs = [ln.split()[8].lower() for ln in open(SIDECAR, encoding='utf-8')
            if not ln.startswith('#') and ln.split() and ln.split()[1] == cls]
    tree = sum(1 for s in srcs if '\\trees\\' in s or '/trees/' in s)
    if srcs and tree == len(srcs):
        return "Tree/leaf LOD textures (%d materials)" % len(srcs)
    if tree == 0:
        return "Building LOD textures (%d materials)" % len(srcs)
    return "Building and tree LOD textures (%d materials, %d trees)" % (len(srcs), tree)


try:
    F = ImageFont.truetype("C:/Windows/Fonts/segoeuib.ttf", 34)
except OSError:
    F = ImageFont.load_default()

files = sorted(glob.glob(os.path.join(SRC, "*.png")))
missing = []
written = 0
for i, f in enumerate(files, 1):          # the audit's numbering: every picture counts, dropped or not
    k = os.path.splitext(os.path.basename(f))[0]
    if k in DROP or (ONLY and k not in ONLY):
        continue
    t = N.get(k)
    if t is None and k.startswith("S_"):
        arr, ch = k[2:].split("_", 1)
        which = "Tree cards" if arr.startswith("C") else holds(SHEET[arr])
        t = which + ": " + CH.get(ch, ch)
    if t is None:
        t = k
        missing.append(k)
    im = Image.open(f).convert("RGB")
    bar = 60
    out = Image.new("RGB", (im.width, im.height + bar), (20, 20, 24))
    out.paste(im, (0, bar))
    dr = ImageDraw.Draw(out)
    title, font = "%02d  %s" % (i, t), F
    for size in range(34, 18, -1):        # a long caption shrinks to fit rather than run off the edge
        try:
            font = ImageFont.truetype("C:/Windows/Fonts/segoeuib.ttf", size)
        except OSError:
            break
        if dr.textlength(title, font=font) <= im.width - 32:
            break
    dr.text((16, 10 + (34 - getattr(font, 'size', 34)) // 2), title, fill=(240, 240, 240), font=font)
    out.save(os.path.join(OUT, "%02d_%s.png" % (i, k)))
    written += 1
    print("%02d %s" % (i, t))
print(written, "written; dropped as duplicates:", len(DROP), "; unlabelled:", missing)

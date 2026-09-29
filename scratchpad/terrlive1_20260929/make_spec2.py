"""TERRLIVE1 rework (FULL ditched, law 2): write a --terrain-preview spec.
usage: python make_spec2.py <out.json> <pics dir> <mode>
modes: whole | renders | crossover | close_after | close_before (close_before runs on the kept build-4 exe)"""
import json, sys, os
out, pics, mode = sys.argv[1], sys.argv[2], sys.argv[3]
D = 'E:/Projects/NifskopeWWE-terrlive1/scratchpad/terrlive1_20260929'
B = D + '/bakes'
W = 'mod/FO4CSLOD/Commonwealth'
STAGE = 'E:/Projects/NifskopeWWE-night/scratchpad/merge1_20260929/stage/mod/FO4CSLOD/Commonwealth'
NEW = f'{D}/whole_hybrid_law2/{W}'     # build 5, law 2
OLD = f'{D}/whole_hybrid_law1/{W}'     # build 4, law 1
two = ['hybrid', 'dynamic']
boston_cells = [-8, -12, 3, -1]
common = dict(cells=boston_cells, stride=1, lodt_far=f'{NEW}/Commonwealth.VT.8.lodt', decals=f'{B}/hybrid/{W}/Commonwealth.lodd')
boston = dict(common, name='boston', ortho=[-4096.0, -24576.0, 16384.0], width=1200, height=1200)
street = dict(common, name='street', eye=[-4096.0, -30720.0, 150.0], at=[-4096.0, -22000.0, 0.0], fov=70.0,
              width=1920, height=1080)
oblique = dict(common, name='oblique', eye=[-6144.0, -48640.0, 1500.0], at=[-6144.0, -26000.0, 0.0], fov=70.0,
               width=1920, height=1080)
whole = dict(name='whole', cells=[-96, -96, 95, 95], stride=4, ortho=[0.0, 0.0, 393216.0], width=1600, height=1600,
             lodt_far=f'{NEW}/Commonwealth.VT.32.lodt', decals=f'{NEW}/Commonwealth.lodd')
# bungo's two circled spots on whole_full.png, as world boxes (px -> world: 491.52 u a pixel, 60 px title bar)
# (a) north square steps: px 890-1060 x 540-620 -> x 44k..128k, y 118k..157k
north = dict(name='north_steps', cells=[4, 24, 38, 44], stride=1, ortho=[86016.0, 137216.0, 61440.0], width=1600, height=1000)
# (b) west dirt outline: px 540-600 x 620-900 -> x -128k..-98k, y -20k..118k
west = dict(name='west_outline', cells=[-40, -12, -18, 34], stride=1, ortho=[-112640.0, 49152.0, 43008.0], width=1000, height=1600)
views = []
if mode == 'whole':
    whole['renders'] = [dict(option=o, out=f'{pics}/whole_{o}.png') for o in two]
    whole['box_count'] = True
    views = [whole]
elif mode == 'renders':
    for v in (boston, street):
        v['renders'] = [dict(option=o, out=f'{pics}/{v["name"]}_{o}.png') for o in two]
        v['box_count'] = True
    views = [boston, street]
elif mode == 'crossover':
    for v in (street, oblique):
        v['lodt_full'] = f'{B}/full/{W}/Commonwealth.VT.2.lodt'    # the old 16 u level, the reference only
        v['crossover'] = dict(threshold=4.0, bin=2048.0, min_pixels=2000, csv=f'{pics}/crossover_{v["name"]}.csv')
        v['renders'] = []
    views = [oblique, street]
elif mode == 'close_after':
    for v in (north, west):
        v['lodt_far'] = f'{NEW}/Commonwealth.VT.8.lodt'
        # overhead, the hybrid IS its baked far levels; 'baked' draws them with no eye-distance fade
        v['renders'] = [dict(option='baked', out=f'{pics}/close_{v["name"]}_after_baked.png',
                             titles=[f'{v["name"]} AFTER -- HYBRID far levels, baked (law 2, band 8192 u)']),
                        dict(option='dynamic', out=f'{pics}/close_{v["name"]}_after_dynamic.png',
                             titles=[f'{v["name"]} AFTER -- DYNAMIC, live (law 2, band 8192 u)'])]
    views = [north, west]
elif mode == 'close_before':
    # what bungo circled: the staged MERGE1 bake (law 1) through the FULL level, and build 4's live ground
    for v in (north, west):
        v['lodt_full'] = f'{STAGE}/Commonwealth.VT.8.lodt'
        v['lodt_far'] = f'{OLD}/Commonwealth.VT.8.lodt'
        v['renders'] = [dict(option='full', out=f'{pics}/close_{v["name"]}_before_baked.png',
                             titles=[f'{v["name"]} BEFORE -- baked (MERGE1 stage, law 1)']),
                        dict(option='dynamic', out=f'{pics}/close_{v["name"]}_before_dynamic.png',
                             titles=[f'{v["name"]} BEFORE -- DYNAMIC (build 4 live ground)'])]
    views = [north, west]
lodl = f'{STAGE}/Commonwealth.lodl' if mode in ('whole', 'close_after', 'close_before') else f'{B}/full/{W}/Commonwealth.lodl'
spec = dict(lodl=lodl, ws='Commonwealth', fade=[8192.0, 12288.0], frames=30, ltex_side=512, decal_normals=True, views=views)
os.makedirs(pics, exist_ok=True)
json.dump(spec, open(out, 'w'), indent=1)
print('spec', out, mode, 'views', [v['name'] for v in views])

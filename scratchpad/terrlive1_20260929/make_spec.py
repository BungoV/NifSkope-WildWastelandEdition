"""TERRLIVE1: write the --terrain-preview spec.
usage: python make_spec.py <out.json> <pics dir> <fade0> <fade1> [crossover|renders|ao|whole]"""
import json, sys, os
out, pics, f0, f1, mode = sys.argv[1], sys.argv[2], float(sys.argv[3]), float(sys.argv[4]), sys.argv[5]
D = 'E:/Projects/NifskopeWWE-terrlive1/scratchpad/terrlive1_20260929'
B = D + '/bakes'
W = 'mod/FO4CSLOD/Commonwealth'
STAGE = 'E:/Projects/NifskopeWWE-night/scratchpad/merge1_20260929/stage/mod/FO4CSLOD/Commonwealth'
boston_cells = [-8, -12, 3, -1]
common = dict(cells=boston_cells, stride=1,
              lodt_full=f'{B}/full/{W}/Commonwealth.VT.2.lodt',
              # the Boston box's own pyramid stops at dim 4 (-12 is not a multiple of 8), so the hybrid's far level
              # comes from the whole-map hybrid bake, which the same exe writes
              lodt_far=f'{D}/whole_hybrid/{W}/Commonwealth.VT.8.lodt',
              decals=f'{B}/hybrid/{W}/Commonwealth.lodd')
# MAPS1's Boston camera: view 8, ortho, cells -5..2 x -10..-3
boston = dict(common, name='boston', ortho=[-4096.0, -24576.0, 16384.0], width=1200, height=1200)
# one low eye-level street view: 150 u over the ground in the middle of MAPS1's Boston view, looking north
street = dict(common, name='street', eye=[-4096.0, -30720.0, 150.0], at=[-4096.0, -22000.0, 0.0], fov=70.0,
              width=1920, height=1080)
whole = dict(name='whole', cells=[-96, -96, 95, 95], stride=4, ortho=[0.0, 0.0, 393216.0], width=1600, height=1600,
             lodt_full=f'{STAGE}/Commonwealth.VT.32.lodt',
             lodt_far=f'{D}/whole_hybrid/{W}/Commonwealth.VT.32.lodt',
             decals=f'{D}/whole_hybrid/{W}/Commonwealth.lodd')
three = ['full', 'hybrid', 'dynamic']
views = []
# the crossover's own view: eye 1500 u over the ground at the box's south edge, looking north and down, so every
# distance bin out to ~45,000 u gets thousands of pixels (the street view's far bins get tens)
oblique = dict(common, name='oblique', eye=[-6144.0, -48640.0, 1500.0], at=[-6144.0, -26000.0, 0.0], fov=70.0,
               width=1920, height=1080)
if mode == 'crossover':
    for v in (street, oblique):
        v['crossover'] = dict(threshold=4.0, bin=2048.0, min_pixels=2000, csv=f'{pics}/crossover_{v["name"]}.csv')
        v['renders'] = []
    views = [oblique, street]
elif mode == 'renders':
    for v in (boston, street):
        v['renders'] = [dict(option=o, out=f'{pics}/{v["name"]}_{o}.png') for o in three]
        v['renders'].append(dict(options=three, out=f'{pics}/{v["name"]}_side_by_side.png'))
        v['box_count'] = True
    views = [boston, street]
elif mode == 'ao':
    for v in (street, boston):
        v = dict(v)
        v['renders'] = [
            dict(options=['full'], ao=1, ao_lod=0.0, out=f'{pics}/ao_{v["name"]}_full_ao16.png',
                 titles=[f'{v["name"]} -- FULL + AO 16 u (reference)']),
            dict(options=['hybrid'], ao=1, ao_lod=1.0, out=f'{pics}/ao_{v["name"]}_a_map32.png',
                 titles=[f'{v["name"]} -- HYBRID + (a) AO map 32 u']),
            dict(options=['hybrid'], ao=0, out=f'{pics}/ao_{v["name"]}_c_none.png',
                 titles=[f'{v["name"]} -- HYBRID + (b)/(c) no ground AO map']),
        ]
        views.append(v)
elif mode == 'whole':
    whole['renders'] = [dict(option=o, out=f'{pics}/whole_{o}.png') for o in three]
    whole['renders'].append(dict(options=three, out=f'{pics}/whole_side_by_side.png'))
    whole['box_count'] = True
    views = [whole]
spec = dict(lodl=f'{B}/full/{W}/Commonwealth.lodl', fade=[f0, f1], frames=30, ltex_side=512, decal_normals=True,
            views=views)
if mode == 'whole':
    spec['lodl'] = f'{STAGE}/Commonwealth.lodl'
os.makedirs(pics, exist_ok=True)
json.dump(spec, open(out, 'w'), indent=1)
print('spec', out, mode, 'views', [v['name'] for v in views])

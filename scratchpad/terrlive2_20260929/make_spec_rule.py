"""TERRLIVE1 rule paint: preview specs. spec_rule_pics.json (pictures) and spec_rule_gpu.json (GPU ms on/off).
OFF views read the OFF bake's far levels with "rule": false; ON views read the rule bake's levels + its .lodr."""
import json
D = 'E:/Projects/NifskopeWWE-terrlive2/scratchpad/terrlive2_20260929'
STAGE = 'E:/Projects/NifskopeWWE-night/scratchpad/merge1_20260929/stage/mod/FO4CSLOD/Commonwealth/Commonwealth.lodl'
W = lambda run, n: '%s/%s/mod/FO4CSLOD/Commonwealth/Commonwealth.%s' % (D, run, n)
RULE = W('whole_rule', 'lodr')
BOTH = ['hybrid', 'dynamic']


def view(name, on, far, cam, cells, stride, titles, out, **kw):
    v = {'name': name, 'cells': cells, 'stride': stride, 'lodt_far': W('whole_rule' if on else 'whole_off', far),
         'renders': [{'options': BOTH, 'titles': titles, 'out': out}]}
    if not on:
        v['rule'] = False
    v.update(cam)
    v.update(kw)
    return v


def T(what, on):
    tag = 'outside paint RULE (switch on)' if on else 'outside paint VANILLA (switch off)'
    return ['%s -- HYBRID far levels, baked -- %s' % (what, tag), '%s -- DYNAMIC, live -- %s' % (what, tag)]


pics = []
for on in (False, True):
    s = 'on' if on else 'off'
    pics.append(view('whole_' + s, on, 'VT.32.lodt', {'ortho': [0.0, 0.0, 393216.0], 'width': 1600, 'height': 1600},
                     [-96, -96, 95, 95], 4, T('whole map', on), 'pics3/whole_%s.png' % s))
    pics.append(view('hills_' + s, on, 'VT.8.lodt',
                     {'eye': [-106496.0, 20480.0, 1500.0], 'at': [-200000.0, 20480.0, 0.0], 'fov': 70.0,
                      'width': 1920, 'height': 1080},
                     [-60, -5, -20, 45], 2, T('low view west over the outside hills', on), 'pics3/hills_%s.png' % s))
pics.append(view('north_steps_on', True, 'VT.8.lodt', {'ortho': [86016.0, 137216.0, 61440.0], 'width': 1600, 'height': 1000},
                 [4, 24, 38, 44], 1, T('north steps', True), 'pics3/north_steps_on.png'))
pics.append(view('west_outline_on', True, 'VT.8.lodt', {'ortho': [-112640.0, 49152.0, 43008.0], 'width': 1000, 'height': 1600},
                 [-40, -12, -18, 34], 1, T('west outline', True), 'pics3/west_outline_on.png'))
base = {'lodl': STAGE, 'ws': 'Commonwealth', 'fade': [8192.0, 12288.0], 'frames': 30, 'ltex_side': 512,
        'decal_normals': True, 'rule': RULE}
json.dump(dict(base, views=pics), open(D + '/spec_rule_pics.json', 'w', newline='\n'), indent=1)

# GPU ms: Boston + street (the old spec's cameras, full lodl, hybrid decals), on vs off; whole = the pictures spec
FULL = D + '/bakes/full/mod/FO4CSLOD/Commonwealth/Commonwealth.lodl'
DEC = D + '/bakes/hybrid/mod/FO4CSLOD/Commonwealth/Commonwealth.lodd'
gpu = []
for on in (False, True):
    s = 'on' if on else 'off'
    gpu.append(view('boston_' + s, on, 'VT.8.lodt', {'ortho': [-4096.0, -24576.0, 16384.0], 'width': 1200, 'height': 1200},
                    [-8, -12, 3, -1], 1, T('Boston', on), 'pics3/gpu_boston_%s.png' % s, decals=DEC, box_count=True))
    gpu.append(view('street_' + s, on, 'VT.8.lodt',
                    {'eye': [-4096.0, -30720.0, 150.0], 'at': [-4096.0, -22000.0, 0.0], 'fov': 70.0, 'width': 1920, 'height': 1080},
                    [-8, -12, 3, -1], 1, T('street', on), 'pics3/gpu_street_%s.png' % s, decals=DEC, box_count=True))
json.dump(dict(base, lodl=FULL, views=gpu), open(D + '/spec_rule_gpu.json', 'w', newline='\n'), indent=1)
print('specs written')

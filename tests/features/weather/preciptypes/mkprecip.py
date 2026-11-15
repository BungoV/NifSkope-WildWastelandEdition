"""WEATHERRECX2 part 4: write the six shipped precipitation type files (today's values, precipmath.h typeTable) into
<root>/Plugins/Fusion Shaders - Assets/Weathers/Precip/<name>.recx. usage: python -I mkprecip.py <root> [--extra]
--extra adds the gate's fixtures: sleetLike (base hail), a mod-namespaced key, an old snake_case name."""
import json, os, sys

V = '1.8'
T = {
 'rain': dict(size=2.0, rate=4.0, drawScale=1.0, translucency=0.9, spin=0.0, sway=0.0, maxMip=4,
              texture='FO4CS/Precip/rain_d.dds', atlas=[4, 2],
              variants=[[0.5, 8], [1, 8], [1.5, 8], [2, 8], [3, 8], [4, 8], [5, 8], [6, 8]],
              material='water', shape='drop', melts=False, wets=1.0, settles=0.0),
 'hail': dict(size=10.0, rate=10.0, drawScale=1.0, translucency=0.5, spin=2.0, sway=0.0, maxMip=2,
              texture='FO4CS/Precip/hail_d.dds', atlas=[4, 2],
              variants=[[5, 50], [8, 50], [12, 50], [16, 50], [20, 50], [26, 50], [34, 50], [44, 50]],
              material='ice', shape='pellet', melts=True, wets=0.2, settles=0.3),
 'snowFine': dict(size=5.5, rate=1.0, drawScale=3.0, translucency=0.6, spin=0.5, sway=6.0, maxMip=4,
              texture='FO4CS/Precip/snow_fine_d.dds', atlas=[4, 4],
              variants=[[3.0, 3.5572], [3.0, 3.5698], [3.0, 3.5572], [3.0, 3.6575], [3.93, 4.5971], [4.1, 4.7982],
                        [4.59, 5.3648], [4.55, 5.3144], [5.5, 3.9031], [5.5, 4.4517], [5.5, 4.7008], [5.5, 4.2866],
                        [10.0, 6.1308], [10.0, 5.6934], [10.0, 5.8648], [10.0, 5.7326]],
              material='drySnow', shape='flake', melts=True, wets=0.1, settles=1.0),
 'snowFarDot': dict(size=6.0, rate=1.0, drawScale=3.0, translucency=0.6, spin=0.0, sway=6.0, maxMip=4,
              texture='FO4CS/Precip/snow_far_dot_d.dds', atlas=[4, 2],
              variants=[[4, 14], [5, 14], [6, 14], [7, 14], [8, 14], [9, 14], [10, 14], [11, 14]],
              material='drySnow', shape='flake', melts=True, wets=0.1, settles=1.0),
 'snowWetClump': dict(size=10.0, rate=2.0, drawScale=2.0, translucency=0.3, spin=0.3, sway=3.0, maxMip=4,
              texture='FO4CS/Precip/snow_wet_clump_d.dds', atlas=[4, 2],
              variants=[[5, 27], [7, 27], [9, 27], [11, 27], [14, 27], [17, 27], [21, 27], [25, 27]],
              material='wetSnow', shape='clump', melts=True, wets=0.6, settles=0.6),
 'snowBigClump': dict(size=20.0, rate=1.0, drawScale=1.5, translucency=0.6, spin=0.4, sway=8.0, maxMip=4,
              texture='FO4CS/Precip/snow_big_clump_d.dds', atlas=[4, 2],
              variants=[[15, 54], [20, 54], [25, 54], [30, 54], [35, 54], [40, 54], [45, 54], [50, 54]],
              material='drySnow', shape='flake', melts=True, wets=0.1, settles=1.0),
}
ORDER = ['base', 'size', 'variants', 'material', 'shape', 'melts', 'wets', 'settles', 'rate', 'texture', 'atlas',
         'maxMip', 'drawScale', 'translucency', 'spin', 'sway']

def write(path, name, t):
    t = {k: t[k] for k in ORDER if k in t} | {k: v for k, v in t.items() if k not in ORDER}
    doc = {'version': V, 'entries': [{'precipitationType': name, 'type': t}]}
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(json.dumps(doc, indent='\t') + '\n')

def main():
    root = sys.argv[1]
    d = os.path.join(root, 'Plugins', 'Fusion Shaders - Assets', 'Weathers', 'Precip')
    os.makedirs(d, exist_ok=True)
    for n, t in T.items():
        write(os.path.join(d, n + '.recx'), n, t)
    if '--extra' in sys.argv:
        write(os.path.join(d, 'sleetLike.recx'), 'sleetLike', {'base': 'hail', 'size': 4.0, 'MyMod:tint': [1, 1, 1]})
        write(os.path.join(d, 'zzOld.recx'), 'snow_far_dot', {'size': 6.0})	# hidden by snowFarDot.recx (same name)
    print('wrote', len(os.listdir(d)), 'files to', d)

main()

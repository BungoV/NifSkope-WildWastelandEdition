"""lane WEATHERRECX3: the hand-broken sets the gate must FAIL.  python -I reds.py <orig Weathers> <migrated Weathers> <out>
  <out>/red_value : migrated, Fallout4/CommonwealthClear atmosphere.visibility x 1.01 (an authored value moved)
  <out>/red_unit  : migrated, Fallout4/CommonwealthRain precipitation.wetting left in minutes under 1.8 (a unit slip)
  <out>/red_ref   : ORIGINAL, Fallout4/CommonwealthFoggy atmosphere.hazeHeight + 100 m (the reference side moved: the
                    true migrated set must then FAIL, proving the old reader reads the original files)
  <out>/empty     : no per-plugin file (the control: both readers on the generated rung alone)
"""
import json, os, shutil, sys

orig, mig, out = sys.argv[1:4]


def clone(src, name):
    d = os.path.join(out, name)
    if os.path.isdir(d):
        shutil.rmtree(d)
    shutil.copytree(src, d)
    return d


def edit(d, rel, fn):
    p = os.path.join(d, rel)
    j = json.load(open(p, encoding='utf-8'))
    fn(j['entries'][0])
    with open(p, 'w', encoding='utf-8', newline='') as f:
        f.write(json.dumps(j, indent=1, ensure_ascii=False) + '\n')


def vis(e):
    e['atmosphere']['visibility'] *= 1.01


def wet(e):
    e['precipitation']['wetting'] *= 60.0


def hh(e):
    e['atmosphere']['hazeHeight'] += 100.0


edit(clone(mig, 'red_value'), 'Fallout4/CommonwealthClear.recx', vis)
edit(clone(mig, 'red_unit'), 'Fallout4/CommonwealthRain.recx', wet)
edit(clone(orig, 'red_ref'), 'Fallout4/CommonwealthFoggy.recx', hh)
e = os.path.join(out, 'empty')
if os.path.isdir(e):
    shutil.rmtree(e)
os.makedirs(e)
print('reds written to', out)

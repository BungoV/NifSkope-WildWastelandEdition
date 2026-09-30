"""TERRLIVE2 rule luma sweep: spec_sw_<name>.json = hills_on, DYNAMIC only, rule from <name>/."""
import json, sys
n = sys.argv[1]
s = json.load(open('spec_hills.json'))
s['rule'] = f'E:/Projects/NifskopeWWE-terrlive2/scratchpad/terrlive2_20260929/{n}/mod/FO4CSLOD/Commonwealth/Commonwealth.lodr'
for v in s['views']:
    v['renders'] = [{"options": ["dynamic"], "loda": False, "out": f"pics/sw_{v['name']}_{n}.png",
                     "titles": [f"hills DYNAMIC {v['name']} {n}"]}]
json.dump(s, open(f'spec_sw_{n}.json', 'w'), indent=1)

"""TERRLIVE2: the outline (north steps) and AO map (street, Boston) picture specs."""
import json
S = 'E:/Projects/NifskopeWWE-terrlive2/scratchpad/terrlive2_20260929'
T1 = 'E:/Projects/NifskopeWWE-terrlive1/scratchpad/terrlive1_20260929'
CW = 'mod/FO4CSLOD/Commonwealth'
LODL = 'E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth/Commonwealth.lodl'
base = {"lodl": LODL, "ws": "Commonwealth", "fade": [8192.0, 12288.0], "frames": 30, "ltex_side": 512,
        "decal_normals": True, "rule": f"{S}/final_on/{CW}/Commonwealth.lodr"}

steps = dict(base, views=[])
for paint in ('on', 'off'):
    v = {"name": f"north_steps_{paint}", "cells": [4, 24, 38, 44], "stride": 1,
         "ortho": [86016.0, 137216.0, 61440.0], "width": 1600, "height": 1000,
         "lodt_far": f"{S}/final_{paint}/{CW}/Commonwealth.VT.8.lodt", "loda": False,
         "renders": [{"option": o, "loda": False, "out": f"pics/steps_after_{o}_{paint}.png",
                      "titles": [f"north steps AFTER (rounded outline) -- {'HYBRID far levels, baked' if o == 'baked' else 'DYNAMIC, live'} -- outside paint {'RULE (on)' if paint == 'on' else 'VANILLA (off)'}"]}
                     for o in ('baked', 'dynamic')]}
    if paint == 'off':
        v["rule"] = False
    steps["views"].append(v)
json.dump(steps, open(f'{S}/spec_steps.json', 'w'), indent=1)

ao = dict(base, loda=f"{S}/final_on/{CW}/Commonwealth.loda", views=[])
common = {"cells": [-8, -12, 3, -1], "stride": 1,
          "lodt_full": f"{T1}/bakes/full/{CW}/Commonwealth.VT.2.lodt",
          "lodt_far": f"{S}/final_on/{CW}/Commonwealth.VT.8.lodt",
          "decals": f"{T1}/bakes/hybrid/{CW}/Commonwealth.lodd"}
street = dict(common, name="street", eye=[-4096.0, -30720.0, 150.0], at=[-4096.0, -22000.0, 0.0], fov=70.0,
              width=1920, height=1080)
boston = dict(common, name="boston", ortho=[-4096.0, -24576.0, 16384.0], width=1200, height=1200)
for v in (street, boston):
    n = v["name"]
    v["renders"] = [
        {"options": ["hybrid"], "loda": False, "out": f"pics/ao_{n}_off.png",
         "titles": [f"{n} -- HYBRID, AO map OFF"]},
        {"options": ["hybrid"], "loda": True, "out": f"pics/ao_{n}_on.png",
         "titles": [f"{n} -- HYBRID, AO map ON (32 u, painted ground only)"]},
        {"options": ["dynamic"], "loda": True, "out": f"pics/ao_{n}_dyn_on.png",
         "titles": [f"{n} -- DYNAMIC, AO map ON (32 u, painted ground only)"]},
        {"options": ["full"], "ao": 1, "ao_lod": 0.0, "loda": False, "out": f"pics/ao_{n}_ref16.png",
         "titles": [f"{n} -- old FULL + baked AO 16 u (reference)"]},
    ]
    ao["views"].append(v)
json.dump(ao, open(f'{S}/spec_aomap.json', 'w'), indent=1)
print('ok')

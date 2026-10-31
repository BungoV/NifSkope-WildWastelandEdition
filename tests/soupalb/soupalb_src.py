"""FIX1031B SOUPALB1 source rule: the ray-traced GI's live soup (rtSoup) carries the per-surface albedo (ALB1).

cellview.cpp builds the probe soup; before SOUPALB1 every albedo site was gated on `baking`, so a live RTXGI soup had
no ALB1 and rtddgi.cpp packed 0.5 grey for every hit. This check reads a cellview.cpp and passes only when:
  1. the gate `soupAlb` includes rtSoup (with the WW_SOUPALB_RED red),
  2. the object triangles, the ground's splat albedo and the ground's triangles all take albedo under soupAlb,
  3. the smooth normals (VNM1) stay a bake's (`baking && smoothOn`): albedo is the only change to the live soup.
  python -I soupalb_src.py <cellview.cpp>     -> SOUPALB SRC OK|FAIL (rc 0|1)
"""
import re
import sys

src = open(sys.argv[1], 'rb').read().decode('utf-8', 'replace').replace('\r\n', '\n')
checks = []
m = re.search(r'const bool soupAlb = baking \|\| \( rtSoup && wwEnvIsEmpty\( "WW_SOUPALB_RED" \) \);', src)
checks.append(('gate soupAlb = baking || rtSoup (red WW_SOUPALB_RED)', m is not None))
checks.append(('object triangles take albedo under soupAlb', src.count('if ( okTri && soupAlb ) {') == 1))
checks.append(('object PBR albedo under soupAlb', 'shapePbrm = soupAlb ? bakePbrmFor( s )' in src))
checks.append(('splat ground albedo under soupAlb',
               re.search(r"if \( soupAlb \) \{\n\s*/\* The ground's albedo per 128-unit quad", src) is not None))
checks.append(('ground triangles take albedo under soupAlb',
               re.search(r"if \( soupAlb \) \{\n\s*/\* the splat's composite", src) is not None))
checks.append(('smooth normals stay a bake\'s (3 sites)', src.count('baking && smoothOn') >= 3
               and 'if ( smoothOn && s.geom.nrm' not in src))
ok = True
for name, good in checks:
    print('  %-50s %s' % (name, 'ok' if good else 'MISSING'))
    ok = ok and good
print('SOUPALB SRC ' + ('OK' if ok else 'FAIL'))
sys.exit(0 if ok else 1)

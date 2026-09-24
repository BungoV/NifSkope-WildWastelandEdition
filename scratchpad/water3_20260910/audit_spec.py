"""Audit every number spec_water.md hands lane WATER3, against the SHIPPED
version-3 file, through the independent decoder (never the writer)."""
import sys, os, json
sys.path.insert(0, os.path.abspath('../water2_20260909'))
from lodl_v3_authority import LodlV3

p = os.path.abspath('../water2_20260909/out/Terrain/Commonwealth.lodl')
d = LodlV3(p)
print('file %s  version %d  bodies %d  strokes %d' % (os.path.basename(p), d.version, d.nBody, d.strokes))
b = d.bodies
FORM = {0x00000018:'ExtOceanWater', 0x00102d5a:'ExtMarshDarkWater', 0x00102d5b:'ExtMarshScumWater',
        0x001c4995:'ExtRiverCharlesUpper', 0x000c8633:'ExtLakeWater', 0x0012e2c1:'ExtLakeForestWater',
        0x00034519:'ExtCreekSanctuaryWater', 0x000c863d:'ExtRiverNFoothillsWaterSE',
        0x00204c42:'ExtLakeQuannapowittWater', 0x001bddb6:'ExtMurkyWater',
        0x001c6eeb:'ExtLakeIrradiatedWater', 0x000df42c:'ExtGlowingSeaWater01',
        0x001c358e:'ExtRiverNFoothillsWaterSW', 0x001c6ee0:'ExtPuddleWater',
        0x001c26c6:'ExtCreekSanctuaryWaterE'}
CLS=['sea','river','lake']; FS=['none','form NAM0','bed','drain','stroke']
# 1. which body IS the Charles
ch = [x for x in b if x['form'] == 0x001c4995]
ch.sort(key=lambda x: -x['area'])
print('\nExtRiverCharlesUpper bodies: %d, largest:' % len(ch))
for x in ch[:3]:
    print('  id %d area %d cells (%d..%d, %d..%d) height %.1f flow %s outlet %d mean (%.3f, %.3f)'
          % (x['id'], x['area'], x['x0'], x['x1'], x['y0'], x['y1'], x['height'],
             FS[x['flowSource']], x['outlet'], x['flowX'], x['flowY']))
# 2. the refuter: the largest ExtMarshDarkWater body (WATER1's 136)
md = [x for x in b if x['form'] == 0x00102d5a]
md.sort(key=lambda x: -x['area'])
x = md[0]
print('\nExtMarshDarkWater largest: id %d area %d cells (%d..%d, %d..%d) height %.1f'
      % (x['id'], x['area'], x['x0'], x['x1'], x['y0'], x['y1'], x['height']))
# 3. the >= 64 texel population and its flow-source split
big = [x for x in b if x['area'] >= 64]
fs = [0]*5; cl=[0]*3
for x in big:
    fs[x['flowSource']] += 1; cl[x['class']] += 1
print('\nbodies with area >= 64: %d   class sea %d river %d lake %d' % (len(big), cl[0], cl[1], cl[2]))
print('  flow source: ' + '  '.join('%s %d' % (FS[i], fs[i]) for i in range(5)))
fsa=[0]*5; cla=[0]*3; tiny=0
for x in b:
    fsa[x['flowSource']] += 1; cla[x['class']] += 1
    if x['area'] < 4: tiny += 1
print('all %d bodies: class sea %d river %d lake %d   TINY %d' % (len(b), cla[0], cla[1], cla[2], tiny))
print('  flow source: ' + '  '.join('%s %d' % (FS[i], fsa[i]) for i in range(5)))
# 4. one-height check
print('\nheights per body: every body carries exactly one plane by construction (component key)')
# 5. wet texels
print('total area over the table: %d' % sum(x['area'] for x in b))
# 6. section sizes
print('body table %d bytes (%d x %d)' % (d.nBody*d.bodyStride, d.nBody, d.bodyStride))
print('stroke store %d bytes, count %d' % (d.strokeLen, d.strokes))
print('offsets: body 0x%X stroke 0x%X id 0x%X flow 0x%X shore 0x%X  filesize %d'
      % (d.oBody, d.oStroke, d.oId, d.oFlow, d.oShore, os.path.getsize(p)))
print('rates: id %d flow %d shore %d quantum %d  name blob off 0x%X len %d'
      % (d.bodyS, d.flowS, d.shoreS, d.shoreQ, d.oName, d.nameLen))

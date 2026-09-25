"""GREY1: which IMGS does the Physical Weathers CommonwealthClear point at (DLCCoast.esm master 1), and what it says."""
import sys, struct
sys.argv = ["x", "nul", "X:/Programs/Steam/steamapps/common/Fallout 4/Data/DLCCoast.esm", "E:/Projects/Fallout 4 Mods/mods/FO4CS Physical Weathers/FO4CSPhysicalWeathers.esp"]
src = open('esm_weather.py').read().split("W = recs[0x3C]")[0]
import io, contextlib
with contextlib.redirect_stdout(io.StringIO()):
    exec(src)
wf = [f for f, r in recs.items() if r[0] == 'WTHR' and edid(f) == 'CommonwealthClear'][0]
F = fields(recs[wf][3])
imsp = [v for t, v in F if t == 'IMSP'][0]
ids = struct.unpack_from('<%dI' % (len(imsp) // 4), imsp)
print('WTHR', hex(wf), 'IMSP', [hex(i) for i in ids])
for i in set(ids):
    if i in recs:
        G = dict(fields(recs[i][3]))
        print(hex(i), edid(i), 'CNAM', struct.unpack_from('<3f', G['CNAM']) if 'CNAM' in G else None,
              'TNAM', struct.unpack_from('<4f', G['TNAM']) if 'TNAM' in G else None,
              'HNAM', [round(x, 3) for x in struct.unpack_from('<9f', G['HNAM'])] if 'HNAM' in G else None)
    else:
        print(hex(i), 'not in ESM/esp IMGS set')
